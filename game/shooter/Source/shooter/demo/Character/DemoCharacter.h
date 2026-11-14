#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "DemoCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class ADemoWeapon;
class UDemoMotionComponent;

UCLASS(Blueprintable)
class SHOOTER_API ADemoCharacter : public ACharacter
{
    GENERATED_BODY()
public:
    ADemoCharacter();
    virtual void Tick(float DeltaSeconds) override;
    virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
    virtual void PawnClientRestart() override;
    virtual void Landed(const FHitResult& Hit) override;
    virtual void OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UFUNCTION(BlueprintPure, Category="Demo") bool IsSprinting() const { return bSprinting; }
    UFUNCTION(BlueprintPure, Category="Demo") float GetAimAlpha() const { return AimAlpha; }
    UFUNCTION(BlueprintPure, Category="Demo") ADemoWeapon* GetWeapon() const { return Weapon; }
    UCameraComponent* GetFirstPersonCamera() const { return Camera; }
    bool CanFire() const;
    void ApplyShotRecoil(FVector2D Kick);
    void ApplyLookInput(FVector2D Delta);
    void RecoverAimRecoil(float DeltaSeconds);
    void RecordElimination() { ++Eliminations; }
    int32 GetEliminations() const { return Eliminations; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Demo|Camera") TObjectPtr<UCameraComponent> Camera;
    /** Optional mannequin/arms mesh: set mesh, AnimBP and socket in a derived Blueprint. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Demo|Appearance") TObjectPtr<USkeletalMeshComponent> FirstPersonArms;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Demo|Camera") TObjectPtr<UDemoMotionComponent> Motion;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Appearance") TObjectPtr<USkeletalMesh> ArmsMesh;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Weapon") TSubclassOf<ADemoWeapon> WeaponClass;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="100")) float WalkSpeed = 440.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="100")) float SprintSpeed = 720.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="100")) float AimSpeed = 240.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="60", ClampMax="120")) float HipFOV = 95.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="55", ClampMax="90")) float ScopeFOV = 78.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="0.08", ClampMax="0.8")) float AimTransitionSeconds = 0.16f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="2", ClampMax="40")) float WeaponResponse = 24.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Recoil", meta=(ClampMin="0")) float RecoilRecoveryDelay = 0.18f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Recoil", meta=(ClampMin="1")) float RecoilRecoverySpeed = 10.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="100")) float CrouchSpeed = 190.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="0", ClampMax="1")) float MotionScale = 1.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="0", ClampMax="0.3")) float CoyoteSeconds = 0.1f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="0", ClampMax="0.3")) float JumpBufferSeconds = 0.12f;
protected:
    virtual void BeginPlay() override;
private:
    friend class FDemoControllerFeelTest;
    friend class ADemoPresentationTest;
    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void GamepadLook(const FInputActionValue& Value);
    void MoveStopped();
    void JumpPressed();
    void JumpReleased();
    void AimPressed();
    void AimReleased();
    void SprintPressed();
    void SprintReleased();
    void FirePressed();
    void FireReleased();
    void ReloadPressed();
    void CrouchPressed();
    void CrouchReleased();
    void CreateInputActions();
    void InstallInputContext();
    UPROPERTY() TObjectPtr<UInputMappingContext> Mapping;
    UPROPERTY() TObjectPtr<UInputAction> MoveAction;
    UPROPERTY() TObjectPtr<UInputAction> LookAction;
    UPROPERTY() TObjectPtr<UInputAction> GamepadLookAction;
    UPROPERTY() TObjectPtr<UInputAction> JumpAction;
    UPROPERTY() TObjectPtr<UInputAction> AimAction;
    UPROPERTY() TObjectPtr<UInputAction> SprintAction;
    UPROPERTY() TObjectPtr<UInputAction> FireAction;
    UPROPERTY() TObjectPtr<UInputAction> ReloadAction;
    UPROPERTY() TObjectPtr<UInputAction> CrouchAction;
    UPROPERTY(VisibleInstanceOnly, Category="Demo") TObjectPtr<ADemoWeapon> Weapon;
    FVector2D MoveInput = FVector2D::ZeroVector;
    FVector2D LookImpulse = FVector2D::ZeroVector;
    FVector2D SmoothedSway = FVector2D::ZeroVector;
    FVector2D AimRecoilDebt = FVector2D::ZeroVector;
    FVector WeaponPoseLocation = FVector(36, 16, -18);
    FRotator WeaponPoseRotation = FRotator::ZeroRotator;
    bool bWantsAim = false;
    bool bWantsSprint = false;
    bool bSprinting = false;
    bool bJumpConsumed = false;
    bool bJumpHeld = false;
    float AimAlpha = 0;
    float AimProgress = 0;
    float SprintAlpha = 0;
    float CurrentEyeHeight = 70.f;
    double LastShotTime = -100;
    double LastGroundedTime = -100;
    double JumpPressedTime = -100;
    int32 Eliminations = 0;
};
