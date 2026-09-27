#include "RangeGame.h"
#include "RangeWeaponTiming.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "Misc/FileHelper.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Engine/StaticMesh.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

void ARangeCharacter::InitializeMP7()
{
 RifleGunMesh=ViewGun->GetSkeletalMeshAsset();
 MP7BodyMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/MP7/SK_MP7Body.SK_MP7Body"));
 MP7ArmsMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/MP7/SK_MP7Arms.SK_MP7Arms"));
 MP7GunMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/MP7/SK_MP7.SK_MP7"));
 MP7MagazineMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Fireline/MP7/SM_MP7Magazine.SM_MP7Magazine"));
 bMP7Ready=MP7BodyMesh&&MP7ArmsMesh&&MP7GunMesh&&MP7MagazineMesh;
 for(const TCHAR* Name:{TEXT("Idle"),TEXT("Draw"),TEXT("Inspect"),TEXT("Reload"),TEXT("EmptyReload")})
  bMP7Ready&=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/MP7/A_MP7_%s.A_MP7_%s"),Name,Name))!=nullptr;
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_MP7_READY %d"),bMP7Ready);
}
void ARangeCharacter::EquipMP7(){RequestWeapon(3);}
void ARangeCharacter::CycleWeaponBack()
{
 const int Current=QueuedWeapon>=0?QueuedWeapon:HandSwitchLeft>0?PendingWeapon:WeaponSlot();
 RequestWeapon(Current==1?2:Current==2?(bMP7Ready?3:1):1);
}
void ARangeCharacter::CancelMP7Action()
{
 if(MP7ActionBlend>0)bMP7ActionReturning=true;
}
bool ARangeCharacter::IsMP7DrawLocked() const
{
 return bMP7&&MP7Action==1&&!bMP7ActionReturning&&MP7ActionTime+KINDA_SMALL_NUMBER<RangeWeaponTiming::MP7DrawReady;
}
bool ARangeCharacter::IsMP7ReloadRecovery() const
{
 return bMP7&&bReloading&&bReloadAmmoCommitted&&ReloadPosition+KINDA_SMALL_NUMBER>=RangeWeaponTiming::MP7ReloadReady(bEmptyReload);
}
void ARangeCharacter::ReleaseMP7ReloadRecovery()
{
 if(!IsMP7ReloadRecovery())return;
 bReloading=false;bReloadPoseHeld=false;ReloadLeft=0;ReloadLeadOut=0;
 // Keep the current pose sample for the graph's blend out; no fire lock here.
 UE_LOG(LogTemp,Display,TEXT("MP7_READY recovery_cancelled time=%.4f ammo=%d"),ReloadPosition,Ammo);
}
void ARangeCharacter::InspectMP7()
{
 if(!bMP7||bReloading||HandSwitchLeft>0||MP7Action==1)return;
 EndAim();MP7Action=2;MP7ActionTime=0;MP7ActionBlend=1;bMP7ActionReturning=false;
}
void ARangeCharacter::UpdateMP7(float Dt)
{
 // Both view meshes lead a gun mesh. Hide the old animated magazine (and
 // its cartridge) until the hand has fetched the replacement at the belt.
 const bool HideMag=bMP7&&(bReloading||bReloadPoseHeld)&&ReloadPosition>=RangeWeaponTiming::MP7MagazineDrop(bEmptyReload)&&ReloadPosition<RangeWeaponTiming::MP7MagazineReveal(bEmptyReload);
 for(auto* Mesh:{Arms.Get(),GetMesh()})for(const TCHAR* Bone:{TEXT("MP7_mag"),TEXT("MP7_bullet")})
  if(Mesh->DoesSocketExist(Bone)&&Mesh->IsBoneHiddenByName(Bone)!=HideMag)
  {if(HideMag)Mesh->HideBoneByName(Bone,PBO_None);else Mesh->UnHideBoneByName(Bone);}
 if(!bMP7)return;
 if(bMP7ActionReturning)
 {
  MP7ActionBlend=FMath::Max(0.f,MP7ActionBlend-Dt/.12f);
  if(MP7ActionBlend<=0){MP7Action=0;MP7ActionTime=0;bMP7ActionReturning=false;}
 }
 else if(MP7Action)
 {
  MP7ActionTime+=Dt;
  if(MP7ActionTime>=(MP7Action==1?50.f/60.f:168.f/60.f))
  {
   if(FParse::Param(FCommandLine::Get(),TEXT("FirelineContinuousContactStudy")))
   {
    // Preserve the final authored pose and use the same existing recovery
    // as an input interruption. An abrupt index/weight reset teleports arms.
    MP7ActionTime=MP7Action==1?50.f/60.f:168.f/60.f;
    bMP7ActionReturning=true;
   }
   else {MP7Action=0;MP7ActionTime=0;MP7ActionBlend=0;}
  }
 }
 // RMB held during the locked part becomes ADS as soon as the weapon is ready.
 if(MP7Action==1)if(auto* PC=Cast<APlayerController>(Controller))bAimHeld|=PC->IsInputKeyDown(EKeys::RightMouseButton);
 if(bAimHeld&&HandSwitchLeft<=0&&!IsMP7DrawLocked())
 {
  if(MP7Action==1)CancelMP7Action();
  ReleaseMP7ReloadRecovery();
 }
}
void ARangeCharacter::StartMP7Audit()
{
 bMP7Audit=FParse::Param(FCommandLine::Get(),TEXT("FirelineMP7Audit"));if(!bMP7Audit)return;
 if(auto* PC=Cast<APlayerController>(Controller))DisableInput(PC);
 check(bMP7Ready);
 FString Output=FPaths::ProjectSavedDir()/TEXT("MP7Audit");IFileManager::Get().MakeDirectory(*Output,true);
 struct FState{FString Checks;int32 Failures=0,StartShots=0,PoseSamples=0,PoseFailures=0,DrawSamples=0;float SampleAt=0;FString Rows=TEXT("time,slot,third,reload,action,action_time,aim,ammo,elbow_l,elbow_r,wrist_l,wrist_r\n");};
 auto State=MakeShared<FState>();
 auto Check=[State](FString Name,bool Pass){State->Failures+=!Pass;State->Checks+=Name+TEXT(": ")+(Pass?TEXT("PASS\n"):TEXT("FAIL\n"));UE_LOG(LogTemp,Display,TEXT("MP7_CHECK %s %s"),*Name,Pass?TEXT("PASS"):TEXT("FAIL"));};
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 auto Trigger=[this](bool Pressed){CastChecked<APlayerController>(Controller)->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::LeftMouseButton,Pressed?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));};
 auto Shot=[Output](FString Name){FScreenshotRequest::RequestScreenshot(Output/(Name+TEXT(".png")),true,false);};
 Later(1,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);EquipMP7();});
 Later(2.5f,[this,Shot,Check]{Check(TEXT("equip with correct asset and ammo"),bMP7&&Ammo==40&&ViewGun->GetSkeletalMeshAsset()==MP7GunMesh);Shot(TEXT("01-hip"));});
 Later(3,[this]{SetCoyoteSight(false);StartAim();});Later(3.7f,[Shot]{Shot(TEXT("02-iron"));});
 Later(4,[this]{SetCoyoteSight(true);});Later(4.7f,[Shot]{Shot(TEXT("03-coyote"));});
 Later(5,[this]{EndAim();Ammo=28;Reload();});Later(5.7f,[Shot]{Shot(TEXT("04-normal-out"));});Later(6.48f,[Shot]{Shot(TEXT("05-normal-insert"));});
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineMP7Capture")))
 {
  for(int I=0;I<24;++I)Later(5.1f+I*.1f,[Shot,I]{Shot(FString::Printf(TEXT("reload-frame-%02d"),I));});
  for(int I=0;I<27;++I)Later(7.65f+I*.1f,[Shot,I]{Shot(FString::Printf(TEXT("empty-frame-%02d"),I));});
 }
 Later(7.6f,[this,Check]{Check(TEXT("normal reload completes"),!bReloading&&Ammo==40);Ammo=0;Reload();});
 Later(8.2f,[Shot]{Shot(TEXT("06-empty-drop"));});Later(8.75f,[Shot]{Shot(TEXT("07-empty-insert"));});Later(9.28f,[Shot]{Shot(TEXT("08-bolt"));});
 Later(10.6f,[this,Check]{Check(TEXT("empty reload completes"),!bReloading&&Ammo==40);InspectMP7();});Later(11.5f,[Shot]{Shot(TEXT("09-inspect"));});
 Later(11.6f,[this,Check]{InspectMP7();Check(TEXT("inspect restarts"),MP7Action==2&&MP7ActionTime==0);});
 Later(12.4f,[this]{Fire();});Later(12.9f,[this,Check]{Check(TEXT("fire interrupts inspect"),MP7Action==0&&Ammo==39);Ammo=24;Reload();});
 Later(13.5f,[this]{Fire();});Later(14,[this,Check]{Check(TEXT("fire cancels reload without free ammo"),!bReloading&&Ammo==23);EquipRifle();});
 Later(14.8f,[this,Shot,Check]{Check(TEXT("M4 preserved"),!bMP7&&Ammo==30&&ViewGun->GetSkeletalMeshAsset()==RifleGunMesh);Shot(TEXT("10-m4-restored"));EquipKnife();});
 Later(15.8f,[this,Check]{Check(TEXT("knife preserved"),bKnife);EquipMP7();});
 Later(17,[this,Check]{Check(TEXT("MP7 ammo preserved"),bMP7&&Ammo==23);ToggleView();Controller->SetControlRotation(FRotator(-8,150,0));});
 Later(18,[Shot]{Shot(TEXT("11-third-hold"));});Later(18.3f,[this]{StartCrouch();});Later(19,[this,Shot,Check]{Check(TEXT("MP7 third-person crouch active"),bIsCrouched);Shot(TEXT("12-third-crouch"));});
 Later(19.3f,[this]{EndCrouch();Ammo=0;Reload();});Later(20.8f,[Shot]{Shot(TEXT("13-third-reload"));});
 Later(22.4f,[this]{ToggleView();Controller->SetControlRotation(FRotator(35,120,0));StartAim();});Later(23.2f,[Shot]{Shot(TEXT("14-aim-up"));});
 Later(24,[this]{EndAim();Controller->SetControlRotation(FRotator::ZeroRotator);});
 Later(24.2f,[this,State,Trigger]{Ammo=40;State->StartShots=Shots;Trigger(true);});
 Later(25.4f,[this,State,Check,Trigger]{Trigger(false);Check(TEXT("1000 RPM sustained fire"),Shots-State->StartShots>=19&&Shots-State->StartShots<=21);});
 Later(25.8f,[this]{Ammo=1;Fire();});
 Later(26.1f,[this,Check]{Check(TEXT("last round starts empty reload"),Ammo==0&&bReloading&&bEmptyReload);});
 Later(29,[this,Check]{Check(TEXT("automatic reload fills MP7 capacity"),Ammo==40&&!bReloading);CycleWeapon();});
 Later(30,[this,Check]{Check(TEXT("wheel forward MP7 to knife"),bKnife);CycleWeaponBack();});
 Later(31.3f,[this,Check]{Check(TEXT("wheel backward knife to MP7"),bMP7&&Ammo==40);EquipRifle();});
 Later(32.2f,[this,Check]{Check(TEXT("M4 ammunition independent"),!bMP7&&Ammo==30);CycleWeapon();CycleWeapon();CycleWeaponBack();});
 Later(34.2f,[this,Check]{Check(TEXT("rapid wheel resolves final MP7 selection"),bMP7&&HandSwitchLeft<=0&&QueuedWeapon<0);});
 auto* Reference=NewObject<USkeletalMeshComponent>(this);AddInstanceComponent(Reference);Reference->SetSkeletalMesh(MP7ArmsMesh);Reference->SetVisibility(false);Reference->SetHiddenInGame(true);Reference->SetCollisionEnabled(ECollisionEnabled::NoCollision);Reference->RegisterComponent();Reference->SetComponentTickEnabled(false);
 TArray<UAnimSequence*> Clips;for(const TCHAR* Name:{TEXT("Idle"),TEXT("Draw"),TEXT("Inspect"),TEXT("Reload"),TEXT("EmptyReload")})Clips.Add(LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/MP7/A_MP7_%s.A_MP7_%s"),Name,Name)));
 FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this,State,Reference,Clips](UWorld* World,ELevelTick,float Dt)
 {
  if(World!=GetWorld()||!bMP7)return;
  if(!bMP7ActionReturning&&!bReloadPoseHeld&&ReloadFireReturnLeft<=0&&(MP7Action!=2||MP7ActionTime>0)&&(!bReloading||ReloadPosition>.3f))
  {
   auto* Actual=bThirdPerson?GetMesh():Arms.Get();
   const int Clip=MP7Action?MP7Action:bReloading?(bEmptyReload?4:3):0;
   const float T=MP7Action?MP7ActionTime:bReloading?ReloadPosition:0;
   auto Error=[&](float Time)
   {
    Reference->PlayAnimation(Clips[Clip],false);Reference->SetPosition(Time,false);Reference->TickAnimation(0,false);Reference->RefreshBoneTransforms();
    // Full-body presentation deliberately retargets elbows and the shared
    // assembly; comparing them to FP torso-local coordinates is invalid.
    // FP retains its exact clip check. TP checks the authored hand/mechanical
    // contacts in receiver space, measured back in component centimetres.
    const FName Frame=bThirdPerson?TEXT("MP7_body"):TEXT("Torso");
    const FTransform ABase=Actual->GetSocketTransform(Frame,RTS_Component),RBase=Reference->GetSocketTransform(Frame,RTS_Component);float Max=0;
    for(const TCHAR* Name:{TEXT("LowerArm_L"),TEXT("LowerArm_R"),TEXT("DJ_wrist_L"),TEXT("DJ_wrist_R"),TEXT("MP7_body"),TEXT("MP7_mag")})
    {
     if(bThirdPerson&&(FString(Name)==TEXT("LowerArm_L")||FString(Name)==TEXT("LowerArm_R")))continue;
     const FVector A=ABase.InverseTransformPosition(Actual->GetSocketTransform(Name,RTS_Component).GetLocation()),B=RBase.InverseTransformPosition(Reference->GetSocketTransform(Name,RTS_Component).GetLocation());Max=FMath::Max(Max,bThirdPerson?ABase.TransformVector(A-B).Size():FVector::Distance(A,B));
    }
    return Max;
   };
   const float E=FMath::Min(Error(T),Error(FMath::Max(0.f,T-Dt)));
   ++State->PoseSamples;if(Clip==1)++State->DrawSamples;
   if(E>.15f){++State->PoseFailures;if(State->PoseFailures<8)UE_LOG(LogTemp,Warning,TEXT("MP7_POSE clip=%d time=%.4f third=%d error_cm=%.4f"),Clip,T,bThirdPerson,E);}
  }
  if(HandSwitchLeft>0||GetGameTimeSinceCreation()-State->SampleAt<.033f)return;
  State->SampleAt=GetGameTimeSinceCreation();auto* M=bThirdPerson?GetMesh():Arms.Get();
  float Below[2],Wrist[2];int I=0;
  for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
  {
   auto P=[M,Side](const TCHAR* B){return M->GetSocketTransform(*FString::Printf(TEXT("%s_%s"),B,Side),RTS_Component).GetLocation();};
   const FVector S=P(TEXT("UpperArm")),E=P(TEXT("LowerArm")),W=P(TEXT("DJ_wrist")),F=P(TEXT("DJ_middle_01"));
   Below[I]=S.Z-E.Z;Wrist[I++]=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp((W-E).GetSafeNormal().Dot((F-W).GetSafeNormal()),-1.,1.)));
  }
  State->Rows+=FString::Printf(TEXT("%.3f,%d,%d,%d,%d,%.3f,%.3f,%d,%.3f,%.3f,%.3f,%.3f\n"),State->SampleAt,WeaponSlot(),bThirdPerson,bReloading,MP7Action,MP7ActionTime,AimAlpha,Ammo,Below[0],Below[1],Wrist[0],Wrist[1]);
 });
 Later(35,[State,Output,Check]{Check(TEXT("FP matches source clips; TP preserves receiver-space hand and mechanical contacts"),State->PoseSamples>100&&State->DrawSamples>0&&State->PoseFailures==0);UE_LOG(LogTemp,Display,TEXT("MP7_POSE samples=%d draw=%d failures=%d"),State->PoseSamples,State->DrawSamples,State->PoseFailures);FFileHelper::SaveStringToFile(State->Rows,*(Output/TEXT("samples.csv")));FFileHelper::SaveStringToFile(State->Checks,*(Output/TEXT("checks.txt")));UE_LOG(LogTemp,Display,TEXT("MP7_AUDIT failures=%d"),State->Failures);FPlatformMisc::RequestExit(false);});
}

