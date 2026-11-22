#include "demo/Character/DemoMovementComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#include "demo/Character/DemoGrapplePhysics.h"

bool UDemoMovementComponent::TryStartSlide(bool bFromLanding)
{
    if (!CharacterOwner || bSliding || !IsMovingOnGround() || (CharacterOwner->bIsCrouched && !bFromLanding)
        || GetWorld()->GetTimeSeconds() < NextSlideTime || Velocity.Size2D() < MinimumSlideSpeed) return false;
    bSliding = true;
    SlideSeconds = 0;
    ActiveSlideMaximumSpeed = FMath::Max(SlideMaximumSpeed,float(Velocity.Size2D()));
    Velocity = Velocity.GetSafeNormal2D() * FMath::Min(float(Velocity.Size2D()) + SlideEntryBoost, ActiveSlideMaximumSpeed);
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
    if (bSliding) return ActiveSlideMaximumSpeed;
    if (IsGrappling()) return GrappleMaximumSpeed;
    if (IsFalling()) return FMath::Max(Super::GetMaxSpeed(), AirborneMomentumSpeed);
    return Super::GetMaxSpeed();
}
float UDemoMovementComponent::GetMaxAcceleration() const { return IsGrappling() ? GrappleSteeringAcceleration : bSliding ? SlideSteeringAcceleration : Super::GetMaxAcceleration(); }
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
    if (PreviousMode == MOVE_Custom && PreviousCustomMode == GrappleMode && !IsGrappling()) GrappleAnchor.Reset();
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


