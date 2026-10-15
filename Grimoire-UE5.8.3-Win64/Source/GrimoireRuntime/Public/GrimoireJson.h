#pragma once
#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

// Tolerant readers for optional API fields. Never assert on absent/null payloads.
namespace GrimoireJson
{
    using FObject = TSharedPtr<FJsonObject>;
    inline FString String(const FObject& O, const FString& Key, const FString& Default = TEXT(""))
    { FString V; return O && O->TryGetStringField(Key, V) ? V : Default; }
    inline bool Bool(const FObject& O, const FString& Key, bool Default = false)
    { bool V; return O && O->TryGetBoolField(Key, V) ? V : Default; }
    inline double Number(const FObject& O, const FString& Key, double Default = 0)
    { double V; return O && O->TryGetNumberField(Key, V) ? V : Default; }
    inline FObject Object(const FObject& O, const FString& Key)
    { const FObject* V = nullptr; return O && O->TryGetObjectField(Key, V) ? *V : nullptr; }
    inline TArray<TSharedPtr<FJsonValue>> Array(const FObject& O, const FString& Key)
    { const TArray<TSharedPtr<FJsonValue>>* V = nullptr; return O && O->TryGetArrayField(Key, V) ? *V : TArray<TSharedPtr<FJsonValue>>(); }
    inline FObject AsObject(const TSharedPtr<FJsonValue>& V)
    { return V && V->Type == EJson::Object ? V->AsObject() : nullptr; }
    inline TSharedPtr<FJsonValue> Value(const FObject& O, const FString& Key)
    { return O ? O->TryGetField(Key) : nullptr; }
    inline FObject Parse(const FString& Text)
    { FObject O; FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O); return O; }
    inline FString Write(const FObject& O)
    { FString Text; if (O) FJsonSerializer::Serialize(O.ToSharedRef(), TJsonWriterFactory<>::Create(&Text)); return Text; }
    inline FString WriteValue(const TSharedPtr<FJsonValue>& V)
    {
        if (!V) return TEXT("null");
        // UE's root-value overload expects container state for scalar values.
        // Serialize a single-element array, then remove only its outer brackets.
        FString Text;
        FJsonSerializer::Serialize(TArray<TSharedPtr<FJsonValue>>{V}, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
        return Text.Mid(1, Text.Len() - 2);
    }
    inline TSharedPtr<FJsonValue> ParseValue(const FString& Text)
    { auto Wrapper = Parse(TEXT("{\"v\":") + Text + TEXT("}")); return Value(Wrapper, TEXT("v")); }
    inline FString First(const FString& A, const FString& B) { return A.IsEmpty() ? B : A; }
}
