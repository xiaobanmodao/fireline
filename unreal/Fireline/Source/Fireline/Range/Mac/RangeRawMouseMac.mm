#include "../RangeRawMouse.h"
#include "Misc/ScopeLock.h"
#include "Mac/MacSystemIncludes.h"
#import <GameController/GameController.h>

namespace
{
struct FRawState
{
 FCriticalSection Lock;
 bool Enabled=false,Audit=false;
 double X=0,Y=0;
 uint64 Events=0;
 void Add(double DX,double DY,bool Fixture=false)
 {
  FScopeLock Guard(&Lock);
  if(!Enabled||Audit!=Fixture||!FMath::IsFinite(DX)||!FMath::IsFinite(DY))return;
  X+=DX;Y+=DY;++Events;
 }
};
struct FMouseHook
{
 GCMouse* Mouse=nil;
 GCMouseMoved Previous=nil,Installed=nil;
};
class FMacRawMouse final : public FRangeRawMouse
{
 TSharedRef<FRawState,ESPMode::ThreadSafe> State=MakeShared<FRawState,ESPMode::ThreadSafe>();
 TArray<FMouseHook> Hooks;
 void Remove(int32 I)
 {
  auto& H=Hooks[I];
  // Don't overwrite a replacement installed by UE during focus recovery.
  if(H.Mouse.mouseInput.mouseMovedHandler==H.Installed)H.Mouse.mouseInput.mouseMovedHandler=H.Previous;
  [H.Installed release];[H.Previous release];[H.Mouse release];Hooks.RemoveAt(I);
 }
 void Refresh()
 {
  @autoreleasepool
  {
   NSArray<GCMouse*>* Mice=GCMouse.mice;
   for(int32 I=Hooks.Num()-1;I>=0;--I)
    if(![Mice containsObject:Hooks[I].Mouse]||Hooks[I].Mouse.mouseInput.mouseMovedHandler!=Hooks[I].Installed)Remove(I);
   for(GCMouse* Mouse in Mice)
   {
    if(!Mouse.mouseInput||Hooks.ContainsByPredicate([Mouse](const FMouseHook& H){return H.Mouse==Mouse;}))continue;
    FMouseHook H;H.Mouse=[Mouse retain];H.Previous=[Mouse.mouseInput.mouseMovedHandler copy];
    const auto SharedState=State;GCMouseMoved Previous=H.Previous;
    H.Installed=[^(GCMouseInput* Input,float DX,float DY)
    {
     // Preserve UE's pointer-lock housekeeping. Its axis events are deliberately
     // ignored by the gameplay look bindings while this raw service owns aiming.
     if(Previous)Previous(Input,DX,DY);
     SharedState->Add(DX,DY);
    } copy];
    Mouse.mouseInput.mouseMovedHandler=H.Installed;Hooks.Add(H);
    UE_LOG(LogTemp,Display,TEXT("FIRELINE_RAW_MOUSE attached source=GCMouse device=%s"),UTF8_TO_TCHAR(Mouse.vendorName.UTF8String?:"Mouse"));
   }
  }
 }
public:
 explicit FMacRawMouse(bool Audit){State->Audit=Audit;Refresh();}
 ~FMacRawMouse() override {SetEnabled(false);for(int32 I=Hooks.Num()-1;I>=0;--I)Remove(I);}
 void SetEnabled(bool Enabled) override
 {
  FScopeLock Guard(&State->Lock);
  if(State->Enabled!=Enabled)
  {
   State->Enabled=Enabled;State->X=State->Y=0;
   UE_LOG(LogTemp,Display,TEXT("FIRELINE_RAW_MOUSE active=%d events=%llu fixture=%d"),Enabled,State->Events,State->Audit);
  }
 }
 FVector2D Consume() override
 {
  Refresh(); // Also repairs a callback replaced by UE after a focus/fullscreen change.
  FScopeLock Guard(&State->Lock);
  FVector2D Result(State->X,State->Y);State->X=State->Y=0;return Result;
 }
 int32 DeviceCount() const override {return Hooks.Num();}
 uint64 EventCount() const override {FScopeLock Guard(&State->Lock);return State->Events;}
 void InjectForAudit(double X,double Y) override {State->Add(X,Y,true);}
};
}
TSharedPtr<FRangeRawMouse> FRangeRawMouse::Create(bool Audit){return MakeShared<FMacRawMouse>(Audit);}
