#include "demo/Character/DemoArmsAnimInstance.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/Weapons/DemoWeapon.h"
#include "demo/Weapons/DemoWeaponVisualData.h"
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
        CameraOrigin = ToComponent.TransformPosition(Character->Camera->GetComponentLocation());
        CameraForward = ToComponent.TransformVectorNoScale(Character->Camera->GetForwardVector()).GetSafeNormal();
        CameraRight = ToComponent.TransformVectorNoScale(Character->Camera->GetRightVector()).GetSafeNormal();
        CameraUp = ToComponent.TransformVectorNoScale(Character->Camera->GetUpVector()).GetSafeNormal();
        Right = Character->GetWeapon()->GetHandTarget(false) * ToComponent;
        Left = Character->GetWeapon()->GetHandTarget(true) * ToComponent;
        WeaponRotation = ToComponent.TransformRotation(Character->GetWeapon()->GetActorQuat())
            * ToComponent.TransformRotation(Character->Camera->GetComponentQuat()).Inverse();
        Reload = Character->GetWeapon()->GetReloadProgress();
        bReloading = Character->GetWeapon()->IsReloading() && Character->ArmsReloadPose;
        if (bReloading && Character->GetWeapon()->VisualData)
            Left = Character->GetWeapon()->VisualData->LeftHand * Character->GetWeapon()->GetActorTransform() * ToComponent;
        Sequence = Character->ArmsIdlePose;
        Idle = Character->ArmsIdlePose;
        SampleTime = Sequence ? FMath::Fmod(Character->GetWorld()->GetTimeSeconds(), Sequence->GetPlayLength()) : 0.f;
        if (bReloading)
        {
            Sequence = Character->ArmsReloadPose;
            SampleTime = Reload * Sequence->GetPlayLength();
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
            FQuat AlignRotation = WeaponRotation;
            if (bReloading)
            {
                const auto Middle = Index(TEXT("middle_01_r"));
                const auto Finger = Index(TEXT("index_01_r"));
                const auto Pinky = Index(TEXT("pinky_01_r"));
                if (Middle.IsValid() && Finger.IsValid() && Pinky.IsValid())
                {
                    const FVector Forward = Global[Middle.GetInt()].GetLocation() - Hand;
                    const FVector Across = Global[Finger.GetInt()].GetLocation() - Global[Pinky.GetInt()].GetLocation();
                    AlignRotation = Right.GetRotation() * FRotationMatrix::MakeFromXY(Forward, Across).ToQuat().Inverse();
                }
            }
            const FTransform Alignment(AlignRotation, Right.GetLocation() - AlignRotation.RotateVector(Hand));
            for (FTransform& Bone : Global) Bone = Bone * Alignment;
        }
        struct FArm { FCompactPoseBoneIndex Upper, Lower, Hand; FTransform U, L, H; };
        TArray<FArm, TInlineAllocator<2>> Arms;
        for (bool bLeft : {true})
        {
            // Let the authored reload articulate the wrist, elbow and fingers.
            // IK only eases the support hand into/out of the clip at its ends.
            const float IKWeight = bReloading ? 1.f - FMath::SmoothStep(0.f, .10f, Reload)
                * (1.f - FMath::SmoothStep(.90f, 1.f, Reload)) : 1.f;
            FArm Arm;
            Arm.Upper = Index(bLeft ? TEXT("upperarm_l") : TEXT("upperarm_r"));
            Arm.Lower = Index(bLeft ? TEXT("lowerarm_l") : TEXT("lowerarm_r"));
            Arm.Hand = Index(bLeft ? TEXT("hand_l") : TEXT("hand_r"));
            if (!Arm.Upper.IsValid() || !Arm.Lower.IsValid() || !Arm.Hand.IsValid()) continue;
            Arm.U = Global[Arm.Upper.GetInt()]; Arm.L = Global[Arm.Lower.GetInt()]; Arm.H = Global[Arm.Hand.GetInt()];
            if (IKWeight <= SMALL_NUMBER)
            {
                // The third-person clip reaches behind the eye to fetch ammo.
                // Shift the entire source limb forward as one rigid branch;
                // retain the authored forearm/hand articulation instead of bending a wrist
                // to keep its skin on the visible side of the near plane.
                float Depth = FVector::DotProduct(Arm.H.GetLocation() - CameraOrigin, CameraForward);
                for (FName Name : {FName(TEXT("middle_01_l")), FName(TEXT("index_03_l")), FName(TEXT("thumb_03_l"))})
                {
                    const auto Finger = Index(Name);
                    if (Finger.IsValid()) Depth = FMath::Min(Depth,
                        static_cast<float>(FVector::DotProduct(Global[Finger.GetInt()].GetLocation() - CameraOrigin, CameraForward)));
                }
                const float Error = 12.f - Depth;
                const FVector Offset = CameraForward * (.5f * (Error + FMath::Sqrt(Error * Error + 4.f)));
                Arm.U.AddToTranslation(Offset); Arm.L.AddToTranslation(Offset); Arm.H.AddToTranslation(Offset);
                // Keep the shoulder outside the first-person framing, retaining
                // a continuous upper arm rather than exposing a cut-off mesh.
                // The forearm and hand keep their source transforms/angles.
                const FVector Upper = Arm.L.GetLocation() - Arm.U.GetLocation();
                const FVector ShoulderHint = CameraOrigin - CameraForward * 18.f - CameraRight * 24.f - CameraUp * 45.f;
                const FVector UpperDirection = (Arm.L.GetLocation() - ShoulderHint).GetSafeNormal();
                Arm.U.SetLocation(Arm.L.GetLocation() - UpperDirection * Upper.Size());
                Arm.U.SetRotation((FQuat::FindBetweenNormals(Upper.GetSafeNormal(), UpperDirection) * Arm.U.GetRotation()).GetNormalized());
                Arms.Add(Arm);
                continue;
            }
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
            if (IKWeight < 1.f)
            {
                FTransform U, L, H;
                U.Blend(Global[Arm.Upper.GetInt()], Arm.U, IKWeight);
                L.Blend(Global[Arm.Lower.GetInt()], Arm.L, IKWeight);
                H.Blend(Global[Arm.Hand.GetInt()], Arm.H, IKWeight);
                Arm.U = U; Arm.L = L; Arm.H = H;
            }
            // The camera-proximity mask is gone in all poses, not only reload.
            // Frame the continuous upper arm outside the eye at rest/ADS too.
            const FVector Upper = Arm.L.GetLocation() - Arm.U.GetLocation();
            const FVector ShoulderHint = CameraOrigin - CameraForward * 18.f - CameraRight * 24.f - CameraUp * 45.f;
            const FVector UpperDirection = (Arm.L.GetLocation() - ShoulderHint).GetSafeNormal();
            Arm.U.SetLocation(Arm.L.GetLocation() - UpperDirection * Upper.Size());
            Arm.U.SetRotation((FQuat::FindBetweenNormals(Upper.GetSafeNormal(), UpperDirection) * Arm.U.GetRotation()).GetNormalized());
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
    bool bReloading = false;
    FTransform Right, Left;
    FVector CameraOrigin, CameraForward, CameraRight, CameraUp;
    FQuat WeaponRotation = FQuat::Identity;
    float Reload = 0;
    UAnimSequence* Sequence = nullptr;
    UAnimSequence* Idle = nullptr;
    double SampleTime = 0;
};
FAnimInstanceProxy* UDemoArmsAnimInstance::CreateAnimInstanceProxy() { return new FDemoArmsProxy(this); }
void UDemoArmsAnimInstance::DestroyAnimInstanceProxy(FAnimInstanceProxy* Proxy) { delete Proxy; }
