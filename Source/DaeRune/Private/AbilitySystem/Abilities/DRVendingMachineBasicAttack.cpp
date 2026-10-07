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
// Plan5(잭팟칩) — 칩 기능 스위치 조회 / 칩 튜닝 상수
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Game/DRProgressionConfig.h"
#include "DaeRune/DRLogChannels.h"

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

		// ★Plan5(잭팟칩)★ 확률 계산 → 굴림 → 발사 → ③ 보정 → ② 예약 (D-55 / D-56 / D-57)
		const FDRJackpotOdds Odds = GetJackpotOdds();
		const int32 CapsuleTier = RollJackpotTier(Odds);

		// 확률 검증용 한 줄 (출력 로그에서 "[Jackpot]" 으로 검색). 잭팟은 수 초에 한 번이라 로그가 넘치지 않는다.
		UE_LOG(LogDR, Log, TEXT("[Jackpot] 결과=%s / 확률 브론즈 %.0f%% 실버 %.0f%% 골드 %.0f%% / 적립(브론즈 %d, 실버 %d)"),
			CapsuleTier == 0 ? TEXT("브론즈") : (CapsuleTier == 1 ? TEXT("실버") : TEXT("골드")),
			Odds.Bronze * 100.f, Odds.Silver * 100.f, Odds.Gold * 100.f, PityBronzeSteps, PitySilverSteps);

		SpawnJackpotCapsule(CapsuleTier, TargetLocation);
		AdvanceJackpotPity(CapsuleTier);

		// ② 쌍둥이 캡슐 — 같은 등급을 다시 굴리지 않고 잠시 뒤 한 발 더.
		// 버튼을 떼도 나가야 하므로 StopAutoFire 에서 이 타이머를 지우지 않는다 (D-56)
		if (HasJackpotFeature(FDRGameplayTags::Get().Upgrade_Feature_Skill_Jackpot_Double))
		{
			PendingDoubleShotTier = CapsuleTier;
			const float Delay = UDRAbilitySystemLibrary::GetChipTuning(GetAvatarActorFromActorInfo()).Jackpot.DoubleShotDelay;
			GetWorld()->GetTimerManager().SetTimer(
				JackpotDoubleShotTimerHandle, this,
				&UDRVendingMachineBasicAttack::FireJackpotDoubleShot,
				FMath::Max(0.01f, Delay), false);
		}

		// BP 몽타주 이벤트는 첫 발만 — 두 번 틀면 첫 동작이 끊긴다 (D-56)
		OnCapsuleShotFired(CapsuleTier);
		OnJackpotStacksChanged(CurrentJackpotStacks, MaxJackpotStacks);

		if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
		{
			DRASC->NotifyVendingMachineStacksChanged(CurrentJackpotStacks, MaxJackpotStacks);
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

	// 공격 속도 버프(스킬 2)는 ★그 위에서★ 나눈다 — 칩과 버프는 곱으로 합성된다(D-38).
	// ★Plan5(공속칩)★ 배율은 라이브러리 한 곳에서 — 자기 버프(칩 ①·③이 스택당 값을 바꿈) + 공유 버프(칩 ②) (D-62).
	//   예전의 리터럴 0.2 는 ChipTuning.AttackSpeedBuff.BaseSpeedPerStack 으로 옮겼다(§9-17).
	const float SpeedMultiplier = FMath::Max(1.f, GetAttackSpeedMultiplier());

	// ★하한 필수★ — SetTimer 는 Rate <= 0 이면 타이머를 지워서 연사가 조용히 멈춘다(D-44).
	return FMath::Max(MinFireInterval, ChipInterval / SpeedMultiplier);
}

// ========================= Plan5(잭팟칩) =========================

bool UDRVendingMachineBasicAttack::HasJackpotFeature(const FGameplayTag& FeatureTag) const
{
	// 잭팟은 수 초에 한 번이라 캐시하지 않는다(발톱칩과 같은 판단).
	return UDRAbilitySystemLibrary::HasUpgradeFeature(GetAbilitySystemComponentFromActorInfo(), FeatureTag);
}

