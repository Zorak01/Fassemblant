#include "demo/Character/DemoGrapplePhysics.h"

DemoGrapplePhysics::FState DemoGrapplePhysics::Step(FState State,FVector Anchor,FVector AnchorVelocity,
    FVector Input,FVector Gravity,double Dt,const FSettings& Settings)
{
    if (Dt <= 0) return State;
    const FVector Toward = (Anchor-State.Position).GetSafeNormal();
    const FVector RelativeVelocity = State.Velocity-AnchorVelocity;
    const double RadialSpeed = FVector::DotProduct(RelativeVelocity,Toward);
    // Feed gravity into the motor, so an upward grapple reaches the requested
    // reel speed instead of spending most of its force just resisting gravity.
    const double Pull = FMath::Clamp((Settings.ReelSpeed-RadialSpeed)*Settings.PullResponse
        -FVector::DotProduct(Gravity,Toward),0.,Settings.MaximumPullAcceleration);
    const FVector Steering = FVector::VectorPlaneProject(Input.GetClampedToMaxSize(1.),Toward)*Settings.SteeringAcceleration;
    const FVector PreviousVelocity = State.Velocity;
    State.Velocity = (State.Velocity+(Toward*Pull+Steering+Gravity)*Dt).GetClampedToMaxSize(Settings.MaximumSpeed);
    State.Position += (PreviousVelocity+State.Velocity)*(.5*Dt);
    const FVector Away = State.Position-Anchor;
    if (Away.SizeSquared() > FMath::Square(State.RopeLength))
    {
        const FVector Direction = Away.GetSafeNormal();
        State.Position = Anchor+Direction*State.RopeLength;
        // A rope removes outward motion only. Never erase the tangential swing
        // or push the player outward when they move closer than its length.
        const double Outward = FVector::DotProduct(State.Velocity-AnchorVelocity,Direction);
        if (Outward > 0) State.Velocity -= Direction*Outward;
    }
    // Ratchet in with actual progress, rather than teleporting the capsule along
    // a prescribed line or snapping it to a shrinking radius on attachment.
    State.RopeLength = FMath::Max(Settings.MinimumLength,FMath::Min(State.RopeLength,FVector::Distance(State.Position,Anchor)+Settings.Slack));
    return State;
}
