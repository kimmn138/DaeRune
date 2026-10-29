// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRUpgradeTypes.h"
#include "DRPlayerState.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayEffect;

// 전투 상태 변경 알림 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChangedSignature, bool, bIsInCombat);
// 오염 상태 변경 알림 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCorruptedStateChangedSignature, bool, bIsCorrupted);
// 캐릭터 클래스 변경 알림 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerClassChanged, ADRPlayerState*, PlayerState, EPlayerCharacterClass, NewClass);
// 준비 상태 변경 알림 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReadyStateChangedSignature, bool, bIsReady);
// 장착 칩 수치 캐시 재구축 알림 (UI 가 피처 태그를 다시 읽어야 한다 — Plan3(공용칩) §7.2)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpgradeRuntimeChangedSignature);

/**
 * DaeRune 플레이어의 게임 상태 관리 클래스
 */
UCLASS()
class DAERUNE_API ADRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ADRPlayerState();
	// GAS 인터페이스 구현
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// 네트워크 리플리케이션 설정
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 전투 시스템
	void EnterCombat();
	void ExitCombat();
	bool IsInCombat() const { return bIsInCombat; }

	/**
	 * ★Plan3(공용칩)★ 업그레이드가 반영된 전투 이탈 대기 시간(초).
	 * 이 시간이 지나야 자동 체력 회복이 시작된다.
	 *
	 * ★타이머를 거는 시점에 계산한다.★ BeginPlay 에서 캐시하면 칩 목록이 그보다
	 * 늦게 도착하는 경우(스폰 후 로드아웃 보고)에 반영되지 않는다. (§7.5.2)
	 */
	UFUNCTION(BlueprintPure, Category = "Combat")
	float GetEffectiveCombatExitDelay() const;

	// 컨테이너 시스템 연동
	int32 GetCurrentContainerIndex() const;

	// UI 및 게임플레이 알림 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnCombatStateChangedSignature OnCombatStateChanged;

	UPROPERTY(BlueprintAssignable)
	FOnCorruptedStateChangedSignature OnCorruptedStateChanged;

	// 오염 상태 시스템
	void SetCorruptedState(bool bNewCorrupted);
	bool IsPlayerCorrupted() const;

	// ========== 대기실 슬롯 ==========

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	int32 GetWaitingRoomSlotIndex() const { return WaitingRoomSlotIndex; }

	void SetWaitingRoomSlotIndex(int32 NewIndex);

	// ========== 호스트 여부 ==========

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsHost() const { return bIsHost; }

	// 호스트 여부 설정 (서버 전용)
	void SetIsHost(bool bNewIsHost);

	// ========== 준비 상태 (대기실) ==========

	UFUNCTION(BlueprintPure, Category = "Lobby")
	bool IsReady() const { return bIsReady; }

	// 준비 상태 설정 (서버 전용)
	void SetReady(bool bNewReady);

	// 준비 상태 변경 알림 (UI 바인딩용)
	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnReadyStateChangedSignature OnReadyStateChanged;

	// ========== 캐릭터 클래스 선택 ==========

	UFUNCTION(BlueprintCallable, Category = "Character Selection")
	EPlayerCharacterClass GetSelectedPlayerClass() const { return SelectedPlayerClass; }

	void SetSelectedPlayerClass(EPlayerCharacterClass NewClass);

	UPROPERTY(BlueprintAssignable, Category = "Character Selection")
	FOnPlayerClassChanged OnPlayerClassChanged;

	// ========== 업그레이드 칩 (Plan2.md 7.1) ==========

	// 이 플레이어가 장착한 칩 Id 목록 (클라이언트가 신고 → 서버가 정화 후 복제)
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	const TArray<FName>& GetEquippedChips() const { return EquippedChips; }

	// 장착 칩을 펼친 수치 캐시. 복제하지 않고 각 머신이 로컬로 구축한다.
	// 비어 있으면 항등이므로 호출부는 칩 유무를 신경 쓰지 않아도 된다.
	const FDRUpgradeRuntime& GetUpgradeRuntime() const { return CachedUpgradeRuntime; }

	// 서버 전용: 정화된 장착 목록을 싣고 캐시를 재구축한다.
	void SetEquippedChips(const TArray<FName>& InChips);

	// 복제된 EquippedChips + 선택 클래스로 수치 캐시를 다시 만들고, 폰에 갱신을 통지한다.
	// 장착이 그대로여도 칩 수치(카탈로그)가 바뀌면 다시 불러야 한다 — 튜닝 파일(UDRTuningSubsystem)이 그렇게 쓴다.
	void RebuildUpgradeRuntime();

	/**
	 * ★Plan3(공용칩)★ 장착 칩이 부여한 기능 스위치 조회.
	 * UI 가 FDRUpgradeRuntime 내부를 직접 뒤지지 않게 하는 단일 경로다.
	 */
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	bool HasUpgradeFeature(FGameplayTag FeatureTag) const
	{
		return CachedUpgradeRuntime.HasFeature(FeatureTag);
	}

	// 수치 캐시가 다시 만들어졌다 (최초 복제 / 로드아웃 변경 / Seamless Travel).
	UPROPERTY(BlueprintAssignable, Category = "Upgrade")
	FOnUpgradeRuntimeChangedSignature OnUpgradeRuntimeChanged;

	// ========== 코스메틱 (Plan(옷장) M3 · D-26) ==========

	// 이 플레이어가 입고 있는 옷 (인덱스 = EDRCosmeticCategory). 클라이언트 신고 → 서버 정화 → 복제 — 칩과 같은 경로
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	const TArray<FName>& GetEquippedSkinIds() const { return EquippedSkinIds; }

	// 서버 전용: 정화된 목록(길이 = 카테고리 수)을 싣고 폰에 바로 입힌다
	void SetEquippedSkinIds(const TArray<FName>& InSkinIds);

	// ========== 스테이지 처치 수 (업적 판정 / 결과창 표시) ==========

	UFUNCTION(BlueprintPure, Category = "Stage")
	int32 GetStageKillCount() const { return StageKillCount; }

	// 서버 전용. Seamless Travel 시 새 PlayerState 가 0에서 시작하므로 별도 리셋 경로는 두지 않는다.
	void AddStageKill();

