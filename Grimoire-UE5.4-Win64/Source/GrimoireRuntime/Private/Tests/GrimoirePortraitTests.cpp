#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GrimoirePortraitLibrary.h"
#include "GrimoireSubsystem.h"
#include "Engine/Texture2D.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoirePortraitTest, "Grimoire.Runtime.MoodPortraits", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrimoirePortraitTest::RunTest(const FString&)
{
    TStrongObjectPtr<UGrimoireDatabase> DB(NewObject<UGrimoireDatabase>());
    TStrongObjectPtr<UGameInstance> Game(NewObject<UGameInstance>());
    TStrongObjectPtr<UGrimoireSubsystem> State(NewObject<UGrimoireSubsystem>(Game.Get()));
    TStrongObjectPtr<UGrimoirePortraitLibrary> Library(NewObject<UGrimoirePortraitLibrary>());
    TStrongObjectPtr<UTexture2D> Texture(UTexture2D::CreateTransient(96, 96));
    DB->GameId = TEXT("portrait-game");
    FGrimoireGameVariable Variable; Variable.Id = TEXT("mood-type"); Variable.Name = TEXT("Mood");
    const TArray<FString> Names = { TEXT("Happy"), TEXT("Sad"), TEXT("Neutral"), TEXT("Confused"), TEXT("Angry"), TEXT("Surprised"), TEXT("Afraid"), TEXT("Proud"), TEXT("Laughing") };
    FGrimoireCharacterPortrait Character; Character.ObjectKey = TEXT("npc_bofur");
    for (int32 I = 0; I < Names.Num(); ++I)
    {
        FGrimoireVariableElement E; E.Id = FString::Printf(TEXT("mood-%d"), I); E.Name = Names[I]; Variable.Elements.Add(E);
        FGrimoirePortraitImage Image; Image.Texture = Texture.Get();
        Image.UVMin = FVector2D(I % 3, I / 3) / 3; Image.UVMax = Image.UVMin + FVector2D(1. / 3, 1. / 3);
        Character.Moods.Add(E.Id, Image);
        if (I == 2) Character.Fallback = Image;
    }
    DB->GameVariables.Add(Variable);
    FGrimoireObject Object; Object.Id = TEXT("bofur-id"); Object.CodeId = Character.ObjectKey; Object.Name = TEXT("Bofur");
    FGrimoireField Field; Field.Id = TEXT("mood-field"); Field.Label = TEXT("Mood"); Field.Kind = TEXT("reference");
    FGrimoireFieldValue Value; Value.ValueJson = TEXT("\"mood-2\""); Field.Values.Add(Value); Object.Fields.Add(Field); DB->Objects.Add(Object);
    Library->Characters.Add(Character);
    FGrimoireDialogLine Line; Line.SpeakerId = Object.Id; Line.Speaker = FText::FromString(Object.Name);
    FSlateBrush Brush; FString Mood;
    TestTrue(TEXT("Imported neutral mood works without a scene actor or runtime state"), Library->ResolvePortrait(DB.Get(), nullptr, Line, Brush, Mood));
    TestTrue(TEXT("Sheet texture is retained"), Brush.GetResourceObject() == Texture.Get());
    for (int32 I = 0; I < Names.Num(); ++I)
    {
        TestTrue(TEXT("Blueprint helper accepts Mood name"), State->SetObjectMood(DB.Get(), Object.CodeId, Names[I]));
        TestTrue(TEXT("Runtime mood resolves"), Library->ResolvePortrait(DB.Get(), State.Get(), Line, Brush, Mood));
        TestEqual(TEXT("Name canonicalizes to element UUID"), Mood, Variable.Elements[I].Id);
        TestTrue(TEXT("Correct atlas cell"), static_cast<FBox2f>(Brush.GetUVRegion()).Min.Equals(FVector2f(I % 3, I / 3) / 3, .0001f));
    }
    TestFalse(TEXT("Unknown Mood is rejected by Blueprint helper"), State->SetObjectMood(DB.Get(), Object.Id, TEXT("Typo")));
    State->SetObjectFieldJson(DB.Get(), Object.Id, Field.Id, TEXT("\"  aNgRy  \""));
    TestTrue(TEXT("Names match UUID mappings case-insensitively"), Library->ResolvePortrait(DB.Get(), State.Get(), Line, Brush, Mood));
    TestTrue(TEXT("Angry cell selected"), static_cast<FBox2f>(Brush.GetUVRegion()).Min.Equals(FVector2f(1.f / 3, 1.f / 3), .0001f));
    for (const FString Json : { TEXT("null"), TEXT("\"unknown\""), TEXT("[]") })
    {
        State->SetObjectFieldJson(DB.Get(), Object.Id, Field.Id, Json);
        TestTrue(TEXT("Unknown/empty mood uses fallback"), Library->ResolvePortrait(DB.Get(), State.Get(), Line, Brush, Mood));
        TestTrue(TEXT("Neutral fallback cell"), static_cast<FBox2f>(Brush.GetUVRegion()).Min.Equals(FVector2f(2.f / 3, 0), .0001f));
    }
    State->SetObjectFieldJson(DB.Get(), Object.Id, Field.Id, TEXT("[\"mood-4\"]"));
    Library->ResolvePortrait(DB.Get(), State.Get(), Line, Brush, Mood); TestEqual(TEXT("Types arrays use first element"), Mood, TEXT("mood-4"));
    State->ResetGameValues(DB.Get()); Library->ResolvePortrait(DB.Get(), State.Get(), Line, Brush, Mood);
    TestEqual(TEXT("Reset restores imported mood"), Mood, TEXT("mood-2"));
    TestEqual(TEXT("Runtime changes never mutate imported data"), DB->Objects[0].Fields[0].Values[0].ValueJson, TEXT("\"mood-2\""));
    Line.IsContext = true;
    TestFalse(TEXT("Context hides portrait"), Library->ResolvePortrait(DB.Get(), State.Get(), Line, Brush, Mood));
    TestNull(TEXT("Hidden portrait has no stale image"), Brush.GetResourceObject());
    Line.IsContext = false; Line.SpeakerId = Object.CodeId;
    TestTrue(TEXT("Speaker Code ID works"), Library->ResolvePortrait(DB.Get(), State.Get(), Line, Brush, Mood));
    Line.SpeakerId = TEXT("Bofur"); TestTrue(TEXT("Plain speaker name works"), Library->ResolvePortrait(DB.Get(), State.Get(), Line, Brush, Mood));
    Line.SpeakerId = TEXT("npc_bartel"); Line.Speaker = FText::FromString(TEXT("Bartel"));
    TestFalse(TEXT("Unconfigured speaker hides image"), Library->ResolvePortrait(DB.Get(), State.Get(), Line, Brush, Mood));
    auto BadImage = Character.Fallback; BadImage.UVMax = BadImage.UVMin;
    TestFalse(TEXT("Empty atlas region rejected"), BadImage.MakeBrush(Brush));
    return true;
}
#endif


