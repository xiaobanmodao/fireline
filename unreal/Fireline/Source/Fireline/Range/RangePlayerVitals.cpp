#include "RangeGame.h"
#include "Components/SkeletalMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "GameFramework/CharacterMovementComponent.h"
float ARangeCharacter::TakeDamage(float Amount,const FDamageEvent&,AController*,AActor*)
{
 const float Before=PlayerVitals.Health+PlayerVitals.Shield,ShieldBefore=PlayerVitals.Shield;
 if(!PlayerVitals.Damage(Amount))return 0;
 if(ShieldBefore>0){PlayerShieldPulse=1;bPlayerShieldBroken=PlayerVitals.Shield<=0;}
 if(PlayerVitals.Health<=0){RespawnLeft=2;GetCharacterMovement()->DisableMovement();}
 return Before-PlayerVitals.Health-PlayerVitals.Shield;
}
void ARangeCharacter::UpdatePlayerVitals(float Dt)
{
 if(!PlayerShieldMaterial)
 {
  if(auto* M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/Fireline/Materials/M_Shield.M_Shield")))
  {PlayerShieldMaterial=UMaterialInstanceDynamic::Create(M,this);GetMesh()->SetOverlayMaterial(PlayerShieldMaterial);}
 }
 if(RespawnLeft>0)
 {
  RespawnLeft-=Dt;if(RespawnLeft<=0){PlayerVitals.Reset();SetActorLocation(SpawnPoint);GetCharacterMovement()->SetMovementMode(MOVE_Walking);PlayerShieldPulse=0;}
 }
 else PlayerVitals.Tick(Dt);
 PlayerShieldPulse=FMath::Max(0.f,PlayerShieldPulse-Dt/(bPlayerShieldBroken?.45f:.22f));
 if(PlayerShieldMaterial)
 {
  PlayerShieldMaterial->SetScalarParameterValue(TEXT("Charge"),PlayerVitals.Shield/100);
  PlayerShieldMaterial->SetScalarParameterValue(TEXT("Pulse"),PlayerShieldPulse);
  const FVector P=GetMesh()->GetSocketLocation(TEXT("Chest"));PlayerShieldMaterial->SetVectorParameterValue(TEXT("HitPoint"),FLinearColor(P.X,P.Y,P.Z));
 }
}
