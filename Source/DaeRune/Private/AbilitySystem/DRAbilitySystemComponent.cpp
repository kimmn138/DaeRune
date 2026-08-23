// Copyright DaeRune


#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "DaeRune/DRLogChannels.h"

void UDRAbilitySystemComponent::AbilityActorInfoSet()
{
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UDRAbilitySystemComponent::ClientEffectApplied);
	OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &UDRAbilitySystemComponent::OnRemoveGameplayEffectCallback);

	// BlockedAbilityTags 카운터 변화를 직접 hook.
	// (GAS PreActivate 가 AddLooseGameplayTags → ApplyAbilityBlockAndCancelTags 순서라
	//  owned tag 이벤트를 트리거로 쓰면 BlockedAbilityTags 가 아직 반영 전이라 결과가 한 박자 늦게 잡힘)
	BlockedAbilityTags.RegisterGenericGameplayEvent()
		.AddUObject(this, &UDRAbilitySystemComponent::HandleBlockedAbilityTagsAnyChange);

	// GA 활성화/종료 hook: 큐잉되어 나중에 발동된 어빌리티의 PressedImage 시각 피드백 보강
	AbilityActivatedCallbacks.AddUObject(this, &UDRAbilitySystemComponent::HandleAbilityActivated);
	AbilityEndedCallbacks.AddUObject(this, &UDRAbilitySystemComponent::HandleAbilityEnded);

	// 글로벌(캐릭터 무관) 감시 태그: State.Carrying 등 LooseGameplayTag 변화도 UI 재평가가 필요
	static const TArray<FGameplayTag> GlobalWatchedTags = {
		FDRGameplayTags::Get().State_Carrying,
	};
	for (const FGameplayTag& Tag : GlobalWatchedTags)
	{
		if (!Tag.IsValid() || RegisteredWatchedTags.Contains(Tag)) continue;
		RegisteredWatchedTags.Add(Tag);
		RegisterGameplayTagEvent(Tag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UDRAbilitySystemComponent::HandleWatchedTagChanged);
	}
}

void UDRAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		if (const UDRGameplayAbility* DRAbility = Cast<UDRGameplayAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.DynamicAbilityTags.AddTag(DRAbility->StartupInputTag);
			FGameplayAbilitySpecHandle Handle = GiveAbility(AbilitySpec);

			// InputTag 캐시에 추가
			if (DRAbility->StartupInputTag.IsValid())
			{
				InputTagToAbilityMap.Add(DRAbility->StartupInputTag, Handle);
			}
		}
	}
	bStartupAbilitiesGiven = true;
	AbilitiesGivenDelegate.Broadcast();
}

void UDRAbilitySystemComponent::AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupPassiveAbilities)
{
	for (const TSubclassOf<UGameplayAbility> AbilityClass : StartupPassiveAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		GiveAbilityAndActivateOnce(AbilitySpec);
	}
}

void UDRAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	if (HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed)) return;

	// 캐시를 사용한 O(1) lookup
	FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecByInputTag(InputTag);
	if (AbilitySpec)
	{
		AbilitySpecInputPressed(*AbilitySpec);
		if (AbilitySpec->IsActive())
		{
			UGameplayAbility* PrimaryInstance = AbilitySpec->GetPrimaryInstance();
			if (PrimaryInstance)
			{
				InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputPressed, AbilitySpec->Handle, PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
			}
		}
	}
}

void UDRAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	if (HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputHeld)) return;

	// 캐시를 사용한 O(1) lookup
	FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecByInputTag(InputTag);
	if (AbilitySpec)
	{
		AbilitySpecInputPressed(*AbilitySpec);
		if (!AbilitySpec->IsActive())
		{
			TryActivateAbility(AbilitySpec->Handle);
		}
	}
}

void UDRAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	if (HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputReleased)) return;

	// 캐시를 사용한 O(1) lookup
	FGameplayAbilitySpec* AbilitySpec = FindAbilitySpecByInputTag(InputTag);
	if (AbilitySpec && AbilitySpec->IsActive())
	{
		AbilitySpecInputReleased(*AbilitySpec);
		UGameplayAbility* PrimaryInstance = AbilitySpec->GetPrimaryInstance();
		if (PrimaryInstance)
		{
			InvokeReplicatedEvent(EAbilityGenericReplicatedEvent::InputReleased, AbilitySpec->Handle, PrimaryInstance->GetCurrentActivationInfo().GetActivationPredictionKey());
		}
	}
}

void UDRAbilitySystemComponent::ForEachAbility(const FForEachAbility& Delegate)
{
	FScopedAbilityListLock ActiveScopeLock(*this); 
	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!Delegate.ExecuteIfBound(AbilitySpec))
		{
			UE_LOG(LogDR, Error, TEXT("Failed to execute delegate in %hs"), __FUNCTION__);
		}
	}
}

FGameplayTag UDRAbilitySystemComponent::GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	if (AbilitySpec.Ability)
	{
		// 캐시된 태그 사용 (FGameplayTag::RequestGameplayTag 호출 비용 제거)
		static const FGameplayTag AbilitiesTag = FGameplayTag::RequestGameplayTag(FName("Abilities"));
		for (const FGameplayTag& Tag : AbilitySpec.Ability.Get()->AbilityTags)
		{
			if (Tag.MatchesTag(AbilitiesTag))
			{
				return Tag;
			}
		}
	}
	return FGameplayTag();
}

FGameplayTag UDRAbilitySystemComponent::GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	// 캐시된 태그 사용 (FGameplayTag::RequestGameplayTag 호출 비용 제거)
	static const FGameplayTag InputTagBase = FGameplayTag::RequestGameplayTag(FName("InputTag"));
	for (const FGameplayTag& Tag : AbilitySpec.DynamicAbilityTags)
	{
		if (Tag.MatchesTag(InputTagBase))
		{
			return Tag;
		}
	}
	return FGameplayTag();
}

void UDRAbilitySystemComponent::OnRep_ActivateAbilities()
{
	Super::OnRep_ActivateAbilities();

	if (!bStartupAbilitiesGiven)
	{
		bStartupAbilitiesGiven = true;
		AbilitiesGivenDelegate.Broadcast();
	}
}

void UDRAbilitySystemComponent::OnRemoveGameplayEffectCallback_Implementation(const FActiveGameplayEffect& EffectRemoved)
{
	FGameplayTagContainer TagContainer;
	EffectRemoved.Spec.GetAllGrantedTags(TagContainer);
	
	EffectRemovedDelegate.Broadcast(TagContainer);
}

void UDRAbilitySystemComponent::ClientEffectApplied_Implementation(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	FGameplayTagContainer TagContainer;
	EffectSpec.GetAllGrantedTags(TagContainer);

	const bool HasDuration = EffectSpec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration;
	const bool DisplayStackCount = EffectSpec.Def->StackingType != EGameplayEffectStackingType::None && EffectSpec.Def->StackLimitCount > 1;

	EffectAssetTags.Broadcast(TagContainer, HasDuration, EffectSpec.Duration, DisplayStackCount, EffectSpec.GetStackCount());
}

FGameplayAbilitySpec* UDRAbilitySystemComponent::FindAbilitySpecByInputTag(const FGameplayTag& InputTag)
{
	// 캐시에서 Handle 조회
	if (const FGameplayAbilitySpecHandle* HandlePtr = InputTagToAbilityMap.Find(InputTag))
	{
		return FindAbilitySpecFromHandle(*HandlePtr);
	}

	// 캐시 미스: 폴백으로 전체 검색 (캐시 동기화 문제 대비)
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			// 캐시에 추가
			InputTagToAbilityMap.Add(InputTag, AbilitySpec.Handle);
			return &AbilitySpec;
		}
	}

	return nullptr;
}

void UDRAbilitySystemComponent::RebuildInputTagCache()
{
	InputTagToAbilityMap.Empty();

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		AddToInputTagCache(AbilitySpec);
	}
}

