#include "RangeGame.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/PackageName.h"
#include "UObject/SavePackage.h"

void URangeAssetTools::PrepareDJNativeHands()
{
#if WITH_EDITOR
 // DJ's palm uses more than four influences. Preserve up to UE's twelve
 // influences and avoid 8-bit weight steps at small finger/wrist joints.
 for(const TCHAR* Path:{TEXT("/Game/Fireline/Hands/SK_RyanAction"),TEXT("/Game/Fireline/Hands/SK_RyanActionArms"),TEXT("/Game/Fireline/Karambit/SK_KarambitBody"),TEXT("/Game/Fireline/Karambit/SK_KarambitArms")})
 {
  auto* Mesh=LoadObject<USkeletalMesh>(nullptr,Path);check(Mesh);
  checkf(Mesh->GetRefSkeleton().FindBoneIndex(TEXT("DJ_wrist_L"))!=INDEX_NONE && Mesh->GetRefSkeleton().FindBoneIndex(TEXT("DJ_wrist_R"))!=INDEX_NONE,TEXT("Native hand binding missing: %s"),Path);
  Mesh->PreEditChange(nullptr);
  for(int32 L=0;L<Mesh->GetLODNum();++L)
  {
   auto* Info=Mesh->GetLODInfo(L);Info->bAllowCPUAccess=true;
   Info->BuildSettings.BoneInfluenceLimit=12;
   Info->BuildSettings.bUseHighPrecisionSkinWeights=true;
  }
  Mesh->PostEditChange();Mesh->MarkPackageDirty();
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
  UPackage::SavePackage(Mesh->GetOutermost(),Mesh,*FPackageName::LongPackageNameToFilename(Mesh->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension()),Args);
  UE_LOG(LogTemp,Display,TEXT("DJ_NATIVE_SKIN_READY asset=%s bones=%d influences=12 high_precision=1"),Path,Mesh->GetRefSkeleton().GetNum());
 }
#endif
}
