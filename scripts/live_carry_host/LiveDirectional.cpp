#include "LiveCarryStudy.h"
#include "AlsCharacter.h"
#include "AlsCharacterMovementComponent.h"
#include "State/AlsPoseState.h"
#include "Utility/AlsGameplayTags.h"
#include "GameFramework/PlayerController.h"
#include "UObject/UnrealType.h"
#include "AnimNodes/AnimNode_TwoWayBlend.h"

void ALiveCarryPawn::RequestTap(const FVector& Direction)
{
    if(!Directional||!Ready||Audit)return;
    AddMovementInput(FRotator(0,FacingYaw,0).RotateVector(Direction),1);
}
void ALiveCarryPawn::LeftDown(){RequestTap(FVector(0,-1,0));}
void ALiveCarryPawn::RightDown(){RequestTap(FVector(0,1,0));}
void ALiveCarryPawn::BackDown(){RequestTap(FVector(-1,0,0));}

void FLiveCarryStudy::DirectionalInput(UWorld* W,float Dt,double Time)
{
    auto* P=Pawn.Get();auto* PC=W->GetFirstPlayerController();FVector Local=FVector::ZeroVector;
    if(Audit)
    {
        const double T=FMath::Max(0.,Time-1.);const int Case=T<45.6?FMath::FloorToInt(T/2.4):T<64.8?19+FMath::FloorToInt((T-45.6)/4.8):23;const double Phase=T-(Case<19?Case*2.4:Case<23?45.6+(Case-19)*4.8:64.8);P->Scenario=Case;
        P->JogRequested=Case>=8&&Case<16;
        if(Time>=1&&Case<16&&Phase>=.25&&Phase<1.9)
        {
            static const float Angles[]={0,-45,-90,-135,180,135,90,45};Local=FRotator(0,Angles[Case%8],0).Vector();
        }
        if(Case==16){Local=FVector::ForwardVector;P->JogRequested=Phase>.65&&Phase<1.75;}
        if(Case==17){Local=FVector(0,FMath::Fmod(Phase,.7)<.35?-1:1,0);P->JogRequested=true;}
        if(Case==18){Local=FRotator(0,65+60*Phase/2.4,0).Vector();P->JogRequested=false;}
        if(Case>=19&&Case<=22){const float Angles[]={90,180,0,-90};P->FacingYaw=FMath::FixedTurn(P->FacingYaw,Angles[Case-19],120*Dt);}
        if(Case==23){Local=FVector::ForwardVector;P->JogRequested=Phase>1;P->FacingYaw=FMath::UnwindDegrees(P->FacingYaw+60*Dt);}
    }
    else
    {
        Local=FVector((PC->IsInputKeyDown(EKeys::W)?1:0)-(PC->IsInputKeyDown(EKeys::S)?1:0),(PC->IsInputKeyDown(EKeys::D)?1:0)-(PC->IsInputKeyDown(EKeys::A)?1:0),0);
        P->JogRequested=PC->IsInputKeyDown(EKeys::LeftShift)||PC->IsInputKeyDown(EKeys::RightShift);
        const float Turn=(PC->IsInputKeyDown(EKeys::E)?1:0)-(PC->IsInputKeyDown(EKeys::Q)?1:0);P->FacingYaw=FMath::UnwindDegrees(P->FacingYaw+Turn*120*Dt);
    }
    P->MoveRequested=FRotator(0,P->FacingYaw,0).RotateVector(Local.GetClampedToMaxSize(1));P->ForwardRequested=!P->MoveRequested.IsNearlyZero();
    const auto& Gait=CastChecked<UAlsCharacterMovementComponent>(Source->GetCharacterMovement())->GetGaitSettings();
    P->GetCharacterMovement()->MaxWalkSpeed=(P->JogRequested?Gait.GetMaxRunSpeed():Gait.GetMaxWalkSpeed())*Retarget.LegScale;
    if(P->ForwardRequested)P->AddMovementInput(P->MoveRequested,1);
}

