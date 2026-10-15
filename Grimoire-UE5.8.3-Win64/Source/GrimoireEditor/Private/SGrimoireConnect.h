#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "GrimoireEditorSubsystem.h"
class SVerticalBox;
class SEditableTextBox;
class SScrollBox;

class SGrimoireConnect : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGrimoireConnect) {} SLATE_END_ARGS()
    void Construct(const FArguments& Arguments);
    virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;
private:
    TWeakObjectPtr<UGrimoireEditorSubsystem> Model;
    TSharedPtr<SVerticalBox> Root;
    TSharedPtr<SScrollBox> BodyScroll;
    FString BodyTab, BodySearch;
    TArray<TSharedPtr<FString>> GameOptions, LanguageOptions, UserOptions, TemplateOptions;
    TMap<FString, FString> TemplateNames;
    TMap<FString, FString> FieldBuffers;
    FString Tab = TEXT("Objects"), Search, SelectedObject, SelectedString, Assignee;
    FString SelectedDialog;
    FString DraftName, DraftCode, DraftTemplate, TranslationBuffer;
    int32 Revision = -1;
    void Rebuild();
    void BuildObjects(const TSharedRef<SVerticalBox>& Body);
    void BuildStrings(const TSharedRef<SVerticalBox>& Body);
    void BuildDialogs(const TSharedRef<SVerticalBox>& Body);
    void BuildVariables(const TSharedRef<SVerticalBox>& Body);
    void BuildObjectDetails(const TSharedRef<SVerticalBox>& Body, const FGrimoireObject& Object);
    void BuildTasks(const TSharedRef<SVerticalBox>& Body);
    void BuildScene(const TSharedRef<SVerticalBox>& Body);
    void BuildCreate(const TSharedRef<SVerticalBox>& Body);
    TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> Action, bool RequireSession = false);
    void Text(const TSharedRef<SVerticalBox>& Box, const FString& Value);
};
