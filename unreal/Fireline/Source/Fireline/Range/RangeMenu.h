#pragma once
#include "Widgets/SCompoundWidget.h"
#include "RangePlayerController.h"
class SRangeMenu : public SCompoundWidget
{
public:
 SLATE_BEGIN_ARGS(SRangeMenu){} SLATE_ARGUMENT(ARangePlayerController*, Controller) SLATE_END_ARGS()
 void Construct(const FArguments& Args);
 virtual bool SupportsKeyboardFocus() const override{return true;}
 virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent& Event) override;
 void OpenSettings();
 void ShowHome(bool Paused);
 void Refresh();
 void FocusFirst();
 TSharedPtr<class SButton> StartButton;
 FReply Apply();
 int32 Page=0;
 FRangePreferences Draft;
private:
 TWeakObjectPtr<ARangePlayerController> Owner;
 bool bPausedMenu=false,bModeExpanded=false;
 FString Status;
 TArray<TSharedPtr<FString>> QualityOptions;
 TArray<TSharedPtr<FString>> SightOptions;
 TSharedRef<SWidget> Button(const FString& Label,TFunction<FReply()> Action,bool Primary=false);
 TSharedRef<SWidget> Text(const FString& Label,int32 Size,FLinearColor Color=FLinearColor(.86,.91,.96));
 TSharedRef<SWidget> Settings();
};
