#include "Modules/ModuleManager.h"
#include "SGrimoireConnect.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"
#include "Framework/Docking/TabManager.h"

class FGrimoireEditorModule : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        FGlobalTabmanager::Get()->RegisterNomadTabSpawner(TEXT("GrimoireConnect"), FOnSpawnTab::CreateLambda([](const FSpawnTabArgs&)
        { return SNew(SDockTab).TabRole(ETabRole::NomadTab) [ SNew(SGrimoireConnect) ]; }))
            .SetDisplayName(NSLOCTEXT("Grimoire", "ConnectTitle", "Grimoire Connect")).SetMenuType(ETabSpawnerMenuType::Hidden);
        UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FGrimoireEditorModule::RegisterMenu));
    }
    virtual void ShutdownModule() override
    {
        UToolMenus::UnRegisterStartupCallback(this); UToolMenus::UnregisterOwner(this);
        if (auto Tab = FGlobalTabmanager::Get()->FindExistingLiveTab(FTabId(TEXT("GrimoireConnect")))) Tab->RequestCloseTab();
        FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(TEXT("GrimoireConnect"));
    }
    void RegisterMenu()
    {
        FToolMenuOwnerScoped Owner(this);
        auto* Menu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Window"));
        Menu->FindOrAddSection(TEXT("Grimoire")).AddMenuEntry(TEXT("GrimoireConnect"),
            NSLOCTEXT("Grimoire", "Open", "Grimoire Connect"), NSLOCTEXT("Grimoire", "Tooltip", "Connect to Grimoire and import game data"), FSlateIcon(),
            FUIAction(FExecuteAction::CreateLambda([] { FGlobalTabmanager::Get()->TryInvokeTab(FTabId(TEXT("GrimoireConnect"))); })));
    }
};
IMPLEMENT_MODULE(FGrimoireEditorModule, GrimoireEditor)
