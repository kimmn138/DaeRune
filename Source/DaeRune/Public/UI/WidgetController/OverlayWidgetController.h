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
// 목표 진행도(숫자) UI 표시/숨김 델리게이트 (Max==0이면 false로 브로드캐스트)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveProgressVisibilityChangedSignature, bool, bShouldShow);
// 목표 클리어 연출 델리게이트 (클리어 애니메이션 → 시작 애니메이션 시퀀스 트리거)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnObjectiveCompletedSignature);
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
// 로봇 청소기 일반 공격(공기탄) 충전 게이지 변경 델리게이트 (3칸, 0 = 빈 게이지)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRobotVacuumAirShotGaugeChangedSignature, int32, CurrentGauge, int32, MaxGauge);
// 로봇 청소기 돌진 게이지 변경 델리게이트 (5칸, 5칸 = 지속 돌진)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRobotVacuumDashGaugeChangedSignature, int32, CurrentGauge, int32, MaxGauge);
// 스킬 입력 키 Press/Release UI 피드백용 델리게이트 (WBP가 자기 InputTag와 비교해 자기 차례만 반응)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityInputPressedSignature,  FGameplayTag, InputTag);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAbilityInputReleasedSignature, FGameplayTag, InputTag);
// 스킬 슬롯 차단 상태 재평가 트리거 (각 슬롯이 자기 AbilityTag 기준으로 IsAbilityBlockedNow 호출)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAbilityBlockStateDirtySignature);

// 조준선 상태 (상태별 텍스처는 OnCrosshairImagesChanged 로 전달)
UENUM(BlueprintType)
enum class EDRCrosshairState : uint8
{
	Normal,			// 기본 (캐릭터 전용 조준선)
	Disabled,		// 부품 운반 중 - 공격 불가 (전 캐릭터 공통 이미지)
	InstallReady	// 부품 운반 중 + 설치 가능한 사이트 조준 중 (캐릭터 전용 이미지)
};

// 조준선 텍스처 세트 전달 델리게이트 (캐릭터 클래스 확정/변경 시 1회 브로드캐스트)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FOnCrosshairImagesChangedSignature, UTexture2D*, NormalTexture, UTexture2D*, DisabledTexture, UTexture2D*, InstallReadyTexture, FVector2D, CrosshairSize);
// 조준선 상태 변경 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCrosshairStateChangedSignature, EDRCrosshairState, NewState);
// 적 타격 확인 델리게이트 (위젯이 히트마커 이미지 표시 + 애니메이션 재생)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCrosshairHitConfirmedSignature);

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

	// Max==0인 경우 숫자 표시(Progress 텍스트) 숨기기 위한 가시성 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Phase|Objective")
	FOnObjectiveProgressVisibilityChangedSignature OnObjectiveProgressVisibilityChanged;

	// 목표 클리어 시 1회 발화 (위젯: 클리어 애니 재생 → 종료 후 새 목표 텍스트 적용 + 시작 애니 재생)
	UPROPERTY(BlueprintAssignable, Category = "Phase|Objective")
	FOnObjectiveCompletedSignature OnObjectiveCompleted;

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

	// 로봇 청소기 일반 공격(공기탄) 충전 게이지 (3칸) — LMB 스킬 아이콘 게이지 표시용
	UPROPERTY(BlueprintAssignable, Category = "GAS|RobotVacuum")
	FOnRobotVacuumAirShotGaugeChangedSignature OnRobotVacuumAirShotGaugeChanged;

	// 로봇 청소기 돌진 게이지 (5칸) — RMB 스킬 아이콘 게이지 표시용
	UPROPERTY(BlueprintAssignable, Category = "GAS|RobotVacuum")
	FOnRobotVacuumDashGaugeChangedSignature OnRobotVacuumDashGaugeChanged;

	// 스킬 버튼이 눌렸을 때 (InputTag 전달, WBP가 자기 것과 비교)
	UPROPERTY(BlueprintAssignable, Category = "GAS|SkillIcon")
	FOnAbilityInputPressedSignature OnAbilityInputPressed;

	// 스킬 버튼이 떼어졌을 때
	UPROPERTY(BlueprintAssignable, Category = "GAS|SkillIcon")
	FOnAbilityInputReleasedSignature OnAbilityInputReleased;

	// 스킬 슬롯 차단 상태가 변했을 가능성을 알리는 신호 (WBP_SkillSlot 이 자기 AbilityTag 로 재평가)
	UPROPERTY(BlueprintAssignable, Category = "GAS|SkillIcon")
	FOnAbilityBlockStateDirtySignature OnAbilityBlockStateDirty;

	// 슬롯이 호출: "지금 이 AbilityTag 가 차단 상태인가?" (State.Carrying 중에는 무조건 true)
	UFUNCTION(BlueprintPure, Category = "GAS|SkillIcon")
	bool IsAbilityBlockedNow(FGameplayTag AbilityTag) const;

	// 캐릭터 클래스에 맞는 스킬아이콘 위젯 클래스를 브로드캐스트
	void BroadcastSkillIconWidgetClass();

	// ========== 조준선 ==========

	// 조준선 텍스처 세트 (기본=캐릭터 전용, 비활성=공통, 설치 가능=캐릭터 전용)
	UPROPERTY(BlueprintAssignable, Category = "UI|Crosshair")
	FOnCrosshairImagesChangedSignature OnCrosshairImagesChanged;

	// 조준선 상태 변경 (Normal / Disabled / InstallReady)
	UPROPERTY(BlueprintAssignable, Category = "UI|Crosshair")
	FOnCrosshairStateChangedSignature OnCrosshairStateChanged;

	// 적 타격 확인 (위젯: 히트마커 표시 + 애니메이션 재생)
	UPROPERTY(BlueprintAssignable, Category = "UI|Crosshair")
	FOnCrosshairHitConfirmedSignature OnCrosshairHitConfirmed;

	// 캐릭터 클래스에 맞는 조준선 텍스처 세트를 브로드캐스트 (HUD 초기화 시 호출)
	void BroadcastCrosshairImages();

	// 적 타격 확인 알림 (PlayerController 의 데미지 확인 Client RPC 에서 호출)
	void NotifyCrosshairEnemyHit();

	// 설치 가능 사이트 라인트레이스 감지 변화 알림 (PlayerController 로컬 감지에서 호출)
	void SetCrosshairSiteDetected(bool bDetected);

private:
	// State.Carrying 태그 변화 콜백 (부품 픽업/드롭/설치 시 조준선 상태 재계산)
	void HandleCarryingTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	// 현재 조건으로 조준선 상태 재계산 후 변경 시에만 브로드캐스트
	void UpdateCrosshairState(bool bForceBroadcast = false);

	// 부품 운반 중 설치 가능한 사이트를 조준하고 있는지 (로컬 라인트레이스 결과)
	bool bCrosshairSiteDetected = false;

	// 마지막으로 브로드캐스트한 조준선 상태 (중복 방송 방지)
	EDRCrosshairState CachedCrosshairState = EDRCrosshairState::Normal;

	// 차단 카운터 변화 콜백 → OnAbilityBlockStateDirty 재방송
	void HandleBlockedTagsChanged();

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
	FDelegateHandle ObjectiveCompletedDelegateHandle;
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
