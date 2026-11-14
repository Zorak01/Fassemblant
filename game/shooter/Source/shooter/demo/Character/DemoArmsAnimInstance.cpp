#include "demo/Character/DemoArmsAnimInstance.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/Weapons/DemoWeapon.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraComponent.h"
#include "TwoBoneIK.h"

class FDemoArmsProxy : public FAnimInstanceProxy
{
public:
    explicit FDemoArmsProxy(UAnimInstance* Instance) : FAnimInstanceProxy(Instance) {}
    virtual void PreUpdate(UAnimInstance* Instance, float Dt) override
    {
        FAnimInstanceProxy::PreUpdate(Instance, Dt);
        auto* Character = Cast<ADemoCharacter>(Instance->TryGetPawnOwner());
        auto* Mesh = Instance->GetSkelMeshComponent();
        bReady = Character && Mesh && Character->GetWeapon();
        if (!bReady) return;
        const FTransform ToComponent = Mesh->GetComponentTransform().Inverse();
        Right = Character->GetWeapon()->GetHandTarget(false) * ToComponent;
        Left = Character->GetWeapon()->GetHandTarget(true) * ToComponent;
        RightPole = ToComponent.TransformPosition(Character->Camera->GetComponentTransform().TransformPosition(FVector(-10, 38, -42)));
        LeftPole = ToComponent.TransformPosition(Character->Camera->GetComponentTransform().TransformPosition(FVector(-8, -38, -38)));
        Reload = Character->GetWeapon()->GetReloadProgress();
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Output.ResetToRefPose();
        if (!bReady) return true;
        const FBoneContainer& Bones = Output.Pose.GetBoneContainer();
        const FReferenceSkeleton& Ref = Bones.GetReferenceSkeleton();
        TArray<FTransform, TInlineAllocator<256>> Global;
        Global.SetNum(Output.Pose.GetNumBones());
        for (FCompactPoseBoneIndex I : Output.Pose.ForEachBoneIndex())
        {
            const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(I);
            Global[I.GetInt()] = Parent.IsValid() ? Output.Pose[I] * Global[Parent.GetInt()] : Output.Pose[I];
        }
        auto Index = [&Bones, &Ref](FName Name) { return Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(Ref.FindBoneIndex(Name))); };
        struct FArm { FCompactPoseBoneIndex Upper, Lower, Hand; FTransform U, L, H; };
        TArray<FArm, TInlineAllocator<2>> Arms;
        TArray<FVector, TInlineAllocator<256>> CurlAxes;
        TArray<float, TInlineAllocator<256>> CurlAngles;
        CurlAxes.Init(FVector::ZeroVector, Global.Num());
        CurlAngles.Init(0.f, Global.Num());
        for (bool bLeft : {false, true})
        {
            FArm Arm;
            Arm.Upper = Index(bLeft ? TEXT("upperarm_l") : TEXT("upperarm_r"));
            Arm.Lower = Index(bLeft ? TEXT("lowerarm_l") : TEXT("lowerarm_r"));
            Arm.Hand = Index(bLeft ? TEXT("hand_l") : TEXT("hand_r"));
            if (!Arm.Upper.IsValid() || !Arm.Lower.IsValid() || !Arm.Hand.IsValid()) continue;
            Arm.U = Global[Arm.Upper.GetInt()]; Arm.L = Global[Arm.Lower.GetInt()]; Arm.H = Global[Arm.Hand.GetInt()];
            const FTransform& Target = bLeft ? Left : Right;
            const FCompactPoseBoneIndex Middle = Index(bLeft ? TEXT("middle_01_l") : TEXT("middle_01_r"));
            const FCompactPoseBoneIndex FingerIndex = Index(bLeft ? TEXT("index_01_l") : TEXT("index_01_r"));
            const FCompactPoseBoneIndex Pinky = Index(bLeft ? TEXT("pinky_01_l") : TEXT("pinky_01_r"));
            FQuat BoneRotation = Target.GetRotation();
            if (Middle.IsValid() && FingerIndex.IsValid() && Pinky.IsValid())
            {
                const FVector Forward = (Global[Middle.GetInt()].GetLocation() - Arm.H.GetLocation()).GetSafeNormal();
                const FVector Across = (Global[FingerIndex.GetInt()].GetLocation() - Global[Pinky.GetInt()].GetLocation()).GetSafeNormal();
                const FQuat PalmBasis = FRotationMatrix::MakeFromXY(Forward, Across).ToQuat();
                // Data targets describe a physical palm, not mirrored skeleton axes.
                // This calibration handles Manny's different left/right bone bases.
                BoneRotation = Target.GetRotation() * PalmBasis.Inverse() * Arm.H.GetRotation();
                for (FCompactPoseBoneIndex I : Output.Pose.ForEachBoneIndex())
                {
                    const FString Name = Ref.GetBoneName(Bones.MakeMeshPoseIndex(I).GetInt()).ToString();
                    if (!Name.EndsWith(bLeft ? TEXT("_l") : TEXT("_r")) || Name.Contains(TEXT("meta"))) continue;
                    const bool bFinger = Name.StartsWith(TEXT("index_")) || Name.StartsWith(TEXT("middle_")) || Name.StartsWith(TEXT("ring_")) || Name.StartsWith(TEXT("pinky_")) || Name.StartsWith(TEXT("thumb_"));
                    if (!bFinger) continue;
                    CurlAxes[I.GetInt()] = Global[I.GetInt()].GetRotation().UnrotateVector(Across);
                    float Curl = Name.StartsWith(TEXT("thumb")) ? 22.f : 48.f;
                    if (Name.StartsWith(TEXT("index")) && !bLeft) Curl = 25.f;
                    if (bLeft && Reload > 0.08f && Reload < 0.78f) Curl = 32.f;
                    CurlAngles[I.GetInt()] = -FMath::DegreesToRadians(Curl);
                }
            }
            AnimationCore::SolveTwoBoneIK(Arm.U, Arm.L, Arm.H, bLeft ? LeftPole : RightPole, Target.GetLocation(), false, 1.0, 1.0);
            Arm.H.SetRotation(BoneRotation);
            Arms.Add(Arm);
        }
        // Rebuild in hierarchy order. Twist bones and finger descendants inherit
        // the solved limb transforms; the source skeleton is never modified.
        for (FCompactPoseBoneIndex I : Output.Pose.ForEachBoneIndex())
        {
            if (!CurlAxes[I.GetInt()].IsNearlyZero()) Output.Pose[I].SetRotation(Output.Pose[I].GetRotation() * FQuat(CurlAxes[I.GetInt()], CurlAngles[I.GetInt()]));
            const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(I);
            FTransform Component = Parent.IsValid() ? Output.Pose[I] * Global[Parent.GetInt()] : Output.Pose[I];
            for (const FArm& Arm : Arms)
            {
                if (I == Arm.Upper) Component = Arm.U;
                else if (I == Arm.Lower) Component = Arm.L;
                else if (I == Arm.Hand) Component = Arm.H;
            }
            Global[I.GetInt()] = Component;
            Output.Pose[I] = Parent.IsValid() ? Component.GetRelativeTransform(Global[Parent.GetInt()]) : Component;
            Output.Pose[I].NormalizeRotation();
        }
        return true;
    }
private:
    bool bReady = false;
    FTransform Right, Left;
    FVector RightPole, LeftPole;
    float Reload = 0;
};
FAnimInstanceProxy* UDemoArmsAnimInstance::CreateAnimInstanceProxy() { return new FDemoArmsProxy(this); }
void UDemoArmsAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
