#include "RangeGame.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "TimerManager.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "HAL/FileManager.h"

// Exercise normal animation playback, rather than freezing reload key poses.
void ARangeCharacter::StartHoldAudit()
{
 const bool bMotionCapture=FParse::Param(FCommandLine::Get(),TEXT("FirelineMotionCapture"));
 const bool bGripDump=FParse::Param(FCommandLine::Get(),TEXT("FirelineGripDump"));
 bHoldAudit=bGripDump||bMotionCapture||FParse::Param(FCommandLine::Get(),TEXT("FirelineHoldAudit"));
 if(!bHoldAudit)return;
 if(auto* PC=Cast<APlayerController>(Controller))DisableInput(PC);
 HoldAuditSamples=TEXT("time,phase,x_cm,y_cm,z_cm,qx,qy,qz,qw,ammo,reloading\n");
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 if(bGripDump)
 {
  // Export the evaluated game geometry, not the FBX source or socket proxies.
  // Component-space centimetres let the import be compared with the source.
  auto Dump=[this](const FString& Label)
  {
   const FString Folder=FPaths::ProjectSavedDir()/TEXT("GripReview");
   IFileManager::Get().MakeDirectory(*Folder,true);
   for(auto* Mesh:{Arms.Get(),ViewGun.Get()})
   {
    const auto& LOD=Mesh->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0];
    TArray<FMatrix44f> Matrices;TArray<FVector3f> Vertices;
    Mesh->GetCurrentRefToLocalMatrices(Matrices,0);
    USkinnedMeshComponent::ComputeSkinnedPositions(Mesh,Vertices,Matrices,LOD,LOD.SkinWeightVertexBuffer);
    FString Obj=FString::Printf(TEXT("# %s evaluated component-space metres\n"),*Mesh->GetSkeletalMeshAsset()->GetPathName());
    for(const auto& V:Vertices)Obj+=FString::Printf(TEXT("v %.9f %.9f %.9f\n"),V.X*.01,V.Y*.01,V.Z*.01);
    FString Weights=TEXT("vertex,bone,weight\n");
    const auto* Indices=LOD.MultiSizeIndexContainer.GetIndexBuffer();
    for(const auto& Section:LOD.RenderSections)
    {
     Obj+=FString::Printf(TEXT("g material_%d\n"),Section.MaterialIndex);
     for(uint32 T=0;T<Section.NumTriangles;++T)
     {
      const uint32 I=Section.BaseIndex+T*3;
      Obj+=FString::Printf(TEXT("f %u %u %u\n"),Indices->Get(I)+1,Indices->Get(I+1)+1,Indices->Get(I+2)+1);
     }
     for(uint32 V=Section.BaseVertexIndex;V<Section.BaseVertexIndex+Section.NumVertices;++V)
      for(uint32 I=0;I<LOD.SkinWeightVertexBuffer.GetMaxBoneInfluences();++I)
      {
       const auto Weight=LOD.SkinWeightVertexBuffer.GetBoneWeight(V,I);if(!Weight)continue;
       const auto Bone=Section.BoneMap[LOD.SkinWeightVertexBuffer.GetBoneIndex(V,I)];
       Weights+=FString::Printf(TEXT("%u,%s,%u\n"),V,*Mesh->GetBoneName(Bone).ToString(),Weight);
      }
    }
    const FString Name=Label+TEXT("-")+Mesh->GetName();
    FFileHelper::SaveStringToFile(Obj,*(Folder/(Name+TEXT(".obj"))));
    FFileHelper::SaveStringToFile(Weights,*(Folder/(Name+TEXT("-weights.csv"))));
   }
   FScreenshotRequest::RequestScreenshot(Folder/(Label+TEXT(".png")),true,false);
  };
  Later(3,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);});
  Later(4,[Dump]{Dump(TEXT("idle"));});
  Later(5,[this]{StartAim();});Later(6,[Dump]{Dump(TEXT("aim"));});
  Later(7,[this]{EndAim();Ammo=29;Reload();});
  Later(8.25f,[Dump]{Dump(TEXT("magazine"));});
  Later(8.75f,[Dump]{Dump(TEXT("charging"));});
  Later(10,[Dump]{Dump(TEXT("returned"));});
  Later(11,[]{FPlatformMisc::RequestExit(false);});
  return;
 }
 if(bMotionCapture)
 {
  HoldAuditSamples=TEXT("game_time,view,reload_position,ammo,reloading\n");
  // Deterministic playback for frame-by-frame visual review. Readback cost
  // must not skip animation time; this mode is not a performance benchmark.
  FApp::SetFixedDeltaTime(1.0/60.0);FApp::SetUseFixedTimeStep(true);
  Later(3,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);});
  Later(5,[this]{Ammo=29;Reload();});
  Later(9,[this]{ToggleView();Controller->SetControlRotation(FRotator(-8,155,0));});
  Later(10,[this]{Ammo=29;Reload();});
  Later(14,[this]{ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);StartCrouch();StartAim();});
  Later(15,[this]{Ammo=29;Reload();});
  Later(19,[this]{
   FFileHelper::SaveStringToFile(HoldAuditSamples,*(FPaths::ProjectSavedDir()/TEXT("motion-capture.csv")));
   UE_LOG(LogTemp,Display,TEXT("FIRELINE_MOTION_CAPTURE_COMPLETE ammo=%d reloading=%d"),Ammo,bReloading);
   FPlatformMisc::RequestExit(false);
  });
  return;
 }
 Later(3,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);});
 Later(13,[this]{Ammo=29;Reload();});
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineHoldCapture")))
 {
  const TArray<float> Times={5.f,5.5f,9.f,12.f,14.3f,18.f};
  for(int I=0;I<Times.Num();++I)
   Later(Times[I],[this,I]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/hold-%02d.png"),I),true,false);});
 }
 Later(20,[this]{
  FFileHelper::SaveStringToFile(HoldAuditSamples,*(FPaths::ProjectSavedDir()/TEXT("hold-audit.csv")));
  UE_LOG(LogTemp,Display,TEXT("FIRELINE_HOLD_AUDIT reload_complete=%d ammo=%d"),!bReloading,Ammo);
  FPlatformMisc::RequestExit(false);
 });
}

