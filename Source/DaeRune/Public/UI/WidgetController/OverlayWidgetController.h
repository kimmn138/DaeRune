// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/WidgetController/DRWidgetController.h"
#include "OverlayWidgetController.generated.h"

// UI 메시지 표시를 위한 데이터 테이블 구조체
struct FDRAbilityInfo;
USTRUCT(BlueprintType)
struct FUIWidgetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag MessageTag = FGameplayTag();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Message = FText();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<class UDRUserWidget> MessageWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	UTexture2D* Image = nullptr;
};

class UDRUserWidget;
class UAbilityInfo;
class UDRAbilitySystemComponent;
class UStatusEffectInfo;

// 어트리뷰트 변경 시 UI 업데이트용 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeChangedSignature, float, NewValue);
// 목표 UI 업데이트용 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveTextChangedSignature, const FText&, ObjectiveTitle, const FText&, ProgressText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveProgressChangedSignature, int32, Current, int32, Max);
// 디버프 변경시 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStatusEffectWidgetSignature, const FEffectInfo&, EffectInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEffectTagRemovedSignature, FGameplayTag, EffectTag, bool, IsDebuff);

/**
 * 메인 게임 UI 오버레이를 관리하는 컨트롤러
 */
UCLASS(BlueprintType, Blueprintable)
class DAERUNE_API UOverlayWidgetController : public UDRWidgetController
{
	GENERATED_BODY()
	
public:
	// 부모 클래스 가상 함수 오버라이드
	virtual void BroadcastInitialValues() override;
	virtual void BindCallbacksToDependencies() override;

	// 체력 관련 UI 업데이트 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnMaxHealthChanged;

	// 물 관련 UI 업데이트 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnWaterChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnMaxWaterChanged;

	// 목표 UI 업데이트 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Phase|Objective")
	FOnObjectiveTextChangedSignature OnObjectiveTextChanged;

	UPROPERTY(BlueprintAssignable, Category = "Phase|Objective")
	FOnObjectiveProgressChangedSignature OnObjectiveProgressChanged;

	// 디버프 변경 브로드캐스트 델리게이트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Debuff")
	TObjectPtr<UStatusEffectInfo> StatusEffectData;
	
	UPROPERTY(BlueprintAssignable, Category = "GAS|Debuff")
	FStatusEffectWidgetSignature StatusEffectWidgetDelegate;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Debuff")
	FEffectTagRemovedSignature EffectTagRemovedDelegate;

private:
	void HandlePhaseObjectiveChanged();
	
	int32 CachedPhaseNumber = -1;
	int32 CachedProgress = -1;
	int32 CachedRequiredCount = -1;
	FText CachedObjectiveTitle;
};
