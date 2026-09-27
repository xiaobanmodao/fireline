#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "RangeAudio.generated.h"

class USoundBase;
class USoundConcurrency;
class USoundAttenuation;

UCLASS()
class FIRELINE_API URangeAudio : public UActorComponent
{
 GENERATED_BODY()
public:
 virtual void BeginPlay() override;
 void Shot(const FVector& Muzzle, const FVector& Impact, bool ThirdPerson, bool Cover, bool Hit, bool Headshot, bool Shield=false, bool Broken=false);
 void KnifePhase(int32 Action,float Previous,float Current,bool Karambit=false);
 void MeleeImpact(const FVector& Impact,bool Hit,bool Cover,bool Shield=false,bool Broken=false);
 void ReloadPhase(float Previous, float Current,bool MP7=false,bool Empty=false);
 // For incoming projectile events. Never play a fly-by for the local shooter's own shot.
 bool Flyby(const FVector& Start, const FVector& End, const AActor* Shooter);
 int32 ShotCount=0, HitCount=0, HeadCount=0, CoverCount=0, ReloadCount=0, FlybyCount=0;
 void ImpactFeedback(bool Headshot,bool Shield,bool Broken);
 int32 ShieldHitCount=0,ShieldBreakCount=0,FleshCount=0;
 int32 LoadedCount() const { return Sounds.Num(); }
private:
 UPROPERTY() TMap<FName,TObjectPtr<USoundBase>> Sounds;
 UPROPERTY() TMap<FName,TObjectPtr<USoundBase>> KnifeSounds;
 UPROPERTY() TObjectPtr<USoundConcurrency> GunConcurrency;
 UPROPERTY() TObjectPtr<USoundConcurrency> FeedbackConcurrency;
 UPROPERTY() TObjectPtr<USoundConcurrency> WorldConcurrency;
 UPROPERTY() TObjectPtr<USoundConcurrency> ReloadConcurrency;
 UPROPERTY() TObjectPtr<USoundAttenuation> WorldAttenuation;
 float LastFlyby=-10;
 void Local(FName Name, float Gain, USoundConcurrency* Concurrency, float Pitch=1);
 void World(FName Name, const FVector& Location, float Gain, float Pitch=1);
};
