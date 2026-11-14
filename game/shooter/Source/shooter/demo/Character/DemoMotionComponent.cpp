#include "demo/Character/DemoMotionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

namespace DemoMotion
{
    template<typename T> void Spring(T& Value, T& Velocity, T Target, float Response, float Dt)
    {
        const T Offset = Value - Target;
        const T Change = Velocity + Offset * Response;
        const float Decay = FMath::Exp(-Response * Dt);
        Value = Target + (Offset + Change * Dt) * Decay;
        Velocity = (Velocity - Change * Response * Dt) * Decay;
    }
}
UDemoMotionComponent::UDemoMotionComponent() { PrimaryComponentTick.bCanEverTick = false; }
void UDemoMotionComponent::Update(float Dt, FVector Velocity, FRotator Heading, bool bGrounded, float Sprint, float Aim, bool bCrouched)
{
    if (Dt <= SMALL_NUMBER) return;
    const FQuat Yaw = FRotator(0, Heading.Yaw, 0).Quaternion();
    const FVector LocalVelocity = Yaw.UnrotateVector(Velocity);
    const FVector RawAcceleration = Yaw.UnrotateVector((Velocity - LastVelocity) / FMath::Max(Dt, 0.001f));
    LastVelocity = Velocity;
    Acceleration = FMath::Lerp(Acceleration, RawAcceleration.GetClampedToMaxSize(4500), 1.f - FMath::Exp(-18.f * Dt));
    const FVector Target(-Acceleration.X * 0.0013f, -Acceleration.Y * 0.0017f, -Acceleration.Z * 0.00025f);
    DemoMotion::Spring(Inertia, InertiaVelocity, Target.GetClampedToMaxSize(4.f), 13.f, Dt);
    DemoMotion::Spring(Landing, LandingVelocity, 0.f, 13.f, Dt);
    const float Speed = Velocity.Size2D();
    const float Stride = FMath::Lerp(WalkingStride, SprintStride, Sprint);
    const float Distance = bGrounded ? Speed * Dt : 0.f;
    GaitDistance += Distance / Stride;
    StepDistance += Distance;
    const float Moving = bGrounded ? FMath::Clamp(Speed / 420.f, 0.f, 1.5f) : 0.f;
    GaitAmplitude = FMath::Lerp(GaitAmplitude, Moving * (bCrouched ? 0.4f : 1.f), 1.f - FMath::Exp(-9.f * Dt));
    const float Phase = GaitDistance * PI;
    const float Stable = 1.f - Aim;
    const float BobSize = FMath::Lerp(0.85f, 2.15f, Sprint) * GaitAmplitude;
    const float Side = FMath::Sin(Phase) * 0.65f * GaitAmplitude;
    const float Down = -FMath::Abs(FMath::Sin(Phase)) * BobSize;
    const float RollTarget = FMath::Clamp(-LocalVelocity.Y * 0.0045f - Acceleration.Y * 0.00018f, -2.4f, 2.4f);
    Roll = FMath::Lerp(Roll, RollTarget, 1.f - FMath::Exp(-10.f * Dt));
    CameraOffset = FVector(-Inertia.X * 0.08f, Side * 0.45f * Stable, Down * Stable + Landing * FMath::Lerp(1.f, 0.22f, Aim)) * Intensity;
    CameraRotation = FRotator(FMath::Sin(2 * Phase) * 0.22f * GaitAmplitude + Inertia.X * 0.07f,
        0, Roll + FMath::Sin(Phase) * 0.35f * GaitAmplitude) * (Stable * Intensity);
    WeaponOffset = (Inertia + FVector(0, Side * 1.7f, Down * 0.7f + Landing * 0.3f)) * (FMath::Lerp(1.f, 0.08f, Aim) * Intensity);
    WeaponRotation = FRotator(-Inertia.X * 0.65f + FMath::Sin(2 * Phase) * GaitAmplitude * 0.7f,
        Inertia.Y * 0.55f, Roll * 1.1f) * (FMath::Lerp(1.f, 0.f, Aim) * Intensity);
    if (StepDistance >= Stride && bGrounded && Speed > 60)
    {
        StepDistance = FMath::Fmod(StepDistance, Stride);
        bRightFoot = !bRightFoot;
        if (FootstepSound) UGameplayStatics::PlaySound2D(this, FootstepSound, FMath::Lerp(0.28f, 0.65f, Sprint) * (bCrouched ? 0.4f : 1.f), bRightFoot ? 0.97f : 1.03f);
    }
    if (Speed < 30) StepDistance = Stride * 0.55f;
}
void UDemoMotionComponent::Land(float VerticalSpeed)
{
    const float Strength = FMath::Clamp(FMath::Abs(VerticalSpeed) / 700.f, 0.1f, 1.5f);
    LandingVelocity -= 130.f * Strength;
    if (LandingSound) UGameplayStatics::PlaySound2D(this, LandingSound, FMath::Clamp(Strength * 0.5f, 0.1f, 0.8f));
}
