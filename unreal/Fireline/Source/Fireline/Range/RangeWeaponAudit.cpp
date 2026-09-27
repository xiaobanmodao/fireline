#include "RangeGame.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "InputKeyEventArgs.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"

void ARangeCharacter::StartWeaponAudit()
{
 bWeaponAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponAudit"));
 if(!bWeaponAudit)return;
 WeaponAuditSamples=TEXT("time,aim,reloading,crouched,ammo,head_z,rear_y,rear_z,front_y,front_z,max_part_cm,max_part_deg,worst_bone,meshes,hip_bore_y,hip_bore_z,hip_roll_deg\n");
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 auto Key=[this](FKey K,EInputEvent E){auto* PC=CastChecked<APlayerController>(Controller);PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,E,FPlatformTime::Cycles64()));};
 Later(3,[this]{ToggleView();Boom->TargetArmLength=230;Controller->SetControlRotation(FRotator(-8,155,0));});
 Later(5,[Key]{Key(EKeys::C,IE_Pressed);});
 Later(6.5,[Key]{Key(EKeys::C,IE_Released);});
 Later(10,[Key]{Key(EKeys::RightMouseButton,IE_Pressed);});
 Later(11.5,[Key]{Key(EKeys::RightMouseButton,IE_Released);});
 Later(12,[this]{ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);});
 Later(14,[Key]{Key(EKeys::RightMouseButton,IE_Pressed);});
 Later(15.5,[this]{Fire();});
 Later(17,[Key]{Key(EKeys::C,IE_Pressed);});
 Later(18.5,[Key]{Key(EKeys::C,IE_Released);});
 Later(19,[this]{Reload();});
 Later(23,[Key]{Key(EKeys::RightMouseButton,IE_Released);});
 Later(25,[this]{
  TActorIterator<ARangeTarget> It(GetWorld());
  if(It)
  {
   auto* Cam=GetWorld()->SpawnActor<ACameraActor>();
   const FVector Target=It->GetActorLocation()+FVector(0,0,38);
   const FVector Pos=Target+It->GetActorForwardVector()*150+It->GetActorRightVector()*90+FVector(0,0,25);
   Cam->SetActorLocationAndRotation(Pos,(Target-Pos).Rotation());Cam->GetCameraComponent()->SetFieldOfView(65);
   CastChecked<APlayerController>(Controller)->SetViewTarget(Cam);
  }
 });
 // Independently apply the receiver's skinning matrix to the mesh-derived
 // landmarks. Bone/socket alignment alone can hide a wrong bind pose.
 Later(15.1,[this]{
  auto* Gun=ViewGun.Get();const auto& Ref=Gun->GetSkeletalMeshAsset()->GetRefSkeleton();
  TArray<FTransform> Bind;for(int I=0;I<Ref.GetNum();++I){const int P=Ref.GetParentIndex(I);Bind.Add(P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bind[P]);}
  TArray<FMatrix44f> Mat;Gun->GetCurrentRefToLocalMatrices(Mat,0);
  FString Data=TEXT("landmark,camera_x,camera_y,camera_z\n");
  for(FName N:{FName("M4_rearsight"),FName("M4_frontsight")})
  {
   const FVector3f BindPoint(Bind[Ref.FindBoneIndex(N)].GetLocation());
   const FVector P(Mat[Ref.FindBoneIndex(TEXT("M4_body"))].TransformPosition(BindPoint));
   const FVector C=Camera->GetComponentTransform().InverseTransformPosition(Gun->GetComponentTransform().TransformPosition(P));
   Data+=FString::Printf(TEXT("%s,%f,%f,%f\n"),*N.ToString(),C.X,C.Y,C.Z);
  }
  FFileHelper::SaveStringToFile(Data,*(FPaths::ProjectSavedDir()/TEXT("sight-skin.csv")));
 });
 const TArray<float> Captures={4,4.5f,6,8,11,13,15,16,18,20.2f,22.5f,24,26};
 for(int I=0;I<Captures.Num();++I)
  Later(Captures[I],[this,I]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/weapon-%02d.png"),I),true,false);});
 Later(28,[this]{
  FFileHelper::SaveStringToFile(WeaponAuditSamples,*(FPaths::ProjectSavedDir()/TEXT("weapon-audit.csv")));
  UE_LOG(LogTemp,Display,TEXT("FIRELINE_WEAPON_AUDIT ammo=%d reload_complete=%d"),Ammo,!bReloading);
  FPlatformMisc::RequestExit(false);
 });
}

