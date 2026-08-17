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

/**
 * DaeRune �÷��̾��� ���� ���� ���� Ŭ����
 */
UCLASS()
class DAERUNE_API ADRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ADRPlayerState();
	// GAS �������̽� ����
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// ��Ʈ��ũ ���ø����̼� ����
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ���� �ý���
	void EnterCombat();
	void ExitCombat();
	bool IsInCombat() const { return bIsInCombat; }

	// �����̳� �ý��� ����
	int32 GetCurrentContainerIndex() const;

	// UI �� �����÷��� �˸� ��������Ʈ
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

	// ========== 스테이지 처치 수 (업적 판정 / 결과창 표시) ==========

	UFUNCTION(BlueprintPure, Category = "Stage")
	int32 GetStageKillCount() const { return StageKillCount; }

	// 서버 전용. Seamless Travel 시 새 PlayerState 가 0에서 시작하므로 별도 리셋 경로는 두지 않는다.
	void AddStageKill();

protected:
	virtual void BeginPlay() override;

	// Seamless Travel 시 커스텀 프로퍼티 복사
	virtual void CopyProperties(APlayerState* PlayerState) override;

	// GAS ������Ʈ��
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UAttributeSet> AttributeSet;

	// ��Ʈ��ũ ����ȭ ������
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

	// 이번 스테이지 처치 수 (스테이지 한정 값 — Seamless Travel 시 복사하지 않는다)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Stage")
	int32 StageKillCount = 0;

	// 리플리케이션 콜백
	UFUNCTION()
	void OnRep_IsInCombat();

	UFUNCTION()
	void OnRep_EquippedChips();

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

	// 복제된 EquippedChips + 선택 클래스로 수치 캐시를 다시 만들고, 폰에 갱신을 통지한다.
	void RebuildUpgradeRuntime();

	// 소유 클라이언트에서만: 현재 선택 클래스의 장착 목록을 서버에 보고
	void ReportUpgradeLoadoutIfLocal();

private:
	// 장착 칩을 펼친 수치 캐시 (복제 안 함 — 서버/각 클라가 각자 구축)
	FDRUpgradeRuntime CachedUpgradeRuntime;

	// ���� ���� Ÿ�̸� ����
	FTimerHandle CombatTimerHandle;

	// ���� ���� ��� �ð�
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float CombatExitDelay = 5.0f;

	// ü�� ȸ�� �ý���
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects")
	TSubclassOf<UGameplayEffect> HealthRegenEffectClass;

	FActiveGameplayEffectHandle HealthRegenEffectHandle;

	void StartHealthRegen();
	void StopHealthRegen();
	void CheckCombatExit();
	void CheckAndStartHealthRegen();
	void CheckHealthRegenStatus();

	// ü�� ���� ����
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	// ����ȭ�� ���� ĳ��
	FGameplayEffectSpecHandle CachedHealthRegenSpec;
	void InitializeHealthRegenSpec();
};
