#pragma once
#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "DemoPlayerController.generated.h"

UCLASS()
class SHOOTER_API ADemoPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    UFUNCTION(Exec) void DemoSensitivity(float DegreesPerCount = 0.10f);
    UFUNCTION(Exec) void DemoADSSensitivity(float Multiplier = 0.22f);
    UFUNCTION(Exec) void DemoInvertMouse(bool bInvert = false);
    UFUNCTION(Exec) void DemoInputSettings();
    UFUNCTION(Exec) void DemoResetRange();
    UFUNCTION(Exec) void DemoTimedDrill();
protected:
    virtual void InitInputSystem() override;
    virtual void SetupInputComponent() override;
};
