// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "DRAbilitySystemComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_FiveParams(FEffectAssetTags, const FGameplayTagContainer& /*AssetTags*/, bool /*bHasDuration*/, const float /*Duration*/, bool /*DisplayStackCount*/, const int32 /*StackCount*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FEffectRemovedSignature, const FGameplayTagContainer& /*AssetTags*/);
DECLARE_MULTICAST_DELEGATE(FAbilitiesGiven);
DECLARE_DELEGATE_OneParam(FForEachAbility, const FGameplayAbilitySpec&);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnVendingMachineStacksChanged, int32 /*CurrentStacks*/, int32 /*MaxStacks*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnVacuumDashGaugeChanged, int32 /*CurrentGauge*/, int32 /*MaxGauge*/);
// 차단된 AbilityTag 집합이 바뀌었음을 알리는 신호 (UI 슬롯이 각자 자기 태그 기준으로 재평가)
DECLARE_MULTICAST_DELEGATE(FOnBlockedAbilityTagsChanged);
// 어빌리티 활성화/종료 시 그 어빌리티의 InputTag 를 전달 (UI Pressed/Released 시각 피드백 보조용)
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAbilityInputActivation, const FGameplayTag /*InputTag*/);

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	void AbilityActorInfoSet();

	FEffectAssetTags EffectAssetTags;
	FEffectRemovedSignature EffectRemovedDelegate;
	FAbilitiesGiven AbilitiesGivenDelegate;
	FOnVendingMachineStacksChanged OnVendingMachineStacksChanged;
	FOnVacuumDashGaugeChanged OnVacuumDashGaugeChanged;
	// BlockAbilitiesWithTag 카운터가 바뀌었을 가능성이 있을 때 발화 (UI 재평가용)
	FOnBlockedAbilityTagsChanged OnBlockedAbilityTagsChanged;
	// GA 가 실제로 활성화될 때 그 어빌리티의 InputTag 와 함께 발화 (큐잉된 입력 활성화 시 UI 피드백 보강용)
	FOnAbilityInputActivation OnAbilityActivatedWithInputTag;
	// GA 가 종료될 때 동일 시그니처
	FOnAbilityInputActivation OnAbilityEndedWithInputTag;

	// 외부에서 "지금 이 AbilityTag 가 차단 상태인가?" 를 묻는 헬퍼
	bool IsAbilityTagBlocked(const FGameplayTag& AbilityTag) const;

	// 동적으로 어빌리티가 부여되는 경로(예: 튜토리얼)에서 호출. 등록 누락 방지.
	void RegisterAbilityTagEvents();

	void AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities);
	void AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupPassiveAbilities);
	bool bStartupAbilitiesGiven = false;

	// InputTag 캐시 초기화 (ClearAllAbilities 후 호출)
	void ClearInputTagCache() { InputTagToAbilityMap.Empty(); }

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagHeld(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
	void ForEachAbility(const FForEachAbility& Delegate); 

	static FGameplayTag GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);
	static FGameplayTag GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);

	// 캐시된 InputTag로 AbilitySpec 조회 (O(1) lookup)
	FGameplayAbilitySpec* FindAbilitySpecByInputTag(const FGameplayTag& InputTag);

	// 캐시 갱신 (TutorialManager 등 외부에서 어빌리티 동적 부여 시 사용)
	void AddToInputTagCache(const FGameplayAbilitySpec& AbilitySpec);
	void RemoveFromInputTagCache(const FGameplayTag& InputTag);

	void NotifyVendingMachineStacksChanged(int32 CurrentStacks, int32 MaxStacks);

	// 청소기 돌진 게이지 변경 통지 (서버 GA → 소유 클라 UI, Plan3 §9.2)
	void NotifyVacuumDashGaugeChanged(int32 CurrentGauge, int32 MaxGauge);

protected:
	// InputTag → AbilitySpecHandle 캐시 (성능 최적화)
	UPROPERTY()
	TMap<FGameplayTag, FGameplayAbilitySpecHandle> InputTagToAbilityMap;

	// 차단 UI 트리거용 태그 변화 콜백
	// - LooseGameplayTag(State.Carrying 등) 변화 시 호출
	void HandleWatchedTagChanged(const FGameplayTag Tag, int32 NewCount);
	// - BlockedAbilityTags 카운터가 직접 바뀔 때 호출 (GAS BlockAbilitiesWithTag 의 정확한 변화 시점)
	void HandleBlockedAbilityTagsAnyChange(const FGameplayTag Tag, int32 NewCount);
	// - GA 활성화/종료 시 InputTag 추출해서 UI 피드백 델리게이트로 전달
	void HandleAbilityActivated(UGameplayAbility* Ability);
	void HandleAbilityEnded(UGameplayAbility* Ability);

	// 이미 RegisterGameplayTagEvent 가 등록된 태그 (중복 등록 방지)
	TSet<FGameplayTag> RegisteredWatchedTags;

	// 전체 캐시 재구축
	void RebuildInputTagCache();

	virtual void OnRep_ActivateAbilities() override;

	UFUNCTION(Client, Reliable)
	void ClientEffectApplied(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle);

	UFUNCTION(Client, Reliable)
	void OnRemoveGameplayEffectCallback(const FActiveGameplayEffect& EffectRemoved);

	// 자판기 잭팟 스택 변경을 클라이언트에 전달
	UFUNCTION(Client, Reliable)
	void ClientVendingMachineStacksChanged(int32 CurrentStacks, int32 MaxStacks);

	// 청소기 돌진 게이지 변경을 클라이언트에 전달
	UFUNCTION(Client, Reliable)
	void ClientVacuumDashGaugeChanged(int32 CurrentGauge, int32 MaxGauge);
};
