// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "DREffectActor.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

// 효과 적용 정책 정의함
UENUM(BlueprintType)
enum class EEffectApplicationPolicy : uint8
{
	ApplyOnOverlap,
	ApplyOnEndOverlap,
	DoNotApply
};

// 효과 제거 정책 정의함
UENUM(BlueprintType)
enum class EEffectRemovalPolicy : uint8
{
	RemoveOnEndOverlap,
	DoNotRemove
};

UCLASS()
class DAERUNE_API ADREffectActor : public AActor
{
	GENERATED_BODY()
	
public:
	ADREffectActor();

protected:
	virtual void BeginPlay() override;

	// 대상에게 이펙트 적용 함수 선언함
	UFUNCTION(BlueprintCallable)
	void ApplyEffectToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> GameplayEffectClass);

	// 오버랩 시 이펙트 처리 함수 선언함
	UFUNCTION(BlueprintCallable)
	void OnOverlap(AActor* TargetActor);

	// 오버랩 종료 시 이펙트 처리 함수 선언함
	UFUNCTION(BlueprintCallable)
	void OnEndOverlap(AActor* TargetActor);

	// 적용 후 액터 파괴 여부 설정 프로퍼티임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	bool bDestroyOnEffectApplication = false;

	// 적 대상에도 이펙트 적용 여부 설정 프로퍼티임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	bool bApplyEffectsToEnemies = false;

	// 즉시 적용할 GameplayEffect 클래스 지정 프로퍼티임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	TSubclassOf<UGameplayEffect> InstantGameplayEffectClass;

	// 즉시 이펙트 적용 정책 설정 프로퍼티임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	EEffectApplicationPolicy InstantEffectApplicationPolicy = EEffectApplicationPolicy::DoNotApply;

	// 지속형 GameplayEffect 클래스 지정 프로퍼티임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	TSubclassOf<UGameplayEffect> DurationGameplayEffectClass;

	// 지속 이펙트 적용 정책 설정 프로퍼티임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	EEffectApplicationPolicy DurationEffectApplicationPolicy = EEffectApplicationPolicy::DoNotApply;

	// 무한 지속형 GameplayEffect 클래스 지정 프로퍼티임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	TSubclassOf<UGameplayEffect> InfiniteGameplayEffectClass;

	// 무한 지속 이펙트 적용 정책 설정 프로퍼티임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	EEffectApplicationPolicy InfiniteEffectApplicationPolicy = EEffectApplicationPolicy::DoNotApply;

	// 무한 지속 이펙트 제거 정책 설정 프로퍼티임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	EEffectRemovalPolicy InfiniteEffectRemovalPolicy = EEffectRemovalPolicy::RemoveOnEndOverlap;

	// 활성화된 이펙트 핸들 매핑 추적용 TMap임
	TMap<FActiveGameplayEffectHandle, UAbilitySystemComponent*> ActiveEffectHandles;

	// 이펙트 레벨 지정 프로퍼티임
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
	float ActorLevel = 1.f;
};
