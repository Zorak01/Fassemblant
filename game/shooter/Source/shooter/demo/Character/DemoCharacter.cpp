#include "demo/Character/DemoCharacter.h"
#include "demo/Weapons/DemoWeapon.h"
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

ADemoCharacter::ADemoCharacter()
{
    PrimaryActorTick.bCanEverTick = true;
    GetCapsuleComponent()->InitCapsuleSize(34.f, 90.f);
    Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
    Camera->SetupAttachment(GetCapsuleComponent());
    Camera->SetRelativeLocation(FVector(0, 0, 70));
    Camera->bUsePawnControlRotation = true;
    Camera->FieldOfView = HipFOV;
    FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
    FirstPersonArms->SetupAttachment(Camera);
    FirstPersonArms->SetOnlyOwnerSee(true);
    FirstPersonArms->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    FirstPersonArms->CastShadow = false;
    GetMesh()->SetOwnerNoSee(true);
    bUseControllerRotationYaw = true;
    auto* Movement = GetCharacterMovement();
    Movement->MaxWalkSpeed = WalkSpeed;
    Movement->MaxAcceleration = 1550;
    Movement->BrakingDecelerationWalking = 1450;
    Movement->GroundFriction = 4.5f;
    Movement->bUseSeparateBrakingFriction = true;
    Movement->BrakingFriction = 3.f;
    Movement->BrakingFrictionFactor = 1.f;
    Movement->GetNavAgentPropertiesRef().bCanCrouch = true;
    Movement->SetCrouchedHalfHeight(52.f);
    Movement->MaxWalkSpeedCrouched = CrouchSpeed;
    Movement->JumpZVelocity = 560;
    Movement->GravityScale = 1.65f;
    Movement->AirControl = 0.3f;
    JumpMaxHoldTime = 0.16f;
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
            Weapon->SetActorRelativeLocation(FVector(36, 16, -18));
        }
    }
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
    // Mouse input is already a per-frame delta. Do not multiply it by frame time.
    const float Sensitivity = MouseSensitivity * FMath::Lerp(1.f, ScopeFOV / HipFOV, AimAlpha);
    // Unreal's MouseY already follows pitch convention. Negating it again inverted look.
    PendingLook += FVector2D(Delta.X, Delta.Y * (bInvertMouseY ? -1.f : 1.f)) * Sensitivity;
    LookImpulse = Delta.GetClampedToMaxSize(20.f);
}
void ADemoCharacter::GamepadLook(const FInputActionValue& Value)
{
    const FVector2D Delta = Value.Get<FVector2D>();
    const float Scale = 150.f * GetWorld()->GetDeltaSeconds() * FMath::Lerp(1.f, 0.4f, AimAlpha);
    // A stick's positive Y is up; it needs the opposite pitch sign from MouseY.
    PendingLook += FVector2D(Delta.X, -Delta.Y) * Scale;
    LookImpulse = Delta * 4.f;
}
void ADemoCharacter::JumpPressed() { UnCrouch(); bJumpHeld = true; JumpPressedTime = GetWorld()->GetTimeSeconds(); }
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
void ADemoCharacter::ApplyShotRecoil() { RecoilPitch += FMath::Lerp(0.6f, 0.22f, AimAlpha); RecoilYaw += FMath::FRandRange(-0.1f, 0.1f); }

