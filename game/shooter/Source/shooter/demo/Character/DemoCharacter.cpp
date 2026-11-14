#include "demo/Character/DemoCharacter.h"
#include "demo/Weapons/DemoWeapon.h"
#include "demo/Game/DemoUserSettings.h"
#include "demo/Character/DemoMotionComponent.h"
#include "demo/Character/DemoArmsAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "UObject/ConstructorHelpers.h"

ADemoCharacter::ADemoCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(0, 0, 70));
    Camera->bUsePawnControlRotation = true;
    Camera->FieldOfView = HipFOV;
    Camera->SetEnableFirstPersonFieldOfView(true);
    Camera->SetEnableFirstPersonScale(true);
    Camera->SetFirstPersonFieldOfView(90.f);
    Camera->SetFirstPersonScale(0.3f);
    Motion = CreateDefaultSubobject<UDemoMotionComponent>(TEXT("Motion"));
    FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
    FirstPersonArms->SetupAttachment(Camera);
    FirstPersonArms->SetOnlyOwnerSee(true);
    FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FirstPersonArms->CastShadow = false;
    FirstPersonArms->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    FirstPersonArms->SetRelativeLocationAndRotation(FVector(2, 0, -154), FRotator(0, -90, 0));
    FirstPersonArms->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    FirstPersonArms->bEnableUpdateRateOptimizations = false;
    GetMesh()->SetOwnerNoSee(true);
    bUseControllerRotationYaw = true;
    auto* Movement = GetCharacterMovement();
    Movement->MaxWalkSpeed = WalkSpeed;
    Movement->MaxAcceleration = 1850;
    Movement->BrakingDecelerationWalking = 2050;
    Movement->GroundFriction = 5.5f;
    Movement->bUseSeparateBrakingFriction = true;
    Movement->BrakingFriction = 3.f;
    Movement->BrakingFrictionFactor = 1.f;
    Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
    Movement->SetCrouchedHalfHeight(52.f);
    Movement->MaxWalkSpeedCrouched = CrouchSpeed;
    Movement->JumpZVelocity = 530;
    Movement->GravityScale = 2.15f;
    Movement->AirControl = 0.32f;
    JumpMaxHoldTime = 0.08f;
    WeaponClass = ADemoWeapon::StaticClass();
}

