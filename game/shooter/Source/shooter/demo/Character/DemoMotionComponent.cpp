#include "demo/Character/DemoMotionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "GameFramework/Actor.h"

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
void UDemoMotionComponent::Update(float Dt, FVector Velocity, FRotator Heading, bool bGrounded, float Sprint, float Aim, bool bCrouched, bool bSliding)
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
    DemoMotion::Spring(ShotRotation, ShotVelocity, FVector2D::ZeroVector, 28.f, Dt);
    SlideAlpha = FMath::Lerp(SlideAlpha, bSliding ? 1.f : 0.f, 1.f - FMath::Exp(-12.f * Dt));
    if (bSliding && !bWasSliding)
    {
        StepDistance = 0.f;
        if (SlideStartSound) UGameplayStatics::PlaySound2D(this, SlideStartSound, 0.38f);
        if (SlideLoopSound)
        {
            // Start silent: the previous loop played at full volume for a frame.
            SlideAudio = UGameplayStatics::SpawnSound2D(this, SlideLoopSound, 0.f);
            if (SlideAudio) SlideAudio->FadeIn(0.10f, 0.28f);
        }
    }
    if (!bSliding && SlideAudio) { SlideAudio->FadeOut(0.12f, 0.f); SlideAudio = nullptr; }
    bWasSliding = bSliding;
    const float Speed = Velocity.Size2D();
    const float Stride = FMath::Lerp(WalkingStride, SprintStride, Sprint);
    const float Distance = bGrounded && !bSliding ? Speed * Dt : 0.f;
    if (SlideAudio) { SlideAudio->SetVolumeMultiplier(FMath::Clamp(Speed / 2600.f, 0.f, 0.38f)); SlideAudio->SetPitchMultiplier(FMath::Clamp(Speed / 1400.f + 0.4f, 0.8f, 1.12f)); }
    GaitDistance += Distance / Stride;
    StepDistance += Distance;
    const float Moving = bGrounded && !bSliding ? FMath::Clamp(Speed / 420.f, 0.f, 1.5f) : 0.f;
    GaitAmplitude = FMath::Lerp(GaitAmplitude, Moving * (bCrouched ? 0.4f : 1.f), 1.f - FMath::Exp(-9.f * Dt));
    const float Phase = GaitDistance * PI;
    const float Stable = 1.f - Aim;
    const float BobSize = FMath::Lerp(0.7f, 1.6f, Sprint) * GaitAmplitude;
    const float Side = FMath::Sin(Phase) * 0.45f * GaitAmplitude;
    // Smooth plant/rebound; abs(sin) had a velocity cusp at every footfall.
    const float Down = -(0.5f - 0.5f * FMath::Cos(2.f * Phase)) * BobSize;
    const float RollTarget = FMath::Clamp(-LocalVelocity.Y * 0.0035f - Acceleration.Y * 0.00015f, -2.2f, 2.2f);
    Roll = FMath::Lerp(Roll, RollTarget, 1.f - FMath::Exp(-10.f * Dt));
    CameraOffset = FVector(-Inertia.X * 0.08f, Side * 0.45f * Stable, Down * Stable + Landing * FMath::Lerp(1.f, 0.22f, Aim)) * Intensity;
    CameraRotation = FRotator(FMath::Sin(2 * Phase) * 0.22f * GaitAmplitude + Inertia.X * 0.07f,
        0, Roll + FMath::Sin(Phase) * 0.35f * GaitAmplitude) * (Stable * Intensity);
    CameraOffset.Z -= SlideAlpha * 1.5f * Stable * Intensity;
    CameraRotation += FRotator(ShotRotation.X, 0, ShotRotation.Y - 2.2f * SlideAlpha * Stable) * Intensity;
    WeaponOffset = (Inertia + FVector(0, Side * 1.7f, Down * 0.7f + Landing * 0.3f)) * (FMath::Lerp(1.f, 0.08f, Aim) * Intensity);
    WeaponRotation = FRotator(-Inertia.X * 0.65f + FMath::Sin(2 * Phase) * GaitAmplitude * 0.7f,
        Inertia.Y * 0.55f, Roll * 1.1f) * (FMath::Lerp(1.f, 0.f, Aim) * Intensity);
    if (StepDistance >= Stride && bGrounded && !bSliding && Speed > 60)
    {
        StepDistance = FMath::Fmod(StepDistance, Stride);
        bRightFoot = !bRightFoot;
        // No immediately repeated sample, and no obvious four-step loop.
        if (FootstepVariations.Num() > 1)
            StepIndex = (StepIndex + FMath::RandRange(1, FootstepVariations.Num() - 1)) % FootstepVariations.Num();
        USoundBase* Step = FootstepVariations.IsEmpty() ? FootstepSound.Get() : FootstepVariations[StepIndex].Get();
        if (Step) UGameplayStatics::PlaySound2D(this, Step,
            FMath::Lerp(0.25f, 0.48f, Sprint) * (bCrouched ? 0.36f : 1.f) * FMath::FRandRange(0.93f, 1.f),
            FMath::FRandRange(0.97f, 1.03f));
    }
    if (Speed < 30) StepDistance = Stride * 0.55f;
}
void UDemoMotionComponent::Shot(float Direction, float Aim)
{
    ShotVelocity += FVector2D(15.f, FMath::Clamp(Direction * 35.f, -5.f, 5.f)) * FMath::Lerp(1.f, 0.28f, Aim);
}
void UDemoMotionComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (SlideAudio) SlideAudio->Stop();
    Super::EndPlay(Reason);
}
void UDemoMotionComponent::Land(float VerticalSpeed)
{
    const float Strength = FMath::Clamp(FMath::Abs(VerticalSpeed) / 700.f, 0.1f, 1.5f);
    LandingVelocity -= 130.f * Strength;
    if (LandingSound) UGameplayStatics::PlaySound2D(this, LandingSound, FMath::Clamp(Strength * 0.5f, 0.1f, 0.8f));
}
