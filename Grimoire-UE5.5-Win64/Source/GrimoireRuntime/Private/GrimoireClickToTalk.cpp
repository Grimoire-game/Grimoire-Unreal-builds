#include "GrimoireClickToTalk.h"
#include "GrimoireData.h"
#include "GrimoireObjectLink.h"
#include "GrimoireDialogActor.h"
#include "Blueprint/AIBlueprintHelperLibrary.h"
#include "Navigation/PathFollowingComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"

UGrimoireClickToTalk::UGrimoireClickToTalk()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
}
APlayerController* UGrimoireClickToTalk::Controller() const { return Cast<APlayerController>(GetOwner()); }
void UGrimoireClickToTalk::BeginPlay()
{
    Super::BeginPlay();
    if (auto* PC = Controller()) AddTickPrerequisiteActor(PC);
    else { LastInteractionStatus = TEXT("Add Grimoire Click To Talk to a PlayerController Blueprint."); SetComponentTickEnabled(false); }
}
bool UGrimoireClickToTalk::IsTalkTarget(AActor* Target) const
{
    auto* PC = Controller();
    if (!IsValid(Target) || !PC || Target == PC->GetPawn() || !Database) return false;
    const auto* Expected = Database->FindObject(TargetObjectKey); if (!Expected) return false;
    TArray<UGrimoireObjectLink*> Links; Target->GetComponents(Links);
    for (const auto* Link : Links)
    {
        if (!Link->Database || (Link->Database != Database && (Database->GameId.IsEmpty() || Link->Database->GameId != Database->GameId))) continue;
        const auto* Object = Link->Database->FindObject(Link->ObjectKey);
        if (Object && Object->Id == Expected->Id) return true;
    }
    return false;
}
bool UGrimoireClickToTalk::EnsureDialogActor()
{
    if (!Database || !Database->FindDialog(DialogKey)) { LastInteractionStatus = TEXT("Dialog is not imported, or Dialog Key is incorrect."); return false; }
    if (IsValid(DialogActor))
    {
        if (DialogActor->Player->Database == Database) return true;
        LastInteractionStatus = TEXT("The assigned dialog actor uses a different database."); return false;
    }
    for (TActorIterator<AGrimoireDialogActor> It(GetWorld()); It; ++It)
        if (It->Player->Database == Database) { DialogActor = *It; return true; }
    DialogActor = GetWorld()->SpawnActor<AGrimoireDialogActor>();
    if (!DialogActor) { LastInteractionStatus = TEXT("Could not create the dialog actor."); return false; }
    DialogActor->Player->Database = Database; return true;
}
bool UGrimoireClickToTalk::CanTalkNow(AActor* Target) const
{
    auto* PC = Controller(); APawn* Pawn = PC ? PC->GetPawn() : nullptr;
    if (!Pawn || !IsValid(Target)) return false;
    if (FVector::Dist(Pawn->GetActorLocation(), Target->GetActorLocation()) > FMath::Max(50.f, InteractionDistance)) return false;
    if (!RequireLineOfSight) return true;
    FCollisionQueryParams Params(SCENE_QUERY_STAT(GrimoireTalk), false, Pawn); Params.AddIgnoredActor(Target);
    return !GetWorld()->LineTraceTestByChannel(Pawn->GetActorLocation(), Target->GetActorLocation(), ECC_Visibility, Params);
}
bool UGrimoireClickToTalk::RequestTalkTo(AActor* Target)
{
    auto* PC = Controller();
    if (!PC || !PC->IsLocalController() || !PC->GetPawn() || !IsTalkTarget(Target) || !EnsureDialogActor()) return false;
    if (DialogActor->Player->IsPlaying) return false;
    CancelApproach(); PendingTarget = Target; ApproachPawn = PC->GetPawn();
    LastPawnLocation = ApproachPawn->GetActorLocation(); Elapsed = StationaryTime = 0;
    LastInteractionStatus = TEXT("Walking to the dialog target.");
    if (auto* Movement = ApproachPawn->GetMovementComponent()) Movement->AddTickPrerequisiteComponent(this);
    if (CanTalkNow(Target)) StartConversation(); else MoveToTarget();
    return true;
}
void UGrimoireClickToTalk::MoveToTarget()
{
    auto* PC = Controller(); if (!PC || !IsValid(PendingTarget)) return;
    // Controller input has already run. Clear its held-click direction before navigation supplies its own.
    if (PC->GetPawn()) PC->GetPawn()->ConsumeMovementInputVector();
    UAIBlueprintHelperLibrary::SimpleMoveToActor(PC, PendingTarget);
    if (auto* Path = PC->FindComponentByClass<UPathFollowingComponent>()) Path->AddTickPrerequisiteComponent(this);
}
void UGrimoireClickToTalk::StopPawn()
{
    if (auto* PC = Controller())
    {
        PC->StopMovement();
        if (APawn* Pawn = PC->GetPawn()) { Pawn->ConsumeMovementInputVector(); if (auto* Movement = Pawn->GetMovementComponent()) Movement->StopMovementImmediately(); }
    }
}
void UGrimoireClickToTalk::LockMovement(bool Lock)
{
    if (Lock)
    { if (!LockedController.IsValid()) if (auto* PC = Controller()) { PC->SetIgnoreMoveInput(true); LockedController = PC; } }
    else if (auto* PC = LockedController.Get()) { PC->SetIgnoreMoveInput(false); LockedController.Reset(); }
}
void UGrimoireClickToTalk::CancelApproach()
{
    if (PendingTarget) { StopPawn(); LastInteractionStatus = TEXT("Approach cancelled."); }
    PendingTarget = nullptr; ApproachPawn.Reset(); Elapsed = StationaryTime = 0;
}
void UGrimoireClickToTalk::StartConversation()
{
    auto* PC = Controller(); AActor* Target = PendingTarget;
    StopPawn(); CancelApproach();
    if (PC && PC->GetPawn() && IsValid(Target))
    { FRotator Facing = (Target->GetActorLocation() - PC->GetPawn()->GetActorLocation()).Rotation(); Facing.Pitch = Facing.Roll = 0; PC->GetPawn()->SetActorRotation(Facing); }
    LockMovement(true);
    const bool Started = IsValid(DialogActor) && DialogActor->StartDialog(DialogKey);
    DialogWasPlaying = IsValid(DialogActor) && DialogActor->Player->IsPlaying;
    if (!DialogWasPlaying) LockMovement(false);
    LastInteractionStatus = Started ? TEXT("Dialog started.") : TEXT("Dialog could not start. Check the dialog requirements.");
}
void UGrimoireClickToTalk::TickComponent(float DT, ELevelTick Type, FActorComponentTickFunction* Tick)
{
    Super::TickComponent(DT, Type, Tick);
    auto* PC = Controller(); if (!PC || !PC->IsLocalController()) return;
    const bool Playing = IsValid(DialogActor) && DialogActor->Player->IsPlaying;
    if (Playing) { DialogWasPlaying = true; LockMovement(true); StopPawn(); return; }
    if (DialogWasPlaying)
    {
        DialogWasPlaying = false; StopPawn(); LockMovement(false);
        LastInteractionStatus = TEXT("Dialog ended. Click the ground to move or the character to talk again."); return;
    }
    if (HandleMouseClicks && PC->WasInputKeyJustPressed(EKeys::LeftMouseButton))
    {
        FHitResult Hit; PC->GetHitResultUnderCursorByChannel(UEngineTypes::ConvertToTraceType(ECC_Visibility), false, Hit);
        if (IsTalkTarget(Hit.GetActor())) RequestTalkTo(Hit.GetActor()); else CancelApproach();
    }
    if (!PendingTarget) return;
    if (!IsValid(PendingTarget) || !ApproachPawn.IsValid() || PC->GetPawn() != ApproachPawn.Get()) { CancelApproach(); return; }
    // Run before path following and pawn movement, so only the navigation direction survives a held click.
    if (HandleMouseClicks && PC->IsInputKeyDown(EKeys::LeftMouseButton)) PC->GetPawn()->ConsumeMovementInputVector();
    if (CanTalkNow(PendingTarget)) { StartConversation(); return; }
    Elapsed += DT;
    const FVector Position = ApproachPawn->GetActorLocation();
    StationaryTime = FVector::DistSquared(Position, LastPawnLocation) < 0.25f ? StationaryTime + DT : 0;
    LastPawnLocation = Position;
    if (Elapsed > FMath::Max(1.f, ApproachTimeout) || StationaryTime > 3.f)
    { CancelApproach(); LastInteractionStatus = TEXT("Cannot reach the character. Check the navigation mesh and obstacles."); return; }
    // The template issues its normal ground path on release; replace that path with the NPC path.
    if (HandleMouseClicks && PC->WasInputKeyJustReleased(EKeys::LeftMouseButton)) MoveToTarget();
}
void UGrimoireClickToTalk::EndPlay(const EEndPlayReason::Type Reason)
{ CancelApproach(); LockMovement(false); Super::EndPlay(Reason); }
