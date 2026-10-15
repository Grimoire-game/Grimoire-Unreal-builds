#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrimoireClickToTalk.generated.h"

class UGrimoireDatabase;
class AGrimoireDialogActor;
class APlayerController;
class APawn;

/** Add to a top-down PlayerController Blueprint. Ground-click movement remains in that Blueprint. */
UCLASS(ClassGroup=(Grimoire), meta=(BlueprintSpawnableComponent))
class GRIMOIRERUNTIME_API UGrimoireClickToTalk : public UActorComponent
{
    GENERATED_BODY()
public:
    UGrimoireClickToTalk();
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire|Interaction") TObjectPtr<UGrimoireDatabase> Database;
    /** UUID or Code ID of the NPC's Grimoire Object Link. The controlled pawn is always excluded. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire|Interaction") FString TargetObjectKey;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire|Interaction") FString DialogKey;
    /** Optional. Otherwise finds the level's dialog actor for this database, or creates one. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire|Interaction") TObjectPtr<AGrimoireDialogActor> DialogActor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire|Interaction", meta=(ClampMin="50", Units="cm")) float InteractionDistance = 180;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire|Interaction", meta=(ClampMin="1", Units="s")) float ApproachTimeout = 30;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire|Interaction") bool HandleMouseClicks = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire|Interaction") bool RequireLineOfSight = true;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Grimoire|Interaction") TObjectPtr<AActor> PendingTarget;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Grimoire|Interaction") FString LastInteractionStatus;
    UFUNCTION(BlueprintCallable, Category="Grimoire|Interaction") bool RequestTalkTo(AActor* Target);
    UFUNCTION(BlueprintCallable, Category="Grimoire|Interaction") void CancelApproach();
    UFUNCTION(BlueprintPure, Category="Grimoire|Interaction") bool IsTalkTarget(AActor* Target) const;
    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    TWeakObjectPtr<APlayerController> LockedController;
    TWeakObjectPtr<APawn> ApproachPawn;
    FVector LastPawnLocation = FVector::ZeroVector;
    float Elapsed = 0, StationaryTime = 0;
    bool DialogWasPlaying = false;
    APlayerController* Controller() const;
    bool EnsureDialogActor();
    bool CanTalkNow(AActor* Target) const;
    void MoveToTarget();
    void StartConversation();
    void LockMovement(bool Lock);
    void StopPawn();
};
