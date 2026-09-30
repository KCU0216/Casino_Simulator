#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CasinoUIRoot.generated.h"

class UOverlay;

UENUM(BlueprintType)
enum class ECasinoUIScreen : uint8
{
    MainMenu, Lobby, Loading, DayIntro, Playing, Payment, DayPassed, Result
};

// Native layout works without a designer asset. Optional BP subclasses must supply all three overlays.
UCLASS()
class CASINO_SIMULATOR_API UCasinoUIRoot : public UUserWidget
{
    GENERATED_BODY()
public:
    void AddHUD(UUserWidget* Widget);
    void AddInteraction(UUserWidget* Widget);
    void AddModal(UUserWidget* Widget);
    void SetHUDVisible(bool bVisible);
protected:
    virtual void NativeOnInitialized() override;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UOverlay> HUDLayer;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UOverlay> InteractionLayer;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UOverlay> ModalLayer;
private:
    void AddToLayer(UOverlay* Layer, UUserWidget* Widget);
};
