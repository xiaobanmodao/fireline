#include "RangeGame.h"
#include "RangePlayerController.h"
#include "RangeCasingFX.h"
#include "Camera/CameraComponent.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "TimerManager.h"
#include "InputKeyEventArgs.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"

void ARangeCharacter::StartWeaponHandlingAudit()
{
 if(!FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponHandlingAudit")))return;
 FApp::SetUseFixedTimeStep(true);FApp::SetFixedDeltaTime(1./60.);ShotSpreadRandom.Initialize(120922);
 struct FState
 {
  int Checks=0,Failures=0,Phase=0,LastShots=0;
  float FirstCone[5]={},LastCone[5]={},MaxPitch[5]={},MaxYaw[5]={},MaxError[5]={};
  int Counts[5]={};
  FString Text,ShotsCSV=TEXT("t,weapon,phase,shot,cone_deg,offset_y_deg,offset_z_deg,pitch_deg,yaw_deg,impact_x,impact_y,impact_z\n");
  FString Frames=TEXT("t,weapon,phase,cone_deg,pitch_deg,yaw_deg,aim,bloom\n");
 };
 auto S=MakeShared<FState>();const FString Out=FPaths::ProjectSavedDir()/TEXT("WeaponHandlingAudit");IFileManager::Get().MakeDirectory(*Out,true);
 auto Check=[S](bool Pass,const TCHAR* Name){++S->Checks;S->Failures+=!Pass;S->Text+=FString(Name)+(Pass?TEXT(": PASS\n"):TEXT(": FAIL\n"));UE_LOG(LogTemp,Display,TEXT("HANDLING_CHECK %s pass=%d"),Name,Pass);};
 auto Later=[this](float T,TFunction<void()> Fn){FTimerHandle H;GetWorldTimerManager().SetTimer(H,MoveTemp(Fn),T,false);};
 auto Key=[this](FKey K,bool Down){CastChecked<ARangePlayerController>(Controller)->InjectGameplayAuditKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Down?IE_Pressed:IE_Released,FPlatformTime::Cycles64()));};
 auto Capture=[Out](const TCHAR* Name){if(FParse::Param(FCommandLine::Get(),TEXT("FirelineWeaponHandlingCapture")))FScreenshotRequest::RequestScreenshot(Out/(FString(Name)+TEXT(".png")),true,false);};

 // Distribution bounds/centroid and rotational invariance, independent of the
 // runtime visual sample count. Test the whole disk, not just profile constants.
 FRandomStream R(8192),Rotated(8192);FVector2D Mean=FVector2D::ZeroVector;float MaxRadius=0;double Moment=0,FrameError=0;
 const FQuat Q=FRotator(48,127,-18).Quaternion();
 for(int I=0;I<4096;++I)
 {
  const FVector V=RangeHandling::SpreadDirection(FQuat::Identity,1.f,R),W=RangeHandling::SpreadDirection(Q,1.f,Rotated);
  const FVector2D Disk(V.Y/V.X,V.Z/V.X);const float Radius=Disk.Size()/FMath::Tan(FMath::DegreesToRadians(1.f));
  Mean+=Disk;Moment+=Radius*Radius;MaxRadius=FMath::Max(MaxRadius,Radius);FrameError=FMath::Max(FrameError,FVector::Distance(W,Q.RotateVector(V)));
 }
 Check(MaxRadius<=1.0001f&&MaxRadius>.99f,TEXT("spread samples stay inside a circular cone"));
 Check((Mean/4096).Size()<.0005&&FMath::Abs(Moment/4096-.5)<.025,TEXT("spread has no directional or radial bias"));
 Check(FrameError<1.e-5,TEXT("spread follows arbitrary camera pitch yaw and roll"));
 RangeHandling::FState Fine,Coarse;Fine.Bloom=Coarse.Bloom=.6f;Fine.QuietTime=Coarse.QuietTime=0;
 for(int I=0;I<18;++I)Fine.Tick(1.f/60,RangeHandling::Rifle);
 for(int I=0;I<6;++I)Coarse.Tick(1.f/20,RangeHandling::Rifle);
 Check(FMath::Abs(Fine.Bloom-Coarse.Bloom)<1.e-5,TEXT("recovery delay crossing is frame-rate independent"));

 Later(1,[this,S]{Controller->SetControlRotation(FRotator(0,30,0));S->Phase=0;});
 Later(1.4f,[Capture]{Capture(TEXT("01-m4-rest"));});
 Later(1.8f,[this,Check]{const int Before=Shots;const float Bloom=WeaponHandling[0].Bloom;const int Seed=ShotSpreadRandom.GetCurrentSeed();FireCooldown=.2f;Fire();Check(Shots==Before&&WeaponHandling[0].Bloom==Bloom&&ShotSpreadRandom.GetCurrentSeed()==Seed,TEXT("blocked fire consumes neither bloom nor spread randomness"));});
 Later(2.1f,[Key]{Key(EKeys::LeftMouseButton,true);});
 Later(2.18f,[Capture]{Capture(TEXT("02-m4-first-shot"));});
 Later(3.5f,[Capture]{Capture(TEXT("03-m4-burst"));});
 Later(4.05f,[this,Key,Check,S]{Key(EKeys::LeftMouseButton,false);Check(S->Counts[0]>=16&&S->FirstCone[0]<.36f&&S->LastCone[0]>.9f&&S->LastCone[0]<=.951f,TEXT("M4 actual held fire grows from accurate hip shot to bounded bloom"));});
 Later(4.8f,[this,Check,Capture]{Check(FMath::IsNearlyEqual(GetHipSpreadAngle(),.35f,.001f)&&RecoilPitch<.03f&&FMath::Abs(RecoilYaw)<.01f,TEXT("M4 stops firing and settles to base accuracy and original view"));Capture(TEXT("04-m4-recovered"));});
 Later(5,[Key]{Key(EKeys::RightMouseButton,true);});
 Later(5.4f,[this,S,Key]{S->Phase=1;Ammo=30;Key(EKeys::LeftMouseButton,true);});
 Later(6.3f,[Capture]{Capture(TEXT("05-m4-ads-burst"));});
 Later(6.6f,[this,S,Key,Check]{Key(EKeys::LeftMouseButton,false);Check(S->Counts[1]>=9&&S->MaxError[1]<.001&&WeaponHandling[0].Bloom>.4f,TEXT("M4 ADS keeps sight-ray precision while burst heat still builds"));Key(EKeys::RightMouseButton,false);});
 Later(6.9f,[this,Check]{const float Before=WeaponHandling[1].Bloom;EquipMP7();Check(Before==0&&WeaponHandling[1].Bloom==0,TEXT("M4 firing does not heat the MP7"));});
 Later(8.4f,[this,S,Capture]{S->Phase=2;Capture(TEXT("06-mp7-rest"));});
 Later(8.6f,[Key]{Key(EKeys::LeftMouseButton,true);});
 Later(8.67f,[Capture]{Capture(TEXT("07-mp7-first-shot"));});
 Later(9.9f,[Capture]{Capture(TEXT("08-mp7-burst"));});
 Later(10.5f,[Key,S,Check]{Key(EKeys::LeftMouseButton,false);Check(S->Counts[2]>=29&&S->FirstCone[2]<.29f&&S->LastCone[2]>1.15f&&S->LastCone[2]<=1.201f,TEXT("MP7 has tighter opening hip shot but wider sustained bloom"));});
 Later(11.2f,[this,Check,Capture]{Check(FMath::IsNearlyEqual(GetHipSpreadAngle(),.28f,.001f)&&RecoilPitch<.03f,TEXT("MP7 recovers its original accuracy and view after release"));Capture(TEXT("09-mp7-recovered"));});
 Later(11.4f,[Key]{Key(EKeys::RightMouseButton,true);});
 Later(11.8f,[this,S,Key]{S->Phase=3;Ammo=40;Key(EKeys::LeftMouseButton,true);});
 Later(12.6f,[Capture]{Capture(TEXT("10-mp7-ads-burst"));});
 Later(13,[this,Key,S,Check]{Key(EKeys::LeftMouseButton,false);Check(S->Counts[3]>=18&&S->MaxError[3]<.001&&WeaponHandling[1].Bloom>.8f,TEXT("MP7 ADS has no added random spread during a sustained burst"));Key(EKeys::RightMouseButton,false);});
 Later(13.7f,[this,Check,S]{
  Check(S->MaxPitch[0]>.6f&&S->MaxPitch[0]<1.9f&&S->MaxPitch[2]>.4f&&S->MaxPitch[2]<1.6f,TEXT("both automatic weapons have visible but bounded light recoil"));
  Check(S->MaxPitch[0]>S->MaxPitch[2]&&S->MaxPitch[1]<S->MaxPitch[0]&&S->MaxPitch[3]<S->MaxPitch[2],TEXT("weapon recoil differs and ADS recoil is gentler"));
  Check(FMath::Max(S->MaxYaw[0],S->MaxYaw[2])<.15f,TEXT("horizontal recoil avoids large sideways kicks"));
  Check(Controller->GetControlRotation().Equals(FRotator(0,30,0),.001f),TEXT("recoil never rewrites raw mouse controller rotation"));
  Check(CasingFX->Ejected==Shots,TEXT("spread does not change shot ammo or casing cadence"));
 });
 Later(14,[this,S,Key]{S->Phase=4;Ammo=1;Key(EKeys::LeftMouseButton,true);});
 Later(14.3f,[this,Key,Check]{Key(EKeys::LeftMouseButton,false);Check(Ammo==0&&bReloading&&bEmptyReload,TEXT("last round still enters automatic empty reload"));});
 Later(15.5f,[this,Check]{Check(Ammo==40&&bReloadAmmoCommitted,TEXT("automatic reload still commits ammo at insertion"));});
 Later(17,[this,Check,S,Out]{
  Check(!bReloading&&Ammo==40,TEXT("empty reload completes after the spread burst"));
  FFileHelper::SaveStringToFile(S->Text,*(Out/TEXT("checks.txt")));FFileHelper::SaveStringToFile(S->ShotsCSV,*(Out/TEXT("shots.csv")));FFileHelper::SaveStringToFile(S->Frames,*(Out/TEXT("frames.csv")));
  UE_LOG(LogTemp,Display,TEXT("WEAPON_HANDLING_AUDIT checks=%d failures=%d shots=%d m4_hip_peak=%.4f mp7_hip_peak=%.4f"),S->Checks,S->Failures,Shots,S->MaxPitch[0],S->MaxPitch[2]);FPlatformMisc::RequestExit(false);
 });
 FWorldDelegates::OnWorldPostActorTick.AddWeakLambda(this,[this,S](UWorld* W,ELevelTick,float){
  if(W!=GetWorld())return;const int I=S->Phase;
  S->MaxPitch[I]=FMath::Max(S->MaxPitch[I],RecoilPitch);S->MaxYaw[I]=FMath::Max(S->MaxYaw[I],FMath::Abs(RecoilYaw));
  S->Frames+=FString::Printf(TEXT("%.4f,%d,%d,%.6f,%.6f,%.6f,%.4f,%.4f\n"),GetGameTimeSinceCreation(),WeaponSlot(),I,GetShotSpreadAngle(),RecoilPitch,RecoilYaw,AimAlpha,WeaponHandling[bMP7?1:0].Bloom);
  if(Shots==S->LastShots)return;S->LastShots=Shots;
  if(S->Counts[I]++==0)S->FirstCone[I]=LastShotSpread;S->LastCone[I]=LastShotSpread;
  const FVector Local=FRotationMatrix(LastShotAim.Rotation()).GetTransposed().TransformVector(LastShotDirection);
  const float Y=FMath::RadiansToDegrees(FMath::Atan2(Local.Y,Local.X)),Z=FMath::RadiansToDegrees(FMath::Atan2(Local.Z,Local.X));
  S->MaxError[I]=FMath::Max(S->MaxError[I],FMath::Max(FMath::Abs(Y),FMath::Abs(Z)));
  S->ShotsCSV+=FString::Printf(TEXT("%.4f,%d,%d,%d,%.6f,%.6f,%.6f,%.6f,%.6f,%.4f,%.4f,%.4f\n"),GetGameTimeSinceCreation(),WeaponSlot(),I,Shots,LastShotSpread,Y,Z,RecoilPitch,RecoilYaw,LastShotEnd.X,LastShotEnd.Y,LastShotEnd.Z);
 });
}
