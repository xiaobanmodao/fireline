#include "RangeGame.h"
#if WITH_EDITOR
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimSequence.h"
#include "Animation/Skeleton.h"
#include "Animation/BlendSpace.h"
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"
#include "TwoBoneIK.h"
#include "AnimationRuntime.h"
#include "Animation/AnimData/CurveIdentifier.h"

namespace
{
const FString Study=TEXT("/Game/Fireline/LocomotionStudy/StrideCoverage/");
void SaveStride(UObject* Asset)
{
 Asset->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 check(UPackage::SavePackage(Asset->GetOutermost(),Asset,*FPackageName::LongPackageNameToFilename(Asset->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args));
}
template<class T>T* CopyStride(T* Source,const FString& Path)
{
 if(auto* Existing=LoadObject<T>(nullptr,*Path))return Existing;
 return DuplicateObject<T>(Source,CreatePackage(*Path),*FPackageName::GetShortName(Path));
}
TArray<FTransform> Components(UAnimSequence* Seq,const FReferenceSkeleton& Ref,int Frame)
{
 TArray<FName> Tracks;Seq->GetDataModel()->GetBoneTrackNames(Tracks);TArray<FTransform> Result;
 for(int I=0;I<Ref.GetNum();++I)
 {
  const FName B=Ref.GetBoneName(I);const int P=Ref.GetParentIndex(I);
  const FTransform T=Tracks.Contains(B)?Seq->GetDataModel()->GetBoneTrackTransform(B,FFrameNumber(Frame)):Ref.GetRefBonePose()[I];
  Result.Add(P<0?T:T*Result[P]);
 }
 return Result;
}
void WriteFrames(UAnimSequence* Seq,const FReferenceSkeleton& Ref,const TArray<TArray<FTransform>>& LocalFrames)
{
 auto& C=Seq->GetController();C.OpenBracket(FText::FromString(TEXT("Isolated native-bind stride adaptation")),false);
 for(int I=0;I<Ref.GetNum();++I)
 {
  TArray<FVector3f> P,S;TArray<FQuat4f> Q;
  for(const auto& Frame:LocalFrames){const auto& T=Frame[I];P.Add(FVector3f(T.GetLocation()));Q.Add(FQuat4f(T.GetRotation().GetNormalized()));S.Add(FVector3f(T.GetScale3D()));}
  C.AddBoneCurve(Ref.GetBoneName(I),false);check(C.SetBoneTrackKeys(Ref.GetBoneName(I),P,Q,S,false));
 }
 C.CloseBracket(false);Seq->bEnableRootMotion=false;Seq->bForceRootLock=true;
}
UAnimSequence* BaseClip(const FString& Name)
{
 const bool NewDiagonal=Name==TEXT("Walk_Bwd_Right")||Name==TEXT("Walk_Bwd_Left");
 return LoadObject<UAnimSequence>(nullptr,*(NewDiagonal?Study+TEXT("Base/A_")+Name:TEXT("/Game/Fireline/LocomotionStudy/Ready/A_")+Name));
}
}
#endif

void URangeAssetTools::BuildStrideCoverageAssets(bool PrepareOnly)
{
#if WITH_EDITOR
 auto* Native=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction"));check(Native);
 auto* Hold=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/SycgffM4/A_M4_Idle"));check(Hold);
 const auto& Ref=Native->GetRefSkeleton();
 if(PrepareOnly)
 {
  TArray<FTransform> Bind;for(int I=0;I<Ref.GetNum();++I){int P=Ref.GetParentIndex(I);Bind.Add(P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bind[P]);}
  TArray<FName> HoldTracks;Hold->GetDataModel()->GetBoneTrackNames(HoldTracks);
  for(const TCHAR* Direction:{TEXT("Bwd_Right"),TEXT("Bwd_Left")})
  {
   auto* Raw=LoadObject<UAnimSequence>(nullptr,*(Study+TEXT("Raw/Ryan_MF_Pistol_Walk_")+Direction));check(Raw);
   // Start with native animation metadata as well as native skeleton. Copying
   // the proxy asset also copies its editor FK ControlRig binding, which must
   // never be carried across to the production skeleton.
   auto* Seq=CopyStride(Hold,Study+TEXT("Base/A_Walk_")+Direction);
   Seq->SetSkeleton(Native->GetSkeleton());Seq->RetargetSource=NAME_None;Seq->SetRetargetSourceAsset(Native);Seq->UpdateRetargetSourceAssetData();
   Seq->GetController().SetFrameRate(Raw->GetDataModel()->GetFrameRate(),false);
   Seq->GetController().SetNumberOfFrames(Raw->GetDataModel()->GetNumberOfFrames(),false);
   TArray<TArray<FTransform>> Frames;
   for(int F=0;F<Raw->GetDataModel()->GetNumberOfKeys();++F)
   {
    auto World=Components(Raw,Ref,F);for(int I=0;I<World.Num();++I)World[I].SetScale3D(Bind[I].GetScale3D());World[0]=Bind[0];
    TArray<FTransform> Local;
    for(int I=0;I<Ref.GetNum();++I)
    {
     const FName B=Ref.GetBoneName(I);const FString N=B.ToString();const int P=Ref.GetParentIndex(I);
     const bool Lower=N==TEXT("Pelvis")||N.StartsWith(TEXT("UpperLeg_"))||N.StartsWith(TEXT("LowerLeg_"))||N.StartsWith(TEXT("Foot"));
     Local.Add(I==0?Bind[0]:Lower?World[I].GetRelativeTransform(World[P]):HoldTracks.Contains(B)?Hold->GetDataModel()->GetBoneTrackTransform(B,FFrameNumber(0)):Ref.GetRefBonePose()[I]);
    }
    Frames.Add(MoveTemp(Local));
   }
   WriteFrames(Seq,Ref,Frames);Seq->PostEditChange();SaveStride(Seq);
  }
  return;
 }
 TArray<FString> Lines;check(FFileHelper::LoadFileToStringArray(Lines,*(FPaths::ProjectDir()/TEXT("Docs/Validation/stride-coverage/calibration.csv"))));
 TMap<FString,TArray<FAnimSyncMarker>> Markers;TArray<FString> MarkerLines;
 check(FFileHelper::LoadFileToStringArray(MarkerLines,*(FPaths::ProjectDir()/TEXT("Docs/Validation/stride-coverage/markers.csv"))));
 for(int I=1;I<MarkerLines.Num();++I){TArray<FString> V;MarkerLines[I].ParseIntoArray(V,TEXT(","));check(V.Num()==3);FAnimSyncMarker M;M.MarkerName=FName(*V[1]);M.Time=FCString::Atof(*V[2]);M.TrackIndex=0;Markers.FindOrAdd(V[0]).Add(M);}
 check(Lines.Num()==17&&Markers.Num()==16);
 for(int Line=1;Line<Lines.Num();++Line)
 {
  TArray<FString> V;Lines[Line].ParseIntoArray(V,TEXT(","));check(V.Num()==3);const FString Name=V[0];const float Scale=FCString::Atof(*V[1]);const float Angle=FMath::DegreesToRadians(FCString::Atof(*V[2]));
  check(Scale>.35f&&Scale<=1.05f);const FVector Direction(-FMath::Sin(Angle),FMath::Cos(Angle),0);
  auto* Base=BaseClip(Name);check(Base);auto* Seq=CopyStride(Hold,Study+TEXT("Ready/A_")+Name);
  Seq->SetRetargetSourceAsset(Native);Seq->UpdateRetargetSourceAssetData();
  Seq->GetController().SetFrameRate(Base->GetDataModel()->GetFrameRate(),false);
  Seq->GetController().SetNumberOfFrames(Base->GetDataModel()->GetNumberOfFrames(),false);
  TArray<TArray<FTransform>> Frames;
  for(int F=0;F<Base->GetDataModel()->GetNumberOfKeys();++F)
  {
   const auto Original=Components(Base,Ref,F);auto Adjusted=Original;
   for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
   {
    const int U=Ref.FindBoneIndex(FName(*(FString(TEXT("UpperLeg_"))+Side))),K=Ref.FindBoneIndex(FName(*(FString(TEXT("LowerLeg_"))+Side))),A=Ref.FindBoneIndex(FName(*(FString(TEXT("Foot_"))+Side)));check(U>=0&&K>=0&&A>=0);
    const FVector Hip=Original[U].GetLocation(),Knee=Original[K].GetLocation(),Foot=Original[A].GetLocation();
    // Epic's stride-plane construction: scale only the horizontal component
    // along movement, about the hip's projection. Preserve foot height/rotation.
    const FVector Target=Foot+Direction*((Foot-Hip).Dot(Direction)*(Scale-1.f));
    FVector NewKnee,NewFoot;const float Upper=(Knee-Hip).Size(),Lower=(Foot-Knee).Size();
    const FVector Axis=(Foot-Hip).GetSafeNormal();FVector Bend=(Knee-Hip)-Axis*((Knee-Hip).Dot(Axis));
    check(!Bend.IsNearlyZero());const FVector Pole=Hip+Bend.GetSafeNormal()*100.f;
    AnimationCore::SolveTwoBoneIK(Hip,Knee,Foot,Pole,Target,NewKnee,NewFoot,Upper,Lower,false,1.,1.);
    check(FVector::Dist(Target,NewFoot)<.01f);
    Adjusted[U].SetRotation((FQuat::FindBetweenNormals((Knee-Hip).GetSafeNormal(),(NewKnee-Hip).GetSafeNormal())*Original[U].GetRotation()).GetNormalized());
    Adjusted[K].SetLocation(NewKnee);Adjusted[K].SetRotation((FQuat::FindBetweenNormals((Foot-Knee).GetSafeNormal(),(NewFoot-NewKnee).GetSafeNormal())*Original[K].GetRotation()).GetNormalized());
    Adjusted[A].SetLocation(NewFoot);
   }
   // Carry remaining branch bones rigidly with their corrected parent.
   TArray<FTransform> Local;
   for(int I=0;I<Ref.GetNum();++I)
   {
    const int P=Ref.GetParentIndex(I);const FString N=Ref.GetBoneName(I).ToString();
    const bool Main=N.StartsWith(TEXT("UpperLeg_"))||N.StartsWith(TEXT("LowerLeg_"))||N==TEXT("Foot_L")||N==TEXT("Foot_R");
    if(P>=0&&!Main)Adjusted[I]=Original[I].GetRelativeTransform(Original[P])*Adjusted[P];
    Local.Add(P<0?Adjusted[I]:Adjusted[I].GetRelativeTransform(Adjusted[P]));
   }
   Frames.Add(MoveTemp(Local));
  }
  WriteFrames(Seq,Ref,Frames);Seq->AuthoredSyncMarkers=Markers[Name];Seq->RefreshSyncMarkerDataFromAuthored();Seq->PostEditChange();SaveStride(Seq);
 }
 auto* Original=LoadObject<UBlendSpace>(nullptr,TEXT("/Game/Fireline/LocomotionStudy/Ready/BS_Directional"));check(Original);
 auto* BS=CopyStride(Original,Study+TEXT("Ready/BS_Directional"));while(BS->GetNumberOfBlendSamples())BS->DeleteSample(BS->GetNumberOfBlendSamples()-1);
 const TCHAR* Directions[]={TEXT("Bwd"),TEXT("Bwd_Left"),TEXT("Left"),TEXT("Fwd_Left"),TEXT("Fwd"),TEXT("Fwd_Right"),TEXT("Right"),TEXT("Bwd_Right"),TEXT("Bwd")};
 for(int D=0;D<=8;++D){BS->AddSample(Hold,FVector(-180+D*45,0,0));for(const TCHAR* G:{TEXT("Walk"),TEXT("Jog")})BS->AddSample(LoadObject<UAnimSequence>(nullptr,*(Study+TEXT("Ready/A_")+G+TEXT("_")+Directions[D])),FVector(-180+D*45,FString(G)==TEXT("Walk")?180:450,0));}
 BS->ValidateSampleData();BS->ResampleData();BS->PostEditChange();SaveStride(BS);
 UE_LOG(LogTemp,Display,TEXT("STRIDE_COVERAGE_BUILT 16 closed gait samples, no native bind or source edits"));
#endif
}

void URangeAssetTools::BuildCMUSideStepAssets()
{
#if WITH_EDITOR
 const bool ContactFix=FParse::Param(FCommandLine::Get(),TEXT("FirelineLocomotionContactFixStudy"));
 const FString Folder=ContactFix?TEXT("/Game/Fireline/LocomotionStudy/CMUSideStep/ContactFix/"):TEXT("/Game/Fireline/LocomotionStudy/CMUSideStep/");
 auto* Native=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction"));check(Native);
 auto* Hold=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/SycgffM4/A_M4_Idle"));check(Hold);
 const auto& Ref=Native->GetRefSkeleton();
 const TArray<FTransform> Base=Components(Hold,Ref,0);
 struct FSideFrame {float Bob=0,Yaw=0,FootPitchL=0,FootPitchR=0,ChestRoll=0;FVector KneeL,FootL,KneeR,FootR,KneeBaseL,FootBaseL,KneeBaseR,FootBaseR;};
 TMap<FString,TArray<FSideFrame>> Tracks;
 TArray<FString> Lines;
 const FString Csv=FPaths::ProjectDir()/TEXT("Docs/Validation/cmu-side-step/native-leg-deltas.csv");
 check(FFileHelper::LoadFileToStringArray(Lines,*Csv)&&Lines.Num()>2);
 for(int Line=1;Line<Lines.Num();++Line)
 {
  TArray<FString> V;Lines[Line].ParseIntoArray(V,TEXT(","));check(V.Num()==28);
  const int Index=FCString::Atoi(*V[1]);auto& Frames=Tracks.FindOrAdd(V[0]);check(Index==Frames.Num());
  FSideFrame F;F.Bob=FCString::Atof(*V[3]);F.Yaw=FCString::Atof(*V[4]);
  auto Point=[&](int Start){return FVector(FCString::Atof(*V[Start]),FCString::Atof(*V[Start+1]),FCString::Atof(*V[Start+2]));};
  F.KneeL=Point(5);F.FootL=Point(8);F.KneeR=Point(11);F.FootR=Point(14);Frames.Add(F);
  auto BaseXY=[&](int Start){return FVector(FCString::Atof(*V[Start]),FCString::Atof(*V[Start+1]),0);};
  Frames.Last().KneeBaseL=BaseXY(17);Frames.Last().FootBaseL=BaseXY(19);
  Frames.Last().KneeBaseR=BaseXY(21);Frames.Last().FootBaseR=BaseXY(23);
  Frames.Last().FootPitchL=FCString::Atof(*V[25]);Frames.Last().FootPitchR=FCString::Atof(*V[26]);Frames.Last().ChestRoll=FCString::Atof(*V[27]);
 }
 check(Tracks.Num()==2);
 const int Pelvis=Ref.FindBoneIndex(TEXT("Pelvis"));check(Pelvis>=0);
 for(const TCHAR* Direction:{TEXT("Left"),TEXT("Right")})
 {
  const FString Name(Direction);const auto* Source=Tracks.Find(Name);check(Source&&Source->Num()>20);
  auto* Seq=CopyStride(Hold,Folder+TEXT("Ready/A_Walk_")+Name);check(Seq);
  Seq->SetSkeleton(Native->GetSkeleton());Seq->RetargetSource=NAME_None;
  // The selected closed cycles advance roughly 112.6/115.6 cm/s. Retiming to the
  // 180 cm/s walk sample avoids one cycle of actor travel outrunning the feet.
  Seq->RateScale=Name==TEXT("Right")?180.f/112.6f:180.f/115.6f;
  Seq->SetRetargetSourceAsset(Native);Seq->UpdateRetargetSourceAssetData();
  Seq->GetController().SetFrameRate(FFrameRate(60,1),false);
  Seq->GetController().SetNumberOfFrames((Source->Num()-1)*(ContactFix?2:1),false);
  TArray<TArray<FTransform>> AllFrames;float MaxFootError=0,MaxLengthError=0;int WorstFrame=-1;FString WorstSide;
  for(int FrameIndex=0;FrameIndex<Source->Num();++FrameIndex)
  {
   const FSideFrame& F=(*Source)[FrameIndex];
   TArray<FTransform> Pose=Base;
   const FQuat Turn(FVector::UpVector,FMath::DegreesToRadians(F.Yaw));
   // Keep the hips high enough for an armed strafe, with a small knee reserve
   // for the widest source step. The earlier 12 cm drop read as a crouch.
   constexpr float StanceSettleCm=2.f;
   Pose[Pelvis].SetLocation(Base[Pelvis].GetLocation()+FVector(0,0,F.Bob-StanceSettleCm));
   Pose[Pelvis].SetRotation((Turn*Base[Pelvis].GetRotation()).GetNormalized());
   for(int I=Pelvis+1;I<Ref.GetNum();++I)
   {
    const int P=Ref.GetParentIndex(I);
    if(P>=0)Pose[I]=Base[I].GetRelativeTransform(Base[P])*Pose[P];
   }
   const int HipL=Ref.FindBoneIndex(TEXT("UpperLeg_L")),HipR=Ref.FindBoneIndex(TEXT("UpperLeg_R"));
   check(HipL>=0&&HipR>=0);
   const float RawLeftX=(Pose[HipL].GetLocation()+Turn.RotateVector(F.FootBaseL+F.FootL)).X;
   const float RawRightX=(Pose[HipR].GetLocation()+Turn.RotateVector(F.FootBaseR+F.FootR)).X;
   // Preserve the source step phase but prevent the two full-width boots from
   // occupying the same space when the mocap foot markers pass close together.
   const float FootSpacingCorrection=FMath::Max(0.f,12.f-(RawLeftX-RawRightX))*.5f;
   for(const TCHAR* Side:{TEXT("L"),TEXT("R")})
   {
    const FString S(Side);
    const int U=Ref.FindBoneIndex(FName(*(FString(TEXT("UpperLeg_"))+S)));
    const int K=Ref.FindBoneIndex(FName(*(FString(TEXT("LowerLeg_"))+S)));
    const int A=Ref.FindBoneIndex(FName(*(FString(TEXT("Foot_"))+S)));
    check(U>=0&&K>=0&&A>=0);
    const FVector Hip=Pose[U].GetLocation(),Knee=Pose[K].GetLocation(),Foot=Pose[A].GetLocation();
    const FVector SourceKnee=S==TEXT("L")?F.KneeL:F.KneeR;
    const FVector SourceFoot=S==TEXT("L")?F.FootL:F.FootR;
    const FVector KneeBase=S==TEXT("L")?F.KneeBaseL:F.KneeBaseR;
    const FVector FootBase=S==TEXT("L")?F.FootBaseL:F.FootBaseR;
    const FVector DonorKnee=Hip+Turn.RotateVector(KneeBase+SourceKnee);
    const FVector DonorFoot=Hip+Turn.RotateVector(FootBase+SourceFoot);
    const FVector AimKnee(DonorKnee.X,DonorKnee.Y,Knee.Z+SourceKnee.Z);
    const FVector Target(DonorFoot.X+(S==TEXT("L")?FootSpacingCorrection:-FootSpacingCorrection),DonorFoot.Y,Foot.Z+SourceFoot.Z+StanceSettleCm);
    const float Upper=(Knee-Hip).Size(),Lower=(Foot-Knee).Size();
    const FVector Axis=(Target-Hip).GetSafeNormal();
    const FVector SourceBend=FVector::VectorPlaneProject(AimKnee-Hip,Axis).GetSafeNormal();
    const FVector ForwardBend=FVector::VectorPlaneProject(Turn.RotateVector(FVector::YAxisVector),Axis).GetSafeNormal();
    check(!ForwardBend.IsNearlyZero());
    // The donor aim briefly changes sides at the right-cycle seam. Anchor the
    // bend to the anatomical forward plane; source variation may only nudge it.
    const FVector Pole=Hip+(ForwardBend*.85f+SourceBend*.15f).GetSafeNormal()*100.f;
    FVector NewKnee,NewFoot;
    AnimationCore::SolveTwoBoneIK(Hip,Knee,Foot,Pole,Target,NewKnee,NewFoot,Upper,Lower,false,1.,1.);
    const float FootError=FVector::Dist(NewFoot,Target);
    if(FootError>MaxFootError)
    {
     MaxFootError=FootError;WorstFrame=FrameIndex;WorstSide=S;
     UE_LOG(LogTemp,Display,TEXT("CMU_REACH %s frame=%d side=%s target_distance=%.3f max_length=%.3f original_distance=%.3f donor_delta=%s"),*Name,FrameIndex,*S,FVector::Dist(Hip,Target),Upper+Lower,FVector::Dist(Hip,Foot),*SourceFoot.ToCompactString());
    }
    MaxLengthError=FMath::Max(MaxLengthError,FMath::Max(FMath::Abs(FVector::Dist(Hip,NewKnee)-Upper),FMath::Abs(FVector::Dist(NewKnee,NewFoot)-Lower)));
    Pose[U].SetRotation((FQuat::FindBetweenNormals((Knee-Hip).GetSafeNormal(),(NewKnee-Hip).GetSafeNormal())*Pose[U].GetRotation()).GetNormalized());
    Pose[K].SetLocation(NewKnee);
    Pose[K].SetRotation((FQuat::FindBetweenNormals((Foot-Knee).GetSafeNormal(),(NewFoot-NewKnee).GetSafeNormal())*Pose[K].GetRotation()).GetNormalized());
    Pose[A].SetLocation(NewFoot);
    const float FootPitch=S==TEXT("L")?F.FootPitchL:F.FootPitchR;
    Pose[A].SetRotation((FQuat(Turn.RotateVector(FVector::XAxisVector),FMath::DegreesToRadians(FootPitch))*Pose[A].GetRotation()).GetNormalized());
   }
   TArray<FTransform> Local;
   for(int I=0;I<Ref.GetNum();++I)
   {
    const int P=Ref.GetParentIndex(I);const FString N=Ref.GetBoneName(I).ToString();
    const bool Main=N.StartsWith(TEXT("UpperLeg_"))||N.StartsWith(TEXT("LowerLeg_"))||N==TEXT("Foot_L")||N==TEXT("Foot_R")||N==TEXT("Pelvis");
    if(P>=0&&!Main)Pose[I]=Base[I].GetRelativeTransform(Base[P])*Pose[P];
    Local.Add(P<0?Pose[I]:Pose[I].GetRelativeTransform(Pose[P]));
   }
   AllFrames.Add(MoveTemp(Local));
  }
  // Reject impossible reaches instead of silently masking foot-slide with IK clamping.
  UE_LOG(LogTemp,Display,TEXT("CMU_SIDE_STEP %s frames=%d foot_error=%.3f worst_frame=%d worst_side=%s length_error=%.3f"),*Name,AllFrames.Num(),MaxFootError,WorstFrame,*WorstSide,MaxLengthError);
  check(MaxFootError<2.f&&MaxLengthError<.1f);
  if(ContactFix)
  {
   // Existing directional samples contain two strides and four alternating
   // L/R contacts. Repeat this closed mocap cycle without changing its rate;
   // mismatching marker patterns disables UE marker synchronization entirely.
   const int Count=AllFrames.Num()-1;
   for(int I=1;I<=Count;++I){auto Copy=AllFrames[I];AllFrames.Add(MoveTemp(Copy));}
  }
  WriteFrames(Seq,Ref,AllFrames);
  TArray<FRichCurveKey> RollKeys;
  for(int I=0;I<AllFrames.Num();++I){const int F=I%(Source->Num()-1);FRichCurveKey Key(I/60.f,(*Source)[F].ChestRoll);Key.InterpMode=RCIM_Linear;RollKeys.Add(Key);}
  const FAnimationCurveIdentifier RollId(TEXT("FL_SideRoll"),ERawCurveTrackTypes::RCT_Float);
  Seq->GetController().AddCurve(RollId,4,false);
  check(Seq->GetController().SetCurveKeys(RollId,RollKeys,false));
  if(ContactFix)
  {
   Seq->AuthoredSyncMarkers.Reset();
   const int Count=Source->Num()-1;
   for(int Side=0;Side<2;++Side)
   {
    auto Height=[&](int I){return (*Source)[I].Bob+(Side?(*Source)[I].FootR.Z:(*Source)[I].FootL.Z);};
    float Min=MAX_flt,Max=-MAX_flt;for(int I=0;I<Count;++I){Min=FMath::Min(Min,Height(I));Max=FMath::Max(Max,Height(I));}
    TArray<FRichCurveKey> Keys;bool Armed=false;
    for(int I=0;I<=Count*3;++I)
    {
     const float H=Height(I%Count)-Min;
     if(H>(Max-Min)*.6f)Armed=true;
     if(Armed&&H<=1.5f)
     {
      if(I>=Count&&I<Count*3){FAnimSyncMarker M;M.MarkerName=Side?TEXT("R"):TEXT("L");M.Time=(I-Count)/60.f;M.TrackIndex=0;Seq->AuthoredSyncMarkers.Add(M);}
      Armed=false;
     }
     if(I<=Count*2){FRichCurveKey K(I/60.f,1.f-FMath::SmoothStep(.8f,2.2f,H));K.InterpMode=RCIM_Linear;Keys.Add(K);}
    }
    const FAnimationCurveIdentifier Id(Side?TEXT("FL_Plant_R"):TEXT("FL_Plant_L"),ERawCurveTrackTypes::RCT_Float);
    Seq->GetController().AddCurve(Id,4,false);check(Seq->GetController().SetCurveKeys(Id,Keys,false));
   }
   Seq->AuthoredSyncMarkers.Sort([](const FAnimSyncMarker& A,const FAnimSyncMarker& B){return A.Time<B.Time;});
   check(Seq->AuthoredSyncMarkers.Num()==4);Seq->RefreshSyncMarkerDataFromAuthored();
  }
  Seq->PostEditChange();SaveStride(Seq);
  if(ContactFix)
  {
   // The legacy lateral jog rotates the leg frame ninety degrees away from
   // this real side step. Mixing it into CMU walk is the 40-degree reversal
   // spike. Use the SAME captured gait/contact phase at both lateral speeds;
   // extend lateral excursion (fixed-length IK) before distance retiming.
   auto* Jog=CopyStride(Seq,Folder+TEXT("Ready/A_Jog_")+Name);
   constexpr double Stride=2.2;
   Jog->RateScale=Seq->RateScale*(450./180.)/Stride;
   TArray<TArray<FTransform>> JogFrames=AllFrames;
   FVector Mean[2]={FVector::ZeroVector,FVector::ZeroVector};
   for(int F=0;F<AllFrames.Num()-1;++F)
   {
    TArray<FTransform> W=AllFrames[F];for(int I=1;I<W.Num();++I)W[I]*=W[Ref.GetParentIndex(I)];
    for(int Side=0;Side<2;++Side)Mean[Side]+=W[Ref.FindBoneIndex(Side?TEXT("Foot_R"):TEXT("Foot_L"))].GetLocation()/double(AllFrames.Num()-1);
   }
   auto WarpTarget=[&](const TArray<FTransform>& W,int Side)
   {
    const FVector L=W[Ref.FindBoneIndex(TEXT("Foot_L"))].GetLocation(),R=W[Ref.FindBoneIndex(TEXT("Foot_R"))].GetLocation();
    const double LX=Mean[0].X+(L.X-Mean[0].X)*Stride,RX=Mean[1].X+(R.X-Mean[1].X)*Stride;
    const double Spacing=FMath::Max(0.,12.-(LX-RX))*.5;
    FVector Target=Side?R:L;Target.X=Side?RX-Spacing:LX+Spacing;return Target;
   };
   // Choose one pelvis reserve from measured limb lengths, not a per-frame
   // collapse at full extension. Keep each original boot height unchanged.
   double Reserve=0;
   for(const auto& Local:JogFrames)
   {
    TArray<FTransform> W=Local;for(int I=1;I<W.Num();++I)W[I]*=W[Ref.GetParentIndex(I)];
    for(int Side=0;Side<2;++Side)
    {
     const FString S=Side?TEXT("R"):TEXT("L");auto Id=[&](const TCHAR* N){return Ref.FindBoneIndex(*FString::Printf(TEXT("%s_%s"),N,*S));};
     const FVector H=W[Id(TEXT("UpperLeg"))].GetLocation(),J=W[Id(TEXT("LowerLeg"))].GetLocation(),E=W[Id(TEXT("Foot"))].GetLocation();
     const FVector Target=WarpTarget(W,Side);
     const double L1=(J-H).Size(),L2=(E-J).Size(),D2=L1*L1+L2*L2+2*L1*L2*FMath::Cos(FMath::DegreesToRadians(25.));
     Reserve=FMath::Max(Reserve,H.Z-E.Z-FMath::Sqrt(FMath::Max(0.,D2-FVector(H.X-Target.X,H.Y-Target.Y,0).SizeSquared())));
    }
   }
   check(Reserve<12.);
   double MaxWarpError=0;
   for(auto& Local:JogFrames)
   {
    TArray<FTransform> W=Local;for(int I=1;I<W.Num();++I)W[I]*=W[Ref.GetParentIndex(I)];
    const auto BeforeDrop=W;
    for(int B=0;B<W.Num();++B)for(int P=B;P>=0;P=Ref.GetParentIndex(P))if(P==Pelvis){W[B].AddToTranslation(-FVector::UpVector*Reserve);break;}
    for(int Side=0;Side<2;++Side)
    {
     const FString S=Side?TEXT("R"):TEXT("L");auto Id=[&](const TCHAR* N){return Ref.FindBoneIndex(*FString::Printf(TEXT("%s_%s"),N,*S));};
     const int U=Id(TEXT("UpperLeg")),K=Id(TEXT("LowerLeg")),A=Id(TEXT("Foot"));
     const FVector H=W[U].GetLocation(),J=W[K].GetLocation(),E=W[A].GetLocation();
     const FVector Target=WarpTarget(BeforeDrop,Side);
     FVector NJ,NE;AnimationCore::SolveTwoBoneIK(H,J,E,J,Target,NJ,NE,false,1.,1.);
     MaxWarpError=FMath::Max(MaxWarpError,FVector::Dist(Target,NE));
     const FQuat UQ=FQuat::FindBetweenNormals((J-H).GetSafeNormal(),(NJ-H).GetSafeNormal()),LQ=FQuat::FindBetweenNormals((E-J).GetSafeNormal(),(NE-NJ).GetSafeNormal());
     for(int B=0;B<W.Num();++B)
     {
      auto Child=[&](int Root){for(int P=B;P>=0;P=Ref.GetParentIndex(P))if(P==Root)return true;return false;};
      if(Child(A))W[B].AddToTranslation(NE-E);
      else if(Child(K)){W[B].SetLocation(NJ+LQ.RotateVector(W[B].GetLocation()-J));W[B].SetRotation((LQ*W[B].GetRotation()).GetNormalized());}
      else if(Child(U)){W[B].SetLocation(H+UQ.RotateVector(W[B].GetLocation()-H));W[B].SetRotation((UQ*W[B].GetRotation()).GetNormalized());}
     }
    }
    for(int I=0;I<W.Num();++I)Local[I]=I?W[I].GetRelativeTransform(W[Ref.GetParentIndex(I)]):W[I];
   }
   check(MaxWarpError<2.);
   WriteFrames(Jog,Ref,JogFrames);Jog->RefreshSyncMarkerDataFromAuthored();Jog->PostEditChange();SaveStride(Jog);
   UE_LOG(LogTemp,Display,TEXT("CMU_CONTACT_JOG %s stride=%.2f rate=%.4f reach_error=%.4f pelvis_reserve=%.4f"),*Name,Stride,Jog->RateScale,MaxWarpError,Reserve);
  }

 }
 auto* Original=LoadObject<UBlendSpace>(nullptr,TEXT("/Game/Fireline/LocomotionStudy/StrideCoverage/Ready/BS_Directional"));check(Original);
 auto* BS=CopyStride(Original,Folder+TEXT("Ready/BS_Directional"));
 // The donor walk and legacy jog have different phases. Shorten stale sample
 // carry during a fast reversal while retaining a brief eased cross-fade.
 BS->TargetWeightInterpolationSpeedPerSec=16.f;
 BS->bTargetWeightInterpolationEaseInOut=true;
 while(BS->GetNumberOfBlendSamples())BS->DeleteSample(BS->GetNumberOfBlendSamples()-1);
 const TCHAR* Directions[]={TEXT("Bwd"),TEXT("Bwd_Left"),TEXT("Left"),TEXT("Fwd_Left"),TEXT("Fwd"),TEXT("Fwd_Right"),TEXT("Right"),TEXT("Bwd_Right"),TEXT("Bwd")};
 for(int D=0;D<=8;++D)
 {
  BS->AddSample(Hold,FVector(-180+D*45,0,0));
  for(const TCHAR* G:{TEXT("Walk"),TEXT("Jog")})
  {
   const FString N=FString(G)+TEXT("_")+Directions[D];
   const bool New=(ContactFix||FString(G)==TEXT("Walk"))&&(FString(Directions[D])==TEXT("Left")||FString(Directions[D])==TEXT("Right"));
   auto* Clip=LoadObject<UAnimSequence>(nullptr,*(New?Folder+TEXT("Ready/A_")+N:TEXT("/Game/Fireline/LocomotionStudy/StrideCoverage/Ready/A_")+N));check(Clip);
   BS->AddSample(Clip,FVector(-180+D*45,FString(G)==TEXT("Walk")?180:450,0));
  }
 }
 BS->ValidateSampleData();BS->ResampleData();BS->PostEditChange();SaveStride(BS);
 UE_LOG(LogTemp,Display,TEXT("CMU_SIDE_STEP_BUILT two original-mocap lateral walk clips, isolated study"));
#endif
}

void URangeAssetTools::BuildBodyCoordinationAssets()
{
#if WITH_EDITOR
 const FString Folder=TEXT("/Game/Fireline/LocomotionStudy/BodyCoordination/");
 auto* Native=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction"));check(Native);
 auto* Hold=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/SycgffM4/A_M4_Idle"));check(Hold);
 const auto& Ref=Native->GetRefSkeleton();const int Chest=Ref.FindBoneIndex(TEXT("Chest"));check(Chest>=0);
 const auto HoldCS=Components(Hold,Ref,0);
 const TCHAR* Directions[]={TEXT("Bwd"),TEXT("Bwd_Left"),TEXT("Left"),TEXT("Fwd_Left"),TEXT("Fwd"),TEXT("Fwd_Right"),TEXT("Right"),TEXT("Bwd_Right"),TEXT("Bwd")};
 for(const TCHAR* Gait:{TEXT("Walk"),TEXT("Jog")})for(int D=0;D<8;++D)
 {
  const FString Name=FString(Gait)+TEXT("_")+Directions[D];
  const bool Pistol=Name==TEXT("Walk_Bwd_Left")||Name==TEXT("Walk_Bwd_Right");
  auto* Base=LoadObject<UAnimSequence>(nullptr,*(Study+TEXT("Ready/A_")+Name));
  auto* Raw=LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("Raw/Ryan_MF_")+(Pistol?TEXT("Pistol_"):TEXT("Rifle_"))+Name));check(Base&&Raw);
  check(Base->GetDataModel()->GetNumberOfKeys()==Raw->GetDataModel()->GetNumberOfKeys());
  auto* Seq=CopyStride(Base,Folder+TEXT("Ready/A_")+Name);
  TArray<TArray<FTransform>> RawFrames;
  FQuat Mean(0,0,0,0),Anchor=FQuat::Identity;
  const int Count=Raw->GetDataModel()->GetNumberOfKeys();
  for(int F=0;F<Count;++F)
  {
   RawFrames.Add(Components(Raw,Ref,F));const FQuat Q=RawFrames.Last()[Chest].GetRotation();
   if(F==0)Anchor=Q;
   // Do not double-count the repeated loop endpoint in the stance reference.
   if(F<Count-1)Mean+=((Q|Anchor)<0)?Q*(-1.):Q;
  }
  Mean.Normalize();const FQuat Registration=(HoldCS[Chest].GetRotation()*Mean.Inverse()).GetNormalized();
  // Register the donor's average carry stance to our accepted weapon/body
  // reference. Apply the SAME rigid orientation offset to BOTH spine levels;
  // retain their relative motion and timing instead of centering each joint
  // independently or forcing the arms to absorb a different donor grip.
  TArray<TArray<FTransform>> Frames;
  for(int F=0;F<Base->GetDataModel()->GetNumberOfKeys();++F)
  {
   TArray<FTransform> Local,World;
   for(int I=0;I<Ref.GetNum();++I)
   {
    const FName Bone=Ref.GetBoneName(I);
    FTransform T=Base->GetDataModel()->GetBoneTrackTransform(Bone,FFrameNumber(F));
    const int Parent=Ref.GetParentIndex(I);
    if(Bone==TEXT("Torso")||Bone==TEXT("Chest"))
    {
     const FQuat Desired=(Registration*RawFrames[F][I].GetRotation()).GetNormalized();
     T.SetRotation((World[Parent].GetRotation().Inverse()*Desired).GetNormalized());
    }
    World.Add(Parent<0?T:T*World[Parent]);Local.Add(T);
   }
   Frames.Add(MoveTemp(Local));
  }
  WriteFrames(Seq,Ref,Frames);Seq->PostEditChange();SaveStride(Seq);
 }
 auto* BaseBS=LoadObject<UBlendSpace>(nullptr,*(Study+TEXT("Ready/BS_Directional")));check(BaseBS);
 auto* BS=CopyStride(BaseBS,Folder+TEXT("Ready/BS_Directional"));while(BS->GetNumberOfBlendSamples())BS->DeleteSample(BS->GetNumberOfBlendSamples()-1);
 for(int D=0;D<=8;++D){BS->AddSample(Hold,FVector(-180+D*45,0,0));for(const TCHAR* G:{TEXT("Walk"),TEXT("Jog")})BS->AddSample(LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("Ready/A_")+G+TEXT("_")+Directions[D])),FVector(-180+D*45,FString(G)==TEXT("Walk")?180:450,0));}
 BS->ValidateSampleData();BS->ResampleData();BS->PostEditChange();SaveStride(BS);
 UE_LOG(LogTemp,Display,TEXT("BODY_COORDINATION_BUILT 16 native clips with retargeted spine; lower body and local contacts preserved"));
