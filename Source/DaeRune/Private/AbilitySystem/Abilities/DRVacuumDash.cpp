// Copyright DaeRune


#include "AbilitySystem/Abilities/DRVacuumDash.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Character/DRRobotVacuumCharacter.h"
#include "DRGameplayTags.h"
#include "TimerManager.h"

void UDRVacuumDash::StartChargingGauge()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	// 게이지/물/버프는 전부 서버 권한 — 클라 인스턴스는 로컬 연출(BP)만 수행
	if (!Avatar || !Avatar->HasAuthority()) return;

	ADRRobotVacuumCharacter* Vacuum = GetVacuum();
	if (!Vacuum)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 충돌 델리게이트 수신 준비 (해제는 EndAbility)
	if (!bImpactBound)
	{
		Vacuum->OnDashImpact.AddDynamic(this, &UDRVacuumDash::HandleDashImpact);
		bImpactBound = true;
	}

	Gauge = 0;
	bDashing = false;
	NotifyGauge();

	// ABP Dash_Charge 상태 전이용 복제 플래그 (§13.6)
	Vacuum->SetDashCharging(true);

	GetWorld()->GetTimerManager().SetTimer(ChargeTimerHandle, this, &UDRVacuumDash::TickCharge, ChargeInterval, true);
}

void UDRVacuumDash::TickCharge()
{
	// 최대치면 대기 (추가 소모 없음)
	if (Gauge >= MaxGauge) return;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	const UDRAttributeSet* AttributeSet = ASC
		? Cast<UDRAttributeSet>(ASC->GetAttributeSet(UDRAttributeSet::StaticClass()))
		: nullptr;
	if (!AttributeSet) return;

	// 물 부족분은 체력으로 대납 (부족분 ×0.5, ExecCalc_WaterCost와 동일 규칙) —
	// 체력마저 부족하면(CheckCost와 동일하게 지불 후 0 이하가 되면) 충전 일시 정지 (게이지 유지, 다음 틱 재시도)
	const float CurrentWater = AttributeSet->GetWater();
	if (CurrentWater < WaterPerGauge)
	{
		const float RequiredHealth = FMath::FloorToFloat((WaterPerGauge - CurrentWater) * 0.5f);
		if (AttributeSet->GetHealth() <= RequiredHealth)
		{
			return;
		}
	}

	if (ChargeCostEffect)
	{
		FGameplayEffectSpecHandle SpecHandle =
			UDRAbilitySystemLibrary::CreateEffectSpec(ASC, ChargeCostEffect, GetAvatarActorFromActorInfo());
		UDRAbilitySystemLibrary::ApplyEffectSpecWithSetByCaller(
			ASC, SpecHandle, FDRGameplayTags::Get().Cost_Water, WaterPerGauge);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("UDRVacuumDash: ChargeCostEffect 미설정 — 물 소모 없이 충전됩니다."));
	}

	Gauge++;
	NotifyGauge();
}

void UDRVacuumDash::ReleaseGauge()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTimerHandle);
	}

	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return;

	// 게이지 0 → 돌진 없음, 그냥 종료
	if (Gauge <= 0)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	ADRRobotVacuumCharacter* Vacuum = GetVacuum();
	if (!Vacuum)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// 충전 종료 (ABP Dash_Charge 탈출)
	Vacuum->SetDashCharging(false);

	bDashing = true;
	ApplyBuffStacks(Gauge);
	Vacuum->SetDashCollisionEnabled(true);

	if (Gauge >= MaxGauge)
	{
		// 지속 돌진: 자동 전진(캐릭터 Tick) + 감쇠 없음. 종료는 충돌/브레이크/강제취소.
		// State.RobotVacuum.SustainedDash 태그도 캐릭터가 함께 부여 (공기탄 반동 예외 판정용)
		Vacuum->SetSustainedDash(true);
	}
	else
	{
		// 일반 돌진: 1초당 1칸 감쇠
		GetWorld()->GetTimerManager().SetTimer(DecayTimerHandle, this, &UDRVacuumDash::TickDecay, DecayInterval, true);
	}
}

void UDRVacuumDash::TickDecay()
{
	Gauge--;
	NotifyGauge();

	if (Gauge <= 0)
	{
		FinishDash(false, nullptr);
		return;
	}

	// 버프 스택 1 제거 → MoveSpeed 어트리뷰트 재계산 → 기존 OnMoveSpeedChanged 바인딩이 CMC 반영
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && SpeedBuffHandle.IsValid())
	{
		ASC->RemoveActiveGameplayEffect(SpeedBuffHandle, 1);
	}
}

