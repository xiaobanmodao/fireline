#include "RangeLobby.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Animation/AnimSequence.h"
#include "UObject/ConstructorHelpers.h"
ARangeLobby::ARangeLobby()
{
 PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.bTickEvenWhenPaused=true;
 RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("LobbyRoot"));
 Camera=CreateDefaultSubobject<UCameraComponent>(TEXT("LobbyCamera"));Camera->SetupAttachment(RootComponent);
 Camera->SetRelativeLocation(FVector(620,-65,115));Camera->SetRelativeRotation((FVector(0,0,103)-Camera->GetRelativeLocation()).Rotation());Camera->FieldOfView=42;
 Character=CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("UnarmedRyan"));Character->SetupAttachment(RootComponent);Character->SetRelativeRotation(FRotator(0,-82,0));
 static ConstructorHelpers::FObjectFinder<USkeletalMesh> Body(TEXT("/Game/Fireline/Hands/SK_RyanAction.SK_RyanAction"));Character->SetSkeletalMesh(Body.Object);
 Character->SetCollisionEnabled(ECollisionEnabled::NoCollision);Character->SetComponentTickEnabled(false);
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
 static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
 int I=0;
 auto Box=[&](FVector P,FVector Size,const TCHAR* Mat,bool Round=false)
 {
  auto* Mesh=CreateDefaultSubobject<UStaticMeshComponent>(*FString::Printf(TEXT("Architecture%d"),I++));Mesh->SetupAttachment(RootComponent);
  Mesh->SetStaticMesh(Round?Cylinder.Object:Cube.Object);Mesh->SetRelativeLocation(P);Mesh->SetRelativeScale3D(Size/100);
  Mesh->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Fireline/Lobby/%s.%s"),Mat,Mat)));
  Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);return Mesh;
 };
 Box(FVector(0,0,-18),FVector(1600,1800,20),TEXT("M_Floor"));
 Box(FVector(-220,0,190),FVector(20,1800,420),TEXT("M_Wall"));
 Box(FVector(0,0,-5),FVector(180,180,10),TEXT("M_Panel"),true);
 Box(FVector(0,0,-1),FVector(185,185,1),TEXT("M_Trim"),true);
 Box(FVector(0,0,0),FVector(178,178,2),TEXT("M_Floor"),true);
 for(int S:{-1,1})
 {
  Box(FVector(-195,S*260,150),FVector(25,45,340),TEXT("M_Panel"));
  Box(FVector(-178,S*260,150),FVector(3,3,310),TEXT("M_Trim"));
  Box(FVector(-203,S*122,138),FVector(10,165,220),TEXT("M_Panel"));
  Box(FVector(-196,S*122,250),FVector(2,165,2),TEXT("M_Trim"));
  Box(FVector(90,S*310,-7),FVector(620,3,1),TEXT("M_Trim"));
  Box(FVector(-205,S*470,185),FVector(8,270,280),TEXT("M_Wall"));
 }
 Box(FVector(-192,0,318),FVector(5,560,15),TEXT("M_Panel"));
 Box(FVector(-184,0,314),FVector(2,420,3),TEXT("M_Warm"));
 auto Light=[&](const TCHAR* Name,FVector P,FLinearColor Color,float Power)
 {
  auto* L=CreateDefaultSubobject<UPointLightComponent>(Name);L->SetupAttachment(RootComponent);L->SetRelativeLocation(P);L->SetIntensity(Power);L->SetAttenuationRadius(1400);L->SetLightColor(Color);L->SetCastShadows(true);
 };
 Light(TEXT("Key"),FVector(230,-190,280),FLinearColor(.8,.9,1),6500);
 Light(TEXT("Rim"),FVector(-110,180,210),FLinearColor(.08,.55,1),4500);
 Light(TEXT("Fill"),FVector(180,220,170),FLinearColor(1,.7,.4),1800);
}
void ARangeLobby::BeginPlay()
{
 Super::BeginPlay();Clock=FPlatformTime::Seconds();
 for(int I=0;I<Character->GetNumMaterials();++I)
 {
  const FString Slot=Character->GetMaterialSlotNames()[I].ToString();const TCHAR* Name=Slot.Contains(TEXT("ArmorBase"))?TEXT("M_Player"):Slot.Contains(TEXT("ArmorUnder"))?TEXT("M_Under"):Slot.Contains(TEXT("Visor"))?TEXT("M_Visor"):nullptr;
  if(Name)Character->SetMaterial(I,LoadObject<UMaterialInterface>(nullptr,*FString::Printf(TEXT("/Game/Fireline/Materials/%s.%s"),Name,Name)));
 }
 Character->PlayAnimation(LoadObject<UAnimSequence>(nullptr,TEXT("/Game/Fireline/Lobby/A_LobbyIdle.A_LobbyIdle")),true);
}
void ARangeLobby::Activate(bool Active){SetActorHiddenInGame(!Active);SetActorTickEnabled(Active);if(Active)Clock=FPlatformTime::Seconds();}
void ARangeLobby::Tick(float Dt)
{
 Super::Tick(Dt);
 // UI world is paused; sample the isolated lobby animation using real elapsed time.
 Character->SetPosition(FMath::Fmod(FPlatformTime::Seconds()-Clock,12.),false);Character->TickAnimation(0,false);Character->RefreshBoneTransforms();
}
