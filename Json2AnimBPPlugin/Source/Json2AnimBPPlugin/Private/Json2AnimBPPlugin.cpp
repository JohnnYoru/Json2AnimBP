#include "Json2AnimBPPlugin.h"
#include "SJson2AnimBPPanel.h"

#include "Animation/AnimBlueprint.h"
#include "Subsystems/AssetEditorSubsystem.h"

#include "Framework/Docking/TabManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "WorkspaceMenuStructure.h"
#include "WorkspaceMenuStructureModule.h"

#define LOCTEXT_NAMESPACE "Json2AnimBP"

IMPLEMENT_MODULE(FJson2AnimBPPluginModule, Json2AnimBPPlugin)

const FName FJson2AnimBPPluginModule::TabId = FName("Json2AnimBPTab");

// ─────────────────────────────────────────────────────────────────────────────
void FJson2AnimBPPluginModule::StartupModule()
{
    // RegisterNomadTabSpawner with Enabled (default) automatically adds the tab
    // to the main editor's Window menu under the group we specify.
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        TabId,
        FOnSpawnTab::CreateRaw(this, &FJson2AnimBPPluginModule::SpawnTab))
        .SetDisplayName(LOCTEXT("TabTitle", "Json2AnimBP"))
        .SetTooltipText(LOCTEXT("TabTooltip", "Import AnimBP nodes from a JSON file."))
        .SetGroup(WorkspaceMenu::GetMenuStructure().GetLevelEditorCategory())
        .SetMenuType(ETabSpawnerMenuType::Enabled);

    // Track newly opened assets to hand the panel a fresh AnimBP pointer
    if (GEditor)
    {
        UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (Sub)
        {
            AssetEditorOpenedHandle = Sub->OnAssetEditorOpened().AddRaw(
                this, &FJson2AnimBPPluginModule::OnAssetEditorOpened);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
void FJson2AnimBPPluginModule::ShutdownModule()
{
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TabId);

    if (GEditor)
    {
        UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
        if (Sub) Sub->OnAssetEditorOpened().Remove(AssetEditorOpenedHandle);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Tab spawner
// ─────────────────────────────────────────────────────────────────────────────
TSharedRef<SDockTab> FJson2AnimBPPluginModule::SpawnTab(const FSpawnTabArgs& /*Args*/)
{
    TSharedPtr<SJson2AnimBPPanel> Panel;

    TSharedRef<SDockTab> Tab = SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        .Label(LOCTEXT("TabTitle", "Json2AnimBP"))
        [
            SAssignNew(Panel, SJson2AnimBPPanel)
        ];

    PanelPtr = Panel;

    // Immediately point the panel at any already-open AnimBP
    Panel->SetAnimBlueprint(FindActiveAnimBlueprint());

    return Tab;
}

// ─────────────────────────────────────────────────────────────────────────────
// Asset editor opened callback
// ─────────────────────────────────────────────────────────────────────────────
void FJson2AnimBPPluginModule::OnAssetEditorOpened(UObject* Asset)
{
    UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(Asset);
    if (!AnimBP) return;

    // Update live panel reference if the tab is open
    if (TSharedPtr<SJson2AnimBPPanel> Panel = PanelPtr.Pin())
    {
        Panel->SetAnimBlueprint(AnimBP);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Utility: scan all open editors for a UAnimBlueprint
// ─────────────────────────────────────────────────────────────────────────────
UAnimBlueprint* FJson2AnimBPPluginModule::FindActiveAnimBlueprint()
{
    if (!GEditor) return nullptr;
    UAssetEditorSubsystem* Sub = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
    if (!Sub) return nullptr;

    TArray<UObject*> Assets = Sub->GetAllEditedAssets();
    // Iterate in reverse so the most recently opened one wins
    for (int32 i = Assets.Num() - 1; i >= 0; --i)
    {
        if (UAnimBlueprint* ABP = Cast<UAnimBlueprint>(Assets[i]))
            return ABP;
    }
    return nullptr;
}

#undef LOCTEXT_NAMESPACE
