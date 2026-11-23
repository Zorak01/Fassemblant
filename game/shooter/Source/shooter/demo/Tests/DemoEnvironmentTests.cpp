#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "demo/World/DemoEnvironmentLayout.h"
#include "demo/World/DemoEnvironmentChunk.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Materials/Material.h"

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
        TestEqual(TEXT("Stair columns persist at every depth"),Upper.Plan.bDescent,Lower.Plan.bDescent);
        if (!Upper.Plan.bDescent)
        {
            TestTrue(TEXT("Rooms without stairs have uninterrupted floors"),Upper.Floors.Num()==1 && Upper.StairFlights.IsEmpty());
            continue;
        }
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
        TestTrue(TEXT("Only descent rooms cut openings into their floors"),Cell.Floors.Num()==(Cell.Plan.bDescent?4:1));
        TestTrue(TEXT("Geometry work per cell stays bounded"),Cell.Blocks.Num()<350);
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
    const auto Plan=DemoEnvironment::PlanCell(1729,FIntVector(0,0,-1));
    Chunk->Generate(1729,FIntVector(0,0,-1),4000,1400,false,nullptr,FDemoEnvironmentPalette());
    TArray<UStaticMeshComponent*> Meshes; Chunk->GetComponents(Meshes);
    int32 Seals=0;
    for (auto* Mesh:Meshes)
    {
        TestTrue(TEXT("Every rendered architecture mesh is a cube"),Mesh->GetStaticMesh() && Mesh->GetStaticMesh()->GetFName()==TEXT("Cube"));
        if (Mesh->GetName().StartsWith(TEXT("ConnectionSeal")))
        {
            ++Seals;
            const int32 Direction=FCString::Atoi(*Mesh->GetName().RightChop(14));
            const bool Active=Direction<4?Plan.Portals[Direction].bOpen:Plan.bDescent;
            TestEqual(TEXT("Only graph openings need streaming seals"),Mesh->IsVisible(),Active);
            TestEqual(TEXT("Unloaded graph openings are solid"),Mesh->GetCollisionEnabled()!=ECollisionEnabled::NoCollision,Active);
        }
    }
    TestEqual(TEXT("Four door seals plus upper and lower shaft seals exist"),Seals,6);
    for (int32 D=0; D<6; ++D)
    {
        const FName Name(*FString::Printf(TEXT("ConnectionSeal%d"),D));
        Chunk->SetConnection(D,true);
        for (auto* Mesh:Meshes) if (Mesh->GetFName()==Name)
            TestTrue(TEXT("Loaded neighbor opens its connection"),!Mesh->IsVisible() && Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision);
        Chunk->SetConnection(D,false);
        const bool Active=D<4?Plan.Portals[D].bOpen:Plan.bDescent;
        for (auto* Mesh:Meshes) if (Mesh->GetFName()==Name)
            TestEqual(TEXT("Unloading a neighbor reseals graph openings only"),Mesh->IsVisible(),Active);
    }
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentGraphTest,"Demo.Gameplay.Environment.BranchingGraph",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentGraphTest::RunTest(const FString& Parameters)
{
    using namespace DemoEnvironment;
    TSet<int32> Degrees, Masks, Forms, Heights;
    int32 WideEdges=0, Loops=0;
    for (int32 Seed=0; Seed<8; ++Seed) for (int32 DX=-1; DX<=1; ++DX) for (int32 DY=-1; DY<=1; ++DY)
    {
        const FIntVector Base(DX*4,DY*4,-Seed*17);
        FCellPlan Plans[16];
        int32 EdgeCount=0, Stairs=0;
        for (int32 I=0; I<16; ++I)
        {
            const auto Cell=Base+FIntVector(I%4,I/4,0);
            Plans[I]=PlanCell(Seed,Cell);
            const auto& Plan=Plans[I];
            Stairs+=Plan.bDescent;
            Forms.Add(int32(Plan.Form)); Heights.Add(int32(Plan.CeilingHeight));
            int32 Mask=0, Degree=0;
            for (int32 D=0; D<4; ++D)
            {
                const auto Other=PlanCell(Seed,Cell+NeighborOffsets()[D]);
                TestEqual(TEXT("Door presence agrees across every shared edge"),Plan.Portals[D].bOpen,Other.Portals[D^1].bOpen);
                TestEqual(TEXT("Door widths agree across every shared edge"),Plan.Portals[D].bWide,Other.Portals[D^1].bWide);
                if (!Plan.Portals[D].bOpen) continue;
                Mask|=1<<D; ++Degree;
                if (Plan.Portals[D].bWide)
                {
                    ++WideEdges;
                    TestTrue(TEXT("Wide portals join one coherent chamber"),Plan.Chamber==Other.Chamber && Plan.Room==Other.Room
                        && Plan.Form==Other.Form && Plan.CeilingHeight==Other.CeilingHeight);
                }
                if ((D==1 && I%4<3)||(D==3 && I/4<3)) ++EdgeCount;
            }
            Masks.Add(Mask); Degrees.Add(Degree);
        }
        TestEqual(TEXT("Each district has three persistent descent columns"),Stairs,3);
        Loops+=EdgeCount>15;
        TSet<int32> Reachable; TArray<int32> Queue={0}; Reachable.Add(0);
        const int32 Steps[]={-1,1,-4,4};
        for (int32 Head=0; Head<Queue.Num(); ++Head)
        {
            const int32 I=Queue[Head];
            for (int32 D=0; D<4; ++D)
            {
                const int32 J=I+Steps[D];
                if (J<0 || J>=16 || (D<2 && I/4!=J/4) || !Plans[I].Portals[D].bOpen || Reachable.Contains(J)) continue;
                Reachable.Add(J); Queue.Add(J);
            }
        }
        TestEqual(TEXT("Every room belongs to the connected district graph"),Reachable.Num(),16);
        for (int32 I=0; I<16; ++I)
        {
            int32 J=I, Hops=0;
            while (!Plans[J].bDescent && Hops<16)
            {
                const int32 D=Plans[J].DescentDirection;
                if (D<0 || D>3 || !Plans[J].Portals[D].bOpen) break;
                const int32 Next=J+Steps[D];
                if (Next<0 || Next>=16 || (D<2 && Next/4!=J/4)) break;
                J=Next; ++Hops;
            }
            TestTrue(TEXT("DOWN signs lead through open doors to real stairs"),Plans[J].bDescent && Hops<16);
        }
    }
    TestTrue(TEXT("Layouts contain side rooms and branch junctions"),Degrees.Contains(1) && Degrees.Contains(3));
    TestTrue(TEXT("Layouts use many exit patterns instead of repeating a cross"),Masks.Num()>=12);
    TestTrue(TEXT("Districts contain multi-cell chambers and circulation loops"),WideEdges>0 && Loops>0);
    TestEqual(TEXT("All spatial grammars appear"),Forms.Num(),int32(ERoomForm::Count));
    TestEqual(TEXT("Ceiling proportions vary"),Heights.Num(),4);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentRoutesTest,"Demo.Gameplay.Environment.TraversableBranches",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentRoutesTest::RunTest(const FString& Parameters)
{
    using namespace DemoEnvironment;
    for (int32 X=-4; X<4; ++X) for (int32 Y=-4; Y<4; ++Y)
    {
        const auto Layout=Generate(1729,FIntVector(X,Y,-2),4000);
        for (int32 D=0; D<4; ++D)
        {
            if (!Layout.Plan.Portals[D].bOpen) continue;
            const FVector End=FVector(2000,2000,92)+FVector(NeighborOffsets()[D])*2000.;
            for (int32 Step=0; Step<=40; ++Step)
            {
                const FVector P=FMath::Lerp(FVector(2000,2000,92),End,Step/40.);
                bool Blocked=false;
                for (const auto& B:Layout.Blocks)
                {
                    if (!B.bCollision) continue;
                    const FVector Local=B.Rotation.UnrotateVector(P-B.Center).GetAbs();
                    // Conservative capsule bounds: architecture must leave a
                    // full standing player clear through every graph doorway.
                    Blocked|=Local.X<B.Size.X*.5+42 && Local.Y<B.Size.Y*.5+42 && Local.Z<B.Size.Z*.5+88;
                }
                for (const auto& Target:Layout.Targets)
                {
                    const FVector Across=FVector::CrossProduct(Target.StrafeDirection,FVector::UpVector).GetSafeNormal();
                    Blocked|=FMath::Abs(FVector::DotProduct(P-Target.Position,Across))<100
                        && FMath::Abs(FVector::DotProduct(P-Target.Position,Target.StrafeDirection))<Target.StrafeDistance+80;
                }
                TestFalse(TEXT("Furnishings and partitions preserve the door-to-hub route"),Blocked);
            }
        }
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoEnvironmentMaterialsTest,"Demo.Gameplay.Environment.InstancedMaterials",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoEnvironmentMaterialsTest::RunTest(const FString& Parameters)
{
    for (const TCHAR* Name:{TEXT("M_TrainingFloor"),TEXT("M_TrainingFrame"),TEXT("M_TrainingArmor"),TEXT("M_TrainingTeal"),
        TEXT("M_TrainingOrange"),TEXT("M_TrainingYellow"),TEXT("M_TrainingWindows"),TEXT("M_TrainingLight"),TEXT("M_MuzzleEmitter")})
    {
        const FString Path=FString::Printf(TEXT("/Game/demo/Materials/%s.%s"),Name,Name);
        auto* Material=LoadObject<UMaterial>(nullptr,*Path);
        TestTrue(FString::Printf(TEXT("%s has persisted instanced-mesh shader support"),Name),
            Material && Material->GetUsageByFlag(MATUSAGE_InstancedStaticMeshes));
    }
    return true;
}
#endif
