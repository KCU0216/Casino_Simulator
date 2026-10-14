#include "UI/CasinoCalculatorWidget.h"
#include "UI/CasinoKeypadStyle.h"

#include "Components/Button.h"
#include "Engine/LocalPlayer.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "InputCoreTypes.h"
#include "Layout/WidgetPath.h"

UCasinoCalculatorWidget::UCasinoCalculatorWidget(const FObjectInitializer& Initializer)
	: Super(Initializer)
{
	SetIsFocusable(true);
}

FName UCasinoCalculatorWidget::ButtonForKey(const FKey& Key)
{
	const FKey Digits[] = { EKeys::Zero, EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four,
		EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	const FKey Numpad[] = { EKeys::NumPadZero, EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree,
		EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
	for (int32 Digit = 0; Digit < UE_ARRAY_COUNT(Digits); ++Digit)
		if (Key == Digits[Digit] || Key == Numpad[Digit])
			return FName(*FString::Printf(TEXT("Button_%d"), Digit));
	// Windows translates both the main Return key and keypad Return to EKeys::Enter.
	if (Key == EKeys::Enter) return TEXT("Button_OK");
	if (Key == EKeys::BackSpace || Key == EKeys::Delete) return TEXT("Button_Delete");
	return NAME_None;
}

bool UCasinoCalculatorWidget::GetActivePath(FWidgetPath& Path) const
{
	const TSharedPtr<SWidget> SlateWidget = GetCachedWidget();
	if (IsDesignTime() || !GetIsEnabled() || !SlateWidget.IsValid() || !FSlateApplication::IsInitialized()) return false;
	// The slot-machine control stays visible when its numeric editing grid is
	// closed. Keyboard focus belongs to the number pad only while that grid is
	// shown. Other consumers display Button_0 as part of their shared keypad.
	UWidget* ActivationWidget = GetWidgetFromName(TEXT("SlotKeypadGrid"));
	if (!ActivationWidget) ActivationWidget = GetWidgetFromName(TEXT("Button_0"));
	const TSharedPtr<SWidget> Activation = ActivationWidget ? ActivationWidget->GetCachedWidget() : nullptr;
	if (!Activation.IsValid() || !Activation->GetVisibility().IsVisible()) return false;
	if (!FSlateApplication::Get().GeneratePathToWidgetUnchecked(SlateWidget.ToSharedRef(), Path)) return false;
	// This also rejects hidden ancestors and inactive WidgetSwitcher pages, whose
	// children can still report Visible at the UMG level.
	for (int32 Index = 0; Index < Path.Widgets.Num(); ++Index)
	{
		const TSharedRef<SWidget>& Widget = Path.Widgets[Index].Widget;
		if (!Widget->IsEnabled() || !Widget->GetVisibility().IsVisible() || Widget->GetRenderOpacity() <= 0.f) return false;
	}
	FWidgetPath ActivationPath;
	if (!FSlateApplication::Get().GeneratePathToWidgetUnchecked(Activation.ToSharedRef(), ActivationPath)) return false;
	for (int32 Index = 0; Index < ActivationPath.Widgets.Num(); ++Index)
	{
		const TSharedRef<SWidget>& Widget = ActivationPath.Widgets[Index].Widget;
		// Disabling one numeric button must not disable the entire shared keypad.
		const bool bCheckEnabled = Index < ActivationPath.Widgets.Num() - 1 || !Cast<UButton>(ActivationWidget);
		if ((bCheckEnabled && !Widget->IsEnabled()) || !Widget->GetVisibility().IsVisible() || Widget->GetRenderOpacity() <= 0.f) return false;
	}
	return Path.IsValid();
}

bool UCasinoCalculatorWidget::OwnsKeypadFocus(uint32 UserIndex) const
{
	const TSharedPtr<SWidget> Widget = GetCachedWidget();
	if (!Widget.IsValid()) return false;
	if (Widget->HasUserFocus(UserIndex).IsSet()) return true;
	if (UWidget* Grid = GetWidgetFromName(TEXT("SlotKeypadGrid")))
	{
		// Lever and exit buttons are siblings of the slot number pad. Their
		// focus belongs to the wider control UI, rather than this input handler.
		const TSharedPtr<SWidget> SlateGrid = Grid->GetCachedWidget();
		return SlateGrid.IsValid() && (SlateGrid->HasUserFocus(UserIndex).IsSet() || SlateGrid->HasUserFocusedDescendants(UserIndex));
	}
	return Widget->HasUserFocusedDescendants(UserIndex);
}

uint32 UCasinoCalculatorWidget::GetFocusUser() const
{
	if (const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer())
		if (const TSharedPtr<const FSlateUser> User = LocalPlayer->GetSlateUser()) return User->GetUserIndex();
	return 0;
}

void UCasinoCalculatorWidget::CapturePreviousFocus()
{
	if (IsDesignTime() || !FSlateApplication::IsInitialized()) return;
	FocusUser = GetFocusUser();
	if (OwnsKeypadFocus(FocusUser)) return;
	PreviousFocus = FSlateApplication::Get().GetUserFocusedWidget(FocusUser);
}

void UCasinoCalculatorWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	ApplyCasinoKeypadStyle(this);
}

void UCasinoCalculatorWidget::NativeConstruct()
{
	SetIsFocusable(true);
	// UIManager can focus a separately managed keypad immediately when adding
	// it. Remember the underlying UI before that happens, rather than on tick.
	CapturePreviousFocus();
	bClaimFocusOnActivation = true;
	LastVisibleTick = MAX_uint64;
	Super::NativeConstruct();
}

void UCasinoCalculatorWidget::SetVisibility(ESlateVisibility InVisibility)
{
	if (InVisibility != ESlateVisibility::Hidden && InVisibility != ESlateVisibility::Collapsed) CapturePreviousFocus();
	Super::SetVisibility(InVisibility);
	if (InVisibility == ESlateVisibility::Hidden || InVisibility == ESlateVisibility::Collapsed)
	{
		RestorePreviousFocus();
		LastVisibleTick = MAX_uint64;
	}
	// A caller may reveal the keypad before its containing panel and then focus
	// a Cancel button. Claim only on a subsequent tick, after that work is done.
	bClaimFocusOnActivation = true;
}

void UCasinoCalculatorWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	FWidgetPath Path;
	if (!GetActivePath(Path))
	{
		RestorePreviousFocus();
		bClaimFocusOnActivation = true;
		LastVisibleTick = MAX_uint64;
		return;
	}

	const uint32 UserIndex = GetFocusUser();
	FSlateApplication& Slate = FSlateApplication::Get();
	const TSharedPtr<SWidget> Widget = GetCachedWidget();
	const TSharedPtr<SWidget> Focus = Slate.GetUserFocusedWidget(UserIndex);
	const bool bAlreadyFocused = OwnsKeypadFocus(UserIndex);
	// Hidden panels are not ticked by Slate. A gap therefore marks a new
	// activation even if only the parent was collapsed, rather than the keypad.
	const bool bResumed = LastVisibleTick == MAX_uint64 ||
		(GFrameCounter > LastVisibleTick && GFrameCounter - LastVisibleTick > 1);
	const bool bParentWasFocused = Focus.IsValid() && Path.ContainsWidget(Focus.Get()) && Focus != Widget;
	if (!bAlreadyFocused && (bClaimFocusOnActivation || bResumed || bParentWasFocused))
	{
		PreviousFocus = Focus;
		FocusUser = UserIndex;
		Slate.SetUserFocus(UserIndex, Widget, EFocusCause::SetDirectly);
	}
	bClaimFocusOnActivation = false;
	LastVisibleTick = GFrameCounter;
}

