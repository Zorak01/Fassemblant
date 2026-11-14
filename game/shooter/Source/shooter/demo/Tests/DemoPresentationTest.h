#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DemoPresentationTest.generated.h"

/** Opt-in standalone play test with real rendering, physics and animation.
 * Enabled only by -DemoPresentationTest, never during ordinary play. */
UCLASS(NotBlueprintable, Transient)
class SHOOTER_API ADemoPresentationTest : public AActor
{
    GENERATED_BODY()
public:
    ADemoPresentationTest();
    virtual void Tick(float Dt) override;
private:
    void Check(const FString& Name, bool bPass, const FString& Detail = TEXT(""));
    void Capture(const FString& Name);
    void Finish();
    UPROPERTY() TObjectPtr<class ADemoCharacter> Character;
    int32 Stage = 0;
    float Elapsed = 0;
    float Total = 0;
    bool bFailed = false;
    double ShotPitch = 0;
    FString Directory;
    TArray<TSharedPtr<class FJsonValue>> Checks;
    TArray<FString> Screenshots;
};
