#include "GrimoireData.h"
#include "GrimoireJson.h"
using namespace GrimoireJson;

FString FGrimoireField::StoredValueJson() const
{
    if (!Multiple) return Values.IsEmpty() ? TEXT("null") : Values[0].ValueJson;
    TArray<FString> Parts;
    for (const auto& V : Values) Parts.Add(V.ValueJson);
    return TEXT("[") + FString::Join(Parts, TEXT(",")) + TEXT("]");
}

const FGrimoireField* FGrimoireObject::FindField(const FString& Key) const
{
    if (Key.IsEmpty()) return nullptr;
    for (const auto& F : Fields) if (F.Id == Key) return &F;
    const FGrimoireField* Match = nullptr;
    for (const auto& F : Fields) if (F.Label.Equals(Key, ESearchCase::IgnoreCase))
    { if (Match) return nullptr; Match = &F; } // Ambiguous labels require UUIDs.
    return Match;
}

const FGrimoireObject* UGrimoireDatabase::FindObject(const FString& Key) const
{
    if (Key.IsEmpty()) return nullptr;
    for (const auto& O : Objects) if (O.Id == Key) return &O;
    for (const auto& O : Objects) if (O.CodeId == Key) return &O;
    return nullptr;
}
const FGrimoireString* UGrimoireDatabase::FindString(const FString& Key) const
{
    if (Key.IsEmpty()) return nullptr;
    for (const auto& S : Strings) if (S.Id == Key) return &S;
    for (const auto& S : Strings) if (S.Key == Key) return &S;
    return nullptr;
}
const FGrimoireDialog* UGrimoireDatabase::FindDialog(const FString& Key) const
{
    if (Key.IsEmpty()) return nullptr;
    for (const auto& D : Dialogs) if (D.Id == Key) return &D;
    for (const auto& D : Dialogs) if (D.Key == Key) return &D;
    return nullptr;
}
bool UGrimoireDatabase::GetObject(const FString& Key, FGrimoireObject& Out) const
{ const auto* O = FindObject(Key); Out = O ? *O : FGrimoireObject(); return O != nullptr; }
bool UGrimoireDatabase::GetField(const FString& Key, const FString& FieldKey, FGrimoireField& Out) const
{ const auto* O = FindObject(Key); const auto* F = O ? O->FindField(FieldKey) : nullptr; Out = F ? *F : FGrimoireField(); return F != nullptr; }
FText UGrimoireDatabase::GetText(const FString& Key, const FString& Language) const
{
    const auto* S = FindString(Key);
    if (!S) return FText::GetEmpty();
    if (!Language.IsEmpty() && !Language.Equals(SourceLanguage, ESearchCase::IgnoreCase))
        for (const auto& T : S->Translations)
            if (T.Language.Equals(Language, ESearchCase::IgnoreCase) && !T.Text.IsEmpty()) return FText::FromString(T.Text);
    return FText::FromString(S->Source);
}
bool UGrimoireDatabase::HasTranslation(const FString& Key, const FString& Language) const
{
    const auto* S = FindString(Key);
    if (!S) return false;
    if (Language.IsEmpty() || Language.Equals(SourceLanguage, ESearchCase::IgnoreCase)) return true;
    for (const auto& T : S->Translations) if (T.Language.Equals(Language, ESearchCase::IgnoreCase) && !T.Text.IsEmpty()) return true;
    return false;
}
FString UGrimoireDatabase::GetVoiceUrl(const FString& Key, const FString& Language) const
{
    const auto* S = FindString(Key);
    if (!S) return FString();
    for (const auto& T : S->Translations)
        if (!Language.IsEmpty() && T.Language.Equals(Language, ESearchCase::IgnoreCase) && !T.Text.IsEmpty()) return T.VoiceUrl;
    return S->VoiceUrl;
}

