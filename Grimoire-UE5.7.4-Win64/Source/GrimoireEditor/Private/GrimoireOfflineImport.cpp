#include "GrimoireEditorSubsystem.h"
#include "GrimoireJson.h"
#include "ScopedTransaction.h"

bool UGrimoireEditorSubsystem::UpdateImportedResources(UGrimoireDatabase* Target, const FString& SourceGameId, const TArray<FGrimoireObject>& Objects, const TArray<FGrimoireDialog>& Dialogs)
{
    if (!Target || SourceGameId.IsEmpty() || Target->GameId != SourceGameId || (Objects.IsEmpty() && Dialogs.IsEmpty())) return false;
    TSet<FString> SeenObjects, SeenDialogs;
    for (const auto& Object : Objects)
    {
        if (Object.Id.IsEmpty() || !Target->Objects.ContainsByPredicate([&](const FGrimoireObject& O) { return O.Id == Object.Id; }) || SeenObjects.Contains(Object.Id)) return false;
        SeenObjects.Add(Object.Id); TSet<FString> Fields;
        for (const auto& Field : Object.Fields)
        { if (Field.Id.IsEmpty() || Fields.Contains(Field.Id)) return false; Fields.Add(Field.Id); }
    }
    for (const auto& Dialog : Dialogs)
    {
        const auto Graph = GrimoireJson::Parse(Dialog.GraphJson);
        if (Dialog.Id.IsEmpty() || !Target->Dialogs.ContainsByPredicate([&](const FGrimoireDialog& D) { return D.Id == Dialog.Id; }) || SeenDialogs.Contains(Dialog.Id) || !Graph || GrimoireJson::Array(Graph, TEXT("nodes")).IsEmpty()) return false;
        SeenDialogs.Add(Dialog.Id);
    }
    const FScopedTransaction Transaction(NSLOCTEXT("Grimoire", "OfflineImport", "Update Grimoire from offline snapshot"));
    Target->Modify();
    for (const auto& Object : Objects) for (auto& Existing : Target->Objects) if (Existing.Id == Object.Id) { Existing = Object; break; }
    for (const auto& Dialog : Dialogs) for (auto& Existing : Target->Dialogs) if (Existing.Id == Dialog.Id) { Existing = Dialog; break; }
    Target->MarkPackageDirty(); Target->PostEditChange(); ++Revision;
    return true;
}
