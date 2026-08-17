// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRProgressionTypes.h"
#include "Game/DRUpgradeTypes.h"
#include "DRGameInstance.generated.h"

class UDRSaveGame;
class UPlayerCharacterClassInfo;
class UDRProgressionConfig;
class UDRChipCatalog;

// 재화 보유량 변경 알림 (업그레이드 화면 바인딩용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrencyChangedSignature, int32, NewCurrency);
// 특정 로봇의 슬롯/장착 상태 변경 알림
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUpgradesChangedSignature, EPlayerCharacterClass, CharacterClass);
// 업그레이드 시스템 해금 알림 (스테이지1 최초 클리어)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpgradeSystemUnlockedSignature);

/**
 *
 */
UCLASS()
class DAERUNE_API UDRGameInstance : public UGameInstance
{
	GENERATED_BODY()

public:
	virtual void Init() override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<class UDRSoundDataAsset> SoundDataAsset;

	// 플레이어 전용 CharacterClassInfo (서버/클라이언트 모두 접근 가능)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Class Defaults")
	TObjectPtr<UPlayerCharacterClassInfo> PlayerCharacterClassInfo;

	// 재화 규칙 + 슬롯 비용 + 칩 카탈로그 (서버/클라 공통). BP에서 DA_ProgressionConfig 지정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	TObjectPtr<UDRProgressionConfig> ProgressionConfig;

	// ========== 맵 전환 시 캐릭터 선택 보존 ==========

	void SavePlayerClassSelection(const FString& PlayerName, EPlayerCharacterClass SelectedClass);
	EPlayerCharacterClass LoadPlayerClassSelection(const FString& PlayerName) const;
	void SaveAllPlayerSelections(UWorld* World);
	void ClearPlayerClassSelections();

	// ========== 진행도 저장/로드 ==========

	UFUNCTION(BlueprintCallable, Category = "Save")
	bool HasCompletedTutorial() const;

	UFUNCTION(BlueprintCallable, Category = "Save")
	void SetTutorialCompleted();

	UFUNCTION(BlueprintCallable, Category = "Save")
	void ResetTutorialProgress();

	UFUNCTION(BlueprintCallable, Category = "Save")
	void LoadProgress();

	UFUNCTION(BlueprintCallable, Category = "Save")
	void SaveProgress();

	// ========== 재화 (계정 공용 지갑, 클라 권위) ==========

	UFUNCTION(BlueprintPure, Category = "Progression|Currency")
	int32 GetCurrency() const;

	UFUNCTION(BlueprintPure, Category = "Progression|Currency")
	int32 GetLifetimeCurrency() const;

	// 치트/디버그용 재화 지급 (음수 지급 불가)
	UFUNCTION(BlueprintCallable, Category = "Progression|Currency")
	void AddCurrency(int32 Amount);

	UPROPERTY(BlueprintAssignable, Category = "Progression|Currency")
	FOnCurrencyChangedSignature OnCurrencyChanged;

	// ========== 업그레이드 시스템 해금 ==========

	// 스테이지1 최초 클리어 전에는 업그레이드 장치 상호작용이 막힌다.
	UFUNCTION(BlueprintPure, Category = "Progression|Upgrade")
	bool IsUpgradeSystemUnlocked() const;

	// 해금 처리 (스테이지 보상 파이프라인 / 치트). 이미 해금돼 있으면 아무 것도 하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Progression|Upgrade")
	void UnlockUpgradeSystem();

	UPROPERTY(BlueprintAssignable, Category = "Progression|Upgrade")
	FOnUpgradeSystemUnlockedSignature OnUpgradeSystemUnlocked;

	// ========== 칩 카탈로그 ==========

	UFUNCTION(BlueprintPure, Category = "Progression|Upgrade")
	UDRChipCatalog* GetChipCatalog() const;

	// ========== 슬롯 조회/해금 ==========

	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	int32 GetMaxSlotCount(EDRChipCategory Category) const;

	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	int32 GetUnlockedSlotCount(EPlayerCharacterClass CharacterClass, EDRChipCategory Category) const;

	// 다음 슬롯 해금 비용. 더 해금할 슬롯이 없으면 -1.
	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	int32 GetNextSlotUnlockCost(EPlayerCharacterClass CharacterClass, EDRChipCategory Category) const;

	// 이 로봇에 슬롯 해금으로 지불한 총액 (환불 상한)
	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	int32 GetClassSpentCurrency(EPlayerCharacterClass CharacterClass) const;

	UFUNCTION(BlueprintCallable, Category = "Progression|Slot")
	EDRUpgradeResult CanUnlockSlot(EPlayerCharacterClass CharacterClass, EDRChipCategory Category) const;

	UFUNCTION(BlueprintCallable, Category = "Progression|Slot")
	EDRUpgradeResult UnlockSlot(EPlayerCharacterClass CharacterClass, EDRChipCategory Category);

	// 마지막으로 해금한 슬롯을 환불한다(순차 해금의 역순). OutRefunded = 돌려받은 재화.
	UFUNCTION(BlueprintCallable, Category = "Progression|Slot")
	EDRUpgradeResult RefundSlot(EPlayerCharacterClass CharacterClass, EDRChipCategory Category, int32& OutRefunded);

