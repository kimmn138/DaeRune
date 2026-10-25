// Copyright DaeRune


#include "AbilitySystem/Abilities/DRVacuumJetJump.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Character/DRRobotVacuumCharacter.h"
#include "DRGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Game/DRProgressionConfig.h"
#include "DaeRune/DRLogChannels.h"

UDRVacuumJetJump::UDRVacuumJetJump()
{
	WaterCost = 30.f;
	// StartupInputTag(InputTag.Q), DamageType(Damage.Physical), DamageEffectClass(GE_Damage)는 BP에서 설정
	// (네이티브 태그는 CDO 생성 시점에 아직 등록 전이라 생성자에서 지정 불가)
}

bool UDRVacuumJetJump::CanActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags, const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags))
	{
		return false;
	}

	// 공중 사용 횟수 제한 — AirJumpsUsed 는 서버 전용 값이므로 실제 차단은 서버 판정에서 이뤄짐 (Plan3 §7.1)
	// ★Plan5(물점프칩)★ 1회 고정 → GetMaxAirJumpsFor (칩③ +1) (D-107)
	if (const ADRRobotVacuumCharacter* Vacuum = Cast<ADRRobotVacuumCharacter>(ActorInfo->AvatarActor.Get()))
	{
		if (Vacuum->GetCharacterMovement()->IsFalling() && Vacuum->AirJumpsUsed >= GetMaxAirJumpsFor(ActorInfo))
		{
			return false;
		}
	}

	return true;
}

int32 UDRVacuumJetJump::GetMaxAirJumpsFor(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const float Upgraded = GetUpgradeRuntimeFor(ActorInfo)
		.ApplySkill(GetUpgradeKeyTag(), EDRUpgradeStat::SkillAirUseCount, static_cast<float>(MaxAirJumps));
	return FMath::Max(0, FMath::RoundToInt(Upgraded));
}

void UDRVacuumJetJump::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// 물 30 차감 (기존 ExecCalc_WaterCost 경로) — 실패 시 즉시 종료
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();

	// 점프음 — 소유 클라는 바로, 다른 머신엔 서버가 멀티캐스트 (클라 · 서버 인스턴스 모두 여기를 지난다)
	if (ADRRobotVacuumCharacter* Vacuum = Cast<ADRRobotVacuumCharacter>(Avatar))
	{
		Vacuum->PlayJetJumpSound();
	}

	// 게임 로직은 서버 권한 전용, 연출은 GameplayCue로 전 클라 전파
	if (Avatar && Avatar->HasAuthority())
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

		// ★Plan5(물점프칩)★ ② 0.3초 무적 — 쓰는 순간부터. 물값은 위 CommitAbility 에서 이미 냈다 (D-108).
		//   여기서는 표식(State.Invulnerable)만 건다. 막는 건 기본 피해 관문(UDRAttributeSet)이다
		if (UDRAbilitySystemLibrary::HasUpgradeFeature(ASC, FDRGameplayTags::Get().Upgrade_Feature_Skill_JetJump_Guard))
		{
			const FDRJetJumpChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(Avatar).JetJump;
			const FActiveGameplayEffectHandle Guard =
				UDRAbilitySystemLibrary::ApplyTimedTagEffect(Avatar, ASC, Tuning.GuardEffect, Tuning.GuardDuration);
			if (!Guard.IsValid())
			{
				UE_LOG(LogDR, Warning, TEXT("[JetJump] ChipTuning.JetJump.GuardEffect 미지정 — 칩②가 무적 없이 피해 0 만 남는다"));
			}
		}

		ADRRobotVacuumCharacter* Vacuum = Cast<ADRRobotVacuumCharacter>(Avatar);
		if (Vacuum)
		{
			// 공중 사용만 센다 → "지상 Q → 공중 Q × 한도" / "스페이스 점프 → 공중 Q × 한도"
			if (Vacuum->GetCharacterMovement()->IsFalling() && Vacuum->AirJumpsUsed < MAX_uint8)
			{
				++Vacuum->AirJumpsUsed;   // ★Plan5(물점프칩)★ bool → 횟수 (D-107)
			}
			// 애님 플래그 (Landed()에서 해제)
			Vacuum->SetJetJumping(true);

			// ★Plan5(물점프칩)★ ①③ 상승 높이 배율 — ★높이는 속도의 제곱에 비례★ 하므로 임펄스에는 √배율 (D-105 · T-109).
			//   0.7 → 임펄스 ×0.837 → 최고 높이 ×0.70. 칩이 없으면 1.0 이라 JumpImpulseZ 그대로
			const float HeightScale = FMath::Max(0.f, GetUpgradedFloat(EDRUpgradeStat::SkillLaunchHeight, 1.f));
			Vacuum->LaunchCharacter(FVector(0.f, 0.f, JumpImpulseZ * FMath::Sqrt(HeightScale)), false, true);
		}

		// ===== AoE 데미지: Z 무관 2D 반경 (Plan3 §7.1) =====
		// ★Plan5(물점프칩)★ 강판정 = Inner × SkillDamage / 약판정 = Outer × SkillDamage × SkillOuterDamage (D-104).
		//   ★GetUpgradedDamage() 를 쓰지 않는다★ — 그 기준인 GA Damage(스케일러블)는 이 스킬에서 0 이다 (T-118)
		const float FinalInner = GetUpgradedFloat(EDRUpgradeStat::SkillDamage, InnerDamage);
		const float FinalOuter = GetUpgradedFloat(EDRUpgradeStat::SkillOuterDamage,
			GetUpgradedFloat(EDRUpgradeStat::SkillDamage, OuterDamage));

		const FVector Origin = Avatar->GetActorLocation();

		// ★Plan5(물점프칩)★ ② 피해 0 이면 판정 자체를 건너뛴다 — 0 피해 GE 를 적에게 걸지 않는다 (D-109)
		if (FinalInner > 0.f || FinalOuter > 0.f)
		{
			TArray<AActor*> Candidates;
			// 구체 수집은 3D라 여유 반경으로 모은 뒤 2D 거리로 재판정
			UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(this, Candidates, { Avatar }, OuterRadius * 1.5f, Origin);

			for (AActor* Target : Candidates)
			{
				if (!IsValid(Target)) continue;
				if (!UDRAbilitySystemLibrary::IsNotFriend(Avatar, Target)) continue;   // 적만

				const float Dist2D = FVector::DistXY(Origin, Target->GetActorLocation());
				if (Dist2D > OuterRadius) continue;

				const float HitDamage = (Dist2D <= InnerRadius) ? FinalInner : FinalOuter;   // SeedProjectile 내/외 패턴
				if (HitDamage <= 0.f) continue;

				FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(Target);
				Params.BaseDamage = HitDamage;
				UDRAbilitySystemLibrary::ApplyDamageEffect(Params);
			}
		}

		// 물 분사 VFX/SFX — 서버 Execute → ASC가 전 클라이언트에 리플리케이트. ②(피해 0)여도 그대로
		if (ASC)
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = Origin;
			ASC->ExecuteGameplayCue(FDRGameplayTags::Get().GameplayCue_Skill_VacuumJetJump, CueParams);
		}
	}

	// 즉발 스킬 — 처리 후 바로 종료
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
