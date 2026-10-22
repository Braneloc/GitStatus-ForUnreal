#include "GitStatusEditorModule.h"

#include "Brushes/SlateImageBrush.h"
#include "DirectoryWatcherModule.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "IDirectoryWatcher.h"
#include "HAL/PlatformProcess.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/Paths.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"
#include "ToolMenus.h"
#include "ToolMenu.h"
#include "ToolMenuEntry.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "GitStatusEditor"

namespace GitStatus
{
    constexpr float SchedulerTickSeconds = 1.0f;
    constexpr double DebounceSeconds = 0.75;
    constexpr double FallbackSeconds = 120.0;
    const FName ToolbarName(TEXT("LevelEditor.LevelEditorToolBar.PlayToolBar"));

    bool RunGit(const FString& Directory, const FString& Command, FString& Output, FString* ErrorMessage = nullptr)
    {
        int32 ExitCode = INDEX_NONE;
        FString Error;
        const FString Arguments = FString::Printf(TEXT("-C \"%s\" %s"), *Directory, *Command);
        const bool bStarted = FPlatformProcess::ExecProcess(
            TEXT("git"), *Arguments, &ExitCode, &Output, &Error);

        Output.TrimStartAndEndInline();
        if (ErrorMessage)
        {
            *ErrorMessage = bStarted
                ? FString::Printf(TEXT("Git exited with code %d: %s"), ExitCode, *Error.TrimStartAndEnd())
                : TEXT("Cannot start Git; check that git.exe is on PATH.");
        }
        return bStarted && ExitCode == 0;
    }

    FString RepositoryNameFromPath(FString Path)
    {
        Path.TrimStartAndEndInline();
        Path.ReplaceInline(TEXT("\\"), TEXT("/"));
        while (Path.EndsWith(TEXT("/")))
        {
            Path.LeftChopInline(1);
        }

        // Supports https://github.com/owner/name.git, git@github.com:owner/name.git,
        // file://.../name.git, and local repositories without a remote.
        int32 Slash = INDEX_NONE;
        int32 Colon = INDEX_NONE;
        if (Path.FindLastChar(TEXT('/'), Slash))
        {
            Path = Path.Mid(Slash + 1);
        }
        else if (Path.FindLastChar(TEXT(':'), Colon))
        {
            Path = Path.Mid(Colon + 1);
        }

        if (Path.EndsWith(TEXT(".git"), ESearchCase::IgnoreCase))
        {
            Path.LeftChopInline(4);
        }
        return Path;
    }
}

void FGitStatusEditorModule::StartupModule()
{
    IconStyle = MakeShared<FSlateStyleSet>(TEXT("GitStatusStyle"));

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("GitStatus"));
    const FString ResourceDirectory = Plugin.IsValid()
        ? FPaths::Combine(Plugin->GetBaseDir(), TEXT("Resources"))
        : FPaths::Combine(FPaths::ProjectPluginsDir(), TEXT("GitStatus/Resources"));

    IconStyle->SetContentRoot(ResourceDirectory);
    IconStyle->Set(TEXT("GitStatus.BranchIcon"),
        new FSlateVectorImageBrush(
            IconStyle->RootToContentDir(TEXT("git-branch-16"), TEXT(".svg")),
            FVector2D(16.0f, 16.0f),
            FLinearColor::White,
            ESlateBrushTileType::NoTile));
    FSlateStyleRegistry::RegisterSlateStyle(*IconStyle);

    RefreshStatus();
    RegisterWatchers();

    if (FSlateApplication::IsInitialized())
    {
        ApplicationActivationHandle = FSlateApplication::Get()
            .OnApplicationActivationStateChanged()
            .AddRaw(this, &FGitStatusEditorModule::OnApplicationActivationChanged);
    }

    StartupCallbackHandle = UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGitStatusEditorModule::RegisterMenus));

    RefreshTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FGitStatusEditorModule::Tick),
        GitStatus::SchedulerTickSeconds);
}

void FGitStatusEditorModule::ShutdownModule()
{
    FTSTicker::GetCoreTicker().RemoveTicker(RefreshTickerHandle);
    StopWatchers();
    if (ApplicationActivationHandle.IsValid() && FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationHandle);
        ApplicationActivationHandle.Reset();
    }
    UToolMenus::UnRegisterStartupCallback(StartupCallbackHandle);
    UToolMenus::UnregisterOwner(this);

    if (IconStyle.IsValid())
    {
        FSlateStyleRegistry::UnRegisterSlateStyle(*IconStyle);
        IconStyle.Reset();
    }
}

void FGitStatusEditorModule::QueueRefresh(double DelaySeconds)
{
    bRefreshRequested = true;
    RefreshAfterSeconds = FPlatformTime::Seconds() + DelaySeconds;
}

