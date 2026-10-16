#include "CasinoTradeUIVisualPreview.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "DynamicRHI.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/UserInterfaceSettings.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ImageUtils.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "RenderDeferredCleanup.h"
#include "RenderingThread.h"
#include "Slate/WidgetRenderer.h"
#include "UI/CasinoKeypadStyle.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCasinoTradePreview, Log, All);

namespace CasinoTradePreview
{
IConsoleCommand* Command = nullptr;
const TCHAR* TradeClassPath = TEXT("/Game/1_BluePrint/Widget/WBP_Trade.WBP_Trade_C");

template<typename T>
T* Find(UUserWidget* Widget, const TCHAR* Name)
{
    return Widget && Widget->WidgetTree ? Cast<T>(Widget->WidgetTree->FindWidget(FName(Name))) : nullptr;
}

void ClearDelegates(UWidget* Widget)
{
    for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
    {
        if (FMulticastDelegateProperty* Delegate = CastField<FMulticastDelegateProperty>(*It))
            Delegate->ClearDelegate(Widget, Delegate->ContainerPtrToValuePtr<void>(Widget));
        else if (FDelegateProperty* Single = CastField<FDelegateProperty>(*It))
            Single->ContainerPtrToValuePtr<FScriptDelegate>(Widget)->Unbind();
    }
}

void PrepareTree(UUserWidget* Widget)
{
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
    UBlueprintGeneratedClass::UnbindDynamicDelegates(Widget->GetClass(), Widget);
    ClearDelegates(Widget);
    ApplyCasinoTradeKeypadLayout(Widget);
    if (Widget->WidgetTree) Widget->WidgetTree->ForEachWidget([](UWidget* Child)
    {
        Child->SetDesignerFlags(EWidgetDesignFlags::Designing);
        ClearDelegates(Child);
        if (UUserWidget* Nested = Cast<UUserWidget>(Child)) PrepareTree(Nested);
    });
}

UUserWidget* Create(FString& Error)
{
    UClass* Class = LoadClass<UUserWidget>(nullptr, TradeClassPath);
    if (!Class) { Error = TEXT("Trade preview class missing."); return nullptr; }
    UUserWidget* Widget = NewObject<UUserWidget>(GetTransientPackage(), Class, NAME_None, RF_Transient);
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
    if (!Widget->Initialize() || !Widget->WidgetTree || !Widget->WidgetTree->RootWidget)
    { Error = TEXT("Trade design-mode instance did not initialize."); return nullptr; }
    // No world or player is created. Design flags suppress live Construct;
    // delegates are removed before TakeWidget or any visual sample assignment.
    PrepareTree(Widget);
    return Widget;
}

FSlateRect Bounds(UWidget* Widget)
{
    if (!Widget) return FSlateRect();
    const FGeometry Geometry = Widget->GetPaintSpaceGeometry();
    const FVector2D Minimum = Geometry.LocalToAbsolute(FVector2D::ZeroVector);
    const FVector2D Maximum = Geometry.LocalToAbsolute(Geometry.GetLocalSize());
    return FSlateRect(Minimum.X, Minimum.Y, Maximum.X, Maximum.Y);
}

bool Contains(const FSlateRect& Outer, const FSlateRect& Inner)
{
    constexpr float Tolerance = 2.f;
    return Inner.Right > Inner.Left && Inner.Bottom > Inner.Top
        && Inner.Left >= Outer.Left - Tolerance && Inner.Top >= Outer.Top - Tolerance
        && Inner.Right <= Outer.Right + Tolerance && Inner.Bottom <= Outer.Bottom + Tolerance;
}

bool Overlaps(const FSlateRect& A, const FSlateRect& B)
{
    return A.Left < B.Right - 1.f && A.Right > B.Left + 1.f
        && A.Top < B.Bottom - 1.f && A.Bottom > B.Top + 1.f;
}

bool CheckGeometry(UUserWidget* Widget, bool KeypadVisible, FString& Error)
{
    UWidget* Panel = Find<UWidget>(Widget, TEXT("Overlay_222"));
    UButton* Input = Find<UButton>(Widget, TEXT("Button_Input"));
    UTextBlock* Value = Find<UTextBlock>(Widget, TEXT("Text_Value"));
    UButton* Send = Find<UButton>(Widget, TEXT("Button_Send"));
    UButton* Exit = Find<UButton>(Widget, TEXT("Button_Exit"));
    UUserWidget* Keypad = Find<UUserWidget>(Widget, TEXT("WBP_Calculate"));
    UWidget* Grid = Find<UWidget>(Keypad, TEXT("GridPanel_35"));
    if (!Panel || !Input || !Value || !Send || !Exit || !Keypad || !Grid)
    { Error = TEXT("Trade geometry contract is missing a panel, control or keypad."); return false; }
    const FSlateRect Viewport = Bounds(Widget->WidgetTree->RootWidget);
    const FSlateRect PanelBounds = Bounds(Panel);
    if (!Contains(Viewport, PanelBounds))
    { Error = TEXT("Trade panel is outside the viewport."); return false; }
    for (UWidget* Control : { static_cast<UWidget*>(Input), static_cast<UWidget*>(Value),
        static_cast<UWidget*>(Send), static_cast<UWidget*>(Exit) })
    {
        if (!Contains(PanelBounds, Bounds(Control)))
        { Error = TEXT("Trade control is outside its panel: ") + Control->GetName(); return false; }
    }
    // The value text is intentionally the input button's content. All other
    // interactive regions, and the keypad beside the panel, must be separate.
    if (!Contains(Bounds(Input), Bounds(Value)))
    { Error = TEXT("Trade amount text overflows its input button."); return false; }
    if (Overlaps(Bounds(Input), Bounds(Send)) || Overlaps(Bounds(Input), Bounds(Exit))
        || Overlaps(Bounds(Send), Bounds(Exit)))
    { Error = TEXT("Trade input, send or close controls overlap."); return false; }
    if (KeypadVisible && (!Contains(Viewport, Bounds(Grid)) || Overlaps(PanelBounds, Bounds(Grid))))
    { Error = TEXT("Trade keypad is offscreen or overlaps the transfer panel."); return false; }
    return true;
}

bool Sample(UUserWidget* Widget, int32 Variant, FString& Error)
{
    UUserWidget* Keypad = Find<UUserWidget>(Widget, TEXT("WBP_Calculate"));
    UTextBlock* Amount = Find<UTextBlock>(Widget, TEXT("Text_Value"));
    FIntProperty* Input = FindFProperty<FIntProperty>(Widget->GetClass(), TEXT("InputValue"));
    if (!Keypad || !Amount || !Input)
    { Error = TEXT("Trade preview sample contract differs."); return false; }
    const int32 Values[] = { 0, 125000, MAX_int32 };
    const TCHAR* Labels[] = { TEXT("0"), TEXT("125,000"), TEXT("2,147,483,647") };
    Input->SetPropertyValue_InContainer(Widget, Values[Variant]);
    Amount->SetText(FText::FromString(Labels[Variant]));
    Keypad->SetVisibility(Variant == 0 ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
    return true;
}

bool Render(FWidgetRenderer& Renderer, UUserWidget* Widget, FIntPoint Size, bool KeypadVisible,
    const FString& Filename, FString& Error)
{
    const float Scale = FMath::Max(.01f, GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size));
    TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, false));
    if (!Target.IsValid()) { Error = TEXT("Trade preview render target creation failed."); return false; }
    Target->InitCustomFormat(Size.X, Size.Y, PF_FloatRGBA, true);
    Target->UpdateResourceImmediate(true);
    TSharedRef<SWidget> SlateRoot = Widget->TakeWidget();
    // GetVisibility() reads the designer's Slate override once TakeWidget has
    // run, so use the requested sample state directly instead of reading it back.
    UUserWidget* Keypad = Find<UUserWidget>(Widget, TEXT("WBP_Calculate"));
    if (!Keypad || !Keypad->GetCachedWidget().IsValid())
    { Error = TEXT("Trade preview did not build its embedded keypad."); return false; }
    Keypad->GetCachedWidget()->SetVisibility(KeypadVisible ? EVisibility::SelfHitTestInvisible : EVisibility::Collapsed);
    Renderer.DrawWidget(Target.Get(), SlateRoot, Scale, FVector2D(Size), 0.f, false);
    FlushRenderingCommands();
    if (!CheckGeometry(Widget, KeypadVisible, Error))
    { Error += TEXT(" [") + FPaths::GetBaseFilename(Filename) + TEXT("]"); return false; }
    FImage Raw;
    if (!FImageUtils::GetRenderTargetImage(Target.Get(), Raw))
    { Error = TEXT("Trade preview target could not be read."); return false; }
    FImage Linear;
    Raw.CopyTo(Linear, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
    for (FLinearColor& Pixel : Linear.AsRGBA32F())
    {
        if (Pixel.A > UE_SMALL_NUMBER) { Pixel.R /= Pixel.A; Pixel.G /= Pixel.A; Pixel.B /= Pixel.A; }
        else Pixel = FLinearColor::Transparent;
    }
    FImage Png;
    Linear.CopyTo(Png, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    const FColor Corner = Png.AsBGRA8()[20 * Size.X + 20];
    int64 Exterior = 0;
    for (const FColor& Pixel : Png.AsBGRA8()) if (Pixel.A <= 64) ++Exterior;
    if (Corner.A > 64 || Exterior < int64(Size.X) * Size.Y / 5)
    { Error = TEXT("Trade preview blocks its surrounding game view: ") + Filename; return false; }
    if (!FImageUtils::SaveImageByExtension(*Filename, Png))
    { Error = TEXT("Trade preview PNG save failed: ") + Filename; return false; }
    UE_LOG(LogCasinoTradePreview, Display,
        TEXT("Rendered %s; DPI %.3f; corner alpha=%d; game-visible exterior=%.1f%%; controls fit, keypad separate."),
        *Filename, Scale, Corner.A, 100.0 * Exterior / (int64(Size.X) * Size.Y));
    Widget->ReleaseSlateResources(true);
    return true;
}
}

