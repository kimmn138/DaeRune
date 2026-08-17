// Copyright DaeRune


#include "AbilitySystem/Abilities/DRVacuumAirShot.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "Actor/DRVacuumAirProjectile.h"
#include "DRGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Interaction/CombatInterface.h"
#include "TimerManager.h"

UDRVacuumAirShot::UDRVacuumAirShot()
{
	// 기본 4단계 ([0] 즉발 ~ [3] 강화) — 수치는 BP에서 조정
	Stages.SetNum(4);
	Stages[0].Damage = 10.f;  Stages[0].ProjectileSpeed = 1500.f;
	Stages[1].Damage = 15.f;  Stages[1].ProjectileSpeed = 1800.f;
	Stages[2].Damage = 22.f;  Stages[2].ProjectileSpeed = 2200.f;
	Stages[3].Damage = 35.f;  Stages[3].ProjectileSpeed = 2600.f;
}

void UDRVacuumAirShot::StartCharging()
{
	UWorld* World = GetWorld();
	if (!World) return;

	ChargeStartTime = World->GetTimeSeconds();
	bCharging = true;
	LastNotifiedStage = -1;
	NotifyGauge(0);

	// 로컬 연출용 단계 상승 통지 타이머 (서버/클라 양쪽에서 돌지만 UI/SFX는 BP에서 소유 클라만 처리)
	World->GetTimerManager().SetTimer(ChargeUITimerHandle, this, &UDRVacuumAirShot::TickChargeUI, ChargeInterval, true);
}

void UDRVacuumAirShot::TickChargeUI()
{
	const int32 Stage = GetChargeStage();
	NotifyGauge(Stage);

	// 최대 단계 도달 시 더 이상 통지할 것이 없음
	if (Stages.Num() > 0 && Stage >= Stages.Num() - 1)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ChargeUITimerHandle);
		}
	}
}

void UDRVacuumAirShot::NotifyGauge(int32 Stage)
{
	if (Stage == LastNotifiedStage) return;
	LastNotifiedStage = Stage;

	OnChargeStageChanged(Stage);

	// 스킬 아이콘 게이지(3칸) 갱신 — 충전은 시간 기반이라 소유 클라 인스턴스에서만 방송한다.
	// (서버 인스턴스까지 통지하면 RTT만큼 늦은 같은 값이 뒤따라와 UI가 되돌아 보임)
	// EndAbility 경유 리셋에서도 안전하도록 ActorInfo 를 직접 검사
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!Info || !Info->OwnerActor.IsValid() || !Info->IsLocallyControlled()) return;

	if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(Info->AbilitySystemComponent.Get()))
	{
		DRASC->NotifyVacuumAirShotGaugeChanged(Stage, GetMaxGauge());
	}
}

int32 UDRVacuumAirShot::GetChargeStage() const
{
	if (!bCharging || Stages.Num() == 0) return 0;
	const UWorld* World = GetWorld();
	if (!World) return 0;

	const double HoldTime = World->GetTimeSeconds() - ChargeStartTime;
	return FMath::Clamp(FMath::FloorToInt32(HoldTime / ChargeInterval), 0, Stages.Num() - 1);
}

void UDRVacuumAirShot::ReleaseAndFire()
{
	const int32 Stage = GetChargeStage();

	bCharging = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeUITimerHandle);
	}

	FireShot(Stage);   // 내부에서 HasAuthority 체크

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UDRVacuumAirShot::FireShot(int32 Stage)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return;
	if (Stages.Num() == 0) return;

	Stage = FMath::Clamp(Stage, 0, Stages.Num() - 1);
	const FVacuumShotStage& StageData = Stages[Stage];
	const bool bMaxStage = (Stage == Stages.Num() - 1);

	const FVector TargetLocation = CalculateTargetLocation();
	ADRProjectile* Projectile = SpawnProjectile(TargetLocation, FireSocketTag);
	if (!Projectile)
	{
		UE_LOG(LogTemp, Warning, TEXT("[VacuumAirShot] 프로젝타일 스폰 실패 — ProjectileClass/Muzzle 소켓 설정 확인"));
		return;
	}

	// 단계별 데미지/탄속 주입 (Plan3 §6.1)
	// 단계별 피해량은 CDO 배열에 있어 Damage(FScalableFloat) 경로를 타지 않는다.
	// 스킬 피해량 칩이 조용히 무효가 되지 않도록 여기서 직접 보정한다. (Plan2.md 8.5)
	Projectile->DamageEffectParams.BaseDamage =
		GetUpgradedFloat(EDRUpgradeStat::SkillDamage, StageData.Damage);
	if (UProjectileMovementComponent* PM = Projectile->ProjectileMovement)
	{
		PM->InitialSpeed = StageData.ProjectileSpeed;
		PM->MaxSpeed = StageData.ProjectileSpeed;
		PM->Velocity = Projectile->GetActorForwardVector() * StageData.ProjectileSpeed;
	}

	if (!bMaxStage) return;

	// ===== 최종 단계 (강화탄) =====

	// 강화 공격 몽타주 (fire-and-forget — 직후 EndAbility가 몽타주를 끊지 않도록 bStopWhenAbilityEnds=false, §13.5-③)
	if (EnhancedAttackMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, EnhancedAttackMontage, 1.f, NAME_None, /*bStopWhenAbilityEnds*/ false);
		if (MontageTask)
		{
			MontageTask->ReadyForActivation();
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[VacuumAirShot] EnhancedAttackMontage 미지정 — 강화탄 애니 생략"));
	}

	Projectile->DamageEffectParams.KnockbackChance = 100.f;
	Projectile->DamageEffectParams.KnockbackForceMagnitude = EnhancedKnockbackForce;
	if (ADRVacuumAirProjectile* AirProjectile = Cast<ADRVacuumAirProjectile>(Projectile))
	{
		AirProjectile->bEnhanced = true;
	}

	// 시전자 후방 반동 — 발밑 사격 시 로켓 점프
	const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Avatar, FireSocketTag);
	const FVector LaunchDir = (TargetLocation - SocketLocation).GetSafeNormal();
	FVector Recoil = -LaunchDir * RecoilImpulse;

	// 돌진 상태(지속 돌진 태그 또는 이속버프) → 수평 반동 제거, 상방 반동만 유지 (§10 결정사항)
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const bool bDashing =
		(ASC && ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().State_RobotVacuum_SustainedDash)) ||
		HasDashBuff();
	if (bDashing)
	{
		Recoil.X = 0.f;
		Recoil.Y = 0.f;
		Recoil.Z = FMath::Max(Recoil.Z, 0.f);
	}

	if (ACharacter* Character = Cast<ACharacter>(Avatar))
	{
		Character->LaunchCharacter(Recoil, false, false);
	}
}

bool UDRVacuumAirShot::HasDashBuff() const
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return false;

	FGameplayTagContainer BuffTagFilter;
	BuffTagFilter.AddTag(FDRGameplayTags::Get().Buff_RobotVacuum_DashSpeed);
	return ASC->GetActiveEffectsWithAllTags(BuffTagFilter).Num() > 0;
}

void UDRVacuumAirShot::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 취소 안전: 타이머 정리 (WaterPump 패턴)
	bCharging = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeUITimerHandle);
	}

	// 발사/취소 공통: 스킬 아이콘 게이지 비우기 (LastNotifiedStage 가드로 중복 방송 없음)
	NotifyGauge(0);

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