#endif
}

void URangeAssetTools::BuildLegDirectionAssets()
{
#if WITH_EDITOR
 const FString Previous=TEXT("/Game/Fireline/LocomotionStudy/BodyCoordination/Ready/");
 const FString Folder=TEXT("/Game/Fireline/LocomotionStudy/LegDirection/Ready/");
 auto* Native=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction"));check(Native);
 auto* Hold=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/SycgffM4/A_M4_Idle"));check(Hold);
 const auto& Ref=Native->GetRefSkeleton();const int Torso=Ref.FindBoneIndex(TEXT("Torso")),Chest=Ref.FindBoneIndex(TEXT("Chest"));
 TArray<FTransform> Bind;TArray<int32> Opposite;
 for(int I=0;I<Ref.GetNum();++I)
 {
  const int P=Ref.GetParentIndex(I);Bind.Add(P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bind[P]);
  FString Name=Ref.GetBoneName(I).ToString();
  if(Name.EndsWith(TEXT("_L")))Name=Name.LeftChop(2)+TEXT("_R");else if(Name.EndsWith(TEXT("_R")))Name=Name.LeftChop(2)+TEXT("_L");
  const int Other=Ref.FindBoneIndex(FName(*Name));Opposite.Add(Other>=0?Other:I);
 }
 const auto HoldCS=Components(Hold,Ref,0);
 const TCHAR* Directions[]={TEXT("Bwd"),TEXT("Bwd_Left"),TEXT("Left"),TEXT("Fwd_Left"),TEXT("Fwd"),TEXT("Fwd_Right"),TEXT("Right"),TEXT("Bwd_Right"),TEXT("Bwd")};
 for(const TCHAR* Gait:{TEXT("Walk"),TEXT("Jog")})for(int D=0;D<8;++D)
 {
  const FString Direction=Directions[D];const bool Mirror=Direction.Contains(TEXT("Left"));
  const FString SourceDirection=Mirror?Direction.Replace(TEXT("Left"),TEXT("Right")):Direction;
  auto* Source=LoadObject<UAnimSequence>(nullptr,*(Previous+TEXT("A_")+Gait+TEXT("_")+SourceDirection));check(Source);
  auto* Seq=CopyStride(Source,Folder+TEXT("A_")+Gait+TEXT("_")+Direction);
  Seq->GetController().SetFrameRate(Source->GetDataModel()->GetFrameRate(),false);
  Seq->GetController().SetNumberOfFrames(Source->GetDataModel()->GetNumberOfFrames(),false);
  TArray<TArray<FTransform>> Frames,WorldFrames;
  for(int F=0;F<Source->GetDataModel()->GetNumberOfKeys();++F)
  {
   TArray<FTransform> Local,World;
   for(int I=0;I<Ref.GetNum();++I)
   {
    const int S=Mirror?Opposite[I]:I,P=Ref.GetParentIndex(I),SP=Ref.GetParentIndex(S);
    FTransform T=Source->GetDataModel()->GetBoneTrackTransform(Ref.GetBoneName(S),FFrameNumber(F));
    if(Mirror)
    {
     // Same bind-space algorithm as UE 5.8 FAnimationRuntime::MirrorPose.
     // Reflect translations and rotations, swap paired bones, retain scale.
     const FQuat SQ=SP>=0?Bind[SP].GetRotation():FQuat::Identity;
     const FQuat TQ=P>=0?Bind[P].GetRotation():FQuat::Identity;
     T.SetTranslation(TQ.UnrotateVector(FAnimationRuntime::MirrorVector(SQ.RotateVector(T.GetTranslation()),EAxis::X)));
     T.SetRotation((TQ.Inverse()*FAnimationRuntime::MirrorQuat(SQ*T.GetRotation(),EAxis::X)*FAnimationRuntime::MirrorQuat(Bind[S].GetRotation(),EAxis::X).Inverse()*Bind[I].GetRotation()).GetNormalized());
    }
    Local.Add(T);World.Add(P<0?T:T*World[P]);
   }
   Frames.Add(Local);WorldFrames.Add(World);
  }
  // Keep the accepted right-handed carry, while retaining mirrored gait timing.
  // Center and attenuate BOTH spine levels in component space together; legs,
  // reference lengths and local grip contacts are never bent to stabilize a gun.
  TArray<FQuat> Means;Means.SetNum(Ref.GetNum());
  for(int Bone:{Torso,Chest})
  {
   FQuat Mean(0,0,0,0),Anchor=WorldFrames[0][Bone].GetRotation();
   for(int F=0;F<Frames.Num()-1;++F){const FQuat Q=WorldFrames[F][Bone].GetRotation();Mean+=((Anchor|Q)<0)?Q*(-1.):Q;}
   Mean.Normalize();Means[Bone]=Mean;
  }
  const FQuat Registration=(HoldCS[Chest].GetRotation()*Means[Chest].Inverse()).GetNormalized();
  for(int F=0;F<Frames.Num();++F)
  {
   TArray<FTransform> World;
   for(int I=0;I<Ref.GetNum();++I)
   {
    const int P=Ref.GetParentIndex(I);FTransform& T=Frames[F][I];
    if(I==Torso||I==Chest)
    {
     // Preserve the donor's relative mean spine shape; reduce exaggerated sway.
     const FQuat Desired=(Registration*FQuat::Slerp(Means[I],WorldFrames[F][I].GetRotation(),.35f)).GetNormalized();
     T.SetRotation((World[P].GetRotation().Inverse()*Desired).GetNormalized());
    }
    else if(I!=0&&!Ref.GetBoneName(I).ToString().StartsWith(TEXT("UpperLeg_"))&&!Ref.GetBoneName(I).ToString().StartsWith(TEXT("LowerLeg_"))&&!Ref.GetBoneName(I).ToString().StartsWith(TEXT("Foot"))&&Ref.GetBoneName(I)!=TEXT("Pelvis"))
    {
     // Mirroring must never exchange weapon/hands or their native local grips.
     T=Source->GetDataModel()->GetBoneTrackTransform(Ref.GetBoneName(I),FFrameNumber(F));
    }
    World.Add(P<0?T:T*World[P]);
   }
  }
  WriteFrames(Seq,Ref,Frames);
  Seq->AuthoredSyncMarkers=Source->AuthoredSyncMarkers;
  if(Mirror)for(auto& M:Seq->AuthoredSyncMarkers){if(M.MarkerName==TEXT("L"))M.MarkerName=TEXT("R");else if(M.MarkerName==TEXT("R"))M.MarkerName=TEXT("L");}
  Seq->RefreshSyncMarkerDataFromAuthored();Seq->PostEditChange();SaveStride(Seq);
 }
 auto* BaseBS=LoadObject<UBlendSpace>(nullptr,*(Previous+TEXT("BS_Directional")));check(BaseBS);
 auto* BS=CopyStride(BaseBS,Folder+TEXT("BS_Directional"));while(BS->GetNumberOfBlendSamples())BS->DeleteSample(BS->GetNumberOfBlendSamples()-1);
 for(int D=0;D<=8;++D){BS->AddSample(Hold,FVector(-180+D*45,0,0));for(const TCHAR* G:{TEXT("Walk"),TEXT("Jog")})BS->AddSample(LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("A_")+G+TEXT("_")+Directions[D])),FVector(-180+D*45,FString(G)==TEXT("Walk")?180:450,0));}
 BS->ValidateSampleData();BS->ResampleData();BS->PostEditChange();SaveStride(BS);
 UE_LOG(LogTemp,Display,TEXT("LEG_DIRECTION_BUILT 16 native clips, paired locomotion mirroring with marker exchange and right-handed carry"));