void UCasinoCalculatorWidget::RestorePreviousFocus()
{
	if (FSlateApplication::IsInitialized())
	{
		FSlateApplication& Slate = FSlateApplication::Get();
		const TSharedPtr<SWidget> Focus = PreviousFocus.Pin();
		if (OwnsKeypadFocus(FocusUser))
		{
			bool bRestored = false;
			FWidgetPath PreviousPath;
			if (Focus.IsValid() && Focus->SupportsKeyboardFocus() &&
				Slate.GeneratePathToWidgetUnchecked(Focus.ToSharedRef(), PreviousPath))
			{
				bool bEnabled = true;
				for (int32 Index = 0; Index < PreviousPath.Widgets.Num(); ++Index)
					bEnabled &= PreviousPath.Widgets[Index].Widget->IsEnabled();
				if (bEnabled) bRestored = Slate.SetUserFocus(FocusUser, Focus, EFocusCause::SetDirectly);
			}
			// The old UI can have been removed in a confirm callback. Never leave
			// this hidden keypad as the focused widget, or override another UI.
			if (!bRestored) Slate.ClearUserFocus(FocusUser, EFocusCause::SetDirectly);
		}
	}
	PreviousFocus.Reset();
}

void UCasinoCalculatorWidget::NativeDestruct()
{
	RestorePreviousFocus();
	bClaimFocusOnActivation = true;
	LastVisibleTick = MAX_uint64;
	Super::NativeDestruct();
}

bool UCasinoCalculatorWidget::HandleCalculatorKey(const FKeyEvent& Event)
{
	if (Event.IsControlDown() || Event.IsAltDown() || Event.IsCommandDown()) return false;
	const FName ButtonName = ButtonForKey(Event.GetKey());
	if (ButtonName.IsNone()) return false;
	FWidgetPath Path;
	if (!GetActivePath(Path)) return false;
	UButton* Button = Cast<UButton>(GetWidgetFromName(ButtonName));
	if (!IsValid(Button) && ButtonName == TEXT("Button_Delete"))
		Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_Backspace")));
	if (!IsValid(Button)) return false;
	// A disabled or hidden OK control must not let Enter fall through to a
	// focused numeric SButton and accidentally append another digit.
	if (!Button->GetIsEnabled()) return true;
	const TSharedPtr<SWidget> SlateButton = Button->GetCachedWidget();
	FWidgetPath ButtonPath;
	if (!SlateButton.IsValid() || !FSlateApplication::Get().GeneratePathToWidgetUnchecked(SlateButton.ToSharedRef(), ButtonPath)) return true;
	for (int32 Index = 0; Index < ButtonPath.Widgets.Num(); ++Index)
		if (!ButtonPath.Widgets[Index].Widget->IsEnabled()) return true;
	// One physical press is one existing click. In particular, holding Enter
	// must not repeatedly pay, trade, confirm a bet, or dismiss another screen.
	if (!Event.IsRepeat()) Button->OnClicked.Broadcast();
	return true;
}

FReply UCasinoCalculatorWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	return HandleCalculatorKey(Event) ? FReply::Handled() : Super::NativeOnPreviewKeyDown(Geometry, Event);
}

FReply UCasinoCalculatorWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	return HandleCalculatorKey(Event) ? FReply::Handled() : Super::NativeOnKeyDown(Geometry, Event);
}

FReply UCasinoCalculatorWidget::NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event)
{
	FWidgetPath Path;
	if (!Event.IsControlDown() && !Event.IsAltDown() && !Event.IsCommandDown()
		&& !ButtonForKey(Event.GetKey()).IsNone() && GetActivePath(Path)) return FReply::Handled();
	return Super::NativeOnKeyUp(Geometry, Event);
}
