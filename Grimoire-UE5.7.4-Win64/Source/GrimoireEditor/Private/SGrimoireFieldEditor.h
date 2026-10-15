#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "GrimoireData.h"
class SVerticalBox;

/** Typed controls backed by glossary JSON; no wire format exposed to designers. */
class SGrimoireFieldEditor : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGrimoireFieldEditor) {}
        SLATE_ARGUMENT(FGrimoireField, Field)
        SLATE_ARGUMENT(FString, InitialJson)
        SLATE_ARGUMENT(TFunction<void(const FString&)>, OnChanged)
    SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    static bool Supports(const FString& Kind);
private:
    FGrimoireField Field;
    TArray<TSharedPtr<FJsonValue>> Values;
    TFunction<void(const FString&)> Changed;
    TSharedPtr<SVerticalBox> Root;
    TSharedPtr<FJsonValue> DefaultValue() const;
    void Rebuild();
    void Set(int32 Index, TSharedPtr<FJsonValue> Value);
    void Notify();
};
