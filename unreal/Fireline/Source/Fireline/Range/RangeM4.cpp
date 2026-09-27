#include "RangeGame.h"
#include "RangeWeaponTiming.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/PlayerController.h"

void ARangeCharacter::StartM4Action(int32 Action)
{
 // Only draw and remove use this action layer; firing uses the original kick.
 if(Action!=1&&Action!=2)return;
 M4PreviousAction=M4Action;M4PreviousTime=M4Time;M4Mix=M4Weight>0?0.f:1.f;
 M4Action=Action;M4Time=0;bM4Returning=false;
 // Remove may start during a held reload pose. Blend out of that sample
 // while the gun lowers instead of first restoring the neutral grip.
 if(Action!=2||!bReloadPoseHeld)M4Weight=1;
}
void ARangeCharacter::CancelM4Action(){if(M4Weight>0)bM4Returning=true;}
bool ARangeCharacter::IsM4DrawLocked() const
{
 return WeaponSlot()==1&&M4Action==1&&!bM4Returning&&M4Time+KINDA_SMALL_NUMBER<RangeWeaponTiming::M4DrawReady;
}
bool ARangeCharacter::IsM4ReloadRecovery() const
{
 return WeaponSlot()==1&&bReloading&&bReloadAmmoCommitted&&ReloadPosition+KINDA_SMALL_NUMBER>=RangeWeaponTiming::M4ReloadReady(bEmptyReload);
}
void ARangeCharacter::ReleaseM4ReloadRecovery()
{
 if(!IsM4ReloadRecovery())return;
 bReloading=false;bReloadPoseHeld=false;ReloadLeft=0;ReloadLeadOut=0;
}
void ARangeCharacter::UpdateM4(float Dt)
{
 // The departing magazine becomes a physical world object at the drop
 // event. Hide its animated duplicate until the replacement is in hand.
 const bool HideMag=WeaponSlot()==1&&(bReloading||bReloadPoseHeld)&&ReloadPosition>=RangeWeaponTiming::M4MagazineDrop&&ReloadPosition<32.f/60.f;
 for(auto* Mesh:{Arms.Get(),GetMesh()})if(Mesh->DoesSocketExist(TEXT("M4_mag"))&&Mesh->IsBoneHiddenByName(TEXT("M4_mag"))!=HideMag)
 {
  if(HideMag)Mesh->HideBoneByName(TEXT("M4_mag"),PBO_None);
  else Mesh->UnHideBoneByName(TEXT("M4_mag"));
 }
 if(WeaponSlot()!=1)return;
 M4Mix=FMath::Min(1.f,M4Mix+Dt/.04f);
 if(bM4Returning)
 {
  M4Weight=FMath::Max(0.f,M4Weight-Dt/.10f);
  if(M4Weight<=0){M4Action=0;M4PreviousAction=0;M4Time=0;bM4Returning=false;}
 }
 else if(M4Action)
 {
  M4Time+=Dt;M4Weight=FMath::Min(1.f,M4Weight+Dt/.10f);
  const float Duration=M4Action==1?RangeWeaponTiming::M4DrawDuration:RangeWeaponTiming::M4RemoveDuration;
  M4Time=FMath::Min(M4Time,Duration);
  if(M4Action!=2&&M4Time>=Duration){M4Action=0;M4PreviousAction=0;M4Time=0;M4Weight=0;}
 }
 if(M4Action==1)if(auto* PC=Cast<APlayerController>(Controller))bAimHeld|=PC->IsInputKeyDown(EKeys::RightMouseButton);
 if(bAimHeld&&HandSwitchLeft<=0&&!IsM4DrawLocked())
 {
  if(M4Action==1)CancelM4Action();
  ReleaseM4ReloadRecovery();
 }
}
