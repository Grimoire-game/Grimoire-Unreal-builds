#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GrimoireEditorSubsystem.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireOfflineImportTest, "Grimoire.Editor.OfflineSnapshotIsAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrimoireOfflineImportTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGrimoireEditorSubsystem> Importer(NewObject<UGrimoireEditorSubsystem>());
    TStrongObjectPtr<UGrimoireDatabase> DB(NewObject<UGrimoireDatabase>()); DB->GameId = TEXT("game");
    FGrimoireObject Bofur; Bofur.Id = TEXT("bofur"); Bofur.Name = TEXT("Original"); DB->Objects.Add(Bofur);
    FGrimoireObject Bartel; Bartel.Id = TEXT("bartel"); Bartel.Name = TEXT("Keep me"); DB->Objects.Add(Bartel);
    FGrimoireDialog Dialog; Dialog.Id = TEXT("opening"); Dialog.GraphJson = TEXT("{\"nodes\":[{\"id\":\"original\"}]}"); DB->Dialogs.Add(Dialog);
    Bofur.Name = TEXT("Updated");
    TestFalse(TEXT("Different game cannot overwrite snapshot"), Importer->UpdateImportedResources(DB.Get(), TEXT("other"), {Bofur}, {}));
    Dialog.GraphJson = TEXT("invalid");
    TestFalse(TEXT("Bad dialog rejects the entire batch"), Importer->UpdateImportedResources(DB.Get(), TEXT("game"), {Bofur}, {Dialog}));
    TestEqual(TEXT("Failed import leaves object unchanged"), DB->Objects[0].Name, TEXT("Original"));
    Dialog.GraphJson = TEXT("{\"nodes\":[{\"id\":\"updated\"}]}");
    TestTrue(TEXT("Valid matching resources update together"), Importer->UpdateImportedResources(DB.Get(), TEXT("game"), {Bofur}, {Dialog}));
    TestEqual(TEXT("Requested resource updated"), DB->Objects[0].Name, TEXT("Updated"));
    TestEqual(TEXT("Unrelated resource retained"), DB->Objects[1].Name, TEXT("Keep me"));
    TestEqual(TEXT("Dialog updated"), DB->Dialogs[0].GraphJson, Dialog.GraphJson);
    return true;
}
#endif
