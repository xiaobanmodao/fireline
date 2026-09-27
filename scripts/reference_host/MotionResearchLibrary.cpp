#include "MotionResearchLibrary.h"
#include "AssetCompilingManager.h"
void UMotionResearchLibrary::FinishReferenceCompilation()
{
    FAssetCompilingManager::Get().FinishAllCompilation();
}
