#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RangeCasingFX.generated.h"

// Two instanced meshes, fixed storage and no Chaos bodies/audio per round.
UCLASS()
class FIRELINE_API ARangeCasingFX : public AActor
{
 GENERATED_BODY()
public:
 ARangeCasingFX();
 virtual void Tick(float Dt) override;
 static FTransform EjectionFrame(class USkeletalMeshComponent* Gun,bool MP7);
 void Eject(class USkeletalMeshComponent* Gun,bool MP7,const FVector& InheritedVelocity);
 int32 ActiveCount() const;
 bool AssetsReady() const;
 int32 Ejected=0,Bounces=0;
 FVector LastOrigin=FVector::ZeroVector,LastVelocity=FVector::ZeroVector;
 static constexpr int32 CasesPerWeapon=32;
private:
 struct FCase
 {
  FVector Position=FVector::ZeroVector,Velocity=FVector::ZeroVector,Spin=FVector::ZeroVector;
  FQuat Rotation=FQuat::Identity;
  FVector LongAxis=FVector::ForwardVector,RestPosition=FVector::ZeroVector,SettlePosition=FVector::ZeroVector;
  FQuat RestRotation=FQuat::Identity,SettleRotation=FQuat::Identity;
  float Radius=.5f,HalfLength=2.25f,SettleTime=0;
  float Age=3;
  int32 Contacts=0;
  bool Resting=false;
 };
 FCase Cases[2][CasesPerWeapon];
 int32 Next[2]={0,0};
 UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> RifleCases;
 UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> MP7Cases;
};
