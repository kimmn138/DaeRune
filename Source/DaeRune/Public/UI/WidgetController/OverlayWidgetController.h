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
// Phase3 타이머 UI 표시/숨김 델리게이트 (Phase 3 진입 시 true, 그 외 false)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPhase3TimerVisibilitySignature, bool, bShouldShow);
// 독가스 경고 UI 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToxicGasWarningUISignature, bool, bIsToxicGasWave);
// 스킬아이콘 위젯 클래스 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkillIconClassChangedSignature, TSubclassOf<UDRUserWidget>, SkillIconWidgetClass);
// 자판기 잭팟 스택 카운트 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnVendingMachineStackCountChangedSignature, int32, CurrentStacks, int32, MaxStacks);
// 자판기 공격속도 버프 스택 변경 델리게이트 (스킬 아이콘 스택 표시용, 만료 시 0 브로드캐스트)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnVendingMachineAttackSpeedBuffSignature, int32, StackCount);
// 스킬 입력 키 Press/Release UI 피드백용 델리게이트 (WBP가 자기 InputTag와 비교해 자기 차례만 반응)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityInputPressedSignature,  FGameplayTag, InputTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityInputReleasedSignature, FGameplayTag, InputTag);

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

	// Phase3 타이머 UI 표시/숨김 델리게이트 (WBP_Phase3Timer에서 바인딩)
	UPROPERTY(BlueprintAssignable, Category = "Phase|Wave")
	FOnPhase3TimerVisibilitySignature OnPhase3TimerVisibilityChanged;

	// 독가스 경고 UI 델리게이트 (Blueprint에서 바인딩하여 경고 위젯 표시/숨김)
	UPROPERTY(BlueprintAssignable, Category = "Phase|Warning")
	FOnToxicGasWarningUISignature OnToxicGasWarning;

	// 스킬아이콘 위젯 클래스 변경 델리게이트 (캐릭터별 스킬아이콘 위젯 교체용)
	UPROPERTY(BlueprintAssignable, Category = "GAS|SkillIcon")
	FOnSkillIconClassChangedSignature OnSkillIconClassChanged;

	// 자판기 잭팟 스택 카운트 변경 델리게이트 (잭팟 스택 UI 업데이트용)
	UPROPERTY(BlueprintAssignable, Category = "GAS|VendingMachine")
	FOnVendingMachineStackCountChangedSignature OnVendingMachineStackCountChanged;

	// 자판기 공격속도 버프 스택 변경 델리게이트 (스킬 아이콘 스택 표시용, 만료 시 0)
	UPROPERTY(BlueprintAssignable, Category = "GAS|VendingMachine")
	FOnVendingMachineAttackSpeedBuffSignature OnVendingMachineAttackSpeedBuff;

	// 스킬 버튼이 눌렸을 때 (InputTag 전달, WBP가 자기 것과 비교)
	UPROPERTY(BlueprintAssignable, Category = "GAS|SkillIcon")
	FOnAbilityInputPressedSignature OnAbilityInputPressed;

	// 스킬 버튼이 떼어졌을 때
	UPROPERTY(BlueprintAssignable, Category = "GAS|SkillIcon")
	FOnAbilityInputReleasedSignature OnAbilityInputReleased;

	// 캐릭터 클래스에 맞는 스킬아이콘 위젯 클래스를 브로드캐스트
	void BroadcastSkillIconWidgetClass();

private:
	void HandlePhaseObjectiveChanged();
	void BindPhaseObjectiveDelegate();
	void BindWaveTimerDelegate();
	void BindPhaseAlarmDelegate();
	void BindToxicGasWarningDelegate();
	void CheckAndBindWaveTimer();
	UFUNCTION()
	void OnPhaseChanged(int32 NewPhaseIndex);
	UFUNCTION()
	void OnToxicGasWarningReceived(bool bIsToxicGasWave);
	
	// Ŭ���� ����Ʈ ASC/AttributeSet ���ε� �Լ�
	UFUNCTION()
	void BindCleanserSite(ADRCleanserSite* FirstCleanserSite, ADRCleanserSite* SecondCleanserSite);

	FTimerHandle PhaseBindingDelayTimer;
	FTimerHandle WaveTimerBindingDelayTimer;
	FTimerHandle PhaseAlarmBindingDelayTimer;
	FTimerHandle ToxicGasWarningBindingDelayTimer;

	// ��������Ʈ �ڵ� ����� ����
	FDelegateHandle PhaseObjectiveDelegateHandle;
	FDelegateHandle WaveTimerDelegateHandle;

	// ASC 델리게이트 핸들 (안전한 정리를 위해)
	TArray<FDelegateHandle> ASCDelegateHandles;
	TArray<TPair<TWeakObjectPtr<UAbilitySystemComponent>, FDelegateHandle>> CleanserSiteDelegateHandles;
	
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
