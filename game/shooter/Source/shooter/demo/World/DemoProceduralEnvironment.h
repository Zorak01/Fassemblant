#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "demo/World/DemoEnvironmentPalette.h"
#include "DemoProceduralEnvironment.generated.h"

/** Single-player sealed interior streaming, including descending decks. Only the nearby working set lives in memory. */
UCLASS(Blueprintable)
class SHOOTER_API ADemoProceduralEnvironment : public AActor
{
    GENERATED_BODY()
public:
    ADemoProceduralEnvironment();
    virtual void Tick(float Dt) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Generation") int32 WorldSeed = 1729;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Generation", meta=(ClampMin="4000", ClampMax="12000")) double ChunkSize = 4000;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Generation", meta=(ClampMin="2", ClampMax="5")) int32 LoadRadius = 2;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Generation", meta=(ClampMin="1200",ClampMax="2000")) double LayerHeight = 1400;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Generation", meta=(ClampMin="1",ClampMax="3")) int32 LoadAbove = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Generation", meta=(ClampMin="2",ClampMax="4")) int32 LoadBelow = 2;
    UFUNCTION(BlueprintPure, Category="Demo|Generation") int32 GetCurrentDeck() const { return -LastCenter.Z; }
    UFUNCTION(BlueprintPure, Category="Demo|Generation") FString GetCurrentRoomName() const;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Demo|Generation", meta=(ClampMin="1", ClampMax="8")) int32 ChunksPerUpdate = 2;
    UPROPERTY(EditAnywhere, Category="Demo|Generation") bool bReplaceAuthoredLayout = true;
    UPROPERTY(EditAnywhere, Category="Demo|Generation") bool bSpawnTargets = true;
    UPROPERTY(EditAnywhere, Category="Demo|Generation") TSubclassOf<class ADemoTarget> TargetClass;
    UPROPERTY(EditAnywhere, Category="Demo|Appearance") TObjectPtr<UMaterialInterface> GroundMaterial;
    UPROPERTY(EditAnywhere, Category="Demo|Appearance") TObjectPtr<UMaterialInterface> StructureMaterial;
    UPROPERTY(EditAnywhere, Category="Demo|Appearance") TObjectPtr<UMaterialInterface> AccentMaterial;
    UPROPERTY(EditAnywhere, Category="Demo|Appearance") FDemoEnvironmentPalette RoomPalette;
    UFUNCTION(BlueprintPure, Category="Demo|Generation") int32 GetLoadedChunkCount() const { return Chunks.Num(); }
protected:
    virtual void BeginPlay() override;
private:
    void RemoveAuthoredLayout();
    void QueueAround(FIntVector Center);
    void GenerateChunk(FIntVector Cell);
    void RebaseIfNeeded(FVector Position);
    UPROPERTY(Transient) TMap<FIntVector, TObjectPtr<class ADemoEnvironmentChunk>> Chunks;
    UPROPERTY(Transient) TObjectPtr<class APawn> TrackedPawn;
    TArray<FIntVector> Pending;
    FIntVector LastCenter = FIntVector::ZeroValue;
    FString CurrentRoomName;
    bool bInitialized = false;
    bool bPreviousBoundsChecks = true;
};
