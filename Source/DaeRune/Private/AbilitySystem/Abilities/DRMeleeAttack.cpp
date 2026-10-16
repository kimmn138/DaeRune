// Copyright DaeRune


#include "AbilitySystem/Abilities/DRMeleeAttack.h"

#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Character/DRCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "DRGameplayTags.h"
#include "GameFramework/Character.h"
// Plan5(발톱칩) — 업그레이드 조회 / 칩 튜닝 상수 / 체력 어트리뷰트
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Game/DRProgressionConfig.h"

// ========================= Plan5(발톱칩) — 칩이 반영된 조회 =========================

float UDRMeleeAttack::GetUpgradedAttackRadius() const
{
	// ★Plan5(씨앗칩)에서 만든 SkillAreaRadius 축을 그대로 재사용한다★ — 새 축이 아니다.
	// 모디파이어마다 TargetAbilityTag 를 지정하므로 씨앗 폭탄의 범위 칩과 간섭하지 않는다.
	//
	// 0 이 되면 구 오버랩이 아무도 못 잡으므로 1uu 하한을 둔다.
	return FMath::Max(1.f, GetUpgradedFloat(EDRUpgradeStat::SkillAreaRadius, AttackRadius));
}

float UDRMeleeAttack::GetWindupPlayRate() const
{
	// ★기준값 1.0 에 곱한다★ — 칩이 없으면 정확히 1.0 이 나와 BaseMontagePlayRate 가 그대로 쓰인다.
	// SkillWindup 은 "초"가 아니라 "배율"이다(선딜 길이는 몽타주 에셋 안에 있어 알 수 없다).
	const float WindupScale = FMath::Max(0.01f, GetUpgradedFloat(EDRUpgradeStat::SkillWindup, 1.f));

	// 선딜 3배 → 재생 속도 1/3. 노티파이까지의 실시간이 3배가 된다.
	// ★Plan5(공속칩)★ 공속 배율(자판기 칩 ② 공유 버프 등)을 곱한다 — 버프가 없으면 1.0 (D-67).
	//   선딜 배율과는 곱으로 합성된다: PlayRate = Base × M / WindupScale
	return BaseMontagePlayRate * GetAttackSpeedMultiplier() / WindupScale;
}

void UDRMeleeAttack::RestoreMontagePlayRate()
{
	ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	if (!Avatar) return;

	// ★Plan5(공속칩)★ 후딜(노티파이 뒤)도 공속 배율만큼 빨라야 한다 — 선딜 배율만 걷어 낸다 (D-67 · T-63)
	const float RestoredPlayRate = BaseMontagePlayRate * GetAttackSpeedMultiplier();

	// ★1P·3P 두 메시 모두★ 되돌려야 손과 몸이 따로 놀지 않는다.
	auto RestoreOn = [RestoredPlayRate](USkeletalMeshComponent* Mesh)
	{
		if (!Mesh) return;

		UAnimInstance* AnimInstance = Mesh->GetAnimInstance();
		if (!AnimInstance) return;

		if (UAnimMontage* ActiveMontage = AnimInstance->GetCurrentActiveMontage())
		{
			AnimInstance->Montage_SetPlayRate(ActiveMontage, RestoredPlayRate);
		}
	};

	RestoreOn(Avatar->GetMesh());                       // 3P

	if (ADRCharacter* DRChar = Cast<ADRCharacter>(Avatar))
	{
		RestoreOn(DRChar->FirstPersonMesh);             // 1P
	}
}

// ========================= Plan5(발톱칩) — 아군 수혈 =========================

void UDRMeleeAttack::TryHealAllyWithOwnHealth(AActor* AllyActor)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(AllyActor) || !Avatar) return;

	// 회복·체력 소모는 전부 서버 권한이다.
	if (!Avatar->HasAuthority()) return;

	// 자기 자신은 대상이 아니다 (BP 가 ActorsToIgnore 로 이미 빼지만 이중 방어)
	if (AllyActor == Avatar) return;

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC) return;

	// 칩이 없으면 여기서 끝난다 — BP 는 조건 없이 항상 부르고, 판단은 C++ 이 한다.
	if (!UDRAbilitySystemLibrary::HasUpgradeFeature(
			SourceASC, FDRGameplayTags::Get().Upgrade_Feature_Skill_Claw_AllyHeal))
	{
		return;
	}

	// ★아군은 "둘 다 Player" 로 ★양성 판정★ 한다.★
	//
	// "유효 피해 대상이 아니다"로 뒤집어 쓰면 ★클렌저 사이트가 아군으로 잡힌다★ —
	// 사이트는 Player/Enemy 액터 태그가 둘 다 없어서 IsNotFriend 가 true 를 돌려주고,
	// ICombatInterface 도 구현하지 않아 IsValidDamageTarget 이 true 가 된다.
	if (UDRAbilitySystemLibrary::IsNotFriend(Avatar, AllyActor)) return;

	const UDRAttributeSet* SelfAS = Cast<UDRAttributeSet>(
		SourceASC->GetAttributeSet(UDRAttributeSet::StaticClass()));
	if (!SelfAS) return;

	const FDRClawChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(Avatar).Claw;

	// ── ① 회복 희망량 = 이번 공격의 피해량 (칩 ②의 −50% 가 이미 반영돼 있다) ──
	float HealAmount = GetUpgradedDamage();
	if (HealAmount <= 0.f) return;

	// ── ② 아군이 ★실제로 받아들일 수 있는 양★ 으로 먼저 자른다 ──
	//
	// ★만피 아군을 때리면 여기서 0 이 되어 체력이 전혀 나가지 않는다.★
	// 회복 차단(칩 ⑥) 아군도 0 이다 — 버려질 힐에 대가를 치를 이유가 없다.
	// 오염된 아군은 요청량 전액이 "정화"에 쓰이므로 만피여도 0 이 아니다.
	HealAmount = UDRAbilitySystemLibrary::GetHealableAmount(AllyActor, HealAmount);
	if (HealAmount <= 0.f) return;

	// ── ③ 내 체력으로 감당 가능한 양으로 다시 자른다 (자살 방지) ──
	//
	// ★체력을 직접 깎는 경로는 사망 판정(Die())을 타지 않는다.★
	// 막지 않으면 "체력 0 인 채 살아 있는" 상태가 된다.
	const float CostRatio = FMath::Max(KINDA_SMALL_NUMBER, Tuning.AllyHealHealthCostRatio);
	const float Spendable = FMath::Max(0.f, SelfAS->GetHealth() - Tuning.AllyHealMinSelfHealth);

	HealAmount = FMath::Min(HealAmount, Spendable / CostRatio);
	if (HealAmount <= 0.f) return;

	const float HealthCost = HealAmount * CostRatio;

	// ── ④ 비용 먼저, 회복 나중 ──
	//
	// 순서를 고정해 두면 "체력이 깎였으면 회복도 갔다" 가 항상 성립한다.
	if (UClass* CostClass = Tuning.SelfCostEffect.LoadSynchronous())
	{
		FGameplayEffectSpecHandle CostSpec = MakeOutgoingGameplayEffectSpec(CostClass, GetAbilityLevel());
		if (CostSpec.IsValid())
		{
			// ★양수(비용 액수)로 넣는다.★ 부호를 빼는 것은 GE 모디파이어의 몫이다.
			CostSpec.Data->SetSetByCallerMagnitude(FDRGameplayTags::Get().Data_HealthCost, HealthCost);
			SourceASC->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
		}
	}

	UDRAbilitySystemLibrary::ApplyHealToActor(AllyActor, SourceASC, Tuning.HealEffect, HealAmount);
}
