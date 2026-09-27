// Research host only. Original ALS assets and animation graph are never edited.
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "AlsCharacter.h"
#include "Utility/AlsGameplayTags.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "ReferenceMotionCapture.h"
#include "WholeCarryReview.h"

class FMotionReferenceModule : public FDefaultGameModuleImpl
{
    FDelegateHandle TickHandle;
    TWeakObjectPtr<AAlsCharacter> Initialized;
    FDelegateHandle BeforeTickHandle;
    FReferenceMotionCapture Capture;
    bool ReviewStarted=false;
public:
    virtual void StartupModule() override
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("WholeCarryReview")))
        {
            TickHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda([this](UWorld* W,ELevelTick,float){
                if(ReviewStarted||!W||!W->IsGameWorld()||W->GetTimeSeconds()<2)return;
                auto* PC=W->GetFirstPlayerController();auto* C=PC?Cast<AAlsCharacter>(PC->GetPawn()):nullptr;if(!C)return;
                ReviewStarted=true;auto* Review=W->SpawnActor<AWholeCarryReview>();
                if(!Review->Initialize(C)){UE_LOG(LogTemp,Error,TEXT("WHOLE_CARRY_REVIEW_INIT_FAILED"));FPlatformMisc::RequestExit(false);}
            });
            return;
        }
        if (FParse::Param(FCommandLine::Get(), TEXT("ReferenceCapture")))
        {
            BeforeTickHandle=FWorldDelegates::OnWorldPreActorTick.AddLambda(
                [this](UWorld* W,ELevelTick,float){Capture.Before(W);});
            TickHandle=FWorldDelegates::OnWorldPostActorTick.AddLambda(
                [this](UWorld* W,ELevelTick,float){Capture.After(W);});
            return;
        }
        if (!FParse::Param(FCommandLine::Get(), TEXT("ReferenceRiflePreview"))) return;
        TickHandle = FWorldDelegates::OnWorldPostActorTick.AddLambda(
            [this](UWorld* World, ELevelTick, float)
            {
                if (!World || !World->IsGameWorld() || World->GetTimeSeconds() < 1.f) return;
                APlayerController* PC = World->GetFirstPlayerController();
                AAlsCharacter* Character = PC ? Cast<AAlsCharacter>(PC->GetPawn()) : nullptr;
                if (!Character || Initialized.Get() == Character) return;
                Character->SetOverlayMode(AlsOverlayModeTags::Rifle);
                PC->SetControlRotation(FRotator(-10.f, Character->GetActorRotation().Yaw, 0.f));
                Initialized = Character;
            });
    }
    virtual void ShutdownModule() override
    {
        FWorldDelegates::OnWorldPostActorTick.Remove(TickHandle);
        FWorldDelegates::OnWorldPreActorTick.Remove(BeforeTickHandle);
    }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FMotionReferenceModule, MotionReference, "MotionReference");