void ADemoCharacter::Tick(float Dt)
{
    Super::Tick(Dt);
    if (!IsLocallyControlled()) return;
    const auto Blend = [Dt](float Response) { return 1.f - FMath::Exp(-Response * Dt); };
    const FVector2D LookStep = PendingLook * Blend(LookResponse);
    AddControllerYawInput(LookStep.X);
    AddControllerPitchInput(LookStep.Y);
    PendingLook -= LookStep;
    SmoothedSway = FMath::Lerp(SmoothedSway, LookImpulse, Blend(9.f));
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
    AimProgress = FMath::FInterpConstantTo(AimProgress, AimTarget, Dt, 1.f / FMath::Max(0.1f, AimTransitionSeconds));
    AimAlpha = FMath::SmoothStep(0.f, 1.f, AimProgress);
    SprintAlpha = FMath::Lerp(SprintAlpha, bSprinting ? 1.f : 0.f, Blend(6.f));
    Movement->MaxWalkSpeed = FMath::Lerp(FMath::Lerp(WalkSpeed, SprintSpeed, SprintAlpha), AimSpeed, AimAlpha);
    Movement->MaxWalkSpeedCrouched = FMath::Lerp(CrouchSpeed, CrouchSpeed * 0.72f, AimAlpha);
    Camera->SetFieldOfView(FMath::Lerp(HipFOV + 7.f * SprintAlpha, ScopeFOV, AimAlpha));
    // Exponential impulse consumption preserves total recoil across frame rates.
    const float RecoilStep = Blend(14.f);
    AddControllerPitchInput(-RecoilPitch * RecoilStep);
    AddControllerYawInput(RecoilYaw * RecoilStep);
    RecoilPitch *= 1.f - RecoilStep;
    RecoilYaw *= 1.f - RecoilStep;
    const float Speed = GetVelocity().Size2D();
    const float MotionTarget = FMath::Clamp(Speed / WalkSpeed, 0.f, 1.6f) * (bGrounded ? 1.f : 0.f) * MotionScale * (bIsCrouched ? 0.45f : 1.f);
    SmoothedMotion = FMath::Lerp(SmoothedMotion, MotionTarget, Blend(7.f));
    const float Motion = SmoothedMotion;
    BobPhase += Dt * FMath::Lerp(10.f, 15.f, SprintAlpha) * FMath::Clamp(Speed / WalkSpeed, 0.f, 1.3f);
    LandingOffset = FMath::Lerp(LandingOffset, LandingTarget, Blend(16.f));
    LandingTarget = FMath::Lerp(LandingTarget, 0.f, Blend(8.f));
    CurrentEyeHeight = FMath::Lerp(CurrentEyeHeight, bIsCrouched ? 42.f : 70.f, Blend(10.f));
    const float Bob = FMath::Sin(2.f * BobPhase) * 0.35f * Motion * (1.f - AimAlpha * 0.98f);
    Camera->SetRelativeLocation(FVector(0, 0, CurrentEyeHeight + LandingOffset * (1.f - AimAlpha * 0.8f) + Bob));
    if (Weapon)
    {
        const float ReloadPhase = Weapon->GetReloadProgress();
        const float ReloadMotion = Weapon->IsReloading() ? FMath::Sin(PI * ReloadPhase) : 0.f;
        FVector Position = FMath::Lerp(FVector(36, 16, -18), FVector(30, 0, -8), AimAlpha);
        Position += FVector(-Weapon->GetRecoil() * 2.2f, FMath::Sin(BobPhase) * Motion, FMath::Cos(BobPhase * 2.f) * 0.65f * Motion) * (1.f - AimAlpha * 0.85f);
        Position += FVector(-6.f * SprintAlpha, 3.f * SprintAlpha, -8.f * SprintAlpha - 10.f * ReloadMotion);
        const float SwayScale = MotionScale * (1.f - AimAlpha * 0.98f);
        const FRotator Rotation(-7.f * SprintAlpha - 16.f * ReloadMotion + Weapon->GetRecoil() * 1.2f + SmoothedSway.Y * 0.08f * SwayScale,
            -10.f * SprintAlpha - SmoothedSway.X * 0.1f * SwayScale, -18.f * ReloadMotion - MoveInput.Y * 1.2f * SwayScale);
        auto* Root = Weapon->GetRootComponent();
        Root->SetRelativeLocationAndRotation(FMath::Lerp(Root->GetRelativeLocation(), Position, Blend(WeaponResponse)),
            FMath::Lerp(Root->GetRelativeRotation(), Rotation, Blend(WeaponResponse)));
        // The magnified scope is unobstructed by the close-up first-person mesh.
        Weapon->SetActorHiddenInGame(AimAlpha > 0.88f);
    }
    LookImpulse = FMath::Lerp(LookImpulse, FVector2D::ZeroVector, Blend(12.f));
}
void ADemoCharacter::Landed(const FHitResult& Hit)
{
    LandingTarget = -FMath::Clamp(FMath::Abs(GetVelocity().Z) * 0.005f, 0.f, 5.f) * MotionScale;
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
