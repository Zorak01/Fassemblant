#pragma once
#include "CoreMinimal.h"

/** Pure, order-independent layout generation shared by streaming and tests. */
namespace DemoEnvironment
{
    enum class ERoomType : uint8 { Concourse, CargoBay, Reactor, ServerHall, Fabrication, Foundry, Transit, Arena, Count };
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
    const TArray<FIntVector>& NeighborOffsets();
    const TCHAR* RoomName(ERoomType Room);
}
