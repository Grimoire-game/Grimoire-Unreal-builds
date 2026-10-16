#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GrimoireApiClient.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireApiTest, "Grimoire.Editor.ApiContract", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrimoireApiTest::RunTest(const FString&)
{
    TestTrue(TEXT("Queued writes are successful"), FGrimoireApiClient::Decode(202, TEXT(R"({"success":true,"data":{"commit_id":"c","status":"pending"}})")).Success);
    TestFalse(TEXT("HTTP success does not override API error"), FGrimoireApiClient::Decode(200, TEXT(R"({"success":false,"error":"No access"})")).Success);
    TestFalse(TEXT("HTTP failure does not override envelope"), FGrimoireApiClient::Decode(500, TEXT(R"({"success":true})")).Success);
    TestFalse(TEXT("Invalid JSON rejected"), FGrimoireApiClient::Decode(200, TEXT("<html>oops</html>")).Success);
    TestTrue(TEXT("Unauthorized gives actionable message"), FGrimoireApiClient::Decode(401, TEXT(R"({"success":false,"error":"Expired"})")).Error.Contains(TEXT("Sign in")));
    TestEqual(TEXT("Query components are encoded"), FGrimoireApiClient::Encode(TEXT("a&b/c")), FString(TEXT("a%26b%2Fc")));
    TestTrue(TEXT("HTTPS endpoint accepted"), FGrimoireApiClient::IsValidBaseUrl(TEXT("https://api.usegrimoire.com")));
    TestFalse(TEXT("No credentials over HTTP"), FGrimoireApiClient::IsValidBaseUrl(TEXT("http://api.usegrimoire.com")));
    TestFalse(TEXT("Reject embedded credentials"), FGrimoireApiClient::IsValidBaseUrl(TEXT("https://user@host")));
    return true;
}
#endif
