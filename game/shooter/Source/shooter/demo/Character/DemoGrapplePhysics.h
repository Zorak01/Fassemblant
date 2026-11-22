#pragma once
#include "CoreMinimal.h"

/** Collision-independent rope integration; collision sweeps remain in CharacterMovement. */
namespace DemoGrapplePhysics
{
    struct FSettings
    {
        double ReelSpeed = 1000;
        double PullResponse = 4.5;
        double MaximumPullAcceleration = 3800;
        double SteeringAcceleration = 1800;
        double MaximumSpeed = 2200;
        double MinimumLength = 130;
        double Slack = 12;
    };
    struct FState
    {
        FVector Position;
        FVector Velocity;
        double RopeLength;
    };
    FState Step(FState State,FVector Anchor,FVector AnchorVelocity,FVector Input,FVector Gravity,double Dt,const FSettings& Settings);
}
