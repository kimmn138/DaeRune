// Copyright DaeRune

#include "AbilitySystem/Abilities/Stage2/DRS2MoleClawAttack.h"

#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Actor/Stage2/DRS2MoleSlashWave.h"
#include "Character/DRCharacter.h"
#include "Character/DREnemy.h"
#include "Character/Stage2/DRS2MoleBoss.h"
#include "Components/CapsuleComponent.h"
#include "DaeRune/DRLogChannels.h"
#include "Engine/World.h"
#include "Game/DRGameStateBase.h"
#include "Interaction/EnemyInterface.h"

UDRS2MoleClawAttack::UDRS2MoleClawAttack()
{
	// AI 가 서버에서 활성화하는 어빌리티다. 타이머/상태를 위해 액터별 인스턴스가 필요하다.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// ★커브 테이블을 쓰는 값은 Value 를 1 로 둔다 (결과 = Value * Curve[Level]).
	Hit1Damage = FScalableFloat(1.f);
	Hit2Damage = FScalableFloat(1.f);

	// ★1타 `/`, 2타 `\` — 두 발이 X 자로 교차해 서로의 사각을 메운다.
	//   같은 방향으로만 피하면 2타에 맞고, 1타와 2타 사이에 방향을 바꿔야 둘 다 피한다.
	SlashRollAngles = { 45.f, -45.f };
}

void UDRS2MoleClawAttack::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	// 커브를 걸어 놓고 Value 를 0으로 둔 상태를 부여 시점에 1회 잡아낸다
	auto Validate = [](const FScalableFloat& Field, const TCHAR* Label)
	{
		if (!Field.Curve.RowName.IsNone() && FMath::IsNearlyZero(Field.Value))
		{
			UE_LOG(LogDR, Error,
				TEXT("[MoleBoss] Claw %s: 커브 행(%s)이 지정됐지만 Value 가 0입니다 — 데미지가 항상 0이 됩니다. Value 를 1 로 설정하세요."),
				Label, *Field.Curve.RowName.ToString());
		}
		else if (Field.Curve.RowName.IsNone())
		{
			UE_LOG(LogDR, Warning,
				TEXT("[MoleBoss] Claw %s 에 커브가 없어 상수 %.1f 로 동작합니다 (구간 스케일 없음)."),
				Label, Field.Value);
		}
	};

	Validate(Hit1Damage, TEXT("Hit1Damage"));
	Validate(Hit2Damage, TEXT("Hit2Damage"));

	// ★검기 클래스 미지정은 "아무 공격도 안 나가는" 무증상 버그가 되므로 부여 시점에 잡는다.
	if (!bUseMeleeSweep && !SlashWaveClass)
	{
		UE_LOG(LogDR, Error,
			TEXT("[MoleBoss] SlashWaveClass 가 비어 있습니다 — 기본공격이 아무것도 발사하지 않습니다. GA_Mole_Claw 에 BP_Mole_SlashWave 를 지정하세요."));
	}

	if (SlashRollAngles.Num() == 0)
	{
		UE_LOG(LogDR, Warning,
			TEXT("[MoleBoss] SlashRollAngles 가 비어 있어 검기가 수평(0도)으로 나갑니다."));
	}
}

float UDRS2MoleClawAttack::GetEffectiveSweepRadius() const
{
	// ★사거리는 캡슐 **표면** 기준이다. 이 보스는 캡슐 반지름이 커서
	//   중심 기준 절대값으로 잡으면 부채꼴이 통째로 몸통 안에 갇힌다.
	const ACharacter* Avatar = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UCapsuleComponent* Capsule = Avatar ? Avatar->GetCapsuleComponent() : nullptr;

	return (Capsule ? Capsule->GetScaledCapsuleRadius() : 0.f) + ClawReach;
}

void UDRS2MoleClawAttack::PerformClawSweep(int32 HitIndex)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar) || !Avatar->HasAuthority()) return;      // ★서버 전용

	if (bUseMeleeSweep)
	{
		PerformMeleeSweep_Legacy(HitIndex);
	}
	else
	{
		FireSlashWaves(HitIndex);
	}

	// 물 보상 감소는 "공격 1회"당 1번만 센다 (2타짜리라 1타에서만 호출)
	if (HitIndex == 0)
	{
		if (ADREnemy* Enemy = Cast<ADREnemy>(Avatar))
		{
			Enemy->OnAttackExecuted();
		}
	}
}

