#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Modules/ModuleManager.h"
#include "Styling/SlateColor.h"

class FSlateStyleSet;

class FGitStatusEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RefreshStatus();
    void RegisterMenus();
    bool Tick(float DeltaSeconds);

    FString GetStatus() const;
    FText GetToolbarLabel() const;
    FText GetToolbarTooltip() const;
    FSlateColor GetStatusColor() const;

    FString Repository;
    FString Branch;
    FString Details;
    bool bAvailable = false;
    bool bHasChanges = false;

    TSharedPtr<FSlateStyleSet> IconStyle;
    FDelegateHandle StartupCallbackHandle;
    FTSTicker::FDelegateHandle RefreshTickerHandle;
};
