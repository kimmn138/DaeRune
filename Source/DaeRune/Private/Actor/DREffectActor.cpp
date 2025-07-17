// Copyright DaeRune


#include "Actor/DREffectActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"

ADREffectActor::ADREffectActor()
{
	// 틱 비활성화하여 매 프레임 처리 안 함
	PrimaryActorTick.bCanEverTick = false;

	// 씬 루트 컴포넌트 생성 및 루트로 설정함
	SetRootComponent(CreateDefaultSubobject<USceneComponent>("SceneRoot"));
}

void ADREffectActor::BeginPlay()
{
	Super::BeginPlay();
}

// 대상에게 이펙트 적용 처리함
void ADREffectActor::ApplyEffectToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
	// 적에게 적용하지 않도록 설정 시 거름 처리함
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 대상의 AbilitySystemComponent 얻기함
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (TargetASC == nullptr) return;

	// 이펙트 컨텍스트 생성 및 소스 설정함
	check(GameplayEffectClass);
	FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(this);
	// 이펙트 스펙 생성함
	const FGameplayEffectSpecHandle EffectSpecHandle = TargetASC->MakeOutgoingSpec(GameplayEffectClass, ActorLevel, EffectContextHandle);
	// 이펙트 적용 및 핸들 반환함
	const FActiveGameplayEffectHandle ActiveEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());

	// 무한 지속 정책 확인함
	const bool bIsInfinite = EffectSpecHandle.Data.Get()->Def.Get()->DurationPolicy == EGameplayEffectDurationType::Infinite;
	if (bIsInfinite && InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		// 무한 지속 이펙트 핸들 저장함
		ActiveEffectHandles.Add(ActiveEffectHandle, TargetASC);
	}

	// 즉시 파괴 설정 시 이펙트 적용 후 자기 파괴함
	if (bDestroyOnEffectApplication && !bIsInfinite)
	{
		Destroy();
	}
}

// 오버랩 시 이펙트 적용 제어함
void ADREffectActor::OnOverlap(AActor* TargetActor)
{
	// 적에게 적용하지 않도록 설정 시 거름 처리함
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 즉시 이펙트 적용 정책 검사함
	if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		ApplyEffectToTarget(TargetActor, InstantGameplayEffectClass);
	}
	// 지속 이펙트 적용 정책 검사함
	if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		ApplyEffectToTarget(TargetActor, DurationGameplayEffectClass);
	}
	// 무한 지속 이펙트 적용 정책 검사함
	if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		ApplyEffectToTarget(TargetActor, InfiniteGameplayEffectClass);
	}
}

// 오버랩 종료 시 이펙트 적용 및 제거 제어함
void ADREffectActor::OnEndOverlap(AActor* TargetActor)
{
	// 적에게 적용하지 않도록 설정 시 거름 처리함
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 즉시 이펙트 적용 정책 검사함
	if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		ApplyEffectToTarget(TargetActor, InstantGameplayEffectClass);
	}
	// 지속 이펙트 적용 정책 검사함
	if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		ApplyEffectToTarget(TargetActor, DurationGameplayEffectClass);
	}
	// 무한 지속 이펙트 적용 정책 검사함
	if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		ApplyEffectToTarget(TargetActor, InfiniteGameplayEffectClass);
	}
	// 무한 지속 제거 정책 검사함
	if (InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		// 대상 ASC 유효성 검사함
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!IsValid(TargetASC)) return;

		// 저장된 핸들 중 대상것 필터링함
		TArray<FActiveGameplayEffectHandle> HandlesToRemove;
		for (TTuple<FActiveGameplayEffectHandle, UAbilitySystemComponent*> HandlePair : ActiveEffectHandles)
		{
			if (TargetASC == HandlePair.Value)
			{
				TargetASC->RemoveActiveGameplayEffect(HandlePair.Key, 1);
				HandlesToRemove.Add(HandlePair.Key);
			}
		}
		// 제거된 핸들 매핑 삭제함
		for (FActiveGameplayEffectHandle& Handle : HandlesToRemove)
		{
			ActiveEffectHandles.FindAndRemoveChecked(Handle);
		}
	}
}
