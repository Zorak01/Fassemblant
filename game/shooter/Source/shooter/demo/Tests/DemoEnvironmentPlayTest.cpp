#include "demo/Tests/DemoEnvironmentPlayTest.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/World/DemoEnvironmentLayout.h"
#include "demo/World/DemoProceduralEnvironment.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"

ADemoEnvironmentPlayTest::ADemoEnvironmentPlayTest()
{
    PrimaryActorTick.bCanEverTick=true;
    PrimaryActorTick.TickGroup=TG_PostUpdateWork;
    Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Automation/EnvironmentPlay"));
    FParse::Value(FCommandLine::Get(),TEXT("DemoReportDir="),Directory);
}
void ADemoEnvironmentPlayTest::Check(const FString& Name,bool bPass)
{
    bFailed|=!bPass;
    Report+=FString::Printf(TEXT("%s %s\n"),bPass?TEXT("PASS"):TEXT("FAIL"),*Name);
    UE_LOG(LogTemp,Display,TEXT("ENVIRONMENT_CHECK %s %s"),bPass?TEXT("PASS"):TEXT("FAIL"),*Name);
}
void ADemoEnvironmentPlayTest::Finish()
{
    for (const auto& Filename:Captures) Check(TEXT("Screenshot saved: ")+FPaths::GetCleanFilename(Filename),IFileManager::Get().FileExists(*Filename));
    IFileManager::Get().MakeDirectory(*Directory,true);
    FFileHelper::SaveStringToFile(Report,*(Directory/TEXT("report.txt")));
    UE_LOG(LogTemp,Display,TEXT("ENVIRONMENT_FINISHED %s"),bFailed?TEXT("FAIL"):TEXT("PASS"));
    SetActorTickEnabled(false);
    FPlatformMisc::RequestExitWithStatus(false,bFailed?1:0);
}
void ADemoEnvironmentPlayTest::Place(FIntVector Cell)
{
    const auto Origin=GetWorld()->OriginLocation;
    const double Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    Character->SetActorLocation(FVector((Cell.X+.5)*Environment->ChunkSize-Origin.X,
        (Cell.Y+.5)*Environment->ChunkSize-Origin.Y,Cell.Z*Environment->LayerHeight-Origin.Z+Half+5),false,nullptr,ETeleportType::TeleportPhysics);
    Character->GetCharacterMovement()->StopMovementImmediately();
    Character->GetController()->SetControlRotation(FRotator(-8,45,0));
    Environment->Tick(0);
    Elapsed=0;
}
void ADemoEnvironmentPlayTest::Tick(float Dt)
{
    Super::Tick(Dt);
    Elapsed+=Dt; Total+=Dt;
    if (Total>150) { Check(TEXT("Traversal completed within timeout"),false); Finish(); return; }
    if (!Character) Character=Cast<ADemoCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if (!Environment) Environment=Cast<ADemoProceduralEnvironment>(UGameplayStatics::GetActorOfClass(this,ADemoProceduralEnvironment::StaticClass()));
    if (!Character || !Environment || !Character->GetController()) return;
    const float Half=Character->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    const FVector Feet=Character->GetActorLocation()-FVector(0,0,Half);
    if (Stage==0)
    {
        if (Elapsed<3) return;
        IFileManager::Get().MakeDirectory(*Directory,true);
        // Capture a selection of genuinely different purposes, including the
        // starting district, instead of testing a separate authored test map.
        Samples.Add(FIntVector(0,0,0));
        const DemoEnvironment::ERoomType Types[]={DemoEnvironment::ERoomType::Hydroponics,DemoEnvironment::ERoomType::Laboratory,
            DemoEnvironment::ERoomType::Quarters,DemoEnvironment::ERoomType::Archives,DemoEnvironment::ERoomType::ControlRoom};
        for (const auto Type:Types)
        {
            bool Found=false;
            for (int32 Z=0; Z>=-2 && !Found; --Z) for (int32 X=-6; X<=6 && !Found; ++X) for (int32 Y=-6; Y<=6; ++Y)
            {
                const FIntVector Cell(X,Y,Z);
                if (DemoEnvironment::PlanCell(Environment->WorldSeed,Cell).Room==Type) { Samples.Add(Cell); Found=true; break; }
            }
            Check(TEXT("Representative room exists"),Found);
        }
        Stage=1; Place(Samples[0]); return;
    }
    if (Stage==1)
    {
        if (Elapsed<1.2) return;
        FHitResult Hit;
        FCollisionQueryParams Params(SCENE_QUERY_STAT(EnvironmentPlay),false,Character);
        Check(TEXT("Playable floor beneath room hub"),GetWorld()->LineTraceSingleByChannel(Hit,Feet+FVector(0,0,50),Feet-FVector(0,0,150),ECC_Visibility,Params));
        Check(TEXT("Opaque ceiling above room hub"),GetWorld()->LineTraceSingleByChannel(Hit,Feet+FVector(0,0,200),Feet+FVector(0,0,Environment->LayerHeight),ECC_Visibility,Params));
        const auto Plan=DemoEnvironment::PlanCell(Environment->WorldSeed,Samples[Sample]);
        const FString Filename=Directory/FString::Printf(TEXT("Room_%02d_%s.png"),Sample,DemoEnvironment::RoomName(Plan.Room));
        Captures.Add(Filename); FScreenshotRequest::RequestScreenshot(Filename,false,false,false,FIntRect(),true);
        Stage=2; Elapsed=0; return;
    }
    if (Stage==2)
    {
        if (Elapsed<.5) return;
        if (++Sample<Samples.Num()) { Stage=1; Place(Samples[Sample]); return; }
        // Physically walk across two graph seams, including a turn when present.
        FIntVector Cell(0,0,0);
        Place(Cell); Waypoints.Reset();
        int32 Previous=INDEX_NONE;
        for (int32 I=0; I<3; ++I)
        {
            const auto Plan=DemoEnvironment::PlanCell(Environment->WorldSeed,Cell);
            int32 Direction=INDEX_NONE;
            for (int32 D=0; D<4; ++D) if (Plan.Portals[D].bOpen && D!=Previous) { Direction=D; break; }
            if (Direction==INDEX_NONE) Direction=Previous;
            if (Direction==INDEX_NONE) { Check(TEXT("Starting room has an exit"),false); Finish(); return; }
            Cell+=DemoEnvironment::NeighborOffsets()[Direction]; Previous=Direction^1;
            Waypoints.Add(FVector((Cell.X+.5)*Environment->ChunkSize,(Cell.Y+.5)*Environment->ChunkSize,0)-FVector(GetWorld()->OriginLocation));
        }
        Waypoint=0; Stage=3; Elapsed=0; return;
    }
    if (Stage==3 || Stage==5)
    {
        const FVector Delta=Waypoints[Waypoint]-Feet;
        if (Delta.Size2D()<55 && FMath::Abs(Delta.Z)<80)
        {
            Check(Stage==3?TEXT("Walked through a streamed graph doorway"):TEXT("Walked a descent waypoint"),true);
            ++Waypoint; Elapsed=0;
            if (Waypoint>=Waypoints.Num())
            {
                Character->GetCharacterMovement()->StopMovementImmediately();
                if (Stage==3) { Stage=4; return; }
                const FString Filename=Directory/TEXT("Descent.png");
                Captures.Add(Filename); FScreenshotRequest::RequestScreenshot(Filename,false,false,false,FIntRect(),true);
                Stage=6; return;
            }
        }
        if (Elapsed>12)
        {
            Check(FString::Printf(TEXT("Traversal stage %d waypoint %d reached target %s from %s (velocity %s, mode %d)"),
                Stage,Waypoint,*Waypoints[Waypoint].ToString(),*Feet.ToString(),*Character->GetVelocity().ToString(),
                int32(Character->GetCharacterMovement()->MovementMode)),false);
            Finish(); return;
        }
        Character->AddMovementInput((Waypoints[Waypoint]-Feet).GetSafeNormal2D());
        return;
    }
    if (Stage==4)
    {
        FIntVector Cell=FIntVector::ZeroValue;
        bool Found=false;
        for (int32 X=0; X<4 && !Found; ++X) for (int32 Y=0; Y<4; ++Y)
            if (DemoEnvironment::PlanCell(Environment->WorldSeed,FIntVector(X,Y,0)).bDescent) { Cell=FIntVector(X,Y,0); Found=true; break; }
        Check(TEXT("Live district contains a descent room"),Found);
        Place(Cell);
        const auto Layout=DemoEnvironment::Generate(Environment->WorldSeed,Cell,Environment->ChunkSize,Environment->LayerHeight);
        const FQuat Turn=Layout.DescentSign.GetRotation();
        const FVector CellOrigin=FVector(Cell.X*Environment->ChunkSize,Cell.Y*Environment->ChunkSize,0)-FVector(GetWorld()->OriginLocation);
        auto Point=[&](FVector P) { return CellOrigin+FVector(2000,2000,0)+Turn.RotateVector(P-FVector(2000,2000,0)); };
        Character->SetActorLocation(Point(FVector(280,650,Half+5)),false,nullptr,ETeleportType::TeleportPhysics);
        Character->GetController()->SetControlRotation(Turn.Rotator());
        Waypoints.Reset();
        const double Height=Environment->LayerHeight;
        for (const FVector P:{FVector(1600,650,-Height*.5),FVector(1725,650,-Height*.5),FVector(1725,1350,-Height*.5),
            FVector(1600,1350,-Height*.5),FVector(400,1350,-Height),FVector(280,1350,-Height)}) Waypoints.Add(Point(P));
        Waypoint=0; Stage=5; Elapsed=0; return;
    }
    if (Stage==6 && Elapsed>1) Finish();
}
