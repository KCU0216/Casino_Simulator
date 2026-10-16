#if WITH_DEV_AUTOMATION_TESTS
#include "RaceGame/RaceManager.h"
#include "RaceGame/RaceRunner.h"
#include "Components/AudioComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRaceAudioFlowTest, "Casino.Race.AudioFlow",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRaceAudioFlowTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Manager = World->SpawnActor<ARaceManager>();
    auto* Runner = World->SpawnActor<ARaceRunner>();
    if (!TestNotNull(TEXT("Manager"), Manager) || !TestNotNull(TEXT("Runner"), Runner)) return false;
    TestTrue(TEXT("Shared cues and crowd use distance attenuation"),
        Manager->RaceCueAudio->bOverrideAttenuation && Manager->CrowdAudio->bOverrideAttenuation
        && Manager->RaceCueAudio->AttenuationOverrides.bAttenuate);
    TestTrue(TEXT("Footsteps follow each runner"), Runner->FootstepAudio->GetAttachParent() == Runner->GetRootComponent());
    TestNull(TEXT("Race crowd sound is unassigned"), Manager->CrowdAudio->Sound.Get());
    TestNull(TEXT("Race footstep sound is unassigned"), Runner->FootstepAudio->Sound.Get());

    Manager->OnRep_Phase();
    Manager->Phase = ERacePhase::Betting;
    Manager->OnRep_Phase();
    Manager->BettingStartServerTime = World->GetTimeSeconds() - Manager->BettingDuration + 2.8;
    Manager->UpdateCountdownAudio();
    TestEqual(TEXT("Countdown begins at three"), Manager->LastCountdownSecond, 3);
    TestTrue(TEXT("Countdown selects short cue"), Manager->RaceCueAudio->Sound == Manager->CountdownSound);
    Manager->RaceCueAudio->SetSound(nullptr);
    Manager->UpdateCountdownAudio();
    TestNull(TEXT("Same countdown second is not replayed"), Manager->RaceCueAudio->Sound.Get());
    Manager->BettingStartServerTime -= 1.0;
    Manager->UpdateCountdownAudio();
    TestEqual(TEXT("Countdown advances with server clock"), Manager->LastCountdownSecond, 2);

    Manager->Phase = ERacePhase::Racing;
    Manager->OnRep_Phase();
    TestTrue(TEXT("Racing plays the start cue"), Manager->RaceCueAudio->Sound == Manager->StartSound);
    Manager->RaceCueAudio->SetSound(nullptr);
    Manager->OnRep_Phase();
    TestNull(TEXT("Repeated phase replication does not replay start"), Manager->RaceCueAudio->Sound.Get());
    Manager->Phase = ERacePhase::Exiting;
    Manager->OnRep_Phase();
    TestNull(TEXT("Race completion does not select a finish cue"), Manager->RaceCueAudio->Sound.Get());
    TestFalse(TEXT("Race crowd stays silent"), Manager->CrowdAudio->IsPlaying());
    auto* LateManager = World->SpawnActor<ARaceManager>();
    LateManager->Phase = ERacePhase::Racing;
    LateManager->OnRep_Phase();
    TestNull(TEXT("Late join does not announce a second start"), LateManager->RaceCueAudio->Sound.Get());

    FRunnerRaceScript Script;
    Script.Speed = 260.f;
    Script.RaceStartServerTime = World->GetTimeSeconds();
    Runner->ServerSetupScript(Script);
    Runner->ServerSetRacing(true, Script.RaceStartServerTime);
    TestFalse(TEXT("Runner footsteps stay silent during racing"), Runner->FootstepAudio->IsPlaying());
    Runner->ServerSetRacing(false);
    TestFalse(TEXT("Runner footsteps stop outside racing"), Runner->FootstepAudio->IsPlaying());
    return true;
}
#endif
