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

			ADRProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRProjectile>(
				SelectedClass, SpawnTransform,
				GetOwningActorFromActorInfo(),
				Cast<APawn>(GetOwningActorFromActorInfo()),
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

			Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();
			Projectile->DamageEffectParams.BaseDamage = SelectedDamage;

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
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return BaseFireInterval;

	int32 BuffStacks = 0;
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

	const float SpeedMultiplier = 1.0f + (BuffStacks * 0.2f);
	return BaseFireInterval / SpeedMultiplier;
}
