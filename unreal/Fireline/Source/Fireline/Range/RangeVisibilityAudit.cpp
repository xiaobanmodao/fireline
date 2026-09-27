#include "RangeGame.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "SkeletalRenderPublic.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"

void ARangeCharacter::StartVisibilityAudit()
{
 bVisibilityAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineVisibilityAudit"));if(!bVisibilityAudit)return;
 if(auto* PC=Cast<APlayerController>(Controller))DisableInput(PC);
 VisibilityAuditSamples=TEXT("index,mesh,yaw,pitch,aim,reload,crouch,render_age,vertices,outside_bounds,bounds_x,bounds_y,bounds_z,extent_x,extent_y,extent_z\n");
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 Later(1,[this]{bAimHeld=true;});
 int Index=0;
 for(float Pitch:{0.f,45.f,-45.f})for(float Yaw:{0.f,45.f,90.f,135.f,180.f,225.f,270.f,315.f})
 {
  const int I=Index++;const float Time=2+I*.8f;
  Later(Time,[this,Pitch,Yaw]{Controller->SetControlRotation(FRotator(Pitch,Yaw,0));});
  Later(Time+.55f,[this,I]{CaptureVisibility(I);});
 }
 Later(21.5f,[this]{StartCrouch();Controller->SetControlRotation(FRotator(0,135,0));});
 Later(22.1f,[this]{CaptureVisibility(24);});
 Later(22.5f,[this]{Ammo=29;Reload();});
 Later(23.8f,[this]{CaptureVisibility(25);});
 Later(25.3f,[this]{CaptureVisibility(26);EndCrouch();bAimHeld=false;});
 Later(26,[this]{CaptureVisibility(27);});
 Later(27,[this]{FFileHelper::SaveStringToFile(VisibilityAuditSamples,*(FPaths::ProjectSavedDir()/TEXT("visibility-audit.csv")));FPlatformMisc::RequestExit(false);});
}
void ARangeCharacter::CaptureVisibility(int Index)
{
 for(auto* Mesh:{ViewGun.Get(),Arms.Get()})
 {
  // Independently skin the actual rendered mesh and compare with culling bounds.
  // This catches a bound around the bind pose or around the wrong parent mesh.
  // Read-only skinning. GetCPUSkinnedVertices recreates the render state twice
  // per sample and can collide with UE 5.8 asynchronous scene uploads.
  TArray<FVector3f> Vertices;TArray<FMatrix44f> Matrices;
  Mesh->GetCurrentRefToLocalMatrices(Matrices,0);
  const auto& LOD=Mesh->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0];
  USkinnedMeshComponent::ComputeSkinnedPositions(Mesh,Vertices,Matrices,LOD,LOD.SkinWeightVertexBuffer);
  const FBox Box=Mesh->Bounds.GetBox().ExpandBy(.1f);
  int Outside=0;for(const auto& V:Vertices)Outside+=!Box.IsInsideOrOn(Mesh->GetComponentTransform().TransformPosition(FVector(V)));
  const FVector C=Camera->GetComponentTransform().InverseTransformPosition(Mesh->Bounds.Origin),E=Mesh->Bounds.BoxExtent;
  const FRotator R=Controller->GetControlRotation();
  VisibilityAuditSamples+=FString::Printf(TEXT("%d,%s,%.2f,%.2f,%.3f,%.3f,%d,%.4f,%d,%d,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f\n"),Index,*Mesh->GetName(),R.Yaw,R.Pitch,AimAlpha,bReloading?ReloadPosition:-1.f,bIsCrouched,GetWorld()->GetTimeSeconds()-Mesh->GetLastRenderTimeOnScreen(),Vertices.Num(),Outside,C.X,C.Y,C.Z,E.X,E.Y,E.Z);
 }
 FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/visibility-%02d.png"),Index),true,false);
}
