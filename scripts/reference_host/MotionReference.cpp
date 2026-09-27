// Research host only. Original ALS assets and animation graph are never edited.
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "AlsCharacter.h"
#include "Utility/AlsGameplayTags.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

class FMotionReferenceModule : public FDefaultGameModuleImpl
{
    FDelegateHandle TickHandle;
    TWeakObjectPtr<AAlsCharacter> Initialized;
public:
    virtual void StartupModule() override
    {
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
    }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FMotionReferenceModule, MotionReference, "MotionReference");