#endif
}


void URangeAssetTools::BuildKneeContinuityAssets()
{
#if WITH_EDITOR
 const FString Previous=TEXT("/Game/Fireline/LocomotionStudy/LegDirection/Ready/");
 const FString Folder=TEXT("/Game/Fireline/LocomotionStudy/KneeContinuity/Ready/");
 auto* Native=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction"));check(Native);
 const auto& Ref=Native->GetRefSkeleton();
 const TCHAR* Directions[]={TEXT("Bwd"),TEXT("Bwd_Left"),TEXT("Left"),TEXT("Fwd_Left"),TEXT("Fwd"),TEXT("Fwd_Right"),TEXT("Right"),TEXT("Bwd_Right"),TEXT("Bwd")};
 for(const TCHAR* Gait:{TEXT("Walk"),TEXT("Jog")})for(int D=0;D<8;++D)
 {
  const FString Direction=Directions[D],Name=FString(Gait)+TEXT("_")+Direction;
  const bool Mirror=Direction.Contains(TEXT("Left"));
  auto* Source=LoadObject<UAnimSequence>(nullptr,*(Previous+TEXT("A_")+Name));check(Source);
  auto* Seq=CopyStride(Source,Folder+TEXT("A_")+Name);
  // Diagnose the uncontracted leg, not the already amplified stride result.
  auto* Base=BaseClip(FString(Gait)+TEXT("_")+(Mirror?Direction.Replace(TEXT("Left"),TEXT("Right")):Direction));check(Base);
  const int Count=Source->GetDataModel()->GetNumberOfKeys();check(Count==Base->GetDataModel()->GetNumberOfKeys());
  TArray<TArray<FTransform>> Frames,Originals,Bases;
  for(int F=0;F<Count;++F){Originals.Add(Components(Source,Ref,F));Bases.Add(Components(Base,Ref,F));}
  TArray<TArray<FVector>> Poles;Poles.SetNum(2);
  TArray<TArray<float>> Weights;Weights.SetNum(2);
  for(int S=0;S<2;++S)
  {
   const FString Side=S==0?TEXT("L"):TEXT("R"),BaseSide=Mirror?(S==0?TEXT("R"):TEXT("L")):Side;
   const int U=Ref.FindBoneIndex(FName(*(TEXT("UpperLeg_")+BaseSide))),K=Ref.FindBoneIndex(FName(*(TEXT("LowerLeg_")+BaseSide))),A=Ref.FindBoneIndex(FName(*(TEXT("Foot_")+BaseSide)));
   TArray<FVector> Normals;TArray<float> Radii;
   const float Upper=FVector::Dist(Bases[0][U].GetLocation(),Bases[0][K].GetLocation());
   // Below 15% of thigh length, point-derived bend planes are ill-conditioned.
   // Recover the authored hinge from adjacent well-bent poses, in thigh space.
   const float Reliable=Upper*.15f;
   for(int F=0;F<Count-1;++F)
   {
    const auto& B=Bases[F];const FVector H=B[U].GetLocation(),J=B[K].GetLocation(),E=B[A].GetLocation(),Axis=(E-H).GetSafeNormal();
    const FVector Bend=(J-H)-Axis*((J-H).Dot(Axis));Radii.Add(Bend.Size());
    Normals.Add(B[U].GetRotation().UnrotateVector((J-H).Cross(E-J).GetSafeNormal()));
   }
   for(int F=0;F<Count;++F)
   {
    const int I=F%(Count-1),N=Count-1;const float W=1.f-FMath::SmoothStep(Reliable*.5f,Reliable,Radii[I]);
    FVector Normal=Normals[I];
    if(W>0)
    {
     int Before=1,After=1;
     while(Before<N&&Radii[(I-Before+N)%N]<Reliable)++Before;
     while(After<N&&Radii[(I+After)%N]<Reliable)++After;
     check(Before<N&&After<N);
     Normal=FMath::Lerp(Normals[(I-Before+N)%N],Normals[(I+After)%N],float(Before)/(Before+After)).GetSafeNormal();
    }
    const auto& B=Bases[F];const FVector H=B[U].GetLocation(),J=B[K].GetLocation(),E=B[A].GetLocation(),Axis=(E-H).GetSafeNormal();
    const FVector Raw=((J-H)-Axis*((J-H).Dot(Axis))).GetSafeNormal();
    const FVector Stable=Axis.Cross(B[U].GetRotation().RotateVector(Normal)).GetSafeNormal();
    FVector Pole=FMath::Lerp(Raw,Stable,W).GetSafeNormal();if(Mirror)Pole.X=-Pole.X;
    Poles[S].Add(Pole);Weights[S].Add(W);
   }
  }
  for(int F=0;F<Count;++F)
  {
   auto Adjusted=Originals[F];const auto& Original=Originals[F];
   for(int S=0;S<2;++S)
   {
    if(Weights[S][F]<=0)continue;
    const FString Side=S==0?TEXT("L"):TEXT("R");
    const int U=Ref.FindBoneIndex(FName(*(TEXT("UpperLeg_")+Side))),K=Ref.FindBoneIndex(FName(*(TEXT("LowerLeg_")+Side))),A=Ref.FindBoneIndex(FName(*(TEXT("Foot_")+Side)));
    const FVector H=Original[U].GetLocation(),J=Original[K].GetLocation(),E=Original[A].GetLocation();FVector Knee,Foot;
    AnimationCore::SolveTwoBoneIK(H,J,E,H+Poles[S][F]*100.f,E,Knee,Foot,FVector::Dist(H,J),FVector::Dist(J,E),false,1.,1.);
    check(FVector::Dist(E,Foot)<.01f);
    Adjusted[U].SetRotation((FQuat::FindBetweenNormals((J-H).GetSafeNormal(),(Knee-H).GetSafeNormal())*Original[U].GetRotation()).GetNormalized());
    Adjusted[K].SetLocation(Knee);Adjusted[K].SetRotation((FQuat::FindBetweenNormals((E-J).GetSafeNormal(),(E-Knee).GetSafeNormal())*Original[K].GetRotation()).GetNormalized());
   }
   TArray<FTransform> Local;
   for(int I=0;I<Ref.GetNum();++I)
   {
    const int P=Ref.GetParentIndex(I);const FString N=Ref.GetBoneName(I).ToString();
    const bool Main=N.StartsWith(TEXT("UpperLeg_"))||N.StartsWith(TEXT("LowerLeg_"))||N==TEXT("Foot_L")||N==TEXT("Foot_R");
    if(P>=0&&!Main)Adjusted[I]=Original[I].GetRelativeTransform(Original[P])*Adjusted[P];
    Local.Add(P<0?Adjusted[I]:Adjusted[I].GetRelativeTransform(Adjusted[P]));
   }
   Frames.Add(MoveTemp(Local));
  }
  WriteFrames(Seq,Ref,Frames);Seq->PostEditChange();SaveStride(Seq);
 }
 auto* BaseBS=LoadObject<UBlendSpace>(nullptr,*(Previous+TEXT("BS_Directional")));check(BaseBS);
 auto* BS=CopyStride(BaseBS,Folder+TEXT("BS_Directional"));
 while(BS->GetNumberOfBlendSamples())BS->DeleteSample(BS->GetNumberOfBlendSamples()-1);
 auto* Hold=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/SycgffM4/A_M4_Idle"));
 for(int D=0;D<=8;++D){BS->AddSample(Hold,FVector(-180+D*45,0,0));for(const TCHAR* G:{TEXT("Walk"),TEXT("Jog")})BS->AddSample(LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("A_")+G+TEXT("_")+Directions[D])),FVector(-180+D*45,FString(G)==TEXT("Walk")?180:450,0));}
 BS->ValidateSampleData();BS->ResampleData();BS->PostEditChange();SaveStride(BS);
