#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrimoireData.h"
#include "GrimoireObjectLink.generated.h"

UCLASS(ClassGroup=(Grimoire), meta=(BlueprintSpawnableComponent))
class GRIMOIRERUNTIME_API UGrimoireObjectLink : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") TObjectPtr<UGrimoireDatabase> Database;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") FString ObjectKey;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") FString PreviewLanguage;
    UPROPERTY(EditAnywhere, Category="Grimoire|Sync") bool SyncPosition = true;
    UPROPERTY(EditAnywhere, Category="Grimoire|Sync") bool SyncRotation = true;
    UPROPERTY(EditAnywhere, Category="Grimoire|Sync") bool SyncScale = true;
    UFUNCTION(BlueprintPure, Category="Grimoire") bool GetObject(FGrimoireObject& Object) const;
    UFUNCTION(BlueprintPure, Category="Grimoire") bool GetField(const FString& FieldIdOrLabel, FGrimoireField& Field) const;
    UFUNCTION(BlueprintPure, Category="Grimoire") bool GetNumber(const FString& FieldIdOrLabel, double& Value, int32 Index = 0) const;
    UFUNCTION(BlueprintPure, Category="Grimoire") bool GetBoolean(const FString& FieldIdOrLabel, bool& Value, int32 Index = 0) const;
    UFUNCTION(BlueprintPure, Category="Grimoire") bool GetVector(const FString& FieldIdOrLabel, FVector& Value, int32 Index = 0) const;
    UFUNCTION(BlueprintPure, Category="Grimoire") bool GetText(const FString& FieldIdOrLabel, FText& Value, int32 Index = 0) const;
    UFUNCTION(BlueprintPure, Category="Grimoire") bool GetReferencedObject(const FString& FieldIdOrLabel, FGrimoireObject& Object, int32 Index = 0) const;
    UFUNCTION(BlueprintPure, Category="Grimoire|Variables") bool GetVariableElement(const FString& FieldIdOrLabel, FGrimoireVariableElement& Element, int32 Index = 0) const;
private:
    const FGrimoireField* FindField(const FString& Key) const;
};
