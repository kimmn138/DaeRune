// Copyright DaeRune

#include "AbilitySystem/Abilities/Stage2/DRS2MoleBurrowStrike.h"

#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Actor/Stage2/DRS2GroundWarning.h"
#include "Character/DRCharacter.h"
#include "Character/DREnemy.h"
#include "Character/Stage2/DRS2MoleBoss.h"
#include "DaeRune/DRLogChannels.h"
#include "Game/DRGameStateBase.h"
#include "NavigationSystem.h"
#include "TimerManager.h"

UDRS2MoleBurrowStrike::UDRS2MoleBurrowStrike()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// ★커브 테이블을 쓰는 값은 Value 를 1 로 둔다 (결과 = Value * Curve[Level]).
	BurrowCooldown = FScalableFloat(1.f);

	// 부모 ApplyCooldown 이 SetByCaller 경로를 타도록 0 초과로 둔다.
	// 실제 값은 GetBaseCooldownDuration 이 BurrowCooldown 커브로 덮어쓴다.
	CooldownDuration = 8.f;
}

void UDRS2MoleBurrowStrike::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
	Super::OnGiveAbility(ActorInfo, Spec);

	// 커브를 걸어 놓고 Value 를 0으로 둔 상태를 부여 시점에 1회 잡아낸다
	auto Validate = [](const FScalableFloat& Field, const TCHAR* Label, const TCHAR* Axis)
	{
		if (!Field.Curve.RowName.IsNone() && FMath::IsNearlyZero(Field.Value))
		{
			UE_LOG(LogDR, Error,
				TEXT("[MoleBoss] BurrowStrike %s: 커브 행(%s)이 지정됐지만 Value 가 0입니다 — 결과가 항상 0이 됩니다. Value 를 1 로 설정하세요."),
				Label, *Field.Curve.RowName.ToString());
		}
		else if (Field.Curve.RowName.IsNone())
		{
			UE_LOG(LogDR, Warning,
				TEXT("[MoleBoss] BurrowStrike %s 에 커브가 없어 상수 %.1f 로 동작합니다 (%s 스케일 없음)."),
				Label, Field.Value, Axis);
		}
	};

	Validate(BurrowCooldown, TEXT("BurrowCooldown"), TEXT("구간"));
	// 융기 데미지는 상속받은 Damage 를 어빌리티 레벨(= 인원수)로 조회한다
	Validate(Damage, TEXT("Damage(융기)"), TEXT("인원수"));
}

ADRS2MoleBoss* UDRS2MoleBurrowStrike::GetMoleBoss() const
{
	return Cast<ADRS2MoleBoss>(GetAvatarActorFromActorInfo());
}

// ================= 쿨다운 (구간 축) =================

float UDRS2MoleBurrowStrike::GetBaseCooldownDuration(const FGameplayAbilityActorInfo* ActorInfo) const
{
	// ★ActorInfo 를 쓰는 이유: ApplyCooldown 은 CDO 에서 호출될 수 있어
	//   GetAvatarActorFromActorInfo()(= GetCurrentActorInfo 기반)가 비어 있을 수 있다.
	//   DRGameplayAbility.h 의 CooldownDuration / GetUpgradeRuntimeFor 주석이 명시한 함정이다.
	const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
	const ADRS2MoleBoss* Boss = Cast<ADRS2MoleBoss>(Avatar);
	const int32 Segment = Boss ? Boss->GetSegmentIndex() : 0;

	// ★구간 축: 어빌리티 레벨(= 인원수)이 아니라 구간을 커브 레벨로 직접 넘긴다
	static const FString Context(TEXT("MoleBoss.BurrowCooldown"));
	return BurrowCooldown.GetValueAtLevel(static_cast<float>(Segment + 1), &Context);
}

// ================= 활성 =================

