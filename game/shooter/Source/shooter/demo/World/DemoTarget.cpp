#include "demo/World/DemoTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ADemoTarget::ADemoTarget()
{
    PrimaryActorTick.bCanEverTick = true;
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetBody"));
    SetRootComponent(Body);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Body->SetStaticMesh(Cube.Object);
    Body->SetRelativeScale3D(FVector(0.15, 0.65, 1.1));
    Body->SetCollisionProfileName(TEXT("BlockAll"));
    Bullseye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bullseye"));
    Bullseye->SetupAttachment(Body);
    Bullseye->SetStaticMesh(Sphere.Object);
    Bullseye->SetAbsolute(false, false, true);
    Bullseye->SetRelativeLocation(FVector(-56, 0, 0));
    Bullseye->SetWorldScale3D(FVector(0.1, 0.28, 0.28));
    Bullseye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ADemoTarget::BeginPlay() { Super::BeginPlay(); Home = GetActorLocation(); Health = MaxHealth; }
float ADemoTarget::TakeDamage(float Amount, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer)
{
    if (!IsActive() || Amount <= 0) return 0;
    const float Applied = FMath::Min(Health, Amount);
    Health -= Applied;
    HitPulse = 1.f;
    if (!IsActive())
    {
        SetActorEnableCollision(false);
        Body->SetVisibility(false, true);
        GetWorldTimerManager().SetTimer(ResetTimer, this, &ThisClass::ResetTarget, ResetSeconds, false);
    }
    return Applied;
}
void ADemoTarget::ResetTarget() { Health = MaxHealth; SetActorEnableCollision(true); Body->SetVisibility(true, true); }
void ADemoTarget::Tick(float Dt)
{
    Super::Tick(Dt);
    MotionTime += Dt;
    HitPulse = FMath::FInterpTo(HitPulse, 0.f, Dt, 12.f);
    SetActorLocation(Home + FVector(0, FMath::Sin(MotionTime * StrafeSpeed) * StrafeDistance, 0));
    SetActorRotation(FRotator(0, 0, HitPulse * 7.f));
}
void ADemoTarget::EndPlay(const EEndPlayReason::Type Reason) { GetWorldTimerManager().ClearTimer(ResetTimer); Super::EndPlay(Reason); }
