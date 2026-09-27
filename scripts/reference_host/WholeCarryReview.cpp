#include "WholeCarryReview.h"
#include "AlsCharacter.h"
#include "AssetCompilingManager.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Utility/AlsGameplayTags.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/InputComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/Canvas.h"
#include "Engine/SkeletalMesh.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "UnrealClient.h"
#include "Materials/MaterialInterface.h"

void UWholeCarryMesh::SetLocalFrame(const TArray<FTransform>& Frame)
{
    if(Frame.Num()!=BoneSpaceTransforms.Num())return;
    BoneSpaceTransforms=Frame;MarkRefreshTransformDirty();
}
bool FWholeCarryClip::Load(const FString& D,const FString& N)
{
    TArray<FString> Lines;
    if(!FFileHelper::LoadFileToStringArray(Lines,*(D/(N+TEXT("-bones.txt")))))return false;
    for(const auto& L:Lines)Names.Add(FName(L));
    TArray<uint8> Bytes;
    if(!FFileHelper::LoadFileToArray(Bytes,*(D/(N+TEXT(".bin")))))return false;
    FMemoryReader Reader(Bytes);uint32 Magic,Count,FramesCount;Reader<<Magic<<Count<<FramesCount;
    if(Magic!=0x46574331||Count!=uint32(Names.Num())||Count>1024||FramesCount>10000)return false;
    if(Bytes.Num()!=12+Count*FramesCount*40)return false;
    Frames.SetNum(FramesCount);
    for(auto& Frame:Frames){Frame.Reserve(Count);for(uint32 B=0;B<Count;++B){float V[10];Reader.Serialize(V,sizeof(V));Frame.Emplace(FQuat(V[3],V[4],V[5],V[6]).GetNormalized(),FVector(V[0],V[1],V[2]),FVector(V[7],V[8],V[9]));}}
    return true;
}
AWholeCarryReview::AWholeCarryReview()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PrePhysics;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
}
bool AWholeCarryReview::Initialize(AAlsCharacter* Source)
{
    Proportions=FParse::Param(FCommandLine::Get(),TEXT("BodyProportionReview"));
    const FString Directory=FPaths::ProjectSavedDir()/(Proportions?TEXT("BodyProportionReview"):TEXT("WholeCarryReview"));ReviewDirectory=Directory;
    const TCHAR* Labels[]={TEXT("source"),TEXT("retarget"),TEXT("contact")};
    Clips.SetNum(3);
    for(int I=0;I<3;++I)if(!Clips[I].Load(Directory,Labels[I]))return false;
    auto* Body=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_RyanAction.SK_RyanAction"));
    auto* Gun=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_M4Action.SK_M4Action"));
    auto* RevisedBody=Proportions?LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/BodyProportionStudy/SK_RyanProportion.SK_RyanProportion")):Body;
    if(!Body||!Gun||!RevisedBody){UE_LOG(LogTemp,Error,TEXT("Missing target mesh"));return false;}
    FAssetCompilingManager::Get().FinishAllCompilation();
    Source->SetOverlayMode(AlsOverlayModeTags::Rifle);
    for(int I=0;I<3;++I)
    {
        auto* M=NewObject<UWholeCarryMesh>(this);AddInstanceComponent(M);M->SetupAttachment(GetRootComponent());
        M->SetSkinnedAssetAndUpdate(I?(I==2?RevisedBody:Body):Source->GetMesh()->GetSkeletalMeshAsset());
        M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->BoundsScale=3;M->RegisterComponent();Bodies.Add(M);
        const auto& Ref=CastChecked<USkeletalMesh>(M->GetSkinnedAsset())->GetRefSkeleton();
        if(Ref.GetNum()!=Clips[I].Names.Num()){UE_LOG(LogTemp,Error,TEXT("BONE_COUNT %d mesh=%d clip=%d"),I,Ref.GetNum(),Clips[I].Names.Num());return false;}
        TArray<int> Mapping;TArray<FName> Ordered;
        for(int B=0;B<Ref.GetNum();++B){int K=Clips[I].Names.Find(Ref.GetBoneName(B));if(K==INDEX_NONE){UE_LOG(LogTemp,Error,TEXT("BONE_MISSING %s"),*Ref.GetBoneName(B).ToString());return false;}Mapping.Add(K);Ordered.Add(Ref.GetBoneName(B));}
        for(auto& Frame:Clips[I].Frames){auto Before=Frame;for(int B=0;B<Ref.GetNum();++B)Frame[B]=Before[Mapping[B]];}Clips[I].Names=Ordered;
        FString Hierarchy;for(int B=0;B<Ref.GetNum();++B){const int P=Ref.GetParentIndex(B);Hierarchy+=Ref.GetBoneName(B).ToString()+TEXT(",")+(P>=0?Ref.GetBoneName(P).ToString():TEXT("None"))+TEXT("\n");}
        FFileHelper::SaveStringToFile(Hierarchy,*(Directory/FString::Printf(TEXT("mesh-%d-hierarchy.csv"),I)));
        if(!I){TArray<USkeletalMeshComponent*> Parts;Source->GetComponents(Parts);for(auto* Part:Parts)if(Part->GetName()==TEXT("OverlaySkeletalMesh")){
            SourceGun=NewObject<USkeletalMeshComponent>(this);AddInstanceComponent(SourceGun);SourceGun->SetupAttachment(M,Part->GetAttachSocketName());SourceGun->SetSkeletalMesh(Part->GetSkeletalMeshAsset());SourceGun->SetRelativeTransform(Part->GetRelativeTransform());SourceGun->SetCollisionEnabled(ECollisionEnabled::NoCollision);SourceGun->RegisterComponent();break;}}

        if(I)
        {
            for(int J=0;J<M->GetNumMaterials();++J)
            {
                const FString Slot=M->GetMaterialSlotNames()[J].ToString();const TCHAR* Path=nullptr;
                if(Slot.Contains(TEXT("ArmorBase")))Path=TEXT("/Game/Fireline/Materials/M_Player.M_Player");
                if(Slot.Contains(TEXT("ArmorUnder")))Path=TEXT("/Game/Fireline/Materials/M_Under.M_Under");
                if(Slot.Contains(TEXT("Visor")))Path=TEXT("/Game/Fireline/Materials/M_Visor.M_Visor");
                if(Path)M->SetMaterial(J,LoadObject<UMaterialInterface>(nullptr,Path));
            }
            auto* G=NewObject<UWholeCarryMesh>(this);AddInstanceComponent(G);G->SetupAttachment(GetRootComponent());G->SetSkinnedAssetAndUpdate(Gun);
            G->SetCollisionEnabled(ECollisionEnabled::NoCollision);G->BoundsScale=3;G->RegisterComponent();Guns.Add(G);
        }
    }
    FString ScaleText;if(!FFileHelper::LoadFileToString(ScaleText,*(Directory/TEXT("leg-scale.txt"))))return false;LegScale=FCString::Atof(*ScaleText);
    if(LegScale<.5f||LegScale>2.f)return false;
    // Read original render vertices and weights without changing CPU access,
    // the mesh, bind pose, materials or any source asset.
    if(Capture || FParse::Param(FCommandLine::Get(),TEXT("WholeCarryCapture")))
    {
        auto* AuditBody=RevisedBody;
        const auto& LOD=AuditBody->GetResourceForRendering()->LODRenderData[0];
        FString Verts=TEXT("vertex,x,y,z\n"),Weights=TEXT("vertex,bone,weight\n");
        for(uint32 V=0;V<LOD.GetNumVertices();++V){const auto P=LOD.StaticVertexBuffers.PositionVertexBuffer.VertexPosition(V);Verts+=FString::Printf(TEXT("%u,%.9f,%.9f,%.9f\n"),V,P.X,P.Y,P.Z);}
        for(const auto& Section:LOD.RenderSections)for(uint32 V=Section.BaseVertexIndex;V<Section.BaseVertexIndex+Section.NumVertices;++V)
            for(uint32 J=0;J<LOD.SkinWeightVertexBuffer.GetMaxBoneInfluences();++J){const auto W=LOD.SkinWeightVertexBuffer.GetBoneWeight(V,J);if(W){const auto B=Section.BoneMap[LOD.SkinWeightVertexBuffer.GetBoneIndex(V,J)];Weights+=FString::Printf(TEXT("%u,%s,%u\n"),V,*AuditBody->GetRefSkeleton().GetBoneName(B).ToString(),W);}}
        FFileHelper::SaveStringToFile(Verts,*(Directory/TEXT("native-bind-vertices.csv")));FFileHelper::SaveStringToFile(Weights,*(Directory/TEXT("native-skin-weights.csv")));
    }
    TArray<uint8> Bytes;
    if(!FFileHelper::LoadFileToArray(Bytes,*(Directory/TEXT("movement.bin"))))return false;
    if(Bytes.Num()!=Clips[0].Frames.Num()*16)return false;
    FMemoryReader Reader(Bytes);FVector Origin;float Distance=0;FVector Last;
    for(int I=0;I<Clips[0].Frames.Num();++I){float V[4];Reader.Serialize(V,sizeof(V));const FVector P(V[1],V[2],V[3]);if(I)Distance+=FVector::Dist2D(P,Last);Last=P;Speeds.Add(V[0]);Travel.Add(FVector(0,Distance,0));}
    Camera=GetWorld()->SpawnActor<ACameraActor>();Camera->GetCameraComponent()->SetFieldOfView(48);
    auto* PC=GetWorld()->GetFirstPlayerController();Source->DisableInput(PC);Source->SetActorHiddenInGame(true);
    PC->SetViewTarget(Camera);PC->ClientSetHUD(AWholeCarryReviewHUD::StaticClass());
    UWidgetLayoutLibrary::RemoveAllWidgets(PC);
    if(auto* Sub=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))Sub->ClearAllMappings();
    // Event bindings preserve a short press+release occurring between frames.
    // Polling only IsInputKeyDown loses those events at a capped frame rate.
    PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;EnableInput(PC);
    InputComponent->Priority=1000;
    const FKey ReviewKeys[]={EKeys::SpaceBar,EKeys::Z,EKeys::Tab,EKeys::R,EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four,EKeys::Five,EKeys::Left,EKeys::Right};
    for(const FKey Key:ReviewKeys){FInputKeyBinding Binding(FInputChord(Key),IE_Pressed);Binding.bConsumeInput=true;Binding.KeyDelegate.GetDelegateForManualSet().BindLambda([this,Key]{PendingKeys.Add(Key);});InputComponent->KeyBindings.Add(Binding);}
    FAssetCompilingManager::Get().FinishAllCompilation();
    Capture=FParse::Param(FCommandLine::Get(),TEXT("WholeCarryCapture"));
    UE_LOG(LogTemp,Display,TEXT("WHOLE_CARRY_REVIEW_READY frames=%d nativeBones=%d"),Clips[0].Frames.Num(),Clips[1].Names.Num());
    return true;
}
bool AWholeCarryReview::Pressed(APlayerController* PC,const FKey& Key)
{
    return PendingKeys.Remove(Key)>0;
}
void AWholeCarryReview::Tick(float Delta)
{
    Super::Tick(Delta);if(Bodies.Num()!=3||!Camera)return;
    auto* PC=GetWorld()->GetFirstPlayerController();if(!PC)return;
    if(Pressed(PC,EKeys::SpaceBar))Paused=!Paused;
    if(Pressed(PC,EKeys::Z))Slow=!Slow;
    if(Pressed(PC,EKeys::Tab))Comparison=!Comparison;
    if(Pressed(PC,EKeys::Five)){Closeup=!Closeup;Comparison=false;}
    if(Pressed(PC,EKeys::R)){Clock=0;Paused=false;}
    const FKey Views[]={EKeys::One,EKeys::Two,EKeys::Three,EKeys::Four};
    for(int I=0;I<4;++I)if(Pressed(PC,Views[I]))View=I;
    if(Pressed(PC,EKeys::Left)){Paused=true;Clock=FMath::Max(0.,Clock-1./60.);}
    if(Pressed(PC,EKeys::Right)){Paused=true;Clock+=1./60.;}
    if(!Paused)Clock+=FMath::Min(Delta,.05f)*(Slow?.25f:1.f);
    const double Duration=(Clips[0].Frames.Num()-1)/60.;if(Clock>=Duration){Clock=Duration;Paused=true;}Time=Clock;
    // Deterministic multi-angle inspection, independent of user's mouse input.
    if(Capture){const float Times[]={.15f,.60f,.85f,2.70f,3.05f,4.60f,5.0f};Comparison=ShotIndex<4;Closeup=Proportions&&ShotIndex>=28;View=ShotIndex%4;Time=Times[(ShotIndex/4)%7];}
    const double FrameTime=Time*60;const int A=FMath::FloorToInt(FrameTime),B=FMath::Min(A+1,Clips[0].Frames.Num()-1);const float Alpha=FrameTime-A;
    Speed=FMath::Lerp(Speeds[A],Speeds[B],Alpha);
    const FVector Base(-1500,-1500,0),Move=FMath::Lerp(Travel[A],Travel[B],Alpha);
    const FVector Dirs[]={FVector(0,1,.12),FVector(-1,0,.12),FVector(0,-1,.12),FVector(1,0,.12)};
    const FVector ScreenRight(Dirs[View].Y,-Dirs[View].X,0);
    for(int I=0;I<3;++I)
    {
        TArray<FTransform> Pose;Pose.SetNum(Clips[I].Names.Num());
        for(int N=0;N<Pose.Num();++N)Pose[N].Blend(Clips[I].Frames[A][N],Clips[I].Frames[B][N],Alpha);
        // Measured native boot sole minimum was -0.678 cm in the recorded clip.
        const FVector Offset=ScreenRight*(Comparison?(I-1)*185:0)+FVector(0,0,I?.70f:0.f);
        Bodies[I]->SetWorldLocation(Base+Move*(I?LegScale:1.f)+Offset);Bodies[I]->SetVisibility(Comparison||I==2);Bodies[I]->SetLocalFrame(Pose);
        if(I)
        {
            auto* G=Guns[I-1].Get();const auto& Ref=CastChecked<USkeletalMesh>(G->GetSkinnedAsset())->GetRefSkeleton();TArray<FTransform> GP=Ref.GetRefBonePose();
            for(int N=0;N<GP.Num();++N){const int K=Clips[I].Names.Find(Ref.GetBoneName(N));if(K!=INDEX_NONE)GP[N]=Pose[K];}
            G->SetWorldLocation(Base+Move*(I?LegScale:1.f)+Offset);G->SetVisibility(Comparison||I==2);G->SetLocalFrame(GP);
        }
    }
    if(SourceGun)SourceGun->SetVisibility(Comparison);
    const float Distance=Comparison?730:(Closeup?245:480);
    const FVector Focus=Base+Move*LegScale+FVector(0,0,Closeup?148:100);const FVector Eye=Focus+Dirs[View]*Distance;
    Camera->SetActorLocationAndRotation(Eye,(Focus-Eye).Rotation());
    if(Capture)
    {
        static float Wait=0;Wait+=Delta;
        if(Wait>.8f && ShotIndex<(Proportions?56:28)){Wait=0;FString Rows=TEXT("time,bone,x,y,z,qx,qy,qz,qw,sx,sy,sz\n");
            Bodies[2]->RefreshBoneTransforms();const auto& Transforms=Bodies[2]->GetComponentSpaceTransforms();
            for(int N=0;N<Transforms.Num();++N){const auto& T=Transforms[N];const auto P=T.GetLocation(),Scale=T.GetScale3D();const auto Q=T.GetRotation();Rows+=FString::Printf(TEXT("%.6f,%s,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f,%.9f\n"),Time,*Clips[2].Names[N].ToString(),P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,Scale.X,Scale.Y,Scale.Z);}
            FFileHelper::SaveStringToFile(Rows,*(ReviewDirectory/FString::Printf(TEXT("pose-%02d.csv"),ShotIndex)));
            FScreenshotRequest::RequestScreenshot(ReviewDirectory/FString::Printf(TEXT("shot-%02d.png"),ShotIndex),true,false);++ShotIndex;}
        if(ShotIndex>=(Proportions?56:28) && Wait>.35f){UE_LOG(LogTemp,Display,TEXT("WHOLE_CARRY_VISUAL_CAPTURE_COMPLETE"));FPlatformMisc::RequestExit(false);}
    }
}
void AWholeCarryReviewHUD::DrawHUD()
{
    Super::DrawHUD();if(!Canvas)return;
    TActorIterator<AWholeCarryReview> It(GetWorld()); AWholeCarryReview* R=It?*It:nullptr;if(!R)return;
    DrawRect(FLinearColor(0.015,.025,.04,.86),0,0,Canvas->ClipX,88);
    DrawText(R->Proportions?TEXT("M4 / RYAN BODY PROPORTION REVIEW"):TEXT("M4 / WHOLE-BODY MOTION STUDY"),FLinearColor::White,22,12,GEngine->GetMediumFont(),1.1f);
    DrawText(TEXT("SPACE pause   Z slow   1/2/3/4 views   5 upper body   TAB compare/focus   arrows frame   R restart"),FLinearColor(.65,.85,1),22,40,GEngine->GetSmallFont(),1.f);
    DrawText(FString::Printf(TEXT("%.2fs   source %.0f cm/s   %s   %s"),R->Time,R->Speed,R->Paused?TEXT("PAUSED"):TEXT("PLAYING"),R->Slow?TEXT("0.25x"):TEXT("1x")),FLinearColor::White,22,63,GEngine->GetSmallFont(),1.f);
    const TCHAR* Labels[]={TEXT("SOURCE / original ALS graph"),R->Proportions?TEXT("BEFORE / previous proportions"):TEXT("RETARGET / before grip fitting"),R->Proportions?TEXT("AFTER / fitted proportions + joined cuffs"):TEXT("CONTACT / reduced carry motion")};
    if(R->Comparison)for(int I=0;I<3;++I)DrawText(Labels[I],FLinearColor::White,Canvas->ClipX*(.10f+.33f*I),Canvas->ClipY-45,GEngine->GetSmallFont(),1.f);
    else DrawText(Labels[2],FLinearColor::White,22,Canvas->ClipY-45,GEngine->GetSmallFont(),1.f);
}
