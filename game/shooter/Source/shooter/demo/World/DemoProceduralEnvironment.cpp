#include "demo/World/DemoProceduralEnvironment.h"
#include "demo/World/DemoEnvironmentChunk.h"
#include "demo/World/DemoEnvironmentLayout.h"
#include "demo/World/DemoTarget.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextRenderActor.h"
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
    HorizonFloor = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HorizonFloor"));
    HorizonFloor->SetupAttachment(RootComponent);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
    HorizonFloor->SetStaticMesh(Cube.Object);
    HorizonFloor->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    HorizonFloor->SetMobility(EComponentMobility::Movable);
    HorizonFloor->SetCastShadow(false);
    HorizonFloor->SetCanEverAffectNavigation(false);
    // Cosmetic only: following the player must never become a moving physics
    // base. Actual collision comes from the immediately prepared nearby chunks.
    HorizonFloor->SetRelativeScale3D(FVector(10000,10000,.3));
    HorizonFloor->SetRelativeLocation(FVector(0,0,-18));
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
    ChunksPerUpdate = FMath::Clamp(ChunksPerUpdate, 1, 8);
    // Preserve the original top-level material overrides as palette defaults.
    if (!RoomPalette.Floor) RoomPalette.Floor = GroundMaterial;
    if (!RoomPalette.Structure) RoomPalette.Structure = StructureMaterial;
    if (!RoomPalette.Accent) RoomPalette.Accent = AccentMaterial;
    if (RoomPalette.Floor) HorizonFloor->SetMaterial(0, RoomPalette.Floor);
    bPreviousBoundsChecks = GetWorldSettings()->bEnableWorldBoundsChecks;
    GetWorldSettings()->bEnableWorldBoundsChecks = false;
    Tick(0.f);
}
void ADemoProceduralEnvironment::RemoveAuthoredLayout()
{
    // Only replace placed layout actors in this level. Keep lighting, sky, post
    // process, player starts, session director and gameplay actors intact.
    TArray<AActor*> Placed;
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (It->GetLevel() != GetLevel() || It->GetOwner()) continue;
        if (It->IsA<AStaticMeshActor>() || It->IsA<ATextRenderActor>() || It->IsA<ADemoTarget>()) Placed.Add(*It);
    }
    for (auto* Actor : Placed) Actor->Destroy();
}
void ADemoProceduralEnvironment::GenerateChunk(FIntPoint Cell)
{
    if (Chunks.Contains(Cell)) return;
    const FIntVector Origin = GetWorld()->OriginLocation;
    const FVector Location(double(Cell.X)*ChunkSize-Origin.X, double(Cell.Y)*ChunkSize-Origin.Y, 0);
    FActorSpawnParameters Params;
    Params.Owner = this;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Chunk = GetWorld()->SpawnActor<ADemoEnvironmentChunk>(Location, FRotator::ZeroRotator, Params);
    if (!Chunk) return;
    Chunk->Generate(WorldSeed, Cell, ChunkSize, bSpawnTargets, TargetClass, RoomPalette);
    Chunks.Add(Cell, Chunk);
}
void ADemoProceduralEnvironment::QueueAround(FIntPoint Center)
{
    const int32 RetainRadius = LoadRadius + 1;
    for (auto It = Chunks.CreateIterator(); It; ++It)
    {
        if (FMath::Abs(int64(It.Key().X)-Center.X) > RetainRadius || FMath::Abs(int64(It.Key().Y)-Center.Y) > RetainRadius)
        {
            if (IsValid(It.Value())) It.Value()->Destroy();
            It.RemoveCurrent();
        }
    }
    Pending.Reset();
    for (int32 X=-LoadRadius; X<=LoadRadius; ++X) for (int32 Y=-LoadRadius; Y<=LoadRadius; ++Y)
    {
        const FIntPoint Cell = Center + FIntPoint(X,Y);
        if (!Chunks.Contains(Cell)) Pending.Add(Cell);
    }
    Pending.Sort([Center](FIntPoint A, FIntPoint B)
    {
        const auto Distance = [Center](FIntPoint P) { return FMath::Square(int64(P.X)-Center.X) + FMath::Square(int64(P.Y)-Center.Y); };
        const auto DA = Distance(A), DB = Distance(B);
        return DA != DB ? DA > DB : (A.X != B.X ? A.X > B.X : A.Y > B.Y);
    }); // Pop nearest first; ties have deterministic ordering.
}
void ADemoProceduralEnvironment::RebaseIfNeeded(FVector Position)
{
    if (FMath::Max(FMath::Abs(Position.X),FMath::Abs(Position.Y)) < 200000.) return;
    const auto Origin = GetWorld()->OriginLocation;
    const int64 X = int64(Origin.X) + FMath::RoundToInt64(Position.X / ChunkSize) * FMath::RoundToInt64(ChunkSize);
    const int64 Y = int64(Origin.Y) + FMath::RoundToInt64(Position.Y / ChunkSize) * FMath::RoundToInt64(ChunkSize);
    if (X >= MIN_int32 && X <= MAX_int32 && Y >= MIN_int32 && Y <= MAX_int32)
        GetWorld()->SetNewWorldOrigin(FIntVector(int32(X),int32(Y),Origin.Z));
}
void ADemoProceduralEnvironment::Tick(float Dt)
{
    Super::Tick(Dt);
    auto* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
    if (!Pawn) return;
    if (TrackedPawn != Pawn)
    {
        if (TrackedPawn && TrackedPawn->GetMovementComponent()) TrackedPawn->GetMovementComponent()->RemoveTickPrerequisiteActor(this);
        TrackedPawn = Pawn;
        if (Pawn->GetMovementComponent()) Pawn->GetMovementComponent()->AddTickPrerequisiteActor(this);
    }
    RebaseIfNeeded(Pawn->GetActorLocation());
    const FVector Position = Pawn->GetActorLocation();
    const FVector Absolute = Position + FVector(GetWorld()->OriginLocation);
    const FIntPoint Center = DemoEnvironment::CellAt(Absolute, ChunkSize);
    if (!bInitialized || Center != LastCenter) HorizonFloor->SetWorldLocation(FVector(Position.X,Position.Y,-18));
    if (!bInitialized)
    {
        // Build collision around the spawn before removing the authored floor.
        for (int32 X=-1; X<=1; ++X) for (int32 Y=-1; Y<=1; ++Y) GenerateChunk(Center + FIntPoint(X,Y));
        if (bReplaceAuthoredLayout) RemoveAuthoredLayout();
        bInitialized = true;
        LastCenter = Center;
        QueueAround(Center);
    }
    else if (Center != LastCenter)
    {
        LastCenter = Center;
        QueueAround(Center);
        // Prepare collision on both sides of every nearby seam, including at a
        // teleport destination. Distant scenery remains budgeted.
        for (int32 X=-1; X<=1; ++X) for (int32 Y=-1; Y<=1; ++Y) GenerateChunk(Center + FIntPoint(X,Y));
    }
    for (int32 I=0; I<ChunksPerUpdate && !Pending.IsEmpty(); ++I) GenerateChunk(Pending.Pop(EAllowShrinking::No));
}
void ADemoProceduralEnvironment::EndPlay(const EEndPlayReason::Type Reason)
{
    if (TrackedPawn && TrackedPawn->GetMovementComponent()) TrackedPawn->GetMovementComponent()->RemoveTickPrerequisiteActor(this);
    for (const auto& Entry : Chunks) if (IsValid(Entry.Value)) Entry.Value->Destroy();
    Chunks.Empty(); Pending.Empty();
    if (GetWorld() && GetWorld()->GetWorldSettings()) GetWorld()->GetWorldSettings()->bEnableWorldBoundsChecks = bPreviousBoundsChecks;
    Super::EndPlay(Reason);
}
