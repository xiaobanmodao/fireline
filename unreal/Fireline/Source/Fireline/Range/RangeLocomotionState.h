#pragma once
#include "CoreMinimal.h"

enum class ERangeLocomotionPhase : uint8 {Grounded,Sliding,Rising,Apex,Falling};

// Game-thread snapshot, copied into animation proxies in PreUpdate. Events
// come from accepted movement callbacks, never key presses or clip endings.
struct FRangeLocomotionState
{
 FVector2D LocalVelocity=FVector2D::ZeroVector; // X forward, Y right; cm/s.
 FVector HorizontalAcceleration=FVector::ZeroVector;
 FVector InputAcceleration=FVector::ZeroVector;
 FVector PreviousHorizontalVelocity=FVector::ZeroVector;
 bool HasVelocitySample=false;
 float Speed=0,VerticalSpeed=0,DirectionDegrees=0,AirTime=0;
 float LastLandingSpeed=0,LastSlideEntrySpeed=0;
 bool Grounded=true,Crouched=false;
 ERangeLocomotionPhase Phase=ERangeLocomotionPhase::Grounded;
 uint32 JumpSerial=0,LandingSerial=0,SlideSerial=0;
};
