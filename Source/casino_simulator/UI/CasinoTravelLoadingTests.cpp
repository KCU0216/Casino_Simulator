#if WITH_DEV_AUTOMATION_TESTS
#include "UI/CasinoTravelLoadingSubsystem.h"
#include "UI/CasinoTravelLoadingView.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Subsystems/SubsystemCollection.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoTravelLoadingPresentationTest, "Casino.UI.TravelLoading.Presentation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoTravelLoadingPresentationTest::RunTest(const FString& Parameters)
{
    const auto DuringMap = MakeCasinoTravelLoadingSnapshot(false, false, false, 0, 4);
    TestFalse(TEXT("Map load is indeterminate instead of reporting invented percentages"), DuringMap.Progress.IsSet());
    const auto Waiting = MakeCasinoTravelLoadingSnapshot(true, true, false, 3, 4);
    TestEqual(TEXT("Ready-player fraction advances the arrival stage"), Waiting.Progress.GetValue(), .6f);
    const auto AllReportedButServerNotReleased = MakeCasinoTravelLoadingSnapshot(true, true, false, 4, 4);
    TestTrue(TEXT("Never full before server release"), AllReportedButServerNotReleased.Progress.GetValue() < 1.f);
    const auto StillPreparingLocal = MakeCasinoTravelLoadingSnapshot(true, false, true, 4, 4);
    TestTrue(TEXT("Never full before this client is ready"), StillPreparingLocal.Progress.GetValue() < 1.f);
    const auto Complete = MakeCasinoTravelLoadingSnapshot(true, true, true, 4, 4);
    TestEqual(TEXT("Full only when both readiness gates pass"), Complete.Progress.GetValue(), 1.f);
    TestEqual(TEXT("Unknown participant count remains safely incomplete"), MakeCasinoTravelLoadingSnapshot(true, false, false, 7, 0).Progress.GetValue(), 0.f);
    FCasinoTravelLoadingViewState View;
    View.Write(Waiting);
    FCasinoTravelLoadingSnapshot Copy = View.Read();
    Copy.Progress = 1.f;
    TestEqual(TEXT("Slate reads isolated snapshots instead of mutable game-thread data"), View.Read().Progress.GetValue(), .6f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoTravelLoadingIsolationTest, "Casino.UI.TravelLoading.WorldIsolationAndExit",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoTravelLoadingIsolationTest::RunTest(const FString& Parameters)
{
    auto* OwnGI = NewObject<UGameInstance>(GEngine);
    auto* OtherGI = NewObject<UGameInstance>(GEngine);
    auto* Loading = NewObject<UCasinoTravelLoadingSubsystem>(OwnGI);
    FSubsystemCollection<UGameInstanceSubsystem> Collection;
    Loading->Initialize(Collection);
    UWorld* OwnWorld = UWorld::CreateWorld(EWorldType::Game, false);
    UWorld* OtherWorld = UWorld::CreateWorld(EWorldType::Game, false);
    OwnWorld->SetGameInstance(OwnGI);
    OtherWorld->SetGameInstance(OtherGI);
    ON_SCOPE_EXIT
    {
        Loading->Deinitialize();
        OwnWorld->DestroyWorld(false);
        OtherWorld->DestroyWorld(false);
    };

    Loading->BeginLoading(4);
    TestTrue(TEXT("Loading begins before map travel"), Loading->IsLoadingActive());
    Loading->HandlePostLoadMap(OtherWorld);
    TestFalse(TEXT("Another PIE instance cannot finish this map-load stage"), Loading->bMapLoaded);
    Loading->HandleNetworkFailure(OtherWorld, nullptr, ENetworkFailure::ConnectionLost, TEXT("foreign error"));
    TestFalse(TEXT("Another PIE instance cannot fail this travel"), Loading->HasLoadingFailed());
    Loading->HandlePostLoadMap(OwnWorld);
    TestTrue(TEXT("Own world enters nonblocking viewport waiting stage"), Loading->bMapLoaded);
    Loading->UpdatePlayerReadiness(4, 4, true);
    Loading->FinishLoading();
    TestEqual(TEXT("Server completion alone cannot dismiss before local spawn readiness"), Loading->CompletionDeadline, 0.0);
    Loading->SetLocalWorldReady(true);
    Loading->FinishLoading();
    TestTrue(TEXT("Both readiness gates allow completion"), Loading->CompletionDeadline > 0.0);

    Loading->UpdatePlayerReadiness(0, 4, false);
    Loading->SetPreparingSpawns(true);
    TestEqual(TEXT("Final-spawn acknowledgements start after the arrival stage rather than resetting the bar"), Loading->ViewState->Read().Progress.GetValue(), .8f);
    Loading->UpdatePlayerReadiness(4, 4, false);
    TestTrue(TEXT("All spawn acknowledgements remain below 100 until server release"), Loading->ViewState->Read().Progress.GetValue() < 1.f);

    Loading->HandleNetworkFailure(OwnWorld, nullptr, ENetworkFailure::ConnectionLost, TEXT("own error"));
    TestTrue(TEXT("Own-world disconnection shows failure"), Loading->HasLoadingFailed());
    TestEqual(TEXT("Failure prevents the pending completion timer from hiding its exit action"), Loading->CompletionDeadline, 0.0);
    TestTrue(TEXT("Failure snapshot exposes exit action"), Loading->ViewState->Read().bFailed);
    int32 ExitCalls = 0;
    Loading->SetFailureExitHandler(FSimpleDelegate::CreateLambda([&ExitCalls] { ++ExitCalls; }));
    Loading->RequestFailureExit();
    TestEqual(TEXT("Safe exit delegates session cleanup once"), ExitCalls, 1);
    TestTrue(TEXT("Failure overlay persists while asynchronous session cleanup is pending"), Loading->IsLoadingActive());
    Loading->RequestFailureExit();
    TestEqual(TEXT("Session cleanup failure can be retried from the same exit action"), ExitCalls, 2);
    Loading->CancelLoading();
    TestFalse(TEXT("Actual return completion can remove the overlay"), Loading->IsLoadingActive());
    Loading->BeginLoading(2);
    TestFalse(TEXT("New attempt clears earlier failure"), Loading->HasLoadingFailed());
    Loading->CancelLoading();
    return true;
}
#endif
