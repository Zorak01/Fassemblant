#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoRangeDirector.generated.h"

/** Session statistics and repeatable timed drills; only present in training maps. */
UCLASS()
class SHOOTER_API ADemoRangeDirector : public AActor
{
    GENERATED_BODY()
public:
    ADemoRangeDirector();
    static ADemoRangeDirector* Find(const UObject* Context);
    void RecordShot(bool bHit, float Damage, bool bKill, bool bHeadshot);
    void ResetSession();
    void StartTimedDrill();
    float GetSeconds() const;
    float GetAccuracy() const { return Shots > 0 ? 100.f * Hits / Shots : 0.f; }
    float GetDPS() const { return DamageDealt / FMath::Max(1.f, GetSeconds()); }
    bool IsTimed() const { return bTimed; }
    bool IsFinished() const { return bFinished; }
    int32 Shots = 0, Hits = 0, Kills = 0, Headshots = 0;
    float DamageDealt = 0;
    UPROPERTY(EditAnywhere, Category="Demo|Range") float DrillSeconds = 60.f;
protected:
    virtual void BeginPlay() override;
private:
    void FinishDrill();
    double SessionStart = 0;
    bool bTimed = false, bFinished = false;
    FTimerHandle DrillTimer;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
};
