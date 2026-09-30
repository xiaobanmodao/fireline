#include "LiveCarryStudy.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/SkeletalMesh.h"
#include "StaticMeshResources.h"
#include "DrawDebugHelpers.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace {
FTransform ReadHold(const TSharedPtr<FJsonObject>& B)
{
    const auto& P=B->GetArrayField(TEXT("p"));const auto& Q=B->GetArrayField(TEXT("q"));const auto& S=B->GetArrayField(TEXT("s"));
    return FTransform(FQuat(Q[0]->AsNumber(),Q[1]->AsNumber(),Q[2]->AsNumber(),Q[3]->AsNumber()),FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber()),FVector(S[0]->AsNumber(),S[1]->AsNumber(),S[2]->AsNumber()));
}
}
bool ALiveCarryPawn::InitializeStudyOptic()
{
    auto* Optic=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/Fireline/Optics/SM_CoyoteSight.SM_CoyoteSight"));
    if(!Optic||!Optic->FindSocket(TEXT("SightCenter"))||!Optic->FindSocket(TEXT("RailFromRear")))return false;
    const auto& Ref=CastChecked<USkeletalMesh>(StudyGun->GetSkinnedAsset())->GetRefSkeleton();TArray<FTransform> B;
    for(int I=0;I<Ref.GetNum();++I){const int Parent=Ref.GetParentIndex(I);B.Add(Parent<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*B[Parent]);}
    const int Rear=Ref.FindBoneIndex(TEXT("M4_rearsight")),Front=Ref.FindBoneIndex(TEXT("M4_frontsight")),Up=Ref.FindBoneIndex(TEXT("M4_sightup")),Gun=Ref.FindBoneIndex(TEXT("M4_body"));
    if(Rear<0||Front<0||Up<0||Gun<0)return false;
    const FVector Origin=B[Rear].GetLocation(),Vertical=(B[Up].GetLocation()-Origin).GetSafeNormal();
    const FQuat Frame=FRotationMatrix::MakeFromXZ(FVector::VectorPlaneProject(B[Front].GetLocation()-Origin,Vertical),Vertical).ToQuat();
    const FTransform Mount(Frame,Origin+Frame.RotateVector(Optic->FindSocket(TEXT("RailFromRear"))->RelativeLocation));
    OpticMount=Mount.GetRelativeTransform(B[Gun]);StudyOptic->SetStaticMesh(Optic);StudyOptic->SetRelativeTransform(OpticMount);
    const auto& BodyRef=CastChecked<USkeletalMesh>(StudyBody->GetSkinnedAsset())->GetRefSkeleton();TArray<FTransform> BodyB;
    for(int I=0;I<BodyRef.GetNum();++I){const int Parent=BodyRef.GetParentIndex(I);BodyB.Add(Parent<0?BodyRef.GetRefBonePose()[I]:BodyRef.GetRefBonePose()[I]*BodyB[Parent]);}
    const int Head=BodyRef.FindBoneIndex(TEXT("Head"));if(Head<0)return false;
    EyeHeadLocal=BodyB[Head].InverseTransformPosition(HeadContourStudy&&ShowHeadContour?ContourEyeBind:FVector(-3.2,16,190));
    FString Text;TSharedPtr<FJsonObject> Calibration;
    if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectSavedDir()/TEXT("LiveCarry/calibration.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Calibration))return false;
    const FTransform Hold=ReadHold(Calibration->GetObjectField(TEXT("target"))->GetObjectField(TEXT("hold"))->GetObjectField(TEXT("M4_body")));
    StockCenterLocal=Hold.InverseTransformPosition(FVector(-21.29994306,6.14827946,152.83730692));
    StockCornerLocal=Hold.InverseTransformPosition(FVector(-21.29994379,6.00041247,156.98676189));
    OpticRows=TEXT("time,visible,source,selected,mount_cm,mount_deg,proxy_error_cm,sight_x,sight_y,sight_z,eye_x,eye_y,eye_z,axis_x,axis_y,axis_z,stock_x,stock_y,stock_z\n");
    UE_LOG(LogTemp,Display,TEXT("LIVE_COYOTE_READY selected=GoldbergR native_mount=1 body_pose_unchanged=1"));return true;
}
void ALiveCarryPawn::UpdateStudyOptic(double Time)
{
    // Same native rigid attachment used by RangeOptics; update after both
    // poseable meshes have evaluated so the optic never lags a frame.
    const FTransform Gun=StudyGun->GetSocketTransform(TEXT("M4_body"));
    StudyOptic->SetWorldTransform(OpticMount*Gun);
    if(Audit)ShowCoyote=Scenario%2==0;
    const bool Visible=ShowCoyote&&!ShowSource&&StudyGun->IsVisible();StudyOptic->SetVisibility(Visible);
    const FTransform Actual=StudyOptic->GetComponentTransform().GetRelativeTransform(Gun);
    OpticMountErrorCm=FVector::Distance(Actual.GetLocation(),OpticMount.GetLocation());
    // atan2 of the normalized relative quaternion stays accurate near zero;
    // acos(dot) reports a false angle when native bind quaternions round off.
    FQuat Delta=Actual.GetRotation()*OpticMount.GetRotation().Inverse();Delta.Normalize();
    const double MountAngle=FMath::RadiansToDegrees(2*FMath::Atan2(FVector(Delta.X,Delta.Y,Delta.Z).Size(),FMath::Abs(Delta.W)));
    FTransform Sight=StudyOptic->GetSocketTransform(TEXT("SightCenter"));
    if(!ShowCoyote){const FVector Rear=StudyGun->GetSocketLocation(TEXT("M4_rearsight")),Forward=(StudyGun->GetSocketLocation(TEXT("M4_frontsight"))-Rear).GetSafeNormal();Sight=FTransform(FRotationMatrix::MakeFromX(Forward).ToQuat(),Rear);}
    const FVector Eye=StudyBody->GetSocketTransform(TEXT("Head")).TransformPosition(EyeHeadLocal),Center=Sight.GetLocation(),Axis=Sight.GetUnitAxis(EAxis::X),Stock=Gun.TransformPosition(StockCenterLocal);
    SightProxyErrorCm=FVector::VectorPlaneProject(Eye-Center,Axis).Size();
    if(ContactGuides&&!ShowSource)
    {
        DrawDebugLine(GetWorld(),Center-Axis*40,Center+Axis*8,FColor::Green,false,0,0,.25f);
        DrawDebugSphere(GetWorld(),Eye,.7f,12,FColor::Magenta,false,0,0,.15f);
        DrawDebugLine(GetWorld(),Eye,Center+Axis*FVector::DotProduct(Eye-Center,Axis),FColor::Magenta,false,0,0,.15f);
        DrawDebugSphere(GetWorld(),Stock,.8f,12,FColor::Orange,false,0,0,.15f);
        DrawDebugSphere(GetWorld(),Gun.TransformPosition(StockCornerLocal),.6f,10,FColor::Yellow,false,0,0,.15f);
    }
    if(Audit)
    {
        if(OpticMountErrorCm>.0001||MountAngle>.001||StudyOptic->IsVisible()!=Visible)UE_LOG(LogTemp,Error,TEXT("LIVE_COYOTE_INVALID mount=%.6f/%.6f"),OpticMountErrorCm,MountAngle);
        const FTransform World=StudyBody->GetComponentTransform();const FVector LC=World.InverseTransformPosition(Center),LE=World.InverseTransformPosition(Eye),LS=World.InverseTransformPosition(Stock),LA=World.InverseTransformVectorNoScale(Axis);
        OpticRows+=FString::Printf(TEXT("%.8f,%d,%d,%d,%.9f,%.9f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.9f,%.9f,%.9f,%.8f,%.8f,%.8f\n"),Time,Visible,ShowSource,ShowCoyote,OpticMountErrorCm,MountAngle,SightProxyErrorCm,LC.X,LC.Y,LC.Z,LE.X,LE.Y,LE.Z,LA.X,LA.Y,LA.Z,LS.X,LS.Y,LS.Z);
    }
}
void ALiveCarryPawn::ExportStudyOptic(const FString& Folder)
{
    FFileHelper::SaveStringToFile(OpticRows,*(Folder/TEXT("optic.csv")));
    const auto* Render=StudyOptic->GetStaticMesh()->GetRenderData();if(!Render||Render->LODResources.IsEmpty())return;const auto& LOD=Render->LODResources[0];
    FString Vertices=TEXT("vertex,x,y,z\n"),Triangles=TEXT("face,a,b,c\n");
    for(uint32 I=0;I<LOD.VertexBuffers.PositionVertexBuffer.GetNumVertices();++I){const FVector3f V=LOD.VertexBuffers.PositionVertexBuffer.VertexPosition(I);Vertices+=FString::Printf(TEXT("%u,%.9f,%.9f,%.9f\n"),I,double(V.X),double(V.Y),double(V.Z));}
    for(int I=0;I<LOD.IndexBuffer.GetNumIndices();I+=3)Triangles+=FString::Printf(TEXT("%d,%u,%u,%u\n"),I/3,LOD.IndexBuffer.GetIndex(I),LOD.IndexBuffer.GetIndex(I+1),LOD.IndexBuffer.GetIndex(I+2));
    FFileHelper::SaveStringToFile(Vertices,*(Folder/TEXT("optic.vertices.csv")));FFileHelper::SaveStringToFile(Triangles,*(Folder/TEXT("optic.triangles.csv")));
}

// Geometry-only review: native bone transforms, scales and movement registration
// remain the same. No failed offline contact pose is consumed by this host.
bool ALiveCarryPawn::InitializeHeadContour()
{
    OriginalBody=Cast<USkeletalMesh>(StudyBody->GetSkinnedAsset());
    ContourBody=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/HeadContourStudy/SK_RyanHeadContourRounded.SK_RyanHeadContourRounded"));
    if(!OriginalBody||!ContourBody)return false;
    const auto& A=OriginalBody->GetRefSkeleton();const auto& B=ContourBody->GetRefSkeleton();
    if(A.GetNum()!=131||A.GetNum()!=B.GetNum())return false;
    for(int I=0;I<A.GetNum();++I)
        if(A.GetBoneName(I)!=B.GetBoneName(I)||A.GetParentIndex(I)!=B.GetParentIndex(I)||!A.GetRefBonePose()[I].Equals(B.GetRefBonePose()[I],.000001))return false;
    FString Text;TSharedPtr<FJsonObject> Calibration;
    if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectSavedDir()/TEXT("LiveCarry/head-contour.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Calibration))return false;
    const auto& V=Calibration->GetArrayField(TEXT("eye_proxy_bind_cm"));if(V.Num()!=3)return false;
    ContourEyeBind=FVector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber());
    ShowHeadContour=!FParse::Param(FCommandLine::Get(),TEXT("StudyOriginalHead"));SelectHeadContour();
    UE_LOG(LogTemp,Display,TEXT("LIVE_HEAD_CONTOUR_READY original_bind=1 original_weights=1 pose_unchanged=1 static_contact_unfinished=1"));return true;
}
void ALiveCarryPawn::SelectHeadContour()
{
    const auto Local=StudyBody->LastFrameLocal;const auto Names=StudyBody->LastFrameNames;
    TArray<FTransform> Before;
    if(!Local.IsEmpty())for(const auto& Name:Names)Before.Add(StudyBody->GetSocketTransform(Name,RTS_Component));
    StudyBody->SetSkinnedAssetAndUpdate(ShowHeadContour?ContourBody:OriginalBody);
    if(!Local.IsEmpty())
    {
        StudyBody->ApplyFrame(Local,Names);
        for(int I=0;I<Names.Num();++I)
            if(!Before[I].Equals(StudyBody->GetSocketTransform(Names[I],RTS_Component),.000001))UE_LOG(LogTemp,Error,TEXT("LIVE_HEAD_SWAP_INVALID bone=%s"),*Names[I].ToString());
        if(Audit)UE_LOG(LogTemp,Display,TEXT("LIVE_HEAD_SWAP_VERIFIED bones=%d contour=%d"),Names.Num(),ShowHeadContour);
    }
    const auto& Ref=CastChecked<USkeletalMesh>(StudyBody->GetSkinnedAsset())->GetRefSkeleton();TArray<FTransform> Bind;
    for(int I=0;I<Ref.GetNum();++I){const int P=Ref.GetParentIndex(I);Bind.Add(P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bind[P]);}
    const int Head=Ref.FindBoneIndex(TEXT("Head"));check(Head>=0);
    EyeHeadLocal=Bind[Head].InverseTransformPosition(ShowHeadContour?ContourEyeBind:FVector(-3.2,16,190));
}
