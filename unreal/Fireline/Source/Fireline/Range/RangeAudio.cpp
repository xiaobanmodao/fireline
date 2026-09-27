#include "RangeAudio.h"
#include "RangeWeaponTiming.h"
#include "RangeGame.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundAttenuation.h"
#include "Engine/World.h"

void URangeAudio::BeginPlay()
{
 Super::BeginPlay();
 for(const TCHAR* Name:{TEXT("S_M4_Shot"),TEXT("S_Headshot"),TEXT("S_Hit"),TEXT("S_Cover"),TEXT("S_Flyby"),TEXT("S_MagOut"),TEXT("S_MagIn"),TEXT("S_BoltPull"),TEXT("S_BoltRelease"),TEXT("S_FleshImpact"),TEXT("S_ShieldHit"),TEXT("S_ShieldBreak")})
 {
  const FString Path=FString::Printf(TEXT("/Game/Fireline/Audio/%s.%s"),Name,Name);
  if(auto* Sound=LoadObject<USoundBase>(nullptr,*Path))Sounds.Add(FName(Name),Sound);
  else UE_LOG(LogTemp,Error,TEXT("FIRELINE_AUDIO_MISSING %s"),*Path);
 }
 for(const TCHAR* Name:{TEXT("S_M9_Draw"),TEXT("S_M9_Swing")})
  if(auto* Sound=LoadObject<USoundBase>(nullptr,*FString::Printf(TEXT("/Game/Fireline/M9/Audio/%s.%s"),Name,Name)))KnifeSounds.Add(FName(Name),Sound);
 auto Concurrency=[this](int Count)
 {
  auto* C=NewObject<USoundConcurrency>(this);
  C->Concurrency.MaxCount=Count;C->Concurrency.ResolutionRule=EMaxConcurrentResolutionRule::StopOldest;
  C->Concurrency.VoiceStealReleaseTime=.015f;return C;
 };
 GunConcurrency=Concurrency(6);FeedbackConcurrency=Concurrency(8);
 WorldConcurrency=Concurrency(12);ReloadConcurrency=Concurrency(2);
 WorldAttenuation=NewObject<USoundAttenuation>(this);
 auto& A=WorldAttenuation->Attenuation;
 A.bAttenuate=true;A.bSpatialize=true;
 A.AttenuationShape=EAttenuationShape::Sphere;A.AttenuationShapeExtents=FVector(180);
 A.FalloffDistance=4200;A.DistanceAlgorithm=EAttenuationDistanceModel::NaturalSound;
 UE_LOG(LogTemp,Display,TEXT("FIRELINE_AUDIO_READY loaded=%d/12"),Sounds.Num());
}
void URangeAudio::Local(FName Name,float Gain,USoundConcurrency* Concurrency,float Pitch)
{
 if(auto* S=Sounds.Find(Name))UGameplayStatics::PlaySound2D(this,*S,Gain,Pitch,0,Concurrency,GetOwner(),false);
}
void URangeAudio::World(FName Name,const FVector& Location,float Gain,float Pitch)
{
 if(auto* S=Sounds.Find(Name))UGameplayStatics::PlaySoundAtLocation(this,*S,Location,FRotator::ZeroRotator,Gain,Pitch,0,WorldAttenuation,WorldConcurrency,GetOwner());
}
void URangeAudio::Shot(const FVector& Muzzle,const FVector& Impact,bool ThirdPerson,bool Cover,bool Hit,bool Headshot,bool Shield,bool Broken)
{
 ++ShotCount;
 const float Pitch=FMath::FRandRange(.985f,1.015f);
 // Local gun reports retain their attack in FPS; world impacts carry direction/distance.
 if(ThirdPerson)World(TEXT("S_M4_Shot"),Muzzle,.8f,Pitch);
 else Local(TEXT("S_M4_Shot"),.8f,GunConcurrency,Pitch);
 if(Hit)
 {
  ImpactFeedback(Headshot,Shield,Broken);
 }
 else if(Cover){++CoverCount;World(TEXT("S_Cover"),Impact,.7f,FMath::FRandRange(.96f,1.04f));}
}
void URangeAudio::ReloadPhase(float Previous,float Current,bool MP7,bool Empty)
{
 struct Event{float Time;const TCHAR* Name;float Gain;};
 if(MP7)
 {
  for(const Event& E:{Event{Empty?.24f:.38f,TEXT("S_MagOut"),.7f},Event{RangeWeaponTiming::MagazineSeat(true,Empty),TEXT("S_MagIn"),.8f},Event{1.55f,TEXT("S_BoltPull"),.7f},Event{1.8f,TEXT("S_BoltRelease"),.75f}})
   if((Empty||E.Time<1.5f)&&Previous<E.Time&&Current>=E.Time){++ReloadCount;Local(E.Name,E.Gain,ReloadConcurrency);}
  return;
 }
 // Match the authored timeline. Compare crossings so low frame rates cannot skip sounds.
 for(const Event& E:{Event{RangeWeaponTiming::M4MagazineDrop,TEXT("S_MagOut"),.8f},Event{RangeWeaponTiming::MagazineSeat(false,Empty),TEXT("S_MagIn"),.85f},Event{75.f/60.f,TEXT("S_BoltPull"),.8f},Event{90.f/60.f,TEXT("S_BoltRelease"),.85f}})
  if((Empty||E.Time<75.f/60.f)&&Previous<E.Time&&Current>=E.Time){++ReloadCount;Local(E.Name,E.Gain,ReloadConcurrency);}
}
bool URangeAudio::Flyby(const FVector& Start,const FVector& End,const AActor* Shooter)
{
 auto* Player=Cast<ARangeCharacter>(GetOwner());
 if(!Player||Shooter==Player||GetWorld()->GetTimeSeconds()-LastFlyby<.12f)return false;
 const FVector Ear=Player->Camera->GetComponentLocation(),Line=End-Start;
 const float Size=Line.SizeSquared();if(Size<1)return false;
 const float T=FVector::DotProduct(Ear-Start,Line)/Size;
 if(T<=0||T>=1)return false;
 const FVector Nearest=Start+T*Line;
 const float Distance=FVector::Distance(Ear,Nearest);
 if(Distance<25||Distance>220)return false;
 LastFlyby=GetWorld()->GetTimeSeconds();++FlybyCount;
 World(TEXT("S_Flyby"),Nearest,.45f);return true;
}