void ARangeCharacter::TickHoldAudit(float Dt)
{
 const float T=GetGameTimeSinceCreation();
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineMotionCapture")))
 {
  static int32 LastStep=-1;
  static int32 CaptureIndex=0;
  const int32 Step=FMath::FloorToInt(T*30.f);
  const int32 View=T<9?0:T<14?1:2;
  const float Start=View==0?4.5f:View==1?9.5f:14.5f;
  if(T>=Start&&T<Start+3.5f&&Step!=LastStep)
  {
   LastStep=Step;
   FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/motion-%04d.png"),CaptureIndex++),true,false);
   HoldAuditSamples+=FString::Printf(TEXT("%.6f,view%d,%.6f,%d,%d\n"),T,View,ReloadPosition,Ammo,bReloading);
  }
  return;
 }
 if(T>=8 && T<10)AddMovementInput(GetActorRightVector(),.65f);
 if(T<4 || T>=20)return;
 const TCHAR* Phase=T<8?TEXT("idle_before"):T<10?TEXT("walking"):T<13?TEXT("stopping"):T<16?TEXT("reload"):TEXT("idle_after");
 const FTransform CameraTransform=Camera->GetComponentTransform();
 const FVector Muzzle=CameraTransform.InverseTransformPosition(ViewGun->GetSocketLocation(TEXT("M4_muzzle")));
 const FQuat Rotation=CameraTransform.InverseTransformRotation(ViewGun->GetSocketQuaternion(TEXT("M4_body")));
 HoldAuditSamples+=FString::Printf(TEXT("%.6f,%s,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%d,%d\n"),
  T,Phase,Muzzle.X,Muzzle.Y,Muzzle.Z,Rotation.X,Rotation.Y,Rotation.Z,Rotation.W,Ammo,bReloading);
}