void URangeAssetTools::PrepareMP7()
{
#if WITH_EDITOR
 auto* Body=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/MP7/SK_MP7Body.SK_MP7Body"));check(Body);
 auto* Skeleton=Body->GetSkeleton();
 Skeleton->AddCompatibleSkeleton(LoadObject<USkeleton>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction_Skeleton.SK_RyanAction_Skeleton")));
 const auto& Ref=Body->GetRefSkeleton();TArray<FTransform> Bind=Ref.GetRefBonePose();
 for(int I=1;I<Bind.Num();++I)Bind[I]=Bind[I]*Bind[Ref.GetParentIndex(I)];
 for(int I=0;I<Skeleton->GetReferenceSkeleton().GetNum();++I)
 {
  const FName Bone=Skeleton->GetReferenceSkeleton().GetBoneName(I);
  Skeleton->SetBoneTranslationRetargetingMode(I,EBoneTranslationRetargetingMode::Animation);
  const FName Name(*FString::Printf(TEXT("Required_MP7_%s"),*Bone.ToString()));
  if(!Skeleton->FindSocket(Name))
  {auto* S=NewObject<USkeletalMeshSocket>(Skeleton);S->SocketName=Name;S->BoneName=Bone;S->bForceAlwaysAnimated=true;Skeleton->Sockets.Add(S);}
 }
 const FVector Muzzle=Bind[Ref.FindBoneIndex(TEXT("MP7_muzzle"))].GetLocation();
 const FVector Forward=Bind[Ref.FindBoneIndex(TEXT("MP7_frontsight"))].GetLocation()-Bind[Ref.FindBoneIndex(TEXT("MP7_rearsight"))].GetLocation();
 const FVector Up=Bind[Ref.FindBoneIndex(TEXT("MP7_sightup"))].GetLocation()-Bind[Ref.FindBoneIndex(TEXT("MP7_rearsight"))].GetLocation();
 const FTransform Local=FTransform(FRotationMatrix::MakeFromXZ(Forward,Up).ToQuat(),Muzzle).GetRelativeTransform(Bind[Ref.FindBoneIndex(TEXT("MP7_body"))]);
 auto* Flash=Skeleton->FindSocket(TEXT("MP7_Flash"));
 if(!Flash){Flash=NewObject<USkeletalMeshSocket>(Skeleton);Flash->SocketName=TEXT("MP7_Flash");Skeleton->Sockets.Add(Flash);}
 Flash->BoneName=TEXT("MP7_body");Flash->RelativeLocation=Local.GetLocation();Flash->RelativeRotation=Local.Rotator();Flash->bForceAlwaysAnimated=true;
 TArray<UObject*> Save{Skeleton};
 for(const TCHAR* Name:{TEXT("SK_MP7Body"),TEXT("SK_MP7Arms"),TEXT("SK_MP7")})
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*FString::Printf(TEXT("/Game/Fireline/MP7/%s.%s"),Name,Name));check(Mesh);
  for(int L=0;L<Mesh->GetLODNum();++L)Mesh->GetLODInfo(L)->bAllowCPUAccess=true;
  Mesh->SetPositiveBoundsExtension(FVector(80));Mesh->SetNegativeBoundsExtension(FVector(80));Mesh->PostEditChange();Save.Add(Mesh);
 }
 for(auto* Asset:Save)
 {
  Asset->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
 }
#endif
}
