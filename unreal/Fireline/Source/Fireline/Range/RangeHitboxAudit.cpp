#include "RangeGame.h"
#include "RangeHitMesh.h"
#include "RangeBallistics.h"
#include "Animation/AnimSingleNodeInstance.h"
#include "Animation/AnimSequence.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "SkeletalRenderPublic.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Rendering/SkinWeightVertexBuffer.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameViewportClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "TimerManager.h"

// Frozen pre-fix implementation, for measuring missed visible surface samples.
static bool LegacyHit(ARangeTarget* T,const FVector& Start,const FVector& Dir,float Limit)
{
 auto B=[&](FName N){return T->Body->GetSocketLocation(N);};float D;
 if(RangeRayCapsule(Start,Dir,Limit,B(TEXT("Pelvis")),B(TEXT("Chest")),18,D))return true;
 for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
  for(const auto& Pair:{TPair<const TCHAR*,const TCHAR*>(TEXT("UpperArm"),TEXT("LowerArm")),{TEXT("LowerArm"),TEXT("UpperHand")},{TEXT("UpperLeg"),TEXT("LowerLeg")},{TEXT("LowerLeg"),TEXT("Foot")}})
  {
   const float R=FString(Pair.Key)==TEXT("UpperLeg")?10:FString(Pair.Key)==TEXT("LowerArm")?7:8;
   if(RangeRayCapsule(Start,Dir,Limit,B(*FString::Printf(TEXT("%s.%s"),Pair.Key,Side)),B(*FString::Printf(TEXT("%s.%s"),Pair.Value,Side)),R,D))return true;
  }
 const FVector H=B(TEXT("Head"))+T->Body->GetUpVector()*6;
 return RangeRayCapsule(Start,Dir,Limit,H,H,13,D);
}
void ARangeCharacter::StartHitboxAudit()
{
 bHitboxAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineHitboxAudit"));if(!bHitboxAudit)return;
 if(auto* PC=Cast<APlayerController>(Controller))DisableInput(PC);
 HitboxAuditSamples=TEXT("pose,heading,region,samples,legacy_misses,new_misses,max_query_us\n");
 auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
 const TCHAR* Names[]={TEXT("Idle"),TEXT("Crouch"),TEXT("Reload"),TEXT("JumpFall")};
 for(int I=0;I<4;++I)
 {
  Later(2+I*3,[this,I,Name=FString(Names[I])]{
   TActorIterator<ARangeTarget> It(GetWorld());if(!It)return;
   auto* Seq=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/%s/A_M4_%s.A_M4_%s"),It->Body->GetSkeletalMeshAsset()->GetPathName().StartsWith(TEXT("/Game/Fireline/DrazenOriginal/"))?TEXT("DrazenOriginal"):TEXT("Action"),*Name,*Name));
   It->Body->PlayAnimation(Seq,false);It->Body->GetSingleNodeInstance()->SetPosition(I==2?1.15f:.05f,false);It->Body->GetSingleNodeInstance()->SetPlaying(false);
  });
  Later(2.6f+I*3,[this,I]{SampleHitbox(I);});
 }
 Later(15,[this]{
  TActorIterator<ARangeTarget> It(GetWorld());if(!It)return;
  It->Body->PlayAnimation(LoadObject<UAnimSequence>(nullptr,It->Body->GetSkeletalMeshAsset()->GetPathName().StartsWith(TEXT("/Game/Fireline/DrazenOriginal/"))?TEXT("/Game/Fireline/DrazenOriginal/A_M4_Idle.A_M4_Idle"):TEXT("/Game/Fireline/Action/A_M4_Idle.A_M4_Idle")),true);
  CastChecked<APlayerController>(Controller)->SetViewTarget(this);
  SetActorLocation(It->GetActorLocation()+It->GetActorForwardVector()*450+FVector(0,0,5));
 });
 auto AimHand=[this](const FString& Side){
  TActorIterator<ARangeTarget> It(GetWorld());if(!It||It->IsHidden())return;auto* Body=It->Body.Get();
  TArray<FFinalSkinVertex> Skin;Body->GetCPUSkinnedVertices(Skin,0);
  const auto& LOD=Body->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0];
  const auto& Ref=Body->GetSkeletalMeshAsset()->GetRefSkeleton();const auto& W=LOD.SkinWeightVertexBuffer;
  FVector Sum=FVector::ZeroVector;int Count=0;
  for(const auto& S:LOD.RenderSections)for(uint32 V=S.BaseVertexIndex;V<S.BaseVertexIndex+S.NumVertices;++V)
  {
   int Bone=0;uint16 Best=0;for(uint32 I=0;I<W.GetMaxBoneInfluences();++I)if(W.GetBoneWeight(V,I)>Best){Best=W.GetBoneWeight(V,I);Bone=S.BoneMap[W.GetBoneIndex(V,I)];}
   const FString Name=Ref.GetBoneName(Bone).ToString().Replace(TEXT("_"),TEXT("."));
   if((Name.Contains(TEXT("Hand"))||Name.Contains(TEXT("Palm")))&&Name.EndsWith(Side)){Sum+=FVector(Skin[V].Position);++Count;}
  }
  UE_LOG(LogTemp,Display,TEXT("FIRELINE_AIM_HAND side=%s vertices=%d"),*Side,Count);
  if(Count){const FVector P=Body->GetComponentTransform().TransformPosition(Sum/Count);Controller->SetControlRotation((P-Camera->GetComponentLocation()).Rotation());}
 };
 Later(15.8f,[AimHand]{AimHand(TEXT(".L"));});
 Later(16,[this]{Fire();TActorIterator<ARangeTarget> It(GetWorld());if(It)UE_LOG(LogTemp,Display,TEXT("FIRELINE_HAND_SHOT left health=%d bone=%s"),It->GetHealth(),*CastChecked<URangeHitMesh>(It->Body)->GetLastHitBone().ToString());});
 Later(16.1f,[]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/hitbox-left-hand.png"),true,false);});
 Later(16.6f,[AimHand]{AimHand(TEXT(".R"));});
 Later(16.8f,[this]{Fire();TActorIterator<ARangeTarget> It(GetWorld());if(It)UE_LOG(LogTemp,Display,TEXT("FIRELINE_HAND_SHOT right health=%d bone=%s"),It->GetHealth(),*CastChecked<URangeHitMesh>(It->Body)->GetLastHitBone().ToString());});
 Later(16.9f,[]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/hitbox-right-hand.png"),true,false);});
 Later(18,[this]{FFileHelper::SaveStringToFile(HitboxAuditSamples,*(FPaths::ProjectSavedDir()/TEXT("hitbox-audit.csv")));FPlatformMisc::RequestExit(false);});
}
void ARangeCharacter::SampleHitbox(int Index)
{
 TActorIterator<ARangeTarget> It(GetWorld());if(!It)return;auto* Target=*It;auto* Body=Target->Body.Get();
 TArray<FFinalSkinVertex> Skin;Body->GetCPUSkinnedVertices(Skin,0);
 const auto& LOD=Body->GetSkeletalMeshAsset()->GetResourceForRendering()->LODRenderData[0];
 const auto& Ref=Body->GetSkeletalMeshAsset()->GetRefSkeleton();
 const auto& Weights=LOD.SkinWeightVertexBuffer;
 const auto* Indices=LOD.MultiSizeIndexContainer.GetIndexBuffer();
 struct FCount{int N=0,OldMiss=0,Miss=0;double MaxUS=0;};
 const FVector Center=Target->GetActorLocation()+FVector(0,0,15);
 for(int Heading=0;Heading<4;++Heading)
 {
  const FVector From=Center+FRotator(0,Target->GetActorRotation().Yaw+30+Heading*90,0).Vector()*350;
  TMap<FString,FCount> Counts;
  for(const auto& S:LOD.RenderSections)for(uint32 T=0;T<S.NumTriangles;T+=2)
  {
   const uint32 A=Indices->Get(S.BaseIndex+T*3),B=Indices->Get(S.BaseIndex+T*3+1),C=Indices->Get(S.BaseIndex+T*3+2);
   const FVector Point=Body->GetComponentTransform().TransformPosition(FVector((Skin[A].Position+Skin[B].Position+Skin[C].Position)/3.f));
   uint16 Best=0;int Bone=0;
   for(uint32 I=0;I<Weights.GetMaxBoneInfluences();++I)if(Weights.GetBoneWeight(A,I)>Best){Best=Weights.GetBoneWeight(A,I);Bone=S.BoneMap[Weights.GetBoneIndex(A,I)];}
   const FString Name=Ref.GetBoneName(Bone).ToString();
   const FString Region=(Name.Contains(TEXT("Hand"))||Name.Contains(TEXT("Palm")))||Name.Contains(TEXT("Thumb"))||Name.Contains(TEXT("Finger"))||Name.Contains(TEXT("Index"))||Name.Contains(TEXT("Middle"))||Name.Contains(TEXT("Ring"))||Name.Contains(TEXT("Pinky"))?TEXT("hands"):Name.Contains(TEXT("Arm"))||Name.Contains(TEXT("Shoulder"))||Name.Contains(TEXT("Collar"))?TEXT("arms_shoulders"):Name.Contains(TEXT("Head"))?TEXT("head"):Name.Contains(TEXT("Leg"))||Name.Contains(TEXT("Hip"))||Name.Contains(TEXT("Shin"))||Name.Contains(TEXT("Foot"))||Name.Contains(TEXT("Toes"))?TEXT("legs_feet"):TEXT("torso");
   const FVector Dir=(Point-From).GetSafeNormal();const float Limit=FVector::Distance(From,Point)+.2f;
   auto& Count=Counts.FindOrAdd(Region);++Count.N;const bool Old=LegacyHit(Target,From,Dir,15000);Count.OldMiss+=!Old;
   float D;bool H;FVector N;const double Time=FPlatformTime::Seconds();
   const bool Hit=Target->TraceShot(From,Dir,Limit,D,H,N);Count.MaxUS=FMath::Max(Count.MaxUS,(FPlatformTime::Seconds()-Time)*1.e6);Count.Miss+=!Hit;
   if(Heading==0&&!Old)DrawDebugPoint(GetWorld(),Point,5,Hit?FColor::Green:FColor::Red,false,2.5f);
  }
  for(const auto& C:Counts)HitboxAuditSamples+=FString::Printf(TEXT("%d,%d,%s,%d,%d,%d,%.2f\n"),Index,Heading,*C.Key,C.Value.N,C.Value.OldMiss,C.Value.Miss,C.Value.MaxUS);
  // Rays well outside the visible silhouette must remain misses.
  int FalseHits=0;for(float Side:{-1.f,1.f})
  {float D;bool H;FVector N;FalseHits+=Target->TraceShot(Center+FVector(0,Side*250,0)-FVector(400,0,0),FVector(1,0,0),800,D,H,N);}
  HitboxAuditSamples+=FString::Printf(TEXT("%d,%d,outside,2,0,%d,0\n"),Index,Heading,FalseHits);
 }
 auto* Cam=GetWorld()->SpawnActor<ACameraActor>();const FVector Pos=Center+FRotator(0,Target->GetActorRotation().Yaw+30,0).Vector()*460;
 Cam->SetActorLocationAndRotation(Pos,(Center-Pos).Rotation());Cam->GetCameraComponent()->SetFieldOfView(50);
 CastChecked<APlayerController>(Controller)->SetViewTarget(Cam);
 FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/hitbox-%02d.png"),Index),true,false);
}
