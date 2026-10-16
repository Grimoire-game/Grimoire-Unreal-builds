#include "GrimoireSubsystem.h"
#include "GrimoireJson.h"
using namespace GrimoireJson;
void UGrimoireSubsystem::SetLanguage(const FString& NewLanguage)
{
    if (Language != NewLanguage) { Language = NewLanguage; OnLanguageChanged.Broadcast(Language); }
}

FString UGrimoireSubsystem::GamePrefix(const UGrimoireDatabase* DB)
{ return (DB->GameId.IsEmpty() ? DB->GetPathName() : DB->GameId) + TEXT("/"); }
bool UGrimoireSubsystem::GetGameVariableJson(UGrimoireDatabase* DB, const FString& Variable, const FString& Element, FString& Json) const
{
    Json.Empty(); const auto* E = DB ? DB->FindVariableElement(Variable, Element) : nullptr; if (!E) return false;
    if (const auto* V = GameValues.Find(GamePrefix(DB) + E->Id)) { Json = *V; return true; }
    const auto V = ParseValue(E->AuthoredValueJson);
    Json = V && !V->IsNull() ? E->AuthoredValueJson : WriteValue(MakeShared<FJsonValueString>(E->Id)); return true;
}
bool UGrimoireSubsystem::SetGameVariableJson(UGrimoireDatabase* DB, const FString& Variable, const FString& Element, const FString& Json, bool EnforceRange)
{
    const auto* E = DB ? DB->FindVariableElement(Variable, Element) : nullptr; auto V = ParseValue(Json); if (!E || !V) return false;
    if (EnforceRange && E->HasRange && V->Type == EJson::Number)
        V = MakeShared<FJsonValueNumber>(FMath::Clamp(V->AsNumber(), E->RangeMin, E->RangeMax));
    const FString ValueJson = WriteValue(V); GameValues.Add(GamePrefix(DB) + E->Id, ValueJson);
    OnGameVariableChanged.Broadcast(DB, E->Id, ValueJson); return true;
}
bool UGrimoireSubsystem::GetGameNumber(UGrimoireDatabase* DB, const FString& Variable, const FString& Element, double& Out) const
{ Out = 0; FString Json; if (!GetGameVariableJson(DB, Variable, Element, Json)) return false; auto V = ParseValue(Json); return V && V->TryGetNumber(Out); }
bool UGrimoireSubsystem::GetGameBoolean(UGrimoireDatabase* DB, const FString& Variable, const FString& Element, bool& Out) const
{ Out = false; FString Json; if (!GetGameVariableJson(DB, Variable, Element, Json)) return false; auto V = ParseValue(Json); return V && V->TryGetBool(Out); }
bool UGrimoireSubsystem::GetGameText(UGrimoireDatabase* DB, const FString& Variable, const FString& Element, FString& Out) const
{ Out.Empty(); FString Json; if (!GetGameVariableJson(DB, Variable, Element, Json)) return false; auto V = ParseValue(Json); return V && V->TryGetString(Out); }
bool UGrimoireSubsystem::SetGameNumber(UGrimoireDatabase* DB, const FString& Variable, const FString& Element, double V)
{ return FMath::IsFinite(V) && SetGameVariableJson(DB, Variable, Element, WriteValue(MakeShared<FJsonValueNumber>(V))); }
bool UGrimoireSubsystem::SetGameBoolean(UGrimoireDatabase* DB, const FString& Variable, const FString& Element, bool V)
{ return SetGameVariableJson(DB, Variable, Element, V ? TEXT("true") : TEXT("false")); }
bool UGrimoireSubsystem::SetGameText(UGrimoireDatabase* DB, const FString& Variable, const FString& Element, const FString& V)
{ return SetGameVariableJson(DB, Variable, Element, WriteValue(MakeShared<FJsonValueString>(V))); }
void UGrimoireSubsystem::ResetGameValues(UGrimoireDatabase* DB)
{
    if (!DB) return; const FString Prefix = GamePrefix(DB);
    for (auto It = GameValues.CreateIterator(); It; ++It) if (It.Key().StartsWith(Prefix)) It.RemoveCurrent();
    for (auto It = ObjectValues.CreateIterator(); It; ++It) if (It.Key().StartsWith(Prefix)) It.RemoveCurrent();
    for (const auto& Variable : DB->GameVariables) for (const auto& E : Variable.Elements)
    { FString Json; GetGameVariableJson(DB, Variable.Id, E.Id, Json); OnGameVariableChanged.Broadcast(DB, E.Id, Json); }
    for (const auto& Object : DB->Objects) for (const auto& Field : Object.Fields)
        OnObjectFieldChanged.Broadcast(DB, Object.Id, Field.Id, Field.StoredValueJson());
}
bool UGrimoireSubsystem::GetObjectFieldJson(UGrimoireDatabase* DB, const FString& ObjectKey, const FString& FieldKey, FString& Json) const
{
    Json.Empty(); const auto* O = DB ? DB->FindObject(ObjectKey) : nullptr; const auto* F = O ? O->FindField(FieldKey) : nullptr;
    if (!F) return false;
    const auto* V = ObjectValues.Find(GamePrefix(DB) + O->Id + TEXT("/") + F->Id);
    Json = V ? *V : F->StoredValueJson(); return true;
}
bool UGrimoireSubsystem::SetObjectFieldJson(UGrimoireDatabase* DB, const FString& ObjectKey, const FString& FieldKey, const FString& Json)
{
    const auto* O = DB ? DB->FindObject(ObjectKey) : nullptr; const auto* F = O ? O->FindField(FieldKey) : nullptr; auto V = ParseValue(Json);
    if (!F || !V) return false;
    const FString ValueJson = WriteValue(V);
    ObjectValues.Add(GamePrefix(DB) + O->Id + TEXT("/") + F->Id, ValueJson);
    OnObjectFieldChanged.Broadcast(DB, O->Id, F->Id, ValueJson); return true;
}
bool UGrimoireSubsystem::SetObjectMood(UGrimoireDatabase* DB, const FString& ObjectKey, const FString& Mood, const FString& FieldKey, const FString& VariableKey)
{
    const auto* Element = DB ? DB->FindVariableElement(VariableKey, Mood.TrimStartAndEnd()) : nullptr;
    return Element && SetObjectFieldJson(DB, ObjectKey, FieldKey, WriteValue(MakeShared<FJsonValueString>(Element->Id)));
}
