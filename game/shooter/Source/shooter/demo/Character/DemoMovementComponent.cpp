#include "demo/Character/DemoMovementComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

bool UDemoMovementComponent::TryStartSlide(bool bFromLanding)
{
    if (!CharacterOwner || bSliding || !IsMovingOnGround() || (CharacterOwner->bIsCrouched && !bFromLanding)
        || GetWorld()->GetTimeSeconds() < NextSlideTime || Velocity.Size2D() < MinimumSlideSpeed) return false;
    bSliding = true;
    SlideSeconds = 0;
    Velocity = Velocity.GetSafeNormal2D() * FMath::Min(Velocity.Size2D() + SlideEntryBoost, SlideMaximumSpeed);
    bWantsToCrouch = true;
    return true;
}
void UDemoMovementComponent::EndSlide(bool bJumping)
{
    if (!bSliding) return;
    if (bJumping) AirborneMomentumSpeed = Velocity.Size2D();
    bSliding = false;
    NextSlideTime = GetWorld()->GetTimeSeconds() + SlideCooldown;
}
float UDemoMovementComponent::GetMaxSpeed() const
{
    if (bSliding) return SlideMaximumSpeed;
    if (IsFalling()) return FMath::Max(Super::GetMaxSpeed(), AirborneMomentumSpeed);
    return Super::GetMaxSpeed();
}
float UDemoMovementComponent::GetMaxAcceleration() const { return bSliding ? SlideSteeringAcceleration : Super::GetMaxAcceleration(); }
float UDemoMovementComponent::GetMaxBrakingDeceleration() const { return bSliding ? SlideBraking : Super::GetMaxBrakingDeceleration(); }
void UDemoMovementComponent::CalcVelocity(float Dt, float Friction, bool bFluid, float BrakingDeceleration)
{
    if (!bSliding) { Super::CalcVelocity(Dt, Friction, bFluid, BrakingDeceleration); return; }
    // Scoped settings also cover braking without input; no persistent mutation
    // leaks from slide into normal walking, landing or a jump.
    const bool bPreviousSeparateFriction = bUseSeparateBrakingFriction;
    bUseSeparateBrakingFriction = false;
    TGuardValue<float> BrakingFactor(BrakingFrictionFactor, 1.f);
    const FVector InputAcceleration = Acceleration;
    {
        TGuardValue<FVector> MomentumOnly(Acceleration, FVector::ZeroVector);
        Super::CalcVelocity(Dt, SlideFriction, false, SlideBraking);
    }
    const float Speed = Velocity.Size2D();
    const FVector Steering = FVector::VectorPlaneProject(InputAcceleration, Velocity.GetSafeNormal2D());
    Velocity = (Velocity + Steering * Dt).GetSafeNormal2D() * Speed;
    bUseSeparateBrakingFriction = bPreviousSeparateFriction;
}
void UDemoMovementComponent::PhysWalking(float Dt, int32 Iterations)
{
    if (bSliding)
    {
        SlideSeconds += Dt;
        if (!bWantsToCrouch || SlideSeconds > SlideMaximumSeconds || Velocity.Size2D() < 300.f) EndSlide();
        else if (CurrentFloor.IsWalkableFloor())
        {
            const FVector Downhill = FVector::VectorPlaneProject(FVector(0, 0, GetGravityZ()), CurrentFloor.HitResult.ImpactNormal);
            Velocity += FVector(Downhill.X, Downhill.Y, 0) * Dt;
        }
    }
    Super::PhysWalking(Dt, Iterations);
}
void UDemoMovementComponent::OnMovementModeChanged(EMovementMode PreviousMode, uint8 PreviousCustomMode)
{
    if (IsFalling())
    {
        AirborneMomentumSpeed = FMath::Max(AirborneMomentumSpeed, Velocity.Size2D());
        EndSlide(true);
    }
    if (IsMovingOnGround() && PreviousMode == MOVE_Falling)
    {
        AirborneMomentumSpeed = 0;
        if (bWantsToCrouch) TryStartSlide(true);
    }
    Super::OnMovementModeChanged(PreviousMode, PreviousCustomMode);
}
