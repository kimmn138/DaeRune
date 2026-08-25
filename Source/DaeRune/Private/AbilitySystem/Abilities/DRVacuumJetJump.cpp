// Copyright DaeRune


#include "AbilitySystem/Abilities/DRVacuumJetJump.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Character/DRRobotVacuumCharacter.h"
#include "DRGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"

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

	// 공중 사용 1회 제한 — bAirJumpUsed는 서버 전용 값이므로 실제 차단은 서버 판정에서 이뤄짐 (Plan3 §7.1)
	if (const ADRRobotVacuumCharacter* Vacuum = Cast<ADRRobotVacuumCharacter>(ActorInfo->AvatarActor.Get()))
	{
		if (Vacuum->GetCharacterMovement()->IsFalling() && Vacuum->bAirJumpUsed)
		{
			return false;
		}
	}

	return true;
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

	// 게임 로직은 서버 권한 전용, 연출은 GameplayCue로 전 클라 전파
	if (Avatar && Avatar->HasAuthority())
	{
		ADRRobotVacuumCharacter* Vacuum = Cast<ADRRobotVacuumCharacter>(Avatar);
		if (Vacuum)
		{
			// 공중 사용만 마킹 → "지상 Q → 공중 Q" 2회 / "스페이스 점프 → 공중 Q" 1회 규칙
			if (Vacuum->GetCharacterMovement()->IsFalling())
			{
				Vacuum->bAirJumpUsed = true;
			}
			// 애님 플래그 (Landed()에서 해제)
			Vacuum->SetJetJumping(true);

			Vacuum->LaunchCharacter(FVector(0.f, 0.f, JumpImpulseZ), false, true);
		}

		// ===== AoE 데미지: Z 무관 2D 반경 (Plan3 §7.1) =====
		const FVector Origin = Avatar->GetActorLocation();

		TArray<AActor*> Candidates;
		// 구체 수집은 3D라 여유 반경으로 모은 뒤 2D 거리로 재판정
		UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(this, Candidates, { Avatar }, OuterRadius * 1.5f, Origin);

		for (AActor* Target : Candidates)
		{
			if (!IsValid(Target)) continue;
			if (!UDRAbilitySystemLibrary::IsNotFriend(Avatar, Target)) continue;   // 적만

			const float Dist2D = FVector::DistXY(Origin, Target->GetActorLocation());
			if (Dist2D > OuterRadius) continue;

			FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(Target);
			Params.BaseDamage = (Dist2D <= InnerRadius) ? InnerDamage : OuterDamage;   // SeedProjectile 내/외 패턴
			UDRAbilitySystemLibrary::ApplyDamageEffect(Params);
		}

		// 물 분사 VFX/SFX — 서버 Execute → ASC가 전 클라이언트에 리플리케이트
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = Origin;
			ASC->ExecuteGameplayCue(FDRGameplayTags::Get().GameplayCue_Skill_VacuumJetJump, CueParams);
		}
	}

	// 즉발 스킬 — 처리 후 바로 종료
	EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
}
