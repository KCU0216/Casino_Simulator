#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "RaceTypes.h"
#include "RaceBillboardWidget.generated.h"

class ARaceManager;

// Presentation only: the Blueprint owns layout, capture selection, animation and timers.
UCLASS(Abstract, Blueprintable)
class CASINO_SIMULATOR_API URaceBillboardWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category="Race|Billboard")
    TObjectPtr<ARaceManager> RaceManager;
    UPROPERTY(BlueprintReadOnly, Category="Race|Billboard")
    ERacePhase CurrentPhase = ERacePhase::Idle;

    // Delivered once on initialization, then once per observed phase change.
    UFUNCTION(BlueprintImplementableEvent, Category="Race|Billboard")
    void OnRacePhaseChanged(ERacePhase NewPhase);
    // Replicated runner references or betting/winner data can arrive after Phase.
    // Check references with IsValid and refresh the displayed data here too.
    UFUNCTION(BlueprintImplementableEvent, Category="Race|Billboard")
    void OnRaceDataUpdated();

    void RefreshFromManager(ARaceManager* Manager);
private:
    bool bPhaseInitialized = false;
};