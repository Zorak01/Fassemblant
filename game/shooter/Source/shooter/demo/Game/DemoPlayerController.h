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
protected:
    virtual void InitInputSystem() override;
};
