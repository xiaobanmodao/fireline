#include "LiveCarryStudy.h"
#include "AlsCharacter.h"
#include "AlsCharacterMovementComponent.h"
#include "AlsAnimationInstance.h"
#include "Utility/AlsGameplayTags.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/LocalPlayer.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindow.h"
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
#include "ShaderCompiler.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "EngineUtils.h"

namespace {
FVector Spawn(-1500,-1000,92.15);
FString TransformRow(const FTransform& T){auto P=T.GetTranslation();auto Q=T.GetRotation();auto S=T.GetScale3D();return FString::Printf(TEXT("%.8f,%.8f,%.8f,%.9f,%.9f,%.9f,%.9f,%.8f,%.8f,%.8f\n"),P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z);}
bool WantsForward(double T){return (T>=1&&T<3.2)||(T>=4&&T<4.08)||(T>=4.20&&T<4.36)||(T>=4.48&&T<4.7)||(T>=5.2&&T<7.1)||(T>=7.18&&T<7.5)||(T>=8.2&&T<9.5);}
void SetReferenceVisibility(AActor* Actor,bool Visible)
{
    Actor->SetActorHiddenInGame(!Visible);
    // Held props are independent attached actors in the source example. Actor
    // hidden state alone does not hide those; they otherwise float behind Ryan.
    TArray<AActor*> Attached;Actor->GetAttachedActors(Attached,true,true);
    for(auto* Prop:Attached)if(IsValid(Prop))Prop->SetActorHiddenInGame(!Visible);
}
}
void ULiveCarryMesh::ApplyFrame(const TArray<FTransform>& Local,const TArray<FName>& Names)
{
    const auto* Asset=Cast<USkeletalMesh>(GetSkinnedAsset());if(!Asset)return;const auto& Ref=Asset->GetRefSkeleton();
    for(int I=0;I<BoneSpaceTransforms.Num();++I){const int J=Names.Find(Ref.GetBoneName(I));if(J>=0)BoneSpaceTransforms[I]=Local[J];}
    LastFrameLocal=Local;LastFrameNames=Names;
    MarkRefreshTransformDirty();RefreshBoneTransforms();
}
ALiveCarryPawn::ALiveCarryPawn()
{
    bUseControllerRotationYaw=false;GetCharacterMovement()->bOrientRotationToMovement=false;GetCharacterMovement()->bUseControllerDesiredRotation=false;
    StudyBody=CreateDefaultSubobject<ULiveCarryMesh>(TEXT("ReviewedRyanBody"));StudyBody->SetupAttachment(RootComponent);
    StudyGun=CreateDefaultSubobject<ULiveCarryMesh>(TEXT("ReviewedM4"));StudyGun->SetupAttachment(RootComponent);
    StudyOptic=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ReviewedCoyote"));StudyOptic->SetupAttachment(StudyGun,TEXT("M4_body"));
    StudyOptic->SetCollisionEnabled(ECollisionEnabled::NoCollision);StudyOptic->SetVisibility(false);
    for(auto* Mesh:{StudyBody.Get(),StudyGun.Get()}){Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetRelativeRotation(FRotator(0,-90,0));Mesh->BoundsScale=3;}
}
void ALiveCarryPawn::BeginPlay()
{
    ACharacter::BeginPlay();
    for(auto* C:{GetMesh(),Arms.Get(),ViewGun.Get(),BodyGun.Get(),ViewKnife.Get(),BodyKnife.Get()}){C->SetVisibility(false,true);C->SetComponentTickEnabled(false);}
    StaticContactStudy=FParse::Param(FCommandLine::Get(),TEXT("LiveStaticContactStudy"));
    NativeReadyAimStudy=FParse::Param(FCommandLine::Get(),TEXT("LiveNativeReadyAimStudy"));
    StudyBody->SetSkinnedAssetAndUpdate(LoadObject<USkeletalMesh>(nullptr,(StaticContactStudy||NativeReadyAimStudy)?TEXT("/Game/ChestPanelStudy/SK_RyanChestPanels.SK_RyanChestPanels"):TEXT("/Game/BodyProportionStudy/SK_RyanProportion.SK_RyanProportion")));
    StudyGun->SetSkinnedAssetAndUpdate(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Hands/SK_M4Action.SK_M4Action")));
    StudyBody->SetVisibility(false);StudyGun->SetVisibility(false);
    HeadContourStudy=FParse::Param(FCommandLine::Get(),TEXT("LiveCarryHeadContourStudy"));
    if(HeadContourStudy&&!InitializeHeadContour()){UE_LOG(LogTemp,Error,TEXT("LIVE_CARRY_INIT_FAILED head contour asset/bind"));FPlatformMisc::RequestExit(false);return;}
    CoyoteStudy=FParse::Param(FCommandLine::Get(),TEXT("LiveCarryCoyoteStudy"));
    ContactGuides=FParse::Param(FCommandLine::Get(),TEXT("StudyOpticDiagnostics"));
    if(CoyoteStudy&&!InitializeStudyOptic()){UE_LOG(LogTemp,Error,TEXT("LIVE_CARRY_INIT_FAILED Coyote asset/socket registration"));FPlatformMisc::RequestExit(false);return;}
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
    Input->BindKey(EKeys::Five,IE_Pressed,this,&ALiveCarryPawn::ToggleSource);
    Input->BindKey(EKeys::F,IE_Pressed,this,&ALiveCarryPawn::ToggleAim);
    Input->BindKey(EKeys::Six,IE_Pressed,this,&ALiveCarryPawn::ToggleDemo);
    Input->BindKey(EKeys::H,IE_Pressed,this,&ALiveCarryPawn::ToggleHeadContour);
    Input->BindKey(EKeys::O,IE_Pressed,this,&ALiveCarryPawn::ToggleStudyOptic);
    Input->BindKey(EKeys::Seven,IE_Pressed,this,&ALiveCarryPawn::ToggleContactGuides);
    Input->BindKey(EKeys::Z,IE_Pressed,this,&ALiveCarryPawn::ToggleContactCloseup);
    Input->BindKey(EKeys::A,IE_Pressed,this,&ALiveCarryPawn::LeftDown);Input->BindKey(EKeys::D,IE_Pressed,this,&ALiveCarryPawn::RightDown);Input->BindKey(EKeys::S,IE_Pressed,this,&ALiveCarryPawn::BackDown);
}
void ALiveCarryPawn::ForwardDown(){if(!Ready||Audit||StaticContactStudy||NativeReadyAimStudy)return;if(Directional){RequestTap(FVector::ForwardVector);return;}ForwardRequested=true;AddMovementInput(FVector::ForwardVector,1);}
void ALiveCarryPawn::ForwardUp(){if(!Audit)ForwardRequested=false;}
void ALiveCarryPawn::OrbitX(float Value){if(Ready&&!Audit)OrbitYaw=FMath::UnwindDegrees(OrbitYaw-Value*.20f);}
void ALiveCarryPawn::OrbitY(float Value){if(Ready&&!Audit)OrbitPitch=FMath::Clamp(OrbitPitch+Value*.15f,-15.f,45.f);}
void ALiveCarryPawn::ResetStudy(){if(Audit)return;GetCharacterMovement()->StopMovementImmediately();SetActorLocation(Spawn,false,nullptr,ETeleportType::TeleportPhysics);ForwardRequested=false;if(AimStudy)AimPitch=0;}
void ALiveCarryPawn::UpdateCamera()
{
    const bool Closeup=(Audit&&FParse::Param(FCommandLine::Get(),TEXT("StudyCloseup")))||(!Audit&&CoyoteStudy&&CloseContactView);
    const FVector Focus=ShowSource?SourceFocus:GetActorLocation()+FVector(0,0,Closeup?58:12);
    const FVector Offset=FRotator(OrbitPitch,OrbitYaw,0).Vector()*(Closeup?260:OrbitDistance);
    StudyCamera->SetActorLocationAndRotation(Focus+Offset,(Focus-(Focus+Offset)).Rotation());
}
void ALiveCarryHUD::DrawHUD()
{
    Super::DrawHUD();auto* P=Cast<ALiveCarryPawn>(GetOwningPawn());if(!Canvas||!P)return;
    DrawRect(FLinearColor(.015,.025,.04,.88),0,0,Canvas->ClipX,90);
    if(P->NativeReadyAimStudy)
    {
        DrawText(TEXT("FIRELINE / NATIVE READY - AIM CONTACT CANDIDATE"),FLinearColor::White,22,12,nullptr,1.25f);
        DrawText(TEXT("RMB/F raise | 6 demo | mouse orbit | 1-4 views | Z close-up | O optic | 7 guides"),FLinearColor(.65,.85,1),22,43);
        DrawText(TEXT("Static raise/lower only | original helmet | eye line / movement / actions unfinished"),FLinearColor::White,22,69);return;
    }
    if(P->StaticContactStudy)
    {
        DrawText(TEXT("FIRELINE / AUTHOR RYAN STATIC CONTACT CANDIDATE"),FLinearColor::White,22,12,nullptr,1.25f);
        DrawText(TEXT("Mouse orbit | 1-4 views | Z close-up | O optic | 7 guides"),FLinearColor(.65,.85,1),22,43);
        DrawText(TEXT("Static aiming only | original helmet | eye line / moving hold unfinished"),FLinearColor::White,22,69);return;
    }
    DrawText(P->HeadContourStudy&&!P->ShowSource?TEXT("FIRELINE / HELMET CONTOUR COMPARISON"):P->CoyoteStudy&&!P->ShowSource?TEXT("FIRELINE / M4 COYOTE CONTACT REVIEW"):P->ShowSource?(P->PresentationStudy?TEXT("ALS POSE REFERENCE / ADAPTED AIM TRANSITIONS"):TEXT("REFERENCE / ORIGINAL ALS RIFLE GRAPH")):P->PresentationStudy?TEXT("FIRELINE / M4 MOVEMENT & AIM REFINEMENT"):P->AimStudy?TEXT("FIRELINE / M4 READY-AIM STUDY"):P->Directional?TEXT("FIRELINE / M4 DIRECTIONAL MOVEMENT STUDY"):TEXT("FIRELINE / LIVE M4 START-STOP STUDY"),FLinearColor::White,22,12,nullptr,1.25f);
    DrawText(P->CoyoteStudy?TEXT("RMB/F aim | WASD/Shift | arrows pitch | 1-4 views | 5 source | 6 demo | O optic | 7 guides | Z close-up"):P->AimStudy?TEXT("RMB/F aim | arrows pitch | WASD/Shift move | Q/E facing | 1-4 views | 5 source | 6 demo | R reset"):P->Directional?TEXT("WASD move | Shift jog | Q/E turn facing | mouse orbit | 1-4 views | R reset"):TEXT("W hold: walk | release: stop | mouse: orbit | 1/2/3/4: views | R: reset position"),FLinearColor(.65,.85,1),22,43);
    if(P->HeadContourStudy)
    {
        if(P->ShowSource)DrawText(TEXT("Original ALS character / Rifle graph reference"),FLinearColor::White,22,69);
        else DrawText(FString::Printf(TEXT("H: %s | original helmet proportions | stock / shoulder contact unfinished"),P->ShowHeadContour?TEXT("REJECTED FLATTENED HELMET"):TEXT("RESTORED ORIGINAL HELMET")),FLinearColor::White,22,69);
        return;
    }
    if(P->CoyoteStudy){DrawText(FString::Printf(TEXT("%s | %s | eye alignment unfinished"),*P->RifleState,P->ShowCoyote?TEXT("COYOTE"):TEXT("IRON")),FLinearColor::White,22,69);return;}
    DrawText(FString::Printf(TEXT("%.0f cm/s | %s | %s"),P->GetVelocity().Size2D(),P->JogRequested?TEXT("JOG"):TEXT("WALK"),P->AimStudy?*FString::Printf(TEXT("%s | pitch %.0f | third-person pose study"),*P->RifleState,P->AimPitch):P->Directional?TEXT("M4 flat ground; aim/fire/actions not enabled"):TEXT("forward M4 only; other actions are not enabled")),FLinearColor::White,22,69);
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
        SetReferenceVisibility(S,false);
        for(TActorIterator<AAlsCharacter> It(W);It;++It)if(*It!=S){SetReferenceVisibility(*It,false);It->SetActorTickEnabled(false);It->GetCharacterMovement()->SetComponentTickEnabled(false);It->GetMesh()->SetComponentTickEnabled(false);}
        Audit=FParse::Param(FCommandLine::Get(),TEXT("LiveCarryAudit"));Parity=FParse::Param(FCommandLine::Get(),TEXT("LiveCarryParity"));FParse::Value(FCommandLine::Get(),TEXT("StudyFPS="),FPS);
        AimStudy=FParse::Param(FCommandLine::Get(),TEXT("LiveCarryAimStudy"));Directional=AimStudy||FParse::Param(FCommandLine::Get(),TEXT("LiveCarryDirectional"));
        if(Audit||Parity){FApp::SetFixedDeltaTime(1./FPS);FApp::SetUseFixedTimeStep(true);}
        return;
    }
    if(!Pawn.IsValid())
    {
        if(W->GetTimeSeconds()-WarmStart<1.1)return;auto* S=Source.Get();auto* PC=W->GetFirstPlayerController();if(!S||!PC)return;
        if(!Retarget.Load(FPaths::ProjectSavedDir()/TEXT("LiveCarry/calibration.json"))){UE_LOG(LogTemp,Error,TEXT("LIVE_CARRY_INIT_FAILED calibration"));Finished=true;FPlatformMisc::RequestExit(false);return;}
        if(AimStudy&&!FParse::Param(FCommandLine::Get(),TEXT("LiveAimCalibrate"))&&!Retarget.LoadAim(FPaths::ProjectSavedDir()/TEXT("LiveCarry/aim-registration.json"))){UE_LOG(LogTemp,Error,TEXT("LIVE_CARRY_INIT_FAILED aim registration"));Finished=true;FPlatformMisc::RequestExit(false);return;}
        Retarget.RefinePresentation=AimStudy&&FParse::Param(FCommandLine::Get(),TEXT("LiveCarryPresentationStudy"));
        if(Retarget.RefinePresentation)ConfigurePresentationTransitions();
        S->SetActorTickEnabled(false);S->GetCharacterMovement()->SetComponentTickEnabled(false);S->SetActorEnableCollision(false);S->SetActorHiddenInGame(true);S->GetMesh()->SetComponentTickEnabled(false);
        // Normalize the real controller's acceleration/deceleration too. The
        // pinned ALS graph divides acceleration by these movement properties.
        if(auto* A=FindFProperty<FFloatProperty>(S->GetCharacterMovement()->GetClass(),TEXT("MaxAccelerationWalking")))A->SetPropertyValue_InContainer(S->GetCharacterMovement(),2048/Retarget.LegScale);
        S->GetCharacterMovement()->BrakingDecelerationWalking=2048/Retarget.LegScale;
        SourceOrigin=FVector(-1500,-1500,92.15);S->SetActorLocation(SourceOrigin,false,nullptr,ETeleportType::TeleportPhysics);
        const auto& Ref=S->GetMesh()->GetSkeletalMeshAsset()->GetRefSkeleton();for(int I=0;I<Ref.GetNum();++I)SourceNames.Add(Ref.GetBoneName(I));
        Pawn=W->SpawnActor<ALiveCarryPawn>(Spawn,FRotator::ZeroRotator);if(!Pawn.IsValid()){Finished=true;return;}
        auto* P=Pawn.Get();P->Audit=Audit||Parity;P->Directional=Directional;P->AimStudy=AimStudy;P->PresentationStudy=Retarget.RefinePresentation;P->ShowSource=AimStudy&&FParse::Param(FCommandLine::Get(),TEXT("LiveAimSourceView"));P->LegScale=Retarget.LegScale;P->GetCharacterMovement()->MaxWalkSpeed=175*Retarget.LegScale;PawnOrigin=P->GetActorLocation();
        if(P->StaticContactStudy)
        {
            FString Text;TSharedPtr<FJsonObject> Json;
            bool Loaded=FFileHelper::LoadFileToString(Text,*(FPaths::ProjectSavedDir()/TEXT("LiveCarry/static-contact.json")))&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Json)&&Json.IsValid();
            if(Loaded)for(const FName& Name:Retarget.Names)
            {
                const TSharedPtr<FJsonObject>* T=nullptr;
                if(!Json->TryGetObjectField(Name.ToString(),T)){Loaded=false;break;}
                const auto& V=(*T)->GetArrayField(TEXT("p"));const auto& Q=(*T)->GetArrayField(TEXT("q"));const auto& Scale=(*T)->GetArrayField(TEXT("s"));
                if(V.Num()!=3||Q.Num()!=4||Scale.Num()!=3){Loaded=false;break;}
                FTransform Pose(FQuat(Q[0]->AsNumber(),Q[1]->AsNumber(),Q[2]->AsNumber(),Q[3]->AsNumber()).GetNormalized(),FVector(V[0]->AsNumber(),V[1]->AsNumber(),V[2]->AsNumber()),FVector(Scale[0]->AsNumber(),Scale[1]->AsNumber(),Scale[2]->AsNumber()));
                if(Pose.ContainsNaN()){Loaded=false;break;}StaticContact.Add(Pose);
            }
            if(!Loaded||StaticContact.Num()!=131){UE_LOG(LogTemp,Error,TEXT("LIVE_CARRY_INIT_FAILED static contact"));Finished=true;FPlatformMisc::RequestExit(false);return;}
            UE_LOG(LogTemp,Display,TEXT("LIVE_STATIC_CONTACT_READY bones=131 original_helmet=1 moving_enabled=0"));
        }
        if(P->NativeReadyAimStudy&&!LoadNativeReadyAim()){UE_LOG(LogTemp,Error,TEXT("LIVE_CARRY_INIT_FAILED native ready aim"));Finished=true;FPlatformMisc::RequestExit(false);return;}
        PC->UnPossess();PC->Possess(P);if(auto* Sub=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))Sub->ClearAllMappings();
        PC->SetViewTarget(P->StudyCamera);PC->ClientSetHUD(ALiveCarryHUD::StaticClass());UWidgetLayoutLibrary::RemoveAllWidgets(PC);
        if(Audit||Parity){PC->SetInputMode(FInputModeUIOnly());PC->bShowMouseCursor=true;}
        else{PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;}
        PC->SetControlRotation(FRotator::ZeroRotator);
        if(Audit&&FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")))
        {
            const auto Window=GEngine->GameViewport->GetWindow();
            const bool NativeWindow=Window.IsValid()&&Window->GetNativeWindow().IsValid()&&Window->GetNativeWindow()->GetOSWindowHandle()!=nullptr;
            checkf(!NativeWindow,TEXT("Background audit unexpectedly created an OS window"));
            UE_LOG(LogTemp,Display,TEXT("LIVE_CARRY_BACKGROUND native_window=%d scripted_input=1"),NativeWindow?1:0);
        }
        FAssetCompilingManager::Get().FinishAllCompilation();if(P->CoyoteStudy&&GShaderCompilingManager)GShaderCompilingManager->FinishAllCompilation();P->Ready=true;Start=W->GetTimeSeconds();Folder=FPaths::ProjectSavedDir()/FString::Printf(TEXT("LiveCarry/%s-%d"),Parity?TEXT("parity"):TEXT("audit"),FPS);IFileManager::Get().MakeDirectory(*Folder,true);
        PoseRows=TEXT("frame,time,bone,x,y,z,qx,qy,qz,qw,sx,sy,sz\n");StateRows=TEXT("frame,time,dt,input,speed,actor_x,actor_y,actor_z,body_x,body_y,body_z,source_speed,failures,reach_margin,left_lock_curve,right_lock_curve,sole_correction,left_lock,right_lock\n");
        if(Directional){Folder=FPaths::ProjectSavedDir()/FString::Printf(TEXT("LiveDirectional/audit-%d"),FPS);IFileManager::Get().MakeDirectory(*Folder,true);SourceRows=PoseRows;StateRows.RemoveAt(StateRows.Len()-1);StateRows+=TEXT(",case,request_x,request_y,jog,view_yaw,actor_yaw,source_yaw,velocity_x,velocity_y,source_gait,pose_gait,turn_yaw_speed,jog_weight,pelvis_reach_offset,trajectory_reach_offset\n");}
        FString RunLabel;FParse::Value(FCommandLine::Get(),TEXT("StudyRun="),RunLabel);
        if(AimStudy){Folder=FPaths::ProjectSavedDir()/FString::Printf(TEXT("LiveAim/%s-%d"),RunLabel.IsEmpty()?TEXT("audit"):*FPaths::MakeValidFileName(RunLabel),FPS);RawRows=PoseRows;StateRows.RemoveAt(StateRows.Len()-1);StateRows+=TEXT(",aim_request,aim_pitch,relaxed_weight,ready_weight,aiming_weight,moving_weight\n");}
        else if(!RunLabel.IsEmpty())Folder=FPaths::ProjectSavedDir()/TEXT("LiveRegression")/FString::Printf(TEXT("%s-%d"),*FPaths::MakeValidFileName(RunLabel),FPS);
        if(Audit&&IFileManager::Get().FileExists(*(Folder/TEXT("poses.csv")))){UE_LOG(LogTemp,Error,TEXT("LIVE_CARRY_INIT_FAILED evidence exists; use a fresh StudyRun label"));Finished=true;FPlatformMisc::RequestExit(false);return;}
        IFileManager::Get().MakeDirectory(*Folder,true);
        UE_LOG(LogTemp,Display,TEXT("LIVE_CARRY_READY native=%s single_physical_mover=Fireline max_speed=%.4f"),*P->StudyBody->GetSkinnedAsset()->GetPathName(),P->GetCharacterMovement()->MaxWalkSpeed);
    }
    if(AimStudy){AimInput(W,Dt,W->GetTimeSeconds()-Start);}
    else if(Directional){DirectionalInput(W,Dt,W->GetTimeSeconds()-Start);}
    else if(Audit){auto* P=Pawn.Get();P->ForwardRequested=WantsForward(W->GetTimeSeconds()-Start);if(P->ForwardRequested)P->AddMovementInput(FVector::ForwardVector,1);}
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
    if(Directional)DirectionalSource(S,P,Dt);
    else {auto& State=const_cast<FAlsLocomotionState&>(S->GetLocomotionState());State.Velocity=SourceVelocity;State.Speed=SourceVelocity.Size2D();State.bHasVelocity=State.Speed>=1;State.bHasInput=P->GetCharacterMovement()->GetCurrentAcceleration().SizeSquared2D()>1;State.InputYawAngle=State.VelocityYawAngle=State.TargetYawAngle=State.SmoothTargetYawAngle=0;State.bMoving=(State.bHasInput&&State.bHasVelocity)||State.Speed>50;}
    auto* SM=S->GetMesh();SM->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;SM->TickAnimation(Dt,false);SM->RefreshBoneTransforms();
    TArray<FTransform> Input=SM->GetComponentSpaceTransforms();
    if(Directional&&Audit)for(int I=0;I<Input.Num();++I)SourceRows+=FString::Printf(TEXT("%d,%.8f,%s,"),Frame,T,*SourceNames[I].ToString())+TransformRow(Input[I]);
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
    if(AimStudy)ReadRifleStates();
    TArray<FTransform> Local,CS;
    Retarget.JogBlend=Directional&&!FParse::Param(FCommandLine::Get(),TEXT("LiveDirectionalCalibrate"))?DirectionalJogWeight():0.;Retarget.CompensateReach=Directional;
    if(AimStudy&&!FParse::Param(FCommandLine::Get(),TEXT("LiveAimCalibrate"))){Retarget.ArmedWeight=1-RelaxedWeight;Retarget.AimWeight=AimingWeight;Retarget.MovingWeight=FMath::Clamp(double(SM->GetAnimInstance()->GetCurveValue(TEXT("PoseMoving"))),0.,1.);Retarget.ViewDirection=FRotator(P->AimPitch,P->FacingYaw-P->GetActorRotation().Yaw+90,0).Vector();}
    if(Retarget.RefinePresentation)
    {
        const auto* FeetProperty=FindFProperty<FStructProperty>(SM->GetAnimInstance()->GetClass(),TEXT("FeetState"));
        check(FeetProperty);const auto* Feet=FeetProperty->ContainerPtrToValuePtr<FAlsFeetState>(SM->GetAnimInstance());
        Retarget.FootLock[0]=Feet->Left.LockAmount;Retarget.FootLock[1]=Feet->Right.LockAmount;
        const FTransform BodyWorld(FRotator(0,P->GetActorRotation().Yaw-90,0),P->GetActorLocation());
        Retarget.ResetContacts=Frame==0||FVector::Dist(BodyWorld.GetTranslation(),Retarget.BodyWorld.GetTranslation())>50;
        Retarget.BodyWorld=BodyWorld;
    }
    if(P->HeadContourStudy&&FParse::Param(FCommandLine::Get(),TEXT("StudyHeadSwapAudit")))
    {
        const bool Next=FMath::FloorToInt(T/.5)%2==0;
        if(Next!=P->ShowHeadContour){P->ShowHeadContour=Next;P->SelectHeadContour();}
    }
    bool Valid=Retarget.Evaluate(Input,SourceNames,Local,CS,!Parity);
    if(AimStudy&&Audit)for(int I=0;I<Retarget.RawMapped.Num();++I)RawRows+=FString::Printf(TEXT("%d,%.8f,%s,"),Frame,T,*Retarget.Names[I].ToString())+TransformRow(Retarget.RawMapped[I]);
    if(Valid&&!Parity)Valid=Retarget.PlaceSoles(SM->GetAnimInstance()->GetCurveValue(TEXT("FootLeftLock")),SM->GetAnimInstance()->GetCurveValue(TEXT("FootRightLock")),Local,CS);
    if(P->StaticContactStudy)
    {
        CS=StaticContact;
        const auto& Ref=CastChecked<USkeletalMesh>(P->StudyBody->GetSkinnedAsset())->GetRefSkeleton();
        for(int I=0;I<Retarget.Names.Num();++I)
        {
            const int B=Ref.FindBoneIndex(Retarget.Names[I]),Parent=B>=0?Ref.GetParentIndex(B):INDEX_NONE;
            Local[I]=Parent>=0?CS[I].GetRelativeTransform(CS[Retarget.Names.Find(Ref.GetBoneName(Parent))]):CS[I];
        }
        Valid=true;
    }
    if(P->NativeReadyAimStudy)Valid=EvaluateNativeReadyAim(AimingWeight,Local,CS);
    if(Valid){LastLocal=MoveTemp(Local);LastComponent=MoveTemp(CS);}else{++Failures;UE_LOG(LogTemp,Error,TEXT("LIVE_CARRY_POSE_INVALID frame=%d time=%.4f"),Frame,T);}
    if(LastLocal.Num())
    {
        P->StudyBody->ApplyFrame(LastLocal,Retarget.Names);P->StudyGun->ApplyFrame(LastLocal,Retarget.Names);
        const float Floor=P->GetCharacterMovement()->CurrentFloor.bBlockingHit?P->GetCharacterMovement()->CurrentFloor.FloorDist:0;
        const FVector Offset(0,0,-P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()-Floor+.70);
        P->StudyBody->SetRelativeLocation(Offset);P->StudyGun->SetRelativeLocation(Offset);P->StudyBody->SetVisibility(true);P->StudyGun->SetVisibility(true);
    }
    if(AimStudy){SetReferenceVisibility(S,P->ShowSource);P->StudyBody->SetVisibility(!P->ShowSource);P->StudyGun->SetVisibility(!P->ShowSource);P->SourceFocus=S->GetActorLocation()+FVector(0,0,6);}
    if(P->CoyoteStudy)P->UpdateStudyOptic(T);
    if(Audit||Parity)Record(Dt,T);
    if(Audit)
    {
        static const double Times[]={.6,1.1,1.6,2.0,3.25,3.8,4.33,4.65,5.3,6.0,7.25,7.8,8.8,10.3};
        const bool ReviewViews=FParse::Param(FCommandLine::Get(),TEXT("StudyReviewViews"));
        static const double ReadyAimTimes[]={1.,1.2,1.4,1.6,3.,3.2,3.4,3.6,4.12};
        const bool Motion=P->NativeReadyAimStudy&&FParse::Param(FCommandLine::Get(),TEXT("StudyMotionFrames"));
        const bool Capture=!Motion&&(P->NativeReadyAimStudy?(Shot<UE_ARRAY_COUNT(ReadyAimTimes)&&T>=ReadyAimTimes[Shot]):ReviewViews?(Shot<4&&T>=2.+Shot*.2):AimStudy?(Shot<(Retarget.RefinePresentation?38:28)&&T>=1.+(Shot/2)*4.+(Shot%2?3.4:1.8)):Directional?(Shot<24&&T>=(Shot<19?1.+Shot*2.4+1.35:Shot<23?46.6+(Shot-19)*4.8+3.8:67.15)):(Shot<UE_ARRAY_COUNT(Times)&&T>=Times[Shot]));
        if(Capture){P->OrbitYaw=(Shot%4)*90+P->GetActorRotation().Yaw;P->UpdateCamera();FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("shot-%02d.png"),Shot++),true,false);}
        if(Motion&&Frame%2==0){P->OrbitYaw=45;P->UpdateCamera();FScreenshotRequest::RequestScreenshot(Folder/FString::Printf(TEXT("motion-%04d.png"),Frame/2),true,false);}
        double Duration=Retarget.RefinePresentation?77.:AimStudy?57.:Directional?69.2:11.;FParse::Value(FCommandLine::Get(),TEXT("StudyDuration="),Duration);
        if(T>=Duration){Finished=true;FFileHelper::SaveStringToFile(PoseRows,*(Folder/TEXT("poses.csv")));FFileHelper::SaveStringToFile(StateRows,*(Folder/TEXT("states.csv")));if(Directional)FFileHelper::SaveStringToFile(SourceRows,*(Folder/TEXT("source-poses.csv")));if(AimStudy)FFileHelper::SaveStringToFile(RawRows,*(Folder/TEXT("raw-poses.csv")));if(P->CoyoteStudy)P->ExportStudyOptic(Folder);UE_LOG(LogTemp,Display,TEXT("LIVE_CARRY_AUDIT_COMPLETE frames=%d failures=%d"),Frame,Failures);FPlatformMisc::RequestExit(false);}
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
    if(Directional){const auto V=P->GetVelocity();StateRows.RemoveAt(StateRows.Len()-1);StateRows+=FString::Printf(TEXT(",%d,%.8f,%.8f,%d,%.8f,%.8f,%.8f,%.8f,%.8f,%d,%.8f,%.8f,%.8f,%.8f,%.8f\n"),P->Scenario,P->MoveRequested.X,P->MoveRequested.Y,P->JogRequested?1:0,P->FacingYaw,P->GetActorRotation().Yaw,Source->GetActorRotation().Yaw,V.X,V.Y,Source->GetGait()==AlsGaitTags::Walking?1:2,Anim->GetCurveValue(TEXT("PoseGait")),Anim->GetCurveValue(TEXT("RotationYawSpeed")),DirectionalJogWeight(),Retarget.PelvisReachOffset,Retarget.TrajectoryReachOffset);}
    if(AimStudy){StateRows.RemoveAt(StateRows.Len()-1);StateRows+=FString::Printf(TEXT(",%d,%.8f,%.8f,%.8f,%.8f,%.8f\n"),P->AimRequested?1:0,P->AimPitch,RelaxedWeight,ReadyWeight,AimingWeight,Retarget.MovingWeight);}
}
