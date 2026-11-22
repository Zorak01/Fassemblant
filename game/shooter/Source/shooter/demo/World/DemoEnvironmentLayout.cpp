#include "demo/World/DemoEnvironmentLayout.h"

FIntPoint DemoEnvironment::CellAt(FVector Position, double Size)
{
    return FIntPoint(FMath::FloorToInt(Position.X / Size), FMath::FloorToInt(Position.Y / Size));
}
uint32 DemoEnvironment::CellSeed(int32 Seed, FIntPoint Cell)
{
    // Explicit integer mixing is stable across loads, unlike generation-order RNG.
    uint32 Hash = static_cast<uint32>(Seed) ^ 0x9e3779b9u;
    for (uint32 Coordinate : {static_cast<uint32>(Cell.X), static_cast<uint32>(Cell.Y)})
    {
        Hash ^= Coordinate + 0x9e3779b9u + (Hash << 6) + (Hash >> 2);
        Hash ^= Hash >> 16; Hash *= 0x85ebca6bu; Hash ^= Hash >> 13;
    }
    return Hash;
}


FIntVector DemoEnvironment::CellAt(FVector Feet,double Size,double Height)
{
    const auto XY = CellAt(Feet,Size);
    return FIntVector(XY.X,XY.Y,FMath::Min(0,FMath::FloorToInt((Feet.Z+Height*.5)/Height)));
}
uint32 DemoEnvironment::CellSeed(int32 Seed,FIntVector Cell)
{
    uint32 Hash = CellSeed(Seed,FIntPoint(Cell.X,Cell.Y));
    Hash ^= uint32(Cell.Z)*0x85ebca6bu; Hash ^= Hash>>16; Hash *= 0xc2b2ae35u;
    return Hash^(Hash>>16);
}
const TArray<FIntVector>& DemoEnvironment::NeighborOffsets()
{
    static const TArray<FIntVector> Offsets = {FIntVector(-1,0,0),FIntVector(1,0,0),FIntVector(0,-1,0),FIntVector(0,1,0),FIntVector(0,0,1),FIntVector(0,0,-1)};
    return Offsets;
}
const TCHAR* DemoEnvironment::RoomName(ERoomType Room)
{
    switch (Room)
    {
    case ERoomType::Concourse: return TEXT("CONCOURSE");
    case ERoomType::CargoBay: return TEXT("STORAGE");
    case ERoomType::Reactor: return TEXT("REACTOR");
    case ERoomType::ServerHall: return TEXT("SERVER HALL");
    case ERoomType::Fabrication: return TEXT("FABRICATION");
    case ERoomType::Foundry: return TEXT("FOUNDRY");
    case ERoomType::Transit: return TEXT("TRANSFER");
    case ERoomType::Arena: return TEXT("COMBAT CELL");
    default: return TEXT("FACILITY");
    }
}
DemoEnvironment::FLayout DemoEnvironment::Generate(int32 Seed,FIntVector Cell,double Size,double LayerHeight)
{
    FLayout Layout;
    Layout.LayerHeight = LayerHeight;
    Layout.CeilingHeight = 900;
    Layout.bCovered = true;
    FRandomStream Random(int32(CellSeed(Seed,Cell)));
    Layout.Room = ERoomType(Random.RandRange(0,int32(ERoomType::Count)-1));
    const auto District = CellAt(FVector(Cell.X,Cell.Y,Cell.Z),4.);
    Layout.Theme = ETheme(CellSeed(Seed^0x61c88647,FIntVector(District.X,District.Y,FMath::FloorToInt(double(Cell.Z)/3)))%uint32(ETheme::Count));
    const FLinearColor Colors[] = {FLinearColor(.68f,.82f,1),FLinearColor(1,.86f,.68f),FLinearColor(.78f,1,.84f),FLinearColor(1,.7f,.46f)};
    Layout.LightColor = Colors[uint8(Layout.Theme)];
    const double Scale = Size/4000.;
    // The whole vertical column shares its orientation. Stairs/holes therefore
    // align across decks even when the room archetype changes below them.
    const FQuat Turn = FRotator(0,(CellSeed(Seed,FIntPoint(Cell.X,Cell.Y))%4)*90.,0).Quaternion();
    auto Point = [Scale,Turn](FVector P)
    {
        P = FVector(2000,2000,0)+Turn.RotateVector(P-FVector(2000,2000,0));
        P.X *= Scale; P.Y *= Scale; return P;
    };
    auto MakeBox = [&](FVector P,FVector D,ESurface Surface=ESurface::Structure,FRotator Rotation=FRotator::ZeroRotator,bool Collision=true)
    {
        D.X *= Scale; D.Y *= Scale;
        return FBlock{Point(P),D,(Turn*Rotation.Quaternion()).Rotator(),Surface,Collision};
    };
    auto Box = [&](FVector P,FVector D,ESurface S=ESurface::Structure,bool Collision=true)
    { Layout.Blocks.Add(MakeBox(P,D,S,FRotator::ZeroRotator,Collision)); };
    auto Diagonal = [&](FVector P,FVector D,ESurface S=ESurface::Secondary,double Yaw=45.)
    { Layout.Blocks.Add(MakeBox(P,D,S,FRotator(0,Yaw,0))); };
    auto Slab = [&](double Z,double Thickness,bool Roof)
    {
        const double Right = Roof?1900.:1600.;
        auto Panel = [&](double X0,double X1,double Y0,double Y1)
        {
            auto B = MakeBox(FVector((X0+X1)*.5,(Y0+Y1)*.5,Z),FVector(X1-X0,Y1-Y0,Thickness));
            if (Roof) Layout.Blocks.Add(B); else Layout.Floors.Add(B);
        };
        Panel(0,400,0,4000); Panel(Right,4000,0,4000);
        Panel(400,Right,0,400); Panel(400,Right,1600,4000);
    };
    Slab(-15,30,false);
    Slab(Layout.CeilingHeight+25,50,true);
    Layout.UpperSeal = FTransform(Turn,Point(FVector(1150,1000,Layout.CeilingHeight+25)),FVector(1520*Scale/100,1220*Scale/100,.5));
    Layout.LowerSeal = FTransform(Turn,Point(FVector(1000,1000,-LayerHeight-15)),FVector(1220*Scale/100,1220*Scale/100,.3));
    Layout.DescentSign = FTransform(Turn,Point(FVector(260,1000,260)));
    // Continuous outer shell: all four doors are sealed by the chunk until their
    // neighbor is loaded. There are no exterior alleys or open-sky archetypes.
    for (bool Axis : {false,true}) for (double Edge : {50.,3950.})
    {
        const double FrameEdge=Edge<2000?70.:3930.;
        for (double Mid : {800.,3200.})
            Box(Axis?FVector(Edge,Mid,LayerHeight*.5):FVector(Mid,Edge,LayerHeight*.5),Axis?FVector(100,1600,LayerHeight):FVector(1600,100,LayerHeight));
        Box(Axis?FVector(FrameEdge,2000,(LayerHeight+360)*.5):FVector(2000,FrameEdge,(LayerHeight+360)*.5),Axis?FVector(120,800,LayerHeight-360):FVector(800,120,LayerHeight-360),ESurface::Secondary);
        for (double Jamb : {1570.,2430.})
            Box(Axis?FVector(FrameEdge,Jamb,180):FVector(Jamb,FrameEdge,180),Axis?FVector(140,60,360):FVector(60,140,360),ESurface::Accent);
    }
    // Internal modular partitions, with two normal-height openings per alcove.
    auto PartitionX = [&](double X,double Y0,double Y1,double Door)
    {
        Box(FVector(X,(Y0+Door-300)*.5,450),FVector(60,Door-300-Y0,900));
        Box(FVector(X,(Door+300+Y1)*.5,450),FVector(60,Y1-Door-300,900));
        Box(FVector(X,Door,630),FVector(75,600,540),ESurface::Secondary);
    };
    auto PartitionY = [&](double Y,double X0,double X1,double Door)
    {
        Box(FVector((X0+Door-300)*.5,Y,450),FVector(Door-300-X0,60,900));
        Box(FVector((Door+300+X1)*.5,Y,450),FVector(X1-Door-300,60,900));
        Box(FVector(Door,Y,630),FVector(600,75,540),ESurface::Secondary);
    };
    PartitionX(2400,100,1600,900); PartitionY(1600,2400,3900,3100);
    PartitionX(1600,2400,3900,3100); PartitionY(2400,100,1600,900);
    PartitionX(2400,2400,3900,3100); PartitionY(2400,2400,3900,3100);
    for (double Lane : {1700.,2300.})
    {
        Box(FVector(2000,Lane,1.5),FVector(3800,8,2),ESurface::Accent,false);
        Box(FVector(Lane,2000,1.5),FVector(8,3800,2),ESurface::Accent,false);
    }
    // Straight and diagonal ceiling modules, all opaque box geometry.
    Box(FVector(2000,2000,790),FVector(3800,100,100),ESurface::Secondary);
    Box(FVector(2000,2000,730),FVector(80,3800,60),ESurface::Secondary);
    for (double X : {1100.,2900.})
        Box(FVector(X,2000,680),FVector(700,40,12),ESurface::Energy,false);
    // A real switchback descent, not a visual ramp over an unbroken floor.
    // Line the service space between a lower ceiling and this floor, so looking
    // sideways during descent never exposes the void above neighboring rooms.
    const double ServiceHeight=LayerHeight-Layout.CeilingHeight;
    for (double X : {400.,1900.}) Box(FVector(X,1000,-ServiceHeight*.5),FVector(20,1200,ServiceHeight));
    for (double Y : {400.,1600.}) Box(FVector(1150,Y,-ServiceHeight*.5),FVector(1500,20,ServiceHeight));
    auto Flight = [&](FVector Start,FVector End)
    {
        Start = Point(Start); End = Point(End);
        const FRotator Rotation = (End-Start).Rotation();
        const FVector Up = Rotation.RotateVector(FVector::UpVector);
        FBlock Ramp{(Start+End)*.5-Up*15,FVector(FVector::Distance(Start,End),450*Scale,30),Rotation,ESurface::Secondary,true};
        Layout.Blocks.Add(Ramp); Layout.StairFlights.Add(Ramp);
        for (double Side : {-1.,1.})
        {
            auto Rail = Ramp;
            Rail.Center += Rotation.RotateVector(FVector(0,Side*225*Scale,0))+FVector(0,0,100);
            Rail.Size.Y = 16*Scale; Rail.Size.Z = 20; Rail.Surface = ESurface::Accent;
            Layout.Blocks.Add(Rail);
            for (double T : {.15,.5,.85})
            {
                const FVector P=FMath::Lerp(Start,End,T)+Rotation.RotateVector(FVector(0,Side*225*Scale,0));
                Layout.Blocks.Add({P+FVector(0,0,50),FVector(20*Scale,20*Scale,100),Turn.Rotator(),ESurface::Secondary,true});
            }
        }
    };
    Flight(FVector(400,650,0),FVector(1600,650,-LayerHeight*.5));
    Box(FVector(1725,1000,-LayerHeight*.5-15),FVector(250,1200,30),ESurface::Secondary);
    Box(FVector(1840,1000,-LayerHeight*.5+55),FVector(20,1200,110),ESurface::Accent);
    for (double Y : {410.,1590.}) Box(FVector(1725,Y,-LayerHeight*.5+55),FVector(250,20,110),ESurface::Accent);
    Flight(FVector(1600,1350,-LayerHeight*.5),FVector(400,1350,-LayerHeight));
    for (double Y : {380.,1620.}) Box(FVector(1000,Y,55),FVector(1200,25,110),ESurface::Accent);
    Box(FVector(1620,1000,55),FVector(25,1200,110),ESurface::Accent);
    Box(FVector(380,1000,55),FVector(25,250,110),ESurface::Accent);
    // Each archetype furnishes actual alcoves, not an outdoor obstacle field.
    for (FVector Module : {FVector(3100,900,0),FVector(900,3100,0),FVector(3100,3100,0)})
    {
        const double X = Module.X, Y = Module.Y;
        switch (Layout.Room)
        {
        case ERoomType::Concourse:
            Box(FVector(X,Y,40),FVector(750,180,80),ESurface::Secondary);
            Diagonal(FVector(X,Y+340,180),FVector(650,60,360));
            Box(FVector(X,Y-380,200),FVector(750,50,400),ESurface::Glass);
            break;
        case ERoomType::CargoBay:
            for (double DX : {-260.,260.})
            {
                const double H = 200+Random.RandRange(0,2)*100;
                Box(FVector(X+DX,Y,H*.5),FVector(380,600,H),ESurface::Secondary);
                Box(FVector(X+DX,Y,H+5),FVector(390,610,10),ESurface::Accent);
            }
            Diagonal(FVector(X,Y,600),FVector(800,45,45));
            break;
        case ERoomType::Reactor:
            Box(FVector(X,Y,35),FVector(650,650,70),ESurface::Secondary);
            Box(FVector(X,Y,290),FVector(330,330,440),ESurface::Energy,false);
            for (double Side : {-1.,1.})
            {
                Diagonal(FVector(X+Side*240,Y,280),FVector(70,420,500),ESurface::Structure,Side*20);
                Box(FVector(X,Y+Side*270,280),FVector(600,70,500));
            }
            Box(FVector(X,Y,560),FVector(650,650,80),ESurface::Secondary);
            break;
        case ERoomType::ServerHall:
            for (double DX : {-300.,300.}) for (double DY : {-300.,300.})
            {
                Box(FVector(X+DX,Y+DY,170),FVector(280,350,340),ESurface::Secondary);
                Box(FVector(X+DX-145,Y+DY,180),FVector(10,300,270),ESurface::Glass);
                for (int32 Slot=0; Slot<4; ++Slot)
                    Box(FVector(X+DX-152,Y+DY,80+Slot*60),FVector(6,260,6),ESurface::Energy,false);
            }
            break;
        case ERoomType::Fabrication:
            Box(FVector(X,Y,70),FVector(850,600,140),ESurface::Secondary);
            for (double Side : {-1.,1.})
            {
                Box(FVector(X+Side*350,Y,280),FVector(70,500,420));
                Diagonal(FVector(X,Y+Side*200,370),FVector(600,50,80),ESurface::Accent,Side*30);
            }
            Box(FVector(X,Y,500),FVector(850,600,60),ESurface::Secondary);
            break;
        case ERoomType::Foundry:
            Box(FVector(X,Y,75),FVector(850,700,150),ESurface::Secondary);
            Box(FVector(X,Y,154),FVector(760,610,8),ESurface::Energy,false);
            for (int32 I=0; I<5; ++I) Box(FVector(X-340+I*170,Y,170),FVector(25,700,25));
            Box(FVector(X,Y,650),FVector(950,120,120),ESurface::Accent);
            break;
        case ERoomType::Transit:
            Box(FVector(X,Y,160),FVector(800,600,320),ESurface::Secondary);
            Box(FVector(X-410,Y,220),FVector(15,500,110),ESurface::Glass);
            Diagonal(FVector(X,Y,335),FVector(750,520,30),ESurface::Accent);
            break;
        case ERoomType::Arena:
            Diagonal(FVector(X,Y,60),FVector(850,120,120),ESurface::Secondary);
            Box(FVector(X+250,Y+330,80),FVector(400,250,160),ESurface::Accent);
            for (double Side : {-1.,1.}) Box(FVector(X-330,Y+Side*240,130),FVector(60,60,260));
            Box(FVector(X-330,Y,185),FVector(80,540,55));
            break;
        default: break;
        }
    }
    for (FVector P : {FVector(2000,900,125),FVector(3100,2000,125)})
    {
        const FVector Position = Point(P);
        const FVector ToCenter = Point(FVector(2000,2000,125))-Position;
        const FVector Direction = Turn.RotateVector(P.X == 2000?FVector(0,1,0):FVector(1,0,0));
        const bool Moving = Layout.Room == ERoomType::Arena || Layout.Room == ERoomType::Transit;
        Layout.Targets.Add({Position,FRotator(0,ToCenter.Rotation().Yaw+180,0),Direction,Moving?float(170*Scale):0.f,float(Random.FRandRange(.8,1.3))});
    }
    return Layout;
}
