#include "demo/Game/DemoGameMode.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/UI/DemoHUD.h"
#include "GameFramework/PlayerController.h"

ADemoGameMode::ADemoGameMode()
{
    DefaultPawnClass = ADemoCharacter::StaticClass();
    HUDClass = ADemoHUD::StaticClass();
    PlayerControllerClass = APlayerController::StaticClass();
}
