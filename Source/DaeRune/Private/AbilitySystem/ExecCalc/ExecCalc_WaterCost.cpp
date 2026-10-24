// Copyright DaeRune


#include "AbilitySystem/ExecCalc/ExecCalc_WaterCost.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "Game/DRProgressionConfig.h"

// Capture�� �Ӽ��� ����
struct FWaterCostStatics
{
    DECLARE_ATTRIBUTE_CAPTUREDEF(Water);
    DECLARE_ATTRIBUTE_CAPTUREDEF(Health);

    FWaterCostStatics()
    {
        // Source(������)�� Water�� Health�� Capture
        DEFINE_ATTRIBUTE_CAPTUREDEF(UDRAttributeSet, Water, Source, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UDRAttributeSet, Health, Source, false);
    }
};

static const FWaterCostStatics& WaterCostStatics()
{
    static FWaterCostStatics Statics;
    return Statics;
}

UExecCalc_WaterCost::UExecCalc_WaterCost()
{
    // Capture�� �Ӽ��� ���
    RelevantAttributesToCapture.Add(WaterCostStatics().WaterDef);
    RelevantAttributesToCapture.Add(WaterCostStatics().HealthDef);
}

void UExecCalc_WaterCost::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

    // SetByCaller�� ���޵� �ʿ� Water �� ��������
    const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
    float RequiredWater = Spec.GetSetByCallerMagnitude(GameplayTags.Cost_Water, false, 0.f);

    if (RequiredWater <= 0.f)
    {
        return;
    }

    // Evaluation Parameters ����
    const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

    FAggregatorEvaluateParameters EvaluationParameters;
    EvaluationParameters.SourceTags = SourceTags;
    EvaluationParameters.TargetTags = TargetTags;

    // ���� Water�� Health �� ��������
    float CurrentWater = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
        WaterCostStatics().WaterDef,
        EvaluationParameters,
        CurrentWater);

    float CurrentHealth = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
        WaterCostStatics().HealthDef,
        EvaluationParameters,
        CurrentHealth);

    // Cost ���
    float ActualWaterCost = 0.f;
    float HealthCost = 0.f;

    // ★Plan3(공용칩) §17.8.2★ 피의 계약 칩: 물을 전혀 쓰지 않고 전액을 체력으로 낸다.
    // ★UDRGameplayAbility::CheckCost 의 분기와 반드시 같은 수식(FloorToFloat 포함)이어야 한다.★
    const UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
    if (UDRAbilitySystemLibrary::HasUpgradeFeature(
            SourceASC, GameplayTags.Upgrade_Feature_Combat_BloodCost))
    {
        const AActor* TuningContext = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
        const float Ratio = UDRAbilitySystemLibrary::GetChipTuning(TuningContext).BloodCostHealthRatio;

        ActualWaterCost = 0.f;
        HealthCost = FMath::FloorToFloat(RequiredWater * Ratio);
    }
    else if (CurrentWater >= RequiredWater)
    {
        // Water�� ����� ���: Water�� ����
        ActualWaterCost = RequiredWater;
        HealthCost = 0.f;
    }
    else
    {
        // Water�� ������ ���: Water ���� + Health �߰� ����
        ActualWaterCost = CurrentWater;
        float WaterShortage = RequiredWater - CurrentWater;
        HealthCost = FMath::FloorToFloat(WaterShortage * 0.5f);
    }

    // Water ���� ����
    if (ActualWaterCost > 0.f)
    {
        FGameplayModifierEvaluatedData WaterData(
            UDRAttributeSet::GetWaterAttribute(),
            EGameplayModOp::Additive,
            -ActualWaterCost);
        OutExecutionOutput.AddOutputModifier(WaterData);
    }

    // Health ���� ���� (�ʿ��� ��츸)
    if (HealthCost > 0.f)
    {
        FGameplayModifierEvaluatedData HealthData(
            UDRAttributeSet::GetHealthAttribute(),
            EGameplayModOp::Additive,
            -HealthCost);
        OutExecutionOutput.AddOutputModifier(HealthData);
    }
}
