#include "UI/CasinoManagedWidget.h"
#include "UI/CasinoUIManagerComponent.h"
void UCasinoManagedWidget::NotifyOpened(UCasinoUIManagerComponent* Manager)
{
    if (bManagedOpen) return;
    UIManager = Manager;
    bManagedOpen = true;
    bHasLoopStatus = false;
    OnOpened();
}
void UCasinoManagedWidget::NotifyClosed()
{
    if (!bManagedOpen) return;
    bManagedOpen = false;
    bHasLoopStatus = false;
    OnClosed();
}
void UCasinoManagedWidget::NotifyLoopStatusUpdated(const FCasinoLoopStatus& Status)
{
    if (!bManagedOpen) return;
    LoopStatus = Status;
    bHasLoopStatus = true;
    OnLoopStatusUpdated(Status);
}
void UCasinoManagedWidget::RequestClose()
{
    if (UIManager) UIManager->CloseInteractionUI(this);
}
