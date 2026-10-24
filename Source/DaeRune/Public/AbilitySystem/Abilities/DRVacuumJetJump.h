// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DRVacuumJetJump.generated.h"

/**
 * 로봇 청소기 1번 스킬: 더블 점프 (Q, Plan3 §7)
 * - 물 30 소모 즉발 점프 + 시전 지점 2D 반경 AoE 데미지 (Z 무시 — 높이 차이 있는 적도 수평거리로 적중)
 * - 지상 Q → 공중 Q, 공중 사용은 AirJumpsUsed 로 MaxAirJumps(칩 반영)회 제한 (착지 시 Landed()에서 리셋)
 * - 연출은 GameplayCue.Skill.VacuumJetJump (서버 Execute → 전 클라 재생)
 */
UCLASS()
class DAERUNE_API UDRVacuumJetJump : public UDRDamageGameplayAbility
{
	GENERATED_BODY()

public:
	UDRVacuumJetJump();

protected:
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr, const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	// 점프 임펄스 (Z 오버라이드 — 낙하 중에도 일정한 점프)
	UPROPERTY(EditDefaultsOnly, Category = "JetJump")
	float JumpImpulseZ = 900.f;

	// 내부 반경 (최대 데미지)
	UPROPERTY(EditDefaultsOnly, Category = "JetJump")
	float InnerRadius = 200.f;

	UPROPERTY(EditDefaultsOnly, Category = "JetJump")
	float InnerDamage = 20.f;

	// 외부 반경 (감소 데미지)
	UPROPERTY(EditDefaultsOnly, Category = "JetJump")
	float OuterRadius = 400.f;

	UPROPERTY(EditDefaultsOnly, Category = "JetJump")
	float OuterDamage = 5.f;

	// ★Plan5(물점프칩)★ 공중 사용 가능 횟수 기본값 (칩③ SkillAirUseCount Flat +1 → 2). 착지하면 다시 채워진다
	UPROPERTY(EditDefaultsOnly, Category = "JetJump", meta = (ClampMin = "0"))
	int32 MaxAirJumps = 1;

	// ★Plan5(물점프칩)★ 칩이 반영된 공중 사용 가능 횟수.
	// ★CanActivateAbility 는 CDO 에서 불릴 수 있어 ActorInfo 를 직접 받는다★ — GetUpgradedInt 는 현재 ActorInfo 판이다 (Plan5 T-114)
	int32 GetMaxAirJumpsFor(const FGameplayAbilityActorInfo* ActorInfo) const;
};
