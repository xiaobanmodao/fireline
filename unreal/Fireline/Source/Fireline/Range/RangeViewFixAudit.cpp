#include "RangeGame.h"
#include "RangeShotFX.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"

void ARangeCharacter::StartViewFixAudit()
{
 bViewFixAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineViewFixAudit"));if(!bViewFixAudit)return;
 ViewFixSamples=TEXT("time,speed,crouched,sliding,w,left_shift,right_shift,c,aim,reloading,ammo,flash_error_cm,flashes\n");
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 auto Key=[this](FKey K,EInputEvent E){CastChecked<APlayerController>(Controller)->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
 auto Reset=[this]{SetActorLocation(FVector(-450,-350,95));GetCharacterMovement()->StopMovementImmediately();Controller->SetControlRotation(FRotator::ZeroRotator);};
 auto Release=[Key]{for(FKey K:{EKeys::W,EKeys::LeftShift,EKeys::RightShift,EKeys::C})Key(K,IE_Released);};
 Later(2,Reset);
 Later(3,[Key]{Key(EKeys::W,IE_Pressed);Key(EKeys::LeftShift,IE_Pressed);});Later(3.6f,[Key]{Key(EKeys::C,IE_Pressed);});Later(4.6f,Release);
 Later(5,Reset);Later(5.1f,[Key]{Key(EKeys::W,IE_Pressed);Key(EKeys::RightShift,IE_Pressed);Key(EKeys::C,IE_Pressed);});Later(6.3f,Release);
 Later(7,[this,Reset]{Reset();ToggleView();Controller->SetControlRotation(FRotator(-10,50,0));});
 Later(7.2f,[Key]{Key(EKeys::W,IE_Pressed);Key(EKeys::RightShift,IE_Pressed);});Later(7.4f,[Key]{Key(EKeys::C,IE_Pressed);});Later(8.5f,Release);
 Later(9,[this,Reset]{ToggleView();Reset();});
 Later(9.4f,[Key]{Key(EKeys::W,IE_Pressed);Key(EKeys::C,IE_Pressed);});Later(9.9f,Release);
 Later(10,[Key]{Key(EKeys::C,IE_Pressed);});Later(10.7f,Release);
 Later(11,[Key]{Key(EKeys::LeftMouseButton,IE_Pressed);});
 Later(12,[Key]{Key(EKeys::RightMouseButton,IE_Pressed);});Later(12.8f,[Key]{Key(EKeys::LeftMouseButton,IE_Released);});
 Later(13,[Key]{Key(EKeys::R,IE_Pressed);});Later(13.1f,[Key]{Key(EKeys::R,IE_Released);});
 Later(16.5f,[Key]{Key(EKeys::RightMouseButton,IE_Released);});
 const TArray<float> Captures={2.6f,3.9f,5.5f,7.65f,10.2f,11.12f,12.22f,14.1f,15.8f,17};
 for(int I=0;I<Captures.Num();++I)Later(Captures[I],[this,I]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/view-fix-%02d.png"),I),true,false);});
 Later(18,[this]{FFileHelper::SaveStringToFile(ViewFixSamples,*(FPaths::ProjectSavedDir()/TEXT("view-fix-audit.csv")));FPlatformMisc::RequestExit(false);});
}
void ARangeCharacter::TickViewFixAudit(float Dt)
{
 const float T=GetGameTimeSinceCreation();auto* PC=CastChecked<APlayerController>(Controller);
 // This audit deliberately keeps normal WASD/sprint/fire polling enabled.
 // No AddMovementInput injection or direct velocity modification by the audit.
 if(T>11&&T<12.7f)PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::MouseX,4.0,Dt,1,FPlatformTime::Cycles64()));
 if(T<2||T>=18)return;
 float Error=0;int Count=0;
 for(TActorIterator<ARangeShotFX> It(GetWorld());It;++It)
 {Error=FMath::Max(Error,FVector::Distance(It->GetFlashBase(),ViewGun->GetSocketLocation(TEXT("M4_Flash"))));++Count;}
 ViewFixSamples+=FString::Printf(TEXT("%.5f,%.5f,%d,%d,%d,%d,%d,%d,%.5f,%d,%d,%.6f,%d\n"),T,GetVelocity().Size2D(),bIsCrouched,bSliding,PC->IsInputKeyDown(EKeys::W),PC->IsInputKeyDown(EKeys::LeftShift),PC->IsInputKeyDown(EKeys::RightShift),PC->IsInputKeyDown(EKeys::C),AimAlpha,bReloading,Ammo,Error,Count);
}