void FGitStatusEditorModule::OnApplicationActivationChanged(const bool bIsActive)
{
    if (bIsActive)
    {
        QueueRefresh(0.0);
    }
}

bool FGitStatusEditorModule::Tick(float DeltaSeconds)
{
    if (bDirectoryChangePending.exchange(false))
    {
        QueueRefresh(GitStatus::DebounceSeconds);
    }
    const double Now = FPlatformTime::Seconds();
    if ((bRefreshRequested && Now >= RefreshAfterSeconds)
        || (Now - LastRefreshSeconds >= GitStatus::FallbackSeconds))
    {
        bRefreshRequested = false;
        RefreshStatus();
    }
    return true;
}

void FGitStatusEditorModule::RefreshStatus()
{
    LastRefreshSeconds = FPlatformTime::Seconds();
    // The .uproject may live below the Git root or in a separate Git worktree.
    const FString Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir());
    FString Output;
    FString Error;

    // Never retain divergence figures when switching projects or repositories.
    bComparisonAvailable = false;
    ComparisonBranch.Empty();
    ComparisonDetails = TEXT("No local origin default branch reference found. Fetch Origin to refresh.");
    BehindComparison = 0;
    AheadComparison = 0;

    if (!GitStatus::RunGit(
            Directory,
            TEXT("-c core.quotePath=false status --porcelain=v1 --branch --untracked-files=normal"),
            Output, &Error))
    {
        bAvailable = false;
        bHasChanges = false;
        Repository.Empty();
        Branch.Empty();
        Details = Error;
        return;
    }

    TArray<FString> Lines;
    Output.ParseIntoArrayLines(Lines, true);
    if (Lines.IsEmpty() || !Lines[0].StartsWith(TEXT("## ")))
    {
        bAvailable = false;
        bHasChanges = false;
        Repository.Empty();
        Branch.Empty();
        Details = TEXT("Git returned an unexpected status format.");
        return;
    }

    FString Header = Lines[0].RightChop(3);
    if (Header.StartsWith(TEXT("No commits yet on ")) || Header.StartsWith(TEXT("Initial commit on ")))
    {
        Branch = Header.RightChop(18);
    }
    else if (Header.StartsWith(TEXT("HEAD ")))
    {
        Branch = TEXT("(detached HEAD)");
    }
    else
    {
        // Strip upstream and ahead/behind annotations from the display name.
        const int32 Upstream = Header.Find(TEXT("..."));
        if (Upstream != INDEX_NONE)
        {
            Header.LeftInline(Upstream);
        }
        const int32 Annotation = Header.Find(TEXT(" ["));
        if (Annotation != INDEX_NONE)
        {
            Header.LeftInline(Annotation);
        }
        Branch = Header;
    }

    // Prefer the remote repository name over the local folder name: this
    // avoids showing arbitrary worktree folder names as the Git project.
    FString Remote;
    FString Root;
    if (GitStatus::RunGit(Directory, TEXT("config --get remote.origin.url"), Remote) && !Remote.IsEmpty())
    {
        Repository = GitStatus::RepositoryNameFromPath(Remote);
    }
    else if (GitStatus::RunGit(Directory, TEXT("rev-parse --show-toplevel"), Root))
    {
        Repository = GitStatus::RepositoryNameFromPath(Root);
    }
    else
    {
        Repository = FPaths::GetCleanFilename(FPaths::GetPath(Directory));
    }

    bAvailable = true;
    const int32 ChangedEntryCount = Lines.Num() - 1;
    bHasChanges = ChangedEntryCount > 0;
    const FString ChangeSummary = bHasChanges
        ? FString::Printf(
            TEXT("%d changed/untracked %s"),
            ChangedEntryCount,
            ChangedEntryCount == 1 ? TEXT("entry") : TEXT("entries"))
        : TEXT("Working tree clean");

    Details = FString::Printf(TEXT("Repository: %s\n%s\n%s"),
        *Repository, *Lines[0], *ChangeSummary);

    // Compare HEAD with the shared integration branch WITHOUT changing its
    // upstream or contacting the network. The next 15-second tick notices
    // updated origin/* references after GitHub Desktop / SourceTree fetches.
    //
    // Teams using Development get that branch preferentially. Other projects
    // fall back to origin/HEAD, i.e. their configured remote default branch.
    FString Unused;
    FString RemoteReference;
    if (GitStatus::RunGit(
        Directory, TEXT("show-ref --verify --quiet refs/remotes/origin/Development"), Unused))
    {
        RemoteReference = TEXT("origin/Development");
    }
    else
    {
        GitStatus::RunGit(
            Directory, TEXT("symbolic-ref --quiet --short refs/remotes/origin/HEAD"),
            RemoteReference);
    }

    if (!RemoteReference.IsEmpty())
    {
        FString CountsText;
        if (GitStatus::RunGit(
                Directory,
                FString::Printf(TEXT("rev-list --left-right --count \"%s...HEAD\""),
                    *RemoteReference),
                CountsText))
        {
            TArray<FString> Counts;
            CountsText.ParseIntoArray(Counts, TEXT("\t"), true);
            if (Counts.Num() == 2 && Counts[0].IsNumeric() && Counts[1].IsNumeric())
            {
                BehindComparison = FCString::Atoi(*Counts[0]);
                AheadComparison = FCString::Atoi(*Counts[1]);
                ComparisonBranch = RemoteReference;
                ComparisonBranch.RemoveFromStart(TEXT("origin/"));
                bComparisonAvailable = true;
                ComparisonDetails = FString::Printf(
                    TEXT("Compared with %s: %d behind, %d ahead.\n"
                         "Uses locally cached remote refs; Fetch Origin to refresh.\n"
                         "No merges or branch changes are performed."),
                    *RemoteReference, BehindComparison, AheadComparison);
            }
        }
    }
}

