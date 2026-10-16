#include "GrimoireDialogPlayer.h"
#include "GrimoireJson.h"
#include "GrimoireSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Internationalization/Regex.h"
using namespace GrimoireJson;

namespace
{
    FString ScalarText(const TSharedPtr<FJsonValue>& V)
    {
        if (!V || V->IsNull()) return FString();
        if (V->Type == EJson::String) return V->AsString();
        return WriteValue(V);
    }
    double ScalarNumber(const TSharedPtr<FJsonValue>& V)
    {
        if (!V || V->IsNull()) return 0;
        if (V->Type == EJson::Boolean) return V->AsBool() ? 1 : 0;
        double N = 0; LexTryParseString(N, *ScalarText(V)); return N;
    }
    bool ScalarBool(const TSharedPtr<FJsonValue>& V)
    {
        if (!V || V->IsNull()) return false;
        if (V->Type == EJson::Boolean) return V->AsBool();
        if (V->Type == EJson::Number) return V->AsNumber() != 0;
        const FString S = ScalarText(V); return S.Equals(TEXT("true"), ESearchCase::IgnoreCase) || S == TEXT("1");
    }
    bool Equal(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
    {
        const bool NA = !A || A->IsNull(), NB = !B || B->IsNull();
        if (NA || NB) return NA == NB;
        if (A->Type == EJson::Boolean || B->Type == EJson::Boolean) return ScalarBool(A) == ScalarBool(B);
        double AN = 0, BN = 0;
        if (LexTryParseString(AN, *ScalarText(A)) && LexTryParseString(BN, *ScalarText(B))) return FMath::Abs(AN - BN) < 1.e-9;
        return ScalarText(A) == ScalarText(B);
    }
    FString PickKey(const FString& Dialog, const FObject& N, const FObject& Option)
    { return Dialog + TEXT("/") + String(N, TEXT("node_identifier")) + TEXT("/") + First(String(Option, TEXT("id")), String(Option, TEXT("option_text"))); }
    bool IsEnd(const FObject& N) { const auto T = String(N, TEXT("type")); return T == TEXT("end-dialog") || T == TEXT("end-text"); }
    UGrimoireSubsystem* RuntimeState(const UGrimoireDialogPlayer* Player)
    { auto* World = Player->GetWorld(); auto* Game = World ? World->GetGameInstance() : nullptr; return Game ? Game->GetSubsystem<UGrimoireSubsystem>() : nullptr; }
    TSharedPtr<FJsonValue> CoerceVariable(const FObject& Definition, TSharedPtr<FJsonValue> V, bool Clamp)
    {
        const FString Type = String(Definition, TEXT("type"));
        if (Type == TEXT("boolean")) return MakeShared<FJsonValueBoolean>(ScalarBool(V));
        if (Type == TEXT("text")) return MakeShared<FJsonValueString>(ScalarText(V));
        if (Type == TEXT("number") || Type == TEXT("range") || Type == TEXT("range-selector"))
        {
            double N = ScalarNumber(V), Low = 0, High = 0;
            if (Clamp && Definition->TryGetNumberField(TEXT("rangeMin"), Low) && Definition->TryGetNumberField(TEXT("rangeMax"), High) && Low <= High) N = FMath::Clamp(N, Low, High);
            return MakeShared<FJsonValueNumber>(N);
        }
        return V;
    }
}

bool UGrimoireDialogPlayer::CompareValues(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, const FString& Op)
{
    if (Op == TEXT("!=")) return !Equal(A, B);
    if (Op == TEXT(">")) return ScalarNumber(A) > ScalarNumber(B);
    if (Op == TEXT("<")) return ScalarNumber(A) < ScalarNumber(B);
    if (Op == TEXT(">=")) return ScalarNumber(A) >= ScalarNumber(B);
    if (Op == TEXT("<=")) return ScalarNumber(A) <= ScalarNumber(B);
    if (Op == TEXT("contains")) return ScalarText(A).Contains(ScalarText(B));
    if (Op == TEXT("startsWith")) return ScalarText(A).StartsWith(ScalarText(B));
    if (Op == TEXT("endsWith")) return ScalarText(A).EndsWith(ScalarText(B));
    if (Op == TEXT("includes") || Op == TEXT("not_includes"))
    {
        bool Found = false;
        if (A && B && !A->IsNull() && !B->IsNull())
        {
            if (A->Type == EJson::Array) { for (const auto& Entry : A->AsArray()) if (ScalarText(Entry) == ScalarText(B)) Found = true; }
            else Found = ScalarText(A).Contains(ScalarText(B), ESearchCase::CaseSensitive);
        }
        return Op == TEXT("includes") ? Found : !Found;
    }
    if (Op == TEXT("in_range"))
    {
        FString Low, High; double Min = 0, Max = 0;
        if (ScalarText(B).Split(TEXT(".."), &Low, &High) && LexTryParseString(Min, *Low) && LexTryParseString(Max, *High))
            return ScalarNumber(A) >= Min && ScalarNumber(A) <= Max;
    }
    return Equal(A, B);
}

void UGrimoireDialogPlayer::BeginPlay()
{
    Super::BeginPlay();
    if (GetWorld() && GetWorld()->GetGameInstance())
        GetWorld()->GetGameInstance()->GetSubsystem<UGrimoireSubsystem>()->OnLanguageChanged.AddDynamic(this, &UGrimoireDialogPlayer::LanguageChanged);
}
void UGrimoireDialogPlayer::EndPlay(const EEndPlayReason::Type Reason)
{
    if (GetWorld() && GetWorld()->GetGameInstance())
        GetWorld()->GetGameInstance()->GetSubsystem<UGrimoireSubsystem>()->OnLanguageChanged.RemoveDynamic(this, &UGrimoireDialogPlayer::LanguageChanged);
    Stop(); Super::EndPlay(Reason);
}
void UGrimoireDialogPlayer::LanguageChanged(const FString&) { if (Language.IsEmpty()) RefreshLanguage(); }
FString UGrimoireDialogPlayer::EffectiveLanguage() const
{
    if (!Language.IsEmpty()) return Language;
    if (GetWorld() && GetWorld()->GetGameInstance()) return GetWorld()->GetGameInstance()->GetSubsystem<UGrimoireSubsystem>()->Language;
    return FString();
}
FText UGrimoireDialogPlayer::Localize(const FObject& Entry, const FString& TextKey)
{
    const FString Id = String(Entry, TEXT("string_id"));
    FString Text = String(Entry, TextKey);
    if (Database && Database->FindString(Id))
    {
        if (Database->HasTranslation(Id, EffectiveLanguage()) && !EffectiveLanguage().IsEmpty() && EffectiveLanguage() != Database->SourceLanguage)
            Text = Database->GetText(Id, EffectiveLanguage()).ToString();
        else Text = First(Text, Database->GetText(Id, FString()).ToString());
    }
    else for (const auto& V : Array(Entry, TEXT("translations")))
    {
        auto T = AsObject(V); const FString Locale = First(String(T, TEXT("language_code")), String(T, TEXT("languageCode")));
        const FString Translation = First(String(T, TEXT("translated_text")), String(T, TEXT("text")));
        if (!EffectiveLanguage().IsEmpty() && Locale.Equals(EffectiveLanguage(), ESearchCase::IgnoreCase) && !Translation.IsEmpty()) { Text = Translation; break; }
    }
    FString Result; int32 End = 0;
    FRegexMatcher Matcher(FRegexPattern(TEXT("#object:([^;#\\s]+);([^#\\s]+)#")), Text);
    while (Matcher.FindNext())
    {
        Result += Text.Mid(End, Matcher.GetMatchBeginning() - End);
        auto Ref = MakeShared<FJsonObject>(); Ref->SetStringField(TEXT("source"), TEXT("object_field"));
        Ref->SetStringField(TEXT("objectId"), Matcher.GetCaptureGroup(1)); Ref->SetStringField(TEXT("fieldId"), Matcher.GetCaptureGroup(2));
        auto V = Resolve(Ref); FString Display = ScalarText(V);
        if (Database && V && V->Type == EJson::String)
        {
            if (const auto* E = Database->FindVariableElement(TEXT(""), V->AsString())) Display = E->Name;
            else if (const auto* O = Database->FindObject(V->AsString())) Display = O->Name;
        }
        Result += Display; End = Matcher.GetMatchEnding();
    }
    return FText::FromString(Result + Text.Mid(End));
}
bool UGrimoireDialogPlayer::LoadDialog(const FString& Id)
{
    const auto* D = Database ? Database->FindDialog(Id) : nullptr;
    if (!D) return false;
    auto NewGraph = Parse(D->GraphJson); if (!NewGraph) return false;
    Graph = NewGraph; CurrentDialogId = D->Id;
    for (const auto& V : Array(Graph, TEXT("variables")))
    {
        auto Definition = AsObject(V); const FString Key = D->Id + TEXT("/") + String(Definition, TEXT("name"));
        if (!Variables.Contains(Key)) Variables.Add(Key, CoerceVariable(Definition, Value(Definition, TEXT("initialValue")), false));
    }
    return true;
}
bool UGrimoireDialogPlayer::StartDialog(const FString& Id, const FString& StartNode)
{
    if (IsPlaying) Stop();
    Picked.Empty(); VisibleOptions.Empty(); Node.Reset(); CurrentLine = FGrimoireDialogLine(); LastEndReason.Empty(); LastRequirements.Empty();
    if (!LoadDialog(Id)) { Finish(TEXT("Missing dialog")); return false; }
    LastRequirements = GetDialogRequirements(Id);
    if (WarnAboutMissingData) for (const auto& R : LastRequirements) if (R.Missing)
        UE_LOG(LogTemp, Warning, TEXT("Grimoire: %s [%s] %s"), *R.UsedBy, *R.Name, *R.Message);
    IsPlaying = true;
    return Run(First(StartNode, String(Graph, TEXT("startingNode"))));
}
bool UGrimoireDialogPlayer::ResolveLink(const FObject& Link, FString& Target, const FString& Suffix)
{
    const FString External = String(Link, TEXT("next_dialog") + Suffix);
    Target = First(String(Link, TEXT("next_node") + Suffix), First(String(Link, TEXT("next_field") + Suffix), String(Link, TEXT("next_section") + Suffix)));
    if (!External.IsEmpty())
    {
        if (!LoadDialog(External)) { Finish(TEXT("Missing linked dialog: ") + External); return false; }
        Target = First(String(Link, TEXT("next_dialog_node") + Suffix), String(Link, TEXT("next_dialog_section") + Suffix));
        Target = First(Target, String(Graph, TEXT("startingNode")));
    }
    if (Target.IsEmpty()) { Finish(TEXT("Completed")); return false; }
    return true;
}
void UGrimoireDialogPlayer::Follow(const FObject& Link)
{ FString Target; if (ResolveLink(Link, Target)) Run(Target); }
bool UGrimoireDialogPlayer::Run(const FString& FirstTarget)
{
    FString Target = FirstTarget;
    for (int32 Step = 0; Step < 512; ++Step)
    {
        Node.Reset(); VisibleOptions.Empty();
        for (const auto& V : Array(Graph, TEXT("nodes")))
        { auto Candidate = AsObject(V); if (String(Candidate, TEXT("node_identifier")) == Target || String(Candidate, TEXT("id")) == Target) { Node = Candidate; break; } }
        if (!Node) { Finish(TEXT("Missing node: ") + Target); return false; }
        const FString Type = String(Node, TEXT("type"));
        if (Type == TEXT("context") && ShowContextNotes && !Localize(Node, TEXT("text")).IsEmpty()) { Show(); return true; }
        if (IsEnd(Node))
        { if (!Localize(Node, TEXT("text")).IsEmpty()) { Show(); return true; } Finish(TEXT("Completed")); return true; }
        if (Type != TEXT("context") && Type != TEXT("condition") && Type != TEXT("setter") && Type != TEXT("jump") && Type != TEXT("external-dialog"))
        { Show(); return true; }
        FObject Link = Node; FString Suffix;
        if (Type == TEXT("setter")) Apply(Object(Node, TEXT("setter")));
        if (Type == TEXT("condition"))
        {
            const auto Condition = Object(Node, TEXT("condition"));
            auto SubjectRef = Object(Condition, TEXT("subjectRef"));
            auto Subject = SubjectRef && !String(SubjectRef, TEXT("source")).IsEmpty() ? Resolve(SubjectRef) : Variable(String(Condition, TEXT("variable")));
            auto Branches = Array(Node, TEXT("conditionOptions"));
            if (Branches.IsEmpty()) Suffix = ScalarBool(Subject) ? TEXT("_true") : TEXT("_false");
            else
            {
                Link.Reset();
                for (const auto& V : Branches)
                {
                    auto B = AsObject(V); auto Ref = Object(B, TEXT("valueRef"));
                    auto Compare = Ref && !String(Ref, TEXT("source")).IsEmpty() ? Resolve(Ref) : Value(B, TEXT("condition_value"));
                    if (CompareValues(Subject, Compare, String(B, TEXT("operator")))) { Link = B; break; }
                }
                if (!Link) { Finish(TEXT("No condition branch matched")); return false; }
            }
        }
        if (!ResolveLink(Link, Target, Suffix)) return LastEndReason == TEXT("Completed");
    }
    Finish(TEXT("Automatic node limit exceeded (possible loop)")); return false;
}
void UGrimoireDialogPlayer::Show()
{
    if (!Node) return;
    CurrentLine = FGrimoireDialogLine(); VisibleOptions.Empty();
    CurrentLine.DialogId = CurrentDialogId; CurrentLine.Language = EffectiveLanguage();
    Database->GetDialogNode(CurrentDialogId, String(Node, TEXT("node_identifier")), CurrentLine.Node);
    CurrentLine.IsContext = CurrentLine.Node.Type == TEXT("context");
    CurrentLine.NodeId = String(Node, TEXT("node_identifier")); CurrentLine.Text = Localize(Node, TEXT("text"));
    CurrentLine.SpeakerId = String(Node, TEXT("speaker")); CurrentLine.IsLast = IsEnd(Node);
    const FString Mode = String(Graph, TEXT("speakerMode"));
    if (CurrentLine.IsContext) CurrentLine.SpeakerId.Empty();
    else if (Mode == TEXT("object"))
    { const auto* O = Database->FindObject(CurrentLine.SpeakerId); if (O) CurrentLine.Speaker = FText::FromString(O->Name); }
    else if (Mode == TEXT("type"))
    { const auto* E = Database->FindVariableElement(TEXT(""), CurrentLine.SpeakerId); if (E) CurrentLine.Speaker = FText::FromString(E->Name); }
    else if (Mode != TEXT("type")) CurrentLine.Speaker = FText::FromString(CurrentLine.SpeakerId);
    if (!CurrentLine.IsContext) CurrentLine.VoiceUrl = First(Database->GetVoiceUrl(String(Node, TEXT("string_id")), EffectiveLanguage()), String(Node, TEXT("voice_url")));
    if (String(Node, TEXT("type")) == TEXT("question"))
    {
        for (const auto& V : Array(Node, TEXT("options")))
        {
            auto O = AsObject(V); if (!O || !Evaluate(Object(O, TEXT("visibilityCondition")))) continue;
            if (Bool(Node, TEXT("pickOnce")) && !Bool(O, TEXT("alwaysAvailable")) && Picked.Contains(PickKey(CurrentDialogId, Node, O))) continue;
            FGrimoireDialogChoice Choice; Choice.Index = VisibleOptions.Num(); Choice.Text = Localize(O, TEXT("option_text"));
            Choice.OptionId = String(O, TEXT("id")); Choice.StringId = String(O, TEXT("string_id"));
            VisibleOptions.Add(O); CurrentLine.Choices.Add(Choice);
        }
    }
    if (CurrentLine.Choices.IsEmpty()) OnLine.Broadcast(CurrentLine); else OnChoices.Broadcast(CurrentLine);
}
void UGrimoireDialogPlayer::Next()
{
    if (!IsPlaying || !Node || !VisibleOptions.IsEmpty()) return;
    if (IsEnd(Node)) Finish(TEXT("Completed")); else Follow(Node);
}
void UGrimoireDialogPlayer::Choose(int32 Index)
{
    if (!IsPlaying || !VisibleOptions.IsValidIndex(Index)) return;
    const auto Option = VisibleOptions[Index];
    if (Bool(Node, TEXT("pickOnce"))) Picked.Add(PickKey(CurrentDialogId, Node, Option));
    Apply(Object(Option, TEXT("variableAction"))); Follow(Option);
}
void UGrimoireDialogPlayer::RefreshLanguage() { if (IsPlaying) Show(); }
void UGrimoireDialogPlayer::Stop() { if (IsPlaying) Finish(TEXT("Stopped")); }
void UGrimoireDialogPlayer::Finish(const FString& Reason)
{
    IsPlaying = false; LastEndReason = Reason; Node.Reset(); VisibleOptions.Empty(); CurrentLine = FGrimoireDialogLine();
    if (!KeepVariablesBetweenRuns) ResetVariables();
    OnDialogEnded.Broadcast(Reason);
}
void UGrimoireDialogPlayer::ResetVariables() { Variables.Empty(); ObjectOverrides.Empty(); ElementOverrides.Empty(); }
bool UGrimoireDialogPlayer::GetCurrentNode(FGrimoireDialogNodeData& Out) const
{ Out = FGrimoireDialogNodeData(); return Node && Database && Database->GetDialogNode(CurrentDialogId, String(Node, TEXT("node_identifier")), Out); }
TArray<FGrimoireDialogRequirement> UGrimoireDialogPlayer::GetDialogRequirements(const FString& Key) const
{ return Database ? Database->GetDialogRequirements(Key, this) : TArray<FGrimoireDialogRequirement>(); }
bool UGrimoireDialogPlayer::SetVariableJson(const FString& Id, const FString& Name, const FString& Json)
{
    auto V = ParseValue(Json); const auto* D = Database ? Database->FindDialog(Id) : nullptr;
    if (!V || !D || Name.IsEmpty()) return false;
    for (const auto& Raw : Array(Parse(D->GraphJson), TEXT("variables")))
    { auto Definition = AsObject(Raw); if (String(Definition, TEXT("name")) == Name) { V = CoerceVariable(Definition, V, true); break; } }
    Variables.Add(D->Id + TEXT("/") + Name, V); return true;
}
FString UGrimoireDialogPlayer::GetVariableJson(const FString& Id, const FString& Name) const
{
    const auto* D = Database ? Database->FindDialog(Id) : nullptr; if (!D) return TEXT("null");
    if (const auto* V = Variables.Find(D->Id + TEXT("/") + Name)) return WriteValue(*V);
    for (const auto& V : Array(Parse(D->GraphJson), TEXT("variables")))
        if (String(AsObject(V), TEXT("name")) == Name) return WriteValue(CoerceVariable(AsObject(V), Value(AsObject(V), TEXT("initialValue")), false));
    return TEXT("null");
}
void UGrimoireDialogPlayer::SetNumber(const FString& Id, const FString& Name, double V) { SetVariableJson(Id, Name, WriteValue(MakeShared<FJsonValueNumber>(V))); }
void UGrimoireDialogPlayer::SetBoolean(const FString& Id, const FString& Name, bool V) { SetVariableJson(Id, Name, V ? TEXT("true") : TEXT("false")); }
void UGrimoireDialogPlayer::SetText(const FString& Id, const FString& Name, const FString& V) { SetVariableJson(Id, Name, WriteValue(MakeShared<FJsonValueString>(V))); }
TSharedPtr<FJsonValue> UGrimoireDialogPlayer::Variable(const FString& Name) const
{ const auto* V = Variables.Find(CurrentDialogId + TEXT("/") + Name); return V ? *V : nullptr; }
FString UGrimoireDialogPlayer::ReadExternalValue_Implementation(const FString&) { return FString(); }
bool UGrimoireDialogPlayer::WriteExternalValue_Implementation(const FString&, const FString&) { return false; }
TSharedPtr<FJsonValue> UGrimoireDialogPlayer::Resolve(const FObject& Ref)
{
    const FString Source = String(Ref, TEXT("source"));
    if (Source == TEXT("literal")) return Value(Ref, TEXT("value"));
    if (Source == TEXT("dialog_variable")) return Variable(String(Ref, TEXT("variableName")));
    const FString Provided = ReadExternalValue(Write(Ref));
    if (!Provided.IsEmpty()) return ParseValue(Provided);
    if (Source == TEXT("type_element") && Database)
    {
        const FString Type = String(Ref, TEXT("typeId")), Element = String(Ref, TEXT("elementId"));
        FString Json;
        if (auto* State = RuntimeState(this)) { if (State->GetGameVariableJson(Database, Type, Element, Json)) return ParseValue(Json); }
        const auto* E = Database->FindVariableElement(Type, Element); if (!E) return nullptr;
        if (const auto* V = ElementOverrides.Find(E->Id)) return *V;
        auto V = ParseValue(E->AuthoredValueJson); return V && !V->IsNull() ? V : MakeShared<FJsonValueString>(E->Id);
    }
    if (Source == TEXT("object_field") && Database)
    {
        const FString ObjectId = String(Ref, TEXT("objectId")), FieldId = String(Ref, TEXT("fieldId"));
        FString Json;
        if (auto* State = RuntimeState(this)) { if (State->GetObjectFieldJson(Database, ObjectId, FieldId, Json)) return ParseValue(Json); }
        if (const auto* Override = ObjectOverrides.Find(ObjectId + TEXT("/") + FieldId)) return *Override;
        const auto* O = Database->FindObject(ObjectId); const auto* F = O ? O->FindField(FieldId) : nullptr;
        if (F) return ParseValue(F->StoredValueJson());
    }
    return nullptr;
}
bool UGrimoireDialogPlayer::Evaluate(const FObject& C)
{
    if (!C) return true;
    auto SR = Object(C, TEXT("subjectRef")), VR = Object(C, TEXT("valueRef"));
    if (String(C, TEXT("variable")).IsEmpty() && String(SR, TEXT("source")).IsEmpty()) return true;
    auto A = !String(SR, TEXT("source")).IsEmpty() ? Resolve(SR) : Variable(String(C, TEXT("variable")));
    auto B = !String(VR, TEXT("source")).IsEmpty() ? Resolve(VR) : Value(C, TEXT("value"));
    return CompareValues(A, B, String(C, TEXT("operator")));
}
void UGrimoireDialogPlayer::Apply(const FObject& A)
{
    if (!A) return;
    auto Target = Object(A, TEXT("targetRef")), ValueRef = Object(A, TEXT("valueRef"));
    const FString Source = String(Target, TEXT("source"));
    const FString Name = Source.IsEmpty() ? String(A, TEXT("variable")) : String(Target, TEXT("variableName"));
    auto Incoming = !String(ValueRef, TEXT("source")).IsEmpty() ? Resolve(ValueRef) : Value(A, TEXT("value"));
    auto Current = Source.IsEmpty() ? Variable(Name) : Resolve(Target);
    const FString Op = String(A, TEXT("operator"));
    if (Op == TEXT("increment")) Incoming = MakeShared<FJsonValueNumber>(ScalarNumber(Current) + ScalarNumber(Incoming));
    if (Op == TEXT("decrement")) Incoming = MakeShared<FJsonValueNumber>(ScalarNumber(Current) - ScalarNumber(Incoming));
    if (Source == TEXT("literal")) return;
    if (!Source.IsEmpty() && Source != TEXT("dialog_variable"))
    {
        if (WriteExternalValue(Write(Target), WriteValue(Incoming))) return;
        if (Source == TEXT("type_element") && Database)
        {
            const FString Type = String(Target, TEXT("typeId")), Element = String(Target, TEXT("elementId"));
            if (auto* State = RuntimeState(this)) State->SetGameVariableJson(Database, Type, Element, WriteValue(Incoming), Bool(A, TEXT("enforceRange"), true));
            else if (const auto* E = Database->FindVariableElement(Type, Element))
            {
                if (Incoming && Incoming->Type == EJson::Number && E->HasRange && Bool(A, TEXT("enforceRange"), true))
                    Incoming = MakeShared<FJsonValueNumber>(FMath::Clamp(Incoming->AsNumber(), E->RangeMin, E->RangeMax));
                ElementOverrides.Add(E->Id, Incoming);
            }
        }
        if (Source == TEXT("object_field"))
        {
            const FString ObjectId = String(Target, TEXT("objectId")), FieldId = String(Target, TEXT("fieldId"));
            if (auto* State = RuntimeState(this)) State->SetObjectFieldJson(Database, ObjectId, FieldId, WriteValue(Incoming));
            else ObjectOverrides.Add(ObjectId + TEXT("/") + FieldId, Incoming);
        }
        return;
    }
    if (Name.IsEmpty()) return;
    for (const auto& V : Array(Graph, TEXT("variables")))
    { auto Definition = AsObject(V); if (String(Definition, TEXT("name")) == Name) { Incoming = CoerceVariable(Definition, Incoming, Bool(A, TEXT("enforceRange"), true)); break; } }
    Variables.Add(CurrentDialogId + TEXT("/") + Name, Incoming);
}
