#include "UI/CasinoRoomDialogWidget.h"
#include "UI/CasinoUIRoot.h"
#include "casino_simulatorPlayerController.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ButtonSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"

namespace
{
TMap<TWeakObjectPtr<APlayerController>, TWeakObjectPtr<UCasinoRoomDialogWidget>> OpenDialogs;
const FLinearColor Background(.007f, .011f, .019f, 1.f);
const FLinearColor Panel(.016f, .024f, .039f, 1.f);
const FLinearColor Gold(.76f, .49f, .15f, 1.f);
const FLinearColor Ivory(.91f, .86f, .75f, 1.f);

bool LocalOwner(APlayerController* Controller)
{
    return IsValid(Controller) && Controller->IsLocalController() && Controller->GetLocalPlayer() && Controller->GetGameInstance();
}

FSlateBrush Brush(FLinearColor Color, float Radius = 8.f, FLinearColor Outline = FLinearColor::Transparent)
{
    FSlateBrush Value;
    Value.DrawAs = ESlateBrushDrawType::RoundedBox;
    Value.TintColor = FSlateColor(Color);
    Value.OutlineSettings = FSlateBrushOutlineSettings(Radius, FSlateColor(Outline), 1.f);
    return Value;
}

void Font(UTextBlock* Text, float Size, FLinearColor Color)
{
    FSlateFontInfo Value = Text->GetFont();
    Value.Size = Size;
    Text->SetFont(Value);
    Text->SetColorAndOpacity(FSlateColor(Color));
    Text->SetVisibility(ESlateVisibility::HitTestInvisible);
}

template <typename T> T* Control(UWidgetTree* Tree, const TCHAR* Name)
{
    return Tree->ConstructWidget<T>(T::StaticClass(), FName(Name));
}

void Add(UVerticalBox* Box, UWidget* Widget, float Bottom = 12.f)
{
    UVerticalBoxSlot* Slot = Box->AddChildToVerticalBox(Widget);
    Slot->SetPadding(FMargin(0.f, 0.f, 0.f, Bottom));
    Slot->SetHorizontalAlignment(HAlign_Fill);
}
}

UCasinoRoomDialogWidget::UCasinoRoomDialogWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::Visible);
}

void UCasinoRoomDialogWidget::FindControls()
{
    if (!WidgetTree) return;
#define CASINO_FIND(Type, Name) Name = Cast<Type>(WidgetTree->FindWidget(TEXT(#Name)))
    CASINO_FIND(UTextBlock, TitleText); CASINO_FIND(UTextBlock, BodyText);
    CASINO_FIND(UTextBlock, RoomNameLabel); CASINO_FIND(UEditableTextBox, RoomNameInput);
    CASINO_FIND(UTextBlock, PasswordLabel); CASINO_FIND(UEditableTextBox, PasswordInput);
    CASINO_FIND(UTextBlock, ErrorText); CASINO_FIND(UButton, SubmitButton);
    CASINO_FIND(UButton, CancelButton); CASINO_FIND(UTextBlock, SubmitCaption);
#undef CASINO_FIND
}

