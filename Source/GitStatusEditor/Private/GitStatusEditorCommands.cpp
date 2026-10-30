#include "GitStatusEditorCommands.h"

#include "Editor.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMisc.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Level.h"
#include "Engine/World.h"

namespace GitStatusCommands
{
    // These files are local to an Unreal project, ignored by Git and not network-accessible.
    // Only explicit, fixed editor operations are accepted; arbitrary console commands are not.
    static FString ControlDirectory() => FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("GitStatusControl"));

    static void GetDirtyPackages(TArray<UPackage*>& Packages)
    {
        UEditorLoadingAndSavingUtils::GetDirtyMapPackages(Packages);
        UEditorLoadingAndSavingUtils::GetDirtyContentPackages(Packages);
    }

    static bool SaveOnePackage(UPackage* Package, FString& Message)
    {
        if (!Package)
        {
            Message = TEXT("The requested package is not loaded in this Unreal Editor.");
            return false;
        }
        if (!Package->IsDirty())
        {
            Message = FString::Printf(TEXT("Already clean: %s"), *Package->GetName());
            return true;
        }
        if (!FPackageName::DoesPackageExist(Package->GetName()))
        {
            Message = TEXT("The package has no existing file. Save it interactively once to choose its location.");
            return false;
        }

        TArray<UPackage*> Packages;
        Packages.Add(Package);
        const bool bSaved = UEditorLoadingAndSavingUtils::SavePackages(Packages, true);
        if (!bSaved || Package->IsDirty())
        {
            Message = FString::Printf(TEXT("Save failed or package remains dirty: %s"), *Package->GetName());
            return false;
        }

        Message = FString::Printf(TEXT("Saved %s"), *Package->GetName());
        return true;
    }

    static bool SaveAll(FString& Message)
    {
        TArray<UPackage*> DirtyPackages;
        GetDirtyPackages(DirtyPackages);
        if (DirtyPackages.IsEmpty())
        {
            Message = TEXT("All loaded packages are already clean.");
            return true;
        }

        // Do not let an unattended command trigger a Save As dialog for a new asset/map.
        for (UPackage* Package : DirtyPackages)
        {
            if (Package && !FPackageName::DoesPackageExist(Package->GetName()))
            {
                Message = FString::Printf(
                    TEXT("Cannot save new package %s unattended. Save it interactively first."),
                    *Package->GetName());
                return false;
            }
        }

        if (!UEditorLoadingAndSavingUtils::SaveDirtyPackages(true, true))
        {
            Message = TEXT("Save All returned a failure. Check Unreal Editor output.");
            return false;
        }

        DirtyPackages.Reset();
        GetDirtyPackages(DirtyPackages);
        if (!DirtyPackages.IsEmpty())
        {
            Message = TEXT("Unreal still has unsaved packages after Save All.");
            return false;
        }

        Message = TEXT("Saved all dirty map and content packages.");
        return true;
    }

    static bool ResolveLoadedPackage(const FString& Input, UPackage*& Package, FString& Message)
    {
        FString PackageName;
        if (Input.StartsWith(TEXT("/")))
        {
            PackageName = FPackageName::ObjectPathToPackageName(Input);
        }
        else
        {
            // Accept Content/Maps/Example.umap and absolute .umap/.uasset filenames.
            const FString FilePath = FPaths::IsRelative(Input)
                ? FPaths::Combine(FPaths::ProjectDir(), Input)
                : Input;
            if (!FPackageName::TryConvertFilenameToLongPackageName(FilePath, PackageName))
            {
                Message = TEXT("Use a loaded asset path such as /Game/Maps/Example or Content/Maps/Example.umap.");
                return false;
            }
        }

        if (!FPackageName::IsValidLongPackageName(PackageName))
        {
            Message = TEXT("Invalid Unreal package name.");
            return false;
        }

        Package = FindPackage(nullptr, *PackageName);
        if (!Package)
        {
            Message = FString::Printf(
                TEXT("Package %s is not loaded. Open it in this editor before requesting a save."),
                *PackageName);
            return false;
        }
        return true;
    }

    static bool Execute(const FString& Action, const FString& AssetPath, FString& Message, bool& bExit)
    {
        bExit = false;

        if (Action == TEXT("SaveAll") || Action == TEXT("SaveAndShutdown"))
        {
            if (!SaveAll(Message))
            {
                return false;
            }
            bExit = Action == TEXT("SaveAndShutdown");
            return true;
        }

        if (Action == TEXT("SaveCurrentLevel"))
        {
            UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
            if (!World || !World->PersistentLevel)
            {
                Message = TEXT("There is no active editor level to save.");
                return false;
            }
            return SaveOnePackage(World->PersistentLevel->GetOutermost(), Message);
        }

        if (Action == TEXT("SaveFile"))
        {
            if (AssetPath.IsEmpty())
            {
                Message = TEXT("SaveFile needs asset_path (a loaded Unreal package or asset filename).");
                return false;
            }
            UPackage* Package = nullptr;
            return ResolveLoadedPackage(AssetPath, Package, Message) && SaveOnePackage(Package, Message);
        }

        if (Action == TEXT("Shutdown"))
        {
            TArray<UPackage*> DirtyPackages;
            GetDirtyPackages(DirtyPackages);
            if (!DirtyPackages.IsEmpty())
            {
                Message = TEXT("Shutdown refused: unsaved packages remain. Use SaveAll or SaveAndShutdown.");
                return false;
            }
            Message = TEXT("Clean editor shutdown requested.");
            bExit = true;
            return true;
        }

        Message = TEXT("Unknown action. Allowed: SaveFile, SaveCurrentLevel, SaveAll, Shutdown, SaveAndShutdown.");
        return false;
    }

    void Poll()
    {
        const FString CommandsDirectory = FPaths::Combine(ControlDirectory(), TEXT("Commands"));
        if (!IFileManager::Get().DirectoryExists(*CommandsDirectory))
        {
            return;
        }

        TArray<FString> Files;
        IFileManager::Get().FindFiles(
            Files, *FPaths::Combine(CommandsDirectory, TEXT("*.json")), true, false);
        Files.Sort();

        // One command per scheduler tick. The calling script should wait for its result.
        if (Files.IsEmpty())
        {
            return;
        }

        const FString CommandFile = FPaths::Combine(CommandsDirectory, Files[0]);
        const FString RequestId = FPaths::GetBaseFilename(Files[0]);
        const FString ResultsDirectory = FPaths::Combine(ControlDirectory(), TEXT("Results"));
        IFileManager::Get().MakeDirectory(*ResultsDirectory, true);

        FString Content;
        FString Action;
        FString AssetPath;
        FString CreatedUtc;
        FString Message;
        bool bExit = false;
        bool bSuccess = false;

        if (!FFileHelper::LoadFileToString(Content, *CommandFile))
        {
            Message = TEXT("Unable to read command file.");
        }
        else
        {
            TSharedPtr<FJsonObject> Request;
            const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Content);
            if (!FJsonSerializer::Deserialize(Reader, Request) || !Request.IsValid()
                || !Request->TryGetStringField(TEXT("action"), Action)
                || !Request->TryGetStringField(TEXT("created_utc"), CreatedUtc))
            {
                Message = TEXT("Invalid command JSON: action and created_utc are required.");
            }
            else
            {
                Request->TryGetStringField(TEXT("asset_path"), AssetPath);

                FDateTime IssuedAt;
                if (!FDateTime::ParseIso8601(*CreatedUtc, IssuedAt))
                {
                    Message = TEXT("Invalid created_utc ISO timestamp.");
                }
                else
                {
                    const double AgeSeconds = (FDateTime::UtcNow() - IssuedAt).GetTotalSeconds();
                    if (AgeSeconds < -5.0 || AgeSeconds > 60.0)
                    {
                        Message = TEXT("Stale or future-dated command refused.");
                    }
                    else
                    {
                        bSuccess = Execute(Action, AssetPath, Message, bExit);
                    }
                }
            }
        }

        const TSharedRef<FJsonObject> Response = MakeShared<FJsonObject>();
        Response->SetStringField(TEXT("request_id"), RequestId);
        Response->SetStringField(TEXT("action"), Action);
        Response->SetBoolField(TEXT("success"), bSuccess);
        Response->SetStringField(TEXT("message"), Message);
        FString ResultText;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ResultText);
        FJsonSerializer::Serialize(Response, Writer);

        const FString ResultFile = FPaths::Combine(ResultsDirectory, RequestId + TEXT(".json"));
        if (!FFileHelper::SaveStringToFile(
                ResultText, *ResultFile, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        {
            // Without acknowledgement, never shut down; retry writing on the next tick.
            UE_LOG(LogTemp, Error, TEXT("GitStatus could not write command result: %s"), *ResultFile);
            return;
        }

        IFileManager::Get().Delete(*CommandFile, false, true);
        UE_LOG(LogTemp, Log, TEXT("GitStatus command %s: %s"), *Action, *Message);
        if (bSuccess && bExit)
        {
            FPlatformMisc::RequestExit(false, TEXT("GitStatus local editor command"));
        }
    }
}
