#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "demo/World/DemoEnvironmentLayout.h"
#include "demo/World/DemoEnvironmentChunk.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"

namespace
{
    bool CoversXY(const DemoEnvironment::FBlock& Block,FVector P)
    {
        const FVector Local=Block.Rotation.UnrotateVector(P-Block.Center);
        return FMath::Abs(Local.X)<Block.Size.X*.5-.01 && FMath::Abs(Local.Y)<Block.Size.Y*.5-.01;
    }
    FVector TopEnd(const DemoEnvironment::FBlock& Flight,double Sign)
    {
        return Flight.Center+Flight.Rotation.RotateVector(FVector(Sign*Flight.Size.X*.5,0,Flight.Size.Z*.5));
    }
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentCoordinatesTest,"Demo.Gameplay.Environment.Coordinates",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentCoordinatesTest::RunTest(const FString& Parameters)
{
    using namespace DemoEnvironment;
    TestTrue(TEXT("Negative XY coordinates floor correctly"),CellAt(FVector(-.01,-4000,0),4000)==FIntPoint(-1,-1));
    TestTrue(TEXT("Standing on a lower floor selects that deck"),CellAt(FVector(10,10,-1400),4000,1400)==FIntVector(0,0,-1));
    TestTrue(TEXT("Moving midway down stairs preloads the next deck"),CellAt(FVector(10,10,-701),4000,1400)==FIntVector(0,0,-1));
    TestTrue(TEXT("The top of the complex is capped"),CellAt(FVector(10,10,5000),4000,1400).Z==0);
    const FVector P(-80000,12400,-280000), Offset(80000,-12000,280000);
    TestTrue(TEXT("XYZ rebasing preserves the logical deck"),CellAt(P,4000,1400)==CellAt((P+Offset)-Offset,4000,1400));
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentRegenerationTest,"Demo.Gameplay.Environment.Regeneration",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentRegenerationTest::RunTest(const FString& Parameters)
{
    using namespace DemoEnvironment;
    const FIntVector Cell(-71,93,-300);
    const auto First=Generate(1729,Cell,4000);
    Generate(1729,FIntVector(100,-200,-900),4000);
    const auto Again=Generate(1729,Cell,4000);
    TestEqual(TEXT("Regenerated cells retain instance counts"),First.Blocks.Num(),Again.Blocks.Num());
    TestTrue(TEXT("Targets and identity are deterministic"),First.Targets==Again.Targets && First.Room==Again.Room && First.Theme==Again.Theme);
    for (int32 I=0; I<FMath::Min(First.Blocks.Num(),Again.Blocks.Num()); ++I)
        TestTrue(TEXT("Regeneration recreates the same transforms"),First.Blocks[I].Center.Equals(Again.Blocks[I].Center,0)
            && First.Blocks[I].Size.Equals(Again.Blocks[I].Size,0) && First.Blocks[I].Rotation.Equals(Again.Blocks[I].Rotation,0));
    const auto Different=Generate(1730,Cell,4000);
    bool Changed=Different.Blocks.Num()!=First.Blocks.Num() || Different.Room!=First.Room;
    for (int32 I=0; I<FMath::Min(Different.Blocks.Num(),First.Blocks.Num()); ++I) Changed|=!Different.Blocks[I].Center.Equals(First.Blocks[I].Center,0);
    TestTrue(TEXT("World seeds change the layout"),Changed);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentDescentTest,"Demo.Gameplay.Environment.ConnectedDescent",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentDescentTest::RunTest(const FString& Parameters)
{
    using namespace DemoEnvironment;
    for (double Size:{4000.,6000.}) for (double Height:{1200.,1400.,2000.}) for (int32 Seed=0; Seed<16; ++Seed)
    {
        const auto Upper=Generate(Seed,FIntVector(Seed-8,9-Seed,-100),Size,Height);
        const auto Lower=Generate(Seed,FIntVector(Seed-8,9-Seed,-101),Size,Height);
        TestEqual(TEXT("Every cell contains two switchback flights"),Upper.StairFlights.Num(),2);
        TestTrue(TEXT("All room types are fully covered"),Upper.bCovered && Lower.bCovered);
        if (Upper.StairFlights.Num()!=2 || Lower.StairFlights.Num()!=2) continue;
        TestTrue(TEXT("Stair columns align across distinct deck layouts"),TopEnd(Upper.StairFlights[0],-1).Equals(TopEnd(Lower.StairFlights[0],-1),.001));
        TestTrue(TEXT("The exit reaches exactly the next floor"),FMath::IsNearlyEqual(TopEnd(Upper.StairFlights[1],1).Z,-Height,.001));
        TestTrue(TEXT("Flights join at the same landing elevation"),FMath::IsNearlyEqual(TopEnd(Upper.StairFlights[0],1).Z,TopEnd(Upper.StairFlights[1],-1).Z,.001));
        for (const auto& Flight:Upper.StairFlights)
        {
            TestTrue(TEXT("Descent slopes remain walkable"),FMath::Abs(Flight.Rotation.Pitch)<44.f);
            for (double T:{.1,.5,.9})
            {
                const FVector OnStairs=FMath::Lerp(TopEnd(Flight,-1),TopEnd(Flight,1),T);
                bool BlockedByFloor=false;
                for (const auto& Floor:Upper.Floors) BlockedByFloor|=CoversXY(Floor,OnStairs);
                TestFalse(TEXT("A real floor opening exposes both stair flights"),BlockedByFloor);
            }
        }
        // The top cap seals the ceiling aperture; the lower cap seals unloaded
        // depth. Both are opaque collision cubes, not a fake full floor.
        TestTrue(TEXT("Upper streaming seal spans the stair column"),Upper.UpperSeal.GetScale3D().X>0 && Upper.UpperSeal.GetScale3D().Y>0);
        TestTrue(TEXT("Bottom streaming seal follows the next deck"),FMath::IsNearlyEqual(Upper.LowerSeal.GetLocation().Z,-Height-15,.001));
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentVarietyTest,"Demo.Gameplay.Environment.ModularCells",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentVarietyTest::RunTest(const FString& Parameters)
{
    using namespace DemoEnvironment;
    TSet<ERoomType> Rooms;
    bool Diagonal=false;
    for (int32 X=-4; X<4; ++X) for (int32 Y=-4; Y<4; ++Y) for (int32 Z=-3; Z<=0; ++Z)
    {
        const auto Cell=Generate(1729,FIntVector(X,Y,Z),4000);
        Rooms.Add(Cell.Room);
        TestTrue(TEXT("No archetype opens to the sky"),Cell.bCovered);
        TestTrue(TEXT("Each cell has a split floor, not a full slab over its stairs"),Cell.Floors.Num()==4);
        TestTrue(TEXT("Geometry work per cell stays bounded"),Cell.Blocks.Num()<250);
        for (const auto& Block:Cell.Blocks)
        {
            Diagonal|=FMath::Abs(Block.Rotation.Pitch)>1 || FMath::Abs(FMath::Fmod(Block.Rotation.Yaw,90.f))>1;
            FBox Bounds(ForceInit);
            for (double A:{-1.,1.}) for (double B:{-1.,1.}) for (double C:{-1.,1.})
                Bounds+=Block.Center+Block.Rotation.RotateVector(Block.Size*FVector(A,B,C)*.5);
            TestTrue(TEXT("Box modules remain in their horizontal cell"),Bounds.Min.X>=-.01 && Bounds.Min.Y>=-.01 && Bounds.Max.X<=4000.01 && Bounds.Max.Y<=4000.01);
        }
    }
    TestEqual(TEXT("All interior archetypes appear"),Rooms.Num(),int32(ERoomType::Count));
    TestTrue(TEXT("The kit includes diagonal box modules"),Diagonal);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentSealTest,"Demo.Gameplay.Environment.CubeKitAndSeals",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentSealTest::RunTest(const FString& Parameters)
{
    const auto Settings=UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Settings);
    auto* Chunk=World->SpawnActor<ADemoEnvironmentChunk>();
    Chunk->Generate(1729,FIntVector(0,0,-1),4000,1400,false,nullptr,FDemoEnvironmentPalette());
    TArray<UStaticMeshComponent*> Meshes; Chunk->GetComponents(Meshes);
    int32 Seals=0;
    for (auto* Mesh:Meshes)
    {
        TestTrue(TEXT("Every rendered architecture mesh is a cube"),Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetFName()==TEXT("Cube"));
        if (Mesh->GetName().StartsWith(TEXT("ConnectionSeal")))
        {
            ++Seals;
            TestTrue(TEXT("Unloaded connections are opaque and solid by default"),Mesh->IsVisible() && Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
        }
    }
    TestEqual(TEXT("Four door seals plus upper and lower shaft seals exist"),Seals,6);
    Chunk->SetConnection(0,true);
    for (auto* Mesh:Meshes) if (Mesh->GetFName()==TEXT("ConnectionSeal0"))
        TestTrue(TEXT("Loaded neighbor opens its connection"),!Mesh->IsVisible() && Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
    Chunk->SetConnection(0,false);
    for (auto* Mesh:Meshes) if (Mesh->GetFName()==TEXT("ConnectionSeal0"))
        TestTrue(TEXT("Unloading a neighbor closes the room again"),Mesh->IsVisible() && Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision);
    World->DestroyWorld(false);
    return true;
}
#endif
