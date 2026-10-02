#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "casino_loop_gamestate.h"
#include "CasinoManagedWidget.generated.h"
class UCasinoUIManagerComponent;
UCLASS(Abstract, Blueprintable)
class CASINO_SIMULATOR_API UCasinoManagedWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|UI")
    void OnOpened();
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|UI")
    void OnClosed();
    // Presentation refresh only: do not reset betting/payment input here.
    UFUNCTION(BlueprintImplementableEvent, Category="Casino|UI")
    void OnLoopStatusUpdated(const FCasinoLoopStatus& Status);
    UPROPERTY(BlueprintReadOnly, Category="Casino|UI")
    FCasinoLoopStatus LoopStatus;
    UPROPERTY(BlueprintReadOnly, Category="Casino|UI")
    bool bHasLoopStatus = false;
    // Visual close only; release server-owned machines separately.
    UFUNCTION(BlueprintCallable, Category="Casino|UI")
    void RequestClose();
    UFUNCTION(BlueprintPure, Category="Casino|UI")
    UCasinoUIManagerComponent* GetUIManager() const { return UIManager; }
    void NotifyOpened(UCasinoUIManagerComponent* Manager);
    void NotifyClosed();
    void NotifyLoopStatusUpdated(const FCasinoLoopStatus& Status);
private:
    UPROPERTY(Transient) TObjectPtr<UCasinoUIManagerComponent> UIManager;
    bool bManagedOpen = false;
};
