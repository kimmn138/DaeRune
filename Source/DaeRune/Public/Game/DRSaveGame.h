// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRProgressionTypes.h"
#include "Game/DRCosmeticTypes.h"
#include "DRSaveGame.generated.h"

/**
 * 플레이어 진행도를 저장하는 SaveGame 클래스.
 * 진행도의 단일 진실 원천(SSOT) — 재화 지갑, 업그레이드 해금 여부, 1회성 보상 원장,
 * 로봇별 슬롯/장착 상태가 여기에만 존재한다.
 * (Plan2.md 4.1 참조)
 */
UCLASS()
class DAERUNE_API UDRSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UDRSaveGame();

	// 튜토리얼 완료 플래그
	UPROPERTY(VisibleAnywhere, Category = "Progress")
	bool bHasCompletedTutorial = false;

	// ========== 재화 (계정 공용 지갑 1종) ==========

	// 현재 보유 재화 (슬롯 해금으로 소비, 환불로 회복)
	UPROPERTY(VisibleAnywhere, Category = "Progress|Currency")
	int32 Currency = 0;

	// 통계용 — 지금까지 획득한 재화 총량 (환불로 줄어들지 않음)
	UPROPERTY(VisibleAnywhere, Category = "Progress|Currency")
	int32 LifetimeCurrency = 0;

	// 이미 지급된 1회성 보상 Id 원장.
	// 스테이지 최초 클리어("StageClear.<StageId>") + 업적/클리어 조건 Id 를 함께 담는다.
	// → "동일한 스테이지 또는 업적을 반복하여 재화를 획득할 수 없다"를 자료구조로 보장한다.
	UPROPERTY(VisibleAnywhere, Category = "Progress|Currency")
	TArray<FName> ClaimedRewards;

	// ========== 업그레이드 ==========

	// 업그레이드 시스템 해금 여부 (스테이지1 최초 클리어 시 true)
	UPROPERTY(VisibleAnywhere, Category = "Progress|Upgrade")
	bool bUpgradeSystemUnlocked = false;

	// 로봇(클래스)별 슬롯 해금 수 + 칸별 장착 칩. 키가 없으면 "아무것도 없는 상태".
	UPROPERTY(VisibleAnywhere, Category = "Progress|Upgrade")
	TMap<EPlayerCharacterClass, FDRClassUpgradeState> ClassUpgrades;

	// ========== 코스메틱 (옷장) — Plan.md 4.2 / 4.5 / 15.11 ==========

	/**
	 * 달성한 업적 Id (로컬 달성 + 스팀 역매핑의 ★합집합★).
	 *
	 * ClaimedRewards 와 ★분리해서 관리한다★:
	 *   ClaimedRewards     = "이 보상의 재화를 이미 지급했다"  → 재화 멱등. 스팀이 절대 건드리지 않는다
	 *   EarnedAchievements = "이 업적을 달성했다(어느 PC에서든)" → 해금 판정. 스팀 역매핑의 대상
	 *
	 * 합치면 스팀 역매핑이 ClaimedRewards 에 Id 를 넣는 순간
	 * UDRGameInstance::ApplyStageReward 의 중복 방지 분기에 걸려
	 * 그 업적의 재화를 ★영원히 못 받게 된다★. (Plan.md 4.5)
	 */
	UPROPERTY(VisibleAnywhere, Category = "Progress|Cosmetic")
	TArray<FName> EarnedAchievements;

	// 로봇(클래스)별 · 카테고리별 장착 스킨. 키가 없으면 "아무것도 안 입은 상태".
	UPROPERTY(VisibleAnywhere, Category = "Progress|Cosmetic")
	TMap<EPlayerCharacterClass, FDRClassCosmeticState> Cosmetics;

	// 스팀에 아직 write 하지 못한 업적 (오프라인 달성분 backfill 큐).
	// 스팀 연동(Plan.md M6) 전까지는 채워두기만 하고 소비하지 않는다.
	UPROPERTY(VisibleAnywhere, Category = "Progress|Cosmetic")
	TArray<FName> PendingSteamAchievements;

	// ========== 버전 ==========

	// 세이브 포맷 버전 (진행도 구조 변경 시 마이그레이션 분기점).
	UPROPERTY(VisibleAnywhere, Category = "Progress")
	int32 SaveVersion = 0;

	// 저장 슬롯 이름
	static const FString SaveSlotName;
	static const int32 UserIndex;

	// 현재 코드가 기대하는 세이브 포맷 버전.
	// v1: 캐릭터별 경험치/레벨 (폐기)
	// v2: 계정 공용 재화 + 랭크 구매형 업그레이드 (폐기)
	// v3: 슬롯 카테고리 분리 (스탯 3 + 돌파 3) (폐기)
	// v4: 구분 없는 통합 슬롯 6칸 + 칩 장착
	//
	// ★코스메틱 필드를 추가하면서 버전을 올리지 않았다★ (Plan.md 4.2)
	// UDRGameInstance::EnsureProgressInitialized 는 버전이 다르면 재화/업그레이드를 ★전부 초기화★한다.
	// 순수 필드 추가는 UE 역직렬화가 기본값으로 채워주므로 버전 상승이 필요 없고,
	// 올리면 테스터 전원의 재화·슬롯·칩이 사라진다. 버전은 ★기존 필드의 의미가 바뀔 때만★ 올린다.
	static constexpr int32 CurrentSaveVersion = 4;
};
