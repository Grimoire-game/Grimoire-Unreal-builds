#include "GrimoireDialogChoiceWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"

void UGrimoireDialogChoiceWidget::ConfigureChoice(UGrimoireDialogPlayer* Player, const FGrimoireDialogChoice& Choice)
{
    DialogPlayer = Player; CurrentChoice = Choice;
    AssignedDialog = Player ? Player->CurrentDialogId : FString();
    AssignedNode = Player ? Player->CurrentLine.NodeId : FString();
    SetIsEnabled(true); RefreshLabel();
}
void UGrimoireDialogChoiceWidget::RefreshLabel()
{ if (auto* Label = Cast<UTextBlock>(GetWidgetFromName(TEXT("ChoiceLabel")))) if (!IsDesignTime()) Label->SetText(CurrentChoice.Text); }
void UGrimoireDialogChoiceWidget::NativePreConstruct() { Super::NativePreConstruct(); RefreshLabel(); }
void UGrimoireDialogChoiceWidget::NativeConstruct()
{
    Super::NativeConstruct(); RefreshLabel();
    if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("ChoiceButton")))) Button->OnClicked.AddUniqueDynamic(this, &UGrimoireDialogChoiceWidget::SelectChoice);
}
void UGrimoireDialogChoiceWidget::NativeDestruct()
{
    if (auto* Button = Cast<UButton>(GetWidgetFromName(TEXT("ChoiceButton")))) Button->OnClicked.RemoveDynamic(this, &UGrimoireDialogChoiceWidget::SelectChoice);
    Super::NativeDestruct();
}
void UGrimoireDialogChoiceWidget::SelectChoice()
{
    if (!GetIsEnabled() || !DialogPlayer || !DialogPlayer->IsPlaying || DialogPlayer->CurrentDialogId != AssignedDialog || DialogPlayer->CurrentLine.NodeId != AssignedNode) return;
    if (!DialogPlayer->CurrentLine.Choices.ContainsByPredicate([this](const FGrimoireDialogChoice& C) { return C.Index == CurrentChoice.Index && C.OptionId == CurrentChoice.OptionId; })) return;
    SetIsEnabled(false); DialogPlayer->Choose(CurrentChoice.Index);
}
