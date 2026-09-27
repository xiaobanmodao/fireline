#pragma once

// Seating frames measured from the delivered mechanical bone tracks at 60 Hz.
// Shared by gameplay and sound; the remaining animation is bolt/grip recovery.
namespace RangeWeaponTiming
{
 // Reload timers and event thresholds use authored clip seconds.
 // Slow their shared clock so pose, ammo, sounds and input gates stay together.
 constexpr float ReloadPlayRate(bool MP7){return MP7?1.f:.85f;}
 // Main action complete; the rest of the source clip is cosmetic settling.
 constexpr float M4DrawDuration=32.f/60.f;
 constexpr float M4RemoveDuration=29.f/60.f;
 constexpr float M4ReloadDuration(bool Empty){return Empty?110.f/60.f:84.f/60.f;}
 constexpr float M4DrawReady=26.f/60.f;
 constexpr float M4ReloadReady(bool Empty){return Empty?103.f/60.f:81.f/60.f;}
 constexpr float M4MagazineDrop=16.f/60.f;
 // Separate the discarded physical magazine from the replacement in the hand.
 // Measured on the delivered MP7 clips: withdrawal / belt fetch / return.
 constexpr float MP7MagazineDrop(bool Empty){return (Empty?17.f:25.f)/60.f;}
 constexpr float MP7MagazineReveal(bool Empty){return (Empty?39.f:54.f)/60.f;}
 constexpr float MagazineDrop(bool MP7,bool Empty){return MP7?MP7MagazineDrop(Empty):M4MagazineDrop;}
 constexpr float MP7DrawReady=21.f/60.f;
 constexpr float MP7ReloadReady(bool Empty){return Empty?143.f/60.f:115.f/60.f;}
 constexpr float MagazineSeat(bool MP7,bool Empty)
 {
  return MP7?(Empty?66.f/60.f:85.f/60.f):55.f/60.f;
 }
}
