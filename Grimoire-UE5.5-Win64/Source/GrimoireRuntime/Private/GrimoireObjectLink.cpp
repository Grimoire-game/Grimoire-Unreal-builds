#include "GrimoireObjectLink.h"
#include "GrimoireSubsystem.h"
#include "GrimoireJson.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

const FGrimoireField* UGrimoireObjectLink::FindField(const FString& Key) const
{ const auto* O = Database ? Database->FindObject(ObjectKey) : nullptr; return O ? O->FindField(Key) : nullptr; }
bool UGrimoireObjectLink::GetObject(FGrimoireObject& Object) const
{
    Object = FGrimoireObject(); if (!Database || !Database->GetObject(ObjectKey, Object)) return false;
    for (auto& F : Object.Fields) { FGrimoireField Current; if (GetField(F.Id, Current)) F = MoveTemp(Current); }
    return true;
}
bool UGrimoireObjectLink::GetField(const FString& Key, FGrimoireField& Field) const
{
    using namespace GrimoireJson;
    Field = FGrimoireField(); if (!Database || !Database->GetField(ObjectKey, Key, Field)) return false;
    auto* Game = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    auto* State = Game ? Game->GetSubsystem<UGrimoireSubsystem>() : nullptr; FString Json;
    if (!State || !State->GetObjectFieldJson(Database, ObjectKey, Key, Json) || Json == Field.StoredValueJson()) return true;
    auto Root = ParseValue(Json); if (!Root) return true;
    TArray<TSharedPtr<FJsonValue>> Values = Field.Multiple && Root->Type == EJson::Array ? Root->AsArray() : TArray<TSharedPtr<FJsonValue>>{ Root };
    Field.Values.Empty(); Field.Plain.Empty();
    for (const auto& V : Values)
    {
        if (!V || V->IsNull()) continue;
        FGrimoireFieldValue Item; Item.ValueJson = WriteValue(V);
        Item.Text = V->Type == EJson::String ? V->AsString() : Item.ValueJson;
        if (V->Type == EJson::Number) Item.Number = V->AsNumber();
        if (V->Type == EJson::Boolean) Item.Boolean = V->AsBool();
        if (Field.Kind == TEXT("reference") && V->Type == EJson::String) Item.Reference.Id = V->AsString();
        if (Field.Kind == TEXT("vector")) { auto O = AsObject(V); Item.Vector = FVector(Number(O, TEXT("x")), Number(O, TEXT("y")), Number(O, TEXT("z"))); }
        Field.Values.Add(MoveTemp(Item));
    }
    return true;
}
bool UGrimoireObjectLink::GetNumber(const FString& Key, double& Value, int32 Index) const
{ Value = 0; FGrimoireField F; if (!GetField(Key, F) || F.Kind != TEXT("number") || !F.Values.IsValidIndex(Index)) return false; Value = F.Values[Index].Number; return true; }
bool UGrimoireObjectLink::GetBoolean(const FString& Key, bool& Value, int32 Index) const
{ Value = false; FGrimoireField F; if (!GetField(Key, F) || F.Kind != TEXT("boolean") || !F.Values.IsValidIndex(Index)) return false; Value = F.Values[Index].Boolean; return true; }
bool UGrimoireObjectLink::GetVector(const FString& Key, FVector& Value, int32 Index) const
{ Value = FVector::ZeroVector; FGrimoireField F; if (!GetField(Key, F) || F.Kind != TEXT("vector") || !F.Values.IsValidIndex(Index)) return false; Value = F.Values[Index].Vector; return true; }
bool UGrimoireObjectLink::GetText(const FString& Key, FText& Value, int32 Index) const
{
    Value = FText::GetEmpty(); FGrimoireField Field; if (!GetField(Key, Field) || Index < 0) return false; const auto* F = &Field;
    if (F->Values.IsEmpty() && Index == 0) { Value = FText::FromString(F->Plain); return true; }
    if (!F->Values.IsValidIndex(Index)) return false;
    const auto& V = F->Values[Index]; FString Language = GetWorld() && !GetWorld()->IsGameWorld() ? PreviewLanguage : FString();
    if (Language.IsEmpty() && GetWorld() && GetWorld()->GetGameInstance())
        if (auto* Sub = GetWorld()->GetGameInstance()->GetSubsystem<UGrimoireSubsystem>()) Language = Sub->Language;
    Value = !V.StringId.IsEmpty() && Database->FindString(V.StringId) ? Database->GetText(V.StringId, Language) : FText::FromString(V.Text);
    return true;
}
bool UGrimoireObjectLink::GetReferencedObject(const FString& Key, FGrimoireObject& Object, int32 Index) const
{
    Object = FGrimoireObject(); FGrimoireField F;
    return GetField(Key, F) && F.Kind == TEXT("reference") && F.Values.IsValidIndex(Index) && Database->GetObject(F.Values[Index].Reference.Id, Object);
}
bool UGrimoireObjectLink::GetVariableElement(const FString& Key, FGrimoireVariableElement& Element, int32 Index) const
{
    Element = FGrimoireVariableElement(); FGrimoireField Field;
    if (!GetField(Key, Field) || !Field.Values.IsValidIndex(Index)) return false;
    const auto& V = Field.Values[Index]; auto Raw = GrimoireJson::ParseValue(V.ValueJson);
    const FString Id = !V.Reference.Id.IsEmpty() ? V.Reference.Id : Raw && Raw->Type == EJson::String ? Raw->AsString() : FString();
    return Database->GetVariableElement(TEXT(""), Id, Element);
}
