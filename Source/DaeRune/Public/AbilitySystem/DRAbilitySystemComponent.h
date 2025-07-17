// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "DRAbilitySystemComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FEffectAssetTags, const FGameplayTagContainer& /*AssetTags*/); // 효과 자산 태그 브로드캐스트 델리게이트 선언
DECLARE_MULTICAST_DELEGATE(FAbilitiesGiven); // 능력 부여 완료 델리게이트 선언
DECLARE_DELEGATE_OneParam(FForEachAbility, const FGameplayAbilitySpec&); // 능력마다 콜백 실행 델리게이트 선언

/**
 * UDRAbilitySystemComponent 클래스: DaeRune 전용 능력 시스템 컴포넌트
 */
UCLASS()
class DAERUNE_API UDRAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()
	
public:
	// 액터 정보 설정 완료 시점 처리 기능
	void AbilityActorInfoSet();

	// 효과 자산 태그 델리게이트 인스턴스
	FEffectAssetTags EffectAssetTags;
	// 능력 부여 완료 델리게이트 인스턴스
	FAbilitiesGiven AbilitiesGivenDelegate;

	// 캐릭터 능력 부여 기능
	void AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities);
	// 캐릭터 수동 능력(패시브) 부여 기능
	void AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupPassiveAbilities);
	// 스타트업 능력 부여 여부 플래그
	bool bStartupAbilitiesGiven = false;

	// 입력 태그 눌림 이벤트 처리 기능
	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	// 입력 태그 유지 이벤트 처리 기능
	void AbilityInputTagHeld(const FGameplayTag& InputTag);
	// 입력 태그 해제 이벤트 처리 기능
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
	// 시작 능력마다 실행할 콜백 처리 기능
	void ForEachAbility(const FForEachAbility& Delegate); 

	// 스펙에서 능력 태그 추출 기능
	static FGameplayTag GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);
	// 스펙에서 입력 태그 추출 기능
	static FGameplayTag GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);

protected:
	// 활성화 능력 복제 시 처리 오버라이드 기능
	virtual void OnRep_ActivateAbilities() override;

	// 클라이언트 측 효과 적용 알림 RPC
	UFUNCTION(Client, Reliable)
	void ClientEffectApplied(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle);
};
