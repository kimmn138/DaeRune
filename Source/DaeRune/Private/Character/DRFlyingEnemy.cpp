// Copyright DaeRune


#include "Character/DRFlyingEnemy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "AI/DRAIController.h"
#include "BrainComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"

ADRFlyingEnemy::ADRFlyingEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// === CharacterMovement 비행 설정 ===
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	MoveComp->DefaultLandMovementMode = EMovementMode::MOVE_Flying;
	MoveComp->SetMovementMode(MOVE_Flying);
	MoveComp->MaxFlySpeed = FlyingSpeed;
	MoveComp->BrakingDecelerationFlying = FlyingDeceleration;
	MoveComp->GravityScale = 0.f;
	MoveComp->bOrientRotationToMovement = false;
	MoveComp->NavAgentProps.bCanFly = true;
	MoveComp->NavAgentProps.bCanWalk = false;
	MoveComp->NavAgentProps.bCanSwim = false;

	// === Flying 모드 안전장치 (Step 2.2) ===
	MoveComp->MaxStepHeight = 0.f;            // 계단 오르기 비활성화
	MoveComp->SetWalkableFloorAngle(0.f);     // 걷기 불가 (안전장치)

	// === 지형지물 충돌 유지 ===
	// 중요: 지형지물(벽, 기둥, 천장 등)과의 충돌은 그대로 유지.
	// "바닥의 영향을 안 받는다" = 바닥 높낮이가 고도에 영향을 주지 않음을 의미.
	// → CapsuleComponent의 기본 Pawn 충돌 프로파일을 유지한다.
	// → GravityScale = 0 + Flying 모드 + Tick Z-보정으로 "고도 불변"을 달성.
	// → 수평 이동 시 지형지물에 부딪히면 자연스럽게 멈춤 (Sweep).
}

void ADRFlyingEnemy::BeginPlay()
{
	Super::BeginPlay();

	// FixedAltitude가 지정되지 않았다면 현재 Z를 기준으로 설정
	if (FixedAltitude < 0.f)
	{
		FixedAltitude = GetActorLocation().Z;
	}
	else
	{
		// 지정된 고도로 초기 위치 조정
		FVector Loc = GetActorLocation();
		Loc.Z = FixedAltitude;
		SetActorLocation(Loc);
	}

	// ===== 비행 루프 사운드 (Plan2.md §5.2 방안 A) =====
	// SoundCue 자체에 Looping=true, SoundAttenuation(SA_Medium), SoundConcurrency(CC_DragonFlyFlight, Max=1) 지정.
	// Concurrency로 동시 인스턴스가 1개로 제한되며, Attenuation으로 멀리 있는 파리 소리는 자동 무음 처리됨.
	// 데디케이티드 서버에서는 오디오 출력이 없으므로 컴포넌트를 만들지 않는다.
	if (FlightSound && GetNetMode() != NM_DedicatedServer)
	{
		FlightLoopComponent = UGameplayStatics::SpawnSoundAttached(
			FlightSound,
			GetRootComponent(),
			NAME_None,
			FVector::ZeroVector,
			EAttachLocation::SnapToTarget,
			/*bStopWhenAttachedToDestroyed=*/true);
	}
}

void ADRFlyingEnemy::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 변경/월드 정리/Destroy 등 어떤 사유든 비행 루프 즉시 정지.
	// (Death 경로에서 이미 호출되었더라도 StopFlightLoop는 중복 호출에 안전)
	StopFlightLoop();

	Super::EndPlay(EndPlayReason);
}

void ADRFlyingEnemy::StopFlightLoop()
{
	if (IsValid(FlightLoopComponent))
	{
		FlightLoopComponent->Stop();
		FlightLoopComponent = nullptr;
	}
}

void ADRFlyingEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority() && !bDead)
	{
		MaintainFixedAltitude();
	}
}

void ADRFlyingEnemy::MaintainFixedAltitude()
{
	FVector Loc = GetActorLocation();
	if (!FMath::IsNearlyEqual(Loc.Z, FixedAltitude, 1.f))
	{
		// Z만 FixedAltitude로 스냅 (XY는 유지)
		FVector NewLoc = FVector(Loc.X, Loc.Y, FixedAltitude);

		// bSweep = true: 천장/장애물이 있으면 충돌로 막힘
		// 이렇게 해야 "지형지물 충돌은 유지" 요구사항을 만족
		SetActorLocation(NewLoc, /*bSweep=*/true, nullptr, ETeleportType::None);

		// Velocity의 Z 성분 제거 (이동 중 수직 튀어오름 방지)
		FVector Vel = GetCharacterMovement()->Velocity;
		Vel.Z = 0.f;
		GetCharacterMovement()->Velocity = Vel;
	}
}

void ADRFlyingEnemy::OnMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	// Flying 모드에서는 MaxFlySpeed를 사용 (부모는 MaxWalkSpeed만 설정)
	GetCharacterMovement()->MaxFlySpeed = Data.NewValue;
}

void ADRFlyingEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ADRFlyingEnemy, bIsLockedDown);
}

