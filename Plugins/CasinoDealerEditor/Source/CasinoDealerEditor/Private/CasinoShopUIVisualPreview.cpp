#include "CasinoShopUIVisualPreview.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/GridPanel.h"
#include "Components/GridSlot.h"
#include "Components/TextBlock.h"
#include "DynamicRHI.h"
#include "Economy/CasinoShopComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
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
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCasinoShopPreview, Log, All);

namespace CasinoShopPreview
{
IConsoleCommand* Command = nullptr;
const TCHAR* MainPath = TEXT("/Game/Store/UI/WBP_StoreMain.WBP_StoreMain_C");
const TCHAR* CardPath = TEXT("/Game/Store/UI/WBP_StoreItemCard.WBP_StoreItemCard_C");
const TCHAR* EntryPath = TEXT("/Game/Store/UI/WBP_StoreItemEntry.WBP_StoreItemEntry_C");

template<typename T>
T* Find(UUserWidget* Widget, const TCHAR* Name)
{
    return Widget && Widget->WidgetTree ? Cast<T>(Widget->WidgetTree->FindWidget(FName(Name))) : nullptr;
}

void ClearRuntimeDelegates(UWidget* Widget)
{
    for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
    {
        if (FMulticastDelegateProperty* Delegate = CastField<FMulticastDelegateProperty>(*It))
            Delegate->ClearDelegate(Widget, Delegate->ContainerPtrToValuePtr<void>(Widget));
        else if (FDelegateProperty* Single = CastField<FDelegateProperty>(*It))
        {
            // Keep the added data-only description binding for an actual
            // runtime source-path check. Every interaction delegate is cleared.
            if (Widget->GetFName() != TEXT("CasinoShopItemDescription") || Single->GetFName() != TEXT("TextDelegate"))
                Single->ContainerPtrToValuePtr<FScriptDelegate>(Widget)->Unbind();
        }
    }
}

void PrepareTree(UUserWidget* Widget)
{
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
    UBlueprintGeneratedClass::UnbindDynamicDelegates(Widget->GetClass(), Widget);
    ClearRuntimeDelegates(Widget);
    if (Widget->WidgetTree) Widget->WidgetTree->ForEachWidget([](UWidget* Child)
    {
        Child->SetDesignerFlags(EWidgetDesignFlags::Designing);
        ClearRuntimeDelegates(Child);
        if (UUserWidget* Nested = Cast<UUserWidget>(Child)) PrepareTree(Nested);
    });
}

UUserWidget* Create(const TCHAR* Path, FString& Error)
{
    UClass* Class = LoadClass<UUserWidget>(nullptr, Path);
    if (!Class) { Error = TEXT("Store preview class missing: ") + FString(Path); return nullptr; }
    UUserWidget* Widget = NewObject<UUserWidget>(GetTransientPackage(), Class, NAME_None, RF_Transient);
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
    if (!Widget->Initialize() || !Widget->WidgetTree || !Widget->WidgetTree->RootWidget)
    { Error = TEXT("Store design-mode instance did not initialize."); return nullptr; }
    PrepareTree(Widget);
    return Widget;
}

UCasinoShopComponent* Catalog(FString& Error)
{
    // Read the real store NPC component template, including its advertised IDs
    // and shared inventory table. No actor, world, BeginPlay or purchase is run.
    UBlueprint* NPC = LoadObject<UBlueprint>(nullptr, TEXT("/Game/1_BluePrint/NPC/BP_NPC_Store.BP_NPC_Store"));
    UBlueprintGeneratedClass* Class = NPC ? Cast<UBlueprintGeneratedClass>(NPC->GeneratedClass) : nullptr;
    if (!NPC || !Class || !NPC->SimpleConstructionScript)
    { Error = TEXT("Real store NPC has no component templates."); return nullptr; }
    for (USCS_Node* Node : NPC->SimpleConstructionScript->GetAllNodes()) if (Node)
        if (UCasinoShopComponent* Shop = Cast<UCasinoShopComponent>(Node->GetActualComponentTemplate(Class)))
            return Shop;
    Error = TEXT("Real store NPC has no CasinoShopComponent template.");
    return nullptr;
}

bool InitializeItem(UUserWidget* Widget, UCasinoShopComponent* Shop, const FCasinoShopItemData& Item, FString& Error)
{
    // The inspected InitializeItem is presentation-only: store ItemData, read
    // the unit price, and update text/icon/tooltip. Never invoke Button_Buy,
    // Construct, ServerBuyShopItem, ProcessPurchase, or any item grant function.
    UFunction* Function = Widget ? Widget->FindFunction(TEXT("InitializeItem")) : nullptr;
    FStructProperty* Data = Function ? FindFProperty<FStructProperty>(Function, TEXT("InItemData")) : nullptr;
    FObjectPropertyBase* Component = Function ? FindFProperty<FObjectPropertyBase>(Function, TEXT("InShopComponent")) : nullptr;
    if (!Function || !Data || Data->Struct != FCasinoShopItemData::StaticStruct() || !Component)
    { Error = TEXT("Store item InitializeItem signature differs."); return false; }
    FStructOnScope Parameters(Function);
    Data->CopyCompleteValue(Data->ContainerPtrToValuePtr<void>(Parameters.GetStructMemory()), &Item);
    Component->SetObjectPropertyValue_InContainer(Parameters.GetStructMemory(), Shop);
    Widget->ProcessEvent(Function, Parameters.GetStructMemory());
    UTextBlock* Name = Find<UTextBlock>(Widget, TEXT("TextBlock_Name"));
    if (!Name || !Name->GetText().EqualTo(Item.DisplayName))
    { Error = TEXT("Real InitializeItem did not update the store item's display name."); return false; }
    if (UTextBlock* Description = Find<UTextBlock>(Widget, TEXT("CasinoShopItemDescription")))
    {
        if (!Description->TextDelegate.IsBound() || !Description->TextDelegate.Execute().EqualTo(Item.Description))
        { Error = TEXT("Store description binding does not resolve the current real ItemData.Description."); return false; }
    }
    return true;
}

bool Populate(UUserWidget* Main, UCasinoShopComponent* Shop, ECasinoShopItemCategory Category, bool Empty, bool LongText, FString& Error)
{
    UGridPanel* Grid = Find<UGridPanel>(Main, TEXT("GridPanel_ItemGrid"));
    FByteProperty* Current = FindFProperty<FByteProperty>(Main->GetClass(), TEXT("CurrentCategory"));
    if (!Grid || !Current) { Error = TEXT("Store preview grid/category contract differs."); return false; }
    Current->SetPropertyValue_InContainer(Main, static_cast<uint8>(Category));
    // Invoke only the inspected color-only routine, preserving the actual
    // selected category colors rather than depicting fabricated runtime states.
    UFunction* UpdateTabs = Main->FindFunction(TEXT("UpdateTabVisuals"));
    if (!UpdateTabs) { Error = TEXT("Store preview UpdateTabVisuals missing."); return false; }
    Main->ProcessEvent(UpdateTabs, nullptr);
    Grid->ClearChildren();
    if (Empty) return true;
    const TArray<FCasinoShopItemData> Items = Shop->GetShopItemsByCategory(Category);
    if (Items.IsEmpty()) { Error = TEXT("Real store catalog category has no sale items."); return false; }
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        UUserWidget* Card = Create(CardPath, Error);
        if (!Card) return false;
        FCasinoShopItemData Item = Items[Index];
        if (LongText && Index == 0)
        {
            Item.DisplayName = FText::FromString(TEXT("긴 아이템 이름도 카드 안에서 표시되는 테스트 상품"));
            Item.Description = FText::FromString(TEXT("아이템 설명이 여러 줄로 길어져도 상품 그림과 가격, 구매 버튼을 가리지 않아야 합니다. 전체 설명은 기존 툴팁을 통해 확인할 수 있습니다."));
        }
        if (!InitializeItem(Card, Shop, Item, Error)) return false;
        // RefreshItems currently uses Index / 2 and Index % 2. Use the same
        // mapping and its explicit padding, without running the live list graph.
        UGridSlot* Slot = Grid->AddChildToGrid(Card, Index / 2, Index % 2);
        if (!Slot) { Error = TEXT("Store preview item could not attach to grid."); return false; }
        Slot->SetPadding(FMargin());
        if (LongText && Index == 1) if (UButton* Buy = Find<UButton>(Card, TEXT("Button_Buy"))) Buy->SetIsEnabled(false);
    }
    return true;
}

