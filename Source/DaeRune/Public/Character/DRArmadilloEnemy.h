// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DREnemy.h"
#include "DRArmadilloEnemy.generated.h"

class USphereComponent;
class UAudioComponent;
class UAnimMontage;

/**
 * Rolling impact result data - collected by C++ collision detection, passed to GA (Blueprint).
 * GA receives this data and handles damage/stun effects directly.
 */
USTRUCT(BlueprintType)
struct FRollImpactResult
{
	GENERATED_BODY()

	/** Impact location (center of effect area) */
	UPROPERTY(BlueprintReadOnly)
	FVector ImpactLocation = FVector::ZeroVector;

	/** Actors within impact radius (excluding self) */
	UPROPERTY(BlueprintReadOnly)
	TArray<AActor*> HitActors;

	/** Whether a player or another enemy was within the impact radius (false = terrain only) */
	UPROPERTY(BlueprintReadOnly)
	bool bHitPlayerOrEnemy = false;
};

/** Delegate broadcast when rolling collision occurs - GA binds to this */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRollImpact, const FRollImpactResult&, ImpactResult);

/** Delegate broadcast when rolling reaches target without collision - GA binds to this */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRollReachedTarget);

/**
 * Armadillo Enemy: Switches between BasicForm (walk/melee) and BallForm (roll/charge skill).
 *
 * C++ handles form switching, charge movement, and collision detection only.
 * Damage/stun effects are applied by GA Blueprint (DRDamageGameplayAbility).
 */
UCLASS()
class DAERUNE_API ADRArmadilloEnemy : public ADREnemy
{
	GENERATED_BODY()

public:
	ADRArmadilloEnemy();
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Combat Interface Override */
	virtual void MulticastHandleDeath_Implementation(const FVector& DeathImpulse) override;

	// ===== Form Switching System =====

	/** Whether currently in ball form */
	UFUNCTION(BlueprintPure, Category = "Armadillo|Form")
	bool IsBallForm() const { return bIsBallForm; }

	/** Request form change (server) - AnimNotify calls FinishFormChange at end of transition animation */
	UFUNCTION(BlueprintCallable, Category = "Armadillo|Form")
	void StartFormChange(bool bToBallForm);

	/** Complete form change (called by AnimNotify) - swaps mesh + updates state */
	UFUNCTION(BlueprintCallable, Category = "Armadillo|Form")
	void FinishFormChange();

	// ===== Rolling Charge Skill =====

	/** Whether currently rolling */
	UFUNCTION(BlueprintPure, Category = "Armadillo|Roll")
	bool IsRolling() const { return bIsRolling; }

	/** Start charge (called by GA) */
	UFUNCTION(BlueprintCallable, Category = "Armadillo|Roll")
	void StartRollCharge(FVector TargetLocation);

	/** Stop charge (on collision or target reached) */
	UFUNCTION(BlueprintCallable, Category = "Armadillo|Roll")
	void StopRollCharge();

	/** Find valid roll target: random player within 3000 units with no wall obstruction */
	UFUNCTION(BlueprintCallable, Category = "Armadillo|Roll")
	AActor* FindRollTarget() const;

	// ===== Rolling Collision Events (GA binds to these) =====

	/** Broadcast on collision - GA receives this to handle damage/stun */
	UPROPERTY(BlueprintAssignable, Category = "Armadillo|Roll")
	FOnRollImpact OnRollImpact;

	/** Broadcast when target reached without collision - GA handles form revert */
	UPROPERTY(BlueprintAssignable, Category = "Armadillo|Roll")
	FOnRollReachedTarget OnRollReachedTarget;

