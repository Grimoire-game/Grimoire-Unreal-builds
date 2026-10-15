#include "GrimoireData.h"
#include "GrimoireJson.h"
using namespace GrimoireJson;

const FGrimoireGameVariable* UGrimoireDatabase::FindGameVariable(const FString& Key) const
{
    if (Key.IsEmpty()) return nullptr;
    for (const auto& V : GameVariables) if (V.Id == Key) return &V;
    const FGrimoireGameVariable* Match = nullptr;
    for (const auto& V : GameVariables) if (V.Name.Equals(Key.TrimStartAndEnd(), ESearchCase::IgnoreCase))
    { if (Match) return nullptr; Match = &V; }
    return Match;
}
const FGrimoireVariableElement* UGrimoireDatabase::FindVariableElement(const FString& Variable, const FString& Element) const
{
    if (Element.IsEmpty()) return nullptr;
    if (Variable.IsEmpty())
    { for (const auto& V : GameVariables) for (const auto& E : V.Elements) if (E.Id == Element) return &E; return nullptr; }
    const auto* V = FindGameVariable(Variable); if (!V) return nullptr;
    for (const auto& E : V->Elements) if (E.Id == Element) return &E;
    const FGrimoireVariableElement* Match = nullptr;
    for (const auto& E : V->Elements) if (E.Name.Equals(Element.TrimStartAndEnd(), ESearchCase::IgnoreCase))
    { if (Match) return nullptr; Match = &E; }
    return Match;
}
bool UGrimoireDatabase::GetGameVariable(const FString& Key, FGrimoireGameVariable& Out) const
{ const auto* V = FindGameVariable(Key); Out = V ? *V : FGrimoireGameVariable(); return V != nullptr; }
bool UGrimoireDatabase::GetVariableElement(const FString& Variable, const FString& Element, FGrimoireVariableElement& Out) const
{ const auto* E = FindVariableElement(Variable, Element); Out = E ? *E : FGrimoireVariableElement(); return E != nullptr; }
bool UGrimoireDatabase::ParseGameVariable(const FObject& Resource, FGrimoireGameVariable& Out, FString& Error)
{
    Out = FGrimoireGameVariable(); Out.Id = String(Resource, TEXT("id"));
    if (Out.Id.IsEmpty()) { Error = TEXT("Variable is missing its id. Previous variables were preserved."); return false; }
    Out.Name = String(Resource, TEXT("name")); Out.ElementType = String(Resource, TEXT("element_type"));
    Out.Description = String(Resource, TEXT("description")); Out.Group = String(Resource, TEXT("group"));
    auto Elements = Array(Resource, TEXT("elements"));
    Elements.StableSort([](const auto& A, const auto& B) { return Number(AsObject(A), TEXT("order_index"), MAX_int32) < Number(AsObject(B), TEXT("order_index"), MAX_int32); });
    TSet<FString> Ids;
    for (const auto& Raw : Elements)
    {
        auto E = AsObject(Raw); FGrimoireVariableElement Item;
        Item.Id = String(E, TEXT("id"));
        if (Item.Id.IsEmpty() || Ids.Contains(Item.Id)) { Error = TEXT("Variable contains an invalid or duplicate element id."); return false; }
        Ids.Add(Item.Id); Item.Name = String(E, TEXT("name")); Item.Description = String(E, TEXT("description"));
        Item.StringId = String(E, TEXT("string_id")); Item.AuthoredValueJson = WriteValue(Value(E, TEXT("value")));
        Item.HasRange = E->TryGetNumberField(TEXT("range_min"), Item.RangeMin) && E->TryGetNumberField(TEXT("range_max"), Item.RangeMax) && Item.RangeMin <= Item.RangeMax;
        Out.Elements.Add(MoveTemp(Item));
    }
    return true;
}

