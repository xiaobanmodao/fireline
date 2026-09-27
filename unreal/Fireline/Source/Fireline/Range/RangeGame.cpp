#include "RangeGame.h"
#include "RangeWeaponTiming.h"
#include "RangeCasingFX.h"
#include "RangeShotFX.h"
#include "RangeAudio.h"
#include "RangeViewMesh.h"
#include "RangeHitMesh.h"
#include "RangePlayerController.h"
#include "RangeAnimInstance.h"
#include "EngineUtils.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMeshSocket.h"
#include "Animation/Skeleton.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimSequence.h"
#include "Animation/BlendSpace.h"
#if WITH_EDITOR
#include "Animation/AnimData/IAnimationDataController.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimBoneCompressionSettings.h"
#include "Animation/AnimBoneCompressionCodec.h"
#endif
#include "Animation/AnimSingleNodeInstance.h"
#include "Engine/Canvas.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "Rendering/SkeletalMeshRenderData.h"
#include "Rendering/SkeletalMeshLODRenderData.h"

namespace
{
 const TCHAR* RyanPath=TEXT("/Game/Fireline/Hands/SK_RyanAction.SK_RyanAction");
 const TCHAR* GunPath=TEXT("/Game/Fireline/Hands/SK_M4Action.SK_M4Action");
 const TCHAR* IdlePath=TEXT("/Game/Fireline/Hands/A_M4_Idle.A_M4_Idle");
 void ApplyTeam(USkeletalMeshComponent* Mesh, bool Red)
 {
  auto* Armor=LoadObject<UMaterialInterface>(nullptr,Red?TEXT("/Game/Fireline/Materials/M_Enemy.M_Enemy"):TEXT("/Game/Fireline/Materials/M_Player.M_Player"));
  auto* Under=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Fireline/Materials/M_Under.M_Under"));
  auto* Visor=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Fireline/Materials/M_Visor.M_Visor"));
  for (int I=0;I<Mesh->GetNumMaterials();++I)
  {
   const FString Slot=Mesh->GetMaterialSlotNames()[I].ToString();
   if(Slot.Contains(TEXT("Drazen_Team")))
   {
    auto* Team=LoadObject<UMaterialInterface>(nullptr,Red?TEXT("/Game/Fireline/DrazenOriginal/M_Red.M_Red"):TEXT("/Game/Fireline/DrazenOriginal/M_Blue.M_Blue"));
    if(Team)Mesh->SetMaterial(I,Team);
   }
   else if (Slot.Contains(TEXT("ArmorBase")) && Armor) Mesh->SetMaterial(I,Armor);
   else if (Slot.Contains(TEXT("ArmorUnder")) && Under) Mesh->SetMaterial(I,Under);
   else if (Slot.Contains(TEXT("Visor")) && Visor) Mesh->SetMaterial(I,Visor);
  }
 }
}
ARangeCharacter::ARangeCharacter()
{
 PrimaryActorTick.bCanEverTick=true;
 CombatAudio=CreateDefaultSubobject<URangeAudio>(TEXT("CombatAudio"));
 GetCapsuleComponent()->InitCapsuleSize(32,90);
 bUseControllerRotationYaw=true;
 GetCharacterMovement()->MaxWalkSpeed=450;
 GetCharacterMovement()->JumpZVelocity=460;
 GetCharacterMovement()->GravityScale=1.5;GetCharacterMovement()->AirControl=.35f;JumpMaxHoldTime=.10f;
 GetCharacterMovement()->GetNavAgentPropertiesRef().bCanCrouch=true;
 GetCharacterMovement()->SetCrouchedHalfHeight(68);
 GetCharacterMovement()->MaxWalkSpeedCrouched=180;
 GetMesh()->SetRelativeLocation(FVector(0,0,-90));
 GetMesh()->SetRelativeRotation(FRotator(0,-90,0));
 GetMesh()->SetRelativeScale3D(FVector(.94));
 GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GetMesh()->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Ryan(RyanPath);
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> GunMesh(GunPath);
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> ArmMesh(TEXT("/Game/Fireline/Hands/SK_RyanActionArms.SK_RyanActionArms"));
 static ConstructorHelpers::FObjectFinder<UAnimSequence> IdleAsset(IdlePath);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> MagazineAsset(TEXT("/Game/Fireline/Action/SM_M4Magazine.SM_M4Magazine"));
 DroppedMagazineMesh=MagazineAsset.Object;
 GetMesh()->SetSkeletalMesh(Ryan.Object);Idle=IdleAsset.Object;
 Boom=CreateDefaultSubobject<USpringArmComponent>(TEXT("ViewBoom"));Boom->SetupAttachment(RootComponent);
 Boom->bUsePawnControlRotation=true;Boom->TargetArmLength=0;Boom->SetRelativeLocation(FVector(0,0,75));
 Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));Camera->SetupAttachment(Boom);Camera->SetFieldOfView(90);Camera->PostProcessSettings.bOverride_MotionBlurAmount=true;Camera->PostProcessSettings.MotionBlurAmount=0;
 Arms=CreateDefaultSubobject<URangeViewMesh>(TEXT("RyanFirstPersonArms"));Arms->SetupAttachment(Camera);
 Arms->SetSkeletalMesh(ArmMesh.Object);Arms->SetRelativeLocation(FVector(20,3,-164));Arms->SetRelativeRotation(FRotator(0,-90,0));Arms->SetRelativeScale3D(FVector(.94));
 Arms->SetCollisionEnabled(ECollisionEnabled::NoCollision);Arms->SetCastShadow(false);
 Arms->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 ViewGun=CreateDefaultSubobject<URangeViewMesh>(TEXT("M4FirstPerson"));ViewGun->SetupAttachment(Arms);ViewGun->SetSkeletalMesh(GunMesh.Object);ViewGun->SetCollisionEnabled(ECollisionEnabled::NoCollision);ViewGun->SetCastShadow(false);ViewGun->bUseAttachParentBound=false;
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> KnifeMesh(TEXT("/Game/Fireline/M9/SK_DU_M9.SK_DU_M9"));
 ViewKnife=CreateDefaultSubobject<URangeViewMesh>(TEXT("M9FirstPerson"));ViewKnife->SetupAttachment(Arms);ViewKnife->SetCollisionEnabled(ECollisionEnabled::NoCollision);ViewKnife->SetCastShadow(false);ViewKnife->bUseAttachParentBound=false;
 BodyKnife=CreateDefaultSubobject<URangeViewMesh>(TEXT("M9ThirdPerson"));BodyKnife->SetupAttachment(GetMesh());BodyKnife->SetCollisionEnabled(ECollisionEnabled::NoCollision);BodyKnife->bUseAttachParentBound=false;
 ViewKnife->SetSkeletalMesh(KnifeMesh.Object);BodyKnife->SetSkeletalMesh(KnifeMesh.Object);
 BodyGun=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("M4ThirdPerson"));BodyGun->SetupAttachment(GetMesh());BodyGun->SetSkeletalMesh(GunMesh.Object);BodyGun->SetCollisionEnabled(ECollisionEnabled::NoCollision);BodyGun->bUseAttachParentBound=true;
 ViewOptic=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoyoteFirstPerson"));ViewOptic->SetupAttachment(ViewGun,TEXT("M4_body"));
 BodyOptic=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoyoteThirdPerson"));BodyOptic->SetupAttachment(BodyGun,TEXT("M4_body"));
 for(auto* Optic:{ViewOptic.Get(),BodyOptic.Get()}){Optic->SetCollisionEnabled(ECollisionEnabled::NoCollision);Optic->SetCanEverAffectNavigation(false);Optic->bUseAttachParentBound=false;Optic->SetVisibility(false);}
 ViewOptic->SetCastShadow(false);ViewOptic->SetOnlyOwnerSee(true);BodyOptic->SetOwnerNoSee(true);
}
void ARangeCharacter::BeginPlay()
{
 Super::BeginPlay();
 ShotSpreadRandom.Initialize(FMath::Rand());
 InitializeKarambit();InitializeMP7();
 InitializeOptics();
 SpawnPoint=GetActorLocation();
 GetMesh()->SetAnimInstanceClass(URangeAnimInstance::StaticClass());
 Arms->SetAnimInstanceClass(URangeAnimInstance::StaticClass());ViewGun->SetLeaderPoseComponent(Arms);BodyGun->SetLeaderPoseComponent(GetMesh());
 ViewKnife->SetLeaderPoseComponent(Arms);BodyKnife->SetLeaderPoseComponent(GetMesh());BodyKnife->SetOwnerNoSee(true);UpdateHandVisibility();
 ApplyTeam(GetMesh(),false);ApplyTeam(Arms,false);
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineLightBodyStart"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineCharacterStart")))
 {
  FTimerHandle BodyView;GetWorldTimerManager().SetTimer(BodyView,[this]{if(!bThirdPerson)ToggleView();Controller->SetControlRotation(FRotator(-8,145,0));},.6f,false);
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineM9Start"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineKarambitStart"))){FTimerHandle KnifeStart;GetWorldTimerManager().SetTimer(KnifeStart,this,&ARangeCharacter::EquipKnife,.8f,false);}
 FActorSpawnParameters CasingSpawn;CasingSpawn.Owner=this;
 CasingFX=GetWorld()->SpawnActor<ARangeCasingFX>(FVector::ZeroVector,FRotator::ZeroRotator,CasingSpawn);
 StartDirectionalAudit();StartMagazineDropAudit();StartBodyPoseDiagnosis();StartM4AnimationAudit();StartMovementSpreadAudit();StartWeaponHandlingAudit();StartWeaponPolishAudit();StartReloadCasingAudit();StartMP7Audit();StartArmPoseAudit();StartKarambitAudit();StartM9Audit();StartHandsAudit();StartWeaponControlsAudit();StartWeaponDrawAudit();StartHoldAudit();StartWeaponAudit();StartMovementAudit();StartShootingAudit();StartMobilityAudit();StartViewFixAudit();StartVisibilityAudit();StartHitboxAudit();StartOpticAudit();
 GetMesh()->SetOwnerNoSee(true);BodyGun->SetOwnerNoSee(true);
 if (auto* PC=Cast<APlayerController>(Controller)) {PC->SetInputMode(FInputModeGameOnly());PC->bShowMouseCursor=false;}
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_SELECTED_PLAYABLE Ryan=%s M4=%s"),*GetMesh()->GetSkeletalMeshAsset()->GetPathName(),*ViewGun->GetSkeletalMeshAsset()->GetPathName());
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineThirdPersonOrbitAudit")))
 {
  struct FOrbitCheck {float ActorYaw=0;FVector CameraOffset=FVector::ZeroVector;FQuat Chest=FQuat::Identity;FString Results;int Failures=0;};
  auto Check=MakeShared<FOrbitCheck>();
  const FString Out=FPaths::ProjectSavedDir()/TEXT("ThirdPersonOrbitAudit");IFileManager::Get().MakeDirectory(*Out,true);
  auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
  auto Record=[Check](const TCHAR* Name,bool Pass){Check->Results+=FString::Printf(TEXT("%s: %s\n"),Name,Pass?TEXT("PASS"):TEXT("FAIL"));Check->Failures+=!Pass;};
  Later(1.f,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);SetActorRotation(FRotator::ZeroRotator);ToggleView();});
  Later(1.25f,[this,Check,Record,Out]{Check->ActorYaw=GetActorRotation().Yaw;Check->CameraOffset=(Camera->GetComponentLocation()-GetActorLocation()).GetSafeNormal();Check->Chest=GetMesh()->GetSocketQuaternion(TEXT("Chest"));Record(TEXT("third-person orbit enabled without movement rotation"),bThirdPersonOrbit&&!GetCharacterMovement()->bUseControllerDesiredRotation&&!GetCharacterMovement()->bOrientRotationToMovement);FScreenshotRequest::RequestScreenshot(Out/TEXT("01-behind.png"),true,false);});
  Later(1.5f,[this]{Controller->SetControlRotation(FRotator(0,180,0));});
  Later(1.8f,[this,Check,Record,Out]{const FVector Offset=(Camera->GetComponentLocation()-GetActorLocation()).GetSafeNormal();Record(TEXT("camera reaches character front while actor yaw stays fixed"),FMath::Abs(FMath::FindDeltaAngleDegrees(Check->ActorYaw,GetActorRotation().Yaw))<.1f&&Check->CameraOffset.Dot(Offset)<-.8f);Record(TEXT("body pose ignores orbit yaw"),FMath::RadiansToDegrees(Check->Chest.AngularDistance(GetMesh()->GetSocketQuaternion(TEXT("Chest"))))<1.f);Check->Chest=GetMesh()->GetSocketQuaternion(TEXT("Chest"));FScreenshotRequest::RequestScreenshot(Out/TEXT("02-front.png"),true,false);});
  Later(2.f,[this]{Controller->SetControlRotation(FRotator(35,180,0));});
  Later(2.25f,[this,Check,Record]{const float Delta=FMath::RadiansToDegrees(Check->Chest.AngularDistance(GetMesh()->GetSocketQuaternion(TEXT("Chest"))));Record(TEXT("body pose ignores orbit pitch"),Delta<1.f);Check->Results+=FString::Printf(TEXT("chest rotation after pitch-only orbit: %.3f deg\n"),Delta);});
  Later(2.5f,[this]{ToggleView();});
  Later(2.75f,[this,Check,Record,Out]{Record(TEXT("first-person return aligns actor to view"),!bThirdPerson&&!bThirdPersonOrbit&&FMath::Abs(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw,Controller->GetControlRotation().Yaw))<.1f);FFileHelper::SaveStringToFile(Check->Results,*(Out/TEXT("checks.txt")));UE_LOG(LogTemp,Display,TEXT("THIRD_PERSON_ORBIT_AUDIT failures=%d"),Check->Failures);FPlatformMisc::RequestExit(false);});
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineAudit")))
 {
  if(auto* AuditPC=Cast<APlayerController>(Controller))DisableInput(AuditPC);
  auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
  Later(4,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);});
  Later(6,[this]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/range-first.png"),true,false);
   UE_LOG(LogTemp,Display,TEXT("FIRELINE_FP gunWorld=%s gunBone=%s bounds=%s"),*ViewGun->GetComponentTransform().ToString(),*ViewGun->GetSocketTransform(TEXT("HandHold_R"),RTS_Component).ToString(),*ViewGun->Bounds.ToString());
   for(const FName Name:{FName(TEXT("Head")),FName(TEXT("HandHold_R")),FName(TEXT("UpperHand_L"))})UE_LOG(LogTemp,Display,TEXT("FIRELINE_BONE %s %s"),*Name.ToString(),*Arms->GetSocketTransform(Name,RTS_Component).ToString());
  });
  Later(7,[this]{Fire();});Later(7.5,[this]{Reload();});Later(8,[this]{Fire();});
  Later(10,[this]{
   FString Report=FString::Printf(TEXT("{\"ammo\":%d,\"shots\":%d,\"hits\":%d,\"reload_complete\":%s}"),Ammo,Shots,Hits,bReloading?TEXT("false"):TEXT("true"));
   FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("range-runtime-check.json")));
   ToggleView();Controller->SetControlRotation(FRotator(-8,155,0));
  });
  Later(12,[this]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/range-third.png"),true,false);});
  Later(13,[this]{Crouch();});
  Later(15,[this]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/range-crouch.png"),true,false);});
  Later(16,[this]{UnCrouch();Controller->SetControlRotation(FRotator(0,0,0));});
  Later(18,[this]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/range-back.png"),true,false);});
  Later(20,[this]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/range-run.png"),true,false);});
  Later(21,[this]{Jump();});
  Later(21.4,[this]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/range-jump.png"),true,false);});
  Later(23,[]{FPlatformMisc::RequestExit(false);});
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineReloadAudit")))
 {
  if(auto* AuditPC=Cast<APlayerController>(Controller))DisableInput(AuditPC);
  auto Later=[this](float Delay,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),Delay,false);};
  const TArray<float> Phases={0,.42f,.85f,1.12f,1.30f,1.90f,2.30f};
  for(int View=0;View<2;++View)
  {
   const float Start=6+View*10;
   Later(Start-.5f,[this,View]{if(bThirdPerson!=(View==1))ToggleView();Controller->SetControlRotation(View?FRotator(-8,155,0):FRotator::ZeroRotator);});
   for(int I=0;I<Phases.Num();++I)
   {
    Later(Start+I*1.1f,[this,T=Phases[I]]{bReloading=true;ReloadAuditSample=T;});
    Later(Start+I*1.1f+.5f,[this,View,I]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/reload-%d-%02d.png"),View,I),true,false);});
   }
  }
  Later(24,[this]{ReloadAuditSample=-1;bReloading=false;Ammo=29;Reload();});
  Later(27,[this]{UE_LOG(LogTemp,Display,TEXT("FIRELINE_RELOAD_AUDIT standing_ammo=%d complete=%d"),Ammo,!bReloading);Crouch();Ammo=29;Reload();});
  Later(28.3,[this]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/reload-crouch.png"),true,false);});
  Later(30,[this]{UE_LOG(LogTemp,Display,TEXT("FIRELINE_RELOAD_AUDIT crouch_ammo=%d complete=%d"),Ammo,!bReloading);UnCrouch();Ammo=29;Reload();});
  Later(31.3,[this]{FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("Screenshots/reload-moving.png"),true,false);});
  Later(33,[this]{UE_LOG(LogTemp,Display,TEXT("FIRELINE_RELOAD_AUDIT moving_ammo=%d complete=%d"),Ammo,!bReloading);if(bThirdPerson)ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);Ammo=29;Reload();});
  Later(36,[this]{Fire();UE_LOG(LogTemp,Display,TEXT("FIRELINE_RELOAD_AUDIT after_reload_shot_ammo=%d"),Ammo);FPlatformMisc::RequestExit(false);});
 }


}
void ARangeCharacter::SetupPlayerInputComponent(UInputComponent* Input)
{
 Super::SetupPlayerInputComponent(Input);
 if(bKarambitAudit||FParse::Param(FCommandLine::Get(),TEXT("FirelineHandsAudit")))return;
 Input->BindAxisKey(EKeys::MouseX,this,&ARangeCharacter::LookX);Input->BindAxisKey(EKeys::MouseY,this,&ARangeCharacter::LookY);
 Input->BindKey(EKeys::SpaceBar,IE_Pressed,this,&ARangeCharacter::StartJump);Input->BindKey(EKeys::SpaceBar,IE_Released,this,&ARangeCharacter::EndJump);
 Input->BindKey(EKeys::C,IE_Pressed,this,&ARangeCharacter::StartCrouch);Input->BindKey(EKeys::C,IE_Released,this,&ARangeCharacter::EndCrouch);
 Input->BindKey(EKeys::RightMouseButton,IE_Pressed,this,&ARangeCharacter::StartAim);Input->BindKey(EKeys::RightMouseButton,IE_Released,this,&ARangeCharacter::EndAim);
 Input->BindKey(EKeys::Zero,IE_Pressed,this,&ARangeCharacter::ToggleEmptyHands);
 Input->BindKey(EKeys::One,IE_Pressed,this,&ARangeCharacter::EquipRifle);
 Input->BindKey(EKeys::Two,IE_Pressed,this,&ARangeCharacter::EquipKnife);
 Input->BindKey(EKeys::Three,IE_Pressed,this,&ARangeCharacter::EquipMP7);
 Input->BindKey(EKeys::MouseScrollUp,IE_Pressed,this,&ARangeCharacter::CycleWeapon);
 Input->BindKey(EKeys::MouseScrollDown,IE_Pressed,this,&ARangeCharacter::CycleWeaponBack);
 Input->BindKey(EKeys::LeftMouseButton,IE_Pressed,this,&ARangeCharacter::KnifeAttackPressed);
 Input->BindKey(EKeys::F,IE_Pressed,this,&ARangeCharacter::TestFingers);
 Input->BindKey(EKeys::V,IE_Pressed,this,&ARangeCharacter::ToggleView);Input->BindKey(EKeys::R,IE_Pressed,this,&ARangeCharacter::Reload);
}
void ARangeCharacter::LookX(float V){if(auto* PC=Cast<ARangePlayerController>(Controller);PC&&PC->UsesNativeRawMouse())return;if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineAudit")))AddControllerYawInput(V*MouseDegreesPerUnit*(Camera->FieldOfView/90.f));}
void ARangeCharacter::LookY(float V){if(auto* PC=Cast<ARangePlayerController>(Controller);PC&&PC->UsesNativeRawMouse())return;if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineAudit")))AddControllerPitchInput(V*MouseDegreesPerUnit*(Camera->FieldOfView/90.f));}
void ARangeCharacter::ApplyRawMouseDelta(const FVector2D& Delta)
{
 const double Gain=MouseDegreesPerUnit*(Camera->FieldOfView/90.f);
 AddControllerYawInput(Delta.X*Gain);AddControllerPitchInput(Delta.Y*Gain);
}
void ARangeCharacter::ToggleView()
{
 const bool WasOrbit=bThirdPersonOrbit;
 bThirdPerson=!bThirdPerson;
 // The third-person view is a model observer for this FPS. Keep scripted
 // gameplay audits on their original camera-facing path for comparison.
 bThirdPersonOrbit=bThirdPerson&&(!FString(FCommandLine::Get()).Contains(TEXT("Audit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineThirdPersonOrbitAudit")));
 if(WasOrbit&&!bThirdPerson&&Controller)SetActorRotation(FRotator(0,Controller->GetControlRotation().Yaw,0));
 bUseControllerRotationYaw=!bThirdPerson;
 GetCharacterMovement()->bUseControllerDesiredRotation=false;
 GetCharacterMovement()->bOrientRotationToMovement=bThirdPerson&&!bThirdPersonOrbit;
 StanceEyeZ=bThirdPerson?30:(bIsCrouched?59:75);
 Boom->SetRelativeLocation(FVector(0,0,StanceEyeZ));Boom->TargetArmLength=bThirdPerson?350:0;Boom->SocketOffset=bThirdPerson?FVector(0,50,15):FVector::ZeroVector;
 GetMesh()->SetOwnerNoSee(!bThirdPerson);BodyGun->SetOwnerNoSee(!bThirdPerson);BodyKnife->SetOwnerNoSee(!bThirdPerson);Arms->SetVisibility(!bThirdPerson,true);UpdateHandVisibility();
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_VIEW third_person=%d orbit=%d"),bThirdPerson,bThirdPersonOrbit);
}
void ARangeCharacter::Tick(float Dt)
{
 Super::Tick(Dt);
 WeaponHandling[0].Tick(Dt,RangeHandling::Rifle);WeaponHandling[1].Tick(Dt,RangeHandling::MP7);
 auto* PC=Cast<APlayerController>(Controller);if(!PC)return;
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineAudit")) && GetGameTimeSinceCreation()>19 && GetGameTimeSinceCreation()<21)AddMovementInput(GetActorForwardVector(),1);
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineReloadAudit")) && GetGameTimeSinceCreation()>30 && GetGameTimeSinceCreation()<33)AddMovementInput(GetActorRightVector(),.65f);
 const FRotator Yaw(0,PC->GetControlRotation().Yaw,0);
 const bool bCMUSideAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineCMUSideStepAudit"));
 const bool bAudit=bCMUSideAudit||FParse::Param(FCommandLine::Get(),TEXT("FirelineDirectionalGameplayAudit"))||bMP7Audit||bOpticAudit||FParse::Param(FCommandLine::Get(),TEXT("FirelineHandsAudit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineShieldAudit"))||bKarambitAudit||bM9Audit||bHitboxAudit||bVisibilityAudit||bMobilityAudit||bShootingAudit||bMovementAudit||bWeaponAudit||bHoldAudit||FParse::Param(FCommandLine::Get(),TEXT("FirelineAudit"))||FParse::Param(FCommandLine::Get(),TEXT("FirelineReloadAudit"));
 const auto* Pad=Cast<ARangePlayerController>(PC);
 const FVector2D PadMove=Pad?Pad->PadMove:FVector2D::ZeroVector;
 const float Forward=bAudit?0.f:(PC->IsInputKeyDown(EKeys::W)?1.f:0)-(PC->IsInputKeyDown(EKeys::S)?1.f:0)+PadMove.Y;
 const float Right=bAudit?0.f:(PC->IsInputKeyDown(EKeys::D)?1.f:0)-(PC->IsInputKeyDown(EKeys::A)?1.f:0)+PadMove.X;
 if(!bSliding){AddMovementInput(Yaw.Vector(),Forward);AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y),Right);}
 if(bCMUSideAudit)
 {
  const float T=GetGameTimeSinceCreation()-2.f;
  if(T>=0)
  {
   const float Phase=FMath::Fmod(T,8.f);
   if(Phase<3.f)AddMovementInput(GetActorRightVector(),.4f);
   else if(Phase>=4.f&&Phase<7.f)AddMovementInput(-GetActorRightVector(),.4f);
  }
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineDirectionalGameplayAudit")))
 {
  const float T=GetGameTimeSinceCreation()-2.f;
  const int Group=int(T/2.f);const float Local=FMath::Fmod(T,2.f);
  float Angle=(Group%8)*45.f;
  if(Group==16||Group==18)Angle=(int(Local/.3f)%2)?-90:90;
  if(Group==17||Group==19)Angle=(int(Local/.3f)%2)?180:0;
  if(T>=0&&Group<20&&Local>.25f&&Local<1.45f)AddMovementInput(FRotator(0,Angle,0).Vector(),Group<8?.4f:1.f);
  if(FParse::Param(FCommandLine::Get(),TEXT("FirelineDirectionBoundaryAudit"))&&Group>=24&&Group<48&&Local>.25f&&Local<1.45f)
  {
   // Half-sector samples, low/intermediate speeds, wrap crossing and smooth
   // direction sweeps. Inputs still go through CharacterMovement, not poses.
   float BoundaryAngle=((Group-24)%8)*45.f+22.5f;
   float Strength=Group<32?.4f:1.f;
   if(Group>=40&&Group<44){BoundaryAngle=(Group%2)*90.f;Strength=Group<42?.2f:(300.f/450.f);}
   if(Group==44||Group==45){BoundaryAngle=179.f+2.f*FMath::Sin(Local*2.f*PI);Strength=Group==44?.4f:1.f;}
   if(Group==46||Group==47){BoundaryAngle=(Local-.25f)*300.f;Strength=Group==46?.4f:1.f;}
   AddMovementInput(FRotator(0,BoundaryAngle,0).Vector(),Strength);
  }
  if(FParse::Param(FCommandLine::Get(),TEXT("FirelineCombatPostureAudit"))&&Group>=24&&Group<40)
  {
   if(Local>.25f&&Local<1.6f)AddMovementInput(FRotator(0,((Group-24)%8)*45.f,0).Vector(),.55f);
   if(Local>.7f&&Local<1.f)StartAim();else EndAim();
   if(Local>1.1f&&Local<1.45f)Fire();
  }
  if(FParse::Param(FCommandLine::Get(),TEXT("FirelineArmedMovementAudit"))&&Group>=24&&Group<40)
  {
   if(Local>.25f&&Local<1.5f){const float RunAngle=Group<30?((Group%3)==0?0.f:(Group%3)==1?45.f:-45.f):0.f;AddMovementInput(FRotator(0,RunAngle,0).Vector(),1.f);}
   if(Group==30||Group==31){if(Local>.8f&&Local<1.3f)StartAim();else EndAim();}
   if(Group==32&&Local>.85f&&Local<.88f)EquipMP7();
   if(Group==33&&Local>.60f&&Local<.63f)Fire();
   if(Group==33&&Local>.85f&&Local<.88f)Reload();
   if((Group==36||Group==37)&&Local>.8f&&Local<.83f)StartCrouch();
   if((Group==36||Group==37)&&Local>1.6f)EndCrouch();
   if((Group==38||Group==39)&&Local>.8f&&Local<.83f&&!bJumpHeld)StartJump();
   if((Group==38||Group==39)&&Local>1.f)EndJump();
   if((Group==34||Group==35)&&Local>1.25f&&Local<1.28f)Fire();
   if(Group==34&&Local>1.5f&&Local<1.53f)Reload();
  }
  if(FParse::Param(FCommandLine::Get(),TEXT("FirelineFullBodyTrace"))&&Group>=20&&Group<24)
  {
   if((Group%2)==1&&Local>.25f&&Local<1.45f)AddMovementInput(FVector::ForwardVector,1.f);
   if(Local>.4f&&Local<.44f&&!bJumpHeld)StartJump();
   if(Local>.55f&&bJumpHeld)EndJump();
  }
 }
 const float RunFireTime=GetGameTimeSinceCreation()-2.f;
 const int RunFireGroup=int(RunFireTime/2.f);const float RunFireLocal=FMath::Fmod(RunFireTime,2.f);
 const bool RunFireAudit=FParse::Param(FCommandLine::Get(),TEXT("FirelineRunFireAudit"))&&RunFireGroup>=44&&RunFireGroup<52;
 if(RunFireAudit)
 {
  if(RunFireLocal>.1f&&RunFireLocal<1.85f)AddMovementInput(FRotator(0,RunFireGroup%4==3?45.f:0.f,0).Vector(),1.f);
  if(RunFireGroup%4==2&&RunFireLocal>.6f&&RunFireLocal<1.5f)StartAim();else EndAim();
 }
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineWalkHoldAudit"))&&RunFireGroup>=52&&RunFireGroup<56&&RunFireLocal>.25f&&RunFireLocal<1.5f)
 {
  const float Angles[]={0.f,90.f,45.f,-45.f};
  AddMovementInput(FRotator(0,Angles[RunFireGroup-52],0).Vector(),1.f);
 }
 const bool HeldFire=((!bAudit||bMP7Audit)&&(PC->IsInputKeyDown(EKeys::LeftMouseButton)||(Pad&&Pad->PadFire)))||(RunFireAudit&&RunFireLocal>.9f&&RunFireLocal<1.4f);
 FireCooldown=FMath::Max(bMP7&&HeldFire?-.12f:0.f,FireCooldown-Dt);HitFlash=FMath::Max(0.f,HitFlash-Dt);
 Kick=FMath::FInterpTo(Kick,0,Dt,16);
 if(ReloadFireReturnLeft>0)
 {
  ReloadFireReturnLeft=FMath::Max(0.f,ReloadFireReturnLeft-Dt);
  if(ReloadFireReturnLeft<=0&&bFireAfterReloadReturn)
  {bFireAfterReloadReturn=false;Fire();}
 }
 if(HeldFire&&FireCooldown<=0)
 {for(int I=0;I<(bMP7?3:1)&&FireCooldown<=0;++I){const int Before=Ammo;Fire();if(Ammo==Before)break;}}
 if(bReloading)
 {
  const float Previous=ReloadPosition;
  if(ReloadAuditSample>=0)ReloadLeft=ReloadDuration-ReloadAuditSample;
  else if(ReloadLeadOut>0)ReloadLeadOut=FMath::Max(0.f,ReloadLeadOut-Dt);
  else ReloadLeft-=Dt*RangeWeaponTiming::ReloadPlayRate(bMP7);
  ReloadPosition=FMath::Clamp(ReloadDuration-ReloadLeft,0.f,ReloadDuration);
  if(ReloadAuditSample<0)CombatAudio->ReloadPhase(Previous,ReloadPosition,bMP7,bEmptyReload);
  if(ReloadPosition>=RangeWeaponTiming::MagazineDrop(bMP7,bEmptyReload)&&!bMagazineDropped){DropMagazine();bMagazineDropped=true;}
  if(!bReloadAmmoCommitted&&ReloadPosition>=RangeWeaponTiming::MagazineSeat(bMP7,bEmptyReload))
  {
   Ammo=MagazineCapacity();bReloadAmmoCommitted=true;
   UE_LOG(LogTemp,Display,TEXT("FIRELINE_RELOAD magazine_seated time=%.4f ammo=%d weapon=%d"),ReloadPosition,Ammo,WeaponSlot());
  }
  if(ReloadLeft<=0){bReloading=false;UE_LOG(LogTemp,Display,TEXT("FIRELINE_RELOAD completed ammo=%d weapon=%d"),Ammo,WeaponSlot());}
 }
 UpdateFiringMovement(Dt);
 UpdateHands(Dt);UpdateMP7(Dt);UpdateM4(Dt);
 // Finish the last shot and any queued switch before starting an empty reload.
 if(Ammo<=0&&FireCooldown<=0&&QueuedWeapon<0)Reload();
 UpdateKnife(Dt);
 UpdateMobility(Dt);
 UpdateShooting(Dt);
 UpdateAim(Dt);
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineContactCarryStudy")))
 {
  const bool Available=WeaponSlot()==1&&!bReloading&&!bReloadPoseHeld&&ReloadFireReturnLeft<=0&&HandSwitchLeft<=0&&M4Weight<.001f&&!bAimHeld&&!bFiringMovement&&!bIsCrouched&&Locomotion.Grounded&&!bSliding;
  const float ForwardCarryWeight=Locomotion.Speed<5.f?1.f:1.f-FMath::SmoothStep(15.,55.,double(FMath::Abs(Locomotion.DirectionDegrees)));
  const float Level=bThirdPersonOrbit?1.f:1.f-FMath::SmoothStep(25.,50.,FMath::Abs(GetControlRotation().GetNormalized().Pitch));
  ContactCarryClock.Update(Dt,Locomotion.Speed,SprintCarryAlpha,Available?ForwardCarryWeight*Level*(1.f-AimAlpha)*(1.f-AirBlend)*(1.f-SlideBlend):0.f);
 }
 if(bMobilityAudit)TickMobilityAudit(Dt);
 if(bViewFixAudit)TickViewFixAudit(Dt);
 if(bShootingAudit)TickShootingAudit(Dt);
 if(bMovementAudit)TickMovementAudit(Dt);
 if(bHoldAudit)TickHoldAudit(Dt);
 if(bM9Audit)TickM9Audit(Dt);
 if(bWeaponAudit)TickWeaponAudit(Dt);
 if(GetActorLocation().Z < -500) {SetActorLocation(FVector(0,0,95));GetCharacterMovement()->StopMovementImmediately();}
}
void ARangeCharacter::Reload()
{
 if(bEmptyHands||bKnife||HandSwitchLeft>0||ReloadFireReturnLeft>0||bReloading||Ammo>=MagazineCapacity()||IsMP7DrawLocked()||IsM4DrawLocked())return;
 CancelMP7Action();CancelM4Action();bEmptyReload=Ammo<=0;ReloadDuration=bMP7?(bEmptyReload?163.f/60.f:139.f/60.f):RangeWeaponTiming::M4ReloadDuration(bEmptyReload);
 bReloadAmmoCommitted=false;
 bReloadPoseHeld=false;bReloading=true;ReloadLeft=ReloadDuration;ReloadPosition=0;bMagazineDropped=false;ReloadLeadOut=(AimAlpha>.05f||(MP7ActionBlend>0&&MP7Action!=1))?.18f:0.f;
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_RELOAD started ammo=%d"),Ammo);
}
ARangeTarget::ARangeTarget()
{
 PrimaryActorTick.bCanEverTick=true;
 Capsule=CreateDefaultSubobject<UCapsuleComponent>(TEXT("TargetCollision"));RootComponent=Capsule;Capsule->InitCapsuleSize(30,90);
 Capsule->SetCollisionProfileName(TEXT("BlockAll"));Capsule->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
 Body=CreateDefaultSubobject<URangeHitMesh>(TEXT("RyanRed"));Body->SetupAttachment(Capsule);Body->SetRelativeLocation(FVector(0,0,-90));Body->SetRelativeRotation(FRotator(0,-90,0));Body->SetRelativeScale3D(FVector(.94));Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Body->VisibilityBasedAnimTickOption=EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Ryan(TEXT("/Game/Fireline/Action/SK_RyanAction.SK_RyanAction"));Body->SetSkeletalMesh(Ryan.Object);
 Gun=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TastyTonyM4"));Gun->SetupAttachment(Body);Gun->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> M4(TEXT("/Game/Fireline/Action/SK_M4Action.SK_M4Action"));Gun->SetSkeletalMesh(M4.Object);Gun->bUseAttachParentBound=true;
 ShieldShell=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("EnergyShieldShell"));ShieldShell->SetupAttachment(Capsule);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> ShellMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 ShieldShell->SetStaticMesh(ShellMesh.Object);ShieldShell->SetRelativeLocation(FVector(0,0,10));ShieldShell->SetRelativeScale3D(FVector(1.3,1.2,2.12));
 ShieldShell->SetCollisionEnabled(ECollisionEnabled::NoCollision);ShieldShell->SetGenerateOverlapEvents(false);ShieldShell->SetCastShadow(false);
 ShieldShell->SetCanEverAffectNavigation(false);

}
void ARangeTarget::BeginPlay(){Super::BeginPlay();ApplyTeam(Body,true);Body->PlayAnimation(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/Action/A_M4_Idle.A_M4_Idle")),true);Gun->SetLeaderPoseComponent(Body);
 if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Fireline/Materials/M_Shield.M_Shield")))
 {ShieldMaterial=UMaterialInstanceDynamic::Create(Material,this);Body->SetOverlayMaterial(ShieldMaterial);ShieldMaterial->SetScalarParameterValue(TEXT("Charge"),1);}
 if(auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Fireline/Materials/M_EnergyShell.M_EnergyShell")))
 {ShellMaterial=UMaterialInstanceDynamic::Create(Material,this);ShieldShell->SetMaterial(0,ShellMaterial);}
 StartShieldAudit();
}
bool ARangeTarget::RegisterHit(int Damage)
{
 if(ResetLeft>0||!Vitals.Damage(Damage))return false;
 if(LifeHitCount++==0)FirstHitTime=GetWorld()->GetTimeSeconds();
 FeedbackLeft=1.5f;
 if(GetHealth()<=0){LastTTK=GetWorld()->GetTimeSeconds()-FirstHitTime;UE_LOG(LogTemp,Display,TEXT("FIRELINE_TTK seconds=%.3f hits=%d"),LastTTK,LifeHitCount);ResetLeft=2;SetActorHiddenInGame(true);SetActorEnableCollision(false);}
 return true;
}
void ARangeTarget::ShieldImpact(const FVector& Point)
{
 ShieldPulse=1;bShieldBroken=GetShield()<=0;
 if(ShieldMaterial){ShieldMaterial->SetVectorParameterValue(TEXT("HitPoint"),FLinearColor(Point.X,Point.Y,Point.Z));ShieldMaterial->SetScalarParameterValue(TEXT("Pulse"),1);}
 if(bShieldBroken)
 {
  // The localized contact sparks belong to the shot; this is the whole shell.
  auto* Break=GetWorld()->SpawnActor<ARangeShieldBreakFX>();
  if(Break)Break->Setup(ShieldShell->GetComponentTransform());
  ShieldShell->SetVisibility(false);
 }
}
void ARangeTarget::Tick(float Dt)
{
 Super::Tick(Dt);FeedbackLeft=FMath::Max(0.f,FeedbackLeft-Dt);
 if(ResetLeft>0)
 {
  ResetLeft-=Dt;if(ResetLeft<=0){Vitals.Reset();LifeHitCount=0;ShieldPulse=0;bShieldBroken=false;SetActorHiddenInGame(false);SetActorEnableCollision(true);}
 }
 else Vitals.Tick(Dt);
 ShieldPulse=FMath::Max(0.f,ShieldPulse-Dt/(bShieldBroken?.45f:.22f));
 // Once broken, let the intact shell collapse so the individual shards carry
 // the break silhouette instead of a still-solid blue bubble.
 const float ShellPresence=GetShield()>0?1.f:0.f;
 ShieldShell->SetVisibility(GetHealth()>0&&GetShield()>0);
 if(ShellMaterial){ShellMaterial->SetScalarParameterValue(TEXT("Presence"),ShellPresence);ShellMaterial->SetScalarParameterValue(TEXT("Pulse"),ShieldPulse);}

 if(ShieldMaterial)
 {
  ShieldMaterial->SetScalarParameterValue(TEXT("Charge"),GetShield()/FRangeVitals::MaxShield);
  ShieldMaterial->SetScalarParameterValue(TEXT("Pulse"),ShieldPulse);
  ShieldMaterial->SetVectorParameterValue(TEXT("Color"),bShieldBroken&&ShieldPulse>0?FLinearColor(.3,1,4):FLinearColor(.03,1.4,3));
 }
}

ARangeGameMode::ARangeGameMode(){DefaultPawnClass=ARangeCharacter::StaticClass();HUDClass=ARangeHUD::StaticClass();PlayerControllerClass=ARangePlayerController::StaticClass();}
void URangeAssetTools::BuildLocomotion()
{
#if WITH_EDITOR
 const FString Path=TEXT("/Game/Fireline/Playable/BS_Ryan");
 UPackage* Package=CreatePackage(*Path);
 auto* BS=NewObject<UBlendSpace>(Package,TEXT("BS_Ryan"),RF_Public|RF_Standalone);
 auto* Idle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/Providers/Ryan/Animations/A_Ryan_CombatIdle.A_Ryan_CombatIdle"));BS->SetSkeleton(Idle->GetSkeleton());
 BS->AddSample(Idle,FVector(0,0,0));
 BS->AddSample(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/Providers/Ryan/Animations/A_Ryan_RunForward.A_Ryan_RunForward")),FVector(65,0,0));
 BS->AddSample(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/Providers/Ryan/Animations/A_Ryan_Sprint.A_Ryan_Sprint")),FVector(100,0,0));
 auto* Crouch=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/Providers/Ryan/Animations/A_Ryan_CrouchIdle.A_Ryan_CrouchIdle"));
 BS->AddSample(Crouch,FVector(0,100,0));BS->AddSample(Crouch,FVector(100,100,0));
 BS->ValidateSampleData();BS->ResampleData();BS->PostEditChange();
 FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 UPackage::SavePackage(Package,BS,*FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()),Args);
#endif
}

void ARangeCharacter::DropMagazine()
{
 auto* Model=bThirdPerson?BodyGun.Get():ViewGun.Get();
 auto* Mesh=bMP7?MP7MagazineMesh.Get():DroppedMagazineMesh.Get();
 if(!Mesh)return;
 const FTransform Pose=Model->GetSocketTransform(WeaponBone(TEXT("mag")));
 FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
 auto* Drop=GetWorld()->SpawnActor<AStaticMeshActor>(Pose.GetLocation(),Pose.Rotator(),Params);
 auto* C=Drop->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(Mesh);
 Drop->SetActorScale3D(FVector(bMP7?.94f:.893f));C->SetCollisionProfileName(TEXT("PhysicsActor"));C->SetCollisionResponseToChannel(ECC_Pawn,ECR_Ignore);C->SetCollisionResponseToChannel(ECC_Visibility,ECR_Ignore);
 C->SetSimulatePhysics(true);C->SetPhysicsLinearVelocity(GetVelocity()+FVector(0,0,bMP7?-180.f:-60.f));C->AddAngularImpulseInDegrees(FVector(45,75,20),NAME_None,true);Drop->SetLifeSpan(8);
 LastDroppedMagazine=Drop;++MagazineDropCount;
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_MAGAZINE_DROPPED weapon=%d empty=%d position=%s"),WeaponSlot(),bEmptyReload,*Pose.GetLocation().ToString());
}
static void BuildMatchedActionAssets(const FString& Directory)
{
#if WITH_EDITOR
 const FString Path=Directory/TEXT("BS_M4");UPackage* Package=CreatePackage(*Path);
 auto* BS=LoadObject<UBlendSpace>(nullptr,*Path);
 if(!BS)BS=NewObject<UBlendSpace>(Package,TEXT("BS_M4"),RF_Public|RF_Standalone);
 Package->FullyLoad();while(BS->GetNumberOfBlendSamples()>0)BS->DeleteSample(BS->GetNumberOfBlendSamples()-1);
 auto* Idle=LoadObject<UAnimSequence>(nullptr,*(Directory/TEXT("A_M4_Idle.A_M4_Idle")));BS->SetSkeleton(Idle->GetSkeleton());
 auto Load=[&Directory](const TCHAR* Name){return LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("%s/A_M4_%s.A_M4_%s"),*Directory,Name,Name));};
 BS->AddSample(Idle,FVector(0,0,0));BS->AddSample(Load(TEXT("Run")),FVector(65,0,0));BS->AddSample(Load(TEXT("Sprint")),FVector(100,0,0));
 BS->AddSample(Load(TEXT("Crouch")),FVector(0,100,0));BS->AddSample(Load(TEXT("CrouchMove")),FVector(180.f/7,100,0));BS->AddSample(Load(TEXT("CrouchMove")),FVector(100,100,0));BS->ValidateSampleData();BS->ResampleData();BS->PostEditChange();
 auto* Skeleton=Idle->GetSkeleton();
 Skeleton->UpdateReferencePoseFromMesh(LoadObject<USkeletalMesh>(nullptr,*(Directory/TEXT("SK_RyanAction.SK_RyanAction"))));
 for(const FMeshBoneInfo& Info:Skeleton->GetReferenceSkeleton().GetRefBoneInfo())
 {
  const FName Bone=Info.Name;if(!Bone.ToString().StartsWith(TEXT("M4_")) && !Bone.ToString().StartsWith(TEXT("DJ_")) && !Bone.ToString().StartsWith(TEXT("Finger_")) && !Bone.ToString().StartsWith(TEXT("PalmCup_")) && Bone!=TEXT("KnifeHold") && !Bone.ToString().StartsWith(TEXT("G_")) && !Bone.ToString().StartsWith(TEXT("K_")))continue;
  Skeleton->SetBoneTranslationRetargetingMode(Skeleton->GetReferenceSkeleton().FindBoneIndex(Bone),EBoneTranslationRetargetingMode::Animation);
  const FName Name(*FString::Printf(TEXT("Required_%s"),*Bone.ToString()));
  if(!Skeleton->FindSocket(Name)){auto* Socket=NewObject<USkeletalMeshSocket>(Skeleton);Socket->SocketName=Name;Socket->BoneName=Bone;Socket->bForceAlwaysAnimated=true;Skeleton->Sockets.Add(Socket);}
 }
 // Locate the actual barrel's end ring in the imported mesh, in the same bind
 // space as skinning. The old authoring marker omitted the source object scale.
 auto* Gun=LoadObject<USkeletalMesh>(nullptr,*(Directory/TEXT("SK_M4Action.SK_M4Action")));
 const auto& Ref=Gun->GetRefSkeleton();TArray<FTransform> Bind;
 for(int I=0;I<Ref.GetNum();++I){const int P=Ref.GetParentIndex(I);Bind.Add(P<0?Ref.GetRefBonePose()[I]:Ref.GetRefBonePose()[I]*Bind[P]);}
 const FTransform Marker=Bind[Ref.FindBoneIndex(TEXT("M4_muzzle"))];
 const FVector Sight=(Bind[Ref.FindBoneIndex(TEXT("M4_frontsight"))].GetLocation()-Bind[Ref.FindBoneIndex(TEXT("M4_rearsight"))].GetLocation()).GetSafeNormal();
 FVector Bore;float Best=0;
 for(EAxis::Type Axis:{EAxis::X,EAxis::Y,EAxis::Z}){const FVector D=Marker.GetUnitAxis(Axis);const float Dot=FVector::DotProduct(D,Sight);if(FMath::Abs(Dot)>Best){Best=FMath::Abs(Dot);Bore=D*FMath::Sign(Dot);}}
 const auto& Positions=Gun->GetResourceForRendering()->LODRenderData[0].StaticVertexBuffers.PositionVertexBuffer;
 TArray<FVector> Near;float Furthest=-BIG_NUMBER;
 for(uint32 I=0;I<Positions.GetNumVertices();++I){const FVector P(Positions.VertexPosition(I));if(FVector::Distance(P,Marker.GetLocation())<8){Near.Add(P);Furthest=FMath::Max(Furthest,FVector::DotProduct(P,Bore));}}
 FBox EndRing(ForceInit);for(const FVector& P:Near)if(Furthest-FVector::DotProduct(P,Bore)<.005f)EndRing+=P;
 check(EndRing.IsValid);const FVector Tip=EndRing.GetCenter();
 const FName SocketName(TEXT("M4_Flash"));auto* Flash=Skeleton->FindSocket(SocketName);
 if(!Flash){Flash=NewObject<USkeletalMeshSocket>(Skeleton);Flash->SocketName=SocketName;Skeleton->Sockets.Add(Flash);}
 Flash->BoneName=TEXT("M4_body");Flash->bForceAlwaysAnimated=true;
 const FTransform Parent=Bind[Ref.FindBoneIndex(Flash->BoneName)];
 Flash->RelativeLocation=Parent.InverseTransformPosition(Tip);
 Flash->RelativeRotation=(Parent.GetRotation().Inverse()*Bore.Rotation().Quaternion()).Rotator();Flash->RelativeScale=FVector::OneVector;
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_MUZZLE_CALIBRATION offset_cm=%.6f vertices=%d tip=%s"),FVector::Distance(Tip,Marker.GetLocation()),Near.Num(),*Tip.ToString());
 Skeleton->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 UPackage::SavePackage(Package,BS,*FPackageName::LongPackageNameToFilename(Path,FPackageName::GetAssetPackageExtension()),Args);
#endif
}

void URangeAssetTools::BuildActionAssets(){BuildMatchedActionAssets(TEXT("/Game/Fireline/Action"));}
void URangeAssetTools::BuildHandsAssets(){BuildMatchedActionAssets(TEXT("/Game/Fireline/Hands"));}
void URangeAssetTools::BuildDrazenAssets()
{
 #if WITH_EDITOR
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Drazen/SK_RyanAction.SK_RyanAction"));
 check(Mesh);
 auto* Gun=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Drazen/SK_M4Action.SK_M4Action"));check(Gun);
 const FReferenceSkeleton& Ref=Gun->GetRefSkeleton();
 const int DebugBone=Ref.FindBoneIndex(TEXT("M4_magretainer"));
 UE_LOG(LogTemp,Display,TEXT("DRAZEN_BIND_MATCH body=%s gun=%s"),*Mesh->GetRefSkeleton().GetRefBonePose()[Mesh->GetRefSkeleton().FindBoneIndex(TEXT("M4_magretainer"))].GetRotation().ToString(),*Ref.GetRefBonePose()[DebugBone].GetRotation().ToString());
 for(USkeletalMesh* Body:{Mesh,LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Drazen/SK_DrazenKnife.SK_DrazenKnife")),LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Drazen/SK_DrazenArms.SK_DrazenArms")),LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Drazen/SK_DrazenKnifeArms.SK_DrazenKnifeArms"))})
 {
  if(!Body)continue;Body->Modify();
  for(int LOD=0;LOD<Body->GetLODNum();++LOD)Body->GetLODInfo(LOD)->bAllowCPUAccess=true;
  {FReferenceSkeletonModifier Edit(Body->GetRefSkeleton(),Body->GetSkeleton());
   for(int I=0;I<Ref.GetNum();++I)if(Ref.GetBoneName(I).ToString().StartsWith(TEXT("M4_")))
   {const int J=Body->GetRefSkeleton().FindBoneIndex(Ref.GetBoneName(I));if(J!=INDEX_NONE)Edit.UpdateRefPoseTransform(J,Ref.GetRefBonePose()[I]);}}
  Body->CalculateInvRefMatrices();Body->PostEditChange();Body->MarkPackageDirty();
  FSavePackageArgs SaveBody;SaveBody.TopLevelFlags=RF_Public|RF_Standalone;
  UPackage::SavePackage(Body->GetOutermost(),Body,*FPackageName::LongPackageNameToFilename(Body->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),SaveBody);
 }
 BuildMatchedActionAssets(TEXT("/Game/Fireline/Drazen"));
 // These clips are baked onto this exact rig; preserve all authored joint offsets.
 auto* NativeSkeleton=Mesh->GetSkeleton();
 for(int Bone=0;Bone<NativeSkeleton->GetReferenceSkeleton().GetNum();++Bone)NativeSkeleton->SetBoneTranslationRetargetingMode(Bone,EBoneTranslationRetargetingMode::Animation);
 NativeSkeleton->Modify();NativeSkeleton->MarkPackageDirty();
 const FString SettingsPath=TEXT("/Game/Fireline/Drazen/BC_Matched");
 UPackage* SettingsPackage=CreatePackage(*SettingsPath);
 auto* Compression=LoadObject<UAnimBoneCompressionSettings>(nullptr,*SettingsPath);
 if(!Compression)
 {
  Compression=NewObject<UAnimBoneCompressionSettings>(SettingsPackage,TEXT("BC_Matched"),RF_Public|RF_Standalone);
  UClass* Safe=LoadClass<UAnimBoneCompressionCodec>(nullptr,TEXT("/Script/ACLPlugin.AnimBoneCompressionCodec_ACLSafe"));check(Safe);
  Compression->Codecs.Reset();Compression->Codecs.Add(NewObject<UAnimBoneCompressionCodec>(Compression,Safe));
 }
 FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;
 UPackage::SavePackage(SettingsPackage,Compression,*FPackageName::LongPackageNameToFilename(SettingsPath,FPackageName::GetAssetPackageExtension()),Save);
 const TCHAR* Clips[]={TEXT("M4_Idle"),TEXT("M4_Aim"),TEXT("M4_Reload"),TEXT("M4_Run"),TEXT("M4_Sprint"),TEXT("M4_Crouch"),TEXT("M4_CrouchMove"),TEXT("M4_Slide"),TEXT("M4_JumpRise"),TEXT("M4_JumpFall"),TEXT("M4_Land"),TEXT("Hands_Open"),TEXT("Hands_Fist"),TEXT("Hands_Test"),TEXT("Karambit_Hold"),TEXT("Karambit_Draw"),TEXT("Karambit_Inspect"),TEXT("Karambit_Slash")};
 for(const TCHAR* AnimationDir:{TEXT("Drazen"),TEXT("Drazen/View")})
 for(const TCHAR* Clip:Clips)
 {
  auto* Seq=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/%s/A_%s.A_%s"),AnimationDir,Clip,Clip));if(!Seq)continue;
  Seq->Modify();Seq->BoneCompressionSettings=Compression;Seq->RetargetSource=NAME_None;Seq->SetRetargetSourceAsset(Mesh);Seq->UpdateRetargetSourceAssetData();
  auto& Controller=Seq->GetController();Controller.OpenBracket(FText::FromString(TEXT("Keep matched mechanical rotations")),false);
  TArray<FName> TrackNames;Seq->GetDataModel()->GetBoneTrackNames(TrackNames);
  for(FName Name:TrackNames)
  {
   if(!Name.ToString().StartsWith(TEXT("M4_"))||Name==TEXT("M4_body"))continue;
   const int I=Ref.FindBoneIndex(Name);if(I==INDEX_NONE)continue;
   TArray<FTransform> Keys;Seq->GetDataModel()->GetBoneTrackTransforms(Name,Keys);
   TArray<FVector3f> Positions,Scales;TArray<FQuat4f> Rotations;
   for(const FTransform& Key:Keys){Positions.Add(FVector3f(Key.GetTranslation()));Scales.Add(FVector3f(Key.GetScale3D()));Rotations.Add(FQuat4f(Ref.GetRefBonePose()[I].GetRotation()));}
   check(Controller.SetBoneTrackKeys(Name,Positions,Rotations,Scales,false));
  }
  Controller.CloseBracket(false);
  if(FString(Clip)==TEXT("M4_Idle"))UE_LOG(LogTemp,Display,TEXT("DRAZEN_RAW_MAG q=%s"),*Seq->GetDataModel()->GetBoneTrackTransform(TEXT("M4_magretainer"),FFrameNumber(0)).GetRotation().ToString());
  Seq->PostEditChange();Seq->MarkPackageDirty();
  UPackage::SavePackage(Seq->GetOutermost(),Seq,*FPackageName::LongPackageNameToFilename(Seq->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Save);
 }
 for(const TCHAR* Weapon:{TEXT("SK_M4Action"),TEXT("SK_Karambit")})
 {
  auto* View=LoadObject<USkeletalMesh>(nullptr,*FString::Printf(TEXT("/Game/Fireline/Drazen/%s.%s"),Weapon,Weapon));check(View);
  View->Modify();for(int LOD=0;LOD<View->GetLODNum();++LOD)View->GetLODInfo(LOD)->bAllowCPUAccess=true;
  View->PostEditChange();View->MarkPackageDirty();
  UPackage::SavePackage(View->GetOutermost(),View,*FPackageName::LongPackageNameToFilename(View->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Save);
 }
 UE_LOG(LogTemp,Display,TEXT("DRAZEN_MATCHED_COMPRESSION clips=18 rigid_rotations=imported_bind precision=ACL_safe"));
 Mesh->Modify();for(int I=0;I<Mesh->GetLODNum();++I)Mesh->GetLODInfo(I)->bAllowCPUAccess=true;
 Mesh->PostEditChange();Mesh->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 UPackage::SavePackage(Mesh->GetOutermost(),Mesh,*FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
#endif
}

void URangeAssetTools::BuildOriginalDrazenAssets()
{
 #if WITH_EDITOR
 auto* Mesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/DrazenOriginal/SK_RyanAction.SK_RyanAction"));
 check(Mesh);
 auto* Gun=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/DrazenOriginal/SK_M4Action.SK_M4Action"));check(Gun);
 const FReferenceSkeleton& Ref=Gun->GetRefSkeleton();
 const int DebugBone=Ref.FindBoneIndex(TEXT("M4_magretainer"));
 UE_LOG(LogTemp,Display,TEXT("DRAZEN_BIND_MATCH body=%s gun=%s"),*Mesh->GetRefSkeleton().GetRefBonePose()[Mesh->GetRefSkeleton().FindBoneIndex(TEXT("M4_magretainer"))].GetRotation().ToString(),*Ref.GetRefBonePose()[DebugBone].GetRotation().ToString());
 for(USkeletalMesh* Body:{Mesh,LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/DrazenOriginal/SK_DrazenKnife.SK_DrazenKnife")),LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/DrazenOriginal/SK_DrazenArms.SK_DrazenArms")),LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/DrazenOriginal/SK_DrazenKnifeArms.SK_DrazenKnifeArms"))})
 {
  if(!Body)continue;Body->Modify();
  for(int LOD=0;LOD<Body->GetLODNum();++LOD)Body->GetLODInfo(LOD)->bAllowCPUAccess=true;
  {FReferenceSkeletonModifier Edit(Body->GetRefSkeleton(),Body->GetSkeleton());
   for(int I=0;I<Ref.GetNum();++I)if(Ref.GetBoneName(I).ToString().StartsWith(TEXT("M4_")))
   {const int J=Body->GetRefSkeleton().FindBoneIndex(Ref.GetBoneName(I));if(J!=INDEX_NONE)Edit.UpdateRefPoseTransform(J,Ref.GetRefBonePose()[I]);}}
  Body->CalculateInvRefMatrices();Body->PostEditChange();Body->MarkPackageDirty();
  FSavePackageArgs SaveBody;SaveBody.TopLevelFlags=RF_Public|RF_Standalone;
  UPackage::SavePackage(Body->GetOutermost(),Body,*FPackageName::LongPackageNameToFilename(Body->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),SaveBody);
 }
 BuildMatchedActionAssets(TEXT("/Game/Fireline/DrazenOriginal"));
 // These clips are baked onto this exact rig; preserve all authored joint offsets.
 auto* NativeSkeleton=Mesh->GetSkeleton();
 for(int Bone=0;Bone<NativeSkeleton->GetReferenceSkeleton().GetNum();++Bone)NativeSkeleton->SetBoneTranslationRetargetingMode(Bone,EBoneTranslationRetargetingMode::Animation);
 NativeSkeleton->Modify();NativeSkeleton->MarkPackageDirty();
 const FString SettingsPath=TEXT("/Game/Fireline/DrazenOriginal/BC_Matched");
 UPackage* SettingsPackage=CreatePackage(*SettingsPath);
 auto* Compression=LoadObject<UAnimBoneCompressionSettings>(nullptr,*SettingsPath);
 if(!Compression)
 {
  Compression=NewObject<UAnimBoneCompressionSettings>(SettingsPackage,TEXT("BC_Matched"),RF_Public|RF_Standalone);
  UClass* Safe=LoadClass<UAnimBoneCompressionCodec>(nullptr,TEXT("/Script/ACLPlugin.AnimBoneCompressionCodec_ACLSafe"));check(Safe);
  Compression->Codecs.Reset();Compression->Codecs.Add(NewObject<UAnimBoneCompressionCodec>(Compression,Safe));
 }
 FSavePackageArgs Save;Save.TopLevelFlags=RF_Public|RF_Standalone;
 UPackage::SavePackage(SettingsPackage,Compression,*FPackageName::LongPackageNameToFilename(SettingsPath,FPackageName::GetAssetPackageExtension()),Save);
 const TCHAR* Clips[]={TEXT("M4_Idle"),TEXT("M4_Aim"),TEXT("M4_Reload"),TEXT("M4_Run"),TEXT("M4_Sprint"),TEXT("M4_Crouch"),TEXT("M4_CrouchMove"),TEXT("M4_Slide"),TEXT("M4_JumpRise"),TEXT("M4_JumpFall"),TEXT("M4_Land"),TEXT("Hands_Open"),TEXT("Hands_Fist"),TEXT("Hands_Test"),TEXT("Karambit_Hold"),TEXT("Karambit_Draw"),TEXT("Karambit_Inspect"),TEXT("Karambit_Slash")};
 for(const TCHAR* AnimationDir:{TEXT("DrazenOriginal")})
 for(const TCHAR* Clip:Clips)
 {
  auto* Seq=LoadObject<UAnimSequence>(nullptr,*FString::Printf(TEXT("/Game/Fireline/%s/A_%s.A_%s"),AnimationDir,Clip,Clip));if(!Seq)continue;
  Seq->Modify();Seq->BoneCompressionSettings=Compression;Seq->RetargetSource=NAME_None;Seq->SetRetargetSourceAsset(Mesh);Seq->UpdateRetargetSourceAssetData();
  auto& Controller=Seq->GetController();Controller.OpenBracket(FText::FromString(TEXT("Keep matched mechanical rotations")),false);
  TArray<FName> TrackNames;Seq->GetDataModel()->GetBoneTrackNames(TrackNames);
  for(FName Name:TrackNames)
  {
   if(!Name.ToString().StartsWith(TEXT("M4_"))||Name==TEXT("M4_body"))continue;
   const int I=Ref.FindBoneIndex(Name);if(I==INDEX_NONE)continue;
   TArray<FTransform> Keys;Seq->GetDataModel()->GetBoneTrackTransforms(Name,Keys);
   TArray<FVector3f> Positions,Scales;TArray<FQuat4f> Rotations;
   for(const FTransform& Key:Keys){Positions.Add(FVector3f(Key.GetTranslation()));Scales.Add(FVector3f(Key.GetScale3D()));Rotations.Add(FQuat4f(Ref.GetRefBonePose()[I].GetRotation()));}
   check(Controller.SetBoneTrackKeys(Name,Positions,Rotations,Scales,false));
  }
  Controller.CloseBracket(false);
  if(FString(Clip)==TEXT("M4_Idle"))UE_LOG(LogTemp,Display,TEXT("DRAZEN_RAW_MAG q=%s"),*Seq->GetDataModel()->GetBoneTrackTransform(TEXT("M4_magretainer"),FFrameNumber(0)).GetRotation().ToString());
  Seq->PostEditChange();Seq->MarkPackageDirty();
  UPackage::SavePackage(Seq->GetOutermost(),Seq,*FPackageName::LongPackageNameToFilename(Seq->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Save);
 }
 for(const TCHAR* Weapon:{TEXT("SK_M4Action"),TEXT("SK_Karambit")})
 {
  auto* View=LoadObject<USkeletalMesh>(nullptr,*FString::Printf(TEXT("/Game/Fireline/DrazenOriginal/%s.%s"),Weapon,Weapon));check(View);
  View->Modify();for(int LOD=0;LOD<View->GetLODNum();++LOD)View->GetLODInfo(LOD)->bAllowCPUAccess=true;
  View->PostEditChange();View->MarkPackageDirty();
  UPackage::SavePackage(View->GetOutermost(),View,*FPackageName::LongPackageNameToFilename(View->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Save);
 }
 UE_LOG(LogTemp,Display,TEXT("DRAZEN_MATCHED_COMPRESSION clips=18 rigid_rotations=imported_bind precision=ACL_safe"));
 Mesh->Modify();for(int I=0;I<Mesh->GetLODNum();++I)Mesh->GetLODInfo(I)->bAllowCPUAccess=true;
 Mesh->PostEditChange();Mesh->MarkPackageDirty();FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
 UPackage::SavePackage(Mesh->GetOutermost(),Mesh,*FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
#endif
}

void ARangeCharacter::InitializeKarambit()
{
 RifleBodyMesh=GetMesh()->GetSkeletalMeshAsset();RifleArmsMesh=Arms->GetSkeletalMeshAsset();
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineLegacyM9")))return;
 if(RifleBodyMesh&&RifleBodyMesh->GetPathName().StartsWith(TEXT("/Game/Fireline/DrazenOriginal/")))
 {
  KarambitBodyMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/DrazenOriginal/SK_DrazenKnife.SK_DrazenKnife"));
  KarambitArmsMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/DrazenOriginal/SK_DrazenKnifeArms.SK_DrazenKnifeArms"));
  ViewKnife->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/DrazenOriginal/SK_Karambit.SK_Karambit")));
  bKarambitReady=KarambitBodyMesh&&KarambitArmsMesh&&ViewKnife->GetSkeletalMeshAsset();
  BodyGun->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/DrazenOriginal/SK_M4Action.SK_M4Action")));
  BodyKnife->SetSkeletalMesh(LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/DrazenOriginal/SK_Karambit.SK_Karambit")));
  UE_LOG(LogTemp,Display,TEXT("FIRELINE_DRAZEN_PLAYER body=%s rifle_arms=%s knife_arms=%s"),*RifleBodyMesh->GetPathName(),*RifleArmsMesh->GetPathName(),KarambitArmsMesh?*KarambitArmsMesh->GetPathName():TEXT("MISSING"));
  ensureMsgf(bKarambitReady,TEXT("Selected Drazen player assets are missing"));
  return;
 }
 KarambitBodyMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Karambit/SK_KarambitBody.SK_KarambitBody"));
 KarambitArmsMesh=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Karambit/SK_KarambitArms.SK_KarambitArms"));
 // Isolated body study: keep accepted first-person arms, rigs and animations.
 if(FParse::Param(FCommandLine::Get(),TEXT("FirelineLightBody")))
 {
  auto* BodyStudy=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/LightBody/SK_LightBody.SK_LightBody"));
  auto* KnifeStudy=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/LightBody/SK_LightBodyKnife.SK_LightBodyKnife"));
  if(BodyStudy&&KnifeStudy)
  {
   RifleBodyMesh=BodyStudy;KarambitBodyMesh=KnifeStudy;GetMesh()->SetSkeletalMesh(BodyStudy);
   UE_LOG(LogTemp,Display,TEXT("FIRELINE_LIGHT_BODY study=true first_person_unchanged=true"));
  }
 }
 auto* Knife=LoadObject<USkeletalMesh>(nullptr,TEXT("/Game/Fireline/Karambit/SK_KrazzyElfKarambit.SK_KrazzyElfKarambit"));
 bKarambitReady=KarambitBodyMesh&&KarambitArmsMesh&&Knife;
 if(!bKarambitReady){UE_LOG(LogTemp,Error,TEXT("FIRELINE_KARAMBIT_MISSING selected A2 assets unavailable"));return;}
 ViewKnife->SetSkeletalMesh(Knife);BodyKnife->SetSkeletalMesh(Knife);
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_RYAN_PLAYER body=%s rifle_arms=%s knife_body=%s"),*RifleBodyMesh->GetPathName(),*RifleArmsMesh->GetPathName(),*KarambitBodyMesh->GetPathName());

}
void ARangeCharacter::UpdateWeaponMeshes()
{
 if(!bKarambitReady)return;
 auto* Body=bKnife?KarambitBodyMesh.Get():bMP7?MP7BodyMesh.Get():RifleBodyMesh.Get();
 auto* View=bKnife?KarambitArmsMesh.Get():bMP7?MP7ArmsMesh.Get():RifleArmsMesh.Get();
 auto* Gun=bMP7?MP7GunMesh.Get():RifleGunMesh.Get();
 if(!Body||!View||!Gun)return;
 if(GetMesh()->GetSkeletalMeshAsset()==Body&&Arms->GetSkeletalMeshAsset()==View&&ViewGun->GetSkeletalMeshAsset()==Gun)return;
 auto* OldBodyAnim=Cast<URangeAnimInstance>(GetMesh()->GetAnimInstance());
 auto Carry=OldBodyAnim?OldBodyAnim->CaptureGait():nullptr;
 GetMesh()->EmptyOverrideMaterials();Arms->EmptyOverrideMaterials();
 GetMesh()->SetSkeletalMesh(Body);Arms->SetSkeletalMesh(View);
 GetMesh()->SetAnimInstanceClass(URangeAnimInstance::StaticClass());Arms->SetAnimInstanceClass(URangeAnimInstance::StaticClass());
 if(auto* NewBodyAnim=Cast<URangeAnimInstance>(GetMesh()->GetAnimInstance())){NewBodyAnim->RestoreGait(Carry);if(Carry){GetMesh()->TickAnimation(0.f,false);GetMesh()->RefreshBoneTransforms();}}
 ViewGun->SetSkeletalMesh(Gun);BodyGun->SetSkeletalMesh(Gun);
 ViewGun->SetLeaderPoseComponent(Arms,true);BodyGun->SetLeaderPoseComponent(GetMesh(),true);
 ViewKnife->SetLeaderPoseComponent(Arms,true);BodyKnife->SetLeaderPoseComponent(GetMesh(),true);
 ViewOptic->AttachToComponent(ViewGun,FAttachmentTransformRules::KeepRelativeTransform,WeaponBone(TEXT("body")));
 BodyOptic->AttachToComponent(BodyGun,FAttachmentTransformRules::KeepRelativeTransform,WeaponBone(TEXT("body")));
 InitializeOptics();
 ApplyTeam(GetMesh(),false);ApplyTeam(Arms,false);
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_WEAPON_MESH slot=%d arms=%s"),WeaponSlot(),*View->GetPathName());
}
