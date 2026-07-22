// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DRProgressionConfig.generated.h"

class UCurveFloat;

/**
 * 레벨/경험치 곡선 및 스테이지 지급 규칙을 담는 DataAsset. 디자이너 튜닝용.
 * 서버/클라 공통 접근을 위해 UDRGameInstance가 보유한다. (Plan2.md 4.3 참조)
 */
UCLASS()
class DAERUNE_API UDRProgressionConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// 최대 레벨. 도달 시 레벨업/스탯 상승 중단.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
	int32 MaxLevel = 20;

	// 레벨 L → L+1 에 필요한 경험치. 가로축 = Level 인 CurveFloat 사용 권장.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
	TObjectPtr<UCurveFloat> XpToNextLevelCurve;

	// 곡선 미설정 시 폴백: 레벨당 필요 경험치(평탄).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Level")
	int32 FallbackXpPerLevel = 100;

	// === 스테이지 종료 경험치 지급 규칙 (페이즈 수 기반) ===

	// 참가 기본 보상
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage Reward")
	int32 BaseStageXp = 10;

	// 클리어(완료)한 페이즈 1개당
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage Reward")
	int32 XpPerClearedPhase = 50;

	// 전 페이즈 완수(게임클리어) 보너스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Stage Reward")
	int32 FullClearBonusXp = 100;

	// 현재 레벨에서 다음 레벨까지 필요한 경험치. 커브가 없으면 폴백값.
	UFUNCTION(BlueprintCallable, Category = "Progression")
	int32 GetXpToNextLevel(int32 CurrentLevel) const;

	// 스테이지 종료 시 지급 경험치 계산.
	UFUNCTION(BlueprintCallable, Category = "Progression")
	int32 CalcStageXp(int32 ClearedPhaseCount, bool bGameClear) const;
};
