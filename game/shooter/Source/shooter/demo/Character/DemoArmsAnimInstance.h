#pragma once
#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "DemoArmsAnimInstance.generated.h"

/** Native mannequin animation layer. Weapon targets are sampled on the game
 * thread; pose evaluation and two-bone IK can run on the animation worker. */
UCLASS(Transient, Blueprintable)
class SHOOTER_API UDemoArmsAnimInstance : public UAnimInstance
{
    GENERATED_BODY()
protected:
    virtual FAnimInstanceProxy* CreateAnimInstanceProxy() override;
    virtual void DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) override;
};
