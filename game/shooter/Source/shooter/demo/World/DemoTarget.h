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
    bool IsActive() const { return Health > 0; }
    UPROPERTY(EditAnywhere, Category="Demo|Target", meta=(ClampMin="1")) float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, Category="Demo|Target", meta=(ClampMin="0.1")) float ResetSeconds = 2.5f;
    UPROPERTY(EditAnywhere, Category="Demo|Target") float StrafeDistance = 0.f;
    UPROPERTY(EditAnywhere, Category="Demo|Target") float StrafeSpeed = 1.f;
protected:
    virtual void BeginPlay() override;
private:
    void ResetTarget();
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Bullseye;
    FVector Home;
    float Health = 100;
    float HitPulse = 0;
    float MotionTime = 0;
    FTimerHandle ResetTimer;
};
