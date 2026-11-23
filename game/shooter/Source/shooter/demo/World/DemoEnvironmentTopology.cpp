#include "demo/World/DemoEnvironmentLayout.h"

namespace DemoEnvironment
{
    namespace
    {
        constexpr int32 DistrictWidth = 4;
        constexpr int32 CellCount = DistrictWidth * DistrictWidth;
        struct FDistrict
        {
            FCellPlan Cells[CellCount];
        };
        struct FEdge { int32 A, B, Direction; };

        FIntVector DistrictAt(FIntVector Cell)
        {
            return FIntVector(FMath::FloorToInt(double(Cell.X)/DistrictWidth),
                FMath::FloorToInt(double(Cell.Y)/DistrictWidth),Cell.Z);
        }
        FDistrict BuildDistrict(int32 Seed,FIntVector District)
        {
            FDistrict Result;
            FRandomStream Random(int32(CellSeed(Seed^0x31a7c953,District)));
            int32 Groups[CellCount], Parents[CellCount];
            for (int32 I=0; I<CellCount; ++I) Groups[I]=Parents[I]=I;

            // One 2x2 chamber and up to three two-cell wings. The rest are
            // independent rooms, so a district is not sixteen identical boxes.
            const int32 LargeX=Random.RandRange(0,2), LargeY=Random.RandRange(0,2);
            const int32 Large=LargeY*DistrictWidth+LargeX;
            for (int32 Y=0; Y<2; ++Y) for (int32 X=0; X<2; ++X)
                Groups[Large+Y*DistrictWidth+X]=Large;
            for (int32 Attempt=0, Paired=0; Attempt<48 && Paired<3; ++Attempt)
            {
                const int32 A=Random.RandRange(0,CellCount-1);
                const bool Horizontal=Random.RandRange(0,1)!=0;
                const int32 B=A+(Horizontal?1:DistrictWidth);
                if (B>=CellCount || (Horizontal && A%DistrictWidth==DistrictWidth-1)) continue;
                if (Groups[A]!=A || Groups[B]!=B || A==Large || B==Large) continue;
                // An anchor already owning a partner must not be paired again.
                bool Used=false;
                for (int32 I=0; I<CellCount; ++I) Used|=(I!=A && Groups[I]==A)||(I!=B && Groups[I]==B);
                if (Used) continue;
                Groups[B]=A; ++Paired;
            }
            TArray<FEdge> Edges;
            for (int32 Y=0; Y<DistrictWidth; ++Y) for (int32 X=0; X<DistrictWidth; ++X)
            {
                const int32 A=Y*DistrictWidth+X;
                if (X+1<DistrictWidth) Edges.Add({A,A+1,1});
                if (Y+1<DistrictWidth) Edges.Add({A,A+DistrictWidth,3});
            }
            auto Root=[&](int32 I) { while (Parents[I]!=I) I=Parents[I]; return I; };
            auto Open=[&](const FEdge& Edge,bool Wide)
            {
                Result.Cells[Edge.A].Portals[Edge.Direction]={true,Wide};
                Result.Cells[Edge.B].Portals[Edge.Direction^1]={true,Wide};
                Parents[Root(Edge.B)]=Root(Edge.A);
            };
            for (const auto& Edge:Edges) if (Groups[Edge.A]==Groups[Edge.B]) Open(Edge,true);
            for (int32 I=Edges.Num()-1; I>0; --I) Edges.Swap(I,Random.RandRange(0,I));
            // A randomized spanning tree guarantees access to every room. Extra
            // edges make loops; absent edges become solid walls, not fake doors.
            for (const auto& Edge:Edges)
                if (Root(Edge.A)!=Root(Edge.B) || Random.FRand()<.14f) Open(Edge,Groups[Edge.A]==Groups[Edge.B]);

            // Each macro boundary has exactly one shared gate. Its hash uses the
            // lower district coordinate, so generation order cannot split a seam.
            for (int32 Direction=0; Direction<4; ++Direction)
            {
                FIntVector Owner=District;
                if (Direction==0) --Owner.X;
                if (Direction==2) --Owner.Y;
                const int32 Gate=CellSeed(Seed^(Direction<2?0x14275391:0x51b84397),Owner)%DistrictWidth;
                const int32 I=Direction<2?Gate*DistrictWidth+(Direction==0?0:3):(Direction==2?0:12)+Gate;
                Result.Cells[I].Portals[Direction].bOpen=true;
            }
            // Vertical columns are selected using XY only, so stair apertures
            // align at every depth. Three columns per district keep descent near.
            FRandomStream Columns(int32(CellSeed(Seed^0x6a129857,FIntPoint(District.X,District.Y))));
            TArray<int32> Indices;
            for (int32 I=0; I<CellCount; ++I) Indices.Add(I);
            for (int32 I=0; I<3; ++I)
            {
                Indices.Swap(I,Columns.RandRange(I,CellCount-1));
                Result.Cells[Indices[I]].bDescent=true;
            }
            for (int32 I=0; I<CellCount; ++I)
            {
                auto& Plan=Result.Cells[I];
                Plan.Chamber=FIntPoint(District.X*DistrictWidth+Groups[I]%DistrictWidth,
                    District.Y*DistrictWidth+Groups[I]/DistrictWidth);
                const uint32 Identity=CellSeed(Seed^0x1d872b41,FIntVector(Plan.Chamber.X,Plan.Chamber.Y,District.Z));
                Plan.Room=ERoomType(Identity%uint32(ERoomType::Count));
                Plan.Form=ERoomForm((Identity>>8)%uint32(ERoomForm::Count));
                Plan.CeilingHeight=650.+((Identity>>16)%4)*150.;
            }
            // Multi-source breadth-first routing labels the actual graph path to
            // descent, rather than pointing through a closed wall at nearby stairs.
            TArray<int32> Queue;
            bool Visited[CellCount]={};
            for (int32 I=0; I<CellCount; ++I) if (Result.Cells[I].bDescent) { Queue.Add(I); Visited[I]=true; }
            const int32 Steps[]={-1,1,-DistrictWidth,DistrictWidth};
            for (int32 Head=0; Head<Queue.Num(); ++Head)
            {
                const int32 A=Queue[Head];
                for (int32 D=0; D<4; ++D)
                {
                    const int32 B=A+Steps[D];
                    if (B<0 || B>=CellCount || (D<2 && B/DistrictWidth!=A/DistrictWidth)) continue;
                    if (!Visited[B] && Result.Cells[A].Portals[D].bOpen)
                    {
                        Visited[B]=true; Result.Cells[B].DescentDirection=D^1; Queue.Add(B);
                    }
                }
            }
            return Result;
        }
    }
    FCellPlan PlanCell(int32 Seed,FIntVector Cell)
    {
        const FIntVector District=DistrictAt(Cell);
        const auto Graph=BuildDistrict(Seed,District);
        return Graph.Cells[(Cell.Y-District.Y*DistrictWidth)*DistrictWidth+Cell.X-District.X*DistrictWidth];
    }
}
