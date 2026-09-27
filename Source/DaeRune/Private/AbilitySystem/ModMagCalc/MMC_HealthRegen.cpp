// Copyright DaeRune


#include "AbilitySystem/ModMagCalc/MMC_HealthRegen.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"

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

    // ★Plan3(공용칩) §17.6.3★ 회복 차단 칩: GE 가 어떤 경로로든 남아 있어도 틱을 0 으로 만든다.
    // StartHealthRegen 의 진입 차단과 ★2중화★ 하는 것이 목적이다 —
    // 진입만 막으면 이미 적용돼 있던 GE 가 계속 회복시킨다.
    if (UDRAbilitySystemLibrary::HasUpgradeFeature(
            TargetASC, FDRGameplayTags::Get().Upgrade_Feature_Combat_NoHealing))
    {
        return 0.f;
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
