#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "MotionResearchLibrary.generated.h"

// Serializes source/base-pose compilation before commandlet pose sampling.
UCLASS()
class UMotionResearchLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable)
    static void FinishReferenceCompilation();
};
