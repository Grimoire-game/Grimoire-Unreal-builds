#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrimoireDialogPlayer.h"
#include "GrimoireDialogWidget.h"
#include "GrimoireDialogActor.generated.h"

/** Drop-in player with an optional default UMG view on the first local player. */
UCLASS(Blueprintable)
class GRIMOIRERUNTIME_API AGrimoireDialogActor : public AActor
{
    GENERATED_BODY()
public:
    AGrimoireDialogActor();
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Grimoire") TObjectPtr<UGrimoireDialogPlayer> Player;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") bool CreateDefaultWidget = true;
    /** Optional Designer-authored child of Grimoire Dialog Widget. Empty uses the native default. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") TSubclassOf<UGrimoireDialogWidget> DialogWidgetClass;
    /** Speaker portraits and their Mood mappings. Kept separate from imported API data. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire|Portraits") TObjectPtr<UGrimoirePortraitLibrary> PortraitLibrary;
    /** For single-player games: switch to Game and UI input while showing a dialog. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Grimoire") bool ManagePlayerInput = true;
    UFUNCTION(BlueprintCallable, Category="Grimoire") bool StartDialog(const FString& DialogIdOrKey);
protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY(Transient) TObjectPtr<UGrimoireDialogWidget> View;
    TWeakObjectPtr<APlayerController> InputOwner;
    bool WasMouseVisible = false;
    UFUNCTION() void Show(const FGrimoireDialogLine& Line);
    UFUNCTION() void Ended(const FString& Reason);
};
