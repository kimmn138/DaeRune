// Copyright DaeRune


#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "DaeRune/DRLogChannels.h"

// 액터 정보 설정 완료 시점 등록 처리 구현
void UDRAbilitySystemComponent::AbilityActorInfoSet()
{
	// 자체 효과 적용 시 델리게이트 바인딩 단계
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UDRAbilitySystemComponent::ClientEffectApplied);
}

// 캐릭터 능력 부여 구현
void UDRAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	// 각 능력 클래스 순회 단계
	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		// DRGameplayAbility 캐스팅 단계
		if (const UDRGameplayAbility* DRAbility = Cast<UDRGameplayAbility>(AbilitySpec.Ability))
		{
			// 동적 입력 태그 추가 단계
			AbilitySpec.DynamicAbilityTags.AddTag(DRAbility->StartupInputTag);
			// 능력 등록 단계
			GiveAbility(AbilitySpec);
		}
	}
	// 부여 플래그 설정 단계
	bStartupAbilitiesGiven = true;
	// 델리게이트 브로드캐스트 단계
	AbilitiesGivenDelegate.Broadcast();
}

// 캐릭터 패시브 능력 부여 구현
void UDRAbilitySystemComponent::AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupPassiveAbilities)
{
	// 각 패시브 능력 클래스 순회 단계
	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupPassiveAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		// 일회성 활성화 후 제거 단계
		GiveAbilityAndActivateOnce(AbilitySpec);
	}
}

// 입력 태그 눌림 이벤트 처리 구현
void UDRAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return; // 유효성 검사 단계

	// 모든 활성화 가능 능력 순회 단계
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		// 동적 태그 검사 단계
		const FString SpecTags = AbilitySpec.GetDynamicSpecSourceTags().ToStringSimple();
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			// 입력 눌림 처리 단계
			AbilitySpecInputPressed(AbilitySpec);
			// 활성화 여부 검사 단계
			if (AbilitySpec.IsActive())
			{
				// 예측 이벤트 전송 단계
				UGameplayAbility* PrimaryInstance = AbilitySpec.GetPrimaryInstance();
				if (PrimaryInstance)
				{
					InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, AbilitySpec.Handle, PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
				}
			}
		}
	}
}

// 입력 태그 유지 이벤트 처리 구현
void UDRAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return; // 유효성 검사 단계

	// 모든 활성화 가능 능력 순회 단계
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		// 동적 태그 검사 단계
		const FString SpecTags = AbilitySpec.GetDynamicSpecSourceTags().ToStringSimple();
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			// 입력 눌림 처리 단계
			AbilitySpecInputPressed(AbilitySpec);
			// 비활성화 상태 검사 단계
			if (!AbilitySpec.IsActive())
			{
				// 능력 활성화 시도 단계
				TryActivateAbility(AbilitySpec.Handle);
			}
		}
	}
}

// 입력 태그 해제 이벤트 처리 구현
void UDRAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return; // 유효성 검사 단계

	// 모든 활성화 가능 능력 순회 단계
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		// 동적 태그 및 활성화 검사 단계
		const FString SpecTags = AbilitySpec.GetDynamicSpecSourceTags().ToStringSimple();
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag) && AbilitySpec.IsActive())
		{
			// 입력 해제 처리 단계
			AbilitySpecInputReleased(AbilitySpec);
			// 예측 이벤트 전송 단계
			UGameplayAbility* PrimaryInstance = AbilitySpec.GetPrimaryInstance();
			if (PrimaryInstance)
			{
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, AbilitySpec.Handle, PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
			}
		}
	}
}

// 능력 순회 콜백 실행 구현
void UDRAbilitySystemComponent::ForEachAbility(const FForEachAbility& Delegate)
{
	// 잠금 범위 설정 단계
	FScopedAbilityListLock ActiveScopeLock(*this); 
	// 활성화 가능 능력 순회 단계
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		// 델리게이트 실행 단계
		if (!Delegate.ExecuteIfBound(AbilitySpec))
		{
			// 실행 실패 로깅 단계
			UE_LOG(LogDR, Error, TEXT("Failed to execute delegate in %hs"), __FUNCTION__);
		}
	}
}

// 스펙에서 능력 태그 추출 구현
FGameplayTag UDRAbilitySystemComponent::GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	// 능력 클래스 존재 검사 단계
	if (AbilitySpec.Ability)
	{
		// 클래스 태그 순회 단계
		for (FGameplayTag Tag : AbilitySpec.Ability.Get()->AbilityTags)
		{
			// 'Abilities' 범위 태그 검사 단계
			if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("Abilities"))))
			{
				return Tag; // 태그 반환 단계
			}
		}
	} 
	return FGameplayTag(); // 기본 태그 반환
}

// 스펙에서 입력 태그 추출 구현
FGameplayTag UDRAbilitySystemComponent::GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	// 동적 태그 순회 단계
	for (FGameplayTag Tag : AbilitySpec.DynamicAbilityTags)
	{
		// 'InputTag' 범위 태그 검사 단계
		if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag(FName("InputTag"))))
		{
			return Tag; // 태그 반환 단계
		}
	}
	return FGameplayTag(); // 기본 태그 반환
}

// 능력 활성화 복제 처리 구현
void UDRAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();

	// 시작능력 부여 여부 검사 단계
	if (!bStartupAbilitiesGiven)
	{
		// 부여 플래그 설정 단계
		bStartupAbilitiesGiven = true;
		// 델리게이트 브로드캐스트 단계
		AbilitiesGivenDelegate.Broadcast();
	}
}

// 클라이언트 효과 적용 알림 RPC 구현
void UDRAbilitySystemComponent::ClientEffectApplied_Implementation(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	// 태그 컨테이너 초기화 단계
	FGameplayTagContainer TagContainer;
	// 자산 태그 수집 단계
	EffectSpec.GetAllAssetTags(TagContainer);

	// 델리게이트 브로드캐스트 단계
	EffectAssetTags.Broadcast(TagContainer);
}