void ADemoCharacter::BeginPlay()
{
    Super::BeginPlay();
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.Instigator = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    if (WeaponClass)
    {
        Weapon = GetWorld()->SpawnActor<ADemoWeapon>(WeaponClass, GetActorTransform(), Params);
        if (Weapon)
        {
            Weapon->AttachToComponent(Camera, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
            WeaponPoseLocation = Weapon->GetHipPosition();
            Weapon->SetActorRelativeLocation(WeaponPoseLocation);
            PrimaryActorTick.AddPrerequisite(Weapon, Weapon->PrimaryActorTick);
        }
    }
    if (ArmsMesh) FirstPersonArms->SetSkeletalMesh(ArmsMesh);
    FirstPersonArms->SetAnimInstanceClass(UDemoArmsAnimInstance::StaticClass());
    FirstPersonArms->AddTickPrerequisiteActor(this);
    if (auto* PC = Cast<APlayerController>(Controller))
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
}

void ADemoCharacter::CreateInputActions()
{
    if (Mapping) return;
    Mapping = NewObject<UInputMappingContext>(this, TEXT("DemoControls"));
    auto Action = [this](FName Name, EInputActionValueType Type)
    {
        auto* Result = NewObject<UInputAction>(this, Name);
        Result->ValueType = Type;
        return Result;
    };
    MoveAction = Action(TEXT("Move"), EInputActionValueType::Axis2D);
    LookAction = Action(TEXT("MouseLook"), EInputActionValueType::Axis2D);
    GamepadLookAction = Action(TEXT("GamepadLook"), EInputActionValueType::Axis2D);
    JumpAction = Action(TEXT("Jump"), EInputActionValueType::Boolean);
    AimAction = Action(TEXT("Scope"), EInputActionValueType::Boolean);
    SprintAction = Action(TEXT("Sprint"), EInputActionValueType::Boolean);
    FireAction = Action(TEXT("Fire"), EInputActionValueType::Boolean);
    ReloadAction = Action(TEXT("Reload"), EInputActionValueType::Boolean);
    CrouchAction = Action(TEXT("Crouch"), EInputActionValueType::Boolean);
    auto MoveKey = [this](FKey Key, bool bSideways, bool bNegative)
    {
        auto& Binding = Mapping->MapKey(MoveAction, Key);
        if (bNegative) Binding.Modifiers.Add(NewObject<UInputModifierNegate>(Mapping));
        if (bSideways)
        {
            auto* Swizzle = NewObject<UInputModifierSwizzleAxis>(Mapping);
            Swizzle->Order = EInputAxisSwizzle::YXZ;
            Binding.Modifiers.Add(Swizzle);
        }
    };
    MoveKey(EKeys::W, false, false); MoveKey(EKeys::S, false, true);
    MoveKey(EKeys::D, true, false); MoveKey(EKeys::A, true, true);
    auto& StickMove = Mapping->MapKey(MoveAction, EKeys::Gamepad_Left2D);
    StickMove.Modifiers.Add(NewObject<UInputModifierDeadZone>(Mapping));
    auto* Swizzle = NewObject<UInputModifierSwizzleAxis>(Mapping);
    Swizzle->Order = EInputAxisSwizzle::YXZ;
    StickMove.Modifiers.Add(Swizzle);
    Mapping->MapKey(LookAction, EKeys::Mouse2D);
    Mapping->MapKey(GamepadLookAction, EKeys::Gamepad_Right2D).Modifiers.Add(NewObject<UInputModifierDeadZone>(Mapping));
    Mapping->MapKey(JumpAction, EKeys::SpaceBar);
    Mapping->MapKey(JumpAction, EKeys::Gamepad_FaceButton_Bottom);
    Mapping->MapKey(AimAction, EKeys::RightMouseButton);
    Mapping->MapKey(AimAction, EKeys::Gamepad_LeftTrigger);
    Mapping->MapKey(SprintAction, EKeys::LeftShift);
    Mapping->MapKey(SprintAction, EKeys::Gamepad_LeftThumbstick);
    Mapping->MapKey(FireAction, EKeys::LeftMouseButton);
    Mapping->MapKey(FireAction, EKeys::Gamepad_RightTrigger);
    Mapping->MapKey(ReloadAction, EKeys::R);
    Mapping->MapKey(ReloadAction, EKeys::Gamepad_FaceButton_Left);
    Mapping->MapKey(CrouchAction, EKeys::LeftControl);
    Mapping->MapKey(CrouchAction, EKeys::C);
    Mapping->MapKey(CrouchAction, EKeys::Gamepad_RightThumbstick);
}

void ADemoCharacter::InstallInputContext()
{
    if (auto* PC = Cast<APlayerController>(Controller))
        if (auto* LP = PC->GetLocalPlayer())
            if (auto* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                Subsystem->RemoveMappingContext(Mapping);
                Subsystem->AddMappingContext(Mapping, 0);
            }
}

void ADemoCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
    Super::SetupPlayerInputComponent(Input);
    CreateInputActions();
    auto* Enhanced = CastChecked<UEnhancedInputComponent>(Input);
    Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
    Enhanced->BindAction(MoveAction, ETriggerEvent::Completed, this, &ThisClass::MoveStopped);
    Enhanced->BindAction(MoveAction, ETriggerEvent::Canceled, this, &ThisClass::MoveStopped);
    Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
    Enhanced->BindAction(GamepadLookAction, ETriggerEvent::Triggered, this, &ThisClass::GamepadLook);
    Enhanced->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::JumpPressed);
    Enhanced->BindAction(JumpAction, ETriggerEvent::Completed, this, &ThisClass::JumpReleased);
    Enhanced->BindAction(JumpAction, ETriggerEvent::Canceled, this, &ThisClass::JumpReleased);
    Enhanced->BindAction(AimAction, ETriggerEvent::Started, this, &ThisClass::AimPressed);
    Enhanced->BindAction(AimAction, ETriggerEvent::Completed, this, &ThisClass::AimReleased);
    Enhanced->BindAction(AimAction, ETriggerEvent::Canceled, this, &ThisClass::AimReleased);
    Enhanced->BindAction(SprintAction, ETriggerEvent::Started, this, &ThisClass::SprintPressed);
    Enhanced->BindAction(SprintAction, ETriggerEvent::Completed, this, &ThisClass::SprintReleased);
    Enhanced->BindAction(SprintAction, ETriggerEvent::Canceled, this, &ThisClass::SprintReleased);
    Enhanced->BindAction(FireAction, ETriggerEvent::Started, this, &ThisClass::FirePressed);
    Enhanced->BindAction(FireAction, ETriggerEvent::Completed, this, &ThisClass::FireReleased);
    Enhanced->BindAction(FireAction, ETriggerEvent::Canceled, this, &ThisClass::FireReleased);
    Enhanced->BindAction(ReloadAction, ETriggerEvent::Started, this, &ThisClass::ReloadPressed);
    Enhanced->BindAction(CrouchAction, ETriggerEvent::Started, this, &ThisClass::CrouchPressed);
    Enhanced->BindAction(CrouchAction, ETriggerEvent::Completed, this, &ThisClass::CrouchReleased);
    Enhanced->BindAction(CrouchAction, ETriggerEvent::Canceled, this, &ThisClass::CrouchReleased);
    InstallInputContext();
}