bool UGrimoireDatabase::ParseObject(const FObject& Doc, FGrimoireObject& Out, FString& Error)
{
    const auto Summary = Object(Doc, TEXT("object"));
    if (String(Summary, TEXT("id")).IsEmpty()) { Error = TEXT("Object document is missing object.id."); return false; }
    Out = FGrimoireObject();
    Out.Id = String(Summary, TEXT("id")); Out.CodeId = String(Summary, TEXT("code_id"));
    Out.Name = String(Summary, TEXT("name")); Out.TemplateName = String(Object(Summary, TEXT("template")), TEXT("name"));
#if WITH_EDITORONLY_DATA
    Out.DocumentJson = Write(Doc);
#endif
    for (const auto& SectionValue : Array(Doc, TEXT("sections")))
    {
        const auto Section = AsObject(SectionValue);
        for (const auto& FieldValue : Array(Section, TEXT("fields")))
        {
            const auto Field = AsObject(FieldValue); if (!Field) continue;
            FGrimoireField F;
            F.Id = String(Field, TEXT("id")); F.Label = String(Field, TEXT("label")); F.Kind = String(Field, TEXT("kind"));
            F.Plain = String(Field, TEXT("plain")); F.Section = String(Section, TEXT("title")); F.Multiple = Bool(Field, TEXT("multiple"));
            const auto Hints = Object(Field, TEXT("hints"));
            if (F.Kind.IsEmpty() || F.Kind == TEXT("empty"))
            {
                const FString Type = String(Hints, TEXT("field_type"));
                F.Kind = Type == TEXT("boolean") ? TEXT("boolean") : Type == TEXT("number") || Type == TEXT("float") ? TEXT("number") : Type == TEXT("vector2") || Type == TEXT("vector3") ? TEXT("vector") : TEXT("text");
            }
            F.Editable = Bool(Hints, TEXT("game_engine_editable")) && !Bool(Hints, TEXT("read_only"));
            for (const auto& Raw : Array(Field, TEXT("values")))
            {
                const auto V = AsObject(Raw); if (!V) continue;
                FGrimoireFieldValue Item;
                Item.Text = String(V, TEXT("plain"));
                TSharedPtr<FJsonValue> Stored;
                if (F.Kind == TEXT("number"))
                { Item.Number = Number(Object(V, TEXT("number")), TEXT("value")); Stored = MakeShared<FJsonValueNumber>(Item.Number); }
                else if (F.Kind == TEXT("boolean"))
                { Item.Boolean = Bool(Object(V, TEXT("boolean")), TEXT("value")); Stored = MakeShared<FJsonValueBoolean>(Item.Boolean); }
                else if (F.Kind == TEXT("vector"))
                {
                    auto Vec = MakeShared<FJsonObject>();
                    for (const auto& C : Array(Object(V, TEXT("vector")), TEXT("components")))
                    {
                        const auto Component = AsObject(C); const FString Axis = String(Component, TEXT("axis")).ToLower();
                        const double N = Number(Component, TEXT("value")); Vec->SetNumberField(Axis, N);
                        if (Axis == TEXT("x")) Item.Vector.X = N;
                        if (Axis == TEXT("y")) Item.Vector.Y = N;
                        if (Axis == TEXT("z")) Item.Vector.Z = N;
                    }
                    Stored = MakeShared<FJsonValueObject>(Vec);
                }
                else if (F.Kind == TEXT("reference"))
                {
                    auto R = Object(V, TEXT("reference"));
                    Item.Reference.Id = String(R, TEXT("id")); Item.Reference.CodeId = String(R, TEXT("code_id"));
                    Item.Reference.Name = String(R, TEXT("name")); Item.Reference.Kind = String(R, TEXT("ref_kind"));
                    Stored = MakeShared<FJsonValueString>(Item.Reference.Id);
                }
                else
                {
                    const auto TextPayload = Object(V, TEXT("text"));
                    Item.StringId = String(TextPayload, TEXT("string_id"));
                    if (F.Kind == TEXT("media")) Item.Text = String(Object(V, TEXT("media")), TEXT("url"), Item.Text);
                    else if (F.Kind == TEXT("link")) Item.Text = String(Object(V, TEXT("link")), TEXT("href"), Item.Text);
                    else if (Item.Text.IsEmpty()) Item.Text = String(TextPayload, TEXT("content"));
                    // Preserve rich text as the optimistic-concurrency base; Blueprint Text stays plain.
                    Stored = MakeShared<FJsonValueString>(String(TextPayload, TEXT("content"), Item.Text));
                }
                Item.ValueJson = WriteValue(Stored); F.Values.Add(MoveTemp(Item));
            }
            Out.Fields.Add(MoveTemp(F));
        }
    }
    return true;
}

bool UGrimoireDatabase::ParseString(const FObject& Resource, FGrimoireString& Out)
{
    if (String(Resource, TEXT("id")).IsEmpty()) return false;
    Out = FGrimoireString(); Out.Id = String(Resource, TEXT("id")); Out.Key = String(Resource, TEXT("abbrev"));
    Out.Source = String(Resource, TEXT("source_text")); Out.VoiceUrl = String(Resource, TEXT("voice_url"));
    for (const auto& V : Array(Resource, TEXT("translations")))
    {
        auto T = AsObject(V); if (!T) continue;
        FGrimoireTranslation Translation;
        Translation.Language = String(T, TEXT("language_code")); Translation.Text = String(T, TEXT("translated_text"));
        Translation.VoiceUrl = String(T, TEXT("voice_url")); Out.Translations.Add(MoveTemp(Translation));
    }
    return true;
}

