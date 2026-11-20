#include "demo/World/DemoRangeDirector.h"
#include "demo/World/DemoTarget.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/Weapons/DemoWeapon.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"

ADemoRangeDirector::ADemoRangeDirector() { PrimaryActorTick.bCanEverTick = false; }
ADemoRangeDirector* ADemoRangeDirector::Find(const UObject* Context)
{
    return Context ? Cast<ADemoRangeDirector>(UGameplayStatics::GetActorOfClass(Context, StaticClass())) : nullptr;
}
void ADemoRangeDirector::BeginPlay() { Super::BeginPlay(); SessionStart = GetWorld()->GetTimeSeconds(); }
void ADemoRangeDirector::RecordShot(bool bHit, float Damage, bool bKill, bool bHeadshot)
{
    if (bFinished) return;
    if (Shots == 0 && !bTimed) SessionStart = GetWorld()->GetTimeSeconds();
    ++Shots;
    Hits += bHit; Kills += bKill; Headshots += bHeadshot;
    DamageDealt += Damage;
}
float ADemoRangeDirector::GetSeconds() const
{
    return bFinished ? DrillSeconds : static_cast<float>(GetWorld()->GetTimeSeconds() - SessionStart);
}
void ADemoRangeDirector::ResetSession()
{
    GetWorldTimerManager().ClearTimer(DrillTimer);
    Shots = Hits = Kills = Headshots = 0; DamageDealt = 0;
    bTimed = bFinished = false;
    SessionStart = GetWorld()->GetTimeSeconds();
    for (TActorIterator<ADemoTarget> It(GetWorld()); It; ++It) It->ResetTarget();
    if (auto* Character = Cast<ADemoCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
        if (auto* Weapon = Character->GetWeapon()) Weapon->RefillAmmo();
}
void ADemoRangeDirector::StartTimedDrill()
{
    ResetSession(); bTimed = true;
    GetWorldTimerManager().SetTimer(DrillTimer, this, &ThisClass::FinishDrill, DrillSeconds, false);
}
void ADemoRangeDirector::FinishDrill() { bFinished = true; }
void ADemoRangeDirector::EndPlay(const EEndPlayReason::Type Reason)
{
    GetWorldTimerManager().ClearTimer(DrillTimer);
    Super::EndPlay(Reason);
}
