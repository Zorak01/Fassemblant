#include "demo/Game/DemoGameMode.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/UI/DemoHUD.h"
#include "demo/Game/DemoPlayerController.h"
#include "demo/Tests/DemoPresentationTest.h"
#include "Misc/CommandLine.h"
#include "Engine/World.h"

ADemoGameMode::ADemoGameMode()
{
    DefaultPawnClass = ADemoCharacter::StaticClass();
    HUDClass = ADemoHUD::StaticClass();
    PlayerControllerClass = ADemoPlayerController::StaticClass();
}
void ADemoGameMode::StartPlay()
{
    Super::StartPlay();
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("DemoPresentationTest"))) GetWorld()->SpawnActor<ADemoPresentationTest>();
#endif
}
