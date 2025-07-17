// Copyright DaeRune


#include "AbilitySystem/ExecCalc/ExecCalc_Damage.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"

UExecCalc_Damage::UExecCalc_Damage()
{
}

// 디버프 결정 로직 구현
void UExecCalc_Damage::DetermineDebuff(const FGameplayEffectCustomExecutionParameters& ExecutionParams, const FGameplayEffectSpec& Spec) const
{
	// 게임플레이 태그 참조
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	// 데미지 타입별 디버프 매핑 순회
	for (TTuple<FGameplayTag, FGameplayTag> Pair : GameplayTags.DamageTypesToDebuffs)
	{
		// 원본 데미지 타입 태그 추출
		const FGameplayTag& DamageType = Pair.Key;
		// 대응 디버프 타입 태그 추출
		const FGameplayTag& DebuffType = Pair.Value;
		// 설정된 데미지 크기 추출
		const float TypeDamage = Spec.GetSetByCallerMagnitude(DamageType, false, -1.f);
		// 데미지 유효성 확인
		if (TypeDamage > -.5f) // .5 padding for floating point [im]precision
		{
			// 디버프 확률 크기 추출
			const float SourceDebuffChance = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Chance, false, -1.f);

			// 랜덤 범위 비교 처리
			const bool bDebuff = FMath::RandRange(1, 100) <= SourceDebuffChance;
			// 디버프 성공 시 처리
			if (bDebuff)
			{
				// 효과 컨텍스트 핸들 참조
				FGameplayEffectContextHandle ContextHandle = Spec.GetContext(); 

				// 디버프 성공 플래그 설정
				UDRAbilitySystemLibrary::SetIsSuccessfulDebuff(ContextHandle, true);
				// 데미지 타입 컨텍스트 설정
				UDRAbilitySystemLibrary::SetDamageType(ContextHandle, DamageType);

				// 디버프 속성 크기 추출
				const float DebuffDamage = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Damage, false, -1.f);
				const float DebuffDuration = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Duration, false, -1.f);
				const float DebuffFrequency = Spec.GetSetByCallerMagnitude(GameplayTags.Debuff_Frequency, false, -1.f);

				// 디버프 속성 컨텍스트 저장
				UDRAbilitySystemLibrary::SetDebuffDamage(ContextHandle, DebuffDamage);
				UDRAbilitySystemLibrary::SetDebuffDuration(ContextHandle, DebuffDuration);
				UDRAbilitySystemLibrary::SetDebuffFrequency(ContextHandle, DebuffFrequency);
			}
		}
	}
}

// 커스텀 실행 구현 정의
void UExecCalc_Damage::Execute_Implementation(const FGameplayEffectCustomExecutionParameters& ExecutionParams, FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{
	// 실행 스펙 참조
	const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();

	// 소스 태그 집합 참조
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	// 타겟 태그 집합 참조
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();
	// 평가 파라미터 초기화
	FAggregatorEvaluateParameters EvaluationParameters;
	// 평가 파라미터 소스 태그 할당
	EvaluationParameters.SourceTags = SourceTags;
	// 평가 파라미터 타겟 태그 할당
	EvaluationParameters.TargetTags = TargetTags;

	// 디버프 결정 로직 실행
	DetermineDebuff(ExecutionParams, Spec);

	// 총 데미지 합산 초기화
	float Damage = 0.0f;
	// 데미지 타입 태그 순회
	for (const FGameplayTag& DamageTypeTag : FDRGameplayTags::Get().DamageTypeTags)
	{
		// 개별 데미지 크기 추출
		float DamageTypeValue = Spec.GetSetByCallerMagnitude(DamageTypeTag, false);
		// 데미지 누적 처리
		Damage += DamageTypeValue;
	}

	// 수정자 데이터 생성
	const FGameplayModifierEvaluatedData EvaluatedData(UDRAttributeSet::GetIncomingDamageAttribute(), EGameplayModOp::Additive, Damage);
	// 출력 수정자 추가
	OutExecutionOutput.AddOutputModifier(EvaluatedData);
}
