#pragma once

#include "CoreMinimal.h"
#include "Enemy/EnemyBaseCharacter.h"
#include "ThiefCharacter.generated.h"


class Acasino_simulatorCharacter;

UENUM(BlueprintType)
enum class EThiefState : uint8
{
    Roaming  UMETA(DisplayName = "¼øÂû"),
    Escaping UMETA(DisplayName = "µµÁÖ")
};

UCLASS()
class CASINO_SIMULATOR_API AThiefCharacter : public AEnemyBaseCharacter
{
    GENERATED_BODY()

public:
    AThiefCharacter();

    UFUNCTION(BlueprintPure, Category = "Enemy|Thief")
    bool CanSteal() const;

    bool TryStealFrom(Acasino_simulatorCharacter* Player,
        float RequestedAmount);

    virtual void HandleEnemyHitReaction(AActor* Attacker) override;

    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
    
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly,
        Category = "Enemy|Thief", meta = (ClampMin = "0.0"))
    float EscapeSpeed = 500.0f;


    UPROPERTY(Replicated, VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Enemy|Thief")
    EThiefState ThiefState = EThiefState::Roaming;

 
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Enemy|Thief")
    float StolenMoney = 0.0f;

    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly,
        Category = "Enemy|Thief")
    TObjectPtr<AActor> EscapeFromActor = nullptr;
};