protected:
	virtual void BeginPlay() override;

	// Seamless Travel 시 커스텀 프로퍼티 복사
	virtual void CopyProperties(APlayerState* PlayerState) override;

	// GAS 컴포넌트들
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UAttributeSet> AttributeSet;

	// 네트워크 동기화 변수들
	UPROPERTY(ReplicatedUsing = OnRep_IsInCombat)
	bool bIsInCombat = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsCorrupted)
	bool bIsCorrupted = false;

	// 대기실 슬롯 인덱스
	UPROPERTY(ReplicatedUsing = OnRep_WaitingRoomSlotIndex, BlueprintReadOnly, Category = "Lobby")
	int32 WaitingRoomSlotIndex = -1;

	// 준비 상태 (대기실, 클라이언트가 토글)
	UPROPERTY(ReplicatedUsing = OnRep_IsReady, BlueprintReadOnly, Category = "Lobby")
	bool bIsReady = false;

	// 호스트 여부 (서버가 접속 시 확정, PlayerId 순서에 의존하지 않음)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Lobby")
	bool bIsHost = false;

	// 캐릭터 클래스 선택
	UPROPERTY(ReplicatedUsing = OnRep_SelectedPlayerClass, BlueprintReadOnly, Category = "Character Selection")
	EPlayerCharacterClass SelectedPlayerClass = EPlayerCharacterClass::Gardener;

	// 장착 칩 Id 목록 (서버가 클라 신고를 정화해 싣는다)
	UPROPERTY(ReplicatedUsing = OnRep_EquippedChips, BlueprintReadOnly, Category = "Upgrade")
	TArray<FName> EquippedChips;

	// 입고 있는 옷 (인덱스 = EDRCosmeticCategory). 남에게 필요한 건 "지금 입은 4벌"뿐이라 이것만 복제한다 (Plan(옷장) D-6)
	UPROPERTY(ReplicatedUsing = OnRep_EquippedSkinIds, BlueprintReadOnly, Category = "Cosmetic")
	TArray<FName> EquippedSkinIds;

	// 이번 스테이지 처치 수 (스테이지 한정 값 — Seamless Travel 시 복사하지 않는다)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stage")
	int32 StageKillCount = 0;

	// 리플리케이션 콜백
	UFUNCTION()
	void OnRep_IsInCombat();

	UFUNCTION()
	void OnRep_EquippedChips();

	UFUNCTION()
	void OnRep_EquippedSkinIds();

	UFUNCTION()
	void OnRep_IsCorrupted();

	UFUNCTION()
	void OnRep_WaitingRoomSlotIndex();

	UFUNCTION()
	void OnRep_IsReady();

	UFUNCTION()
	void OnRep_SelectedPlayerClass();

	// 로컬 플레이어의 대기실 UI 갱신 (대기실 관련 OnRep 공용 헬퍼)
	void RefreshLocalWaitingRoomUI() const;

	// 소유 클라이언트에서만: 현재 선택 클래스의 장착 목록을 서버에 보고
	void ReportUpgradeLoadoutIfLocal();

	// 이 머신에서 나를 PlayerState 로 가진 로컬 컨트롤러 (없으면 null)
	class ADRPlayerController* FindLocalOwningController() const;

	// 입고 있는 옷을 폰에 반영한다. 폰이 아직 없으면 아무것도 안 한다 —
	// 폰이 나중에 오면 ADRCharacter::PossessedBy / OnRep_PlayerState 가 여기서 끌어간다 (Plan(옷장) M3)
	void RefreshCosmeticVisuals();

	// 이 머신의 로컬 컨트롤러가 나를 소유하면: 현재 선택 클래스의 옷 목록을 서버에 보고
	void ReportCosmeticIfLocal();

private:
	// 장착 칩을 펼친 수치 캐시 (복제 안 함 — 서버/각 클라가 각자 구축)
	FDRUpgradeRuntime CachedUpgradeRuntime;

	// 전투 상태 타이머 관리
	FTimerHandle CombatTimerHandle;

	// 전투 종료 대기 시간
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float CombatExitDelay = 5.0f;

	// 업그레이드로 줄어든 전투 이탈 대기의 하한. 0에 가까우면 "전투 중"이 의미를 잃는다.
	UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.1"))
	float MinCombatExitDelay = 1.0f;

	// 체력 회복 시스템
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects")
	TSubclassOf<UGameplayEffect> HealthRegenEffectClass;

	FActiveGameplayEffectHandle HealthRegenEffectHandle;

	void StartHealthRegen();
	void StopHealthRegen();
	void CheckCombatExit();
	void CheckAndStartHealthRegen();
	void CheckHealthRegenStatus();

	// 체력 변경 감지
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	// 최적화를 위한 캐싱
	FGameplayEffectSpecHandle CachedHealthRegenSpec;
	void InitializeHealthRegenSpec();
};
