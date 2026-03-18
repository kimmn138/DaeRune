// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DREffectActor.h"
#include "Components/DecalComponent.h"
#include "DRPoisonGasActor.generated.h"

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

	UFUNCTION(BlueprintCallable)
	void ApplySlowEffectToTarget(AActor* TargetActor);

	// 부모의 OnOverlap/OnEndOverlap을 확장
	UFUNCTION(BlueprintCallable)
	void OnPoisonGasOverlap(AActor* TargetActor);

	UFUNCTION(BlueprintCallable)
	void OnPoisonGasEndOverlap(AActor* TargetActor);

	void TransitionToActive();
	void RemoveAllPoisonEffects();

	// 두 번째 Infinite Effect (슬로우용)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Effects")
	TSubclassOf<UGameplayEffect> SlowGameplayEffectClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Effects")
	EEffectApplicationPolicy SlowEffectApplicationPolicy = EEffectApplicationPolicy::ApplyOnOverlap;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Effects")
	EEffectRemovalPolicy SlowEffectRemovalPolicy = EEffectRemovalPolicy::RemoveOnEndOverlap;

	// 슬로우 Effect 핸들 별도 관리
	TMap<FActiveGameplayEffectHandle, UAbilitySystemComponent*> SlowEffectHandles;

	// 타겟별 중첩 카운트 (static으로 모든 가스 액터가 공유)
	static TMap<TWeakObjectPtr<AActor>, int32> OverlapCountMap;

	// Decal 컴포넌트
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDecalComponent> GroundDecal;

	// Decal 설정
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
	TObjectPtr<UMaterialInterface> WarningDecalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
	TObjectPtr<UMaterialInterface> ActiveDecalMaterial;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Visual")
	float DecalRadius = 312.5f;

	// 내부 상태
	EPoisonGasPhase CurrentPhase = EPoisonGasPhase::Warning;

	FTimerHandle PhaseTransitionTimerHandle;

	// 이 가스 액터가 현재 GE를 적용 중인 타겟
	TSet<TWeakObjectPtr<AActor>> ActiveEffectTargets;

	// ===== 주기적 체크 시스템 =====

	// 주기적 효과 체크 타이머
	FTimerHandle EffectCheckTimerHandle;

	// 체크 주기 (초)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Detection")
	float EffectCheckInterval = 0.2f;

	// 효과 판정 반지름 (DecalRadius와 동일하게 설정)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Poison Gas|Detection")
	float EffectRadius = 312.5f;

	// 타이머 콜백: 주변 대상 탐색 및 효과 적용/제거
	void CheckNearbyTargets();

	// 대상이 효과 범위 안에 있는지 판정
	bool IsTargetInEffectZone(AActor* Target) const;
};
