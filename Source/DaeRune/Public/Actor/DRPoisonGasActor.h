// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DREffectActor.h"
#include "DRPoisonGasActor.generated.h"

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

	UFUNCTION(BlueprintCallable)
	void ApplySlowEffectToTarget(AActor* TargetActor);

	// 부모의 OnOverlap/OnEndOverlap을 확장
	UFUNCTION(BlueprintCallable)
	void OnPoisonGasOverlap(AActor* TargetActor);

	UFUNCTION(BlueprintCallable)
	void OnPoisonGasEndOverlap(AActor* TargetActor);

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
};