#endif
}


void URangeAssetTools::BuildArmedLocomotionAssets()
{
#if WITH_EDITOR
 const FString Previous=TEXT("/Game/Fireline/LocomotionStudy/KneeContinuity/Ready/"),Folder=TEXT("/Game/Fireline/LocomotionStudy/ArmedLocomotion/Ready/");
 auto* Native=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction"));
 auto* Manny=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple"));
 auto* Hold=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/SycgffM4/A_M4_Idle"));check(Native&&Manny&&Hold);
 const auto& Ref=Native->GetRefSkeleton();const auto& DonorRef=Manny->GetRefSkeleton();
 const int Torso=Ref.FindBoneIndex(TEXT("Torso")),Chest=Ref.FindBoneIndex(TEXT("Chest"));
 const int Hand=DonorRef.FindBoneIndex(TEXT("hand_r")),Head=DonorRef.FindBoneIndex(TEXT("head"));
 const auto HoldCS=Components(Hold,Ref,0);
 const TCHAR* Directions[]={TEXT("Bwd"),TEXT("Bwd_Left"),TEXT("Left"),TEXT("Fwd_Left"),TEXT("Fwd"),TEXT("Fwd_Right"),TEXT("Right"),TEXT("Bwd_Right"),TEXT("Bwd")};
 for(int G=0;G<3;++G)for(int D=0;D<8;++D)
 {
  const FString Gait=G==0?TEXT("Walk"):TEXT("Jog"),Direction=Directions[D],Name=(G==2?FString(TEXT("Sprint")):Gait)+TEXT("_")+Direction;
  const bool Mirror=Direction.Contains(TEXT("Left"));const FString SourceDirection=Mirror?Direction.Replace(TEXT("Left"),TEXT("Right")):Direction;
  const FString SourceName=Gait+TEXT("_")+SourceDirection;
  auto* Base=LoadObject<UAnimSequence>(nullptr,*(Previous+TEXT("A_")+Gait+TEXT("_")+Direction));
  auto* Raw=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Fireline/LocomotionStudy/BodyCoordination/Raw/Ryan_MF_")+FString(G==0&&SourceDirection==TEXT("Bwd_Right")?TEXT("Pistol_"):TEXT("Rifle_"))+SourceName));
  auto* Donor=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Characters/Mannequins/Anims/")+FString(G==0&&SourceDirection==TEXT("Bwd_Right")?TEXT("Pistol/"):TEXT("Rifle/"))+Gait+TEXT("/MF_")+(G==0&&SourceDirection==TEXT("Bwd_Right")?TEXT("Pistol_"):TEXT("Rifle_"))+SourceName));
  auto* FullStride=BaseClip(SourceName);check(Base&&Raw&&Donor&&FullStride);
  auto* Seq=CopyStride(Base,Folder+TEXT("A_")+Name);
  const int Count=Base->GetDataModel()->GetNumberOfKeys();check(Count==Raw->GetDataModel()->GetNumberOfKeys()&&Count==Donor->GetDataModel()->GetNumberOfKeys());
  TArray<TArray<FTransform>> RawFrames,DonorFrames;FVector HandMean=FVector::ZeroVector;FQuat HandQ(0,0,0,0);TArray<FQuat> SpineMean;SpineMean.SetNum(2);SpineMean[0]=SpineMean[1]=FQuat(0,0,0,0);
  for(int F=0;F<Count;++F)
  {
   RawFrames.Add(Components(Raw,Ref,F));DonorFrames.Add(Components(Donor,DonorRef,F));
   if(F==Count-1)continue;
   HandMean+=DonorFrames[F][Hand].GetLocation()-DonorFrames[F][Head].GetLocation();
   FQuat Q=DonorFrames[F][Hand].GetRotation();if((Q|DonorFrames[0][Hand].GetRotation())<0)Q=Q*(-1.);HandQ+=Q;
   for(int I=0;I<2;++I){const int B=I==0?Torso:Chest;Q=RawFrames[F][B].GetRotation();if((Q|RawFrames[0][B].GetRotation())<0)Q=Q*(-1.);SpineMean[I]+=Q;}
  }
  HandMean/=Count-1;HandQ.Normalize();for(auto& Q:SpineMean)Q.Normalize();
  // Preserve the source forward lean. Register yaw only, instead of rotating
  // the mean spine all the way back to the static weapon stance.
  TArray<FTransform> Bind=Ref.GetRefBonePose();for(int I=1;I<Bind.Num();++I)Bind[I]=Bind[I]*Bind[Ref.GetParentIndex(I)];
  FVector RawForward=SpineMean[1].RotateVector(Bind[Chest].GetRotation().UnrotateVector(FVector::YAxisVector));
  const FQuat Registration(FVector::UpVector,FMath::Atan2(RawForward.X,RawForward.Y));
  TArray<TArray<FTransform>> Frames;TArray<TArray<FRichCurveKey>> Curves;Curves.SetNum(7);
  for(int F=0;F<Count;++F)
  {
   const auto Original=Components(Base,Ref,F);auto Adjusted=Original;
   if(G==2)
   {
    const auto Full=Components(FullStride,Ref,F);
    for(int S=0;S<2;++S)
    {
     const FString Side=S==0?TEXT("L"):TEXT("R"),Other=Mirror?(S==0?TEXT("R"):TEXT("L")):Side;
     const int U=Ref.FindBoneIndex(FName(*(TEXT("UpperLeg_")+Side))),K=Ref.FindBoneIndex(FName(*(TEXT("LowerLeg_")+Side))),A=Ref.FindBoneIndex(FName(*(TEXT("Foot_")+Side))),SU=Ref.FindBoneIndex(FName(*(TEXT("UpperLeg_")+Other))),SA=Ref.FindBoneIndex(FName(*(TEXT("Foot_")+Other)));
     const FVector H=Original[U].GetLocation(),J=Original[K].GetLocation(),E=Original[A].GetLocation();
     FVector Reach=Full[SA].GetLocation()-Full[SU].GetLocation();if(Mirror)Reach.X=-Reach.X;
     const float UL=FVector::Dist(H,J),LL=FVector::Dist(J,E);Reach=Reach.GetClampedToMaxSize(UL+LL-.1f);
     // Keep the repaired knee plane as the target stride expands.
     const FVector Axis=(E-H).GetSafeNormal(),Pole=H+FVector::VectorPlaneProject(J-H,Axis).GetSafeNormal()*100.f;
     FVector Knee,Foot;AnimationCore::SolveTwoBoneIK(H,J,E,Pole,H+Reach,Knee,Foot,UL,LL,false,1.,1.);
     Adjusted[U].SetRotation((FQuat::FindBetweenNormals((J-H).GetSafeNormal(),(Knee-H).GetSafeNormal())*Original[U].GetRotation()).GetNormalized());
     Adjusted[K].SetLocation(Knee);Adjusted[K].SetRotation((FQuat::FindBetweenNormals((E-J).GetSafeNormal(),(Foot-Knee).GetSafeNormal())*Original[K].GetRotation()).GetNormalized());Adjusted[A].SetLocation(Foot);
    }
   }
   TArray<FTransform> Local,World;
   for(int I=0;I<Ref.GetNum();++I)
   {
    const int Parent=Ref.GetParentIndex(I);const FString B=Ref.GetBoneName(I).ToString();
    const bool Leg=B.StartsWith(TEXT("UpperLeg_"))||B.StartsWith(TEXT("LowerLeg_"))||B==TEXT("Foot_L")||B==TEXT("Foot_R");
    FTransform T=Parent<0?Original[I]:Leg?Adjusted[I].GetRelativeTransform(Adjusted[Parent]):Original[I].GetRelativeTransform(Original[Parent]);
    if(I==Torso||I==Chest)
    {
     const int S=I==Torso?0:1;
     FQuat Mean=(Registration*SpineMean[S]).GetNormalized();
     FQuat Motion=(Registration*RawFrames[F][I].GetRotation()).GetNormalized();
     // Right-handed carry keeps its mean yaw; only gait excursions mirror.
     FQuat Delta=(Motion*Mean.Inverse()).GetNormalized();if(Mirror)Delta=FAnimationRuntime::MirrorQuat(Delta,EAxis::X);
     const FQuat Target=(FQuat::Slerp(FQuat::Identity,Delta,G==2?.85f:.65f)*FQuat::Slerp(HoldCS[I].GetRotation(),Mean,G==0?.45f:G==1?.7f:1.f)).GetNormalized();
     T.SetRotation((World[Parent].GetRotation().Inverse()*Target).GetNormalized());
    }
    Local.Add(T);World.Add(Parent<0?T:T*World[Parent]);
   }
   Frames.Add(MoveTemp(Local));
   FVector Translation=(DonorFrames[F][Hand].GetLocation()-DonorFrames[F][Head].GetLocation()-HandMean)*(G==2?.8f:.6f);
   FQuat Delta=(DonorFrames[F][Hand].GetRotation()*HandQ.Inverse()).GetNormalized();Delta=FQuat::Slerp(FQuat::Identity,Delta,G==2?.8f:.6f).GetNormalized();
   if(Mirror){Translation.X=-Translation.X;Delta=FAnimationRuntime::MirrorQuat(Delta,EAxis::X);}
   if(G==2){Translation+=FVector(0,0,-6);Delta=(FQuat(-FVector::XAxisVector,FMath::DegreesToRadians(8.f))*Delta).GetNormalized();}
   if(Delta.W<0)Delta=Delta*(-1.);
   const float Values[]={float(Translation.X),float(Translation.Y),float(Translation.Z),float(Delta.X),float(Delta.Y),float(Delta.Z),float(Delta.W-1.)};
   const float Time=F/float(Base->GetDataModel()->GetFrameRate().AsDecimal());
   for(int C=0;C<7;++C){FRichCurveKey K(Time,Values[C]);K.InterpMode=RCIM_Linear;Curves[C].Add(K);}
  }
  WriteFrames(Seq,Ref,Frames);
  const TCHAR* Names[]={TEXT("FL_CarryX"),TEXT("FL_CarryY"),TEXT("FL_CarryZ"),TEXT("FL_CarryQX"),TEXT("FL_CarryQY"),TEXT("FL_CarryQZ"),TEXT("FL_CarryQW")};
  for(int C=0;C<7;++C){FAnimationCurveIdentifier Id(Names[C],ERawCurveTrackTypes::RCT_Float);Seq->GetController().AddCurve(Id,4,false);check(Seq->GetController().SetCurveKeys(Id,Curves[C],false));}
  // Source full stride runs ~609-623 cm/s; bounded 1.12-1.15 rate at 700.
  Seq->RateScale=G==2?700.f/(SourceDirection.Contains(TEXT("Bwd"))?609.f:SourceDirection==TEXT("Right")?614.5f:623.45f):1.f;
  Seq->PostEditChange();SaveStride(Seq);
 }
 auto* BS=CopyStride(LoadObject<UBlendSpace>(nullptr,*(Previous+TEXT("BS_Directional"))),Folder+TEXT("BS_Directional"));
 const_cast<FBlendParameter&>(BS->GetBlendParameter(1)).Max=700;while(BS->GetNumberOfBlendSamples())BS->DeleteSample(BS->GetNumberOfBlendSamples()-1);
 for(int D=0;D<=8;++D){BS->AddSample(Hold,FVector(-180+D*45,0,0));for(int G=0;G<3;++G){const TCHAR* Gait=G==0?TEXT("Walk"):G==1?TEXT("Jog"):TEXT("Sprint");BS->AddSample(LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("A_")+Gait+TEXT("_")+Directions[D])),FVector(-180+D*45,G==0?180:G==1?450:700,0));}}
 BS->ValidateSampleData();BS->ResampleData();BS->PostEditChange();SaveStride(BS);
