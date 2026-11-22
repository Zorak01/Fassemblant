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
    bool StartGrapple(class UPrimitiveComponent* Anchor,FVector LocalPoint);
    void EndGrapple();
    bool HandleGrappleJump();
    bool IsGrappling() const { return MovementMode == MOVE_Custom && CustomMovementMode == GrappleMode; }
    FVector GetGrappleAnchor() const;
    double GetGrappleRopeLength() const { return GrappleRopeLength; }
    virtual bool IsFalling() const override;
    virtual FVector ConstrainInputAcceleration(const FVector& InputAcceleration) const override;
    virtual void PhysCustom(float Dt,int32 Iterations) override;
    virtual void ApplyWorldOffset(const FVector& Offset,bool bWorldShift) override;
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
    UPROPERTY(EditAnywhere, Category="Demo|Grapple", meta=(ClampMin="100")) float GrappleReelSpeed = 1000.f;
    UPROPERTY(EditAnywhere, Category="Demo|Grapple", meta=(ClampMin="0")) float GrappleSteeringAcceleration = 1800.f;
    UPROPERTY(EditAnywhere, Category="Demo|Grapple", meta=(ClampMin="1")) float GrapplePullResponse = 4.5f;
    UPROPERTY(EditAnywhere, Category="Demo|Grapple", meta=(ClampMin="100")) float GrapplePullAcceleration = 3800.f;
    UPROPERTY(EditAnywhere, Category="Demo|Grapple", meta=(ClampMin="100")) float GrappleMaximumSpeed = 2200.f;
    UPROPERTY(EditAnywhere, Category="Demo|Grapple", meta=(ClampMin="0", ClampMax="1")) float GrappleGravityMultiplier = .55f;
    UPROPERTY(EditAnywhere, Category="Demo|Grapple", meta=(ClampMin="100")) float GrappleArrivalDistance = 130.f;
    UPROPERTY(EditAnywhere, Category="Demo|Grapple", meta=(ClampMin="0")) float GrappleAttachLift = 270.f;
    UPROPERTY(EditAnywhere, Category="Demo|Grapple", meta=(ClampMin="0")) float GrappleJumpWindow = .18f;
private:
    static constexpr uint8 GrappleMode = 1;
    bool bSliding = false;
    float SlideSeconds = 0.f;
    float AirborneMomentumSpeed = 0.f;
    float ActiveSlideMaximumSpeed = 1050.f;
    double NextSlideTime = 0;
    TWeakObjectPtr<class UPrimitiveComponent> GrappleAnchor;
    FVector GrappleLocalPoint = FVector::ZeroVector;
    FVector PreviousAnchorPosition = FVector::ZeroVector;
    double GrappleRopeLength = 0;
    double GrappleAttachTime = 0;
    bool bGrappleJumpUsed = false;
};
