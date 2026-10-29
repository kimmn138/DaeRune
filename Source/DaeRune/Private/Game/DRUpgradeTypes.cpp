// Copyright DaeRune


#include "Game/DRUpgradeTypes.h"
#include "UI/DRUpgradeUILibrary.h"	// 스탯 이름 · 수치 포맷 (칩 UI 와 같은 규칙)

#define LOCTEXT_NAMESPACE "DRUpgrade"

FText FDRUpgradeModifier::GetDetailText() const
{
	if (!DetailOverride.IsEmpty())
	{
		return DetailOverride;
	}

	// 열거형 DisplayName(에디터 전용 · 현지화 불가) 대신 LOCTEXT 이름
	const FText StatName = UDRUpgradeUILibrary::GetStatDisplayName(Stat);
	const FText Sign = Value < 0.f ? LOCTEXT("ModifierSignMinus", "-") : LOCTEXT("ModifierSignPlus", "+");

	const FNumberFormattingOptions Options = UDRUpgradeUILibrary::MakeValueFormatOptions();

	if (Op == EDRUpgradeOp::Percent)
	{
		return FText::Format(
			LOCTEXT("ModifierPercentFmt", "{0} {1}{2}%"),
			StatName, Sign, FText::AsNumber(FMath::Abs(Value) * 100.f, &Options));
	}

	// 시간 단위 스탯은 "초"를 붙인다 — 안 붙이면 "-0.5" 가 비율인지 시간인지 구분이 안 된다.
	return FText::Format(
		DRIsTimeStat(Stat)
			? LOCTEXT("ModifierFlatSecFmt", "{0} {1}{2}초")
			: LOCTEXT("ModifierFlatFmt", "{0} {1}{2}"),
		StatName, Sign, FText::AsNumber(FMath::Abs(Value), &Options));
}

#undef LOCTEXT_NAMESPACE
