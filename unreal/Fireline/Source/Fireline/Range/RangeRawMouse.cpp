#include "RangeRawMouse.h"
#if !PLATFORM_MAC
TSharedPtr<FRangeRawMouse> FRangeRawMouse::Create(bool){return nullptr;}
#endif
