#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Interfaces/IHttpRequest.h"
#include "Containers/Ticker.h"

struct GRIMOIREEDITOR_API FGrimoireApiResult
{
    bool Success = false;
    int32 Status = 0;
    int32 Count = -1;
    FString Error;
    FString Code;
    TSharedPtr<FJsonValue> Data;
};
using FGrimoireApiCallback = TFunction<void(const FGrimoireApiResult&)>;

/** Editor-only credentials, held in memory. Closing the editor destroys the session. */
class GRIMOIREEDITOR_API FGrimoireApiClient : public TSharedFromThis<FGrimoireApiClient>
{
public:
#if WITH_DEV_AUTOMATION_TESTS
    // Offline contract tests inject a transport; this never reads real credentials.
    TFunction<void(const FString&, const FString&, const TMap<FString, FString>&,
        const TSharedPtr<FJsonObject>&, const FString&, FGrimoireApiCallback)> TestTransport;
#endif
    FString BaseUrl = TEXT("https://api.usegrimoire.com");
    FString UserId;
    FString UserName;
    FString TempToken;
    ~FGrimoireApiClient();
    bool IsSignedIn() const { return !Token.IsEmpty(); }
    void Login(const FString& Email, const FString& Password, FGrimoireApiCallback Done);
    void Verify2FA(const FString& Code, FGrimoireApiCallback Done);
    void SignOut();
    void Cancel();
    void Request(const FString& Verb, const FString& Path, const TMap<FString, FString>& Query,
        const TSharedPtr<FJsonObject>& Body, FGrimoireApiCallback Done);
    static FString Encode(const FString& Value);
    static bool IsValidBaseUrl(const FString& Url);
    static FGrimoireApiResult Decode(int32 Status, const FString& Body);
private:
    FString Token;
    FDateTime ExpiresAt;
    TArray<TSharedRef<IHttpRequest, ESPMode::ThreadSafe>> Pending;
    TArray<FTSTicker::FDelegateHandle> RetryTimers;
    uint64 Generation = 0;
    void Send(const FString& Verb, const FString& Path, const TMap<FString, FString>& Query,
        const TSharedPtr<FJsonObject>& Body, bool Auth, FGrimoireApiCallback Done, int32 RetryAttempt = 0);
    void AcceptSession(FGrimoireApiResult Result, FGrimoireApiCallback Done);
};