void ADemoCharacter::PawnClientRestart() { Super::PawnClientRestart(); CreateInputActions(); InstallInputContext(); }
void ADemoCharacter::Move(const FInputActionValue& Value)
{
    MoveInput = Value.Get<FVector2D>().GetClampedToMaxSize(1.f);
    if (!Controller) return;
    const FRotator Yaw(0, Controller->GetControlRotation().Yaw, 0);
    AddMovementInput(Yaw.Vector(), MoveInput.X);
    AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), MoveInput.Y);
}
void ADemoCharacter::MoveStopped() { MoveInput = FVector2D::ZeroVector; }
void ADemoCharacter::Look(const FInputActionValue& Value)
{
    const FVector2D Delta = Value.Get<FVector2D>();
    const auto* Settings = UDemoUserSettings::Get();
    const float Sensitivity = Settings ? Settings->MouseSensitivity : 0.10f;
    const float ADSMultiplier = Settings ? Settings->ADSSensitivityMultiplier : 0.22f;
    const bool bInvert = Settings && Settings->bInvertMouseY;
    // Per-count input goes straight into this frame's controller rotation. No dt,
    // acceleration curve, smoothing queue, or hidden FOV sensitivity multiplier.
    ApplyLookInput(FVector2D(Delta.X, Delta.Y * (bInvert ? -1.f : 1.f)) * Sensitivity * FMath::Lerp(1.f, ADSMultiplier, AimAlpha));
    LookImpulse = Delta.GetClampedToMaxSize(20.f);
}
void ADemoCharacter::GamepadLook(const FInputActionValue& Value)
{
    const FVector2D Delta = Value.Get<FVector2D>();
    const float Scale = 150.f * GetWorld()->GetDeltaSeconds() * FMath::Lerp(1.f, 0.4f, AimAlpha);
    // Positive stick Y and positive world pitch both look upward.
    ApplyLookInput(FVector2D(Delta.X, Delta.Y) * Scale);
    LookImpulse = Delta * 4.f;
}
void ADemoCharacter::JumpPressed()
{
    UnCrouch(); bJumpHeld = true; JumpPressedTime = GetWorld()->GetTimeSeconds();
    if (GetCharacterMovement()->IsMovingOnGround() && !bIsCrouched)
    {
        Jump(); bJumpConsumed = true; JumpPressedTime = -100;
    }
}
void ADemoCharacter::JumpReleased() { bJumpHeld = false; StopJumping(); }
void ADemoCharacter::AimPressed() { bWantsAim = true; bWantsSprint = false; }
void ADemoCharacter::AimReleased() { bWantsAim = false; }
void ADemoCharacter::SprintPressed() { UnCrouch(); bWantsSprint = true; bWantsAim = false; FireReleased(); }
void ADemoCharacter::SprintReleased() { bWantsSprint = false; }
void ADemoCharacter::FirePressed() { if (Weapon) Weapon->SetTrigger(true); }
void ADemoCharacter::FireReleased() { if (Weapon) Weapon->SetTrigger(false); }
void ADemoCharacter::ReloadPressed() { if (Weapon && Weapon->Reload()) bWantsAim = false; }
void ADemoCharacter::CrouchPressed() { bWantsSprint = false; Crouch(); }
void ADemoCharacter::CrouchReleased() { UnCrouch(); }
bool ADemoCharacter::CanFire() const { return !bSprinting && SprintAlpha < 0.15f; }
void ADemoCharacter::ApplyLookInput(FVector2D Delta)
{
    // Counter-steering pays down recoverable kick. Recovery never drags the view
    // past the position the player deliberately compensated to.
    for (int32 Axis = 0; Axis < 2; ++Axis)
        if (Delta[Axis] * AimRecoilDebt[Axis] < 0.f)
            AimRecoilDebt[Axis] += FMath::Sign(Delta[Axis]) * FMath::Min(FMath::Abs(Delta[Axis]), FMath::Abs(AimRecoilDebt[Axis]));
    AddControllerYawInput(Delta.X);
    AddControllerPitchInput(Delta.Y);
}
void ADemoCharacter::ApplyShotRecoil(FVector2D Kick)
{
    if (!Controller) return;
    FRotator Rotation = Controller->GetControlRotation();
    const float OldPitch = FRotator::NormalizeAxis(Rotation.Pitch);
    // Unreal world pitch is POSITIVE upwards; legacy input scales are disabled.
    Rotation.Pitch = FMath::Clamp(OldPitch + Kick.Y, -85.f, 85.f);
    Rotation.Yaw += Kick.X;
    Controller->SetControlRotation(Rotation);
    AimRecoilDebt += FVector2D(Kick.X, Rotation.Pitch - OldPitch);
    LastShotTime = GetWorld()->GetTimeSeconds();
}
void ADemoCharacter::RecoverAimRecoil(float Dt)
{
    if (!Controller || GetWorld()->GetTimeSeconds() - LastShotTime < RecoilRecoveryDelay) return;
    const FVector2D Recovery = AimRecoilDebt * (1.f - FMath::Exp(-RecoilRecoverySpeed * Dt));
    FRotator Rotation = Controller->GetControlRotation();
    Rotation.Pitch = FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch) - Recovery.Y, -85.f, 85.f);
    Rotation.Yaw -= Recovery.X;
    Controller->SetControlRotation(Rotation);
    AimRecoilDebt -= Recovery;
}

