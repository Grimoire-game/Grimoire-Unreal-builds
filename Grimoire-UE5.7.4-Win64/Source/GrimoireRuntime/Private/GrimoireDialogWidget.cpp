#include "GrimoireDialogWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/VerticalBox.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "Components/ScrollBox.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "GrimoirePortraitLibrary.h"
#include "GrimoireSubsystem.h"
#include "GrimoireDialogChoiceWidget.h"
#include "Engine/GameInstance.h"

void UGrimoireChoiceButton::Bind() { OnClicked.AddUniqueDynamic(this, &UGrimoireChoiceButton::Selected); }
void UGrimoireChoiceButton::Selected() { if (Player) { if (ChoiceIndex < 0) Player->Next(); else Player->Choose(ChoiceIndex); } }
TSharedRef<SWidget> UGrimoireDialogWidget::RebuildWidget()
{
    if (WidgetTree && !WidgetTree->RootWidget)
    {
        auto* Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(); WidgetTree->RootWidget = Canvas;
        Panel = WidgetTree->ConstructWidget<UBorder>(); Panel->SetPadding(FMargin(24)); Panel->SetBrushColor(FLinearColor(0.025f, 0.018f, 0.05f, 0.96f));
        auto* CanvasSlot = Canvas->AddChildToCanvas(Panel); CanvasSlot->SetAnchors(FAnchors(0.06f, 0.64f, 0.94f, 0.97f)); CanvasSlot->SetOffsets(FMargin(0));
        auto* Scroll = WidgetTree->ConstructWidget<UScrollBox>(); Panel->SetContent(Scroll);
        Lines = WidgetTree->ConstructWidget<UVerticalBox>(); Scroll->AddChild(Lines);
        auto* Scale = WidgetTree->ConstructWidget<UScaleBox>(); Scale->SetStretch(EStretch::ScaleToFit); PortraitFrame = Scale;
        auto* PortraitSlot = Canvas->AddChildToCanvas(Scale); PortraitSlot->SetAnchors(FAnchors(0.94f, 0.97f));
        PortraitSlot->SetAlignment(FVector2D(1, 1)); PortraitSlot->SetOffsets(FMargin(-8, -8, 220, 220));
        BoundPortraitImage = WidgetTree->ConstructWidget<UImage>(); Scale->AddChild(BoundPortraitImage);
        PortraitFrame->SetVisibility(ESlateVisibility::Collapsed);
    }
    return Super::RebuildWidget();
}
void UGrimoireDialogWidget::NativeConstruct()
{ Super::NativeConstruct(); BindDesignerWidgets(); SetPlayer(Player); if (!Player || !Player->IsPlaying) SetVisibility(ESlateVisibility::Collapsed); }
void UGrimoireDialogWidget::BindDesignerWidgets()
{
    if (Lines) return; // Native fallback has its own layout.
    DesignerSpeaker = Cast<UTextBlock>(GetWidgetFromName(SpeakerWidgetName));
    DesignerText = Cast<UTextBlock>(GetWidgetFromName(DialogTextWidgetName));
    DesignerChoices = Cast<UVerticalBox>(GetWidgetFromName(ChoicesWidgetName));
    DesignerContinue = Cast<UButton>(GetWidgetFromName(ContinueWidgetName));
    DesignerContinueFrame = GetWidgetFromName(ContinueFrameName);
    if (!DesignerContinueFrame) DesignerContinueFrame = DesignerContinue;
    BoundPortraitImage = Cast<UImage>(GetWidgetFromName(PortraitWidgetName));
    PortraitFrame = GetWidgetFromName(PortraitFrameName);
    if (!PortraitFrame) PortraitFrame = BoundPortraitImage;
    if (DesignerContinue) DesignerContinue->OnClicked.AddUniqueDynamic(this, &UGrimoireDialogWidget::ContinueClicked);
}
void UGrimoireDialogWidget::ContinueClicked()
{ if (Player && Player->IsPlaying && Player->CurrentLine.Choices.IsEmpty()) Player->Next(); }
void UGrimoireDialogWidget::Unbind()
{
    if (RuntimeState) RuntimeState->OnObjectFieldChanged.RemoveDynamic(this, &UGrimoireDialogWidget::ObjectFieldChanged);
    RuntimeState = nullptr;
    if (!Player) return;
    Player->OnLine.RemoveDynamic(this, &UGrimoireDialogWidget::ShowLine);
    Player->OnChoices.RemoveDynamic(this, &UGrimoireDialogWidget::ShowLine);
    Player->OnDialogEnded.RemoveDynamic(this, &UGrimoireDialogWidget::Ended);
}
void UGrimoireDialogWidget::NativeDestruct()
{
    if (DesignerContinue) DesignerContinue->OnClicked.RemoveDynamic(this, &UGrimoireDialogWidget::ContinueClicked);
    Unbind(); Super::NativeDestruct();
}
void UGrimoireDialogWidget::SetPlayer(UGrimoireDialogPlayer* NewPlayer)
{
    Unbind(); Player = NewPlayer;
    if (!Player) { Ended(FString()); return; }
    Player->OnLine.AddUniqueDynamic(this, &UGrimoireDialogWidget::ShowLine);
    Player->OnChoices.AddUniqueDynamic(this, &UGrimoireDialogWidget::ShowLine);
    Player->OnDialogEnded.AddUniqueDynamic(this, &UGrimoireDialogWidget::Ended);
    RuntimeState = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGrimoireSubsystem>() : nullptr;
    if (RuntimeState) RuntimeState->OnObjectFieldChanged.AddUniqueDynamic(this, &UGrimoireDialogWidget::ObjectFieldChanged);
    if (Player->IsPlaying) ShowLine(Player->CurrentLine);
    else Ended(FString());
}
void UGrimoireDialogWidget::ShowLine(const FGrimoireDialogLine& Line)
{
    if (DesignerText && DesignerChoices)
    {
        SetVisibility(ESlateVisibility::SelfHitTestInvisible); RefreshPortrait();
        DesignerText->SetText(Line.Text);
        if (DesignerSpeaker) { DesignerSpeaker->SetText(Line.Speaker); DesignerSpeaker->SetVisibility(Line.IsContext || Line.Speaker.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible); }
        DesignerChoices->ClearChildren();
        const bool HasChoices = !Line.Choices.IsEmpty();
        DesignerChoices->SetVisibility(HasChoices ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
        if (DesignerContinueFrame) DesignerContinueFrame->SetVisibility(HasChoices ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
        for (const auto& Choice : Line.Choices)
        {
            if (ChoiceWidgetClass)
            {
                auto* Row = CreateWidget<UGrimoireDialogChoiceWidget>(GetOwningPlayer(), ChoiceWidgetClass);
                if (Row) { Row->ConfigureChoice(Player, Choice); DesignerChoices->AddChildToVerticalBox(Row)->SetPadding(FMargin(0, 4)); continue; }
            }
            auto* Button = WidgetTree->ConstructWidget<UGrimoireChoiceButton>(); Button->Player = Player; Button->ChoiceIndex = Choice.Index; Button->Bind();
            auto* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(Choice.Text); Label->SetAutoWrapText(true); Button->AddChild(Label);
            DesignerChoices->AddChildToVerticalBox(Button)->SetPadding(FMargin(0, 4));
        }
        return;
    }
    if (!Lines) return; Lines->ClearChildren(); SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    RefreshPortrait();
    auto AddText = [this](const FText& Text, bool Speaker)
    {
        auto* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(Text); Label->SetAutoWrapText(true);
        auto Font = Label->GetFont(); Font.Size = 20; Label->SetFont(Font);
        Label->SetColorAndOpacity(Speaker ? FLinearColor(0.65f, 0.4f, 1.f) : FLinearColor(0.95f, 0.95f, 0.98f));
        Lines->AddChildToVerticalBox(Label)->SetPadding(FMargin(0, 0, 0, Speaker ? 8 : 14));
    };
    if (!Line.IsContext && !Line.Speaker.IsEmpty()) AddText(Line.Speaker, true);
    AddText(Line.Text, false);
    auto AddButton = [this](const FText& Text, int32 Index)
    {
        auto* Button = WidgetTree->ConstructWidget<UGrimoireChoiceButton>(); Button->Player = Player; Button->ChoiceIndex = Index; Button->Bind();
        Button->SetBackgroundColor(FLinearColor(0.26f, 0.07f, 0.72f));
        auto* Label = WidgetTree->ConstructWidget<UTextBlock>(); Label->SetText(Text); Label->SetAutoWrapText(true);
        auto Font = Label->GetFont(); Font.Size = 18; Label->SetFont(Font);
        Button->AddChild(Label); auto* Slot = Lines->AddChildToVerticalBox(Button); Slot->SetPadding(FMargin(0, 4, 0, 4));
    };
    if (Line.Choices.IsEmpty()) AddButton(FText::FromString(TEXT("Continue")), -1);
    else for (const auto& Choice : Line.Choices) AddButton(Choice.Text, Choice.Index);
}
void UGrimoireDialogWidget::SetPortraitLibrary(UGrimoirePortraitLibrary* Library) { PortraitLibrary = Library; RefreshPortrait(); }
void UGrimoireDialogWidget::ObjectFieldChanged(UGrimoireDatabase* DB, const FString&, const FString&, const FString&)
{
    if (Player && Player->Database && DB && (DB == Player->Database || (!DB->GameId.IsEmpty() && DB->GameId == Player->Database->GameId))) RefreshPortrait();
}
void UGrimoireDialogWidget::RefreshPortrait()
{
    FSlateBrush Brush; CurrentPortraitMood.Empty();
    HasPortrait = Player && Player->IsPlaying && PortraitLibrary && PortraitLibrary->ResolvePortrait(Player->Database, RuntimeState, Player->CurrentLine, Brush, CurrentPortraitMood);
    if (BoundPortraitImage) BoundPortraitImage->SetBrush(Brush);
    if (PortraitFrame) PortraitFrame->SetVisibility(HasPortrait ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (Panel) Panel->SetPadding(HasPortrait ? FMargin(24, 24, 236, 24) : FMargin(24));
}
void UGrimoireDialogWidget::Ended(const FString&) { RefreshPortrait(); SetVisibility(ESlateVisibility::Collapsed); }
