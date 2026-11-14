#include "demo/Game/DemoUserSettings.h"
#include "Engine/Engine.h"

UDemoUserSettings* UDemoUserSettings::Get() { return GEngine ? Cast<UDemoUserSettings>(GEngine->GetGameUserSettings()) : nullptr; }
void UDemoUserSettings::SetToDefaults()
{
    Super::SetToDefaults();
    MouseSensitivity = 0.10f;
    ADSSensitivityMultiplier = 0.22f;
    InputSettingsRevision = 0;
    bInvertMouseY = false;
}
void UDemoUserSettings::ValidateSettings()
{
    Super::ValidateSettings();
    MouseSensitivity = FMath::IsFinite(MouseSensitivity) ? FMath::Clamp(MouseSensitivity, 0.005f, 1.f) : 0.10f;
    if (InputSettingsRevision < 1)
    {
        // Migrate the old stock ADS gain, preserving custom sensitivity choices.
        if (FMath::IsNearlyEqual(ADSSensitivityMultiplier, 0.75f)) ADSSensitivityMultiplier = 0.22f;
        InputSettingsRevision = 1;
    }
    ADSSensitivityMultiplier = FMath::IsFinite(ADSSensitivityMultiplier) ? FMath::Clamp(ADSSensitivityMultiplier, 0.05f, 2.f) : 0.22f;
}
