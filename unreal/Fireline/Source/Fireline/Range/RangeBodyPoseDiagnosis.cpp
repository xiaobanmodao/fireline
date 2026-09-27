// Samples the evaluated body, including the runtime body pose control.
// Geometry acceptance still requires inspecting the recorded native views.
#include "RangeGame.h"
#include "RangeAnimInstance.h"
#include "RangePlayerController.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "DrawDebugHelpers.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "InputKeyEventArgs.h"

void ARangeCharacter::StartBodyPoseDiagnosis()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineBodyPoseDiagnosisAudit")))return;
 FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);
 struct FState{FString Case=TEXT("warmup"),Rows=TEXT("time,case,slot,aim,control_pitch,reload,side,shoulder_x,shoulder_y,shoulder_z,elbow_x,elbow_y,elbow_z,wrist_x,wrist_y,wrist_z,upper_cm,forearm_cm,elbow_bend,wrist_angle,clavicle_gap_cm,bore_yaw,bore_pitch,bore_roll,source_contact_cm,forearm_alias_cm,elbow_body_penetration_cm,min_elbow_torso_side_cm\n");FVector Offset=FVector(180,180,35),Origin;bool Bones=false;};
 auto S=MakeShared<FState>();S->Origin=GetActorLocation();auto* Cam=GetWorld()->SpawnActor<ACameraActor>();Cam->GetCameraComponent()->SetFieldOfView(48);
 const FString Out=FPaths::ProjectSavedDir()/TEXT("BodyPoseDiagnosis");IFileManager::Get().MakeDirectory(*Out,true);IFileManager::Get().MakeDirectory(*(Out/TEXT("motion")),true);
 auto Later=[this](float T,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),T,false);};
 auto Capture=[Out,S](FString Label){S->Case=Label;FScreenshotRequest::RequestScreenshot(Out/(Label+TEXT(".png")),true,false);};
 auto Key=[this](FKey K,bool Down){CastChecked<ARangePlayerController>(Controller)->InjectGameplayAuditKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));};
 Later(.7f,[this,Cam]{if(!bThirdPerson)ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);CastChecked<APlayerController>(Controller)->SetViewTarget(Cam);});
 // Both normal firearm holds are captured from the same three body-relative views.
 for(int Weapon=0;Weapon<2;++Weapon)
 {
  const float T=1.5f+Weapon*11.f;const FString Prefix=Weapon?TEXT("mp7"):TEXT("m4");
  Later(T,[this,Weapon,S]{SetActorLocation(S->Origin);EndAim();EndCrouch();if(Weapon)EquipMP7();else EquipRifle();Controller->SetControlRotation(FRotator::ZeroRotator);S->Bones=false;});
  Later(T+1.2f,[S]{S->Offset=FVector(195,75,25);});Later(T+1.4f,[Capture,Prefix]{Capture(Prefix+TEXT("-hold-front"));});
  Later(T+1.6f,[S]{S->Bones=true;});Later(T+1.7f,[Capture,Prefix]{Capture(Prefix+TEXT("-hold-front-bones"));});
  Later(T+1.9f,[S]{S->Bones=false;S->Offset=FVector(15,-220,20);});Later(T+2.1f,[Capture,Prefix]{Capture(Prefix+TEXT("-hold-left"));});
  Later(T+2.3f,[S]{S->Bones=true;});Later(T+2.4f,[Capture,Prefix]{Capture(Prefix+TEXT("-hold-left-bones"));});
  Later(T+2.6f,[S]{S->Bones=false;S->Offset=FVector(-180,130,35);});Later(T+2.8f,[Capture,Prefix]{Capture(Prefix+TEXT("-hold-back"));});
  Later(T+3,[this,S]{StartAim();S->Offset=FVector(15,-220,20);});Later(T+3.4f,[Capture,Prefix]{Capture(Prefix+TEXT("-ads-level"));});
  Later(T+3.6f,[this]{Controller->SetControlRotation(FRotator(35,0,0));});Later(T+4,[Capture,Prefix]{Capture(Prefix+TEXT("-ads-up"));});
  Later(T+4.2f,[this]{Controller->SetControlRotation(FRotator(-35,0,0));});Later(T+4.6f,[Capture,Prefix]{Capture(Prefix+TEXT("-ads-down"));});
  Later(T+4.8f,[this]{EndAim();Controller->SetControlRotation(FRotator::ZeroRotator);StartCrouch();});Later(T+5.2f,[Capture,Prefix]{Capture(Prefix+TEXT("-crouch"));});
  Later(T+5.5f,[this]{EndCrouch();Ammo=12;Reload();});
  for(int I=0;I<7;++I)Later(T+5.6f+I*.25f,[Capture,Prefix,I]{Capture(Prefix+FString::Printf(TEXT("-reload-%02d"),I));});
  Later(T+8.2f,[Key]{Key(EKeys::W,true);});Later(T+8.6f,[Capture,Prefix]{Capture(Prefix+TEXT("-walk"));});Later(T+8.9f,[Key]{Key(EKeys::W,false);});
 }
 Later(23.7f,[this,S]{SetActorLocation(S->Origin);EndAim();EquipKnife();S->Bones=false;Controller->SetControlRotation(FRotator::ZeroRotator);});
 Later(25,[S]{S->Offset=FVector(195,75,25);});Later(25.2f,[Capture]{Capture(TEXT("knife-hold-front"));});
 Later(25.4f,[S]{S->Bones=true;});Later(25.5f,[Capture]{Capture(TEXT("knife-hold-front-bones"));});
 Later(25.7f,[S]{S->Bones=false;S->Offset=FVector(15,-220,20);});Later(25.9f,[Capture]{Capture(TEXT("knife-hold-left"));});
 Later(26.1f,[S]{S->Bones=true;});Later(26.2f,[Capture]{Capture(TEXT("knife-hold-left-bones"));});
 Later(26.4f,[this,S]{S->Bones=false;StartCrouch();});Later(26.9f,[Capture]{Capture(TEXT("knife-crouch"));});
 Later(27.1f,[this]{EndCrouch();StartKnifeAction(2);});
 for(int I=0;I<9;++I)Later(27.2f+I*.45f,[Capture,I]{Capture(FString::Printf(TEXT("knife-inspect-%02d"),I));});
 // Wide captures cover extreme view angles and actual movement transitions.
 for(int Weapon=0;Weapon<2;++Weapon){
  const float T=32.f+Weapon*10.f;const FString Prefix=Weapon?TEXT("mp7"):TEXT("m4");
  Later(T,[this,Weapon,S]{SetActorLocation(S->Origin);EndAim();EndCrouch();if(Weapon)EquipMP7();else EquipRifle();S->Offset=FVector(35,-340,0);S->Bones=false;});
  Later(T+1.3f,[this]{StartAim();Controller->SetControlRotation(FRotator(89,0,0));});
  Later(T+1.9f,[Capture,Prefix]{Capture(Prefix+TEXT("-ads-up-89"));});
  Later(T+2.1f,[this]{Controller->SetControlRotation(FRotator(-89,0,0));});
  Later(T+2.7f,[Capture,Prefix]{Capture(Prefix+TEXT("-ads-down-89"));});
  Later(T+2.9f,[this]{EndAim();Controller->SetControlRotation(FRotator::ZeroRotator);Ammo=0;Reload();});
  for(int I=0;I<4;++I)Later(T+3.2f+I*.45f,[Capture,Prefix,I]{Capture(Prefix+FString::Printf(TEXT("-empty-reload-%02d"),I));});
  Later(T+6.f,[Key]{Key(EKeys::W,true);Key(EKeys::LeftShift,true);});
  Later(T+6.5f,[Key]{Key(EKeys::C,true);});
  Later(T+6.7f,[Capture,Prefix]{Capture(Prefix+TEXT("-slide"));});
  Later(T+7.2f,[Key]{Key(EKeys::C,false);Key(EKeys::W,false);Key(EKeys::LeftShift,false);Key(EKeys::SpaceBar,true);});
  Later(T+7.5f,[Capture,Prefix]{Capture(Prefix+TEXT("-jump"));});
  Later(T+7.8f,[Key]{Key(EKeys::SpaceBar,false);});
 }
 FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this,S,Cam,Out](UWorld* W,ELevelTick,float)
 {
  if(W!=GetWorld()||GetGameTimeSinceCreation()<1)return;
  const FVector Focus=GetActorLocation()+FVector(0,0,35);const FVector Pos=Focus+GetActorRotation().RotateVector(S->Offset);Cam->SetActorLocationAndRotation(Pos,(Focus-Pos).Rotation());
  if(FParse::Param(FCommandLine::Get(),TEXT("FirelineBodyPoseMotionCapture"))&&!FScreenshotRequest::IsScreenshotRequested()){
   const double T=GetGameTimeSinceCreation();const int Frame=FMath::RoundToInt(T*60);const bool Motion=(T>=7&&T<=9)||(T>=18&&T<=21)||(T>=27.1&&T<=31.4);
   if(Motion&&Frame%3==0)FScreenshotRequest::RequestScreenshot(Out/FString::Printf(TEXT("motion/%d-%05d.png"),WeaponSlot(),Frame),true,false);
  }
  auto* M=GetMesh();const FTransform Body=GetActorTransform();
  auto Point=[M,&Body](FName N){return Body.InverseTransformPosition(M->GetSocketLocation(N));};
  FRotator Bore=FRotator::ZeroRotator;
  if(!bKnife){const FVector Rear=Point(WeaponBone(TEXT("rearsight"))),Front=Point(WeaponBone(TEXT("frontsight"))),Up=Point(WeaponBone(TEXT("sightup")))-Rear;Bore=FRotationMatrix::MakeFromXZ(Front-Rear,Up).Rotator();}
  const auto& Ref=M->GetSkeletalMeshAsset()->GetRefSkeleton();TArray<FTransform> Bind=Ref.GetRefBonePose();for(int I=1;I<Bind.Num();++I)Bind[I]=Bind[I]*Bind[Ref.GetParentIndex(I)];
  for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
  {
   auto Name=[Side](const TCHAR* N){return FName(*FString::Printf(TEXT("%s_%s"),N,Side));};
   const FVector A=Point(Name(TEXT("UpperArm"))),E=Point(Name(TEXT("LowerArm"))),H=Point(Name(TEXT("DJ_wrist"))),P=Point(Name(TEXT("DJ_middle_01")));
   auto Angle=[](FVector X,FVector Y){return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(X.GetSafeNormal().Dot(Y.GetSafeNormal()),-1.,1.)));};
   const int UI=Ref.FindBoneIndex(Name(TEXT("UpperArm"))),CI=Ref.FindBoneIndex(Name(TEXT("Shoulder")));
   const FVector Expected=M->GetSocketTransform(Name(TEXT("Shoulder")),RTS_World).TransformPosition(Bind[CI].InverseTransformPosition(Bind[UI].GetLocation()));
   const float Gap=FVector::Distance(Expected,M->GetSocketLocation(Name(TEXT("UpperArm"))));
   const FVector PoseMetrics=CastChecked<URangeAnimInstance>(M->GetAnimInstance())->GetBodyPoseAuditMetrics();
   const auto LowerName=Name(TEXT("LowerArm")),AliasName=Name(TEXT("DJ_forearm"));
   const int LI=Ref.FindBoneIndex(LowerName),AI=Ref.FindBoneIndex(AliasName);
   const FVector AliasExpected=M->GetSocketTransform(LowerName,RTS_Component).TransformPosition(Bind[LI].InverseTransformPosition(Bind[AI].GetLocation()));
   const float AliasGap=FVector::Distance(AliasExpected,M->GetSocketTransform(AliasName,RTS_Component).GetLocation());
   S->Rows+=FString::Printf(TEXT("%.4f,%s,%d,%.4f,%.3f,%d,%s,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.4f,%.4f,%.4f,%.4f\n"),GetGameTimeSinceCreation(),*S->Case,WeaponSlot(),AimAlpha,Controller->GetControlRotation().GetNormalized().Pitch,bReloading,Side,A.X,A.Y,A.Z,E.X,E.Y,E.Z,H.X,H.Y,H.Z,(E-A).Size(),(H-E).Size(),Angle(E-A,H-E),Angle(H-E,P-H),Gap,Bore.Yaw,Bore.Pitch,Bore.Roll,PoseMetrics.X,AliasGap,PoseMetrics.Y,PoseMetrics.Z);
   if(S->Bones)
   {
    const FColor Color=FString(Side)==TEXT("L")?FColor::Yellow:FColor::Magenta;
    for(auto Pair:{TPair<FVector,FVector>(A,E),TPair<FVector,FVector>(E,H),TPair<FVector,FVector>(H,P)})DrawDebugLine(W,Body.TransformPosition(Pair.Key),Body.TransformPosition(Pair.Value),Color,false,0,1,2.5f);
    for(auto V:{A,E,H})DrawDebugSphere(W,Body.TransformPosition(V),1.5,8,Color,false,0,1,1);
   }
  }
 });
 Later(52,[S,Out]{FFileHelper::SaveStringToFile(S->Rows,*(Out/TEXT("samples.csv")));UE_LOG(LogTemp,Display,TEXT("BODY_POSE_DIAGNOSIS complete; observations only, no anatomy acceptance checks"));FPlatformMisc::RequestExit(false);});
}
