#include "CasinoShopUILayoutMigration.h"

#include "CasinoMenuLayoutMigration.h"
#include "CasinoWidgetStyle.h"
#include "Components/Border.h"
#include "Components/BorderSlot.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/GridPanel.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/SizeBoxSlot.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Economy/CasinoShopComponent.h"
#include "UObject/UnrealType.h"

namespace CasinoShopLayout
{
using namespace CasinoWidgetStyle;

void FillContent(UContentWidget* Container)
{
    if (UWidget* Child = Container->GetContent())
    {
        if (UBorderSlot* Slot = Cast<UBorderSlot>(Child->Slot))
        { Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill); }
        if (USizeBoxSlot* Slot = Cast<USizeBoxSlot>(Child->Slot))
        { Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill); }
        if (UButtonSlot* Slot = Cast<UButtonSlot>(Child->Slot))
        { Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill); }
    }
}

void ItemText(UTextBlock* Block, float Size, FLinearColor Color, bool Wrap = false)
{
    TextStyle(Block, Size, Color);
    Block->SetJustification(ETextJustify::Left);
    Block->SetAutoWrapText(Wrap);
    Block->SetTextOverflowPolicy(ETextOverflowPolicy::Ellipsis);
    Block->SetClipping(EWidgetClipping::ClipToBounds);
}

void ColumnSlot(UWidget* Child, FMargin Padding = FMargin())
{
    if (UVerticalBoxSlot* Slot = Cast<UVerticalBoxSlot>(Child->Slot))
    {
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        Slot->SetPadding(Padding); Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetVerticalAlignment(VAlign_Center);
    }
}

void StretchBox(USizeBox* Box, float Height)
{
    Box->ClearWidthOverride(); Box->ClearMinDesiredWidth(); Box->ClearMaxDesiredWidth();
    Box->SetHeightOverride(Height);
    FillContent(Box);
}