bool RenderCasinoTradePreviews(FString& OutError)
{
    using namespace CasinoTradePreview;
    OutError.Reset();
    if (!IsInGameThread() || IsRunningCommandlet() || !GIsEditor || !FApp::CanEverRender()
        || !FSlateApplication::IsInitialized() || !GDynamicRHI || GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::Null)
    { OutError = TEXT("Trade previews require the rendering editor with Slate."); return false; }
    const FString Saved = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    const FString Directory = FPaths::ConvertRelativePathToFull(Saved / TEXT("CasinoDealerSetup/TradePreviews"));
    if (!Directory.StartsWith(Saved, ESearchCase::IgnoreCase) || !IFileManager::Get().MakeDirectory(*Directory, true))
    { OutError = TEXT("Trade previews must stay within the project Saved directory."); return false; }
    FWidgetRenderer* Renderer = new FWidgetRenderer(false, true);
    ON_SCOPE_EXIT { FlushRenderingCommands(); BeginCleanup(Renderer); };
    if (!Renderer->GetSlateRenderer()) { OutError = TEXT("Trade Slate renderer unavailable."); return false; }
    const FIntPoint Sizes[] = { FIntPoint(1920, 1080), FIntPoint(1280, 720), FIntPoint(1440, 1080), FIntPoint(2560, 1080) };
    const TCHAR* Variants[] = { TEXT("Default"), TEXT("Keypad"), TEXT("LargeAmount") };
    FString Manifest = TEXT("Actual WBP_Trade GeneratedClass design-mode previews. All interaction and property delegates are cleared before TakeWidget; no world, player, live Construct, transfer, RPC or authored asset save is executed. Amount text and keypad visibility alter transient copies only. Shared keypad styling and trade-specific placement use the same native helpers as the live widget. PNGs preserve straight alpha. Each preview checks game-visible exterior, viewport containment, amount containment, distinct input/send/close bounds, and a visible keypad separate from the main panel.\n");
    int32 Rendered = 0;
    for (const FIntPoint Size : Sizes) for (int32 Variant = 0; Variant < UE_ARRAY_COUNT(Variants); ++Variant)
    {
        TStrongObjectPtr<UUserWidget> Widget(Create(OutError));
        if (!Widget.IsValid() || !Sample(Widget.Get(), Variant, OutError)) return false;
        const FString Filename = Directory / FString::Printf(TEXT("%s_%dx%d.png"), Variants[Variant], Size.X, Size.Y);
        if (!Render(*Renderer, Widget.Get(), Size, Variant != 0, Filename, OutError)) return false;
        ++Rendered;
        Manifest += Filename + TEXT("\n");
    }
    if (!FFileHelper::SaveStringToFile(Manifest, *(Directory / TEXT("preview-info.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    { OutError = TEXT("Trade preview manifest could not be written."); return false; }
    UE_LOG(LogCasinoTradePreview, Display, TEXT("Completed %d trade previews: %s"), Rendered, *Directory);
    return true;
}

void RegisterCasinoTradePreviewCommands()
{
    if (!CasinoTradePreview::Command)
        CasinoTradePreview::Command = IConsoleManager::Get().RegisterConsoleCommand(TEXT("CasinoTrade.RenderPreviews"),
            TEXT("Render real trade and embedded keypad widgets in design mode without sending money."),
            FConsoleCommandDelegate::CreateLambda([]()
            {
                FString Error;
                if (!RenderCasinoTradePreviews(Error)) UE_LOG(LogCasinoTradePreview, Error, TEXT("Trade previews failed: %s"), *Error);
            }), ECVF_Default);
}

void UnregisterCasinoTradePreviewCommands()
{
    if (CasinoTradePreview::Command)
    { IConsoleManager::Get().UnregisterConsoleObject(CasinoTradePreview::Command); CasinoTradePreview::Command = nullptr; }
}
