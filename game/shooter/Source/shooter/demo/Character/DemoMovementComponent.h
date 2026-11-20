#pragma once
#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DemoMovementComponent.generated.h"

/** Grounded momentum slide. Uses CharacterMovement collision, floors and steps. */
UCLASS()
class SHOOTER_API UDemoMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()
public:
    bool TryStartSlide(bool bFromLanding = false);
    void EndSlide(bool bJumping = false);
    bool IsSliding() const { return bSliding; }
    virtual float GetMaxSpeed() const override;
    virtual float GetMaxAcceleration() const override;
    virtual float GetMaxBrakingDeceleration() const override;
    virtual void CalcVelocity(float Dt, float Friction, bool bFluid, float BrakingDeceleration) override;
    virtual void PhysWalking(float Dt, int32 Iterations) override;
    virtual void OnMovementModeChanged(EMovementMode PreviousMode, uint8 PreviousCustomMode) override;
    UPROPERTY(EditAnywhere, Category="Demo|Slide") float MinimumSlideSpeed = 520.f;
    UPROPERTY(EditAnywhere, Category="Demo|Slide") float SlideEntryBoost = 90.f;
    UPROPERTY(EditAnywhere, Category="Demo|Slide") float SlideMaximumSpeed = 1050.f;
    UPROPERTY(EditAnywhere, Category="Demo|Slide") float SlideFriction = 0.25f;
    UPROPERTY(EditAnywhere, Category="Demo|Slide") float SlideBraking = 155.f;
    UPROPERTY(EditAnywhere, Category="Demo|Slide") float SlideSteeringAcceleration = 320.f;
    UPROPERTY(EditAnywhere, Category="Demo|Slide") float SlideMaximumSeconds = 1.5f;
    UPROPERTY(EditAnywhere, Category="Demo|Slide") float SlideCooldown = 0.35f;
private:
    bool bSliding = false;
    float SlideSeconds = 0.f;
    float AirborneMomentumSpeed = 0.f;
    double NextSlideTime = 0;
};
