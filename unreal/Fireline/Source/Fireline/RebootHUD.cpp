#include "RebootHUD.h"
#include "Engine/Canvas.h"
#include "Variant_Shooter/ShooterCharacter.h"
#include "Variant_Shooter/Weapons/ShooterWeapon.h"
#include "GameFramework/PlayerController.h"
void ARebootHUD::DrawHUD()
{
    Super::DrawHUD();
    const float X = Canvas ? FMath::Max(16.f, Canvas->SizeX - 586.f) : 16.f;
    const float Y = 16.f;
    DrawRect(FLinearColor(0.025f,0.035f,0.055f,0.85f),X,Y,570,120);
    DrawText(TEXT("FIRELINE / UE5 ENGINE BASELINE"),FLinearColor(0.3f,0.8f,1.f),X+12,Y+10,nullptr,1.3f);
    DrawText(TEXT("Temporary Epic mannequin + weapon. Selected art: Ryan / TastyTony."),FLinearColor::White,X+12,Y+36);
    DrawText(TEXT("WASD move | Space jump | Shift sprint | Ctrl crouch | LMB fire | R reload"),FLinearColor::White,X+12,Y+57);
    if (auto* Character=Cast<AShooterCharacter>(GetOwningPawn()))
    {
        if (auto* Weapon=Character->GetActiveWeapon())
        {
            const FString Status=Weapon->IsReloading()?TEXT("RELOADING - mechanic only; authored animation pending"):FString::Printf(TEXT("AMMO %d / %d"),Weapon->GetAmmo(),Weapon->GetCapacity());
            DrawText(Status,FLinearColor(1.f,0.75f,0.3f),X+12,Y+82);
        }
        else DrawText(TEXT("Walk over the rifle pickup to equip."),FLinearColor(1.f,0.75f,0.3f),X+12,Y+82);
    }
}