namespace
{
    TArray<FGrimoireTranslation> ReadTranslations(const FObject& Entry)
    {
        TArray<FGrimoireTranslation> Result;
        for (const auto& V : Array(Entry, TEXT("translations")))
        {
            auto T = AsObject(V); FGrimoireTranslation Item;
            Item.Language = First(String(T, TEXT("language_code")), String(T, TEXT("languageCode")));
            Item.Text = First(String(T, TEXT("translated_text")), String(T, TEXT("text")));
            Item.VoiceUrl = First(String(T, TEXT("voice_url")), String(T, TEXT("voiceUrl")));
            Result.Add(MoveTemp(Item));
        }
        return Result;
    }
    FString NextNode(const FObject& Entry)
    { return First(String(Entry, TEXT("next_node")), First(String(Entry, TEXT("next_field")), String(Entry, TEXT("next_section")))); }
    void AddStringData(const UGrimoireDatabase* DB, FGrimoireDialogNodeData& N)
    {
        if (const auto* S = DB->FindString(N.StringId))
        { N.SourceText = First(N.SourceText, S->Source); N.VoiceUrl = First(S->VoiceUrl, N.VoiceUrl); N.Translations = S->Translations; }
        for (auto& O : N.Options) if (const auto* S = DB->FindString(O.StringId))
        { O.SourceText = First(O.SourceText, S->Source); O.Translations = S->Translations; }
    }
}
FGrimoireDialogNodeData UGrimoireDatabase::ParseNodeData(const FString& Dialog, const FObject& N)
{
    FGrimoireDialogNodeData Out; if (!N) return Out;
    Out.DialogId = Dialog; Out.Id = String(N, TEXT("id")); Out.Identifier = First(String(N, TEXT("node_identifier")), Out.Id);
    Out.Name = String(N, TEXT("name")); Out.Type = String(N, TEXT("type")); Out.SourceText = String(N, TEXT("text"));
    Out.StringId = String(N, TEXT("string_id")); Out.SpeakerId = String(N, TEXT("speaker")); Out.VoiceUrl = String(N, TEXT("voice_url"));
    Out.NextNode = NextNode(N); Out.NextDialog = String(N, TEXT("next_dialog"));
    Out.NextDialogNode = First(String(N, TEXT("next_dialog_node")), String(N, TEXT("next_dialog_section")));
    Out.PickOnce = Bool(N, TEXT("pickOnce")); Out.Translations = ReadTranslations(N);
    Out.ConditionJson = Write(Object(N, TEXT("condition"))); Out.SetterJson = Write(Object(N, TEXT("setter")));
    for (const auto& B : Array(N, TEXT("conditionOptions"))) Out.ConditionBranchesJson.Add(WriteValue(B));
    for (const auto& V : Array(N, TEXT("options")))
    {
        auto O = AsObject(V); if (!O) continue;
        FGrimoireDialogOptionData Option; Option.Id = String(O, TEXT("id")); Option.SourceText = String(O, TEXT("option_text"));
        Option.StringId = String(O, TEXT("string_id")); Option.NextNode = NextNode(O); Option.NextDialog = String(O, TEXT("next_dialog"));
        Option.NextDialogNode = First(String(O, TEXT("next_dialog_node")), String(O, TEXT("next_dialog_section")));
        Option.AlwaysAvailable = Bool(O, TEXT("alwaysAvailable")); Option.Translations = ReadTranslations(O);
        Option.VisibilityConditionJson = Write(Object(O, TEXT("visibilityCondition"))); Option.VariableActionJson = Write(Object(O, TEXT("variableAction")));
        Option.RawJson = Write(O); Out.Options.Add(MoveTemp(Option));
    }
    for (const auto& Field : N->Values) Out.JsonFields.Add(Field.Key, WriteValue(Field.Value));
    Out.RawJson = Write(N); return Out;
}
TArray<FGrimoireDialogNodeData> UGrimoireDatabase::GetDialogNodes(const FString& Key) const
{
    TArray<FGrimoireDialogNodeData> Out; const auto* D = FindDialog(Key); if (!D) return Out;
    for (const auto& V : Array(Parse(D->GraphJson), TEXT("nodes"))) { auto N = ParseNodeData(D->Id, AsObject(V)); AddStringData(this, N); Out.Add(MoveTemp(N)); }
    return Out;
}
bool UGrimoireDatabase::GetDialogNode(const FString& Key, const FString& NodeId, FGrimoireDialogNodeData& Out) const
{
    Out = FGrimoireDialogNodeData(); const auto* D = FindDialog(Key); if (!D || NodeId.IsEmpty()) return false;
    for (const auto& V : Array(Parse(D->GraphJson), TEXT("nodes")))
    {
        auto N = AsObject(V);
        if (String(N, TEXT("id")) == NodeId || String(N, TEXT("node_identifier")) == NodeId)
        { Out = ParseNodeData(D->Id, N); AddStringData(this, Out); return true; }
    }
    return false;
}
