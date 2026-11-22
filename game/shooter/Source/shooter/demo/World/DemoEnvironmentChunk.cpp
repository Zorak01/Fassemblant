#include "demo/World/DemoEnvironmentChunk.h"
#include "demo/World/DemoEnvironmentLayout.h"
#include "demo/World/DemoTarget.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/RectLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/World.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ADemoEnvironmentChunk::ADemoEnvironmentChunk()
{
    PrimaryActorTick.bCanEverTick = false;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("ChunkRoot")));
    RootComponent->SetMobility(EComponentMobility::Static);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    BoxMesh = Cube.Object; CylinderMesh = Cylinder.Object; SphereMesh = Sphere.Object;
    Ground = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(TEXT("Ground"));
    Ground->SetupAttachment(RootComponent);
    Ground->SetStaticMesh(BoxMesh);
    Ground->SetMobility(EComponentMobility::Static);
    Ground->SetCollisionProfileName(TEXT("BlockAll"));
    Ground->SetCanEverAffectNavigation(false);
    RoomLight = CreateDefaultSubobject<URectLightComponent>(TEXT("RoomLight"));
    RoomLight->SetupAttachment(RootComponent);
    RoomLight->SetMobility(EComponentMobility::Movable);
    RoomLight->SetRelativeRotation(FRotator(-90,0,0));
    RoomLight->SetIntensityUnits(ELightUnits::Lumens);
    RoomLight->SetCastShadows(false);
    RoomLight->SetVisibility(false);
    for (int32 I=0; I<4; ++I)
    {
        auto* Sign = CreateDefaultSubobject<UTextRenderComponent>(*FString::Printf(TEXT("DoorSign%d"),I));
        Sign->SetupAttachment(RootComponent);
        Sign->SetMobility(EComponentMobility::Movable);
        Sign->SetHorizontalAlignment(EHTA_Center);
        Sign->SetVerticalAlignment(EVRTA_TextCenter);
        Sign->SetTextRenderColor(FColor(215,237,242));
        Sign->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Sign->SetCullDistance(14000.f);
        Signs.Add(Sign);
    }
}
void ADemoEnvironmentChunk::Generate(int32 Seed,FIntPoint Cell,double Size,bool bTargets,TSubclassOf<ADemoTarget> TargetClass,
    const FDemoEnvironmentPalette& Palette)
{
    using namespace DemoEnvironment;
    const auto Layout = DemoEnvironment::Generate(Seed,Cell,Size);
    UMaterialInterface* Primary = Palette.Structure;
    UMaterialInterface* Secondary = Palette.Secondary;
    UMaterialInterface* Accent = Palette.Accent;
    UMaterialInterface* Energy = Palette.Light;
    switch (Layout.Theme)
    {
    case ETheme::Arctic: Primary = Palette.Secondary; Secondary = Palette.Structure; break;
    case ETheme::Industrial: Accent = Palette.WarmAccent; break;
    case ETheme::Botanical: Primary = Palette.Secondary; break;
    case ETheme::Hazard: Accent = Palette.HazardAccent; Energy = Palette.Heat; break;
    default: break;
    }
    if (Layout.Room == ERoomType::Foundry) Energy = Palette.Heat;
    UMaterialInterface* Surfaces[] = {Primary,Secondary,Accent,Palette.Glass.Get(),Energy,Palette.Accent.Get()};
    if (Palette.Floor) Ground->SetMaterial(0,Palette.Floor);
    Ground->AddInstance(FTransform(FQuat::Identity,FVector(Size*.5,Size*.5,-15),FVector(Size/100,Size/100,.3)));
    TMap<uint8,TArray<FTransform>> Transforms;
    for (const auto& Block : Layout.Blocks)
    {
        const uint8 Key = (uint8(Block.Surface)*3+uint8(Block.Shape))*2+uint8(Block.bCollision);
        if (!Batches.Contains(Key))
        {
            auto* Mesh = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,*FString::Printf(TEXT("RoomBatch%d"),Key));
            Mesh->SetupAttachment(RootComponent);
            Mesh->SetMobility(EComponentMobility::Static);
            Mesh->SetStaticMesh(Block.Shape == EShape::Cylinder ? CylinderMesh.Get() : Block.Shape == EShape::Sphere ? SphereMesh.Get() : BoxMesh.Get());
            if (Surfaces[uint8(Block.Surface)]) Mesh->SetMaterial(0,Surfaces[uint8(Block.Surface)]);
            Mesh->SetCollisionProfileName(TEXT("BlockAll"));
            if (!Block.bCollision) Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Mesh->SetCanEverAffectNavigation(false);
            Mesh->RegisterComponent();
            Batches.Add(Key,Mesh);
        }
        Transforms.FindOrAdd(Key).Emplace(Block.Rotation,Block.Center,Block.Size/100.);
    }
    for (const auto& Batch : Transforms) Batches[Batch.Key]->AddInstances(Batch.Value,false,false,false);
    RoomLight->SetRelativeLocation(FVector(Size*.5,Size*.5,Layout.CeilingHeight-160));
    RoomLight->SetSourceWidth(Size*.65); RoomLight->SetSourceHeight(Size*.65);
    RoomLight->SetAttenuationRadius(Size*.8);
    RoomLight->SetIntensity(4000000.f*FMath::Square(float(Size/6000.)));
    RoomLight->SetLightColor(Layout.LightColor);
    RoomLight->SetVisibility(Layout.bCovered);
    const FVector Positions[] = {FVector(80,Size*.5,460),FVector(Size-80,Size*.5,460),FVector(Size*.5,80,460),FVector(Size*.5,Size-80,460)};
    const float Headings[] = {180,0,-90,90};
    const FText Label = FText::FromString(FString::Printf(TEXT("%s / %02u"),RoomName(Layout.Room),CellSeed(Seed,Cell)%100));
    for (int32 I=0; I<Signs.Num(); ++I)
    {
        Signs[I]->SetText(Label);
        Signs[I]->SetWorldSize(52.f*float(Size/6000.));
        Signs[I]->SetRelativeLocationAndRotation(Positions[I],FRotator(0,Headings[I],0));
    }
    if (!bTargets || !TargetClass) return;
    for (const auto& Spawn : Layout.Targets)
    {
        FActorSpawnParameters Params; Params.Owner = this;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        auto* Target = GetWorld()->SpawnActor<ADemoTarget>(TargetClass,GetActorLocation()+Spawn.Position,Spawn.Rotation,Params);
        if (!Target) continue;
        Target->StrafeDirection = Spawn.StrafeDirection;
        Target->StrafeDistance = Spawn.StrafeDistance; Target->StrafeSpeed = Spawn.StrafeSpeed;
        Targets.Add(Target);
        TArray<UStaticMeshComponent*> Parts; Target->GetComponents(Parts);
        for (auto* Part : Parts)
        {
            UMaterialInterface* Material = Part->GetFName() == TEXT("Bullseye") || Part->GetFName() == TEXT("HealthBar") ? Accent : Secondary;
            if (Material) Part->SetMaterial(0,Material);
        }
    }
}
void ADemoEnvironmentChunk::EndPlay(const EEndPlayReason::Type Reason)
{
    for (const auto& Target : Targets) if (IsValid(Target)) Target->Destroy();
    Super::EndPlay(Reason);
}
