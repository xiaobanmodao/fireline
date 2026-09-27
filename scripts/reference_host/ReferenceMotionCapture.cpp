#include "ReferenceMotionCapture.h"
#include "AlsCharacter.h"
#include "Utility/AlsGameplayTags.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"

namespace {
FString Row(const FTransform& T)
{
    const auto P=T.GetLocation();const auto Q=T.GetRotation();const auto S=T.GetScale3D();
    return FString::Printf(TEXT("%.8f,%.8f,%.8f,%.9f,%.9f,%.9f,%.9f,%.8f,%.8f,%.8f\n"),
        P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z);
}
const TCHAR* Header=TEXT("x,y,z,qx,qy,qz,qw,sx,sy,sz\n");
}
void FReferenceMotionCapture::Before(UWorld* World)
{
    if(Finished||!World||!World->IsGameWorld()||World->GetTimeSeconds()<2) return;
    auto* PC=World->GetFirstPlayerController();auto* C=PC?Cast<AAlsCharacter>(PC->GetPawn()):nullptr;
    if(!C)return;
    if(Start<0)
    {
        Character=C;Start=World->GetTimeSeconds();C->DisableInput(PC);
        C->SetOverlayMode(AlsOverlayModeTags::Rifle);
        C->SetDesiredGait(AlsGaitTags::Walking);
        C->SetDesiredRotationMode(AlsRotationModeTags::ViewDirection);
        PC->SetControlRotation(FRotator(0,C->GetActorRotation().Yaw,0));
        C->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
        C->GetMesh()->bEnableUpdateRateOptimizations=false;
        FApp::SetFixedDeltaTime(1.0/60.0);FApp::SetUseFixedTimeStep(true);
        Poses=FString(TEXT("frame,time,bone,"))+Header;
        States=TEXT("frame,time,speed,actor_x,actor_y,actor_z\n");
        const FString Folder=FPaths::ProjectSavedDir()/TEXT("WholeCarrySource");
        IFileManager::Get().MakeDirectory(*Folder,true);
        const auto& Ref=C->GetMesh()->GetSkeletalMeshAsset()->GetRefSkeleton();
        auto Bind=Ref.GetRefBonePose();FString Rows=FString(TEXT("bone,parent,"))+Header;
        for(int I=0;I<Bind.Num();++I){const int P=Ref.GetParentIndex(I);if(P>=0)Bind[I]*=Bind[P];
            Rows+=Ref.GetBoneName(I).ToString()+TEXT(",")+(P>=0?Ref.GetBoneName(P).ToString():TEXT("None"))+TEXT(",")+Row(Bind[I]);}
        FFileHelper::SaveStringToFile(Rows,*(Folder/TEXT("bind.csv")));
    }
    const double T=World->GetTimeSeconds()-Start;
    if(T>=1.5&&T<5.5)C->AddMovementInput(C->GetActorForwardVector(),1.f);
}
void FReferenceMotionCapture::After(UWorld* World)
{
    if(Finished||Start<0||!Character.IsValid()||Character->GetWorld()!=World)return;
    auto* C=Character.Get();auto* Mesh=C->GetMesh();const double T=World->GetTimeSeconds()-Start;
    const auto& Ref=Mesh->GetSkeletalMeshAsset()->GetRefSkeleton();
    const auto& Pose=Mesh->GetComponentSpaceTransforms();
    for(int I=0;I<Pose.Num();++I)Poses+=FString::Printf(TEXT("%d,%.8f,%s,"),Frame,T,*Ref.GetBoneName(I).ToString())+Row(Pose[I]);
    const auto P=C->GetActorLocation();States+=FString::Printf(TEXT("%d,%.8f,%.8f,%.8f,%.8f,%.8f\n"),Frame,T,C->GetVelocity().Size2D(),P.X,P.Y,P.Z);
    ++Frame;
    if(T>=7.5)
    {
        Finished=true;const FString Folder=FPaths::ProjectSavedDir()/TEXT("WholeCarrySource");
        FFileHelper::SaveStringToFile(Poses,*(Folder/TEXT("poses.csv")));
        FFileHelper::SaveStringToFile(States,*(Folder/TEXT("states.csv")));
        UE_LOG(LogTemp,Display,TEXT("WHOLE_CARRY_SOURCE_COMPLETE frames=%d"),Frame);
        FPlatformMisc::RequestExit(false);
    }
}