void URangeAudio::MeleeImpact(const FVector& Impact,bool Hit,bool Cover,bool Shield,bool Broken)
{
 if(Hit)ImpactFeedback(false,Shield,Broken);
 else if(Cover)World(TEXT("S_Cover"),Impact,.5f);
}

void URangeAudio::KnifePhase(int32 Action,float Previous,float Current,bool Karambit)
{
 auto Play=[this,Previous,Current](float Time,FName Name,float Gain,float Pitch)
 {
  if(Previous<Time&&Current>=Time)if(auto* Sound=KnifeSounds.Find(Name))
   UGameplayStatics::PlaySound2D(this,*Sound,Gain,Pitch,0,ReloadConcurrency,GetOwner(),false);
 };
 if(Karambit&&Action==2)
 {
  Play(.20f,TEXT("S_M9_Swing"),.16f,1.2f);
  for(float Time:{1.63056f,2.04722f,2.46389f,2.88056f,3.29722f,3.71389f,4.13056f})Play(Time,TEXT("S_M9_Swing"),.16f,1.15f);
 }
 if(Action==1){Play(.10f,TEXT("S_M9_Draw"),.65f,1.05f);Play(.19f,TEXT("S_M9_Swing"),.24f,1.25f);}
 if(Action==3)Play(.10f,TEXT("S_M9_Swing"),.8f,1.1f);
}

void URangeAudio::ImpactFeedback(bool Headshot,bool Shield,bool Broken)
{
 // Exactly one surface cue: a break replaces the normal shield hit, even with overflow damage.
 const FName Cue=Shield?(Broken?TEXT("S_ShieldBreak"):TEXT("S_ShieldHit")):TEXT("S_FleshImpact");
 if(Shield){if(Broken)++ShieldBreakCount;else ++ShieldHitCount;}else ++FleshCount;
 Local(Cue,Broken?1.f:.82f,FeedbackConcurrency,FMath::FRandRange(.98f,1.02f));
 if(Headshot){++HeadCount;Local(TEXT("S_Headshot"),.75f,FeedbackConcurrency);}else ++HitCount;
 UE_LOG(LogTemp,Display,TEXT("IMPACT_FEEDBACK cue=%s head=%d shield=%d break=%d"),*Cue.ToString(),Headshot,Shield,Broken);
}