bool Render(FWidgetRenderer& Renderer, UUserWidget* Widget, FIntPoint Size, const FString& Filename,
    FString& Error, bool UseDPI = true, bool CheckExterior = true)
{
    const float Scale = UseDPI ? FMath::Max(.01f, GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size)) : 1.f;
    TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(FVector2D(Size), TF_Bilinear, false));
    if (!Target.IsValid()) { Error = TEXT("Store preview render target creation failed."); return false; }
    // Preserve translucent navy and straight-alpha PNG color, matching the
    // existing in-game menu preview pipeline instead of baking a fake game.
    Target->InitCustomFormat(Size.X, Size.Y, PF_FloatRGBA, true); Target->UpdateResourceImmediate(true);
    Renderer.DrawWidget(Target.Get(), Widget->TakeWidget(), Scale, FVector2D(Size), 0.f, false);
    FlushRenderingCommands();
    FImage Raw;
    if (!FImageUtils::GetRenderTargetImage(Target.Get(), Raw))
    { Error = TEXT("Store preview target could not be read."); return false; }
    FImage Linear;
    Raw.CopyTo(Linear, ERawImageFormat::RGBA32F, EGammaSpace::Linear);
    for (FLinearColor& Pixel : Linear.AsRGBA32F())
    {
        if (Pixel.A > UE_SMALL_NUMBER) { Pixel.R /= Pixel.A; Pixel.G /= Pixel.A; Pixel.B /= Pixel.A; }
        else Pixel = FLinearColor::Transparent;
    }
    FImage Png;
    Linear.CopyTo(Png, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    if (CheckExterior)
    {
        const FColor Corner = Png.AsBGRA8()[20 * Size.X + 20];
        int64 Exterior = 0;
        for (const FColor& Pixel : Png.AsBGRA8()) if (Pixel.A <= 64) ++Exterior;
        if (Corner.A > 64 || Exterior < int64(Size.X) * Size.Y / 5)
        { Error = TEXT("Store preview blocks its surrounding game view: ") + Filename; return false; }
        UE_LOG(LogCasinoShopPreview, Display, TEXT("Store alpha %s: corner=%d, game-visible exterior=%.1f%%"),
            *FPaths::GetBaseFilename(Filename), Corner.A, 100.0 * Exterior / (int64(Size.X) * Size.Y));
    }
    if (!FImageUtils::SaveImageByExtension(*Filename, Png))
    { Error = TEXT("Store preview PNG save failed: ") + Filename; return false; }
    UE_LOG(LogCasinoShopPreview, Display, TEXT("Rendered %s; project DPI %.3f"), *Filename, Scale);
    Widget->ReleaseSlateResources(true);
    return true;
}
}