bool UGrimoireDatabase::ParseDialog(const FObject& Resource, FGrimoireDialog& Out, FString& Error)
{
    auto Data = Object(Resource, TEXT("data"));
    if (String(Resource, TEXT("id")).IsEmpty() || !Data)
    { Error = TEXT("Dialog is missing its id or graph data."); return false; }
    Out = FGrimoireDialog(); Out.Id = String(Resource, TEXT("id")); Out.Key = String(Resource, TEXT("dialog_key")); Out.Name = String(Resource, TEXT("name"));
    // Normalize a copy; the response may still be used by the importer.
    Data = Parse(Write(Data));
    auto Nodes = Array(Data, TEXT("nodes"));
    if (Nodes.IsEmpty())
    {
        auto Sections = Array(Data, TEXT("sections"));
        Sections.StableSort([](const auto& A, const auto& B) { return Number(AsObject(A), TEXT("order_index")) < Number(AsObject(B), TEXT("order_index")); });
        for (int32 SI = 0; SI < Sections.Num(); ++SI)
        {
            auto S = AsObject(Sections[SI]); auto Fields = Array(S, TEXT("fields"));
            auto Order = Array(S, TEXT("fieldOrder"));
            auto Rank = [&Order](const TSharedPtr<FJsonValue>& V)
            { const FString Id = String(AsObject(V), TEXT("id")); for (int32 I = 0; I < Order.Num(); ++I) if (Order[I]->Type == EJson::String && Order[I]->AsString() == Id) return I; return MAX_int32; };
            Fields.StableSort([&Rank](const auto& A, const auto& B)
            { const int32 RA = Rank(A), RB = Rank(B); return RA != RB ? RA < RB : Number(AsObject(A), TEXT("order_index")) < Number(AsObject(B), TEXT("order_index")); });
            const FString SectionId = First(String(S, TEXT("section_identifier")), String(S, TEXT("id")));
            for (int32 FI = 0; FI < Fields.Num(); ++FI)
            {
                auto N = AsObject(Fields[FI]); if (!N) continue;
                N->SetStringField(TEXT("node_identifier"), FI == 0 ? SectionId : SectionId + FString::Printf(TEXT("__%d"), FI + 1));
                const FString Type = String(N, TEXT("type"));
                if (String(N, TEXT("next_node")).IsEmpty() && String(N, TEXT("next_field")).IsEmpty() && String(N, TEXT("next_section")).IsEmpty() && String(N, TEXT("next_dialog")).IsEmpty()
                    && Type != TEXT("question") && Type != TEXT("condition") && Type != TEXT("end-dialog") && Type != TEXT("end-text"))
                {
                    if (FI + 1 < Fields.Num()) N->SetStringField(TEXT("next_node"), SectionId + FString::Printf(TEXT("__%d"), FI + 2));
                    else if (SI + 1 < Sections.Num())
                    { auto Next = AsObject(Sections[SI + 1]); N->SetStringField(TEXT("next_node"), First(String(Next, TEXT("section_identifier")), String(Next, TEXT("id")))); }
                }
                Nodes.Add(Fields[FI]);
            }
        }
    }
    TSet<FString> Identifiers;
    for (const auto& V : Nodes)
    {
        auto N = AsObject(V); if (!N) { Error = TEXT("Invalid dialog node."); return false; }
        const FString Id = First(String(N, TEXT("node_identifier")), String(N, TEXT("id")));
        if (Id.IsEmpty() || Identifiers.Contains(Id)) { Error = TEXT("Dialog has an empty or duplicate node identifier."); return false; }
        Identifiers.Add(Id); N->SetStringField(TEXT("node_identifier"), Id);
        for (const FString Key : { FString(TEXT("options")), FString(TEXT("conditionOptions")) })
        {
            auto Options = Array(N, Key);
            Options.StableSort([](const auto& A, const auto& B) { return Number(AsObject(A), TEXT("order_index")) < Number(AsObject(B), TEXT("order_index")); });
            N->SetArrayField(Key, Options);
        }
    }
    Data->SetArrayField(TEXT("nodes"), Nodes);
    Data->SetStringField(TEXT("startingNode"), First(String(Data, TEXT("startingNode")), First(String(Data, TEXT("startingSection")), Nodes.IsEmpty() ? FString() : String(AsObject(Nodes[0]), TEXT("node_identifier")))));
    Out.GraphJson = Write(Data); return true;
}
