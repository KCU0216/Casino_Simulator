#if WITH_DEV_AUTOMATION_TESTS
#include "Mining/CartBase.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCartCarryAssistTest, "Casino.Mining.CartCarryAssist",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FCartCarryAssistTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
    ON_SCOPE_EXIT { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); };
    auto* Cart = World->SpawnActor<ACartBase>();
    if (!TestNotNull(TEXT("Cart"), Cart)) return false;

    TestTrue(TEXT("A grounded blocked cart receives upward assistance"),
        Cart->CalculateStepAssistAcceleration(0.f, 0.f) > 980.f);
    TestEqual(TEXT("No upward force at maximum ground clearance"),
        Cart->CalculateStepAssistAcceleration(0.f, 100.f), 0.f);
    TestEqual(TEXT("No downward force over a drop"),
        Cart->CalculateStepAssistAcceleration(-200.f, 0.f), 0.f);
    TestEqual(TEXT("Assistance is independent of world elevation"),
        Cart->CalculateStepAssistAcceleration(500.f, 525.f), Cart->CalculateStepAssistAcceleration(0.f, 25.f));
    TestTrue(TEXT("Assistance never exceeds its acceleration cap"),
        Cart->CalculateStepAssistAcceleration(0.f, -100.f) <= Cart->MaxStepAssistAcceleration);

    Cart->SetActorRotation(FRotator::ZeroRotator);
    TestTrue(TEXT("Upright cart and yaw motion need no tilt torque"),
        Cart->CalculateTiltTorque(FVector(0.f, 0.f, 5.f)).IsNearlyZero());
    const FVector Damping = Cart->CalculateTiltTorque(FVector(1.f, -2.f, 5.f));
    TestTrue(TEXT("Tilt damping opposes pitch and roll without damping yaw"),
        Damping.X < 0.f && Damping.Y > 0.f && Damping.Z == 0.f);
    for (float Yaw : { 0.f, 90.f, 180.f })
    {
        Cart->SetActorRotation(FRotator(30.f, Yaw, 25.f));
        const FVector Torque = Cart->CalculateTiltTorque(FVector::ZeroVector);
        const FVector RestoreAxis = FVector::CrossProduct(Cart->CartMesh->GetUpVector(), FVector::UpVector);
        TestTrue(TEXT("Excess tilt restores upright at every heading"), FVector::DotProduct(Torque, RestoreAxis) > 0.f);
        TestTrue(TEXT("Restoring torque leaves yaw free and stays bounded"),
            FMath::IsNearlyZero(Torque.Z) && Torque.Size() <= Cart->MaxTiltTorque + KINDA_SMALL_NUMBER);
    }
    Cart->SetActorRotation(FRotator(0.f, 0.f, 180.f));
    TestFalse(TEXT("Upside-down cart has a recovery torque"),
        Cart->CalculateTiltTorque(FVector::ZeroVector).IsNearlyZero());
    return true;
}
#endif
