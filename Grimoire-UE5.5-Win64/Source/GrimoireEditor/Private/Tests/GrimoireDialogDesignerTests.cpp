#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/EngineVersionComparison.h"
#include "GrimoireEditorSubsystem.h"
#include "GrimoireDialogWidget.h"
#include "GrimoireDialogChoiceWidget.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Image.h"
#include "Components/VerticalBox.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireDialogDesignerTest, "Grimoire.Editor.DialogDesignerCompatibility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrimoireDialogDesignerTest::RunTest(const FString&)
{
    auto CreateWidgetBlueprint = [](const TCHAR* BaseName)
    {
        return Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
            UUserWidget::StaticClass(), GetTransientPackage(),
            MakeUniqueObjectName(GetTransientPackage(), UWidgetBlueprint::StaticClass(), FName(BaseName)),
            BPTYPE_Normal, UWidgetBlueprint::StaticClass(), UWidgetBlueprintGeneratedClass::StaticClass()));
    };
    TStrongObjectPtr<UWidgetBlueprint> Dialog(CreateWidgetBlueprint(TEXT("GrimoireDesignerDialogTest")));
    TStrongObjectPtr<UWidgetBlueprint> Choice(CreateWidgetBlueprint(TEXT("GrimoireDesignerChoiceTest")));
    if (!TestNotNull(TEXT("Dialog Blueprint created"), Dialog.Get()) || !TestNotNull(TEXT("Choice Blueprint created"), Choice.Get())) return false;
    UWidgetTree* Tree = Dialog->WidgetTree;
    auto* Column = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("DialogColumn"));
    Tree->RootWidget = Column;
    Column->AddChild(Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("SpeakerName")));
    Column->AddChild(Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("DialogText")));
    Column->AddChild(Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Continue")));
    Column->AddChild(Tree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("Portrait")));

    TStrongObjectPtr<UGrimoireEditorSubsystem> Editor(NewObject<UGrimoireEditorSubsystem>());
    const FString Error = Editor->ConfigureDialogDesigner(Dialog.Get(), Choice.Get());
    TestEqual(TEXT("Designer configures and compiles both widgets"), Error, FString());
    TestTrue(TEXT("Dialog Blueprint compiles"), Dialog->Status != BS_Error);
    TestTrue(TEXT("Choice Blueprint compiles"), Choice->Status != BS_Error);
    TestTrue(TEXT("Dialog uses Grimoire parent"), Dialog->ParentClass == UGrimoireDialogWidget::StaticClass());
    TestTrue(TEXT("Choice uses Grimoire parent"), Choice->ParentClass == UGrimoireDialogChoiceWidget::StaticClass());
    if (!Error.IsEmpty() || !Dialog->GeneratedClass || !Choice->GeneratedClass) return false;
    const auto* Defaults = Cast<UGrimoireDialogWidget>(Dialog->GeneratedClass->GetDefaultObject());
    if (!TestNotNull(TEXT("Grimoire dialog defaults available"), Defaults)) return false;
    TestEqual(TEXT("Dialog text binding retained"), Defaults->DialogTextWidgetName, FName(TEXT("DialogText")));
    TestEqual(TEXT("Choices container configured"), Defaults->ChoicesWidgetName, FName(TEXT("ChoicesContainer")));
    TestTrue(TEXT("Generated choice class assigned"), Defaults->ChoiceWidgetClass.Get() == Choice->GeneratedClass);
    TestNotNull(TEXT("Choice button created"), Cast<UButton>(Choice->WidgetTree->FindWidget(TEXT("ChoiceButton"))));
    TestNotNull(TEXT("Choice label created"), Cast<UTextBlock>(Choice->WidgetTree->FindWidget(TEXT("ChoiceLabel"))));
#if !UE_VERSION_OLDER_THAN(5, 6, 0)
    TestTrue(TEXT("Dialog widget GUID registered"), Dialog->WidgetVariableNameToGuidMap.Contains(TEXT("ChoicesContainer")));
    TestTrue(TEXT("Choice widget GUID registered"), Choice->WidgetVariableNameToGuidMap.Contains(TEXT("ChoiceButton")));
#endif
    return true;
}
#endif
