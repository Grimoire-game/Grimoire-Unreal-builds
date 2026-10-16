#include "GrimoireApiClient.h"
#include "GrimoireJson.h"
#include "HttpModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Interfaces/IHttpResponse.h"
using namespace GrimoireJson;

FGrimoireApiClient::~FGrimoireApiClient() { Cancel(); }
FString FGrimoireApiClient::Encode(const FString& V) { return FGenericPlatformHttp::UrlEncode(V); }
bool FGrimoireApiClient::IsValidBaseUrl(const FString& Url)
{
    // Credentials must never travel over cleartext, including custom endpoints.
    if (!Url.StartsWith(TEXT("https://")) || Url.Contains(TEXT("?")) || Url.Contains(TEXT("#")) || Url.Contains(TEXT("@")) || Url.Contains(TEXT(" ")) || Url.Contains(TEXT("\n")) || Url.Contains(TEXT("\r"))) return false;
    const FString Host = Url.Mid(8); return !Host.IsEmpty() && !Host.StartsWith(TEXT("/"));
}
void FGrimoireApiClient::Cancel()
{
    ++Generation;
    for (auto Handle : RetryTimers) FTSTicker::RemoveTicker(Handle);
    RetryTimers.Empty();
    auto Requests = MoveTemp(Pending); Pending.Empty();
    for (auto& R : Requests) { R->OnProcessRequestComplete().Unbind(); R->CancelRequest(); }
}
void FGrimoireApiClient::SignOut()
{ Cancel(); Token.Empty(); TempToken.Empty(); UserId.Empty(); UserName.Empty(); ExpiresAt = FDateTime(); }

