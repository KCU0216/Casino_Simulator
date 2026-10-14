#pragma once

#include "CoreMinimal.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Styling/SlateBrush.h"
#include "WidgetBlueprint.h"

class UButton;

/** Shared presentation helpers for the existing casino WidgetBlueprints. */
namespace CasinoWidgetStyle
{
inline const FLinearColor Background(.007f, .011f, .019f, 1.f);
inline const FLinearColor Panel(.016f, .024f, .039f, 1.f);
inline const FLinearColor Gold(.76f, .49f, .15f, 1.f);
inline const FLinearColor Ivory(.91f, .86f, .75f, 1.f);
inline const FLinearColor Muted(.39f, .46f, .55f, 1.f);

FSlateBrush Brush(FLinearColor Color, float Radius = 8.f, FLinearColor Outline = FLinearColor::Transparent);
void TextStyle(UTextBlock* Text, float Size = 20.f, FLinearColor Color = Ivory);
void ButtonStyle(UButton* Button, bool Primary = false);
UCanvasPanelSlot* Place(UCanvasPanel* Canvas, UWidget* Widget, const FAnchors& Anchors,
    const FMargin& Offsets = FMargin(), FVector2D Alignment = FVector2D::ZeroVector, int32 Order = 0, bool Auto = false);
void Label(UWidgetBlueprint* Blueprint, UCanvasPanel* Canvas, const TCHAR* Name, const TCHAR* Value,
    FAnchors Anchors, FMargin Offsets, float Size, FLinearColor Color = Ivory);

template<typename T>
T* Find(UWidgetBlueprint* Blueprint, const TCHAR* Name)
{
    return Blueprint && Blueprint->WidgetTree ? Cast<T>(Blueprint->WidgetTree->FindWidget(FName(Name))) : nullptr;
}

template<typename T>
T* Decoration(UWidgetBlueprint* Blueprint, const TCHAR* Name)
{
    if (T* Existing = Find<T>(Blueprint, Name)) return Existing;
    T* Widget = Blueprint->WidgetTree->ConstructWidget<T>(T::StaticClass(), FName(Name));
    Widget->bIsVariable = false;
    Blueprint->OnVariableAdded(Widget->GetFName());
    return Widget;
}
}