void UDRS2MoleBurrowStrike::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	bBurrowPhaseStarted = false;
	CachedTarget = nullptr;
	WarningActor = nullptr;
	PendingEruptGround = FVector::ZeroVector;

	ADRS2MoleBoss* Boss = GetMoleBoss();
	if (!Boss || !Boss->HasAuthority())
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ★후보가 한 명도 없으면 커밋 전에 취소한다.
	//   전원이 열차에 앉아 있는 동안 쿨다운만 태우고 영구 잠수하는 사태를 막는다.
	if (!HasAnyValidTarget())
	{
		UE_LOG(LogDR, Verbose, TEXT("[MoleBoss] 굴착 강습 취소 — 유효한 타깃 후보 없음"));
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// 쿨다운은 여기서 적용된다 (구간별 값 — GetBaseCooldownDuration)
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ★잠수 몽타주는 보스가 재생하고 길이를 돌려준다.
	//   그 길이를 그대로 타이머로 쓰므로 "에셋 길이 vs 코드 상수" 불일치가 생길 수 없다.
	//   몽타주가 없을 때만 BP 프로퍼티 BurrowDuration 으로 폴백한다.
	const float MontageLength = Boss->PlayBurrowMontage();
	const float ActualBurrowDuration = (MontageLength > 0.f) ? MontageLength : BurrowDuration;

	// BP: 흙먼지/사운드 등 FX 만 (몽타주는 위에서 이미 재생됨)
	K2_OnBurrowStarted();

	if (ActualBurrowDuration > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			BurrowTimer, this, &UDRS2MoleBurrowStrike::BeginBurrowPhase, ActualBurrowDuration, false);
	}
	else
	{
		BeginBurrowPhase();
	}
}

// ================= 잠수 → 타깃 지정 → 경고 =================

void UDRS2MoleBurrowStrike::BeginBurrowPhase()
{
	if (bBurrowPhaseStarted) return;
	bBurrowPhaseStarted = true;

	ADRS2MoleBoss* Boss = GetMoleBoss();
	if (!Boss || !Boss->HasAuthority())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		return;
	}

	// 무적 진입 (숨김 + 콜리전 해제 + 디버프 정리)
	Boss->EnterBurrowedState();

	// 잠수 시점 기준으로 다시 뽑는다 (활성 시점과 상황이 달라졌을 수 있다)
	ADRCharacter* Target = PickRandomTarget();
	if (!Target || !ResolveEruptGround(Target, PendingEruptGround))
	{
		UE_LOG(LogDR, Warning, TEXT("[MoleBoss] 잠수 후 타깃/착지점 확보 실패 — 제자리에서 복귀합니다."));
		Boss->ExitBurrowedState();
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		return;
	}

	CachedTarget = Target;

	// ★경고 반경은 반드시 보스의 융기 반경과 같은 함수에서 나온다 (어긋나면 회피 불가)
	if (WarningActorClass)
	{
		const FTransform WarningTransform(FRotator::ZeroRotator, PendingEruptGround);

		ADRS2GroundWarning* Warning = GetWorld()->SpawnActorDeferred<ADRS2GroundWarning>(
			WarningActorClass, WarningTransform, Boss, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

		if (Warning)
		{
			Warning->InitWarning(Boss->GetEruptRadius(), WarningDuration);
			Warning->FinishSpawning(WarningTransform);
			WarningActor = Warning;
		}
	}
	else
	{
		UE_LOG(LogDR, Error,
			TEXT("[MoleBoss] WarningActorClass 미설정 — 경고 없이 융기합니다. GA_S2MoleBoss_BurrowStrike 를 확인하세요."));
	}

	// 추종 모드일 때만 위치를 갱신한다 (기본값은 고정)
	if (bWarningFollowsTarget)
	{
		GetWorld()->GetTimerManager().SetTimer(
			WarningFollowTimer, this, &UDRS2MoleBurrowStrike::HandleWarningFollowTick,
			WarningFollowInterval, true);
	}

	GetWorld()->GetTimerManager().SetTimer(
		WarningTimer, this, &UDRS2MoleBurrowStrike::HandleWarningFinished, WarningDuration, false);
}

void UDRS2MoleBurrowStrike::HandleWarningFollowTick()
{
	if (!CachedTarget.IsValid() || !WarningActor.IsValid()) return;

	FVector NewGround;
	if (ResolveEruptGround(CachedTarget.Get(), NewGround))
	{
		PendingEruptGround = NewGround;
		WarningActor->UpdateGroundLocation(NewGround);
	}
}

