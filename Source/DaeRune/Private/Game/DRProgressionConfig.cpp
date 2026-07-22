// Copyright DaeRune


#include "Game/DRProgressionConfig.h"
#include "Curves/CurveFloat.h"

int32 UDRProgressionConfig::GetXpToNextLevel(int32 CurrentLevel) const
{
	// 커브가 있으면 레벨을 가로축으로 샘플링, 없으면 평탄 폴백.
	if (XpToNextLevelCurve)
	{
		const float Value = XpToNextLevelCurve->GetFloatValue(static_cast<float>(CurrentLevel));
		return FMath::Max(1, FMath::RoundToInt(Value));
	}
	return FMath::Max(1, FallbackXpPerLevel);
}

int32 UDRProgressionConfig::CalcStageXp(int32 ClearedPhaseCount, bool bGameClear) const
{
	const int32 SafeCleared = FMath::Max(0, ClearedPhaseCount);
	int32 Xp = BaseStageXp + XpPerClearedPhase * SafeCleared;
	if (bGameClear)
	{
		Xp += FullClearBonusXp;
	}
	return FMath::Max(0, Xp);
}
