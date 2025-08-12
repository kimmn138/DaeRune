// Copyright DaeRune


#include "AbilitySystem/ExecCalc/ExecCalc_WaterCost.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"

// Capture할 속성들 정의
struct FWaterCostStatics
{
    DECLARE_ATTRIBUTE_CAPTUREDEF(Water);
    DECLARE_ATTRIBUTE_CAPTUREDEF(Health);

    FWaterCostStatics()
    {
        // Source(시전자)의 Water와 Health를 Capture
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
    // Capture할 속성들 등록
    RelevantAttributesToCapture.Add(WaterCostStatics().WaterDef);
    RelevantAttributesToCapture.Add(WaterCostStatics().HealthDef);
}

void UExecCalc_WaterCost::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

    // SetByCaller로 전달된 필요 Water 양 가져오기
    const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
    float RequiredWater = Spec.GetSetByCallerMagnitude(GameplayTags.Cost_Water, false, 0.f);

    if (RequiredWater <= 0.f)
    {
        return;
    }

    // Evaluation Parameters 설정
    const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

    FAggregatorEvaluateParameters EvaluationParameters;
    EvaluationParameters.SourceTags = SourceTags;
    EvaluationParameters.TargetTags = TargetTags;

    // 현재 Water와 Health 값 가져오기
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

    // Cost 계산
    float ActualWaterCost = 0.f;
    float HealthCost = 0.f;

    if (CurrentWater >= RequiredWater)
    {
        // Water가 충분한 경우: Water만 차감
        ActualWaterCost = RequiredWater;
        HealthCost = 0.f;
    }
    else
    {
        // Water가 부족한 경우: Water 전부 + Health 추가 차감
        ActualWaterCost = CurrentWater;
        float WaterShortage = RequiredWater - CurrentWater;
        HealthCost = FMath::FloorToFloat(WaterShortage * 0.5f);
    }

    // Water 차감 적용
    if (ActualWaterCost > 0.f)
    {
        FGameplayModifierEvaluatedData WaterData(
            UDRAttributeSet::GetWaterAttribute(),
            EGameplayModOp::Additive,
            -ActualWaterCost);
        OutExecutionOutput.AddOutputModifier(WaterData);
    }

    // Health 차감 적용 (필요한 경우만)
    if (HealthCost > 0.f)
    {
        FGameplayModifierEvaluatedData HealthData(
            UDRAttributeSet::GetHealthAttribute(),
            EGameplayModOp::Additive,
            -HealthCost);
        OutExecutionOutput.AddOutputModifier(HealthData);
    }
}
