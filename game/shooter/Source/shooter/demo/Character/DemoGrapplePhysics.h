#pragma once
#include "CoreMinimal.h"

/** Collision-independent rope integration; collision sweeps remain in CharacterMovement. */
namespace DemoGrapplePhysics
{
    struct FSettings
    {
        double ReelSpeed = 1900;
        double PullResponse = 10;
        double MaximumPullAcceleration = 12000;
        double SteeringAcceleration = 2400;
        double MaximumSpeed = 3200;
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
