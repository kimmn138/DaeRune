// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "Game/DRUpgradeTypes.h"
#include "DRGameplayAbility.generated.h"

/**
 *
 */
UCLASS()
class DAERUNE_API UDRGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag StartupInputTag;

    // Water Cost ���� (��������Ʈ���� ���� ����)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost")
    float WaterCost = 0.f;

    // Cost üũ �������̵�
    virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const override;

    // Cost ���� �������̵�
    virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;

    // 스킬 차단 UI 트리거를 위해 AbilityTags 를 ActivationOwnedTags 에 자동 머지
    virtual void OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec) override;

    // ========== 업그레이드 칩 반영 (Plan2.md 8.5) ==========

    // 쿨다운 기본값. 0보다 크면 쿨다운 GE 의 Duration 을 SetByCaller(Data.Cooldown) 로 주입한다.
    // 0이면 GE 에 설정된 고정 Duration 이 그대로 쓰인다(기존 어빌리티 하위 호환).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown")
    float CooldownDuration = 0.f;

    // 업그레이드로 줄어든 쿨다운의 하한
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cooldown", meta = (ClampMin = "0.01"))
    float MinCooldownDuration = 0.05f;

    // 이 어빌리티의 업그레이드 키 태그 (GetAssetTags() 중 "Abilities" 접두 태그)
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    FGameplayTag GetUpgradeKeyTag() const;

    // 소유 PlayerState 의 장착 칩 캐시. 없으면 항등(빈) 캐시를 돌려주므로 호출부에 분기가 필요 없다.
    const FDRUpgradeRuntime& GetUpgradeRuntime() const;

    // ActorInfo 를 직접 받는 버전.
    // CheckCost/ApplyCost/ApplyCooldown 은 CDO 에서 호출될 수 있어 GetCurrentActorInfo() 가 비어 있을 수 있다.
    // 그 경로에서는 반드시 전달받은 ActorInfo 를 써야 CheckCost 와 ApplyCost 의 수치가 어긋나지 않는다.
    const FDRUpgradeRuntime& GetUpgradeRuntimeFor(const FGameplayAbilityActorInfo* ActorInfo) const;

    // 스킬 단위 스탯 조회
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    float GetUpgradedFloat(EDRUpgradeStat Stat, float BaseValue) const;

    // 스킬 단위 정수 스탯 조회 (반올림 + 하한 클램프)
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    int32 GetUpgradedInt(EDRUpgradeStat Stat, int32 BaseValue, int32 MinValue = 1) const;

    // 업그레이드가 반영된 물 소모량 (0 하한)
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    float GetEffectiveWaterCost() const;

    // ActorInfo 를 직접 받는 물 소모량 (CheckCost/ApplyCost 전용)
    float GetEffectiveWaterCostFor(const FGameplayAbilityActorInfo* ActorInfo) const;

protected:
    // 쿨다운 적용 오버라이드 — CooldownDuration > 0 일 때 업그레이드 보정값을 SetByCaller 로 주입
    virtual void ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const override;
};
