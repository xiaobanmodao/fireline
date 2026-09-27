#pragma once
#include <algorithm>
#include <cmath>

// Fireline tuning, not claimed Apex values. Kept engine-independent so the
// entry rules can be tested without a renderer or an animation asset.
namespace RangeLocomotion
{
inline constexpr float SlideMinSpeed=350.f;
inline constexpr float SlideBoost=140.f;
inline constexpr float SlideMinEntrySpeed=650.f;
inline constexpr float SlideMaxSpeed=880.f;
inline constexpr float SlideExitSpeed=230.f;
inline constexpr float SlideDuration=.95f;
inline constexpr float SlideCooldown=.55f;
inline constexpr float SlideFriction=.25f;
inline constexpr float SlideBraking=650.f;

inline bool CanEnterSlide(bool Grounded,bool Sliding,bool Crouched,bool Sprint,
                          bool Reloading,bool Aiming,float Cooldown,float Speed)
{
 return Grounded&&!Sliding&&!Crouched&&Sprint&&!Reloading&&!Aiming&&
        Cooldown<=0.f&&std::isfinite(Speed)&&Speed>=SlideMinSpeed;
}
inline float SlideEntrySpeed(float Speed)
{
 // The boost has a ceiling; external overspeed must not be instantly erased.
 return std::max(Speed,std::clamp(Speed+SlideBoost,SlideMinEntrySpeed,SlideMaxSpeed));
}
inline float FallBlend(float VerticalSpeed)
{
 const float T=std::clamp((80.f-VerticalSpeed)/180.f,0.f,1.f);
 return T*T*(3.f-2.f*T);
}
}
