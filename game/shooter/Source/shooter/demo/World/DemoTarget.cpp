#include "demo/World/DemoTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ADemoTarget::ADemoTarget()
{
    PrimaryActorTick.bCanEverTick = true;
    TargetRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TargetRoot"));
    SetRootComponent(TargetRoot);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TargetBody"));
    Body->SetupAttachment(TargetRoot);
    Body->SetStaticMesh(Cube.Object);
    Body->SetRelativeScale3D(FVector(0.18, 0.6, 0.8));
    Body->SetCollisionProfileName(TEXT("BlockAll"));
    Bullseye = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Bullseye"));
    Bullseye->SetupAttachment(TargetRoot);
    Bullseye->SetStaticMesh(Sphere.Object);
    Bullseye->SetRelativeLocation(FVector(-11, 0, 8));
    Bullseye->SetRelativeScale3D(FVector(0.045, 0.23, 0.23));
    Bullseye->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
    Head->SetupAttachment(TargetRoot);
    Head->SetStaticMesh(Sphere.Object);
    Head->SetRelativeLocation(FVector(0, 0, 62));
    Head->SetRelativeScale3D(FVector(.22, .28, .3));
    Head->SetCollisionProfileName(TEXT("BlockAll"));
    Stand = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Stand"));
    Stand->SetupAttachment(TargetRoot);
    Stand->SetStaticMesh(Cube.Object);
    Stand->SetRelativeLocation(FVector(4, 0, -55));
    Stand->SetRelativeScale3D(FVector(.12, .15, .5));
    Stand->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HealthBar = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HealthBar"));
    HealthBar->SetupAttachment(TargetRoot);
    HealthBar->SetStaticMesh(Cube.Object);
    HealthBar->SetRelativeLocation(FVector(-12, 0, 91));
    HealthBar->SetRelativeScale3D(FVector(.025, .6, .035));
    HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}
void ADemoTarget::BeginPlay() { Super::BeginPlay(); Home = GetActorLocation(); Health = MaxHealth; }
FVector ADemoTarget::GetHeadLocation() const { return Head->GetComponentLocation(); }
float ADemoTarget::TakeDamage(float Amount, const FDamageEvent& Event, AController* EventInstigator, AActor* Causer)
{
    if (!IsActive() || Amount <= 0) return 0;
    bHeadshot = Event.IsOfType(FPointDamageEvent::ClassID) && static_cast<const FPointDamageEvent&>(Event).HitInfo.GetComponent() == Head;
    const float Applied = FMath::Min(Health, Amount * (bHeadshot ? 2.f : 1.f));
    Health -= Applied;
    HitPulse = 1.f;
    if (!IsActive())
    {
        SetActorEnableCollision(false);
        TargetRoot->SetVisibility(false, true);
        GetWorldTimerManager().SetTimer(ResetTimer, this, &ThisClass::ResetTarget, ResetSeconds, false);
    }
    return Applied;
}
void ADemoTarget::ResetTarget()
{
    GetWorldTimerManager().ClearTimer(ResetTimer);
    Health = MaxHealth; bHeadshot = false; HitPulse = 0;
    SetActorEnableCollision(true); TargetRoot->SetVisibility(true, true);
    HealthBar->SetRelativeScale3D(FVector(.025, .6, .035));
}
void ADemoTarget::Tick(float Dt)
{
    Super::Tick(Dt);
    MotionTime += Dt;
    HitPulse = FMath::FInterpTo(HitPulse, 0.f, Dt, 12.f);
    SetActorLocation(Home + FVector(0, FMath::Sin(MotionTime * StrafeSpeed) * StrafeDistance, 0));
    Body->SetRelativeRotation(FRotator(0, 0, HitPulse * 5.f));
    HealthBar->SetRelativeScale3D(FVector(.025, FMath::Max(.001f, .6f * Health / MaxHealth), .035));
}
void ADemoTarget::EndPlay(const EEndPlayReason::Type Reason) { GetWorldTimerManager().ClearTimer(ResetTimer); Super::EndPlay(Reason); }
