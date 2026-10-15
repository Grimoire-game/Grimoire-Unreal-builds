#include "GrimoireData.h"
#include "GrimoireJson.h"
#include "GrimoireObjectLink.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Internationalization/Regex.h"
using namespace GrimoireJson;

TArray<FGrimoireDialogRequirement> UGrimoireDatabase::GetDialogRequirements(const FString& Key, const UObject* WorldContext) const
{
    TArray<FGrimoireDialogRequirement> Out;
    TSet<FString> SceneObjects, Visited, Reported;
    UWorld* World = WorldContext && GEngine ? GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::ReturnNull) : nullptr;
    if (World) for (TActorIterator<AActor> It(World); It; ++It)
    {
        TArray<UGrimoireObjectLink*> Links; It->GetComponents(Links);
        for (const auto* Link : Links) if (Link && Link->Database && (Link->Database == this || (!GameId.IsEmpty() && Link->Database->GameId == GameId)))
            if (const auto* O = FindObject(Link->ObjectKey)) SceneObjects.Add(O->Id);
    }
    auto Add = [&Out, &Reported](const FString& Kind, const FString& Id, const FString& Field, const FString& Name, const FString& Where,
        bool Missing, bool InScene, bool Fallback, const FString& Message)
    {
        const FString Identity = Kind + TEXT("/") + Id + TEXT("/") + Field + TEXT("/") + Where;
        if (Reported.Contains(Identity)) return; Reported.Add(Identity);
        FGrimoireDialogRequirement R; R.Kind = Kind; R.ResourceId = Id; R.FieldId = Field; R.Name = Name; R.UsedBy = Where;
        R.Missing = Missing; R.InScene = InScene; R.UsesImportedFallback = Fallback; R.Message = Message; Out.Add(MoveTemp(R));
    };
    TArray<FString> Queue { Key };
    for (int32 Index = 0; Index < Queue.Num(); ++Index)
    {
        const auto* D = FindDialog(Queue[Index]);
        if (!D)
        { Add(TEXT("Dialog"), Queue[Index], TEXT(""), Queue[Index], TEXT("Dialog entry"), true, false, false, TEXT("Missing dialog. Import / update game.")); continue; }
        if (Visited.Contains(D->Id)) continue; Visited.Add(D->Id);
        auto Graph = Parse(D->GraphJson); auto Nodes = Array(Graph, TEXT("nodes"));
        const FString Label = First(D->Name, First(D->Key, D->Id));
        FGrimoireDialogNodeData Start;
        if (!GetDialogNode(D->Id, String(Graph, TEXT("startingNode")), Start))
            Add(TEXT("Node"), D->Id, String(Graph, TEXT("startingNode")), TEXT("Starting node"), Label, true, false, false, TEXT("Starting node is missing. Re-import; if it persists, fix the starting node in Grimoire."));
        TSet<FString> LocalVariables;
        for (const auto& V : Array(Graph, TEXT("variables"))) LocalVariables.Add(String(AsObject(V), TEXT("name")));
        for (const auto& RawNode : Nodes)
        {
            auto N = AsObject(RawNode); const FString Where = Label + TEXT(" / ") + First(String(N, TEXT("name")), String(N, TEXT("node_identifier")));
            auto CheckObject = [&](const FString& Id, const FString& Field, bool Speaker)
            {
                const auto* O = FindObject(Id); const auto* F = O && !Speaker ? O->FindField(Field) : nullptr;
                const bool Exists = O && (Speaker || F), InScene = O && SceneObjects.Contains(O->Id);
                const FString Name = (O ? O->Name : Id) + (Speaker ? FString() : TEXT(" / ") + (F ? F->Label : Field));
                Add(Speaker ? TEXT("Speaker") : TEXT("Object field"), O ? O->Id : Id, Field, Name, Where, !Exists, InScene, Exists && !InScene,
                    !O ? TEXT("Object not imported. Import / update game.") : !Speaker && !F ? TEXT("Field not imported. Re-import; verify the field still exists in Grimoire.") :
                    InScene ? TEXT("Object Link in this level; imported data and shared runtime values are available.") :
                    Speaker ? TEXT("Speaker name is imported. A scene Actor is optional.") : TEXT("Uses imported data and shared runtime values. Link an Actor if this object needs a scene representation."));
            };
            auto CheckElement = [&](const FString& Type, const FString& Id)
            {
                const auto* E = FindVariableElement(Type, Id);
                Add(TEXT("Game variable"), Type, Id, E ? E->Name : Id, Where, !E, false, E != nullptr,
                    E ? TEXT("Synced variable element is available to dialog setters and gameplay.") : TEXT("Variable element not imported. Click Sync variables. If it remains missing, fix the reference in Grimoire."));
            };
            const FString Speaker = String(N, TEXT("speaker"));
            if (!Speaker.IsEmpty() && String(N, TEXT("type")) != TEXT("context"))
            {
                if (String(Graph, TEXT("speakerMode")) == TEXT("object")) CheckObject(Speaker, TEXT(""), true);
                if (String(Graph, TEXT("speakerMode")) == TEXT("type")) CheckElement(TEXT(""), Speaker);
            }
            TFunction<void(const TSharedPtr<FJsonValue>&, int32)> Scan;
            Scan = [&](const TSharedPtr<FJsonValue>& V, int32 Depth)
            {
                if (!V || Depth > 64) return;
                if (V->Type == EJson::Array) { for (const auto& Child : V->AsArray()) Scan(Child, Depth + 1); return; }
                if (V->Type == EJson::String)
                {
                    FRegexMatcher Matcher(FRegexPattern(TEXT("#object:([^;#]+);([^#]+)#")), V->AsString());
                    while (Matcher.FindNext()) CheckObject(Matcher.GetCaptureGroup(1), Matcher.GetCaptureGroup(2), false);
                    return;
                }
                auto O = AsObject(V); if (!O) return;
                const FString Source = String(O, TEXT("source"));
                if (Source == TEXT("object_field")) CheckObject(String(O, TEXT("objectId")), String(O, TEXT("fieldId")), false);
                else if (Source == TEXT("type_element")) CheckElement(String(O, TEXT("typeId")), String(O, TEXT("elementId")));
                const FString Variable = Source == TEXT("dialog_variable") ? String(O, TEXT("variableName")) : String(O, TEXT("variable"));
                if (!Variable.IsEmpty() && !LocalVariables.Contains(Variable))
                    Add(TEXT("Dialog variable"), D->Id, Variable, Variable, Where, true, false, false, TEXT("Dialog variable is not declared. Re-import or add its definition in Grimoire."));
                const FString StringId = String(O, TEXT("string_id"));
                if (const auto* S = FindString(StringId))
                {
                    Scan(MakeShared<FJsonValueString>(S->Source), Depth + 1);
                    for (const auto& T : S->Translations) Scan(MakeShared<FJsonValueString>(T.Text), Depth + 1);
                }
                if (!StringId.IsEmpty() && !FindString(StringId))
                {
                    const bool HasText = !First(String(O, TEXT("text")), String(O, TEXT("option_text"))).IsEmpty();
                    Add(TEXT("String"), StringId, TEXT(""), StringId, Where, !HasText, false, HasText,
                        HasText ? TEXT("Using node source text. Import / update game to include the string's translations.") : TEXT("Text and string are missing. Import / update game."));
                }
                for (const FString Suffix : { FString(), FString(TEXT("_true")), FString(TEXT("_false")) })
                {
                    const FString External = String(O, TEXT("next_dialog") + Suffix);
                    const auto* TargetDialog = External.IsEmpty() ? D : FindDialog(External);
                    if (!External.IsEmpty())
                    {
                        Add(TEXT("Dialog"), External, TEXT(""), TargetDialog ? TargetDialog->Name : External, Where, !TargetDialog, false, false,
                            TargetDialog ? TEXT("Linked dialog imported.") : TEXT("Linked dialog missing. Import / update game."));
                        if (TargetDialog) Queue.AddUnique(TargetDialog->Id);
                    }
                    const FString Target = External.IsEmpty()
                        ? First(String(O, TEXT("next_node") + Suffix), First(String(O, TEXT("next_field") + Suffix), String(O, TEXT("next_section") + Suffix)))
                        : First(String(O, TEXT("next_dialog_node") + Suffix), String(O, TEXT("next_dialog_section") + Suffix));
                    FGrimoireDialogNodeData TargetNode;
                    if (TargetDialog && !Target.IsEmpty() && !GetDialogNode(TargetDialog->Id, Target, TargetNode))
                        Add(TEXT("Node"), TargetDialog->Id, Target, Target, Where, true, false, false, TEXT("Linked node is missing. Re-import; if it persists, repair the link in Grimoire."));
                }
                for (const auto& Pair : O->Values) Scan(Pair.Value, Depth + 1);
            };
            Scan(RawNode, 0);
        }
    }
    Out.StableSort([](const auto& A, const auto& B) { return A.Missing && !B.Missing; });
    return Out;
}