void FLiveCarryStudy::DirectionalSource(AAlsCharacter* S,ALiveCarryPawn* P,float Dt)
{
    // Feed the real mover into the original character's full state update.
    // Only the hidden evaluator owns this reflected state. Its movement tick
    // remains disabled, so Tick cannot advance a second physical character.
    auto* Accel=FindFProperty<FStructProperty>(S->GetCharacterMovement()->GetClass(),TEXT("Acceleration"));
    auto* View=FindFProperty<FStructProperty>(S->GetClass(),TEXT("ReplicatedViewRotation"));
    check(Accel&&View);
    *Accel->ContainerPtrToValuePtr<FVector>(S->GetCharacterMovement())=P->GetCharacterMovement()->GetCurrentAcceleration()/Retarget.LegScale;
    *View->ContainerPtrToValuePtr<FRotator>(S)=FRotator(0,P->FacingYaw,0);
    // This scalar normally updates during source movement physics. That tick is
    // deliberately disabled; reproduce its speed-range mapping for rotation speed.
    auto* Movement=CastChecked<UAlsCharacterMovementComponent>(S->GetCharacterMovement());
    const auto& Settings=Movement->GetGaitSettings();const float Speed=S->GetVelocity().Size2D();
    if(auto* Amount=FindFProperty<FFloatProperty>(Movement->GetClass(),TEXT("GaitAmount")))
        Amount->SetPropertyValue_InContainer(Movement,Speed>Settings.GetMaxWalkSpeed()?FMath::GetMappedRangeValueClamped(FVector2f(Settings.GetMaxWalkSpeed(),Settings.GetMaxRunSpeed()),FVector2f(1,2),Speed):FMath::GetMappedRangeValueClamped(FVector2f(0,Settings.GetMaxWalkSpeed()),FVector2f(0,1),Speed));
    S->SetDesiredGait(P->JogRequested?AlsGaitTags::Running:AlsGaitTags::Walking);
    // ALS uses this transient flag to decide whether to reset animation state.
    // Our hidden mesh is explicitly evaluated every frame, including NullRHI.
    S->GetMesh()->bRecentlyRendered=true;
    S->Tick(Dt);
    // ALS applies authored yaw-offset and turn-in-place curves to its actor.
    // Carry that rotation with the whole target, never with the waist alone.
    P->SetActorRotation(FRotator(0,S->GetActorRotation().Yaw,0));
}


double FLiveCarryStudy::DirectionalJogWeight()
{
    // AB_Als_Rifle Relaxed/TwoWayBlend_4 consumes GaitRunningAmount but has
    // asymmetric interpolation (instant rise, decay speed 5). Read the actual
    // evaluated node state; the compiled class renumbers this node as _1.
    // Validate its settings/input against the source state, not only its name.
    for(auto* Instance:static_cast<const USkeletalMeshComponent*>(Source->GetMesh())->GetLinkedAnimInstances())
        if(Instance&&Instance->GetClass()->GetName()==TEXT("AB_Als_Rifle_C"))
        {
            if(auto* Property=FindFProperty<FStructProperty>(Instance->GetClass(),TEXT("AnimGraphNode_TwoWayBlend_1")))
            {
                check(Property->Struct==FAnimNode_TwoWayBlend::StaticStruct());
                const auto* Node=Property->ContainerPtrToValuePtr<FAnimNode_TwoWayBlend>(Instance);
                check(Node->AlphaScaleBiasClamp.bInterpResult&&FMath::IsNearlyEqual(Node->AlphaScaleBiasClamp.InterpSpeedIncreasing,0.f)&&FMath::IsNearlyEqual(Node->AlphaScaleBiasClamp.InterpSpeedDecreasing,5.f));
                if(auto* StateProperty=FindFProperty<FStructProperty>(Source->GetMesh()->GetAnimInstance()->GetClass(),TEXT("PoseState")))
                {
                    const auto* State=StateProperty->ContainerPtrToValuePtr<FAlsPoseState>(Source->GetMesh()->GetAnimInstance());
                    checkf(FMath::Abs(Node->Alpha-State->GaitRunningAmount)<.001,TEXT("Pinned Rifle node does not follow source RunningAmount"));
                }
                return FMath::Clamp(double(Node->AlphaScaleBias.ApplyTo(Node->AlphaScaleBiasClamp.InterpolatedResult)),0.,1.);
            }
        }
    UE_LOG(LogTemp,Fatal,TEXT("Pinned ALS Rifle jog blend node missing; stop rather than guess its weight"));return 0;
}
