#if WITH_DEV_AUTOMATION_TESTS
#include "Online/CasinoRoomPassword.h"
#include "Online/CasinoOnlineSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoRoomPasswordProofTest, "Casino.Online.RoomPassword.Proof",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoRoomPasswordProofTest::RunTest(const FString& Parameters)
{
    const FString Salt(TEXT("0123456789abcdef0123456789abcdef"));
    const FString Nonce(TEXT("fedcba9876543210fedcba9876543210"));
    const FString Id(TEXT("epic|product"));
    TArray<uint8> Key;
    if (!TestTrue(TEXT("Derives a room key"), CasinoRoomPassword::DeriveKey(Salt, TEXT("known-pass"), Key))) return false;
    TestEqual(TEXT("SHA256 matches an independently calculated vector"), BytesToHexLower(Key.GetData(), Key.Num()),
        FString(TEXT("304bffb6a7a49829288dd15449fb5aa1329ee98353fa94bb51c02f456398f69e")));
    const FString Proof = CasinoRoomPassword::MakeProof(Key, Salt, Id, Nonce);
    TestEqual(TEXT("HMAC matches an independently calculated vector"), Proof,
        FString(TEXT("450986418c6136db38cbb9f2d8218eaf2283e312")));
    TSet<FString> Accepted;
    TestFalse(TEXT("Another EOS identity cannot use the proof"),
        CasinoRoomPassword::ConsumeProof(Key, Salt, TEXT("other-player"), Nonce, Proof, Accepted));
    TestFalse(TEXT("Wrong nonce cannot use the proof"), CasinoRoomPassword::ConsumeProof(Key, Salt, Id,
        TEXT("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"), Proof, Accepted));
    TArray<uint8> WrongKey;
    CasinoRoomPassword::DeriveKey(Salt, TEXT("wrong-password"), WrongKey);
    TestFalse(TEXT("Wrong password rejected without consuming retry nonce"), CasinoRoomPassword::ConsumeProof(Key, Salt, Id,
        Nonce, CasinoRoomPassword::MakeProof(WrongKey, Salt, Id, Nonce), Accepted));
    TestEqual(TEXT("Failed attempts retain the retry opportunity"), Accepted.Num(), 0);
    TestTrue(TEXT("Correct password accepted"), CasinoRoomPassword::ConsumeProof(Key, Salt, Id, Nonce, Proof, Accepted));
    TestFalse(TEXT("Captured proof replay rejected"), CasinoRoomPassword::ConsumeProof(Key, Salt, Id, Nonce, Proof, Accepted));
    TestFalse(TEXT("Missing credentials rejected"), CasinoRoomPassword::ConsumeProof(Key, Salt, Id, FString(), FString(), Accepted));
    TestFalse(TEXT("Invalid published salt fails closed"), CasinoRoomPassword::DeriveKey(TEXT("?bad&salt"), TEXT("password"), WrongKey));
    TestFalse(TEXT("Oversized password rejected"), CasinoRoomPassword::DeriveKey(Salt, FString::ChrN(65, TEXT('x')), WrongKey));

    const FString UnicodePassword(TEXT("\uBE44\uBC00 ?&=+#/ \U0001F3B2"));
    TArray<uint8> UnicodeKey;
    TestTrue(TEXT("Unicode and URL punctuation are valid password characters"),
        CasinoRoomPassword::DeriveKey(Salt, UnicodePassword, UnicodeKey));
    const FString UnicodeProof = CasinoRoomPassword::MakeProof(UnicodeKey, Salt, Id, Nonce);
    const FString Options = TEXT("?CasinoRoomNonce=") + Nonce + TEXT("?CasinoRoomProof=") + UnicodeProof;
    TestEqual(TEXT("Travel options preserve nonce"), UGameplayStatics::ParseOption(Options, TEXT("CasinoRoomNonce")), Nonce);
    TestEqual(TEXT("Travel options preserve proof"), UGameplayStatics::ParseOption(Options, TEXT("CasinoRoomProof")), UnicodeProof);
    TestFalse(TEXT("Travel options do not contain the password"), Options.Contains(UnicodePassword));
    TSet<FString> UnicodeAccepted;
    TestTrue(TEXT("Unicode password proof accepted after travel-option parsing"), CasinoRoomPassword::ConsumeProof(UnicodeKey, Salt, Id,
        UGameplayStatics::ParseOption(Options, TEXT("CasinoRoomNonce")),
        UGameplayStatics::ParseOption(Options, TEXT("CasinoRoomProof")), UnicodeAccepted));
    TArray<uint8> TrimmedKey;
    CasinoRoomPassword::DeriveKey(Salt, TEXT(" password "), TrimmedKey);
    CasinoRoomPassword::DeriveKey(Salt, TEXT("password"), WrongKey);
    TestFalse(TEXT("Password whitespace is significant"), TrimmedKey == WrongKey);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCasinoRoomPasswordStateTest, "Casino.Online.RoomPassword.State",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCasinoRoomPasswordStateTest::RunTest(const FString& Parameters)
{
    auto* Online = NewObject<UCasinoOnlineSubsystem>();
    TestTrue(TEXT("Public room needs no password"), Online->ValidateRoomJoinOptions(FString(), TEXT("player")));
    Online->HostRoomSalt = TEXT("0123456789abcdef0123456789abcdef");
    CasinoRoomPassword::DeriveKey(Online->HostRoomSalt, TEXT("private-password"), Online->HostPasswordKey);
    const FString Id(TEXT("test-player"));
    const FString Nonce(TEXT("fedcba9876543210fedcba9876543210"));
    const FString Proof = CasinoRoomPassword::MakeProof(Online->HostPasswordKey, Online->HostRoomSalt, Id, Nonce);
    const FString Options = TEXT("?CasinoRoomNonce=") + Nonce + TEXT("?CasinoRoomProof=") + Proof;
    TestFalse(TEXT("Protected server denies absent credentials"), Online->ValidateRoomJoinOptions(FString(), Id));
    TestFalse(TEXT("Protected server denies spoofed identity"), Online->ValidateRoomJoinOptions(Options, TEXT("other-player")));
    TestTrue(TEXT("Server verifies parsed credentials against connecting identity"), Online->ValidateRoomJoinOptions(Options, Id));
    TestFalse(TEXT("Server denies nonce replay"), Online->ValidateRoomJoinOptions(Options, Id));

    Online->PendingJoinSessionId = TEXT("stable-room-id");
    Online->PendingJoinPassword = TEXT("temporary-password");
    Online->PendingJoinNonce = Nonce;
    Online->PendingJoinProof = Proof;
    Online->bRoomAdmissionAccepted = true;
    Online->LastError = TEXT("Incorrect room password. Please try again.");
    Online->ClearPendingJoinCredentials();
    TestTrue(TEXT("Clear pending target after failure/acceptance"), Online->PendingJoinSessionId.IsEmpty());
    TestTrue(TEXT("Clear temporary password"), Online->PendingJoinPassword.IsEmpty());
    TestTrue(TEXT("Clear pending nonce and proof"), Online->PendingJoinNonce.IsEmpty() && Online->PendingJoinProof.IsEmpty());
    TestEqual(TEXT("Pending cleanup preserves host credentials"), Online->HostPasswordKey.Num(), 32);
    TestEqual(TEXT("Pending cleanup preserves replay protection"), Online->AcceptedJoinNonces.Num(), 1);
    Online->ClearRoomCredentials();
    TestTrue(TEXT("Room cleanup clears host secret/salt/replay state"),
        Online->HostPasswordKey.IsEmpty() && Online->HostRoomSalt.IsEmpty() && Online->AcceptedJoinNonces.IsEmpty());
    TestFalse(TEXT("Room cleanup revokes voice admission"), Online->bRoomAdmissionAccepted);
    TestEqual(TEXT("Rejection survives cleanup for newly created menu UI"), Online->LastError,
        FString(TEXT("Incorrect room password. Please try again.")));
    return true;
}
#endif
