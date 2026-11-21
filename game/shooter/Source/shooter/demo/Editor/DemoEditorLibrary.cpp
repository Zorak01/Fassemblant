#include "demo/Editor/DemoEditorLibrary.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMesh.h"
#if WITH_EDITOR
#include "Animation/Skeleton.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "HAL/FileManager.h"
#include "MeshDescription.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "SkeletalMeshAttributes.h"
#include "StaticMeshAttributes.h"
#include "UObject/SavePackage.h"
#include "UObject/Package.h"

namespace DemoAssets
{
    bool Save(UObject* Object)
    {
        UPackage* Package = Object->GetOutermost();
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        Args.SaveFlags = SAVE_NoError;
        return UPackage::SavePackage(Package, Object, *Filename, Args);
    }
}
#endif

void UDemoEditorLibrary::MountTemplateResources()
{
#if WITH_EDITOR
    const FString Templates = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("../Templates/"));
    FPackageName::RegisterMountPoint(TEXT("/Game/Characters/"), Templates / TEXT("TemplateResources/High/Characters/Content/"));
    FPackageName::RegisterMountPoint(TEXT("/Game/Weapons/"), Templates / TEXT("TemplateResources/Standard/Weapons/Content/"));
#endif
}

static USkeletalMesh* BuildMannequinArms(bool bTemplate, bool bRebuild = false)
{
#if WITH_EDITOR
    const TCHAR* Path = bTemplate ? TEXT("/Game/demo/Character/SK_ViewmodelArms") : TEXT("/Game/demo/Character/Mannequin/SK_DemoArms");
    auto* Existing = LoadObject<USkeletalMesh>(nullptr, *(FString(Path) + TEXT(".") + FPackageName::GetShortName(Path)), nullptr, LOAD_NoWarn);
    if (Existing && !bRebuild) return Existing;
    FPackageName::RegisterMountPoint(TEXT("/MoverTests/"), FPaths::EnginePluginsDir() / TEXT("Experimental/MoverTests/Content/"));
    FPackageName::RegisterMountPoint(TEXT("/MoverExamples/"), FPaths::EnginePluginsDir() / TEXT("Experimental/MoverExamples/Content/"));
    auto* Source = LoadObject<USkeletalMesh>(nullptr, bTemplate ? TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple") : TEXT("/MoverTests/Characters/Mannequins/Meshes/SKM_Manny_Simple.SKM_Manny_Simple"));
    if (!Source || !Source->GetSkeleton()) { UE_LOG(LogTemp, Error, TEXT("Epic Manny source mesh/skeleton is unavailable.")); return nullptr; }
    auto* Mesh = Existing ? Existing : DuplicateObject<USkeletalMesh>(Source, CreatePackage(Path), *FPackageName::GetShortName(Path));
    Mesh->SetFlags(RF_Public | RF_Standalone);
    auto* Skeleton = bTemplate ? Source->GetSkeleton() : DuplicateObject<USkeleton>(Source->GetSkeleton(), CreatePackage(TEXT("/Game/demo/Character/Mannequin/SK_DemoSkeleton")), TEXT("SK_DemoSkeleton"));
    if (!bTemplate) Skeleton->SetFlags(RF_Public | RF_Standalone);
    Mesh->SetSkeleton(Skeleton);
    Mesh->SetPhysicsAsset(nullptr);
    Mesh->SetPostProcessAnimBlueprint(nullptr);
    if (!bTemplate) Skeleton->SetPreviewMesh(Mesh);
    auto* Surface = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/demo/Materials/M_Weapon.M_Weapon"));
    if (!bTemplate) for (auto& Slot : Mesh->GetMaterials()) Slot.MaterialInterface = Surface;
    const FReferenceSkeleton& Ref = Mesh->GetRefSkeleton();
    TArray<bool> ArmBones;
    ArmBones.SetNumZeroed(Ref.GetNum());
    for (int32 I = 0; I < Ref.GetNum(); ++I)
    {
        const FString Name = Ref.GetBoneName(I).ToString();
        const int32 Parent = Ref.GetParentIndex(I);
        const bool bLimbRoot = Name == TEXT("upperarm_l") || Name == TEXT("upperarm_r");
        ArmBones[I] = bLimbRoot || (Parent >= 0 && ArmBones[Parent]);
    }
    // Remove body/head/legs from every source LOD while retaining the original
    // skeleton, skinning weights, UVs, normals and hand/finger hierarchy.
    int32 Kept = 0;
    for (int32 LOD = 0; LOD < Mesh->GetLODNum(); ++LOD)
    {
        const FMeshDescription* Description = Source->GetMeshDescription(LOD);
        if (!Description) continue;
        FSkeletalMeshConstAttributes Attributes(*Description);
        auto Weights = Attributes.GetVertexSkinWeights();
        TSet<FVertexID> Keep;
        for (FVertexID Vertex : Description->Vertices().GetElementIDs())
        {
            float Influence = 0;
            for (const auto Weight : Weights.Get(Vertex))
                if (ArmBones.IsValidIndex(Weight.GetBoneIndex()) && ArmBones[Weight.GetBoneIndex()]) Influence += Weight.GetWeight();
            if (Influence > 0.45f) Keep.Add(Vertex);
        }
        FMeshDescription Filtered;
        FSkeletalMeshAttributes Dest(Filtered);
        Dest.Register();
        Dest.GetVertexInstanceUVs().SetNumChannels(Attributes.GetVertexInstanceUVs().GetNumChannels());
        for (int32 I = 0; I < Ref.GetNum(); ++I)
        {
            const FBoneID Bone = Dest.CreateBone();
            Dest.GetBoneNames()[Bone] = Ref.GetBoneName(I);
            Dest.GetBoneParentIndices()[Bone] = Ref.GetParentIndex(I);
            Dest.GetBonePoses()[Bone] = Ref.GetRefBonePose()[I];
        }
        TMap<FPolygonGroupID, FPolygonGroupID> Groups;
        for (FPolygonGroupID Group : Description->PolygonGroups().GetElementIDs())
        {
            const FPolygonGroupID NewGroup = Filtered.CreatePolygonGroup();
            Groups.Add(Group, NewGroup);
            Dest.GetPolygonGroupMaterialSlotNames()[NewGroup] = Attributes.GetPolygonGroupMaterialSlotNames()[Group];
        }
        TMap<FVertexID, FVertexID> Vertices;
        for (FTriangleID Triangle : Description->Triangles().GetElementIDs())
        {
            bool bArm = true;
            for (FVertexID Vertex : Description->GetTriangleVertices(Triangle)) bArm &= Keep.Contains(Vertex);
            if (!bArm) continue;
            if (LOD == 0) ++Kept;
            TArray<FVertexInstanceID> Corners;
            for (FVertexInstanceID Corner : Description->GetTriangleVertexInstances(Triangle))
            {
                const FVertexID Vertex = Description->GetVertexInstanceVertex(Corner);
                FVertexID* Found = Vertices.Find(Vertex);
                FVertexID NewVertex;
                if (Found) NewVertex = *Found;
                else
                {
                    NewVertex = Filtered.CreateVertex();
                    Vertices.Add(Vertex, NewVertex);
                    Dest.GetVertexPositions()[NewVertex] = Attributes.GetVertexPositions()[Vertex];
                    TArray<UE::AnimationCore::FBoneWeight> BoneWeights;
                    for (const auto Weight : Weights.Get(Vertex)) BoneWeights.Add(Weight);
                    Dest.GetVertexSkinWeights().Set(NewVertex, BoneWeights);
                }
                const FVertexInstanceID NewCorner = Filtered.CreateVertexInstance(NewVertex);
                Dest.GetVertexInstanceNormals()[NewCorner] = Attributes.GetVertexInstanceNormals()[Corner];
                Dest.GetVertexInstanceTangents()[NewCorner] = Attributes.GetVertexInstanceTangents()[Corner];
                Dest.GetVertexInstanceBinormalSigns()[NewCorner] = Attributes.GetVertexInstanceBinormalSigns()[Corner];
                Dest.GetVertexInstanceColors()[NewCorner] = Attributes.GetVertexInstanceColors()[Corner];
                for (int32 Channel = 0; Channel < Attributes.GetVertexInstanceUVs().GetNumChannels(); ++Channel)
                    Dest.GetVertexInstanceUVs().Set(NewCorner, Channel, Attributes.GetVertexInstanceUVs().Get(Corner, Channel));
                Corners.Add(NewCorner);
            }
            Filtered.CreateTriangle(Groups[Description->GetTrianglePolygonGroup(Triangle)], Corners);
        }
        Mesh->CreateMeshDescription(LOD, MoveTemp(Filtered));
        Mesh->CommitMeshDescription(LOD);
    }
    if (Kept < 100) { UE_LOG(LogTemp, Error, TEXT("Arms extraction found too little geometry (%d triangles)."), Kept); return nullptr; }
    Mesh->Build();
    if (!bTemplate) FAssetRegistryModule::AssetCreated(Skeleton);
    if (!Existing) FAssetRegistryModule::AssetCreated(Mesh);
    if (!bTemplate) DemoAssets::Save(Skeleton);
    DemoAssets::Save(Mesh);
    TArray<FTransform> Global;
    for (int32 I = 0; I < Ref.GetNum(); ++I)
    {
        const int32 Parent = Ref.GetParentIndex(I);
        Global.Add(Parent < 0 ? Ref.GetRefBonePose()[I] : Ref.GetRefBonePose()[I] * Global[Parent]);
        if (Ref.GetBoneName(I).ToString().Contains(TEXT("arm_")) || Ref.GetBoneName(I).ToString().StartsWith(TEXT("hand_")))
            UE_LOG(LogTemp, Display, TEXT("MANNY_BONE %s %s %s"), *Ref.GetBoneName(I).ToString(), *Global[I].GetLocation().ToString(), *Global[I].Rotator().ToString());
    }
    UE_LOG(LogTemp, Display, TEXT("DEMO_ARMS_CREATED %d triangles, mannequin skeleton preserved"), Kept);
    return Mesh;
#else
    return nullptr;
#endif
}

USkeletalMesh* UDemoEditorLibrary::PrepareMannequin() { return BuildMannequinArms(false); }
USkeletalMesh* UDemoEditorLibrary::PrepareViewmodelArms(bool bRebuild) { return BuildMannequinArms(true, bRebuild); }

UStaticMesh* UDemoEditorLibrary::CreateProfileMesh(const FString& Name, const TArray<FVector>& Sections, int32 Sides, float WallThickness, UMaterialInterface* Material)
{
#if WITH_EDITOR
    if (Sections.Num() < 2 || Sides < 4 || Sides > 64 || !Name.StartsWith(TEXT("SM_"))) return nullptr;
    const FString PackageName = TEXT("/Game/demo/Weapons/Meshes/") + Name;
    if (auto* Existing = LoadObject<UStaticMesh>(nullptr, *(PackageName + TEXT(".") + Name), nullptr, LOAD_NoWarn)) return Existing;
    FMeshDescription Description;
    FStaticMeshAttributes Attributes(Description);
    Attributes.Register();
    Attributes.GetVertexInstanceUVs().SetNumChannels(1);
    const FPolygonGroupID Group = Description.CreatePolygonGroup();
    Attributes.GetPolygonGroupMaterialSlotNames()[Group] = TEXT("Surface");
    auto Triangle = [&](FVector A, FVector B, FVector C)
    {
        const FVector Normal = FVector::CrossProduct(B - A, C - A).GetSafeNormal();
        const FVector Tangent = (B - A).GetSafeNormal();
        TArray<FVertexInstanceID> Corners;
        for (const FVector Point : {A, B, C})
        {
            FVertexID Vertex = Description.CreateVertex();
            Attributes.GetVertexPositions()[Vertex] = FVector3f(Point);
            FVertexInstanceID Corner = Description.CreateVertexInstance(Vertex);
            Attributes.GetVertexInstanceNormals()[Corner] = FVector3f(Normal);
            Attributes.GetVertexInstanceTangents()[Corner] = FVector3f(Tangent);
            Attributes.GetVertexInstanceBinormalSigns()[Corner] = 1.f;
            const FVector Bitangent = FVector::CrossProduct(Normal, Tangent);
            Attributes.GetVertexInstanceUVs().Set(Corner, 0, FVector2f(FVector::DotProduct(Point - A, Tangent) * 0.05f, FVector::DotProduct(Point - A, Bitangent) * 0.05f));
            Corners.Add(Corner);
        }
        Description.CreatePolygon(Group, Corners);
    };
    auto Point = [Sides](FVector Section, int32 Index, float Shrink)
    {
        const float Angle = 2.f * PI * Index / Sides + PI / Sides;
        return FVector(Section.X, FMath::Cos(Angle) * FMath::Max(0.01, Section.Y - Shrink), FMath::Sin(Angle) * FMath::Max(0.01, Section.Z - Shrink));
    };
    for (int32 Layer = 0; Layer < (WallThickness > 0 ? 2 : 1); ++Layer)
        for (int32 I = 0; I < Sections.Num() - 1; ++I)
            for (int32 J = 0; J < Sides; ++J)
            {
                const FVector A = Point(Sections[I], J, Layer * WallThickness), B = Point(Sections[I + 1], J, Layer * WallThickness);
                const FVector C = Point(Sections[I + 1], (J + 1) % Sides, Layer * WallThickness), D = Point(Sections[I], (J + 1) % Sides, Layer * WallThickness);
                if (Layer) { Triangle(A, B, C); Triangle(A, C, D); } else { Triangle(A, C, B); Triangle(A, D, C); }
            }
    for (int32 End : {0, Sections.Num() - 1})
        for (int32 J = 0; J < Sides; ++J)
        {
            FVector A = Point(Sections[End], J, 0), B = Point(Sections[End], (J + 1) % Sides, 0);
            FVector C = WallThickness > 0 ? Point(Sections[End], (J + 1) % Sides, WallThickness) : FVector(Sections[End].X, 0, 0);
            FVector D = WallThickness > 0 ? Point(Sections[End], J, WallThickness) : C;
            if (End == 0) { Triangle(A, C, B); if (WallThickness > 0) Triangle(A, D, C); }
            else { Triangle(A, B, C); if (WallThickness > 0) Triangle(A, C, D); }
        }
    auto* Mesh = NewObject<UStaticMesh>(CreatePackage(*PackageName), *Name, RF_Public | RF_Standalone);
    Mesh->GetStaticMaterials().Add(FStaticMaterial(Material, TEXT("Surface")));
    UStaticMesh::FBuildMeshDescriptionsParams Params;
    Params.bBuildSimpleCollision = false;
    Params.bFastBuild = false;
    Mesh->BuildFromMeshDescriptions({&Description}, Params);
    FAssetRegistryModule::AssetCreated(Mesh);
    DemoAssets::Save(Mesh);
    return Mesh;
#else
    return nullptr;
#endif
}
