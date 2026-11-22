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
    BoxMesh = Cube.Object;
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
    StairLight = CreateDefaultSubobject<URectLightComponent>(TEXT("StairLight"));
    StairLight->SetupAttachment(RootComponent); StairLight->SetMobility(EComponentMobility::Movable);
    StairLight->SetRelativeRotation(FRotator(-90,0,0)); StairLight->SetIntensityUnits(ELightUnits::Lumens);
    StairLight->SetCastShadows(false);
    for (int32 I=0; I<6; ++I)
    {
        auto* Seal = CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("ConnectionSeal%d"),I));
        Seal->SetupAttachment(RootComponent); Seal->SetMobility(EComponentMobility::Movable);
        Seal->SetStaticMesh(BoxMesh); Seal->SetCollisionProfileName(TEXT("BlockAll"));
        Seal->SetCanEverAffectNavigation(false); Seal->ComponentTags.Add(TEXT("NoGrapple"));
        ConnectionSeals.Add(Seal);
    }
    for (int32 I=0; I<5; ++I)
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
void ADemoEnvironmentChunk::Generate(int32 Seed,FIntVector Cell,double Size,double LayerHeight,bool bTargets,TSubclassOf<ADemoTarget> TargetClass,
    const FDemoEnvironmentPalette& Palette)
{
    using namespace DemoEnvironment;
    const auto Layout = DemoEnvironment::Generate(Seed,Cell,Size,LayerHeight);
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
    UMaterialInterface* Surfaces[] = {Primary,Secondary,Accent,Palette.Glass.Get(),Energy};
    if (Palette.Floor) Ground->SetMaterial(0,Palette.Floor);
    TArray<FTransform> FloorTransforms;
    for (const auto& Floor : Layout.Floors) FloorTransforms.Emplace(Floor.Rotation,Floor.Center,Floor.Size/100.);
    Ground->AddInstances(FloorTransforms,false,false,false);
    TMap<uint8,TArray<FTransform>> Transforms;
    for (const auto& Block : Layout.Blocks)
    {
        const uint8 Key = uint8(Block.Surface)*2+uint8(Block.bCollision);
        if (!Batches.Contains(Key))
        {
            auto* Mesh = NewObject<UHierarchicalInstancedStaticMeshComponent>(this,*FString::Printf(TEXT("RoomBatch%d"),Key));
            Mesh->SetupAttachment(RootComponent);
            Mesh->SetMobility(EComponentMobility::Static);
            Mesh->SetStaticMesh(BoxMesh);
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
    RoomLight->SetIntensity(4000000.f*FMath::Square(float(Size/4000.)));
    RoomLight->SetLightColor(Layout.LightColor);
    RoomLight->SetVisibility(true);
    const FVector StairCenter = Layout.DescentSign.GetRotation().RotateVector(FVector(-950,-1000,0));
    // Use the same column rotation as the descent, offset from the room center.
    StairLight->SetRelativeLocation(FVector(Size*.5,Size*.5,-LayerHeight*.5+300)+StairCenter*(Size/4000.));
    StairLight->SetSourceWidth(800*Size/4000.); StairLight->SetSourceHeight(900*Size/4000.);
    StairLight->SetAttenuationRadius(LayerHeight*1.8); StairLight->SetIntensity(300000.f);
    StairLight->SetLightColor(Layout.LightColor);
    const FVector Positions[] = {FVector(140*Size/4000.,Size*.5,440),FVector(Size-140*Size/4000.,Size*.5,440),FVector(Size*.5,140*Size/4000.,440),FVector(Size*.5,Size-140*Size/4000.,440)};
    const float Headings[] = {0,180,90,-90};
    const FText Label = FText::FromString(FString::Printf(TEXT("%s / DECK %03d"),RoomName(Layout.Room),-Cell.Z));
    for (int32 I=0; I<4; ++I)
    {
        Signs[I]->SetText(Label);
        Signs[I]->SetWorldSize(52.f*float(Size/4000.));
        Signs[I]->SetRelativeLocationAndRotation(Positions[I],FRotator(0,Headings[I],0));
    }
    Signs[4]->SetText(FText::FromString(FString::Printf(TEXT("DOWN / DECK %03d"),1-Cell.Z)));
    Signs[4]->SetWorldSize(40.f*float(Size/4000.));
    Signs[4]->SetRelativeTransform(Layout.DescentSign);
    const FVector CapPositions[] = {FVector(70*Size/4000.,Size*.5,180),FVector(Size-70*Size/4000.,Size*.5,180),FVector(Size*.5,70*Size/4000.,180),FVector(Size*.5,Size-70*Size/4000.,180)};
    for (int32 I=0; I<4; ++I)
    {
        ConnectionSeals[I]->SetRelativeLocation(CapPositions[I]);
        ConnectionSeals[I]->SetRelativeScale3D(I<2?FVector(1.2*Size/4000.,8.2*Size/4000.,3.8):FVector(8.2*Size/4000.,1.2*Size/4000.,3.8));
    }
    ConnectionSeals[4]->SetRelativeTransform(Layout.UpperSeal);
    ConnectionSeals[5]->SetRelativeTransform(Layout.LowerSeal);
    for (auto* Seal : {ConnectionSeals[0].Get(),ConnectionSeals[1].Get(),ConnectionSeals[2].Get(),ConnectionSeals[3].Get(),ConnectionSeals[4].Get(),ConnectionSeals[5].Get()})
        if (Primary) Seal->SetMaterial(0,Primary);
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
void ADemoEnvironmentChunk::SetConnection(int32 Direction,bool bConnected)
{
    if (!ConnectionSeals.IsValidIndex(Direction)) return;
    auto* Seal = ConnectionSeals[Direction].Get();
    Seal->SetVisibility(!bConnected);
    Seal->SetCollisionEnabled(bConnected?ECollisionEnabled::NoCollision:ECollisionEnabled::QueryAndPhysics);
}
void ADemoEnvironmentChunk::EndPlay(const EEndPlayReason::Type Reason)
{
    for (const auto& Target : Targets) if (IsValid(Target)) Target->Destroy();
    Super::EndPlay(Reason);
}
