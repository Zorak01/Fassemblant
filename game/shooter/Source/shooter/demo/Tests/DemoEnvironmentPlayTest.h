#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoEnvironmentPlayTest.generated.h"

/** Opt-in rendered traversal check: -DemoEnvironmentTest, never normal play. */
UCLASS(NotBlueprintable, Transient)
class SHOOTER_API ADemoEnvironmentPlayTest : public AActor
{
    GENERATED_BODY()
public:
    ADemoEnvironmentPlayTest();
    virtual void Tick(float Dt) override;
private:
    void Check(const FString& Name,bool bPass);
    void Finish();
    void Place(FIntVector Cell);
    UPROPERTY() TObjectPtr<class ADemoCharacter> Character;
    UPROPERTY() TObjectPtr<class ADemoProceduralEnvironment> Environment;
    TArray<FIntVector> Samples;
    TArray<FVector> Waypoints;
    TArray<FString> Captures;
    FString Directory, Report;
    int32 Stage=0, Sample=0, Waypoint=0;
    float Elapsed=0, Total=0;
    bool bFailed=false;
};
