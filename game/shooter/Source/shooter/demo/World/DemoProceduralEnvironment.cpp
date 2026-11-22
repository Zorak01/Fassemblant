#include "demo/World/DemoProceduralEnvironment.h"
#include "demo/World/DemoEnvironmentChunk.h"
#include "demo/World/DemoEnvironmentLayout.h"
#include "demo/World/DemoTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Engine/SkyLight.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PawnMovementComponent.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

ADemoProceduralEnvironment::ADemoProceduralEnvironment()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("EnvironmentRoot")));
    TargetClass = ADemoTarget::StaticClass();
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Floor(TEXT("/Game/demo/Materials/M_TrainingFloor.M_TrainingFloor"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Structure(TEXT("/Game/demo/Materials/M_TrainingFrame.M_TrainingFrame"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Accent(TEXT("/Game/demo/Materials/M_TrainingTeal.M_TrainingTeal"));
    GroundMaterial = Floor.Object; StructureMaterial = Structure.Object; AccentMaterial = Accent.Object;
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Secondary(TEXT("/Game/demo/Materials/M_TrainingArmor.M_TrainingArmor"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Warm(TEXT("/Game/demo/Materials/M_TrainingOrange.M_TrainingOrange"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Hazard(TEXT("/Game/demo/Materials/M_TrainingYellow.M_TrainingYellow"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Glass(TEXT("/Game/demo/Materials/M_TrainingWindows.M_TrainingWindows"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Light(TEXT("/Game/demo/Materials/M_TrainingLight.M_TrainingLight"));
    static ConstructorHelpers::FObjectFinder<UMaterialInterface> Heat(TEXT("/Game/demo/Materials/M_MuzzleEmitter.M_MuzzleEmitter"));
    RoomPalette.Secondary = Secondary.Object; RoomPalette.WarmAccent = Warm.Object;
    RoomPalette.HazardAccent = Hazard.Object; RoomPalette.Glass = Glass.Object;
    RoomPalette.Light = Light.Object; RoomPalette.Heat = Heat.Object;
}
void ADemoProceduralEnvironment::BeginPlay()
{
    Super::BeginPlay();
    ChunkSize = FMath::Clamp(ChunkSize, 4000.0, 12000.0);
    LoadRadius = FMath::Clamp(LoadRadius, 2, 5);
    LayerHeight = FMath::Clamp(LayerHeight,1200.,2000.);
    LoadAbove = FMath::Clamp(LoadAbove,1,3); LoadBelow = FMath::Clamp(LoadBelow,2,4);
    ChunksPerUpdate = FMath::Clamp(ChunksPerUpdate, 1, 8);
    // Preserve the original top-level material overrides as palette defaults.
    if (!RoomPalette.Floor) RoomPalette.Floor = GroundMaterial;
    if (!RoomPalette.Structure) RoomPalette.Structure = StructureMaterial;
    if (!RoomPalette.Accent) RoomPalette.Accent = AccentMaterial;
    bPreviousBoundsChecks = GetWorldSettings()->bEnableWorldBoundsChecks;
    GetWorldSettings()->bEnableWorldBoundsChecks = false;
    Tick(0.f);
}
void ADemoProceduralEnvironment::RemoveAuthoredLayout()
{
    // Keep gameplay/player starts/post processing, but remove the outdoor layout
    // and sky sources. Interior cells supply their own lighting and enclosure.
    TArray<AActor*> Placed;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->GetLevel() != GetLevel() || It->GetOwner()) continue;
        if (It->IsA<AStaticMeshActor>() || It->IsA<ATextRenderActor>() || It->IsA<ADemoTarget>() || It->IsA<ASkyAtmosphere>() || It->IsA<ASkyLight>() || It->IsA<ADirectionalLight>() || It->IsA<AExponentialHeightFog>()) Placed.Add(*It);
    }
    for (auto* Actor : Placed) Actor->Destroy();
}

void ADemoProceduralEnvironment::GenerateChunk(FIntVector Cell)
{
    if (Cell.Z > 0 || Chunks.Contains(Cell)) return;
    const FIntVector Origin = GetWorld()->OriginLocation;
    const FVector Position(double(Cell.X)*ChunkSize-Origin.X,double(Cell.Y)*ChunkSize-Origin.Y,double(Cell.Z)*LayerHeight-Origin.Z);
    FActorSpawnParameters Params; Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Chunk = GetWorld()->SpawnActor<ADemoEnvironmentChunk>(Position,FRotator::ZeroRotator,Params);
    if (!Chunk) return;
    Chunk->Generate(WorldSeed,Cell,ChunkSize,LayerHeight,bSpawnTargets,TargetClass,RoomPalette);
    Chunks.Add(Cell,Chunk);
    const auto& Offsets = DemoEnvironment::NeighborOffsets();
    for (int32 I=0; I<Offsets.Num(); ++I)
    {
        if (auto* Other = Chunks.Find(Cell+Offsets[I]); Other && IsValid(*Other))
        {
            Chunk->SetConnection(I,true);
            (*Other)->SetConnection(I^1,true);
        }
    }
}
void ADemoProceduralEnvironment::QueueAround(FIntVector Center)
{
    const int32 RetainRadius = LoadRadius+1;
    const auto& Offsets = DemoEnvironment::NeighborOffsets();
    for (auto It=Chunks.CreateIterator(); It; ++It)
    {
        const auto Delta = It.Key()-Center;
        if (FMath::Abs(Delta.X)>RetainRadius || FMath::Abs(Delta.Y)>RetainRadius || Delta.Z>LoadAbove+1 || Delta.Z < -LoadBelow-1)
        {
            // Seal surviving neighbors before removing any room geometry.
            for (int32 I=0; I<Offsets.Num(); ++I)
                if (auto* Other=Chunks.Find(It.Key()+Offsets[I]); Other && IsValid(*Other)) (*Other)->SetConnection(I^1,false);
            if (IsValid(It.Value())) It.Value()->Destroy();
            It.RemoveCurrent();
        }
    }
    Pending.Reset();
    for (int32 Z=LoadAbove; Z>=-LoadBelow; --Z)
        for (int32 X=-LoadRadius; X<=LoadRadius; ++X) for (int32 Y=-LoadRadius; Y<=LoadRadius; ++Y)
        {
            const auto Cell = Center+FIntVector(X,Y,Z);
            if (Cell.Z<=0 && !Chunks.Contains(Cell)) Pending.Add(Cell);
        }
    Pending.Sort([Center](FIntVector A,FIntVector B)
    {
        auto Distance = [Center](FIntVector P)
        {
            const auto D=P-Center;
            return (int64(D.X)*D.X+int64(D.Y)*D.Y)*8+int64(D.Z)*D.Z;
        };
        const auto DA=Distance(A), DB=Distance(B);
        if (DA!=DB) return DA>DB;
        if (A.Z!=B.Z) return A.Z>B.Z;
        return A.X!=B.X ? A.X>B.X : A.Y>B.Y;
    });
}
void ADemoProceduralEnvironment::RebaseIfNeeded(FVector Position)
{
    if (Position.GetAbsMax()<200000.) return;
    const auto Old=GetWorld()->OriginLocation;
    const int64 X=int64(Old.X)+FMath::RoundToInt64(Position.X);
    const int64 Y=int64(Old.Y)+FMath::RoundToInt64(Position.Y);
    const int64 Z=int64(Old.Z)+FMath::RoundToInt64(Position.Z);
    if (X>=MIN_int32 && X<=MAX_int32 && Y>=MIN_int32 && Y<=MAX_int32 && Z>=MIN_int32 && Z<=MAX_int32)
        GetWorld()->SetNewWorldOrigin(FIntVector(int32(X),int32(Y),int32(Z)));
}
void ADemoProceduralEnvironment::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* Pawn=UGameplayStatics::GetPlayerPawn(this,0);
    if (!Pawn) return;
    if (TrackedPawn!=Pawn)
    {
        if (TrackedPawn && TrackedPawn->GetMovementComponent()) TrackedPawn->GetMovementComponent()->RemoveTickPrerequisiteActor(this);
        TrackedPawn=Pawn;
        if (Pawn->GetMovementComponent()) Pawn->GetMovementComponent()->AddTickPrerequisiteActor(this);
    }
    RebaseIfNeeded(Pawn->GetActorLocation());
    float Radius,HalfHeight;
    Pawn->GetSimpleCollisionCylinder(Radius,HalfHeight);
    const FVector AbsoluteFeet=Pawn->GetActorLocation()+FVector(GetWorld()->OriginLocation)-FVector(0,0,HalfHeight);
    const auto Center=DemoEnvironment::CellAt(AbsoluteFeet,ChunkSize,LayerHeight);
    if (!bInitialized || Center!=LastCenter)
    {
        // Prepare adjacent decks as well as both sides of horizontal seams before
        // movement runs. In particular, the next stair exit must already exist.
        for (int32 Z=-1; Z<=1; ++Z) for (int32 X=-1; X<=1; ++X) for (int32 Y=-1; Y<=1; ++Y)
            GenerateChunk(Center+FIntVector(X,Y,Z));
        if (!bInitialized)
        {
            if (bReplaceAuthoredLayout) RemoveAuthoredLayout();
            const auto Origin=GetWorld()->OriginLocation;
            Pawn->SetActorLocation(FVector((Center.X+.5)*ChunkSize-Origin.X,(Center.Y+.5)*ChunkSize-Origin.Y,
                Center.Z*LayerHeight-Origin.Z+HalfHeight+5),false,nullptr,ETeleportType::TeleportPhysics);
            if (Pawn->GetMovementComponent()) Pawn->GetMovementComponent()->StopMovementImmediately();
            bInitialized=true;
        }
        LastCenter=Center;
        QueueAround(Center);
    }
    for (int32 I=0; I<ChunksPerUpdate && !Pending.IsEmpty(); ++I) GenerateChunk(Pending.Pop(EAllowShrinking::No));
}
void ADemoProceduralEnvironment::EndPlay(const EEndPlayReason::Type Reason)
{
    if (TrackedPawn && TrackedPawn->GetMovementComponent()) TrackedPawn->GetMovementComponent()->RemoveTickPrerequisiteActor(this);
    for (const auto& Entry:Chunks) if (IsValid(Entry.Value)) Entry.Value->Destroy();
    Chunks.Empty(); Pending.Empty();
    if (GetWorld() && GetWorld()->GetWorldSettings()) GetWorld()->GetWorldSettings()->bEnableWorldBoundsChecks=bPreviousBoundsChecks;
    Super::EndPlay(Reason);
}