	// ===== Ball Form Mesh Reference =====

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Armadillo|Mesh")
	TObjectPtr<USkeletalMeshComponent> BallFormMesh;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount) override;

	// 외곽선을 BallFormMesh에도 적용 (볼 폼일 때 기본 GetMesh()는 숨겨지므로)
	virtual void ApplyWaveOutline() override;

	// ===== Form Switching Config =====

	/** Target state during form transition (true=transitioning to ball, false=transitioning to basic) */
	UPROPERTY(BlueprintReadOnly, Category = "Armadillo|Form")
	bool bPendingBallForm = false;

	/** Ball form state (replicated) */
	UPROPERTY(ReplicatedUsing = OnRep_BallForm, BlueprintReadOnly, Category = "Armadillo|Form")
	bool bIsBallForm = false;

	UFUNCTION()
	void OnRep_BallForm();

	// ===== Form Change Transition Montages =====
	// GA의 PlayMontage 노드는 서버(호스트)에서만 재생되므로, 원격 클라이언트에는
	// StartFormChange()에서 MulticastPlayFormChangeMontage()로 동일 몽타주를 재생해준다.
	// 두 메시(기본/볼) 모두 동시에 재생 — 전환 중 bIsBallForm 복제로 가시성이 바뀌어도 포즈가 이어지도록.

	/** Basic mesh: Basic -> Ball transition montage */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Form")
	TObjectPtr<UAnimMontage> FormChangeToBallMontage_Basic;

	/** Ball mesh: Basic -> Ball transition montage */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Form")
	TObjectPtr<UAnimMontage> FormChangeToBallMontage_Ball;

	/** Basic mesh: Ball -> Basic transition montage */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Form")
	TObjectPtr<UAnimMontage> FormChangeToBasicMontage_Basic;

	/** Ball mesh: Ball -> Basic transition montage */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Form")
	TObjectPtr<UAnimMontage> FormChangeToBasicMontage_Ball;

	/** 원격 클라이언트에서 폼 체인지 전환 몽타주 재생 (서버는 GA가 직접 재생하므로 스킵) */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayFormChangeMontage(bool bToBallForm);

	/** Ball form capsule radius */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Form")
	float BallFormCapsuleRadius = 40.f;

	/** Ball form capsule half height */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Form")
	float BallFormCapsuleHalfHeight = 40.f;

	/** Default capsule radius (saved in BeginPlay) */
	float DefaultCapsuleRadius = 0.f;

	/** Default capsule half height (saved in BeginPlay) */
	float DefaultCapsuleHalfHeight = 0.f;

	/** 현재 폼(bIsBallForm)에 맞는 캡슐 크기 적용. 캡슐 크기는 복제되지 않으므로
	    서버/클라이언트 모두 OnRep_BallForm 경로에서 이 함수로 동기화한다. */
	void ApplyFormCapsuleSize();

	// ===== Rolling Charge Config =====

	/** Rolling state (replicated). OnRep starts/stops the loop sound on remote clients;
	    server-side StartRollCharge/StopRollCharge invoke OnRep_IsRolling() manually for parity. */
	UPROPERTY(ReplicatedUsing = OnRep_IsRolling, BlueprintReadOnly, Category = "Armadillo|Roll")
	bool bIsRolling = false;

	UFUNCTION()
	void OnRep_IsRolling();

	/** Charge target location */
	UPROPERTY(BlueprintReadOnly, Category = "Armadillo|Roll")
	FVector RollTargetLocation;

	/** Charge direction (normalized) */
	UPROPERTY(BlueprintReadOnly, Category = "Armadillo|Roll")
	FVector RollDirection;

	/** Current charge speed */
	UPROPERTY(BlueprintReadOnly, Category = "Armadillo|Roll")
	float CurrentRollSpeed = 0.f;

	/** Initial charge speed */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
	float RollInitialSpeed = 200.f;

	/** Maximum charge speed */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
	float RollMaxSpeed = 2000.f;

	/** Charge acceleration (speed increase per second) */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
	float RollAcceleration = 400.f;

	/** Forward collision detection sphere radius */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
	float RollDetectionRadius = 30.f;

	/** Impact effect sphere radius */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
	float RollImpactRadius = 50.f;

	/** Target search max range */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
	float RollTargetSearchRange = 3000.f;

	/** If movement during a roll stays below StuckMoveThreshold for this many seconds, treat as stuck and end the roll. */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
	float StuckTimeLimit = 3.f;

	/** 2D distance (cm) below which we consider the armadillo to not be moving for stuck detection. */
	UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
	float StuckMoveThreshold = 10.f;

	FVector LastStuckCheckLocation = FVector::ZeroVector;
	float StuckTimeAccumulator = 0.f;

private:
	// ===== Roll/Impact Sound (Plan2.md §6.2, §6.3) =====

	/** Loop sound component (spawned on first roll, stopped on roll end / death / EndPlay). */
	UPROPERTY()
	TObjectPtr<UAudioComponent> RollLoopComponent;

	/** Start roll loop sound. Safe to call multiple times (skips if already playing). Skipped on dedicated server. */
	void StartArmadilloRollLoop();

	/** Stop roll loop sound. Safe to call multiple times. */
	void StopArmadilloRollLoop();

	/** Multicast impact sound (server -> all clients). Reliable: discrete impact event must not be dropped. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayRollImpactSound(FVector_NetQuantize Location);

	/** Tick processing during charge (movement + collision detection) */
	void TickRollCharge(float DeltaTime);

	/** Forward collision detection (SphereTrace) */
	bool DetectRollCollision(FHitResult& OutHit) const;

	/**
	 * Collects actors within impact radius and constructs FRollImpactResult,
	 * then broadcasts OnRollImpact delegate.
	 * Does NOT apply damage/stun - GA handles that via delegate.
	 */
	void BroadcastRollImpact(const FVector& ImpactLocation);

	/** Update mesh visibility */
	void UpdateMeshVisibility();
};