FDRJackpotOdds UDRVendingMachineBasicAttack::GetJackpotOdds() const
{
	// 칩이 없을 때의 기본표 = BP 값(골드는 나머지). 합이 어긋나 있어도 정규화로 맞춘다.
	FDRJackpotOdds Base(BronzeCapsuleChance, SilverCapsuleChance,
		FMath::Max(0.f, 1.f - BronzeCapsuleChance - SilverCapsuleChance));
	Base = Base.Normalized(FDRJackpotOdds());

	const FDRJackpotChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(GetAvatarActorFromActorInfo()).Jackpot;
	const FDRGameplayTags& Tags = FDRGameplayTags::Get();

	// §12 규칙(같은 그룹 칩은 1개)으로 잭팟 칩은 하나만 끼울 수 있다 — 아래 분기는 서로 배타적이다.

	// ③ 적립 카드 — 자기 기본표에서 색마다 누적 보정을 빼고, ★실제로 뺀 만큼만★ 골드에 더한다 (D-57)
	if (HasJackpotFeature(Tags.Upgrade_Feature_Skill_Jackpot_Pity))
	{
		FDRJackpotOdds Odds = Tuning.PityBaseOdds.Normalized(Base);
		const float Step = FMath::Max(0.f, Tuning.PityStep);
		const float TakeBronze = FMath::Min(Odds.Bronze, PityBronzeSteps * Step);
		const float TakeSilver = FMath::Min(Odds.Silver, PitySilverSteps * Step);
		Odds.Bronze -= TakeBronze;
		Odds.Silver -= TakeSilver;
		Odds.Gold += TakeBronze + TakeSilver;
		return Odds.Normalized(Base);
	}

	// ① 황금 레버 — 표를 통째로 바꾼다
	if (HasJackpotFeature(Tags.Upgrade_Feature_Skill_Jackpot_GoldRush))
	{
		return Tuning.GoldRushOdds.Normalized(Base);
	}

	// ② 쌍둥이 캡슐 — 기본 골드 확률을 브론즈·실버로 옮긴다. 고정표가 아니라 규칙이다 (D-55)
	if (HasJackpotFeature(Tags.Upgrade_Feature_Skill_Jackpot_Double))
	{
		FDRJackpotOdds Odds = Base;
		const float Share = FMath::Clamp(Tuning.DoubleGoldToBronzeShare, 0.f, 1.f);
		Odds.Bronze += Odds.Gold * Share;
		Odds.Silver += Odds.Gold * (1.f - Share);
		Odds.Gold = 0.f;
		return Odds.Normalized(Base);
	}

	return Base;
}

int32 UDRVendingMachineBasicAttack::RollJackpotTier(const FDRJackpotOdds& Odds) const
{
	const float Roll = FMath::FRand();
	if (Roll < Odds.Bronze) return 0;
	if (Roll < Odds.Bronze + Odds.Silver) return 1;

	// ★골드 0% 인데 부동소수 오차(합 0.99999994 등)로 여기까지 떨어지는 경우를 막는다★ (T-57)
	if (Odds.Gold <= 0.f)
	{
		return (Odds.Silver > 0.f) ? 1 : 0;
	}
	return 2;
}

float UDRVendingMachineBasicAttack::GetJackpotCapsuleBaseDamage(int32 Tier) const
{
	switch (Tier)
	{
	case 0:
		return BronzeCapsuleDamage;
	case 1:
		return SilverCapsuleDamage;
	default:
		// ③ 적립 카드의 단점 — 골드만 PityGoldDamage(기본 120). BP 의 GoldCapsuleDamage 는 건드리지 않는다 (D-59 / T-58)
		if (HasJackpotFeature(FDRGameplayTags::Get().Upgrade_Feature_Skill_Jackpot_Pity))
		{
			return UDRAbilitySystemLibrary::GetChipTuning(GetAvatarActorFromActorInfo()).Jackpot.PityGoldDamage;
		}
		return GoldCapsuleDamage;
	}
}

