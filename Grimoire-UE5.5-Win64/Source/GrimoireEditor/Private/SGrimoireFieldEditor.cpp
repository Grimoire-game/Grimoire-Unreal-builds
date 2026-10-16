#include "SGrimoireFieldEditor.h"
#include "GrimoireJson.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
using namespace GrimoireJson;
bool SGrimoireFieldEditor::Supports(const FString& Kind)
{ return Kind == TEXT("text") || Kind == TEXT("number") || Kind == TEXT("boolean") || Kind == TEXT("vector") || Kind == TEXT("media") || Kind == TEXT("link"); }
void SGrimoireFieldEditor::Construct(const FArguments& Args)
{
    Field = Args._Field; Changed = Args._OnChanged;
    auto V = ParseValue(Args._InitialJson);
    if (Field.Multiple && V && V->Type == EJson::Array) Values = V->AsArray();
    else if (!Field.Multiple) Values.Add(V && !V->IsNull() ? V : DefaultValue());
    ChildSlot [ SAssignNew(Root, SVerticalBox) ]; Rebuild();
}
TSharedPtr<FJsonValue> SGrimoireFieldEditor::DefaultValue() const
{
    if (Field.Kind == TEXT("number")) return MakeShared<FJsonValueNumber>(0);
    if (Field.Kind == TEXT("boolean")) return MakeShared<FJsonValueBoolean>(false);
    if (Field.Kind == TEXT("vector")) return MakeShared<FJsonValueObject>(Parse(TEXT("{\"x\":0,\"y\":0,\"z\":0}")));
    return MakeShared<FJsonValueString>(TEXT(""));
}
void SGrimoireFieldEditor::Set(int32 Index, TSharedPtr<FJsonValue> V)
{ if (Values.IsValidIndex(Index)) { Values[Index] = V; Notify(); } }
void SGrimoireFieldEditor::Notify()
{
    if (!Changed) return;
    Changed(WriteValue(Field.Multiple ? TSharedPtr<FJsonValue>(MakeShared<FJsonValueArray>(Values)) : Values.IsEmpty() ? nullptr : Values[0]));
}
void SGrimoireFieldEditor::Rebuild()
{
    Root->ClearChildren();
    for (int32 I = 0; I < Values.Num(); ++I)
    {
        const auto V = Values[I]; auto Row = SNew(SHorizontalBox);
        if (Field.Kind == TEXT("boolean"))
            Row->AddSlot().FillWidth(1) [ SNew(SCheckBox).IsChecked(V && V->Type == EJson::Boolean && V->AsBool() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
                .OnCheckStateChanged_Lambda([this, I](ECheckBoxState State) { Set(I, MakeShared<FJsonValueBoolean>(State == ECheckBoxState::Checked)); }) ];
        else if (Field.Kind == TEXT("number"))
            Row->AddSlot().FillWidth(1) [ SNew(SSpinBox<double>).MinValue(TOptional<double>()).MaxValue(TOptional<double>()).Value(V && V->Type == EJson::Number ? V->AsNumber() : 0)
                .OnValueChanged_Lambda([this, I](double N) { if (FMath::IsFinite(N)) Set(I, MakeShared<FJsonValueNumber>(N)); }) ];
        else if (Field.Kind == TEXT("vector"))
        {
            for (const FString Axis : { FString(TEXT("x")), FString(TEXT("y")), FString(TEXT("z")) })
            {
                // Preserve two-dimensional vectors without introducing an extra axis.
                if (Axis == TEXT("z") && AsObject(V) && !AsObject(V)->HasField(Axis)) continue;
                Row->AddSlot().AutoWidth().Padding(6, 0) [ SNew(STextBlock).Text(FText::FromString(Axis.ToUpper())) ];
                Row->AddSlot().FillWidth(1) [ SNew(SSpinBox<double>).MinValue(TOptional<double>()).MaxValue(TOptional<double>()).Value(Number(AsObject(V), Axis))
                    .OnValueChanged_Lambda([this, I, Axis](double N)
                    { if (!FMath::IsFinite(N)) return; auto O = AsObject(Values[I]); O = O ? Parse(Write(O)) : MakeShared<FJsonObject>(); O->SetNumberField(Axis, N); Set(I, MakeShared<FJsonValueObject>(O)); }) ];
            }
        }
        else
            Row->AddSlot().FillWidth(1) [ SNew(SEditableTextBox).Text(FText::FromString(V && V->Type == EJson::String ? V->AsString() : FString()))
                .OnTextChanged_Lambda([this, I](const FText& T) { Set(I, MakeShared<FJsonValueString>(T.ToString())); }) ];
        if (Field.Multiple) Row->AddSlot().AutoWidth().Padding(6, 0) [ SNew(SButton).Text(FText::FromString(TEXT("Remove")))
            .OnClicked_Lambda([this, I] { Values.RemoveAt(I); Notify(); Rebuild(); return FReply::Handled(); }) ];
        Root->AddSlot().AutoHeight().Padding(0, 2) [ Row ];
    }
    if (Field.Multiple) Root->AddSlot().AutoHeight() [ SNew(SButton).Text(FText::FromString(TEXT("Add value")))
        .OnClicked_Lambda([this] { Values.Add(DefaultValue()); Notify(); Rebuild(); return FReply::Handled(); }) ];
}