void UDRAbilitySystemComponent::AddToInputTagCache(const FGameplayAbilitySpec& AbilitySpec)
{
	FGameplayTag InputTag = GetInputTagFromSpec(AbilitySpec);
	if (InputTag.IsValid())
	{
		InputTagToAbilityMap.Add(InputTag, AbilitySpec.Handle);
	}
}

void UDRAbilitySystemComponent::RemoveFromInputTagCache(const FGameplayTag& InputTag)
{
	InputTagToAbilityMap.Remove(InputTag);
}

void UDRAbilitySystemComponent::NotifyVendingMachineStacksChanged(int32 CurrentStacks, int32 MaxStacks)
{
	ClientVendingMachineStacksChanged(CurrentStacks, MaxStacks);
}

void UDRAbilitySystemComponent::ClientVendingMachineStacksChanged_Implementation(int32 CurrentStacks, int32 MaxStacks)
{
	OnVendingMachineStacksChanged.Broadcast(CurrentStacks, MaxStacks);
}

void UDRAbilitySystemComponent::NotifyVacuumDashGaugeChanged(int32 CurrentGauge, int32 MaxGauge)
{
	ClientVacuumDashGaugeChanged(CurrentGauge, MaxGauge);
}

void UDRAbilitySystemComponent::ClientVacuumDashGaugeChanged_Implementation(int32 CurrentGauge, int32 MaxGauge)
{
	OnVacuumDashGaugeChanged.Broadcast(CurrentGauge, MaxGauge);
}

void UDRAbilitySystemComponent::NotifyVacuumAirShotGaugeChanged(int32 CurrentGauge, int32 MaxGauge)
{
	// 호출자(GA)가 이미 소유 클라 인스턴스임을 보장 → 그대로 로컬 방송
	OnVacuumAirShotGaugeChanged.Broadcast(CurrentGauge, MaxGauge);
}

bool UDRAbilitySystemComponent::IsAbilityTagBlocked(const FGameplayTag& AbilityTag) const
{
	if (!AbilityTag.IsValid()) return false;
	FGameplayTagContainer Single;
	Single.AddTag(AbilityTag);
	return AreAbilityTagsBlocked(Single);
}

void UDRAbilitySystemComponent::RegisterAbilityTagEvents()
{
	// BlockedAbilityTags 직접 hook 으로 충분해진 후로는 이 함수가 사실상 필요 없지만,
	// 외부 동적 부여 경로에서 안전하게 호출해도 무해하도록 유지 (no-op).
}

void UDRAbilitySystemComponent::HandleWatchedTagChanged(const FGameplayTag Tag, int32 NewCount)
{
	// State.Carrying 같은 LooseGameplayTag 변화 시 UI 재평가 트리거
	OnBlockedAbilityTagsChanged.Broadcast();
}

void UDRAbilitySystemComponent::HandleBlockedAbilityTagsAnyChange(const FGameplayTag Tag, int32 NewCount)
{
	// BlockedAbilityTags 카운터가 0↔양수로 바뀐 정확한 시점. UI 슬롯 재평가.
	OnBlockedAbilityTagsChanged.Broadcast();
}

void UDRAbilitySystemComponent::HandleAbilityActivated(UGameplayAbility* Ability)
{
	if (!Ability) return;
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Ability->GetCurrentAbilitySpecHandle());
	if (!Spec) return;

	const FGameplayTag InputTag = GetInputTagFromSpec(*Spec);
	if (InputTag.IsValid())
	{
		OnAbilityActivatedWithInputTag.Broadcast(InputTag);
	}
}

void UDRAbilitySystemComponent::HandleAbilityEnded(UGameplayAbility* Ability)
{
	if (!Ability) return;
	FGameplayAbilitySpec* Spec = FindAbilitySpecFromHandle(Ability->GetCurrentAbilitySpecHandle());
	if (!Spec) return;

	const FGameplayTag InputTag = GetInputTagFromSpec(*Spec);
	if (InputTag.IsValid())
	{
		OnAbilityEndedWithInputTag.Broadcast(InputTag);
	}
}
