#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DemoMotionComponent.generated.h"

class USoundBase;

/** Distance-driven gait, acceleration inertia and landing spring. No random shake. */
UCLASS(ClassGroup=Demo, meta=(BlueprintSpawnableComponent))
class SHOOTER_API UDemoMotionComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UDemoMotionComponent();
    void Update(float Dt, FVector Velocity, FRotator Heading, bool bGrounded, float Sprint, float Aim, bool bCrouched, bool bSliding = false);
    void Land(float VerticalSpeed);
    void Shot(float Direction, float Aim);
    float GetSlideAlpha() const { return SlideAlpha; }
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    FVector CameraOffset = FVector::ZeroVector;
    FRotator CameraRotation = FRotator::ZeroRotator;
    FVector WeaponOffset = FVector::ZeroVector;
    FRotator WeaponRotation = FRotator::ZeroRotator;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo|Motion", meta=(ClampMin="0", ClampMax="2")) float Intensity = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo|Motion") TObjectPtr<USoundBase> FootstepSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo|Motion") TObjectPtr<USoundBase> LandingSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo|Motion") TArray<TObjectPtr<USoundBase>> FootstepVariations;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo|Motion") TObjectPtr<USoundBase> SlideLoopSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo|Motion") TObjectPtr<USoundBase> SlideStartSound;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo|Motion", meta=(ClampMin="50")) float WalkingStride = 155.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Demo|Motion", meta=(ClampMin="50")) float SprintStride = 185.f;
private:
    FVector LastVelocity = FVector::ZeroVector;
    FVector Acceleration = FVector::ZeroVector;
    FVector Inertia = FVector::ZeroVector;
    FVector InertiaVelocity = FVector::ZeroVector;
    float GaitDistance = 0;
    float GaitAmplitude = 0;
    float StepDistance = 0;
    float Landing = 0;
    float LandingVelocity = 0;
    float Roll = 0;
    bool bRightFoot = false;
    bool bWasSliding = false;
    int32 StepIndex = 0;
    float SlideAlpha = 0;
    FVector2D ShotRotation = FVector2D::ZeroVector;
    FVector2D ShotVelocity = FVector2D::ZeroVector;
    UPROPERTY(Transient) TObjectPtr<class UAudioComponent> SlideAudio;
};
