#include "RangeShotFX.h"
#include "Engine/SkeletalMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "ProceduralMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
ARangeShotFX::ARangeShotFX()
{
 PrimaryActorTick.bCanEverTick=true;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Sphere(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Material(TEXT("/Game/Fireline/Materials/M_ShotFX.M_ShotFX"));
 for(int I=0;I<11;++I)
 {
  auto* P=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Visual%d"),I));
  P->SetupAttachment(RootComponent);P->SetStaticMesh(I==1||I==2?Sphere.Object:Cube.Object);
  P->SetMaterial(0,Material.Object);P->SetCollisionEnabled(ECollisionEnabled::NoCollision);P->SetCastShadow(false);Pieces.Add(P);
 }
 for(int I=0;I<12;++I)
 {
  auto* Shard=CreateDefaultSubobject<UProceduralMeshComponent>(*FString::Printf(TEXT("ShieldShard%d"),I));
  Shard->SetupAttachment(RootComponent);Shard->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Shard->SetCastShadow(false);Shard->SetVisibility(false);Shard->SetMaterial(0,Material.Object);Shards.Add(Shard);
 }
 BloodSprite=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BloodMistSprite"));BloodSprite->SetupAttachment(RootComponent);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Plane(TEXT("/Engine/BasicShapes/Plane.Plane"));
 static ConstructorHelpers::FObjectFinder<UMaterialInterface> Blood(TEXT("/Game/Fireline/Materials/M_BloodMistFlipbook.M_BloodMistFlipbook"));
 BloodSprite->SetStaticMesh(Plane.Object);BloodSprite->SetMaterial(0,Blood.Object);BloodSprite->SetCollisionEnabled(ECollisionEnabled::NoCollision);BloodSprite->SetCastShadow(false);BloodSprite->SetVisibility(false);
 FlashLight=CreateDefaultSubobject<UPointLightComponent>(TEXT("Flash"));FlashLight->SetupAttachment(RootComponent);
 FlashLight->SetCastShadows(false);FlashLight->SetIntensity(800);FlashLight->SetAttenuationRadius(100);FlashLight->SetLightColor(FLinearColor(1,.6,.2));
}
void ARangeShotFX::Setup(USkeletalMeshComponent* Gun,const FVector& End,const FVector& Normal,bool Impact,bool Target)
{
 const FName FlashSocket=Gun->GetSkeletalMeshAsset()->GetName().Contains(TEXT("MP7"))?FName(TEXT("MP7_Flash")):FName(TEXT("M4_Flash"));
 Start=Gun->GetSocketLocation(FlashSocket);Finish=End;Distance=FVector::Distance(Start,Finish);Axis=Normal.GetSafeNormal();
 Axis.FindBestAxisVectors(Tangent,Bitangent);bImpact=Impact;bTarget=Target;
 for(int I=0;I<Pieces.Num();++I)
 {
  auto* M=Pieces[I]->CreateAndSetMaterialInstanceDynamic(0);Colors.Add(M);
  M->SetVectorParameterValue(TEXT("Color"),I==2?FLinearColor(.025,.035,.045):FLinearColor(5,2.5,.65));
 }
 // Flash follows the animated gun; only the projectile/impact stay in world
 // space. Its rear tip begins at the muzzle instead of straddling the barrel.
 const FVector Bore=Gun->GetSocketTransform(FlashSocket).GetUnitAxis(EAxis::X);
 Pieces[1]->AttachToComponent(Gun,FAttachmentTransformRules::KeepWorldTransform,FlashSocket);
 Pieces[1]->SetWorldLocation(Start+Bore*4.5f);Pieces[1]->SetWorldRotation(Bore.Rotation());Pieces[1]->SetWorldScale3D(FVector(.09,.028,.028));
 Pieces[2]->SetWorldLocation(Finish+Axis*.4);Pieces[2]->SetWorldRotation(Axis.Rotation());Pieces[2]->SetWorldScale3D(FVector(.004,.025,.025));
 Pieces[2]->SetVisibility(bImpact&&!bTarget);
 FlashLight->AttachToComponent(Gun,FAttachmentTransformRules::SnapToTargetNotIncludingScale,FlashSocket);
 SetLifeSpan(bImpact&&!bTarget?1.8f:.18f);Tick(0);
}
void ARangeShotFX::SetFleshImpact()
{
 bFlesh=true;SetLifeSpan(.34f);
 BloodMaterial=BloodSprite->CreateAndSetMaterialInstanceDynamic(0);
 // Variation belongs to this impact, so the billboard cannot jitter each frame.
 BloodVariation=FMath::FRandRange(.9f,1.1f);BloodAngle=FMath::FRandRange(-18.f,18.f);
 // The darker blood cloud needs a wider silhouette than bright shield sparks
 // to remain readable over the target's red armor.
 BloodSprite->SetVisibility(true);BloodSprite->SetWorldScale3D(FVector(.46f*BloodImpactSize*BloodVariation));
 for(int I=2;I<Pieces.Num();++I)Pieces[I]->SetVisibility(false);
 Tick(0);
}
void ARangeShotFX::SetShieldImpact(bool Broken)
{
 bShield=true;bShieldBreak=Broken;SetLifeSpan(Broken?.5f:.24f);
 for(int I=3;I<Colors.Num();++I)Colors[I]->SetVectorParameterValue(TEXT("Color"),FLinearColor(.15,3,7));
 if(Broken)
 {
  ShardVelocities.Reset();ShardSpins.Reset();
  for(int I=0;I<Shards.Num();++I)
  {
   // Unequal triangular/quadrilateral slivers, with a thin back face: no cube silhouettes.
   const float Width=FMath::FRandRange(6.f,11.f), Height=FMath::FRandRange(11.f,22.f);
   const float Tip=FMath::FRandRange(-.45f,.45f)*Width;
   TArray<FVector> V={FVector(-Width*.55f,0,-Height*.5f),FVector(Width*.45f,0,-Height*.38f),FVector(Tip,0,Height*.55f),FVector(-Width*.25f,0,Height*.12f)};
   TArray<int32> T={0,1,2,0,2,3,2,1,0,3,2,0};
   TArray<FVector> N;TArray<FVector2D> UV;TArray<FColor> C;TArray<FProcMeshTangent> Tangents;
   for(int J=0;J<4;++J){N.Add(FVector(0,1,0));UV.Add(FVector2D(J&1,J>>1));C.Add(FColor::White);Tangents.Add(FProcMeshTangent(1,0,0));}
   Shards[I]->CreateMeshSection(0,V,T,N,UV,C,Tangents,false);
   auto* M=Shards[I]->CreateAndSetMaterialInstanceDynamic(0);
   M->SetVectorParameterValue(TEXT("Color"),FLinearColor(.06,1.7f+FMath::FRandRange(0.f,1.1f),3.5f));
   const float A=I*2*PI/Shards.Num()+FMath::FRandRange(-.3f,.3f);
   ShardVelocities.Add((Axis*FMath::FRandRange(35.f,90.f)+(Tangent*FMath::Cos(A)+Bitangent*FMath::Sin(A))*FMath::FRandRange(85.f,165.f))*ImpactSize);
   ShardSpins.Add(FVector(FMath::FRandRange(-410.f,410.f),FMath::FRandRange(-480.f,480.f),FMath::FRandRange(-520.f,520.f)));
   Shards[I]->SetWorldLocation(Finish+Axis*2);Shards[I]->SetWorldRotation(FRotator::ZeroRotator);Shards[I]->SetVisibility(true);
  }
 }
 Tick(0);
}
FVector ARangeShotFX::GetFlashBase() const{return FlashLight->GetComponentLocation();}
void ARangeShotFX::Tick(float Dt)
{
 Super::Tick(Dt);Age+=Dt;
 if(Colors.IsEmpty())return;
 const FVector D=(Finish-Start).GetSafeNormal();const float Along=FMath::Clamp(Age/.045f,0.f,1.f)*Distance;
 const float Length=FMath::Min(70.f,Along);
 Pieces[0]->SetVisibility(Age<.045f&&Length>1);
 Pieces[0]->SetWorldLocation(Start+D*(Along-Length*.5f));Pieces[0]->SetWorldRotation(D.Rotation());Pieces[0]->SetWorldScale3D(FVector(Length/100,.007,.007));
 Pieces[1]->SetVisibility(Age<.028f);FlashLight->SetVisibility(Age<.028f);
 if(bFlesh&&BloodMaterial)
 {
  const float T=FMath::Clamp(Age/.32f,0.f,1.f);
  BloodMaterial->SetScalarParameterValue(TEXT("Frame"),T*15);
  BloodMaterial->SetScalarParameterValue(TEXT("Fade"),FMath::Clamp((1-T)*4,0.f,1.f));
  BloodSprite->SetVisibility(T<1);
  if(auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0))
  {
   const FVector Toward=(Camera->GetCameraLocation()-Finish).GetSafeNormal();
   BloodSprite->SetWorldLocation(Finish+Toward*8+FVector(0,0,Age*7));
   BloodSprite->SetWorldRotation((FRotationMatrix::MakeFromZ(Toward).ToQuat()*FQuat(Toward,FMath::DegreesToRadians(BloodAngle))).Rotator());
  }
 }

 for(int I=3;I<11;++I)
 {
  const float A=(I-3)*.7854f+.4f;
  const float ShieldSize=bShield&&!bShieldBreak?1.3f:1.f;
  const FVector V=(Axis*(bFlesh?95.f:70.f)+(Tangent*FMath::Cos(A)+Bitangent*FMath::Sin(A))*(bFlesh?110.f:bShieldBreak?120.f:95.f))*ImpactSize*(bShield&&!bShieldBreak?1.2f:1.f);
  const float Lifetime=bShieldBreak?.45f:bShield?.22f:bFlesh?.25f:.15f;
  Pieces[I]->SetVisibility(!bFlesh&&!bShieldBreak&&bImpact&&Age<Lifetime&&(bShield||I<7));
  Pieces[I]->SetWorldLocation(Finish+V*Age+FVector(0,0,-180*Age*Age));Pieces[I]->SetWorldRotation(V.Rotation());
  Pieces[I]->SetWorldScale3D((bFlesh?FVector(.036,.023,.018):bShieldBreak?FVector(.06,.02,.004):FVector(.024,.006,.006))*ImpactSize*ShieldSize*FMath::Max(0.f,1-Age/Lifetime));
 }
 if(bShieldBreak)
 for(int I=0;I<Shards.Num();++I)
 {
  const float Life=.32f+(I%3)*.045f;
  Shards[I]->SetVisibility(Age<Life);
  Shards[I]->SetWorldLocation(Finish+Axis*2+ShardVelocities[I]*Age+FVector(0,0,-280*Age*Age));
  const FVector V=ShardVelocities[I];
  Shards[I]->SetWorldRotation((V.Rotation()+FRotator(ShardSpins[I].X*Age,ShardSpins[I].Y*Age,ShardSpins[I].Z*Age)));
  Shards[I]->SetWorldScale3D(FVector(FMath::Max(.1f,1-Age/Life)));
 }
}