void UDRS2MoleClawAttack::FireSlashWaves(int32 HitIndex)
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (!SlashWaveClass)
	{
		UE_LOG(LogDR, Error, TEXT("[MoleBoss] SlashWaveClass 미지정 — 검기가 발사되지 않습니다."));
		return;
	}

	const FVector Muzzle = ResolveMuzzleLocation();
	const float   BaseYaw = ResolveFireYaw(Muzzle);
	const float   Roll = ResolveSlashRoll(HitIndex);        // ★1타 +45 / 2타 -45
	const float   HitDamage = ResolveDamage(HitIndex);      // ★구간 축 (기존 로직 그대로)

	const int32 Count = FMath::Max(1, WavesPerHit);
	for (int32 i = 0; i < Count; ++i)
	{
		float Yaw = BaseYaw;
		if (Count > 1)
		{
			Yaw += FanSpreadAngle * (static_cast<float>(i) / static_cast<float>(Count - 1) - 0.5f);
		}

		// ★FRotator(Pitch, Yaw, Roll) — 인자 순서를 틀리면 "대각선이 안 나온다"로 보이지만
		//   실제로는 엉뚱한 방향으로 날아간다. Pitch 0 고정이 대각선 사양의 전제다.
		const FRotator Aim(0.f, Yaw, Roll);
		const FTransform SpawnTransform(Aim.Quaternion(), Muzzle);

		ADRS2MoleSlashWave* Wave = World->SpawnActorDeferred<ADRS2MoleSlashWave>(
			SlashWaveClass,
			SpawnTransform,
			GetOwningActorFromActorInfo(),
			Cast<APawn>(GetOwningActorFromActorInfo()),
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (!Wave) continue;

		// ★투사체는 "맞는 순간"에 TargetASC 를 채운다 (UDRProjectileSpell 과 같은 관례).
		Wave->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();
		Wave->DamageEffectParams.BaseDamage = HitDamage;     // 구간/타수별 값으로 덮어쓴다
		Wave->InitWave(WaveSpeed, WaveMaxRange);
		Wave->FinishSpawning(SpawnTransform);
	}
}

FVector UDRS2MoleClawAttack::ResolveMuzzleLocation() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar)) return FVector::ZeroVector;

	FVector Forward = Avatar->GetActorForwardVector();
	Forward.Z = 0.f;
	if (!Forward.Normalize()) Forward = FVector::ForwardVector;

	const ACharacter* AvatarCharacter = Cast<ACharacter>(Avatar);
	const UCapsuleComponent* Capsule = AvatarCharacter ? AvatarCharacter->GetCapsuleComponent() : nullptr;
	const float CapsuleRadius = Capsule ? Capsule->GetScaledCapsuleRadius() : 0.f;

	// ★몸통 "안"에서 출발한다 — 밀착한 플레이어도 통과 경로에 들어오게 하려는 의도다.
	//   보스는 반매몰이라 액터 Z 가 곧 지면 Z 이므로, 고도는 여기에 오프셋만 더하면 된다.
	FVector Muzzle = Avatar->GetActorLocation() + Forward * (CapsuleRadius * MuzzleForwardRatio);
	Muzzle.Z = Avatar->GetActorLocation().Z + MuzzleHeight;

	return Muzzle;
}

float UDRS2MoleClawAttack::ResolveFireYaw(const FVector& MuzzleLocation) const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar)) return 0.f;

	const float ForwardYaw = Avatar->GetActorRotation().Yaw;

	const AActor* Target = ResolveAimTarget(MuzzleLocation);
	if (!IsValid(Target)) return ForwardYaw;

	FVector ToTarget = Target->GetActorLocation() - MuzzleLocation;
	ToTarget.Z = 0.f;
	if (!ToTarget.Normalize()) return ForwardYaw;

	// ★전방에서 ±MaxAimYawFromForward 만큼만 보정한다.
	//   0 이면 순수 전방(느린 회전 탓에 스트레이프에 영구히 빗나감),
	//   180 이면 완전 조준(회피 불가)이 된다. 그 사이에서 "옆으로 파고들면 못 맞춘다"가 성립한다.
	const float DesiredYaw = ToTarget.Rotation().Yaw;
	const float DeltaYaw = FMath::FindDeltaAngleDegrees(ForwardYaw, DesiredYaw);
	const float ClampedDelta = FMath::Clamp(DeltaYaw, -MaxAimYawFromForward, MaxAimYawFromForward);

	return ForwardYaw + ClampedDelta;
}

