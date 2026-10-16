#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Framework/Commands/InputChord.h"
#include "CasinoVoiceSettingsWidgets.generated.h"

class UCasinoOnlineSubsystem;
class UCheckBox;
class UInputKeySelector;
class UScrollBox;
class USlider;
class UTextBlock;
class APlayerState;
struct FCasinoVoiceParticipant;

// Each row owns its delegates, so a late roster refresh cannot change their recipient.
UCLASS()
class CASINO_SIMULATOR_API UCasinoVoiceParticipantRowWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void UpdateParticipant(UCasinoOnlineSubsystem* Online, APlayerState* Player, const FString& VoiceId,
        bool bTalking, bool bVoiceAvailable = true);
    void PrepareDesignerPreview(const FCasinoVoiceParticipant& Sample);
    void StopEditing();
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativePreConstruct() override;
    virtual void NativeDestruct() override;
private:
    void BuildPanel();
    UFUNCTION() void HandleVolume(float Value);
    UFUNCTION() void HandleMuted(bool bMuted);
    UFUNCTION() void BeginVolumeCapture();
    UFUNCTION() void EndVolumeCapture();
    UPROPERTY(Transient) TObjectPtr<UTextBlock> NameText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> StateText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> VolumeText;
    UPROPERTY(Transient) TObjectPtr<USlider> VolumeSlider;
    UPROPERTY(Transient) TObjectPtr<UCheckBox> MuteCheck;
    TWeakObjectPtr<UCasinoOnlineSubsystem> VoiceSource;
    FString ParticipantId;
    bool bUpdating = false;
    bool bDragging = false;
};

// Embedded in WBP_Settings without changing that Blueprint's parent or events.
UCLASS()
class CASINO_SIMULATOR_API UCasinoVoiceSettingsWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void PrepareDesignerPreview(const TArray<FCasinoVoiceParticipant>& Samples);
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    void BuildPanel();
    UFUNCTION() void RefreshVoiceSettings();
    UFUNCTION() void HandleOutput(float Value);
    UFUNCTION() void HandleInput(float Value);
    UFUNCTION() void HandleEnabled(bool bEnabled);
    UFUNCTION() void HandleMicrophoneMuted(bool bMuted);
    UFUNCTION() void HandlePushToTalk(bool bEnabled);
    UFUNCTION() void BeginVolumeCapture();
    UFUNCTION() void EndVolumeCapture();
    UPROPERTY(Transient) TObjectPtr<USlider> OutputSlider;
    UPROPERTY(Transient) TObjectPtr<USlider> InputSlider;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> OutputText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> InputText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PushToTalkText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> ConnectionText;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> EmptyRosterText;
    UPROPERTY(Transient) TObjectPtr<UCheckBox> EnabledCheck;
    UPROPERTY(Transient) TObjectPtr<UCheckBox> MicrophoneMuteCheck;
    UPROPERTY(Transient) TObjectPtr<UCheckBox> PushToTalkCheck;
    UPROPERTY(Transient) TObjectPtr<UScrollBox> ParticipantList;
    UPROPERTY(Transient) TMap<FString, TObjectPtr<UCasinoVoiceParticipantRowWidget>> ParticipantRows;
    TWeakObjectPtr<UCasinoOnlineSubsystem> VoiceSource;
    FTimerHandle RefreshTimer;
    bool bUpdating = false;
    bool bDragging = false;
    bool bPreview = false;
};

UCLASS()
class CASINO_SIMULATOR_API UCasinoVoiceKeyBindingWidget : public UUserWidget
{
    GENERATED_BODY()
protected:
    virtual TSharedRef<SWidget> RebuildWidget() override;
    virtual void NativePreConstruct() override;
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;
private:
    void BuildPanel();
    UFUNCTION() void HandleSelectedKey(FInputChord Chord);
    UFUNCTION() void HandleSelectingKey();
    UFUNCTION() void HandleResetKey();
    UFUNCTION() void RefreshBinding();
    UPROPERTY(Transient) TObjectPtr<UInputKeySelector> KeySelector;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> HintText;
    TWeakObjectPtr<UCasinoOnlineSubsystem> VoiceSource;
    FTimerHandle RefreshTimer;
    bool bUpdating = false;
    bool bCapturing = false;
};
