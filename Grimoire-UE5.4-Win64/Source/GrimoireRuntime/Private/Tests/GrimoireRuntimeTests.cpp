#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GrimoireData.h"
#include "GrimoireDialogPlayer.h"
#include "GrimoireObjectLink.h"
#include "GrimoireJson.h"
#include "UObject/StrongObjectPtr.h"
using namespace GrimoireJson;

namespace
{
    constexpr auto TestFlags = EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter;
    const TCHAR* ObjectFixture = TEXT(R"json({"schema_version":"1.0","object":{"id":"object-1","code_id":"bed","name":"Bed"},"sections":[{"title":"Stats","fields":[
      {"id":"hp","label":"Health","kind":"number","hints":{"game_engine_editable":true},"values":[{"number":{"value":42.5}}]},
      {"id":"title","label":"Title","kind":"text","values":[{"plain":"Rest","text":{"content":"<b>Rest</b>","string_id":"str-1"}}]},
      {"id":"bool","label":"Open","kind":"boolean","values":[{"boolean":{"value":true}}]},
      {"id":"xyz","label":"Position","kind":"vector","values":[{"vector":{"components":[{"axis":"x","value":1},{"axis":"y","value":2},{"axis":"z","value":3}]}}]},
      {"id":"refs","label":"Levels","kind":"reference","multiple":true,"values":[{"reference":{"id":"level-1","code_id":"l1","name":"Level 1","ref_kind":"object"}}]},
      {"id":"locked","label":"Read only","kind":"number","hints":{"game_engine_editable":true,"read_only":true},"values":[]}
    ]}]})json");
    bool AddDialog(UGrimoireDatabase* DB, const FString& Json)
    { FGrimoireDialog D; FString Error; if (!UGrimoireDatabase::ParseDialog(Parse(Json), D, Error)) return false; DB->Dialogs.Add(D); return true; }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireObjectTest, "Grimoire.Runtime.TypedObjectsAndReferences", TestFlags)
bool FGrimoireObjectTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGrimoireDatabase> DB(NewObject<UGrimoireDatabase>());
    FGrimoireObject O; FString Error;
    TestTrue(TEXT("Parse API document"), UGrimoireDatabase::ParseObject(Parse(ObjectFixture), O, Error)); DB->Objects.Add(O);
    FGrimoireObject Target; Target.Id = TEXT("level-1"); Target.CodeId = TEXT("l1"); DB->Objects.Add(Target);
    TStrongObjectPtr<UGrimoireObjectLink> Link(NewObject<UGrimoireObjectLink>()); Link->Database = DB.Get(); Link->ObjectKey = TEXT("bed");
    double N = 0; TestTrue(TEXT("Read case-insensitive label"), Link->GetNumber(TEXT("health"), N)); TestEqual(TEXT("Precise number"), N, 42.5);
    bool B = false; TestTrue(TEXT("Read bool"), Link->GetBoolean(TEXT("bool"), B)); TestTrue(TEXT("Bool value"), B);
    FVector V; TestTrue(TEXT("Read vector"), Link->GetVector(TEXT("xyz"), V)); TestEqual(TEXT("Vector value"), V, FVector(1,2,3));
    TestFalse(TEXT("Reject mismatched getter"), Link->GetNumber(TEXT("title"), N));
    TestFalse(TEXT("Reject out-of-bounds list"), Link->GetNumber(TEXT("hp"), N, 8));
    TestTrue(TEXT("Resolve imported reference offline"), Link->GetReferencedObject(TEXT("Levels"), Target)); TestEqual(TEXT("Reference target"), Target.CodeId, FString(TEXT("l1")));
    FGrimoireField Field; DB->GetField(TEXT("bed"), TEXT("locked"), Field); TestFalse(TEXT("Read-only hint wins"), Field.Editable);
    DB->GetField(TEXT("bed"), TEXT("title"), Field); TestEqual(TEXT("Keep rich text concurrency base"), Field.StoredValueJson(), FString(TEXT("\"<b>Rest</b>\"")));
    TestFalse(TEXT("Malformed document rejected"), UGrimoireDatabase::ParseObject(Parse(TEXT("{}")), O, Error));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireLocalizationTest, "Grimoire.Runtime.LocalizationFallback", TestFlags)
