#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RangeShotFX.generated.h"
UCLASS()
class FIRELINE_API ARangeShotFX : public AActor
{
 GENERATED_BODY()
public:
 static constexpr float ImpactSize=1.6f;
 static constexpr float BloodImpactSize=2.f;
 ARangeShotFX();
 void Setup(class USkeletalMeshComponent* Gun,const FVector& End,const FVector& Normal,bool Impact,bool Target);
 void SetShieldImpact(bool Broken);
 void SetFleshImpact();
 FVector GetFlashBase() const;
 bool HasBloodSprite() const {return bFlesh&&BloodMaterial!=nullptr;}
 float GetBloodVariation() const {return BloodVariation;}
 float GetBloodAngle() const {return BloodAngle;}
 virtual void Tick(float Dt) override;
private:
 UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Pieces;
 UPROPERTY() TArray<TObjectPtr<class UMaterialInstanceDynamic>> Colors;
 UPROPERTY() TObjectPtr<class UPointLightComponent> FlashLight;
 UPROPERTY() TObjectPtr<class UStaticMeshComponent> BloodSprite;
 UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> BloodMaterial;
 UPROPERTY() TArray<TObjectPtr<class UProceduralMeshComponent>> Shards;
 FVector Start,Finish,Axis,Tangent,Bitangent;
 TArray<FVector> ShardVelocities;
 TArray<FVector> ShardSpins;
 float BloodVariation=1, BloodAngle=0;
 float Age=0,Distance=0;
 bool bImpact=false,bTarget=false,bShield=false,bShieldBreak=false,bFlesh=false;
};

// A separate world-space burst covers the enemy's entire shield silhouette.
// The ordinary ARangeShotFX still supplies the contact-point sparks.
UCLASS()
class FIRELINE_API ARangeShieldBreakFX : public AActor
{
 GENERATED_BODY()
public:
 ARangeShieldBreakFX();
 void Setup(const FTransform& ShellWorld);
 virtual void Tick(float Dt) override;
 int32 GetFragmentCount() const {return Fragments.Num();}
private:
 struct FFragment
 {
  FVector Center,Normal,SpinAxis;
  float Speed=0,SpinRate=0,Delay=0;
 };
 UPROPERTY() TObjectPtr<class UProceduralMeshComponent> Mesh;
 UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Material;
 TArray<FFragment> Fragments;
 TArray<FVector> BaseVertices;
 float Age=0;
};
