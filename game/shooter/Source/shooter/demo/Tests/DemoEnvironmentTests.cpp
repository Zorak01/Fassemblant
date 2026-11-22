#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "demo/World/DemoEnvironmentLayout.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentCoordinatesTest, "Demo.Gameplay.Environment.Coordinates", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentCoordinatesTest::RunTest(const FString& Parameters)
{
    using namespace DemoEnvironment;
    TestTrue(TEXT("Origin uses the first cell"), CellAt(FVector::ZeroVector,6000) == FIntPoint(0,0));
    TestTrue(TEXT("Negative coordinates floor instead of truncating"), CellAt(FVector(-.01,-6000,0),6000) == FIntPoint(-1,-1));
    TestTrue(TEXT("Both horizontal boundaries advance independently"), CellAt(FVector(-6000.01,6000,500),6000) == FIntPoint(-2,1));
    const FVector Absolute(270123,-417234,100);
    const FVector Rebase(200000,-400000,0);
    TestTrue(TEXT("World-origin shifts preserve logical cell identity"), CellAt(Absolute,6000) == CellAt((Absolute-Rebase)+Rebase,6000));
    for (FIntPoint Cell : {FIntPoint(-23,47),FIntPoint(23,-47),FIntPoint(-23,-47),FIntPoint(23,47)})
    {
        const FVector Corner(double(Cell.X)*6000,double(Cell.Y)*6000,0);
        TestTrue(TEXT("All four quadrants address their own cells"), CellAt(Corner+FVector(3000,3000,0),6000) == Cell);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentRegenerationTest, "Demo.Gameplay.Environment.Regeneration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentRegenerationTest::RunTest(const FString& Parameters)
{
    using namespace DemoEnvironment;
    const FIntPoint Cell(-71,93);
    const auto First = Generate(1729,Cell,6000);
    // Visiting unrelated cells must not consume a shared RNG or alter a revisit.
    Generate(1729,FIntPoint(100,-200),6000);
    const auto Again = Generate(1729,Cell,6000);
    TestEqual(TEXT("Revisited cell retains its instance count"), First.Blocks.Num(),Again.Blocks.Num());
    TestTrue(TEXT("Revisited cell retains its targets"), First.Targets == Again.Targets);
    TestTrue(TEXT("Room identity and theme survive unloading"), First.Room == Again.Room && First.Theme == Again.Theme && First.bCovered == Again.bCovered);
    for (int32 I=0; I<FMath::Min(First.Blocks.Num(),Again.Blocks.Num()); ++I)
    {
        TestTrue(TEXT("Recreated geometry has the same transforms"), First.Blocks[I].Center.Equals(Again.Blocks[I].Center,0)
            && First.Blocks[I].Size.Equals(Again.Blocks[I].Size,0) && First.Blocks[I].Rotation.Equals(Again.Blocks[I].Rotation,0));
    }
    const auto OtherSeed = Generate(1730,Cell,6000);
    bool bDifferent = First.Blocks.Num() != OtherSeed.Blocks.Num();
    for (int32 I=0; I<FMath::Min(First.Blocks.Num(),OtherSeed.Blocks.Num()); ++I)
        bDifferent |= !First.Blocks[I].Center.Equals(OtherSeed.Blocks[I].Center,0);
    TestTrue(TEXT("Changing the world seed produces a different layout"), bDifferent);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentBoundsTest, "Demo.Gameplay.Environment.TraversableCells", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentBoundsTest::RunTest(const FString& Parameters)
{
    using namespace DemoEnvironment;
    for (double Size : {4000.,6000.,12000.}) for (int32 Seed=0; Seed<32; ++Seed)
    {
        const auto Layout = Generate(Seed,FIntPoint(Seed-16,8-Seed),Size);
        const double StreetHalfWidth = 190.*Size/6000.;
        for (const auto& Block : Layout.Blocks)
        {
            FBox Bounds(ForceInit);
            for (float X : {-1.f,1.f}) for (float Y : {-1.f,1.f}) for (float Z : {-1.f,1.f})
                Bounds += Block.Center + Block.Rotation.RotateVector(Block.Size * FVector(X,Y,Z) * .5);
            TestTrue(TEXT("Rotated obstacles stay inside their own chunk"), Bounds.Min.X >= 0 && Bounds.Min.Y >= 0 && Bounds.Max.X <= Size && Bounds.Max.Y <= Size);
            if (Block.bCollision && Bounds.Min.Z < 230 && Bounds.Max.Z > 5)
            {
                const bool bCrossesVerticalStreet = Bounds.Max.X > Size*.5-StreetHalfWidth && Bounds.Min.X < Size*.5+StreetHalfWidth;
                const bool bCrossesHorizontalStreet = Bounds.Max.Y > Size*.5-StreetHalfWidth && Bounds.Min.Y < Size*.5+StreetHalfWidth;
                TestFalse(TEXT("A clear route connects the two X neighbors"), bCrossesHorizontalStreet);
                TestFalse(TEXT("A clear route connects the two Y neighbors"), bCrossesVerticalStreet);
            }
        }
        TestEqual(TEXT("Rooms provide two encounter targets"), Layout.Targets.Num(),2);
        for (const auto& Target : Layout.Targets)
        {
            TestTrue(TEXT("Targets occupy a connected central street"), FMath::IsNearlyEqual(Target.Position.X,Size*.5) || FMath::IsNearlyEqual(Target.Position.Y,Size*.5));
            for (double Phase : {-1.,0.,1.})
            {
                const FVector P = Target.Position+Target.StrafeDirection*Target.StrafeDistance*Phase;
                TestTrue(TEXT("Patrol stays inside its owning room"), P.X > 0 && P.Y > 0 && P.X < Size && P.Y < Size);
            }
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentVarietyTest, "Demo.Gameplay.Environment.RoomVariety", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentVarietyTest::RunTest(const FString& Parameters)
{
    using namespace DemoEnvironment;
    TSet<ERoomType> Rooms;
    TSet<ETheme> Themes;
    bool bCurved = false, bPlants = false, bElevated = false, bCovered = false, bOpen = false;
    for (int32 X=-8; X<8; ++X) for (int32 Y=-8; Y<8; ++Y)
    {
        const auto Layout = Generate(1729,FIntPoint(X,Y),6000);
        Rooms.Add(Layout.Room); Themes.Add(Layout.Theme);
        bCovered |= Layout.bCovered; bOpen |= !Layout.bCovered;
        for (const auto& Block : Layout.Blocks)
        {
            bCurved |= Block.Shape != EShape::Box;
            bPlants |= Block.Surface == ESurface::Foliage;
            bElevated |= Block.bCollision && Block.Center.Z > 350 && Block.Size.Z <= 40;
        }
        TestTrue(TEXT("Geometry work per room stays bounded"), Layout.Blocks.Num() < 300);
        const FIntPoint District = CellAt(FVector(X,Y,0),4);
        const auto Neighbor = Generate(1729,District*4,6000);
        TestTrue(TEXT("Nearby rooms share their district theme"), Layout.Theme == Neighbor.Theme);
    }
    TestEqual(TEXT("All room archetypes appear"),Rooms.Num(),int32(ERoomType::Count));
    TestEqual(TEXT("All environmental themes appear"),Themes.Num(),int32(ETheme::Count));
    TestTrue(TEXT("Rooms vary in silhouettes, vegetation, height and enclosure"),bCurved && bPlants && bElevated && bCovered && bOpen);
    return true;
}
#endif
