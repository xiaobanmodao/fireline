#include "RangeGame.h"
#include "RangeAudio.h"
#include "RangeShotFX.h"
#include "Components/StaticMeshComponent.h"
#include "EngineUtils.h"
#include "Engine/DamageEvents.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
void ARangeTarget::StartShieldAudit()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineShieldAudit")))return;
 static bool Started=false;if(Started)return;Started=true;
 auto Later=[this](float T,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),T,false);};
 auto Check=[this](const TCHAR* Label,float H,float S,float Tolerance=.1f){const bool Pass=FMath::Abs(Vitals.Health-H)<Tolerance&&FMath::Abs(Vitals.Shield-S)<Tolerance;UE_LOG(LogTemp,Display,TEXT("SHIELD_CHECK %s pass=%d health=%.2f shield=%.2f"),Label,Pass,Vitals.Health,Vitals.Shield);checkf(Pass,TEXT("Shield audit failed: %s"),Label);};
 auto Shot=[this](int D){RegisterHit(D);ShieldImpact(Body->GetSocketLocation(TEXT("Chest")));};
 auto Capture=[](const TCHAR* Name){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/shield-%s.png"),Name),true,false);};
 Later(1,[this]{auto* P=Cast<ARangeCharacter>(UGameplayStatics::GetPlayerPawn(this,0));if(!P)return;if(P->bThirdPerson)P->ToggleView();P->SetActorLocation(GetActorLocation()+GetActorForwardVector()*320+FVector(0,0,5));P->GetController()->SetControlRotation((Body->GetSocketLocation(TEXT("Chest"))-P->Camera->GetComponentLocation()).Rotation());});
 Later(2,[this,Check,Capture]{Check(TEXT("full"),100,100);check(ShellMaterial&&ShieldShell->IsVisible()&&ShieldShell->GetCollisionEnabled()==ECollisionEnabled::NoCollision);Capture(TEXT("charged"));});
 Later(3,[Shot,Check]{for(int I=0;I<5;++I)Shot(18);Check(TEXT("five_body"),100,10);});
 Later(3.06f,[Capture]{Capture(TEXT("hit"));});
 Later(3.2f,[this]{auto* P=CastChecked<ARangeCharacter>(UGameplayStatics::GetPlayerPawn(this,0));P->SetActorLocation(GetActorLocation()+GetActorForwardVector()*160+FVector(0,0,5));P->GetController()->SetControlRotation((Body->GetSocketLocation(TEXT("Head"))-P->Camera->GetComponentLocation()).Rotation());});
 Later(3.3f,[Capture]{Capture(TEXT("hud-near"));});
 Later(3.5f,[this]{auto* P=CastChecked<ARangeCharacter>(UGameplayStatics::GetPlayerPawn(this,0));P->SetActorLocation(GetActorLocation()+GetActorForwardVector()*320+FVector(0,0,5));P->GetController()->SetControlRotation((Body->GetSocketLocation(TEXT("Chest"))-P->Camera->GetComponentLocation()).Rotation());});
 Later(4,[Shot,Check]{Shot(18);Check(TEXT("overflow_break"),92,0);});
 Later(4.06f,[this,Capture]{bool Found=false;for(TActorIterator<ARangeShieldBreakFX> It(GetWorld());It;++It)Found|=It->GetFragmentCount()==120;checkf(Found,TEXT("Full-shell break must cover all 120 facets"));Capture(TEXT("break"));});
 Later(4.7f,[this,Capture]{check(!ShieldShell->IsVisible());Capture(TEXT("empty"));});
 Later(5,[Shot,Check]{for(int I=0;I<5;++I)Shot(18);Check(TEXT("eleven_body_alive"),2,0);Shot(18);Check(TEXT("twelve_body_dead"),0,0);});
 Later(7.2f,[Check]{Check(TEXT("respawn"),100,100);});
 Later(7.5f,[Shot,Check]{Shot(27);Check(TEXT("head_not_lethal"),100,73);});
 Later(8.f,[this]{auto* P=CastChecked<ARangeCharacter>(UGameplayStatics::GetPlayerPawn(this,0));P->SetActorLocation(GetActorLocation()+GetActorForwardVector()*1200+FVector(0,0,5));P->GetController()->SetControlRotation((Body->GetSocketLocation(TEXT("Head"))-P->Camera->GetComponentLocation()).Rotation());});
 Later(8.1f,[Capture]{Capture(TEXT("hud-far"));});
 Later(9.f,[this]{auto* P=CastChecked<ARangeCharacter>(UGameplayStatics::GetPlayerPawn(this,0));P->SetActorLocation(GetActorLocation()+GetActorForwardVector()*320+FVector(0,0,5));P->GetController()->SetControlRotation((Body->GetSocketLocation(TEXT("Chest"))-P->Camera->GetComponentLocation()).Rotation());});
 Later(12.4f,[Check]{Check(TEXT("regen_wait"),100,73);});
 Later(13.5f,[this,Check]{Check(TEXT("regen_rate"),100,93,3);check(ShieldShell->IsVisible());});
 Later(14,[Shot]{Shot(18);});
 Later(14.1f,[this]{const float Before=Vitals.Shield;FTimerHandle H;GetWorldTimerManager().SetTimer(H,[this,Before]{checkf(FMath::IsNearlyEqual(Before,Vitals.Shield,.01f),TEXT("Damage must interrupt recharge"));UE_LOG(LogTemp,Display,TEXT("SHIELD_AUDIT_PASS recharge_interrupted=true material=%d"),ShieldMaterial!=nullptr);check(ShieldMaterial);},1,false);});
 Later(16,[this]{
  Vitals.Reset();auto* P=CastChecked<ARangeCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
  const FVector Point=Body->GetSocketLocation(TEXT("Chest"));P->GetController()->SetControlRotation((Point-P->Camera->GetComponentLocation()).Rotation());P->Fire();
 });
 Later(16.06f,[Capture]{Capture(TEXT("actual-shot"));});
 Later(16.2f,[Check]{Check(TEXT("actual_rifle_body"),100,82);});
 Later(17,[this]{
  Vitals.Shield=10;Vitals.Health=100;
  auto* P=CastChecked<ARangeCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
  P->GetController()->SetControlRotation((Body->GetSocketLocation(TEXT("Chest"))-P->Camera->GetComponentLocation()).Rotation());P->Fire();
 });
 Later(17.06f,[this,Capture]{bool Found=false;for(TActorIterator<ARangeShieldBreakFX> It(GetWorld());It;++It)Found|=It->GetFragmentCount()==120;check(Found);Capture(TEXT("actual-break"));});
 Later(17.2f,[Check]{Check(TEXT("actual_break"),92,0);});
 Later(18,[this]{auto* P=CastChecked<ARangeCharacter>(UGameplayStatics::GetPlayerPawn(this,0));P->GetController()->SetControlRotation((Body->GetSocketLocation(TEXT("Chest"))-P->Camera->GetComponentLocation()).Rotation());P->Fire();});
 Later(18.06f,[this,Capture]{bool Found=false;for(TActorIterator<ARangeShotFX> It(GetWorld());It;++It)Found|=It->HasBloodSprite();check(Found);UE_LOG(LogTemp,Display,TEXT("ENERGY_VFX_AUDIT_PASS shell_charged_hidden_recharged=1 blood_sprite=1"));Capture(TEXT("actual-flesh"));});
 Later(18.13f,[Capture]{Capture(TEXT("blood-middle"));});
 Later(18.25f,[Capture]{Capture(TEXT("blood-fade"));});
 Later(18.2f,[this,Check]{Check(TEXT("actual_flesh"),74,0);auto* P=CastChecked<ARangeCharacter>(UGameplayStatics::GetPlayerPawn(this,0));auto* A=P->CombatAudio.Get();check(A->LoadedCount()==12&&A->ShieldHitCount==1&&A->ShieldBreakCount==1&&A->FleshCount==1);UE_LOG(LogTemp,Display,TEXT("IMPACT_AUDIT_PASS loaded=12 shield=1 break=1 flesh=1"));});
 Later(19,[this]{auto* P=CastChecked<ARangeCharacter>(UGameplayStatics::GetPlayerPawn(this,0));FDamageEvent E;P->TakeDamage(108,E,nullptr,this);check(P->PlayerVitals.Shield==0&&P->PlayerVitals.Health==92);UE_LOG(LogTemp,Display,TEXT("SHIELD_PLAYER_CHECK pass=1 health=92 shield=0"));});
 Later(19.06f,[Capture]{Capture(TEXT("player-broken"));});
 // Optional visual matrix for the combat HUD. Equipment transitions and reload
 // use the real game paths; only starting vitals/ammunition are test fixtures.
 const bool HUDCapture=FParse::Param(FCommandLine::Get(),TEXT("FirelineHUDCapture"));
 if(HUDCapture)
 {
  auto Player=[this]{return CastChecked<ARangeCharacter>(UGameplayStatics::GetPlayerPawn(this,0));};
  Later(20.2f,[Player]{auto* P=Player();P->PlayerVitals.Reset();FDamageEvent E;P->TakeDamage(37,E,nullptr,nullptr);});
  Later(20.3f,[Capture]{Capture(TEXT("hud-partial"));});
  Later(21.2f,[Player]{auto* P=Player();FDamageEvent E;P->TakeDamage(143,E,nullptr,nullptr);P->Ammo=2;});
  Later(21.3f,[Capture]{Capture(TEXT("hud-low"));});
  Later(22.2f,[Player]{auto* P=Player();P->Ammo=0;P->Reload();});
  Later(22.5f,[Capture]{Capture(TEXT("hud-empty-reload"));});
  Later(23.4f,[Capture]{Capture(TEXT("hud-magazine-seated"));});
  Later(24.7f,[Player]{Player()->EquipMP7();});
  Later(26.2f,[Capture]{Capture(TEXT("hud-mp7"));});
  Later(26.5f,[Player]{Player()->EquipKnife();});
  Later(28.5f,[Capture]{Capture(TEXT("hud-knife"));});
  Later(29.f,[Player]{Player()->ToggleEmptyHands();});
  Later(30.2f,[Capture]{Capture(TEXT("hud-unarmed"));});
 }
 Later(HUDCapture?31.f:20.f,[]{UE_LOG(LogTemp,Display,TEXT("SHIELD_FULL_AUDIT_PASS"));FPlatformMisc::RequestExit(false);});
}