void UCasinoRoomDialogWidget::EnsureLayout()
{
    if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("WidgetTree"), RF_Transient);
    FindControls();
    if (WidgetTree->RootWidget && RoomNameInput && PasswordInput && ErrorText && SubmitButton && CancelButton)
    { ApplyMode(); return; }
    // An incomplete BP layout may already own objects with the required names.
    // Use a fresh transient tree so fallback controls retain those exact names.
    if (WidgetTree->RootWidget) WidgetTree = NewObject<UWidgetTree>(this, NAME_None, RF_Transient);
    UOverlay* Root = Control<UOverlay>(WidgetTree, TEXT("RoomDialogRoot"));
    WidgetTree->RootWidget = Root;
    UBorder* Back = Control<UBorder>(WidgetTree, TEXT("RoomDialogBackdrop"));
    Back->SetBrush(Brush(FLinearColor(0.f, 0.f, 0.f, .72f), 0.f));
    Back->SetVisibility(ESlateVisibility::Visible);
    UOverlaySlot* BackSlot = Root->AddChildToOverlay(Back);
    BackSlot->SetHorizontalAlignment(HAlign_Fill);
    BackSlot->SetVerticalAlignment(VAlign_Fill);
    USizeBox* Size = Control<USizeBox>(WidgetTree, TEXT("RoomDialogSize"));
    Size->SetWidthOverride(560.f);
    UOverlaySlot* Center = Root->AddChildToOverlay(Size);
    Center->SetHorizontalAlignment(HAlign_Center); Center->SetVerticalAlignment(VAlign_Center);
    UBorder* Card = Control<UBorder>(WidgetTree, TEXT("RoomDialogCard"));
    Card->SetBrush(Brush(Panel, 12.f, Gold)); Card->SetPadding(FMargin(30.f)); Size->SetContent(Card);
    UVerticalBox* Form = Control<UVerticalBox>(WidgetTree, TEXT("RoomDialogForm")); Card->SetContent(Form);
    TitleText = Control<UTextBlock>(WidgetTree, TEXT("TitleText")); Font(TitleText, 28.f, Gold); Add(Form, TitleText, 16.f);
    BodyText = Control<UTextBlock>(WidgetTree, TEXT("BodyText")); Font(BodyText, 17.f, Ivory); BodyText->SetAutoWrapText(true); Add(Form, BodyText, 22.f);
    RoomNameLabel = Control<UTextBlock>(WidgetTree, TEXT("RoomNameLabel")); RoomNameLabel->SetText(FText::FromString(TEXT("방 제목"))); Font(RoomNameLabel, 17.f, Ivory); Add(Form, RoomNameLabel, 6.f);
    RoomNameInput = Control<UEditableTextBox>(WidgetTree, TEXT("RoomNameInput")); Add(Form, RoomNameInput, 16.f);
    PasswordLabel = Control<UTextBlock>(WidgetTree, TEXT("PasswordLabel")); Font(PasswordLabel, 17.f, Ivory); Add(Form, PasswordLabel, 6.f);
    PasswordInput = Control<UEditableTextBox>(WidgetTree, TEXT("PasswordInput")); Add(Form, PasswordInput, 16.f);
    for (UEditableTextBox* Input : {RoomNameInput.Get(), PasswordInput.Get()})
    {
        FEditableTextBoxStyle Style = Input->GetWidgetStyle();
        Style.BackgroundImageNormal = Brush(Background, 6.f);
        Style.BackgroundImageHovered = Brush(Background, 6.f, Gold);
        Style.BackgroundImageFocused = Brush(Background, 6.f, Gold);
        Style.BackgroundImageReadOnly = Style.BackgroundImageNormal;
        Style.ForegroundColor = FSlateColor(Ivory); Style.Padding = FMargin(12.f);
        Style.TextStyle.Font.Size = 20.f; Input->SetWidgetStyle(Style);
        Input->SetClearKeyboardFocusOnCommit(false);
    }
    RoomNameInput->SetHintText(FText::FromString(TEXT("최대 48자")));
    PasswordInput->SetHintText(FText::FromString(TEXT("비밀번호 입력"))); PasswordInput->SetIsPassword(true);
    ErrorText = Control<UTextBlock>(WidgetTree, TEXT("ErrorText")); Font(ErrorText, 16.f, FLinearColor(.95f, .25f, .18f, 1.f)); ErrorText->SetAutoWrapText(true); Add(Form, ErrorText, 18.f);
    UHorizontalBox* Buttons = Control<UHorizontalBox>(WidgetTree, TEXT("RoomDialogButtons")); Add(Form, Buttons, 0.f);
    SubmitButton = Control<UButton>(WidgetTree, TEXT("SubmitButton")); CancelButton = Control<UButton>(WidgetTree, TEXT("CancelButton"));
    SubmitCaption = Control<UTextBlock>(WidgetTree, TEXT("SubmitCaption")); Font(SubmitCaption, 19.f, Background); SubmitButton->SetContent(SubmitCaption);
    UTextBlock* CancelCaption = Control<UTextBlock>(WidgetTree, TEXT("CancelCaption")); Font(CancelCaption, 19.f, Ivory); CancelCaption->SetText(FText::FromString(TEXT("취소"))); CancelButton->SetContent(CancelCaption);
    for (UButton* Button : {SubmitButton.Get(), CancelButton.Get()})
    {
        FButtonStyle Style = Button->GetStyle();
        const bool Primary = Button == SubmitButton;
        Style.Normal = Brush(Primary ? Gold : Background, 6.f, Gold);
        Style.Hovered = Brush(Primary ? FLinearColor(.94f, .67f, .29f, 1.f) : Panel, 6.f, Gold);
        Style.Pressed = Style.Normal; Style.Disabled = Brush(Background, 6.f);
        Style.NormalPadding = FMargin(20.f, 12.f); Style.PressedPadding = Style.NormalPadding;
        Button->SetStyle(Style);
        UButtonSlot* Content = Cast<UButtonSlot>(Button->GetContent()->Slot);
        Content->SetHorizontalAlignment(HAlign_Center); Content->SetVerticalAlignment(VAlign_Center);
        UHorizontalBoxSlot* ButtonRowSlot = Buttons->AddChildToHorizontalBox(Button);
        ButtonRowSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill)); ButtonRowSlot->SetPadding(FMargin(Primary ? 0.f : 12.f, 0.f, 0.f, 0.f));
    }
    ApplyMode();
}