bool UDemoMovementComponent::IsFalling() const { return IsGrappling() || Super::IsFalling(); }
FVector UDemoMovementComponent::ConstrainInputAcceleration(const FVector& Input) const
{
    return IsGrappling() ? Input : Super::ConstrainInputAcceleration(Input);
}
FVector UDemoMovementComponent::GetGrappleAnchor() const
{
    return GrappleAnchor.IsValid() ? GrappleAnchor->GetComponentTransform().TransformPosition(GrappleLocalPoint) : FVector::ZeroVector;
}
bool UDemoMovementComponent::StartGrapple(UPrimitiveComponent* Anchor,FVector LocalPoint)
{
    if (!HasValidData() || !IsValid(Anchor) || MovementMode == MOVE_None || IsSwimming()) return false;
    const bool bGrounded = IsMovingOnGround();
    EndSlide(true);
    CharacterOwner->StopJumping();
    CharacterOwner->UnCrouch();
    GrappleAnchor = Anchor; GrappleLocalPoint = LocalPoint;
    PreviousAnchorPosition = GetGrappleAnchor();
    GrappleRopeLength = FVector::Distance(UpdatedComponent->GetComponentLocation(),PreviousAnchorPosition)+12.;
    GrappleAttachTime = GetWorld()->GetTimeSeconds(); bGrappleJumpUsed = false;
    if (bGrounded) Velocity.Z = FMath::Max(Velocity.Z,double(GrappleAttachLift));
    SetMovementMode(MOVE_Custom,GrappleMode);
    return true;
}
void UDemoMovementComponent::EndGrapple()
{
    if (!IsGrappling()) return;
    AirborneMomentumSpeed = FMath::Max(AirborneMomentumSpeed,float(Velocity.Size2D()));
    GrappleAnchor.Reset();
    SetMovementMode(MOVE_Falling);
}
bool UDemoMovementComponent::HandleGrappleJump()
{
    if (!IsGrappling()) return false;
    if (!bGrappleJumpUsed && GetWorld()->GetTimeSeconds()-GrappleAttachTime <= GrappleJumpWindow)
    {
        Velocity.Z = FMath::Max(Velocity.Z,double(JumpZVelocity)*.95);
        bGrappleJumpUsed = true;
    }
    else EndGrapple();
    return true;
}
void UDemoMovementComponent::ApplyWorldOffset(const FVector& Offset,bool bWorldShift)
{
    Super::ApplyWorldOffset(Offset,bWorldShift);
    PreviousAnchorPosition += Offset;
}
void UDemoMovementComponent::PhysCustom(float Dt,int32 Iterations)
{
    if (!IsGrappling()) { Super::PhysCustom(Dt,Iterations); return; }
    if (!HasValidData() || Dt < MIN_TICK_TIME) return;
    if (!GrappleAnchor.IsValid() || !GrappleAnchor->IsQueryCollisionEnabled()
        || (GrappleAnchor->GetOwner() && !GrappleAnchor->GetOwner()->GetActorEnableCollision()))
    { EndGrapple(); StartNewPhysics(Dt,Iterations); return; }
    const FVector Anchor = GetGrappleAnchor();
    if (FVector::DistSquared(Anchor,PreviousAnchorPosition) > FMath::Square(1500.))
    { EndGrapple(); StartNewPhysics(Dt,Iterations); return; }
    const FVector AnchorVelocity = (Anchor-PreviousAnchorPosition)/Dt;
    const FVector AnchorStart = PreviousAnchorPosition;
    PreviousAnchorPosition = Anchor;
    DemoGrapplePhysics::FSettings Settings;
    Settings.ReelSpeed = GrappleReelSpeed; Settings.PullResponse = GrapplePullResponse;
    Settings.MaximumPullAcceleration = GrapplePullAcceleration; Settings.SteeringAcceleration = GrappleSteeringAcceleration;
    Settings.MaximumSpeed = GrappleMaximumSpeed; Settings.MinimumLength = GrappleArrivalDistance;
    const FVector Input = Acceleration/FMath::Max(1.f,GrappleSteeringAcceleration);
    float Remaining = Dt;
    int32 Steps = 0;
    while (Remaining >= MIN_TICK_TIME && IsGrappling() && Steps < 64)
    {
        ++Steps;
        const float Step = Steps == 64 ? Remaining : FMath::Min(Remaining,1.f/120.f);
        Remaining -= Step;
        const FVector StepAnchor = FMath::Lerp(AnchorStart,Anchor,1.-Remaining/Dt);
        const FVector OldPosition = UpdatedComponent->GetComponentLocation();
        if (FVector::DistSquared(OldPosition,StepAnchor) <= FMath::Square(double(GrappleArrivalDistance)))
        { EndGrapple(); StartNewPhysics(Remaining+Step,Iterations); return; }
        const auto Next = DemoGrapplePhysics::Step({OldPosition,Velocity,GrappleRopeLength},StepAnchor,AnchorVelocity,
            Input,FVector(0,0,GetGravityZ()*GrappleGravityMultiplier),Step,Settings);
        Velocity = Next.Velocity;
        const FVector Delta = Next.Position-OldPosition;
        FHitResult Hit;
        SafeMoveUpdatedComponent(Delta,UpdatedComponent->GetComponentQuat(),true,Hit);
        if (!HasValidData() || !IsGrappling()) return;
        if (Hit.bBlockingHit)
        {
            if (Velocity.Z <= 0 && IsValidLandingSpot(UpdatedComponent->GetComponentLocation(),Hit))
            {
                EndGrapple(); ProcessLanded(Hit,Remaining,Iterations); return;
            }
            HandleImpact(Hit,Step,Delta);
            if (!HasValidData() || !IsGrappling()) return;
            SlideAlongSurface(Delta,1.f-Hit.Time,Hit.Normal,Hit,true);
            if (FVector::DotProduct(Velocity,Hit.Normal) < 0) Velocity = FVector::VectorPlaneProject(Velocity,Hit.Normal);
        }
        // A wall can prevent the pure solver's requested move. Reel only the
        // distance actually swept, not progress the capsule failed to make.
        GrappleRopeLength = FMath::Max(Settings.MinimumLength,FMath::Min(GrappleRopeLength,
            FVector::Distance(UpdatedComponent->GetComponentLocation(),StepAnchor)+Settings.Slack));
    }
}
