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
// 목표 진행도(숫자) UI 표시/숨김 델리게이트 (Max==0이면 false로 브로드캐스트)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveProgressVisibilityChangedSignature, bool, bShouldShow);
// 목표 클리어 연출 델리게이트 (클리어 애니메이션 → 시작 애니메이션 시퀀스 트리거)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnObjectiveCompletedSignature);
// 디버프 변경시 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStatusEffectWidgetSignature, const FEffectInfo&, EffectInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FEffectTagRemovedSignature, FGameplayTag, EffectTag, bool, IsDebuff);
// 웨이브 타이머 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnWaveTimerChangedSignature, int32, WaveNumber, float, RemainingTime, bool, bIsRestTime);
// 웨이브 알림 델리게이트
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
// 업그레이드 칩에 따른 HUD 모듈 표시/숨김 (Plan3(공용칩) §7.2).
// 체력/물은 WBP_HealthWaterBar 한 위젯이라 하나의 플래그로 묶인다.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHudFeatureVisibilityChangedSignature, bool, bShowVitals, bool, bShowSkillIcons);
// 장착 칩이 최대 물을 늘렸는지 (물 HUD 테두리 교체용 — UDRHealthWaterBarWidget)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWaterTankExpandedChangedSignature, bool, bExpanded);

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

	// 델리게이트 언바인딩 함수
	void UnbindAllDelegates();

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

	// Max==0인 경우 숫자 표시(Progress 텍스트) 숨기기 위한 가시성 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Phase|Objective")
	FOnObjectiveProgressVisibilityChangedSignature OnObjectiveProgressVisibilityChanged;

	// 목표 클리어 시 1회 발화 (위젯: 클리어 애니 재생 → 종료 후 새 목표 텍스트 적용 + 시작 애니 재생)
	UPROPERTY(BlueprintAssignable, Category = "Phase|Objective")
	FOnObjectiveCompletedSignature OnObjectiveCompleted;

	// 디버프 변경 브로드캐스트 델리게이트
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Debuff")
	TObjectPtr<UStatusEffectInfo> StatusEffectData;
	
	UPROPERTY(BlueprintAssignable, Category = "GAS|Debuff")
	FStatusEffectWidgetSignature StatusEffectWidgetDelegate;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Debuff")
	FEffectTagRemovedSignature EffectTagRemovedDelegate;

	// 웨이브 타이머 업데이트 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Phase|Wave")
	FOnWaveTimerChangedSignature OnWaveTimerChanged;

	// 웨이브 알람 델리게이트
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

	/**
	 * ★Plan5(공속칩)★ 자판기 공속 버프 남은 시간 그래프의 채움 비율(0~1). 버프가 없으면 0.
	 * WBP_SkillIcon_VendingMachine 의 UpdateBuffUI 타이머가 매 틱 부른다 — 위젯이 20초를 직접 세지 않는다 (Plan5 D-76).
	 *  - 기본·칩 ②: 남은 시간 ÷ 20초 / 칩 ①: 남은 시간 ÷ 5초 (실제 걸린 GE 의 지속을 그대로 읽는다)
	 *  - 칩 ③: 유지 구간은 남은 시간 ÷ BuffDuration(20초), 1초씩 줄어드는 구간은 0 (그래프는 비고 숫자만 줄어든다)
	 * 자기 버프와 공유 버프가 같이 있으면 더 많이 남은 쪽을 보여 준다.
	 * GE 의 시작 시각은 서버가 복제하므로, 10중첩에서 다시 써서 스택 수가 안 바뀌어도 그래프가 다시 찬다.
	 */
	UFUNCTION(BlueprintPure, Category = "GAS|VendingMachine")
	float GetAttackSpeedBuffFillRatio() const;

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

	// ========== 업그레이드 칩 기능 스위치 (Plan3(공용칩) §7.2) ==========

	/**
	 * HUD 모듈 표시 여부. 칩 CM.Util.HudDisplay 를 빼면 체력·물·스킬 아이콘이 숨는다.
	 * 위젯은 이 값으로 Visibility 를 Visible / Collapsed 로만 토글한다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "GAS|Upgrade")
	FOnHudFeatureVisibilityChangedSignature OnHudFeatureVisibilityChanged;

	/**
	 * 현재 PlayerState 의 피처 태그를 읽어 방송한다.
	 * 위젯 최초 생성 시 1회 + 장착 칩이 바뀔 때마다 호출된다.
	 *
	 * ★PlayerState 를 읽는 이유★ (GameInstance 세이브가 아니라)
	 *  - 실제로 효과가 도는 것은 "서버가 승인해 복제한 EquippedChips" 이므로 그것이 SSOT 다.
	 *  - 관전 모드에서는 ★보고 있는 대상★ 의 상태를 따라가야 맞다.
	 */
	UFUNCTION()
	void RefreshHudFeatureVisibility();

	/**
	 * 물 HUD 테두리 교체 신호. 장착 칩이 최대 물을 늘리면(지금은 CM.Stat.MaxWater "확장 물탱크") true.
	 * 칩 Id 가 아니라 수치(MaxWater 의 Flat > 0 또는 Mult > 1)로 판정해 칩 이름·수치가 바뀌어도 따라간다.
	 * RefreshHudFeatureVisibility 와 같은 시점(최초 1회 + 장착 칩 변경)에 방송된다.
	 */
	UPROPERTY(BlueprintAssignable, Category = "GAS|Upgrade")
	FOnWaterTankExpandedChangedSignature OnWaterTankExpandedChanged;

	UFUNCTION(BlueprintPure, Category = "GAS|Upgrade")
	bool IsWaterTankExpanded();

	/**
	 * 오염 상태가 바뀌면 체력·최대 체력을 다시 보낸다.
	 * 체력 HUD 는 최대 체력이 바뀔 때만 오염 색을 다시 칠하는데, 기본 최대 체력이 100 인 청소기는
	 * 오염(최대 체력 100)에 들어가고 나와도 최대 체력이 그대로라 이벤트가 오지 않는다.
	 */
	UFUNCTION()
	void HandleCorruptedStateChanged(bool bIsCorrupted);

