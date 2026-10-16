#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GrimoireDialogPlayer.h"
#include "GrimoireDialogChoiceWidget.generated.h"

/** Designer-editable answer row. Include a Button named ChoiceButton and Text Block named ChoiceLabel. */
UCLASS(Blueprintable)
class GRIMOIRERUNTIME_API UGrimoireDialogChoiceWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Grimoire|Dialog") void ConfigureChoice(UGrimoireDialogPlayer* Player, const FGrimoireDialogChoice& Choice);
    UPROPERTY(BlueprintReadOnly, Category="Grimoire|Dialog") FGrimoireDialogChoice CurrentChoice;
    UFUNCTION(BlueprintCallable, Category="Grimoire|Dialog") void SelectChoice();
protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual void NativePreConstruct() override;
private:
    UPROPERTY(Transient) TObjectPtr<UGrimoireDialogPlayer> DialogPlayer;
    FString AssignedDialog, AssignedNode;
    void RefreshLabel();
};
