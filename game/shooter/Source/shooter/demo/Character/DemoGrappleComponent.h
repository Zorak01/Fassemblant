#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DemoGrappleComponent.generated.h"

UENUM(BlueprintType)
enum class EDemoGrappleState : uint8 { Ready, Launching, Attached, Retracting };

/** Target acquisition, hook travel, cable presentation and lifecycle. Movement owns rope physics. */
UCLASS(ClassGroup=Demo,meta=(BlueprintSpawnableComponent))
class SHOOTER_API UDemoGrappleComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UDemoGrappleComponent();
    void Toggle();
    void ResetGrapple();
    bool HasAimTarget() const { return bHasAimTarget; }
    FVector GetAimPoint() const { return AimPoint; }
    EDemoGrappleState GetState() const { return State; }
    float GetCooldownRemaining() const;
    virtual void TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* ThisTick) override;
    virtual void ApplyWorldOffset(const FVector& Offset,bool bWorldShift) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere,Category="Demo|Grapple",meta=(ClampMin="200")) float MaximumRange = 2800.f;
    UPROPERTY(EditAnywhere,Category="Demo|Grapple",meta=(ClampMin="0")) float MinimumRange = 150.f;
    UPROPERTY(EditAnywhere,Category="Demo|Grapple",meta=(ClampMin="1000")) float HookTravelSpeed = 11000.f;
    UPROPERTY(EditAnywhere,Category="Demo|Grapple",meta=(ClampMin="0")) float CooldownSeconds = 5.f;
    UPROPERTY(EditAnywhere,Category="Demo|Grapple",meta=(ClampMin="0.2")) float MaximumAttachSeconds = 5.f;
    UPROPERTY(EditAnywhere,Category="Demo|Grapple",meta=(ClampMin="90",ClampMax="180")) float MaximumViewAngle = 120.f;
    UPROPERTY(EditAnywhere,Category="Demo|Grapple",meta=(ClampMin="0")) float LookAwayGraceSeconds = .12f;
    UPROPERTY(EditAnywhere,Category="Demo|Grapple") FVector LauncherOffset = FVector(15,-14,-10);
    UPROPERTY(EditAnywhere,Category="Demo|Grapple") TObjectPtr<class UMaterialInterface> CableMaterial;
    UPROPERTY(EditAnywhere,Category="Demo|Grapple") TObjectPtr<class USoundBase> LaunchSound;
    UPROPERTY(EditAnywhere,Category="Demo|Grapple") TObjectPtr<class USoundBase> AttachSound;
protected:
    virtual void BeginPlay() override;
private:
    bool TraceAim(FHitResult& Hit) const;
    bool RopeBlocked(FVector Anchor) const;
    FVector LauncherPosition() const;
    void BeginRetract(float Cooldown);
    void UpdateVisuals();
    UPROPERTY(Transient) TObjectPtr<class ADemoCharacter> Character;
    UPROPERTY(Transient) TObjectPtr<class UDemoMovementComponent> Movement;
    UPROPERTY(Transient) TObjectPtr<class UCableComponent> Cable;
    UPROPERTY(Transient) TObjectPtr<class UStaticMeshComponent> Hook;
    TWeakObjectPtr<class UPrimitiveComponent> AnchorComponent;
    EDemoGrappleState State = EDemoGrappleState::Ready;
    FVector LocalAnchor = FVector::ZeroVector;
    FVector HookPosition = FVector::ZeroVector;
    FVector FlightStart = FVector::ZeroVector;
    FVector AimPoint = FVector::ZeroVector;
    double PhaseStart = 0, CooldownEnd = 0;
    float FlightSeconds = 0, LookAwaySeconds = 0;
    bool bHasAimTarget = false;
};
