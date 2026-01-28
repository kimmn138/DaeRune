// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/WidgetController/DRWidgetController.h"
#include "OverlayWidgetController.generated.h"

// UI �޽��� ǥ�ø� ���� ������ ���̺� ����ü
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
class ADRCleanserSite;
class UDRCleanserSiteAttributeSet;

// ��Ʈ����Ʈ ���� �� UI ������Ʈ�� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeChangedSignature, float, NewValue);
// ��ǥ UI ������Ʈ�� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveTextChangedSignature, const FText&, ObjectiveTitle, const FText&, ProgressText);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveProgressChangedSignature, int32, Current, int32, Max);
// ����� ����� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStatusEffectWidgetSignature, const FEffectInfo&, EffectInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEffectTagRemovedSignature, FGameplayTag, EffectTag, bool, IsDebuff);
// ���̺� Ÿ�̸� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWaveTimerChangedSignature, int32, WaveNumber, float, RemainingTime, bool, bIsRestTime);
// ���̺� �˸� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhaseAlarmSignature, const FText&, PhaseText);

/**
 * ���� ���� UI �������̸� �����ϴ� ��Ʈ�ѷ�
 */
UCLASS(BlueprintType, Blueprintable)
class DAERUNE_API UOverlayWidgetController : public UDRWidgetController
{
	GENERATED_BODY()
	
public:
	// �θ� Ŭ���� ���� �Լ� �������̵�
	virtual void BroadcastInitialValues() override;
	virtual void BindCallbacksToDependencies() override;
	void BindCallbacksCleanserSiteToDependencies();

	// ��������Ʈ ����ε� �Լ�
	void UnbindAllDelegates();

	// ü�� ���� UI ������Ʈ ��������Ʈ
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnMaxHealthChanged;

	// �� ���� UI ������Ʈ ��������Ʈ
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnWaterChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnMaxWaterChanged;

	// Ŭ��������Ʈ ü�� ���� UI ������Ʈ ��������Ʈ
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnFirstCleanserHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnFirstCleanserMaxHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnSecondCleanserHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature  OnSecondCleanserMaxHealthChanged;

	// ��ǥ UI ������Ʈ ��������Ʈ
	UPROPERTY(BlueprintAssignable, Category = "Phase|Objective")
	FOnObjectiveTextChangedSignature OnObjectiveTextChanged;

	UPROPERTY(BlueprintAssignable, Category = "Phase|Objective")
	FOnObjectiveProgressChangedSignature OnObjectiveProgressChanged;

	// ����� ���� ��ε�ĳ��Ʈ ��������Ʈ
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Debuff")
	TObjectPtr<UStatusEffectInfo> StatusEffectData;
	
	UPROPERTY(BlueprintAssignable, Category = "GAS|Debuff")
	FStatusEffectWidgetSignature StatusEffectWidgetDelegate;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Debuff")
	FEffectTagRemovedSignature EffectTagRemovedDelegate;

	// ���̺� Ÿ�̸� ������Ʈ ��������Ʈ
	UPROPERTY(BlueprintAssignable, Category = "Phase|Wave")
	FOnWaveTimerChangedSignature OnWaveTimerChanged;

	// ���̺� �˶� ��������Ʈ
	UPROPERTY(BlueprintAssignable, Category = "Phase|Alarm")
	FOnPhaseAlarmSignature OnPhaseAlarm;

private:
	void HandlePhaseObjectiveChanged();
	void BindPhaseObjectiveDelegate();
	void BindWaveTimerDelegate();
	void BindPhaseAlarmDelegate();
	void CheckAndBindWaveTimer();
	UFUNCTION()
	void OnPhaseChanged(int32 NewPhaseIndex);
	
	// Ŭ���� ����Ʈ ASC/AttributeSet ���ε� �Լ�
	UFUNCTION()
	void BindCleanserSite(ADRCleanserSite* FirstCleanserSite, ADRCleanserSite* SecondCleanserSite);

	FTimerHandle PhaseBindingDelayTimer;
	FTimerHandle WaveTimerBindingDelayTimer;
	FTimerHandle PhaseAlarmBindingDelayTimer;

	// ��������Ʈ �ڵ� ����� ����
	FDelegateHandle PhaseObjectiveDelegateHandle;
	FDelegateHandle WaveTimerDelegateHandle;
	
	// Phase 알람용 캐시 (Phase 변경 시 알람 표시 여부 판단)
	int32 CachedPhaseNumber = -1;

	// 웨이브 타이머용 캐시 (Phase 3 웨이브 타이머 바인딩 여부 판단)
	int32 CachedWavePhaseNumber = -1;

	int32 CachedProgress = -1;
	int32 CachedRequiredCount = -1;
	FText CachedObjectiveTitle;

	// 페이즈 알람 중복 방지용 플래그
	bool bPhaseAlarmShown = false;
};
