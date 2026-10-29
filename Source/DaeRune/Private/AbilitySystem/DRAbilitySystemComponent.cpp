// Copyright DaeRune


#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "DaeRune/DRLogChannels.h"

void UDRAbilitySystemComponent::AbilityActorInfoSet()
{
	if (bActorInfoDelegatesBound) return;
	bActorInfoDelegatesBound = true;

	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UDRAbilitySystemComponent::HandleEffectAppliedToSelf);
	OnAnyGameplayEffectRemovedDelegate().AddUObject(this, &UDRAbilitySystemComponent::HandleEffectRemoved);

	// BlockedAbilityTags 카운터 변화를 직접 hook.
	// (GAS PreActivate 가 AddLooseGameplayTags → ApplyAbilityBlockAndCancelTags 순서라
	//  owned tag 이벤트를 트리거로 쓰면 BlockedAbilityTags 가 아직 반영 전이라 결과가 한 박자 늦게 잡힘)
	BlockedAbilityTags.RegisterGenericGameplayEvent()
		.AddUObject(this, &UDRAbilitySystemComponent::HandleAbilityBlockStateChanged);

	// State.Carrying 등 Loose 태그 변화도 스킬 슬롯 재평가가 필요하다
	RegisterGameplayTagEvent(FDRGameplayTags::Get().State_Carrying, EGameplayTagEventType::NewOrRemoved)
		.AddUObject(this, &UDRAbilitySystemComponent::HandleAbilityBlockStateChanged);

	// GA 활성화/종료 hook: 큐잉되어 나중에 발동된 어빌리티의 PressedImage 시각 피드백 보강
	AbilityActivatedCallbacks.AddUObject(this, &UDRAbilitySystemComponent::HandleAbilityActivated);
	AbilityEndedCallbacks.AddUObject(this, &UDRAbilitySystemComponent::HandleAbilityEnded);
}

void UDRAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		if (!AbilityClass) continue;

		FGameplayAbilitySpec AbilitySpec(AbilityClass, 1);
		if (const UDRGameplayAbility* DRAbility = Cast<UDRGameplayAbility>(AbilitySpec.Ability))
		{
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(DRAbility->StartupInputTag);
		}
		else
		{
			// 입력 태그가 없어 키로는 발동하지 않는다 (이벤트 · 태그 트리거로만)
			UE_LOG(LogDR, Warning, TEXT("[ASC] %s 는 UDRGameplayAbility 가 아니라 입력 태그 없이 부여합니다."),
				*GetNameSafe(AbilityClass));
		}
		GiveAbility(AbilitySpec);
	}
	bStartupAbilitiesGiven = true;
	AbilitiesGivenDelegate.Broadcast();
}

void UDRAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return;
	// Block.InputPressed 는 호출자(ADRPlayerController::AbilityInputTagPressed)가 UI 피드백과 함께 이미 거른다

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
		for (const FGameplayTag& Tag : AbilitySpec.Ability->GetAssetTags())
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
	for (const FGameplayTag& Tag : AbilitySpec.GetDynamicSpecSourceTags())
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

bool UDRAbilitySystemComponent::HasStatusIconTag(const FGameplayTagContainer& Tags)
{
	static const FGameplayTag BuffRoot = FGameplayTag::RequestGameplayTag(TEXT("Buff"));
	static const FGameplayTag DebuffRoot = FGameplayTag::RequestGameplayTag(TEXT("Debuff"));
	return Tags.HasTag(BuffRoot) || Tags.HasTag(DebuffRoot);
}

void UDRAbilitySystemComponent::HandleEffectAppliedToSelf(UAbilitySystemComponent* Source, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle)
{
	FGameplayTagContainer GrantedTags;
	EffectSpec.GetAllGrantedTags(GrantedTags);
	if (!HasStatusIconTag(GrantedTags)) return;

	// 스택형이면 엔진이 넘기는 스펙이 이미 쌓인 활성 효과의 스펙이라 총 스택 수가 들어 있다
	const bool bHasDuration = EffectSpec.Def->DurationPolicy == EGameplayEffectDurationType::HasDuration;
	const bool bDisplayStackCount = EffectSpec.Def->StackingType != EGameplayEffectStackingType::None && EffectSpec.Def->StackLimitCount > 1;

	ClientEffectApplied(GrantedTags, bHasDuration, EffectSpec.Duration, bDisplayStackCount, EffectSpec.GetStackCount());
}

void UDRAbilitySystemComponent::ClientEffectApplied_Implementation(const FGameplayTagContainer& GrantedTags, bool bHasDuration, float Duration, bool bDisplayStackCount, int32 StackCount)
{
	EffectAssetTags.Broadcast(GrantedTags, bHasDuration, Duration, bDisplayStackCount, StackCount);
}

void UDRAbilitySystemComponent::HandleEffectRemoved(const FActiveGameplayEffect& EffectRemoved)
{
	FGameplayTagContainer GrantedTags;
	EffectRemoved.Spec.GetAllGrantedTags(GrantedTags);
	if (!HasStatusIconTag(GrantedTags)) return;

	EffectRemovedDelegate.Broadcast(GrantedTags);
}

FGameplayAbilitySpec* UDRAbilitySystemComponent::FindAbilitySpecByInputTag(const FGameplayTag& InputTag)
{
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			return &AbilitySpec;
		}
	}
	return nullptr;
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

void UDRAbilitySystemComponent::HandleAbilityBlockStateChanged(const FGameplayTag Tag, int32 NewCount)
{
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
