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
    void Generate(int32 Seed, FIntVector Cell, double Size, double LayerHeight, bool bTargets, TSubclassOf<class ADemoTarget> TargetClass,
        const FDemoEnvironmentPalette& Palette);
    void SetConnection(int32 Direction, bool bConnected);
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<class UHierarchicalInstancedStaticMeshComponent> Ground;
    UPROPERTY() TMap<uint8,TObjectPtr<class UHierarchicalInstancedStaticMeshComponent>> Batches;
    UPROPERTY() TObjectPtr<class UStaticMesh> BoxMesh;
    UPROPERTY() TObjectPtr<class URectLightComponent> RoomLight;
    UPROPERTY() TObjectPtr<class URectLightComponent> StairLight;
    UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> ConnectionSeals;
    UPROPERTY() TArray<TObjectPtr<class UTextRenderComponent>> Signs;
    UPROPERTY() TArray<TObjectPtr<class ADemoTarget>> Targets;
};
