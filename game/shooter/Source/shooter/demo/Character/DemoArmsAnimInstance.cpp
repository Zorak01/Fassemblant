#include "demo/Character/DemoArmsAnimInstance.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/Weapons/DemoWeapon.h"
#include "Animation/AnimInstanceProxy.h"
#include "Animation/AnimNodeBase.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimationPoseData.h"
#include "AnimationRuntime.h"
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
        WeaponRotation = ToComponent.TransformRotation(Character->GetWeapon()->GetActorQuat())
            * ToComponent.TransformRotation(Character->Camera->GetComponentQuat()).Inverse();
        Reload = Character->GetWeapon()->GetReloadProgress();
        Sequence = Character->ArmsIdlePose;
        Idle = Character->ArmsIdlePose;
        SampleTime = Sequence ? FMath::Fmod(Character->GetWorld()->GetTimeSeconds(), Sequence->GetPlayLength()) : 0.f;
        if (Character->GetWeapon()->IsReloading())
        {
            // Our magazine/handle path differs from the template rifle reload.
            // Use the holding pose as the IK source so its open-hand gestures
            // and shoulder travel cannot fight the actual mechanical targets.
            Sequence = Idle;
            SampleTime = 0.0;
        }
        else if (Character->ArmsFirePose && Character->GetWeapon()->GetShotAge() < Character->ArmsFirePose->GetPlayLength())
        {
            Sequence = Character->ArmsFirePose;
            SampleTime = Character->GetWeapon()->GetShotAge();
        }
    }
    virtual bool Evaluate(FPoseContext& Output) override
    {
        Output.ResetToRefPose();
        if (!bReady) return true;
        if (Sequence)
        {
            FAnimationPoseData PoseData(Output);
            if (Sequence->IsValidAdditive() && Idle)
            {
                Idle->GetAnimationPose(PoseData, FAnimExtractContext(0.0, false));
                FPoseContext Additive(Output);
                Additive.ResetToAdditiveIdentity();
                FAnimationPoseData AdditiveData(Additive);
                Sequence->GetAnimationPose(AdditiveData, FAnimExtractContext(SampleTime, false));
                FAnimationRuntime::AccumulateAdditivePose(PoseData, AdditiveData, 1.f, Sequence->GetAdditiveAnimType());
                Output.Pose.NormalizeRotations();
            }
            else Sequence->GetAnimationPose(PoseData, FAnimExtractContext(SampleTime, false));
        }
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
        // Anchor the authored holding pose as a rigid viewmodel to the firing
        // hand. Preserve the entire dominant arm, wrist and finger articulation;
        // forcing both elbows through camera-space poles distorts Manny's skin.
        const auto Grip = Index(TEXT("hand_r"));
        if (Grip.IsValid())
        {
            const FVector Hand = Global[Grip.GetInt()].GetLocation();
            const FTransform Alignment(WeaponRotation, Right.GetLocation() - WeaponRotation.RotateVector(Hand));
            for (FTransform& Bone : Global) Bone = Bone * Alignment;
        }
        struct FArm { FCompactPoseBoneIndex Upper, Lower, Hand; FTransform U, L, H; };
        TArray<FArm, TInlineAllocator<2>> Arms;
        for (bool bLeft : {true})
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
            }
            const FVector AuthoredElbow = Arm.L.GetLocation();
            AnimationCore::SolveTwoBoneIK(Arm.U, Arm.L, Arm.H, AuthoredElbow, Target.GetLocation(), false, 1.0, 1.0);
            Arm.H.SetRotation(BoneRotation);
            Arms.Add(Arm);
        }
        // Rebuild in hierarchy order. Twist bones and finger descendants inherit
        // the solved limb transforms; the source skeleton is never modified.
        for (FCompactPoseBoneIndex I : Output.Pose.ForEachBoneIndex())
        {
            const FCompactPoseBoneIndex Parent = Bones.GetParentBoneIndex(I);
            FTransform Component = Parent.IsValid() ? Output.Pose[I] * Global[Parent.GetInt()] : Global[I.GetInt()];
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
    FQuat WeaponRotation = FQuat::Identity;
    float Reload = 0;
    UAnimSequence* Sequence = nullptr;
    UAnimSequence* Idle = nullptr;
    double SampleTime = 0;
};
FAnimInstanceProxy* UDemoArmsAnimInstance::CreateAnimInstanceProxy() { return new FDemoArmsProxy(this); }
void UDemoArmsAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