bool RenderCasinoShopPreviews(FString& OutError)
{
    using namespace CasinoShopPreview;
    OutError.Reset();
    if (!IsInGameThread() || IsRunningCommandlet() || !GIsEditor || !FApp::CanEverRender()
        || !FSlateApplication::IsInitialized() || !GDynamicRHI || GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::Null)
    { OutError = TEXT("Store previews require the rendering editor with Slate."); return false; }
    UCasinoShopComponent* Shop = Catalog(OutError);
    if (!Shop) return false;
    const FString Saved = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    const FString Directory = FPaths::ConvertRelativePathToFull(Saved / TEXT("CasinoDealerSetup/ShopPreviews"));
    if (!Directory.StartsWith(Saved, ESearchCase::IgnoreCase) || !IFileManager::Get().MakeDirectory(*Directory, true))
    { OutError = TEXT("Store previews must stay within the project Saved directory."); return false; }
    FWidgetRenderer* Renderer = new FWidgetRenderer(false, true);
    ON_SCOPE_EXIT { FlushRenderingCommands(); BeginCleanup(Renderer); };
    if (!Renderer->GetSlateRenderer()) { OutError = TEXT("Store Slate renderer unavailable."); return false; }
    const FIntPoint Sizes[] = { FIntPoint(1920, 1080), FIntPoint(2560, 1080), FIntPoint(1440, 1080), FIntPoint(1280, 720) };
    const TCHAR* Variants[] = { TEXT("Cigarette"), TEXT("Alcohol"), TEXT("LongText_Disabled"), TEXT("Empty") };
    FString Manifest = TEXT("Actual store WidgetBlueprint design-mode previews. Catalog and advertised sale IDs read from the real NPC component template. Existing InitializeItem and color-only UpdateTabVisuals are the only invoked Blueprint functions; delegates are cleared, except the new description property binding checked against real ItemData. No live Construct, purchase RPC, inventory grant, currency modification or configuration save is executed. Long-text/disabled cases alter transient copies only. PNGs retain alpha so game visibility can be verified.\n");
    int32 Rendered = 0;
    for (const FIntPoint Size : Sizes) for (int32 Variant = 0; Variant < UE_ARRAY_COUNT(Variants); ++Variant)
    {
        TStrongObjectPtr<UUserWidget> Main(Create(MainPath, OutError));
        if (!Main.IsValid() || !Populate(Main.Get(), Shop, Variant == 1 ? ECasinoShopItemCategory::Alcohol : ECasinoShopItemCategory::Cigarette,
            Variant == 3, Variant == 2, OutError)) return false;
        const FString Filename = Directory / FString::Printf(TEXT("%s_%dx%d.png"), Variants[Variant], Size.X, Size.Y);
        if (!Render(*Renderer, Main.Get(), Size, Filename, OutError)) return false;
        ++Rendered; Manifest += Filename + TEXT("\n");
    }
    const TArray<FCasinoShopItemData> Items = Shop->GetShopItems();
    if (Items.IsEmpty()) { OutError = TEXT("Real store catalog is empty."); return false; }
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        TStrongObjectPtr<UUserWidget> Card(Create(CardPath, OutError));
        if (!Card.IsValid() || !InitializeItem(Card.Get(), Shop, Items[Index], OutError)) return false;
        const FString Filename = Directory / FString::Printf(TEXT("Item_%s.png"), *Items[Index].ItemId.ToString());
        if (!Render(*Renderer, Card.Get(), FIntPoint(410, 244), Filename, OutError, false, false)) return false;
        ++Rendered;
        Manifest += FString::Printf(TEXT("%s; actual item=%s; description binding verified\n"), *Filename, *Items[Index].DisplayName.ToString());
    }
    TStrongObjectPtr<UUserWidget> Entry(Create(EntryPath, OutError));
    if (!Entry.IsValid() || !InitializeItem(Entry.Get(), Shop, Items[0], OutError)
        || !Render(*Renderer, Entry.Get(), FIntPoint(940, 92), Directory / TEXT("LegacyEntry.png"), OutError, false, false)) return false;
    ++Rendered;
    if (!FFileHelper::SaveStringToFile(Manifest, *(Directory / TEXT("preview-info.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    { OutError = TEXT("Store preview manifest could not be written."); return false; }
    UE_LOG(LogCasinoShopPreview, Display, TEXT("Completed %d store previews: %s"), Rendered, *Directory);
    return true;
}

void RegisterCasinoShopPreviewCommands()
{
    if (!CasinoShopPreview::Command)
        CasinoShopPreview::Command = IConsoleManager::Get().RegisterConsoleCommand(TEXT("CasinoShop.RenderPreviews"),
            TEXT("Render real store and item widgets in design mode using the real catalog, without purchasing."),
            FConsoleCommandDelegate::CreateLambda([]()
            {
                FString Error;
                if (!RenderCasinoShopPreviews(Error)) UE_LOG(LogCasinoShopPreview, Error, TEXT("Store previews failed: %s"), *Error);
            }), ECVF_Default);
}

void UnregisterCasinoShopPreviewCommands()
{
    if (CasinoShopPreview::Command)
    { IConsoleManager::Get().UnregisterConsoleObject(CasinoShopPreview::Command); CasinoShopPreview::Command = nullptr; }
}
