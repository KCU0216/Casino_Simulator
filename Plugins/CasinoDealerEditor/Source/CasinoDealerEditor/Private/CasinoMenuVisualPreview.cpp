#include "CasinoMenuVisualPreview.h"

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/WidgetSwitcher.h"
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
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UI/CasinoRoomDialogWidget.h"

DEFINE_LOG_CATEGORY_STATIC(LogCasinoMenuPreview, Log, All);

namespace
{
IConsoleCommand* PreviewCommand = nullptr;
IConsoleCommand* SettingsPreviewCommand = nullptr;
const TCHAR* MainClassPath = TEXT("/Game/Blackjack/Level/WBP_MainMenu.WBP_MainMenu_C");
const TCHAR* LobbyClassPath = TEXT("/Game/Blackjack/Level/WBP_LobbyUI.WBP_LobbyUI_C");
const TCHAR* PlayerClassPath = TEXT("/Game/Blackjack/Level/WBP_LobbyPlayerEntry.WBP_LobbyPlayerEntry_C");
const TCHAR* RoomClassPath = TEXT("/Game/Blackjack/Level/WBP_RoomEntry.WBP_RoomEntry_C");
const TCHAR* SettingsClassPath = TEXT("/Game/KCU/WBP_Settings.WBP_Settings_C");

void ClearPreviewDelegates(UWidget* Widget)
{
    for (TFieldIterator<FProperty> It(Widget->GetClass()); It; ++It)
    {
        if (FMulticastDelegateProperty* Delegate = CastField<FMulticastDelegateProperty>(*It))
            Delegate->ClearDelegate(Widget, Delegate->ContainerPtrToValuePtr<void>(Widget));
        else if (FDelegateProperty* SingleDelegate = CastField<FDelegateProperty>(*It))
            SingleDelegate->ContainerPtrToValuePtr<FScriptDelegate>(Widget)->Unbind();
    }
    // These controls broadcast when sample values/options are assigned.
    if (UComboBoxString* Combo = Cast<UComboBoxString>(Widget))
    { Combo->OnSelectionChanged.Clear(); Combo->OnOpening.Clear(); }
    if (USlider* Slider = Cast<USlider>(Widget)) Slider->OnValueChanged.Clear();
    if (UCheckBox* Check = Cast<UCheckBox>(Widget)) Check->OnCheckStateChanged.Clear();
}

void PreparePreviewTree(UUserWidget* Widget)
{
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
    UBlueprintGeneratedClass::UnbindDynamicDelegates(Widget->GetClass(), Widget);
    ClearPreviewDelegates(Widget);
    if (!Widget->WidgetTree) return;
    Widget->WidgetTree->ForEachWidget([](UWidget* Child)
    {
        Child->SetDesignerFlags(EWidgetDesignFlags::Designing);
        ClearPreviewDelegates(Child);
        // WidgetTree iteration stops at a nested UserWidget's own tree.
        if (UUserWidget* Nested = Cast<UUserWidget>(Child)) PreparePreviewTree(Nested);
    });
}

UUserWidget* CreatePreview(const TCHAR* ClassPath, FString& Error)
{
    UClass* Class = LoadClass<UUserWidget>(nullptr, ClassPath);
    if (!Class) { Error = TEXT("Could not load menu preview class: ") + FString(ClassPath); return nullptr; }
    UUserWidget* Widget = NewObject<UUserWidget>(GetTransientPackage(), Class, NAME_None, RF_Transient);
    Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
    if (!Widget->Initialize() || !Widget->WidgetTree || !Widget->WidgetTree->RootWidget)
    { Error = TEXT("Could not initialize designer menu preview: ") + FString(ClassPath); return nullptr; }
    // Programmatic USlider.SetValue broadcasts in UE5.7. Unbind the transient
    // instance's Blueprint event handlers before injecting visual sample data.
    PreparePreviewTree(Widget);
    return Widget;
}

template <typename T>
T* Find(UUserWidget* Root, const TCHAR* Name)
{
    return Root && Root->WidgetTree ? Cast<T>(Root->WidgetTree->FindWidget(FName(Name))) : nullptr;
}

bool Text(UUserWidget* Root, const TCHAR* Name, const FString& Value, FString& Error)
{
    UTextBlock* Block = Find<UTextBlock>(Root, Name);
    if (!Block) { Error = TEXT("Preview widget contract missing text: ") + FString(Name); return false; }
    Block->SetText(FText::FromString(Value));
    return true;
}

bool SelectMenuPage(UUserWidget* Menu, bool Login, FString& Error)
{
    UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(Menu, TEXT("WidgetSwitcher_1"));
    UWidget* Page = Find<UWidget>(Menu, Login ? TEXT("LoginPanel") : TEXT("MainMenuPanel"));
    if (!Switcher || !Page) { Error = TEXT("Main-menu preview has no expected switcher/page."); return false; }
    Switcher->SetActiveWidget(Page);
    return true;
}

bool AddRoomSamples(UUserWidget* Menu, FString& Error)
{
    UVerticalBox* List = Find<UVerticalBox>(Menu, TEXT("RoomListBox"));
    if (!List) { Error = TEXT("Main-menu preview has no RoomListBox."); return false; }
    List->ClearChildren();
    const TCHAR* Names[] = { TEXT("친구들과 함께하는 방"), TEXT("CASINO NIGHT"), TEXT("긴 방 이름도 레이아웃 안에서 표시되는 테스트 방") };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
    {
        UUserWidget* Entry = CreatePreview(RoomClassPath, Error);
        if (!Entry || !Text(Entry, TEXT("RoomNameText"), Names[Index], Error)
            || !Text(Entry, TEXT("PlayerCountText"), FString::Printf(TEXT("%d / 4"), Index + 1), Error)) return false;
        FCasinoRoomInfo SampleRoom;
        SampleRoom.bPasswordProtected = Index == 1;
        UCasinoRoomDialogWidget::RefreshRoomPasswordIndicator(Entry, SampleRoom);
        if (!List->AddChildToVerticalBox(Entry)) { Error = TEXT("Could not attach room-entry preview."); return false; }
    }
    return true;
}

bool AddPlayerSamples(UUserWidget* Lobby, FString& Error)
{
    UVerticalBox* List = Find<UVerticalBox>(Lobby, TEXT("PlayerListBox"));
    if (!List) { Error = TEXT("Lobby preview has no PlayerListBox."); return false; }
    List->ClearChildren();
    const TCHAR* Names[] = { TEXT("플레이어 1 (나)"), TEXT("Player Two"), TEXT("긴 플레이어 이름 표시 테스트"), TEXT("플레이어 4") };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
    {
        UUserWidget* Entry = CreatePreview(PlayerClassPath, Error);
        if (!Entry || !Text(Entry, TEXT("PlayerNameText"), Names[Index], Error)
            || !Text(Entry, TEXT("ReadyStateText"), Index == 3 ? TEXT("대기") : TEXT("준비"), Error)) return false;
        USlider* Volume = Find<USlider>(Entry, TEXT("PlayerVolumeSlider"));
        UCheckBox* Mute = Find<UCheckBox>(Entry, TEXT("PlayerMuteCheck"));
        if (!Volume || !Mute) { Error = TEXT("Player entry preview is missing voice controls."); return false; }
        Volume->SetValue(Volume->GetMinValue() + (Volume->GetMaxValue() - Volume->GetMinValue()) * (Index == 2 ? .35f : .65f));
        Mute->SetIsChecked(Index == 2);
        if (!List->AddChildToVerticalBox(Entry)) { Error = TEXT("Could not attach player-entry preview."); return false; }
    }
    if (!Text(Lobby, TEXT("Ready_Text"), TEXT("게임 시작"), Error)) return false;
    if (UButton* Ready = Find<UButton>(Lobby, TEXT("ReadyButton"))) Ready->SetIsEnabled(false);
    if (USlider* Output = Find<USlider>(Lobby, TEXT("VoiceOutputSlider"))) Output->SetValue(.75f);
    if (UCheckBox* Enabled = Find<UCheckBox>(Lobby, TEXT("VoiceEnabledCheck"))) Enabled->SetIsChecked(true);
    if (UCheckBox* PTT = Find<UCheckBox>(Lobby, TEXT("PushToTalkCheck"))) PTT->SetIsChecked(true);
    return true;
}

bool SelectSettingsPage(UUserWidget* Settings, int32 Index, FString& Error)
{
    const TCHAR* Pages[] = { TEXT("SettingsMenuPanel"), TEXT("GraphicPanel"), TEXT("AudioPanel"), TEXT("KeySettingsPanel") };
    UWidgetSwitcher* Switcher = Find<UWidgetSwitcher>(Settings, TEXT("SettingsSwitcher"));
    UWidget* Page = Index >= 0 && Index < UE_ARRAY_COUNT(Pages) ? Find<UWidget>(Settings, Pages[Index]) : nullptr;
    if (!Switcher || !Page || Switcher->GetChildAt(Index) != Page)
    { Error = TEXT("Settings preview switcher/page contract is missing or reordered."); return false; }
    Switcher->SetActiveWidgetIndex(Index);
    return true;
}

bool AddSettingsSamples(UUserWidget* Settings, FString& Error)
{
    const TCHAR* Names[] = { TEXT("Combo_WindowMode"), TEXT("Combo_Resolution"), TEXT("Combo_Quality"), TEXT("Combo_FrameRate") };
    const TArray<FString> Options[] = {
        { TEXT("창 모드"), TEXT("테두리 없는 창"), TEXT("전체 화면") },
        { TEXT("1280 x 720"), TEXT("1920 x 1080"), TEXT("2560 x 1440") },
        { TEXT("낮음"), TEXT("중간"), TEXT("높음"), TEXT("매우 높음") },
        { TEXT("30fps"), TEXT("60fps"), TEXT("120fps"), TEXT("제한 없음") }
    };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
    {
        UComboBoxString* Combo = Find<UComboBoxString>(Settings, Names[Index]);
        if (!Combo) { Error = TEXT("Settings preview is missing combo: ") + FString(Names[Index]); return false; }
        Combo->OnSelectionChanged.Clear();
        Combo->OnOpening.Clear();
        // Designer asset defaults remain intact. Only an empty transient list
        // receives sample resolutions/options normally populated at runtime.
        if (Combo->GetOptionCount() == 0)
            for (const FString& Option : Options[Index]) Combo->AddOption(Option);
        if (Combo->GetSelectedOption().IsEmpty())
            Combo->SetSelectedIndex(FMath::Min(Index == 2 ? 2 : 1, Combo->GetOptionCount() - 1));
    }
    USlider* Volume = Find<USlider>(Settings, TEXT("Slider_MasterVolume"));
    USlider* Sensitivity = Find<USlider>(Settings, TEXT("Slider_MouseSensitivity"));
    if (!Volume || !Sensitivity) { Error = TEXT("Settings preview is missing its volume/sensitivity sliders."); return false; }
    Volume->OnValueChanged.Clear(); Sensitivity->OnValueChanged.Clear();
    Volume->SetValue(FMath::Clamp(.75f, Volume->GetMinValue(), Volume->GetMaxValue()));
    Sensitivity->SetValue(FMath::Clamp(1.f, Sensitivity->GetMinValue(), Sensitivity->GetMaxValue()));
    return true;
}

bool RenderOne(FWidgetRenderer& Renderer, UUserWidget* Widget, const FIntPoint& Size,
    const FString& Filename, FString& Error, bool bCheckMenuPalette = true)
{
    const float Scale = FMath::Max(.01f, GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size));
    const FVector2D DrawSize(Size.X, Size.Y);
    // Render linear colors into a linear target. Convert the readback explicitly
    // to sRGB below: UE's PNG compressor preserves linear BGRA8 bytes as-is.
    TStrongObjectPtr<UTextureRenderTarget2D> Target(FWidgetRenderer::CreateTargetFor(DrawSize, TF_Bilinear, false));
    if (!Target.IsValid()) { Error = TEXT("Could not create menu preview render target."); return false; }
    Renderer.DrawWidget(Target.Get(), Widget->TakeWidget(), Scale, DrawSize, 0.f, false);
    FlushRenderingCommands();
    FImage Image;
    if (!FImageUtils::GetRenderTargetImage(Target.Get(), Image))
    { Error = TEXT("Could not read menu preview render target: ") + Filename; return false; }
    FImage PngImage;
    Image.CopyTo(PngImage, ERawImageFormat::BGRA8, EGammaSpace::sRGB);
    const FColor BackgroundPixel = PngImage.AsBGRA8()[20 * Size.X + 20];
    const FColor ExpectedBackground = FLinearColor(.007f, .011f, .019f, 1.f).ToFColorSRGB();
    const auto Matches = [](FColor Actual, FColor Expected)
    {
        // The linear render target is BGRA8; its quantization can shift a very
        // dark color by a few display-space levels during the sRGB conversion.
        return FMath::Abs(int32(Actual.R) - int32(Expected.R)) <= 4
            && FMath::Abs(int32(Actual.G) - int32(Expected.G)) <= 4
            && FMath::Abs(int32(Actual.B) - int32(Expected.B)) <= 4;
    };
    if (bCheckMenuPalette && !Matches(BackgroundPixel, ExpectedBackground))
    { Error = TEXT("Preview background does not match the menu's sRGB palette: ") + Filename; return false; }
    if (bCheckMenuPalette && FPaths::GetBaseFilename(Filename).StartsWith(TEXT("Login_")))
    {
        const FColor GoldPixel = PngImage.AsBGRA8()[(Size.Y / 2 + 10) * Size.X + Size.X / 2];
        const FColor ExpectedGold = FLinearColor(.76f, .49f, .15f, 1.f).ToFColorSRGB();
        if (!Matches(GoldPixel, ExpectedGold))
        { Error = TEXT("Preview login button does not match the menu's sRGB gold: ") + Filename; return false; }
        UE_LOG(LogCasinoMenuPreview, Display, TEXT("Palette check: background %s / expected %s; gold %s / expected %s"),
            *BackgroundPixel.ToString(), *ExpectedBackground.ToString(), *GoldPixel.ToString(), *ExpectedGold.ToString());
    }
    if (!FImageUtils::SaveImageByExtension(*Filename, PngImage))
    { Error = TEXT("Could not save menu preview: ") + Filename; return false; }
    UE_LOG(LogCasinoMenuPreview, Display, TEXT("Rendered %s (%d x %d, project DPI %.3f)"), *Filename, Size.X, Size.Y, Scale);
    Widget->ReleaseSlateResources(true);
    return true;
}
}

