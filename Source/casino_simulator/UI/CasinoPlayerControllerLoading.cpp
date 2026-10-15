#include "casino_simulatorPlayerController.h"
#include "casino_simulatorGameMode.h"
#include "UI/CasinoTravelLoadingSubsystem.h"
#include "UI/CasinoUIManagerComponent.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/CharacterMovementComponent.h"

void Acasino_simulatorPlayerController::ClientBeginCasinoMatchLoading_Implementation(FGuid Generation)
{
    if (!IsLocalController() || !Generation.IsValid()) return;
    ClientLoadingGeneration = Generation;
    bClientMatchLoading = true;
    bClientWorldReadySent = false;
    bClientSpawnReadySent = false;
    bPendingCasinoSpawn = false;
    ClientLoadingPawn.Reset();
    ClientFinalizedPawn.Reset();
    LocalLoadingReadySince = 0.0;
    if (auto* Loading = GetGameInstance()->GetSubsystem<UCasinoTravelLoadingSubsystem>())
    {
        const auto* GS = GetWorld()->GetGameState<ACasinoLoopGameState>();
        Loading->BeginLoading(GS ? GS->MatchLoadingStatus.ExpectedPlayers : 0);
        Loading->SetLocalWorldReady(false);
        auto* GI = GetGameInstance();
        Loading->SetFailureExitHandler(FSimpleDelegate::CreateWeakLambda(GI, [GI]
        {
            if (auto* Online = GI->GetSubsystem<UCasinoOnlineSubsystem>()) Online->LeaveRoom();
        }));
    }
    SetCasinoUIScreen(ECasinoUIScreen::Loading);
    ApplyUIScreenInput();
}

void Acasino_simulatorPlayerController::ClientFinalizeCasinoSpawn_Implementation(FGuid Generation, FTransform SpawnTransform)
{
    if (!IsLocalController() || !bClientMatchLoading || Generation != ClientLoadingGeneration) return;
    PendingCasinoSpawn = SpawnTransform;
    bPendingCasinoSpawn = true;
    bClientSpawnReadySent = false;
    ClientFinalizedPawn.Reset();
}

void Acasino_simulatorPlayerController::ServerReportCasinoWorldReady_Implementation(FGuid Generation)
{
    if (auto* GM = GetWorld()->GetAuthGameMode<Acasino_simulatorGameMode>())
        GM->NotifyClientWorldReady(this, Generation);
}

void Acasino_simulatorPlayerController::ServerReportCasinoSpawnReady_Implementation(FGuid Generation)
{
    if (auto* GM = GetWorld()->GetAuthGameMode<Acasino_simulatorGameMode>())
        GM->NotifyClientSpawnReady(this, Generation);
}

void Acasino_simulatorPlayerController::UpdateCasinoMatchLoading()
{
    if (!IsLocalController() || !GetGameInstance()) return;
    auto* GS = GetWorld()->GetGameState<ACasinoLoopGameState>();
    if (!GS) return;
    // A listen-host readiness RPC can finish the server barrier synchronously.
    // Use one snapshot for this tick so the subsystem receives Complete on the
    // next tick before the controller stops updating and dismisses its overlay.
    const FCasinoMatchLoadingStatus Status = GS->MatchLoadingStatus;
    if ((Status.IsPending() || Status.Stage == ECasinoMatchLoadingStage::Failed) && Status.Generation.IsValid()
        && Status.Generation != ClientLoadingGeneration)
        ClientBeginCasinoMatchLoading_Implementation(Status.Generation);
    if (!bClientMatchLoading || Status.Generation != ClientLoadingGeneration) return;
    auto* Loading = GetGameInstance()->GetSubsystem<UCasinoTravelLoadingSubsystem>();
    if (!Loading) return;
    if (Status.Stage == ECasinoMatchLoadingStage::Failed)
    {
        Loading->FailLoading(Status.FailureReason.ToString());
        bShowMouseCursor = true;
        SetInputMode(FInputModeGameAndUI().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
        return;
    }
    Loading->SetPreparingSpawns(Status.Stage == ECasinoMatchLoadingStage::PreparingSpawns);
    Loading->UpdatePlayerReadiness(Status.ReadyPlayers, Status.ExpectedPlayers,
        Status.Stage == ECasinoMatchLoadingStage::Complete);
    APawn* LoadingPawn = GetPawn();
    const bool bWorldReady = UIManager->IsLocalUIReady() && !UIManager->IsTravelPending()
        && !GetWorld()->IsInSeamlessTravel() && HasClientLoadedCurrentWorld()
        && IsValid(LoadingPawn) && LoadingPawn->GetWorld() == GetWorld() && LoadingPawn->HasActorBegunPlay()
        && IsValid(PlayerState) && Loading->AreLocalLoadingTasksComplete();
    if (!bWorldReady)
    {
        LocalLoadingReadySince = 0.0;
        Loading->SetLocalWorldReady(false);
        return;
    }
    if (ClientLoadingPawn.Get() != LoadingPawn)
    {
        ClientLoadingPawn = LoadingPawn;
        bClientWorldReadySent = false;
        LocalLoadingReadySince = 0.0;
    }
    if (LocalLoadingReadySince == 0.0) LocalLoadingReadySince = FPlatformTime::Seconds();
    // Give the new world a few rendered frames before accepting local readiness.
    if (FPlatformTime::Seconds() - LocalLoadingReadySince < 0.25) return;
    Loading->SetLocalWorldReady(true);
    if (!bClientWorldReadySent && Status.Stage == ECasinoMatchLoadingStage::WaitingForPlayers)
    {
        bClientWorldReadySent = true;
        ServerReportCasinoWorldReady(ClientLoadingGeneration);
    }
    if (bPendingCasinoSpawn && Status.Stage == ECasinoMatchLoadingStage::PreparingSpawns)
    {
        if (ClientFinalizedPawn.Get() != LoadingPawn)
        {
            // Controller and pawn use separate replication channels. Apply the server's
            // final transform before acknowledging, even if pawn movement arrives later.
            LoadingPawn->SetActorLocationAndRotation(PendingCasinoSpawn.GetLocation(), PendingCasinoSpawn.Rotator(),
                false, nullptr, ETeleportType::TeleportPhysics);
            SetControlRotation(PendingCasinoSpawn.Rotator());
            SetViewTarget(LoadingPawn);
            if (auto* LoadingCharacter = Cast<ACharacter>(LoadingPawn))
            {
                LoadingCharacter->GetCharacterMovement()->StopMovementImmediately();
                LoadingCharacter->GetCharacterMovement()->DisableMovement();
            }
            ClientFinalizedPawn = LoadingPawn;
            LocalSpawnAppliedFrame = GFrameCounter;
        }
        if (!bClientSpawnReadySent && GFrameCounter > LocalSpawnAppliedFrame + 1
            && LoadingPawn->GetActorLocation().Equals(PendingCasinoSpawn.GetLocation(), 2.0f))
        {
            bClientSpawnReadySent = true;
            ServerReportCasinoSpawnReady(ClientLoadingGeneration);
        }
    }
    if (Status.Stage == ECasinoMatchLoadingStage::Complete && bClientSpawnReadySent)
    {
        bClientMatchLoading = false;
        bPendingCasinoSpawn = false;
        Loading->FinishLoading();
        RefreshCasinoUIScreen();
    }
}
