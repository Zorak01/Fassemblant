#include "demo/Game/DemoPlayerController.h"
#include "demo/Game/DemoUserSettings.h"
#include "GameFramework/PlayerInput.h"
#include "GameFramework/InputSettings.h"

void ADemoPlayerController::InitInputSystem()
{
    Super::InitInputSystem();
    // Captured game-mode mouse uses Unreal's high-precision relative input path.
    // Neutralize inherited axis scales as well as old per-machine Input.ini values.
    auto* InputSettings = GetMutableDefault<UInputSettings>();
    InputSettings->bEnableMouseSmoothing = false;
    InputSettings->bEnableFOVScaling = false;
    InputSettings->bEnableLegacyInputScales = false;
    if (PlayerInput)
    {
        FInputAxisProperties Raw;
        Raw.DeadZone = 0.f; Raw.Sensitivity = 1.f; Raw.Exponent = 1.f; Raw.bInvert = false;
        for (FKey Key : {EKeys::MouseX, EKeys::MouseY, EKeys::Mouse2D}) PlayerInput->SetAxisProperties(Key, Raw);
    }
}
void ADemoPlayerController::DemoSensitivity(float Value)
{
    if (auto* Settings = UDemoUserSettings::Get()) { Settings->MouseSensitivity = Value; Settings->ValidateSettings(); Settings->SaveSettings(); }
    DemoInputSettings();
}
void ADemoPlayerController::DemoADSSensitivity(float Value)
{
    if (auto* Settings = UDemoUserSettings::Get()) { Settings->ADSSensitivityMultiplier = Value; Settings->ValidateSettings(); Settings->SaveSettings(); }
    DemoInputSettings();
}
void ADemoPlayerController::DemoInvertMouse(bool bInvert)
{
    if (auto* Settings = UDemoUserSettings::Get()) { Settings->bInvertMouseY = bInvert; Settings->SaveSettings(); }
    DemoInputSettings();
}
void ADemoPlayerController::DemoInputSettings()
{
    if (auto* Settings = UDemoUserSettings::Get()) ClientMessage(FString::Printf(TEXT("Mouse %.3f deg/count | ADS %.2fx | Invert Y %s | Raw relative input"), Settings->MouseSensitivity, Settings->ADSSensitivityMultiplier, Settings->bInvertMouseY ? TEXT("on") : TEXT("off")));
}
