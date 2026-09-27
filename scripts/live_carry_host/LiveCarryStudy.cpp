#include "LiveCarryStudy.h"
#include "AlsCharacter.h"
#include "AlsCharacterMovementComponent.h"
#include "AlsAnimationInstance.h"
#include "Utility/AlsGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/SkeletalMesh.h"
#include "EnhancedInputSubsystems.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "UnrealClient.h"
#include "AssetCompilingManager.h"
#include "Serialization/MemoryReader.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"

namespace {
FVector Spawn(-1500,-1000,92.15);
FString TransformRow(const FTransform& T){auto P=T.GetTranslation();auto Q=T.GetRotation();auto S=T.GetScale3D();return FString::Printf(TEXT("%.8f,%.8f,%.8f,%.9f,%.9f,%.9f,%.9f,%.8f,%.8f,%.8f\n"),P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z);}
bool WantsForward(double T){return (T>=1&&T<3.2)||(T>=4&&T<4.08)||(T>=4.20&&T<4.36)||(T>=4.48&&T<4.7)||(T>=5.2&&T<7.1)||(T>=7.18&&T<7.5)||(T>=8.2&&T<9.5);}
}
void ULiveCarryMesh::ApplyFrame(const TArray<FTransform>& Local,const TArray<FName>& Names)
{
    const auto* Asset=Cast<USkeletalMesh>(GetSkinnedAsset());if(!Asset)return;const auto& Ref=Asset->GetRefSkeleton();
    for(int I=0;I<BoneSpaceTransforms.Num();++I){const int J=Names.Find(Ref.GetBoneName(I));if(J>=0)BoneSpaceTransforms[I]=Local[J];}
    MarkRefreshTransformDirty();RefreshBoneTransforms();
}
ALiveCarryPawn::ALiveCarryPawn()
{
    bUseControllerRotationYaw=false;GetCharacterMovement()->bOrientRotationToMovement=false;GetCharacterMovement()->bUseControllerDesiredRotation=false;
    StudyBody=CreateDefaultSubobject<ULiveCarryMesh>(TEXT("ReviewedRyanBody"));StudyBody->SetupAttachment(RootComponent);
    StudyGun=CreateDefaultSubobject<ULiveCarryMesh>(TEXT("ReviewedM4"));StudyGun->SetupAttachment(RootComponent);
    for(auto* Mesh:{StudyBody.Get(),StudyGun.Get()}){Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetRelativeRotation(FRotator(0,-90,0));Mesh->BoundsScale=3;}
}
void ALiveCarryPawn::BeginPlay()
{
    ACharacter::BeginPlay();
    for(auto* C:{GetMesh(),Arms.Get(),ViewGun.Get(),BodyGun.Get(),ViewKnife.Get(),BodyKnife.Get()}){C->SetVisibility(false,true);C->SetComponentTickEnabled(false);}
    StudyBody->SetSkinnedAssetAndUpdate(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/BodyProportionStudy/SK_RyanProportion.SK_RyanProportion")));
    StudyGun->SetSkinnedAssetAndUpdate(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_M4Action.SK_M4Action")));
    StudyBody->SetVisibility(false);StudyGun->SetVisibility(false);
    auto* M=GetCharacterMovement();M->GroundFriction=8;M->BrakingDecelerationWalking=2048;M->BrakingFrictionFactor=2;M->MaxAcceleration=2048;
    StudyCamera=GetWorld()->SpawnActor<ACameraActor>();StudyCamera->GetCameraComponent()->SetFieldOfView(48);StudyCamera->GetCameraComponent()->PostProcessSettings.bOverride_MotionBlurAmount=true;StudyCamera->GetCameraComponent()->PostProcessSettings.MotionBlurAmount=0;
}
void ALiveCarryPawn::Tick(float Dt){ACharacter::Tick(Dt);}
void ALiveCarryPawn::SetupPlayerInputComponent(UInputComponent* Input)
{
    Input->BindKey(EKeys::W,IE_Pressed,this,&ALiveCarryPawn::ForwardDown);Input->BindKey(EKeys::W,IE_Released,this,&ALiveCarryPawn::ForwardUp);
    Input->BindAxisKey(EKeys::MouseX,this,&ALiveCarryPawn::OrbitX);Input->BindAxisKey(EKeys::MouseY,this,&ALiveCarryPawn::OrbitY);
    Input->BindKey(EKeys::One,IE_Pressed,this,&ALiveCarryPawn::Front);Input->BindKey(EKeys::Two,IE_Pressed,this,&ALiveCarryPawn::Right);Input->BindKey(EKeys::Three,IE_Pressed,this,&ALiveCarryPawn::Back);Input->BindKey(EKeys::Four,IE_Pressed,this,&ALiveCarryPawn::Left);
    Input->BindKey(EKeys::R,IE_Pressed,this,&ALiveCarryPawn::ResetStudy);
}
void ALiveCarryPawn::ForwardDown(){if(!Ready||Audit)return;ForwardRequested=true;AddMovementInput(FVector::ForwardVector,1);}
void ALiveCarryPawn::ForwardUp(){if(!Audit)ForwardRequested=false;}
void ALiveCarryPawn::OrbitX(float Value){if(Ready&&!Audit)OrbitYaw=FMath::UnwindDegrees(OrbitYaw-Value*.20f);}
void ALiveCarryPawn::OrbitY(float Value){if(Ready&&!Audit)OrbitPitch=FMath::Clamp(OrbitPitch+Value*.15f,-15.f,45.f);}
void ALiveCarryPawn::ResetStudy(){if(Audit)return;GetCharacterMovement()->StopMovementImmediately();SetActorLocation(Spawn,false,nullptr,ETeleportType::TeleportPhysics);ForwardRequested=false;}
void ALiveCarryPawn::UpdateCamera(){const FVector Focus=GetActorLocation()+FVector(0,0,12);const FVector Offset=FRotator(OrbitPitch,OrbitYaw,0).Vector()*OrbitDistance;StudyCamera->SetActorLocationAndRotation(Focus+Offset,(Focus-(Focus+Offset)).Rotation());}
void ALiveCarryHUD::DrawHUD()
{
    Super::DrawHUD();auto* P=Cast<ALiveCarryPawn>(GetOwningPawn());if(!Canvas||!P)return;
    DrawRect(FLinearColor(.015,.025,.04,.88),0,0,Canvas->ClipX,90);
    DrawText(TEXT("FIRELINE / LIVE M4 START-STOP STUDY"),FLinearColor::White,22,12,nullptr,1.25f);
    DrawText(TEXT("W hold: walk | release: stop | mouse: orbit | 1/2/3/4: views | R: reset position"),FLinearColor(.65,.85,1),22,43);
    DrawText(FString::Printf(TEXT("%.0f cm/s | %s | flat ground, forward M4 only; other actions are not enabled"),P->GetVelocity().Size2D(),P->ForwardRequested?TEXT("INPUT"):TEXT("NO INPUT")),FLinearColor::White,22,69);
}
void FLiveCarryStudy::Before(UWorld* W,float Dt)
{
    if(Finished||!W||!W->IsGameWorld())return;
    if(WarmStart<0)
    {
        if(W->GetTimeSeconds()<2)return;auto* PC=W->GetFirstPlayerController();auto* S=PC?Cast<AAlsCharacter>(PC->GetPawn()):nullptr;if(!S)return;
        Source=S;WarmStart=W->GetTimeSeconds();S->DisableInput(PC);S->SetOverlayMode(AlsOverlayModeTags::Rifle);S->SetDesiredGait(AlsGaitTags::Walking);S->SetDesiredRotationMode(AlsRotationModeTags::ViewDirection);S->SetActorRotation(FRotator::ZeroRotator);PC->SetControlRotation(FRotator::ZeroRotator);
        S->GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;S->GetMesh()->bEnableUpdateRateOptimizations=false;
        S->GetMesh()->SetComponentTickEnabled(false);
        S->SetActorHiddenInGame(true);
        for(TActorIterator<AAlsCharacter> It(W);It;++It)if(*It!=S){It->SetActorHiddenInGame(true);It->SetActorTickEnabled(false);It->GetCharacterMovement()->SetComponentTickEnabled(false);It->GetMesh()->SetComponentTickEnabled(false);}
        Audit=FParse::Param(FCommandLine::Get(),TEXT("LiveCarryAudit"));Parity=FParse::Param(FCommandLine::Get(),TEXT("LiveCarryParity"));FParse::Value(FCommandLine::Get(),TEXT("StudyFPS="),FPS);
        if(Audit||Parity){FApp::SetFixedDeltaTime(1./FPS);FApp::SetUseFixedTimeStep(true);}
        return;
    }
    if(!Pawn.IsValid())
    {
        if(W->GetTimeSeconds()-WarmStart<1.1)return;auto* S=Source.Get();auto* PC=W->GetFirstPlayerController();if(!S||!PC)return;
        if(!Retarget.Load(FPaths::ProjectSavedDir()/TEXT("LiveCarry/calibration.json"))){UE_LOG(LogTemp,Error,TEXT("LIVE_CARRY_INIT_FAILED calibration"));Finished=true;FPlatformMisc::RequestExit(false);return;}
        S->SetActorTickEnabled(false);S->GetCharacterMovement()->SetComponentTickEnabled(false);S->SetActorEnableCollision(false);S->SetActorHiddenInGame(true);S->GetMesh()->SetComponentTickEnabled(false);
        // Normalize the real controller's acceleration/deceleration too. The
        // pinned ALS graph divides acceleration by these movement properties.
        if(auto* A=FindFProperty<FFloatProperty>(S->GetCharacterMovement()->GetClass(),TEXT("MaxAccelerationWalking")))A->SetPropertyValue_InContainer(S->GetCharacterMovement(),2048/Retarget.LegScale);
        S->GetCharacterMovement()->BrakingDecelerationWalking=2048/Retarget.LegScale;
        SourceOrigin=FVector(-1500,-1500,92.15);S->SetActorLocation(SourceOrigin,false,nullptr,ETeleportType::TeleportPhysics);
        const auto& Ref=S->GetMesh()->GetSkeletalMeshAsset()->GetRefSkeleton();for(int I=0;I<Ref.GetNum();++I)SourceNames.Add(Ref.GetBoneName(I));
        Pawn=W->SpawnActor<ALiveCarryPawn>(Spawn,FRotator::ZeroRotator);if(!Pawn.IsValid()){Finished=true;return;}
        auto* P=Pawn.Get();P->Audit=Audit||Parity;P->GetCharacterMovement()->MaxWalkSpeed=175*Retarget.LegScale;PawnOrigin=P->GetActorLocation();
        PC->UnPossess();PC->Possess(P);if(auto* Sub=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))Sub->ClearAllMappings();
        PC->SetViewTarget(P->StudyCamera);PC->ClientSetHUD(ALiveCarryHUD::StaticClass());UWidgetLayoutLibrary::RemoveAllWidgets(PC);PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;PC->SetControlRotation(FRotator::ZeroRotator);
        FAssetCompilingManager::Get().FinishAllCompilation();P->Ready=true;Start=W->GetTimeSeconds();Folder=FPaths::ProjectSavedDir()/FString::Printf(TEXT("LiveCarry/%s-%d"),Parity?TEXT("parity"):TEXT("audit"),FPS);IFileManager::Get().MakeDirectory(*Folder,true);
        PoseRows=TEXT("frame,time,bone,x,y,z,qx,qy,qz,qw,sx,sy,sz\n");StateRows=TEXT("frame,time,dt,input,speed,actor_x,actor_y,actor_z,body_x,body_y,body_z,source_speed,failures,reach_margin,left_lock_curve,right_lock_curve,sole_correction,left_lock,right_lock\n");
        UE_LOG(LogTemp,Display,TEXT("LIVE_CARRY_READY native=%s single_physical_mover=Fireline max_speed=%.4f"),*P->StudyBody->GetSkinnedAsset()->GetPathName(),P->GetCharacterMovement()->MaxWalkSpeed);
    }
    if(Audit){auto* P=Pawn.Get();P->ForwardRequested=WantsForward(W->GetTimeSeconds()-Start);if(P->ForwardRequested)P->AddMovementInput(FVector::ForwardVector,1);}
    else if(!Parity){auto* P=Pawn.Get();P->ForwardRequested=W->GetFirstPlayerController()->IsInputKeyDown(EKeys::W);if(P->ForwardRequested)P->AddMovementInput(FVector::ForwardVector,1);}
}
void FLiveCarryStudy::After(UWorld* W,float Dt)
{
    if(Finished||!Source.IsValid()||Source->GetWorld()!=W)return;
    if(!Pawn.IsValid())
    {
        auto* Mesh=Source->GetMesh();Mesh->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;Mesh->TickAnimation(Dt,false);Mesh->RefreshBoneTransforms();return;
    }
    auto* P=Pawn.Get();auto* S=Source.Get();const double T=W->GetTimeSeconds()-Start;
    // The original ALS character is a pose evaluator, never a second mover.
    // It receives the real Fireline capsule displacement and velocity in source
    // leg units. Original ALS graph owns blends, phase, foot locks and stops.
    const FVector SourceVelocity=P->GetVelocity()/Retarget.LegScale;
    const FVector SourceLocation=SourceOrigin+(P->GetActorLocation()-PawnOrigin)/Retarget.LegScale;
    S->SetActorLocation(SourceLocation,false,nullptr,FVector::Dist(SourceLocation,S->GetActorLocation())>50?ETeleportType::TeleportPhysics:ETeleportType::None);
    S->GetCharacterMovement()->Velocity=SourceVelocity;
    auto& State=const_cast<FAlsLocomotionState&>(S->GetLocomotionState());State.Velocity=SourceVelocity;State.Speed=SourceVelocity.Size2D();State.bHasVelocity=State.Speed>=1;State.bHasInput=P->GetCharacterMovement()->GetCurrentAcceleration().SizeSquared2D()>1;State.InputYawAngle=State.VelocityYawAngle=State.TargetYawAngle=State.SmoothTargetYawAngle=0;State.bMoving=(State.bHasInput&&State.bHasVelocity)||State.Speed>50;
    auto* SM=S->GetMesh();SM->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;SM->TickAnimation(Dt,false);SM->RefreshBoneTransforms();
    TArray<FTransform> Input=SM->GetComponentSpaceTransforms();
    if(Parity)
    {
        static TArray<uint8> Bytes;static TArray<FName> Order;static TArray<int32> Parent;static int Count=0,Frames=0;
        if(!Bytes.Num())
        {
            const FString D=FPaths::ProjectSavedDir()/TEXT("LiveCarry");TArray<FString> Lines;FFileHelper::LoadFileToStringArray(Lines,*(D/TEXT("source-bones.txt")));for(const auto& N:Lines)Order.Add(FName(N));FFileHelper::LoadFileToArray(Bytes,*(D/TEXT("source.bin")));FMemoryReader Reader(Bytes);uint32 Magic,C,F;Reader<<Magic<<C<<F;Count=C;Frames=F;
            for(FName N:Order)Parent.Add(Order.Find(SM->GetParentBone(N)));
        }
        if(Frame>=Frames){Finished=true;FFileHelper::SaveStringToFile(PoseRows,*(Folder/TEXT("poses.csv")));UE_LOG(LogTemp,Display,TEXT("LIVE_CARRY_PARITY_COMPLETE frames=%d failures=%d"),Frame,Failures);FPlatformMisc::RequestExit(false);return;}
        TArray<FTransform> CS;CS.SetNum(Count);FMemoryReader Reader(Bytes);Reader.Seek(12+int64(Frame)*Count*40);
        for(int I=0;I<Count;++I){float V[10];Reader.Serialize(V,sizeof(V));CS[I]=FTransform(FQuat(V[3],V[4],V[5],V[6]).GetNormalized(),FVector(V[0],V[1],V[2]),FVector(V[7],V[8],V[9]));if(Parent[I]>=0)CS[I]*=CS[Parent[I]];}
        Input=CS;SourceNames=Order;
    }
    TArray<FTransform> Local,CS;
    bool Valid=Retarget.Evaluate(Input,SourceNames,Local,CS,!Parity);
    if(Valid&&!Parity)Valid=Retarget.PlaceSoles(SM->GetAnimInstance()->GetCurveValue(TEXT("FootLeftLock")),SM->GetAnimInstance()->GetCurveValue(TEXT("FootRightLock")),Local,CS);
    if(Valid){LastLocal=MoveTemp(Local);LastComponent=MoveTemp(CS);}else{++Failures;UE_LOG(LogTemp,Error,TEXT("LIVE_CARRY_POSE_INVALID frame=%d time=%.4f"),Frame,T);}
    if(LastLocal.Num())
    {
        P->StudyBody->ApplyFrame(LastLocal,Retarget.Names);P->StudyGun->ApplyFrame(LastLocal,Retarget.Names);
        const float Floor=P->GetCharacterMovement()->CurrentFloor.bBlockingHit?P->GetCharacterMovement()->CurrentFloor.FloorDist:0;
        const FVector Offset(0,0,-P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Floor+.70);
        P->StudyBody->SetRelativeLocation(Offset);P->StudyGun->SetRelativeLocation(Offset);P->StudyBody->SetVisibility(true);P->StudyGun->SetVisibility(true);
    }
    if(Audit||Parity)Record(Dt,T);
    if(Audit)
    {
        static const double Times[]={.6,1.1,1.6,2.0,3.25,3.8,4.33,4.65,5.3,6.0,7.25,7.8,8.8,10.3};
        if(Shot<UE_ARRAY_COUNT(Times)&&T>=Times[Shot]){P->OrbitYaw=(Shot%4)*90;P->UpdateCamera();FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("shot-%02d.png"),Shot++),true,false);}
        if(T>=11){Finished=true;FFileHelper::SaveStringToFile(PoseRows,*(Folder/TEXT("poses.csv")));FFileHelper::SaveStringToFile(StateRows,*(Folder/TEXT("states.csv")));UE_LOG(LogTemp,Display,TEXT("LIVE_CARRY_AUDIT_COMPLETE frames=%d failures=%d"),Frame,Failures);FPlatformMisc::RequestExit(false);}
    }
    P->UpdateCamera();if(auto* Manager=W->GetFirstPlayerController()->PlayerCameraManager.Get())Manager->UpdateCamera(Dt);++Frame;
}
void FLiveCarryStudy::Record(float Dt,double T)
{
    auto* P=Pawn.Get();const auto& Actual=P->StudyBody->GetComponentSpaceTransforms();const auto& Ref=CastChecked<USkeletalMesh>(P->StudyBody->GetSkinnedAsset())->GetRefSkeleton();
    for(int I=0;I<Actual.Num();++I)PoseRows+=FString::Printf(TEXT("%d,%.8f,%s,"),Frame,T,*Ref.GetBoneName(I).ToString())+TransformRow(Actual[I]);
    const FVector A=P->GetActorLocation(),B=P->StudyBody->GetComponentLocation();
    auto* Anim=Source->GetMesh()->GetAnimInstance();const auto* Property=FindFProperty<FStructProperty>(Anim->GetClass(),TEXT("FeetState"));const auto* Feet=Property?Property->ContainerPtrToValuePtr<FAlsFeetState>(Anim):nullptr;
    StateRows+=FString::Printf(TEXT("%d,%.8f,%.8f,%d,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f,%d,%.8f,%.8f,%.8f,%.8f,%.8f,%.8f\n"),Frame,T,Dt,P->ForwardRequested?1:0,P->GetVelocity().Size2D(),A.X,A.Y,A.Z,B.X,B.Y,B.Z,Source->GetVelocity().Size2D(),Failures,Retarget.MinReachMargin,Anim->GetCurveValue(TEXT("FootLeftLock")),Anim->GetCurveValue(TEXT("FootRightLock")),Retarget.MaxSoleCorrection,Feet?Feet->Left.LockAmount:-1,Feet?Feet->Right.LockAmount:-1);
}
