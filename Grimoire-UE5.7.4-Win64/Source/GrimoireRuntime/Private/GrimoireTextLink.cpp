#include "GrimoireTextLink.h"
#include "GrimoireSubsystem.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

namespace
{
    UGrimoireSubsystem* LanguageSubsystem(const UWorld* World)
    { return World && World->GetGameInstance() ? World->GetGameInstance()->GetSubsystem<UGrimoireSubsystem>() : nullptr; }
}
FText UGrimoireTextLink::GetLocalizedText() const
{
    if (!Database) return FText::GetEmpty();
    const auto* Sub = LanguageSubsystem(GetWorld());
    const bool EditorPreview = GetWorld() && !GetWorld()->IsGameWorld();
    const FString Language = EditorPreview ? PreviewLanguage : Sub ? Sub->Language : FString();
    if (EditorPreview && !Database->HasTranslation(StringKey, Language)) return FText::FromString(TEXT("not translated"));
    return Database->GetText(StringKey, Language);
}
void UGrimoireTextLink::RefreshText()
{
    const FText Text = GetLocalizedText();
    if (UpdateTextRender && GetOwner())
    {
        TArray<UTextRenderComponent*> Components; GetOwner()->GetComponents(Components);
        for (auto* Component : Components) Component->SetText(Text);
    }
    OnTextChanged.Broadcast(Text);
}
void UGrimoireTextLink::BeginPlay()
{
    Super::BeginPlay();
    if (auto* Sub = LanguageSubsystem(GetWorld())) Sub->OnLanguageChanged.AddDynamic(this, &UGrimoireTextLink::LanguageChanged);
    RefreshText();
}
void UGrimoireTextLink::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* Sub = LanguageSubsystem(GetWorld())) Sub->OnLanguageChanged.RemoveDynamic(this, &UGrimoireTextLink::LanguageChanged);
    Super::EndPlay(Reason);
}
void UGrimoireTextLink::LanguageChanged(const FString&) { RefreshText(); }
void UGrimoireTextBlock::SynchronizeProperties()
{
    Super::SynchronizeProperties();
    if (auto* Sub = LanguageSubsystem(GetWorld())) Sub->OnLanguageChanged.AddUniqueDynamic(this, &UGrimoireTextBlock::LanguageChanged);
    RefreshText();
}
void UGrimoireTextBlock::ReleaseSlateResources(bool bChildren)
{
    if (auto* Sub = LanguageSubsystem(GetWorld())) Sub->OnLanguageChanged.RemoveDynamic(this, &UGrimoireTextBlock::LanguageChanged);
    Super::ReleaseSlateResources(bChildren);
}
void UGrimoireTextBlock::RefreshText()
{
    const auto* Sub = LanguageSubsystem(GetWorld());
    const FString Language = LanguageOverride.IsEmpty() && Sub ? Sub->Language : LanguageOverride;
    SetText(Database ? Database->GetText(StringKey, Language) : FText::GetEmpty());
}
void UGrimoireTextBlock::LanguageChanged(const FString&) { if (LanguageOverride.IsEmpty()) RefreshText(); }
