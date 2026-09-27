#include "RangeGame.h"
#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "ReferenceSkeleton.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

namespace
{
void SaveLocomotion(UObject* Asset)
{
 Asset->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 check(UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
}
template<class T>T* CopyStudy(T* Source,const FString& Path)
{
 if(auto* Existing=LoadObject<T>(nullptr,*Path))return Existing;
 return DuplicateObject<T>(Source,CreatePackage(*Path),*FPackageName::GetShortName(Path));
}
}
#endif

void URangeAssetTools::PrepareLocomotionProxy()
{
#if WITH_EDITOR
 auto* Original=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction"));check(Original);
 auto* Mesh=CopyStudy(Original,TEXT("/Game/Fireline/LocomotionStudy/SK_RyanRetargetProxy"));
 auto* Skeleton=CopyStudy(Original->GetSkeleton(),TEXT("/Game/Fireline/LocomotionStudy/SK_RyanRetargetSkeleton"));
 Mesh->SetSkeleton(Skeleton);
 // IK Retargeter works in scale-free component space. Normalize ONLY an
 // isolated proxy, leaving the production mesh/skeleton/bind untouched.
 const auto& Native=Original->GetRefSkeleton();TArray<FTransform> World;
 for(int I=0;I<Native.GetNum();++I){const int P=Native.GetParentIndex(I);World.Add(P<0?Native.GetRefBonePose()[I]:Native.GetRefBonePose()[I]*World[P]);}
 FReferenceSkeleton Ref=Native;
 {FReferenceSkeletonModifier Edit(Ref,Skeleton);
  for(int I=0;I<Ref.GetNum();++I)
  {
   const int P=Ref.GetParentIndex(I);FTransform W=World[I];W.SetScale3D(FVector::OneVector);
   FTransform Parent=P<0?FTransform::Identity:World[P];Parent.SetScale3D(FVector::OneVector);
   Edit.UpdateRefPoseTransform(I,P<0?W:W.GetRelativeTransform(Parent));
  }
 }
 Mesh->SetRefSkeleton(Ref);Mesh->GetRefBasesInvMatrix().Reset();Mesh->CalculateInvRefMatrices();
 Skeleton->UpdateReferencePoseFromMesh(Mesh);Skeleton->SetPreviewMesh(Mesh);
 for(int I=0;I<Ref.GetNum();++I)Skeleton->SetBoneTranslationRetargetingMode(I,EBoneTranslationRetargetingMode::Animation);
 SaveLocomotion(Skeleton);SaveLocomotion(Mesh);
 UE_LOG(LogTemp,Display,TEXT("LOCOMOTION_PROXY normalized isolated reference, original root scale=%s"),*Native.GetRefBonePose()[0].GetScale3D().ToString());
#endif
}

void URangeAssetTools::BuildDirectionalAssets()
{
#if WITH_EDITOR
 auto* Native=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction"));check(Native);
 auto* Hold=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/SycgffM4/A_M4_Idle"));check(Hold);
 const auto& Ref=Native->GetRefSkeleton();TArray<FTransform> RefCS;
 for(int I=0;I<Ref.GetNum();++I){const int P=Ref.GetParentIndex(I);RefCS.Add(P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*RefCS[P]);}
 const TCHAR* Directions[]={TEXT("Bwd"),TEXT("Bwd_Left"),TEXT("Left"),TEXT("Fwd_Left"),TEXT("Fwd"),TEXT("Fwd_Right"),TEXT("Right"),TEXT("Bwd_Right"),TEXT("Bwd")};
 for(const TCHAR* Gait:{TEXT("Walk"),TEXT("Jog")})for(int D=0;D<8;++D)
 {
  // The source backward-diagonal walks are truncated (33 cm foot seam).
  // Interpolate the closed cardinal walks instead; never play those loops.
  if(FString(Gait)==TEXT("Walk")&&(D==1||D==7))continue;
  const FString RawPath=FString::Printf(TEXT("/Game/Fireline/LocomotionStudy/Raw/Ryan_MF_Rifle_%s_%s"),Gait,Directions[D]);
  auto* Raw=LoadObject<UAnimSequence>(nullptr,*RawPath);check(Raw);
  const FString Path=FString::Printf(TEXT("/Game/Fireline/LocomotionStudy/Ready/A_%s_%s"),Gait,Directions[D]);
  auto* Seq=CopyStudy(Raw,Path);Seq->SetSkeleton(Native->GetSkeleton());
  Seq->RetargetSource=NAME_None;Seq->SetRetargetSourceAsset(Native);Seq->UpdateRetargetSourceAssetData();
  const auto* Model=Raw->GetDataModel();const int Frames=Model->GetNumberOfKeys();
  TArray<FName> Tracks;Model->GetBoneTrackNames(Tracks);TArray<FName> HoldTracks;Hold->GetDataModel()->GetBoneTrackNames(HoldTracks);
  TArray<TArray<FTransform>> Baked;Baked.SetNum(Ref.GetNum());
  for(int F=0;F<Frames;++F)
  {
   TArray<FTransform> World;World.SetNum(Ref.GetNum());
   for(int I=0;I<Ref.GetNum();++I)
   {
    const FName Bone=Ref.GetBoneName(I);const int P=Ref.GetParentIndex(I);
    const FTransform Local=Tracks.Contains(Bone)?Model->GetBoneTrackTransform(Bone,FFrameNumber(F)):Ref.GetRefBonePose()[I];
    World[I]=P<0?Local:Local*World[P];
   }
   // Re-encode the solver's centimeter component pose into the ORIGINAL
   // scaled bind. No bone length/weight edits and no scale-one root on Ryan.
   TArray<FTransform> Scaled=World;
   for(int I=0;I<Scaled.Num();++I)Scaled[I].SetScale3D(RefCS[I].GetScale3D());
   Scaled[0]=RefCS[0];
   for(int I=0;I<Ref.GetNum();++I)
   {
    const FName Bone=Ref.GetBoneName(I);const FString Name=Bone.ToString();const int P=Ref.GetParentIndex(I);
    const bool Lower=Name==TEXT("Pelvis")||Name.StartsWith(TEXT("UpperLeg_"))||Name.StartsWith(TEXT("LowerLeg_"))||Name.StartsWith(TEXT("Foot"));
    FTransform Local=Lower?Scaled[I].GetRelativeTransform(Scaled[P]):HoldTracks.Contains(Bone)?Hold->GetDataModel()->GetBoneTrackTransform(Bone,FFrameNumber(0)):Ref.GetRefBonePose()[I];
    if(I==0)Local=Ref.GetRefBonePose()[0];
    Baked[I].Add(Local);
   }
  }
  auto& Controller=Seq->GetController();Controller.OpenBracket(FText::FromString(TEXT("Restore native scaled binding and accepted upper pose")),false);
  for(int I=0;I<Ref.GetNum();++I)
  {
   TArray<FVector3f> Positions,Scales;TArray<FQuat4f> Rotations;
   for(const auto& T:Baked[I]){Positions.Add(FVector3f(T.GetLocation()));Scales.Add(FVector3f(T.GetScale3D()));Rotations.Add(FQuat4f(T.GetRotation().GetNormalized()));}
   Controller.AddBoneCurve(Ref.GetBoneName(I),false);check(Controller.SetBoneTrackKeys(Ref.GetBoneName(I),Positions,Rotations,Scales,false));
  }
  Controller.CloseBracket(false);
  auto* Source=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Rifle/%s/MF_Rifle_%s_%s"),Gait,Gait,Directions[D]));check(Source);
  if(Source->AuthoredSyncMarkers.IsEmpty())
  {
   // The four diagonal jogs are rotated copies of their cardinal clip.
   // Verified at 61 phases: pelvis/foot heights differ by < .005 cm.
   const TCHAR* Cardinal=FString(Directions[D]).StartsWith(TEXT("Fwd"))?TEXT("Fwd"):TEXT("Bwd");
   Source=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Characters/Mannequins/Anims/Rifle/%s/MF_Rifle_%s_%s"),Gait,Gait,Cardinal));
  }
  check(Source&&Source->AuthoredSyncMarkers.Num()>=2&&FMath::Abs(Source->GetPlayLength()-Seq->GetPlayLength())<.001f);
  Seq->AuthoredSyncMarkers=Source->AuthoredSyncMarkers;Seq->RefreshSyncMarkerDataFromAuthored();
  Seq->bEnableRootMotion=false;Seq->bForceRootLock=true;Seq->PostEditChange();SaveLocomotion(Seq);
 }
 const FString Path=TEXT("/Game/Fireline/LocomotionStudy/Ready/BS_Directional");UPackage* Package=CreatePackage(*Path);
 auto* BS=LoadObject<UBlendSpace>(nullptr,*Path);if(!BS)BS=NewObject<UBlendSpace>(Package,TEXT("BS_Directional"),RF_Public|RF_Standalone);
 BS->SetSkeleton(Native->GetSkeleton());while(BS->GetNumberOfBlendSamples())BS->DeleteSample(BS->GetNumberOfBlendSamples()-1);
 auto& Direction=const_cast<FBlendParameter&>(BS->GetBlendParameter(0));Direction.DisplayName=TEXT("Direction");Direction.Min=-180;Direction.Max=180;Direction.GridNum=8;Direction.bWrapInput=true;
 auto& Speed=const_cast<FBlendParameter&>(BS->GetBlendParameter(1));Speed.DisplayName=TEXT("Speed");Speed.Min=0;Speed.Max=450;Speed.GridNum=5;
 for(int D=0;D<=8;++D)
 {
  BS->AddSample(Hold,FVector(-180+D*45,0,0));
  for(const TCHAR* Gait:{TEXT("Walk"),TEXT("Jog")})
  {
   if(FString(Gait)==TEXT("Walk")&&(D==1||D==7))continue;
   const FString Clip=FString::Printf(TEXT("/Game/Fireline/LocomotionStudy/Ready/A_%s_%s"),Gait,Directions[D]);
   BS->AddSample(LoadObject<UAnimSequence>(nullptr,*Clip),FVector(-180+D*45,FString(Gait)==TEXT("Walk")?180:450,0));
  }
 }
 BS->bAllowMarkerBasedSync=true;BS->TargetWeightInterpolationSpeedPerSec=8.f;
 BS->ValidateSampleData();BS->ResampleData();BS->PostEditChange();
 check(BS->GetUniqueMarkerNames()&&BS->GetUniqueMarkerNames()->Num()==2);SaveLocomotion(BS);
 UE_LOG(LogTemp,Display,TEXT("DIRECTIONAL_READY 14 closed native-bind clips; backward-diagonal walks blend cardinals"));
#endif
}

void URangeAssetTools::BuildContactPhaseAssets()
{
#if WITH_EDITOR
 // Separate derived packages: never rewrite the rejected baseline, source
 // animation tracks, native reference skeleton, mesh or weights.
 TArray<FString> Lines;check(FFileHelper::LoadFileToStringArray(Lines,*(FPaths::ProjectDir()/TEXT("Docs/Validation/contact-phase/markers.csv"))));
 TMap<FString,TArray<FAnimSyncMarker>> Markers;
 for(int I=1;I<Lines.Num();++I)
 {
  TArray<FString> Fields;Lines[I].ParseIntoArray(Fields,TEXT(","));check(Fields.Num()==3);
  FAnimSyncMarker Marker;Marker.MarkerName=FName(*Fields[1]);Marker.Time=FCString::Atof(*Fields[2]);Marker.TrackIndex=0;
  Markers.FindOrAdd(Fields[0]).Add(Marker);
 }
 check(Markers.Num()==14);
 TMap<UAnimSequence*,UAnimSequence*> Replacements;
 for(const auto& Pair:Markers)
 {
  auto* Source=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Fireline/LocomotionStudy/Ready/A_")+Pair.Key));check(Source);
  auto* Seq=CopyStudy(Source,TEXT("/Game/Fireline/LocomotionStudy/ContactPhase/A_")+Pair.Key);
  for(const auto& Marker:Pair.Value)check(Marker.Time>=0&&Marker.Time<Seq->GetPlayLength());
  Seq->AuthoredSyncMarkers=Pair.Value;Seq->RefreshSyncMarkerDataFromAuthored();Seq->PostEditChange();SaveLocomotion(Seq);
  Replacements.Add(Source,Seq);
 }
 auto* Original=LoadObject<UBlendSpace>(nullptr,TEXT("/Game/Fireline/LocomotionStudy/Ready/BS_Directional"));check(Original);
 auto* Candidate=CopyStudy(Original,TEXT("/Game/Fireline/LocomotionStudy/ContactPhase/BS_Directional"));
 while(Candidate->GetNumberOfBlendSamples())Candidate->DeleteSample(Candidate->GetNumberOfBlendSamples()-1);
 for(const auto& Sample:Original->GetBlendSamples())
 {
  auto* Replacement=Replacements.Find(Sample.Animation);
  Candidate->AddSample(Replacement?*Replacement:Sample.Animation.Get(),Sample.SampleValue);
 }
 Candidate->ValidateSampleData();Candidate->ResampleData();Candidate->PostEditChange();
 check(Candidate->GetUniqueMarkerNames()&&Candidate->GetUniqueMarkerNames()->Num()==2);SaveLocomotion(Candidate);
 UE_LOG(LogTemp,Display,TEXT("CONTACT_PHASE_READY 14 marker-only copies; original candidate preserved"));
#endif
}
