#include "SGrimoireConnect.h"
#include "SGrimoireFieldEditor.h"
#include "GrimoireJson.h"
#include "GrimoireObjectLink.h"
#include "GrimoireTextLink.h"
#include "GrimoireDialogPlayer.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Styling/AppStyle.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/MessageDialog.h"
#include "ScopedTransaction.h"
using namespace GrimoireJson;

void SGrimoireConnect::Construct(const FArguments& Arguments)
{
    Model = GEditor->GetEditorSubsystem<UGrimoireEditorSubsystem>();
    ChildSlot [ SNew(SBorder).Padding(16) [ SAssignNew(Root, SVerticalBox) ] ]; Rebuild();
}
void SGrimoireConnect::Tick(const FGeometry& G, double Time, float Delta)
{ SCompoundWidget::Tick(G, Time, Delta); if (Model.IsValid() && Revision != Model->Revision) Rebuild(); }
void SGrimoireConnect::Text(const TSharedRef<SVerticalBox>& Box, const FString& Value)
{ Box->AddSlot().AutoHeight().Padding(0, 4) [ SNew(STextBlock).Text(FText::FromString(Value)).AutoWrapText(true) ]; }
TSharedRef<SWidget> SGrimoireConnect::Button(const FString& Label, TFunction<void()> Action, bool RequireSession)
{
    return SNew(SButton).Text(FText::FromString(Label)).ContentPadding(FMargin(10, 5))
        .IsEnabled_Lambda([this, RequireSession] { return Model.IsValid() && !Model->Busy && !GEditor->PlayWorld && (!RequireSession || (Model->Api->IsSignedIn() && !Model->GameId.IsEmpty())); })
        .OnClicked_Lambda([Action = MoveTemp(Action)] { Action(); return FReply::Handled(); });
}
void SGrimoireConnect::Rebuild()
{
    if (!Model.IsValid()) return; auto* M = Model.Get(); Revision = M->Revision;
    const float ScrollOffset = BodyScroll && BodyTab == Tab && BodySearch == Search ? BodyScroll->GetScrollOffset() : 0;
    BodyTab = Tab; BodySearch = Search; Root->ClearChildren();
    Text(Root.ToSharedRef(), TEXT("GRIMOIRE  /  UNREAL CONNECT"));
    Text(Root.ToSharedRef(), M->Status);
    if (!M->Api->IsSignedIn())
    {
        TSharedPtr<SEditableTextBox> Url, Email, Password, Code;
        Root->AddSlot().AutoHeight().Padding(0, 3) [ SAssignNew(Url, SEditableTextBox).Text(FText::FromString(M->Api->BaseUrl)).HintText(FText::FromString(TEXT("API URL"))) ];
        Root->AddSlot().AutoHeight().Padding(0, 3) [ SAssignNew(Email, SEditableTextBox).HintText(FText::FromString(TEXT("Email"))) ];
        Root->AddSlot().AutoHeight().Padding(0, 3) [ SAssignNew(Password, SEditableTextBox).IsPassword(true).HintText(FText::FromString(TEXT("Password"))) ];
        Root->AddSlot().AutoHeight().Padding(0, 3) [ Button(TEXT("Sign in"), [this, Url, Email, Password]
        { const FString Secret = Password->GetText().ToString(); Password->SetText(FText::GetEmpty()); Model->Login(Url->GetText().ToString(), Email->GetText().ToString(), Secret); }) ];
        if (!M->Api->TempToken.IsEmpty())
        {
            Root->AddSlot().AutoHeight().Padding(0, 3) [ SAssignNew(Code, SEditableTextBox).HintText(FText::FromString(TEXT("Two-factor code"))) ];
            Root->AddSlot().AutoHeight() [ Button(TEXT("Verify code"), [this, Code] { Model->Verify2FA(Code->GetText().ToString()); }) ];
        }
        Text(Root.ToSharedRef(), TEXT("Credentials are held only for this editor session. Imported assets work offline."));
    }
    else
    {
        Text(Root.ToSharedRef(), TEXT("Signed in as ") + M->Api->UserName);
        GameOptions.Empty(); for (const auto& Game : M->Games) GameOptions.Add(MakeShared<FString>(String(Game, TEXT("id"))));
        auto GameLabel = [this](const FString& Id)
        { for (const auto& G : Model->Games) if (String(G, TEXT("id")) == Id) return String(G, TEXT("company_name")) + TEXT(" / ") + String(G, TEXT("name")); return FString(TEXT("Select company / game")); };
        Root->AddSlot().AutoHeight().Padding(0, 5)
        [ SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&GameOptions).IsEnabled(!M->Busy)
            .OnGenerateWidget_Lambda([GameLabel](TSharedPtr<FString> Id) { return SNew(STextBlock).Text(FText::FromString(GameLabel(*Id))); })
            .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Id, ESelectInfo::Type) { if (Id) for (const auto& G : Model->Games) if (String(G, TEXT("id")) == *Id) { SelectedObject.Empty(); SelectedString.Empty(); FieldBuffers.Empty(); Model->SelectGame(G); break; } })
            [ SNew(STextBlock).Text(FText::FromString(GameLabel(M->GameId))) ] ];
        Root->AddSlot().AutoHeight().Padding(0, 5)
        [ SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0) [ Button(TEXT("Import / update game"), [this] { Model->ImportGame(); }, true) ]
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 6, 0) [ Button(TEXT("Locate asset"), [this] { if (Model->Database) GEditor->SyncBrowserToObjects(TArray<UObject*>{Model->Database.Get()}); }) ]
            + SHorizontalBox::Slot().AutoWidth() [ Button(TEXT("Sign out"), [this] { Model->SignOut(); }) ] ];
    }
    if (M->Busy)
        Root->AddSlot().AutoHeight() [ SNew(SButton).Text(FText::FromString(TEXT("Cancel request"))).OnClicked_Lambda([this] { Model->Cancel(); return FReply::Handled(); }) ];
    if (M->Database)
        Text(Root.ToSharedRef(), FString::Printf(TEXT("%s  |  %d objects  /  %d strings  /  %d dialogs  |  Imported %s UTC"), *M->Database->GameName, M->Database->Objects.Num(), M->Database->Strings.Num(), M->Database->Dialogs.Num(), *M->Database->ImportedAt.ToString()));
    auto Tabs = SNew(SHorizontalBox);
    for (const FString Name : { FString(TEXT("Objects")), FString(TEXT("Strings")), FString(TEXT("Dialogs")), FString(TEXT("Variables")), FString(TEXT("Tasks")), FString(TEXT("Scene")), FString(TEXT("Create")) })
        Tabs->AddSlot().AutoWidth().Padding(0, 0, 5, 0) [ Button(Tab == Name ? TEXT("[ ") + Name + TEXT(" ]") : Name, [this, Name] { Tab = Name; Search.Empty(); Rebuild(); }) ];
    Root->AddSlot().AutoHeight().Padding(0, 8) [ Tabs ];
    Root->AddSlot().AutoHeight().Padding(0, 3)
    [ SNew(SEditableTextBox).HintText(FText::FromString(TEXT("Search this tab (press Enter)"))).Text(FText::FromString(Search))
        .OnTextCommitted_Lambda([this](const FText& Value, ETextCommit::Type Type) { if (Type == ETextCommit::OnEnter) { Search = Value.ToString(); Rebuild(); } }) ];
    auto Body = SNew(SVerticalBox);
    Root->AddSlot().FillHeight(1).Padding(0, 8) [ SAssignNew(BodyScroll, SScrollBox) + SScrollBox::Slot() [ Body ] ];
    BodyScroll->SetScrollOffset(ScrollOffset);
    if (Tab == TEXT("Tasks")) BuildTasks(Body);
    else if (Tab == TEXT("Create")) BuildCreate(Body);
    else if (!M->Database) Text(Body, TEXT("Import a game to browse its data and use it in your level."));
    else if (Tab == TEXT("Objects")) BuildObjects(Body);
    else if (Tab == TEXT("Strings")) BuildStrings(Body);
    else if (Tab == TEXT("Dialogs")) BuildDialogs(Body);
    else if (Tab == TEXT("Variables")) BuildVariables(Body);
    else BuildScene(Body);
}
void SGrimoireConnect::BuildObjects(const TSharedRef<SVerticalBox>& Body)
{
    auto* DB = Model->Database.Get();
    Text(Body, TEXT("OBJECT LIBRARY  -  Click an object to expand it; click again to close. Field sections can also be collapsed."));
    for (const auto& O : DB->Objects)
    {
        if (!Search.IsEmpty() && !O.Name.Contains(Search) && !O.CodeId.Contains(Search) && !O.TemplateName.Contains(Search)) continue;
        const bool Selected = SelectedObject == O.Id;
        Body->AddSlot().AutoHeight().Padding(0, 3)
        [ Button((Selected ? TEXT("[-]  ") : TEXT("[+]  ")) + O.Name + TEXT("    |    ") + O.TemplateName + TEXT("    |    ") + O.CodeId,
            [this, Id = O.Id] { SelectedObject = SelectedObject == Id ? FString() : Id; Rebuild(); }) ];
        if (Selected)
        {
            auto Details = SNew(SVerticalBox);
            Body->AddSlot().AutoHeight().Padding(12, 0, 0, 12)
            [ SNew(SBorder).Padding(14).BorderBackgroundColor(FLinearColor(0.12f, 0.14f, 0.17f)) [ Details ] ];
            BuildObjectDetails(Details, O);
        }
    }
}
void SGrimoireConnect::BuildObjectDetails(const TSharedRef<SVerticalBox>& Body, const FGrimoireObject& Object)
{
    Body->AddSlot().AutoHeight()
    [ SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth() [ Button(TEXT("Link selected Actor"), [this, Id = Object.Id] { Model->LinkSelectedActor(Id); }) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(8, 0) [ Button(TEXT("Close details"), [this] { SelectedObject.Empty(); Rebuild(); }) ] ];
    Text(Body, TEXT("Code ID: ") + Object.CodeId + TEXT("\nObject ID: ") + Object.Id);
    TArray<FString> Sections; for (const auto& Field : Object.Fields) Sections.AddUnique(Field.Section);
    for (const auto& Section : Sections)
    {
        auto Fields = SNew(SVerticalBox);
        Body->AddSlot().AutoHeight().Padding(0, 7)
        [ SNew(SExpandableArea).InitiallyCollapsed(false).HeaderPadding(FMargin(8)).Padding(FMargin(12))
            .HeaderContent() [ SNew(STextBlock).Text(FText::FromString(Section.IsEmpty() ? TEXT("Fields") : Section)).Font(FAppStyle::GetFontStyle("BoldFont")) ]
            .BodyContent() [ Fields ] ];
        for (const auto& Field : Object.Fields)
        {
            if (Field.Section != Section) continue;
            auto Row = SNew(SVerticalBox);
            Fields->AddSlot().AutoHeight().Padding(0, 4)
            [ SNew(SBorder).Padding(10).BorderBackgroundColor(FLinearColor(0.07f, 0.08f, 0.1f)) [ Row ] ];
            Row->AddSlot().AutoHeight().Padding(0, 0, 0, 5)
            [ SNew(STextBlock).Text(FText::FromString(Field.Label)).Font(FAppStyle::GetFontStyle("BoldFont")) ];
            Text(Row, Field.Kind + (Field.Editable ? TEXT("  - editable; changes require review") : TEXT("  - imported value")));
            if (!Field.Editable || !SGrimoireFieldEditor::Supports(Field.Kind))
            {
                const FString Value = Field.Plain.IsEmpty() ? Field.StoredValueJson() : Field.Plain;
                Text(Row, Value.IsEmpty() || Value == TEXT("null") || Value == TEXT("[]") ? TEXT("Not set") : Value);
            }
            else
            {
                const FString Key = Object.Id + TEXT("/") + Field.Id;
                if (!FieldBuffers.Contains(Key)) FieldBuffers.Add(Key, Field.StoredValueJson());
                Row->AddSlot().AutoHeight().Padding(0, 3)
                [ SNew(SHorizontalBox)
                    + SHorizontalBox::Slot().FillWidth(1) [ SNew(SGrimoireFieldEditor).Field(Field).InitialJson(FieldBuffers[Key])
                        .OnChanged([this, Key](const FString& Value) { FieldBuffers.Add(Key, Value); }) ]
                    + SHorizontalBox::Slot().AutoWidth().Padding(6, 0) [ Button(TEXT("Queue for review"), [this, Id = Object.Id, FieldId = Field.Id, Key] { Model->QueueField(Id, FieldId, FieldBuffers[Key]); }, true) ] ];
            }
        }
    }
    const auto Doc = Parse(Object.DocumentJson);
    for (const auto& V : Array(Doc, TEXT("tasks")))
    { auto Task = AsObject(V); for (const auto& C : Array(Task, TEXT("content"))) Text(Body, TEXT("Task / note: ") + String(AsObject(C), TEXT("body"))); }
}
void SGrimoireConnect::BuildStrings(const TSharedRef<SVerticalBox>& Body)
{
    auto* DB = Model->Database.Get();
    LanguageOptions.Empty(); LanguageOptions.Add(MakeShared<FString>(TEXT("")));
    for (const auto& L : DB->Languages) LanguageOptions.Add(MakeShared<FString>(L));
    Body->AddSlot().AutoHeight()
    [ SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&LanguageOptions).IsEnabled(!Model->Busy)
        .OnGenerateWidget_Lambda([](TSharedPtr<FString> L) { return SNew(STextBlock).Text(FText::FromString(L->IsEmpty() ? TEXT("Source language") : *L)); })
        .OnSelectionChanged_Lambda([this](TSharedPtr<FString> L, ESelectInfo::Type) { if (L) { Model->SetLocale(*L); if (Model->Database) TranslationBuffer = Model->Database->GetText(SelectedString, *L).ToString(); } })
        [ SNew(STextBlock).Text(FText::FromString(Model->Locale.IsEmpty() ? TEXT("Source language") : Model->Locale)) ] ];
    if (const auto* S = DB->FindString(SelectedString))
    {
        Text(Body, TEXT("Selected: ") + S->Key + TEXT("  /  ") + S->Source);
        Body->AddSlot().AutoHeight() [ Button(TEXT("Link text on selected Actor"), [this]
        {
            auto* Actor = GEditor->GetSelectedActors()->GetTop<AActor>(); if (!Actor) { Model->Report(TEXT("Select an Actor first.")); return; }
            FScopedTransaction Transaction(NSLOCTEXT("Grimoire", "LinkText", "Link Grimoire text")); Actor->Modify();
            auto* Link = Actor->FindComponentByClass<UGrimoireTextLink>();
            if (!Link) { Link = NewObject<UGrimoireTextLink>(Actor, NAME_None, RF_Transactional); Actor->AddInstanceComponent(Link); Link->RegisterComponent(); }
            Link->Modify(); Link->Database = Model->Database; Link->StringKey = SelectedString; Link->PreviewLanguage = Model->Locale;
            Link->RefreshText(); Actor->MarkPackageDirty(); Model->Report(TEXT("Text linked. TextRender components update automatically; UMG can use Grimoire Text."));
        }) ];
        if (!Model->Locale.IsEmpty() && Model->Locale != DB->SourceLanguage)
        {
            Body->AddSlot().AutoHeight().Padding(0, 5) [ SNew(SEditableTextBox).Text(FText::FromString(TranslationBuffer)).OnTextChanged_Lambda([this](const FText& V) { TranslationBuffer = V.ToString(); }) ];
            Body->AddSlot().AutoHeight() [ Button(TEXT("Save translation to Grimoire"), [this] { Model->PushTranslation(SelectedString, Model->Locale, TranslationBuffer); }, true) ];
        }
    }
    for (const auto& S : DB->Strings)
    {
        if (!Search.IsEmpty() && !S.Key.Contains(Search) && !S.Source.Contains(Search)) continue;
        const FString Preview = DB->HasTranslation(S.Id, Model->Locale) ? DB->GetText(S.Id, Model->Locale).ToString() : TEXT("not translated");
        Body->AddSlot().AutoHeight().Padding(0, 3) [ Button((S.Key.IsEmpty() ? S.Id : S.Key) + TEXT("  /  ") + Preview, [this, Id = S.Id] { SelectedString = Id; TranslationBuffer = Model->Database->GetText(Id, Model->Locale).ToString(); Rebuild(); }) ];
    }
}
void SGrimoireConnect::BuildDialogs(const TSharedRef<SVerticalBox>& Body)
{
    Body->AddSlot().AutoHeight()
    [ SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth() [ Button(TEXT("Add Dialog Player to level"), [this] { Model->AddDialogPlayer(); }) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(8, 0) [ Button(TEXT("Sync variables"), [this] { Model->SyncVariables(); }, true) ]
        + SHorizontalBox::Slot().AutoWidth() [ Button(TEXT("Refresh requirements"), [this] { Rebuild(); }) ] ];
    Text(Body, TEXT("Expand a dialog to check its required data and inspect nodes. On the DialogPlayer component, Show Context Notes controls whether context nodes are displayed. On Line / On Choices expose the full Node data to Blueprints."));
    for (const auto& D : Model->Database->Dialogs)
    {
        if (!Search.IsEmpty() && !D.Name.Contains(Search) && !D.Key.Contains(Search)) continue;
        Body->AddSlot().AutoHeight().Padding(0, 4)
        [ Button((SelectedDialog == D.Id ? TEXT("[-]  ") : TEXT("[+]  ")) + D.Name + TEXT("    |    ") + D.Key,
            [this, Id = D.Id] { SelectedDialog = SelectedDialog == Id ? FString() : Id; Rebuild(); }) ];
        if (SelectedDialog != D.Id) continue;
        auto Detail = SNew(SVerticalBox);
        Body->AddSlot().AutoHeight().Padding(12, 0, 0, 12) [ SNew(SBorder).Padding(14) [ Detail ] ];
        Text(Detail, TEXT("Dialog ID: ") + D.Id);
        const auto Requirements = Model->Database->GetDialogRequirements(D.Id, GEditor->GetEditorWorldContext().World());
        int32 Missing = 0; for (const auto& R : Requirements) if (R.Missing) ++Missing;
        Text(Detail, Missing ? FString::Printf(TEXT("%d missing requirements - see the actions below."), Missing) : TEXT("Required data is available. Scene links below are optional when imported values suffice."));
        if (!Model->Database->VariablesImported) Text(Detail, TEXT("Game variables have not been synced yet. Click Sync variables to make them available to your game."));
        for (const auto& R : Requirements)
        {
            auto Row = SNew(SVerticalBox);
            Detail->AddSlot().AutoHeight().Padding(0, 5) [ SNew(SBorder).Padding(10).BorderBackgroundColor(R.Missing ? FLinearColor(0.32f, 0.11f, 0.07f) : FLinearColor(0.08f, 0.12f, 0.14f)) [ Row ] ];
            Text(Row, (R.Missing ? TEXT("MISSING  |  ") : TEXT("AVAILABLE  |  ")) + R.Kind + TEXT("  |  ") + R.Name);
            Text(Row, R.Message);
            Text(Row, TEXT("Used by: ") + R.UsedBy + TEXT("\nID: ") + R.ResourceId + (R.FieldId.IsEmpty() ? FString() : TEXT(" / ") + R.FieldId));
            if (!R.InScene && Model->Database->FindObject(R.ResourceId) && (R.Kind == TEXT("Object field") || R.Kind == TEXT("Speaker")))
                Row->AddSlot().AutoHeight() [ Button(TEXT("Link selected Actor to this object"), [this, Id = R.ResourceId] { Model->LinkSelectedActor(Id); }) ];
        }
        auto Nodes = SNew(SVerticalBox);
        Detail->AddSlot().AutoHeight().Padding(0, 10)
        [ SNew(SExpandableArea).InitiallyCollapsed(true).HeaderPadding(FMargin(8)).Padding(FMargin(10))
            .HeaderContent() [ SNew(STextBlock).Text(FText::FromString(TEXT("Inspect imported nodes"))).Font(FAppStyle::GetFontStyle("BoldFont")) ]
            .BodyContent() [ Nodes ] ];
        for (const auto& N : Model->Database->GetDialogNodes(D.Id))
        {
            Text(Nodes, N.Identifier + TEXT("  |  ") + N.Type + TEXT("  |  ") + N.Name);
            if (!N.SourceText.IsEmpty()) Text(Nodes, N.SourceText);
            for (const auto& O : N.Options) Text(Nodes, TEXT("  Answer: ") + O.SourceText + TEXT("  [") + O.Id + TEXT("]"));
        }
    }
}
void SGrimoireConnect::BuildVariables(const TSharedRef<SVerticalBox>& Body)
{
    Body->AddSlot().AutoHeight() [ Button(TEXT("Sync variables"), [this] { Model->SyncVariables(); }, true) ];
    Text(Body, TEXT("Game variables and their elements are imported from Grimoire. In Blueprints, use Grimoire Subsystem > Get / Set Game Number, Boolean or Text. Dialog setters share those session values. Syncing updates the asset's defaults; gameplay does not write to the API."));
    if (!Model->Database->VariablesImported) { Text(Body, TEXT("Not synced yet. Click Sync variables or Import / update game.")); return; }
    Text(Body, FString::Printf(TEXT("%d variables  |  Synced %s UTC"), Model->Database->GameVariables.Num(), *Model->Database->VariablesImportedAt.ToString()));
    for (const auto& V : Model->Database->GameVariables)
    {
        bool Match = Search.IsEmpty() || V.Name.Contains(Search) || V.Id.Contains(Search) || V.Group.Contains(Search);
        for (const auto& E : V.Elements) Match |= E.Name.Contains(Search) || E.Id.Contains(Search);
        if (!Match) continue;
        auto Elements = SNew(SVerticalBox);
        Body->AddSlot().AutoHeight().Padding(0, 6)
        [ SNew(SExpandableArea).InitiallyCollapsed(Search.IsEmpty()).HeaderPadding(FMargin(10)).Padding(FMargin(12))
            .HeaderContent() [ SNew(STextBlock).Text(FText::FromString(V.Name + TEXT("  |  ") + V.ElementType + FString::Printf(TEXT("  |  %d elements"), V.Elements.Num()))).Font(FAppStyle::GetFontStyle("BoldFont")) ]
            .BodyContent() [ Elements ] ];
        Text(Elements, V.Description + TEXT("\nVariable ID: ") + V.Id + TEXT("\nGroup: ") + V.Group);
        for (const auto& E : V.Elements)
        {
            auto Row = SNew(SVerticalBox);
            Elements->AddSlot().AutoHeight().Padding(0, 4) [ SNew(SBorder).Padding(10) [ Row ] ];
            Text(Row, E.Name + TEXT("  |  Default: ") + (E.AuthoredValueJson == TEXT("null") ? TEXT("element ID") : E.AuthoredValueJson));
            if (E.HasRange) Text(Row, FString::Printf(TEXT("Range: %g to %g"), E.RangeMin, E.RangeMax));
            Text(Row, E.Description + TEXT("\nElement ID: ") + E.Id);
        }
    }
}
void SGrimoireConnect::BuildTasks(const TSharedRef<SVerticalBox>& Body)
{
    UserOptions.Empty(); UserOptions.Add(MakeShared<FString>(TEXT("")));
    for (const auto& U : Model->Users) UserOptions.Add(MakeShared<FString>(String(U, TEXT("id"))));
    auto Label = [this](const FString& Id) { if (Id.IsEmpty()) return FString(TEXT("All assignees")); for (const auto& U : Model->Users) if (String(U, TEXT("id")) == Id) return First(String(U, TEXT("name")), String(U, TEXT("username"))); return Id; };
    Body->AddSlot().AutoHeight()
    [ SNew(SHorizontalBox)
        + SHorizontalBox::Slot().AutoWidth() [ Button(TEXT("Load / refresh tasks"), [this] { Model->LoadTasks(Assignee); }, true) ]
        + SHorizontalBox::Slot().AutoWidth().Padding(6, 0) [ Button(TEXT("Assigned to me"), [this] { Assignee = Model->Api->UserId; Model->LoadTasks(Assignee); }, true) ]
        + SHorizontalBox::Slot().FillWidth(1) [ SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&UserOptions).IsEnabled(!Model->Busy)
            .OnGenerateWidget_Lambda([Label](TSharedPtr<FString> V) { return SNew(STextBlock).Text(FText::FromString(Label(*V))); })
            .OnSelectionChanged_Lambda([this](TSharedPtr<FString> V, ESelectInfo::Type) { if (V) { Assignee = *V; Model->LoadTasks(Assignee); } })
            [ SNew(STextBlock).Text(FText::FromString(Label(Assignee))) ] ] ];
    for (const auto& Task : Model->Tasks)
    {
        FString Description; for (const auto& V : Array(Task, TEXT("content"))) Description += String(AsObject(V), TEXT("body")) + TEXT("\n");
        if (!Search.IsEmpty() && !Description.Contains(Search)) continue;
        Text(Body, TEXT("[") + String(Task, TEXT("status")) + TEXT("] ") + Description);
        auto Actions = SNew(SHorizontalBox);
        for (const auto& S : Model->Statuses)
            Actions->AddSlot().AutoWidth().Padding(0, 0, 4, 3) [ Button(String(S, TEXT("label")), [this, Id = String(Task, TEXT("id")), Key = String(S, TEXT("key"))] { Model->UpdateTask(Id, Key); }, true) ];
        Body->AddSlot().AutoHeight() [ Actions ];
    }
}
void SGrimoireConnect::BuildScene(const TSharedRef<SVerticalBox>& Body)
{
    Body->AddSlot().AutoHeight() [ Button(TEXT("Refresh level links"), [this] { Rebuild(); }) ];
    Text(Body, TEXT("Queue transforms for review, restore approved transforms, or explicitly unlink before deleting an Actor. Save the level before syncing. Position is centimeters; rotation x/y/z is roll/pitch/yaw in degrees."));
    auto* World = GEditor->GetEditorWorldContext().World(); if (!World) return;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        auto* Link = It->FindComponentByClass<UGrimoireObjectLink>();
        auto* TextLink = It->FindComponentByClass<UGrimoireTextLink>();
        if ((!Link || Link->ObjectKey.IsEmpty()) && !TextLink) continue;
        if (!Search.IsEmpty() && !It->GetActorLabel().Contains(Search)) continue;
        Text(Body, It->GetActorLabel() + TEXT("  /  ") + (Link ? Link->ObjectKey : TextLink->StringKey));
        TWeakObjectPtr<AActor> Actor(*It); TWeakObjectPtr<UGrimoireObjectLink> WeakLink(Link);
        auto Actions = SNew(SHorizontalBox);
        Actions->AddSlot().AutoWidth() [ Button(TEXT("Select Actor"), [Actor] { if (Actor.IsValid()) { GEditor->SelectNone(false, true); GEditor->SelectActor(Actor.Get(), true, true); } }) ];
        if (Link)
        {
            Actions->AddSlot().AutoWidth().Padding(6, 0) [ Button(TEXT("Queue transform"), [this, WeakLink] { Model->SyncActor(WeakLink.Get()); }, true) ];
            Actions->AddSlot().AutoWidth().Padding(6, 0) [ Button(TEXT("Reset to Grimoire"), [this, WeakLink] { Model->SyncActor(WeakLink.Get(), false, true); }, true) ];
            Actions->AddSlot().AutoWidth().Padding(6, 0) [ Button(TEXT("Unlink"), [this, WeakLink] { Model->SyncActor(WeakLink.Get(), true); }, true) ];
        }
        Body->AddSlot().AutoHeight() [ Actions ];
    }
}
void SGrimoireConnect::BuildCreate(const TSharedRef<SVerticalBox>& Body)
{
    Text(Body, TEXT("Create a draft library object in ") + Model->GameName + TEXT(". This writes immediately to Grimoire; it does not link an Actor."));
    TemplateOptions.Empty(); TemplateNames.Empty(); TemplateOptions.Add(MakeShared<FString>(TEXT(""))); TemplateNames.Add(TEXT(""), TEXT("Blank object"));
    if (Model->Database) for (const auto& O : Model->Database->Objects)
    {
        auto Template = Object(Object(Parse(O.DocumentJson), TEXT("object")), TEXT("template"));
        const FString Id = String(Template, TEXT("id"));
        if (!Id.IsEmpty() && !TemplateNames.Contains(Id)) { TemplateOptions.Add(MakeShared<FString>(Id)); TemplateNames.Add(Id, First(String(Template, TEXT("name")), Id)); }
    }
    Body->AddSlot().AutoHeight().Padding(0, 3)
    [ SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&TemplateOptions)
        .OnGenerateWidget_Lambda([this](TSharedPtr<FString> Id) { return SNew(STextBlock).Text(FText::FromString(TemplateNames.FindRef(*Id))); })
        .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Id, ESelectInfo::Type) { if (Id) { DraftTemplate = *Id; Rebuild(); } })
        [ SNew(STextBlock).Text(FText::FromString(TemplateNames.Contains(DraftTemplate) ? TemplateNames[DraftTemplate] : DraftTemplate)) ] ];
    Body->AddSlot().AutoHeight().Padding(0, 3) [ SNew(SEditableTextBox).HintText(FText::FromString(TEXT("Object name (required)"))).Text(FText::FromString(DraftName)).OnTextChanged_Lambda([this](const FText& V) { DraftName = V.ToString(); }) ];
    Body->AddSlot().AutoHeight().Padding(0, 3) [ SNew(SEditableTextBox).HintText(FText::FromString(TEXT("Code ID (optional)"))).Text(FText::FromString(DraftCode)).OnTextChanged_Lambda([this](const FText& V) { DraftCode = V.ToString(); }) ];
    Body->AddSlot().AutoHeight().Padding(0, 3) [ SNew(SEditableTextBox).HintText(FText::FromString(TEXT("Template UUID (optional; empty creates a blank object)"))).Text(FText::FromString(DraftTemplate)).OnTextChanged_Lambda([this](const FText& V) { DraftTemplate = V.ToString(); }) ];
    Body->AddSlot().AutoHeight() [ Button(TEXT("Create draft in Grimoire..."), [this]
    {
        FString Description = TEXT("Create draft '") + DraftName + TEXT("' in ") + Model->GameName + TEXT("?\n") + (DraftTemplate.IsEmpty() ? TEXT("Blank object") : TEXT("Template: ") + (TemplateNames.Contains(DraftTemplate) ? TemplateNames[DraftTemplate] : DraftTemplate));
        if (Model->Database) for (const auto& O : Model->Database->Objects) if (O.Name.Equals(DraftName.TrimStartAndEnd(), ESearchCase::IgnoreCase)) { Description += TEXT("\nAn imported object already uses this name."); break; }
        if (FMessageDialog::Open(EAppMsgType::YesNo, FText::FromString(Description)) == EAppReturnType::Yes) Model->CreateObject(DraftName, DraftCode, DraftTemplate);
    }, true) ];
}
