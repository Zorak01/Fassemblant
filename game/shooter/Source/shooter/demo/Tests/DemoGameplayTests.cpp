#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/Weapons/DemoWeapon.h"
#include "demo/World/DemoTarget.h"
#include "Engine/World.h"
#include "Engine/DamageEvents.h"
#include "TimerManager.h"
#include "Tests/AutomationCommon.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FDemoVerifyReload, FAutomationTestBase*, Test, UWorld*, World, ADemoWeapon*, Weapon);
bool FDemoVerifyReload::Update()
{
    // A separate engine frame is required: TimerManager ticks at most once per frame.
    World->GetTimerManager().Tick(2.f);
    Test->TestEqual(TEXT("Reload fills magazine"), Weapon->GetAmmo(), 30);
    Test->TestEqual(TEXT("Only missing rounds deducted"), Weapon->GetReserve(), 179);
    Test->TestFalse(TEXT("Reload state returns to ready"), Weapon->IsReloading());
    World->DestroyWorld(false);
    return true;
}

DEFINE_LATENT_AUTOMATION_COMMAND_THREE_PARAMETER(FDemoVerifyTargetReset, FAutomationTestBase*, Test, UWorld*, World, ADemoTarget*, Target);
bool FDemoVerifyTargetReset::Update()
{
    World->GetTimerManager().Tick(3.f);
    Test->TestTrue(TEXT("Target resets after delay"), Target->IsActive());
    Test->TestTrue(TEXT("Collision restored"), Target->GetActorEnableCollision());
    World->DestroyWorld(false);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoWeaponTest, "Demo.Gameplay.WeaponLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDemoWeaponTest::RunTest(const FString& Parameters)
{
    const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    auto* Character = World->SpawnActor<ADemoCharacter>(FVector::ZeroVector, FRotator::ZeroRotator, Params);
    Character->DispatchBeginPlay();
    auto* Weapon = Character->GetWeapon();
    if (!TestNotNull(TEXT("Character owns a weapon"), Weapon)) { World->DestroyWorld(false); return false; }
    if (!Weapon->HasActorBegunPlay()) Weapon->DispatchBeginPlay();
    TestEqual(TEXT("Starts with full magazine"), Weapon->GetAmmo(), 30);
    TestFalse(TEXT("Cannot reload a full magazine"), Weapon->Reload());
    Weapon->SetTrigger(true);
    TestEqual(TEXT("First press fires one round"), Weapon->GetAmmo(), 29);
    Weapon->SetTrigger(true);
    TestEqual(TEXT("Cadence prevents another shot at the same time"), Weapon->GetAmmo(), 29);
    Weapon->SetTrigger(false);
    TestTrue(TEXT("Partial magazine can reload"), Weapon->Reload());
    TestFalse(TEXT("Repeated reload request rejected"), Weapon->Reload());
    TestEqual(TEXT("Ammo is not transferred early"), Weapon->GetAmmo(), 29);
    Weapon->SetTrigger(true);
    TestEqual(TEXT("Reload blocks shooting"), Weapon->GetAmmo(), 29);
    Weapon->SetTrigger(false);
    World->GetTimerManager().Tick(0.01f);
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.02f));
    ADD_LATENT_AUTOMATION_COMMAND(FDemoVerifyReload(this, World, Weapon));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoTargetTest, "Demo.Gameplay.TargetLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDemoTargetTest::RunTest(const FString& Parameters)
{
    const auto Settings = UWorld::InitializationValues().AllowAudioPlayback(false).CreatePhysicsScene(true).CreateNavigation(false).CreateAISystem(false).ShouldSimulatePhysics(false);
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Settings);
    auto* Target = World->SpawnActor<ADemoTarget>();
    Target->DispatchBeginPlay();
    FDamageEvent DamageEvent;
    TestEqual(TEXT("Damage applied"), Target->TakeDamage(34.f, DamageEvent, nullptr, nullptr), 34.f);
    Target->TakeDamage(34.f, DamageEvent, nullptr, nullptr);
    TestTrue(TEXT("Target survives two hits"), Target->IsActive());
    TestEqual(TEXT("Final damage clamps to remaining health"), Target->TakeDamage(34.f, DamageEvent, nullptr, nullptr), 32.f);
    TestFalse(TEXT("Three hits disable target"), Target->IsActive());
    TestFalse(TEXT("Down target collision disabled"), Target->GetActorEnableCollision());
    TestEqual(TEXT("Down target ignores damage"), Target->TakeDamage(34.f, DamageEvent, nullptr, nullptr), 0.f);
    World->GetTimerManager().Tick(0.01f);
    ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(0.02f));
    ADD_LATENT_AUTOMATION_COMMAND(FDemoVerifyTargetReset(this, World, Target));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDemoControllerFeelTest, "Demo.Gameplay.ControllerFeel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FDemoControllerFeelTest::RunTest(const FString& Parameters)
{
    UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
    auto* Character = World->SpawnActor<ADemoCharacter>();
    Character->Look(FInputActionValue(FVector2D(10, -10)));
    TestTrue(TEXT("Mouse right produces positive yaw"), Character->PendingLook.X > 0);
    TestTrue(TEXT("Mouse up produces negative pitch (look up)"), Character->PendingLook.Y < 0);
    Character->PendingLook = FVector2D::ZeroVector;
    Character->bInvertMouseY = true;
    Character->Look(FInputActionValue(FVector2D(10, -10)));
    TestTrue(TEXT("Optional invert Y reverses only pitch"), Character->PendingLook.Y > 0 && Character->PendingLook.X > 0);
    auto* Movement = Character->GetCharacterMovement();
    TestTrue(TEXT("Crouching supported"), Movement->GetNavAgentPropertiesRef().bCanCrouch);
    Movement->SetMovementMode(MOVE_Walking);
    Character->CrouchPressed();
    Movement->Crouch(false);
    TestTrue(TEXT("Crouch lowers collision capsule"), Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() < 90.f);
    TestTrue(TEXT("Crouch stops sprint"), !Character->bWantsSprint);
    Character->CrouchReleased();
    Movement->UnCrouch(false);
    TestFalse(TEXT("Releasing crouch restores standing when clear"), Character->bIsCrouched);
    World->DestroyWorld(false);
    return true;
}
#endif
