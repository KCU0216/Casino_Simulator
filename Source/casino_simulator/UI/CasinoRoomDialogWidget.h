#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "CasinoRoomDialogWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

UENUM(BlueprintType)
enum class ECasinoRoomDialogMode : uint8 { Create, Join };

/** Local modal room form. A WidgetBlueprint child may replace the named controls. */
UCLASS(BlueprintType, Blueprintable)
class CASINO_SIMULATOR_API UCasinoRoomDialogWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    explicit UCasinoRoomDialogWidget(const FObjectInitializer& Initializer);

    UFUNCTION(BlueprintCallable, Category="Casino|Room")
    static UCasinoRoomDialogWidget* ShowCreateRoomDialog(APlayerController* PlayerController,
        TSubclassOf<UCasinoRoomDialogWidget> DialogClass = nullptr);
    UFUNCTION(BlueprintCallable, Category="Casino|Room")
    static UCasinoRoomDialogWidget* ShowJoinRoomDialog(APlayerController* PlayerController, const FCasinoRoomInfo& Room,
        TSubclassOf<UCasinoRoomDialogWidget> DialogClass = nullptr);
    UFUNCTION(BlueprintCallable, Category="Casino|Room")
    static void RefreshRoomPasswordIndicator(UUserWidget* Entry, const FCasinoRoomInfo& Room);
    UFUNCTION(BlueprintCallable, Category="Casino|Room")
    static void RestoreMainMenuError(UUserWidget* Menu);

    UFUNCTION(BlueprintCallable, Category="Casino|Room") void EnsureLayout();
    UFUNCTION(BlueprintCallable, Category="Casino|Room") void ConfigureCreate();
    UFUNCTION(BlueprintCallable, Category="Casino|Room") void ConfigureJoin(const FCasinoRoomInfo& Room);
    UFUNCTION(BlueprintCallable, Category="Casino|Room") void Submit();
    UFUNCTION(BlueprintCallable, Category="Casino|Room") void Cancel();
    UFUNCTION(BlueprintPure, Category="Casino|Room") ECasinoRoomDialogMode GetDialogMode() const { return Mode; }
    UFUNCTION(BlueprintPure, Category="Casino|Room") bool IsSubmitting() const { return bSubmitting; }
    UFUNCTION(BlueprintPure, Category="Casino|Room") UEditableTextBox* GetRoomNameInput() const { return RoomNameInput; }
    UFUNCTION(BlueprintPure, Category="Casino|Room") UEditableTextBox* GetPasswordInput() const { return PasswordInput; }
    UFUNCTION(BlueprintPure, Category="Casino|Room") UTextBlock* GetErrorText() const { return ErrorText; }
#if WITH_EDITOR
    void PrepareDesignerPreview(bool bCreate, const FCasinoRoomInfo& Room);
#endif

protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
    virtual FReply NativeOnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;

    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> TitleText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> BodyText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> RoomNameLabel;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> RoomNameInput;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> PasswordLabel;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> PasswordInput;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ErrorText;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> SubmitButton;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UButton> CancelButton;
    UPROPERTY(BlueprintReadOnly, meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SubmitCaption;

private:
    static UCasinoRoomDialogWidget* Show(APlayerController* Controller, ECasinoRoomDialogMode Mode,
        const FCasinoRoomInfo& Room, TSubclassOf<UCasinoRoomDialogWidget> DialogClass);
    void FindControls();
    void ApplyMode();
    void SetBusy(bool bBusy);
    void ShowError(const FString& Message);
    void CloseDialog(bool bRestoreFocus);
    void UnbindOnline();
    void FocusForm();
    UFUNCTION() void HandleOnlineChanged();
    UFUNCTION() void HandleOnlineError(FString Operation, FString Message);
    UFUNCTION() void HandleCommitted(const FText& Text, ETextCommit::Type Method);

    UPROPERTY(Transient) TObjectPtr<UCasinoOnlineSubsystem> Online;
    UPROPERTY(Transient) FCasinoRoomInfo RequestedRoom;
    ECasinoRoomDialogMode Mode = ECasinoRoomDialogMode::Create;
    bool bSubmitting = false;
    bool bClosed = false;
    bool bPreviousCursor = true;
    uint32 FocusUser = 0;
    TWeakPtr<SWidget> PreviousFocus;
};
