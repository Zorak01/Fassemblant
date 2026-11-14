#include "demo/Weapons/DemoWeapon.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/Weapons/DemoWeaponVisualData.h"
#include "demo/World/DemoTarget.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ADemoWeapon::ADemoWeapon()
{
    PrimaryActorTick.bCanEverTick = true;
    RecoilPattern = {FVector2D(0.02, 0.52), FVector2D(-0.04, 0.56), FVector2D(0.06, 0.60),
        FVector2D(0.11, 0.58), FVector2D(0.14, 0.54), FVector2D(0.09, 0.50),
        FVector2D(-0.07, 0.48), FVector2D(-0.13, 0.46), FVector2D(-0.10, 0.45), FVector2D(0.03, 0.44)};
    WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
    SetRootComponent(WeaponRoot);
    Magazine = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Magazine"));
    Magazine->SetupAttachment(WeaponRoot);
    Magazine->SetRelativeLocation(FVector(8, 0, -5));
    Magazine->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Bolt = CreateDefaultSubobject<USceneComponent>(TEXT("Bolt")); Bolt->SetupAttachment(WeaponRoot);
    ChargingHandle = CreateDefaultSubobject<USceneComponent>(TEXT("ChargingHandle")); ChargingHandle->SetupAttachment(WeaponRoot);
    Trigger = CreateDefaultSubobject<USceneComponent>(TEXT("Trigger")); Trigger->SetupAttachment(WeaponRoot);
    Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
    Muzzle->SetupAttachment(WeaponRoot);
    Muzzle->SetRelativeLocation(FVector(59, 0, 0));
    MuzzleFlash = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MuzzleFlash"));
    MuzzleFlash->SetupAttachment(Muzzle);
    MuzzleFlash->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MuzzleFlash->SetVisibility(false);
}