void ARangeCharacter::TickWeaponAudit(float Dt)
{
 const float T=GetGameTimeSinceCreation();
 if(T>=7 && T<9)AddMovementInput(GetActorRightVector(),.65f);
 if(T<3 || T>=28)return;
 float Error=0,Angle=0;FString Worst=TEXT("none");int Count=0;
 static TMap<TWeakObjectPtr<USkeletalMeshComponent>,FTransform> CalibratedGrips;
 auto Check=[&](USkeletalMeshComponent* Gun)
 {
  ++Count;const FReferenceSkeleton& Ref=Gun->GetSkeletalMeshAsset()->GetRefSkeleton();
  for(int I=0;I<Ref.GetNum();++I)
  {
   const FName Name=Ref.GetBoneName(I);if(!Name.ToString().StartsWith(TEXT("M4_")))continue;
   int Parent=Ref.GetParentIndex(I);if(Parent<0)continue;
   // Drazen uses the accepted right glove as its physical assembly parent.
   // Test that grip in both older diagnostic assets and the current hierarchy.
   if(Name==TEXT("M4_body")){const int Native=Ref.FindBoneIndex(TEXT("Palm_R"));const int Grip=Native!=INDEX_NONE?Native:Ref.FindBoneIndex(TEXT("G_UpperHand_R"));if(Grip!=INDEX_NONE)Parent=Grip;}
   const FTransform Local=Gun->GetSocketTransform(Name,RTS_Component).GetRelativeTransform(Gun->GetSocketTransform(Ref.GetBoneName(Parent),RTS_Component));
   FTransform Expected=Ref.GetRefBonePose()[I];
   // The accepted animation authors adjusted the grip from the T-pose bind.
   // For this assembly root, require no drift from its first held pose;
   // every mechanical child still uses its independent authored bind transform.
   if(Name==TEXT("M4_body")&&(Ref.GetBoneName(Parent)==TEXT("G_UpperHand_R")||Ref.GetBoneName(Parent)==TEXT("Palm_R")))
   {
    const TWeakObjectPtr<USkeletalMeshComponent> Key(Gun);
    if(!CalibratedGrips.Contains(Key))CalibratedGrips.Add(Key,Local);
    Expected=CalibratedGrips[Key];
   }
   const FVector ParentScale=Gun->GetSocketTransform(Ref.GetBoneName(Parent),RTS_Component).GetScale3D()*Gun->GetComponentScale();
   const float D=((Local.GetLocation()-Expected.GetLocation())*ParentScale).Size();
   // atan2(delta.xyz, delta.w) is scale invariant and gives zero for
   // identical quaternions; acos(dot) amplified approximate normalization error.
   const FQuat Delta=Local.GetRotation()*Expected.GetRotation().Inverse();
   const float A=FMath::RadiansToDegrees(2.0*FMath::Atan2(FVector(Delta.X,Delta.Y,Delta.Z).Size(),FMath::Abs(Delta.W)));
   if(A>.1f){static TSet<FString> Reported;const FString Key=Gun->GetName()+TEXT(":")+Name.ToString();if(!Reported.Contains(Key)){Reported.Add(Key);UE_LOG(LogTemp,Warning,TEXT("FIRELINE_RIGID_ROTATION %s angle=%f local=%s expected=%s"),*Key,A,*Local.GetRotation().ToString(),*Expected.GetRotation().ToString());}}
   if(D>Error){Error=D;Worst=Gun->GetName()+TEXT(":")+Name.ToString();}Angle=FMath::Max(Angle,A);
  }
 };
 if(!bReloading){Check(BodyGun);Check(ViewGun);}
 for(TActorIterator<ARangeTarget> It(GetWorld());It;++It)Check(It->Gun);
 const FTransform View=Camera->GetComponentTransform();
 const FVector Rear=View.InverseTransformPosition(ViewGun->GetSocketLocation(TEXT("M4_rearsight")));
 const FVector Front=View.InverseTransformPosition(ViewGun->GetSocketLocation(TEXT("M4_frontsight")));
 const FVector Muzzle=View.InverseTransformPosition(ViewGun->GetSocketLocation(TEXT("M4_muzzle")));
 const FVector Direction=(Front-Rear).GetSafeNormal();
 const FVector Intersection=Muzzle+Direction*((2000.f-Muzzle.X)/FMath::Max(Direction.X,.001f));
 const FVector Up=(View.InverseTransformPosition(ViewGun->GetSocketLocation(TEXT("M4_sightup")))-Rear).GetSafeNormal();
 const FQuat Frame=FRotationMatrix::MakeFromXZ(Direction,Up).ToQuat();
 const float Head=GetMesh()->GetSocketTransform(TEXT("Head"),RTS_Component).GetLocation().Z;
 WeaponAuditSamples+=FString::Printf(TEXT("%.5f,%.4f,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%s,%d,%.6f,%.6f,%.6f\n"),
  T,AimAlpha,bReloading,bIsCrouched,Ammo,Head,Rear.Y,Rear.Z,Front.Y,Front.Z,Error,Angle,*Worst,Count,Intersection.Y,Intersection.Z,Frame.Rotator().Roll);
}
