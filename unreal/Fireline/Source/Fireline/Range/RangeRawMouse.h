#pragma once
#include "CoreMinimal.h"

// Gameplay consumes only raw relative motion; Slate keeps ownership of the cursor/buttons.
class FRangeRawMouse
{
public:
 virtual ~FRangeRawMouse()=default;
 static TSharedPtr<FRangeRawMouse> Create(bool Audit=false);
 virtual void SetEnabled(bool Enabled)=0;
 virtual FVector2D Consume()=0;
 virtual int32 DeviceCount() const=0;
 virtual uint64 EventCount() const=0;
 virtual void InjectForAudit(double X,double Y)=0;
};
