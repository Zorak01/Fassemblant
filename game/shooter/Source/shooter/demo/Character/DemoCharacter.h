#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "InputActionValue.h"
#include "DemoCharacter.generated.h"

class UCameraComponent;
class UInputAction;
class UInputMappingContext;
class ADemoWeapon;

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
    void ApplyShotRecoil();
    void RecordElimination() { ++Eliminations; }
    int32 GetEliminations() const { return Eliminations; }

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Demo|Camera") TObjectPtr<UCameraComponent> Camera;
    /** Optional mannequin/arms mesh: set mesh, AnimBP and socket in a derived Blueprint. */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Demo|Appearance") TObjectPtr<USkeletalMeshComponent> FirstPersonArms;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Weapon") TSubclassOf<ADemoWeapon> WeaponClass;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="100")) float WalkSpeed = 440.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="100")) float SprintSpeed = 720.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="100")) float AimSpeed = 240.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="60", ClampMax="120")) float HipFOV = 95.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="15", ClampMax="90")) float ScopeFOV = 38.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="0.01")) float MouseSensitivity = 0.12f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera") bool bInvertMouseY = false;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="10", ClampMax="100")) float LookResponse = 32.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="0.1", ClampMax="0.8")) float AimTransitionSeconds = 0.32f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="2", ClampMax="30")) float WeaponResponse = 10.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="100")) float CrouchSpeed = 190.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Camera", meta=(ClampMin="0", ClampMax="1")) float MotionScale = 1.f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="0", ClampMax="0.3")) float CoyoteSeconds = 0.1f;
    UPROPERTY(EditDefaultsOnly, Category="Demo|Movement", meta=(ClampMin="0", ClampMax="0.3")) float JumpBufferSeconds = 0.12f;
protected:
    virtual void BeginPlay() override;
private:
    friend class FDemoControllerFeelTest;
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
    FVector2D PendingLook = FVector2D::ZeroVector;
    FVector2D SmoothedSway = FVector2D::ZeroVector;
    bool bWantsAim = false;
    bool bWantsSprint = false;
    bool bSprinting = false;
    bool bJumpConsumed = false;
    bool bJumpHeld = false;
    float AimAlpha = 0;
    float AimProgress = 0;
    float SprintAlpha = 0;
    float BobPhase = 0;
    float LandingOffset = 0;
    float LandingTarget = 0;
    float SmoothedMotion = 0;
    float CurrentEyeHeight = 70.f;
    float RecoilPitch = 0;
    float RecoilYaw = 0;
    double LastGroundedTime = -100;
    double JumpPressedTime = -100;
    int32 Eliminations = 0;
};
