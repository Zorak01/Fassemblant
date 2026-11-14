#include "demo/Tests/DemoPresentationTest.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/Character/DemoMotionComponent.h"
#include "demo/Weapons/DemoWeapon.h"
#include "demo/Weapons/DemoWeaponVisualData.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
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
    switch (Stage)
    {
    case 0:
        if (Elapsed < 5.f) return;
        IFileManager::Get().MakeDirectory(*Directory, true);
        Check(TEXT("Detailed weapon data loaded"), Weapon->VisualData && Weapon->VisualData->Parts.Num() > 20);
        Check(TEXT("Mannequin arms and animation active"), Character->FirstPersonArms->GetSkeletalMeshAsset() && Character->FirstPersonArms->GetAnimInstance());
        Capture(TEXT("01_Hip"));
        Next(); break;
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
        if (Elapsed < 0.85f) return;
        Check(TEXT("Reload active in live game"), Weapon->IsReloading());
        Check(TEXT("Reload hand follows magazine"), FVector::Distance(Character->FirstPersonArms->GetSocketLocation(TEXT("hand_l")), Weapon->GetHandTarget(true).GetLocation()) < 3.f);
        Capture(TEXT("04_Reload"));
        Next(); break;
    case 7:
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
    default:
        if (Elapsed > 1.f) Finish();
        break;
    }
}
