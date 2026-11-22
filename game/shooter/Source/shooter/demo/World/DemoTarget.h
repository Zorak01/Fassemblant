#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoTarget.generated.h"

UCLASS(Blueprintable)
class SHOOTER_API ADemoTarget : public AActor
{
    GENERATED_BODY()
public:
    ADemoTarget();
    virtual float TakeDamage(float Amount, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer) override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    virtual void ApplyWorldOffset(const FVector& Offset, bool bWorldShift) override;
    bool IsActive() const { return Health > 0; }
    bool WasLastHitHeadshot() const { return bHeadshot; }
    void ResetTarget();
    FVector GetHeadLocation() const;
    UPROPERTY(EditAnywhere, Category="Demo|Target", meta=(ClampMin="1")) float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, Category="Demo|Target", meta=(ClampMin="0.1")) float ResetSeconds = 2.5f;
    UPROPERTY(EditAnywhere, Category="Demo|Target") float StrafeDistance = 0.f;
    UPROPERTY(EditAnywhere, Category="Demo|Target") float StrafeSpeed = 1.f;
    UPROPERTY(EditAnywhere, Category="Demo|Target") FVector StrafeDirection = FVector(0,1,0);
protected:
    virtual void BeginPlay() override;
private:
    UPROPERTY(VisibleAnywhere) TObjectPtr<class USceneComponent> TargetRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Bullseye;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Head;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Stand;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> HealthBar;
    FVector Home;
    float Health = 100;
    float HitPulse = 0;
    float MotionTime = 0;
    bool bHeadshot = false;
    FTimerHandle ResetTimer;
};
