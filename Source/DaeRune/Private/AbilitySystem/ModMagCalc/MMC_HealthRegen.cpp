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

    // AttributeSet 가져오기
    const UAbilitySystemComponent* TargetASC = nullptr;
    if (Spec.GetContext().GetInstigatorAbilitySystemComponent())
    {
        TargetASC = Spec.GetContext().GetInstigatorAbilitySystemComponent();
    }

    if (!TargetASC) return 0.f;

    const UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(
        TargetASC->GetAttributeSet(UDRPlayerAttributeSet::StaticClass()));

    if (!PlayerAS) return 0.f;

    // 부패 상태 확인
    if (PlayerAS->IsCorrupted())
    {
        return 0.f; // 부패 상태에서는 자연 회복 없음
    }

    // 현재 컨테이너 계산
    const int32 ContainerIndex = PlayerAS->GetCurrentContainerIndex();
    const float ContainerHealth = PlayerAS->GetContainerHealth();
    const float ContainerMax = (ContainerIndex + 1) * ContainerHealth;

    // 현재 컨테이너가 가득 차면 회복 중지
    if (FMath::IsNearlyEqual(CurrentHealth, ContainerMax, 0.1f))
    {
        return 0.f;
    }

    // 회복 가능량 계산 (현재 컨테이너 최대치까지만)
    const float PossibleHeal = ContainerMax - CurrentHealth;
    return FMath::Min(NORMAL_HEAL_PER_TICK, PossibleHeal);
}
