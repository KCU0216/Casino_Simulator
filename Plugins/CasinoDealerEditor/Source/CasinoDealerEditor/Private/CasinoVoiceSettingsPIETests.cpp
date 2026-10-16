#if WITH_DEV_AUTOMATION_TESTS

#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CheckBox.h"
#include "Components/InputKeySelector.h"
#include "Components/ScrollBox.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "Online/CasinoOnlineSettings.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "UI/CasinoVoiceSettingsWidgets.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace CasinoMatchLoadingPIE
{
bool QueueTravel(FAutomationTestBase* Test, int32 PlayerCount, bool bCheckResults,
    TSharedPtr<IAutomationLatentCommand> AfterTravel);
}

namespace CasinoVoiceSettingsPIE
{
FString PreferenceSnapshot()
{
    const auto* Preferences = GetDefault<UCasinoVoicePreferences>();
    FString Result;
    for (TFieldIterator<FProperty> It(UCasinoVoicePreferences::StaticClass()); It; ++It)
        if (It->HasAnyPropertyFlags(CPF_Config))
        {
            Result += It->GetName() + TEXT("=");
            It->ExportText_InContainer(0, Result, Preferences, nullptr, nullptr, PPF_None);
            Result += TEXT("\n");
        }
    return Result;
}

APlayerController* LocalController(UWorld* World)
{
    if (World)
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
            if (APlayerController* Controller = It->Get(); Controller && Controller->IsLocalController()) return Controller;
    return nullptr;
}

TMap<FString, TWeakObjectPtr<UUserWidget>> ReadParticipantRows(UCasinoVoiceSettingsWidget* Voice)
{
    TMap<FString, TWeakObjectPtr<UUserWidget>> Result;
    FMapProperty* Property = Voice ? FindFProperty<FMapProperty>(Voice->GetClass(), TEXT("ParticipantRows")) : nullptr;
    FStrProperty* Key = Property ? CastField<FStrProperty>(Property->KeyProp) : nullptr;
    FObjectPropertyBase* Value = Property ? CastField<FObjectPropertyBase>(Property->ValueProp) : nullptr;
    if (!Key || !Value) return Result;
    FScriptMapHelper Map(Property, Property->ContainerPtrToValuePtr<void>(Voice));
    for (int32 Index = 0; Index < Map.GetMaxIndex(); ++Index)
        if (Map.IsValidIndex(Index))
            Result.Add(Key->GetPropertyValue(Map.GetKeyPtr(Index)),
                Cast<UUserWidget>(Value->GetObjectPropertyValue(Map.GetValuePtr(Index))));
    return Result;
}

class FActualSettingsUI final : public IAutomationLatentCommand
{
public:
    explicit FActualSettingsUI(FAutomationTestBase* InTest) : Test(InTest) {}
    ~FActualSettingsUI() override { Cleanup(); }