FGrimoireApiResult FGrimoireApiClient::Decode(int32 Status, const FString& Text)
{
    FGrimoireApiResult R; R.Status = Status;
    auto Envelope = Parse(Text);
    if (!Envelope) { R.Error = FString::Printf(TEXT("HTTP %d: response was not a JSON object."), Status); return R; }
    R.Success = Status >= 200 && Status < 300 && Bool(Envelope, TEXT("success"));
    R.Data = Value(Envelope, TEXT("data"));
    R.Count = static_cast<int32>(Number(Envelope, TEXT("count"), -1));
    if (!R.Success)
    {
        R.Code = String(Envelope, TEXT("code"));
        R.Error = String(Envelope, TEXT("error"), FString::Printf(TEXT("HTTP %d: API request failed."), Status));
        if (Status == 401) R.Error += TEXT(" Sign in again.");
        if (Status == 429) R.Error += TEXT(" Wait before retrying.");
    }
    return R;
}
void FGrimoireApiClient::Send(const FString& Verb, const FString& Path, const TMap<FString, FString>& Query,
    const FObject& Body, bool Auth, FGrimoireApiCallback Done, int32 RetryAttempt)
{
    FGrimoireApiResult Failure;
    if (!IsValidBaseUrl(BaseUrl)) { Failure.Error = TEXT("API base URL must be an HTTPS origin."); Done(Failure); return; }
#if WITH_DEV_AUTOMATION_TESTS
    if (TestTransport)
    {
        TWeakPtr<FGrimoireApiClient> Weak = AsShared(); const uint64 Started = Generation;
        TestTransport(Verb, Path, Query, Body, Auth ? Token : FString(), [Weak, Started, Done = MoveTemp(Done)](const auto& R)
        { if (auto Self = Weak.Pin()) if (Started == Self->Generation) Done(R); });
        return;
    }
#endif
    FString Url = BaseUrl; Url.RemoveFromEnd(TEXT("/")); Url += Path;
    bool FirstParameter = true;
    for (const auto& Pair : Query)
        if (!Pair.Value.IsEmpty()) { Url += (FirstParameter ? TEXT("?") : TEXT("&")) + Encode(Pair.Key) + TEXT("=") + Encode(Pair.Value); FirstParameter = false; }
    auto Request = FHttpModule::Get().CreateRequest(); Request->SetURL(Url); Request->SetVerb(Verb);
    Request->SetTimeout(30); Request->SetDelegateThreadPolicy(EHttpRequestDelegateThreadPolicy::CompleteOnGameThread);
    Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
    if (Auth) Request->SetHeader(TEXT("Authorization"), TEXT("Bearer ") + Token);
    if (Body) { Request->SetHeader(TEXT("Content-Type"), TEXT("application/json")); Request->SetContentAsString(Write(Body)); }
    TWeakPtr<FGrimoireApiClient> Weak = AsShared();
    const uint64 Started = Generation;
    // Shared callback also handles a synchronous ProcessRequest failure exactly once.
    auto Callback = MakeShared<FGrimoireApiCallback>(MoveTemp(Done));
    Request->OnProcessRequestComplete().BindLambda([Weak, Started, Callback, Verb, Path, Query, Body, Auth, RetryAttempt](FHttpRequestPtr Req, FHttpResponsePtr Response, bool Connected)
    {
        auto Self = Weak.Pin(); if (!Self || Started != Self->Generation) return;
        Self->Pending.Remove(Req.ToSharedRef());
        if (!*Callback) return;
        auto Complete = MoveTemp(*Callback);
        if (!Connected || !Response) { FGrimoireApiResult R; R.Error = TEXT("Could not reach Grimoire. Check the connection and API URL."); Complete(R); return; }
        const int32 Status = Response->GetResponseCode();
        if (Verb == TEXT("GET") && RetryAttempt < 3 && (Status == 429 || Status == 502 || Status == 503 || Status == 504))
        {
            float Delay = FMath::Pow(2.f, static_cast<float>(RetryAttempt + 1));
            float RetryAfter;
            if (LexTryParseString(RetryAfter, *Response->GetHeader(TEXT("Retry-After")))) Delay = FMath::Clamp(RetryAfter, 1.f, 300.f);
            Self->RetryTimers.Add(FTSTicker::GetCoreTicker().AddTicker(TEXT("GrimoireReadRetry"), Delay,
                [Weak, Verb, Path, Query, Body, Auth, RetryAttempt, Complete = MoveTemp(Complete)](float) mutable
                { if (auto S = Weak.Pin()) S->Send(Verb, Path, Query, Body, Auth, MoveTemp(Complete), RetryAttempt + 1); return false; }));
            return;
        }
        Complete(Decode(Response->GetResponseCode(), Response->GetContentAsString()));
    });
    Pending.Add(Request);
    if (!Request->ProcessRequest() && *Callback)
    {
        Pending.Remove(Request); Request->OnProcessRequestComplete().Unbind();
        Failure.Error = TEXT("Could not start the HTTP request."); auto Complete = MoveTemp(*Callback); Complete(Failure);
    }
}
void FGrimoireApiClient::AcceptSession(FGrimoireApiResult R, FGrimoireApiCallback Done)
{
    if (R.Success)
    {
        auto Data = AsObject(R.Data);
        if (Bool(Data, TEXT("requires_2fa"))) TempToken = String(Data, TEXT("temp_token"));
        else if (!String(Data, TEXT("token")).IsEmpty())
        {
            Token = String(Data, TEXT("token")); TempToken.Empty();
            auto User = Object(Data, TEXT("user")); UserId = String(User, TEXT("id"));
            UserName = First(String(User, TEXT("full_name")), First(String(User, TEXT("username")), String(User, TEXT("email"))));
            FString Duration = String(Data, TEXT("expires_in"), TEXT("24h")).TrimStartAndEnd().ToLower();
            double Seconds = 86400; const TCHAR Unit = Duration.IsEmpty() ? TEXT('h') : Duration[Duration.Len() - 1];
            if (Unit == TEXT('s') || Unit == TEXT('m') || Unit == TEXT('h') || Unit == TEXT('d')) Duration.LeftChopInline(1);
            double Amount;
            if (LexTryParseString(Amount, *Duration) && Amount > 0)
                Seconds = FMath::Min(Amount * (Unit == TEXT('s') ? 1 : Unit == TEXT('m') ? 60 : Unit == TEXT('d') ? 86400 : 3600), 31536000.0);
            ExpiresAt = FDateTime::UtcNow() + FTimespan::FromSeconds(Seconds);
        }
        else { R.Success = false; R.Error = Bool(Data, TEXT("requires_2fa_setup")) ? TEXT("Complete two-factor setup in Grimoire, then sign in again.") : TEXT("Authentication response did not contain a session."); }
    }
    Done(R);
}
void FGrimoireApiClient::Login(const FString& Email, const FString& Password, FGrimoireApiCallback Done)
{
    SignOut(); auto Body = MakeShared<FJsonObject>(); Body->SetStringField(TEXT("email"), Email); Body->SetStringField(TEXT("password"), Password);
    TWeakPtr<FGrimoireApiClient> Weak = AsShared();
    Send(TEXT("POST"), TEXT("/api/v1/auth/login"), {}, Body, false, [Weak, Done = MoveTemp(Done)](const auto& R) mutable
    { if (auto Self = Weak.Pin()) Self->AcceptSession(R, MoveTemp(Done)); });
}
void FGrimoireApiClient::Verify2FA(const FString& Code, FGrimoireApiCallback Done)
{
    auto Body = MakeShared<FJsonObject>(); Body->SetStringField(TEXT("temp_token"), TempToken); Body->SetStringField(TEXT("code"), Code);
    TWeakPtr<FGrimoireApiClient> Weak = AsShared();
    Send(TEXT("POST"), TEXT("/api/v1/auth/verify-2fa"), {}, Body, false, [Weak, Done = MoveTemp(Done)](const auto& R) mutable
    { if (auto Self = Weak.Pin()) Self->AcceptSession(R, MoveTemp(Done)); });
}
void FGrimoireApiClient::Request(const FString& Verb, const FString& Path, const TMap<FString, FString>& Query, const FObject& Body, FGrimoireApiCallback Done)
{
    if (!IsSignedIn()) { FGrimoireApiResult R; R.Error = TEXT("Sign in to Grimoire first."); Done(R); return; }
    if (FDateTime::UtcNow() + FTimespan::FromMinutes(5) < ExpiresAt) { Send(Verb, Path, Query, Body, true, MoveTemp(Done)); return; }
    TWeakPtr<FGrimoireApiClient> Weak = AsShared();
    Send(TEXT("POST"), TEXT("/api/v1/auth/refresh"), {}, nullptr, true, [Weak, Verb, Path, Query, Body, Done = MoveTemp(Done)](const auto& R) mutable
    {
        auto Self = Weak.Pin(); if (!Self) return;
        if (!R.Success) { if (R.Status == 401) Self->SignOut(); Done(R); return; }
        Self->AcceptSession(R, [Weak, Verb, Path, Query, Body, Done = MoveTemp(Done)](const auto& Session) mutable
        { if (auto S = Weak.Pin()) { if (!Session.Success) Done(Session); else S->Send(Verb, Path, Query, Body, true, MoveTemp(Done)); } });
    });
}
