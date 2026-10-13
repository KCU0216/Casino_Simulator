#include "Online/CasinoRoomPassword.h"
#include "IPlatformCrypto.h"
#include "Misc/SecureHash.h"

bool CasinoRoomPassword::IsHexToken(const FString& Value, int32 Length)
{
    if (Value.Len() != Length) return false;
    for (TCHAR Character : Value)
        if (!((Character >= TEXT('0') && Character <= TEXT('9')) ||
            (Character >= TEXT('a') && Character <= TEXT('f')))) return false;
    return true;
}

bool CasinoRoomPassword::DeriveKey(const FString& RoomSalt, const FString& Password, TArray<uint8>& OutKey)
{
    OutKey.Reset();
    if (!IsHexToken(RoomSalt, 32) || Password.IsEmpty() || Password.Len() > MaxPasswordLength) return false;
    const FString Material = TEXT("CasinoRoomPassword:v1\n") + RoomSalt + TEXT("\n") + Password;
    const FTCHARToUTF8 Bytes(*Material);
    TUniquePtr<FEncryptionContext> Crypto = IPlatformCrypto::Get().CreateContext();
    return Crypto && Crypto->CalcSHA256(MakeArrayView(reinterpret_cast<const uint8*>(Bytes.Get()), Bytes.Length()), OutKey)
        && OutKey.Num() == 32;
}

FString CasinoRoomPassword::MakeProof(TConstArrayView<uint8> Key, const FString& RoomSalt,
    const FString& UniqueId, const FString& Nonce)
{
    if (Key.Num() != 32 || !IsHexToken(RoomSalt, 32) || !IsHexToken(Nonce, 32) || UniqueId.IsEmpty()) return FString();
    const FString Material = TEXT("CasinoRoomAdmission:v1\n") + RoomSalt + TEXT("\n") + UniqueId + TEXT("\n") + Nonce;
    const FTCHARToUTF8 Bytes(*Material);
    uint8 Hash[20];
    FSHA1::HMACBuffer(Key.GetData(), Key.Num(), Bytes.Get(), Bytes.Length(), Hash);
    return BytesToHexLower(Hash, UE_ARRAY_COUNT(Hash));
}

bool CasinoRoomPassword::ConsumeProof(TConstArrayView<uint8> Key, const FString& RoomSalt,
    const FString& UniqueId, const FString& Nonce, const FString& SuppliedProof, TSet<FString>& AcceptedNonces)
{
    if (!IsHexToken(SuppliedProof, 40) || !IsHexToken(Nonce, 32) || AcceptedNonces.Contains(Nonce)) return false;
    const FString ExpectedProof = MakeProof(Key, RoomSalt, UniqueId, Nonce);
    if (ExpectedProof.Len() != 40) return false;
    uint32 Difference = 0;
    for (int32 Index = 0; Index < 40; ++Index) Difference |= ExpectedProof[Index] ^ SuppliedProof[Index];
    if (Difference != 0) return false;
    AcceptedNonces.Add(Nonce);
    return true;
}
