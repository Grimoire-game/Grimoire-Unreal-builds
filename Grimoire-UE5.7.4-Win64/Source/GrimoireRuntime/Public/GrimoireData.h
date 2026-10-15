#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Dom/JsonObject.h"
#include "GrimoireData.generated.h"

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireReference
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString CodeId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Kind;
};

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireFieldValue
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Text;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") double Number = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") bool Boolean = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FVector Vector = FVector::ZeroVector;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FGrimoireReference Reference;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString StringId;
    // Stored glossary value, suitable as base_value for an engine commit.
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString ValueJson;
};

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireField
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Label;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Section;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Kind;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Plain;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") bool Multiple = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") bool Editable = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireFieldValue> Values;
    FString StoredValueJson() const;
};

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireObject
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString CodeId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString TemplateName;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireField> Fields;
#if WITH_EDITORONLY_DATA
    // Editor-only task/authoring metadata must not be cooked into a game.
    UPROPERTY() FString DocumentJson;
#endif
    const FGrimoireField* FindField(const FString& IdOrLabel) const;
};

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireTranslation
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Language;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Text;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString VoiceUrl;
};

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireString
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Key;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Source;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString VoiceUrl;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireTranslation> Translations;
};

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireDialog
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Key;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString GraphJson;
};

/** A game variable element. AuthoredValueJson is immutable imported data. */
USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireVariableElement
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Description;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString StringId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString AuthoredValueJson;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") bool HasRange = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") double RangeMin = 0;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") double RangeMax = 0;
};

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireGameVariable
{
    GENERATED_BODY()
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Id;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Name;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString ElementType;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Description;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString Group;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireVariableElement> Elements;
};

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireDialogOptionData
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString Id;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString SourceText;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString StringId;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString NextNode;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString NextDialog;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString NextDialogNode;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") bool AlwaysAvailable = false;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireTranslation> Translations;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString VisibilityConditionJson;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString VariableActionJson;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString RawJson;
};

/** Copy of the authored node, including hidden options and complete extension data. */
USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireDialogNodeData
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString DialogId;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString Id;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString Identifier;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString Name;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString Type;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString SourceText;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString StringId;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString SpeakerId;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString VoiceUrl;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString NextNode;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString NextDialog;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString NextDialogNode;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") bool PickOnce = false;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireTranslation> Translations;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireDialogOptionData> Options;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString ConditionJson;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString SetterJson;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") TArray<FString> ConditionBranchesJson;
    /** All top-level fields as JSON values, including custom metadata. */
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") TMap<FString, FString> JsonFields;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString RawJson;
};

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireDialogRequirement
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString Kind;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString ResourceId;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString FieldId;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString Name;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString UsedBy;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") bool Missing = false;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") bool InScene = false;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") bool UsesImportedFallback = false;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString Message;
};

/** Imported game snapshot. Reference this asset from an Actor/Widget so it is cooked. No credentials. */
UCLASS(BlueprintType)
class GRIMOIRERUNTIME_API UGrimoireDatabase : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString GameId;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString GameName;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FString SourceLanguage;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") TArray<FString> Languages;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FDateTime ImportedAt;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireObject> Objects;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireString> Strings;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireDialog> Dialogs;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireGameVariable> GameVariables;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") bool VariablesImported = false;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") FDateTime VariablesImportedAt;

    UFUNCTION(BlueprintPure, Category="Grimoire|Variables") bool GetGameVariable(const FString& IdOrName, FGrimoireGameVariable& Variable) const;
    /** Pass an empty VariableIdOrName to look up an element by its global UUID. */
    UFUNCTION(BlueprintPure, Category="Grimoire|Variables") bool GetVariableElement(const FString& VariableIdOrName, const FString& ElementIdOrName, FGrimoireVariableElement& Element) const;
    UFUNCTION(BlueprintPure, Category="Grimoire|Dialogs") bool GetDialogNode(const FString& DialogIdOrKey, const FString& NodeIdOrIdentifier, FGrimoireDialogNodeData& Node) const;
    UFUNCTION(BlueprintPure, Category="Grimoire|Dialogs") TArray<FGrimoireDialogNodeData> GetDialogNodes(const FString& DialogIdOrKey) const;
    UFUNCTION(BlueprintPure, Category="Grimoire|Dialogs", meta=(WorldContext="WorldContextObject")) TArray<FGrimoireDialogRequirement> GetDialogRequirements(const FString& DialogIdOrKey, const UObject* WorldContextObject) const;
    const FGrimoireGameVariable* FindGameVariable(const FString& IdOrName) const;
    const FGrimoireVariableElement* FindVariableElement(const FString& VariableIdOrName, const FString& ElementIdOrName) const;
    static bool ParseGameVariable(const TSharedPtr<FJsonObject>& Resource, FGrimoireGameVariable& Out, FString& Error);
    static FGrimoireDialogNodeData ParseNodeData(const FString& DialogId, const TSharedPtr<FJsonObject>& Node);

    UFUNCTION(BlueprintPure, Category="Grimoire") bool GetObject(const FString& IdOrKey, FGrimoireObject& Object) const;
    UFUNCTION(BlueprintPure, Category="Grimoire") bool GetField(const FString& ObjectKey, const FString& FieldIdOrLabel, FGrimoireField& Field) const;
    UFUNCTION(BlueprintPure, Category="Grimoire") FText GetText(const FString& StringIdOrKey, const FString& Language) const;
    UFUNCTION(BlueprintPure, Category="Grimoire") bool HasTranslation(const FString& StringIdOrKey, const FString& Language) const;
    const FGrimoireObject* FindObject(const FString& IdOrKey) const;
    const FGrimoireDialog* FindDialog(const FString& IdOrKey) const;
    const FGrimoireString* FindString(const FString& IdOrKey) const;
    FString GetVoiceUrl(const FString& StringIdOrKey, const FString& Language) const;

    static bool ParseObject(const TSharedPtr<FJsonObject>& Document, FGrimoireObject& Out, FString& Error);
    static bool ParseString(const TSharedPtr<FJsonObject>& Resource, FGrimoireString& Out);
    static bool ParseDialog(const TSharedPtr<FJsonObject>& Resource, FGrimoireDialog& Out, FString& Error);
};
