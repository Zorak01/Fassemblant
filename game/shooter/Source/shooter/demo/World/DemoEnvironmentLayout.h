#pragma once
#include "CoreMinimal.h"

/** Pure, order-independent layout generation shared by streaming and tests. */
namespace DemoEnvironment
{
    enum class ERoomType : uint8
    {
        Concourse, CargoBay, Reactor, ServerHall, Fabrication, Foundry, Transit, Arena,
        Hydroponics, Laboratory, MedBay, Quarters, MessHall, Security, Armory, Archives,
        PowerStation, Cooling, Maintenance, FreightDepot, ControlRoom, Ventilation, Training, Checkpoint, Count
    };
    enum class ERoomForm : uint8 { Hall, Suites, Gallery, Workshop, Diagonal, Pillared, Count };
    struct FPortal
    {
        bool bOpen = false;
        bool bWide = false;
    };
    /** A district graph is generated before geometry. Shared edges have one owner. */
    struct FCellPlan
    {
        ERoomType Room = ERoomType::Concourse;
        ERoomForm Form = ERoomForm::Hall;
        FPortal Portals[4]; // west, east, south, north; world axes, independent of kit rotation
        FIntPoint Chamber; // cells with the same identity share a larger chamber
        bool bDescent = false;
        int32 DescentDirection = INDEX_NONE;
        double CeilingHeight = 900;
    };
    enum class ETheme : uint8 { Arctic, Industrial, Botanical, Hazard, Count };
    enum class ESurface : uint8 { Structure, Secondary, Accent, Glass, Energy };
    struct FBlock
    {
        FVector Center;
        FVector Size;
        FRotator Rotation = FRotator::ZeroRotator;
        ESurface Surface = ESurface::Structure;
        bool bCollision = true;
    };
    struct FTarget
    {
        FVector Position;
        FRotator Rotation;
        FVector StrafeDirection;
        float StrafeDistance = 0;
        float StrafeSpeed = 1;
        bool operator==(const FTarget& Other) const
        {
            return Position == Other.Position && Rotation.Equals(Other.Rotation,0) && StrafeDirection == Other.StrafeDirection
                && StrafeDistance == Other.StrafeDistance && StrafeSpeed == Other.StrafeSpeed;
        }
    };
    struct FLayout
    {
        FCellPlan Plan;
        ERoomType Room = ERoomType::Concourse;
        ETheme Theme = ETheme::Arctic;
        bool bCovered = true;
        double CeilingHeight = 900;
        FLinearColor LightColor = FLinearColor::White;
        double LayerHeight = 1400;
        TArray<FBlock> Floors;
        TArray<FBlock> Blocks;
        TArray<FBlock> StairFlights;
        FTransform UpperSeal;
        FTransform LowerSeal;
        FTransform DescentSign;
        TArray<FTarget> Targets;
    };

    FIntPoint CellAt(FVector AbsolutePosition, double ChunkSize);
    uint32 CellSeed(int32 WorldSeed, FIntPoint Cell);
    FIntVector CellAt(FVector AbsoluteFeet, double ChunkSize, double LayerHeight);
    uint32 CellSeed(int32 WorldSeed, FIntVector Cell);
    FLayout Generate(int32 WorldSeed, FIntVector Cell, double ChunkSize, double LayerHeight = 1400);
    FCellPlan PlanCell(int32 WorldSeed, FIntVector Cell);
    const TArray<FIntVector>& NeighborOffsets();
    const TCHAR* RoomName(ERoomType Room);
}
