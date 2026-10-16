#include "GrimoireEditorSubsystem.h"
#include "GrimoireJson.h"
#include "GrimoireObjectLink.h"
#include "GrimoireDialogPlayer.h"
#include "GrimoireDialogActor.h"
#include "GrimoireTextLink.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "Engine/Selection.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "ScopedTransaction.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectIterator.h"
using namespace GrimoireJson;

namespace
{
    const TCHAR* SettingsSection = TEXT("Grimoire.Connect");
    FString AssetPath(const FString& GameId)
    { FString Safe = GameId; Safe.ReplaceInline(TEXT("-"), TEXT("")); return TEXT("/Game/Grimoire/G_") + Safe + TEXT("/Database"); }
    bool ListResult(const FGrimoireApiResult& R, TArray<TSharedPtr<FJsonObject>>& Out)
    {
        if (!R.Success || !R.Data || R.Data->Type != EJson::Array) return false;
        Out.Empty(); for (const auto& V : R.Data->AsArray()) { auto O = AsObject(V); if (!O) return false; Out.Add(O); } return true;
    }
    FObject VectorJson(const FVector& V)
    { auto O = MakeShared<FJsonObject>(); O->SetNumberField(TEXT("x"), V.X); O->SetNumberField(TEXT("y"), V.Y); O->SetNumberField(TEXT("z"), V.Z); return O; }
    bool ReadVector(const FObject& O, FVector& V)
    { return O && O->TryGetNumberField(TEXT("x"), V.X) && O->TryGetNumberField(TEXT("y"), V.Y) && O->TryGetNumberField(TEXT("z"), V.Z) && !V.ContainsNaN(); }
}
void UGrimoireEditorSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection); Api = MakeShared<FGrimoireApiClient>();
    GConfig->GetString(SettingsSection, TEXT("ApiUrl"), Api->BaseUrl, GEditorPerProjectIni);
    GConfig->GetString(SettingsSection, TEXT("GameId"), GameId, GEditorPerProjectIni);
    GConfig->GetString(SettingsSection, TEXT("GameName"), GameName, GEditorPerProjectIni);
    GConfig->GetString(SettingsSection, TEXT("Locale"), Locale, GEditorPerProjectIni);
    FGuid Id;
    if (FGuid::Parse(GameId, Id)) Database = LoadObject<UGrimoireDatabase>(nullptr, *(AssetPath(GameId) + TEXT(".Database")));
    else GameId.Empty();
}
void UGrimoireEditorSubsystem::Deinitialize() { Api->SignOut(); Api.Reset(); Super::Deinitialize(); }
void UGrimoireEditorSubsystem::Report(const FString& Message) { Status = Message; ++Revision; }
void UGrimoireEditorSubsystem::SaveSettings()
{
    GConfig->SetString(SettingsSection, TEXT("ApiUrl"), *Api->BaseUrl, GEditorPerProjectIni);
    GConfig->SetString(SettingsSection, TEXT("GameId"), *GameId, GEditorPerProjectIni);
    GConfig->SetString(SettingsSection, TEXT("GameName"), *GameName, GEditorPerProjectIni);
    GConfig->SetString(SettingsSection, TEXT("Locale"), *Locale, GEditorPerProjectIni);
    GConfig->Flush(false, GEditorPerProjectIni);
}
void UGrimoireEditorSubsystem::Cancel()
{ Api->Cancel(); Busy = false; Staging = nullptr; ImportIds.Empty(); SeenIds.Empty(); Report(TEXT("Cancelled. Previously imported data is unchanged. A submitted write may already have reached Grimoire.")); }
void UGrimoireEditorSubsystem::SignOut()
{ Cancel(); Api->SignOut(); Games.Empty(); Tasks.Empty(); Users.Empty(); Statuses.Empty(); Report(TEXT("Signed out. Imported assets remain available offline.")); }
bool UGrimoireEditorSubsystem::CanStart()
{
    if (Busy) return false;
    if (!Api->IsSignedIn() || GameId.IsEmpty()) { Report(TEXT("Sign in and select a game first.")); return false; }
    if (GEditor->PlayWorld) { Report(TEXT("Exit Play mode before changing Grimoire data.")); return false; }
    Busy = true; return true;
}
void UGrimoireEditorSubsystem::Query(const FString& Verb, const FString& Path, const FObject& Body, FGrimoireApiCallback Done, TMap<FString, FString> Parameters)
{
    if (!GameId.IsEmpty()) Parameters.Add(TEXT("game_id"), GameId);
    TWeakObjectPtr<UGrimoireEditorSubsystem> Weak(this);
    Api->Request(Verb, Path, Parameters, Body, [Weak, Path, Done = MoveTemp(Done)](const auto& R)
    {
        if (!Weak.IsValid()) return;
        if (!R.Success) { Weak->Busy = false; Weak->Staging = nullptr; Weak->Report(Path == TEXT("/api/v1/variables") ? TEXT("Variables were not synced; previous data is unchanged. Check variables:read access for this game. ") + R.Error : R.Error); return; }
        Done(R);
    });
}
void UGrimoireEditorSubsystem::Login(const FString& Url, const FString& Email, const FString& Password)
{
    if (Busy) return;
    FString Normalized = Url.TrimStartAndEnd(); while (Normalized.EndsWith(TEXT("/"))) Normalized.LeftChopInline(1);
    if (!FGrimoireApiClient::IsValidBaseUrl(Normalized)) { Report(TEXT("Enter a valid HTTPS API URL.")); return; }
    if (Email.TrimStartAndEnd().IsEmpty() || Password.IsEmpty()) { Report(TEXT("Enter your email and password.")); return; }
    if (Normalized != Api->BaseUrl) { GameId.Empty(); GameName.Empty(); Database = nullptr; Games.Empty(); }
    Api->BaseUrl = Normalized; SaveSettings(); Busy = true; Report(TEXT("Signing in..."));
    TWeakObjectPtr<UGrimoireEditorSubsystem> Weak(this);
    Api->Login(Email.TrimStartAndEnd(), Password, [Weak](const auto& R)
    {
        if (!Weak.IsValid()) return; Weak->Busy = false;
        if (!R.Success) Weak->Report(R.Error);
        else if (!Weak->Api->TempToken.IsEmpty()) Weak->Report(TEXT("Enter the two-factor authentication code."));
        else Weak->ListGames();
    });
}
void UGrimoireEditorSubsystem::Verify2FA(const FString& Code)
{
    if (Busy) return; Busy = true; TWeakObjectPtr<UGrimoireEditorSubsystem> Weak(this);
    Api->Verify2FA(Code.TrimStartAndEnd(), [Weak](const auto& R)
    { if (Weak.IsValid()) { Weak->Busy = false; if (R.Success && Weak->Api->IsSignedIn()) Weak->ListGames(); else Weak->Report(R.Error); } });
}
void UGrimoireEditorSubsystem::ListGames()
{
    Busy = true; TWeakObjectPtr<UGrimoireEditorSubsystem> Weak(this);
    Api->Request(TEXT("GET"), TEXT("/api/v1/games"), {}, nullptr, [Weak](const auto& R)
    {
        if (!Weak.IsValid()) return; Weak->Busy = false;
        if (!ListResult(R, Weak->Games)) { Weak->Report(R.Success ? TEXT("Invalid game directory response.") : R.Error); return; }
        Weak->Report(TEXT("Choose your company / game, then Import game."));
        auto Previous = Weak->Games.FindByPredicate([Weak](const auto& G) { return String(G, TEXT("id")) == Weak->GameId; });
        if (Previous) Weak->SelectGame(*Previous);
        else { Weak->GameId.Empty(); Weak->GameName.Empty(); Weak->Database = nullptr; Weak->SaveSettings(); }
    });
}
void UGrimoireEditorSubsystem::SelectGame(const FObject& Game)
{
    if (Busy) return;
    const FString Id = String(Game, TEXT("id")); FGuid Guid;
    if (!FGuid::Parse(Id, Guid)) { Report(TEXT("The game directory returned an invalid game UUID.")); return; }
    GameId = Id; GameName = String(Game, TEXT("name")); Tasks.Empty(); Users.Empty(); Statuses.Empty();
    Database = LoadObject<UGrimoireDatabase>(nullptr, *(AssetPath(GameId) + TEXT(".Database")));
    SaveSettings(); Report(TEXT("Selected ") + String(Game, TEXT("company_name")) + TEXT(" / ") + GameName);
}
void UGrimoireEditorSubsystem::ImportGame()
{
    if (!CanStart()) return;
    VariablesOnly = false; VariableIds.Empty(); ElementIds.Empty();
    Staging = NewObject<UGrimoireDatabase>(this); Staging->GameId = GameId; Staging->GameName = GameName;
    for (const auto& Game : Games) if (String(Game, TEXT("id")) == GameId)
    {
        Staging->SourceLanguage = String(Game, TEXT("default_language"));
        for (const auto& V : Array(Game, TEXT("languages"))) if (V->Type == EJson::String) Staging->Languages.Add(V->AsString());
    }
    ImportingDialogs = false; ImportIds.Empty(); SeenIds.Empty(); ImportList(0);
}
void UGrimoireEditorSubsystem::ImportList(int32 Offset)
{
    if (Offset >= 10000) { Busy = false; Staging = nullptr; Report(TEXT("Import exceeded 10,000 records. No snapshot was replaced.")); return; }
    Report(ImportingDialogs ? TEXT("Listing dialogs...") : TEXT("Listing objects..."));
    Query(TEXT("GET"), ImportingDialogs ? TEXT("/api/v1/dialogs") : TEXT("/api/v1/objects"), nullptr, [this, Offset](const auto& R)
    {
        TArray<FObject> Items;
        if (!ListResult(R, Items)) { Busy = false; Staging = nullptr; Report(TEXT("Invalid list response. Existing data is unchanged.")); return; }
        for (const auto& Item : Items)
        {
            const FString Id = String(Item, TEXT("id"));
            if (Id.IsEmpty() || SeenIds.Contains(Id)) { Busy = false; Staging = nullptr; Report(TEXT("Invalid/repeated page. Re-import after the library stops changing.")); return; }
            SeenIds.Add(Id); ImportIds.Add(Id);
        }
        if (Items.Num() == 100) ImportList(Offset + Items.Num());
        else { ImportIndex = 0; ImportNext(); }
    }, { {TEXT("limit"), TEXT("100")}, {TEXT("offset"), FString::FromInt(Offset)} });
}
void UGrimoireEditorSubsystem::ImportNext()
{
    if (ImportIndex >= ImportIds.Num())
    {
        if (!ImportingDialogs) { ImportingDialogs = true; ImportIds.Empty(); SeenIds.Empty(); ImportList(0); }
        else ImportStrings();
        return;
    }
    if (ImportIds.Num() > 10000) { Busy = false; Staging = nullptr; Report(TEXT("Linked resource import exceeded 10,000 records.")); return; }
    Report(FString::Printf(TEXT("Importing %s %d / %d..."), ImportingDialogs ? TEXT("dialog") : TEXT("object"), ImportIndex + 1, ImportIds.Num()));
    const FString Id = ImportIds[ImportIndex];
    Query(TEXT("GET"), (ImportingDialogs ? TEXT("/api/v1/dialogs/") : TEXT("/api/v1/objects/")) + FGrimoireApiClient::Encode(Id), nullptr, [this, Id](const auto& R)
    {
        FString Error;
        if (ImportingDialogs)
        {
            FGrimoireDialog Dialog;
            if (!UGrimoireDatabase::ParseDialog(AsObject(R.Data), Dialog, Error)) { Busy = false; Staging = nullptr; Report(Error); return; }
            // Follow external edges as well as the directory; cycles are de-duplicated.
            for (const auto& V : Array(Parse(Dialog.GraphJson), TEXT("nodes")))
            {
                auto Node = AsObject(V); TArray<FObject> Links { Node };
                for (const auto& O : Array(Node, TEXT("options"))) Links.Add(AsObject(O));
                for (const auto& O : Array(Node, TEXT("conditionOptions"))) Links.Add(AsObject(O));
                for (const auto& Link : Links) for (const FString Key : { FString(TEXT("next_dialog")), FString(TEXT("next_dialog_true")), FString(TEXT("next_dialog_false")) })
                { const FString Target = String(Link, Key); if (!Target.IsEmpty() && !SeenIds.Contains(Target)) { SeenIds.Add(Target); ImportIds.Add(Target); } }
            }
            Staging->Dialogs.Add(MoveTemp(Dialog));
        }
        else
        {
            FGrimoireObject O;
            if (!UGrimoireDatabase::ParseObject(AsObject(R.Data), O, Error)) { Busy = false; Staging = nullptr; Report(Error); return; }
            Staging->Objects.Add(MoveTemp(O));
        }
        ++ImportIndex; ImportNext();
    });
}
void UGrimoireEditorSubsystem::ImportStrings()
{
    Report(TEXT("Importing source strings and translations..."));
    Query(TEXT("GET"), TEXT("/api/v1/strings"), nullptr, [this](const auto& R)
    {
        TArray<FObject> Items;
        if (!ListResult(R, Items)) { Busy = false; Staging = nullptr; Report(TEXT("Invalid string list. Existing snapshot is unchanged.")); return; }
        for (const auto& Item : Items)
        { FGrimoireString S; if (!UGrimoireDatabase::ParseString(Item, S)) { Busy = false; Staging = nullptr; Report(TEXT("Invalid string record.")); return; } Staging->Strings.Add(MoveTemp(S)); }
        ImportVariables(0);
    }, {{TEXT("include_translations"), TEXT("true")}, {TEXT("include_context"), TEXT("false")}});
}
void UGrimoireEditorSubsystem::SyncVariables()
{
    if (!Database) { Report(TEXT("Import / update game first to create a database.")); return; }
    if (!CanStart()) return;
    VariablesOnly = true; VariableIds.Empty(); ElementIds.Empty();
    Staging = DuplicateObject<UGrimoireDatabase>(Database, this);
    Staging->ClearFlags(RF_Public | RF_Standalone); Staging->GameVariables.Empty();
    ImportVariables(0);
}
void UGrimoireEditorSubsystem::ImportVariables(int32 Offset)
{
    if (Offset >= 10000) { Busy = false; Staging = nullptr; Report(TEXT("Variables exceed the 10,000-record safety limit. Previous data is unchanged.")); return; }
    Report(FString::Printf(TEXT("Syncing game variables (%d)..."), Offset));
    Query(TEXT("GET"), TEXT("/api/v1/variables"), nullptr, [this, Offset](const auto& R)
    {
        TArray<FObject> Items;
        if (!ListResult(R, Items)) { Busy = false; Staging = nullptr; Report(TEXT("Invalid variable list. Previous data is unchanged.")); return; }
        for (const auto& Item : Items)
        {
            FGrimoireGameVariable Variable; FString Error;
            if (!UGrimoireDatabase::ParseGameVariable(Item, Variable, Error)) { Busy = false; Staging = nullptr; Report(Error); return; }
            if (VariableIds.Contains(Variable.Id)) { Busy = false; Staging = nullptr; Report(TEXT("Repeated variable page. Previous data is unchanged; retry the sync.")); return; }
            VariableIds.Add(Variable.Id);
            for (const auto& E : Variable.Elements)
            {
                if (ElementIds.Contains(E.Id)) { Busy = false; Staging = nullptr; Report(TEXT("Duplicate variable element id. Previous data is unchanged.")); return; }
                ElementIds.Add(E.Id);
            }
            Staging->GameVariables.Add(MoveTemp(Variable));
        }
        if (Items.Num() == 200) { ImportVariables(Offset + Items.Num()); return; }
        Staging->VariablesImported = true; Staging->VariablesImportedAt = FDateTime::UtcNow(); FinishImport();
    }, {{TEXT("limit"), TEXT("200")}, {TEXT("offset"), FString::FromInt(Offset)}});
}
void UGrimoireEditorSubsystem::FinishImport()
{
    const FString Path = AssetPath(GameId); UPackage* Package = CreatePackage(*Path);
    if (!Package) { Busy = false; Staging = nullptr; Report(TEXT("Could not create database package.")); return; }
    Package->FullyLoad();
    if (!Database) Database = FindObject<UGrimoireDatabase>(Package, TEXT("Database"));
    if (!Database)
    { Database = NewObject<UGrimoireDatabase>(Package, TEXT("Database"), RF_Public | RF_Standalone | RF_Transactional); FAssetRegistryModule::AssetCreated(Database); }
    Database->Modify(); Database->GameId = Staging->GameId; Database->GameName = Staging->GameName;
    Database->SourceLanguage = Staging->SourceLanguage; Database->Languages = MoveTemp(Staging->Languages);
    Database->Objects = MoveTemp(Staging->Objects); Database->Strings = MoveTemp(Staging->Strings); Database->Dialogs = MoveTemp(Staging->Dialogs);
    Database->GameVariables = MoveTemp(Staging->GameVariables); Database->VariablesImported = Staging->VariablesImported;
    Database->VariablesImportedAt = Staging->VariablesImportedAt;
    if (!VariablesOnly) Database->ImportedAt = FDateTime::UtcNow(); Database->MarkPackageDirty();
    const FString Filename = FPackageName::LongPackageNameToFilename(Path, FPackageName::GetAssetPackageExtension());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
    FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
    const bool Saved = UPackage::SavePackage(Package, Database, *Filename, Args);
    Busy = false; Staging = nullptr;
    Report(Saved ? FString::Printf(TEXT("Saved %d objects, %d strings, %d dialogs and %d game variables."), Database->Objects.Num(), Database->Strings.Num(), Database->Dialogs.Num(), Database->GameVariables.Num())
        : TEXT("Imported, but the asset could not be saved. Check file permissions/source control and use Save All."));
}
void UGrimoireEditorSubsystem::LoadTasks(const FString& Assignee)
{
    if (!CanStart()) return; Report(TEXT("Loading workflow tasks..."));
    Query(TEXT("GET"), TEXT("/api/v1/users"), nullptr, [this, Assignee](const auto& R)
    {
        if (!ListResult(R, Users)) { Busy = false; Report(TEXT("Invalid users response.")); return; }
        Query(TEXT("GET"), TEXT("/api/v1/statuses"), nullptr, [this, Assignee](const auto& S)
        {
            Statuses.Empty(); for (const auto& V : Array(AsObject(S.Data), TEXT("statuses"))) if (auto O = AsObject(V)) Statuses.Add(O);
            Query(TEXT("GET"), TEXT("/api/v1/tasks"), nullptr, [this](const auto& T)
            { Busy = false; if (!ListResult(T, Tasks)) Report(TEXT("Invalid tasks response.")); else Report(FString::Printf(TEXT("Loaded %d tasks."), Tasks.Num())); },
                {{TEXT("user_id"), Assignee}, {TEXT("is_task"), TEXT("true")}, {TEXT("archived"), TEXT("false")}});
        }, {{TEXT("domain"), TEXT("tasks")}});
    });
}
void UGrimoireEditorSubsystem::CompleteWrite(const FGrimoireApiResult& R, const FString& Message)
{ Busy = false; Report(Message); }
void UGrimoireEditorSubsystem::UpdateTask(const FString& Id, const FString& Key)
{
    if (!CanStart()) return; auto Body = MakeShared<FJsonObject>(); Body->SetStringField(TEXT("status"), Key);
    Query(TEXT("PATCH"), TEXT("/api/v1/tasks/") + FGrimoireApiClient::Encode(Id), Body, [this, Id, Key](const auto& R)
    { for (auto& T : Tasks) if (String(T, TEXT("id")) == Id) T->SetStringField(TEXT("status"), Key); CompleteWrite(R, TEXT("Task status updated.")); });
}
void UGrimoireEditorSubsystem::CreateObject(const FString& Name, const FString& CodeId, const FString& TemplateId)
{
    if (Name.TrimStartAndEnd().IsEmpty()) { Report(TEXT("An object name is required.")); return; }
    if (!CanStart()) return; auto Body = MakeShared<FJsonObject>();
    Body->SetStringField(TEXT("name"), Name.TrimStartAndEnd()); Body->SetStringField(TEXT("status"), TEXT("draft"));
    if (!CodeId.TrimStartAndEnd().IsEmpty()) Body->SetStringField(TEXT("code_id"), CodeId.TrimStartAndEnd());
    if (!TemplateId.TrimStartAndEnd().IsEmpty()) Body->SetStringField(TEXT("template_id"), TemplateId.TrimStartAndEnd());
    Query(TEXT("POST"), TEXT("/api/v1/objects"), Body, [this](const auto& R)
    { CompleteWrite(R, TEXT("Draft created in Grimoire: ") + String(AsObject(R.Data), TEXT("id")) + TEXT(". Import game to use it here.")); });
}
void UGrimoireEditorSubsystem::PushTranslation(const FString& Id, const FString& Language, const FString& Text)
{
    if (Id.IsEmpty() || Language.IsEmpty()) { Report(TEXT("Choose a string and translation language.")); return; }
    if (!CanStart()) return; auto Body = MakeShared<FJsonObject>(); Body->SetStringField(TEXT("translated_text"), Text);
    Query(TEXT("PATCH"), TEXT("/api/v1/strings/") + FGrimoireApiClient::Encode(Id) + TEXT("/translations/") + FGrimoireApiClient::Encode(Language), Body,
        [this](const auto& R) { CompleteWrite(R, TEXT("Translation saved in Grimoire. Import game to update the local snapshot.")); });
}
void UGrimoireEditorSubsystem::QueueCommit(const FString& Title, const FObject& Change, FGrimoireApiCallback Done)
{
    auto Body = MakeShared<FJsonObject>(); Body->SetStringField(TEXT("title"), Title); Body->SetStringField(TEXT("source"), TEXT("unreal"));
    Body->SetArrayField(TEXT("changes"), {MakeShared<FJsonValueObject>(Change)});
    Query(TEXT("POST"), TEXT("/api/v1/engine-commits"), Body, MoveTemp(Done));
}
void UGrimoireEditorSubsystem::QueueField(const FString& ObjectId, const FString& FieldId, const FString& Json)
{
    const auto* O = Database ? Database->FindObject(ObjectId) : nullptr; const auto* F = O ? O->FindField(FieldId) : nullptr;
    auto V = ParseValue(Json);
    if (!F || !F->Editable || !V) { Report(TEXT("Select an editable field and enter a valid JSON value.")); return; }
    if (!CanStart()) return;
    auto Change = MakeShared<FJsonObject>(); Change->SetStringField(TEXT("object_id"), O->Id); Change->SetStringField(TEXT("field_id"), F->Id);
    Change->SetField(TEXT("value"), V); Change->SetField(TEXT("base_value"), ParseValue(F->StoredValueJson()));
    QueueCommit(TEXT("Unreal: ") + O->Name + TEXT(" / ") + F->Label, Change, [this](const auto& R)
    { CompleteWrite(R, TEXT("Field change queued for review in Grimoire. Import again after approval.")); });
}
void UGrimoireEditorSubsystem::LinkSelectedActor(const FString& Id)
{
    if (Busy || GEditor->PlayWorld || !Database || !Database->FindObject(Id)) return;
    AActor* Actor = GEditor->GetSelectedActors()->GetTop<AActor>();
    if (!Actor || !Actor->GetWorld() || Actor->GetWorld()->WorldType != EWorldType::Editor) { Report(TEXT("Select an Actor in the level first.")); return; }
    auto* Link = Actor->FindComponentByClass<UGrimoireObjectLink>();
    if (Link && !Link->ObjectKey.IsEmpty() && (Link->Database != Database || Link->ObjectKey != Id))
    { Report(TEXT("Unlink this Actor from its current object before assigning another.")); return; }
    const FScopedTransaction Transaction(NSLOCTEXT("Grimoire", "LinkActor", "Link Actor to Grimoire")); Actor->Modify();
    if (!Link) { Link = NewObject<UGrimoireObjectLink>(Actor, NAME_None, RF_Transactional); Actor->AddInstanceComponent(Link); Link->RegisterComponent(); }
    Link->Modify(); Link->Database = Database; Link->ObjectKey = Id; Actor->MarkPackageDirty();
    Report(TEXT("Actor linked locally. Use Queue transform to register the instance in Grimoire."));
}
void UGrimoireEditorSubsystem::AddDialogPlayer()
{
    if (Busy || !Database || GEditor->PlayWorld) return;
    auto* World = GEditor->GetEditorWorldContext().World(); if (!World) return;
    for (TActorIterator<AGrimoireDialogActor> It(World); It; ++It)
        if (It->Player->Database == Database)
        { GEditor->SelectNone(false, true); GEditor->SelectActor(*It, true, true); Report(TEXT("Selected the existing Grimoire Dialog Actor.")); return; }
    FScopedTransaction Transaction(NSLOCTEXT("Grimoire", "AddPlayer", "Add Grimoire dialog player"));
    auto* Actor = World->SpawnActor<AGrimoireDialogActor>(); if (!Actor) return; Actor->SetActorLabel(TEXT("Grimoire Dialog"));
    Actor->Player->Database = Database;
    Actor->MarkPackageDirty(); GEditor->SelectNone(false, true); GEditor->SelectActor(Actor, true, true);
    Report(TEXT("Dialog Actor added with a default UMG view. Call StartDialog with a dialog key to play."));
}
FString UGrimoireEditorSubsystem::InstanceId(const AActor* Actor)
{ return Actor ? TEXT("Unreal:") + SceneId(Actor) + TEXT(":") + Actor->GetActorGuid().ToString(EGuidFormats::DigitsWithHyphens) : FString(); }
FString UGrimoireEditorSubsystem::SceneId(const AActor* Actor)
{ return Actor && Actor->GetLevel() ? Actor->GetLevel()->GetOutermost()->GetName() : FString(); }
void UGrimoireEditorSubsystem::SyncActor(UGrimoireObjectLink* Link, bool Unlink, bool Reset)
{
    if (!Link || !Link->GetOwner() || !Link->Database || Link->Database->GameId != GameId) { Report(TEXT("Select the linked Actor's game first.")); return; }
    const auto* O = Link->Database->FindObject(Link->ObjectKey); if (!O) { Report(TEXT("Linked object is missing from the imported data.")); return; }
    auto* Actor = Link->GetOwner();
    if (!Actor->GetActorGuid().IsValid() || !FPackageName::DoesPackageExist(SceneId(Actor))) { Report(TEXT("Save the level before syncing its Actors.")); return; }
    if (!CanStart()) return;
    const FString Id = O->Id, EngineId = InstanceId(Actor), Scene = SceneId(Actor);
    const FTransform Transform = Actor->GetActorTransform();
    TWeakObjectPtr<UGrimoireObjectLink> WeakLink(Link);
    Query(TEXT("GET"), TEXT("/api/v1/objects/") + FGrimoireApiClient::Encode(Id), nullptr,
        [this, WeakLink, Id, EngineId, Scene, Transform, Unlink, Reset](const auto& R)
    {
        auto* L = WeakLink.Get(); if (!L || !L->GetOwner() || GEditor->PlayWorld) { Busy = false; Report(TEXT("Actor no longer available, or Play mode started.")); return; }
        auto Document = AsObject(R.Data); if (!Document || !Object(Document, TEXT("object"))) { Busy = false; Report(TEXT("Invalid object response.")); return; }
        auto Instances = Array(Document, TEXT("game_engine_data")); FObject Existing;
        for (const auto& V : Instances) { auto I = AsObject(V); if (String(I, TEXT("engine_instance_id")) == EngineId && String(I, TEXT("scene")) == Scene) { Existing = I; break; } }
        if (Reset)
        {
            FVector Position, Rotation, Scale;
            if (!Existing || !ReadVector(Object(Existing, TEXT("location")), Position) || !ReadVector(Object(Existing, TEXT("rotation")), Rotation) || !ReadVector(Object(Existing, TEXT("scale")), Scale))
            { Busy = false; Report(TEXT("No valid approved transform for this Actor in Grimoire.")); return; }
            FScopedTransaction Transaction(NSLOCTEXT("Grimoire", "ResetTransform", "Reset Actor to Grimoire")); L->GetOwner()->Modify();
            FTransform Result = L->GetOwner()->GetActorTransform();
            if (L->SyncPosition) Result.SetLocation(Position);
            if (L->SyncRotation) Result.SetRotation(FRotator(Rotation.Y, Rotation.Z, Rotation.X).Quaternion());
            if (L->SyncScale) Result.SetScale3D(Scale);
            L->GetOwner()->SetActorTransform(Result); L->GetOwner()->MarkPackageDirty(); Busy = false; Report(TEXT("Restored the approved Grimoire transform.")); return;
        }
        TArray<TSharedPtr<FJsonValue>> Merged;
        for (const auto& V : Instances) if (AsObject(V) != Existing || !Existing) Merged.Add(V);
        if (!Unlink)
        {
            auto Instance = Existing ? Parse(Write(Existing)) : MakeShared<FJsonObject>();
            Instance->SetStringField(TEXT("engine_instance_id"), EngineId); Instance->SetStringField(TEXT("scene"), Scene);
            if (L->SyncPosition || !Existing) Instance->SetObjectField(TEXT("location"), VectorJson(Transform.GetLocation()));
            const auto Rot = Transform.Rotator();
            if (L->SyncRotation || !Existing) Instance->SetObjectField(TEXT("rotation"), VectorJson(FVector(Rot.Roll, Rot.Pitch, Rot.Yaw)));
            if (L->SyncScale || !Existing) Instance->SetObjectField(TEXT("scale"), VectorJson(Transform.GetScale3D()));
            Merged.Add(MakeShared<FJsonValueObject>(Instance));
        }
        auto Change = MakeShared<FJsonObject>(); Change->SetStringField(TEXT("object_id"), Id); Change->SetArrayField(TEXT("game_engine_data"), Merged);
        QueueCommit((Unlink ? TEXT("Unreal unlink: ") : TEXT("Unreal transform: ")) + L->GetOwner()->GetActorLabel(), Change,
            [this, WeakLink, Unlink](const auto& Commit)
        {
            if (Unlink && WeakLink.IsValid())
            {
                FScopedTransaction Transaction(NSLOCTEXT("Grimoire", "UnlinkActor", "Unlink Actor from Grimoire"));
                WeakLink->Modify(); WeakLink->ObjectKey.Empty(); WeakLink->MarkPackageDirty();
            }
            CompleteWrite(Commit, TEXT("Engine change queued for review in Grimoire. It takes effect after approval."));
        });
    });
}
void UGrimoireEditorSubsystem::SetLocale(const FString& Language)
{
    Locale = Language; SaveSettings();
    for (TObjectIterator<UGrimoireObjectLink> It; It; ++It)
        if (It->GetWorld() && It->GetWorld()->WorldType == EWorldType::Editor && It->Database == Database)
        { It->Modify(); It->PreviewLanguage = Language; It->MarkPackageDirty(); }
    for (TObjectIterator<UGrimoireTextLink> It; It; ++It)
        if (It->GetWorld() && It->GetWorld()->WorldType == EWorldType::Editor && It->Database == Database)
        { It->Modify(); It->PreviewLanguage = Language; It->RefreshText(); It->MarkPackageDirty(); }
    Report(TEXT("Preview language: ") + (Language.IsEmpty() ? TEXT("source") : Language));
}
