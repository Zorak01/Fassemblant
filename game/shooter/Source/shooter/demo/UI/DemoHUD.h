#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DemoHUD.generated.h"

UCLASS()
class SHOOTER_API ADemoHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
