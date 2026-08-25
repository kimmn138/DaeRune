// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DREffectActor.h"
#include "Components/DecalComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
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
	FVector FindGroundLocation() const;

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

	// 이 가스 액터가 현재 GE를 적용 중인 타겟
	TSet<TWeakObjectPtr<AActor>> ActiveEffectTargets;

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
};
