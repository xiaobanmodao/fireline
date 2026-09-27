#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/HUD.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "RangeVitals.h"
#include "RangeWeaponHandling.h"
#include "RangeLocomotionState.h"
#include "RangeContactCarry.h"
#include "RangeGame.generated.h"
class UCameraComponent;
class USpringArmComponent;
class UAnimSequence;
class UBlendSpace;
UCLASS()
class FIRELINE_API ARangeCharacter : public ACharacter
{
 GENERATED_BODY()
public:
 friend class ARangePlayerController;
 ARangeCharacter();
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 virtual void Landed(const FHitResult& Hit) override;
 virtual void OnJumped_Implementation() override;
 virtual void OnStartCrouch(float HeightAdjust,float ScaledHeightAdjust) override;
 virtual void OnEndCrouch(float HeightAdjust,float ScaledHeightAdjust) override;
 virtual void SetupPlayerInputComponent(UInputComponent* Input) override;
 int32 Ammo=30, Hits=0, Shots=0, Kills=0, Headshots=0, BulletHits=0;
 bool bMP7=false,bMP7Ready=false,bMP7Audit=false,bEmptyReload=false;
 int32 RifleAmmo=30,MP7Ammo=40,MP7Action=0;
 float MP7ActionTime=0,MP7ActionBlend=0,ReloadDuration=2.35f;
 bool bMP7ActionReturning=false;
 int32 MagazineCapacity() const{return bMP7?40:30;}
 int32 WeaponSlot() const{return bEmptyHands?0:bKnife?2:bMP7?3:1;}
 FName WeaponBone(const TCHAR* Part) const{return FName(*FString::Printf(TEXT("%s_%s"),bMP7?TEXT("MP7"):TEXT("M4"),Part));}
 UPROPERTY() TObjectPtr<class USkeletalMesh> RifleGunMesh;
 UPROPERTY() TObjectPtr<class USkeletalMesh> MP7BodyMesh;
 UPROPERTY() TObjectPtr<class USkeletalMesh> MP7ArmsMesh;
 UPROPERTY() TObjectPtr<class USkeletalMesh> MP7GunMesh;
 UPROPERTY() TObjectPtr<class UStaticMesh> MP7MagazineMesh;
 // Selected sycgff M4 clips: hold, draw, remove, reload. No donor fire clip.
 int32 M4Action=0,M4PreviousAction=0;
 float M4Time=0,M4PreviousTime=0,M4Mix=1,M4Weight=0,HandSwitchDuration=.48f;
 bool bM4Returning=false,bSwitchFromM4=false;
 void StartM4Action(int32 Action);
 void UpdateM4(float Dt);
 void CancelM4Action();
 bool IsM4DrawLocked() const;
 bool IsM4ReloadRecovery() const;
 void ReleaseM4ReloadRecovery();
 void StartBodyPoseDiagnosis();
 void StartM4AnimationAudit();
 void InitializeMP7();
 void EquipMP7();
 void CycleWeaponBack();
 void UpdateMP7(float Dt);
 void InspectMP7();
 void CancelMP7Action();
 bool IsMP7DrawLocked() const;
 bool IsMP7ReloadRecovery() const;
 void ReleaseMP7ReloadRecovery();
 void StartWeaponPolishAudit();
 void StartMP7Audit();
 FRangeVitals PlayerVitals;
 FVector SpawnPoint;float PlayerShieldPulse=0,RespawnLeft=0;bool bPlayerShieldBroken=false;
 UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> PlayerShieldMaterial;
 virtual float TakeDamage(float DamageAmount,const FDamageEvent& DamageEvent,AController* EventInstigator,AActor* DamageCauser) override;
 void UpdatePlayerVitals(float Dt);
 int32 LastDamage=0;
 bool bLastShieldHit=false,bLastShieldBreak=false;float LastKillTTK=0;
 bool bLastHeadshot=false,bLastKill=false;
 float FeedbackLeft=0;
 bool bReloading=false, bThirdPerson=false, bThirdPersonOrbit=false;
 float HitFlash=0, ReloadLeft=0, ReloadPosition=2.35f;
 float ReloadFireReturnLeft=0;
 bool bFireAfterReloadReturn=false;
 bool bMagazineDropped=false,bReloadPoseHeld=false;
 bool bReloadAmmoCommitted=false;
 UPROPERTY() TObjectPtr<class ARangeCasingFX> CasingFX;
 void StartReloadCasingAudit();
 float AimAlpha=0;
 // One presentation state shared by body and view; independent of mesh swaps.
 FRangeContactClock ContactCarryClock;
 float SprintCarryAlpha=0,SprintCarryVelocity=0,SprintFireRecovery=0;
 bool bFiringMovement=false;
 void UpdateFiringMovement(float Dt);
 bool bAimHeld=false;
 bool bSliding=false;
 float SlideTime=0,SlideBlend=0,AirBlend=0,LandingTime=1,LandingStrength=0;
 FRangeLocomotionState Locomotion;
 float ReloadAuditSample=-1;
 void DropMagazine();
 void StartMagazineDropAudit();
 TWeakObjectPtr<class AStaticMeshActor> LastDroppedMagazine;
 int32 MagazineDropCount=0;
 UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Boom;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Arms;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> ViewGun;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> BodyGun;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> ViewOptic;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class UStaticMeshComponent> BodyOptic;
 bool SetCoyoteSight(bool Enabled);
 bool UsesCoyoteSight() const{return bCoyoteSight;}
 bool HasCoyoteSight() const;
 void InitializeOptics();
 void UpdateOpticVisibility();
 void StartOpticAudit();
 bool bOpticAudit=false;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> ViewKnife;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> BodyKnife;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class URangeAudio> CombatAudio;
 bool bEmptyHands=false,bHandTest=false,bKnife=false;
 // 0 hold, 1 draw, 2 inspect, 3 slash. Previous sample is held during cancellation.
 int32 KnifeAction=0,KnifePreviousAction=0,KnifeActionSerial=0;
 float KnifeTime=0,KnifePreviousTime=0,KnifeBlend=1;
 bool bKarambitReady=false;
 UPROPERTY() TObjectPtr<class USkeletalMesh> RifleBodyMesh;
 UPROPERTY() TObjectPtr<class USkeletalMesh> RifleArmsMesh;
 UPROPERTY() TObjectPtr<class USkeletalMesh> KarambitBodyMesh;
 UPROPERTY() TObjectPtr<class USkeletalMesh> KarambitArmsMesh;
 void InitializeKarambit();
 void UpdateWeaponMeshes();
 void StartKarambitAudit();
 void TickKarambitAudit();
 bool bKarambitAudit=false;
 int32 KarambitBoundsOutside=0,KarambitBoundsSamples=0;
 float KarambitBoundsTime=-1;
 void EquipKnife();
 void KnifeAttackPressed();
 void RequestWeapon(int32 Mode);
 void CycleWeapon();
 void StartWeaponControlsAudit();
 void StartWeaponDrawAudit();
 void StartKnifeAction(int32 Action);
 void UpdateKnife(float Dt);
 void KnifeContact();
 void StartM9Audit();
 void TickM9Audit(float Dt);
 bool bM9Audit=false;
 int32 PendingWeapon=1,QueuedWeapon=-1,QueuedKnifeAction=-1;
 bool bSwitchFromKnife=false,bKnifeContactDone=false;
 FString M9AuditSamples;
 float HandFistAlpha=0,HandTestTime=0,HandTestBlendOut=0,HandSwitchLeft=0,HandLower=0;
 void ToggleEmptyHands();
 void EquipRifle();
 void TestFingers();
 void UpdateHands(float Dt);
 void UpdateHandVisibility();
 void StartHandsAudit();
 void StartArmPoseAudit();
 void ToggleView();
 void Reload();
 void Fire();
 float GetHipSpreadAngle() const;
 float GetShotSpreadAngle() const;
 void StartWeaponHandlingAudit();
 void StartMovementSpreadAudit();
 bool IsSprintHeld() const;
 void ApplyRawMouseDelta(const FVector2D& Delta);
 void SetMouseSensitivity(float Value){MouseDegreesPerUnit=FMath::Clamp(Value,.005f,.15f);}
 float GetMouseSensitivity() const{return MouseDegreesPerUnit;}
protected:
 UPROPERTY() TObjectPtr<UAnimSequence> Idle;
 UPROPERTY() TObjectPtr<class UStaticMesh> DroppedMagazineMesh;
 // Mouse axes use unit sensitivity; this is the only gameplay gain, in degrees per input unit.
 UPROPERTY(EditAnywhere, Category="Input", meta=(ClampMin="0.001", ClampMax="1.0")) float MouseDegreesPerUnit=.05f;
 float FireCooldown=0, Kick=0, ReloadLeadOut=0;
 bool bAimPoseReady=false;
 bool bCoyoteSight=false;
 FVector IronAimLocation=FVector::ZeroVector,CoyoteAimLocation=FVector::ZeroVector;
 FQuat IronAimRotation=FQuat::Identity,CoyoteAimRotation=FQuat::Identity;
 float SightChangeLeft=0;
 FTransform SightChangeFrom;
 FVector AimLocation=FVector::ZeroVector;
 FQuat AimRotation=FQuat::Identity;
 FVector RifleHipLocation=FVector(20,3,-164);
 FQuat RifleHipRotation=FRotator(0,-90,0).Quaternion();
 void StartAim();
 void EndAim();
 void UpdateAim(float Dt);
 float RecoilPitch=0,RecoilYaw=0,RecoilPitchVelocity=0,RecoilYawVelocity=0;
 RangeHandling::FState WeaponHandling[2];
 FRandomStream ShotSpreadRandom;
 FVector LastShotDirection=FVector::ForwardVector,LastShotAim=FVector::ForwardVector;
 FVector LastShotEnd=FVector::ZeroVector;
 float LastShotSpread=0;
 void UpdateShooting(float Dt);
 bool bCrouchHeld=false,bJumpHeld=false,bBufferedTapRelease=false;
 float SlideCooldown=0,SlideInputBuffer=0,JumpBuffer=0,StanceEyeZ=75;
 float GroundJumpPending=0;
 void StartSlide();
 void UpdateMobility(float Dt);
 void UpdateLocomotionState(float Dt);
 void ApplyMobilitySettings();
 void EndSlide();
 bool bMobilityAudit=false;
 FString MobilityAuditSamples;
 void StartMobilityAudit();
 void StartDirectionalAudit();
 void TickMobilityAudit(float Dt);
 bool bViewFixAudit=false;
 FString ViewFixSamples;
 void StartViewFixAudit();
 void TickViewFixAudit(float Dt);
 bool bShootingAudit=false;
 FString ShootingAuditSamples;
 void StartShootingAudit();
 void TickShootingAudit(float Dt);
 bool bMovementAudit=false;
 FString MovementAuditSamples;
 void StartMovementAudit();
 void TickMovementAudit(float Dt);
 bool bWeaponAudit=false;
 FString WeaponAuditSamples;
 void StartWeaponAudit();
 void TickWeaponAudit(float Dt);
 bool bVisibilityAudit=false;
 FString VisibilityAuditSamples;
 void StartVisibilityAudit();
 void CaptureVisibility(int Index);
 bool bHitboxAudit=false;
 FString HitboxAuditSamples;
 void StartHitboxAudit();
 void SampleHitbox(int Index);
 float WalkBobAmount=0, WalkBobPhase=0;
 FVector ViewGaitTranslation=FVector::ZeroVector;
 FQuat ViewGaitRotation=FQuat::Identity;
 bool bHoldAudit=false;
 FString HoldAuditSamples;
 void StartHoldAudit();
 void TickHoldAudit(float Dt);
 void LookX(float V); void LookY(float V);
 void StartJump(); void EndJump(); void StartCrouch(); void EndCrouch();
};
UCLASS()
class FIRELINE_API ARangeTarget : public AActor
{
 GENERATED_BODY()
public:
 ARangeTarget();
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 bool RegisterHit(int Damage=RangeDamage::Body);
 void ShieldImpact(const FVector& Point);
 void StartShieldAudit();
 bool TraceShot(const FVector& Start,const FVector& Direction,float MaxDistance,float& Distance,bool& Headshot,FVector& Normal) const;
 int GetHealth() const{return FMath::CeilToInt(Vitals.Health);}
 float GetShield() const{return Vitals.Shield;}
 float GetLastTTK() const{return LastTTK;}
 float FeedbackLeft=0;
 UPROPERTY(VisibleAnywhere) TObjectPtr<class UCapsuleComponent> Capsule;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Body;
 UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> Gun;
private:
 float ResetLeft=0,FirstHitTime=0,LastTTK=0;int LifeHitCount=0;
 FRangeVitals Vitals;
 float ShieldPulse=0;bool bShieldBroken=false;
 UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> ShieldMaterial;
 UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> ShellMaterial;
 UPROPERTY() TObjectPtr<class UStaticMeshComponent> ShieldShell;
};
UCLASS()
class FIRELINE_API ARangeHUD : public AHUD
{
 GENERATED_BODY()
public:
 ARangeHUD();
 virtual void DrawHUD() override;
private:
 UPROPERTY() TObjectPtr<class UTexture2D> RifleIcon;
 UPROPERTY() TObjectPtr<class UTexture2D> MP7Icon;
 UPROPERTY() TObjectPtr<class UTexture2D> KarambitIcon;
};
UCLASS()
class FIRELINE_API ARangeGameMode : public AGameModeBase
{
 GENERATED_BODY()
public:
 ARangeGameMode();
};
UCLASS()
class FIRELINE_API URangeAssetTools : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildLocomotion();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildActionAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void PrepareLocomotionProxy();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void PrepareFullArmProxy();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildDirectionalAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildContactPhaseAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildStrideCoverageAssets(bool PrepareOnly);
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildCMUSideStepAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildAuthoredGroundAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildBodyCoordinationAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildArmedReadyAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildLegDirectionAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildKneeContinuityAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildArmedLocomotionAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildHandsAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildDrazenAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void BuildOriginalDrazenAssets();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void PrepareViewBounds();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void PrepareSelectedM9();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void PrepareHitMesh();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void PrepareSelectedKarambit();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void PrepareMP7();
 UFUNCTION(BlueprintCallable, Category="Fireline") static void PrepareDJNativeHands();
};
