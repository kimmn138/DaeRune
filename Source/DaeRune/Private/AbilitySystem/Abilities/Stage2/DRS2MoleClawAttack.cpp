// Copyright DaeRune

#include "AbilitySystem/Abilities/Stage2/DRS2MoleClawAttack.h"

#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Character/DREnemy.h"
#include "Character/Stage2/DRS2MoleBoss.h"
#include "DaeRune/DRLogChannels.h"

UDRS2MoleClawAttack::UDRS2MoleClawAttack()
{
	// AI 가 서버에서 활성화하는 어빌리티다. 타이머/상태를 위해 액터별 인스턴스가 필요하다.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NetExecutionPolicy = EGameplayAbilityNetExecutionPolicy::ServerOnly;

	// ★커브 테이블을 쓰는 값은 Value 를 1 로 둔다 (결과 = Value * Curve[Level]).
	Hit1Damage = FScalableFloat(1.f);
	Hit2Damage = FScalableFloat(1.f);
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
}

void UDRS2MoleClawAttack::PerformClawSweep(int32 HitIndex)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!IsValid(Avatar) || !Avatar->HasAuthority()) return;

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

	UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(
		Avatar, Candidates, ActorsToIgnore, SweepRadius + VerticalTolerance, Origin);

	const float RadiusSq = FMath::Square(SweepRadius);

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

	// 물 보상 감소는 "공격 1회"당 1번만 센다 (2타짜리라 1타에서만 호출)
	if (HitIndex == 0)
	{
		if (ADREnemy* Enemy = Cast<ADREnemy>(Avatar))
		{
			Enemy->OnAttackExecuted();
		}
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
