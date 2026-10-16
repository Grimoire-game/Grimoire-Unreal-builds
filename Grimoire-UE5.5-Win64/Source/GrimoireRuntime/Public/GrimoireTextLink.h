#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Components/TextBlock.h"
#include "GrimoireData.h"
#include "GrimoireTextLink.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FGrimoireTextChanged, const FText&, Text);

/** Drives TextRender components on its Actor, or custom UI through OnTextChanged. */
UCLASS(ClassGroup=(Grimoire), meta=(BlueprintSpawnableComponent))
class GRIMOIRERUNTIME_API UGrimoireTextLink : public UActorComponent
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") TObjectPtr<UGrimoireDatabase> Database;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") FString StringKey;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") FString PreviewLanguage;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") bool UpdateTextRender = true;
    UPROPERTY(BlueprintAssignable, Category="Grimoire") FGrimoireTextChanged OnTextChanged;
    UFUNCTION(BlueprintPure, Category="Grimoire") FText GetLocalizedText() const;
    UFUNCTION(BlueprintCallable, CallInEditor, Category="Grimoire") void RefreshText();
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UFUNCTION() void LanguageChanged(const FString& Language);
};

/** Drop into any UMG Widget Blueprint and select the imported database and string key. */
UCLASS(meta=(DisplayName="Grimoire Text"))
class GRIMOIRERUNTIME_API UGrimoireTextBlock : public UTextBlock
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") TObjectPtr<UGrimoireDatabase> Database;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") FString StringKey;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") FString LanguageOverride;
    UFUNCTION(BlueprintCallable, Category="Grimoire") void RefreshText();
    virtual void SynchronizeProperties() override;
    virtual void ReleaseSlateResources(bool bReleaseChildren) override;
private:
    UFUNCTION() void LanguageChanged(const FString& Language);
};
