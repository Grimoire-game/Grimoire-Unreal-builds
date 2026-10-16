#include "GrimoireDialogActor.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "GrimoirePortraitLibrary.h"

AGrimoireDialogActor::AGrimoireDialogActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Player = CreateDefaultSubobject<UGrimoireDialogPlayer>(TEXT("DialogPlayer"));
}
void AGrimoireDialogActor::BeginPlay()
{
    Super::BeginPlay();
    if (CreateDefaultWidget)
    {
        auto* Controller = GetWorld()->GetFirstPlayerController();
        if (Controller && Controller->IsLocalController())
        {
            View = CreateWidget<UGrimoireDialogWidget>(Controller, DialogWidgetClass ? DialogWidgetClass.Get() : UGrimoireDialogWidget::StaticClass());
            if (View) { View->SetPortraitLibrary(PortraitLibrary); View->SetPlayer(Player); View->AddToViewport(20); }
        }
    }
    Player->OnLine.AddDynamic(this, &AGrimoireDialogActor::Show);
    Player->OnChoices.AddDynamic(this, &AGrimoireDialogActor::Show);
    Player->OnDialogEnded.AddDynamic(this, &AGrimoireDialogActor::Ended);
    if (Player->IsPlaying) Show(Player->CurrentLine);
}
bool AGrimoireDialogActor::StartDialog(const FString& Key) { return Player->StartDialog(Key); }
void AGrimoireDialogActor::Show(const FGrimoireDialogLine&)
{
    if (!ManagePlayerInput || !View) return;
    auto* Controller = View->GetOwningPlayer(); if (!Controller) return;
    if (!InputOwner.IsValid()) { InputOwner = Controller; WasMouseVisible = Controller->bShowMouseCursor; }
    FInputModeGameAndUI Input; Input.SetHideCursorDuringCapture(false);
    Controller->SetInputMode(Input); Controller->bShowMouseCursor = true;
}
void AGrimoireDialogActor::Ended(const FString&)
{
    if (auto* Controller = InputOwner.Get()) { Controller->bShowMouseCursor = WasMouseVisible; Controller->SetInputMode(FInputModeGameOnly()); }
    InputOwner.Reset();
}
void AGrimoireDialogActor::EndPlay(const EEndPlayReason::Type Reason)
{
    Ended(FString());
    if (View) View->RemoveFromParent();
    Super::EndPlay(Reason);
}
