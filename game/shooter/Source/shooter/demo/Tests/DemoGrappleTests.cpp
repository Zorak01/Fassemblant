#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "demo/Character/DemoGrapplePhysics.h"
#include "demo/Character/DemoMovementComponent.h"
#include "demo/Character/DemoCharacter.h"
#include "Components/BoxComponent.h"
#include "Engine/World.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoGrappleRopeTest,"Demo.Gameplay.Grapple.UnilateralRope",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoGrappleRopeTest::RunTest(const FString& Parameters)
{
    using namespace DemoGrapplePhysics;
    FSettings Settings;
    Settings.ReelSpeed = Settings.PullResponse = Settings.SteeringAcceleration = 0;
    const auto Swing = Step({FVector(500,0,0),FVector(250,700,0),500},FVector::ZeroVector,FVector::ZeroVector,
        FVector::ZeroVector,FVector::ZeroVector,.01,Settings);
    TestTrue(TEXT("Taut rope prevents extending beyond its anchor radius"),Swing.Position.Size() <= 500.00001);
    TestTrue(TEXT("Constraint preserves lateral swing instead of pulling in a straight line"),Swing.Velocity.Y > 690);
    TestTrue(TEXT("Outward radial motion is removed"),FVector::DotProduct(Swing.Velocity,Swing.Position.GetSafeNormal()) < .001);
    const auto Slack = Step({FVector(500,0,0),FVector(-200,0,0),1000},FVector::ZeroVector,FVector::ZeroVector,
        FVector::ZeroVector,FVector::ZeroVector,.01,Settings);
    TestTrue(TEXT("Slack rope never pushes a closer player away"),Slack.Position.X < 500 && Slack.Velocity.Equals(FVector(-200,0,0),.0001));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoGrappleSteeringTest,"Demo.Gameplay.Grapple.ReelAndSteer",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoGrappleSteeringTest::RunTest(const FString& Parameters)
{
    using namespace DemoGrapplePhysics;
    FSettings Settings;
    const FVector Anchor(2000,0,0);
    FState Straight{FVector::ZeroVector,FVector(0,500,0),2000};
    auto Steering = Straight;
    for (int32 I=0; I<120; ++I)
    {
        Straight = Step(Straight,Anchor,FVector::ZeroVector,FVector::ZeroVector,FVector::ZeroVector,1./120,Settings);
        Steering = Step(Steering,Anchor,FVector::ZeroVector,FVector(0,1,0),FVector::ZeroVector,1./120,Settings);
    }
    TestTrue(TEXT("Winch reels the player closer to the anchor"),FVector::Distance(Straight.Position,Anchor) < 1500);
    TestTrue(TEXT("Strafing changes the swing trajectory"),Steering.Position.Y > Straight.Position.Y+100);
    TestTrue(TEXT("Steering remains within its speed envelope"),Steering.Velocity.Size() <= Settings.MaximumSpeed+.01);
    TestTrue(TEXT("Rope length reels down with actual progress"),Straight.RopeLength < 1900);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoGrappleIntegrationTest,"Demo.Gameplay.Grapple.IntegrationStability",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoGrappleIntegrationTest::RunTest(const FString& Parameters)
{
    using namespace DemoGrapplePhysics;
    FSettings Settings;
    const FState Initial{FVector::ZeroVector,FVector(0,500,100),2200};
    auto Integrate = [&](int32 Rate,FVector Offset)
    {
        FState State = Initial; State.Position += Offset;
        for (int32 I=0; I<Rate; ++I)
            State = Step(State,FVector(2000,0,700)+Offset,FVector::ZeroVector,FVector(0,.3,0),FVector(0,0,-1150),1./Rate,Settings);
        return State;
    };
    const auto At60 = Integrate(60,FVector::ZeroVector), At120 = Integrate(120,FVector::ZeroVector);
    TestTrue(TEXT("60 and 120 Hz trajectories remain within a capsule radius"),FVector::Distance(At60.Position,At120.Position) < 34);
    const FVector Rebase(-200000,400000,0);
    const auto Shifted = Integrate(120,Rebase);
    TestTrue(TEXT("Origin shifting does not change the rope solution"),(Shifted.Position-Rebase).Equals(At120.Position,.001));
    TestTrue(TEXT("Origin shifting does not inject velocity"),Shifted.Velocity.Equals(At120.Velocity,.001));
    const auto Paused = Step(Initial,FVector(2000,0,700),FVector::ZeroVector,FVector::ZeroVector,FVector::ZeroVector,0,Settings);
    TestTrue(TEXT("A paused frame does not move the player"),Paused.Position == Initial.Position && Paused.Velocity == Initial.Velocity);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoGrappleLifecycleTest,"Demo.Gameplay.Grapple.MomentumRelease",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoGrappleLifecycleTest::RunTest(const FString& Parameters)
{
    const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&Settings);
    auto* Character = World->SpawnActor<ADemoCharacter>();
    auto* Movement = CastChecked<UDemoMovementComponent>(Character->GetCharacterMovement());
    auto* Anchor = World->SpawnActor<AActor>();
    auto* Surface = NewObject<UBoxComponent>(Anchor);
    Anchor->SetRootComponent(Surface); Surface->SetBoxExtent(FVector(100));
    Surface->SetWorldLocation(FVector(1500,0,700)); Surface->RegisterComponent();
    Movement->SetMovementMode(MOVE_Falling);
    Movement->Velocity = FVector(200,800,120);
    const FVector BeforeAttach=Movement->Velocity;
    const FVector Toward=(Surface->GetComponentLocation()-Character->GetActorLocation()).GetSafeNormal();
    TestTrue(TEXT("Valid surface starts grapple movement"),Movement->StartGrapple(Surface,FVector::ZeroVector));
    TestTrue(TEXT("Grapple keeps free-air stance"),Movement->IsGrappling() && Movement->IsFalling());
    TestTrue(TEXT("Attachment engages a decisive inward pull"),FVector::DotProduct(Movement->Velocity,Toward)>=Movement->GrappleAttachPull-.001);
    TestTrue(TEXT("Attachment preserves existing swing momentum"),FVector::VectorPlaneProject(Movement->Velocity,Toward).Equals(FVector::VectorPlaneProject(BeforeAttach,Toward),.001));
    const FVector BeforeHop=Movement->Velocity;
    TestTrue(TEXT("An early jump adds latch-hop assistance"),Movement->HandleGrappleJump() && Movement->Velocity.Z > 400);
    TestTrue(TEXT("Latch hop preserves existing tangential momentum"),Movement->Velocity.X == BeforeHop.X && Movement->Velocity.Y == BeforeHop.Y);
    const FVector BeforeRelease = Movement->Velocity;
    TestTrue(TEXT("A subsequent jump releases the grapple"),Movement->HandleGrappleJump() && !Movement->IsGrappling());
    TestTrue(TEXT("Release preserves full velocity"),Movement->Velocity == BeforeRelease);
    TestTrue(TEXT("Release preserves the airborne speed ceiling"),Movement->GetMaxSpeed() >= BeforeRelease.Size2D());
    Movement->SetMovementMode(MOVE_Walking); Movement->Velocity = FVector(1900,0,0);
    TestTrue(TEXT("Fast grapple momentum can enter a slide"),Movement->TryStartSlide());
    TestTrue(TEXT("Landing slide does not discard grapple speed"),Movement->Velocity.Size2D() >= 1900 && Movement->GetMaxSpeed() >= 1900);
    World->DestroyWorld(false);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoGrappleStrongPullTest,"Demo.Gameplay.Grapple.UpwardMotor",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDemoGrappleStrongPullTest::RunTest(const FString& Parameters)
{
    using namespace DemoGrapplePhysics;
    FSettings Settings;
    FState State{FVector::ZeroVector,FVector::ZeroVector,3000};
    for (int32 I=0; I<30; ++I) State=Step(State,FVector(0,0,3000),FVector::ZeroVector,FVector::ZeroVector,FVector(0,0,-1150),1./120,Settings);
    TestTrue(TEXT("The winch accelerates upward strongly within a quarter second"),State.Velocity.Z>1500);
    TestTrue(TEXT("Reeling produces real displacement against gravity"),State.Position.Z>220);
    return true;
}
#endif
