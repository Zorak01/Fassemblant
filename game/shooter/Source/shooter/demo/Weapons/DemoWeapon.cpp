#include "demo/Weapons/DemoWeapon.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/World/DemoTarget.h"
#include "Camera/CameraComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ADemoWeapon::ADemoWeapon()
{
    PrimaryActorTick.bCanEverTick = true;
    WeaponRoot = CreateDefaultSubobject<USceneComponent>(TEXT("WeaponRoot"));
    SetRootComponent(WeaponRoot);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial"));
    CubeMesh = Cube.Object; CylinderMesh = Cylinder.Object; WeaponMaterial = Material.Object;
    Part(TEXT("Receiver"), FVector(2, 0, 0), FVector(0.30, 0.055, 0.075));
    Part(TEXT("Handguard"), FVector(23, 0, 0), FVector(0.23, 0.048, 0.055));
    Part(TEXT("Barrel"), FVector(40, 0, 0), FVector(0.022, 0.022, 0.25), true)->SetRelativeRotation(FRotator(90, 0, 0));
    Part(TEXT("Suppressor"), FVector(54, 0, 0), FVector(0.045, 0.045, 0.16), true)->SetRelativeRotation(FRotator(90, 0, 0));
    Part(TEXT("Stock"), FVector(-20, 0, -1), FVector(0.18, 0.04, 0.085));
    Part(TEXT("Grip"), FVector(-3, 0, -7), FVector(0.045, 0.04, 0.10))->SetRelativeRotation(FRotator(-18, 0, 0));
    Magazine = Part(TEXT("Magazine"), FVector(9, 0, -8), FVector(0.065, 0.045, 0.13));
    Part(TEXT("OpticMount"), FVector(0, 0, 4.8), FVector(0.09, 0.04, 0.018));
    // Open reflex sight: frame surrounds the camera's aiming axis at full ADS.
    Part(TEXT("OpticLeft"), FVector(0, -2.7, 8), FVector(0.016, 0.009, 0.065));
    Part(TEXT("OpticRight"), FVector(0, 2.7, 8), FVector(0.016, 0.009, 0.065));
    Part(TEXT("OpticTop"), FVector(0, 0, 11), FVector(0.016, 0.062, 0.009));
    Muzzle = CreateDefaultSubobject<USceneComponent>(TEXT("Muzzle"));
    Muzzle->SetupAttachment(WeaponRoot);
    Muzzle->SetRelativeLocation(FVector(63, 0, 0));
    MuzzleFlash = Part(TEXT("MuzzleFlash"), FVector(64, 0, 0), FVector(0.07, 0.015, 0.015));
    MuzzleFlash->SetVisibility(false);
}

UStaticMeshComponent* ADemoWeapon::Part(FName Name, FVector Position, FVector Scale, bool bCylinder)
{
    auto* Mesh = CreateDefaultSubobject<UStaticMeshComponent>(Name);
    Mesh->SetupAttachment(WeaponRoot);
    Mesh->SetStaticMesh(bCylinder ? CylinderMesh : CubeMesh);
    Mesh->SetMaterial(0, WeaponMaterial);
    Mesh->SetRelativeLocation(Position);
    Mesh->SetRelativeScale3D(Scale);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->CastShadow = false;
    Mesh->SetOnlyOwnerSee(true);
    return Mesh;
}
void ADemoWeapon::BeginPlay()
{
    Super::BeginPlay();
    Ammo = FMath::Max(1, MagazineSize);
    Reserve = FMath::Max(0, StartingReserve);
}
void ADemoWeapon::SetTrigger(bool bDown)
{
    bTriggerHeld = bDown;
    if (bDown) TryFire();
}
void ADemoWeapon::Tick(float Dt)
{
    Super::Tick(Dt);
    Recoil = FMath::FInterpTo(Recoil, 0.f, Dt, 15.f);
    Flash = FMath::Max(0.f, Flash - Dt);
    MuzzleFlash->SetVisibility(Flash > 0);
    if (IsReloading())
    {
        const float Progress = GetReloadProgress();
        const float Drop = FMath::Sin(FMath::Clamp((Progress - 0.12f) / 0.72f, 0.f, 1.f) * PI);
        Magazine->SetRelativeLocation(FVector(9, -5.f * Drop, -8.f - 19.f * Drop));
        Magazine->SetRelativeRotation(FRotator(0, 0, -24.f * Drop));
    }
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
    NextShotTime = Now + 60.f / FMath::Max(60.f, RoundsPerMinute);
    --Ammo;
    Recoil = FMath::Min(Recoil + 0.65f, 2.f);
    Flash = 0.035f;
    auto* Camera = Character->GetFirstPersonCamera();
    const FVector Start = Camera->GetComponentLocation();
    const float SpreadDegrees = FMath::Lerp(0.65f, 0.025f, Character->GetAimAlpha()) + Character->GetVelocity().Size2D() * 0.0006f;
    const FVector Direction = FMath::VRandCone(Camera->GetForwardVector(), FMath::DegreesToRadians(SpreadDegrees));
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
        DrawDebugPoint(GetWorld(), Hit.ImpactPoint + Hit.ImpactNormal, 6.f, FColor(255, 190, 95), false, 0.12f);
    }
    DrawDebugLine(GetWorld(), Muzzle->GetComponentLocation(), bFinalHit ? Hit.ImpactPoint : AimPoint, FColor(255, 205, 115), false, 0.035f, 0, 0.5f);
    if (FireSound) UGameplayStatics::PlaySound2D(this, FireSound, 0.65f, FMath::FRandRange(0.97f, 1.03f));
    Character->ApplyShotRecoil();
    OnShot(Hit, bFinalHit);
}
bool ADemoWeapon::Reload()
{
    if (IsReloading() || Ammo >= MagazineSize || Reserve <= 0) return false;
    State = EDemoWeaponState::Reloading;
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
    Magazine->SetRelativeLocation(FVector(9, 0, -8));
    Magazine->SetRelativeRotation(FRotator::ZeroRotator);
    OnReloadFinished();
}
float ADemoWeapon::GetReloadProgress() const { return IsReloading() ? FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - ReloadStart) / FMath::Max(0.1f, ReloadSeconds)), 0.f, 1.f) : 0.f; }
float ADemoWeapon::GetHitAge() const { return static_cast<float>(GetWorld()->GetTimeSeconds() - LastHitTime); }
void ADemoWeapon::EndPlay(const EEndPlayReason::Type Reason) { GetWorldTimerManager().ClearTimer(ReloadTimer); Super::EndPlay(Reason); }