TSharedRef<SWidget> UCasinoRoomDialogWidget::RebuildWidget() { EnsureLayout(); return Super::RebuildWidget(); }

void UCasinoRoomDialogWidget::ConfigureCreate() { Mode = ECasinoRoomDialogMode::Create; RequestedRoom = FCasinoRoomInfo(); EnsureLayout(); ApplyMode(); }
void UCasinoRoomDialogWidget::ConfigureJoin(const FCasinoRoomInfo& Room) { Mode = ECasinoRoomDialogMode::Join; RequestedRoom = Room; EnsureLayout(); ApplyMode(); }

void UCasinoRoomDialogWidget::ApplyMode()
{
    const bool Create = Mode == ECasinoRoomDialogMode::Create;
    const bool Password = Create || RequestedRoom.bPasswordProtected;
    if (TitleText) TitleText->SetText(FText::FromString(Create ? TEXT("방 만들기") : TEXT("방 참가")));
    if (BodyText) BodyText->SetText(FText::FromString(Create ? TEXT("방 제목을 정하세요. 비밀번호를 비워두면 누구나 참가할 수 있습니다.") : RequestedRoom.RoomName));
    if (RoomNameLabel) RoomNameLabel->SetVisibility(Create ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    if (RoomNameInput) RoomNameInput->SetVisibility(Create ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    if (PasswordLabel) { PasswordLabel->SetText(FText::FromString(Create ? TEXT("비밀번호 (선택, 최대 64자)") : TEXT("비밀번호"))); PasswordLabel->SetVisibility(Password ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
    if (PasswordInput) { PasswordInput->SetIsPassword(true); PasswordInput->SetVisibility(Password ? ESlateVisibility::Visible : ESlateVisibility::Collapsed); }
    if (SubmitCaption) SubmitCaption->SetText(FText::FromString(bSubmitting ? TEXT("처리 중...") : Create ? TEXT("만들기") : TEXT("참가")));
}

void UCasinoRoomDialogWidget::NativeConstruct()
{
    Super::NativeConstruct(); EnsureLayout();
    if (IsDesignTime()) return;
    if (SubmitButton) SubmitButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Submit);
    if (CancelButton) CancelButton->OnClicked.AddUniqueDynamic(this, &ThisClass::Cancel);
    if (RoomNameInput) RoomNameInput->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleCommitted);
    if (PasswordInput) PasswordInput->OnTextCommitted.AddUniqueDynamic(this, &ThisClass::HandleCommitted);
    if (LocalOwner(GetOwningPlayer()))
    {
        Online = GetOwningPlayer()->GetGameInstance()->GetSubsystem<UCasinoOnlineSubsystem>();
        if (Online) { Online->OnChanged.AddUniqueDynamic(this, &ThisClass::HandleOnlineChanged); Online->OnError.AddUniqueDynamic(this, &ThisClass::HandleOnlineError); }
    }
}

void UCasinoRoomDialogWidget::UnbindOnline()
{
    if (Online) { Online->OnChanged.RemoveDynamic(this, &ThisClass::HandleOnlineChanged); Online->OnError.RemoveDynamic(this, &ThisClass::HandleOnlineError); }
    Online = nullptr;
}

void UCasinoRoomDialogWidget::NativeDestruct()
{
    UnbindOnline();
    if (PasswordInput) PasswordInput->SetText(FText::GetEmpty());
    if (TWeakObjectPtr<UCasinoRoomDialogWidget>* Existing = OpenDialogs.Find(GetOwningPlayer())) if (Existing->Get() == this) OpenDialogs.Remove(GetOwningPlayer());
    Super::NativeDestruct();
}

UCasinoRoomDialogWidget* UCasinoRoomDialogWidget::Show(APlayerController* Controller, ECasinoRoomDialogMode InMode,
    const FCasinoRoomInfo& Room, TSubclassOf<UCasinoRoomDialogWidget> DialogClass)
{
    if (!LocalOwner(Controller)) return nullptr;
    UCasinoOnlineSubsystem* Subsystem = Controller->GetGameInstance()->GetSubsystem<UCasinoOnlineSubsystem>();
    if (!Subsystem || Subsystem->State != ECasinoOnlineState::Ready) return nullptr;
    for (auto It = OpenDialogs.CreateIterator(); It; ++It) if (!It.Key().IsValid() || !It.Value().IsValid()) It.RemoveCurrent();
    if (TWeakObjectPtr<UCasinoRoomDialogWidget>* Existing = OpenDialogs.Find(Controller))
    {
        if (UCasinoRoomDialogWidget* Dialog = Existing->Get())
        {
            if (!Dialog->bClosed && (Dialog->GetParent() || Dialog->IsInViewport()))
            { Dialog->FocusForm(); return Dialog; }
        }
        OpenDialogs.Remove(Controller);
    }
    UClass* Class = DialogClass ? DialogClass.Get() : StaticClass();
    if (Class->HasAnyClassFlags(CLASS_Abstract)) return nullptr;
    UCasinoRoomDialogWidget* Dialog = CreateWidget<UCasinoRoomDialogWidget>(Controller, Class);
    if (!Dialog) return nullptr;
    Dialog->bPreviousCursor = Controller->bShowMouseCursor;
    if (FSlateApplication::IsInitialized())
    {
        if (TSharedPtr<FSlateUser> User = Controller->GetLocalPlayer()->GetSlateUser()) Dialog->FocusUser = User->GetUserIndex();
        Dialog->PreviousFocus = FSlateApplication::Get().GetUserFocusedWidget(Dialog->FocusUser);
    }
    if (InMode == ECasinoRoomDialogMode::Create) Dialog->ConfigureCreate(); else Dialog->ConfigureJoin(Room);
    Dialog->SetVisibility(ESlateVisibility::Visible);
    // Managed screens share the modal layer. The legacy L_MainMenu level BP
    // instead adds its menu to the global viewport, above all player layers.
    // Match that host so the popup cannot be hidden behind the existing menu.
    bool bAdded = false;
    if (Acasino_simulatorPlayerController* CasinoController = Cast<Acasino_simulatorPlayerController>(Controller))
    {
        if (IsValid(CasinoController->ActiveScreenWidget) && CasinoController->ActiveScreenWidget->GetParent()
            && IsValid(CasinoController->UIRoot) && CasinoController->UIRoot->IsInViewport())
        {
            CasinoController->UIRoot->AddModal(Dialog);
            bAdded = Dialog->GetParent() != nullptr;
        }
    }
    if (!bAdded)
    {
        Dialog->AddToViewport(10000);
        bAdded = Dialog->IsInViewport();
    }
    if (!bAdded) { Dialog->UnbindOnline(); return nullptr; }
    OpenDialogs.Add(Controller, Dialog); Dialog->FocusForm(); return Dialog;
}

UCasinoRoomDialogWidget* UCasinoRoomDialogWidget::ShowCreateRoomDialog(APlayerController* Controller, TSubclassOf<UCasinoRoomDialogWidget> DialogClass)
{ return Show(Controller, ECasinoRoomDialogMode::Create, FCasinoRoomInfo(), DialogClass); }
UCasinoRoomDialogWidget* UCasinoRoomDialogWidget::ShowJoinRoomDialog(APlayerController* Controller, const FCasinoRoomInfo& Room, TSubclassOf<UCasinoRoomDialogWidget> DialogClass)
{
    if (!Room.bPasswordProtected)
    {
        if (LocalOwner(Controller))
            if (UCasinoOnlineSubsystem* Subsystem = Controller->GetGameInstance()->GetSubsystem<UCasinoOnlineSubsystem>())
                if (Subsystem->State == ECasinoOnlineState::Ready) Subsystem->JoinRoomByIdWithPassword(Room.SessionId, FString());
        return nullptr;
    }
    return Show(Controller, ECasinoRoomDialogMode::Join, Room, DialogClass);
}

void UCasinoRoomDialogWidget::FocusForm()
{
    APlayerController* Controller = GetOwningPlayer();
    if (!LocalOwner(Controller) || bClosed) return;
    UWidget* Focus = Mode == ECasinoRoomDialogMode::Create ? static_cast<UWidget*>(RoomNameInput.Get())
        : RequestedRoom.bPasswordProtected ? static_cast<UWidget*>(PasswordInput.Get()) : static_cast<UWidget*>(SubmitButton.Get());
    if (!Focus) Focus = this;
    FInputModeUIOnly Input; Input.SetWidgetToFocus(Focus->TakeWidget()); Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    Controller->SetInputMode(Input); Controller->bShowMouseCursor = true; Focus->SetUserFocus(Controller);
}

void UCasinoRoomDialogWidget::SetBusy(bool Busy)
{
    bSubmitting = Busy;
    if (SubmitButton) SubmitButton->SetIsEnabled(!Busy);
    if (CancelButton) CancelButton->SetIsEnabled(!Busy);
    if (RoomNameInput) RoomNameInput->SetIsReadOnly(Busy);
    if (PasswordInput) PasswordInput->SetIsReadOnly(Busy);
    ApplyMode();
    if (SubmitCaption) SubmitCaption->SetColorAndOpacity(FSlateColor(Busy ? Ivory : Background));
}

void UCasinoRoomDialogWidget::ShowError(const FString& Message)
{
    if (ErrorText) { ErrorText->SetText(FText::FromString(Message)); ErrorText->SetVisibility(ESlateVisibility::HitTestInvisible); }
}

void UCasinoRoomDialogWidget::Submit()
{
    if (bClosed || bSubmitting) return;
    EnsureLayout();
    if (!LocalOwner(GetOwningPlayer()) || !Online || Online->State != ECasinoOnlineState::Ready)
    { ShowError(TEXT("온라인 준비 상태를 확인한 뒤 다시 시도하세요.")); return; }
    const FString Name = RoomNameInput->GetText().ToString().TrimStartAndEnd();
    const FString Password = PasswordInput->GetText().ToString();
    if (Mode == ECasinoRoomDialogMode::Create && (Name.IsEmpty() || Name.Len() > 48))
    { ShowError(TEXT("방 제목을 1~48자로 입력하세요.")); RoomNameInput->SetUserFocus(GetOwningPlayer()); return; }
    if (Password.Len() > 64) { ShowError(TEXT("비밀번호는 64자까지 입력할 수 있습니다.")); return; }
    if (Mode == ECasinoRoomDialogMode::Join && RequestedRoom.bPasswordProtected && Password.IsEmpty())
    { ShowError(TEXT("비밀번호를 입력하세요.")); PasswordInput->SetUserFocus(GetOwningPlayer()); return; }
    if (Mode == ECasinoRoomDialogMode::Join && RequestedRoom.SessionId.IsEmpty())
    { ShowError(TEXT("방 목록을 다시 검색한 뒤 참가하세요.")); return; }
    ShowError(FString()); SetBusy(true);
    if (Mode == ECasinoRoomDialogMode::Create) Online->CreateRoomWithOptions(Name, Password);
    else Online->JoinRoomByIdWithPassword(RequestedRoom.SessionId, Password);
    // Validation failures may leave State unchanged without an async callback.
    if (Online && Online->State == ECasinoOnlineState::Ready && bSubmitting) SetBusy(false);
}

void UCasinoRoomDialogWidget::HandleOnlineChanged()
{
    if (!Online || bClosed) return;
    if ((Online->State == ECasinoOnlineState::InRoom || Online->State == ECasinoOnlineState::InGame) && bSubmitting) CloseDialog(false);
    else if (Online->State == ECasinoOnlineState::Creating || Online->State == ECasinoOnlineState::Searching || Online->State == ECasinoOnlineState::Joining) SetBusy(true);
    else if (Online->State == ECasinoOnlineState::Ready || Online->State == ECasinoOnlineState::Offline) SetBusy(false);
}

void UCasinoRoomDialogWidget::HandleOnlineError(FString Operation, FString Message)
{
    if (bClosed) return;
    const bool Matches = Operation == (Mode == ECasinoRoomDialogMode::Create ? TEXT("CreateRoom") : TEXT("JoinRoom"));
    if (Matches || (bSubmitting && (Operation == TEXT("EOS") || Operation == TEXT("Network") || Operation == TEXT("Travel") || Operation == TEXT("FindRooms"))))
    { SetBusy(false); ShowError(Message); }
}

void UCasinoRoomDialogWidget::CloseDialog(bool RestoreFocus)
{
    if (bClosed) return;
    bClosed = true; SetBusy(false); UnbindOnline();
    if (PasswordInput) PasswordInput->SetText(FText::GetEmpty());
    APlayerController* Controller = GetOwningPlayer();
    if (TWeakObjectPtr<UCasinoRoomDialogWidget>* Existing = OpenDialogs.Find(Controller))
        if (Existing->Get() == this) OpenDialogs.Remove(Controller);
    RemoveFromParent();
    if (RestoreFocus && LocalOwner(Controller))
    {
        FInputModeUIOnly Input;
        Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        if (TSharedPtr<SWidget> Focus = PreviousFocus.Pin())
        {
            if (Focus->SupportsKeyboardFocus())
            {
                Input.SetWidgetToFocus(Focus);
                if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetUserFocus(FocusUser, Focus);
            }
        }
        Controller->SetInputMode(Input);
        Controller->bShowMouseCursor = bPreviousCursor;
    }
}

void UCasinoRoomDialogWidget::Cancel() { if (!bSubmitting) CloseDialog(true); }
void UCasinoRoomDialogWidget::HandleCommitted(const FText&, ETextCommit::Type Method) { if (Method == ETextCommit::OnEnter) Submit(); }
FReply UCasinoRoomDialogWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{ if (Event.GetKey() == EKeys::Escape) { Cancel(); return FReply::Handled(); } return Super::NativeOnPreviewKeyDown(Geometry, Event); }
FReply UCasinoRoomDialogWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{ if (Event.GetKey() == EKeys::Enter) { Submit(); return FReply::Handled(); } return Super::NativeOnKeyDown(Geometry, Event); }
FReply UCasinoRoomDialogWidget::NativeOnMouseButtonDown(const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }
FReply UCasinoRoomDialogWidget::NativeOnMouseButtonUp(const FGeometry&, const FPointerEvent&) { return FReply::Handled(); }

void UCasinoRoomDialogWidget::RefreshRoomPasswordIndicator(UUserWidget* Entry, const FCasinoRoomInfo& Room)
{
    if (Entry && Entry->WidgetTree) if (UTextBlock* Marker = Cast<UTextBlock>(Entry->WidgetTree->FindWidget(TEXT("CasinoRoomPasswordMarker"))))
    { Marker->SetText(FText::FromString(TEXT("비밀번호"))); Marker->SetVisibility(Room.bPasswordProtected ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
}

void UCasinoRoomDialogWidget::RestoreMainMenuError(UUserWidget* Menu)
{
    if (!Menu || !Menu->WidgetTree || !LocalOwner(Menu->GetOwningPlayer())) return;
    if (UCasinoOnlineSubsystem* Subsystem = Menu->GetOwningPlayer()->GetGameInstance()->GetSubsystem<UCasinoOnlineSubsystem>())
        if (!Subsystem->LastError.IsEmpty()) if (UTextBlock* Status = Cast<UTextBlock>(Menu->WidgetTree->FindWidget(TEXT("StatusText")))) Status->SetText(FText::FromString(Subsystem->LastError));
}

#if WITH_EDITOR
void UCasinoRoomDialogWidget::PrepareDesignerPreview(bool Create, const FCasinoRoomInfo& Room)
{ if (IsDesignTime()) { if (Create) ConfigureCreate(); else ConfigureJoin(Room); } }
#endif
