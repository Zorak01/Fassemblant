#include "demo/Tests/DemoPresentationTest.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/Character/DemoMotionComponent.h"
#include "demo/Weapons/DemoWeapon.h"
#include "demo/Weapons/DemoWeaponVisualData.h"
#include "demo/World/DemoRangeDirector.h"
#include "demo/World/DemoTarget.h"
#include "demo/Character/DemoMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

ADemoPresentationTest::ADemoPresentationTest()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
    Directory = FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir() / TEXT("Automation/Presentation"));
    FParse::Value(FCommandLine::Get(), TEXT("DemoReportDir="), Directory);
}
void ADemoPresentationTest::Check(const FString& Name, bool bPass, const FString& Detail)
{
    auto Entry = MakeShared<FJsonObject>();
    Entry->SetStringField(TEXT("name"), Name); Entry->SetBoolField(TEXT("passed"), bPass); Entry->SetStringField(TEXT("detail"), Detail);
    Checks.Add(MakeShared<FJsonValueObject>(Entry));
    bFailed |= !bPass;
    UE_LOG(LogTemp, Display, TEXT("PRESENTATION_CHECK %s %s %s"), bPass ? TEXT("PASS") : TEXT("FAIL"), *Name, *Detail);
}
void ADemoPresentationTest::Capture(const FString& Name)
{
    const FString Filename = Directory / (Name + TEXT(".png"));
    Screenshots.Add(Filename);
    FScreenshotRequest::RequestScreenshot(Filename, false, false, false, FIntRect(), true);
}
void ADemoPresentationTest::Finish()
{
    for (const FString& Filename : Screenshots) Check(TEXT("Renderer screenshot saved"), IFileManager::Get().FileExists(*Filename), FPaths::GetCleanFilename(Filename));
    auto Report = MakeShared<FJsonObject>();
    Report->SetBoolField(TEXT("passed"), !bFailed);
    Report->SetArrayField(TEXT("checks"), Checks);
    Report->SetNumberField(TEXT("durationSeconds"), Total);
    FString Json;
    FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json));
    IFileManager::Get().MakeDirectory(*Directory, true);
    FFileHelper::SaveStringToFile(Json, *(Directory / TEXT("report.json")));
    UE_LOG(LogTemp, Display, TEXT("PRESENTATION_FINISHED %s %s"), bFailed ? TEXT("FAIL") : TEXT("PASS"), *Directory);
    SetActorTickEnabled(false);
    FPlatformMisc::RequestExitWithStatus(false, bFailed ? 1 : 0);
}
void ADemoPresentationTest::Tick(float Dt)
{
    Super::Tick(Dt);
    Elapsed += Dt; Total += Dt;
    if (!Character) Character = Cast<ADemoCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!Character || !Character->GetWeapon())
    {
        if (Total > 15) { Check(TEXT("Playable pawn and weapon spawned"), false); Finish(); }
        return;
    }
    auto* Weapon = Character->GetWeapon();
    auto Next = [this] { ++Stage; Elapsed = 0; };
    auto ReloadPalm = [this, Weapon](const FString& Phase)
    {
        auto* Mesh = Character->FirstPersonArms.Get();
        const FVector Hand = Mesh->GetSocketLocation(TEXT("hand_l"));
        const FVector Forward = (Mesh->GetSocketLocation(TEXT("middle_01_l")) - Hand).GetSafeNormal();
        const FVector Across = (Mesh->GetSocketLocation(TEXT("index_01_l")) - Mesh->GetSocketLocation(TEXT("pinky_01_l"))).GetSafeNormal();
        const FVector Normal = FVector::CrossProduct(Forward, Across).GetSafeNormal();
        const FTransform Target = Weapon->GetHandTarget(true);
        const float Facing = FVector::DotProduct(Normal, Target.GetUnitAxis(EAxis::Z));
        Check(TEXT("Reload palm faces grip: ") + Phase, Facing > .95f,
            FString::Printf(TEXT("normal %.3f"), Facing));
        Check(TEXT("Reload fingers follow grip: ") + Phase,
            FVector::DotProduct(Forward, Target.GetUnitAxis(EAxis::X)) > .95f);
        if (Phase == TEXT("Magazine"))
        {
            // Check physical facing as well as matching a potentially bad target.
            // For the mirrored left hand, forward cross index-to-pinky points
            // out of the back of the hand. The palm faces the opposite way.
            Check(TEXT("Magazine palm faces inward"), FVector::DotProduct(-Normal,
                Weapon->GetActorQuat().GetAxisY()) > .8f);
            Check(TEXT("Magazine fingers point upward"), FVector::DotProduct(Forward,
                Weapon->GetActorQuat().GetAxisZ()) > .8f);
        }
        Capture(Phase == TEXT("Magazine") ? TEXT("04_Reload") : TEXT("Reload_") + Phase);
    };
    switch (Stage)
    {
    case 0:
    {
        if (Elapsed < 5.f) return;
        IFileManager::Get().MakeDirectory(*Directory, true);
        int32 MechanicalGroups = 0;
        bool bDetailedBody = false;
        if (Weapon->VisualData)
        {
            for (const FDemoWeaponPart& Part : Weapon->VisualData->Parts)
            {
                if (Part.Name.ToString().StartsWith(TEXT("CarbineV2_")) && Part.Mesh) ++MechanicalGroups;
                if (Part.Name == TEXT("CarbineV2_Body") && Part.Mesh)
                    bDetailedBody = Part.Mesh->GetNumVertices(0) > 2000 && Part.Mesh->GetStaticMaterials().Num() >= 3;
            }
        }
        Check(TEXT("Authored carbine and mechanical groups loaded"), bDetailedBody && MechanicalGroups == 5);
        Check(TEXT("Mannequin arms and animation active"), Character->FirstPersonArms->GetSkeletalMeshAsset() && Character->FirstPersonArms->GetAnimInstance());
        Check(TEXT("Training session director loaded"), ADemoRangeDirector::Find(this) != nullptr);
        Check(TEXT("Layered shot and impact cues assigned"), Weapon->FireSound && Weapon->HitSound && Weapon->KillSound && Weapon->ImpactSound);
        Check(TEXT("Slide audio and footstep variations assigned"), Character->Motion->SlideLoopSound && Character->Motion->FootstepVariations.Num() == 4);
        Capture(TEXT("01_Hip"));
        Next(); break;
    }
    case 1:
        if (Elapsed < 0.4f) return;
        ShotPitch = Character->GetControlRotation().Pitch;
        Weapon->SetTrigger(true);
        Check(TEXT("Firing produces upward aim kick"), Character->GetControlRotation().Pitch > ShotPitch);
        Next(); break;
    case 2:
        if (Elapsed < 0.12f) return;
        Capture(TEXT("02_Fire"));
        Weapon->SetTrigger(false);
        Check(TEXT("Live firing consumes ammunition"), Weapon->GetAmmo() < Weapon->MagazineSize);
        Next(); break;
    case 3:
        if (Elapsed < 0.6f) return;
        Character->AimPressed();
        Next(); break;
    case 4:
        if (Elapsed < 0.7f) return;
        if (Weapon->VisualData)
        {
            const FTransform Sight = Weapon->VisualData->Sight * Weapon->GetActorTransform();
            const FVector Local = Character->Camera->GetComponentTransform().InverseTransformPosition(Sight.GetLocation());
            Check(TEXT("Physical optic aligned to view axis"), FMath::Abs(Local.Y) < 0.08 && FMath::Abs(Local.Z) < 0.08, Local.ToString());
        }
        Check(TEXT("Gun remains visible during ADS"), !Weapon->IsHidden() && Character->GetAimAlpha() > 0.99f);
        for (bool bLeft : {false, true})
        {
            const FVector Hand = Character->FirstPersonArms->GetSocketLocation(bLeft ? TEXT("hand_l") : TEXT("hand_r"));
            const float Error = FVector::Distance(Hand, Weapon->GetHandTarget(bLeft).GetLocation());
            Check(bLeft ? TEXT("Left hand grips weapon") : TEXT("Right hand grips weapon"), Error < 3.f, FString::Printf(TEXT("%.2f cm"), Error));
            const FVector Middle = Character->FirstPersonArms->GetSocketLocation(bLeft ? TEXT("middle_01_l") : TEXT("middle_01_r"));
            const FVector Forward = (Middle - Hand).GetSafeNormal();
            const float Alignment = FVector::DotProduct(Forward, Weapon->GetHandTarget(bLeft).GetUnitAxis(EAxis::X));
            Check(bLeft ? TEXT("Left palm orientation matches grip") : TEXT("Right palm orientation matches grip"), Alignment > 0.95f, FString::Printf(TEXT("%.3f dot"), Alignment));
        }
        Capture(TEXT("03_ADS"));
        Next(); break;
    case 5:
        if (Elapsed < 0.4f) return;
        Character->ReloadPressed();
        Next(); break;
    case 6:
        if (Elapsed >= .25f && Elapsed - Dt < .25f) ReloadPalm(TEXT("Reach"));
        if (Elapsed >= .55f && Elapsed - Dt < .55f) ReloadPalm(TEXT("Extract"));
        if (Elapsed < 0.85f) return;
        Check(TEXT("Reload active in live game"), Weapon->IsReloading());
        Check(TEXT("Reload hand follows magazine"), FVector::Distance(Character->FirstPersonArms->GetSocketLocation(TEXT("hand_l")), Weapon->GetHandTarget(true).GetLocation()) < 3.f);
        ReloadPalm(TEXT("Magazine"));
        Next(); break;
    case 7:
        if (Elapsed >= .35f && Elapsed - Dt < .35f) ReloadPalm(TEXT("Insert"));
        if (Elapsed >= .75f && Elapsed - Dt < .75f) ReloadPalm(TEXT("Handle"));
        if (Elapsed < 1.4f) return;
        Check(TEXT("Reload finishes and fills magazine"), !Weapon->IsReloading() && Weapon->GetAmmo() == Weapon->MagazineSize);
        Character->SprintPressed();
        Next(); break;
    case 8:
        Character->Move(FInputActionValue(FVector2D(1, 0)));
        if (Elapsed < 0.8f) return;
        Check(TEXT("Live sprint reaches speed"), Character->IsSprinting() && Character->GetVelocity().Size2D() > 500.f);
        Check(TEXT("Sprint drives gait feedback"), !Character->Motion->CameraOffset.IsNearlyZero(0.1f));
        Capture(TEXT("05_Sprint"));
        Character->JumpPressed();
        Next(); break;
    case 9:
        Character->Move(FInputActionValue(FVector2D(1, 0)));
        if (Elapsed > 0.08f) Character->JumpReleased();
        if (Elapsed < 0.18f) return;
        Check(TEXT("Jump has upward momentum"), Character->GetVelocity().Z > 0, FString::Printf(TEXT("Z %.2f, ground %d, held %d, jump count %d"), Character->GetVelocity().Z, Character->GetCharacterMovement()->IsMovingOnGround(), Character->bJumpHeld, Character->JumpCurrentCount));
        Capture(TEXT("06_Jump"));
        Character->SprintReleased(); Character->MoveStopped();
        Next(); break;
    case 10:
        if (Elapsed < 1.f) return;
        Character->Move(FInputActionValue(FVector2D(0, -1)));
        if (Elapsed < 1.3f) return;
        Capture(TEXT("07_Strafe"));
        Character->MoveStopped();
        Next(); break;
    case 11:
        Character->SetActorLocation(FVector(-700,-2600,100), false, nullptr, ETeleportType::TeleportPhysics);
        Character->GetController()->SetControlRotation(FRotator::ZeroRotator);
        Character->GetCharacterMovement()->StopMovementImmediately();
        Character->SprintPressed();
        Next(); break;
    case 12:
        Character->Move(FInputActionValue(FVector2D(1,0)));
        if (Elapsed < 1.2f) return;
        Character->CrouchPressed();
        Check(TEXT("Sprint plus crouch enters slide"), Character->IsSliding());
        Next(); break;
    case 13:
        Character->Move(FInputActionValue(FVector2D(1,0)));
        if (Elapsed < .28f) return;
        Check(TEXT("Slide lowers collision capsule"), Character->bIsCrouched && Character->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() <= 52.1f);
        Check(TEXT("Slide conserves useful forward momentum"), Character->IsSliding() && Character->GetVelocity().Size2D() > 500.f);
        Capture(TEXT("08_Slide"));
        Next(); break;
    case 14:
        Character->Move(FInputActionValue(FVector2D(1,0)));
        if (Elapsed < .48f) return;
        Check(TEXT("Slide passes beneath standing-height gate"), Character->GetActorLocation().X > 380.f && Character->IsSliding(), Character->GetActorLocation().ToString());
        Character->JumpPressed();
        Next(); break;
    case 15:
        Character->Move(FInputActionValue(FVector2D(1,0)));
        if (Elapsed < .16f) return;
        Check(TEXT("Slide jump carries horizontal momentum"), Character->GetCharacterMovement()->IsFalling() && Character->GetVelocity().Size2D() > 350.f && Character->GetVelocity().Z > 0, Character->GetVelocity().ToString());
        Capture(TEXT("09_SlideJump"));
        Character->JumpReleased(); Character->MoveStopped(); Character->CrouchReleased();
        if (auto* Range = ADemoRangeDirector::Find(this)) Range->StartTimedDrill();
        Character->SetActorLocation(FVector(-900,0,100),false,nullptr,ETeleportType::TeleportPhysics);
        Character->GetCharacterMovement()->StopMovementImmediately();
        Character->AimPressed();
        for (TActorIterator<ADemoTarget> It(GetWorld()); It; ++It)
            if (It->StrafeDistance == 0 && It->GetActorLocation().X < 600 && It->GetActorLocation().Y < 0 && It->GetActorLocation().Y > -1000) { TestTarget = *It; break; }
        Next(); break;
    case 16:
        if (!TestTarget) { Check(TEXT("Precision dummy found"), false); Finish(); return; }
        Character->GetController()->SetControlRotation((TestTarget->GetHeadLocation() - Character->Camera->GetComponentLocation()).Rotation());
        if (Elapsed < 1.f) return;
        Weapon->SetTrigger(true); Weapon->SetTrigger(false);
        Check(TEXT("Head hit applies critical damage"), Weapon->WasLastHitHeadshot() && FMath::IsNearlyEqual(Weapon->GetLastDamage(), 68.f), FString::Printf(TEXT("Damage %.1f"), Weapon->GetLastDamage()));
        Next(); break;
    case 17:
        Character->GetController()->SetControlRotation((TestTarget->GetHeadLocation() - Character->Camera->GetComponentLocation()).Rotation());
        if (Elapsed < .16f) return;
        Weapon->SetTrigger(true); Weapon->SetTrigger(false);
        Check(TEXT("Second head hit downs dummy"), !TestTarget->IsActive() && Weapon->WasLastHitKill());
        if (auto* Range = ADemoRangeDirector::Find(this))
            Check(TEXT("Timed drill records hits and accuracy"), Range->Hits == 2 && Range->Kills == 1 && Range->Headshots == 2 && FMath::IsNearlyEqual(Range->GetAccuracy(),100.f));
        Capture(TEXT("10_CriticalHit"));
        Next(); break;
    case 18:
        if (Elapsed < .4f) return;
        if (auto* Range = ADemoRangeDirector::Find(this))
        {
            Range->ResetSession();
            Check(TEXT("Range reset refills weapon and resets statistics"), Range->Shots == 0 && !Range->IsTimed() && Weapon->GetAmmo() == Weapon->MagazineSize && Weapon->GetReserve() >= 600 && TestTarget->IsActive());
        }
        Character->AimReleased();
        Character->SetActorLocation(FVector(-100,-3400,1100),false,nullptr,ETeleportType::TeleportPhysics);
        Character->GetCharacterMovement()->SetMovementMode(MOVE_Flying);
        Character->GetController()->SetControlRotation(FRotator(-12,14,0));
        Next(); break;
    case 19:
        if (Elapsed < .7f) return;
        Capture(TEXT("11_Facility"));
        Next(); break;
    case 20:
        // Use the open return lane: jumping into a slide gate should correctly
        // block momentum, whereas this case tests an unobstructed landing.
        Character->SetActorLocation(FVector(-700,-3400,100),false,nullptr,ETeleportType::TeleportPhysics);
        Character->GetController()->SetControlRotation(FRotator::ZeroRotator);
        Character->GetCharacterMovement()->StopMovementImmediately();
        Character->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        Character->SprintPressed();
        Next(); break;
    case 21:
        Character->Move(FInputActionValue(FVector2D(1,0)));
        if (Elapsed < 1.1f) return;
        Character->JumpPressed();
        Next(); break;
    case 22:
        Character->Move(FInputActionValue(FVector2D(1,0)));
        if (Elapsed < .14f) return;
        Check(TEXT("Sprint takeoff preserves air speed"), Character->GetCharacterMovement()->IsFalling() && Character->GetVelocity().Size2D() > 600.f, Character->GetVelocity().ToString());
        Character->JumpReleased(); Character->CrouchPressed();
        Next(); break;
    case 23:
        Character->Move(FInputActionValue(FVector2D(1,0)));
        if (Elapsed < .85f) return;
        Check(TEXT("Holding crouch chains airborne momentum into a landing slide"), Character->IsSliding() && Character->bIsCrouched && Character->GetVelocity().Size2D() > 350.f, Character->GetVelocity().ToString());
        Capture(TEXT("12_LandingSlide"));
        Character->CrouchReleased(); Character->MoveStopped();
        Next(); break;
    default:
        if (Elapsed > 1.f) Finish();
        break;
    }
}
