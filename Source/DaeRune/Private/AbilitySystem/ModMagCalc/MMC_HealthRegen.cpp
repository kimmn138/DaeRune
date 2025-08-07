// Copyright DaeRune


#include "AbilitySystem/ModMagCalc/MMC_HealthRegen.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"

UMMC_HealthRegen::UMMC_HealthRegen()
{
	HealthDef.AttributeToCapture = UDRAttributeSet::GetHealthAttribute();
	HealthDef.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
	HealthDef.bSnapshot = false;

	MaxHealthDef.AttributeToCapture = UDRAttributeSet::GetMaxHealthAttribute();
	MaxHealthDef.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
	MaxHealthDef.bSnapshot = false;

	RelevantAttributesToCapture.Add(HealthDef);
	RelevantAttributesToCapture.Add(MaxHealthDef);
}

float UMMC_HealthRegen::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	// 회복량 상수
	constexpr float NORMAL_HEAL_PER_TICK = 5.f;

	// 소스·타겟의 Gameplay Tag 컨테이너 가져오기
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// 태그 기반 필터링·조건 적용 시 사용할 EvaluationParameters 설정
	FAggregatorEvaluateParameters EvaluationParameters;
	EvaluationParameters.SourceTags = SourceTags;
	EvaluationParameters.TargetTags = TargetTags;

	// Health 값 캡처 (속성 캡처 방식)
	float CurrentHealth = 0.f;
	GetCapturedAttributeMagnitude(HealthDef, Spec, EvaluationParameters, CurrentHealth);

	float MaxHealth = 0.f;
	GetCapturedAttributeMagnitude(MaxHealthDef, Spec, EvaluationParameters, MaxHealth);

	// 체력이 이미 최대치면 회복 불필요
	if (CurrentHealth >= MaxHealth) return 0.f;

	// 부패 상태 판단 (MaxHealth == 100)
	const bool bIsCorrupted = FMath::IsNearlyEqual(MaxHealth, UDRPlayerAttributeSet::CORRUPT_MAX_HEALTH);

	if (bIsCorrupted)
	{
		// 부패 상태: 체력 재생 없음
		return 0.f;
	}

	// 정상 상태: 컨테이너 기반 회복
	if (CurrentHealth <= 0.f) return 0.f;

	// 현재 컨테이너 계산
	int32 ContainerIndex = FMath::FloorToInt(CurrentHealth / UDRPlayerAttributeSet::CONTAINER_HEALTH);

	// 정확히 컨테이너 경계에 있는 경우
	if (FMath::IsNearlyEqual(CurrentHealth, ContainerIndex * UDRPlayerAttributeSet::CONTAINER_HEALTH))
	{
		ContainerIndex = FMath::Max(0, ContainerIndex - 1);
	}

	ContainerIndex = FMath::Clamp(ContainerIndex, 0, UDRPlayerAttributeSet::NUM_CONTAINERS - 1);

	// 현재 컨테이너의 최대 체력
	const float ContainerMax = (ContainerIndex + 1) * UDRPlayerAttributeSet::CONTAINER_HEALTH;

	// 현재 컨테이너가 가득 찼으면 0 반환
	if (FMath::IsNearlyEqual(CurrentHealth, ContainerMax, 0.1f))
	{
		return 0.f;
	}

	// 회복 가능량 (현재 컨테이너 최대치까지만)
	const float PossibleHeal = ContainerMax - CurrentHealth;
	return FMath::Min(NORMAL_HEAL_PER_TICK, PossibleHeal);
}