void ADemoWeapon::BuildVisuals()
{
    if (!VisualData) return;
    for (const auto& Part : VisualData->Parts)
    {
        if (!Part.Mesh) continue;
        auto* Mesh = NewObject<UStaticMeshComponent>(this, Part.Name);
        USceneComponent* Parent = WeaponRoot;
        switch (Part.Attachment)
        {
        case EDemoWeaponAttachment::Magazine: Parent = Magazine; break;
        case EDemoWeaponAttachment::Bolt: Parent = Bolt; break;
        case EDemoWeaponAttachment::ChargingHandle: Parent = ChargingHandle; break;
        case EDemoWeaponAttachment::Trigger: Parent = Trigger; break;
        case EDemoWeaponAttachment::MuzzleFlash: Parent = MuzzleFlash; break;
        default: break;
        }
        Mesh->SetupAttachment(Parent);
        Mesh->SetStaticMesh(Part.Mesh);
        if (Part.Material) Mesh->SetMaterial(0, Part.Material);
        Mesh->SetRelativeTransform(Part.Transform);
        Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Mesh->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
        Mesh->SetOnlyOwnerSee(true);
        Mesh->CastShadow = false;
        Mesh->RegisterComponent();
        VisualParts.Add(Mesh);
    }
    Muzzle->SetRelativeLocation(VisualData->Muzzle);
    MuzzleFlash->SetVisibility(false, true);
    auto* Marking = NewObject<UTextRenderComponent>(this, TEXT("ReceiverMarking"));
    Marking->SetupAttachment(WeaponRoot);
    Marking->SetText(FText::FromString(TEXT("R17 / KESTREL")));
    Marking->SetWorldSize(0.85f);
    Marking->SetTextRenderColor(FColor(180, 190, 195));
    Marking->SetRelativeLocationAndRotation(FVector(-7, -3.08, -1.1), FRotator(0, -90, 0));
    Marking->SetFirstPersonPrimitiveType(EFirstPersonPrimitiveType::FirstPerson);
    Marking->SetOnlyOwnerSee(true);
    Marking->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Marking->RegisterComponent();
}
void ADemoWeapon::BeginPlay()
{
    Super::BeginPlay();
    Ammo = FMath::Max(1, MagazineSize);
    Reserve = FMath::Max(0, StartingReserve);
    BuildVisuals();
}
void ADemoWeapon::SetTrigger(bool bDown)
{
    bTriggerHeld = bDown;
    if (bDown) TryFire();
}
void ADemoWeapon::Tick(float Dt)
{
    Super::Tick(Dt);
    // Exact critically damped return: same recovery curve at 30/60/144 fps.
    const float Response = 28.f;
    const float Decay = FMath::Exp(-Response * Dt);
    const float Change = RecoilVelocity + Response * Recoil;
    Recoil = (Recoil + Change * Dt) * Decay;
    RecoilVelocity = (RecoilVelocity - Response * Change * Dt) * Decay;
    Flash = FMath::Max(0.f, Flash - Dt);
    MuzzleFlash->SetVisibility(Flash > 0, true);
    const float Cycle = FMath::Sin(PI * FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - LastShotTime) / 0.085), 0.f, 1.f));
    float BoltPull = Cycle;
    if (IsReloading())
    {
        const float Progress = GetReloadProgress();
        const float Drop = FMath::Sin(FMath::Clamp((Progress - 0.13f) / 0.62f, 0.f, 1.f) * PI);
        Magazine->SetRelativeLocation(FVector(8 - 3.f * Drop, -9.f * Drop, -5.f - 11.f * Drop));
        Magazine->SetRelativeRotation(FRotator(0, 0, -24.f * Drop));
        BoltPull = FMath::Sin(FMath::Clamp((Progress - 0.78f) / 0.17f, 0.f, 1.f) * PI);
    }
    Bolt->SetRelativeLocation(FVector(-3.5f * BoltPull, 0, 0));
    ChargingHandle->SetRelativeLocation(FVector(-4.5f * (IsReloading() ? BoltPull : Cycle * 0.3f), 0, 0));
    Trigger->SetRelativeRotation(FRotator(Flash > 0 ? -14.f : 0.f, 0, 0));
    if (bTriggerHeld) TryFire();
}
void ADemoWeapon::TryFire()
{
    auto* Character = Cast<ADemoCharacter>(GetOwner());
    const double Now = GetWorld()->GetTimeSeconds();
    if (!Character || !Character->CanFire() || IsReloading() || Now < NextShotTime) return;
    if (Ammo <= 0)
    {
        bTriggerHeld = false;
        if (EmptySound) UGameplayStatics::PlaySound2D(this, EmptySound);
        Reload();
        return;
    }
    const float ShotInterval = 60.f / FMath::Max(60.f, RoundsPerMinute);
    if (Now - NextShotTime > ShotInterval) NextShotTime = Now;
    NextShotTime += ShotInterval;
    --Ammo;
    Recoil = FMath::Min(Recoil + 0.55f, 1.6f);
    RecoilVelocity += 8.f;
    Flash = 0.035f;
    MuzzleFlash->SetVisibility(true, true);
    auto* Camera = Character->GetFirstPersonCamera();
    const FVector Start = Camera->GetComponentLocation();
    const float Aim = Character->GetAimAlpha();
    const float SpreadDegrees = FMath::Lerp(0.45f, 0.012f, Aim) + Character->GetVelocity().Size2D() * FMath::Lerp(0.00035f, 0.000015f, Aim);
    // Control rotation is authoritative. Camera/viewmodel animation never steers bullets.
    const FVector Direction = FMath::VRandCone(Character->GetBaseAimRotation().Vector(), FMath::DegreesToRadians(SpreadDegrees));
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DemoWeaponTrace), true, Character);
    Query.AddIgnoredActor(this);
    FHitResult Hit;
    const bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, Start + Direction * Range, ECC_Visibility, Query);
    // A second trace prevents the camera from firing through cover in front of the barrel.
    FHitResult BarrelHit;
    const FVector AimPoint = bHit ? Hit.ImpactPoint : Start + Direction * Range;
    if (GetWorld()->LineTraceSingleByChannel(BarrelHit, Muzzle->GetComponentLocation(), AimPoint, ECC_Visibility, Query)) Hit = BarrelHit;
    const bool bFinalHit = Hit.bBlockingHit;
    if (bFinalHit && Hit.GetActor())
    {
        auto* Target = Cast<ADemoTarget>(Hit.GetActor());
        const bool bWasActive = Target && Target->IsActive();
        UGameplayStatics::ApplyPointDamage(Hit.GetActor(), Damage, Direction, Hit, Character->GetController(), this, UDamageType::StaticClass());
        if (bWasActive)
        {
            LastHitTime = Now;
            bLastHitKill = !Target->IsActive();
            if (bLastHitKill) Character->RecordElimination();
        }
        if (Hit.GetComponent() && Hit.GetComponent()->IsSimulatingPhysics()) Hit.GetComponent()->AddImpulseAtLocation(Direction * 12000.f, Hit.ImpactPoint);
        if (bDrawDebugTraces) DrawDebugPoint(GetWorld(), Hit.ImpactPoint + Hit.ImpactNormal, 6.f, FColor(255, 190, 95), false, 0.12f);
    }
    if (bDrawDebugTraces) DrawDebugLine(GetWorld(), Muzzle->GetComponentLocation(), bFinalHit ? Hit.ImpactPoint : AimPoint, FColor(255, 205, 115), false, 0.035f, 0, 0.5f);
    if (FireSound) UGameplayStatics::PlaySound2D(this, FireSound, 0.65f, FMath::FRandRange(0.97f, 1.03f));
    if (Now - LastShotTime > BurstResetSeconds) BurstIndex = 0;
    const FVector2D PatternKick = RecoilPattern.IsEmpty() ? FVector2D(0, 0.52) : RecoilPattern[BurstIndex % RecoilPattern.Num()];
    Character->ApplyShotRecoil(PatternKick * FMath::Lerp(1.f, ADSRecoilMultiplier, Aim));
    ++BurstIndex;
    LastShotTime = Now;
    OnShot(Hit, bFinalHit);
}
bool ADemoWeapon::Reload()
{
    if (IsReloading() || Ammo >= MagazineSize || Reserve <= 0) return false;
    State = EDemoWeaponState::Reloading;
    BurstIndex = 0;
    ReloadStart = GetWorld()->GetTimeSeconds();
    bTriggerHeld = false;
    if (ReloadSound) UGameplayStatics::PlaySound2D(this, ReloadSound);
    GetWorldTimerManager().SetTimer(ReloadTimer, this, &ThisClass::FinishReload, FMath::Max(0.1f, ReloadSeconds), false);
    OnReloadStarted();
    return true;
}
void ADemoWeapon::FinishReload()
{
    const int32 Transfer = FMath::Min(FMath::Max(0, MagazineSize - Ammo), Reserve);
    Ammo += Transfer; Reserve -= Transfer;
    State = EDemoWeaponState::Ready;
    Magazine->SetRelativeLocation(FVector(8, 0, -5));
    Magazine->SetRelativeRotation(FRotator::ZeroRotator);
    OnReloadFinished();
}
float ADemoWeapon::GetReloadProgress() const { return IsReloading() ? FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - ReloadStart) / FMath::Max(0.1f, ReloadSeconds)), 0.f, 1.f) : 0.f; }
float ADemoWeapon::GetHitAge() const { return static_cast<float>(GetWorld()->GetTimeSeconds() - LastHitTime); }
void ADemoWeapon::EndPlay(const EEndPlayReason::Type Reason) { GetWorldTimerManager().ClearTimer(ReloadTimer); Super::EndPlay(Reason); }

