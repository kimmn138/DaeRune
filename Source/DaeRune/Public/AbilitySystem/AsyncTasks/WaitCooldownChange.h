// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "WaitCooldownChange.generated.h"

class UAbilitySystemComponent; 
struct FGameplayEffectSpec;
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCooldownChangeSignature, float, TimeRemaining);

/**
 * 
 */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = "AsyncTask"))
class DAERUNE_API UWaitCooldownChange : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	
public:
	// 쿨다운 시작 알림 델리게이트
	UPROPERTY(BlueprintAssignable)
	FCooldownChangeSignature CooldownStart;

	// 쿨다운 종료 알림 델리게이트
	UPROPERTY(BlueprintAssignable)
	FCooldownChangeSignature CooldownEnd;

	// 쿨다운 변경 대기 작업 생성 함수
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UWaitCooldownChange* WaitForCooldownChange(UAbilitySystemComponent* AbilitySystemComponent, const FGameplayTag& InCooldownTag);

	// 작업 종료 함수
	UFUNCTION(BlueprintCallable)
	void EndTask();

protected:
	// 어빌리티 시스템 컴포넌트 참조 변수
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ASC;

	// 감시할 쿨다운 태그 변수
	FGameplayTag CooldownTag;

	// 쿨다운 태그 변경 핸들러 함수
	void CooldownTagChanged(const FGameplayTag InCooldownTag, int32 NewCount);
	// 액티브 이펙트 추가 시 호출되는 핸들러 함수
	void OnActiveEffectAdded(UAbilitySystemComponent* TargetASC, const FGameplayEffectSpec& SpecApplied, FActiveGameplayEffectHandle ActiveEffectHandle);
};
