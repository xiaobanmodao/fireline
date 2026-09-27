#pragma once
#include "CoreMinimal.h"
class UWorld;
class AAlsCharacter;
// Bounded capture of the source animation graph after world actor evaluation.
struct FReferenceMotionCapture
{
    TWeakObjectPtr<AAlsCharacter> Character;
    double Start = -1;
    int32 Frame = 0;
    FString Poses, States;
    bool Finished = false;
    void Before(UWorld* World);
    void After(UWorld* World);
};
