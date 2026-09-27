#include "RangeGame.h"
#include "RangeWeaponTiming.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"

void ARangeCharacter::RequestWeapon(int32 Mode)
{
 if(HandSwitchLeft>0){QueuedWeapon=Mode;return;}
 if(Mode<0||Mode>3||Mode==WeaponSlot())return;
 if(Mode==3&&!bMP7Ready)return;
 if(Mode==2&&!ViewKnife->GetSkeletalMeshAsset())return;
 bFireAfterReloadReturn=false;ReloadFireReturnLeft=0;
 // Preserve the current reload pose while lowering; never snap back to hold.
 // Preserve rounds already committed at insertion; cancellation grants none.
 if(bReloading){bReloading=false;bReloadPoseHeld=true;ReloadLeft=0;ReloadLeadOut=0;}
 EndAim();CancelMP7Action();if(bHandTest)HandTestBlendOut=.18f;bHandTest=false;
 PendingWeapon=Mode;QueuedWeapon=-1;QueuedKnifeAction=-1;
 bSwitchFromKnife=bKnife;bSwitchFromM4=WeaponSlot()==1;
 HandSwitchDuration=bSwitchFromM4?RangeWeaponTiming::M4RemoveDuration+.24f:.48f;
 HandSwitchLeft=HandSwitchDuration;
 if(bSwitchFromM4)StartM4Action(2);else CancelM4Action();
}
void ARangeCharacter::CycleWeapon()
{
 // Cycle from the last requested slot, so quick wheel reversals remain deterministic.
 const int32 Current=QueuedWeapon>=0?QueuedWeapon:HandSwitchLeft>0?PendingWeapon:WeaponSlot();
 RequestWeapon(Current==1?(bMP7Ready?3:2):Current==3?2:1);
}
void ARangeCharacter::ToggleEmptyHands(){RequestWeapon(bEmptyHands?1:0);}
void ARangeCharacter::EquipRifle(){RequestWeapon(1);}
void ARangeCharacter::EquipKnife(){RequestWeapon(2);}
void ARangeCharacter::TestFingers()
{
 if(bKnife){StartKnifeAction(2);return;}
 if(bMP7){InspectMP7();return;}
 if(!bEmptyHands||HandSwitchLeft>0)return;
 if(bHandTest){bHandTest=false;HandTestBlendOut=.18f;}
 else if(HandTestBlendOut<=0){bHandTest=true;HandTestTime=0;}
}
void ARangeCharacter::UpdateHandVisibility()
{
 ViewGun->SetVisibility(!bThirdPerson&&!bEmptyHands&&!bKnife&&(M4Action!=0||HandSwitchLeft<=0||HandSwitchLeft>.24f||HandSwitchLeft<.08f));
 BodyGun->SetVisibility(!bEmptyHands&&!bKnife);
 ViewKnife->SetVisibility(!bThirdPerson&&bKnife);BodyKnife->SetVisibility(bKnife);
 UpdateOpticVisibility();
}
void ARangeCharacter::UpdateHands(float Dt)
{
 // Preserve the last test sample until the graph has blended out; resetting
 // its time at cancel would snap the wrist/fingers before the arms lower.
 HandTestBlendOut=FMath::Max(0.f,HandTestBlendOut-Dt);
 if(HandSwitchLeft>0)
 {
  const float Before=HandSwitchLeft;HandSwitchLeft=FMath::Max(0.f,HandSwitchLeft-Dt);
  if(Before>.24f&&HandSwitchLeft<=.24f)
  {
   bReloadPoseHeld=false;
   if(!bKnife&&!bEmptyHands){if(bMP7)MP7Ammo=Ammo;else RifleAmmo=Ammo;}
   bEmptyHands=PendingWeapon==0;bKnife=PendingWeapon==2;bMP7=PendingWeapon==3;
   Ammo=bMP7?MP7Ammo:RifleAmmo;
   M4Action=(!bMP7&&!bKnife&&!bEmptyHands)?1:0;M4PreviousAction=M4Action;M4Time=0;M4PreviousTime=0;M4Mix=1;M4Weight=M4Action?1.f:0.f;bM4Returning=false;
   MP7Action=bMP7?1:0;MP7ActionTime=0;MP7ActionBlend=bMP7?1.f:0.f;bMP7ActionReturning=false;
   AimAlpha=0;SightChangeLeft=0;bAimPoseReady=false;
   KnifeAction=bKnife?1:0;KnifePreviousAction=KnifeAction;KnifeTime=0;KnifePreviousTime=0;KnifeBlend=1;bKnifeContactDone=false;
   if(bKnife)++KnifeActionSerial;
   // SetSkeletalMesh evaluates the new animation instance immediately. Publish
   // draw and its initial sample first, so that evaluation cannot render hold.
   UpdateWeaponMeshes();
   UE_LOG(LogTemp,Display,TEXT("FIRELINE_HAND_MODE empty=%d knife=%d"),bEmptyHands,bKnife);
  }
  const float Phase=HandSwitchLeft>.24f?(1.f-(HandSwitchLeft-.24f)/(HandSwitchDuration-.24f))*.5f:1.f-HandSwitchLeft/.48f;
  const bool Authored=(bSwitchFromM4&&HandSwitchLeft>.24f)||(HandSwitchLeft<=.24f&&(bKnife||M4Action==1));
  HandLower=Authored?0.f:FMath::Square(FMath::Sin(PI*Phase));
  UpdateHandVisibility();
 }else {HandLower=0;if(QueuedWeapon>=0&&!bReloading){const int32 Next=QueuedWeapon;QueuedWeapon=-1;RequestWeapon(Next);}}
 if(bHandTest){HandTestTime=FMath::Min(8.f,HandTestTime+Dt);if(HandTestTime>=8){bHandTest=false;}}
 const auto* PC=Cast<APlayerController>(Controller);
 const bool Close=bEmptyHands&&!bHandTest&&PC&&PC->IsInputKeyDown(EKeys::LeftMouseButton);
 HandFistAlpha=FMath::FInterpConstantTo(HandFistAlpha,Close?1.f:0.f,Dt,4.f);
}
void ARangeCharacter::StartHandsAudit()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineHandsAudit")))return;
 if(auto* PC=Cast<APlayerController>(Controller)){DisableInput(PC);PC->SetIgnoreLookInput(true);PC->SetIgnoreMoveInput(true);}
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 auto Capture=[this](const TCHAR* Label){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/hands-%s.png"),Label),true,false);};
 Later(3,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);});
 Later(4,[Capture]{Capture(TEXT("rifle"));});
 Later(5,[this]{ToggleEmptyHands();});Later(6,[Capture]{Capture(TEXT("open"));});
 Later(6.5f,[this]{const int Before=Ammo;Fire();Reload();StartAim();UE_LOG(LogTemp,Display,TEXT("FIRELINE_HAND_GUARDS ammo_unchanged=%d reload_blocked=%d aim_blocked=%d"),Before==Ammo,!bReloading,!bAimHeld);TestFingers();});
 for(int I=0;I<5;++I)Later(7.f+I,[Capture,I]{Capture(*FString::Printf(TEXT("finger-%d"),I));});
 Later(12,[Capture]{Capture(TEXT("fist"));});Later(13.f,[Capture]{Capture(TEXT("wrist"));});Later(14.f,[Capture]{Capture(TEXT("wrist-reverse"));});
 Later(14.7f,[this]{TestFingers();});Later(14.95f,[this]{TestFingers();});
 Later(15,[this]{ToggleView();Controller->SetControlRotation(FRotator(-10,145,0));});Later(16,[Capture]{Capture(TEXT("third-empty"));});
 Later(17,[this]{EquipRifle();});Later(18,[Capture]{Capture(TEXT("third-rifle"));});
 Later(19,[this]{ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);StartAim();});Later(20,[Capture]{Capture(TEXT("aim"));});
 Later(21,[this]{EndAim();Ammo=29;Reload();});Later(22.1f,[Capture]{Capture(TEXT("reload"));});
 // Cover the charging-handle release and support-hand return, not just insertion.
 Later(22.95f,[Capture]{Capture(TEXT("reload-late"));});
 Later(23.15f,[Capture]{Capture(TEXT("reload-return"));});
 Later(23.38f,[Capture]{Capture(TEXT("reload-grip"));});
 Later(25,[this]{FFileHelper::SaveStringToFile(FString::Printf(TEXT("{\"empty\":%s,\"ammo\":%d,\"reload_complete\":%s,\"finger_bones\":%d}"),bEmptyHands?TEXT("true"):TEXT("false"),Ammo,bReloading?TEXT("false"):TEXT("true"),((Arms->DoesSocketExist(TEXT("Finger_thumb_01_R"))&&Arms->DoesSocketExist(TEXT("Finger_pinky_03_L")))||(Arms->DoesSocketExist(TEXT("Thumb1_R"))&&Arms->DoesSocketExist(TEXT("Pinky3_L"))))),*(FPaths::ProjectSavedDir()/TEXT("hands-audit.json")));FPlatformMisc::RequestExit(false);});
}