private:
	// State.Carrying 태그 변화 콜백 (부품 픽업/드롭/설치 시 조준선 상태 재계산)
	void HandleCarryingTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	// ★Plan5(공속칩)★ 공속 버프 스택이 늘거나 줄 때마다(칩 ③의 1초 감소 포함) 자판기 스킬 아이콘 숫자 갱신.
	// 적용·제거 델리게이트는 "스택만 줄어드는" 순간을 모른다 — 태그 AnyCountChange 로 받는다 (Plan5 D-75 · T-72)
	void HandleAttackSpeedBuffTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	// ★Plan5(공속칩)★ 칩 ③의 "1초씩 줄어드는 구간" 판별용 — 자기 버프 스택이 줄면 true, 늘거나 0 이 되면 false.
	// 남은 시간만으로는 유지 구간의 마지막 1초와 감소 구간을 구분할 수 없어 스택 흐름으로 본다 (Plan5 T-73)
	int32 LastSelfAttackSpeedStacks = 0;
	bool bAttackSpeedBuffDecaying = false;

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

	// 웨이브 방어 UI(타이머 + 클렌저 HP) 표시 플래그 변경 콜백 (Plan6 §5.4)
	// 페이즈 인덱스 하드코딩 대신 GameState의 복제 플래그를 구독한다.
	UFUNCTION()
	void HandleWaveDefenseUIActiveChanged(bool bIsActive);

	// 목표 데이터에 PhaseAlarmText가 없을 때 사용하는 레거시 문구 (스테이지1 호환)
	FText GetLegacyPhaseAlarmText(int32 PhaseIndex) const;
	

	// GameState 델리게이트 묶기 (목표 · 페이즈 알람 · 독가스 경고). GameState 가 아직 없으면 재시도한다.
	void TryBindGameStateDelegates();
	FTimerHandle GameStateBindRetryTimer;
	bool bGameStateDelegatesBound = false;
	static constexpr float GameStateBindRetryInterval = 0.1f;

	// Buff.* / Debuff.* 상태 아이콘 태그인지 (디버프면 bOutIsDebuff = true)
	static bool IsStatusEffectTag(const FGameplayTag& Tag, bool& bOutIsDebuff);

	// 값 변화 델리게이트를 묶은 어트리뷰트 (해제용)
	TArray<FGameplayAttribute> BoundAttributes;
	
	// Phase 알람용 캐시 (Phase 변경 시 알람 표시 여부 판단)
	int32 CachedPhaseNumber = -1;

	// 웨이브 타이머용 캐시 (Phase 3 웨이브 타이머 바인딩 여부 판단)
	int32 CachedWavePhaseNumber = -1;

	// 페이즈 알람 중복 방지용 플래그
	bool bPhaseAlarmShown = false;

	// 페이즈가 바뀌었으나 아직 새 목표 데이터가 도착하지 않은 상태 (Plan6 §5.4)
	// GameMode가 페이즈 인덱스를 먼저 복제하고 목표는 그 뒤에 설정하므로,
	// 알람 문구 결정은 목표 데이터가 도착한 시점으로 미룬다.
	bool bPhaseAlarmPending = false;

	// 같은 문구를 연속으로 다시 띄우지 않기 위한 캐시
	FText CachedAlarmText;
};