#endif
}

void URangeAssetTools::BuildArmedReadyAssets()
{
#if WITH_EDITOR
 const FString Folder=TEXT("/Game/Fireline/LocomotionStudy/ArmedReady/Ready/");
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction"));
 auto* Hold=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/SycgffM4/A_M4_Idle"));
 auto* Raw=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/LocomotionStudy/ArmedReady/Raw/Ryan_MF_Rifle_Idle_ADS"));check(Mesh&&Hold&&Raw);
 const auto& Ref=Mesh->GetRefSkeleton();const auto Source=Components(Raw,Ref,0),Original=Components(Hold,Ref,0);
 auto Bind=Ref.GetRefBonePose();for(int I=1;I<Bind.Num();++I)Bind[I]=Bind[I]*Bind[Ref.GetParentIndex(I)];
 const int Chest=Ref.FindBoneIndex(TEXT("Chest"));
 const FVector Forward=Source[Chest].GetRotation().RotateVector(Bind[Chest].GetRotation().UnrotateVector(FVector::YAxisVector));
 const FQuat Register(FVector::UpVector,FMath::Atan2(Forward.X,Forward.Y));
 TArray<FTransform> World,Local;
 for(int I=0;I<Ref.GetNum();++I)
 {
  const int Parent=Ref.GetParentIndex(I);const FString Name=Ref.GetBoneName(I).ToString();
  const bool Body=Name==TEXT("Pelvis")||Name==TEXT("Torso")||Name==TEXT("Chest")||Name.StartsWith(TEXT("UpperLeg_"))||Name.StartsWith(TEXT("LowerLeg_"))||Name.StartsWith(TEXT("Foot"));
  FTransform T=Parent<0?Original[I]:Original[I].GetRelativeTransform(Original[Parent]);
  if(Body)
  {
   FTransform W((Register*Source[I].GetRotation()).GetNormalized(),Register.RotateVector(Source[I].GetLocation()),Bind[I].GetScale3D());
   T=Parent<0?W:W.GetRelativeTransform(World[Parent]);
  }
  Local.Add(T);World.Add(Parent<0?T:T*World[Parent]);
 }
 auto* Idle=CopyStride(Hold,Folder+TEXT("A_Ready"));
 TArray<TArray<FTransform>> Frames;Frames.Init(Local,Idle->GetDataModel()->GetNumberOfKeys());
 WriteFrames(Idle,Ref,Frames);Idle->PostEditChange();SaveStride(Idle);
 // Reuse the exact repaired gait clips, changing only the zero-speed row.
 auto* BS=CopyStride(LoadObject<UBlendSpace>(nullptr,TEXT("/Game/Fireline/LocomotionStudy/ArmedLocomotion/Ready/BS_Directional")),Folder+TEXT("BS_Directional"));
 for(int I=BS->GetNumberOfBlendSamples()-1;I>=0;--I)if(BS->GetBlendSample(I).SampleValue.Y<1.f)BS->DeleteSample(I);
 for(int D=0;D<=8;++D)BS->AddSample(Idle,FVector(-180+D*45,0,0));
 BS->ValidateSampleData();BS->ResampleData();BS->PostEditChange();SaveStride(BS);
#endif
}