bool Card(UWidgetBlueprint* Blueprint, FString& Error)
{
    USizeBox* Root = Find<USizeBox>(Blueprint, TEXT("SizeBox_Card"));
    UBorder* Frame = Find<UBorder>(Blueprint, TEXT("Border_Card"));
    UHorizontalBox* Row = Find<UHorizontalBox>(Blueprint, TEXT("HorizontalBox_Main"));
    USizeBox* IconBox = Find<USizeBox>(Blueprint, TEXT("SizeBox_Icon"));
    UImage* Icon = Find<UImage>(Blueprint, TEXT("Image_ItemIcon"));
    UVerticalBox* Info = Find<UVerticalBox>(Blueprint, TEXT("VerticalBox_Card"));
    USizeBox* NameBox = Find<USizeBox>(Blueprint, TEXT("SizeBox_Name"));
    USizeBox* EffectBox = Find<USizeBox>(Blueprint, TEXT("SizeBox_Effect"));
    USizeBox* PriceBox = Find<USizeBox>(Blueprint, TEXT("SizeBox_Price"));
    UTextBlock* Name = Find<UTextBlock>(Blueprint, TEXT("TextBlock_Name"));
    UTextBlock* Effect = Find<UTextBlock>(Blueprint, TEXT("TextBlock_Effect"));
    UTextBlock* Price = Find<UTextBlock>(Blueprint, TEXT("TextBlock_Price"));
    UButton* Buy = Find<UButton>(Blueprint, TEXT("Button_Buy"));
    UTextBlock* BuyLabel = Find<UTextBlock>(Blueprint, TEXT("TextBlock_BuyLabel"));
    if (!Root || !Frame || !Row || !IconBox || !Icon || !Info || !NameBox || !EffectBox
        || !PriceBox || !Name || !Effect || !Price || !Buy || !BuyLabel)
    { Error = TEXT("Store item-card layout differs from the inspected version."); return false; }

    // Two columns are populated by the existing RefreshItems graph. A narrower
    // card fixes its former 500px card / 720px grid overflow without changing it.
    Root->SetWidthOverride(410.f); Root->SetHeightOverride(244.f);
    Frame->SetBrush(Brush(Panel, 10.f, Gold * .35f));
    Frame->SetBrushColor(FLinearColor::White); Frame->SetPadding(FMargin(16.f));
    FillContent(Root); FillContent(Frame);
    IconBox->SetWidthOverride(138.f); IconBox->SetHeightOverride(192.f);
    UBorder* IconStage = Decoration<UBorder>(Blueprint, TEXT("CasinoShopIconStage"));
    IconStage->SetBrush(Brush(Background, 8.f)); IconStage->SetPadding(FMargin(9.f));
    UScaleBox* IconFit = Decoration<UScaleBox>(Blueprint, TEXT("CasinoShopIconFit"));
    IconFit->SetStretch(EStretch::ScaleToFit); IconFit->SetStretchDirection(EStretchDirection::Both);
    if (Icon->GetParent() != IconFit) { Icon->RemoveFromParent(); IconFit->SetContent(Icon); }
    if (IconFit->GetParent() != IconStage) { IconFit->RemoveFromParent(); IconStage->SetContent(IconFit); }
    if (IconStage->GetParent() != IconBox) { IconStage->RemoveFromParent(); IconBox->SetContent(IconStage); }
    FillContent(IconBox); FillContent(IconStage);
    if (UScaleBoxSlot* Slot = Cast<UScaleBoxSlot>(Icon->Slot))
    { Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetVerticalAlignment(VAlign_Center); }
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(IconBox->Slot))
    {
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic)); Slot->SetPadding(FMargin());
        Slot->SetVerticalAlignment(VAlign_Center);
    }
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Info->Slot))
    {
        Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); Slot->SetPadding(FMargin(16.f, 0.f, 0.f, 0.f));
        Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Center);
    }
    StretchBox(NameBox, 30.f); StretchBox(EffectBox, 22.f); StretchBox(PriceBox, 30.f);
    ItemText(Name, 20.f, Ivory); ItemText(Effect, 15.f, Gold); ItemText(Price, 22.f, Gold);

    USizeBox* DescriptionBox = Decoration<USizeBox>(Blueprint, TEXT("CasinoShopDescriptionBox"));
    UTextBlock* Description = Decoration<UTextBlock>(Blueprint, TEXT("CasinoShopItemDescription"));
    // The existing InitializeItem already populates Name/Effect/Price/Icon and
    // the full tooltip. Only the added description uses the same real ItemData.
    Description->bIsVariable = true;
    Description->SetVisibility(ESlateVisibility::HitTestInvisible);
    ItemText(Description, 15.f, Muted, true);
    Description->SetText(FText::GetEmpty());
    DescriptionBox->SetHeightOverride(40.f);
    if (Description->GetParent() != DescriptionBox)
    { Description->RemoveFromParent(); DescriptionBox->SetContent(Description); }
    FillContent(DescriptionBox);
    if (DescriptionBox->GetParent() != Info)
    { DescriptionBox->RemoveFromParent(); Info->InsertChildAt(2, DescriptionBox); }
    FStructProperty* ItemProperty = FindFProperty<FStructProperty>(Blueprint->GeneratedClass, TEXT("ItemData"));
    FProperty* DescriptionProperty = FindFProperty<FProperty>(FCasinoShopItemData::StaticStruct(), TEXT("Description"));
    if (!ItemProperty || ItemProperty->Struct != FCasinoShopItemData::StaticStruct() || !DescriptionProperty)
    { Error = TEXT("Store item-card has no matching ItemData.Description property."); return false; }
    FDelegateEditorBinding Binding;
    Binding.ObjectName = Description->GetName(); Binding.PropertyName = TEXT("Text");
    Binding.Kind = EBindingKind::Property;
    Binding.SourcePath.Segments.Add(FEditorPropertyPathSegment(ItemProperty));
    Binding.SourcePath.Segments.Add(FEditorPropertyPathSegment(DescriptionProperty));
    Blueprint->Bindings.RemoveAll([&Binding](const FDelegateEditorBinding& Existing) { return Existing == Binding; });
    Blueprint->Bindings.Add(Binding);
    for (UWidget* Child : Info->GetAllChildren()) ColumnSlot(Child, FMargin(0.f, 0.f, 0.f, 4.f));
    USizeBox* BuyHeight = Decoration<USizeBox>(Blueprint, TEXT("CasinoShopBuyHeight"));
    if (Buy->GetParent() != BuyHeight)
    {
        const int32 Index = Info->GetChildIndex(Buy);
        Buy->RemoveFromParent(); BuyHeight->SetContent(Buy);
        Info->InsertChildAt(Index, BuyHeight);
    }
    BuyHeight->SetHeightOverride(44.f); FillContent(BuyHeight); ColumnSlot(BuyHeight, FMargin(0.f, 4.f, 0.f, 0.f));
    BuyLabel->SetText(FText::FromString(TEXT("구매")));
    ButtonStyle(Buy, true); TextStyle(BuyLabel, 18.f, Background);
    BuyLabel->SetColorAndOpacity(FSlateColor::UseForeground());
    return true;
}

