// Copyright DaeRune


#include "Actor/DRPoisonGasActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actor/DRCleanserSite.h"

TMap<TWeakObjectPtr<AActor>, int32> ADRPoisonGasActor::OverlapCountMap;

ADRPoisonGasActor::ADRPoisonGasActor()
{
	InfiniteEffectApplicationPolicy = EEffectApplicationPolicy::ApplyOnOverlap;
	InfiniteEffectRemovalPolicy = EEffectRemovalPolicy::RemoveOnEndOverlap;

	bApplyEffectsToEnemies = true;
}

void ADRPoisonGasActor::BeginPlay()
{
	Super::BeginPlay();
}

void ADRPoisonGasActor::ApplySlowEffectToTarget(AActor* TargetActor)
{
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 클렌저사이트 필터링
	if (Cast<ADRCleanserSite>(TargetActor)) return;
	
	if (!SlowGameplayEffectClass) return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC) return;

	FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(this);
	EffectContextHandle.AddInstigator(this, this);
	
	const FGameplayEffectSpecHandle EffectSpecHandle = TargetASC->MakeOutgoingSpec(SlowGameplayEffectClass, ActorLevel, EffectContextHandle);
	const FActiveGameplayEffectHandle ActiveEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());

	// Infinite인 경우에만 핸들 저장
	const bool bIsInfinite = EffectSpecHandle.Data.Get()->Def.Get()->DurationPolicy == EGameplayEffectDurationType::Infinite;
	if (bIsInfinite && SlowEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		SlowEffectHandles.Add(ActiveEffectHandle, TargetASC);
	}
}

void ADRPoisonGasActor::OnPoisonGasOverlap(AActor* TargetActor)
{
	// 적 필터링
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 클렌저사이트 필터링
	if (Cast<ADRCleanserSite>(TargetActor)) return;

	int32& Count = OverlapCountMap.FindOrAdd(TargetActor);
	Count++;
	
	// 처음 진입할 때만 GE 적용
	if (Count == 1)
	{
		OnOverlap(TargetActor);
		ApplySlowEffectToTarget(TargetActor);
	}
}

void ADRPoisonGasActor::OnPoisonGasEndOverlap(AActor* TargetActor)
{
	// 적 필터링
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 클렌저사이트 필터링
	if (Cast<ADRCleanserSite>(TargetActor)) return;
	
	int32* CountPtr = OverlapCountMap.Find(TargetActor);
	if (!CountPtr) return;

	(*CountPtr)--;

	// 모든 가스 영역에서 나갔을 때만 GE 제거
	if (*CountPtr <= 0)
	{
		OnEndOverlap(TargetActor);
		
		// 슬로우 Effect 제거
		if (SlowEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
		{
			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
			if (!IsValid(TargetASC)) return;

			TArray<FActiveGameplayEffectHandle> HandlesToRemove;
			for (const TTuple<FActiveGameplayEffectHandle, UAbilitySystemComponent*>& HandlePair : SlowEffectHandles)
			{
				if (TargetASC == HandlePair.Value)
				{
					TargetASC->RemoveActiveGameplayEffect(HandlePair.Key, 1);
					HandlesToRemove.Add(HandlePair.Key);
				}
			}
	
			for (const FActiveGameplayEffectHandle& Handle : HandlesToRemove)
			{
				SlowEffectHandles.FindAndRemoveChecked(Handle);
			}
		}
	
		// EndOverlap 시 슬로우 적용하는 케이스 (일반적이진 않지만)
		if (SlowEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
		{
			ApplySlowEffectToTarget(TargetActor);
		}
		
		OverlapCountMap.Remove(TargetActor);
	}
}