FTransform ADemoWeapon::GetAimPose() const
{
    const FTransform Sight = VisualData ? VisualData->Sight : FTransform(FVector(2, 0, 8.2));
    const float Relief = VisualData ? VisualData->EyeRelief : 24.f;
    const FQuat Rotation = Sight.GetRotation().Inverse();
    return FTransform(Rotation, FVector(Relief, 0, 0) - Rotation.RotateVector(Sight.GetLocation()));
}
FVector ADemoWeapon::GetHipPosition() const { return VisualData ? VisualData->HipPosition : FVector(25, 12, -16); }
FTransform ADemoWeapon::GetHandTarget(bool bLeft) const
{
    if (!VisualData) return GetActorTransform();
    if (!bLeft) return VisualData->RightHand * GetActorTransform();
    FTransform Local = VisualData->LeftHand;
    if (IsReloading())
    {
        const float P = GetReloadProgress();
        const float Reach = FMath::SmoothStep(0.04f, 0.16f, P) * (1.f - FMath::SmoothStep(0.70f, 0.83f, P));
        FTransform Grab = VisualData->MagazineHand;
        Grab.SetLocation(Grab.GetLocation() - FVector(8, 0, -5));
        const FTransform MagazineLocal = Grab * Magazine->GetRelativeTransform();
        Local.Blend(VisualData->LeftHand, MagazineLocal, Reach);
        const float Handle = FMath::Sin(FMath::Clamp((P - 0.79f) / 0.18f, 0.f, 1.f) * PI);
        FTransform HandlePose(VisualData->LeftHand.GetRotation(), FVector(-3.f - Handle * 4.5f, -4.5f, 3));
        FTransform Before = Local;
        Local.Blend(Before, HandlePose, FMath::Clamp(Handle * 1.5f, 0.f, 1.f));
    }
    return Local * GetActorTransform();
}