AActor* UDRS2MoleClawAttack::ResolveAimTarget(const FVector& MuzzleLocation) const
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar)) return nullptr;

	// ① AI 서비스가 잡아 둔 전투 타깃 우선
	if (Avatar->Implements<UEnemyInterface>())
	{
		AActor* CombatTarget = IEnemyInterface::Execute_GetCombatTarget(Avatar);
		if (const ADRCharacter* TargetCharacter = Cast<ADRCharacter>(CombatTarget))
		{
			const bool bSkipSeated = bExcludeSeatedPlayersForAim && TargetCharacter->IsSeatedOnTrain();
			if (!bSkipSeated)
			{
				return CombatTarget;
			}
		}
	}

	// ② 폴백 — 최근접 생존 플레이어 (스킬1 PickRandomTarget 과 같은 소스)
	const UWorld* World = Avatar->GetWorld();
	const ADRGameStateBase* DRGameState = World ? World->GetGameState<ADRGameStateBase>() : nullptr;
	if (!DRGameState) return nullptr;

	ADRCharacter* Nearest = nullptr;
	float NearestDistSq = TNumericLimits<float>::Max();

	for (ADRCharacter* Player : DRGameState->GetAlivePlayers())   // IsDead 필터 포함
	{
		if (!IsValid(Player)) continue;

		// 좌석 착석자는 "겨누지" 않는다. 다만 경로상에 있으면 검기에 맞기는 한다.
		if (bExcludeSeatedPlayersForAim && Player->IsSeatedOnTrain()) continue;

		const float DistSq = FVector::DistSquared2D(Player->GetActorLocation(), MuzzleLocation);
		if (DistSq < NearestDistSq)
		{
			NearestDistSq = DistSq;
			Nearest = Player;
		}
	}

	return Nearest;
}

float UDRS2MoleClawAttack::ResolveSlashRoll(int32 HitIndex) const
{
	if (SlashRollAngles.Num() == 0) return 0.f;

	// 배열이 짧으면 마지막 값으로 폴백 — 3타 이상으로 늘려도 코드 수정이 필요 없다.
	return SlashRollAngles[FMath::Clamp(HitIndex, 0, SlashRollAngles.Num() - 1)];
}

void UDRS2MoleClawAttack::PerformMeleeSweep_Legacy(int32 HitIndex)
{
	// ★원거리 전환 전의 부채꼴 판정. bUseMeleeSweep 으로만 도달한다 (A/B 비교 · 롤백용).
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar)) return;

	const FVector Origin = Avatar->GetActorLocation();

	FVector Forward = Avatar->GetActorForwardVector();
	Forward.Z = 0.f;
	if (!Forward.Normalize()) return;

	const float CosHalfAngle = FMath::Cos(FMath::DegreesToRadians(SweepAngle * 0.5f));

	// (부모 UDRDamageGameplayAbility::Damage 와 이름이 겹치지 않도록 구분한다)
	const float HitDamage = ResolveDamage(HitIndex);

	// 오버랩 구는 넉넉히, 판정은 XY 로 (헤더 VerticalTolerance 주석 참고)
	TArray<AActor*> Candidates;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Avatar);

	// 부채꼴의 꼭짓점은 캡슐 중심 그대로다. 반경만 몸통을 뚫고 바깥까지 뻗는다.
	const float EffectiveRadius = GetEffectiveSweepRadius();

	UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(
		Avatar, Candidates, ActorsToIgnore, EffectiveRadius + VerticalTolerance, Origin);

	const float RadiusSq = FMath::Square(EffectiveRadius);

	for (AActor* Target : Candidates)
	{
		if (!IsValid(Target)) continue;

		// 기본공격은 플레이어만 때린다 (융기와 달리 아군 오사가 없다)
		if (!UDRAbilitySystemLibrary::IsNotFriend(Avatar, Target)) continue;

		FVector ToTarget = Target->GetActorLocation() - Origin;
		ToTarget.Z = 0.f;                                       // ① XY 평면으로 눕히고
		if (ToTarget.SizeSquared() > RadiusSq) continue;        // ② XY 거리로 사거리 판정
		if (!ToTarget.Normalize()) continue;
		if (FVector::DotProduct(Forward, ToTarget) < CosHalfAngle) continue;   // ③ 부채꼴 각도 판정

		FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(Target);
		Params.BaseDamage = HitDamage;                          // ★구간/타수별 값으로 덮어쓴다
		UDRAbilitySystemLibrary::ApplyDamageEffect(Params);
	}
}

float UDRS2MoleClawAttack::ResolveDamage(int32 HitIndex) const
{
	int32 Segment = 0;
	if (const ADRS2MoleBoss* Boss = Cast<ADRS2MoleBoss>(GetAvatarActorFromActorInfo()))
	{
		Segment = Boss->GetSegmentIndex();
	}

	// ★구간 축: 어빌리티 레벨(= 인원수)이 아니라 구간을 커브 레벨로 직접 넘긴다.
	//   FScalableFloat 은 GetValueAtLevel 에 어떤 축이든 넣을 수 있다.
	const float CurveLevel = static_cast<float>(Segment + 1);   // 구간 0/1/2 → 커브 레벨 1/2/3

	static const FString Hit1Context(TEXT("MoleBoss.Claw.Hit1"));
	static const FString Hit2Context(TEXT("MoleBoss.Claw.Hit2"));

	return (HitIndex <= 0)
		? Hit1Damage.GetValueAtLevel(CurveLevel, &Hit1Context)
		: Hit2Damage.GetValueAtLevel(CurveLevel, &Hit2Context);
}
