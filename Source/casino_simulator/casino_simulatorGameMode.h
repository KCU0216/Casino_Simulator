// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "TimerManager.h"
#include "casino_loop_gamestate.h"
#include "casino_simulatorGameMode.generated.h"



class APawn;
class Acasino_simulatorCharacter;


/**
 *  Simple GameMode for a first person game
 */
UCLASS(abstract)
class Acasino_simulatorGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	Acasino_simulatorGameMode();
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	UFUNCTION(BlueprintCallable, Category = "Police")
	void ArrestPlayer(APawn* TargetPlayer, AActor* PoliceActor, AActor* JailPoint);

	UFUNCTION(BlueprintCallable, Category = "Police|Jail")
	bool PayBail(APawn* Player, float BailAmount);

	UFUNCTION(BlueprintCallable, Category = "Thief")
	void StealMoney(APawn* TargetPlayer, AActor* ThiefActor);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thief")
	int St_Money_Max = 1000;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Thief")
	int St_Money_Min = 100;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1"))
    int32 StartDay = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1"))
    int32 DaysToPlay = 5;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1.0"))
    float DayDurationSeconds = 600.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1.0"))
    float PaymentDurationSeconds = 30.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="0.1"))
    float ResultDurationSeconds = 5.0f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop")
    bool bAutoStartDayLoop = false;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop")
    TArray<int32> DailyPayments = {100, 200, 350, 500, 750};
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1"))
    int32 DefaultDailyPayment = 100;
    // Place one tagged TargetPoint per player. Locations must be clear of collision.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop")
    FName PaymentSpawnTag = TEXT("CasinoPaymentSpawn");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Casino|Loop", meta=(ClampMin="1.0"))
    float PaymentRadius = 400.0f;

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Casino|Loop")
    void StartDayLoop();
    UFUNCTION(BlueprintPure, Category="Casino|Loop")
    bool CanPlayCasino() const;
    // Server only; client UI calls the PlayerController RPC instead.
    bool SubmitDailyPayment(Acasino_simulatorCharacter* Player, int32 Amount);

    // Implement in existing GameMode BP: cancel all casino timers/payouts,
    // forfeit current wagers without charging twice, and release seats.
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|Loop")
    void ForceEndCasinoGamesForDay();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void PreLogin(const FString& Options, const FString& Address,
        const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
private:
    FTimerHandle OnlineArrivalTimer;
    double OnlineArrivalDeadline = 0.0;
    int32 OnlineExpectedPlayers = 0;
    void WaitForOnlinePlayers();
    FTimerHandle DayLoopTimer;
    FTimerHandle PaymentTimer;
    FTimerHandle NextDayTimer;
    TArray<TWeakObjectPtr<AActor>> PaymentSpawns;
    bool bCollectingPayment = false;
    void BeginCasinoDay(int32 Day);
    void BeginPaymentPhase();
    void FinishPaymentPhase();
    void AdvanceCasinoDay();
    bool MovePlayersToCentralSpawns(bool bForPayment);

};