	// 칸별 점유 칩 목록 (길이 = MaxSlotCount, 빈 칸은 NAME_None). 업그레이드 화면 좌측.
	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	TArray<FName> GetSlotAssignments(EPlayerCharacterClass CharacterClass, EDRChipCategory Category) const;

	// 해금된 칸 중 비어 있는 칸 수
	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	int32 GetFreeSlotCount(EPlayerCharacterClass CharacterClass, EDRChipCategory Category) const;

	// ========== 칩 조회/장착 ==========

	// 슬롯 해금 단계(RequiredSlotTier)를 충족했는가
	UFUNCTION(BlueprintPure, Category = "Progression|Chip")
	bool IsChipUnlocked(EPlayerCharacterClass CharacterClass, FName ChipId) const;

	UFUNCTION(BlueprintPure, Category = "Progression|Chip")
	bool IsChipEquipped(EPlayerCharacterClass CharacterClass, FName ChipId) const;

	// 장착 가능 여부. 실패 시 OutRequiredSlots/OutFreeSlots 로 UI가 필요한 슬롯 수를 표시한다.
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	EDRUpgradeResult CanEquipChip(EPlayerCharacterClass CharacterClass, FName ChipId,
		int32& OutRequiredSlots, int32& OutFreeSlots) const;

	// 칩 장착. PreferredSlotIndex 는 드래그앤드롭한 칸(유효하지 않으면 앞에서부터 채운다).
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	EDRUpgradeResult EquipChip(EPlayerCharacterClass CharacterClass, FName ChipId, int32 PreferredSlotIndex = -1);

	// 칩 해제 (그 칩이 점유한 모든 칸을 비운다)
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	EDRUpgradeResult UnequipChip(EPlayerCharacterClass CharacterClass, FName ChipId);

	// 특정 칸을 점유한 칩을 해제
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	EDRUpgradeResult UnequipSlot(EPlayerCharacterClass CharacterClass, EDRChipCategory Category, int32 SlotIndex);

	// 업그레이드 화면 우측 칩 목록 (그리기에 필요한 모든 정보 포함)
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	void GetChipViewModels(EPlayerCharacterClass CharacterClass, EDRChipCategory Category,
		TArray<FDRChipViewModel>& OutViewModels) const;

	UPROPERTY(BlueprintAssignable, Category = "Progression|Upgrade")
	FOnUpgradesChangedSignature OnUpgradesChanged;

	// ========== 서버 보고 / 미리보기 ==========

	// 서버 보고용 장착 칩 목록 (중복 없음)
	UFUNCTION(BlueprintPure, Category = "Progression|Upgrade")
	TArray<FName> GetEquippedChips(EPlayerCharacterClass CharacterClass) const;

	// 현재 장착 상태를 수치 캐시로 펼친다
	void BuildUpgradeRuntime(EPlayerCharacterClass CharacterClass, FDRUpgradeRuntime& OutRuntime) const;

	// "이 칩을 끼면/빼면" 상태를 미리 펼친다 (스탯 미리보기용. NAME_None 이면 무시)
	void BuildPreviewRuntime(EPlayerCharacterClass CharacterClass, FName AddChipId, FName RemoveChipId,
		FDRUpgradeRuntime& OutRuntime) const;

	// ========== 스테이지 보상 ==========

	// 서버가 보고한 성과를 1회성 원장과 대조해 지갑에 반영하고 저장한다. 반환은 결과창용 요약.
	UFUNCTION(BlueprintCallable, Category = "Progression|Reward")
	FDRStageRewardResult ApplyStageReward(const FDRStageRewardReport& Report);

	// 이미 지급된 보상인가
	UFUNCTION(BlueprintPure, Category = "Progression|Reward")
	bool IsRewardClaimed(FName RewardId) const;

private:
	// LoadProgress 후 세이브 포맷 마이그레이션 + 누락 키 lazy 초기화 + 상태 정화.
	void EnsureProgressInitialized();

	// 세이브가 없으면 로드/생성까지 보장. 실패하면 nullptr.
	UDRSaveGame* GetOrLoadSaveGame();

	// 로봇별 상태 접근 (없으면 기본값 추가)
	FDRClassUpgradeState& FindOrAddClassState(EPlayerCharacterClass CharacterClass);
	const FDRClassUpgradeState* FindClassState(EPlayerCharacterClass CharacterClass) const;

	// 칩 정의 조회 (카탈로그 미지정 시 nullptr)
	const struct FDRUpgradeChipDefinition* FindChip(FName ChipId) const;

	// 배열 길이/해금 수 클램프 + 고아·잠긴·점유수 불일치 칩 정리. 변경이 있으면 true.
	// OutReclaimedCurrency: 해금 수가 줄어들어 환급해야 하는 재화.
	bool SanitizeClassState(EPlayerCharacterClass CharacterClass, FDRClassUpgradeState& State,
		int32& OutReclaimedCurrency) const;

	// 환불 비율 (Config 미지정 시 전액 환불로 간주)
	float GetRefundRatio() const;

	UPROPERTY()
	TObjectPtr<UDRSaveGame> CurrentSaveGame;

	// 맵 전환 시 캐릭터 선택 보존용 (GameInstance는 맵 전환에서 절대 파괴되지 않음)
	TMap<FString, EPlayerCharacterClass> PlayerClassSelections;
};
