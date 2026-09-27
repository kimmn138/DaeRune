// Copyright DaeRune


#include "AbilitySystem/Abilities/DRVendingMachineBasicAttack.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "ActiveGameplayEffectHandle.h"
#include "GameplayEffect.h"
#include "Actor/DRProjectile.h"
#include "Character/DRCharacter.h"
#include "Interaction/CombatInterface.h"
#include "DRGameplayTags.h"

void UDRVendingMachineBasicAttack::StartAutoFire()
{
	if (bIsFiring) return;
	bIsFiring = true;

	const double CurrentTime = GetWorld()->GetTimeSeconds();
	const float Interval = GetCurrentFireInterval();
	const double TimeSinceLastShot = CurrentTime - LastShotTime;

	if (TimeSinceLastShot >= Interval)
	{
		// 인터벌 충분히 경과 → 즉시 발사
		ExecuteShot();
		GetWorld()->GetTimerManager().SetTimer(
			AutoFireTimerHandle, this,
			&UDRVendingMachineBasicAttack::FireShotAndScheduleNext,
			Interval, false);
	}
	else
	{
		// 연타로 인해 인터벌 미경과 → 남은 시간 후 발사
		const float RemainingTime = Interval - TimeSinceLastShot;
		GetWorld()->GetTimerManager().SetTimer(
			AutoFireTimerHandle, this,
			&UDRVendingMachineBasicAttack::FireShotAndScheduleNext,
			RemainingTime, false);
	}
}

void UDRVendingMachineBasicAttack::StopAutoFire()
{
	bIsFiring = false;
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(AutoFireTimerHandle);
	}
}

void UDRVendingMachineBasicAttack::FireShotAndScheduleNext()
{
	if (!bIsFiring) return;

	ExecuteShot();

	// 매 발사마다 새로 스케줄링 (공격속도 변경 즉시 반영)
	const float Interval = GetCurrentFireInterval();
	GetWorld()->GetTimerManager().SetTimer(
		AutoFireTimerHandle, this,
		&UDRVendingMachineBasicAttack::FireShotAndScheduleNext,
		Interval, false);
}

