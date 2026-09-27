#pragma once
#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "RebootHUD.generated.h"
UCLASS()
class FIRELINE_API ARebootHUD : public AHUD
{
    GENERATED_BODY()
public:
    virtual void DrawHUD() override;
};
