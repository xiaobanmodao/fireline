#include "RangeMenu.h"
#include "RangeLobby.h"
#include "RangeGame.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Components/SkeletalMeshComponent.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateColorBrush.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/PlayerInput.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "AudioDevice.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "AudioDeviceHandle.h"

// Draw the settings icon directly; avoids platform-dependent emoji fallback glyphs.
class SRangeGear : public SLeafWidget
{
public:
 SLATE_BEGIN_ARGS(SRangeGear){} SLATE_END_ARGS()
 void Construct(const FArguments&){}
 virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(30,30);}
 virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
 {
  const FVector2D Center=G.GetLocalSize()*.5;const float R=FMath::Min(G.GetLocalSize().X,G.GetLocalSize().Y)*.42f;
  TArray<FVector2D> Edge,Hole;
  for(int I=0;I<=64;++I){const float A=I*2*PI/64;const float Radius=R*((I%8>=2&&I%8<=5)?1.f:.76f);Edge.Add(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*Radius);}
  for(int I=0;I<=32;++I){const float A=I*2*PI/32;Hole.Add(Center+FVector2D(FMath::Cos(A),FMath::Sin(A))*R*.32f);}
  FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Edge,ESlateDrawEffect::None,FLinearColor(.85,.95,1),true,1.7f);
  FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),Hole,ESlateDrawEffect::None,FLinearColor(.85,.95,1),true,1.7f);return Layer;
 }
};
namespace
{
 const FLinearColor Cyan(.25f,.83f,.95f),Muted(.46f,.59f,.69f),Panel(.028f,.046f,.068f,.96f);
 const FSlateColorBrush Fill(FLinearColor::White);
 const FButtonStyle MenuButtonStyle=FButtonStyle()
  .SetNormal(FSlateColorBrush(FLinearColor::White))
  .SetHovered(FSlateColorBrush(FLinearColor(1.3,1.3,1.3)))
  .SetPressed(FSlateColorBrush(FLinearColor(.75,.75,.75)))
  .SetNormalPadding(FMargin(0)).SetPressedPadding(FMargin(0));
 const TCHAR* PrefSection=TEXT("Fireline.Preferences");
 FString PreferencePath()
 {
  FString AuditPath;
  if(FParse::Param(FCommandLine::Get(),TEXT("FirelineMenuAudit"))&&FParse::Value(FCommandLine::Get(),TEXT("FirelinePreferencesPath="),AuditPath))return AuditPath;
  return FString(FPlatformProcess::UserSettingsDir())/TEXT("Fireline/Preferences.json");
 }
 TSharedPtr<FJsonObject> ReadPreferences()
 {
  FString Data;TSharedPtr<FJsonObject> Object;
  if(FFileHelper::LoadFileToString(Data,*PreferencePath()))FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Data),Object);
  return Object;
 }
 FSlateFontInfo Font(int32 Size){return FCoreStyle::GetDefaultFontStyle("Regular",Size);}
}
TSharedRef<SWidget> SRangeMenu::Text(const FString& Label,int32 Size,FLinearColor Color)
{
 return SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(Size)).ColorAndOpacity(Color);
}
TSharedRef<SWidget> SRangeMenu::Button(const FString& Label,TFunction<FReply()> Action,bool Primary)
{
 return SNew(SBox).HeightOverride(56)[SNew(SButton)
  .ButtonStyle(&MenuButtonStyle)
  .ButtonColorAndOpacity(Primary?Cyan:FLinearColor(.08,.13,.18))
  .ContentPadding(FMargin(22,10)).HAlign(HAlign_Left)
  .OnClicked_Lambda([Action=MoveTemp(Action)]{return Action();})
  [Text(Label,18,Primary?FLinearColor(.012,.033,.045):FLinearColor(.86,.93,.97))]];
}
void SRangeMenu::Construct(const FArguments& Args)
{
 Owner=Args._Controller;Draft=Owner->Preferences;
 for(const TCHAR* Q:{TEXT("保持当前"),TEXT("低"),TEXT("中"),TEXT("高"),TEXT("极高")})QualityOptions.Add(MakeShared<FString>(Q));
 for(const TCHAR* S:{TEXT("机瞄"),TEXT("Coyote 红点")})SightOptions.Add(MakeShared<FString>(S));
 Refresh();
}
void SRangeMenu::ShowHome(bool Paused){bPausedMenu=Paused;Page=0;Status.Empty();Refresh();}
void SRangeMenu::OpenSettings(){Draft=Owner->Preferences;Status.Empty();Page=1;Refresh();}
FReply SRangeMenu::OnKeyDown(const FGeometry&,const FKeyEvent& Event)
{
 if(Event.GetKey()==EKeys::Escape||Event.GetKey()==EKeys::Gamepad_FaceButton_Right||Event.GetKey()==EKeys::Gamepad_Special_Right)
 {
  if(Page){Page=0;Refresh();}
  else if(bPausedMenu&&Owner.IsValid())Owner->ResumeTraining();
  return FReply::Handled();
 }
 return FReply::Unhandled();
}
TSharedRef<SWidget> SRangeMenu::Settings()
{
 auto Rows=SNew(SVerticalBox);
 Rows->AddSlot().AutoHeight().Padding(0,0,0,24)[Text(TEXT("设置"),32)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,8)[Text(TEXT("枪械瞄具"),17)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,22)[SNew(SBox).HeightOverride(42)[SNew(SComboBox<TSharedPtr<FString>>)
  .OptionsSource(&SightOptions).InitiallySelectedItem(SightOptions[Draft.CoyoteSight?1:0])
  .OnGenerateWidget_Lambda([this](TSharedPtr<FString> V){return Text(*V,16);})
  .OnSelectionChanged_Lambda([this](TSharedPtr<FString> V,ESelectInfo::Type){Draft.CoyoteSight=SightOptions.IndexOfByKey(V)==1;Status.Empty();})
  [SNew(STextBlock).Font(Font(16)).Text_Lambda([this]{return FText::FromString(*SightOptions[Draft.CoyoteSight?1:0]);})]]];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,8)[Text(TEXT("鼠标灵敏度"),17)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,10)[SNew(STextBlock).Font(Font(12)).ColorAndOpacity(Cyan).Text_Lambda([this]{return FText::FromString(Owner.IsValid()?Owner->RawMouseStatus():FString());})];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,25)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SSlider).SliderBarColor(FLinearColor(.14,.23,.30)).SliderHandleColor(Cyan).Value_Lambda([this]{return (Draft.Sensitivity-.005f)/.145f;}).OnValueChanged_Lambda([this](float V){Draft.Sensitivity=.005f+V*.145f;Status.Empty();})]
  +SHorizontalBox::Slot().AutoWidth().Padding(20,0).VAlign(VAlign_Center)[SNew(STextBlock).Font(Font(18)).ColorAndOpacity(Cyan).Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("%.2f ×"),Draft.Sensitivity/.05f));})]];
 Rows->AddSlot().AutoHeight().Padding(0,4,0,12)[Text(TEXT("手柄"),22)];
 for(int I=0;I<4;++I)
 {
  const TCHAR* Labels[]={TEXT("转向速度"),TEXT("开镜灵敏度倍率"),TEXT("移动摇杆死区"),TEXT("视角摇杆死区")};
  auto Value=[this,I]()->float&{if(I==0)return Draft.PadLookSpeed;if(I==1)return Draft.PadADSScale;if(I==2)return Draft.PadMoveDeadZone;return Draft.PadLookDeadZone;};
  const float Min=I==0?60.f:I==1?.15f:0.f,Max=I==0?540.f:I==1?1.f:.35f;
  Rows->AddSlot().AutoHeight().Padding(0,0,0,6)[Text(Labels[I],16)];
  Rows->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(SHorizontalBox)
   +SHorizontalBox::Slot().FillWidth(1)[SNew(SSlider).StepSize(.025f).RequiresControllerLock(false).Value_Lambda([Value,Min,Max]{return (Value()-Min)/(Max-Min);}).OnValueChanged_Lambda([Value,Min,Max](float X){Value()=FMath::Lerp(Min,Max,X);})]
   +SHorizontalBox::Slot().AutoWidth().Padding(16,0)[SNew(STextBlock).Font(Font(16)).Text_Lambda([Value,I]{return FText::FromString(I==0?FString::Printf(TEXT("%.0f°/秒"),Value()):FString::Printf(TEXT("%.0f%%"),Value()*100));})]];
 }
 Rows->AddSlot().AutoHeight().Padding(0,0,0,18)[SNew(SCheckBox).IsChecked_Lambda([this]{return Draft.PadInvertY?ECheckBoxState::Checked:ECheckBoxState::Unchecked;}).OnCheckStateChanged_Lambda([this](ECheckBoxState V){Draft.PadInvertY=V==ECheckBoxState::Checked;})[Text(TEXT("手柄垂直视角反转"),16)]];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,20)[SNew(STextBlock).Font(Font(13)).AutoWrapText(true).ColorAndOpacity(Muted).Text(FText::FromString(TEXT("左摇杆移动 / 按下冲刺 · 右摇杆视角 / 按下切刀\nRT/R2 开火 · LT/L2 开镜 · A/× 跳跃 · B/○ 按住蹲伏、冲刺中滑铲\nX/□ 换弹 · Y/△ 或 RB/R1 下一武器 · LB/L1 上一武器\n方向键↑ 检视 · ↓ 空手 · View/Share 第三人称 · Menu/Options 暂停\n菜单：方向键选择 · A/× 确认 · B/○ 返回")))];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,8)[Text(TEXT("主音量"),17)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,25)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SSlider).SliderBarColor(FLinearColor(.14,.23,.30)).SliderHandleColor(Cyan).Value_Lambda([this]{return Draft.Volume;}).OnValueChanged_Lambda([this](float V){Draft.Volume=V;Status.Empty();})]
  +SHorizontalBox::Slot().AutoWidth().Padding(20,0).VAlign(VAlign_Center)[SNew(STextBlock).Font(Font(18)).ColorAndOpacity(Cyan).Text_Lambda([this]{return FText::FromString(FString::Printf(TEXT("%d %%"),FMath::RoundToInt(Draft.Volume*100)));})]];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,10)[Text(TEXT("画质预设"),17)];
 Rows->AddSlot().AutoHeight().Padding(0,0,0,22)[SNew(SBox).HeightOverride(42)[SNew(SComboBox<TSharedPtr<FString>>)
  .OptionsSource(&QualityOptions).InitiallySelectedItem(QualityOptions[FMath::Clamp(Draft.Quality+1,0,4)])
  .OnGenerateWidget_Lambda([this](TSharedPtr<FString> V){return Text(*V,16);})
  .OnSelectionChanged_Lambda([this](TSharedPtr<FString> V,ESelectInfo::Type){Draft.Quality=QualityOptions.IndexOfByKey(V)-1;Status.Empty();})
  [SNew(STextBlock).Font(Font(16)).Text_Lambda([this]{return FText::FromString(*QualityOptions[FMath::Clamp(Draft.Quality+1,0,4)]);})]]];
 for(bool Full:{true,false})Rows->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(SCheckBox)
  .IsChecked_Lambda([this,Full]{return (Full?Draft.Fullscreen:Draft.VSync)?ECheckBoxState::Checked:ECheckBoxState::Unchecked;})
  .OnCheckStateChanged_Lambda([this,Full](ECheckBoxState S){(Full?Draft.Fullscreen:Draft.VSync)=S==ECheckBoxState::Checked;Status.Empty();})
  [Text(Full?TEXT("无边框全屏"):TEXT("垂直同步"),17)]];
 Rows->AddSlot().AutoHeight().Padding(0,12,0,10)[SNew(STextBlock).Font(Font(14)).ColorAndOpacity(Cyan).Text_Lambda([this]{return FText::FromString(Status.IsEmpty()?TEXT("应用后生效，并在下次启动时保留。") : Status);})];
 Rows->AddSlot().AutoHeight().Padding(0,4,0,10)[Button(TEXT("应用并保存"),[this]{return Apply();},true)];
 Rows->AddSlot().AutoHeight()[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,8,0)[Button(TEXT("恢复默认"),[this]{Draft.PadLookSpeed=240;Draft.PadADSScale=.45f;Draft.PadMoveDeadZone=.15f;Draft.PadLookDeadZone=.12f;Draft.PadInvertY=false;Draft.Sensitivity=.05f;Draft.Volume=1;Draft.Quality=2;Draft.VSync=false;Draft.Fullscreen=false;Draft.CoyoteSight=true;Status=TEXT("默认值已填入，点击应用保存。");Refresh();return FReply::Handled();})]
  +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("返回"),[this]{Page=0;Refresh();return FReply::Handled();})]];
 return Rows;
}
FReply SRangeMenu::Apply()
{
 if(Owner.IsValid()){Status=Owner->ApplyPreferences(Draft)?TEXT("设置已保存"):TEXT("保存失败，请检查本地设置目录权限。");}
 return FReply::Handled();
}
void SRangeMenu::Refresh()
{
 StartButton.Reset();
 RegisterActiveTimer(0.f,FWidgetActiveTimerDelegate::CreateLambda([this](double,float){FocusFirst();return EActiveTimerReturnType::Stop;}));
 if(Page==0&&!bPausedMenu)
 {
  auto Modes=SNew(SVerticalBox);
  if(bModeExpanded)
  {
   Modes->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SBorder).BorderImage(&Fill).BorderBackgroundColor(Panel).Padding(18)
    [SNew(SVerticalBox)
     +SVerticalBox::Slot().AutoHeight()[Text(TEXT("选择模式"),14,Muted)]
     +SVerticalBox::Slot().AutoHeight().Padding(0,10,0,0)[Button(TEXT("✓  训练场"),[this]{bModeExpanded=false;Refresh();return FReply::Handled();})]]];
  }
  Modes->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SButton).ButtonStyle(&MenuButtonStyle).ButtonColorAndOpacity(Panel).ContentPadding(18)
   .OnClicked_Lambda([this]{bModeExpanded=!bModeExpanded;Refresh();return FReply::Handled();})
   [SNew(SVerticalBox)
    +SVerticalBox::Slot().AutoHeight()[Text(TEXT("游戏模式  /  选择"),12,Muted)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,7,0,0)[Text(TEXT("训练场"),24)]
    +SVerticalBox::Slot().AutoHeight().Padding(0,5,0,0)[Text(TEXT("自由练习 · 熟悉武器与移动"),12,Muted)]]];
  Modes->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(68)[SAssignNew(StartButton,SButton).ButtonStyle(&MenuButtonStyle).ButtonColorAndOpacity(Cyan).HAlign(HAlign_Center)
   .OnClicked_Lambda([this]{Owner->ResumeTraining();return FReply::Handled();})[Text(TEXT("开始游戏  →"),25,FLinearColor(.01,.025,.035))]]];
  ChildSlot[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
   [SNew(SBox).WidthOverride(1180).HeightOverride(660)
    [SNew(SOverlay)
     +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(28,20)
      [SNew(SVerticalBox)
       +SVerticalBox::Slot().AutoHeight()[Text(TEXT("F I R E L I N E"),32)]
       +SVerticalBox::Slot().AutoHeight().Padding(0,6)[Text(TEXT("火力对决  /  大厅"),13,Cyan)]]
     +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(28,20)
      [SNew(SBox).WidthOverride(52).HeightOverride(52)[SNew(SButton).ButtonStyle(&MenuButtonStyle).ButtonColorAndOpacity(Panel).HAlign(HAlign_Center).VAlign(VAlign_Center)
       .ToolTipText(FText::FromString(TEXT("设置"))).OnClicked_Lambda([this]{OpenSettings();return FReply::Handled();})[SNew(SBox).WidthOverride(28).HeightOverride(28)[SNew(SRangeGear)]]]]
     +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Bottom).Padding(28,28)
      [SNew(SBox).WidthOverride(305)[Modes]]
     +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Bottom).Padding(28,28)
      [SNew(SVerticalBox)
       +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,13)[Text(TEXT("就绪  /  READY"),12,Cyan)]
       +SVerticalBox::Slot().AutoHeight()[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().Padding(0,0,10,0)[SNew(SButton).ButtonStyle(&MenuButtonStyle).ButtonColorAndOpacity(Panel).ContentPadding(FMargin(16,10)).OnClicked_Lambda([this]{Page=2;Refresh();return FReply::Handled();})[Text(TEXT("制作与素材"),12,Muted)]]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SButton).ButtonStyle(&MenuButtonStyle).ButtonColorAndOpacity(Panel).ContentPadding(FMargin(16,10)).OnClicked_Lambda([this]{UKismetSystemLibrary::QuitGame(Owner.Get(),Owner.Get(),EQuitPreference::Quit,false);return FReply::Handled();})[Text(TEXT("退出"),12,Muted)]]]]
    ]]];
  if(Owner.IsValid()&&Owner->MenuWidget.IsValid())FocusFirst();
  return;
 }

 auto Home=SNew(SVerticalBox);
 Home->AddSlot().AutoHeight()[Text(TEXT("F I R E L I N E"),54,FLinearColor(.91,.97,1))];
 Home->AddSlot().AutoHeight().Padding(2,10,0,0)[Text(TEXT("火 力 对 决"),19,Cyan)];
 Home->AddSlot().AutoHeight().Padding(2,20,0,42)[Text(bPausedMenu?TEXT("训练已暂停"):TEXT("移动。瞄准。掌控节奏。"),16,Muted)];
 Home->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(bPausedMenu?TEXT("继续训练  →"):TEXT("进入训练场  →"),[this]{Owner->ResumeTraining();return FReply::Handled();},true)];
 Home->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("设置"),[this]{OpenSettings();return FReply::Handled();})];
 if(bPausedMenu)Home->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("返回主菜单"),[this]{Owner->ShowMenu(false);return FReply::Handled();})];
 else Home->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("制作与素材"),[this]{Page=2;Refresh();return FReply::Handled();})];
 Home->AddSlot().AutoHeight()[Button(TEXT("退出游戏"),[this]{if(Owner.IsValid())UKismetSystemLibrary::QuitGame(Owner.Get(),Owner.Get(),EQuitPreference::Quit,false);return FReply::Handled();})];
 auto Info=SNew(SVerticalBox);
 Info->AddSlot().AutoHeight().Padding(0,0,0,22)[Text(TEXT("制作与素材"),30)];
 for(const FString& Line:{TEXT("角色  /  Ryan · rcorre"),TEXT("备用角色素材  /  Drazen · drazenk"),TEXT("步枪  /  TastyTony · Pichuliru"),TEXT("HK MP7  /  TastyTony · CC BY 4.0"),TEXT("MP7 动作  /  TheParaziT · CC BY 4.0"),TEXT("Coyote 瞄具  /  GoldbergR · CC BY 4.0"),TEXT("历史手部与动作适配参考  /  WRAD · wwwriks"),TEXT("爪刀  /  KrazzyElf"),TEXT("手部模型与接触姿势  /  DJMaesen"),TEXT("M4 动作  /  sycgff · CC BY 4.0"),TEXT("爪刀动作参考  /  Artem Dubinin")})Info->AddSlot().AutoHeight().Padding(0,0,0,18)[Text(Line,17)];
 Info->AddSlot().AutoHeight().Padding(0,12,0,28)[SNew(STextBlock).Text(FText::FromString(TEXT("角色、武器与动作的改编和整合由本项目完成。\n各素材署名与许可说明保留在游戏随附的 Legal 目录。"))).Font(Font(14)).ColorAndOpacity(Muted).AutoWrapText(true)];
 Info->AddSlot().AutoHeight()[Button(TEXT("返回"),[this]{Page=0;Refresh();return FReply::Handled();})];
 TSharedRef<SWidget> Content=Page==1?Settings():Page==2?StaticCastSharedRef<SWidget>(Info):StaticCastSharedRef<SWidget>(Home);
 ChildSlot[SNew(SOverlay)
  +SOverlay::Slot()[SNew(SBorder).BorderImage(&Fill).BorderBackgroundColor(FLinearColor(.008,.017,.028,.68))]
  +SOverlay::Slot().Padding(32)[SNew(SScaleBox).Stretch(EStretch::ScaleToFit)
   [SNew(SBox).WidthOverride(600).HeightOverride(Page==1?760:650)[SNew(SBorder).BorderImage(&Fill).BorderBackgroundColor(Panel).Padding(32)
    [SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)+SScrollBox::Slot()[Content]]]]]];
 if(Owner.IsValid()&&Owner->MenuWidget.IsValid())FocusFirst();
}

