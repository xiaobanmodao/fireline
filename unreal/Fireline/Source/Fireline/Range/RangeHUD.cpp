#include "RangeGame.h"
#include "RangePlayerController.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "CanvasItem.h"
#include "GeomTools.h"
#include "Styling/CoreStyle.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include <initializer_list>

ARangeHUD::ARangeHUD()
{
 static ConstructorHelpers::FObjectFinder<UTexture2D> M4(TEXT("/Game/Fireline/UI/T_Weapon_M4.T_Weapon_M4"));
 static ConstructorHelpers::FObjectFinder<UTexture2D> MP7(TEXT("/Game/Fireline/UI/T_Weapon_MP7.T_Weapon_MP7"));
 static ConstructorHelpers::FObjectFinder<UTexture2D> Knife(TEXT("/Game/Fireline/UI/T_Weapon_Karambit.T_Weapon_Karambit"));
 RifleIcon=M4.Object;MP7Icon=MP7.Object;KarambitIcon=Knife.Object;
}

void ARangeHUD::DrawHUD()
{
 Super::DrawHUD();if(!Canvas)return;
 if(auto* PC=Cast<ARangePlayerController>(GetOwningPlayerController());PC&&PC->IsMenuOpen())return;
 auto* P=Cast<ARangeCharacter>(GetOwningPawn());if(!P)return;
 const float S=FMath::Max(.65f,FMath::Min(Canvas->SizeX/1280.f,Canvas->SizeY/720.f));
 const float W=Canvas->SizeX,H=Canvas->SizeY;
 const FLinearColor Panel(.018f,.025f,.036f,.82f),Cyan(.12f,.78f,1.f),White(.88f,.93f,.97f),Muted(.47f,.58f,.65f),Flesh(1.f,.3f,.25f);
 // Training statistics sit in a small header; persistent control instructions
 // belong in the menu, not across the combat view.
 DrawRect(Panel,22*S,20*S,214*S,43*S);
 DrawRect(Cyan,22*S,20*S,3*S,43*S);
 DrawText(TEXT("TRAINING RANGE"),White,34*S,25*S,nullptr,1.05f*S);
 DrawText(FString::Printf(TEXT("HITS %d   KILLS %d   ACC %.0f%%"),P->Hits,P->Kills,P->Shots?100.f*P->BulletHits/P->Shots:0.f),Muted,34*S,45*S,nullptr,.72f*S);
 // Apex-inspired edge composition: segmented armor over a thin health bar,
 // an original pilot insignia, and one large active-magazine readout.
 // All coordinates below are logical 1280x720 units, scaled once by S.
 auto Poly=[&](float OX,float OY,std::initializer_list<FVector2D> Points,FLinearColor Color)
 {
  TArray<FVector2D> V;for(const auto& Q:Points)V.Add(FVector2D(OX+Q.X*S,OY+Q.Y*S));
  TArray<FVector2D> Triangles;
  if(!FGeomTools2D::TriangulatePoly(Triangles,V))return;
  for(int I=0;I+2<Triangles.Num();I+=3)
  {
   FCanvasTriangleItem Item(Triangles[I],Triangles[I+1],Triangles[I+2],GWhiteTexture);
   Item.SetColor(Color);Item.BlendMode=SE_BLEND_Translucent;Canvas->DrawItem(Item);
  }
 };
 auto Plate=[&](float PX,float PY,float PW,float PH,FLinearColor C,float Cut=9.f)
 {Poly(PX,PY,{{Cut,0},{PW,0},{PW,PH-Cut},{PW-Cut,PH},{0,PH},{0,Cut}},C);};
 auto Text=[&](const FString& T,FLinearColor C,float PX,float PY,float Size=1.f)
 {
  const int FontSize=FMath::RoundToInt(FMath::Max(7.f,9.f*Size)*S);
  FCanvasTextItem Item(FVector2D(PX,PY),FText::FromString(T),GEngine->GetSmallFont(),C);
  Item.SlateFontInfo=FCoreStyle::GetDefaultFontStyle(Size>2.f?"Bold":"Regular",FontSize);
  Item.EnableShadow(FLinearColor(0,0,0,.55f),FVector2D(S,S));Canvas->DrawItem(Item);
 };
 const FLinearColor Edge(.36f,.46f,.52f,.65f),Track(.08f,.13f,.17f,.95f),Amber(1.f,.68f,.22f);
 const float LX=26*S,LY=H-105*S;
 Plate(LX,LY,306,81,Edge);Plate(LX+S,LY+S,304,79,Panel);
 Plate(LX+4*S,LY+4*S,58,73,FLinearColor(.07,.18,.24,.95));
 // Project-authored helmet badge; no imported Apex portrait or artwork.
 Poly(LX,LY,{{20,18},{43,18},{49,27},{46,48},{32,57},{18,48},{15,27}},FLinearColor(.52,.66,.72));
 Poly(LX,LY,{{19,29},{45,29},{43,38},{21,38}},Cyan);
 Poly(LX,LY,{{26,43},{38,43},{36,50},{28,50}},Track);
 DrawRect(Cyan,LX+18*S,LY+65*S,29*S,2*S);
 Text(TEXT("PILOT"),White,LX+74*S,LY+8*S,.95f);
 Text(TEXT("YOU"),Muted,LX+263*S,LY+9*S,.66f);
 const float BarX=LX+74*S,BarY=LY+33*S,SegmentW=46.f,SegmentGap=3.f;
 const float Shield=FMath::Clamp(P->PlayerVitals.Shield,0.f,100.f);
 for(int I=0;I<4;++I)
 {
  const float PX=BarX+I*(SegmentW+SegmentGap)*S;
  DrawRect(Edge,PX,BarY,SegmentW*S,12*S);
  DrawRect(Track,PX+S,BarY+S,(SegmentW-2)*S,10*S);
  const float Fill=FMath::Clamp((Shield-I*25.f)/25.f,0.f,1.f);
  if(Fill>0) {DrawRect(Cyan,PX+S,BarY+S,(SegmentW-2)*S*Fill,10*S);DrawRect(FLinearColor(.65,.94,1),PX+S,BarY+S,(SegmentW-2)*S*Fill,2*S);}
 }
 Text(FString::Printf(TEXT("%d"),FMath::CeilToInt(Shield)),Shield>0?Cyan:Muted,LX+272*S,BarY,.74f);
 const float Health=FMath::Clamp(P->PlayerVitals.Health,0.f,100.f);
 DrawRect(Track,BarX,LY+52*S,193*S,7*S);
 DrawRect(Health<=25?Flesh:White,BarX,LY+52*S,193*S*Health/100.f,7*S);
 Text(FString::Printf(TEXT("%d"),FMath::CeilToInt(Health)),Health<=25?Flesh:White,LX+272*S,LY+48*S,.74f);
 Text(Health<=25?TEXT("LOW HEALTH"):Shield<=0?TEXT("SHIELD DEPLETED"):TEXT("SHIELD  /  HEALTH"),Health<=25?Flesh:Muted,BarX,LY+65*S,.58f);
 if(Health<=25)
 {const float Pulse=.35f+.3f*(.5f+.5f*FMath::Sin(GetWorld()->GetTimeSeconds()*5.f));DrawRect(FLinearColor(1,.2,.15,Pulse),LX+74*S,LY+77*S,218*S,2*S);}
 const float RX=W-310*S,RY=H-116*S;
 const int Slot=P->WeaponSlot();
 const TCHAR* Names[]={TEXT("M4A1"),TEXT("HK MP7"),TEXT("KNIFE")};
 const int Slots[]={1,3,2};
 for(int I=0;I<3;++I)
 {
  const bool Active=Slot==Slots[I];const float PX=RX+I*96*S;
  Plate(PX,RY-27*S,92,23,Active?FLinearColor(.07,.23,.29,.94):Panel,5);
  Text(FString::Printf(TEXT("%d"),Slots[I]),Active?Cyan:Muted,PX+8*S,RY-23*S,.7f);
  Text(Names[I],Active?White:Muted,PX+25*S,RY-23*S,.66f);
  if(Active)DrawRect(Cyan,PX+6*S,RY-6*S,80*S,S);
 }
 Plate(RX,RY,284,92,Edge);Plate(RX+S,RY+S,282,90,Panel);
 DrawRect(Cyan,RX+13*S,RY+11*S,3*S,16*S);
 const FString Name=P->bKnife?(P->bKarambitReady?TEXT("KARAMBIT"):TEXT("M9")):P->bEmptyHands?TEXT("UNARMED"):P->bMP7?TEXT("HK MP7"):TEXT("M4A1");
 Text(Name,White,RX+23*S,RY+10*S,1.05f);
 const bool Gun=!P->bKnife&&!P->bEmptyHands;
 // These alpha silhouettes are orthographic renders of the selected game
 // meshes. Contain-fit preserves their real side-view aspect ratios.
 UTexture2D* Icon=Gun?(P->bMP7?MP7Icon.Get():RifleIcon.Get()):(P->bKnife&&P->bKarambitReady?KarambitIcon.Get():nullptr);
 if(Icon)
 {
  const float Fit=FMath::Min(142.f/Icon->GetSizeX(),42.f/Icon->GetSizeY())*S;
  const float IW=Icon->GetSizeX()*Fit,IH=Icon->GetSizeY()*Fit;
  DrawTexture(Icon,RX+14*S+(142*S-IW)*.5f,RY+34*S+(42*S-IH)*.5f,IW,IH,0,0,1,1,White,BLEND_Translucent);
 }
 if(Gun)
 {
  const FLinearColor AmmoColor=P->Ammo==0?Flesh:P->Ammo<=P->MagazineCapacity()/4?Amber:White;
  Text(FString::Printf(TEXT("%02d"),P->Ammo),AmmoColor,RX+174*S,RY+26*S,3.1f);
  Text(FString::Printf(TEXT("/ %d"),P->MagazineCapacity()),Muted,RX+243*S,RY+47*S,.85f);
  Text(TEXT("MAG"),Muted,RX+243*S,RY+65*S,.52f);
  Text(P->bReloading?TEXT("RELOADING"):TEXT("AUTO"),P->bReloading?Amber:Muted,RX+16*S,RY+77*S,.62f);
 }
 else
 {
  Text(P->bKnife?TEXT("MELEE"):TEXT("HANDS FREE"),Muted,RX+172*S,RY+46*S,.88f);
 }
 const float X=W*.5f,Y=H*.5f;
 if(P->AimAlpha<.9f)
 {
  const float Projected=FMath::Tan(FMath::DegreesToRadians(P->GetShotSpreadAngle()))*Canvas->SizeX*.5f/FMath::Tan(FMath::DegreesToRadians(P->Camera->FieldOfView*.5f));
  const float Gap=(P->bKnife||P->bEmptyHands)?4.f:FMath::Max(3.f,Projected);
  const FLinearColor C(.85,1,1);
  DrawLine(X-Gap-5,Y,X-Gap,Y,C);DrawLine(X+Gap,Y,X+Gap+5,Y,C);
  DrawLine(X,Y-Gap-5,X,Y-Gap,C);DrawLine(X,Y+Gap,X,Y+Gap+5,C);
 }
 if(P->HitFlash>0)
 {
  const FLinearColor C=P->bLastKill?FLinearColor(1,.3,.12):P->bLastShieldHit?FLinearColor(.1,.85,1):P->bLastHeadshot?FLinearColor(1,.75,.2):FLinearColor::White;
  const float Radius=P->bLastShieldBreak?16*S:12*S;
  for(float SX:{-1.f,1.f})for(float SY:{-1.f,1.f})DrawLine(X+SX*7*S,Y+SY*7*S,X+SX*Radius,Y+SY*Radius,C,P->bLastShieldBreak?2.5f:2.f);
  if(P->bLastShieldBreak)for(int I=0;I<4;++I)
  {const float A=I*PI*.5f+.32f;DrawLine(X+FMath::Cos(A)*20*S,Y+FMath::Sin(A)*20*S,X+FMath::Cos(A+.17f)*27*S,Y+FMath::Sin(A+.17f)*27*S,C,1.3f);}
 }
 if(P->FeedbackLeft>0)
 {
  const FString Label=P->bLastKill?(P->bLastHeadshot?TEXT("HEADSHOT  /  ELIMINATED"):TEXT("ELIMINATED")):FString::Printf(TEXT("%s  %d"),P->bLastShieldBreak?TEXT("SHIELD BROKEN"):P->bLastShieldHit?TEXT("SHIELD"):TEXT("HIT"),P->LastDamage);
  float Width=0,Height=0;GetTextSize(Label,Width,Height);
  FLinearColor Tint=P->bLastKill?FLinearColor(1,.65,.25):P->bLastShieldHit?Cyan:Flesh;Tint.A=FMath::Min(1.f,P->FeedbackLeft/.2f);
  DrawText(Label,Tint,X-Width*.5f,Y+35*S);
 }
 if(P->PlayerShieldPulse>0)
 {const float A=P->PlayerShieldPulse*.23f;DrawRect(FLinearColor(.1,.7,1,A),0,0,4*S,H);DrawRect(FLinearColor(.1,.7,1,A),W-4*S,0,4*S,H);}
 for(TActorIterator<ARangeTarget> It(GetWorld());It;++It)
 {
  if(It->FeedbackLeft<=0||It->GetHealth()<=0)continue;
  FVector2D Screen;
  if(GetOwningPlayerController()->ProjectWorldLocationToScreen(It->Body->GetSocketLocation(TEXT("Head"))+FVector(0,0,27),Screen))
  {
   const float Distance=FVector::Distance(P->Camera->GetComponentLocation(),It->Body->GetSocketLocation(TEXT("Head")));
   // Screen-space size is deliberately capped smaller nearby, where a wide
   // floating bar would cover the enemy's face and weapon.
   const float BarW=FMath::Clamp(38.f+.025f*(Distance-200.f),38.f,62.f)*S;
   // 1 px border + 3 shield + 1 separator + 3 health + 1 border.
   const float BarH=9.f*S;
   const float Left=Screen.X-BarW*.5f,Top=Screen.Y-BarH;
   DrawRect(Panel,Left,Top,BarW,BarH);
   DrawRect(Cyan,Left+S,Top+S,(BarW-2*S)*It->GetShield()/100.f,3*S);
   DrawRect(Flesh,Left+S,Top+BarH-4*S,(BarW-2*S)*It->GetHealth()/100.f,3*S);
  }
 }

}