bool FGrimoireLocalizationTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGrimoireDatabase> DB(NewObject<UGrimoireDatabase>()); DB->SourceLanguage = TEXT("en");
    FGrimoireString S;
    TestTrue(TEXT("Parse strings"), UGrimoireDatabase::ParseString(Parse(TEXT(R"({"id":"str-1","abbrev":"greeting","source_text":"Hello","voice_url":"source.ogg","translations":[{"language_code":"nl","translated_text":"Hallo","voice_url":"nl.ogg"},{"language_code":"fr","translated_text":""}]})")), S));
    DB->Strings.Add(S);
    TestEqual(TEXT("Locale case-insensitive"), DB->GetText(TEXT("greeting"), TEXT("NL")).ToString(), FString(TEXT("Hallo")));
    TestEqual(TEXT("Missing translation falls back"), DB->GetText(TEXT("str-1"), TEXT("de")).ToString(), FString(TEXT("Hello")));
    TestEqual(TEXT("Empty translation falls back"), DB->GetText(TEXT("str-1"), TEXT("fr")).ToString(), FString(TEXT("Hello")));
    TestFalse(TEXT("Preview detects missing translation"), DB->HasTranslation(TEXT("str-1"), TEXT("fr")));
    TestEqual(TEXT("Localized voice"), DB->GetVoiceUrl(TEXT("str-1"), TEXT("nl")), FString(TEXT("nl.ogg")));
    TestTrue(TEXT("Missing keys return empty"), DB->GetText(TEXT("missing"), TEXT("nl")).IsEmpty());
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireDialogTest, "Grimoire.Runtime.DialogConditionsSettersAndChoices", TestFlags)
bool FGrimoireDialogTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGrimoireDatabase> DB(NewObject<UGrimoireDatabase>());
    TestTrue(TEXT("Import dialog"), AddDialog(DB.Get(), TEXT(R"json({"id":"d1","dialog_key":"bed-talk","data":{"startingNode":"set","variables":[{"name":"count","type":"number","initialValue":0}],"nodes":[
      {"id":"set","type":"setter","setter":{"variable":"count","operator":"increment","value":2},"next_node":"condition"},
      {"id":"condition","type":"condition","condition":{"variable":"count"},"conditionOptions":[{"operator":">=","condition_value":2,"next_node":"ask"}]},
      {"id":"ask","type":"question","text":"Rest?","pickOnce":true,"options":[{"id":"yes","option_text":"Yes","next_node":"ask"},{"id":"hidden","option_text":"Hidden","visibilityCondition":{"variable":"count","operator":"==","value":0}},{"id":"leave","option_text":"Leave","alwaysAvailable":true,"next_node":"end"}]},
      {"id":"end","type":"end-text","text":"Goodbye"}
    ]}})json")));
    TStrongObjectPtr<UGrimoireDialogPlayer> Player(NewObject<UGrimoireDialogPlayer>()); Player->Database = DB.Get();
    TestTrue(TEXT("Starts and walks automatic nodes"), Player->StartDialog(TEXT("bed-talk")));
    TestEqual(TEXT("Setter applied"), Player->GetVariableJson(TEXT("d1"), TEXT("count")), FString(TEXT("2")));
    TestEqual(TEXT("Hidden choice excluded"), Player->CurrentLine.Choices.Num(), 2);
    Player->Next(); TestEqual(TEXT("Next does not bypass a question"), Player->CurrentLine.NodeId, FString(TEXT("ask")));
    Player->Choose(99); TestEqual(TEXT("Invalid choice is ignored"), Player->CurrentLine.Choices.Num(), 2);
    Player->Choose(0); TestEqual(TEXT("Pick-once removes choice on return"), Player->CurrentLine.Choices.Num(), 1);
    Player->Choose(0); TestTrue(TEXT("End text waits"), Player->IsPlaying && Player->CurrentLine.IsLast);
    Player->Next(); TestFalse(TEXT("Ends after final line"), Player->IsPlaying);
    TestEqual(TEXT("Variables reset after ending"), Player->GetVariableJson(TEXT("d1"), TEXT("count")), FString(TEXT("0")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireLegacyTest, "Grimoire.Runtime.LegacySectionsAndExternalDialogs", TestFlags)
bool FGrimoireLegacyTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGrimoireDatabase> DB(NewObject<UGrimoireDatabase>());
    TestTrue(TEXT("Legacy import"), AddDialog(DB.Get(), TEXT(R"({"id":"legacy","data":{"startingSection":"s1","sections":[{"id":"s1","fields":[{"id":"b","type":"text","text":"Second","order_index":1},{"id":"a","type":"text","text":"First","order_index":0}],"fieldOrder":["a","b"]},{"id":"s2","fields":[{"id":"jump","type":"external-dialog","next_dialog":"other"}]}]}})")));
    TestTrue(TEXT("External import"), AddDialog(DB.Get(), TEXT(R"({"id":"other","data":{"startingNode":"o","nodes":[{"id":"o","type":"end-text","text":"Other dialog"}]}})")));
    TStrongObjectPtr<UGrimoireDialogPlayer> P(NewObject<UGrimoireDialogPlayer>()); P->Database = DB.Get();
    TestTrue(TEXT("Legacy starts"), P->StartDialog(TEXT("legacy"))); TestEqual(TEXT("Field order applied"), P->CurrentLine.Text.ToString(), FString(TEXT("First")));
    P->Next(); TestEqual(TEXT("Implicit field link"), P->CurrentLine.Text.ToString(), FString(TEXT("Second")));
    P->Next(); TestEqual(TEXT("Implicit section then external link"), P->CurrentDialogId, FString(TEXT("other")));
    TestEqual(TEXT("External start node"), P->CurrentLine.Text.ToString(), FString(TEXT("Other dialog")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireLoopTest, "Grimoire.Runtime.MalformedGraphsAndLoopGuard", TestFlags)
bool FGrimoireLoopTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGrimoireDatabase> DB(NewObject<UGrimoireDatabase>());
    TestFalse(TEXT("Duplicate node ids rejected"), AddDialog(DB.Get(), TEXT(R"({"id":"bad","data":{"nodes":[{"id":"same"},{"id":"same"}]}})")));
    AddDialog(DB.Get(), TEXT(R"({"id":"loop","data":{"nodes":[{"id":"a","type":"jump","next_node":"a"}]}})"));
    TStrongObjectPtr<UGrimoireDialogPlayer> P(NewObject<UGrimoireDialogPlayer>()); P->Database = DB.Get();
    TestFalse(TEXT("Loop fails without hanging game"), P->StartDialog(TEXT("loop")));
    TestTrue(TEXT("Explicit loop reason"), P->LastEndReason.Contains(TEXT("limit")));
    TestFalse(TEXT("Missing dialog fails"), P->StartDialog(TEXT("missing")));
    AddDialog(DB.Get(), TEXT(R"({"id":"broken","data":{"nodes":[{"id":"a","type":"external-dialog","next_dialog":"missing"}]}})"));
    TestFalse(TEXT("Missing external dialog fails"), P->StartDialog(TEXT("broken")));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireCompareTest, "Grimoire.Runtime.ConditionOperators", TestFlags)
bool FGrimoireCompareTest::RunTest(const FString&)
{
    auto Check = [this](const TCHAR* A, const TCHAR* B, const TCHAR* Op, bool Expected)
    { TestEqual(FString::Printf(TEXT("%s %s %s"), A, Op, B), UGrimoireDialogPlayer::CompareValues(ParseValue(A), ParseValue(B), Op), Expected); };
    Check(TEXT("2"), TEXT("\"2\""), TEXT("=="), true); Check(TEXT("null"), TEXT("0"), TEXT("=="), false);
    Check(TEXT("true"), TEXT("\"true\""), TEXT("=="), true); Check(TEXT("4"), TEXT("3"), TEXT(">"), true);
    Check(TEXT("\"Hello\""), TEXT("\"ELL\""), TEXT("contains"), true);
    Check(TEXT("[1,2]"), TEXT("2"), TEXT("includes"), true); Check(TEXT("[1,2]"), TEXT("3"), TEXT("not_includes"), true);
    Check(TEXT("4"), TEXT("\"1..5\""), TEXT("in_range"), true); Check(TEXT("6"), TEXT("\"1..5\""), TEXT("in_range"), false);
    return true;
}
#endif
