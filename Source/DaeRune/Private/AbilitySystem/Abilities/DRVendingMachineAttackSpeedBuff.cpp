// Copyright DaeRune


#include "AbilitySystem/Abilities/DRVendingMachineAttackSpeedBuff.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffect.h"
#include "Character/DRCharacter.h"
#include "DRGameplayTags.h"
// Plan5(공속칩) — 칩 기능 스위치 / 튜닝 상수 / 아군 탐색
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Game/DRProgressionConfig.h"
#include "DaeRune/DRLogChannels.h"

void UDRVendingMachineAttackSpeedBuff::ApplyAttackSpeedBuff()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Avatar || !ASC || !Avatar->HasAuthority()) return;

	const FDRGameplayTags& Tags = FDRGameplayTags::Get();
	const FDRUpgradeRuntime& Runtime = GetUpgradeRuntime();
	const FDRAttackSpeedBuffChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(Avatar).AttackSpeedBuff;

	// ========== ② 공유 회로 — 본인 + 반경 안 아군에게 공유 버프 (D-63 · D-66) ==========
	if (Runtime.HasFeature(Tags.Upgrade_Feature_Skill_AttackSpeedBuff_Share))
	{
		if (UClass* SharedClass = Tuning.SharedEffect.LoadSynchronous())
		{
			// ★시전 순간 1회 판정★ — 오라가 아니다. 반경 밖에 있던 아군은 이번 시전의 버프를 받지 않는다.
			TArray<AActor*> Targets;
			UDRAbilitySystemLibrary::GetAlivePlayersWithinRadius(Avatar, Avatar->GetActorLocation(), Tuning.SharedRadius, Targets);
			Targets.AddUnique(Avatar);

			for (AActor* Target : Targets)
			{
				ApplyBuffEffect(Target, SharedClass, Tuning.SharedDuration);
			}

			UE_LOG(LogDR, Log, TEXT("[AttackSpeedBuff] Share: %s → %d명(본인 포함), 반경 %.0f"),
				*GetNameSafe(Avatar), Targets.Num(), Tuning.SharedRadius);
			return;
		}

		UE_LOG(LogDR, Warning, TEXT("[AttackSpeedBuff] ChipTuning.AttackSpeedBuff.SharedEffect 미지정 — 칩 ②가 기본 버프로 동작"));
	}

	// ========== ③ 잔류 가속 — BuffDuration 유지 뒤 DecayInterval 마다 1스택씩 감소 (D-72 · D-73) ==========
	if (Runtime.HasFeature(Tags.Upgrade_Feature_Skill_AttackSpeedBuff_Decay))
	{
		if (UClass* DecayClass = Tuning.DecayEffect.LoadSynchronous())
		{
			// ⓐ 지속 = 감소 간격. GE 의 Stack Expiration Policy(Remove Single Stack and Refresh Duration)가
			//    만료될 때마다 1스택을 빼고 이 간격으로 다시 잰다.
			const float Interval = FMath::Max(0.1f, Tuning.DecayInterval);
			const FActiveGameplayEffectHandle Handle = ApplyBuffEffect(Avatar, DecayClass, Interval);

			// ⓑ ★매 적용 직후★ 첫 만료를 BuffDuration 뒤로 민다 (T-69).
			//    스택이 쌓일 때 Refresh 정책이 시작 시각을 '지금'으로 되돌리므로, 감소 도중에 다시 쓰면
			//    감소가 멈추고 다시 BuffDuration 동안 유지된다(사용자 확정).
			if (Handle.IsValid())
			{
				ASC->ModifyActiveEffectStartTime(Handle, FMath::Max(0.f, BuffDuration - Interval));
			}
			return;
		}

		UE_LOG(LogDR, Warning, TEXT("[AttackSpeedBuff] ChipTuning.AttackSpeedBuff.DecayEffect 미지정 — 칩 ③이 기본 버프로 동작"));
	}

	// ========== 기본 / ① 과부하 모터(지속만 짧아진다 — 스택당 값은 GetAttackSpeedMultiplier 가 정한다) ==========
	const float Duration = Runtime.HasFeature(Tags.Upgrade_Feature_Skill_AttackSpeedBuff_Overdrive)
		? Tuning.OverdriveDuration
		: BuffDuration;
	ApplyBuffEffect(Avatar, AttackSpeedBuffEffect, Duration);
}

FActiveGameplayEffectHandle UDRVendingMachineAttackSpeedBuff::ApplyBuffEffect(
	AActor* Target, TSubclassOf<UGameplayEffect> EffectClass, float Duration) const
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	if (!TargetASC || !EffectClass) return FActiveGameplayEffectHandle();

	// 어빌리티 컨텍스트로 만든다 (BP 의 MakeOutgoingSpec 노드와 같은 경로)
	FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(EffectClass, GetAbilityLevel());
	if (!SpecHandle.IsValid()) return FActiveGameplayEffectHandle();

	// ★지속시간을 스펙에 고정한다★ — GE 에셋의 Duration 이 고정값(20초)이든 SetByCaller 든 이 값이 이긴다.
	//   SetByCaller 도 같이 채워 두면 에셋을 SetByCaller 로 바꿔도 경고 없이 같은 값이 나온다.
	const float FinalDuration = FMath::Max(0.01f, Duration);
	SpecHandle.Data->SetSetByCallerMagnitude(FDRGameplayTags::Get().Data_Duration, FinalDuration);
	SpecHandle.Data->SetDuration(FinalDuration, true);

	return TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

float UDRVendingMachineAttackSpeedBuff::FindBuffDuration(const UAbilitySystemComponent* ASC)
{
	if (ASC)
	{
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			// Spec.Ability 는 BP 클래스의 CDO 다 — BuffDuration 은 EditDefaultsOnly 라 CDO 값이 곧 실제 값
			if (const UDRVendingMachineAttackSpeedBuff* BuffAbility = Cast<UDRVendingMachineAttackSpeedBuff>(Spec.Ability))
			{
				return BuffAbility->GetBuffDuration();
			}
		}
	}
	return GetDefault<UDRVendingMachineAttackSpeedBuff>()->GetBuffDuration();
}

void UDRVendingMachineAttackSpeedBuff::NotifySkillActivated()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return;

	// ★Plan5(공속칩)★ 칩 ②를 끼면 자기 버프 대신 공유 버프를 받으므로 두 스택을 합친다 (D-70)
	const FDRGameplayTags& Tags = FDRGameplayTags::Get();
	const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const int32 Stacks =
		UDRAbilitySystemLibrary::GetEffectStackCountWithTag(ASC, Tags.Buff_VendingMachine_AttackSpeed) +
		UDRAbilitySystemLibrary::GetEffectStackCountWithTag(ASC, Tags.Buff_AttackSpeed_Shared);

	// ApplyAttackSpeedBuff 직후 호출되는 시점이므로 현재 조회되는 Stacks가 곧 사용 후의 스택 수.
	const int32 ClampedStacks = FMath::Clamp(Stacks, 1, 255);

	if (ADRCharacter* DRChar = Cast<ADRCharacter>(Avatar))
	{
		DRChar->MulticastPlayVendingSkillUse(static_cast<uint8>(ClampedStacks));
	}
}