void ADemoCharacter::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!IsLocallyControlled()) return;
    const auto Blend = [Dt](float Response) { return 1.f - FMath::Exp(-Response * Dt); };
    SmoothedSway = FMath::Lerp(SmoothedSway, LookImpulse, Blend(22.f));
    auto* Movement = GetCharacterMovement();
    const double Now = GetWorld()->GetTimeSeconds();
    const bool bGrounded = Movement->IsMovingOnGround();
    if (bGrounded) { LastGroundedTime = Now; bJumpConsumed = false; }
    if (!bJumpConsumed && Now - JumpPressedTime <= JumpBufferSeconds && Now - LastGroundedTime <= CoyoteSeconds)
    {
        if (bGrounded) Jump();
        else LaunchCharacter(FVector(0, 0, Movement->JumpZVelocity), false, true);
        bJumpConsumed = true;
        JumpPressedTime = -100;
    }
    if (!bJumpHeld) StopJumping();
    const bool bReloading = Weapon && Weapon->IsReloading();
    bSprinting = bWantsSprint && MoveInput.X > 0.1f && bGrounded && !bIsCrouched && !Movement->bWantsToCrouch && !bReloading && !bWantsAim;
    const float AimTarget = bWantsAim && !bReloading && !bSprinting ? 1.f : 0.f;
    AimProgress = FMath::FInterpConstantTo(AimProgress, AimTarget, Dt, 1.f / FMath::Max(0.08f, AimTransitionSeconds));
    AimAlpha = FMath::SmoothStep(0.f, 1.f, AimProgress);
    SprintAlpha = FMath::Lerp(SprintAlpha, bSprinting ? 1.f : 0.f, Blend(6.f));
    Movement->MaxWalkSpeed = FMath::Lerp(FMath::Lerp(WalkSpeed, SprintSpeed, SprintAlpha), AimSpeed, AimAlpha);
    Movement->MaxWalkSpeedCrouched = FMath::Lerp(CrouchSpeed, CrouchSpeed * 0.72f, AimAlpha);
    Camera->SetFieldOfView(FMath::Lerp(HipFOV + 7.f * SprintAlpha, ScopeFOV, AimAlpha));
    RecoverAimRecoil(Dt);
    Motion->Intensity = MotionScale;
    Motion->Update(Dt, GetVelocity(), GetControlRotation(), bGrounded, SprintAlpha, AimAlpha, bIsCrouched);
    CurrentEyeHeight = FMath::Lerp(CurrentEyeHeight, bIsCrouched ? 42.f : 70.f, Blend(10.f));
    Camera->SetRelativeLocation(FVector(0, 0, CurrentEyeHeight) + Motion->CameraOffset);
    Camera->ClearAdditiveOffset();
    Camera->AddAdditiveOffset(FTransform(Motion->CameraRotation), 0.f);
    Camera->SetFirstPersonFieldOfView(FMath::Lerp(90.f, ScopeFOV, AimAlpha));
    if (Weapon)
    {
        const float ReloadPhase = Weapon->GetReloadProgress();
        const float ReloadMotion = Weapon->IsReloading() ? FMath::Sin(PI * ReloadPhase) : 0.f;
        const FTransform AimPose = Weapon->GetAimPose();
        FVector Position = FMath::Lerp(Weapon->GetHipPosition(), AimPose.GetLocation(), AimAlpha);
        const float Idle = (1.f - AimAlpha) * (1.f - SprintAlpha) * MotionScale;
        Position += Motion->WeaponOffset + FVector(0, FMath::Sin(Now * 1.7f) * 0.12f, FMath::Sin(Now * 2.1f) * 0.10f) * Idle;
        Position += FVector(-6.f * SprintAlpha - 2.f * ReloadMotion, 3.f * SprintAlpha - 6.f * ReloadMotion, -8.f * SprintAlpha + 9.f * ReloadMotion);
        const float SwayScale = MotionScale * (1.f - AimAlpha * 0.98f);
        const FRotator Rotation = AimPose.Rotator() * AimAlpha + Motion->WeaponRotation + FRotator(-12.f * SprintAlpha + 9.f * ReloadMotion + SmoothedSway.Y * 0.035f * SwayScale,
            -16.f * SprintAlpha - 14.f * ReloadMotion - SmoothedSway.X * 0.045f * SwayScale, -32.f * ReloadMotion - MoveInput.Y * 0.7f * SwayScale);
        auto* Root = Weapon->GetRootComponent();
        WeaponPoseLocation = FMath::Lerp(WeaponPoseLocation, Position, Blend(WeaponResponse));
        WeaponPoseRotation = FMath::Lerp(WeaponPoseRotation, Rotation, Blend(WeaponResponse));
        // Cosmetic kick is applied AFTER pose interpolation: firing never waits on
        // weapon lag, and this offset never changes bullet direction.
        const float VisualRecoil = FMath::Lerp(1.f, 0.14f, AimAlpha);
        Root->SetRelativeLocationAndRotation(WeaponPoseLocation + FVector(-3.6f, 0.08f, 0.4f) * Weapon->GetRecoil() * VisualRecoil,
            WeaponPoseRotation + FRotator(Weapon->GetRecoil() * 2.6f, 0, Weapon->GetRecoil() * 0.45f) * VisualRecoil);
    }
    LookImpulse = FMath::Lerp(LookImpulse, FVector2D::ZeroVector, Blend(12.f));
}
void ADemoCharacter::Landed(const FHitResult& Hit)
{
    Motion->Land(GetVelocity().Z);
    Super::Landed(Hit);
}
void ADemoCharacter::OnStartCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnStartCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    CurrentEyeHeight += HalfHeightAdjust;
    Camera->AddRelativeLocation(FVector(0, 0, HalfHeightAdjust));
}
void ADemoCharacter::OnEndCrouch(float HalfHeightAdjust, float ScaledHalfHeightAdjust)
{
    Super::OnEndCrouch(HalfHeightAdjust, ScaledHalfHeightAdjust);
    CurrentEyeHeight -= HalfHeightAdjust;
    Camera->AddRelativeLocation(FVector(0, 0, -HalfHeightAdjust));
}
void ADemoCharacter::EndPlay(const EEndPlayReason::Type Reason)
{
    if (auto* PC = Cast<APlayerController>(Controller))
        if (auto* LP = PC->GetLocalPlayer())
            if (auto* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
                if (Mapping) Subsystem->RemoveMappingContext(Mapping);
    if (Weapon) Weapon->Destroy();
    Super::EndPlay(Reason);
}
