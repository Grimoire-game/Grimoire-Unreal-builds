#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Styling/SlateBrush.h"
#include "GrimoireDialogPlayer.h"
#include "GrimoirePortraitLibrary.generated.h"

class UTexture2D;
class UGrimoireSubsystem;

/** A whole texture, or a rectangle in a sprite sheet (UV origin is top left). */
USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoirePortraitImage
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait") TObjectPtr<UTexture2D> Texture;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait") FVector2D UVMin = FVector2D::ZeroVector;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait") FVector2D UVMax = FVector2D(1, 1);
    bool MakeBrush(FSlateBrush& Brush) const;
};

USTRUCT(BlueprintType)
struct GRIMOIRERUNTIME_API FGrimoireCharacterPortrait
{
    GENERATED_BODY()
    /** Speaker object UUID or Code ID; plain-name speakers also match the object's name. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait") FString ObjectKey;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait") FString MoodField = TEXT("Mood");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait") FString MoodVariable = TEXT("Mood");
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait") FGrimoirePortraitImage Fallback;
    /** Keys may be Mood element UUIDs or names, e.g. Angry. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portrait") TMap<FString, FGrimoirePortraitImage> Moods;
};

/** Game-owned portrait art, separate from the API snapshot so importing preserves it. */
UCLASS(BlueprintType)
class GRIMOIRERUNTIME_API UGrimoirePortraitLibrary : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Portraits") TArray<FGrimoireCharacterPortrait> Characters;
    /** Reads the current speaker's runtime Mood field, falling back to imported data. */
    UFUNCTION(BlueprintPure, Category="Grimoire|Portraits")
    bool ResolvePortrait(UGrimoireDatabase* Database, UGrimoireSubsystem* RuntimeState, const FGrimoireDialogLine& Line, FSlateBrush& Brush, FString& Mood) const;
};
