#pragma once
#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
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
    // Visual close only; release server-owned machines separately.
    UFUNCTION(BlueprintCallable, Category="Casino|UI")
    void RequestClose();
    UFUNCTION(BlueprintPure, Category="Casino|UI")
    UCasinoUIManagerComponent* GetUIManager() const { return UIManager; }
    void NotifyOpened(UCasinoUIManagerComponent* Manager);
    void NotifyClosed();
private:
    UPROPERTY(Transient) TObjectPtr<UCasinoUIManagerComponent> UIManager;
    bool bManagedOpen = false;
};
