#include "UI/CasinoManagedWidget.h"
#include "UI/CasinoUIManagerComponent.h"
void UCasinoManagedWidget::NotifyOpened(UCasinoUIManagerComponent* Manager)
{
    if (bManagedOpen) return;
    UIManager = Manager;
    bManagedOpen = true;
    OnOpened();
}
void UCasinoManagedWidget::NotifyClosed()
{
    if (!bManagedOpen) return;
    bManagedOpen = false;
    OnClosed();
}
void UCasinoManagedWidget::RequestClose()
{
    if (UIManager) UIManager->CloseInteractionUI(this);
}
