#include "CasinoTradeUILayoutMigration.h"

#include "CasinoMenuLayoutMigration.h"
#include "CasinoWidgetStyle.h"
#include "Blueprint/UserWidget.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "HAL/FileManager.h"
#include "Misc/DateTime.h"
#include "Misc/Paths.h"

namespace CasinoTradeLayout
{
const TCHAR* AssetPath = TEXT("/Game/1_BluePrint/Widget/WBP_Trade.WBP_Trade");

void FillOverlay(UOverlay* Panel, UWidget* Widget)
{
    if (Widget->GetParent() != Panel)
    {
        Widget->RemoveFromParent();
        Panel->AddChildToOverlay(Widget);
    }
    if (UOverlaySlot* Slot = Cast<UOverlaySlot>(Widget->Slot))
    {
        Slot->SetPadding(FMargin());
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetVerticalAlignment(VAlign_Fill);
    }
}

void LabelStyle(UTextBlock* Label, float Size, const FLinearColor& Color)
{
    CasinoWidgetStyle::TextStyle(Label, Size, Color);
    Label->SetJustification(ETextJustify::Left);
    Label->SetAutoWrapText(false);
}

bool ButtonLabel(UWidgetBlueprint* Blueprint, UButton* Button, const TCHAR* Name,
    const TCHAR* Text, bool Primary, FString& Error)
{
    UWidget* Previous = Button->GetContent();
    UTextBlock* Label = CasinoWidgetStyle::Decoration<UTextBlock>(Blueprint, Name);
    if (Previous && Previous != Label)
    {
        Error = TEXT("Trade button has unexpected content: ") + Button->GetName();
        return false;
    }
    Label->SetText(FText::FromString(Text));
    Label->SetVisibility(ESlateVisibility::HitTestInvisible);
    if (Label->GetParent() != Button)
    {
        Label->RemoveFromParent();
        Button->SetContent(Label);
    }
    CasinoWidgetStyle::ButtonStyle(Button, Primary);
    Button->SetBackgroundColor(FLinearColor::White);
    Button->SetColorAndOpacity(FLinearColor::White);
    if (UButtonSlot* Slot = Cast<UButtonSlot>(Label->Slot))
    {
        Slot->SetHorizontalAlignment(HAlign_Center);
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetPadding(FMargin());
    }
    return true;
}

bool ApplyLayout(UWidgetBlueprint* Blueprint, FString& Error)
{
    UCanvasPanel* Root = CasinoWidgetStyle::Find<UCanvasPanel>(Blueprint, TEXT("CanvasPanel_41"));
    UOverlay* Panel = CasinoWidgetStyle::Find<UOverlay>(Blueprint, TEXT("Overlay_222"));
    UImage* Frame = CasinoWidgetStyle::Find<UImage>(Blueprint, TEXT("Image_0"));
    UTextBlock* Title = CasinoWidgetStyle::Find<UTextBlock>(Blueprint, TEXT("TextBlock_52"));
    UTextBlock* AmountLabel = CasinoWidgetStyle::Find<UTextBlock>(Blueprint, TEXT("TextBlock_265"));
    UTextBlock* Amount = CasinoWidgetStyle::Find<UTextBlock>(Blueprint, TEXT("Text_Value"));
    UButton* Input = CasinoWidgetStyle::Find<UButton>(Blueprint, TEXT("Button_Input"));
    UButton* Cancel = CasinoWidgetStyle::Find<UButton>(Blueprint, TEXT("Button_Exit"));
    UButton* Send = CasinoWidgetStyle::Find<UButton>(Blueprint, TEXT("Button_Send"));
    UUserWidget* Keypad = CasinoWidgetStyle::Find<UUserWidget>(Blueprint, TEXT("WBP_Calculate"));
    if (!Root || Blueprint->WidgetTree->RootWidget != Root || !Panel || !Frame || !Title
        || !AmountLabel || !Amount || !Input || !Cancel || !Send || !Keypad
        || Panel->GetParent() != Root || Keypad->GetParent() != Root)
    {
        Error = TEXT("Trade panel differs from the inspected widget contract.");
        return false;
    }
    if (Input->GetContent() && Input->GetContent() != Amount)
    {
        Error = TEXT("Trade amount button has unexpected content.");
        return false;
    }

    UBorder* Shade = CasinoWidgetStyle::Decoration<UBorder>(Blueprint, TEXT("CasinoTradeShade"));
    Shade->SetBrush(CasinoWidgetStyle::Brush(FLinearColor(.007f, .011f, .019f, .12f), 0.f));
    Shade->SetBrushColor(FLinearColor::White);
    Shade->SetVisibility(ESlateVisibility::HitTestInvisible);
    CasinoWidgetStyle::Place(Root, Shade, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(), FVector2D::ZeroVector, -10);

    // The form and its separate keypad occupy a shared 1160px-wide stage.
    CasinoWidgetStyle::Place(Root, Panel, FAnchors(.5f, .5f), FMargin(-220.f, 0.f, 720.f, 540.f),
        FVector2D(.5f, .5f), 10);
    Frame->SetBrush(CasinoWidgetStyle::Brush(FLinearColor(.016f, .024f, .039f, .94f), 14.f,
        FLinearColor(.76f, .49f, .15f, .65f)));
    Frame->SetColorAndOpacity(FLinearColor::White);
    FillOverlay(Panel, Frame);

    UCanvasPanel* Content = CasinoWidgetStyle::Decoration<UCanvasPanel>(Blueprint, TEXT("CasinoTradeContent"));
    Content->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    FillOverlay(Panel, Content);
    CasinoWidgetStyle::Label(Blueprint, Content, TEXT("CasinoTradeEyebrow"), TEXT("PLAYER TRANSFER"),
        FAnchors(), FMargin(32.f, 30.f, 500.f, 22.f), 14.f, CasinoWidgetStyle::Gold);
    Title->SetText(FText::FromString(TEXT("플레이어 거래")));
    LabelStyle(Title, 32.f, CasinoWidgetStyle::Ivory);
    CasinoWidgetStyle::Place(Content, Title, FAnchors(), FMargin(32.f, 62.f, 656.f, 52.f));
    CasinoWidgetStyle::Label(Blueprint, Content, TEXT("CasinoTradeDescription"), TEXT("보낼 금액을 입력하고 상대에게 보내세요."),
        FAnchors(), FMargin(32.f, 120.f, 656.f, 28.f), 17.f, CasinoWidgetStyle::Muted);

    UBorder* Rule = CasinoWidgetStyle::Decoration<UBorder>(Blueprint, TEXT("CasinoTradeHeaderRule"));
    Rule->SetBrush(CasinoWidgetStyle::Brush(FLinearColor(.76f, .49f, .15f, .30f), 0.f));
    Rule->SetBrushColor(FLinearColor::White);
    Rule->SetVisibility(ESlateVisibility::HitTestInvisible);
    CasinoWidgetStyle::Place(Content, Rule, FAnchors(), FMargin(32.f, 164.f, 656.f, 1.f));

    AmountLabel->SetText(FText::FromString(TEXT("보낼 금액")));
    LabelStyle(AmountLabel, 19.f, CasinoWidgetStyle::Ivory);
    CasinoWidgetStyle::Place(Content, AmountLabel, FAnchors(), FMargin(32.f, 188.f, 656.f, 30.f));
    CasinoWidgetStyle::Place(Content, Input, FAnchors(), FMargin(32.f, 230.f, 656.f, 94.f), FVector2D::ZeroVector, 10);
    CasinoWidgetStyle::ButtonStyle(Input);
    Input->SetBackgroundColor(FLinearColor::White);
    Input->SetColorAndOpacity(FLinearColor::White);
    // Keeping the original text as button content preserves its saved visibility
    // and lets the entire amount field route pointer clicks to Button_Input.
    if (Amount->GetParent() != Input)
    {
        Amount->RemoveFromParent();
        Input->SetContent(Amount);
    }
    Amount->SetText(FText::FromString(TEXT("0")));
    LabelStyle(Amount, 40.f, CasinoWidgetStyle::Gold);
    Amount->SetJustification(ETextJustify::Right);
    if (UButtonSlot* Slot = Cast<UButtonSlot>(Amount->Slot))
    {
        Slot->SetHorizontalAlignment(HAlign_Fill);
        Slot->SetVerticalAlignment(VAlign_Center);
        Slot->SetPadding(FMargin(8.f, 0.f));
    }
    CasinoWidgetStyle::Label(Blueprint, Content, TEXT("CasinoTradeKeypadHint"), TEXT("금액을 누르면 숫자 키패드가 열립니다."),
        FAnchors(), FMargin(32.f, 342.f, 656.f, 26.f), 16.f, CasinoWidgetStyle::Muted);
    CasinoWidgetStyle::Label(Blueprint, Content, TEXT("CasinoTradeConfirmHint"), TEXT("숫자키로 입력 · Enter로 입력 완료"),
        FAnchors(), FMargin(32.f, 376.f, 656.f, 26.f), 16.f, CasinoWidgetStyle::Muted);

    if (!ButtonLabel(Blueprint, Cancel, TEXT("CasinoTradeCancelLabel"), TEXT("취소"), false, Error)
        || !ButtonLabel(Blueprint, Send, TEXT("CasinoTradeSendLabel"), TEXT("보내기"), true, Error)) return false;
    CasinoWidgetStyle::Place(Content, Cancel, FAnchors(), FMargin(32.f, 448.f, 228.f, 60.f));
    CasinoWidgetStyle::Place(Content, Send, FAnchors(), FMargin(280.f, 448.f, 408.f, 60.f));

    // Preserve the original root-level shared keypad and all its event wiring.
    CasinoWidgetStyle::Place(Root, Keypad, FAnchors(0.f, 0.f, 1.f, 1.f), FMargin(), FVector2D(.5f, .5f), 100);
    return true;
}

bool Backup(FString& Error)
{
    const FString Source = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("1_BluePrint/Widget/WBP_Trade.uasset"));
    const FString Stamp = FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")) + TEXT("_")
        + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
    const FString Directory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("CasinoDealerSetup/TradeBackup"), Stamp);
    IFileManager& Files = IFileManager::Get();
    if (!Files.FileExists(*Source) || !Files.MakeDirectory(*Directory, true))
    {
        Error = TEXT("Could not prepare the trade UI backup.");
        return false;
    }
    const FString Destination = FPaths::Combine(Directory, TEXT("WBP_Trade.uasset"));
    if (Files.Copy(*Destination, *Source, false) != COPY_OK)
    {
        Error = TEXT("Could not back up the trade UI: ") + Destination;
        return false;
    }
    UE_LOG(LogTemp, Display, TEXT("Trade UI backup: %s"), *Destination);
    return true;
}
}

bool MigrateCasinoTradeLayout(FString& OutError)
{
    OutError.Reset();
    if (!CasinoTradeLayout::Backup(OutError)) return false;
    return MigrateCasinoPresentationWidget(CasinoTradeLayout::AssetPath, CasinoTradeLayout::ApplyLayout, OutError);
}
