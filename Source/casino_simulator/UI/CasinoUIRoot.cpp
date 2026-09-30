#include "UI/CasinoUIRoot.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"

void UCasinoUIRoot::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (!WidgetTree->RootWidget)
    {
        UOverlay* Root = WidgetTree->ConstructWidget<UOverlay>();
        WidgetTree->RootWidget = Root;
        HUDLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("HUDLayer"));
        InteractionLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("InteractionLayer"));
        ModalLayer = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("ModalLayer"));
        for (UOverlay* Layer : {HUDLayer.Get(), InteractionLayer.Get(), ModalLayer.Get()})
        {
            Layer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
            UOverlaySlot* LayerSlot = Root->AddChildToOverlay(Layer);
            LayerSlot->SetHorizontalAlignment(HAlign_Fill);
            LayerSlot->SetVerticalAlignment(VAlign_Fill);
        }
        Root->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    ensureMsgf(HUDLayer && InteractionLayer && ModalLayer, TEXT("UI root needs HUDLayer, InteractionLayer and ModalLayer overlays."));
}

void UCasinoUIRoot::AddToLayer(UOverlay* Layer, UUserWidget* Widget)
{
    if (!Layer || !Widget) return;
    if (Widget->GetParent() == Layer) return;
    Widget->RemoveFromParent();
    UOverlaySlot* LayerSlot = Layer->AddChildToOverlay(Widget);
    LayerSlot->SetHorizontalAlignment(HAlign_Fill);
    LayerSlot->SetVerticalAlignment(VAlign_Fill);
}
void UCasinoUIRoot::AddHUD(UUserWidget* Widget) { AddToLayer(HUDLayer, Widget); }
void UCasinoUIRoot::AddInteraction(UUserWidget* Widget) { AddToLayer(InteractionLayer, Widget); }
void UCasinoUIRoot::AddModal(UUserWidget* Widget) { AddToLayer(ModalLayer, Widget); }
void UCasinoUIRoot::SetHUDVisible(bool bVisible)
{
    if (HUDLayer) HUDLayer->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