ARangeShieldBreakFX::ARangeShieldBreakFX()
{
 PrimaryActorTick.bCanEverTick=true;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
 Mesh=CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("WholeShieldFracture"));
 Mesh->SetupAttachment(RootComponent);Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 Mesh->SetCastShadow(false);Mesh->SetCanEverAffectNavigation(false);
}
void ARangeShieldBreakFX::Setup(const FTransform& ShellWorld)
{
 SetActorTransform(ShellWorld);
 constexpr int32 Bands=6,Sectors=12;
 FRandomStream Random(FMath::Rand());
 TArray<FVector> Grid;Grid.SetNum((Bands+1)*Sectors);
 auto At=[&Grid](int32 B,int32 S)->FVector&{return Grid[B*Sectors+(S+Sectors)%Sectors];};
 for(int32 B=0;B<=Bands;++B)
 for(int32 S=0;S<Sectors;++S)
 {
  const float Latitude=-HALF_PI+PI*B/Bands+(B==0||B==Bands?0.f:Random.FRandRange(-.095f,.095f));
  const float Longitude=2.f*PI*S/Sectors+Random.FRandRange(-.09f,.09f);
  At(B,S)=FVector(50.f*FMath::Cos(Latitude)*FMath::Cos(Longitude),50.f*FMath::Cos(Latitude)*FMath::Sin(Longitude),50.f*FMath::Sin(Latitude));
 }
 TArray<int32> Indices;TArray<FVector> Normals;TArray<FVector2D> UV;TArray<FColor> Colors;TArray<FProcMeshTangent> Tangents;
 BaseVertices.Reserve(120*3);Fragments.Reserve(120);
 for(int32 B=0;B<Bands;++B)
 for(int32 S=0;S<Sectors;++S)
 {
  const FVector Corners[4]={At(B,S),At(B,S+1),At(B+1,S+1),At(B+1,S)};
  auto Triangle=[&](int32 A,int32 C,int32 D)
  {
   const FVector Center=(Corners[A]+Corners[C]+Corners[D])/3.f;
   const FVector Outward=Center.GetSafeNormal();
   const FVector Cross=FVector::CrossProduct(Corners[C]-Corners[A],Corners[D]-Corners[A]);
   if(FVector::DotProduct(Cross,Outward)<0)Swap(C,D);
   FVector Tangent,Bitangent;Outward.FindBestAxisVectors(Tangent,Bitangent);
   FFragment Fragment;Fragment.Center=Center;Fragment.Normal=Outward;
   Fragment.SpinAxis=(Tangent*Random.FRandRange(-1.f,1.f)+Bitangent*Random.FRandRange(-1.f,1.f)).GetSafeNormal();
   Fragment.Speed=Random.FRandRange(130.f,235.f);Fragment.SpinRate=Random.FRandRange(-7.f,7.f);Fragment.Delay=Random.FRandRange(0.f,.018f);
   Fragments.Add(Fragment);
   const int32 First=BaseVertices.Num();
   for(int32 V:{A,C,D})
   {BaseVertices.Add(Corners[V]);Normals.Add(Corners[V].GetSafeNormal());UV.Add(FVector2D(V&1,V>>1));Colors.Add(FColor::White);Tangents.Add(FProcMeshTangent(Tangent.X,Tangent.Y,Tangent.Z));}
   Indices.Add(First);Indices.Add(First+1);Indices.Add(First+2);
  };
  if(B==0)Triangle(1,2,3);
  else if(B==Bands-1)Triangle(0,1,3);
  else if((B+S)&1){Triangle(0,1,3);Triangle(1,2,3);}
  else {Triangle(0,1,2);Triangle(0,2,3);}
 }
 Mesh->CreateMeshSection(0,BaseVertices,Indices,Normals,UV,Colors,Tangents,false);
 if(auto* Source=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Fireline/Materials/M_EnergyShell.M_EnergyShell")))
 {
  Material=UMaterialInstanceDynamic::Create(Source,this);Mesh->SetMaterial(0,Material);
  Material->SetScalarParameterValue(TEXT("Presence"),.7f);Material->SetScalarParameterValue(TEXT("Pulse"),1.5f);
 }
 SetLifeSpan(.38f);
 Tick(0.f);
}
void ARangeShieldBreakFX::Tick(float Dt)
{
 Super::Tick(Dt);Age+=Dt;
 if(Fragments.IsEmpty())return;
 TArray<FVector> Animated;Animated.SetNumUninitialized(BaseVertices.Num());
 for(int32 I=0;I<Fragments.Num();++I)
 {
  const auto& F=Fragments[I];const float T=FMath::Max(0.f,Age-F.Delay);
  const FQuat Turn(F.SpinAxis,F.SpinRate*T);
  const FVector Travel=F.Normal*(F.Speed*T)+FVector(0,0,-130.f*T*T);
  const float Shrink=FMath::Max(.7f,.91f-.32f*T);
  for(int32 J=0;J<3;++J)Animated[I*3+J]=F.Center+Turn.RotateVector((BaseVertices[I*3+J]-F.Center)*Shrink)+Travel;
 }
 const TArray<FVector> EmptyNormals;const TArray<FVector2D> EmptyUV;const TArray<FColor> EmptyColors;const TArray<FProcMeshTangent> EmptyTangents;
 Mesh->UpdateMeshSection(0,Animated,EmptyNormals,EmptyUV,EmptyColors,EmptyTangents);
 if(Material)
 {
  const float Fade=FMath::Clamp(1.f-Age/.33f,0.f,1.f);
  Material->SetScalarParameterValue(TEXT("Presence"),.7f*Fade*Fade);
  Material->SetScalarParameterValue(TEXT("Pulse"),1.5f*Fade);
 }
}
