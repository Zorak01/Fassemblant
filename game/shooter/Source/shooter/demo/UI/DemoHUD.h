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
private:
    float CrosshairGap = 6.f;
    float CrosshairOpacity = 1.f;
    UPROPERTY(Transient) TObjectPtr<class ADemoRangeDirector> RangeDirector;
    UPROPERTY(Transient) TObjectPtr<class ADemoProceduralEnvironment> Interior;
};
