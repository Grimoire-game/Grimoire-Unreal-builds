#pragma once
#include "CoreMinimal.h"
#include "EditorSubsystem.h"
#include "GrimoireApiClient.h"
#include "GrimoireData.h"
#include "GrimoireEditorSubsystem.generated.h"
class UGrimoireObjectLink;

UCLASS()
class GRIMOIREEDITOR_API UGrimoireEditorSubsystem : public UEditorSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;
    TSharedPtr<FGrimoireApiClient> Api;
    TArray<TSharedPtr<FJsonObject>> Games, Tasks, Users, Statuses;
    UPROPERTY(Transient) TObjectPtr<UGrimoireDatabase> Database;
    FString GameId, GameName, Locale, Status = TEXT("Sign in to connect to Grimoire.");
    bool Busy = false;
    int32 Revision = 0;

    void Login(const FString& Url, const FString& Email, const FString& Password);
    void Verify2FA(const FString& Code);
    void SignOut();
    void Cancel();
    void SelectGame(const TSharedPtr<FJsonObject>& Game);
    void ImportGame();
    void SyncVariables();
    void LoadTasks(const FString& Assignee = TEXT(""));
    void UpdateTask(const FString& TaskId, const FString& StatusKey);
    void CreateObject(const FString& Name, const FString& CodeId, const FString& TemplateId);
    void PushTranslation(const FString& StringId, const FString& Language, const FString& Text);
    void QueueField(const FString& ObjectId, const FString& FieldId, const FString& ValueJson);
    void LinkSelectedActor(const FString& ObjectId);
    void AddDialogPlayer();
    void SyncActor(UGrimoireObjectLink* Link, bool Unlink = false, bool Reset = false);
    void SetLocale(const FString& Language);
    void Report(const FString& Message);
    /** Editor scripting: update existing resources from a trusted offline snapshot of the same game. Does not contact the API or save automatically. */
    UFUNCTION(BlueprintCallable, Category="Grimoire|Import") bool UpdateImportedResources(UGrimoireDatabase* Target, const FString& SourceGameId, const TArray<FGrimoireObject>& Objects, const TArray<FGrimoireDialog>& Dialogs);
    /** Connect a Designer layout with Speaker name, dialog text, Continue and portrait widgets. Returns an error, or an empty string on success. Does not save automatically. */
    UFUNCTION(BlueprintCallable, Category="Grimoire|Designer") FString ConfigureDialogDesigner(UBlueprint* DialogBlueprint, UBlueprint* ChoiceBlueprint);
    static FString InstanceId(const AActor* Actor);
    static FString SceneId(const AActor* Actor);
private:
    UPROPERTY(Transient) TObjectPtr<UGrimoireDatabase> Staging;
    TArray<FString> ImportIds;
    TSet<FString> SeenIds;
    bool ImportingDialogs = false;
    bool VariablesOnly = false;
    TSet<FString> VariableIds, ElementIds;
    int32 ImportIndex = 0;
    void Query(const FString& Verb, const FString& Path, const TSharedPtr<FJsonObject>& Body,
        FGrimoireApiCallback Done, TMap<FString, FString> Parameters = {});
    void ListGames();
    void ImportList(int32 Offset);
    void ImportNext();
    void ImportStrings();
    void ImportVariables(int32 Offset);
    void FinishImport();
    void SaveSettings();
    bool CanStart();
    void CompleteWrite(const FGrimoireApiResult& Result, const FString& Message);
    void QueueCommit(const FString& Title, const TSharedPtr<FJsonObject>& Change, FGrimoireApiCallback Done);
};
