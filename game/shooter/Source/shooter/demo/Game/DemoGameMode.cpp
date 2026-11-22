#include "demo/Game/DemoGameMode.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/UI/DemoHUD.h"
#include "demo/Game/DemoPlayerController.h"
#include "demo/Tests/DemoPresentationTest.h"
#include "demo/World/DemoProceduralEnvironment.h"
#include "EngineUtils.h"
#include "Misc/CommandLine.h"
#include "Engine/World.h"

ADemoGameMode::ADemoGameMode()
{
    DefaultPawnClass = ADemoCharacter::StaticClass();
    HUDClass = ADemoHUD::StaticClass();
    PlayerControllerClass = ADemoPlayerController::StaticClass();
    ProceduralEnvironmentClass = ADemoProceduralEnvironment::StaticClass();
}
void ADemoGameMode::StartPlay()
{
    Super::StartPlay();
    // The explicit finite mode keeps the authored facility available, including
    // its location-specific presentation/collision regression tests.
    if (bInfiniteEnvironment && ProceduralEnvironmentClass && !FParse::Param(FCommandLine::Get(), TEXT("DemoFiniteRange")))
    {
        TActorIterator<ADemoProceduralEnvironment> Existing(GetWorld());
        if (!Existing) GetWorld()->SpawnActor<ADemoProceduralEnvironment>(ProceduralEnvironmentClass);
    }
#if !UE_BUILD_SHIPPING
    if (FParse::Param(FCommandLine::Get(), TEXT("DemoPresentationTest"))) GetWorld()->SpawnActor<ADemoPresentationTest>();
#endif
}
