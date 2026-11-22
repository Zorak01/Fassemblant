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

const TCHAR* DemoEnvironment::RoomName(ERoomType Room)
{
    switch (Room)
    {
    case ERoomType::Atrium: return TEXT("ATRIUM");
    case ERoomType::CargoBay: return TEXT("CARGO BAY");
    case ERoomType::Reactor: return TEXT("REACTOR");
    case ERoomType::ServerHall: return TEXT("SERVER HALL");
    case ERoomType::Hydroponics: return TEXT("HYDROPONICS");
    case ERoomType::Foundry: return TEXT("FOUNDRY");
    case ERoomType::Transit: return TEXT("TRANSIT");
    case ERoomType::Arena: return TEXT("COMBAT ARENA");
    default: return TEXT("FACILITY");
    }
}
DemoEnvironment::FLayout DemoEnvironment::Generate(int32 Seed, FIntPoint Cell, double Size)
{
    FLayout Layout;
    FRandomStream Random(static_cast<int32>(CellSeed(Seed, Cell)));
    Layout.Room = static_cast<ERoomType>(Random.RandRange(0,int32(ERoomType::Count)-1));
    const FIntPoint District = CellAt(FVector(Cell.X,Cell.Y,0),4.);
    Layout.Theme = static_cast<ETheme>(CellSeed(Seed ^ 0x61c88647,District) % uint32(ETheme::Count));
    switch (Layout.Theme)
    {
    case ETheme::Arctic: Layout.LightColor = FLinearColor(.65,.82,1); break;
    case ETheme::Industrial: Layout.LightColor = FLinearColor(1,.86,.68); break;
    case ETheme::Botanical: Layout.LightColor = FLinearColor(.78,1,.84); break;
    case ETheme::Hazard: Layout.LightColor = FLinearColor(1,.69,.44); break;
    default: break;
    }
    Layout.bCovered = Layout.Room != ERoomType::Atrium && Layout.Room != ERoomType::Arena;
    Layout.CeilingHeight = Layout.Room == ERoomType::Reactor || Layout.Room == ERoomType::Foundry ? 1050 : 800;
    const double Scale = Size / 6000.;
    const FQuat Turn = FRotator(0,Random.RandRange(0,3)*90.f,0).Quaternion();
    auto Point = [Scale,Turn](FVector P)
    {
        P = FVector(3000,3000,0) + Turn.RotateVector(P-FVector(3000,3000,0));
        P.X *= Scale; P.Y *= Scale;
        return P;
    };
    auto Block = [&](FVector Center,FVector Dimensions,ESurface Surface=ESurface::Structure,
        EShape Shape=EShape::Box,FRotator Rotation=FRotator::ZeroRotator,bool bCollision=true)
    {
        Dimensions.X *= Scale; Dimensions.Y *= Scale;
        if (Shape == EShape::Cylinder && (FMath::Abs(Rotation.Pitch) > 45.f || FMath::Abs(Rotation.Roll) > 45.f))
            Dimensions.Z *= Scale;
        // Stretch horizontal room dimensions while preserving ramp rise.
        if (Shape == EShape::Box && FMath::Abs(Rotation.Pitch) < 45.f)
            Rotation.Pitch = FMath::RadiansToDegrees(FMath::Atan(FMath::Tan(FMath::DegreesToRadians(Rotation.Pitch))/Scale));
        Layout.Blocks.Add({Point(Center),Dimensions,(Turn*Rotation.Quaternion()).Rotator(),Surface,Shape,bCollision});
    };
    auto Box = [&](FVector P,FVector D,ESurface S=ESurface::Structure,bool Collision=true)
    { Block(P,D,S,EShape::Box,FRotator::ZeroRotator,Collision); };
    auto Cylinder = [&](FVector P,FVector D,ESurface S=ESurface::Secondary,FRotator R=FRotator::ZeroRotator,bool Collision=true)
    { Block(P,D,S,EShape::Cylinder,R,Collision); };
    auto Sphere = [&](FVector P,FVector D,ESurface S=ESurface::Foliage)
    { Block(P,D,S,EShape::Sphere,FRotator::ZeroRotator,false); };

    // Identical cardinal door positions and floor height allow arbitrary neighbors
    // to connect. Border streets remain open even between two enclosed rooms.
    const double Height = Layout.CeilingHeight;
    const bool Open = Layout.Room == ERoomType::Arena;
    for (bool EastWest : {false,true}) for (double Side : {160.,5840.})
    {
        for (double Segment : {1360.,4640.})
        {
            if (!Open)
                Box(EastWest?FVector(Side,Segment,Height*.5):FVector(Segment,Side,Height*.5),
                    EastWest?FVector(100,2400,Height):FVector(2400,100,Height));
            else
                Box(EastWest?FVector(Side,Segment,150):FVector(Segment,Side,150),
                    EastWest?FVector(100,1700,300):FVector(1700,100,300));
        }
        Box(EastWest?FVector(Side,3000,(Height+360)*.5):FVector(3000,Side,(Height+360)*.5),
            EastWest?FVector(120,1040,Height-360):FVector(1040,120,Height-360),ESurface::Secondary);
        for (double Edge : {2460.,3540.})
        {
            const double Face = Side < 3000 ? Side-65 : Side+65;
            Box(EastWest?FVector(Side,Edge,270):FVector(Edge,Side,270),
                EastWest?FVector(135,55,540):FVector(55,135,540),ESurface::Secondary);
            Box(EastWest?FVector(Face,Edge,270):FVector(Edge,Face,270),
                EastWest?FVector(8,26,450):FVector(26,8,450),ESurface::Energy,false);
        }
    }
    if (Layout.bCovered)
    {
        Box(FVector(3000,3000,Height+25),FVector(5580,5580,50));
        for (double Lane : {1500.,4500.})
        {
            Box(FVector(3000,Lane,Height-50),FVector(5400,90,90),ESurface::Secondary);
            Box(FVector(Lane,3000,Height-110),FVector(70,5000,55),ESurface::Secondary);
            Box(FVector(3000,Lane,Height-100),FVector(3300,35,8),ESurface::Energy,false);
        }
    }
    else
    {
        for (double Lane : {700.,5300.})
            Box(FVector(3000,Lane,720),FVector(5400,100,100),ESurface::Secondary);
    }
    // Flush guidance strips never change collision or obstruct movement.
    for (double Lane : {2640.,3360.})
    {
        Box(FVector(3000,Lane,1.5),FVector(5500,10,2),ESurface::Accent,false);
        Box(FVector(Lane,3000,1.5),FVector(10,5500,2),ESurface::Accent,false);
    }
    const double CoverHeight = Random.FRandRange(100,135);
    switch (Layout.Room)
    {
    case ERoomType::Atrium:
        for (double X : {1400.,4600.}) for (double Y : {1400.,4600.})
        {
            Cylinder(FVector(X,Y,350),FVector(150,150,700));
            Cylinder(FVector(X,Y,35),FVector(240,240,70),ESurface::Accent);
            Box(FVector(X,Y+370,28),FVector(850,150,56),ESurface::Secondary);
            Box(FVector(X,Y-370,28),FVector(850,150,56),ESurface::Secondary);
            Sphere(FVector(X,Y,690),FVector(400,400,70),ESurface::Energy);
        }
        // An open skylight and suspended framing create a recognizable silhouette.
        Box(FVector(3000,3000,760),FVector(1800,80,80),ESurface::Secondary);
        Box(FVector(3000,3000,760),FVector(80,1800,80),ESurface::Secondary);
        break;
    case ERoomType::CargoBay:
        for (double X : {1050.,4950.}) for (double Y : {1500.,4500.})
        {
            Box(FVector(X,Y,210),FVector(1200,1100,420),ESurface::Secondary);
            for (int32 Rib=0; Rib<5; ++Rib)
                Box(FVector(X-460+Rib*230,Y,215),FVector(25,1120,390));
            Box(FVector(X,Y,425),FVector(1220,1120,10),ESurface::Accent);
        }
        for (double Y : {1500.,4500.})
        {
            Block(FVector(1900,Y,210),FVector(1600,380,35),ESurface::Structure,EShape::Box,FRotator(-14.5,0,0));
            Box(FVector(3000,Y,405),FVector(3900,360,30),ESurface::Secondary);
            for (double Side : {-210.,210.})
                Box(FVector(3000,Y+Side,475),FVector(3900,22,110),ESurface::Accent);
        }
        break;
    case ERoomType::Reactor:
        for (double X : {1550.,4450.}) for (double Y : {1550.,4450.})
        {
            Cylinder(FVector(X,Y,50),FVector(800,800,100));
            Cylinder(FVector(X,Y,340),FVector(440,440,510),ESurface::Energy);
            Cylinder(FVector(X,Y,625),FVector(800,800,100));
            for (double Angle : {0.,120.,240.})
            {
                const double R = FMath::DegreesToRadians(Angle);
                Cylinder(FVector(X+FMath::Cos(R)*320,Y+FMath::Sin(R)*320,340),FVector(65,65,510));
            }
            Cylinder(FVector(X,Y,730),FVector(160,160,190),ESurface::Accent);
        }
        Cylinder(FVector(3000,1550,850),FVector(130,130,2900),ESurface::Secondary,FRotator(90,0,0));
        Cylinder(FVector(3000,4450,850),FVector(130,130,2900),ESurface::Secondary,FRotator(90,0,0));
        break;
    case ERoomType::ServerHall:
        for (double X : {1200.,1900.,4100.,4800.}) for (double Y : {1000.,1850.,4150.,5000.})
        {
            Box(FVector(X,Y,185),FVector(340,560,370),ESurface::Secondary);
            Box(FVector(X-175,Y,195),FVector(12,475,300),ESurface::Glass);
            for (int32 Slot=0; Slot<4; ++Slot)
            {
                Box(FVector(X-184,Y,95+Slot*65),FVector(7,440,8),ESurface::Accent,false);
                Box(FVector(X-189,Y-180,105+Slot*65),FVector(5,20,10),ESurface::Energy,false);
            }
        }
        break;
    case ERoomType::Hydroponics:
        for (double X : {1450.,4550.}) for (double Y : {1450.,4550.})
        {
            Box(FVector(X,Y,35),FVector(1100,1000,70),ESurface::Secondary);
            Box(FVector(X,Y,73),FVector(1000,900,8),ESurface::Glass,false);
            for (double DX : {-300.,300.}) for (double DY : {-230.,230.})
            {
                const float H = Random.FRandRange(160,240);
                Cylinder(FVector(X+DX,Y+DY,70+H*.5),FVector(30,30,H),ESurface::Secondary);
                Sphere(FVector(X+DX,Y+DY,70+H),FVector(350,300,130));
                Sphere(FVector(X+DX+90,Y+DY,140+H),FVector(200,200,90));
            }
            Box(FVector(X,Y,540),FVector(1100,900,25),ESurface::Secondary);
            Box(FVector(X,Y,520),FVector(1000,800,12),ESurface::Energy,false);
        }
        break;
    case ERoomType::Foundry:
        for (double X : {1400.,4600.}) for (double Y : {1450.,4550.})
        {
            Box(FVector(X,Y,70),FVector(1400,1150,140),ESurface::Secondary);
            Box(FVector(X,Y,145),FVector(1220,970,10),ESurface::Energy,false);
            for (int32 Bar=0; Bar<7; ++Bar)
                Box(FVector(X-570+Bar*190,Y,165),FVector(35,1150,35));
            Cylinder(FVector(X,Y,670),FVector(180,180,700),ESurface::Secondary,FRotator(90,0,0));
        }
        for (double X : {1000.,5000.})
            Cylinder(FVector(X,3000,850),FVector(160,160,5000),ESurface::Accent,FRotator(0,0,90));
        break;
    case ERoomType::Transit:
        for (double X : {1450.,4550.}) for (double Y : {1500.,4500.})
        {
            Box(FVector(X,Y,30),FVector(1300,1550,60),ESurface::Secondary);
            Box(FVector(X,Y,230),FVector(1050,1300,340));
            Box(FVector(X,Y,410),FVector(1080,1330,20),ESurface::Accent);
            for (double Side : {-535.,535.})
                Box(FVector(X+Side,Y,285),FVector(16,1000,130),ESurface::Glass);
            for (double Rail : {-450.,450.})
                Box(FVector(X+Rail,Y,7),FVector(25,1900,14),ESurface::Accent);
        }
        break;
    case ERoomType::Arena:
        for (double X : {1450.,4550.}) for (double Y : {1450.,4550.})
        {
            Box(FVector(X,Y,CoverHeight*.5),FVector(150,1000,CoverHeight),ESurface::Secondary);
            Box(FVector(X,Y,CoverHeight+4),FVector(160,1010,8),ESurface::Accent);
            Block(FVector(X+400,Y,92),FVector(740,560,35),ESurface::Structure,EShape::Box,FRotator(12,0,0));
            for (double Side : {-340.,340.}) Box(FVector(X-400,Y+Side,130),FVector(65,65,260));
            Box(FVector(X-400,Y,185),FVector(80,745,55));
        }
        break;
    default: break;
    }
    // Facing and movement are part of the deterministic layout, not shared RNG.
    for (FVector Position : {FVector(3000,1400,125),FVector(4500,3000,125)})
    {
        const FVector WorldPosition = Point(Position);
        const FVector ToCenter = FVector(Size*.5,Size*.5,125)-WorldPosition;
        const bool AlongY = Position.X == 3000;
        const FVector Direction = Turn.RotateVector(AlongY?FVector(0,1,0):FVector(1,0,0));
        const bool Moving = Layout.Room == ERoomType::Arena || Layout.Room == ERoomType::Transit;
        Layout.Targets.Add({WorldPosition,FRotator(0,ToCenter.Rotation().Yaw+180,0),Direction,
            Moving?float(250*Scale):0.f,float(Random.FRandRange(.8f,1.3f))});
    }
    return Layout;
}
