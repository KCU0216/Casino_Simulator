#if WITH_DEV_AUTOMATION_TESTS

#include "Blackjack/BlackjackPlayerComponent.h"
#include "Blackjack/BlackjackTableActor.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Online/CasinoOnlineSettings.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"
#include "casino_loop_gamestate.h"
#include "casino_simulatorCharacter.h"
#include "casino_simulatorPlayerController.h"

namespace BlackjackSeatFacingPIE
{
struct FPlaySettingsSnapshot
{
    EPlayNetMode Mode = PIE_Standalone;
    bool bOneProcess = true;
    bool bSeparateServer = false;
    int32 Players = 1;
    bool bRestored = false;
    TArray<FNetDriverDefinition> OriginalNetDriverDefinitions;

    FPlaySettingsSnapshot()
    {
        ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
        Settings->GetPlayNetMode(Mode);
        Settings->GetRunUnderOneProcess(bOneProcess);
        Settings->GetPlayNumberOfClients(Players);
        bSeparateServer = Settings->bLaunchSeparateServer;
        Settings->SetPlayNetMode(PIE_ListenServer);
        Settings->SetRunUnderOneProcess(true);
        Settings->SetPlayNumberOfClients(2);
        Settings->bLaunchSeparateServer = false;
        // Local replication needs no EOS user. Override only the live engine's
        // test driver definition; production config files are never changed.
        OriginalNetDriverDefinitions = GEngine->NetDriverDefinitions;
        FNetDriverDefinition* GameDriver = GEngine->NetDriverDefinitions.FindByPredicate(
            [](const FNetDriverDefinition& Definition) { return Definition.DefName == TEXT("GameNetDriver"); });
        if (!GameDriver)
        {
            FNetDriverDefinition Definition;
            Definition.DefName = TEXT("GameNetDriver");
            GameDriver = &GEngine->NetDriverDefinitions.Add_GetRef(Definition);
        }
        GameDriver->DriverClassName = TEXT("/Script/OnlineSubsystemUtils.IpNetDriver");
        GameDriver->DriverClassNameFallback = GameDriver->DriverClassName;
    }
    ~FPlaySettingsSnapshot() { Restore(); }
    void Restore()
    {
        if (bRestored) return;
        ULevelEditorPlaySettings* Settings = GetMutableDefault<ULevelEditorPlaySettings>();
        Settings->SetPlayNetMode(Mode);
        Settings->SetRunUnderOneProcess(bOneProcess);
        Settings->SetPlayNumberOfClients(Players);
        Settings->bLaunchSeparateServer = bSeparateServer;
        if (GEngine) GEngine->NetDriverDefinitions = OriginalNetDriverDefinitions;
        bRestored = true;
    }
};

class FRestoreSettingsCommand final : public IAutomationLatentCommand
{
public:
    explicit FRestoreSettingsCommand(TSharedPtr<FPlaySettingsSnapshot> InSettings) : Settings(MoveTemp(InSettings)) {}
    virtual bool Update() override { Settings->Restore(); return true; }
private:
    TSharedPtr<FPlaySettingsSnapshot> Settings;
};

class FSeatReplicationCommand final : public IAutomationLatentCommand
{
public:
    explicit FSeatReplicationCommand(FAutomationTestBase* InTest) : Test(InTest) {}
    virtual bool Update() override
    {
        if (Started == 0.0) Started = FPlatformTime::Seconds();
        if (FPlatformTime::Seconds() - Started > 90.0)
        {
            Test->AddError(FString::Printf(TEXT("Network seat-facing stage %d timed out (server=%s client=%s)."), Stage,
                *GetNameSafe(ServerWorld.Get()), *GetNameSafe(ClientWorld.Get())));
            Cleanup();
            return true;
        }
        if (Stage == 0) return FindPlayers();
        if (!AllPawnsValid()) { Test->AddError(TEXT("A possessed network pawn disappeared.")); Cleanup(); return true; }
        if (Stage == 1)
        {
            ABlackjackTableActor* ClientTable = FindClientTable();
            if (!ClientTable) return false;
            ForcePlaying();
            if (!Test->TestTrue(TEXT("Host can claim seat 0"), Table->TryClaimSeat(Pawns[0].Get(), 0))
                || !Test->TestTrue(TEXT("Remote player can claim seat 1"), Table->TryClaimSeat(Pawns[1].Get(), 1)))
            { Cleanup(); return true; }
            Pawns[0]->GetBlackjackPlayerComponent()->EnterBlackjackSeatMode(Table.Get(), 0);
            // Exercise the owning client's actual reliable seat-mode RPC.
            Pawns[2]->GetBlackjackPlayerComponent()->EnterBlackjackSeatMode(ClientTable, 1);
            Advance(2);
            return false;
        }
        if (Stage == 2)
        {
            if (!AllSeatStates(true) || GFrameCounter < StageFrame + 12) return false;
            CheckSeated(TEXT("After replication"));
            SetDivergentControlYaw();
            Advance(3);
            return false;
        }
        if (Stage == 3)
        {
            if (GFrameCounter < StageFrame + 12) return false;
            CheckSeated(TEXT("After stale control rotation and network ticks"));
            Pawns[0]->GetBlackjackPlayerComponent()->CompleteExitBlackjackSeat();
            Pawns[2]->GetBlackjackPlayerComponent()->CompleteExitBlackjackSeat();
            Advance(4);
            return false;
        }
        if (Stage == 4)
        {
            if (!AllSeatStates(false) || GFrameCounter < StageFrame + 12) return false;
            for (int32 Index = 0; Index < 4; ++Index)
            {
                Test->TestEqual(FString::Printf(TEXT("Replica %d restores its original yaw flag"), Index),
                    Pawns[Index]->bUseControllerRotationYaw, OriginalYawFlags[Index]);
                ExitYaw[Index] = Pawns[Index]->GetActorRotation().Yaw;
            }
            // With zero new view input, UpdateRotation must not resurrect the
            // divergent view yaw used while seated.
            for (Acasino_simulatorPlayerController* Controller : {HostController.Get(), RemoteServerController.Get(), RemoteOwnerController.Get()})
                if (Controller) Controller->UpdateRotation(1.0f / 60.0f);
            Advance(5);
            return false;
        }
        if (Stage == 5)
        {
            if (GFrameCounter < StageFrame + 12) return false;
            for (int32 Index = 0; Index < 4; ++Index)
                Test->TestTrue(FString::Printf(TEXT("Replica %d does not snap to stale view yaw after exit"), Index),
                    FMath::Abs(FMath::FindDeltaAngleDegrees(ExitYaw[Index], Pawns[Index]->GetActorRotation().Yaw)) < 2.0f);
            Cleanup();
            return true;
        }
        return false;
    }

private:
    bool FindPlayers()
    {
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType != EWorldType::PIE || !Context.World()) continue;
            if (Context.World()->GetNetMode() == NM_ListenServer) ServerWorld = Context.World();
            else if (Context.World()->GetNetMode() == NM_Client) ClientWorld = Context.World();
        }
        if (!ServerWorld.IsValid() || !ClientWorld.IsValid()) return false;
        for (FConstPlayerControllerIterator It = ServerWorld->GetPlayerControllerIterator(); It; ++It)
        {
            Acasino_simulatorPlayerController* Controller = Cast<Acasino_simulatorPlayerController>(It->Get());
            if (!Controller || !Controller->GetPawn() || !Controller->PlayerState) continue;
            if (Controller->IsLocalController()) HostController = Controller;
            else RemoteServerController = Controller;
        }
        for (FConstPlayerControllerIterator It = ClientWorld->GetPlayerControllerIterator(); It; ++It)
        {
            Acasino_simulatorPlayerController* Controller = Cast<Acasino_simulatorPlayerController>(It->Get());
            if (Controller && Controller->IsLocalController() && Controller->GetPawn() && Controller->PlayerState) RemoteOwnerController = Controller;
        }
        if (!HostController.IsValid() || !RemoteServerController.IsValid() || !RemoteOwnerController.IsValid()) return false;
        const int32 HostId = HostController->PlayerState->GetPlayerId();
        const int32 RemoteId = RemoteServerController->PlayerState->GetPlayerId();
        if (HostId == RemoteId || RemoteOwnerController->PlayerState->GetPlayerId() != RemoteId) return false;
        Pawns[0] = Cast<Acasino_simulatorCharacter>(HostController->GetPawn());
        Pawns[1] = Cast<Acasino_simulatorCharacter>(RemoteServerController->GetPawn());
        Pawns[2] = Cast<Acasino_simulatorCharacter>(RemoteOwnerController->GetPawn());
        for (TActorIterator<Acasino_simulatorCharacter> It(ClientWorld.Get()); It; ++It)
            if (It->GetPlayerState() && It->GetPlayerState()->GetPlayerId() == HostId) Pawns[3] = *It;
        if (!AllPawnsValid()) return false;
        Test->TestTrue(TEXT("Host on the remote client is a simulated proxy"), Pawns[3]->GetLocalRole() == ROLE_SimulatedProxy);
        for (int32 Index = 0; Index < 4; ++Index) OriginalYawFlags[Index] = Pawns[Index]->bUseControllerRotationYaw;
        ForcePlaying();
        FActorSpawnParameters Spawn;
        Spawn.Name = MakeUniqueObjectName(ServerWorld.Get(), ABlackjackTableActor::StaticClass(), TEXT("SeatFacingNetworkTestTable"));
        Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Table = ServerWorld->SpawnActor<ABlackjackTableActor>(Pawns[0]->GetActorLocation(), FRotator(0.0f, 73.0f, 0.0f), Spawn);
        if (!Test->TestNotNull(TEXT("Replicated native test table"), Table.Get())) return true;
        Table->SetReplicateMovement(true);
        Table->bAlwaysRelevant = true;
        Table->ForceNetUpdate();
        Test->AddInfo(FString::Printf(TEXT("Network seat test: hostId=%d remoteId=%d table=%s"), HostId, RemoteId, *Table->GetName()));
        Advance(1);
        return false;
    }

    bool AllPawnsValid() const
    {
        for (const TWeakObjectPtr<Acasino_simulatorCharacter>& Pawn : Pawns)
            if (!Pawn.IsValid() || !Pawn->GetBlackjackPlayerComponent()) return false;
        return true;
    }
    ABlackjackTableActor* FindClientTable() const
    {
        if (!Table.IsValid()) return nullptr;
        for (TActorIterator<ABlackjackTableActor> It(ClientWorld.Get()); It; ++It)
            if (It->GetClass() == ABlackjackTableActor::StaticClass()
                && It->GetActorLocation().Equals(Table->GetActorLocation(), 2.0f)
                && It->GetActorRotation().Equals(Table->GetActorRotation(), 2.0f)) return *It;
        return nullptr;
    }
    void ForcePlaying()
    {
        if (ACasinoLoopGameState* GameState = ServerWorld->GetGameState<ACasinoLoopGameState>())
        {
            FCasinoLoopStatus Status = GameState->LoopStatus;
            Status.Phase = ECasinoLoopPhase::Playing;
            Status.DayEndServerTime = GameState->GetServerWorldTimeSeconds() + 600.0;
            GameState->LoopStatus = Status;
            GameState->OnLoopChanged.Broadcast();
            GameState->ForceNetUpdate();
        }
    }
    bool AllSeatStates(bool bSeated) const
    {
        const int32 Seats[] = {0, 1, 1, 0};
        for (int32 Index = 0; Index < 4; ++Index)
        {
            UBlackjackPlayerComponent* Component = Pawns[Index]->GetBlackjackPlayerComponent();
            if (Component->IsInBlackjackSeat() != bSeated || (bSeated && Component->GetCurrentSeatIndex() != Seats[Index])) return false;
        }
        return true;
    }
    void CheckSeated(const TCHAR* Phase)
    {
        const int32 Seats[] = {0, 1, 1, 0};
        for (int32 Index = 0; Index < 4; ++Index)
        {
            Acasino_simulatorCharacter* Pawn = Pawns[Index].Get();
            const float ExpectedYaw = Table->GetSeatPoint(Seats[Index])->GetComponentRotation().Yaw;
            const FRotator Body = Pawn->GetActorRotation();
            Test->TestFalse(FString::Printf(TEXT("%s replica %d ignores controller yaw"), Phase, Index), Pawn->bUseControllerRotationYaw);
            Test->TestTrue(FString::Printf(TEXT("%s replica %d faces its SeatPoint"), Phase, Index),
                FMath::Abs(FMath::FindDeltaAngleDegrees(ExpectedYaw, Body.Yaw)) < 2.0f);
            Test->TestTrue(FString::Printf(TEXT("%s replica %d remains upright"), Phase, Index),
                FMath::Abs(Body.Pitch) < 0.1f && FMath::Abs(Body.Roll) < 0.1f);
        }
        if (USkeletalMeshComponent* Mesh = Pawns[3]->GetMesh())
        {
            const FQuat ExpectedMesh = Pawns[3]->GetActorQuat() * Pawns[3]->GetBaseRotationOffset();
            Test->TestTrue(FString::Printf(TEXT("%s simulated host mesh converges to its seated body rotation"), Phase),
                FMath::RadiansToDegrees(ExpectedMesh.AngularDistance(Mesh->GetComponentQuat())) < 3.0f);
        }
    }
    void SetDivergentControlYaw()
    {
        for (Acasino_simulatorPlayerController* Controller : {HostController.Get(), RemoteServerController.Get(), RemoteOwnerController.Get()})
        {
            if (!Controller) continue;
            FRotator View = Controller->GetControlRotation();
            View.Yaw += 142.0f;
            Controller->SetControlRotation(View);
            Controller->UpdateRotation(1.0f / 60.0f);
        }
    }
    void Advance(int32 NextStage) { Stage = NextStage; StageFrame = GFrameCounter; Started = FPlatformTime::Seconds(); }
    void Cleanup()
    {
        if (!Table.IsValid()) return;
        for (int32 Index : {0, 1}) if (Pawns[Index].IsValid()) Table->ForceLeaveSeat(Pawns[Index].Get());
        Table->Destroy();
    }

    FAutomationTestBase* Test;
    int32 Stage = 0;
    double Started = 0.0;
    uint64 StageFrame = 0;
    TWeakObjectPtr<UWorld> ServerWorld, ClientWorld;
    TWeakObjectPtr<Acasino_simulatorPlayerController> HostController, RemoteServerController, RemoteOwnerController;
    // Listen host, server copy of remote, remote owner, remote simulated host.
    TWeakObjectPtr<Acasino_simulatorCharacter> Pawns[4];
    bool OriginalYawFlags[4] = {};
    float ExitYaw[4] = {};
    TWeakObjectPtr<ABlackjackTableActor> Table;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBlackjackSeatFacingNetworkPIETest,
    "Casino.Blackjack.SeatFacing.NetworkPIE", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FBlackjackSeatFacingNetworkPIETest::RunTest(const FString& Parameters)
{
    if (!GEditor || !GEngine || GEditor->PlayWorld) { AddError(TEXT("Run network seat-facing tests from a stopped editor.")); return false; }
    const FString Map = GetDefault<UCasinoOnlineSettings>()->GameMap.ToSoftObjectPath().GetLongPackageName();
    if (!TestFalse(TEXT("Gameplay map is configured"), Map.IsEmpty())) return false;
    TSharedPtr<BlackjackSeatFacingPIE::FPlaySettingsSnapshot> Settings = MakeShared<BlackjackSeatFacingPIE::FPlaySettingsSnapshot>();
    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(Map));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<BlackjackSeatFacingPIE::FSeatReplicationCommand>(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    FAutomationTestFramework::Get().EnqueueLatentCommand(MakeShared<BlackjackSeatFacingPIE::FRestoreSettingsCommand>(Settings));
    return true;
}

#endif
