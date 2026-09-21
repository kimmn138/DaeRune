// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRProgressionTypes.h"
#include "Game/DRUpgradeTypes.h"
#include "Game/DRCosmeticTypes.h"
#include "DRGameInstance.generated.h"

class UDRSaveGame;
class UPlayerCharacterClassInfo;
class UDRProgressionConfig;
class UDRChipCatalog;
class UDRCosmeticCatalog;
class UTexture2D;

// 재화 보유량 변경 알림 (업그레이드 화면 바인딩용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCurrencyChangedSignature, int32, NewCurrency);
// 특정 로봇의 슬롯/장착 상태 변경 알림
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnUpgradesChangedSignature, EPlayerCharacterClass, CharacterClass);
// 업그레이드 시스템 해금 알림 (스테이지1 최초 클리어)
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpgradeSystemUnlockedSignature);
// 특정 로봇의 장착 코스메틱 변경 알림 (옷장 화면 갱신용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCosmeticsChangedSignature, EPlayerCharacterClass, CharacterClass);
// 새 스킨 해금 알림 (해금 토스트용)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSkinUnlockedSignature, FName, SkinId);

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

	// 업그레이드 화면 색/브러시 SSOT. BP에서 DA_UpgradeUIStyle 지정. (Plan2.md 21.6)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Progression")
	TObjectPtr<class UDRUpgradeUIStyle> UpgradeUIStyle;

	// 위젯이 스타일을 읽는 단일 경로 (미지정이면 nullptr — 위젯은 자체 기본값으로 폴백한다)
	UFUNCTION(BlueprintPure, Category = "Progression|UI")
	UDRUpgradeUIStyle* GetUpgradeUIStyle() const;

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

	// ★치트/디버그 전용★ — 해금 상태를 임의로 되돌린다(잠김 UI 재확인용).
	// 정식 해금 경로는 UnlockUpgradeSystem() 이고, 이 함수는 테스트 외에 부르지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Progression|Upgrade")
	void DebugSetUpgradeSystemUnlocked(bool bUnlocked);

	UPROPERTY(BlueprintAssignable, Category = "Progression|Upgrade")
	FOnUpgradeSystemUnlockedSignature OnUpgradeSystemUnlocked;

	// ========== 칩 카탈로그 ==========

	UFUNCTION(BlueprintPure, Category = "Progression|Upgrade")
	UDRChipCatalog* GetChipCatalog() const;

	// ========== 슬롯 조회/해금 (카테고리 구분 없는 통합 6칸) ==========

	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	int32 GetMaxSlotCount() const;

	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	int32 GetUnlockedSlotCount(EPlayerCharacterClass CharacterClass) const;

	// 다음 슬롯 해금 비용. 더 해금할 슬롯이 없으면 -1.
	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	int32 GetNextSlotUnlockCost(EPlayerCharacterClass CharacterClass) const;

	// 이 로봇에 슬롯 해금으로 지불한 총액 (환불 상한)
	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	int32 GetClassSpentCurrency(EPlayerCharacterClass CharacterClass) const;

	UFUNCTION(BlueprintCallable, Category = "Progression|Slot")
	EDRUpgradeResult CanUnlockSlot(EPlayerCharacterClass CharacterClass) const;

	UFUNCTION(BlueprintCallable, Category = "Progression|Slot")
	EDRUpgradeResult UnlockSlot(EPlayerCharacterClass CharacterClass);

	// 마지막으로 해금한 슬롯을 환불한다(순차 해금의 역순). OutRefunded = 돌려받은 재화.
	UFUNCTION(BlueprintCallable, Category = "Progression|Slot")
	EDRUpgradeResult RefundSlot(EPlayerCharacterClass CharacterClass, int32& OutRefunded);

	// 칸별 점유 칩 목록 (길이 = MaxSlotCount, 빈 칸은 NAME_None). 업그레이드 화면 좌측.
	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	TArray<FName> GetSlotAssignments(EPlayerCharacterClass CharacterClass) const;

	// 업그레이드 화면 좌측 6칸을 그리는 ★단일 진입점★. 길이 = GetMaxSlotCount().
	// UI 가 GetSlotAssignments() 와 카탈로그를 직접 조합하지 않게 한다.
	UFUNCTION(BlueprintCallable, Category = "Progression|Slot")
	void GetSlotViewModels(EPlayerCharacterClass CharacterClass, TArray<FDRSlotViewModel>& OutSlots) const;

	// 특정 칸(0-base)의 해금 비용. 범위 밖이면 -1.
	// 시안이 "모든 잠긴 칸에 비용 표시"라서 GetNextSlotUnlockCost() 만으로는 부족하다.
	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	int32 GetSlotUnlockCost(int32 SlotIndex) const;

	// 해금된 칸 중 비어 있는 칸 수
	UFUNCTION(BlueprintPure, Category = "Progression|Slot")
	int32 GetFreeSlotCount(EPlayerCharacterClass CharacterClass) const;

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

	// 칩 장착. PreferredSlotIndex(0..MaxSlots-1) 는 드래그앤드롭한 칸(유효하지 않으면 앞에서부터 채운다).
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	EDRUpgradeResult EquipChip(EPlayerCharacterClass CharacterClass, FName ChipId, int32 PreferredSlotIndex = -1);

	// 칩 해제 (그 칩이 점유한 모든 칸을 비운다)
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	EDRUpgradeResult UnequipChip(EPlayerCharacterClass CharacterClass, FName ChipId);

	// 특정 칸을 점유한 칩을 해제 (다중 칸 칩이면 그 칩의 모든 칸이 비워진다)
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	EDRUpgradeResult UnequipSlot(EPlayerCharacterClass CharacterClass, int32 SlotIndex);

	// ========== 칸 단위 조작 (드래그앤드롭 전용) ==========
	//
	// CanEquipChip() 은 "어딘가에 넣을 수 있는가"만 본다. 드래그앤드롭은 ★특정 칸★ 판정이 필요하다.

	// 이 칩을 "이 칸에" 놓을 수 있는가. 잠긴 칸 / 점유 칸(=교체 가능)까지 구분해 돌려준다.
	// 점유 칸이어도 교체가 성립하면 Success 다 — 드롭 하이라이트가 그대로 쓰는 판정이다.
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	EDRUpgradeResult CanEquipChipAtSlot(EPlayerCharacterClass CharacterClass, FName ChipId, int32 SlotIndex,
		int32& OutRequiredSlots, int32& OutFreeSlots) const;

	// 빈 칸이면 장착, 점유 칸이면 기존 칩을 해제하고 교체한다.
	// 실패 시 아무 것도 바꾸지 않는다(사본 작업 후 성공했을 때만 커밋).
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	EDRUpgradeResult SwapOrEquipChip(EPlayerCharacterClass CharacterClass, FName ChipId, int32 SlotIndex);

	// 슬롯 → 슬롯 이동. 목적지가 점유돼 있으면 자리 교환한다.
	// 다중 칸 칩은 통째로 재배치된다.
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	EDRUpgradeResult MoveChip(EPlayerCharacterClass CharacterClass, int32 FromSlotIndex, int32 ToSlotIndex);

	// 이 칩을 이 칸에 놓으면 실제로 어느 칸들이 채워지는지 (하이라이트 미리보기).
	// EquipChip() 과 ★같은 배정 함수★를 쓴다 — UI 가 자체 예측하면 실제 결과와 어긋난다.
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	void PreviewSlotAssignment(EPlayerCharacterClass CharacterClass, FName ChipId, int32 PreferredSlotIndex,
		TArray<int32>& OutSlotIndices) const;

	// 업그레이드 화면 우측 칩 목록 — 이 로봇의 칩 전체 (그리기에 필요한 모든 정보 포함)
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	void GetChipViewModels(EPlayerCharacterClass CharacterClass,
		TArray<FDRChipViewModel>& OutViewModels) const;

	// 위와 같지만 분류로 걸러낸 목록. Category 는 ★UI 필터★ 이지 장착 제한이 아니다.
	UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
	void GetChipViewModelsByCategory(EPlayerCharacterClass CharacterClass, EDRChipCategory Category,
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

	// "이 칩을 끼면/빼면 무엇이 얼마나 변하는가"를 UI 가 쓸 수 있는 줄 목록으로 만든다.
	// BuildUpgradeRuntime/BuildPreviewRuntime 은 BP 노출이 아니므로 이 래퍼가 필요하다.
	// 변화가 있는 항목만 담는다. (Plan2.md 21.4)
	UFUNCTION(BlueprintCallable, Category = "Progression|Upgrade")
	void GetStatPreview(EPlayerCharacterClass CharacterClass, FName AddChipId, FName RemoveChipId,
		TArray<FDRStatPreviewLine>& OutLines) const;

	// ========== 스테이지 보상 ==========

	// 서버가 보고한 성과를 1회성 원장과 대조해 지갑에 반영하고 저장한다. 반환은 결과창용 요약.
	UFUNCTION(BlueprintCallable, Category = "Progression|Reward")
	FDRStageRewardResult ApplyStageReward(const FDRStageRewardReport& Report);

	// 이미 지급된 보상인가
	UFUNCTION(BlueprintPure, Category = "Progression|Reward")
	bool IsRewardClaimed(FName RewardId) const;

	// ========== 코스메틱 / 옷장 (계정 단위, 클라 권위) — Plan.md 5.1 / 15.11 ==========
	//
	// 해금 상태는 ★저장하지 않고 매번 파생★한다 (EarnedAchievements ∪ ClaimedRewards).
	// 파생값을 저장하면 카탈로그의 해금 조건을 조정했을 때 둘이 어긋난다. (Plan.md 4.5)

	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	UDRCosmeticCatalog* GetCosmeticCatalog() const;

	// 해금 조건이 비어 있으면 "기본 제공"으로 true. 정의가 없으면 false.
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	bool IsSkinUnlocked(FName SkinId) const;

	// 장착 중인 스킨. 없으면 NAME_None(기본 외형).
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	FName GetEquippedSkin(EPlayerCharacterClass CharacterClass, EDRCosmeticCategory Category) const;

	// 서버 보고 / 외형 적용용 — 길이는 항상 EDRCosmeticCategory::Count 다.
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	TArray<FName> GetEquippedSkins(EPlayerCharacterClass CharacterClass) const;

	/**
	 * 장착 (세이브 기록 + 즉시 저장 + OnCosmeticsChanged).
	 * NAME_None 은 "기본 외형으로 되돌리기"라 항상 허용된다.
	 * 잠겼거나 / 다른 로봇 소속이거나 / 카테고리가 다르면 false 를 돌려주고 아무 것도 바꾸지 않는다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	bool EquipSkin(EPlayerCharacterClass CharacterClass, EDRCosmeticCategory Category, FName SkinId);

	// 4개 카테고리를 ★전부★ 해제한다 (옷장의 Clear All 버튼). 바뀐 게 있으면 true.
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	bool ClearAllSkins(EPlayerCharacterClass CharacterClass);

	/**
	 * 옷장 화면 그리기의 ★단일 진입점★ (GetChipViewModels 와 같은 규약).
	 * UI 가 카탈로그와 세이브를 직접 조합하지 않게 한다.
	 * 인덱스 0 은 항상 "기본"(장착 해제) 칸이다.
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void GetSkinViewModels(EPlayerCharacterClass CharacterClass, EDRCosmeticCategory Category,
		TArray<FDRSkinViewModel>& OutViewModels) const;

	// 이 로봇에 (잠긴 것 포함) 고를 옷이 하나라도 정의돼 있는가. 옷장 프롬프트 표시용.
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	bool HasAnySkinAvailable(EPlayerCharacterClass CharacterClass) const;

	// ★치트/디버그 전용★ — 업적을 임의로 달성 처리 (DebugSetUpgradeSystemUnlocked 선례)
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void DebugGrantAchievement(FName AchievementId);

	// ★치트/디버그 전용★ — 카탈로그의 모든 해금 조건을 달성 처리
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void DebugUnlockAllSkins();

	UPROPERTY(BlueprintAssignable, Category = "Cosmetic")
	FOnCosmeticsChangedSignature OnCosmeticsChanged;

	UPROPERTY(BlueprintAssignable, Category = "Cosmetic")
	FOnSkinUnlockedSignature OnSkinUnlocked;

private:
	// ========== 코스메틱 내부 ==========

	FDRClassCosmeticState& FindOrAddCosmeticState(EPlayerCharacterClass CharacterClass);
	const FDRClassCosmeticState* FindCosmeticState(EPlayerCharacterClass CharacterClass) const;

	// 카탈로그 조회 (미지정이면 nullptr)
	const FDRSkinDefinition* FindSkinDef(FName SkinId) const;

	// 카탈로그에서 사라졌거나 소속이 어긋난 장착 Id 를 NAME_None 으로 되돌린다. 변경이 있으면 true.
	bool SanitizeCosmeticState(EPlayerCharacterClass CharacterClass, FDRClassCosmeticState& State) const;

	// 썸네일 동기 로드 (옷장은 로비 UI라 아이콘 수가 적다 — ResolveChipIcon 과 같은 판단)
	UTexture2D* ResolveSkinIcon(const TSoftObjectPtr<UTexture2D>& Icon) const;

	// HowToUnlock 이 비어 있을 때 쓸 기본 잠금 문구
	FText MakeDefaultUnlockHint(const FDRSkinDefinition& Def) const;

	// 지금 해금돼 있는 스킨 Id 집합 (보상 반영 전후 비교용)
	TSet<FName> SnapshotUnlockedSkins() const;

	// 스냅샷 이후 새로 해금된 스킨만 OnSkinUnlocked 로 알린다
	void BroadcastNewlyUnlockedSkins(const TSet<FName>& BeforeUnlocked);

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

	// 칩 정의 1개 → 뷰모델 1개. GetChipViewModels* 두 경로가 공유한다(표시 로직 복제 방지).
	void MakeChipViewModel(EPlayerCharacterClass CharacterClass, const struct FDRUpgradeChipDefinition& Chip,
		FDRChipViewModel& OutViewModel) const;

	/**
	 * 칸 배정 규칙의 ★단일 구현★. RequiredSlots 개의 빈 칸을 골라 OutSlotIndices 에 담는다.
	 *
	 * 규칙(Plan2.md 6.4):
	 *   1) PreferredSlotIndex 가 해금됐고 비어 있으면 그 칸을 먼저 쓴다 (= 드롭한 칸)
	 *   2) 모자란 만큼 앞에서부터 빈 칸을 채운다
	 *   3) 연속 칸을 요구하지 않는다
	 *
	 * EquipChip / SwapOrEquipChip / MoveChip / PreviewSlotAssignment 이 전부 이 함수만 쓴다.
	 * 배정 로직이 복제되면 미리보기 하이라이트와 실제 결과가 어긋난다.
	 *
	 * @return 필요한 칸을 모두 확보했으면 true. false 면 OutSlotIndices 는 비어 있다.
	 */
	bool ComputeAssignment(const FDRClassUpgradeState& State, int32 RequiredSlots, int32 PreferredSlotIndex,
		TArray<int32>& OutSlotIndices) const;

	// 그 칩이 차지해야 하는 칸 수. 카탈로그에서 삭제된 고아 칩이면 현재 점유 칸 수로 폴백한다.
	int32 GetChipSlotCount(const FDRClassUpgradeState& State, FName ChipId) const;

	/**
	 * 칩 아이콘 폴백을 ★한 곳에서★ 처리한다.
	 *
	 * 카탈로그에 Icon 을 안 넣은 칩이 흔하다. 폴백을 위젯마다 따로 하면
	 * 어떤 위젯은 기본 아이콘이 뜨고 어떤 위젯은 흰 사각형이 되는 불일치가 생긴다
	 * (칩 카드는 폴백하는데 드래그 비주얼은 안 해서 흰색으로 나오던 문제).
	 * 여기서 한 번 해결하면 카드·슬롯·드래그 비주얼·툴팁이 전부 같은 그림을 받는다.
	 */
	UTexture2D* ResolveChipIcon(UTexture2D* ChipIcon) const;

	// 미리보기 1줄 생성 헬퍼 (전역/스킬 공통). 변화가 없으면 false 를 돌려 줄을 버린다.
	bool MakeStatPreviewLine(EDRUpgradeStat Stat, const FGameplayTag& SkillTag,
		const FDRResolvedStat& Current, const FDRResolvedStat& Preview, FDRStatPreviewLine& OutLine) const;

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