bool Store(UWidgetBlueprint* Blueprint, FString& Error)
{
    UCanvasPanel* Root = Find<UCanvasPanel>(Blueprint, TEXT("CanvasPanel_33"));
    UBorder* Frame = Find<UBorder>(Blueprint, TEXT("Border_StorePanel"));
    UOverlay* Overlay = Find<UOverlay>(Blueprint, TEXT("Overlay_98"));
    UVerticalBox* OldColumn = Find<UVerticalBox>(Blueprint, TEXT("VerticalBox_Main"));
    UHorizontalBox* OldTabs = Find<UHorizontalBox>(Blueprint, TEXT("HorizontalBox_Tabs"));
    UScrollBox* Scroll = Find<UScrollBox>(Blueprint, TEXT("ScrollBox_ItemList"));
    USizeBox* GridArea = Find<USizeBox>(Blueprint, TEXT("SizeBox_GridArea"));
    UGridPanel* Grid = Find<UGridPanel>(Blueprint, TEXT("GridPanel_ItemGrid"));
    UButton* Cigarette = Find<UButton>(Blueprint, TEXT("Button_Cigarette"));
    UButton* Alcohol = Find<UButton>(Blueprint, TEXT("Button_Alcohol"));
    UButton* Close = Find<UButton>(Blueprint, TEXT("Button_Close"));
    if (!Root || !Frame || !Overlay || !OldColumn || !OldTabs || !Scroll || !GridArea || !Grid || !Cigarette || !Alcohol || !Close)
    { Error = TEXT("Store layout differs from the inspected version."); return false; }

    FLinearColor Dim = Background; Dim.A = .18f;
    UBorder* Shade = Decoration<UBorder>(Blueprint, TEXT("CasinoShopBackdrop"));
    Shade->SetBrush(Brush(Dim, 0.f)); Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Root, Shade, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(), FVector2D::ZeroVector, -10);
    FLinearColor FrameColor = Background; FrameColor.A = .94f;
    Frame->SetBrush(Brush(FrameColor, 12.f, Gold * .45f)); Frame->SetBrushColor(FLinearColor::White);
    Frame->SetPadding(FMargin(28.f));
    Place(Root, Frame, FAnchors(.5f, .5f), FMargin(0.f, 0.f, 1250.f, 800.f), FVector2D(.5f, .5f));
    FillContent(Frame);
    UCanvasPanel* Content = Decoration<UCanvasPanel>(Blueprint, TEXT("CasinoShopContent"));
    if (Content->GetParent() != Overlay)
    {
        Content->RemoveFromParent();
        UOverlaySlot* Slot = Overlay->AddChildToOverlay(Content);
        Slot->SetHorizontalAlignment(HAlign_Fill); Slot->SetVerticalAlignment(VAlign_Fill); Slot->SetPadding(FMargin());
    }
    // Retain original empty decorative containers and every widget identity.
    // The category handlers reference the same buttons, labels and inner borders.
    Place(Content, OldColumn, FAnchors(0.f, 0.f), FMargin(0.f, 0.f, 1.f, 1.f));
    Label(Blueprint, Content, TEXT("CasinoShopTitle"), TEXT("CASINO STORE"), FAnchors(0.f, .025f, 1.f, .025f), FMargin(0.f, 0.f, 0.f, 50.f), 36.f, Gold);
    Label(Blueprint, Content, TEXT("CasinoShopSubtitle"), TEXT("상점"), FAnchors(0.f, .095f, .5f, .095f), FMargin(0.f, 0.f, 0.f, 30.f), 18.f, Muted);
    Place(Content, Close, FAnchors(1.f, .025f), FMargin(0.f, 0.f, 120.f, 46.f), FVector2D(1.f, 0.f), 5);
    if (UTextBlock* CloseLabel = Cast<UTextBlock>(Close->GetContent())) CloseLabel->SetText(FText::FromString(TEXT("닫기")));
    ButtonStyle(Close);
    UBorder* Rule = Decoration<UBorder>(Blueprint, TEXT("CasinoShopHeaderRule"));
    Rule->SetBrush(Brush(Gold * .4f, 0.f)); Rule->SetVisibility(ESlateVisibility::HitTestInvisible);
    Place(Content, Rule, FAnchors(0.f, .16f, 1.f, .16f), FMargin(0.f, 0.f, 0.f, 1.f));
    Label(Blueprint, Content, TEXT("CasinoShopCategoryTitle"), TEXT("카테고리"), FAnchors(0.f, .21f, .18f, .21f), FMargin(0.f, 0.f, 0.f, 32.f), 20.f, Ivory);
    UVerticalBox* Categories = Decoration<UVerticalBox>(Blueprint, TEXT("CasinoShopCategories"));
    for (UButton* Button : { Cigarette, Alcohol })
    {
        if (Button->GetParent() != Categories) { Button->RemoveFromParent(); Categories->AddChildToVerticalBox(Button); }
        ColumnSlot(Button, FMargin(0.f, 0.f, 0.f, 14.f)); ButtonStyle(Button);
        Button->SetBackgroundColor(FLinearColor::White);
        // UpdateTabVisuals keeps the original active-state colors at runtime.
        // A transparent outer layer lets our rounded button chrome carry focus.
        if (UBorder* Outer = Cast<UBorder>(Button->GetContent()))
        {
            Outer->SetBrush(Brush(FLinearColor::Transparent, 6.f)); Outer->SetBrushColor(FLinearColor::White);
            Outer->SetPadding(FMargin(0.f)); FillContent(Outer);
            if (UBorder* Inner = Cast<UBorder>(Outer->GetContent()))
            {
                Inner->SetBrush(Brush(FLinearColor::White, 6.f)); Inner->SetPadding(FMargin(10.f, 4.f));
                Inner->SetHorizontalAlignment(HAlign_Left); Inner->SetVerticalAlignment(VAlign_Center);
                if (UTextBlock* Caption = Cast<UTextBlock>(Inner->GetContent()))
                {
                    Caption->SetText(FText::FromString(Button == Cigarette ? TEXT("담배") : TEXT("주류")));
                    TextStyle(Caption, 20.f, Ivory); Caption->SetJustification(ETextJustify::Left);
                }
            }
        }
        FillContent(Button);
    }
    Place(Content, Categories, FAnchors(0.f, .28f, .18f, .58f));
    Label(Blueprint, Content, TEXT("CasinoShopSideHint"), TEXT("구매한 아이템은\n인벤토리에 보관됩니다"), FAnchors(0.f, .67f, .18f, .67f), FMargin(0.f, 0.f, 0.f, 90.f), 16.f, Muted);
    Find<UTextBlock>(Blueprint, TEXT("CasinoShopSideHint"))->SetAutoWrapText(true);
    Label(Blueprint, Content, TEXT("CasinoShopItemsTitle"), TEXT("판매 아이템"), FAnchors(.22f, .21f, 1.f, .21f), FMargin(0.f, 0.f, 0.f, 32.f), 22.f, Ivory);
    Place(Content, Scroll, FAnchors(.22f, .28f, 1.f, .90f));
    Scroll->SetScrollbarThickness(FVector2D(6.f, 6.f)); Scroll->SetScrollbarPadding(FMargin(8.f, 0.f, 0.f, 0.f));
    Scroll->SetClipping(EWidgetClipping::ClipToBounds);
    GridArea->ClearWidthOverride(); GridArea->ClearHeightOverride();
    FillContent(GridArea);
    if (UScrollBoxSlot* Slot = Cast<UScrollBoxSlot>(GridArea->Slot))
    { Slot->SetHorizontalAlignment(HAlign_Center); Slot->SetPadding(FMargin()); }
    Grid->SetColumnFill(0, 1.f); Grid->SetColumnFill(1, 1.f);
    Label(Blueprint, Content, TEXT("CasinoShopFooterHint"), TEXT("아이템을 선택하고 구매 버튼을 누르세요"), FAnchors(.22f, .94f, 1.f, .94f), FMargin(0.f, 0.f, 0.f, 32.f), 16.f, Muted);
    for (const TCHAR* ImageName : { TEXT("Image_StoreLogo"), TEXT("Image_CornerCards"), TEXT("Image_CornerDice") })
    {
        UImage* Art = Find<UImage>(Blueprint, ImageName);
        if (!Art) { Error = TEXT("Store decoration missing: ") + FString(ImageName); return false; }
        // Keep existing artist assets without obscuring item details or controls.
        const bool Logo = Art->GetName() == TEXT("Image_StoreLogo");
        const bool Cards = Art->GetName() == TEXT("Image_CornerCards");
        Art->SetRenderOpacity(Logo ? .12f : .18f);
        Place(Content, Art, FAnchors(Logo ? .78f : 0.f, Logo ? .005f : Cards ? .78f : .89f),
            FMargin(0.f, 0.f, Logo ? 150.f : 84.f, Logo ? 75.f : 84.f), FVector2D::ZeroVector, -1);
    }
    return true;
}