    bool Update() override
    {
        const double Now = FPlatformTime::Seconds();
        if (!Started) Started = Now;
        if (Now - Started > 20.)
        {
            Test->AddError(FString::Printf(TEXT("Actual two-player voice settings UI timed out at stage %d."), Stage));
            Cleanup();
            return true;
        }
        if (Stage == 0)
        {
            UWorld* Client = nullptr;
            for (const auto& Context : GEngine->GetWorldContexts())
                if (Context.WorldType == EWorldType::PIE && Context.World() && Context.World()->GetNetMode() == NM_Client)
                    Client = Context.World();
            APlayerController* Owner = LocalController(Client);
            AGameStateBase* GameState = Client ? Client->GetGameState() : nullptr;
            if (!Owner || !Owner->PlayerState || !GameState || GameState->PlayerArray.Num() != 2) return false;
            ClientOwner = Owner;
            Online = Client->GetGameInstance()->GetSubsystem<UCasinoOnlineSubsystem>();
            UClass* SettingsClass = LoadClass<UUserWidget>(nullptr, TEXT("/Game/KCU/WBP_Settings.WBP_Settings_C"));
            if (!Test->TestNotNull(TEXT("Actual settings Blueprint exists"), SettingsClass)
                || !Test->TestNotNull(TEXT("Owning client's online subsystem exists"), Online.Get())) return true;
            OriginalPreferences = PreferenceSnapshot();
            Settings.Reset(CreateWidget<UUserWidget>(Owner, SettingsClass));
            if (!Test->TestNotNull(TEXT("Settings uses the remote client's actual controller"), Settings.Get())) return true;
            Switcher = Cast<UWidgetSwitcher>(Settings->GetWidgetFromName(TEXT("SettingsSwitcher")));
            Voice = Cast<UCasinoVoiceSettingsWidget>(Settings->GetWidgetFromName(TEXT("CasinoVoiceSettingsPanel")));
            Binding = Cast<UCasinoVoiceKeyBindingWidget>(Settings->GetWidgetFromName(TEXT("CasinoVoiceKeyBindingPanel")));
            if (!Test->TestNotNull(TEXT("Existing settings page switcher is preserved"), Switcher.Get())
                || !Test->TestNotNull(TEXT("Actual Blueprint embeds the native voice panel"), Voice.Get())
                || !Test->TestNotNull(TEXT("Actual Blueprint embeds the native talk binding panel"), Binding.Get())) return true;
            Switcher->SetActiveWidgetIndex(0);
            Settings->AddToViewport(2000);
            Settings->ForceLayoutPrepass();
            Test->TestTrue(TEXT("Settings is owned by the remote client"), Settings->GetOwningPlayer() == Owner);
            Test->TestTrue(TEXT("Embedded voice panel retains the remote client owner"), Voice->GetOwningPlayer() == Owner);
            Test->TestNotNull(TEXT("Existing master volume slider remains in the real Blueprint"),
                Cast<USlider>(Settings->GetWidgetFromName(TEXT("Slider_MasterVolume"))));
            Test->TestNotNull(TEXT("Existing mouse sensitivity slider remains in the real Blueprint"),
                Cast<USlider>(Settings->GetWidgetFromName(TEXT("Slider_MouseSensitivity"))));
            Test->TestNotNull(TEXT("Existing mouse Y inversion remains in the real Blueprint"),
                Cast<UCheckBox>(Settings->GetWidgetFromName(TEXT("Check_InvertMouseY"))));
            Stage = 1; StageStarted = Now;
            return false;
        }
        if (!Settings.IsValid() || !Switcher.IsValid() || !Voice.IsValid() || !ClientOwner.IsValid())
        {
            Test->AddError(TEXT("Actual client settings UI was destroyed before its integration checks finished."));
            Cleanup(); return true;
        }
        if (Now - StageStarted < .45) return false;
        if (Stage == 1)
        {
            Test->TestEqual(TEXT("Inactive audio page does not populate a background roster"), ReadParticipantRows(Voice.Get()).Num(), 0);
            Switcher->SetActiveWidgetIndex(2);
            Settings->ForceLayoutPrepass();
            Stage = 2; StageStarted = Now;
            return false;
        }
        if (Stage == 2)
        {
            AGameStateBase* GameState = ClientOwner->GetWorld()->GetGameState();
            const auto Rows = ReadParticipantRows(Voice.Get());
            TMap<FString, FString> Expected;
            for (APlayerState* Player : GameState->PlayerArray)
                if (Player && Player != ClientOwner->PlayerState) Expected.Add(Player->GetPathName(), Player->GetPlayerName());
            if (Rows.Num() != Expected.Num()) return false;
            UScrollBox* List = Cast<UScrollBox>(Voice->GetWidgetFromName(TEXT("VoiceParticipantList")));
            if (!Test->TestNotNull(TEXT("Actual audio page contains the participant scroll box"), List)) return true;
            Test->TestEqual(TEXT("Two-player settings includes exactly one remote participant"), Rows.Num(), 1);
            Test->TestFalse(TEXT("Roster excludes the owning player's own state"), Rows.Contains(ClientOwner->PlayerState->GetPathName()));
            for (const auto& Entry : Expected)
            {
                UUserWidget* Row = Rows.FindRef(Entry.Key).Get();
                if (!Test->TestNotNull(TEXT("Remote PlayerState has a stable identity-keyed row"), Row)) return true;
                Test->TestTrue(TEXT("Remote participant row belongs to the listening client"), Row->GetOwningPlayer() == ClientOwner.Get());
                Test->TestTrue(TEXT("Participant row is attached to the actual scroll box"), Row->GetParent() == List);
                UTextBlock* Name = Cast<UTextBlock>(Row->GetWidgetFromName(TEXT("VoicePlayerName")));
                if (!Test->TestNotNull(TEXT("Participant row has its real display name"), Name)) return true;
                Test->TestEqual(TEXT("Participant row displays the replicated remote player's name"), Name->GetText().ToString(), Entry.Value);
                if (!Online->IsVoiceConnected())
                {
                    USlider* Volume = Cast<USlider>(Row->GetWidgetFromName(TEXT("VoicePlayerVolume")));
                    UCheckBox* Mute = Cast<UCheckBox>(Row->GetWidgetFromName(TEXT("VoicePlayerMute")));
                    if (!Test->TestNotNull(TEXT("Actual participant volume control exists"), Volume)
                        || !Test->TestNotNull(TEXT("Actual participant mute control exists"), Mute)) return true;
                    Test->TestFalse(TEXT("Disconnected participant volume waits for voice connection"), Volume->GetIsEnabled());
                    Test->TestFalse(TEXT("Disconnected participant mute waits for voice connection"), Mute->GetIsEnabled());
                }
            }
            OriginalRows = Rows;
            Stage = 3; StageStarted = Now;
            return false;
        }
        if (Stage == 3)
        {
            const auto CurrentRows = ReadParticipantRows(Voice.Get());
            Test->TestEqual(TEXT("Repeated timer refresh retains the remote roster size"), CurrentRows.Num(), OriginalRows.Num());
            for (const auto& Entry : OriginalRows)
                Test->TestTrue(TEXT("Repeated timer refresh preserves each participant row instance"),
                    CurrentRows.FindRef(Entry.Key).Get() == Entry.Value.Get());
            Switcher->SetActiveWidgetIndex(3);
            Settings->ForceLayoutPrepass();
            Stage = 4; StageStarted = Now;
            return false;
        }
        if (Stage == 4)
        {
            UInputKeySelector* Selector = Binding.IsValid()
                ? Cast<UInputKeySelector>(Binding->GetWidgetFromName(TEXT("PushToTalkKeySelector"))) : nullptr;
            if (!Test->TestNotNull(TEXT("Actual controls page contains its talk key selector"), Selector)) return true;
            Test->TestEqual(TEXT("Talk key selector displays the currently saved valid binding"),
                Selector->GetSelectedKey().Key, Online->GetPushToTalkKey());
            Test->TestFalse(TEXT("Simply viewing controls does not capture keys"), Online->IsVoiceKeyCaptureActive());
            Test->TestTrue(TEXT("Reading audio and controls leaves every voice preference unchanged"),
                PreferenceSnapshot() == OriginalPreferences);
            Settings->RemoveFromParent();
            Stage = 5; StageStarted = Now;
            return false;
        }
        Test->TestFalse(TEXT("Removed test settings is no longer attached to the viewport"), Settings->IsInViewport());
        Test->TestTrue(TEXT("Removing the real settings panel clears retained participant rows"), ReadParticipantRows(Voice.Get()).IsEmpty());
        Test->TestFalse(TEXT("Removing settings leaves key capture released"), Online->IsVoiceKeyCaptureActive());
        Test->TestTrue(TEXT("UI teardown leaves every voice preference unchanged"), PreferenceSnapshot() == OriginalPreferences);
        Cleanup();
        return true;
    }
private:
    void Cleanup()
    {
        if (Settings.IsValid()) Settings->RemoveFromParent();
        Settings.Reset();
    }
    FAutomationTestBase* Test;
    TStrongObjectPtr<UUserWidget> Settings;
    TWeakObjectPtr<UWidgetSwitcher> Switcher;
    TWeakObjectPtr<UCasinoVoiceSettingsWidget> Voice;
    TWeakObjectPtr<UCasinoVoiceKeyBindingWidget> Binding;
    TWeakObjectPtr<APlayerController> ClientOwner;
    TWeakObjectPtr<UCasinoOnlineSubsystem> Online;
    TMap<FString, TWeakObjectPtr<UUserWidget>> OriginalRows;
    FString OriginalPreferences;
    double Started = 0., StageStarted = 0.;
    int32 Stage = 0;
};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoVoiceSettingsNetworkPIETest, "Casino.Voice.Settings.NetworkPIE",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCasinoVoiceSettingsNetworkPIETest::RunTest(const FString& Parameters)
{
    return CasinoMatchLoadingPIE::QueueTravel(this, 2, false, MakeShared<CasinoVoiceSettingsPIE::FActualSettingsUI>(this));
}

#endif
