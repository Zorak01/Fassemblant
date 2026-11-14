#include "demo/UI/DemoHUD.h"
#include "demo/Character/DemoCharacter.h"
#include "demo/Weapons/DemoWeapon.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"


#include "GameFramework/PlayerController.h"

void ADemoHUD::DrawHUD()
{
    Super::DrawHUD();
    auto* Character = Cast<ADemoCharacter>(GetOwningPawn());
    if (!Canvas || !Character) return;
    auto* Weapon = Character->GetWeapon();
    if (!Weapon) return;
    const float S = FMath::Clamp(Canvas->SizeY / 1080.f, 0.6f, 2.f);
    const float W = Canvas->SizeX, H = Canvas->SizeY;
    const float X = W * 0.5f, Y = H * 0.5f;
    const FLinearColor White(0.88f, 0.93f, 0.96f, 1);
    const FLinearColor Muted(0.43f, 0.55f, 0.6f, 1);
    const FLinearColor Accent(0.25f, 0.95f, 0.76f, 1);
    const FLinearColor Dark(0.015f, 0.025f, 0.035f, 0.82f);
    const float Aim = Character->GetAimAlpha();
    if (!Character->IsSprinting() && !Weapon->IsReloading())
    {
        const float Gap = (6.f + Character->GetVelocity().Size2D() * 0.015f + Weapon->GetRecoil() * 9.f) * S;
        const FLinearColor Crosshair(0.9f, 0.95f, 1.f, 0.8f * (1.f - Aim));
        for (int32 Sign : {-1, 1})
        {
            DrawLine(X + Sign * Gap, Y, X + Sign * (Gap + 6.f * S), Y, Crosshair, S);
            DrawLine(X, Y + Sign * Gap, X, Y + Sign * (Gap + 6.f * S), Crosshair, S);
        }
        DrawRect(FLinearColor(Accent.R, Accent.G, Accent.B, 0.9f * (1.f - Aim)), X - S, Y - S, 2 * S, 2 * S);
    }
    if (Weapon->GetHitAge() < 0.15f)
    {
        const FLinearColor HitColor = Weapon->WasLastHitKill() ? FLinearColor(1, 0.45f, 0.25f) : White;
        for (int32 SX : {-1, 1}) for (int32 SY : {-1, 1}) DrawLine(X + SX * 8 * S, Y + SY * 8 * S, X + SX * 14 * S, Y + SY * 14 * S, HitColor, 2 * S);
    }
    auto Text = [this, S](const FString& Value, FLinearColor Color, float Left, float Top, float Scale = 1.f)
    { DrawText(Value, Color, Left, Top, GEngine->GetSmallFont(), S * Scale); };
    DrawRect(Dark, 32 * S, 30 * S, 235 * S, 67 * S);
    DrawRect(Accent, 32 * S, 30 * S, 3 * S, 67 * S);
    Text(TEXT("DEMO / FIRING RANGE"), White, 47 * S, 43 * S, 1.1f);
    Text(FString::Printf(TEXT("TARGETS CLEARED  %02d"), Character->GetEliminations()), Muted, 47 * S, 68 * S);
    DrawRect(Dark, W - 245 * S, H - 120 * S, 213 * S, 88 * S);
    Text(TEXT("KESTREL R17  /  AUTO"), Muted, W - 225 * S, H - 108 * S);
    Text(FString::Printf(TEXT("%02d"), Weapon->GetAmmo()), Weapon->GetAmmo() <= 5 ? FLinearColor(1, 0.45f, 0.25f) : White, W - 225 * S, H - 88 * S, 2.5f);
    Text(FString::Printf(TEXT("/ %03d"), Weapon->GetReserve()), Muted, W - 155 * S, H - 68 * S, 1.3f);
    Text(TEXT("WASD MOVE   SHIFT RUN   CTRL / C CROUCH   SPACE JUMP   RMB AIM   LMB FIRE   R RELOAD"), Muted, 32 * S, H - 48 * S);
    if (Weapon->IsReloading())
    {
        Text(TEXT("RELOADING"), White, X - 45 * S, Y + 72 * S);
        DrawRect(Dark, X - 65 * S, Y + 95 * S, 130 * S, 3 * S);
        DrawRect(Accent, X - 65 * S, Y + 95 * S, 130 * S * Weapon->GetReloadProgress(), 3 * S);
    }
    else if (Weapon->GetAmmo() == 0) Text(Weapon->GetReserve() > 0 ? TEXT("R  RELOAD") : TEXT("OUT OF AMMO - RESTART RANGE"), White, X - 65 * S, Y + 72 * S);
}
