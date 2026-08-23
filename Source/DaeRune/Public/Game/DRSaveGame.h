// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRProgressionTypes.h"
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
	// v3: 계정 공용 재화 + 슬롯 해금 / 칩 장착
	static constexpr int32 CurrentSaveVersion = 3;
};