void SRangeMenu::FocusFirst()
{
 if(!FSlateApplication::IsInitialized())return;
 if(StartButton.IsValid()){FSlateApplication::Get().SetKeyboardFocus(StartButton,EFocusCause::SetDirectly);return;}
 TFunction<TSharedPtr<SWidget>(TSharedRef<SWidget>)> Find=[&](TSharedRef<SWidget> W)->TSharedPtr<SWidget>
 {
  if(!W->GetVisibility().IsVisible()||!W->IsEnabled())return nullptr;
  if(W->SupportsKeyboardFocus())return W;
  auto* Children=W->GetChildren();for(int I=0;I<Children->Num();++I)if(auto Found=Find(Children->GetChildAt(I)))return Found;
  return nullptr;
 };
 if(auto First=Find(ChildSlot.GetWidget()))FSlateApplication::Get().SetKeyboardFocus(First,EFocusCause::SetDirectly);
}

void ARangePlayerController::InitializeMenu()
{
 PrimaryActorTick.bTickEvenWhenPaused=true;bShouldPerformFullTickWhenPaused=true;
 if(auto* S=UGameUserSettings::GetGameUserSettings())
 {
  Preferences.Quality=S->GetOverallScalabilityLevel();Preferences.Fullscreen=S->GetFullscreenMode()!=EWindowMode::Windowed;Preferences.VSync=S->IsVSyncEnabled();
 }
 GConfig->GetFloat(PrefSection,TEXT("Sensitivity"),Preferences.Sensitivity,GGameUserSettingsIni);
 GConfig->GetFloat(PrefSection,TEXT("Volume"),Preferences.Volume,GGameUserSettingsIni);
 if(auto Saved=ReadPreferences())
 {
  double Value=0;
  if(Saved->TryGetNumberField(TEXT("sensitivity"),Value))Preferences.Sensitivity=Value;
  if(Saved->TryGetNumberField(TEXT("volume"),Value))Preferences.Volume=Value;
  if(Saved->TryGetNumberField(TEXT("quality"),Value))Preferences.Quality=FMath::Clamp(int32(Value),-1,3);
  Saved->TryGetBoolField(TEXT("fullscreen"),Preferences.Fullscreen);Saved->TryGetBoolField(TEXT("vsync"),Preferences.VSync);
  Saved->TryGetBoolField(TEXT("coyoteSight"),Preferences.CoyoteSight);
  if(Saved->TryGetNumberField(TEXT("padLookSpeed"),Value))Preferences.PadLookSpeed=Value;
  if(Saved->TryGetNumberField(TEXT("padADSScale"),Value))Preferences.PadADSScale=Value;
  if(Saved->TryGetNumberField(TEXT("padMoveDeadZone"),Value))Preferences.PadMoveDeadZone=Value;
  if(Saved->TryGetNumberField(TEXT("padLookDeadZone"),Value))Preferences.PadLookDeadZone=Value;
  Saved->TryGetBoolField(TEXT("padInvertY"),Preferences.PadInvertY);
 }
 ApplyPreferences(Preferences,false);ShowMenu(false);
}
void ARangePlayerController::ShowMenu(bool PauseMenu)
{
 if(!IsLocalController()||!GEngine||!GEngine->GameViewport)return;
 ResetGamepad();StopRawMouse();
 if(PlayerInput)PlayerInput->FlushPressedKeys();
 if(!MenuWidget.IsValid())
 {
  SAssignNew(MenuWidget,SRangeMenu).Controller(this);GEngine->GameViewport->AddViewportWidgetContent(MenuWidget.ToSharedRef(),100);
  SetIgnoreLookInput(true);SetIgnoreMoveInput(true);
 }
 if(!PauseMenu&&!Lobby)Lobby=GetWorld()->SpawnActor<ARangeLobby>(FVector(0,0,-20000),FRotator::ZeroRotator);
 if(Lobby)Lobby->Activate(!PauseMenu);
 SetViewTarget(PauseMenu?GetPawn():static_cast<AActor*>(Lobby.Get()));
 MenuWidget->ShowHome(PauseMenu);bShowMouseCursor=true;
 FInputModeUIOnly Mode;Mode.SetWidgetToFocus(MenuWidget);Mode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);SetInputMode(Mode);SetPause(true);
 MenuWidget->FocusFirst();
}
void ARangePlayerController::ResumeTraining()
{
 if(MenuWidget.IsValid()&&GEngine&&GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
 if(Lobby)Lobby->Activate(false);SetViewTarget(GetPawn());
 MenuWidget.Reset();SetPause(false);ResetIgnoreLookInput();ResetIgnoreMoveInput();bShowMouseCursor=false;bTrainingStarted=true;
 ResetGamepad();StopRawMouse();
 if(PlayerInput)PlayerInput->FlushPressedKeys();
 FInputModeGameOnly Mode;Mode.SetConsumeCaptureMouseDown(true);SetInputMode(Mode);
}
bool ARangePlayerController::ApplyPreferences(const FRangePreferences& V,bool Save)
{
 Preferences=V;Preferences.PadLookSpeed=FMath::Clamp(V.PadLookSpeed,60.f,540.f);Preferences.PadADSScale=FMath::Clamp(V.PadADSScale,.15f,1.f);Preferences.PadMoveDeadZone=FMath::Clamp(V.PadMoveDeadZone,0.f,.35f);Preferences.PadLookDeadZone=FMath::Clamp(V.PadLookDeadZone,0.f,.35f);Preferences.Sensitivity=FMath::Clamp(V.Sensitivity,.005f,.15f);Preferences.Volume=FMath::Clamp(V.Volume,0.f,1.f);
 if(auto* P=Cast<ARangeCharacter>(GetPawn()))
 {
  P->SetMouseSensitivity(Preferences.Sensitivity);
  if(!P->SetCoyoteSight(Preferences.CoyoteSight)){Preferences.CoyoteSight=false;UE_LOG(LogTemp,Warning,TEXT("FIRELINE_SIGHT_ASSET_UNAVAILABLE"));}
 }
 if(auto Device=GetWorld()->GetAudioDevice())Device->SetTransientPrimaryVolume(Preferences.Volume);
 if(auto* S=UGameUserSettings::GetGameUserSettings())
 {
  if(V.Quality>=0)S->SetOverallScalabilityLevel(FMath::Clamp(V.Quality,0,3));
  S->SetVSyncEnabled(V.VSync);
  S->SetFullscreenMode(V.Fullscreen?EWindowMode::WindowedFullscreen:EWindowMode::Windowed);
  S->ApplySettings(false);
  if(Save)S->SaveSettings();
 }
 if(Save)
 {
  auto Object=MakeShared<FJsonObject>();Object->SetNumberField(TEXT("sensitivity"),Preferences.Sensitivity);Object->SetNumberField(TEXT("volume"),Preferences.Volume);Object->SetNumberField(TEXT("quality"),Preferences.Quality);Object->SetBoolField(TEXT("fullscreen"),Preferences.Fullscreen);Object->SetBoolField(TEXT("vsync"),Preferences.VSync);
  Object->SetBoolField(TEXT("coyoteSight"),Preferences.CoyoteSight);
  Object->SetNumberField(TEXT("padLookSpeed"),Preferences.PadLookSpeed);Object->SetNumberField(TEXT("padADSScale"),Preferences.PadADSScale);Object->SetNumberField(TEXT("padMoveDeadZone"),Preferences.PadMoveDeadZone);Object->SetNumberField(TEXT("padLookDeadZone"),Preferences.PadLookDeadZone);Object->SetBoolField(TEXT("padInvertY"),Preferences.PadInvertY);
  FString Data;FJsonSerializer::Serialize(Object,TJsonWriterFactory<>::Create(&Data));IFileManager::Get().MakeDirectory(*FPaths::GetPath(PreferencePath()),true);
  if(!FFileHelper::SaveStringToFile(Data,*PreferencePath())){UE_LOG(LogTemp,Error,TEXT("FIRELINE_SETTINGS_SAVE_FAILED"));return false;}
  UE_LOG(LogTemp,Display,TEXT("FIRELINE_SETTINGS_SAVED sensitivity=%.4f volume=%.2f quality=%d fullscreen=%d vsync=%d"),Preferences.Sensitivity,Preferences.Volume,Preferences.Quality,Preferences.Fullscreen,Preferences.VSync);
 }
 return true;
}
void ARangePlayerController::EndPlay(const EEndPlayReason::Type Reason)
{
 if(MenuWidget.IsValid()&&GEngine&&GEngine->GameViewport)GEngine->GameViewport->RemoveViewportWidgetContent(MenuWidget.ToSharedRef());
 ResetGamepad();StopRawMouse();RawMouse.Reset();MenuWidget.Reset();Super::EndPlay(Reason);
}
void ARangePlayerController::TickMenuAudit(float Dt)
{
 MenuAuditTime+=Dt;
 // Measure the interval: two endpoints can coincide on a breathing cycle.
 if(MenuAuditStep==1&&Lobby)LobbyAuditBreathMax=FMath::Max(LobbyAuditBreathMax,FMath::RadiansToDegrees(LobbyAuditChest.AngularDistance(Lobby->Character->GetBoneQuaternion(TEXT("Chest")))));
 auto Shot=[this](const TCHAR* N){FString Dir=FPaths::ProjectSavedDir()/TEXT("Screenshots");FParse::Value(FCommandLine::Get(),TEXT("FirelineMenuCaptureDir="),Dir);IFileManager::Get().MakeDirectory(*Dir,true);FScreenshotRequest::RequestScreenshot(Dir/FString::Printf(TEXT("menu-%s.png"),N),true,false);};
 if(MenuAuditStep==0&&MenuAuditTime>3){check(Lobby&&GetViewTarget()==Lobby.Get());LobbyAuditChest=Lobby->Character->GetBoneQuaternion(TEXT("Chest"));Shot(TEXT("home"));++MenuAuditStep;}
 else if(MenuAuditStep==1&&MenuAuditTime>4.5f){LobbyAuditHead=Lobby->Character->GetBoneQuaternion(TEXT("Head"));const float Breath=LobbyAuditBreathMax;check(Breath>.1f);UE_LOG(LogTemp,Display,TEXT("LOBBY_BREATH_AUDIT chest_delta=%.3f"),Breath);Shot(TEXT("look-left"));++MenuAuditStep;}
 else if(MenuAuditStep==2&&MenuAuditTime>9.5f){const float Angle=FMath::RadiansToDegrees(LobbyAuditHead.AngularDistance(Lobby->Character->GetBoneQuaternion(TEXT("Head"))));check(Angle>5);UE_LOG(LogTemp,Display,TEXT("LOBBY_MOTION_AUDIT head_delta=%.2f paused=%d"),Angle,IsPaused());Shot(TEXT("look-right"));++MenuAuditStep;}
 else if(MenuAuditStep==3&&MenuAuditTime>10.5f){MenuWidget->OpenSettings();++MenuAuditStep;}
 else if(MenuAuditStep==4&&MenuAuditTime>11.5f){Shot(TEXT("settings"));++MenuAuditStep;}
 else if(MenuAuditStep==5&&MenuAuditTime>12.5f)
 {
  const auto Original=Preferences;auto Probe=Preferences;Probe.Sensitivity=.0615f;Probe.Volume=.37f;Probe.Quality=-1;Probe.PadLookSpeed=315;Probe.PadADSScale=.6f;Probe.PadMoveDeadZone=.18f;Probe.PadLookDeadZone=.14f;Probe.PadInvertY=true;ApplyPreferences(Probe);
  auto Saved=ReadPreferences();double Sense=0,Volume=0;if(Saved){Saved->TryGetNumberField(TEXT("sensitivity"),Sense);Saved->TryGetNumberField(TEXT("volume"),Volume);}
  const bool Verified=FMath::IsNearlyEqual(float(Sense),.0615f)&&FMath::IsNearlyEqual(float(Volume),.37f)&&FMath::IsNearlyEqual(CastChecked<ARangeCharacter>(GetPawn())->GetMouseSensitivity(),.0615f);
  check(Verified);
  check(Saved&&Saved->GetNumberField(TEXT("padLookSpeed"))==315&&FMath::IsNearlyEqual(float(Saved->GetNumberField(TEXT("padADSScale"))),.6f)&&Saved->GetBoolField(TEXT("padInvertY")));
  for(bool RedDot:{false,true})
  {
   Probe.CoyoteSight=RedDot;check(ApplyPreferences(Probe));
   const auto Stored=ReadPreferences();bool Selected=!RedDot;
   check(Stored&&Stored->TryGetBoolField(TEXT("coyoteSight"),Selected)&&Selected==RedDot);
   check(CastChecked<ARangeCharacter>(GetPawn())->UsesCoyoteSight()==RedDot);
  }
  ApplyPreferences(Original);UE_LOG(LogTemp,Display,TEXT("FIRELINE_MENU_SETTINGS_AUDIT saved_and_applied=%d optic_iron_and_coyote=1"),Verified);ResumeTraining();check(GetViewTarget()==GetPawn()&&Lobby->IsHidden());++MenuAuditStep;
 }
 else if(MenuAuditStep==6&&MenuAuditTime>13.5f){Shot(TEXT("playing"));++MenuAuditStep;}
 else if(MenuAuditStep==7&&MenuAuditTime>14.5f){ShowMenu(true);++MenuAuditStep;}
 else if(MenuAuditStep==8&&MenuAuditTime>15.5f){Shot(TEXT("pause"));++MenuAuditStep;}
 else if(MenuAuditStep==9&&MenuAuditTime>16.5f){check(IsPaused()&&IsMenuOpen()&&bShowMouseCursor);ShowMenu(false);check(GetViewTarget()==Lobby.Get()&&!Lobby->IsHidden());++MenuAuditStep;}
 else if(MenuAuditStep==10&&MenuAuditTime>17.5f){Shot(TEXT("return-lobby"));++MenuAuditStep;}
 else if(MenuAuditStep==11&&MenuAuditTime>18.5f){ResumeTraining();check(GetViewTarget()==GetPawn()&&!IsPaused()&&!IsMenuOpen());UE_LOG(LogTemp,Display,TEXT("FIRELINE_LOBBY_AUDIT_PASS settings=1 training=1 pause=1 return_lobby=1"));FPlatformMisc::RequestExit(false);++MenuAuditStep;}
}
