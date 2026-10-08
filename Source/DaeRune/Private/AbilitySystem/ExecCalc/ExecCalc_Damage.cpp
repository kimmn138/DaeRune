// Copyright DaeRune


#include "AbilitySystem/ExecCalc/ExecCalc_Damage.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/DRCleanserSiteAttributeSet.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Game/DRProgressionConfig.h"

UExecCalc_Damage::UExecCalc_Damage()
{
}

void UExecCalc_Damage::DetermineDebuff(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FGameplayEffectSpec& Spec) const
{
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	for (TTuple<FGameplayTag, FGameplayTag> Pair : GameplayTags.DamageTypesToDebuffs)
	{
		const FGameplayTag& DamageType = Pair.Key;
		const FGameplayTag& DebuffType = Pair.Value;
		const float TypeDamage = Spec.GetSetByCallerMagnitude(DamageType, false, -1.f);
		if (TypeDamage > -.5f) // .5 padding for floating point [im]precision
		{
			// Determine if there was a successful debuff
			const float SourceDebuffChance = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Chance, false, -1.f);

			const bool bDebuff = FMath::RandRange(1, 100) <= SourceDebuffChance;
			if (bDebuff)
			{
				FGameplayEffectContextHandle ContextHandle = Spec.GetContext(); 

				UDRAbilitySystemLibrary::SetIsSuccessfulDebuff(ContextHandle, true);
				UDRAbilitySystemLibrary::SetDamageType(ContextHandle, DamageType);

				const float DebuffDamage = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Damage, false, -1.f);
				const float DebuffDuration = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Duration, false, -1.f);

				UDRAbilitySystemLibrary::SetDebuffDamage(ContextHandle, DebuffDamage);
				UDRAbilitySystemLibrary::SetDebuffDuration(ContextHandle, DebuffDuration);
			}
		}
	}
}

