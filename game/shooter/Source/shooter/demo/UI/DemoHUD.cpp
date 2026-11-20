#include "demo/UI/DemoHUD.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/Weapons/DemoWeapon.h"
#include "demo/World/DemoRangeDirector.h"
#include "Camera/CameraComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

void ADemoHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* Character = Cast<ADemoCharacter>(GetOwningPawn());
    if (!Canvas || !Character || !Character->GetWeapon()) return;
    auto* Weapon = Character->GetWeapon();
    if (!RangeDirector) RangeDirector = ADemoRangeDirector::Find(this);
    const float Dt = GetWorld()->GetDeltaSeconds();
    const float S = FMath::Clamp(Canvas->SizeY / 1080.f, .65f, 2.f);
    const float W = Canvas->SizeX, H = Canvas->SizeY, X = W * .5f, Y = H * .5f;
    const FLinearColor White(.9f,.94f,.98f,1), Muted(.52f,.64f,.7f,1), Accent(.2f,.95f,.78f,1);
    const FLinearColor Orange(1,.49f,.22f,1), Dark(.012f,.022f,.032f,.88f);
    const float Aim = Character->GetAimAlpha();
    const float Visibility = (Character->IsSprinting() || Weapon->IsReloading()) ? 0.f : 1.f - Aim;
    CrosshairOpacity = FMath::FInterpTo(CrosshairOpacity, Visibility, Dt, 18.f);
    // Match the actual cone projected through the current camera FOV.
    const float Radius = FMath::Tan(FMath::DegreesToRadians(Weapon->GetSpreadDegrees())) * W
        / (2.f * FMath::Tan(FMath::DegreesToRadians(Character->Camera->FieldOfView * .5f)));
    const float Kick = FMath::Exp(-Weapon->GetShotAge() * 22.f) * 4.f * S;
    CrosshairGap = FMath::FInterpTo(CrosshairGap, FMath::Clamp(Radius, 5.f*S, 34.f*S), Dt, 18.f);
    const float Gap = CrosshairGap + Kick;
    const float HitAge = Weapon->GetHitAge();
    const float HitAlpha = FMath::Clamp(1.f - HitAge / .24f, 0.f, 1.f);
    FLinearColor Reticle = FMath::Lerp(White, Weapon->WasLastHitKill() ? Orange : Accent, HitAlpha);
    Reticle.A = CrosshairOpacity;
    const FLinearColor Outline(0,0,0,.8f * CrosshairOpacity);
    auto Stroke = [this, S, Reticle, Outline](float A, float B, float C, float D)
    { DrawLine(A,B,C,D,Outline,4.f*S); DrawLine(A,B,C,D,Reticle,1.5f*S); };
    for (int32 Sign : {-1,1})
    {
        Stroke(X+Sign*Gap,Y,X+Sign*(Gap+7.f*S),Y);
        Stroke(X,Y+Sign*Gap,X,Y+Sign*(Gap+7.f*S));
    }
    DrawRect(Outline, X-2*S,Y-2*S,4*S,4*S);
    DrawRect(FLinearColor(Accent.R,Accent.G,Accent.B,CrosshairOpacity),X-S,Y-S,2*S,2*S);
    if (HitAlpha > 0)
    {
        FLinearColor C = Weapon->WasLastHitKill() ? Orange : (Weapon->WasLastHitHeadshot() ? FLinearColor(1,.85f,.35f,1) : White);
        C.A = HitAlpha;
        const float Inner = (8.f + 3.f * HitAlpha) * S;
        for (int32 SX : {-1,1}) for (int32 SY : {-1,1})
            DrawLine(X+SX*Inner,Y+SY*Inner,X+SX*(Inner+6*S),Y+SY*(Inner+6*S),C,2*S);
    }
    auto Text = [this,S](const FString& Value,FLinearColor C,float Left,float Top,float Scale=1.f)
    { DrawText(Value,C,Left,Top,GEngine->GetSmallFont(),S*Scale); };
    if (HitAge < .65f)
    {
        FLinearColor C = Weapon->WasLastHitHeadshot() ? FLinearColor(1,.8f,.3f,1) : White;
        C.A = FMath::Clamp((.65f-HitAge)*5.f,0.f,1.f);
        Text(FString::Printf(TEXT("%.0f%s"),Weapon->GetLastDamage(), Weapon->WasLastHitHeadshot()?TEXT(" CRITICAL"):TEXT("")), C, X+28*S,Y-40*S-HitAge*25*S,1.3f);
    }
    DrawRect(Dark,28*S,28*S,345*S,113*S);
    DrawRect(Accent,28*S,28*S,3*S,113*S);
    Text(TEXT("PULSE / TRAINING FACILITY"),White,44*S,40*S,1.15f);
    if (RangeDirector)
    {
        const bool Timed = RangeDirector->IsTimed();
        const FString Mode = Timed ? (RangeDirector->IsFinished()?TEXT("DRILL COMPLETE"):FString::Printf(TEXT("TIME TRIAL  %02d"),FMath::CeilToInt(FMath::Max(0.f,RangeDirector->DrillSeconds-RangeDirector->GetSeconds())))) : TEXT("FREE PRACTICE");
        Text(Mode,Timed?Orange:Accent,44*S,66*S);
        Text(FString::Printf(TEXT("ACCURACY %.0f%%   HITS %d/%d"),RangeDirector->GetAccuracy(),RangeDirector->Hits,RangeDirector->Shots),White,44*S,89*S);
        Text(FString::Printf(TEXT("KILLS %02d    DPS %.0f    HEAD %02d"),RangeDirector->Kills,RangeDirector->GetDPS(),RangeDirector->Headshots),Muted,44*S,112*S);
    }
    DrawRect(Dark,W-250*S,H-128*S,222*S,100*S);
    DrawRect(Accent,W-250*S,H-128*S,222*S*(float(Weapon->GetAmmo())/Weapon->MagazineSize),3*S);
    Text(TEXT("KESTREL R17 / AUTO"),Muted,W-230*S,H-111*S);
    Text(FString::Printf(TEXT("%02d"),Weapon->GetAmmo()),Weapon->GetAmmo()<=5?Orange:White,W-230*S,H-88*S,2.6f);
    Text(FString::Printf(TEXT("/ %03d"),Weapon->GetReserve()),Muted,W-148*S,H-64*S,1.2f);
    const float Speed = Character->GetVelocity().Size2D() * .036f;
    Text(FString::Printf(TEXT("%02.0f KM/H  %s"),Speed,Character->IsSliding()?TEXT("SLIDING"):Character->IsSprinting()?TEXT("SPRINT"):TEXT("")),Character->IsSliding()?Accent:Muted,X-55*S,H-60*S,1.1f);
    DrawRect(Dark,28*S,H-86*S,780*S,58*S);
    Text(TEXT("SHIFT SPRINT + CTRL / C SLIDE   SPACE SLIDE JUMP   RMB AIM   R RELOAD"),White,42*S,H-76*S);
    Text(TEXT("E REFILL & RESET    T START 60s DRILL    WASD MOVE"),Muted,42*S,H-52*S);
    if (Weapon->IsReloading())
    {
        Text(TEXT("RELOADING"),White,X-43*S,Y+70*S);
        DrawRect(Dark,X-64*S,Y+94*S,128*S,3*S);
        DrawRect(Accent,X-64*S,Y+94*S,128*S*Weapon->GetReloadProgress(),3*S);
    }
}