void UDRVendingMachineBasicAttack::SpawnJackpotCapsule(int32 Tier, const FVector& TargetLocation)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return;

	TSubclassOf<ADRProjectile> CapsuleClass;
	switch (Tier)
	{
	case 0:  CapsuleClass = BronzeCapsuleClass; break;
	case 1:  CapsuleClass = SilverCapsuleClass; break;
	default: CapsuleClass = GoldCapsuleClass;   break;
	}

	const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Avatar, FireSocketTag);

	if (CapsuleClass)
	{
		const FRotator Rotation = (TargetLocation - SocketLocation).Rotation();

		FTransform SpawnTransform;
		SpawnTransform.SetLocation(SocketLocation);
		SpawnTransform.SetRotation(Rotation.Quaternion());
		// ★Plan5(동전칩)★ 캡슐도 기본 공격 투사체다 — 동전과 같은 크기 칩을 받는다(D-39).
		//   이 경로는 SpawnProjectile() 을 타지 않으므로 여기서 따로 넣는다.
		SpawnTransform.SetScale3D(FVector(GetUpgradedProjectileScale()));

		ADRProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRProjectile>(
			CapsuleClass, SpawnTransform,
			GetOwningActorFromActorInfo(),
			Cast<APawn>(GetOwningActorFromActorInfo()),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		// 예전에는 스폰 실패를 확인하지 않아 크래시 여지가 있었다
		if (Projectile)
		{
			Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();
			// ★Plan5(동전칩)★ 캡슐 피해도 SkillDamage 칩을 탄다(D-39) — 등급 피해(③이면 골드 120)에서 출발한다(D-60).
			//   GetUpgradedDamage() 와 같은 순서로 런타임 배율까지 곱한다(자판기는 지금 1.0).
			Projectile->DamageEffectParams.BaseDamage =
				GetUpgradedFloat(EDRUpgradeStat::SkillDamage, GetJackpotCapsuleBaseDamage(Tier)) * GetRuntimeDamageMultiplier();

			Projectile->FinishSpawning(SpawnTransform);
		}
	}

	// 잭팟 캡슐 발사 사운드 멀티캐스트 (Plan2.md §3.3) — ② 의 두 번째 발도 소리를 낸다 (D-56)
	if (ADRCharacter* DRChar = Cast<ADRCharacter>(Avatar))
	{
		DRChar->MulticastPlayVendingCapsuleShot(static_cast<uint8>(Tier), SocketLocation);
	}
}

void UDRVendingMachineBasicAttack::AdvanceJackpotPity(int32 Tier)
{
	// 적립 카드를 안 꼈으면 상태를 건드리지 않는다
	if (!HasJackpotFeature(FDRGameplayTags::Get().Upgrade_Feature_Skill_Jackpot_Pity)) return;

	if (Tier == 0)
	{
		++PityBronzeSteps;
	}
	else if (Tier == 1)
	{
		++PitySilverSteps;
	}
	else
	{
		// 골드 → 처음 확률로 초기화
		PityBronzeSteps = 0;
		PitySilverSteps = 0;
	}
}

void UDRVendingMachineBasicAttack::FireJackpotDoubleShot()
{
	const int32 Tier = PendingDoubleShotTier;
	PendingDoubleShotTier = INDEX_NONE;

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority() || Tier == INDEX_NONE) return;

	// 그사이 죽었으면 쏘지 않는다 (T-54)
	if (Avatar->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Avatar)) return;

	// 같은 등급 · 다시 굴리지 않음 · 조준은 지금 다시 (D-56). 잭팟 스택·③ 보정은 건드리지 않는다 (T-55)
	SpawnJackpotCapsule(Tier, CalculateTargetLocation());
}
