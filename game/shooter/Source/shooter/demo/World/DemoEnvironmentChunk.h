#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "demo/World/DemoEnvironmentPalette.h"
#include "DemoEnvironmentChunk.generated.h"

/** One disposable collision/rendering chunk; geometry is instanced by material. */
UCLASS(NotBlueprintable, Transient)
class SHOOTER_API ADemoEnvironmentChunk : public AActor
{
    GENERATED_BODY()
public:
    ADemoEnvironmentChunk();
    void Generate(int32 Seed, FIntPoint Cell, double Size, bool bTargets, TSubclassOf<class ADemoTarget> TargetClass,
        const FDemoEnvironmentPalette& Palette);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<class UHierarchicalInstancedStaticMeshComponent> Ground;
    UPROPERTY() TMap<uint8,TObjectPtr<class UHierarchicalInstancedStaticMeshComponent>> Batches;
    UPROPERTY() TObjectPtr<class UStaticMesh> BoxMesh;
    UPROPERTY() TObjectPtr<class UStaticMesh> CylinderMesh;
    UPROPERTY() TObjectPtr<class UStaticMesh> SphereMesh;
    UPROPERTY() TObjectPtr<class URectLightComponent> RoomLight;
    UPROPERTY() TArray<TObjectPtr<class UTextRenderComponent>> Signs;
    UPROPERTY() TArray<TObjectPtr<class ADemoTarget>> Targets;
};
