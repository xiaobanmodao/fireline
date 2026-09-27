#include "RangeGame.h"
#include "RangePlayerController.h"
#include "RangeCasingFX.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"

void ARangeCharacter::StartMovementSpreadAudit()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineMovementSpreadAudit")))return;
 FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);ShotSpreadRandom.Initialize(220922);
 struct FState{int Checks=0,Failures=0,LastShots=0;float Walk[2]={};FString ChecksText,Frames=TEXT("time,weapon,speed,vz,hip,shot_cone,bloom,aim,sliding\n"),ShotsText=TEXT("time,weapon,cone,angular_error\n");TWeakObjectPtr<AStaticMeshActor> Wall;};
 auto S=MakeShared<FState>();const FString Out=FPaths::ProjectSavedDir()/TEXT("MovementSpreadAudit");IFileManager::Get().MakeDirectory(*Out,true);
 auto Check=[S](bool Pass,const TCHAR* Name){++S->Checks;S->Failures+=!Pass;S->ChecksText+=FString(Name)+(Pass?TEXT(": PASS\n"):TEXT(": FAIL\n"));UE_LOG(LogTemp,Display,TEXT("MOVEMENT_SPREAD_CHECK %s pass=%d"),Name,Pass);};
 auto Later=[this](float T,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),T,false);};
 auto Key=[this](FKey K,bool Down){CastChecked<ARangePlayerController>(Controller)->InjectGameplayAuditKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));};
 auto Capture=[Out](FString Name){if(FParse::Param(FCommandLine::Get(),TEXT("FirelineMovementSpreadCapture")))FScreenshotRequest::RequestScreenshot(Out/(Name+TEXT(".png")),true,false);};
 auto Cube=[this](FVector Position,FVector Scale){auto* A=GetWorld()->SpawnActor<AStaticMeshActor>();auto* C=A->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));C->SetCollisionProfileName(TEXT("BlockAll"));A->SetActorLocation(Position);A->SetActorScale3D(Scale);return A;};
 Cube(FVector(-6000,0,-25),FVector(100,100,.5));
 auto Reset=[this,Key]{for(FKey K:{EKeys::W,EKeys::D,EKeys::C,EKeys::LeftShift,EKeys::LeftMouseButton,EKeys::RightMouseButton})Key(K,false);GetCharacterMovement()->StopMovementImmediately();SetActorLocation(FVector(-6000,0,95));Controller->SetControlRotation(FRotator::ZeroRotator);};
 for(int Gun=0;Gun<2;++Gun)
 {
  const float T=1+Gun*10.f;
  Later(T,[this,Reset,Gun]{Reset();if(Gun)EquipMP7();else EquipRifle();});
  Later(T+.5f,[this,Check,Gun]{Check(FMath::IsNearlyEqual(GetHipSpreadAngle(),Gun?.28f:.35f,.001f),TEXT("stationary accuracy remains unchanged"));});
  Later(T+.6f,[Key]{Key(EKeys::W,true);});
  Later(T+1.2f,[this,S,Gun,Check,Key,Capture]{S->Walk[Gun]=GetHipSpreadAngle();Check(FMath::Abs(GetVelocity().Size2D()-450)<1&&S->Walk[Gun]>(Gun?.49f:.69f),TEXT("real walking speed widens the actual cone"));Capture(FString::Printf(TEXT("%d-walk"),Gun));Key(EKeys::LeftMouseButton,true);});
  Later(T+1.7f,[this,Gun,Check,Key]{Key(EKeys::LeftMouseButton,false);Check(LastShotSpread>(Gun?.7f:.8f),TEXT("walking fire combines movement and sustained bloom"));});
  Later(T+1.8f,[Key]{Key(EKeys::D,true);});
  Later(T+2.2f,[this,S,Gun,Check,Key]{Check(FMath::Abs(GetVelocity().Size2D()-450)<1&&FMath::Abs(GetHipSpreadAngle()-S->Walk[Gun])<.002f,TEXT("diagonal movement has no extra speed or spread"));Key(EKeys::D,false);Key(EKeys::LeftShift,true);});
  Later(T+2.8f,[this,Gun,Check,Key]{Check(GetVelocity().Size2D()>695&&GetHipSpreadAngle()>(Gun?.63f:.91f),TEXT("sprint speed adds more spread than walking"));Key(EKeys::C,true);});
  Later(T+3.f,[this,Gun,Check,Key,Capture]{Check(bSliding&&GetVelocity().Size2D()>500&&GetHipSpreadAngle()<=(Gun?.661f:.951f),TEXT("slide uses its real speed with a bounded movement penalty"));Capture(FString::Printf(TEXT("%d-slide"),Gun));Key(EKeys::LeftMouseButton,true);});
  Later(T+3.5f,[Reset]{Reset();});
  Later(T+4.2f,[Key]{Key(EKeys::C,true);Key(EKeys::W,true);});
  Later(T+5.f,[this,Check,Gun,S,Key]{Check(bIsCrouched&&GetVelocity().Size2D()<181&&GetHipSpreadAngle()<S->Walk[Gun]&&GetHipSpreadAngle()>(Gun?.32f:.42f),TEXT("slow crouch movement has a smaller penalty"));Key(EKeys::W,false);Key(EKeys::C,false);});
  Later(T+5.5f,[this,Check,Gun,Key]{Check(GetVelocity().Size2D()<.1f&&FMath::IsNearlyEqual(GetHipSpreadAngle(),Gun?.28f:.35f,.001f),TEXT("stopping restores movement accuracy without residual lag"));Key(EKeys::W,true);Key(EKeys::RightMouseButton,true);});
  Later(T+6.1f,[this,Check,Key]{Check(GetVelocity().Size2D()>249&&GetVelocity().Size2D()<251&&GetShotSpreadAngle()<.0001f,TEXT("moving fully aimed fire preserves sight precision"));Key(EKeys::LeftMouseButton,true);});
  Later(T+6.5f,[Reset]{Reset();});
  Later(T+6.8f,[this,S,Cube,Key]{S->Wall=Cube(GetActorLocation()+FVector(95,0,0),FVector(.5,4,4));Key(EKeys::W,true);});
  Later(T+7.4f,[this,Check,Gun,Key,S]{Check(GetVelocity().Size2D()<1&&FMath::IsNearlyEqual(GetHipSpreadAngle(),Gun?.28f:.35f,.002f),TEXT("held forward against a wall does not widen spread"));Key(EKeys::W,false);if(S->Wall.IsValid())S->Wall->Destroy();});
  Later(T+7.7f,[Key]{Key(EKeys::SpaceBar,true);});
  Later(T+7.85f,[this,Check,Gun,Key]{Check(FMath::Abs(GetVelocity().Z)>100&&GetVelocity().Size2D()<1&&FMath::IsNearlyEqual(GetHipSpreadAngle(),Gun?.28f:.35f,.002f),TEXT("vertical jump velocity is not mistaken for horizontal speed"));Key(EKeys::SpaceBar,false);});
 }
 Later(20,[this,S,Check]{Check(S->Walk[0]-.35f>S->Walk[1]-.28f,TEXT("MP7 is more accurate on the move than M4 for equal speed"));Check(CasingFX->Ejected==Shots&&Shots>20,TEXT("moving fire still ejects one spent case per emitted round"));});
 Later(21,[this,Reset,Check]{
  Reset();
  // Enlarged inspection of the actual imported runtime assets, separate from
  // normal-size gameplay. Both mouth and side silhouette must be visible.
  for(int I=0;I<2;++I)
  {
   const FString Path=I?TEXT("/Game/Fireline/Effects/Casings/SM_Casing46.SM_Casing46"):TEXT("/Game/Fireline/Effects/Casings/SM_Casing556.SM_Casing556");
   auto* Mesh=LoadObject<UStaticMesh>(nullptr,*Path);Check(Mesh&&Mesh->GetStaticMaterials().Num()==2,TEXT("spent casing imports retain separate dark interior material"));if(!Mesh)continue;
   auto* A=GetWorld()->SpawnActor<AStaticMeshActor>();auto* C=A->GetStaticMeshComponent();C->SetMobility(EComponentMobility::Movable);C->SetStaticMesh(Mesh);C->SetCollisionEnabled(ECollisionEnabled::NoCollision);C->SetCastShadow(false);
   const FVector Extent=Mesh->GetBounds().BoxExtent;const FVector Axis=Extent.X>=Extent.Y&&Extent.X>=Extent.Z?FVector::ForwardVector:Extent.Y>=Extent.Z?FVector::RightVector:FVector::UpVector;
   A->SetActorScale3D(FVector(9));A->SetActorLocation(Camera->GetComponentLocation()+FVector(65,(I?19:-19),14));A->SetActorRotation(FQuat::FindBetweenNormals(Axis,FVector(-.72,.66,.22).GetSafeNormal()));
  }
 });
 Later(21.6f,[Capture]{Capture(TEXT("imported-empty-case-closeup-9x"));});
 Later(22,[this,S,Out]{FFileHelper::SaveStringToFile(S->ChecksText,*(Out/TEXT("checks.txt")));FFileHelper::SaveStringToFile(S->Frames,*(Out/TEXT("frames.csv")));FFileHelper::SaveStringToFile(S->ShotsText,*(Out/TEXT("shots.csv")));UE_LOG(LogTemp,Display,TEXT("MOVEMENT_SPREAD_AUDIT checks=%d failures=%d shots=%d"),S->Checks,S->Failures,Shots);FPlatformMisc::RequestExit(false);});
 FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this,S,Check](UWorld* W,ELevelTick,float){
  if(W!=GetWorld())return;
  S->Frames+=FString::Printf(TEXT("%.5f,%d,%.5f,%.5f,%.6f,%.6f,%.6f,%.6f,%d\n"),GetGameTimeSinceCreation(),WeaponSlot(),GetVelocity().Size2D(),GetVelocity().Z,GetHipSpreadAngle(),GetShotSpreadAngle(),WeaponHandling[bMP7?1:0].Bloom,AimAlpha,bSliding);
  if(Shots==S->LastShots)return;S->LastShots=Shots;
  const float Error=FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(LastShotAim.Dot(LastShotDirection),-1.,1.)));
  if(Error>LastShotSpread+.002f)Check(false,TEXT("actual moving shot outside reported cone"));
  S->ShotsText+=FString::Printf(TEXT("%.5f,%d,%.6f,%.6f\n"),GetGameTimeSinceCreation(),WeaponSlot(),LastShotSpread,Error);
 });
}
