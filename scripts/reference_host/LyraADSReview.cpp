#include "LyraADSReview.h"
#include "AlsCharacter.h"
#include "Animation/AnimSequence.h"
#include "AssetCompilingManager.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindow.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"

namespace {
FTransform ReadTransform(const TSharedPtr<FJsonObject>& O)
{
    const auto& P=O->GetArrayField(TEXT("p"));const auto& Q=O->GetArrayField(TEXT("q"));const auto& S=O->GetArrayField(TEXT("s"));
    return FTransform(FQuat(Q[0]->AsNumber(),Q[1]->AsNumber(),Q[2]->AsNumber(),Q[3]->AsNumber()).GetNormalized(),FVector(P[0]->AsNumber(),P[1]->AsNumber(),P[2]->AsNumber()),FVector(S[0]->AsNumber(),S[1]->AsNumber(),S[2]->AsNumber()));
}
FString Row(int Frame,double Time,const FName& Bone,const FTransform& T)
{
    const auto P=T.GetTranslation(),S=T.GetScale3D();const auto Q=T.GetRotation();
    return FString::Printf(TEXT("%d,%.8f,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),Frame,Time,*Bone.ToString(),P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z);
}
}
ALyraADSReview::ALyraADSReview()
{
    PrimaryActorTick.bCanEverTick=true;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
    Body=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("OriginalManny"));Body->SetupAttachment(RootComponent);
    Gun=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("OriginalRifle"));Gun->SetupAttachment(Body);
    for(auto* M:{Body.Get(),Gun.Get()}){M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetComponentTickEnabled(false);M->BoundsScale=2;M->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;}
}
bool ALyraADSReview::Initialize(AAlsCharacter* Source)
{
    Capture=FParse::Param(FCommandLine::Get(),TEXT("LyraADSCapture"));
    RawClip=FParse::Param(FCommandLine::Get(),TEXT("LyraADSRaw"));
    ToggleAudit=FParse::Param(FCommandLine::Get(),TEXT("LyraADSToggleAudit"));
    FString Label;FParse::Value(FCommandLine::Get(),TEXT("ADSRun="),Label);if(Label.IsEmpty())Label=TEXT("interactive");
    Directory=FPaths::ProjectSavedDir()/TEXT("LyraADSReview")/Label;IFileManager::Get().MakeDirectory(*Directory,true);
    FString Text;TSharedPtr<FJsonObject> Equipment;
    if(!FFileHelper::LoadFileToString(Text,*(FPaths::ProjectDir()/TEXT("../ADSLayoutStudy/lyra-equipment.json")))||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Equipment))return false;
    const auto& Actors=Equipment->GetArrayField(TEXT("actors"));if(Actors.Num()!=1)return false;const auto Info=Actors[0]->AsObject();
    const auto& Parts=Info->GetArrayField(TEXT("components"));if(Parts.Num()!=1)return false;const auto Part=Parts[0]->AsObject();
    if(Info->GetStringField(TEXT("socket"))!=TEXT("weapon_r")||!Part->HasTypedField<EJson::Null>(TEXT("parent")))return false;
    auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Characters/Heroes/Mannequin/Meshes/SKM_Manny.SKM_Manny"));
    auto* Weapon=LoadObject<USkeletalMesh>(nullptr,*Part->GetStringField(TEXT("mesh")));
    Clip=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Characters/Heroes/Mannequin/Animations/Locomotion/Rifle/MM_Rifle_Idle_ADS.MM_Rifle_Idle_ADS"));
    if(!Mesh||!Weapon||!Clip)return false;FAssetCompilingManager::Get().FinishAllCompilation();
    Body->SetSkeletalMesh(Mesh);Body->PlayAnimation(Clip,true);Gun->SetSkeletalMesh(Weapon);
    Body->SetDisablePostProcessBlueprint(RawClip);
    Gun->AttachToComponent(Body,FAttachmentTransformRules::KeepRelativeTransform,FName(*Info->GetStringField(TEXT("socket"))));
    Gun->SetRelativeTransform(ReadTransform(Part->GetObjectField(TEXT("world_at_identity_actor")))*ReadTransform(Info->GetObjectField(TEXT("equipment_relative"))));
    SetActorLocation(Source->GetActorLocation()-FVector(0,0,Source->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()));
    auto* PC=GetWorld()->GetFirstPlayerController();if(!PC)return false;Source->DisableInput(PC);Source->SetActorHiddenInGame(true);
    UWidgetLayoutLibrary::RemoveAllWidgets(PC);
    if(auto* Input=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))Input->ClearAllMappings();
    TArray<AActor*> Attached;Source->GetAttachedActors(Attached,true,true);for(auto* A:Attached)A->SetActorHiddenInGame(true);
    Camera=GetWorld()->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(35);Camera->GetCameraComponent()->bConstrainAspectRatio=false;PC->SetViewTarget(Camera);PC->ClientSetHUD(ALyraADSReviewHUD::StaticClass());
    if(!Capture){EnableInput(PC);InputComponent->BindKey(EKeys::One,IE_Pressed,this,&ALyraADSReview::SetFront);InputComponent->BindKey(EKeys::Two,IE_Pressed,this,&ALyraADSReview::SetRight);InputComponent->BindKey(EKeys::Three,IE_Pressed,this,&ALyraADSReview::SetBack);InputComponent->BindKey(EKeys::Four,IE_Pressed,this,&ALyraADSReview::SetLeft);InputComponent->BindKey(EKeys::SpaceBar,IE_Pressed,this,&ALyraADSReview::TogglePause);InputComponent->BindKey(EKeys::Z,IE_Pressed,this,&ALyraADSReview::ToggleClose);InputComponent->BindKey(EKeys::P,IE_Pressed,this,&ALyraADSReview::TogglePostProcess);InputComponent->BindAxisKey(EKeys::MouseX,this,&ALyraADSReview::OrbitX);InputComponent->BindAxisKey(EKeys::MouseY,this,&ALyraADSReview::OrbitY);}
    if(Capture)
    {
        const auto Window=GetWorld()->GetGameViewport()->GetWindow();const auto Native=Window.IsValid()?Window->GetNativeWindow():nullptr;
        if(!Native.IsValid()||Native->GetOSWindowHandle()!=nullptr)return false;
        FApp::SetFixedDeltaTime(1./30);FApp::SetUseFixedTimeStep(true);
        Poses=GunPoses=TEXT("frame,time,bone,x,y,z,qx,qy,qz,qw,sx,sy,sz\n");
        UE_LOG(LogTemp,Display,TEXT("LYRA_ADS_BACKGROUND native_window=0 physical_input=0"));
    }
    UE_LOG(LogTemp,Display,TEXT("LYRA_ADS_READY original_mesh=1 original_weapon=1 original_equipment=1 single_node_clip=1 source_game_graph=0"));return true;
}
void ALyraADSReview::TogglePostProcess()
{
    RawClip=!RawClip;Body->SetDisablePostProcessBlueprint(RawClip);
}
void ALyraADSReview::Tick(float Delta)
{
    Super::Tick(Delta);if(!Clip)return;
    Clock+=Delta;if(!Paused)AnimationTime+=Delta;
    if(Capture&&ToggleAudit&&ToggleStage<2&&Clock>=ToggleStage+1)
    {
        TogglePostProcess();++ToggleStage;
        UE_LOG(LogTemp,Display,TEXT("LYRA_ADS_PP_TOGGLE frame=%d raw=%d"),Frame,RawClip?1:0);
    }
    Body->SetPosition(FMath::Fmod(AnimationTime,Clip->GetPlayLength()),false);Body->TickAnimation(0,false);Body->RefreshBoneTransforms();
    Gun->RefreshBoneTransforms();Gun->UpdateComponentToWorld();
    if(Capture){View=FMath::Min(3,int(Clock));Yaw=View==1?-90:View==2?180:View==3?90:0;}
    const float D=Closeup?265:440;const float Angle=FMath::DegreesToRadians(Yaw),Tilt=FMath::DegreesToRadians(Pitch);
    const FVector Focus=GetActorLocation()+FVector(0,15,Closeup?145:100),Eye=Focus+FVector(FMath::Sin(Angle)*D*FMath::Cos(Tilt),FMath::Cos(Angle)*D*FMath::Cos(Tilt),D*FMath::Sin(Tilt));
    Camera->SetActorLocationAndRotation(Eye,(Focus-Eye).Rotation());
    if(Capture)
    {
        const auto& Ref=Body->GetSkeletalMeshAsset()->GetRefSkeleton();for(int I=0;I<Ref.GetNum();++I)Poses+=Row(Frame,AnimationTime,Ref.GetBoneName(I),Body->GetSocketTransform(Ref.GetBoneName(I),RTS_Component));
        const auto& G=Gun->GetSkeletalMeshAsset()->GetRefSkeleton();const auto World=Body->GetComponentTransform();for(int I=0;I<G.GetNum();++I)GunPoses+=Row(Frame,AnimationTime,G.GetBoneName(I),Gun->GetSocketTransform(G.GetBoneName(I)).GetRelativeTransform(World));
        if(Shot<4&&Clock>=Shot+0.5){FScreenshotRequest::RequestScreenshot(Directory/FString::Printf(TEXT("shot-%02d.png"),Shot),true,false);++Shot;}
        ++Frame;
        if(Clock>=4.3){FFileHelper::SaveStringToFile(Poses,*(Directory/TEXT("poses.csv")));FFileHelper::SaveStringToFile(GunPoses,*(Directory/TEXT("gun-poses.csv")));UE_LOG(LogTemp,Display,TEXT("LYRA_ADS_COMPLETE frames=%d"),Frame);FPlatformMisc::RequestExit(false);}
    }
}
void ALyraADSReviewHUD::DrawHUD()
{
    Super::DrawHUD();if(!Canvas)return;TActorIterator<ALyraADSReview> It(GetWorld());if(!It)return;auto* R=*It;
    DrawRect(FLinearColor(.015,.025,.04,.87),0,0,Canvas->ClipX,92);
    DrawText(TEXT("REFERENCE / ORIGINAL LYRA ADS CLIP + EQUIPMENT MOUNT"),FLinearColor::White,22,12,nullptr,1.1f);
    DrawText(TEXT("Mouse orbit | 1-4 views | SPACE pause | Z close-up / full body | P post-process A/B"),FLinearColor(.65,.85,1),22,42);
    DrawText(FString::Printf(TEXT("%.2fs | mesh post-process %s | not final Lyra gameplay graph | no Fireline retarget"),R->AnimationTime,R->RawClip?TEXT("OFF / raw clip"):TEXT("ON / original")),FLinearColor::White,22,68);
}
