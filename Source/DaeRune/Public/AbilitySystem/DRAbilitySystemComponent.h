// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "DRAbilitySystemComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_FiveParams(FEffectAssetTags, const FGameplayTagContainer& /*AssetTags*/, bool /*bHasDuration*/, const float /*Duration*/, bool /*DisplayStackCount*/, const int32 /*StackCount*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FEffectRemovedSignature, const FGameplayTagContainer& /*AssetTags*/);
DECLARE_MULTICAST_DELEGATE(FAbilitiesGiven);
DECLARE_DELEGATE_OneParam(FForEachAbility, const FGameplayAbilitySpec&);

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

protected:
	// InputTag → AbilitySpecHandle 캐시 (성능 최적화)
	UPROPERTY()
	TMap<FGameplayTag, FGameplayAbilitySpecHandle> InputTagToAbilityMap;

	// 전체 캐시 재구축
	void RebuildInputTagCache();

	virtual void OnRep_ActivateAbilities() override;

	UFUNCTION(Client, Reliable)
	void ClientEffectApplied(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle);

	UFUNCTION(Client, Reliable)
	void OnRemoveGameplayEffectCallback(const FActiveGameplayEffect& EffectRemoved);
};
