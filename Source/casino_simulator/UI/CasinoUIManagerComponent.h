#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "UI/CasinoUIRoot.h"
#include "casino_loop_gamestate.h"
#include "CasinoUIManagerComponent.generated.h"
class Acasino_simulatorPlayerController;

// Local presentation only. Gameplay authority remains in game/server code.
UCLASS(ClassGroup=(Casino), meta=(BlueprintSpawnableComponent))
class CASINO_SIMULATOR_API UCasinoUIManagerComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Casino|UI")
    bool ShowInteractionUI(UUserWidget* Widget);
    UFUNCTION(BlueprintCallable, Category="Casino|UI")
    void CloseInteractionUI(UUserWidget* Widget);
    UFUNCTION(BlueprintCallable, Category="Casino|UI")
    bool ShowScreenUI(UUserWidget* Widget, ECasinoUIScreen ExpectedScreen);
    UFUNCTION(BlueprintCallable, Category="Casino|UI")
    UUserWidget* OpenInteractionUI(TSubclassOf<UUserWidget> WidgetClass);
    UFUNCTION(BlueprintCallable, Category="Casino|UI")
    void ShowWorldEventAnnouncement();
    void ClearWorldEventAnnouncement();
    UPROPERTY(BlueprintReadOnly, Category="Casino|UI") TObjectPtr<UCasinoUIRoot> UIRoot;
    UPROPERTY(BlueprintReadOnly, Category="Casino|UI") TObjectPtr<UUserWidget> ActiveScreenWidget;
    UPROPERTY(BlueprintReadOnly, Category="Casino|UI") ECasinoUIScreen UIScreen = ECasinoUIScreen::Loading;
    bool IsTravelPending() const { return bUITravelPending; }

    void UpdateUI();
    void PrepareTravel();
    void ShutdownUI();
    void EnsureUIRoot();
    UFUNCTION() void RefreshCasinoUIScreen();
    void SetCasinoUIScreen(ECasinoUIScreen Screen);
    void ApplyUIScreenInput();
    void CloseManagedWidget(UUserWidget* Widget);
    void ClearInteractionWidgets();
private:
    void UpdateActiveScreenLoopStatus(const FCasinoLoopStatus& Status);
#if WITH_DEV_AUTOMATION_TESTS
    friend class FCasinoUIScreenStatusTest;
#endif
    Acasino_simulatorPlayerController* GetCasinoController() const;
    void ApplyInteractionInput();
    UPROPERTY() TArray<TObjectPtr<UUserWidget>> ManagedInteractions;
    UPROPERTY() TMap<TSubclassOf<UUserWidget>, TObjectPtr<UUserWidget>> CachedInteractions;
    UPROPERTY() TObjectPtr<ACasinoLoopGameState> UIObservedGameState;
    bool bClosingWidgets = false;
    bool bUIScreenInitialized = false;
    bool bUITravelPending = false;
    TWeakObjectPtr<UWorld> UIObservedWorld;
    TWeakObjectPtr<UWorld> UITravelOrigin;

};
