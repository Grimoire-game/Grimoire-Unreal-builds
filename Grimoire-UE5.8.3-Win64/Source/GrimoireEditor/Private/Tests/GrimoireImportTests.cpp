#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GrimoireEditorSubsystem.h"
#include "GrimoireJson.h"
#include "UObject/StrongObjectPtr.h"
#include "Misc/Paths.h"
using namespace GrimoireJson;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireImportTest, "Grimoire.Editor.PaginatedImportAndWrites", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrimoireImportTest::RunTest(const FString&)
{
    // Persist test data only in the supplied disposable validation project.
    if (FPaths::GetBaseFilename(FPaths::GetProjectFilePath()) != TEXT("GrimoireHost"))
    { AddInfo(TEXT("Use UnrealValidation/GrimoireHost.uproject to run the import integration test.")); return true; }
    TStrongObjectPtr<UGrimoireEditorSubsystem> M(NewObject<UGrimoireEditorSubsystem>());
    M->Api = MakeShared<FGrimoireApiClient>(); M->GameId = TEXT("11111111-2222-4333-8444-555555555555"); M->GameName = TEXT("Automation fixture");
    int32 PageCalls = 0, ObjectCalls = 0, CommitCalls = 0, CreateCalls = 0;
    bool FailObject = false;
    int32 VariableCalls = 0, VariableFailure = 0;
    M->Api->TestTransport = [this, &PageCalls, &ObjectCalls, &CommitCalls, &CreateCalls, &FailObject, &VariableCalls, &VariableFailure](const FString& Verb, const FString& Path,
        const TMap<FString, FString>& Query, const FObject& Body, const FString& Token, FGrimoireApiCallback Done)
    {
        auto Reply = [&Done](const FString& Json, int32 Status = 200) { Done(FGrimoireApiClient::Decode(Status, Json)); };
        if (Path == TEXT("/api/v1/auth/login"))
        {
            TestTrue(TEXT("Login sends no bearer"), Token.IsEmpty());
            TestEqual(TEXT("Login email wire name"), String(Body, TEXT("email")), FString(TEXT("test@example.invalid")));
            Reply(TEXT(R"({"success":true,"data":{"token":"fixture-session","expires_in":"24h","user":{"id":"user-1"}}})")); return;
        }
        TestEqual(TEXT("Authenticated API calls use session"), Token, FString(TEXT("fixture-session")));
        TestEqual(TEXT("Every data request is game scoped"), Query.FindRef(TEXT("game_id")), FString(TEXT("11111111-2222-4333-8444-555555555555")));
        if (Path == TEXT("/api/v1/objects") && Verb == TEXT("GET"))
        {
            ++PageCalls; const int32 Offset = FCString::Atoi(*Query.FindRef(TEXT("offset")));
            TArray<TSharedPtr<FJsonValue>> Items;
            for (int32 I = Offset; I < FMath::Min(Offset + 100, 101); ++I)
            { auto O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("id"), FString::Printf(TEXT("o%d"), I)); Items.Add(MakeShared<FJsonValueObject>(O)); }
            auto E = MakeShared<FJsonObject>(); E->SetBoolField(TEXT("success"), true); E->SetArrayField(TEXT("data"), Items); E->SetNumberField(TEXT("count"), Items.Num()); Reply(Write(E)); return;
        }
        if (Path.StartsWith(TEXT("/api/v1/objects/")))
        {
            ++ObjectCalls;
            if (FailObject) { Reply(TEXT(R"({"success":false,"error":"Fixture failure"})"), 503); return; }
            const FString Id = Path.Mid(FString(TEXT("/api/v1/objects/")).Len());
            auto E = Parse(TEXT(R"({"success":true,"data":{"schema_version":"1.0","object":{},"sections":[{"fields":[{"id":"hp","label":"Health","kind":"number","hints":{"game_engine_editable":true},"values":[{"number":{"value":10}}]}]}]}})"));
            auto Summary = Object(Object(E, TEXT("data")), TEXT("object")); Summary->SetStringField(TEXT("id"), Id); Summary->SetStringField(TEXT("code_id"), TEXT("key-") + Id);
            Reply(Write(E)); return;
        }
        if (Path == TEXT("/api/v1/dialogs")) { Reply(TEXT(R"({"success":true,"data":[]})")); return; }
        if (Path == TEXT("/api/v1/variables"))
        {
            ++VariableCalls; int32 Offset = FCString::Atoi(*Query.FindRef(TEXT("offset")));
            TestEqual(TEXT("Variable page size"), Query.FindRef(TEXT("limit")), FString(TEXT("200")));
            if (Offset > 0 && VariableFailure == 1) { Reply(TEXT(R"({"success":false,"error":"No variables:read scope"})"), 403); return; }
            if (Offset > 0 && VariableFailure == 2) Offset = 0;
            TArray<TSharedPtr<FJsonValue>> Items;
            for (int32 I = Offset; I < FMath::Min(Offset + 200, 201); ++I)
            {
                auto V = Parse(TEXT(R"({"element_type":"number","elements":[{"id":"","name":"Easy","value":3}]})"));
                V->SetStringField(TEXT("id"), FString::Printf(TEXT("v%d"), I)); V->SetStringField(TEXT("name"), FString::Printf(TEXT("Variable %d"), I));
                AsObject(Array(V, TEXT("elements"))[0])->SetStringField(TEXT("id"), FString::Printf(TEXT("e%d"), I));
                Items.Add(MakeShared<FJsonValueObject>(V));
            }
            auto Envelope = MakeShared<FJsonObject>(); Envelope->SetBoolField(TEXT("success"), true); Envelope->SetArrayField(TEXT("data"), Items); Reply(Write(Envelope)); return;
        }
        if (Path == TEXT("/api/v1/strings"))
        { TestEqual(TEXT("Translations requested"), Query.FindRef(TEXT("include_translations")), FString(TEXT("true"))); Reply(TEXT(R"({"success":true,"data":[{"id":"s1","abbrev":"hello","source_text":"Hello"}]})")); return; }
        if (Path == TEXT("/api/v1/engine-commits"))
        {
            ++CommitCalls; TestEqual(TEXT("Unreal source"), String(Body, TEXT("source")), FString(TEXT("unreal")));
            auto Changes = Array(Body, TEXT("changes")); TestEqual(TEXT("One change"), Changes.Num(), 1);
            if (Changes.Num() == 1) { auto C = AsObject(Changes[0]); TestEqual(TEXT("Original base value"), Number(C, TEXT("base_value")), 10.0); TestEqual(TEXT("New numeric value"), Number(C, TEXT("value")), 25.0); }
            Reply(TEXT(R"({"success":true,"data":{"commit":{"id":"commit1","status":"pending"}}})"), 201); return;
        }
        if (Path == TEXT("/api/v1/objects") && Verb == TEXT("POST"))
        { ++CreateCalls; TestEqual(TEXT("Creation is always draft"), String(Body, TEXT("status")), FString(TEXT("draft"))); TestFalse(TEXT("Blank templates omitted"), Body->HasField(TEXT("template_id"))); Reply(TEXT(R"({"success":true,"data":{"id":"new-object"}})"), 201); return; }
        AddError(TEXT("Unexpected endpoint: ") + Path); Reply(TEXT(R"({"success":false,"error":"Unexpected endpoint"})"), 400);
    };
    M->Api->Login(TEXT("test@example.invalid"), TEXT("fixture-only"), [](const auto&) {});
    M->ImportGame();
    TestFalse(TEXT("Import ends busy state"), M->Busy);
    TestEqual(TEXT("Fetches second page"), PageCalls, 2); TestEqual(TEXT("Fetches all object details"), ObjectCalls, 101);
    if (!TestNotNull(TEXT("Database created"), M->Database.Get())) return false;
    TestEqual(TEXT("No pagination truncation"), M->Database->Objects.Num(), 101);
    TestEqual(TEXT("Strings imported"), M->Database->GetText(TEXT("hello"), TEXT("")).ToString(), FString(TEXT("Hello")));
    TestEqual(TEXT("Variables paginated"), VariableCalls, 2);
    TestEqual(TEXT("All game variables imported"), M->Database->GameVariables.Num(), 201);
    TestTrue(TEXT("Sync marker set"), M->Database->VariablesImported);
    const auto ImportedAt = M->Database->ImportedAt;
    VariableFailure = 1; M->SyncVariables();
    TestFalse(TEXT("Failed variable sync ends"), M->Busy);
    TestEqual(TEXT("Second-page failure preserves variables"), M->Database->GameVariables.Num(), 201);
    TestTrue(TEXT("Permission error actionable"), M->Status.Contains(TEXT("variables:read")));
    VariableFailure = 2; M->SyncVariables();
    TestTrue(TEXT("Repeated page rejected"), M->Status.Contains(TEXT("Repeated")));
    VariableFailure = 0; M->SyncVariables();
    TestEqual(TEXT("Variable-only sync keeps object import time"), M->Database->ImportedAt, ImportedAt);
    TestEqual(TEXT("Variable-only sync keeps objects"), M->Database->Objects.Num(), 101);
    M->QueueField(TEXT("o0"), TEXT("hp"), TEXT("25")); TestEqual(TEXT("Engine edit submitted"), CommitCalls, 1);
    M->CreateObject(TEXT("Draft"), TEXT(""), TEXT("")); TestEqual(TEXT("Draft creation submitted"), CreateCalls, 1);
    FailObject = true; M->ImportGame();
    TestEqual(TEXT("Failed refresh preserves existing snapshot"), M->Database->Objects.Num(), 101);
    TestFalse(TEXT("Failure releases busy state"), M->Busy);
    TestEqual(TEXT("Failure is visible"), M->Status, FString(TEXT("Fixture failure")));
    M->Api->SignOut(); return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireAuthLifecycleTest, "Grimoire.Editor.TwoFactorRefreshAndCancellation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrimoireAuthLifecycleTest::RunTest(const FString&)
{
    auto Client = MakeShared<FGrimoireApiClient>(); FGrimoireApiCallback Pending;
    int32 Refreshes = 0, Delivered = 0;
    Client->TestTransport = [this, &Pending, &Refreshes](const FString&, const FString& Path, const auto&, const FObject& Body, const FString& Token, FGrimoireApiCallback Done)
    {
        if (Path.EndsWith(TEXT("login"))) Done(FGrimoireApiClient::Decode(200, TEXT(R"({"success":true,"data":{"requires_2fa":true,"temp_token":"challenge"}})")));
        else if (Path.EndsWith(TEXT("verify-2fa")))
        { TestEqual(TEXT("2FA challenge sent"), String(Body, TEXT("temp_token")), FString(TEXT("challenge"))); TestEqual(TEXT("2FA code sent"), String(Body, TEXT("code")), FString(TEXT("123456"))); Done(FGrimoireApiClient::Decode(200, TEXT(R"({"success":true,"data":{"token":"short-session","expires_in":"1s"}})"))); }
        else if (Path.EndsWith(TEXT("refresh")))
        { ++Refreshes; TestEqual(TEXT("Refresh uses old token"), Token, FString(TEXT("short-session"))); Done(FGrimoireApiClient::Decode(200, TEXT(R"({"success":true,"data":{"token":"new-session","expires_in":"24h"}})"))); }
        else { TestEqual(TEXT("API gets refreshed token"), Token, FString(TEXT("new-session"))); Pending = MoveTemp(Done); }
    };
    Client->Login(TEXT("test@example.invalid"), TEXT("fake"), [](const auto&) {});
    TestFalse(TEXT("Challenge is not a session"), Client->IsSignedIn());
    Client->Verify2FA(TEXT("123456"), [](const auto&) {}); TestTrue(TEXT("2FA creates session"), Client->IsSignedIn());
    Client->Request(TEXT("GET"), TEXT("/api/v1/games"), {}, nullptr, [&Delivered](const auto&) { ++Delivered; });
    TestEqual(TEXT("Near-expiry session refreshed once"), Refreshes, 1);
    Client->Cancel();
    Pending(FGrimoireApiClient::Decode(200, TEXT(R"({"success":true,"data":[]})")));
    TestEqual(TEXT("Late completion after cancel ignored"), Delivered, 0);
    Client->SignOut(); TestFalse(TEXT("Sign out clears credentials"), Client->IsSignedIn());
    return true;
}
#endif
