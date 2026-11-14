#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoWeapon.generated.h"

class UStaticMeshComponent;
class USceneComponent;
class USoundBase;
class ADemoCharacter;
class UDemoWeaponVisualData;

UENUM(BlueprintType)
enum class EDemoWeaponState : uint8 { Ready, Reloading };

/** Local single-player weapon. Gameplay timing is independent from cosmetic motion. */
UCLASS(Blueprintable)
class SHOOTER_API ADemoWeapon : public AActor
{
    GENERATED_BODY()
public:
    ADemoWeapon();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void SetTrigger(bool bDown);
    UFUNCTION(BlueprintCallable, Category="Demo|Weapon") bool Reload();
    UFUNCTION(BlueprintPure, Category="Demo|Weapon") bool IsReloading() const { return State == EDemoWeaponState::Reloading; }
    UFUNCTION(BlueprintPure, Category="Demo|Weapon") float GetReloadProgress() const;
    UFUNCTION(BlueprintPure, Category="Demo|Weapon") int32 GetAmmo() const { return Ammo; }
    UFUNCTION(BlueprintPure, Category="Demo|Weapon") int32 GetReserve() const { return Reserve; }
    float GetRecoil() const { return Recoil; }
    float GetFlash() const { return Flash; }
    float GetHitAge() const;
    bool WasLastHitKill() const { return bLastHitKill; }
    FTransform GetAimPose() const;
    FVector GetHipPosition() const;
    FTransform GetHandTarget(bool bLeft) const;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Demo|Appearance") TObjectPtr<UDemoWeaponVisualData> VisualData;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Demo|Weapon", meta=(ClampMin="1")) int32 MagazineSize = 30;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Demo|Weapon", meta=(ClampMin="0")) int32 StartingReserve = 180;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Demo|Weapon", meta=(ClampMin="60")) float RoundsPerMinute = 600.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Demo|Weapon", meta=(ClampMin="0.1")) float ReloadSeconds = 1.9f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Demo|Weapon", meta=(ClampMin="1")) float Damage = 34.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Demo|Weapon", meta=(ClampMin="100")) float Range = 20000.f;
    /** X = yaw, Y = upward pitch, in degrees. Repeats predictably during a burst. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Demo|Recoil") TArray<FVector2D> RecoilPattern;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Recoil", meta=(ClampMin="0", ClampMax="1")) float ADSRecoilMultiplier = 0.7f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Recoil", meta=(ClampMin="0.1")) float BurstResetSeconds = 0.28f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Audio") TObjectPtr<USoundBase> FireSound;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Audio") TObjectPtr<USoundBase> ReloadSound;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Audio") TObjectPtr<USoundBase> EmptySound;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Debug") bool bDrawDebugTraces = false;

    UFUNCTION(BlueprintImplementableEvent, Category="Demo|Weapon") void OnShot(const FHitResult& Hit, bool bHit);
    UFUNCTION(BlueprintImplementableEvent, Category="Demo|Weapon") void OnReloadStarted();
    UFUNCTION(BlueprintImplementableEvent, Category="Demo|Weapon") void OnReloadFinished();

protected:
    virtual void BeginPlay() override;
private:
    void TryFire();
    void FinishReload();
    void BuildVisuals();
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> WeaponRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Magazine;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Bolt;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> ChargingHandle;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Trigger;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent> Muzzle;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> MuzzleFlash;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> VisualParts;
    UPROPERTY(VisibleInstanceOnly, Category="Demo|Weapon") EDemoWeaponState State = EDemoWeaponState::Ready;
    int32 Ammo = 0;
    int32 Reserve = 0;
    bool bTriggerHeld = false;
    bool bLastHitKill = false;
    double NextShotTime = 0;
    double ReloadStart = 0;
    double LastHitTime = -100;
    float Recoil = 0;
    float RecoilVelocity = 0;
    int32 BurstIndex = 0;
    double LastShotTime = -100;
    float Flash = 0;
    FTimerHandle ReloadTimer;
};