void UExecCalc_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;

	// DebuffMore actions
	DetermineDebuff(ExecutionParams, Spec);

	// Get Damage Set by Caller Magnitude
	float Damage = 0.0f;
	for (const FGameplayTag& DamageTypeTag : FDRGameplayTags::Get().DamageTypeTags)
	{
		float DamageTypeValue = Spec.GetSetByCallerMagnitude(DamageTypeTag, false);
		Damage += DamageTypeValue;
	}

	// Elite Roar Aura: enemies with Buff.Elite.Roar deal 20% more damage
	const UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
	if (SourceASC && SourceASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Buff_Elite_Roar))
	{
		Damage *= 1.2f;
	}

	// ===== 업그레이드 칩: 주는 피해 배율 (Plan3(공용칩) §17.6.1) =====
	//
	// ★여기가 "입히는 모든 피해량"의 유일한 관문이다.★
	// EDRUpgradeStat::SkillDamage 는 어빌리티 단위라 VacuumAirShot::Stages[] 나 디버프 DoT 처럼
	// 그 경로를 안 타는 피해원이 샌다. 칩 ⑥⑦은 반드시 여기서 적용한다.
	//
	// 적/클렌저 사이트가 소스인 경우 GetUpgradeRuntimeForASC 가 빈 캐시를 돌려주므로
	// 배율이 정확히 1.0 이 되어 기존 동작이 그대로 유지된다.
	//
	// ★Plan5(탑승칩)★ 탑승 공유 · 이전 피해(Damage.MountShared)는 ★받는 피해★ 다 — 넘겨주는 사람의
	// "주는 피해" 배율((a) DamageDealt · (b) 광폭화 · (c) 시너지 · (d) 탑승)을 전부 건너뛴다 (D-82 · T-77).
	// 넘어오는 양은 그 사람이 받은 최종 피해(받는 쪽 배율 · 데미지 디버프 반영)라 여기서 더 곱할 것이 없다.
	// (SourceAbilityTags 는 서버 전용 필드지만 ExecCalc 는 서버에서만 돈다)
	const bool bMountShared = UDRAbilitySystemLibrary::GetSourceAbilityTags(Spec.GetContext())
		.HasTagExact(FDRGameplayTags::Get().Damage_MountShared);

	if (SourceASC && !bMountShared)
	{
		const FDRGameplayTags& UpgradeTags = FDRGameplayTags::Get();
		const FDRUpgradeRuntime& SourceRuntime = UDRAbilitySystemLibrary::GetUpgradeRuntimeForASC(SourceASC);
		const AActor* SourceAvatar = SourceASC->GetAvatarActor();

		// (a) 정적 배율 — DamageDealt 스탯. 칩이 없으면 Apply 가 항등이라 정확히 1.0 이다.
		float OutgoingMultiplier = SourceRuntime.Apply(EDRUpgradeStat::DamageDealt, 1.f);

		// (b) 광폭화 — 잃은 컨테이너 수만큼 가산한 뒤 상한을 건다.
		//     ★상한은 (a)+(b) 합에만 건다.★ 시너지(c)는 출처가 다른 축이라 상한 밖에서 곱한다.
		if (SourceRuntime.HasFeature(UpgradeTags.Upgrade_Feature_Combat_Berserk))
		{
			if (const UDRPlayerAttributeSet* SourcePlayerAS =
					UDRAbilitySystemLibrary::GetPlayerAttributeSetFromASC(SourceASC))
			{
				const FDRChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(SourceAvatar);

				// GetCurrentContainerIndex() 는 사망 시 -1 을 돌려주므로 0 으로 바닥을 친다
				const int32 ContainerIndex = FMath::Max(0, SourcePlayerAS->GetCurrentContainerIndex());
				const int32 LostContainers =
					FMath::Max(0, SourcePlayerAS->GetNumContainers() - 1 - ContainerIndex);

				OutgoingMultiplier += Tuning.BerserkBonusPerLostContainer * LostContainers;
				OutgoingMultiplier = FMath::Min(OutgoingMultiplier, Tuning.BerserkMaxMultiplier);
			}
		}

		// (c) 시너지 오라 — ★이건 ASC 의 진짜 게임플레이 태그다★ (칩 피처 태그가 아니다).
		//     태그 카운트 = 나를 버프하고 있는 팀원 수 = 스택 수. (§17.7.3)
		const int32 SynergyStacks = SourceASC->GetGameplayTagCount(UpgradeTags.Buff_Upgrade_Synergy);
		if (SynergyStacks > 0)
		{
			const FDRChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(SourceAvatar);
			OutgoingMultiplier *= (1.f + Tuning.SynergyDamageDealtPerStack * SynergyStacks);
		}

		// (d) ★Plan5(탑승칩)★ ① 탑승 공격력 — 탑의 맨 아래 청소기가 칩①이면 위 인원 × 8% (상한 24%).
		//     시너지처럼 광폭화 상한 밖에서 곱한다 (D-78). 칩 · 탑승이 없으면 정확히 1.0.
		//     블록 자체가 공유 · 이전 피해를 건너뛰므로 여기서 따로 거를 것이 없다 (D-82)
		OutgoingMultiplier *= UDRAbilitySystemLibrary::GetRideDamageMultiplier(SourceAvatar);

		if (!FMath::IsNearlyEqual(OutgoingMultiplier, 1.f))
		{
			Damage *= OutgoingMultiplier;
		}
	}

	// 타겟의 AttributeSet 타입에 따라 올바른 IncomingDamage Attribute 사용
	UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();
	FGameplayAttribute IncomingDamageAttribute;

	if (TargetASC)
	{
		// 클렌저 사이트 AttributeSet 체크
		const bool bTargetIsCleanserSite = TargetASC->HasAttributeSetForAttribute(UDRCleanserSiteAttributeSet::GetIncomingDamageAttribute());
		if (bTargetIsCleanserSite)
		{
			IncomingDamageAttribute = UDRCleanserSiteAttributeSet::GetIncomingDamageAttribute();
		}
		// 일반 캐릭터 AttributeSet
		else
		{
			IncomingDamageAttribute = UDRAttributeSet::GetIncomingDamageAttribute();
		}

		// New Phase1 적 → 플레이어/적 대미지 0.7배 (클렌저 사이트 제외)
		if (!bTargetIsCleanserSite
			&& SourceASC
			&& SourceASC->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Enemy_Phase1))
		{
			Damage *= 0.7f;
		}

		// ★Plan5(씨앗칩)★ 음수 피해는 곧 회복이다(적 체력이 차오른다).
		// 업그레이드 칩은 곱연산이라 뒤집히지 않지만, 여기는 ★모든 피해가 지나는 단 하나의 관문★ 이라
		// 칩 밖의 배율(포효 ×1.2 / Phase1 ×0.7 / 향후 추가분)까지 이 한 줄로 덮을 수 있다.
		Damage = FMath::Max(0.f, Damage);

		const FGameplayModifierEvaluatedData EvaluatedData(IncomingDamageAttribute, EGameplayModOp::Additive, Damage);
		OutExecutionOutput.AddOutputModifier(EvaluatedData);
	}
}
