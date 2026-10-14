#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CasinoCalculatorWidget.generated.h"

class FWidgetPath;
class UButton;

/** Keyboard support for the existing shared number pad. Its Blueprint click logic remains authoritative. */
UCLASS(Blueprintable)
class CASINO_SIMULATOR_API UCasinoCalculatorWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	explicit UCasinoCalculatorWidget(const FObjectInitializer& Initializer);
	virtual void SetVisibility(ESlateVisibility InVisibility) override;

protected:
	virtual void NativePreConstruct() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	friend class FCasinoCalculatorKeyboardTest;
	friend class FCasinoSlotKeypadTest;

	static FName ButtonForKey(const FKey& Key);
	bool GetActivePath(FWidgetPath& Path) const;
	bool OwnsKeypadFocus(uint32 UserIndex) const;
	bool HandleCalculatorKey(const FKeyEvent& Event);
	void CapturePreviousFocus();
	void RestorePreviousFocus();
	uint32 GetFocusUser() const;

	TWeakPtr<SWidget> PreviousFocus;
	uint32 FocusUser = 0;
	uint64 LastVisibleTick = MAX_uint64;
	bool bClaimFocusOnActivation = true;
};
