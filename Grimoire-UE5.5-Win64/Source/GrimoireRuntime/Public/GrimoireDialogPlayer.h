#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrimoireData.h"
#include "GrimoireDialogPlayer.generated.h"

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireDialogChoice
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") int32 Index = 0;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FText Text;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString OptionId;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString StringId;
};
USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireDialogLine
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString NodeId;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString DialogId;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString Language;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") bool IsContext = false;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FGrimoireDialogNodeData Node;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString SpeakerId;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FText Speaker;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FText Text;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString VoiceUrl;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") bool IsLast = false;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireDialogChoice> Choices;
};
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGrimoireLineEvent, const FGrimoireDialogLine&, Line);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGrimoireDialogEnded, const FString&, Reason);

/** Offline graph runner; add to an Actor and bind OnLine/OnChoices from UMG. */
UCLASS(Blueprintable, ClassGroup=(Grimoire), meta=(BlueprintSpawnableComponent))
class GRIMOIRERUNTIME_API UGrimoireDialogPlayer : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") TObjectPtr<UGrimoireDatabase> Database;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") FString Language;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") bool KeepVariablesBetweenRuns = false;
    /** Display context notes without a speaker; Next continues past them. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") bool ShowContextNotes = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") bool WarnAboutMissingData = true;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") TArray<FGrimoireDialogRequirement> LastRequirements;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") bool IsPlaying = false;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString CurrentDialogId;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString LastEndReason;
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FGrimoireDialogLine CurrentLine;
    UPROPERTY(BlueprintAssignable, Category="Grimoire") FGrimoireLineEvent OnLine;
    UPROPERTY(BlueprintAssignable, Category="Grimoire") FGrimoireLineEvent OnChoices;
    UPROPERTY(BlueprintAssignable, Category="Grimoire") FGrimoireDialogEnded OnDialogEnded;

    UFUNCTION(BlueprintCallable, Category="Grimoire") bool StartDialog(const FString& DialogIdOrKey, const FString& StartNode = TEXT(""));
    UFUNCTION(BlueprintCallable, Category="Grimoire") void Next();
    UFUNCTION(BlueprintCallable, Category="Grimoire") void Choose(int32 Index);
    UFUNCTION(BlueprintCallable, Category="Grimoire") void Stop();
    UFUNCTION(BlueprintCallable, Category="Grimoire") void RefreshLanguage();
    UFUNCTION(BlueprintCallable, Category="Grimoire") bool SetVariableJson(const FString& DialogIdOrKey, const FString& Name, const FString& ValueJson);
    UFUNCTION(BlueprintPure, Category="Grimoire") FString GetVariableJson(const FString& DialogIdOrKey, const FString& Name) const;
    UFUNCTION(BlueprintCallable, Category="Grimoire") void SetNumber(const FString& DialogIdOrKey, const FString& Name, double Value);
    UFUNCTION(BlueprintCallable, Category="Grimoire") void SetBoolean(const FString& DialogIdOrKey, const FString& Name, bool Value);
    UFUNCTION(BlueprintCallable, Category="Grimoire") void SetText(const FString& DialogIdOrKey, const FString& Name, const FString& Value);
    UFUNCTION(BlueprintCallable, Category="Grimoire") void ResetVariables();
    UFUNCTION(BlueprintPure, Category="Grimoire|Dialogs") bool GetCurrentNode(FGrimoireDialogNodeData& NodeData) const;
    UFUNCTION(BlueprintPure, Category="Grimoire|Dialogs") TArray<FGrimoireDialogRequirement> GetDialogRequirements(const FString& DialogIdOrKey) const;

    /** Return a JSON primitive/array; empty means use the imported object-field fallback. */
    UFUNCTION(BlueprintNativeEvent, Category="Grimoire|External Values") FString ReadExternalValue(const FString& ReferenceJson);
    virtual FString ReadExternalValue_Implementation(const FString& ReferenceJson);
    /** Return true when your save-game/gameplay system handled the write. */
    UFUNCTION(BlueprintNativeEvent, Category="Grimoire|External Values") bool WriteExternalValue(const FString& ReferenceJson, const FString& ValueJson);
    virtual bool WriteExternalValue_Implementation(const FString& ReferenceJson, const FString& ValueJson);

    static bool CompareValues(const TSharedPtr<FJsonValue>& Left, const TSharedPtr<FJsonValue>& Right, const FString& Operator);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TSharedPtr<FJsonObject> Graph;
    TSharedPtr<FJsonObject> Node;
    TArray<TSharedPtr<FJsonObject>> VisibleOptions;
    TMap<FString, TSharedPtr<FJsonValue>> Variables;
    TMap<FString, TSharedPtr<FJsonValue>> ObjectOverrides;
    TMap<FString, TSharedPtr<FJsonValue>> ElementOverrides;
    TSet<FString> Picked;
    UFUNCTION() void LanguageChanged(const FString& NewLanguage);
    bool LoadDialog(const FString& Id);
    bool Run(const FString& Target);
    bool ResolveLink(const TSharedPtr<FJsonObject>& Link, FString& Target, const FString& Suffix = TEXT(""));
    void Follow(const TSharedPtr<FJsonObject>& Link);
    void Show();
    void Finish(const FString& Reason);
    TSharedPtr<FJsonValue> Variable(const FString& Name) const;
    TSharedPtr<FJsonValue> Resolve(const TSharedPtr<FJsonObject>& Ref);
    bool Evaluate(const TSharedPtr<FJsonObject>& Condition);
    void Apply(const TSharedPtr<FJsonObject>& Action);
    FString EffectiveLanguage() const;
    FText Localize(const TSharedPtr<FJsonObject>& Entry, const FString& TextKey);
};
