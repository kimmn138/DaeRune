// Copyright DaeRune


#include "Actor/DREffectActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actor/DRCleanserSite.h"

ADREffectActor::ADREffectActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 기본 씬 컴포넌트 설정
	SetRootComponent(CreateDefaultSubobject<USceneComponent>("SceneRoot"));
}

void ADREffectActor::BeginPlay()
{
	Super::BeginPlay();
}

void ADREffectActor::ApplyEffectToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
	// 적 태그 확인 - bApplyEffectsToEnemies 설정에 따라 필터링
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 클렌저사이트 필터링
	if (Cast<ADRCleanserSite>(TargetActor)) return;

	// 타겟의 AbilitySystemComponent 가져오기
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (TargetASC == nullptr) return;

	// GameplayEffect 클래스 유효성 검증
	check(GameplayEffectClass);
	// 효과 컨텍스트 생성 - 소스 정보 설정
	FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(this);
	EffectContextHandle.AddInstigator(this, this);
	// GameplayEffect 스펙 생성 및 적용
	const FGameplayEffectSpecHandle EffectSpecHandle = TargetASC->MakeOutgoingSpec(GameplayEffectClass, ActorLevel, EffectContextHandle);
	const FActiveGameplayEffectHandle ActiveEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());

	// 무한 지속 효과이고 오버랩 종료 시 제거 정책인 경우 핸들 저장
	const bool bIsInfinite = EffectSpecHandle.Data.Get()->Def.Get()->DurationPolicy == EGameplayEffectDurationType::Infinite;
	if (bIsInfinite && InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		ActiveEffectHandles.Add(ActiveEffectHandle, TargetASC);
	}

	// 효과 적용 후 액터 파괴 (무한 효과는 제외)
	if (bDestroyOnEffectApplication && !bIsInfinite)
	{
		Destroy();
	}
}

void ADREffectActor::OnOverlap(AActor* TargetActor)
{
	// 적 필터링
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 클렌저사이트 필터링
	if (Cast<ADRCleanserSite>(TargetActor)) return;

	// 각 효과 타입별 적용 정책에 따라 오버랩 시 효과 적용
	if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		ApplyEffectToTarget(TargetActor, InstantGameplayEffectClass);
	}
	if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		ApplyEffectToTarget(TargetActor, DurationGameplayEffectClass);
	}
	if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlap)
	{
		ApplyEffectToTarget(TargetActor, InfiniteGameplayEffectClass);
	}
}

void ADREffectActor::OnEndOverlap(AActor* TargetActor)
{
	// 적 필터링
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 클렌저사이트 필터링
	if (Cast<ADRCleanserSite>(TargetActor)) return;

	// 오버랩 종료 시 효과 적용
	if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		ApplyEffectToTarget(TargetActor, InstantGameplayEffectClass);
	}
	if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		ApplyEffectToTarget(TargetActor, DurationGameplayEffectClass);
	}
	if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
	{
		ApplyEffectToTarget(TargetActor, InfiniteGameplayEffectClass);
	}
	// 무한 효과 제거 처리
	if (InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
		if (!IsValid(TargetASC)) return;

		// 해당 타겟의 모든 활성 무한 효과 제거
		TArray<FActiveGameplayEffectHandle> HandlesToRemove;
		for (TTuple<FActiveGameplayEffectHandle, UAbilitySystemComponent*> HandlePair : ActiveEffectHandles)
		{
			if (TargetASC == HandlePair.Value)
			{
				// 효과 제거 (스택 1개 제거)
				TargetASC->RemoveActiveGameplayEffect(HandlePair.Key, 1);
				HandlesToRemove.Add(HandlePair.Key);
			}
		}
		// 제거된 핸들들을 맵에서 삭제
		for (FActiveGameplayEffectHandle& Handle : HandlesToRemove)
		{
			ActiveEffectHandles.FindAndRemoveChecked(Handle);
		}
	}
}
