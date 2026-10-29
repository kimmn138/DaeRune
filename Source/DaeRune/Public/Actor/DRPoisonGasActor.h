// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DREffectActor.h"
#include "Components/DecalComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "Subsystems/WorldSubsystem.h"
#include "DRPoisonGasActor.generated.h"

class ADRPoisonGasActor;

UENUM(BlueprintType)
enum class EPoisonGasPhase : uint8
{
	Warning,
	Active
};

/**
 *
 */
UCLASS()
class DAERUNE_API ADRPoisonGasActor : public ADREffectActor
{
	GENERATED_BODY()

public:
	ADRPoisonGasActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 독가스 구역 진입/이탈 (구체 오버랩 · BP 가 부른다). 겹침 관리는 UDRPoisonGasSubsystem 이 한다.
	UFUNCTION(BlueprintCallable)
	void OnPoisonGasOverlap(AActor* TargetActor);

	UFUNCTION(BlueprintCallable)
	void OnPoisonGasEndOverlap(AActor* TargetActor);

	void TransitionToActive();
	FVector FindGroundLocation() const;

	// 독 · 감속 대상인지 (적은 설정에 따라, 클렌저 사이트는 제외)
	bool CanAffect(const AActor* TargetActor) const;

	// 두 번째 Infinite Effect (슬로우용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Effects")
	TSubclassOf<UGameplayEffect> SlowGameplayEffectClass;

	// Decal 컴포넌트
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDecalComponent> GroundDecal;

	// Decal 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
	TObjectPtr<UMaterialInterface> WarningDecalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
	float DecalRadius = 312.5f;

	// 활성화 나이아가라 시스템 에셋 (블루프린트에서 할당)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
	TObjectPtr<UNiagaraSystem> ActiveNiagaraSystem;

	// 나이아가라 컴포넌트
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UNiagaraComponent> ActiveNiagaraComponent;

	// 내부 상태
	EPoisonGasPhase CurrentPhase = EPoisonGasPhase::Warning;

	FTimerHandle PhaseTransitionTimerHandle;

	// 활성 진입 시 GameState에 카운트 증가를 알렸는지 (EndPlay 누수 방지용)
	bool bNotifiedActive = false;

	// ===== 구체 오버랩 탐지 시스템 =====

	// 효과 적용 구체 콜리전
	UPROPERTY(VisibleAnywhere, Category = "Poison Gas|Detection")
	TObjectPtr<USphereComponent> EffectSphere;

	// 구체 콜리전 반지름
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Detection")
	float EffectSphereRadius = 312.5f;

	// 오버랩 델리게이트 시그니처에 맞는 래퍼
	UFUNCTION()
	void OnEffectSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnEffectSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

public:
	// 대상에게 독(Infinite) · 감속 GE 를 건다 — 지속형 효과의 핸들을 돌려준다 (UDRPoisonGasSubsystem 이 보관)
	TArray<FActiveGameplayEffectHandle> ApplyPoisonEffects(UAbilitySystemComponent* TargetASC);
};

/**
 * 독가스 겹침 관리 (월드당 하나, 서버).
 * 여러 가스 구역에 겹쳐 서 있어도 독 · 감속 GE 는 한 번만 걸고, 마지막 구역을 벗어날 때 뗀다.
 * ★GE 핸들은 대상별로 여기서 보관한다★ — 예전에는 처음 건 가스 액터가 핸들을 들고 있어서,
 * 그 가스가 먼저 사라지면 남은 가스가 떼지 못해 독 · 감속이 영구히 남았다.
 */
UCLASS()
class DAERUNE_API UDRPoisonGasSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	void Enter(ADRPoisonGasActor* Gas, AActor* Target);
	void Leave(ADRPoisonGasActor* Gas, AActor* Target);

	// 가스가 사라질 때 — 그 가스 안에 있던 모든 대상을 내보낸다
	void LeaveAll(ADRPoisonGasActor* Gas);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override
	{
		return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
	}

private:
	struct FTargetState
	{
		// 대상이 지금 안에 있는 가스들 (같은 가스의 이중 진입/이탈은 집합이라 무해)
		TSet<TWeakObjectPtr<ADRPoisonGasActor>> Gases;
		TWeakObjectPtr<UAbilitySystemComponent> ASC;
		TArray<FActiveGameplayEffectHandle> Handles;
	};

	// 키는 폰 — 폰이 파괴돼 약포인터가 낡아도 키 비교는 그대로 된다
	TMap<TWeakObjectPtr<AActor>, FTargetState> Targets;

	static void RemoveEffects(FTargetState& State);
};
