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
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnVacuumAirShotGaugeChanged, int32 /*CurrentGauge*/, int32 /*MaxGauge*/);
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
	// 소유 플레이어 UI 용 콜백을 묶는다 (플레이어 전용 — 적은 리스너가 없어 부르지 않는다).
	// ASC 는 PlayerState 에 있어 리스폰 · 클래스 변경으로 다시 빙의해도 같은 객체다 — 여러 번 불려도 1회만 묶는다.
	void AbilityActorInfoSet();

	FEffectAssetTags EffectAssetTags;
	FEffectRemovedSignature EffectRemovedDelegate;
	FAbilitiesGiven AbilitiesGivenDelegate;
	FOnVendingMachineStacksChanged OnVendingMachineStacksChanged;
	FOnVacuumDashGaugeChanged OnVacuumDashGaugeChanged;
	FOnVacuumAirShotGaugeChanged OnVacuumAirShotGaugeChanged;
	// BlockAbilitiesWithTag 카운터가 바뀌었을 가능성이 있을 때 발화 (UI 재평가용)
	FOnBlockedAbilityTagsChanged OnBlockedAbilityTagsChanged;
	// GA 가 실제로 활성화될 때 그 어빌리티의 InputTag 와 함께 발화 (큐잉된 입력 활성화 시 UI 피드백 보강용)
	FOnAbilityInputActivation OnAbilityActivatedWithInputTag;
	// GA 가 종료될 때 동일 시그니처
	FOnAbilityInputActivation OnAbilityEndedWithInputTag;

	// 외부에서 "지금 이 AbilityTag 가 차단 상태인가?" 를 묻는 헬퍼
	bool IsAbilityTagBlocked(const FGameplayTag& AbilityTag) const;

	void AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities);
	bool bStartupAbilitiesGiven = false;

	void AbilityInputTagPressed(const FGameplayTag& InputTag);
	void AbilityInputTagHeld(const FGameplayTag& InputTag);
	void AbilityInputTagReleased(const FGameplayTag& InputTag);
	void ForEachAbility(const FForEachAbility& Delegate); 

	static FGameplayTag GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);
	static FGameplayTag GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);

	// InputTag 로 AbilitySpec 조회. 스펙이 몇 개뿐이라 매번 찾는다 —
	// 캐시를 두면 서버가 어빌리티를 다시 부여할 때(로비 클래스 변경) 클라 캐시가 낡은 핸들을 가리켜 입력이 무시된다.
	FGameplayAbilitySpec* FindAbilitySpecByInputTag(const FGameplayTag& InputTag);

	void NotifyVendingMachineStacksChanged(int32 CurrentStacks, int32 MaxStacks);

	// 청소기 돌진 게이지 변경 통지 (서버 GA → 소유 클라 UI, Plan3 §9.2)
	void NotifyVacuumDashGaugeChanged(int32 CurrentGauge, int32 MaxGauge);

	// 청소기 일반 공격(공기탄) 충전 게이지 변경 통지.
	// 돌진과 달리 충전이 시간 기반이라 소유 클라 인스턴스가 같은 값을 로컬로 계산하므로 RPC 없이 즉시 방송한다.
	// (반드시 소유 클라에서 도는 GA 인스턴스에서만 호출 — 서버 인스턴스까지 호출하면 값이 중복 방송된다)
	void NotifyVacuumAirShotGaugeChanged(int32 CurrentGauge, int32 MaxGauge);

protected:
	// AbilityActorInfoSet 이 콜백을 이미 묶었는지
	bool bActorInfoDelegatesBound = false;

	// 스킬 슬롯 차단 UI 재평가 신호 — BlockedAbilityTags 카운터(GAS BlockAbilitiesWithTag 의 정확한 변화 시점)와
	// State.Carrying 같은 Loose 태그 변화 둘 다에서 온다
	void HandleAbilityBlockStateChanged(const FGameplayTag Tag, int32 NewCount);
	// GA 활성화/종료 시 InputTag 추출해서 UI 피드백 델리게이트로 전달
	void HandleAbilityActivated(UGameplayAbility* Ability);
	void HandleAbilityEnded(UGameplayAbility* Ability);

	// 상태 아이콘(Buff.* / Debuff.*) 효과 적용 (서버) → 소유 클라에 필요한 값만 보낸다
	void HandleEffectAppliedToSelf(UAbilitySystemComponent* Source, const FGameplayEffectSpec& EffectSpec, FActiveGameplayEffectHandle ActiveEffectHandle);
	// 상태 아이콘 효과 제거 — 서버와 소유 클라(복제된 제거) 각자 로컬에서 방송한다
	void HandleEffectRemoved(const FActiveGameplayEffect& EffectRemoved);
	// 상태 아이콘(Buff.* / Debuff.*) 태그가 있는지
	static bool HasStatusIconTag(const FGameplayTagContainer& Tags);

	virtual void OnRep_ActivateAbilities() override;

	// 상태 아이콘 효과 적용을 소유 클라에 알린다 — 피해처럼 아이콘과 무관한 효과는 보내지 않는다
	UFUNCTION(Client, Reliable)
	void ClientEffectApplied(const FGameplayTagContainer& GrantedTags, bool bHasDuration, float Duration, bool bDisplayStackCount, int32 StackCount);

	// 자판기 잭팟 스택 변경을 클라이언트에 전달
	UFUNCTION(Client, Reliable)
	void ClientVendingMachineStacksChanged(int32 CurrentStacks, int32 MaxStacks);

	// 청소기 돌진 게이지 변경을 클라이언트에 전달
	UFUNCTION(Client, Reliable)
	void ClientVacuumDashGaugeChanged(int32 CurrentGauge, int32 MaxGauge);
};
