#include "GrimoireEditorSubsystem.h"
#include "GrimoireDialogWidget.h"
#include "GrimoireDialogChoiceWidget.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Components/Border.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/OverlaySlot.h"
#include "Components/ButtonSlot.h"
#include "ScopedTransaction.h"

FString UGrimoireEditorSubsystem::ConfigureDialogDesigner(UBlueprint* DialogBlueprint, UBlueprint* ChoiceBlueprint)
{
    auto* BP = Cast<UWidgetBlueprint>(DialogBlueprint); auto* ChoiceBP = Cast<UWidgetBlueprint>(ChoiceBlueprint);
    if (!BP || !ChoiceBP || !BP->WidgetTree || !BP->WidgetTree->RootWidget || !ChoiceBP->WidgetTree) return TEXT("Two valid Widget Blueprints are required.");
    if (BP->ParentClass != UUserWidget::StaticClass() && !BP->ParentClass->IsChildOf(UGrimoireDialogWidget::StaticClass())) return TEXT("Dialog already has a different custom parent; it was not changed.");
    if (ChoiceBP->ParentClass != UUserWidget::StaticClass() && !ChoiceBP->ParentClass->IsChildOf(UGrimoireDialogChoiceWidget::StaticClass())) return TEXT("Choice already has a different custom parent; it was not changed.");
    auto Normalize = [](FString S) { S.ReplaceInline(TEXT(" "), TEXT("")); S.ReplaceInline(TEXT("_"), TEXT("")); return S.ToLower(); };
    TArray<UWidget*> Widgets; BP->WidgetTree->GetAllWidgets(Widgets);
    auto Find = [&](const FString& Label) -> UWidget*
    {
        TArray<UWidget*> Matches;
        for (auto* W : Widgets) if (Normalize(W->GetName()) == Normalize(Label) || Normalize(W->GetDisplayLabel()) == Normalize(Label)) Matches.Add(W);
        return Matches.Num() == 1 ? Matches[0] : nullptr;
    };
    auto* Speaker = Cast<UTextBlock>(Find(TEXT("Speaker name")));
    auto* Body = Cast<UTextBlock>(Find(TEXT("dialog text")));
    auto* Continue = Cast<UButton>(Find(TEXT("Continue")));
    auto* Portrait = Cast<UImage>(Find(TEXT("portrait")));
    auto* TextColumn = Body ? Cast<UVerticalBox>(Body->GetParent()) : nullptr;
    if (!Speaker || !Body || !Continue || !Portrait || !TextColumn) return TEXT("Expected unique Speaker name, dialog text, Continue and portrait widgets, with dialog text in a Vertical Box.");
    const FScopedTransaction Transaction(NSLOCTEXT("Grimoire", "ConnectDesigner", "Connect Grimoire dialog widgets"));
    BP->Modify(); ChoiceBP->Modify(); BP->WidgetTree->Modify(); ChoiceBP->WidgetTree->Modify();
    BP->ParentClass = UGrimoireDialogWidget::StaticClass(); ChoiceBP->ParentClass = UGrimoireDialogChoiceWidget::StaticClass();
    auto* Choices = Cast<UVerticalBox>(BP->WidgetTree->FindWidget(TEXT("ChoicesContainer")));
    if (!Choices)
    {
        Choices = BP->WidgetTree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("ChoicesContainer")); Choices->bIsVariable = true;
        TextColumn->AddChildToVerticalBox(Choices)->SetPadding(FMargin(0, 10, 0, 0));
    }
    Choices->SetVisibility(ESlateVisibility::Collapsed);
    // Keep long answers accessible without pushing the portrait or Continue off screen.
    if (auto* Row = Cast<UHorizontalBox>(TextColumn->GetParent()))
    {
        Row->Modify(); TextColumn->Modify(); Row->RemoveChild(TextColumn);
        auto* Scroll = BP->WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("DialogContentScroll"));
        Scroll->AddChild(TextColumn); auto* Slot = Row->AddChildToHorizontalBox(Scroll);
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); Slot->SetPadding(FMargin(0, 0, 220, 0));
    }
    Body->Modify(); Body->SetAutoWrapText(true); Body->SetColorAndOpacity(FLinearColor::White);
    Speaker->Modify(); Speaker->SetColorAndOpacity(FLinearColor(.65f, .4f, 1));
    for (auto* W : Widgets)
    {
        W->Modify(); W->SetIsEnabled(true); W->SetRenderOpacity(1.f);
        if (auto* Border = Cast<UBorder>(W)) Border->SetContentColorAndOpacity(FLinearColor::White);
    }
    if (auto* Background = Cast<UBorder>(Find(TEXT("Dialog background"))))
    {
        FSlateBrush Brush = Background->Background; Brush.TintColor = FSlateColor(FLinearColor::White); Background->SetBrush(Brush);
        Background->SetBrushColor(FLinearColor(.025f, .018f, .05f, .96f));
    }
    if (!Cast<UCanvasPanel>(BP->WidgetTree->RootWidget))
    {
        UWidget* OriginalRoot = BP->WidgetTree->RootWidget;
        auto* Canvas = BP->WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("DialogScreen"));
        BP->WidgetTree->RootWidget = Canvas;
        auto* Slot = Canvas->AddChildToCanvas(OriginalRoot); Slot->SetAnchors(FAnchors(.06f, 1.f, .94f, 1.f));
        Slot->SetOffsets(FMargin(0, -320, 0, 280));
    }
    if (auto* Canvas = Cast<UCanvasPanel>(BP->WidgetTree->RootWidget))
        if (Canvas->GetChildrenCount() == 1)
            if (auto* Slot = Cast<UCanvasPanelSlot>(Canvas->GetChildAt(0)->Slot)) Slot->SetOffsets(FMargin(0, -400, 0, 360));
    if (auto* Frame = Cast<USizeBox>(Portrait->GetParent()))
    {
        Frame->Modify(); Frame->SetWidthOverride(300); Frame->SetHeightOverride(300); Frame->SetRenderTranslation(FVector2D(0, -150));
        if (auto* Slot = Cast<UOverlaySlot>(Frame->Slot)) { Slot->SetHorizontalAlignment(HAlign_Right); Slot->SetVerticalAlignment(VAlign_Bottom); }
    }
    if (!ChoiceBP->WidgetTree->RootWidget)
    {
        auto* Size = ChoiceBP->WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("ChoiceSize")); Size->SetMinDesiredHeight(44);
        ChoiceBP->WidgetTree->RootWidget = Size;
        auto* Button = ChoiceBP->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("ChoiceButton")); Button->bIsVariable = true;
        Button->SetBackgroundColor(FLinearColor(.26f, .07f, .72f)); Size->AddChild(Button);
        auto* Label = ChoiceBP->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("ChoiceLabel")); Label->bIsVariable = true;
        Label->SetText(FText::FromString(TEXT("Example answer"))); Label->SetAutoWrapText(true); Label->SetColorAndOpacity(FLinearColor::White);
        auto Font = Label->GetFont(); Font.Size = 20; Label->SetFont(Font);
        auto* Slot = Cast<UButtonSlot>(Button->AddChild(Label)); Slot->SetPadding(FMargin(14, 8)); Slot->SetHorizontalAlignment(HAlign_Left);
    }
    if (!Cast<UButton>(ChoiceBP->WidgetTree->FindWidget(TEXT("ChoiceButton"))) || !Cast<UTextBlock>(ChoiceBP->WidgetTree->FindWidget(TEXT("ChoiceLabel")))) return TEXT("Choice widget requires ChoiceButton and ChoiceLabel.");
    for (auto* WidgetBP : {BP, ChoiceBP})
    {
        TArray<UWidget*> All; WidgetBP->WidgetTree->GetAllWidgets(All);
        for (auto* W : All) if (!WidgetBP->WidgetVariableNameToGuidMap.Contains(W->GetFName())) WidgetBP->OnVariableAdded(W->GetFName());
    }
    FBlueprintEditorUtils::RefreshAllNodes(ChoiceBP); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(ChoiceBP); FKismetEditorUtilities::CompileBlueprint(ChoiceBP);
    FBlueprintEditorUtils::RefreshAllNodes(BP); FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP); FKismetEditorUtilities::CompileBlueprint(BP);
    if (BP->Status == BS_Error || ChoiceBP->Status == BS_Error) return TEXT("Widget Blueprint compilation failed; inspect the compiler results.");
    auto* Defaults = Cast<UGrimoireDialogWidget>(BP->GeneratedClass->GetDefaultObject()); Defaults->Modify();
    Defaults->SetColorAndOpacity(FLinearColor::White); Defaults->SetRenderOpacity(1.f); Defaults->SetIsEnabled(true);
    Defaults->SpeakerWidgetName = Speaker->GetFName(); Defaults->DialogTextWidgetName = Body->GetFName(); Defaults->ChoicesWidgetName = Choices->GetFName();
    Defaults->ContinueWidgetName = Continue->GetFName(); Defaults->ContinueFrameName = Continue->GetParent() ? Continue->GetParent()->GetFName() : Continue->GetFName();
    Defaults->PortraitWidgetName = Portrait->GetFName(); Defaults->PortraitFrameName = Portrait->GetParent() ? Portrait->GetParent()->GetFName() : Portrait->GetFName();
    Defaults->ChoiceWidgetClass = ChoiceBP->GeneratedClass;
    BP->MarkPackageDirty(); ChoiceBP->MarkPackageDirty(); return FString();
}
