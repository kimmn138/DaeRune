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
    // ȸ���� ���
    constexpr float NORMAL_HEAL_PER_TICK = 10.f;

    // �ҽ���Ÿ���� Gameplay Tag �����̳� ��������
    const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

    // �±� ��� ���͸������� ���� �� ����� EvaluationParameters ����
    FAggregatorEvaluateParameters EvaluationParameters;
    EvaluationParameters.SourceTags = SourceTags;
    EvaluationParameters.TargetTags = TargetTags;

    // Health �� ĸó (�Ӽ� ĸó ���)
    float CurrentHealth = 0.f;
    GetCapturedAttributeMagnitude(HealthDef, Spec, EvaluationParameters, CurrentHealth);

    float MaxHealth = 0.f;
    GetCapturedAttributeMagnitude(MaxHealthDef, Spec, EvaluationParameters, MaxHealth);

    // ü���� �̹� �ִ�ġ�� ȸ�� ���ʿ�
    if (CurrentHealth >= MaxHealth) return 0.f;

    // AttributeSet ��������
    const UAbilitySystemComponent* TargetASC = nullptr;
    if (Spec.GetContext().GetInstigatorAbilitySystemComponent())
    {
        TargetASC = Spec.GetContext().GetInstigatorAbilitySystemComponent();
    }

    if (!TargetASC) return 0.f;

    const UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(
        TargetASC->GetAttributeSet(UDRPlayerAttributeSet::StaticClass()));

    if (!PlayerAS) return 0.f;

    // ���� ���� Ȯ��
    if (PlayerAS->IsCorrupted())
    {
        return 0.f; // ���� ���¿����� �ڿ� ȸ�� ����
    }

    // ���� �����̳� ���
    const int32 ContainerIndex = PlayerAS->GetCurrentContainerIndex();
    const float ContainerHealth = PlayerAS->GetContainerHealth();
    const float ContainerMax = (ContainerIndex + 1) * ContainerHealth;

    // ���� �����̳ʰ� ���� ���� ȸ�� ����
    if (FMath::IsNearlyEqual(CurrentHealth, ContainerMax, 0.1f))
    {
        return 0.f;
    }

    // ȸ�� ���ɷ� ��� (���� �����̳� �ִ�ġ������)
    const float PossibleHeal = ContainerMax - CurrentHealth;
    return FMath::Min(NORMAL_HEAL_PER_TICK, PossibleHeal);
}
