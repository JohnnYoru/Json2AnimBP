#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class SJson2AnimBPPanel;
class UAnimBlueprint;

class FJson2AnimBPPluginModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    // Spawns the nomad tab that holds the panel.
    TSharedRef<SDockTab> SpawnTab(const FSpawnTabArgs& SpawnTabArgs);

    // Fired whenever any asset editor opens; used to hand the panel a fresh AnimBP ref.
    void OnAssetEditorOpened(UObject* Asset);

    // Walk all open editors and return the first (most recent) UAnimBlueprint found.
    static UAnimBlueprint* FindActiveAnimBlueprint();

    TWeakPtr<SJson2AnimBPPanel> PanelPtr;
    FDelegateHandle           AssetEditorOpenedHandle;

    static const FName TabId;
};
