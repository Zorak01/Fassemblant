#pragma once
#include "CoreMinimal.h"

/** Pure, order-independent layout generation shared by streaming and tests. */
namespace DemoEnvironment
{
    enum class ERoomType : uint8 { Atrium, CargoBay, Reactor, ServerHall, Hydroponics, Foundry, Transit, Arena, Count };
    enum class ETheme : uint8 { Arctic, Industrial, Botanical, Hazard, Count };
    enum class ESurface : uint8 { Structure, Secondary, Accent, Glass, Energy, Foliage };
    enum class EShape : uint8 { Box, Cylinder, Sphere };
    struct FBlock
    {
        FVector Center;
        FVector Size;
        FRotator Rotation = FRotator::ZeroRotator;
        ESurface Surface = ESurface::Structure;
        EShape Shape = EShape::Box;
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
        ERoomType Room = ERoomType::Atrium;
        ETheme Theme = ETheme::Arctic;
        bool bCovered = false;
        double CeilingHeight = 800;
        FLinearColor LightColor = FLinearColor::White;
        TArray<FBlock> Blocks;
        TArray<FTarget> Targets;
    };

    FIntPoint CellAt(FVector AbsolutePosition, double ChunkSize);
    uint32 CellSeed(int32 WorldSeed, FIntPoint Cell);
    FLayout Generate(int32 WorldSeed, FIntPoint Cell, double ChunkSize);
    const TCHAR* RoomName(ERoomType Room);
}
