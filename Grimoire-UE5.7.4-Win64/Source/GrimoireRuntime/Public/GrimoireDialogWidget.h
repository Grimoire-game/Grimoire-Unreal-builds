#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "GrimoireDialogPlayer.h"
#include "GrimoireDialogWidget.generated.h"
class UVerticalBox;
class UImage;
class UGrimoirePortraitLibrary;
class UGrimoireSubsystem;
class UBorder;
class UWidget;
class UTextBlock;
class UGrimoireDialogChoiceWidget;

UCLASS()
class GRIMOIRERUNTIME_API UGrimoireChoiceButton : public UButton
{
    GENERATED_BODY()
public:
    UPROPERTY() TObjectPtr<UGrimoireDialogPlayer> Player;
    int32 ChoiceIndex = -1;
    void Bind();
private:
    UFUNCTION() void Selected();
};

/** Ready-to-use basic UI. Create Widget, Set Player, Add to Viewport, then StartDialog. */
UCLASS(Blueprintable)
class GRIMOIRERUNTIME_API UGrimoireDialogWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Grimoire") void SetPlayer(UGrimoireDialogPlayer* NewPlayer);
    UFUNCTION(BlueprintCallable, Category="Grimoire|Portraits") void SetPortraitLibrary(UGrimoirePortraitLibrary* Library);
    UPROPERTY(BlueprintReadOnly, Transient, Category="Grimoire|Portraits") FString CurrentPortraitMood;
    UPROPERTY(BlueprintReadOnly, Transient, Category="Grimoire|Portraits") bool HasPortrait = false;
    /** Use a child Widget Blueprint to design answer buttons. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grimoire|Designer") TSubclassOf<UGrimoireDialogChoiceWidget> ChoiceWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="Grimoire|Designer") FName SpeakerWidgetName = TEXT("SpeakerText");
    UPROPERTY(EditDefaultsOnly, Category="Grimoire|Designer") FName DialogTextWidgetName = TEXT("DialogText");
    UPROPERTY(EditDefaultsOnly, Category="Grimoire|Designer") FName ChoicesWidgetName = TEXT("ChoicesContainer");
    UPROPERTY(EditDefaultsOnly, Category="Grimoire|Designer") FName ContinueWidgetName = TEXT("ContinueButton");
    UPROPERTY(EditDefaultsOnly, Category="Grimoire|Designer") FName ContinueFrameName = TEXT("ContinueFrame");
    UPROPERTY(EditDefaultsOnly, Category="Grimoire|Designer") FName PortraitWidgetName = TEXT("PortraitImage");
    UPROPERTY(EditDefaultsOnly, Category="Grimoire|Designer") FName PortraitFrameName = TEXT("PortraitFrame");
    UFUNCTION(BlueprintPure, Category="Grimoire|Dialog") UGrimoireDialogPlayer* GetDialogPlayer() const { return Player; }
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual TSharedRef<SWidget> RebuildWidget() override;
private:
    UPROPERTY(Transient) TObjectPtr<UGrimoireDialogPlayer> Player;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> Lines;
    UPROPERTY(Transient) TObjectPtr<UGrimoirePortraitLibrary> PortraitLibrary;
    UPROPERTY(Transient) TObjectPtr<UGrimoireSubsystem> RuntimeState;
    UPROPERTY(Transient) TObjectPtr<UImage> BoundPortraitImage;
    UPROPERTY(Transient) TObjectPtr<UWidget> PortraitFrame;
    UPROPERTY(Transient) TObjectPtr<UBorder> Panel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DesignerSpeaker;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> DesignerText;
    UPROPERTY(Transient) TObjectPtr<UVerticalBox> DesignerChoices;
    UPROPERTY(Transient) TObjectPtr<UButton> DesignerContinue;
    UPROPERTY(Transient) TObjectPtr<UWidget> DesignerContinueFrame;
    UFUNCTION() void ContinueClicked();
    void BindDesignerWidgets();
    UFUNCTION() void ObjectFieldChanged(UGrimoireDatabase* Database, const FString& ObjectId, const FString& FieldId, const FString& Json);
    UFUNCTION() void ShowLine(const FGrimoireDialogLine& Line);
    UFUNCTION() void Ended(const FString& Reason);
    void Unbind();
    void RefreshPortrait();
};
