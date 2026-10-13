#pragma once

#include "CoreMinimal.h"

// Prototype game-entry credential. EOS lobby membership is a separate boundary.
// Only the protection flag and fresh room salt are published; never this key/proof.
namespace CasinoRoomPassword
{
    constexpr int32 MaxPasswordLength = 64;
    bool IsHexToken(const FString& Value, int32 Length);
    bool DeriveKey(const FString& RoomSalt, const FString& Password, TArray<uint8>& OutKey);
    FString MakeProof(TConstArrayView<uint8> Key, const FString& RoomSalt,
        const FString& UniqueId, const FString& Nonce);
    bool ConsumeProof(TConstArrayView<uint8> Key, const FString& RoomSalt,
        const FString& UniqueId, const FString& Nonce, const FString& SuppliedProof,
        TSet<FString>& AcceptedNonces);
}
