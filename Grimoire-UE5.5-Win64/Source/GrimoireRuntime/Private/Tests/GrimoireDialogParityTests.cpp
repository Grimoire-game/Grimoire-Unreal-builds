#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GrimoireData.h"
#include "GrimoireDialogPlayer.h"
#include "GrimoireObjectLink.h"
#include "GrimoireSubsystem.h"
#include "GrimoireJson.h"
#include "UObject/StrongObjectPtr.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
using namespace GrimoireJson;

namespace
{
    constexpr auto Flags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
    bool AddGraph(UGrimoireDatabase* DB, const TCHAR* Json)
    { FGrimoireDialog D; FString Error; if (!UGrimoireDatabase::ParseDialog(Parse(Json), D, Error)) return false; DB->Dialogs.Add(D); return true; }
    void AddVariables(UGrimoireDatabase* DB)
    {
        FGrimoireGameVariable V; FString Error;
        UGrimoireDatabase::ParseGameVariable(Parse(TEXT(R"({"id":"difficulty","name":"Difficulty","element_type":"range","elements":[{"id":"easy","name":"Easy","value":3,"range_min":0,"range_max":10}]})")), V, Error);
        DB->GameVariables.Add(V); DB->VariablesImported = true;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireContextDataTest, "Grimoire.Runtime.ContextNotesAndNodeData", Flags)
bool FGrimoireContextDataTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGrimoireDatabase> DB(NewObject<UGrimoireDatabase>());
    AddGraph(DB.Get(), TEXT(R"({"id":"d","data":{"nodes":[{"id":"context-id","node_identifier":"context","name":"Arrival","type":"context","speaker":"should-not-display","voice_url":"silence.ogg","text":"A door opens.","next_node":"ask","custom":{"camera":"wide"}}, {"id":"ask","type":"question","text":"Hello?","options":[{"id":"a","option_text":"Hello","next_node":"end"},{"id":"b","option_text":"Hidden","visibilityCondition":{"subjectRef":{"source":"literal","value":false},"value":true}}]}, {"id":"end","type":"end-text","text":"Bye"}]}})"));
    TStrongObjectPtr<UGrimoireDialogPlayer> P(NewObject<UGrimoireDialogPlayer>()); P->Database = DB.Get();
    TestTrue(TEXT("Context starts"), P->StartDialog(TEXT("d")));
    TestTrue(TEXT("Context exposed to Blueprint"), P->CurrentLine.IsContext);
    TestEqual(TEXT("Context text"), P->CurrentLine.Text.ToString(), FString(TEXT("A door opens.")));
    TestTrue(TEXT("Context has no speaker or voice"), P->CurrentLine.Speaker.IsEmpty() && P->CurrentLine.SpeakerId.IsEmpty() && P->CurrentLine.VoiceUrl.IsEmpty());
    FGrimoireDialogNodeData Node;
    TestTrue(TEXT("Current node query"), P->GetCurrentNode(Node));
    TestEqual(TEXT("Authored node name exposed"), Node.Name, FString(TEXT("Arrival")));
    TestTrue(TEXT("Custom metadata kept"), Node.JsonFields.FindRef(TEXT("custom")).Contains(TEXT("wide")));
    P->Next(); TestEqual(TEXT("Context advances to question"), P->CurrentLine.NodeId, FString(TEXT("ask")));
    TestEqual(TEXT("Only visible choice displayed"), P->CurrentLine.Choices.Num(), 1);
    TestEqual(TEXT("Full node retains hidden choices"), P->CurrentLine.Node.Options.Num(), 2);
    if (P->CurrentLine.Choices.Num()) TestEqual(TEXT("Choice id exposed"), P->CurrentLine.Choices[0].OptionId, FString(TEXT("a")));
    P->ShowContextNotes = false; P->StartDialog(TEXT("d"));
    TestEqual(TEXT("Disabled contexts skip"), P->CurrentLine.NodeId, FString(TEXT("ask")));
    TestTrue(TEXT("Lookup unvisited node by identifier"), DB->GetDialogNode(TEXT("d"), TEXT("end"), Node));
    TestTrue(TEXT("Lookup by raw id"), DB->GetDialogNode(TEXT("d"), TEXT("context-id"), Node));
    TestFalse(TEXT("Missing node clears output"), DB->GetDialogNode(TEXT("d"), TEXT("unknown"), Node));
    TestTrue(TEXT("No stale node"), Node.Id.IsEmpty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireGameVariablesTest, "Grimoire.Runtime.SharedGameVariablesAndObjectValues", Flags)
bool FGrimoireGameVariablesTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGrimoireDatabase> DB(NewObject<UGrimoireDatabase>()); DB->GameId = TEXT("game-a"); AddVariables(DB.Get());
    TStrongObjectPtr<UGrimoireDatabase> Other(NewObject<UGrimoireDatabase>()); Other->GameId = TEXT("game-b"); AddVariables(Other.Get());
    TStrongObjectPtr<UGameInstance> Game(NewObject<UGameInstance>()); Game->Init();
    const auto WorldOptions = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &WorldOptions);
    World->SetGameInstance(Game.Get());
    auto* State = Game->GetSubsystem<UGrimoireSubsystem>();
    double N = 0;
    TestTrue(TEXT("Variable names resolve case-insensitively"), State->GetGameNumber(DB.Get(), TEXT("difficulty"), TEXT("EASY"), N)); TestEqual(TEXT("Default variable"), N, 3.0);
    State->SetGameNumber(DB.Get(), TEXT("Difficulty"), TEXT("Easy"), 20);
    State->GetGameNumber(DB.Get(), TEXT(""), TEXT("easy"), N); TestEqual(TEXT("Ranges enforced"), N, 10.0);
    State->GetGameNumber(Other.Get(), TEXT(""), TEXT("easy"), N); TestEqual(TEXT("Games isolated"), N, 3.0);
    TestFalse(TEXT("Invalid JSON rejected"), State->SetGameVariableJson(DB.Get(), TEXT(""), TEXT("easy"), TEXT("bad-json")));
    TestFalse(TEXT("Unknown elements rejected"), State->SetGameNumber(DB.Get(), TEXT(""), TEXT("missing"), 5));
    State->ResetGameValues(DB.Get());
    FGrimoireObject O; FString Error;
    UGrimoireDatabase::ParseObject(Parse(TEXT(R"({"object":{"id":"npc","code_id":"bofur"},"sections":[{"fields":[{"id":"hp","kind":"number","values":[{"number":{"value":4}}]}]}]})")), O, Error); DB->Objects.Add(O);
    AddGraph(DB.Get(), TEXT(R"({"id":"set","data":{"nodes":[{"id":"s","type":"setter","setter":{"targetRef":{"source":"type_element","typeId":"difficulty","elementId":"easy"},"operator":"increment","value":2},"next_node":"object"},{"id":"object","type":"setter","setter":{"targetRef":{"source":"object_field","objectId":"npc","fieldId":"hp"},"value":9},"next_node":"end"},{"id":"end","type":"end-dialog"}]}})"));
    AddGraph(DB.Get(), TEXT(R"({"id":"read","data":{"nodes":[{"id":"check","type":"condition","condition":{"subjectRef":{"source":"type_element","typeId":"difficulty","elementId":"easy"}},"conditionOptions":[{"operator":"==","condition_value":5,"next_node":"ok"}]},{"id":"ok","type":"end-text","text":"Shared value"}]}})"));
    auto* Actor = World->SpawnActor<AActor>();
    auto* First = NewObject<UGrimoireDialogPlayer>(Actor); First->Database = DB.Get(); Actor->AddInstanceComponent(First);
    auto* Second = NewObject<UGrimoireDialogPlayer>(Actor); Second->Database = DB.Get(); Actor->AddInstanceComponent(Second);
    auto* Link = NewObject<UGrimoireObjectLink>(Actor); Link->Database = DB.Get(); Link->ObjectKey = TEXT("bofur"); Actor->AddInstanceComponent(Link);
    TestTrue(TEXT("Setter dialog completes"), First->StartDialog(TEXT("set")));
    TestFalse(TEXT("Automatic end reached"), First->IsPlaying);
    TestTrue(TEXT("Another player reads shared value after completion"), Second->StartDialog(TEXT("read")));
    TestEqual(TEXT("Shared condition matched"), Second->CurrentLine.Text.ToString(), FString(TEXT("Shared value")));
    AddGraph(DB.Get(), TEXT(R"({"id":"tokens","data":{"nodes":[{"id":"text","type":"end-text","text":"Health: #object:npc;hp#","translations":[{"language_code":"nl","translated_text":"Gezondheid: #object:npc;hp#"}]}]}})"));
    Second->Language = TEXT("nl"); Second->StartDialog(TEXT("tokens"));
    TestEqual(TEXT("Localized inline token reads shared object value"), Second->CurrentLine.Text.ToString(), FString(TEXT("Gezondheid: 9")));
    State->GetGameNumber(DB.Get(), TEXT(""), TEXT("easy"), N); TestEqual(TEXT("Gameplay sees dialog change"), N, 5.0);
    Link->GetNumber(TEXT("hp"), N); TestEqual(TEXT("Object Link sees dialog setter"), N, 9.0);
    TestEqual(TEXT("Database stays immutable"), DB->FindObject(TEXT("npc"))->FindField(TEXT("hp"))->Values[0].Number, 4.0);
    State->ResetGameValues(DB.Get()); Link->GetNumber(TEXT("hp"), N); TestEqual(TEXT("Reset restores imported object default"), N, 4.0);
    Game->Shutdown(); World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireRequirementsTest, "Grimoire.Runtime.DialogDependencyReport", Flags)
bool FGrimoireRequirementsTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGrimoireDatabase> DB(NewObject<UGrimoireDatabase>()); AddVariables(DB.Get());
    FGrimoireObject O; O.Id = TEXT("npc"); O.Name = TEXT("Bofur"); FGrimoireField F; F.Id = TEXT("hp"); F.Label = TEXT("Health"); O.Fields.Add(F); DB->Objects.Add(O);
    AddGraph(DB.Get(), TEXT(R"({"id":"d1","name":"First","data":{"speakerMode":"object","nodes":[{"id":"a","type":"question","speaker":"npc","text":"#object:npc;hp#","options":[{"id":"hidden","option_text":"x","visibilityCondition":{"subjectRef":{"source":"object_field","objectId":"absent","fieldId":"flag"}},"variableAction":{"targetRef":{"source":"type_element","typeId":"difficulty","elementId":"missing"}},"next_dialog":"d2"}]}]}})"));
    AddGraph(DB.Get(), TEXT(R"({"id":"d2","data":{"nodes":[{"id":"a","type":"question","options":[{"next_dialog":"d1"},{"next_dialog":"absent-dialog"},{"next_node":"absent-node"}]}]}})"));
    const auto Report = DB->GetDialogRequirements(TEXT("d1"), nullptr);
    auto Has = [&Report](const FString& Kind, const FString& Id, bool Missing)
    { return Report.ContainsByPredicate([&](const auto& R) { return R.Kind == Kind && R.ResourceId == Id && R.Missing == Missing; }); };
    TestTrue(TEXT("Hidden answer object requirement found"), Has(TEXT("Object field"), TEXT("absent"), true));
    TestTrue(TEXT("Missing setter element found"), Has(TEXT("Game variable"), TEXT("difficulty"), true));
    TestTrue(TEXT("External dialog dependency found"), Has(TEXT("Dialog"), TEXT("absent-dialog"), true));
    TestTrue(TEXT("Invalid local edge found"), Has(TEXT("Node"), TEXT("d2"), true));
    TestTrue(TEXT("Inline object token uses imported fallback"), Report.ContainsByPredicate([](const auto& R) { return R.Kind == TEXT("Object field") && R.ResourceId == TEXT("npc") && !R.Missing && R.UsesImportedFallback; }));
    TestTrue(TEXT("External cycles terminate"), Report.Num() < 20);
    return true;
}
#endif
