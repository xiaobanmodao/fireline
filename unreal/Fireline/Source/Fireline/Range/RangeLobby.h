#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RangeLobby.generated.h"
UCLASS()
class FIRELINE_API ARangeLobby : public AActor
{
 GENERATED_BODY()
public:
 ARangeLobby();
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 void Activate(bool Active);
 UPROPERTY() TObjectPtr<class USkeletalMeshComponent> Character;
 UPROPERTY() TObjectPtr<class UCameraComponent> Camera;
private:
 double Clock=0;
};
