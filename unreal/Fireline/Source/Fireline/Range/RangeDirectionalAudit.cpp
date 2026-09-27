#include "RangeGame.h"
#include "RangeAnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/App.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "TimerManager.h"

void ARangeCharacter::StartDirectionalAudit()
{
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineDirectionalGameplayAudit")))
 {
  check(FParse::Param(FCommandLine::Get(),TEXT("FirelineDirectionalLocomotion")));
  FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);
  struct FGameReview{FTimerHandle Timer;float Time=0;int Group=-1,Frame=-1;bool Shot=false;FString CSV=TEXT("group,time,speed,direction,root_x,root_y,root_scale,hip_l_z,knee_l_z,foot_l_z,hip_r_z,knee_r_z,foot_r_z,weapon\n");};
  auto S=MakeShared<FGameReview>();auto* PC=CastChecked<APlayerController>(Controller);PC->SetIgnoreLookInput(true);
  auto* Floor=GetWorld()->SpawnActor<AStaticMeshActor>();auto* FloorMesh=Floor->GetStaticMeshComponent();FloorMesh->SetMobility(EComponentMobility::Movable);
  FloorMesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube")));FloorMesh->SetCollisionProfileName(TEXT("BlockAll"));
  Floor->SetActorLocation(FVector(-6000,0,-15));Floor->SetActorScale3D(FVector(50,50,.3));
  auto* View=GetWorld()->SpawnActor<ACameraActor>();
  USkeletalMeshComponent* SourceReference=nullptr;
  if(FParse::Param(FCommandLine::Get(),TEXT("FirelineSourceBodyReference")))
  {
   SourceReference=NewObject<USkeletalMeshComponent>(this);
   SourceReference->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")));
   SourceReference->SetCollisionEnabled(ECollisionEnabled::NoCollision);SourceReference->RegisterComponent();SourceReference->SetWorldScale3D(FVector(.94));
  }
  PC->ConsoleCommand(TEXT("r.MotionBlurQuality 0"));PC->ConsoleCommand(TEXT("r.AntiAliasingMethod 1"));
  const bool Trace=FParse::Param(FCommandLine::Get(),TEXT("FirelineFullBodyTrace"));
  const bool Boundary=FParse::Param(FCommandLine::Get(),TEXT("FirelineDirectionBoundaryAudit"));
  const bool ArmedStudy=FParse::Param(FCommandLine::Get(),TEXT("FirelineArmedLocomotionStudy"));
  const bool KneeStudy=ArmedStudy||FParse::Param(FCommandLine::Get(),TEXT("FirelineKneeContinuityStudy"));
  const bool GaitStudy=KneeStudy||FParse::Param(FCommandLine::Get(),TEXT("FirelineGaitContinuityStudy"));
  const bool LegStudy=GaitStudy||FParse::Param(FCommandLine::Get(),TEXT("FirelineLegDirectionStudy"));
  const bool BodyStudy=LegStudy||FParse::Param(FCommandLine::Get(),TEXT("FirelineBodyCoordinationStudy"));
  const int GroupLimit=FParse::Param(FCommandLine::Get(),TEXT("FirelineCarryLoopAudit"))?3:FParse::Param(FCommandLine::Get(),TEXT("FirelineCombatPostureAudit"))?40:FParse::Param(FCommandLine::Get(),TEXT("FirelineWalkHoldAudit"))?56:FParse::Param(FCommandLine::Get(),TEXT("FirelineRunFireAudit"))?52:FParse::Param(FCommandLine::Get(),TEXT("FirelineReadyAudit"))?44:FParse::Param(FCommandLine::Get(),TEXT("FirelineArmedMovementAudit"))?40:Boundary?48:Trace?24:20;
  const bool ContactStudy=FParse::Param(FCommandLine::Get(),TEXT("FirelineContactPhaseStudy"));
  const bool StrideStudy=FParse::Param(FCommandLine::Get(),TEXT("FirelineStrideCoverageStudy"));
  check(!Boundary||(Trace&&(StrideStudy||BodyStudy)));
  FString Dir=FPaths::ProjectSavedDir()/(FParse::Param(FCommandLine::Get(),TEXT("FirelineReadyAudit"))?(FParse::Param(FCommandLine::Get(),TEXT("FirelineArmedReadyStudy"))?TEXT("ArmedReadyCandidate"):TEXT("ArmedReadyBaseline")):!ArmedStudy&&FParse::Param(FCommandLine::Get(),TEXT("FirelineArmedMovementAudit"))?TEXT("ArmedLocomotionBaseline"):ArmedStudy?TEXT("ArmedLocomotionBoundary"):KneeStudy?TEXT("KneeContinuityBoundary"):GaitStudy?TEXT("GaitContinuityBoundary"):LegStudy?TEXT("LegDirectionBoundary"):Boundary?(BodyStudy?TEXT("BodyCoordinationBoundary"):TEXT("StrideCoverageBoundary")):Trace?(BodyStudy?TEXT("BodyCoordinationTrace"):StrideStudy?TEXT("StrideCoverageTrace"):ContactStudy?TEXT("ContactPhaseTrace"):TEXT("FullBodyTrace")):TEXT("DirectionalGameplay"));FString ReviewName;if(FParse::Value(FCommandLine::Get(),TEXT("FirelineReviewName="),ReviewName))Dir=FPaths::ProjectSavedDir()/FPaths::GetCleanFilename(ReviewName);IFileManager::Get().MakeDirectory(*Dir,true);
  auto ContactRows=MakeShared<FString>(TEXT("group,time,weight,contact_error,wrist_bend,bone_error,phase,weapon,reload_position,ammo,plant_l,plant_r,locked_l,locked_r\n"));
  auto PoseRows=MakeShared<FString>(TEXT("group,time,stage,bone,x,y,z,qx,qy,qz,qw,world_x,world_y,world_z\n"));
  auto StateRows=MakeShared<FString>(TEXT("group,time,speed,direction,vz,grounded,jump_serial,land_serial,air_weight,slide_weight,land_weight,directional_weight,camera_x,camera_y,camera_z,actor_yaw,aim_held,reloading,sliding,ammo\n"));
  auto SampleRows=MakeShared<FString>(TEXT("group,time,clip,weight,clip_time,previous_time,previous_marker,next_marker\n"));
  auto ViewRows=MakeShared<FString>(TEXT("group,time,speed,aim,reloading,view_x,view_y,view_z,view_qx,view_qy,view_qz,view_qw,feedback_x,feedback_y,feedback_z,feedback_angle,camera_pitch,camera_yaw,camera_roll,sprint_carry,firing,speed_cap,kick\n"));
  auto ArmRows=MakeShared<FString>(TEXT("group,time,contact_error_cm,elbow_penetration_cm,elbow_lateral_min_cm,bone,x,y,z,qx,qy,qz,qw\n"));
  if(Trace)FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this,S,PoseRows,StateRows,SampleRows,ArmRows,ViewRows,ContactRows,GroupLimit](UWorld* W,ELevelTick,float)
  {
   if(W!=GetWorld()||S->Group<0||S->Group>=GroupLimit)return;
   auto* Anim=Cast<URangeAnimInstance>(GetMesh()->GetAnimInstance());if(!Anim)return;
   const auto T=Anim->GetLocomotionTrace();
   *ContactRows+=FString::Printf(TEXT("%d,%.5f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%d,%.6f,%.6f,%.0f,%.0f\n"),S->Group,GetGameTimeSinceCreation()-2.f-S->Group*2.f,T.ContactWeight,T.ContactMetrics.X,T.ContactMetrics.Y,T.ContactMetrics.Z,ContactCarryClock.Phase,WeaponSlot(),ReloadPosition,Ammo,T.FootPlant.X,T.FootPlant.Y,T.FootLocked.X,T.FootLocked.Y);const FTransform World=GetMesh()->GetComponentTransform();
   // Timer callbacks can catch up in pairs. Use actual world time for each
   // evaluated frame, not the timer accumulator, or distinct poses share a key.
   const float Local=GetGameTimeSinceCreation()-2.f-S->Group*2.f;
   auto Write=[&](int Stage,FName Bone,const FTransform& Pose)
   {
    const FVector P=Pose.GetLocation(),WP=World.TransformPosition(P);const FQuat Q=Pose.GetRotation();
    *PoseRows+=FString::Printf(TEXT("%d,%.5f,%d,%s,%.5f,%.5f,%.5f,%.6f,%.6f,%.6f,%.6f,%.5f,%.5f,%.5f\n"),S->Group,Local,Stage,*Bone.ToString(),P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,WP.X,WP.Y,WP.Z);
   };
   int Stage=0;for(const auto* Poses:{&T.Ground,&T.Mobility,&T.Layered,&T.Final})
   {for(int I=0;I<Poses->Num();++I)Write(Stage,RangePoseTrace::Bones[I],(*Poses)[I]);++Stage;}
   for(FName Bone:RangePoseTrace::Bones)Write(4,Bone,GetMesh()->GetSocketTransform(Bone,RTS_Component));
   Stage=5;for(const auto* Poses:{&T.BodySolved,&T.ContactClip,&T.ContactRegistered,&T.ContactTargets}){for(int I=0;I<Poses->Num();++I)Write(Stage,RangePoseTrace::Bones[I],(*Poses)[I]);++Stage;}
   const FVector Metrics=Anim->GetBodyPoseAuditMetrics();
   for(const TCHAR* Bone:{TEXT("UpperArm_L"),TEXT("LowerArm_L"),TEXT("DJ_wrist_L"),TEXT("DJ_middle_01_L"),TEXT("UpperArm_R"),TEXT("LowerArm_R"),TEXT("DJ_wrist_R"),TEXT("DJ_middle_01_R"),bMP7?TEXT("MP7_rearsight"):TEXT("M4_rearsight"),bMP7?TEXT("MP7_frontsight"):TEXT("M4_frontsight")})
   {
    const auto Pose=GetMesh()->GetSocketTransform(Bone,RTS_Component);const FVector P=Pose.GetLocation();const FQuat Q=Pose.GetRotation();
    *ArmRows+=FString::Printf(TEXT("%d,%.5f,%.6f,%.6f,%.6f,%s,%.5f,%.5f,%.5f,%.6f,%.6f,%.6f,%.6f\n"),S->Group,Local,Metrics.X,Metrics.Y,Metrics.Z,Bone,P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W);
   }
   const FVector V=Arms->GetRelativeLocation();const FQuat Q=Arms->GetRelativeRotation().Quaternion();const FRotator CamR=Camera->GetRelativeRotation();
   *ViewRows+=FString::Printf(TEXT("%d,%.5f,%.5f,%.5f,%d,%.6f,%.6f,%.6f,%.8f,%.8f,%.8f,%.8f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%d,%.6f,%.6f\n"),S->Group,Local,Locomotion.Speed,AimAlpha,bReloading,V.X,V.Y,V.Z,Q.X,Q.Y,Q.Z,Q.W,ViewGaitTranslation.X,ViewGaitTranslation.Y,ViewGaitTranslation.Z,FMath::RadiansToDegrees(ViewGaitRotation.AngularDistance(FQuat::Identity)),CamR.Pitch,CamR.Yaw,CamR.Roll,SprintCarryAlpha,bFiringMovement,GetCharacterMovement()->MaxWalkSpeed,Kick);
   const FVector Cam=Camera->GetRelativeLocation();
   *StateRows+=FString::Printf(TEXT("%d,%.5f,%.4f,%.4f,%.4f,%d,%d,%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%d,%d,%d,%d\n"),S->Group,Local,Locomotion.Speed,Locomotion.DirectionDegrees,Locomotion.VerticalSpeed,Locomotion.Grounded,Locomotion.JumpSerial,Locomotion.LandingSerial,T.Weights.X,T.Weights.Y,T.Weights.Z,T.DirectionalWeight,Cam.X,Cam.Y,Cam.Z,GetActorRotation().Yaw,bAimHeld,bReloading,bSliding,Ammo);
   for(const auto& Sample:T.Samples)*SampleRows+=FString::Printf(TEXT("%d,%.5f,%s,%.6f,%.6f,%.6f,%d,%d\n"),S->Group,Local,*Sample.Clip,Sample.Weight,Sample.Time,Sample.PreviousTime,Sample.PreviousMarker,Sample.NextMarker);
  });
  GetWorldTimerManager().SetTimer(S->Timer,[this,S,PC,View,SourceReference,Dir,Trace,Boundary,GroupLimit,PoseRows,StateRows,SampleRows,ArmRows,ViewRows,ContactRows]
  {
   S->Time+=1.f/60;if(S->Time<2)return;
   const float T=S->Time-2;const int Group=int(T/2.f);const float Local=FMath::Fmod(T,2.f);
   if(Group>=GroupLimit){FFileHelper::SaveStringToFile(S->CSV,*(Dir/TEXT("runtime.csv")));if(Trace){FFileHelper::SaveStringToFile(*ContactRows,*(Dir/TEXT("contact.csv")));FFileHelper::SaveStringToFile(*PoseRows,*(Dir/TEXT("poses.csv")));FFileHelper::SaveStringToFile(*StateRows,*(Dir/TEXT("states.csv")));FFileHelper::SaveStringToFile(*SampleRows,*(Dir/TEXT("samples.csv")));FFileHelper::SaveStringToFile(*ArmRows,*(Dir/TEXT("arms.csv")));FFileHelper::SaveStringToFile(*ViewRows,*(Dir/TEXT("view.csv")));}GetWorldTimerManager().ClearTimer(S->Timer);FPlatformMisc::RequestExit(false);return;}
   if(S->Group!=Group)
   {
    S->Group=Group;S->Shot=false;SetActorLocation(FVector(-6000,0,95));GetCharacterMovement()->StopMovementImmediately();
    Controller->SetControlRotation(FRotator::ZeroRotator);if(!bThirdPerson)ToggleView();
    if(Group==18)EquipMP7();
    if(FParse::Param(FCommandLine::Get(),TEXT("FirelineCombatPostureAudit"))){if(Group==24)EquipRifle();if(Group==32)EquipMP7();if(Group>=24){EndAim();Ammo=MagazineCapacity();}}
    if(FParse::Param(FCommandLine::Get(),TEXT("FirelineArmedMovementAudit"))&&Group>=24){if(Group==24||Group==30||Group==32||Group==34||Group==36||Group==38)EquipRifle();if(Group==27||Group==31||Group==33||Group==35||Group==37||Group==39)EquipMP7();}
    if(FParse::Param(FCommandLine::Get(),TEXT("FirelineReadyAudit"))){if(Group==40)EquipRifle();if(Group==42)EquipMP7();}
    if(FParse::Param(FCommandLine::Get(),TEXT("FirelineRunFireAudit"))&&Group>=44)
    {
     EndAim();if(Group==44)EquipRifle();if(Group==48)EquipMP7();Ammo=MagazineCapacity();
     if(Group%4!=1&&bThirdPerson)ToggleView();
    }
    if(Boundary&&Group==24)EquipRifle();
    if(Boundary&&Group==32)EquipMP7();
    if(Trace&&Group>=20&&Group<24){EndJump();if(Group>=22&&bThirdPerson)ToggleView();}
    if(FParse::Param(FCommandLine::Get(),TEXT("FirelineReadyAudit"))&&((Group>=24&&Group<40)||Group==8||Group==9)&&bThirdPerson)ToggleView();
   }
   // Inputs are applied in character Tick, before movement evaluation.
   FVector Center=GetActorLocation()+FVector(0,0,15);
   const FVector Offset=Trace?(Group%3==0?FVector(300,0,30):Group%3==1?FVector(0,-300,30):FVector(-280,-280,100)):FVector(-280,-280,100);
   if(SourceReference)
   {
    const FVector ScreenRight=FVector::UpVector.Cross(-Offset).GetSafeNormal();
    SourceReference->SetWorldLocation(GetMesh()->GetComponentLocation()+ScreenRight*130.f);
    SourceReference->SetWorldRotation(GetMesh()->GetComponentQuat());Center+=ScreenRight*65.f;
    if(auto* Anim=Cast<URangeAnimInstance>(GetMesh()->GetAnimInstance()))
    {
     float Best=0;FString Clip;float Time=0;
     for(const auto& Sample:Anim->GetLocomotionTrace().Samples)if(Sample.Weight>Best&&Sample.Clip.Contains(TEXT("A_MM_"))){Best=Sample.Weight;Clip=Sample.Clip;Time=Sample.Time;}
     const int Start=Clip.Find(TEXT("A_MM_"));
     if(Start>=0){FString Name=Clip.Mid(Start+5);int Dot;if(Name.FindChar('.',Dot))Name=Name.Left(Dot);auto* Sequence=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_")+Name));if(Sequence){SourceReference->PlayAnimation(Sequence,false);SourceReference->SetPosition(Time,false);SourceReference->SetPlayRate(0);}}
    }
   }
   View->SetActorLocation(Center+Offset);View->SetActorRotation((Center-View->GetActorLocation()).Rotation());PC->SetViewTarget((FParse::Param(FCommandLine::Get(),TEXT("FirelineRunFireAudit"))&&Group>=44&&Group%4!=1)||(Trace&&Group>=22&&Group<24)||(FParse::Param(FCommandLine::Get(),TEXT("FirelineReadyAudit"))&&((Group>=24&&Group<40)||Group==8||Group==9))?static_cast<AActor*>(this):View);
   auto Bone=[this](const TCHAR* Name){return GetMesh()->GetSocketTransform(FName(Name),RTS_Component);};
   const FTransform Root=Bone(TEXT("Root"));
   S->CSV+=FString::Printf(TEXT("%d,%.5f,%.4f,%.4f,%.5f,%.5f,%.5f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d\n"),Group,Local,Locomotion.Speed,Locomotion.DirectionDegrees,Root.GetLocation().X,Root.GetLocation().Y,Root.GetScale3D().X,Bone(TEXT("UpperLeg_L")).GetLocation().Z,Bone(TEXT("LowerLeg_L")).GetLocation().Z,Bone(TEXT("Foot_L")).GetLocation().Z,Bone(TEXT("UpperLeg_R")).GetLocation().Z,Bone(TEXT("LowerLeg_R")).GetLocation().Z,Bone(TEXT("Foot_R")).GetLocation().Z,WeaponSlot());
   if(!S->Shot&&Local>.95f){S->Shot=true;UE_LOG(LogTemp,Display,TEXT("DIRECTIONAL_GAME group=%d pos=%s velocity=%s stateSpeed=%.3f acceleration=%s"),Group,*GetActorLocation().ToString(),*GetVelocity().ToString(),Locomotion.Speed,*GetCharacterMovement()->GetCurrentAcceleration().ToString());if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineTraceOnly"))&&(!FParse::Param(FCommandLine::Get(),TEXT("FirelineContactFixVisualAudit"))||Group==0||Group==2||Group==6||Group==10||Group==14||Group==16||Group==18||Group==19))FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%02d.png"),Group),false,false);}
   const int Frame=int(Local*(Boundary?5:10));
   if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineTraceOnly"))&&(!FParse::Param(FCommandLine::Get(),TEXT("FirelineContactFixVisualAudit"))||Group==2||Group==6||Group==10||Group==14||Group==16||Group==18||Group==19)&&(Group>=16||Trace)&&Frame!=S->Frame){S->Frame=Frame;FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("transition-%02d-%02d.png"),Group,Frame),false,false);}
  },1.f/60,true);
  return;
 }
 const bool ArmedSource=FParse::Param(FCommandLine::Get(),TEXT("FirelineArmedSourceAudit"));
 if(!ArmedSource&&!FParse::Param(FCommandLine::Get(),TEXT("FirelineDirectionalAudit")))return;
 FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);
 struct FReview
 {
  TArray<USkeletalMeshComponent*> Meshes;TArray<UAnimSequence*> Clips;
  FTimerHandle Timer;float Elapsed=0;int Group=-1;bool Shot=false;
  FString CSV=TEXT("group,time,source_l_x,source_l_y,source_l_z,target_l_x,target_l_y,target_l_z,target_r_z,target_root_scale\n");
 };
 auto State=MakeShared<FReview>();
 auto* PC=CastChecked<APlayerController>(Controller);
 PC->ConsoleCommand(TEXT("r.MotionBlurQuality 0"));PC->ConsoleCommand(TEXT("r.AntiAliasingMethod 1"));
 auto* CameraActor=GetWorld()->SpawnActor<ACameraActor>();
 CameraActor->SetActorLocation(FVector(-30,-350,150));CameraActor->SetActorRotation(FRotator(-8,180,0));PC->SetViewTarget(CameraActor);
 if(PC->GetHUD())PC->GetHUD()->bShowHUD=false;
 SetActorHiddenInGame(true);
 for(int I=0;I<2;++I)
 {
  auto* Actor=GetWorld()->SpawnActor<AActor>();
  auto* Mesh=NewObject<USkeletalMeshComponent>(Actor);Actor->SetRootComponent(Mesh);Mesh->RegisterComponent();
  Mesh->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,I==0?TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"):TEXT("/Game/Fireline/Hands/SK_RyanAction")));
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->bPauseAnims=true;
  Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
  Actor->SetActorLocation(FVector(-450,I==0?-240:-460,2));Actor->SetActorRotation(FRotator(0,-90,0));Actor->SetActorScale3D(FVector(I==0?1.f:.94f));
  State->Meshes.Add(Mesh);
 }
 const FString Dir=FPaths::ProjectSavedDir()/(ArmedSource?TEXT("ArmedSourceReview"):TEXT("DirectionalReview"));IFileManager::Get().MakeDirectory(*Dir,true);
 GetWorldTimerManager().SetTimer(State->Timer,[this,State,Dir,ArmedSource]
 {
  State->Elapsed+=1.f/60;const float T=State->Elapsed-1.f;if(T<0)return;
  const int Index=int(T/2.2f);
  if(Index>=(ArmedSource?2:14))
  {
   FFileHelper::SaveStringToFile(State->CSV,*(Dir/TEXT("comparison.csv")));
   GetWorldTimerManager().ClearTimer(State->Timer);FPlatformMisc::RequestExit(false);return;
  }
  const int Groups[]={0,1,2,4,6,7,8,9,10,11,12,13,14,15};const int Group=ArmedSource?8:Groups[Index];
  const TCHAR* Dirs[]={TEXT("Fwd"),TEXT("Fwd_Right"),TEXT("Right"),TEXT("Bwd_Right"),TEXT("Bwd"),TEXT("Bwd_Left"),TEXT("Left"),TEXT("Fwd_Left")};
  const TCHAR* Gait=Group<8?TEXT("Walk"):TEXT("Jog");const TCHAR* Direction=Dirs[Group%8];
  if(State->Group!=(ArmedSource?Index:Group))
  {
   State->Group=ArmedSource?Index:Group;State->Shot=false;State->Clips.Reset();
   State->Clips.Add(LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Rifle/%s/MF_Rifle_%s_%s"),Gait,Gait,Direction)));
   State->Clips.Add(LoadObject<UAnimSequence>(nullptr,ArmedSource?*FString::Printf(TEXT("/Game/Fireline/LocomotionStudy/ArmedLocomotion/Ready/A_%s_Fwd"),Index==0?TEXT("Jog"):TEXT("Sprint")):*FString::Printf(TEXT("/Game/Fireline/LocomotionStudy/Ready/A_%s_%s"),Gait,Direction)));
   for(int I=0;I<2;++I){check(State->Clips[I]);State->Meshes[I]->PlayAnimation(State->Clips[I],true);}
  }
  const float LocalTime=FMath::Fmod(T,2.2f);
  const float Phase=FMath::Fmod(LocalTime/State->Clips[0]->GetPlayLength(),1.f);
  for(int I=0;I<2;++I){auto* M=State->Meshes[I];M->SetPosition(Phase*State->Clips[I]->GetPlayLength(),false);M->TickAnimation(0,false);M->RefreshBoneTransforms();if(ArmedSource){FVector Root=M->GetSocketTransform(I==0?TEXT("root"):TEXT("Root"),RTS_Component).GetLocation();Root.Z=0;const FTransform Frame(FRotator(0,-90,0),FVector(-450,I==0?-240:-460,2),FVector(I==0?1.f:.94f));M->SetWorldLocation(Frame.GetLocation()-Frame.TransformVector(Root));}}
  const FVector S=State->Meshes[0]->GetSocketTransform(TEXT("foot_l"),RTS_Component).GetLocation();
  const FVector L=State->Meshes[1]->GetSocketTransform(TEXT("Foot_L"),RTS_Component).GetLocation();
  const FVector R=State->Meshes[1]->GetSocketTransform(TEXT("Foot_R"),RTS_Component).GetLocation();
  const float Scale=State->Meshes[1]->GetSocketTransform(TEXT("Root"),RTS_Component).GetScale3D().X;
  State->CSV+=FString::Printf(TEXT("%d,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f,%.5f\n"),Group,LocalTime,S.X,S.Y,S.Z,L.X,L.Y,L.Z,R.Z,Scale);
  if(ArmedSource)FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%02d-%03d.png"),Index,int(LocalTime*60)),false,false);
  if(!State->Shot&&LocalTime>.45f){State->Shot=true;FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("%02d-%s-%s.png"),Group,Gait,Direction),false,false);}
 },1.f/60,true);
}
