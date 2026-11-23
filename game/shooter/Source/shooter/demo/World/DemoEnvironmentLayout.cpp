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
    case ERoomType::Hydroponics: return TEXT("HYDROPONICS");
    case ERoomType::Laboratory: return TEXT("LABORATORY");
    case ERoomType::MedBay: return TEXT("MEDICAL");
    case ERoomType::Quarters: return TEXT("CREW QUARTERS");
    case ERoomType::MessHall: return TEXT("MESS HALL");
    case ERoomType::Security: return TEXT("SECURITY");
    case ERoomType::Armory: return TEXT("ARMORY");
    case ERoomType::Archives: return TEXT("ARCHIVES");
    case ERoomType::PowerStation: return TEXT("POWER STATION");
    case ERoomType::Cooling: return TEXT("COOLANT EXCHANGE");
    case ERoomType::Maintenance: return TEXT("MAINTENANCE");
    case ERoomType::FreightDepot: return TEXT("FREIGHT DEPOT");
    case ERoomType::ControlRoom: return TEXT("CONTROL ROOM");
    case ERoomType::Ventilation: return TEXT("VENTILATION");
    case ERoomType::Training: return TEXT("TRAINING COURSE");
    case ERoomType::Checkpoint: return TEXT("CHECKPOINT");
    default: return TEXT("FACILITY");
    }
}
DemoEnvironment::FLayout DemoEnvironment::Generate(int32 Seed,FIntVector Cell,double Size,double LayerHeight)
{
    FLayout Layout;
    Layout.Plan = PlanCell(Seed,Cell);
    Layout.LayerHeight = LayerHeight;
    Layout.CeilingHeight = FMath::Min(Layout.Plan.CeilingHeight,LayerHeight-200.);
    Layout.bCovered = true;
    FRandomStream Random(int32(CellSeed(Seed,Cell)));
    Layout.Room = Layout.Plan.Room;
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
        if (!Layout.Plan.bDescent)
        {
            const auto Panel=MakeBox(FVector(2000,2000,Z),FVector(4000,4000,Thickness));
            if (Roof) Layout.Blocks.Add(Panel); else Layout.Floors.Add(Panel);
            return;
        }
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
    // Shells obey the district graph in world axes. A closed graph edge is a
    // continuous wall; a shared chamber removes most of the dividing wall.
    for (int32 D=0; D<4; ++D)
    {
        const bool Axis=D<2;
        const double Edge=(D%2==0)?70.:3930.;
        const auto Portal=Layout.Plan.Portals[D];
        const double Width=Portal.bWide?2800.:800.;
        const double Height=Portal.bWide?Layout.CeilingHeight:360.;
        auto Wall=[&](double Mid,double Span,double Z,double H,ESurface Surface)
        {
            FVector P=Axis?FVector(Edge,Mid,Z):FVector(Mid,Edge,Z);
            FVector S=Axis?FVector(140,Span,H):FVector(Span,140,H);
            P.X*=Scale; P.Y*=Scale; S.X*=Scale; S.Y*=Scale;
            Layout.Blocks.Add({P,S,FRotator::ZeroRotator,Surface,true});
        };
        if (!Portal.bOpen) { Wall(2000,4000,LayerHeight*.5,LayerHeight,ESurface::Structure); continue; }
        const double Side=(4000.-Width)*.5;
        Wall(Side*.5,Side,LayerHeight*.5,LayerHeight,ESurface::Structure);
        Wall(4000.-Side*.5,Side,LayerHeight*.5,LayerHeight,ESurface::Structure);
        Wall(2000,Width,(LayerHeight+Height)*.5,LayerHeight-Height,ESurface::Secondary);
        for (double Jamb:{2000.-Width*.5-30.,2000.+Width*.5+30.})
            Wall(Jamb,60,Height*.5,Height,ESurface::Accent);
    }
    // A second grammar controls the plan within a room: open halls, segmented
    // suites, galleries, machine bays, angular corners and structural colonnades.
    const double Ceiling=Layout.CeilingHeight;
    auto PartitionX = [&](double X,double Y0,double Y1,double Door)
    {
        Box(FVector(X,(Y0+Door-300)*.5,Ceiling*.5),FVector(60,Door-300-Y0,Ceiling));
        Box(FVector(X,(Door+300+Y1)*.5,Ceiling*.5),FVector(60,Y1-Door-300,Ceiling));
        Box(FVector(X,Door,(Ceiling+360)*.5),FVector(75,600,Ceiling-360),ESurface::Secondary);
    };
    auto PartitionY = [&](double Y,double X0,double X1,double Door)
    {
        Box(FVector((X0+Door-300)*.5,Y,Ceiling*.5),FVector(Door-300-X0,60,Ceiling));
        Box(FVector((Door+300+X1)*.5,Y,Ceiling*.5),FVector(X1-Door-300,60,Ceiling));
        Box(FVector(Door,Y,(Ceiling+360)*.5),FVector(600,75,Ceiling-360),ESurface::Secondary);
    };
    TArray<FVector> Bays={FVector(3100,900,0),FVector(900,3100,0),FVector(3100,3100,0)};
    if (!Layout.Plan.bDescent) Bays.Add(FVector(900,900,0));
    for (const FVector Bay:Bays)
    {
        const double InnerX=Bay.X<2000?1600.:2400., InnerY=Bay.Y<2000?1600.:2400.;
        const double X0=Bay.X<2000?100.:2400., Y0=Bay.Y<2000?100.:2400.;
        const double DoorX=Bay.X+Random.FRandRange(-120,120), DoorY=Bay.Y+Random.FRandRange(-120,120);
        switch (Layout.Plan.Form)
        {
        case ERoomForm::Suites:
            PartitionX(InnerX,Y0,Y0+1500,DoorY); PartitionY(InnerY,X0,X0+1500,DoorX); break;
        case ERoomForm::Gallery:
            PartitionX(InnerX,Y0,Y0+1500,DoorY);
            Box(FVector(Bay.X,InnerY,100),FVector(700,60,200),ESurface::Glass); break;
        case ERoomForm::Workshop:
            Box(FVector(InnerX,Bay.Y,Ceiling-100),FVector(100,1400,200),ESurface::Accent);
            for (double Side:{-1.,1.}) Box(FVector(InnerX,Bay.Y+Side*620,(Ceiling-200)*.5),FVector(90,90,Ceiling-200));
            break;
        case ERoomForm::Diagonal:
            Diagonal(FVector(Bay.X+(Bay.X<2000?-450.:450.),Bay.Y+(Bay.Y<2000?-450.:450.),Ceiling*.5),
                FVector(850,100,Ceiling),ESurface::Secondary,Bay.X==Bay.Y?-45.:45.); break;
        case ERoomForm::Pillared:
            for (double Side:{-1.,1.})
                Box(FVector(Bay.X+Side*530,Bay.Y,Ceiling*.5),FVector(120,160,Ceiling),ESurface::Secondary);
            Box(FVector(Bay.X,Bay.Y,Ceiling-100),FVector(1250,200,200),ESurface::Accent); break;
        default: break;
        }
    }
    for (double Lane : {1700.,2300.})
    {
        Box(FVector(2000,Lane,1.5),FVector(3800,8,2),ESurface::Accent,false);
        Box(FVector(Lane,2000,1.5),FVector(8,3800,2),ESurface::Accent,false);
    }
    // Straight and diagonal ceiling modules, all opaque box geometry.
    Box(FVector(2000,2000,Ceiling-60),FVector(3800,100,100),ESurface::Secondary);
    Box(FVector(2000,2000,Ceiling-120),FVector(80,3800,60),ESurface::Secondary);
    for (double X : {1100.,2900.})
        Box(FVector(X,2000,Ceiling-160),FVector(700,40,12),ESurface::Energy,false);
    if (Layout.Plan.bDescent)
    {
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
    }
    // Purpose-specific equipment is composed independently of the room plan.
    for (FVector Module : Bays)
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
            Box(FVector(X,Y,Ceiling-100),FVector(950,120,120),ESurface::Accent);
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
        case ERoomType::Hydroponics:
            for (double Side:{-1.,1.})
            {
                Box(FVector(X+Side*280,Y,65),FVector(330,900,130),ESurface::Secondary);
                for (int32 I=0; I<5; ++I)
                {
                    const double H=Random.FRandRange(100,250);
                    Box(FVector(X+Side*280,Y-350+I*175,130+H*.5),FVector(160,90,H),ESurface::Accent);
                    Diagonal(FVector(X+Side*280,Y-350+I*175,200+H*.3),FVector(230,130,35),ESurface::Secondary,Side*35);
                }
                Box(FVector(X+Side*280,Y,Ceiling-160),FVector(30,950,20),ESurface::Energy,false);
            }
            break;
        case ERoomType::Laboratory:
            for (double Side:{-1.,1.})
            {
                Box(FVector(X+Side*320,Y,95),FVector(250,900,190),ESurface::Secondary);
                Box(FVector(X+Side*320,Y,200),FVector(280,940,20),ESurface::Accent);
                for (double DY:{-250.,250.})
                {
                    Box(FVector(X+Side*320,Y+DY,290),FVector(160,180,160),ESurface::Glass);
                    Box(FVector(X+Side*320,Y+DY,380),FVector(190,210,20));
                    Box(FVector(X+Side*320,Y+DY,280),FVector(55,60,110),ESurface::Energy,false);
                }
            }
            break;
        case ERoomType::MedBay:
            for (double DY:{-300.,300.})
            {
                Box(FVector(X,Y+DY,65),FVector(700,260,130),ESurface::Secondary);
                Box(FVector(X,Y+DY,140),FVector(650,240,20),ESurface::Accent);
                Box(FVector(X+330,Y+DY,210),FVector(40,280,170));
                Box(FVector(X+420,Y+DY,310),FVector(100,140,100),ESurface::Glass);
            }
            Box(FVector(X-470,Y,220),FVector(100,140,60),ESurface::Energy,false);
            Box(FVector(X-470,Y,220),FVector(100,60,140),ESurface::Energy,false);
            break;
        case ERoomType::Quarters:
            for (double Side:{-1.,1.})
            {
                for (double Z:{80.,280.})
                    Box(FVector(X+Side*340,Y,Z),FVector(240,850,55),ESurface::Secondary);
                for (double DY:{-440.,440.})
                    Box(FVector(X+Side*340,Y+DY,200),FVector(260,30,400));
                Box(FVector(X+Side*340,Y+250,325),FVector(220,170,35),ESurface::Accent);
            }
            Box(FVector(X,Y+500,150),FVector(180,160,300),ESurface::Secondary);
            break;
        case ERoomType::MessHall:
            for (double DY:{-300.,300.})
            {
                Box(FVector(X,Y+DY,120),FVector(850,220,25),ESurface::Secondary);
                Box(FVector(X,Y+DY,55),FVector(80,160,110));
                for (double Side:{-1.,1.})
                    Box(FVector(X,Y+DY+Side*150,65),FVector(800,65,50),ESurface::Accent);
                for (int32 I=0; I<3; ++I)
                    Box(FVector(X-260+I*260,Y+DY,145),FVector(65,100,25),ESurface::Accent,false);
            }
            break;
        case ERoomType::Security:
            Box(FVector(X,Y,90),FVector(900,350,180),ESurface::Secondary);
            for (double DX:{-300.,0.,300.})
            {
                Box(FVector(X+DX,Y+100,260),FVector(230,45,150));
                Box(FVector(X+DX,Y+72,260),FVector(190,8,110),ESurface::Energy,false);
            }
            Box(FVector(X,Y+400,220),FVector(900,80,440),ESurface::Glass);
            break;
        case ERoomType::Armory:
            for (double Side:{-1.,1.})
            {
                Box(FVector(X+Side*420,Y,210),FVector(140,1000,420),ESurface::Secondary);
                for (int32 I=0; I<6; ++I)
                {
                    Box(FVector(X+Side*330,Y-400+I*160,240),FVector(70,35,200));
                    Diagonal(FVector(X+Side*300,Y-400+I*160,170),FVector(130,35,55),ESurface::Accent,Side*20);
                }
            }
            Box(FVector(X,Y,65),FVector(250,600,130),ESurface::Secondary);
            break;
        case ERoomType::Archives:
            for (double DX:{-400.,0.,400.})
            {
                for (double DY:{-350.,350.}) Box(FVector(X+DX,Y+DY,240),FVector(35,35,480));
                for (double Z:{60.,200.,340.,480.})
                {
                    Box(FVector(X+DX,Y,Z),FVector(210,740,20),ESurface::Secondary);
                    for (int32 I=0; I<4; ++I)
                        Box(FVector(X+DX,Y-270+I*180,Z+60),FVector(150,140,100),I%2?ESurface::Accent:ESurface::Structure);
                }
            }
            break;
        case ERoomType::PowerStation:
            for (double DX:{-340.,340.})
            {
                Box(FVector(X+DX,Y,220),FVector(240,850,440),ESurface::Secondary);
                for (int32 I=0; I<5; ++I)
                {
                    Box(FVector(X+DX,Y-320+I*160,220),FVector(270,35,360));
                    Box(FVector(X+DX,Y-320+I*160,450),FVector(180,70,15),ESurface::Energy,false);
                }
            }
            Box(FVector(X,Y,Ceiling-90),FVector(1000,180,100),ESurface::Accent);
            break;
        case ERoomType::Cooling:
            for (double DY:{-350.,350.})
            {
                Box(FVector(X,Y+DY,170),FVector(900,200,340),ESurface::Secondary);
                for (int32 I=0; I<8; ++I)
                    Box(FVector(X-380+I*110,Y+DY,180),FVector(30,230,280),ESurface::Accent);
            }
            // Rectangular ductwork; the kit never introduces round pipes.
            Box(FVector(X,Y,Ceiling-120),FVector(1000,260,180));
            for (double DX:{-400.,400.}) Box(FVector(X+DX,Y+350,Ceiling*.5),FVector(110,110,Ceiling));
            break;
        case ERoomType::Maintenance:
            Box(FVector(X,Y+300,80),FVector(900,280,160),ESurface::Secondary);
            Box(FVector(X,Y+430,280),FVector(900,40,250));
            for (int32 I=0; I<6; ++I)
                Box(FVector(X-350+I*140,Y+400,280),FVector(35,35,Random.FRandRange(70,160)),ESurface::Accent);
            Box(FVector(X-300,Y-250,100),FVector(240,320,200),ESurface::Accent);
            Box(FVector(X+300,Y-250,60),FVector(300,350,120),ESurface::Secondary);
            break;
        case ERoomType::FreightDepot:
            Box(FVector(X,Y,30),FVector(1050,850,60),ESurface::Secondary);
            for (int32 I=0; I<4; ++I)
                Box(FVector(X-390+I*260,Y,75),FVector(80,900,45),ESurface::Accent);
            Box(FVector(X,Y+170,230),FVector(650,400,280));
            Box(FVector(X,Y+170,380),FVector(670,420,20),ESurface::Accent);
            for (double DX:{-500.,500.}) Box(FVector(X+DX,Y,Ceiling*.5),FVector(80,100,Ceiling),ESurface::Secondary);
            Box(FVector(X,Y,Ceiling-60),FVector(1100,100,100),ESurface::Accent);
            break;
        case ERoomType::ControlRoom:
            for (double DX:{-340.,0.,340.})
            {
                Diagonal(FVector(X+DX,Y,130),FVector(280,350,160),ESurface::Secondary,15);
                Box(FVector(X+DX,Y+350,350),FVector(310,60,270));
                Box(FVector(X+DX,Y+310,350),FVector(270,15,220),ESurface::Energy,false);
            }
            Box(FVector(X,Y-330,60),FVector(750,160,120),ESurface::Accent);
            break;
        case ERoomType::Ventilation:
            for (double Side:{-1.,1.})
            {
                Box(FVector(X+Side*360,Y,Ceiling*.5),FVector(300,800,Ceiling),ESurface::Secondary);
                for (int32 I=0; I<7; ++I)
                    Box(FVector(X+Side*195,Y,80+I*70),FVector(25,680,20),ESurface::Accent);
            }
            Box(FVector(X,Y,Ceiling-80),FVector(600,700,120));
            break;
        case ERoomType::Training:
            for (int32 I=0; I<3; ++I)
            {
                const double H=80+I*65;
                Box(FVector(X-340+I*340,Y+(I%2?260.:-260.),H*.5),FVector(240,400,H),ESurface::Secondary);
                Box(FVector(X-340+I*340,Y+(I%2?260.:-260.),H+3),FVector(245,405,6),ESurface::Accent);
            }
            Diagonal(FVector(X,Y,420),FVector(950,70,70),ESurface::Accent,-30);
            break;
        case ERoomType::Checkpoint:
            for (double DX:{-400.,400.})
            {
                Box(FVector(X+DX,Y,200),FVector(180,650,400),ESurface::Secondary);
                Box(FVector(X+DX,Y-335,260),FVector(120,15,160),ESurface::Glass);
            }
            Box(FVector(X,Y,460),FVector(1000,180,120),ESurface::Accent);
            for (double DY:{-420.,420.}) Box(FVector(X,Y+DY,3),FVector(900,80,5),ESurface::Accent,false);
            break;
        default: break;
        }
    }
    for (FVector P : {FVector(2250,900,125),FVector(3100,2250,125)})
    {
        const FVector Position = Point(P);
        const FVector ToCenter = Point(FVector(2000,2000,125))-Position;
        const FVector Direction = Turn.RotateVector(P.Y == 900?FVector(0,1,0):FVector(1,0,0));
        const bool Moving = Layout.Room == ERoomType::Arena || Layout.Room == ERoomType::Transit;
        Layout.Targets.Add({Position,FRotator(0,ToCenter.Rotation().Yaw+180,0),Direction,Moving?float(170*Scale):0.f,float(Random.FRandRange(.8,1.3))});
    }
    return Layout;
}