// ===== 경직(Lockdown) 시스템 =====

void ADRFlyingEnemy::StartSkillLockdown()
{
	if (!HasAuthority()) return;
	if (bDead) return;

	bIsLockedDown = true;

	// Blackboard 업데이트 (BT가 즉시 공격 중단)
	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsBool(DRBlackboardKeys::IsLockedDown, true);
	}

	// Lockdown GE 적용 (이동 속도 0 + 공격 차단 태그)
	if (LockdownEffectClass && AbilitySystemComponent)
	{
		FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
		Context.AddSourceObject(this);
		FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
			LockdownEffectClass, 1.f, Context);

		Spec.Data->SetDuration(SkillLockdownDuration, true);

		LockdownEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data);
	}

	// 경직 종료 타이머
	GetWorldTimerManager().SetTimer(
		SkillLockdownTimerHandle,
		this,
		&ADRFlyingEnemy::EndSkillLockdown,
		SkillLockdownDuration,
		false);

	// AI 이동 정지
	if (DRAIController)
	{
		DRAIController->StopMovement();
	}
}

void ADRFlyingEnemy::EndSkillLockdown()
{
	if (!HasAuthority()) return;

	bIsLockedDown = false;

	if (UBlackboardComponent* BB = GetBlackboardComponent())
	{
		BB->SetValueAsBool(DRBlackboardKeys::IsLockedDown, false);
	}

	// GE 제거 (Duration 기반이므로 자동 만료되지만 조기 해제 시 명시적 제거)
	if (AbilitySystemComponent && LockdownEffectHandle.IsValid())
	{
		AbilitySystemComponent->RemoveActiveGameplayEffect(LockdownEffectHandle);
		LockdownEffectHandle = FActiveGameplayEffectHandle();
	}
}

// ===== 사망 처리: Death 애니메이션 + 중력 추락 =====

void ADRFlyingEnemy::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
	// 0. 비행 루프 사운드 정지 (모든 인스턴스에서 실행됨 - 서버+클라 동기화)
	StopFlightLoop();

	// 1. 타이머 정리
	GetWorldTimerManager().ClearTimer(SkillLockdownTimerHandle);

	// 2. 경직 상태 해제 (안전장치)
	if (HasAuthority())
	{
		bIsLockedDown = false;
		if (UBlackboardComponent* BB = GetBlackboardComponent())
		{
			BB->SetValueAsBool(DRBlackboardKeys::IsLockedDown, false);
		}
		if (AbilitySystemComponent && LockdownEffectHandle.IsValid())
		{
			AbilitySystemComponent->RemoveActiveGameplayEffect(LockdownEffectHandle);
			LockdownEffectHandle = FActiveGameplayEffectHandle();
		}
	}

	// 3. 중력 추락 활성화: Flying → Falling 모드로 전환
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp)
	{
		// 속도 초기화 (수평/수직 모두 - 중력이 다시 가속시킴)
		MoveComp->Velocity = FVector::ZeroVector;

		// 중력 활성화
		MoveComp->GravityScale = DeathGravityScale;

		// Falling 모드로 전환 → CharacterMovement가 중력을 자동 적용
		MoveComp->SetMovementMode(MOVE_Falling);

		// Falling 시 순수 중력만 적용
		MoveComp->FallingLateralFriction = 0.f;
		MoveComp->AirControl = 0.f;
	}

	// 4. AI 정지 (BT 중단)
	if (DRAIController)
	{
		DRAIController->StopMovement();
		if (UBrainComponent* Brain = DRAIController->GetBrainComponent())
		{
			Brain->StopLogic(TEXT("Dead"));
		}
	}

	// 5. 부모 사망 처리 (Death 애니메이션, Dissolve, LifeSpan 등)
	Super::MulticastHandleDeath_Implementation(DeathImpulse);

	// 6. Super가 이동을 비활성화하므로 낙하를 위해 복원
	if (MoveComp)
	{
		MoveComp->SetComponentTickEnabled(true);
		MoveComp->SetMovementMode(MOVE_Falling);
		MoveComp->GravityScale = DeathGravityScale;
	}

	// 7. 캡슐 충돌 복원 (Super가 NoCollision으로 설정하므로 바닥 착지용으로 재활성화)
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
		Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	}

	// 8. 착지 감지 바인딩
	LandedDelegate.AddDynamic(this, &ADRFlyingEnemy::OnDeathLanded);
}

void ADRFlyingEnemy::OnDeathLanded(const FHitResult& Hit)
{
	if (!bDead) return;

	// 중복 호출 방지
	LandedDelegate.RemoveDynamic(this, &ADRFlyingEnemy::OnDeathLanded);

	// 낙하 정지
	UCharacterMovementComponent* MoveComp = GetCharacterMovement();
	if (MoveComp)
	{
		MoveComp->StopMovementImmediately();
		MoveComp->SetMovementMode(MOVE_None);
		MoveComp->DisableMovement();
	}

	// 착지 후 추가 체류 시간 설정
	SetLifeSpan(PostLandingLifeSpan);

	// 캡슐 충돌 완전 비활성화
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
	}
}
