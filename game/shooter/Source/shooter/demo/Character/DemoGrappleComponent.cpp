#include "demo/Character/DemoGrappleComponent.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/Character/DemoMovementComponent.h"
#include "demo/Weapons/DemoWeapon.h"
#include "Camera/CameraComponent.h"
#include "CableComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

UDemoGrappleComponent::UDemoGrappleComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/demo/Materials/M_TrainingTeal.M_TrainingTeal"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Launch(TEXT("/Game/demo/Audio/Training/S_R17Empty.S_R17Empty"));
    static ConstructorHelpers::FObjectFinder<USoundBase> Attach(TEXT("/Game/demo/Audio/Training/S_BallisticImpact.S_BallisticImpact"));
    CableMaterial = Material.Object; LaunchSound = Launch.Object; AttachSound = Attach.Object;
}
void UDemoGrappleComponent::BeginPlay()
{
    Super::BeginPlay();
    Character = Cast<ADemoCharacter>(GetOwner());
    if (!Character) { SetComponentTickEnabled(false); return; }
    Movement = CastChecked<UDemoMovementComponent>(Character->GetCharacterMovement());
    Movement->AddTickPrerequisiteComponent(this);
    Cable = NewObject<UCableComponent>(Character,TEXT("GrappleCable"));
    Cable->SetupAttachment(Character->Camera);
    Cable->SetRelativeLocation(LauncherOffset);
    Cable->SetOnlyOwnerSee(true); Cable->SetCastShadow(false);
    Cable->bEnableCollision = false; Cable->bUseSubstepping = true;
    Cable->NumSegments = 16; Cable->NumSides = 4; Cable->CableWidth = 1.4f;
    Cable->SubstepTime = 1.f/120.f; Cable->SolverIterations = 6;
    Cable->CableGravityScale = .25f; Cable->bAttachStart = Cable->bAttachEnd = true;
    Cable->bResetAfterTeleport = true;
    Cable->PrimaryComponentTick.TickGroup = TG_PostPhysics;
    Cable->AddTickPrerequisiteComponent(Movement);
    if (CableMaterial) Cable->SetMaterial(0,CableMaterial);
    Cable->RegisterComponent(); Cable->SetVisibility(false); Cable->SetComponentTickEnabled(false);
    Hook = NewObject<UStaticMeshComponent>(Character,TEXT("GrappleHook"));
    Hook->SetupAttachment(Character->GetRootComponent());
    Hook->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
    Hook->SetRelativeScale3D(FVector(.045,.045,.045));
    Hook->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Hook->SetOnlyOwnerSee(true); Hook->SetCastShadow(false);
    if (CableMaterial) Hook->SetMaterial(0,CableMaterial);
    Hook->RegisterComponent(); Hook->SetVisibility(false);
}
float UDemoGrappleComponent::GetCooldownRemaining() const
{
    return GetWorld() ? float(FMath::Max(0.,CooldownEnd-GetWorld()->GetTimeSeconds())) : 0.f;
}
FVector UDemoGrappleComponent::LauncherPosition() const
{
    return Character->Camera->GetComponentTransform().TransformPosition(LauncherOffset);
}
bool UDemoGrappleComponent::TraceAim(FHitResult& Hit) const
{
    if (!Character || !Movement || Movement->MovementMode == MOVE_None || Movement->IsSwimming()) return false;
    const FVector Start = Character->Camera->GetComponentLocation();
    const FVector End = Start+Character->GetBaseAimRotation().Vector()*MaximumRange;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DemoGrappleAim),true,Character);
    if (Character->GetWeapon()) Query.AddIgnoredActor(Character->GetWeapon());
    if (!GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Query) || !Hit.GetComponent()) return false;
    return Hit.Distance >= MinimumRange && !Hit.GetComponent()->ComponentHasTag(TEXT("NoGrapple"))
        && (!Hit.GetActor() || !Hit.GetActor()->ActorHasTag(TEXT("NoGrapple")));
}
bool UDemoGrappleComponent::RopeBlocked(FVector Anchor) const
{
    const FVector Start = LauncherPosition();
    const FVector End = Anchor+(Start-Anchor).GetSafeNormal()*8.;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(DemoGrappleObstruction),true,Character);
    if (Character->GetWeapon()) Query.AddIgnoredActor(Character->GetWeapon());
    FHitResult Hit;
    // Do not ignore the anchor's entire HISM component: it can also contain an
    // intervening wall. Only a hit very near the actual endpoint is tolerated.
    return GetWorld()->LineTraceSingleByChannel(Hit,Start,End,ECC_Visibility,Query)
        && FVector::DistSquared(Hit.ImpactPoint,Anchor) > FMath::Square(20.);
}
void UDemoGrappleComponent::Toggle()
{
    if (!Character || !Movement) return;
    if (State == EDemoGrappleState::Launching || State == EDemoGrappleState::Attached)
    { BeginRetract(State == EDemoGrappleState::Attached ? CooldownSeconds : .25f); return; }
    if (State != EDemoGrappleState::Ready || GetCooldownRemaining() > 0) return;
    FHitResult Hit;
    if (!TraceAim(Hit)) return;
    AnchorComponent = Hit.GetComponent();
    LocalAnchor = AnchorComponent->GetComponentTransform().InverseTransformPosition(Hit.ImpactPoint);
    FlightStart = HookPosition = LauncherPosition();
    FlightSeconds = FMath::Max(.04f,float(FVector::Distance(FlightStart,Hit.ImpactPoint))/FMath::Max(1000.f,HookTravelSpeed));
    PhaseStart = GetWorld()->GetTimeSeconds(); LookAwaySeconds = 0;
    State = EDemoGrappleState::Launching; bHasAimTarget = false;
    Cable->SetAttachEndToComponent(nullptr);
    Cable->SetVisibility(true); Cable->SetComponentTickEnabled(true); Hook->SetVisibility(true);
    if (LaunchSound) UGameplayStatics::PlaySound2D(this,LaunchSound,.55f,1.25f);
    UpdateVisuals();
}
void UDemoGrappleComponent::BeginRetract(float Cooldown)
{
    if (Movement) Movement->EndGrapple();
    AnchorComponent.Reset();
    State = EDemoGrappleState::Retracting;
    CooldownEnd = GetWorld()->GetTimeSeconds()+FMath::Max(0.f,Cooldown);
    PhaseStart = GetWorld()->GetTimeSeconds();
    if (Cable) Cable->SetAttachEndToComponent(nullptr);
}
void UDemoGrappleComponent::TickComponent(float Dt,ELevelTick TickType,FActorComponentTickFunction* ThisTick)
{
    Super::TickComponent(Dt,TickType,ThisTick);
    if (!Character || !Movement) return;
    const double Now = GetWorld()->GetTimeSeconds();
    if (State == EDemoGrappleState::Ready)
    {
        FHitResult Hit;
        bHasAimTarget = GetCooldownRemaining() <= 0 && TraceAim(Hit);
        if (bHasAimTarget) AimPoint = Hit.ImpactPoint;
        return;
    }
    if (State == EDemoGrappleState::Launching || State == EDemoGrappleState::Attached)
    {
        if (!AnchorComponent.IsValid() || !AnchorComponent->IsQueryCollisionEnabled()
            || (AnchorComponent->GetOwner() && !AnchorComponent->GetOwner()->GetActorEnableCollision()))
            BeginRetract(State == EDemoGrappleState::Attached ? CooldownSeconds : .25f);
        else
        {
            const FVector Anchor = AnchorComponent->GetComponentTransform().TransformPosition(LocalAnchor);
            if (RopeBlocked(Anchor)) BeginRetract(State == EDemoGrappleState::Attached ? CooldownSeconds : .25f);
            else if (State == EDemoGrappleState::Launching)
            {
                const double Alpha = FMath::Clamp((Now-PhaseStart)/FlightSeconds,0.,1.);
                HookPosition = FMath::Lerp(FlightStart,Anchor,Alpha);
                if (Alpha >= 1)
                {
                    if (!Movement->StartGrapple(AnchorComponent.Get(),LocalAnchor)) BeginRetract(.25f);
                    else
                    {
                        State = EDemoGrappleState::Attached; PhaseStart = Now;
                        Cable->SetAttachEndToComponent(AnchorComponent.Get()); Cable->EndLocation = LocalAnchor;
                        if (AttachSound) UGameplayStatics::PlaySound2D(this,AttachSound,.5f,1.1f);
                    }
                }
            }
            else
            {
                HookPosition = Anchor;
                const FVector Toward = (Anchor-Character->Camera->GetComponentLocation()).GetSafeNormal();
                const double Facing = FVector::DotProduct(Toward,Character->GetBaseAimRotation().Vector());
                LookAwaySeconds = Facing < FMath::Cos(FMath::DegreesToRadians(MaximumViewAngle)) ? LookAwaySeconds+Dt : 0;
                if (!Movement->IsGrappling() || Now-PhaseStart > MaximumAttachSeconds || LookAwaySeconds > LookAwayGraceSeconds
                    || FVector::DistSquared(Character->GetActorLocation(),Anchor) > FMath::Square(double(MaximumRange)*1.5)) BeginRetract(CooldownSeconds);
            }
        }
    }
    if (State == EDemoGrappleState::Retracting)
    {
        HookPosition = FMath::VInterpConstantTo(HookPosition,LauncherPosition(),Dt,HookTravelSpeed*1.5f);
        if (FVector::DistSquared(HookPosition,LauncherPosition()) < FMath::Square(20.) || Now-PhaseStart > .4)
        {
            State = EDemoGrappleState::Ready;
            Cable->SetVisibility(false); Cable->SetComponentTickEnabled(false); Hook->SetVisibility(false);
        }
    }
    UpdateVisuals();
}
void UDemoGrappleComponent::UpdateVisuals()
{
    if (!Cable || !Hook || State == EDemoGrappleState::Ready) return;
    Hook->SetWorldLocation(HookPosition);
    if (State != EDemoGrappleState::Attached) Cable->EndLocation = Cable->GetComponentTransform().InverseTransformPosition(HookPosition);
    Cable->CableLength = float(FVector::Distance(LauncherPosition(),HookPosition))*(State == EDemoGrappleState::Attached ? 1.01f : 1.03f);
}
void UDemoGrappleComponent::ResetGrapple()
{
    if (Movement) Movement->EndGrapple();
    State = EDemoGrappleState::Ready; CooldownEnd = 0; bHasAimTarget = false; AnchorComponent.Reset();
    if (Cable) { Cable->SetVisibility(false); Cable->SetComponentTickEnabled(false); Cable->SetAttachEndToComponent(nullptr); }
    if (Hook) Hook->SetVisibility(false);
}
void UDemoGrappleComponent::ApplyWorldOffset(const FVector& Offset,bool bWorldShift)
{
    Super::ApplyWorldOffset(Offset,bWorldShift);
    FlightStart += Offset; HookPosition += Offset; AimPoint += Offset;
}
void UDemoGrappleComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    if (Movement) { Movement->RemoveTickPrerequisiteComponent(this); Movement->EndGrapple(); }
    if (Cable) Cable->DestroyComponent();
    if (Hook) Hook->DestroyComponent();
    Super::EndPlay(Reason);
}
