#include "RangeCasingFX.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

ARangeCasingFX::ARangeCasingFX()
{
 PrimaryActorTick.bCanEverTick=true;
 PrimaryActorTick.bStartWithTickEnabled=false;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 RifleCases=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("556SpentCases"));
 MP7Cases=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("46SpentCases"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Rifle(TEXT("/Game/Fireline/Effects/Casings/SM_Casing556.SM_Casing556"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> MP7(TEXT("/Game/Fireline/Effects/Casings/SM_Casing46.SM_Casing46"));
 RifleCases->SetStaticMesh(Rifle.Object);MP7Cases->SetStaticMesh(MP7.Object);
 for(auto* Mesh:{RifleCases.Get(),MP7Cases.Get()})
 {
  Mesh->SetupAttachment(RootComponent);Mesh->SetMobility(EComponentMobility::Movable);
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);Mesh->SetGenerateOverlapEvents(false);
  Mesh->SetCastShadow(false);Mesh->SetCanEverAffectNavigation(false);
 }
}
bool ARangeCasingFX::AssetsReady() const{return RifleCases->GetStaticMesh()&&MP7Cases->GetStaticMesh();}
FTransform ARangeCasingFX::EjectionFrame(USkeletalMeshComponent* Gun,bool MP7)
{
 const FString Prefix=MP7?TEXT("MP7_"):TEXT("M4_");
 const FVector Rear=Gun->GetSocketLocation(*(Prefix+TEXT("rearsight")));
 const FVector Forward=Gun->GetSocketLocation(*(Prefix+TEXT("frontsight")))-Rear;
 const FVector Up=Gun->GetSocketLocation(*(Prefix+TEXT("sightup")))-Rear;
 const FQuat Rotation=FRotationMatrix::MakeFromXZ(Forward,Up).ToQuat();
 // Measured in source bore coordinates: centre of the right-side receiver
 // window, outside the side wall. Never use the muzzle or moving bolt origin.
 const FVector Port=MP7?FVector(6.4,2.65,-7.85):FVector(11.08,2.7,-6.5);
 const float Scale=Gun->GetComponentScale().GetAbs().GetMax();
 return FTransform(Rotation,Rear+Rotation.RotateVector(Port*Scale));
}
void ARangeCasingFX::Eject(USkeletalMeshComponent* Gun,bool MP7,const FVector& InheritedVelocity)
{
 if(!Gun||!AssetsReady())return;
 const int Type=MP7?1:0,Index=Next[Type];Next[Type]=(Index+1)%CasesPerWeapon;
 auto* Mesh=MP7?MP7Cases.Get():RifleCases.Get();
 while(Mesh->GetInstanceCount()<CasesPerWeapon)Mesh->AddInstance(FTransform(FQuat::Identity,FVector::ZeroVector,FVector::ZeroVector),true);
 const FTransform Frame=EjectionFrame(Gun,MP7);FCase& C=Cases[Type][Index];C=FCase();
 const FVector Extent=Mesh->GetStaticMesh()->GetBounds().BoxExtent;
 C.HalfLength=Extent.GetMax();C.Radius=FMath::Max(Extent.GetMin(),.1);
 C.LongAxis=Extent.X>=Extent.Y&&Extent.X>=Extent.Z?FVector::ForwardVector:Extent.Y>=Extent.Z?FVector::RightVector:FVector::UpVector;
 C.Age=0;C.Position=Frame.GetLocation();
 C.Rotation=Frame.GetRotation()*FRotator(FMath::FRandRange(-5.f,5.f),FMath::FRandRange(-8.f,8.f),FMath::FRandRange(-20.f,20.f)).Quaternion()*FQuat::FindBetweenNormals(C.LongAxis,FVector::ForwardVector);
 // A narrow, weapon-relative ejection cone. Fast initial escape, then gravity;
 // do not throw towards the lens or impose a world-axis spin on a tilted gun.
 const float Speed=FMath::FRandRange(.93f,1.07f);
 const FVector Toss=MP7?FVector(125.f,360.f,100.f):FVector(150.f,400.f,125.f);
 C.Velocity=Frame.TransformVectorNoScale((Toss+FVector(FMath::FRandRange(-15.f,15.f),FMath::FRandRange(-20.f,20.f),FMath::FRandRange(-18.f,18.f)))*Speed)+InheritedVelocity;
 C.Spin=Frame.TransformVectorNoScale(FVector(FMath::FRandRange(-3.f,3.f),FMath::FRandRange(7.f,13.f),FMath::FRandRange(12.f,18.f)));
 Mesh->UpdateInstanceTransform(Index,FTransform(C.Rotation,C.Position),true,true,true);
 LastOrigin=C.Position;LastVelocity=C.Velocity;++Ejected;SetActorTickEnabled(true);
}
int32 ARangeCasingFX::ActiveCount() const
{
 int Count=0;for(const auto& Type:Cases)for(const auto& C:Type)Count+=C.Age<2.8f;return Count;
}
void ARangeCasingFX::Tick(float Dt)
{
 Super::Tick(Dt);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(FirelineCasings),false,GetOwner());
 FCollisionObjectQueryParams Objects;Objects.AddObjectTypesToQuery(ECC_WorldStatic);
 // Sweeps avoid tunnelling; bound collision work even during a long render stall.
 const float StepTime=FMath::Min(Dt,.1f);const int Steps=FMath::Clamp(FMath::CeilToInt(StepTime*120),1,12);const float H=StepTime/Steps;
 for(int Type=0;Type<2;++Type)
 {
  auto* Mesh=Type?MP7Cases.Get():RifleCases.Get();bool Dirty=false;
  for(int I=0;I<CasesPerWeapon;++I)
  {
   FCase& C=Cases[Type][I];if(C.Age>=2.8f)continue;
   C.Age+=Dt;
   for(int S=0;S<Steps&&!C.Resting&&C.Age<2.8f;++S)
   {
    const FVector Gravity(0,0,GetWorld()->GetGravityZ());const FVector End=C.Position+C.Velocity*H+Gravity*(.5f*H*H);C.Velocity+=Gravity*H;
    FHitResult Hit;
    const FQuat ShapeRotation=C.Rotation*FQuat::FindBetweenNormals(FVector::UpVector,C.LongAxis);
    if(GetWorld()->SweepSingleByObjectType(Hit,C.Position,End,ShapeRotation,Objects,FCollisionShape::MakeCapsule(C.Radius,C.HalfLength),Query))
    {
     C.Position=Hit.Location+Hit.Normal*(.08f+Hit.PenetrationDepth);const float Into=C.Velocity.Dot(Hit.Normal);
     if(Into<0)
     {
      const FVector Tangent=C.Velocity-Hit.Normal*Into;
      const float Restitution=C.Contacts==0?.23f:C.Contacts==1?.12f:.04f;
      C.Velocity=Tangent*.64f-Hit.Normal*Into*Restitution;
     }
     C.Spin*=.48f;++C.Contacts;++Bounces;
     if(Hit.Normal.Z>.55f&&(C.Contacts>=3||(FMath::Abs(Into)<75.f&&C.Velocity.SizeSquared()<8100)))
     {
      C.Resting=true;C.Velocity=FVector::ZeroVector;
      FVector Long=C.Rotation.RotateVector(C.LongAxis);Long=(Long-Hit.Normal*Long.Dot(Hit.Normal)).GetSafeNormal();
      if(Long.IsNearlyZero())Long=FVector::CrossProduct(Hit.Normal,FVector::RightVector).GetSafeNormal();
      C.SettlePosition=C.Position;C.SettleRotation=C.Rotation;
      C.RestPosition=C.Position-Hit.Normal*(C.Position-Hit.ImpactPoint).Dot(Hit.Normal)+Hit.Normal*(C.Radius+.04f);
      C.RestRotation=FRotationMatrix::MakeFromXZ(Long,Hit.Normal).ToQuat()*FQuat::FindBetweenNormals(C.LongAxis,FVector::ForwardVector);
     }
    }
    else C.Position=End;
    if(!C.Resting){C.Spin*=FMath::Exp(-.2f*H);C.Rotation=(FQuat(C.Spin.GetSafeNormal(),C.Spin.Size()*H)*C.Rotation).GetNormalized();}
   }
   if(C.Resting)
   {
    C.SettleTime=FMath::Min(.10f,C.SettleTime+StepTime);const float T=C.SettleTime/.10f,S=T*T*(3-2*T);
    C.Position=FMath::Lerp(C.SettlePosition,C.RestPosition,S);C.Rotation=FQuat::Slerp(C.SettleRotation,C.RestRotation,S);
   }
   const float Scale=FMath::Clamp((2.8f-C.Age)/.12f,0.f,1.f);
   Mesh->UpdateInstanceTransform(I,FTransform(C.Rotation,C.Position,FVector(Scale)),true,false,true);Dirty=true;
  }
  if(Dirty)Mesh->MarkRenderStateDirty();
 }
 if(ActiveCount()==0)SetActorTickEnabled(false);
}
