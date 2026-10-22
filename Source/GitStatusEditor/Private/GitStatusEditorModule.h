#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateColor.h"
#include <atomic>

class FSlateStyleSet;
struct FFileChangeData;

class FGitStatusEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RefreshStatus();
    void RegisterMenus();
    void RegisterWatchers();
    void StopWatchers();
    void WatchDirectory(const FString& Directory, bool bGitMetadata);
    void OnGitMetadataChanged(const TArray<FFileChangeData>& Changes);
    void OnProjectFilesChanged(const TArray<FFileChangeData>& Changes);
    void OnApplicationActivationChanged(const bool bIsActive);
    void QueueRefresh(double DelaySeconds);
    bool Tick(float DeltaSeconds);

    FString GetStatus() const;
    FText GetToolbarLabel() const;
    FText GetComparisonLabel() const;
    FText GetToolbarTooltip() const;
    FSlateColor GetStatusColor() const;
    FSlateColor GetComparisonColor() const;

    FString Repository;
    FString Branch;
    FString Details;
    FString ComparisonBranch;
    FString ComparisonDetails;
    int32 BehindComparison = 0;
    int32 AheadComparison = 0;
    bool bComparisonAvailable = false;
    bool bAvailable = false;
    bool bHasChanges = false;

    struct FWatchedDirectory { FString Path; FDelegateHandle Handle; };
    TArray<FWatchedDirectory> WatchedDirectories;
    // Directory watcher callbacks can run off the game thread.
    std::atomic<bool> bDirectoryChangePending{false};
    bool bRefreshRequested = false;
    double RefreshAfterSeconds = 0.0;
    double LastRefreshSeconds = 0.0;

    TSharedPtr<FSlateStyleSet> IconStyle;
    FDelegateHandle StartupCallbackHandle;
    FDelegateHandle ApplicationActivationHandle;
    FTSTicker::FDelegateHandle RefreshTickerHandle;
};
