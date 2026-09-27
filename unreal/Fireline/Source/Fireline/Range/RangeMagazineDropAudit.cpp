#include "RangeGame.h"
#include "RangeWeaponTiming.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"

void ARangeCharacter::StartMagazineDropAudit()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineMagazineDropAudit")))return;
 FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);
 const FString Out=FPaths::ProjectSavedDir()/TEXT("MagazineDropAudit");IFileManager::Get().MakeDirectory(*Out,true);
 struct FState{FString Checks;int Failures=0,Count=0,DropBefore=0;FVector M4Rear,M4Direction,DropStart;TWeakObjectPtr<AStaticMeshActor> Drop;};
 auto S=MakeShared<FState>();
 auto Check=[S](bool Pass,FString Name){++S->Count;S->Failures+=!Pass;S->Checks+=Name+(Pass?TEXT(": PASS\n"):TEXT(": FAIL\n"));UE_LOG(LogTemp,Display,TEXT("MAG_DROP_CHECK %s %s"),*Name,Pass?TEXT("PASS"):TEXT("FAIL"));};
 auto Later=[this](float T,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),T,false);};
 auto Capture=[Out](FString Name){FScreenshotRequest::RequestScreenshot(Out/(Name+TEXT(".png")),true,false);};
 auto Rear=[this]{return Camera->GetComponentTransform().InverseTransformPosition(ViewGun->GetSocketLocation(WeaponBone(TEXT("rearsight"))));};
 auto Bore=[this]{return Camera->GetComponentTransform().InverseTransformVectorNoScale(ViewGun->GetSocketLocation(WeaponBone(TEXT("frontsight")))-ViewGun->GetSocketLocation(WeaponBone(TEXT("rearsight")))).GetSafeNormal();};
 Later(.6f,[this]{Controller->SetControlRotation(FRotator::ZeroRotator);SetCoyoteSight(false);});
 Later(1.5f,[S,Rear,Bore,Capture]{S->M4Rear=Rear();S->M4Direction=Bore();Capture(TEXT("m4-hip"));});
 Later(1.8f,[this]{StartAim();});Later(2.2f,[this,Check,Capture]{Check(AimAlpha>.99f,TEXT("M4 iron aim remains available"));Capture(TEXT("m4-iron"));EndAim();EquipMP7();});
 Later(3.8f,[S,Check,Rear,Bore,Capture]{Check(FVector::Distance(S->M4Rear,Rear())<.1f,TEXT("M4 and MP7 hip rear-sight anchors match"));Check(S->M4Direction.Dot(Bore())>.99999f,TEXT("M4 and MP7 hip bore angles match"));Capture(TEXT("mp7-hip"));});
 for(int Case=0;Case<4;++Case)
 {
  const float T=4.5f+Case*3.7f;const bool Empty=Case%2==1;const FString Prefix=FString::Printf(TEXT("%s-%s"),Case<2?TEXT("fp"):TEXT("tp"),Empty?TEXT("empty"):TEXT("loaded"));
  Later(T,[this,S,Empty,Case]{if(Case==2){ToggleView();Controller->SetControlRotation(FRotator(-15,140,0));}Ammo=Empty?0:17;S->DropBefore=MagazineDropCount;Reload();});
  const float Drop=RangeWeaponTiming::MP7MagazineDrop(Empty),Reveal=RangeWeaponTiming::MP7MagazineReveal(Empty);
  Later(T+Drop-.06f,[this,Check,Prefix]{Check(!bMagazineDropped,Prefix+TEXT(" before release no physical duplicate"));});
  Later(T+Drop+.07f,[this,S,Check,Prefix]{
   S->Drop=LastDroppedMagazine;if(S->Drop.IsValid())S->DropStart=S->Drop->GetActorLocation();
   Check(bMagazineDropped&&MagazineDropCount==S->DropBefore+1&&S->Drop.IsValid(),Prefix+TEXT(" exactly one discarded actor"));
   Check(Arms->IsBoneHiddenByName(TEXT("MP7_mag"))&&GetMesh()->IsBoneHiddenByName(TEXT("MP7_mag")),Prefix+TEXT(" old animated magazine hidden in both views"));
   Check(S->Drop.IsValid()&&S->Drop->GetStaticMeshComponent()->IsSimulatingPhysics(),Prefix+TEXT(" old magazine simulates physics"));
  });
  Later(T+Reveal+.06f,[this,Check,Prefix]{Check(!Arms->IsBoneHiddenByName(TEXT("MP7_mag"))&&!GetMesh()->IsBoneHiddenByName(TEXT("MP7_mag")),Prefix+TEXT(" fetched replacement visible before seating"));Check(!bReloadAmmoCommitted,Prefix+TEXT(" fetch does not grant ammunition"));});
  Later(T+1.15f,[S,Check,Prefix]{Check(S->Drop.IsValid()&&S->Drop->GetActorLocation().Z<S->DropStart.Z-15,Prefix+TEXT(" discarded magazine falls independently"));});
  Later(T+2.9f,[this,S,Check,Capture,Prefix]{
   Check(!bReloading&&Ammo==40,Prefix+TEXT(" reload completes with forty rounds"));
   Check(MagazineDropCount==S->DropBefore+1,Prefix+TEXT(" no repeated drop on frame crossings"));
   if(S->Drop.IsValid()){
    auto* C=S->Drop->GetStaticMeshComponent();FHitResult Hit;FCollisionQueryParams Params;Params.AddIgnoredActor(this);Params.AddIgnoredActor(S->Drop.Get());const FVector P=C->Bounds.Origin;
    GetWorld()->LineTraceSingleByChannel(Hit,P,P-FVector(0,0,35),ECC_WorldStatic,Params);
    Check(Hit.bBlockingHit&&C->GetPhysicsLinearVelocity().Size()<20,Prefix+TEXT(" discarded magazine rests on collision surface"));
   }else Check(false,Prefix+TEXT(" discarded magazine persists on ground"));
   Capture(Prefix+TEXT("-complete"));
  });
  if(FParse::Param(FCommandLine::Get(),TEXT("FirelineMagazineDropCapture")))for(int I=0;I<29;++I)Later(T+.06f+I*.1f,[Capture,Prefix,I]{Capture(FString::Printf(TEXT("%s-%02d"),*Prefix,I));});
 }
 Later(19.5f,[this,S]{if(bThirdPerson)ToggleView();Controller->SetControlRotation(FRotator::ZeroRotator);Ammo=15;S->DropBefore=MagazineDropCount;Reload();});
 Later(19.65f,[this]{Fire();});
 Later(20,[this,S,Check]{Check(Ammo==14&&!bReloading&&MagazineDropCount==S->DropBefore,TEXT("early fire cancellation has no phantom drop"));Ammo=15;Reload();});
 Later(20.65f,[this]{Fire();});
 Later(21,[this,S,Check]{Check(Ammo==14&&!bReloading&&MagazineDropCount==S->DropBefore+1,TEXT("post-drop fire preserves old rounds without another discard"));Check(!Arms->IsBoneHiddenByName(TEXT("MP7_mag")),TEXT("fire interruption restores visible usable magazine"));Ammo=0;Reload();FApp::SetFixedDeltaTime(1./15.);});
 Later(22.7f,[this,Check]{Check(Ammo==40&&bReloadAmmoCommitted,TEXT("15 FPS empty reload commits at seating"));EquipRifle();});
 Later(24,[this,Check,S,Out]{Check(!bMP7&&MP7Ammo==40,TEXT("seated MP7 ammo survives weapon switch"));Check(FMath::IsNearlyEqual(RangeWeaponTiming::ReloadPlayRate(false),.85f),TEXT("M4 reload remains slowed"));FFileHelper::SaveStringToFile(S->Checks,*(Out/TEXT("checks.txt")));UE_LOG(LogTemp,Display,TEXT("MAG_DROP_AUDIT checks=%d failures=%d"),S->Count,S->Failures);FPlatformMisc::RequestExit(false);});
}