void UDRS2MoleBurrowStrike::HandleWarningFinished()
{
	GetWorld()->GetTimerManager().ClearTimer(WarningFollowTimer);

	// 경고 액터는 SetLifeSpan 으로 스스로 사라진다 (복제 삭제)
	WarningActor = nullptr;

	if (EruptDelayAfterWarning > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			EruptTimer, this, &UDRS2MoleBurrowStrike::HandleErupt, EruptDelayAfterWarning, false);
	}
	else
	{
		HandleErupt();
	}
}

// ================= 융기 =================

void UDRS2MoleBurrowStrike::HandleErupt()
{
	ADRS2MoleBoss* Boss = GetMoleBoss();
	if (!Boss || !Boss->HasAuthority())
	{
		CancelAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true);
		return;
	}

	// 타깃 쪽을 보며 솟아오른다
	FRotator Facing = Boss->GetActorRotation();
	if (CachedTarget.IsValid())
	{
		const FVector ToTarget = CachedTarget->GetActorLocation() - PendingEruptGround;
		if (!ToTarget.GetSafeNormal2D().IsNearlyZero())
		{
			Facing = ToTarget.Rotation();
		}
	}

	// ★텔레포트 → 융기 몽타주 → 가시화 → 링 버퍼가 이 안에서 순서대로 처리된다
	Boss->EruptAt(PendingEruptGround, FRotator(0.f, Facing.Yaw, 0.f));

	ApplyEruptDamage(Boss->GetActorLocation());

	// 융기 몽타주가 도는 동안 어빌리티를 살려 둬 재발동을 막는다 (길이는 몽타주 우선)
	const float EmergeLength = Boss->GetEmergeMontageLength();
	const float ActualRecoverDuration = (EmergeLength > 0.f) ? EmergeLength : EmergeRecoverDuration;

	if (ActualRecoverDuration > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			EmergeTimer, this, &UDRS2MoleBurrowStrike::HandleEmergeFinished, ActualRecoverDuration, false);
	}
	else
	{
		HandleEmergeFinished();
	}
}

void UDRS2MoleBurrowStrike::HandleEmergeFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UDRS2MoleBurrowStrike::ApplyEruptDamage(const FVector& Center)
{
	ADRS2MoleBoss* Boss = GetMoleBoss();
	if (!Boss) return;

	const float Radius = Boss->GetEruptRadius();
	const float RadiusSq = FMath::Square(Radius);

	TArray<AActor*> Candidates;
	TArray<AActor*> ActorsToIgnore;
	ActorsToIgnore.Add(Boss);

	UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(
		Boss, Candidates, ActorsToIgnore, Radius + VerticalTolerance, Center);

	for (AActor* Target : Candidates)
	{
		if (!IsValid(Target) || Target == Boss) continue;

		// ★IsNotFriend 를 쓰지 않는다 — 아군(다른 적)에게도 맞아야 하는 사양이다.
		//   대신 타입으로 직접 거른다 (CleanserSite 등이 섞여 들어오는 것을 막는다).
		const bool bIsPlayer = (Cast<ADRCharacter>(Target) != nullptr);
		const bool bIsEnemy = (Cast<ADREnemy>(Target) != nullptr);
		if (!bIsPlayer && !(bIsEnemy && bHitOtherEnemies)) continue;

		FVector Flat = Target->GetActorLocation() - Center;
		Flat.Z = 0.f;
		if (Flat.SizeSquared() > RadiusSq) continue;

		// 띄우기: 중심에서 방사형 + 상향. 정중앙이면 순수 상향.
		const FVector Radial = Flat.IsNearlyZero() ? FVector::ZeroVector : Flat.GetSafeNormal();
		const FVector Knockback = Radial * LaunchRadial + FVector(0.f, 0.f, LaunchZ);

		// ★BaseDamage 를 덮어쓰지 않는다.
		//   MakeDamageEffectParamsFromClassDefaults 가 이미
		//   Damage.GetValueAtLevel(GetAbilityLevel()) 를 넣어 주고,
		//   두더지 보스는 어빌리티 레벨 자체가 인원수라 인원 축이 그대로 반영된다.
		FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(Target);
		// 어트리뷰트셋이 이 벡터를 읽어 LaunchCharacter 를 수행한다
		// (DRPlayerAttributeSet.cpp:322-326 / DREnemyAttributeSet.cpp:169-172)
		Params.KnockbackForce = Knockback;
		Params.DeathImpulse = Knockback;

		UDRAbilitySystemLibrary::ApplyDamageEffect(Params);
	}
}