void UDRVacuumDash::ApplyBuffStacks(int32 Stacks)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	if (!SpeedBuffEffect)
	{
		UE_LOG(LogTemp, Warning, TEXT("UDRVacuumDash: SpeedBuffEffect 미설정 — 이속 버프 없이 진행합니다."));
		return;
	}

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());

	for (int32 i = 0; i < Stacks; ++i)
	{
		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(SpeedBuffEffect, GetAbilityLevel(), ContextHandle);
		if (SpecHandle.IsValid())
		{
			SpeedBuffHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

void UDRVacuumDash::ClearAllBuff()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && SpeedBuffHandle.IsValid())
	{
		// StacksToRemove 기본값 -1 = 전량 제거
		ASC->RemoveActiveGameplayEffect(SpeedBuffHandle);
	}
	SpeedBuffHandle.Invalidate();
}

void UDRVacuumDash::HandleDashImpact(AActor* HitActor, const FHitResult& Hit)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return;
	if (!bDashing) return;

	// 판정 시점 게이지 저장 (FinishDash가 0으로 리셋하기 전)
	const int32 G = Gauge;

	ADRRobotVacuumCharacter* Vacuum = GetVacuum();
	const bool bWasSustained = Vacuum && Vacuum->bSustainedDash;

	// 대상 데미지: 적 = G×10, 타 플레이어 = G×5. 지형(ASC 없음)은 대상 데미지 없음.
	if (IsValid(HitActor) && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
	{
		const bool bEnemy = UDRAbilitySystemLibrary::IsNotFriend(Avatar, HitActor);
		FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(HitActor);
		Params.BaseDamage = G * (bEnemy ? EnemyDamagePerGauge : PlayerDamagePerGauge);
		// ApplyDamageEffect 직접 호출 — 아군 필터에 걸리지 않음 (Plan3 §8.2)
		UDRAbilitySystemLibrary::ApplyDamageEffect(Params);
	}

	// 지속 돌진 충돌은 자해 50
	if (bWasSustained && SelfDamageEffect)
	{
		UDRAbilitySystemLibrary::CreateAndApplyEffectSpec(
			GetAbilitySystemComponentFromActorInfo(), SelfDamageEffect, Avatar);
	}

	FinishDash(true, HitActor);
}

void UDRVacuumDash::OnBrakePressed()
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar || !Avatar->HasAuthority()) return;
	if (!bDashing) return;

	// 브레이크: 자기 데미지 없음, 게이지/버프 소멸
	FinishDash(false, nullptr);
}

void UDRVacuumDash::FinishDash(bool bFromImpact, AActor* HitActor)
{
	Gauge = 0;
	bDashing = false;
	NotifyGauge();

	ClearAllBuff();

	ADRRobotVacuumCharacter* Vacuum = GetVacuum();
	const bool bWasSustained = Vacuum && Vacuum->bSustainedDash;

	if (Vacuum)
	{
		Vacuum->SetSustainedDash(false);
		Vacuum->SetDashCollisionEnabled(false);

		if (bFromImpact)
		{
			Vacuum->MulticastPlayDashImpactSound(Vacuum->GetActorLocation());
		}
	}

	// 종료 연출 몽타주 (§13.5-④⑤): 충돌 = Crush(일반/지속 공통), S 브레이크(지속) = Stop, 자연 감쇠 = 없음.
	// 직후 EndAbility가 몽타주를 끊지 않도록 bStopWhenAbilityEnds=false (fire-and-forget)
	UAnimMontage* FinishMontage = bFromImpact ? DashCrushMontage.Get()
	                            : (bWasSustained ? DashStopMontage.Get() : nullptr);
	if (FinishMontage)
	{
		UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
			this, NAME_None, FinishMontage, 1.f, NAME_None, /*bStopWhenAbilityEnds*/ false);
		if (MontageTask)
		{
			MontageTask->ReadyForActivation();
		}
	}

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UDRVacuumDash::NotifyGauge()
{
	OnGaugeChanged(Gauge, MaxGauge);

	if (UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo()))
	{
		DRASC->NotifyVacuumDashGaugeChanged(Gauge, MaxGauge);
	}
}

ADRRobotVacuumCharacter* UDRVacuumDash::GetVacuum() const
{
	return Cast<ADRRobotVacuumCharacter>(GetAvatarActorFromActorInfo());
}

void UDRVacuumDash::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	// 사망/스턴 강제 취소 대비 정리 보장 (WaterPump EndAbility 패턴)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeTimerHandle);
		World->GetTimerManager().ClearTimer(DecayTimerHandle);
	}

	ClearAllBuff();

	if (ADRRobotVacuumCharacter* Vacuum = GetVacuum())
	{
		if (Vacuum->HasAuthority())
		{
			Vacuum->SetDashCharging(false);
			Vacuum->SetSustainedDash(false);
			Vacuum->SetDashCollisionEnabled(false);

			// 강제 취소 경로에서도 게이지 UI 리셋
			if (Gauge != 0)
			{
				Gauge = 0;
				NotifyGauge();
			}
		}

		if (bImpactBound)
		{
			Vacuum->OnDashImpact.RemoveDynamic(this, &UDRVacuumDash::HandleDashImpact);
			bImpactBound = false;
		}
	}

	Gauge = 0;
	bDashing = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
