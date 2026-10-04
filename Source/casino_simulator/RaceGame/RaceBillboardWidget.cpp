#include "RaceBillboardWidget.h"
#include "RaceManager.h"

void URaceBillboardWidget::RefreshFromManager(ARaceManager* Manager)
{
    if (!IsValid(Manager)) return;
    if (RaceManager != Manager)
    {
        RaceManager = Manager;
        bPhaseInitialized = false;
    }
    if (!bPhaseInitialized || CurrentPhase != Manager->Phase)
    {
        bPhaseInitialized = true;
        CurrentPhase = Manager->Phase;
        OnRacePhaseChanged(CurrentPhase);
        OnRaceDataUpdated();
    }
}