// ================= 타깃 선정 =================

bool UDRS2MoleBurrowStrike::HasAnyValidTarget() const
{
	return PickRandomTarget() != nullptr;
}

ADRCharacter* UDRS2MoleBurrowStrike::PickRandomTarget() const
{
	const AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar)) return nullptr;

	const UWorld* World = Avatar->GetWorld();
	if (!World) return nullptr;

	const ADRGameStateBase* DRGameState = World->GetGameState<ADRGameStateBase>();
	if (!DRGameState) return nullptr;

	const float MaxRangeSq = FMath::Square(MaxTargetRange);
	const FVector Origin = Avatar->GetActorLocation();

	TArray<ADRCharacter*> Pool;
	for (ADRCharacter* Player : DRGameState->GetAlivePlayers())   // IsDead 필터 포함
	{
		if (!IsValid(Player)) continue;

		// 열차 좌석 위로 솟아오르는 그림을 막는다
		if (bExcludeSeatedPlayers && Player->IsSeatedOnTrain()) continue;

		if (FVector::DistSquared2D(Player->GetActorLocation(), Origin) > MaxRangeSq) continue;

		Pool.Add(Player);
	}

	if (Pool.Num() == 0) return nullptr;

	return Pool[FMath::RandRange(0, Pool.Num() - 1)];
}

bool UDRS2MoleBurrowStrike::ResolveEruptGround(const AActor* Target, FVector& OutGround) const
{
	if (!IsValid(Target)) return false;

	UWorld* World = GetWorld();
	if (!World) return false;

	// ① 타깃 발밑에서 하향 트레이스 → 실제 지면
	const FVector Start = Target->GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, 1500.f);

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(Target);
	if (const AActor* Avatar = GetAvatarActorFromActorInfo())
	{
		Params.AddIgnoredActor(Avatar);
	}

	if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		return false;
	}

	// ② NavMesh 투영 — 열차 지붕/난간 위처럼 "솟아오를 수 없는 곳"을 배제한다
	if (UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World))
	{
		FNavLocation NavLocation;
		if (NavSys->ProjectPointToNavigation(Hit.ImpactPoint, NavLocation, FVector(200.f, 200.f, 200.f)))
		{
			OutGround = NavLocation.Location;
			return true;
		}
	}

	// NavMesh 가 없거나 투영에 실패하면 트레이스 결과를 그대로 쓴다
	OutGround = Hit.ImpactPoint;
	return true;
}

// ================= 종료 / 취소 =================

void UDRS2MoleBurrowStrike::ClearAllTimers()
{
	if (UWorld* World = GetWorld())
	{
		FTimerManager& TM = World->GetTimerManager();
		TM.ClearTimer(BurrowTimer);
		TM.ClearTimer(WarningTimer);
		TM.ClearTimer(EruptTimer);
		TM.ClearTimer(EmergeTimer);
		TM.ClearTimer(WarningFollowTimer);
	}
}

void UDRS2MoleBurrowStrike::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// ★어떤 경로로 끝나든 여기서 전부 되돌린다.
	//   보스 도망(NotifyStashed → CancelAbilities)도 이 경로를 탄다.
	ClearAllTimers();

	if (ADRS2GroundWarning* Warning = WarningActor.Get())
	{
		Warning->Destroy();
	}
	WarningActor = nullptr;

	// 잠수한 채로 어빌리티가 끝나면 보스가 영원히 숨겨진다
	if (ADRS2MoleBoss* Boss = GetMoleBoss())
	{
		if (Boss->IsBurrowed())
		{
			Boss->ExitBurrowedState();
		}
	}

	CachedTarget = nullptr;
	bBurrowPhaseStarted = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