FString FGitStatusEditorModule::GetStatus() const
{
    return !bAvailable ? TEXT("Unavailable")
        : bHasChanges ? TEXT("Modified") : TEXT("Clean");
}

FText FGitStatusEditorModule::GetToolbarLabel() const
{
    if (!bAvailable)
    {
        return LOCTEXT("UnavailableLabel", "Git: unavailable");
    }

    return FText::FromString(FString::Printf(TEXT("%s / %s  (%s)"),
        *Repository, *Branch, *GetStatus()));
}

FText FGitStatusEditorModule::GetComparisonLabel() const
{
    if (!bComparisonAvailable)
    {
        return FText::GetEmpty();
    }

    return FText::FromString(FString::Printf(TEXT("%s  ↓%d ↑%d"),
        *ComparisonBranch, BehindComparison, AheadComparison));
}

FSlateColor FGitStatusEditorModule::GetComparisonColor() const
{
    return BehindComparison > 0
        ? FSlateColor(FLinearColor(0.98f, 0.70f, 0.27f))
        : FSlateColor(FLinearColor(0.33f, 0.80f, 0.45f));
}

FSlateColor FGitStatusEditorModule::GetStatusColor() const
{
    if (!bAvailable)
    {
        return FSlateColor(FLinearColor(0.90f, 0.32f, 0.32f));
    }
    return bHasChanges
        ? FSlateColor(FLinearColor(0.98f, 0.70f, 0.27f))
        : FSlateColor(FLinearColor(0.33f, 0.80f, 0.45f));
}

FText FGitStatusEditorModule::GetToolbarTooltip() const
{
    return FText::FromString(FString::Printf(
        TEXT("Unreal project: %s\n%s\n%s\n"
             "Read-only Git status: no automatic fetch, merge, push or file modifications."),
        *FPaths::ProjectDir(), *Details, *ComparisonDetails));
}

void FGitStatusEditorModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);
    UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu(GitStatus::ToolbarName);

    FToolMenuSection& Section = Toolbar->AddSection(
        TEXT("GitStatusSection"),
        TAttribute<FText>(),
        FToolMenuInsert(TEXT("Play"), EToolMenuInsertType::After));

    // Prefer the right-hand end of the Play toolbar where space permits.
    Section.Alignment = EToolMenuSectionAlign::Last;

    Section.AddEntry(FToolMenuEntry::InitWidget(
        TEXT("GitStatusWidget"),
        SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(3.0f, 0.0f)
        [
            SNew(SImage)
            .Image(IconStyle->GetBrush(TEXT("GitStatus.BranchIcon")))
            .ColorAndOpacity_Lambda([this]() { return GetStatusColor(); })
            .ToolTipText_Lambda([this]() { return GetToolbarTooltip(); })
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(4.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return GetToolbarLabel(); })
            .ToolTipText_Lambda([this]() { return GetToolbarTooltip(); })
        ]
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(8.0f, 0.0f, 3.0f, 0.0f)
        [
            SNew(STextBlock)
            .Text_Lambda([this]() { return GetComparisonLabel(); })
            .ColorAndOpacity_Lambda([this]() { return GetComparisonColor(); })
            .Visibility_Lambda([this]() {
                return bComparisonAvailable ? EVisibility::Visible : EVisibility::Collapsed;
            })
            .ToolTipText_Lambda([this]() { return GetToolbarTooltip(); })
        ],
        LOCTEXT("GitStatusToolbar", "Git Status"),
        true, false, true,
        LOCTEXT("GitStatusTooltip", "Current Git repository, branch and file status")));
}

IMPLEMENT_MODULE(FGitStatusEditorModule, GitStatusEditor)

#undef LOCTEXT_NAMESPACE
