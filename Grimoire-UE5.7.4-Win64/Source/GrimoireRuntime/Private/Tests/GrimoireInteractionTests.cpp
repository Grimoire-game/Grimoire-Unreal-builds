#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "GrimoireClickToTalk.h"
#include "GrimoireDialogActor.h"
#include "GrimoireObjectLink.h"
#include "GrimoireJson.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGrimoireClickToTalkTest, "Grimoire.Runtime.ClickToTalk", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGrimoireClickToTalkTest::RunTest(const FString&)
{
    const auto Options = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Options);
    TStrongObjectPtr<UGrimoireDatabase> DB(NewObject<UGrimoireDatabase>());
    DB->GameId = TEXT("game"); FGrimoireObject Object; Object.Id = TEXT("bartel"); Object.CodeId = TEXT("npc_bartel"); DB->Objects.Add(Object);
    FGrimoireDialog Dialog; FString Error;
    UGrimoireDatabase::ParseDialog(GrimoireJson::Parse(TEXT(R"({"id":"opening","dialog_key":"new_dialog","data":{"nodes":[{"id":"hello","type":"end-text","text":"Hello!"}]}})")), Dialog, Error); DB->Dialogs.Add(Dialog);
    auto* PC = World->SpawnActor<APlayerController>(); PC->SetAsLocalPlayerController();
    auto* Pawn = World->SpawnActor<APawn>(); PC->Possess(Pawn);
    auto* NPC = World->SpawnActor<AActor>();
    for (auto* Actor : TArray<AActor*>{Pawn, NPC})
    { auto* Link = NewObject<UGrimoireObjectLink>(Actor); Link->Database = DB.Get(); Link->ObjectKey = TEXT("bartel"); Actor->AddInstanceComponent(Link); }
    auto* Talk = NewObject<UGrimoireClickToTalk>(PC); PC->AddInstanceComponent(Talk); Talk->RegisterComponent();
    Talk->Database = DB.Get(); Talk->TargetObjectKey = TEXT("npc_bartel"); Talk->DialogKey = TEXT("new_dialog"); Talk->RequireLineOfSight = false;
    Talk->DialogActor = World->SpawnActor<AGrimoireDialogActor>(); Talk->DialogActor->Player->Database = DB.Get();
    TestFalse(TEXT("Controlled pawn is never an interaction target"), Talk->IsTalkTarget(Pawn));
    TestTrue(TEXT("Object UUID matches configured Code ID"), Talk->IsTalkTarget(NPC));
    TestTrue(TEXT("Nearby click starts dialog"), Talk->RequestTalkTo(NPC));
    TestTrue(TEXT("Dialog is playing"), Talk->DialogActor->Player->IsPlaying);
    TestTrue(TEXT("Gameplay movement is locked during dialog"), PC->IsMoveInputIgnored());
    TestFalse(TEXT("Repeated interaction cannot restart active dialog"), Talk->RequestTalkTo(NPC));
    Talk->DialogActor->Player->Next(); Talk->TickComponent(.016f, LEVELTICK_All, nullptr);
    TestFalse(TEXT("Movement resumes after dialog ends"), PC->IsMoveInputIgnored());
    TestTrue(TEXT("Can talk again after ending"), Talk->RequestTalkTo(NPC));
    Talk->DialogActor->Player->Stop(); Talk->TickComponent(.016f, LEVELTICK_All, nullptr);
    Talk->DialogKey = TEXT("missing");
    TestFalse(TEXT("Missing dialog rejected before movement"), Talk->RequestTalkTo(NPC));
    TestFalse(TEXT("Failure leaves movement unlocked"), PC->IsMoveInputIgnored());
    World->DestroyWorld(false); return true;
}
#endif