void URangeAssetTools::BuildAuthoredGroundAssets()
{
#if WITH_EDITOR
 const bool FullBody=FParse::Param(FCommandLine::Get(),TEXT("FirelineSourceBodyStudy"));
 const bool Combat=FullBody||FParse::Param(FCommandLine::Get(),TEXT("FirelineCombatGroundStudy"));
 const bool Matched=Combat||FParse::Param(FCommandLine::Get(),TEXT("FirelineMatchedGroundStudy"));
 const FString Folder=FullBody?TEXT("/Game/Fireline/LocomotionStudy/SourceBody/"):Combat?TEXT("/Game/Fireline/LocomotionStudy/CombatGround/"):Matched?TEXT("/Game/Fireline/LocomotionStudy/MatchedGround/"):TEXT("/Game/Fireline/LocomotionStudy/AuthoredGround/");
 auto* Native=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction"));check(Native);
 auto* Hold=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/SycgffM4/A_M4_Idle"));check(Hold);
 const auto& Ref=Native->GetRefSkeleton();const auto Base=Components(Hold,Ref,0);
 auto PostureCurves=[&](UAnimSequence* To,UAnimSequence* Source)
 {
  const auto& SR=Source->GetSkeleton()->GetReferenceSkeleton();
  TArray<FRichCurveKey> Low,High,Whole;
  auto MeanLean=[&](UAnimSequence* A)
  {
   const auto& R=A->GetSkeleton()->GetReferenceSkeleton();double Sum=0;
   for(int F=0;F<A->GetDataModel()->GetNumberOfKeys();++F){const auto W=Components(A,R,F);const FVector D=W[R.FindBoneIndex(TEXT("neck_01"))].GetLocation()-W[R.FindBoneIndex(TEXT("pelvis"))].GetLocation();Sum+=FMath::Atan2(D.Y,D.Z);}
   return FMath::RadiansToDegrees(Sum/A->GetDataModel()->GetNumberOfKeys());
  };
  float Register=0;const FString SourceName=Source->GetName();
  if(SourceName.Contains(TEXT("Walk_"))||SourceName.Contains(TEXT("Jog_")))
  {
   const FString Gait=SourceName.Contains(TEXT("Walk_"))?TEXT("Walk"):TEXT("Jog");
   auto* Front=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_")+Gait+TEXT("_Fwd")));check(Front);
   // All directions share the forward armed stance; keep each source cycle's
   // changing tilt, rather than importing a nearly upright lateral baseline.
   Register=MeanLean(Front)-MeanLean(Source);
  }
  for(int F=0;F<To->GetDataModel()->GetNumberOfKeys();++F)
  {
   const auto W=Components(Source,SR,FMath::Min(F,Source->GetDataModel()->GetNumberOfKeys()-1));
   auto At=[&](const TCHAR* B){return W[SR.FindBoneIndex(B)].GetLocation();};
   const FVector L=At(TEXT("spine_03"))-At(TEXT("pelvis")),H=At(TEXT("neck_01"))-At(TEXT("spine_03"));
   const float T=To->GetDataModel()->GetFrameRate().AsSeconds(FFrameNumber(F));
   Low.Add(FRichCurveKey(T,FMath::RadiansToDegrees(FMath::Atan2(L.Y,L.Z))));High.Add(FRichCurveKey(T,FMath::RadiansToDegrees(FMath::Atan2(H.Y,H.Z))));
   const FVector Body=At(TEXT("neck_01"))-At(TEXT("pelvis"));Whole.Add(FRichCurveKey(T,Register+FMath::RadiansToDegrees(FMath::Atan2(Body.Y,Body.Z))));
  }
  for(int I=0;I<3;++I){FAnimationCurveIdentifier Id(I==2?TEXT("FL_BodyLean"):I?TEXT("FL_SpineHigh"):TEXT("FL_SpineLow"),ERawCurveTrackTypes::RCT_Float);To->GetController().AddCurve(Id,4,false);check(To->GetController().SetCurveKeys(Id,I==2?Whole:I?High:Low,false));}
 };
 UAnimSequence* CombatIdle=Hold;
 if(Combat){CombatIdle=CopyStride(Hold,Folder+TEXT("Ready/A_CombatIdle"));auto* SourceIdle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Idle_ADS"));check(SourceIdle);PostureCurves(CombatIdle,SourceIdle);CombatIdle->PostEditChange();SaveStride(CombatIdle);}
 TArray<FString> Names;
 for(const TCHAR* G:{TEXT("Walk"),TEXT("Jog")})for(const TCHAR* D:{TEXT("Fwd"),TEXT("Right"),TEXT("Bwd"),TEXT("Left")})for(const TCHAR* S:{TEXT("Start"),TEXT("Stop"),TEXT("Pivot")})Names.Add(FString(G)+TEXT("_")+D+TEXT("_")+S);
 Names.Add(TEXT("Jog_Left"));Names.Add(TEXT("Jog_Right"));Names.Add(TEXT("MM_Jog_Left"));Names.Add(TEXT("MM_Jog_Right"));
 for(const TCHAR* G:{TEXT("Walk"),TEXT("Jog")})for(const TCHAR* D:{TEXT("Fwd"),TEXT("Right"),TEXT("Bwd"),TEXT("Left")})Names.Add(FString(TEXT("Mirror_"))+G+TEXT("_")+D+TEXT("_Stop"));
 if(Matched){Names.Reset();for(const TCHAR* G:{TEXT("Walk"),TEXT("Jog")})for(const TCHAR* D:{TEXT("Fwd"),TEXT("Right"),TEXT("Bwd"),TEXT("Left")})Names.Add(FString(TEXT("MM_"))+G+TEXT("_")+D);}
 for(const FString& Name:Names)
 {
  const bool Mirror=Name.StartsWith(TEXT("Mirror_"));FString MotionName=Mirror?Name.RightChop(7):Name;
  if(Mirror){MotionName=MotionName.Replace(TEXT("Right"),TEXT("TEMP")).Replace(TEXT("Left"),TEXT("Right")).Replace(TEXT("TEMP"),TEXT("Left"));}
  const bool SideJog=Name.StartsWith(TEXT("MM_"));const FString SourceName=(SideJog?TEXT("MM_Rifle_"):TEXT("MF_Rifle_"))+(SideJog?Name.RightChop(3):MotionName);
  auto* Raw=LoadObject<UAnimSequence>(nullptr,*((Combat&&!FullBody?TEXT("/Game/Fireline/LocomotionStudy/MatchedGround/"):Folder)+TEXT("Raw/Ryan_")+SourceName));check(Raw);
  auto* Original=LoadObject<UAnimSequence>(nullptr,*(TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/")+SourceName));check(Original);
  auto* Seq=CopyStride(Hold,Folder+TEXT("Ready/A_")+Name);Seq->SetSkeleton(Native->GetSkeleton());Seq->RetargetSource=NAME_None;Seq->SetRetargetSourceAsset(Native);Seq->UpdateRetargetSourceAssetData();
  const auto* Model=Raw->GetDataModel();const int N=Model->GetNumberOfKeys();
  Seq->GetController().SetFrameRate(Model->GetFrameRate(),false);Seq->GetController().SetNumberOfFrames(N-1,false);Seq->RateScale=1.f;
  TArray<TArray<FTransform>> Frames;TArray<FRichCurveKey> Distance;
  FVector Previous=FVector::ZeroVector;float Travel=0;
  for(int F=0;F<N;++F)
  {
   auto World=Components(Raw,Ref,F);const FVector RootTravel=World[0].GetLocation();
   // Remove translation at the root, not at the pelvis: preserve the authored
   // braking/weight-transfer motion while CharacterMovement owns travel.
   for(int I=0;I<World.Num();++I){World[I].AddToTranslation(-RootTravel);World[I].SetScale3D(Base[I].GetScale3D());}
   World[0]=Base[0];TArray<FTransform> Local;
   for(int I=0;I<Ref.GetNum();++I)
   {
    const FString B=Ref.GetBoneName(I).ToString();const int P=Ref.GetParentIndex(I);
    const bool Lower=B==TEXT("Pelvis")||B.StartsWith(TEXT("UpperLeg_"))||B.StartsWith(TEXT("LowerLeg_"))||B.StartsWith(TEXT("Foot"));
    FTransform T=P<0?Base[0]:Lower?World[I].GetRelativeTransform(World[P]):Base[I].GetRelativeTransform(Base[P]);
    const bool Upper=FullBody&&(B==TEXT("Torso")||B==TEXT("Chest")||B==TEXT("Neck")||B==TEXT("Head")||B.StartsWith(TEXT("Shoulder_"))||B.StartsWith(TEXT("UpperArm_"))||B.StartsWith(TEXT("LowerArm_")));
    // Full FK rotation chain, with the original native local lengths/scales.
    // Proxy helper offsets are never copied into the render skeleton.
    if(Upper)T.SetRotation(World[I].GetRelativeTransform(World[P]).GetRotation());
    Local.Add(T);
   }
   Frames.Add(MoveTemp(Local));
   const FVector Root=Original->GetDataModel()->GetBoneTrackTransform(TEXT("root"),FFrameNumber(F)).GetLocation();
   if(F)Travel+=FVector::Dist2D(Root,Previous);Previous=Root;
   Distance.Add(FRichCurveKey(Model->GetFrameRate().AsSeconds(FFrameNumber(F)),Travel));
  }
  if(Mirror)
  {
   TArray<FTransform> Bind;for(int I=0;I<Ref.GetNum();++I){int P=Ref.GetParentIndex(I);Bind.Add(P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bind[P]);}
   for(auto& Frame:Frames)
   {
    TArray<FTransform> CS;for(int I=0;I<Ref.GetNum();++I){int P=Ref.GetParentIndex(I);CS.Add(P<0?Frame[I]:Frame[I]*CS[P]);}auto M=CS;
    for(int I=0;I<Ref.GetNum();++I)
    {
     FString B=Ref.GetBoneName(I).ToString();const bool Lower=B==TEXT("Pelvis")||B.StartsWith(TEXT("UpperLeg_"))||B.StartsWith(TEXT("LowerLeg_"))||B.StartsWith(TEXT("Foot"));if(!Lower)continue;
     FString Other=B;if(B.EndsWith(TEXT("_L")))Other=B.LeftChop(2)+TEXT("_R");else if(B.EndsWith(TEXT("_R")))Other=B.LeftChop(2)+TEXT("_L");const int J=Ref.FindBoneIndex(FName(*Other));check(J>=0);
     FVector P=CS[J].GetLocation();P.X=-P.X;const FQuat D=(CS[J].GetRotation()*Bind[J].GetRotation().Inverse()).GetNormalized();
     M[I].SetLocation(P);M[I].SetRotation((FQuat(D.X,-D.Y,-D.Z,D.W)*Bind[I].GetRotation()).GetNormalized());
    }
    for(int I=0;I<Ref.GetNum();++I){int P=Ref.GetParentIndex(I);FString B=Ref.GetBoneName(I).ToString();bool Lower=B==TEXT("Pelvis")||B.StartsWith(TEXT("UpperLeg_"))||B.StartsWith(TEXT("LowerLeg_"))||B.StartsWith(TEXT("Foot"));if(P>=0&&!Lower)M[I]=Frame[I]*M[P];Frame[I]=P<0?M[I]:M[I].GetRelativeTransform(M[P]);}
   }
  }
  WriteFrames(Seq,Ref,Frames);Seq->AuthoredSyncMarkers.Reset();
  // Pose-search features on native proportions; compared to the last final
  // pose, not to an assumed gait phase. These do not animate extra bones.
  for(const TCHAR* Bone:{TEXT("Foot_L"),TEXT("Foot_R"),TEXT("LowerLeg_L"),TEXT("LowerLeg_R")})
  {
   const int B=Ref.FindBoneIndex(Bone);TArray<FRichCurveKey> Axes[3];
   for(int F=0;F<N;++F){FTransform T=Frames[F][B];for(int P=Ref.GetParentIndex(B);P>=0;P=Ref.GetParentIndex(P))T=T*Frames[F][P];for(int Axis=0;Axis<3;++Axis)Axes[Axis].Add(FRichCurveKey(Model->GetFrameRate().AsSeconds(FFrameNumber(F)),T.GetLocation()[Axis]));}
   for(int Axis=0;Axis<3;++Axis){FAnimationCurveIdentifier Feature(FName(*FString::Printf(TEXT("FL_%s_%d"),Bone,Axis)),ERawCurveTrackTypes::RCT_Float);Seq->GetController().AddCurve(Feature,4,false);check(Seq->GetController().SetCurveKeys(Feature,Axes[Axis],false));}
  }
  FAnimationCurveIdentifier Id(TEXT("FL_Travel"),ERawCurveTrackTypes::RCT_Float);Seq->GetController().AddCurve(Id,4,false);check(Seq->GetController().SetCurveKeys(Id,Distance,false));
  if(SideJog)
  {
   Seq->RateScale=(Name.Contains(TEXT("Walk"))?180.f:450.f)/(Travel/Seq->GetPlayLength()*.94f);
   Seq->AuthoredSyncMarkers=Original->AuthoredSyncMarkers;Seq->RefreshSyncMarkerDataFromAuthored();
  }
   for(int Side=0;Side<2;++Side)
   {
    const FName Bone=Side?TEXT("Foot_R"):TEXT("Foot_L");const int B=Ref.FindBoneIndex(Bone);TArray<float> Heights;float MinZ=FLT_MAX;
    for(const auto& Frame:Frames){FTransform T=Frame[B];for(int P=Ref.GetParentIndex(B);P>=0;P=Ref.GetParentIndex(P))T=T*Frame[P];Heights.Add(T.GetLocation().Z);MinZ=FMath::Min(MinZ,Heights.Last());}
    TArray<FRichCurveKey> Keys;for(int F=0;F<N;++F)Keys.Add(FRichCurveKey(Model->GetFrameRate().AsSeconds(FFrameNumber(F)),1.f-FMath::SmoothStep(.8f,2.2f,Heights[F]-MinZ)));
    FAnimationCurveIdentifier Plant(Side?TEXT("FL_Plant_R"):TEXT("FL_Plant_L"),ERawCurveTrackTypes::RCT_Float);Seq->GetController().AddCurve(Plant,4,false);check(Seq->GetController().SetCurveKeys(Plant,Keys,false));
   }
  if(Combat&&!FullBody)PostureCurves(Seq,Original);
  Seq->PostEditChange();SaveStride(Seq);
  UE_LOG(LogTemp,Display,TEXT("AUTHORED_GROUND_BUILT %s frames=%d travel=%.2f"),*Name,N,Travel);
 }
 auto* Old=LoadObject<UBlendSpace>(nullptr,TEXT("/Game/Fireline/LocomotionStudy/CMUSideStep/ContactFix/Ready/BS_Directional"));check(Old);
 auto* BS=CopyStride(Old,Folder+TEXT("Ready/BS_Directional"));while(BS->GetNumberOfBlendSamples())BS->DeleteSample(BS->GetNumberOfBlendSamples()-1);
 for(const auto& Sample:Old->GetBlendSamples())
 {
  if(Matched&&!FMath::IsNearlyZero(FMath::Fmod(FMath::Abs(Sample.SampleValue.X),90.f)))continue;
  UAnimSequence* A=Combat&&Sample.SampleValue.Y==0?CombatIdle:Sample.Animation.Get();
  if(Matched&&Sample.SampleValue.Y>0){const int D=(FMath::RoundToInt(Sample.SampleValue.X/90.f)+4)%4;const TCHAR* Directions[]={TEXT("Fwd"),TEXT("Right"),TEXT("Bwd"),TEXT("Left")};A=LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("Ready/A_MM_")+(Sample.SampleValue.Y>400?TEXT("Jog_"):TEXT("Walk_"))+Directions[D]));}
  else
  if(Sample.SampleValue.Y>400.f&&FMath::IsNearlyEqual(FMath::Abs(Sample.SampleValue.X),90.f))A=LoadObject<UAnimSequence>(nullptr,*(Folder+TEXT("Ready/A_MM_Jog_")+(Sample.SampleValue.X<0?TEXT("Left"):TEXT("Right"))));
  check(A);BS->AddSample(A,Sample.SampleValue);
 }
 BS->ValidateSampleData();BS->ResampleData();BS->PostEditChange();SaveStride(BS);
#endif
}
