#include "LiveCarryStudy.h"
#include "AlsCharacter.h"
#include "AlsCharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimNode_StateMachine.h"

void FLiveCarryStudy::ReadRifleStates()
{
    for(auto* Instance:static_cast<const USkeletalMeshComponent*>(Source->GetMesh())->GetLinkedAnimInstances())
        if(Instance&&Instance->GetClass()->GetName()==TEXT("AB_Als_Rifle_C"))
        {
            const auto* Machine=Instance->GetStateMachineInstanceFromName(TEXT("Rifle States"));
            checkf(Machine,TEXT("Pinned Rifle state machine missing"));
            const int L=Machine->GetStateIndex(TEXT("Relaxed")),R=Machine->GetStateIndex(TEXT("Ready")),A=Machine->GetStateIndex(TEXT("Aiming"));
            check(L!=INDEX_NONE&&R!=INDEX_NONE&&A!=INDEX_NONE);
            RelaxedWeight=Machine->GetStateWeight(L);ReadyWeight=Machine->GetStateWeight(R);AimingWeight=Machine->GetStateWeight(A);
            Pawn->RifleState=Instance->GetCurrentStateName(Instance->GetStateMachineIndex(TEXT("Rifle States"))).ToString();
            return;
        }
    UE_LOG(LogTemp,Fatal,TEXT("Pinned Rifle linked instance missing"));
}

void FLiveCarryStudy::AimInput(UWorld* W,float Dt,double Time)
{
    auto* P=Pawn.Get();auto* PC=W->GetFirstPlayerController();FVector Local=FVector::ZeroVector;
    if(Audit)
    {
        const double T=FMath::Max(0.,Time-1.);P->Scenario=FMath::Min(13,FMath::FloorToInt(T/4));const double Phase=T-P->Scenario*4;
        P->AimRequested=Time>=1&&Phase>.5&&Phase<3.;P->JogRequested=false;P->AimPitch=0;
        if(P->Scenario==1||P->Scenario==13)P->AimRequested=false;
        if(P->Scenario>=2&&P->Scenario<=9){static const float Angles[]={0,-45,-90,-135,180,135,90,45};if((Phase>.25&&Phase<1.45)||(Phase>3.1&&Phase<3.4))Local=FRotator(0,Angles[P->Scenario-2],0).Vector();P->JogRequested=!P->AimRequested;}
        if(P->Scenario==10)P->AimRequested=FMath::Fmod(Phase,.4)<.2;
        if(P->Scenario==11){P->AimRequested=true;P->AimPitch=30*FMath::Sin(Phase*UE_PI*.5);}
        if(P->Scenario==12){P->AimRequested=true;Local=FVector(0,1,0);P->FacingYaw=FMath::UnwindDegrees(P->FacingYaw+60*Dt);P->AimPitch=20*FMath::Sin(Phase*UE_PI*.5);}
    }
    else
    {
        Local=FVector((PC->IsInputKeyDown(EKeys::W)?1:0)-(PC->IsInputKeyDown(EKeys::S)?1:0),(PC->IsInputKeyDown(EKeys::D)?1:0)-(PC->IsInputKeyDown(EKeys::A)?1:0),0);
        P->AimRequested=PC->IsInputKeyDown(EKeys::RightMouseButton);
        P->JogRequested=!P->AimRequested&&(PC->IsInputKeyDown(EKeys::LeftShift)||PC->IsInputKeyDown(EKeys::RightShift));
        P->FacingYaw=FMath::UnwindDegrees(P->FacingYaw+((PC->IsInputKeyDown(EKeys::E)?1:0)-(PC->IsInputKeyDown(EKeys::Q)?1:0))*120*Dt);
        P->AimPitch=FMath::Clamp(P->AimPitch+((PC->IsInputKeyDown(EKeys::Up)?1:0)-(PC->IsInputKeyDown(EKeys::Down)?1:0))*30*Dt,-30.f,30.f);
    }
    P->MoveRequested=FRotator(0,P->FacingYaw,0).RotateVector(Local.GetClampedToMaxSize(1));P->ForwardRequested=!P->MoveRequested.IsNearlyZero();
    const auto& Gait=CastChecked<UAlsCharacterMovementComponent>(Source->GetCharacterMovement())->GetGaitSettings();
    P->GetCharacterMovement()->MaxWalkSpeed=(P->JogRequested?Gait.GetMaxRunSpeed():Gait.GetMaxWalkSpeed())*Retarget.LegScale;
    if(P->ForwardRequested)P->AddMovementInput(P->MoveRequested,1);
}
