#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "LiveCarryStudy.h"
class FFirelineLiveStudyModule : public FDefaultGameModuleImpl
{
    FLiveCarryStudy Study;FDelegateHandle Before,After;
public:
    virtual void StartupModule() override{Before=FWorldDelegates::OnWorldPreActorTick.AddLambda([this](UWorld* W,ELevelTick,float Dt){Study.Before(W,Dt);});After=FWorldDelegates::OnWorldPostActorTick.AddLambda([this](UWorld* W,ELevelTick,float Dt){Study.After(W,Dt);});}
    virtual void ShutdownModule() override{FWorldDelegates::OnWorldPreActorTick.Remove(Before);FWorldDelegates::OnWorldPostActorTick.Remove(After);}
};
IMPLEMENT_PRIMARY_GAME_MODULE(FFirelineLiveStudyModule,FirelineLiveStudy,"FirelineLiveStudy");
