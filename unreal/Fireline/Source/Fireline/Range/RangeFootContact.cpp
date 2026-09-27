#include "RangeFootContact.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "TwoBoneIK.h"

void FRangeFootContact::Initialize(USkeletalMeshComponent* Mesh)
{
 *this=FRangeFootContact();
 const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
 Bind=Ref.GetRefBonePose();Pelvis=Ref.FindBoneIndex(TEXT("Pelvis"));
 for(int I=0;I<Ref.GetNum();++I){Parents.Add(Ref.GetParentIndex(I));if(I)Bind[I]*=Bind[Parents[I]];}
 for(int I=0;I<2;++I){const TCHAR* Side=I?TEXT("R"):TEXT("L");auto Id=[&](const TCHAR* N){return Ref.FindBoneIndex(*FString::Printf(TEXT("%s_%s"),N,Side));};Feet[I].Upper=Id(TEXT("UpperLeg"));Feet[I].Knee=Id(TEXT("LowerLeg"));Feet[I].Ankle=Id(TEXT("Foot"));}
 Forward=Mesh->GetRelativeRotation().UnrotateVector(FVector::ForwardVector);
 Enabled=Feet[0].Ankle>=0&&Feet[1].Ankle>=0;
}
void FRangeFootContact::Apply(FPoseContext& Output)
{
 if(!Enabled)return;
 if(GaitWeight<.001f){for(auto& F:Feet){F.Locked=false;F.Weight=0;}PelvisDrop=0;return;}
 const auto& Bones=Output.Pose.GetBoneContainer();TArray<FTransform> C;C.SetNum(Parents.Num());
 for(int I=0;I<C.Num();++I){const auto K=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(I));C[I]=K.GetInt()>=0?Output.Pose[K]:FTransform::Identity;if(Parents[I]>=0)C[I]*=C[Parents[I]];}
 auto Child=[&](int I,int Root){for(;I>=0;I=Parents[I])if(I==Root)return true;return false;};
 if(!Grounded){for(auto& F:Feet){F.Locked=false;F.Weight=0;}PelvisDrop=0;return;}
 FVector Targets[2];bool Correcting[2]={false,false};
 for(int I=0;I<2;++I)
 {
  auto& F=Feet[I];const float W=(Speed>6.f||KeepStationaryPlant)?FMath::Clamp(Plant[I],0.f,1.f):0.f;
  const FVector Ankle=C[F.Ankle].GetLocation(),CurrentWorld=MeshWorld.TransformPosition(Ankle);Targets[I]=Ankle;
  if(!F.Locked&&W>=.75f){F.Anchor=CurrentWorld;F.Locked=true;F.Releasing=false;F.Weight=1;}
  if(!F.Locked)continue;
  const FVector Anchor=MeshWorld.InverseTransformPosition(F.Anchor);const double Error=(Anchor-Ankle).Size();
  if(Error>80.f){F.Locked=false;F.Weight=0;continue;}
  if(W<.02f||Error>18.f)F.Releasing=true;
  F.Weight=FMath::FInterpConstantTo(F.Weight,F.Releasing?0.f:FMath::SmoothStep(.02f,.75f,W),DeltaTime,12.f);
  if(F.Weight<.001f){F.Locked=false;continue;}
  Targets[I]=Ankle+FVector(Anchor.X-Ankle.X,Anchor.Y-Ankle.Y,0).GetClampedToMaxSize(18.f)*F.Weight;
 }
 for(int I=0;I<2;++I)Correcting[I]=FVector::DistSquared(Targets[I],C[Feet[I].Ankle].GetLocation())>.0001;
 // Full extension is a kinematic singularity: a few cm of boot correction can
 // flip a knee by tens of degrees. Share a small vertical pelvis adjustment
 // between BOTH fixed-length legs, preserving torso/stock/hand contacts.
 double Drop=0;
 for(int I=0;I<2;++I)
 {
  if(CorrectionOnly&&!Correcting[I])continue;
  const auto& F=Feet[I];const FVector H=C[F.Upper].GetLocation(),J=C[F.Knee].GetLocation(),E=C[F.Ankle].GetLocation();
  const double L1=(J-H).Size(),L2=(E-J).Size();
  const double SafeSquared=L1*L1+L2*L2+2*L1*L2*FMath::Cos(FMath::DegreesToRadians(25.));
  const double Horizontal=FVector(H.X-Targets[I].X,H.Y-Targets[I].Y,0).SizeSquared();
  Drop=FMath::Max(Drop,H.Z-Targets[I].Z-FMath::Sqrt(FMath::Max(0.,SafeSquared-Horizontal)));
 }
 PelvisDrop=FMath::FInterpConstantTo(PelvisDrop,FMath::Clamp(float(Drop),0.f,8.f),DeltaTime,45.f);
 for(int B=0;B<C.Num();++B)if(Child(B,Pelvis))C[B].AddToTranslation(-FVector::UpVector*PelvisDrop);
 for(int I=0;I<2;++I)
 {
  // An unplanted swing needs no IK: retain its authored hinge and straight-leg
  // passage instead of inflating an almost-zero pole into a 25-degree bend.
  if(CorrectionOnly&&!Correcting[I])continue;
  const auto& F=Feet[I];const FVector Hip=C[F.Upper].GetLocation(),Knee=C[F.Knee].GetLocation(),Ankle=C[F.Ankle].GetLocation();
  const FVector Axis=(Targets[I]-Hip).GetSafeNormal();
  FVector Bend=FVector::VectorPlaneProject(Forward,Axis);
  if(AuthoredKneeWeight>0.f)
  {
   const FVector SourceAxis=(Ankle-Hip).GetSafeNormal();
   FVector SourceBend=FVector::VectorPlaneProject(Knee-Hip,SourceAxis);
   if(SourceBend.SizeSquared()<.01f)SourceBend=FVector::VectorPlaneProject((C[F.Upper].GetRotation()*Bind[F.Upper].GetRotation().Inverse()).RotateVector(Forward),SourceAxis);
   // Transport the original hinge plane to the corrected ankle axis. Using
   // Knee-Hip against the NEW axis can cross the knee and reverse the pole.
   const FVector Authored=FQuat::FindBetweenNormals(SourceAxis,Axis).RotateVector(SourceBend).GetSafeNormal();
   const FVector Fixed=Bend.GetSafeNormal();Bend=FQuat::Slerp(FQuat::Identity,FQuat::FindBetweenNormals(Fixed,Authored),AuthoredKneeWeight).RotateVector(Fixed);
  }
  if(Bend.SizeSquared()<.0001f)Bend=FVector::VectorPlaneProject(Forward,Axis);
  const FVector Pole=Hip+Bend.GetSafeNormal()*100.f;
  FVector NewKnee,NewAnkle;AnimationCore::SolveTwoBoneIK(Hip,Knee,Ankle,Pole,Targets[I],NewKnee,NewAnkle,false,1.,1.);
  const FVector RestUpper=(Bind[F.Knee].GetLocation()-Bind[F.Upper].GetLocation()).GetSafeNormal(),RestLower=(Bind[F.Ankle].GetLocation()-Bind[F.Knee].GetLocation()).GetSafeNormal();
  const FQuat FixedUpper=(FQuat::FindBetweenNormals(RestUpper,(NewKnee-Hip).GetSafeNormal())*Bind[F.Upper].GetRotation()).GetNormalized();
  const FQuat AuthoredUpper=(FQuat::FindBetweenNormals((Knee-Hip).GetSafeNormal(),(NewKnee-Hip).GetSafeNormal())*C[F.Upper].GetRotation()).GetNormalized();
  const FQuat Upper=FQuat::Slerp(FixedUpper,AuthoredUpper,AuthoredKneeWeight).GetNormalized();
  const FQuat FixedLower=(FQuat::FindBetweenNormals(RestLower,(NewAnkle-NewKnee).GetSafeNormal())*Bind[F.Knee].GetRotation()).GetNormalized();
  const FQuat AuthoredLower=(FQuat::FindBetweenNormals((Ankle-Knee).GetSafeNormal(),(NewAnkle-NewKnee).GetSafeNormal())*C[F.Knee].GetRotation()).GetNormalized();
  const FQuat Lower=FQuat::Slerp(FixedLower,AuthoredLower,AuthoredKneeWeight).GetNormalized();
  const FQuat U=(Upper*C[F.Upper].GetRotation().Inverse()).GetNormalized(),L=(Lower*C[F.Knee].GetRotation().Inverse()).GetNormalized();
  for(int B=0;B<C.Num();++B)
  {
   if(Child(B,F.Ankle))C[B].AddToTranslation(NewAnkle-Ankle);
   else if(Child(B,F.Knee)){C[B].SetLocation(NewKnee+L.RotateVector(C[B].GetLocation()-Knee));C[B].SetRotation((L*C[B].GetRotation()).GetNormalized());}
   else if(Child(B,F.Upper)){C[B].SetLocation(Hip+U.RotateVector(C[B].GetLocation()-Hip));C[B].SetRotation((U*C[B].GetRotation()).GetNormalized());}
  }
 }
 for(int I=0;I<C.Num();++I){const auto K=Bones.MakeCompactPoseIndex(FMeshPoseBoneIndex(I));if(K.GetInt()<0)continue;const FTransform Local=Parents[I]>=0?C[I].GetRelativeTransform(C[Parents[I]]):C[I];Output.Pose[K].Blend(Output.Pose[K],Local,GaitWeight);Output.Pose[K].NormalizeRotation();}
}
