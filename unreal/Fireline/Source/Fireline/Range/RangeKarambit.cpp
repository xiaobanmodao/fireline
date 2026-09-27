#include "RangeGame.h"
#include "RangeAnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraComponent.h"
#include "TimerManager.h"
#include "Misc/App.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

void URangeAssetTools::PrepareSelectedKarambit()
{
#if WITH_EDITOR
 const bool ArmsOnly=FParse::Param(FCommandLine::Get(),TEXT("FirelineKarambitArmsOnly"));
 const FString Dir=TEXT("/Game/Fireline/Karambit");
 auto* Body=LoadObject<USkeletalMesh>(nullptr,*(Dir/TEXT("SK_KarambitBody.SK_KarambitBody")));check(Body);auto* Sk=Body->GetSkeleton();check(Sk);
 if(!ArmsOnly)Sk->AddCompatibleSkeleton(LoadObject<USkeleton>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction_Skeleton.SK_RyanAction_Skeleton")));
 for(int32 I=0;!ArmsOnly&&I<Sk->GetReferenceSkeleton().GetNum();++I)
 {
  const FName Bone=Sk->GetReferenceSkeleton().GetBoneName(I);Sk->SetBoneTranslationRetargetingMode(I,EBoneTranslationRetargetingMode::Animation);
  if(Bone.ToString().StartsWith(TEXT("DJ_"))||Bone.ToString().StartsWith(TEXT("Finger_"))||Bone.ToString().StartsWith(TEXT("HandMeta_"))||Bone==FName(TEXT("HandHold_R")))
  {
   const FName Name(*FString::Printf(TEXT("Required_Karambit_%s"),*Bone.ToString()));
   if(!Sk->FindSocket(Name)){auto* Socket=NewObject<USkeletalMeshSocket>(Sk);Socket->SocketName=Name;Socket->BoneName=Bone;Socket->bForceAlwaysAnimated=true;Sk->Sockets.Add(Socket);}
  }
 }
 TArray<UObject*> Save;if(!ArmsOnly)Save.Add(Sk);
 for(const TCHAR* Name:{TEXT("SK_KarambitBody"),TEXT("SK_KarambitArms"),TEXT("SK_KrazzyElfKarambit")})
 {
  if(ArmsOnly&&FString(Name)==TEXT("SK_KrazzyElfKarambit"))continue;
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,*FString::Printf(TEXT("%s/%s.%s"),*Dir,Name,Name));check(Mesh);
  for(int32 L=0;L<Mesh->GetLODNum();++L)Mesh->GetLODInfo(L)->bAllowCPUAccess=true;
  Mesh->PostEditChange();Save.Add(Mesh);
 }
 for(auto* Asset:Save)
 {
  Asset->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
 }
#endif
}
void ARangeCharacter::StartKarambitAudit()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineKarambitAudit")))return;
 check(bKarambitReady);bKarambitAudit=true;
 // This audit drives actions directly. Desktop mouse/key events must not
 // replace the named hold/inspect poses while their screenshots are taken.
 if(auto* PC=Cast<APlayerController>(Controller))
 {
  DisableInput(PC);PC->SetIgnoreLookInput(true);PC->SetIgnoreMoveInput(true);
 }
 // Read final component bounds after skeletal followers have ticked, not in
 // the owner's input tick while this frame's pose may still be evaluating.
 FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this](UWorld* World,ELevelTick,float){if(World==GetWorld())TickKarambitAudit();});
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineKarambitCapture"))){FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1.0/60.0);}
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 auto Capture=[this](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/karambit-key-%s.png"),Name),true,false);};
 Later(3,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);EquipKnife();});
 Later(3.7f,[Capture]{Capture(TEXT("draw"));});Later(4.5f,[Capture]{Capture(TEXT("hold"));});
 Later(5,[this]{TestFingers();});Later(5.8f,[Capture]{Capture(TEXT("inspect-open"));});Later(7.4f,[Capture]{Capture(TEXT("inspect-face"));});Later(9.2f,[Capture]{Capture(TEXT("inspect-catch"));});
 Later(10.2f,[this]{UE_LOG(LogTemp,Display,TEXT("KARAMBIT_INSPECT_FINISHED %d"),KnifeAction==0);TestFingers();});
 Later(10.8f,[this]{KnifeAttackPressed();});Later(11.f,[Capture]{Capture(TEXT("slash"));});
 Later(12,[this]{ToggleView();Controller->SetControlRotation(FRotator(-8,150,0));TestFingers();});
 Later(14.4f,[Capture]{Capture(TEXT("third-inspect"));});Later(17.2f,[this]{Crouch();});Later(17.7f,[Capture]{Capture(TEXT("crouch"));});
 Later(18.2f,[this]{UnCrouch();KnifeAttackPressed();});Later(18.4f,[Capture]{Capture(TEXT("third-slash"));});
 Later(19.5f,[this]{EquipRifle();});Later(20.2f,[Capture]{Capture(TEXT("third-rifle-return"));});
 Later(21,[this]{ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);StartAim();});Later(21.7f,[Capture]{Capture(TEXT("rifle-aim"));});
 Later(22,[this]{EndAim();Ammo=29;Reload();});Later(23.1f,[Capture]{Capture(TEXT("rifle-reload"));});
 Later(25,[this]{ToggleEmptyHands();});Later(25.6f,[Capture]{Capture(TEXT("empty"));});Later(26,[this]{EquipRifle();});
 Later(27,[this]{auto* Anim=Cast<URangeAnimInstance>(Arms->GetAnimInstance());const bool Restored=Arms->GetSkeletalMeshAsset()==RifleArmsMesh&&GetMesh()->GetSkeletalMeshAsset()==RifleBodyMesh;const FString Report=FString::Printf(TEXT("{\"rifle_mesh_restored\":%s,\"rifle_mode\":%s,\"ammo\":%d,\"reload_complete\":%s,\"rifle_anim\":%s,\"bounds_outside\":%d,\"bounds_samples\":%d}"),Restored?TEXT("true"):TEXT("false"),!bKnife&&!bEmptyHands?TEXT("true"):TEXT("false"),Ammo,!bReloading?TEXT("true"):TEXT("false"),Anim&&Anim->HoldAsset?TEXT("true"):TEXT("false"),KarambitBoundsOutside,KarambitBoundsSamples);FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("karambit-runtime-audit.json")));UE_LOG(LogTemp,Display,TEXT("KARAMBIT_AUDIT %s"),*Report);FPlatformMisc::RequestExit(false);});
}
void ARangeCharacter::TickKarambitAudit()
{
 if(!bKarambitAudit)return;
 if(GetGameTimeSinceCreation()-KarambitBoundsTime>.25f)
 {
  KarambitBoundsTime=GetGameTimeSinceCreation();
  for(auto* Mesh:{Arms.Get(),ViewKnife.Get(),ViewGun.Get()})if(Mesh->IsVisible())
  {
   TArray<FVector3f> Vertices;TArray<FMatrix44f> Matrices;Mesh->GetCurrentRefToLocalMatrices(Matrices,0);
   const auto& LOD=Mesh->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0];
   USkinnedMeshComponent::ComputeSkinnedPositions(Mesh,Vertices,Matrices,LOD,LOD.SkinWeightVertexBuffer);
   const FBox Box=Mesh->Bounds.GetBox().ExpandBy(.1f);int Outside=0;
   for(const auto& V:Vertices)Outside+=!Box.IsInsideOrOn(Mesh->GetComponentTransform().TransformPosition(FVector(V)));
   KarambitBoundsOutside+=Outside;++KarambitBoundsSamples;
   if(Outside)UE_LOG(LogTemp,Error,TEXT("KARAMBIT_BOUNDS_OUTSIDE %s vertices=%d t=%.3f action=%d action_time=%.3f"),*Mesh->GetName(),Outside,GetGameTimeSinceCreation(),KnifeAction,KnifeTime);
  }
 }
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineKarambitCapture")))return;
 const float T=GetGameTimeSinceCreation();if(T<3||T>26)return;
 static int32 Last=-1;static int32 Index=0;const int32 Step=FMath::FloorToInt(T*30.f);
 if(Step!=Last){Last=Step;FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/karambit-play-%04d.png"),Index++),true,false);}
}
