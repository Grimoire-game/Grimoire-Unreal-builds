#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GrimoireData.h"
#include "GrimoireSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGrimoireLanguageChanged, const FString&, Language);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FGrimoireGameValueChanged, UGrimoireDatabase*, Database, const FString&, ElementId, const FString&, ValueJson);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FGrimoireObjectValueChanged, UGrimoireDatabase*, Database, const FString&, ObjectId, const FString&, FieldId, const FString&, ValueJson);

UCLASS()
class GRIMOIRERUNTIME_API UGrimoireSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category="Grimoire") FString Language;
    UPROPERTY(BlueprintAssignable, Category="Grimoire") FGrimoireLanguageChanged OnLanguageChanged;
    UFUNCTION(BlueprintCallable, Category="Grimoire") void SetLanguage(const FString& NewLanguage);
    /** Session values are shared by dialog players and gameplay, scoped to a game. */
    UPROPERTY(BlueprintAssignable, Category="Grimoire|Variables") FGrimoireGameValueChanged OnGameVariableChanged;
    UPROPERTY(BlueprintAssignable, Category="Grimoire|Objects") FGrimoireObjectValueChanged OnObjectFieldChanged;
    /** Set a Types field using an imported element UUID or name (no JSON formatting required). */
    UFUNCTION(BlueprintCallable, Category="Grimoire|Objects") bool SetObjectMood(UGrimoireDatabase* Database, const FString& ObjectKey, const FString& Mood, const FString& FieldKey = TEXT("Mood"), const FString& VariableKey = TEXT("Mood"));
    UFUNCTION(BlueprintPure, Category="Grimoire|Variables") bool GetGameVariableJson(UGrimoireDatabase* Database, const FString& VariableIdOrName, const FString& ElementIdOrName, FString& ValueJson) const;
    UFUNCTION(BlueprintCallable, Category="Grimoire|Variables") bool SetGameVariableJson(UGrimoireDatabase* Database, const FString& VariableIdOrName, const FString& ElementIdOrName, const FString& ValueJson, bool EnforceRange = true);
    UFUNCTION(BlueprintPure, Category="Grimoire|Variables") bool GetGameNumber(UGrimoireDatabase* Database, const FString& VariableIdOrName, const FString& ElementIdOrName, double& Value) const;
    UFUNCTION(BlueprintPure, Category="Grimoire|Variables") bool GetGameBoolean(UGrimoireDatabase* Database, const FString& VariableIdOrName, const FString& ElementIdOrName, bool& Value) const;
    UFUNCTION(BlueprintPure, Category="Grimoire|Variables") bool GetGameText(UGrimoireDatabase* Database, const FString& VariableIdOrName, const FString& ElementIdOrName, FString& Value) const;
    UFUNCTION(BlueprintCallable, Category="Grimoire|Variables") bool SetGameNumber(UGrimoireDatabase* Database, const FString& VariableIdOrName, const FString& ElementIdOrName, double Value);
    UFUNCTION(BlueprintCallable, Category="Grimoire|Variables") bool SetGameBoolean(UGrimoireDatabase* Database, const FString& VariableIdOrName, const FString& ElementIdOrName, bool Value);
    UFUNCTION(BlueprintCallable, Category="Grimoire|Variables") bool SetGameText(UGrimoireDatabase* Database, const FString& VariableIdOrName, const FString& ElementIdOrName, const FString& Value);
    UFUNCTION(BlueprintCallable, Category="Grimoire|Variables") void ResetGameValues(UGrimoireDatabase* Database);
    UFUNCTION(BlueprintPure, Category="Grimoire|Objects") bool GetObjectFieldJson(UGrimoireDatabase* Database, const FString& ObjectKey, const FString& FieldKey, FString& ValueJson) const;
    UFUNCTION(BlueprintCallable, Category="Grimoire|Objects") bool SetObjectFieldJson(UGrimoireDatabase* Database, const FString& ObjectKey, const FString& FieldKey, const FString& ValueJson);
private:
    TMap<FString, FString> GameValues;
    TMap<FString, FString> ObjectValues;
    static FString GamePrefix(const UGrimoireDatabase* Database);
};
