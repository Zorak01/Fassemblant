#pragma once
#include "CoreMinimal.h"
#include "GameFramework/GameUserSettings.h"
#include "DemoUserSettings.generated.h"

/** Per-user settings, saved in Saved/Config; never changes shared Blueprint defaults. */
UCLASS(Config=GameUserSettings)
class SHOOTER_API UDemoUserSettings : public UGameUserSettings
{
    GENERATED_BODY()
public:
    static UDemoUserSettings* Get();
    virtual void SetToDefaults() override;
    virtual void ValidateSettings() override;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Demo|Input") float MouseSensitivity = 0.10f;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Demo|Input") float ADSSensitivityMultiplier = 0.22f;
    UPROPERTY(Config) int32 InputSettingsRevision = 0;
    UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category="Demo|Input") bool bInvertMouseY = false;
};