bool LegacyEntry(UWidgetBlueprint* Blueprint, FString& Error)
{
    USizeBox* Root = Find<USizeBox>(Blueprint, TEXT("SizeBox_613"));
    UBorder* Frame = Find<UBorder>(Blueprint, TEXT("Border_40"));
    UHorizontalBox* Row = Find<UHorizontalBox>(Blueprint, TEXT("HorizontalBox_34"));
    UButton* Buy = Find<UButton>(Blueprint, TEXT("Button_Buy"));
    if (!Root || !Frame || !Row || !Buy)
    { Error = TEXT("Store legacy entry differs from the inspected version."); return false; }
    Root->SetWidthOverride(940.f); Root->SetHeightOverride(92.f);
    Frame->SetBrush(Brush(Panel, 8.f, Gold * .25f)); Frame->SetBrushColor(FLinearColor::White);
    Frame->SetPadding(FMargin(18.f, 12.f)); FillContent(Root); FillContent(Frame);
    const TCHAR* Names[] = { TEXT("TextBlock_Name"), TEXT("TextBlock_Effect"), TEXT("TextBlock_Description"), TEXT("TextBlock_Price") };
    const float Weights[] = { 1.5f, .9f, 2.f, 1.f };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
    {
        UTextBlock* Block = Find<UTextBlock>(Blueprint, Names[Index]);
        if (!Block) { Error = TEXT("Store legacy text missing: ") + FString(Names[Index]); return false; }
        ItemText(Block, Index == 2 ? 15.f : Index == 0 ? 20.f : 18.f, Index == 1 || Index == 3 ? Gold : Index == 2 ? Muted : Ivory, Index == 2);
        if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Block->Slot))
        {
            FSlateChildSize Size(ESlateSizeRule::Fill); Size.Value = Weights[Index]; Slot->SetSize(Size);
            Slot->SetPadding(FMargin(0.f, 0.f, 16.f, 0.f)); Slot->SetVerticalAlignment(VAlign_Center);
        }
    }
    ButtonStyle(Buy, true);
    if (UTextBlock* Label = Cast<UTextBlock>(Buy->GetContent())) TextStyle(Label, 18.f, Background);
    if (UHorizontalBoxSlot* Slot = Cast<UHorizontalBoxSlot>(Buy->Slot))
    { Slot->SetPadding(FMargin()); Slot->SetVerticalAlignment(VAlign_Center); }
    return true;
}
}

bool MigrateCasinoShopLayouts(FString& OutError)
{
    using namespace CasinoShopLayout;
    const TSet<FName> DescriptionBinding = { FName(TEXT("CasinoShopItemDescription")) };
    return MigrateCasinoPresentationWidget(TEXT("/Game/Store/UI/WBP_StoreItemCard.WBP_StoreItemCard"), Card, OutError, DescriptionBinding)
        && MigrateCasinoPresentationWidget(TEXT("/Game/Store/UI/WBP_StoreItemEntry.WBP_StoreItemEntry"), LegacyEntry, OutError)
        && MigrateCasinoPresentationWidget(TEXT("/Game/Store/UI/WBP_StoreMain.WBP_StoreMain"), Store, OutError);
}