void UDRVendingMachineBasicAttack::ExecuteShot()
{
	if (!GetAvatarActorFromActorInfo() || !GetAvatarActorFromActorInfo()->HasAuthority()) return;

	LastShotTime = GetWorld()->GetTimeSeconds();

	const FVector TargetLocation = CalculateTargetLocation();

	if (CurrentJackpotStacks >= MaxJackpotStacks)
	{
		// ===== 잭팟 발동: 스택 소모 후 랜덤 캡슐 발사 =====
		CurrentJackpotStacks = 0;

		const float Roll = FMath::FRand();

		TSubclassOf<ADRProjectile> SelectedClass;
		float SelectedDamage;
		int32 CapsuleTier;

		if (Roll < BronzeCapsuleChance)
		{
			SelectedClass = BronzeCapsuleClass;
			SelectedDamage = BronzeCapsuleDamage;
			CapsuleTier = 0;
		}
		else if (Roll < BronzeCapsuleChance + SilverCapsuleChance)
		{
			SelectedClass = SilverCapsuleClass;
			SelectedDamage = SilverCapsuleDamage;
			CapsuleTier = 1;
		}
		else
		{
			SelectedClass = GoldCapsuleClass;
			SelectedDamage = GoldCapsuleDamage;
			CapsuleTier = 2;
		}

		if (SelectedClass)
		{
			const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
				GetAvatarActorFromActorInfo(), FireSocketTag);
			FRotator Rotation = (TargetLocation - SocketLocation).Rotation();

			FTransform SpawnTransform;
			SpawnTransform.SetLocation(SocketLocation);
			SpawnTransform.SetRotation(Rotation.Quaternion());
			// ★Plan5(동전칩)★ 캡슐도 기본 공격 투사체다 — 동전과 같은 크기 칩을 받는다(D-39).
			//   이 분기는 SpawnProjectile() 을 타지 않으므로 여기서 따로 넣는다.
			SpawnTransform.SetScale3D(FVector(GetUpgradedProjectileScale()));

			ADRProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRProjectile>(
				SelectedClass, SpawnTransform,
				GetOwningActorFromActorInfo(),
				Cast<APawn>(GetOwningActorFromActorInfo()),
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

			Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();
			// ★Plan5(동전칩)★ 캡슐 피해도 SkillDamage 칩을 탄다(D-39). 상수를 그대로 넣으면
			//   ①의 +20% 도, ②③의 −30% 도 캡슐에서만 빠진다(T-1 과 같은 누락 — UDRVacuumAirShot 과 같은 보정).
			//   GetUpgradedDamage() 와 같은 순서로 런타임 배율까지 곱한다(자판기는 지금 1.0).
			Projectile->DamageEffectParams.BaseDamage =
				GetUpgradedFloat(EDRUpgradeStat::SkillDamage, SelectedDamage) * GetRuntimeDamageMultiplier();

			Projectile->FinishSpawning(SpawnTransform);
		}

		OnCapsuleShotFired(CapsuleTier);
		OnJackpotStacksChanged(CurrentJackpotStacks, MaxJackpotStacks);

		if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
		{
			DRASC->NotifyVendingMachineStacksChanged(CurrentJackpotStacks, MaxJackpotStacks);
		}

		// 잭팟 캡슐 발사 사운드 멀티캐스트 (Plan2.md §3.3)
		if (ADRCharacter* DRChar = Cast<ADRCharacter>(GetAvatarActorFromActorInfo()))
		{
			const FVector ShotLocation = ICombatInterface::Execute_GetCombatSocketLocation(
				GetAvatarActorFromActorInfo(), FireSocketTag);
			DRChar->MulticastPlayVendingCapsuleShot(static_cast<uint8>(CapsuleTier), ShotLocation);
		}
	}
	else
	{
		// ===== 일반 공격: 부모의 SpawnProjectile 재사용 =====
		SpawnProjectile(TargetLocation, FireSocketTag);

		CurrentJackpotStacks++;

		OnNormalShotFired();
		OnJackpotStacksChanged(CurrentJackpotStacks, MaxJackpotStacks);

		if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
		{
			DRASC->NotifyVendingMachineStacksChanged(CurrentJackpotStacks, MaxJackpotStacks);
		}

		// 일반 코인 발사 사운드 멀티캐스트 (Plan2.md §3.3)
		if (ADRCharacter* DRChar = Cast<ADRCharacter>(GetAvatarActorFromActorInfo()))
		{
			const FVector ShotLocation = ICombatInterface::Execute_GetCombatSocketLocation(
				GetAvatarActorFromActorInfo(), FireSocketTag);
			DRChar->MulticastPlayVendingCoinShot(ShotLocation);
		}
	}
}

float UDRVendingMachineBasicAttack::GetCurrentFireInterval() const
{
	// ★Plan5(동전칩)★ 발사 간격 칩(SkillFireInterval, 초 단위)부터 반영한다.
	//   칩이 없으면 ApplySkill 이 항등이라 BaseFireInterval(BP 0.6초)이 그대로 나온다.
	//   VM.Asc.CoinHeavy: 0.6 + 0.4 = 1.0초
	const float ChipInterval = GetUpgradedFloat(EDRUpgradeStat::SkillFireInterval, BaseFireInterval);

	// 공격 속도 버프(스킬 2)는 기존 공식 그대로 ★그 위에서★ 나눈다 — 칩과 버프는 곱으로 합성된다(D-38).
	// (예전에는 ASC 가 없으면 BaseFireInterval 을 바로 돌려줬는데, 그러면 칩이 무시되므로 계산을 이어 간다)
	int32 BuffStacks = 0;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayTagContainer BuffTagFilter;
		BuffTagFilter.AddTag(FDRGameplayTags::Get().Buff_VendingMachine_AttackSpeed);

		TArray<FActiveGameplayEffectHandle> ActiveEffects =
			ASC->GetActiveEffectsWithAllTags(BuffTagFilter);

		for (const FActiveGameplayEffectHandle& EffectHandle : ActiveEffects)
		{
			const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(EffectHandle);
			if (ActiveGE)
			{
				BuffStacks = ActiveGE->Spec.GetStackCount();
				break;
			}
		}
	}

	const float SpeedMultiplier = 1.0f + (BuffStacks * 0.2f);

	// ★하한 필수★ — SetTimer 는 Rate <= 0 이면 타이머를 지워서 연사가 조용히 멈춘다(D-44).
	return FMath::Max(MinFireInterval, ChipInterval / SpeedMultiplier);
}
