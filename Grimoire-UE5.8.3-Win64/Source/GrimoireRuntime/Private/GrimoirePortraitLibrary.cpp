#include "GrimoirePortraitLibrary.h"
#include "GrimoireSubsystem.h"
#include "GrimoireJson.h"
#include "Engine/Texture2D.h"

bool FGrimoirePortraitImage::MakeBrush(FSlateBrush& Brush) const
{
    Brush = FSlateBrush();
    if (!Texture || UVMin.ContainsNaN() || UVMax.ContainsNaN() || UVMin.X < 0 || UVMin.Y < 0 || UVMax.X > 1 || UVMax.Y > 1 || UVMax.X <= UVMin.X || UVMax.Y <= UVMin.Y) return false;
    Brush.SetResourceObject(Texture); Brush.DrawAs = ESlateBrushDrawType::Image;
    Brush.ImageSize = FVector2D(Texture->GetSizeX(), Texture->GetSizeY()) * (UVMax - UVMin);
    Brush.SetUVRegion(FBox2f(FVector2f(UVMin), FVector2f(UVMax)));
    return true;
}

bool UGrimoirePortraitLibrary::ResolvePortrait(UGrimoireDatabase* DB, UGrimoireSubsystem* State, const FGrimoireDialogLine& Line, FSlateBrush& Brush, FString& Mood) const
{
    Brush = FSlateBrush(); Mood.Empty();
    if (!DB || Line.IsContext) return false;
    const FString SpeakerKey = Line.SpeakerId.TrimStartAndEnd();
    const FString SpeakerName = Line.Speaker.ToString().TrimStartAndEnd();
    const auto* Speaker = DB->FindObject(SpeakerKey);
    const FGrimoireCharacterPortrait* Match = nullptr;
    // An explicit object match always wins over a potentially ambiguous display name.
    for (const auto& Character : Characters)
    {
        const auto* Object = DB->FindObject(Character.ObjectKey);
        if (Object && ((Speaker && Object->Id == Speaker->Id) || (!SpeakerKey.IsEmpty() && Character.ObjectKey.Equals(SpeakerKey, ESearchCase::IgnoreCase))))
        { Match = &Character; Speaker = Object; break; }
    }
    if (!Match && !Speaker && !SpeakerName.IsEmpty())
        for (const auto& Character : Characters)
        {
            const auto* Object = DB->FindObject(Character.ObjectKey);
            if (Object && Object->Name.Equals(SpeakerName, ESearchCase::IgnoreCase)) { Match = &Character; Speaker = Object; break; }
        }
    if (!Match || !Speaker) return false;
    FString Json;
    if (!State || !State->GetObjectFieldJson(DB, Speaker->Id, Match->MoodField, Json))
        if (const auto* Field = Speaker->FindField(Match->MoodField)) Json = Field->StoredValueJson();
    auto Value = GrimoireJson::ParseValue(Json);
    // Single and multi-value Types fields both select their first mood, as in Unity.
    if (Value && Value->Type == EJson::Array) Value = Value->AsArray().IsEmpty() ? nullptr : Value->AsArray()[0];
    if (Value && Value->Type == EJson::String) Mood = Value->AsString().TrimStartAndEnd();
    const auto* Element = DB->FindVariableElement(Match->MoodVariable, Mood);
    if (!Mood.IsEmpty())
        for (const auto& Pair : Match->Moods)
        {
            const FString Key = Pair.Key.TrimStartAndEnd();
            const auto* Candidate = DB->FindVariableElement(Match->MoodVariable, Key);
            if (Key.Equals(Mood, ESearchCase::IgnoreCase) || (Element && Candidate && Element->Id == Candidate->Id))
                if (Pair.Value.MakeBrush(Brush)) return true;
        }
    return Match->Fallback.MakeBrush(Brush);
}