bool RenderCasinoMenuPreviews(FString& OutError)
{
    OutError.Reset();
    if (!IsInGameThread() || IsRunningCommandlet() || !GIsEditor || !FApp::CanEverRender()
        || !FSlateApplication::IsInitialized() || !GDynamicRHI || GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::Null)
    { OutError = TEXT("Menu previews require the running editor with Slate and a rendering RHI."); return false; }
    const FString SavedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    const FString Directory = FPaths::ConvertRelativePathToFull(SavedRoot / TEXT("CasinoDealerSetup/MenuPreviews"));
    if (!Directory.StartsWith(SavedRoot, ESearchCase::IgnoreCase) || !IFileManager::Get().MakeDirectory(*Directory, true))
    { OutError = TEXT("Menu preview output must stay within the project Saved directory."); return false; }
    FWidgetRenderer* Renderer = new FWidgetRenderer(false, true);
    ON_SCOPE_EXIT { FlushRenderingCommands(); BeginCleanup(Renderer); };
    if (!Renderer->GetSlateRenderer()) { OutError = TEXT("Slate widget renderer is unavailable."); return false; }
    const FIntPoint Sizes[] = { FIntPoint(1920, 1080), FIntPoint(2560, 1080), FIntPoint(1440, 1080) };
    const TCHAR* Variants[] = { TEXT("Login"), TEXT("MainMenu_Empty"), TEXT("MainMenu_Rooms"), TEXT("Lobby") };
    FString Manifest = TEXT("Visual previews use real WidgetBlueprints in design mode. Room/player names are transient sample data. No session, login, or voice events are invoked.\n");
    for (const FIntPoint& Size : Sizes)
    {
        const float Scale = FMath::Max(.01f, GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(Size));
        Manifest += FString::Printf(TEXT("Pixels %dx%d, DPI %.4f, logical %.2fx%.2f\n"), Size.X, Size.Y, Scale, Size.X / Scale, Size.Y / Scale);
        for (int32 Variant = 0; Variant < UE_ARRAY_COUNT(Variants); ++Variant)
        {
            TStrongObjectPtr<UUserWidget> Widget(CreatePreview(Variant == 3 ? LobbyClassPath : MainClassPath, OutError));
            if (!Widget.IsValid()) return false;
            if (Variant < 3 && !SelectMenuPage(Widget.Get(), Variant == 0, OutError)) return false;
            if (Variant == 2 && !AddRoomSamples(Widget.Get(), OutError)) return false;
            if (Variant == 3 && !AddPlayerSamples(Widget.Get(), OutError)) return false;
            const FString Filename = Directory / FString::Printf(TEXT("%s_%dx%d.png"), Variants[Variant], Size.X, Size.Y);
            if (!RenderOne(*Renderer, Widget.Get(), Size, Filename, OutError)) return false;
        }
        for (int32 Variant = 0; Variant < 3; ++Variant)
        {
            TStrongObjectPtr<UCasinoRoomDialogWidget> Dialog(NewObject<UCasinoRoomDialogWidget>(GetTransientPackage(), NAME_None, RF_Transient));
            Dialog->SetDesignerFlags(EWidgetDesignFlags::Designing);
            if (!Dialog->Initialize()) { OutError = TEXT("Could not initialize native room dialog preview."); return false; }
            FCasinoRoomInfo Room;
            Room.RoomName = TEXT("친구들과 함께하는 비공개 방");
            Room.SessionId = TEXT("transient-preview-room");
            Room.bPasswordProtected = true;
            Dialog->PrepareDesignerPreview(Variant == 0, Room);
            if (UEditableTextBox* Name = Find<UEditableTextBox>(Dialog.Get(), TEXT("RoomNameInput")))
                Name->SetText(FText::FromString(TEXT("오늘의 카지노")));
            if (UEditableTextBox* Password = Find<UEditableTextBox>(Dialog.Get(), TEXT("PasswordInput")))
                Password->SetText(FText::FromString(TEXT("preview123")));
            if (Variant == 2 && !Text(Dialog.Get(), TEXT("ErrorText"), TEXT("비밀번호가 맞지 않습니다. 다시 입력하세요."), OutError)) return false;
            const TCHAR* Name = Variant == 0 ? TEXT("CreateRoom") : Variant == 1 ? TEXT("JoinPassword") : TEXT("JoinPassword_Error");
            const FString Filename = Directory / FString::Printf(TEXT("%s_%dx%d.png"), Name, Size.X, Size.Y);
            if (!RenderOne(*Renderer, Dialog.Get(), Size, Filename, OutError, false)) return false;
        }
    }
    if (!FFileHelper::SaveStringToFile(Manifest, *(Directory / TEXT("preview-info.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    { OutError = TEXT("Could not write menu preview manifest."); return false; }
    UE_LOG(LogCasinoMenuPreview, Display, TEXT("Completed 21 menu and room-dialog previews: %s"), *Directory);
    return true;
}

bool RenderCasinoSettingsPreviews(FString& OutError)
{
    OutError.Reset();
    if (!IsInGameThread() || IsRunningCommandlet() || !GIsEditor || !FApp::CanEverRender()
        || !FSlateApplication::IsInitialized() || !GDynamicRHI || GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::Null)
    { OutError = TEXT("Settings previews require the running editor with Slate and a rendering RHI."); return false; }
    const FString SavedRoot = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir());
    const FString Directory = FPaths::ConvertRelativePathToFull(SavedRoot / TEXT("CasinoDealerSetup/SettingsPreviews"));
    if (!Directory.StartsWith(SavedRoot, ESearchCase::IgnoreCase) || !IFileManager::Get().MakeDirectory(*Directory, true))
    { OutError = TEXT("Settings preview output must stay within the project Saved directory."); return false; }
    FWidgetRenderer* Renderer = new FWidgetRenderer(false, true);
    ON_SCOPE_EXIT { FlushRenderingCommands(); BeginCleanup(Renderer); };
    if (!Renderer->GetSlateRenderer()) { OutError = TEXT("Slate widget renderer is unavailable."); return false; }
    const FIntPoint Sizes[] = { FIntPoint(1920, 1080), FIntPoint(2560, 1080), FIntPoint(1440, 1080), FIntPoint(1280, 720) };
    const TCHAR* Names[] = { TEXT("SettingsMenu"), TEXT("Graphics"), TEXT("Audio"), TEXT("Controls") };
    FString Manifest = TEXT("Actual WBP_Settings design-mode previews, including the real nested MainMenu instance. All dynamic delegates and nested widgets are disabled for runtime events before injecting transient selections. No Construct, settings apply/reset, subsystem, login, or config-save calls are made.\n");
    int32 Rendered = 0;
    for (const FIntPoint& Size : Sizes)
    {
        for (int32 Page = 0; Page < UE_ARRAY_COUNT(Names); ++Page)
        {
            TStrongObjectPtr<UUserWidget> Settings(CreatePreview(SettingsClassPath, OutError));
            if (!Settings.IsValid() || !AddSettingsSamples(Settings.Get(), OutError) || !SelectSettingsPage(Settings.Get(), Page, OutError)) return false;
            const bool bStyled = Find<UWidget>(Settings.Get(), TEXT("CasinoThemeBackground")) != nullptr;
            const FString Filename = Directory / FString::Printf(TEXT("%s_%dx%d.png"), Names[Page], Size.X, Size.Y);
            if (!RenderOne(*Renderer, Settings.Get(), Size, Filename, OutError, bStyled)) return false;
            ++Rendered;
            Manifest += FString::Printf(TEXT("%s themed=%d\n"), *Filename, bStyled);
        }
        // Use the actual serialized child already present in the main menu,
        // including its switcher slot, to expose embedding/Fill layout defects.
        TStrongObjectPtr<UUserWidget> Menu(CreatePreview(MainClassPath, OutError));
        UUserWidget* Settings = Menu.IsValid() ? Find<UUserWidget>(Menu.Get(), TEXT("WBP_Settings")) : nullptr;
        UWidgetSwitcher* MainSwitcher = Menu.IsValid() ? Find<UWidgetSwitcher>(Menu.Get(), TEXT("WidgetSwitcher_1")) : nullptr;
        if (!Settings || !MainSwitcher)
        { OutError = TEXT("Main-menu preview has no embedded WBP_Settings/switcher."); return false; }
        PreparePreviewTree(Settings);
        if (!AddSettingsSamples(Settings, OutError) || !SelectSettingsPage(Settings, 1, OutError)) return false;
        Settings->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        MainSwitcher->SetActiveWidget(Settings);
        if (MainSwitcher->GetActiveWidget() != Settings)
        { OutError = TEXT("Embedded settings is not a direct main-menu switcher page."); return false; }
        const bool bStyled = Find<UWidget>(Settings, TEXT("CasinoThemeBackground")) != nullptr;
        const FString Filename = Directory / FString::Printf(TEXT("MainMenu_Settings_Graphics_%dx%d.png"), Size.X, Size.Y);
        if (!RenderOne(*Renderer, Menu.Get(), Size, Filename, OutError, bStyled)) return false;
        ++Rendered;
        Manifest += FString::Printf(TEXT("%s embedded themed=%d\n"), *Filename, bStyled);
    }
    if (!FFileHelper::SaveStringToFile(Manifest, *(Directory / TEXT("preview-info.txt")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    { OutError = TEXT("Could not write settings preview manifest."); return false; }
    UE_LOG(LogCasinoMenuPreview, Display, TEXT("Completed %d settings previews: %s"), Rendered, *Directory);
    return true;
}

void RegisterCasinoMenuPreviewCommands()
{
    if (!PreviewCommand)
    {
        PreviewCommand = IConsoleManager::Get().RegisterConsoleCommand(TEXT("CasinoMenu.RenderPreviews"),
            TEXT("Render menu, lobby and room-dialog design previews into Saved/CasinoDealerSetup/MenuPreviews."),
            FConsoleCommandDelegate::CreateLambda([]()
            {
                FString Error;
                if (!RenderCasinoMenuPreviews(Error)) UE_LOG(LogCasinoMenuPreview, Error, TEXT("Menu previews failed: %s"), *Error);
            }), ECVF_Default);
    }
    if (!SettingsPreviewCommand)
    {
        SettingsPreviewCommand = IConsoleManager::Get().RegisterConsoleCommand(TEXT("CasinoSettings.RenderPreviews"),
            TEXT("Render four settings pages and embedded main-menu settings in design mode into Saved/CasinoDealerSetup/SettingsPreviews."),
            FConsoleCommandDelegate::CreateLambda([]()
            {
                FString Error;
                if (!RenderCasinoSettingsPreviews(Error)) UE_LOG(LogCasinoMenuPreview, Error, TEXT("Settings previews failed: %s"), *Error);
            }), ECVF_Default);
    }
}

void UnregisterCasinoMenuPreviewCommands()
{
    if (PreviewCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(PreviewCommand, false);
        PreviewCommand = nullptr;
    }
    if (SettingsPreviewCommand)
    {
        IConsoleManager::Get().UnregisterConsoleObject(SettingsPreviewCommand, false);
        SettingsPreviewCommand = nullptr;
    }
}
