// Copyright DaeRune


#include "UI/DRUpgradeUILibrary.h"

#define LOCTEXT_NAMESPACE "DRUpgradeUI"

namespace
{
	// 수치 표기 공통 포맷 (소수점은 필요할 때만 최대 2자리).
	// 1자리로 두면 0.05 단위 칩이 0.1 로 반올림돼 오표기가 된다.
	// MinimumFractionalDigits 가 0이라 기존 표기(+20 / +0.5 / 12%)는 전혀 변하지 않는다.
	// (Plan3(공용칩) §8.3 — FDRUpgradeModifier::GetDetailText 의 같은 포맷과 ★쌍으로★ 유지할 것)
	FNumberFormattingOptions MakeValueFormat()
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(0);
		Options.SetMaximumFractionalDigits(2);
		return Options;
	}

	// 칩 카드 윗줄 한계 — 텍스트 폭 100px · Pretendard 15(한글 1자 ≈ 20px) → 공백 빼고 5자 (Plan2.md §22 STEP 5-5)
	constexpr int32 MaxCardTopChars = 5;

	// FChar::IsDigit 는 전각 숫자 등도 참이라 쓰지 않는다 — 수치 폰트에 있는 건 ASCII 숫자뿐이다
	bool IsAsciiDigit(TCHAR Char)
	{
		return Char >= TEXT('0') && Char <= TEXT('9');
	}
}

FText UDRUpgradeUILibrary::GetUpgradeResultText(EDRUpgradeResult Result, int32 Arg0, int32 Arg1)
{
	switch (Result)
	{
	case EDRUpgradeResult::Success:
		return FText::GetEmpty();

	case EDRUpgradeResult::InvalidConfig:
		// 콘텐츠 미설정. 플레이어 잘못이 아니므로 조치 가능한 문구로 쓰지 않는다.
		return LOCTEXT("Result_InvalidConfig", "업그레이드 데이터가 설정되지 않았습니다.");

	case EDRUpgradeResult::SystemLocked:
		return LOCTEXT("Result_SystemLocked", "스테이지 1을 클리어하면 열립니다.");

	case EDRUpgradeResult::UnknownChip:
		return LOCTEXT("Result_UnknownChip", "존재하지 않는 칩입니다.");

	case EDRUpgradeResult::WrongClass:
		return LOCTEXT("Result_WrongClass", "다른 로봇의 칩입니다.");

	case EDRUpgradeResult::ChipLocked:
		return FText::Format(
			LOCTEXT("Result_ChipLocked", "슬롯을 {0}칸 이상 해금해야 사용할 수 있습니다."),
			FText::AsNumber(Arg0));

	case EDRUpgradeResult::AlreadyEquipped:
		return LOCTEXT("Result_AlreadyEquipped", "이미 장착된 칩입니다.");

	case EDRUpgradeResult::NotEquipped:
		return LOCTEXT("Result_NotEquipped", "장착되지 않은 칩입니다.");

	case EDRUpgradeResult::NotEnoughSlots:
		return FText::Format(
			LOCTEXT("Result_NotEnoughSlots", "슬롯이 부족합니다. (필요 {0}칸 / 남은 {1}칸)"),
			FText::AsNumber(Arg0), FText::AsNumber(Arg1));

	case EDRUpgradeResult::NotEnoughCurrency:
		return FText::Format(
			LOCTEXT("Result_NotEnoughCurrency", "재화가 부족합니다. (필요 {0})"),
			FormatCurrency(Arg0));

	case EDRUpgradeResult::AllSlotsUnlocked:
		return LOCTEXT("Result_AllSlotsUnlocked", "모든 슬롯을 해금했습니다.");

	case EDRUpgradeResult::NoSlotToRefund:
		return LOCTEXT("Result_NoSlotToRefund", "환불할 슬롯이 없습니다.");

	case EDRUpgradeResult::RefundDisabled:
		return LOCTEXT("Result_RefundDisabled", "슬롯 환불이 비활성화되어 있습니다.");

	case EDRUpgradeResult::SlotOccupied:
		return LOCTEXT("Result_SlotOccupied", "칩을 먼저 해제해야 합니다.");

	case EDRUpgradeResult::PreviousSlotLocked:
		return LOCTEXT("Result_PreviousSlotLocked", "앞 슬롯을 먼저 해금해야 합니다.");

	case EDRUpgradeResult::SkillConflict:
		// ★Plan5(스킬당1칩)★ 어떤 칩과 부딪히는지는 뷰모델의 ConflictingChipName 으로 따로 보여 준다
		return LOCTEXT("Result_SkillConflict", "같은 스킬의 칩은 하나만 장착할 수 있습니다. 장착된 칩을 먼저 해제하세요.");

	default:
		break;
	}

	return FText::GetEmpty();
}

FText UDRUpgradeUILibrary::GetStatDisplayName(EDRUpgradeStat Stat)
{
	return UEnum::GetDisplayValueAsText(Stat);
}

FText UDRUpgradeUILibrary::GetStatShortName(EDRUpgradeStat Stat)
{
	// 카드에 들어가는 한글은 5자까지다. 정식 명칭이 그걸 넘는 것만 줄인다.
	switch (Stat)
	{
	case EDRUpgradeStat::ContainerHealth:
		return LOCTEXT("StatShort_ContainerHealth", "컨테이너");		// 정식: 컨테이너 체력

	case EDRUpgradeStat::SkillDamage:
		return LOCTEXT("StatShort_SkillDamage", "스킬 피해");			// 정식: 스킬 피해량

	case EDRUpgradeStat::SkillWaterCost:
		return LOCTEXT("StatShort_SkillWaterCost", "물 소모");			// 정식: 스킬 물 소모량

	case EDRUpgradeStat::SkillCooldown:
		return LOCTEXT("StatShort_SkillCooldown", "쿨다운");			// 정식: 스킬 딜레이

	case EDRUpgradeStat::SkillProjectileCount:
		return LOCTEXT("StatShort_SkillProjectile", "투사체");			// 정식: 스킬 투사체 수

	case EDRUpgradeStat::HealthRegenDelay:
		return LOCTEXT("StatShort_RegenDelay", "회복 시작");			// 정식: 회복 시작 시간

	case EDRUpgradeStat::SkillHealing:
		return LOCTEXT("StatShort_SkillHealing", "회복량");			// 정식: 스킬 회복량

	case EDRUpgradeStat::SkillWaterGrant:
		return LOCTEXT("StatShort_SkillWaterGrant", "물 지급");		// 정식: 스킬 물 지급량

	case EDRUpgradeStat::SkillWindup:
		return LOCTEXT("StatShort_SkillWindup", "선딜");			// 정식: 스킬 선딜레이

	case EDRUpgradeStat::SkillFireInterval:
		return LOCTEXT("StatShort_SkillFireInterval", "발사 간격");		// 정식: 스킬 발사 간격 (Plan5(동전칩))

	case EDRUpgradeStat::SkillProjectileScale:
		return LOCTEXT("StatShort_SkillProjectileScale", "탄 크기");		// 정식: 스킬 투사체 크기 (Plan5(동전칩))

	case EDRUpgradeStat::SkillChargeGrowth:
		return LOCTEXT("StatShort_SkillChargeGrowth", "차지 증가");		// 정식: 스킬 차지 증가량 (Plan5(공기탄칩))

	case EDRUpgradeStat::SkillOuterDamage:
		return LOCTEXT("StatShort_SkillOuterDamage", "외곽 피해");		// 정식: 스킬 외곽 피해 (Plan5(물점프칩))

	case EDRUpgradeStat::SkillLaunchHeight:
		return LOCTEXT("StatShort_SkillLaunchHeight", "상승 높이");		// 정식: 스킬 상승 높이 (Plan5(물점프칩))

	case EDRUpgradeStat::SkillAirUseCount:
		return LOCTEXT("StatShort_SkillAirUseCount", "공중 횟수");		// 정식: 스킬 공중 사용 횟수 (Plan5(물점프칩))

	case EDRUpgradeStat::SkillSelfDamage:
		return LOCTEXT("StatShort_SkillSelfDamage", "자해 피해");		// 정식: 스킬 자해 피해 (Plan5(돌진칩))

	case EDRUpgradeStat::SkillSpeedBonus:
		return LOCTEXT("StatShort_SkillSpeedBonus", "이속 버프");		// 정식: 스킬 이속 버프 (Plan5(돌진칩))

	case EDRUpgradeStat::SkillComboWindup:
		return LOCTEXT("StatShort_SkillComboWindup", "연속 선딜");		// 정식: 스킬 연속 선딜레이 (Plan5(가방칩))

	case EDRUpgradeStat::SkillComboRecovery:
		return LOCTEXT("StatShort_SkillComboRecovery", "연속 후딜");		// 정식: 스킬 연속 후딜레이 (Plan5(가방칩))

	case EDRUpgradeStat::SkillDuration:
		return LOCTEXT("StatShort_SkillDuration", "지속 시간");		// 정식: 스킬 지속 시간 (Plan5(도발칩 · 모자칩))

	// SkillAreaRadius("스킬 범위")는 이미 5자 이내라 폴백으로 충분하다

	default:
		break;
	}

	// 최대 체력 / 최대 물 / 이동 속도 / 받는 피해 / 물 획득량 은 이미 5자 이내다
	return GetStatDisplayName(Stat);
}

FText UDRUpgradeUILibrary::FormatCurrency(int32 Amount)
{
	// 천 단위 구분은 컬처에 따라 자동 처리된다
	return FText::AsNumber(Amount);
}

FText UDRUpgradeUILibrary::FormatValue(float Value, EDRUpgradeOp Op)
{
	const FNumberFormattingOptions Options = MakeValueFormat();
	const FText Sign = Value < 0.f
		? LOCTEXT("ValueSignMinus", "-")
		: LOCTEXT("ValueSignPlus", "+");

	if (Op == EDRUpgradeOp::Percent)
	{
		return FText::Format(
			LOCTEXT("ValuePercentFmt", "{0}{1}%"),
			Sign, FText::AsNumber(FMath::Abs(Value) * 100.f, &Options));
	}

	return FText::Format(
		LOCTEXT("ValueFlatFmt", "{0}{1}"),
		Sign, FText::AsNumber(FMath::Abs(Value), &Options));
}

void UDRUpgradeUILibrary::MakeShortLabel(const FDRUpgradeChipDefinition& Chip, FText& OutTop, FText& OutBottom)
{
	OutTop = FText::GetEmpty();
	OutBottom = FText::GetEmpty();

	// 0) 디자이너가 카드 라벨을 직접 정했으면 그대로 쓴다 (카드에 들어가는지는 ValidateCatalog 가 본다)
	if (!Chip.CardLabelTop.IsEmpty())
	{
		OutTop = Chip.CardLabelTop;
		OutBottom = Chip.CardLabelBottom;
		return;
	}

	// 1) 디자이너가 EffectSummary 를 "ATK +5" 형태로 써 뒀으면 그 표기를 존중한다.
	//    마지막 공백을 기준으로 쪼개고, ★두 토막이 모두 카드에 들어갈 때만★ 채택한다.
	//    문장형 요약은 윗줄이 3줄로 늘어 SLOT 배지를 덮고, "2배" 같은 한글 단위는 수치 폰트에 없어 깨진다.
	if (!Chip.EffectSummary.IsEmpty())
	{
		const FString Summary = Chip.EffectSummary.ToString().TrimStartAndEnd();

		int32 SpaceIndex = INDEX_NONE;
		if (Summary.FindLastChar(TEXT(' '), SpaceIndex) && SpaceIndex > 0)
		{
			const FString Head = Summary.Left(SpaceIndex).TrimStartAndEnd();
			const FString Tail = Summary.Mid(SpaceIndex + 1).TrimStartAndEnd();

			if (IsValidCardTopLabel(Head) && IsValidCardValueLabel(Tail))
			{
				OutTop = FText::FromString(Head);
				OutBottom = FText::FromString(Tail);
				return;
			}
		}
	}

	// 2) 대표 모디파이어(첫 번째 장점)로 자동 생성
	for (const FDRUpgradeModifier& Modifier : Chip.Modifiers)
	{
		if (!Modifier.bIsDrawback)
		{
			// 카드에 들어가야 하므로 정식 명칭이 아니라 짧은 이름을 쓴다
			OutTop = GetStatShortName(Modifier.Stat);
			OutBottom = FormatValue(Modifier.Value, Modifier.Op);
			return;
		}
	}

	// 3) 장점 모디파이어가 없는 칩 — 이름만 보여 준다.
	//    효과가 코드 구동인 칩은 모디파이어가 단점뿐이라, 그 수치를 띄우면
	//    "물 소모 +100%" 처럼 단점이 칩의 효과로 읽힌다.
	OutTop = Chip.DisplayName;
}

bool UDRUpgradeUILibrary::IsValidCardTopLabel(const FString& Text)
{
	int32 Count = 0;
	for (const TCHAR Char : Text)
	{
		if (!FChar::IsWhitespace(Char))
		{
			++Count;
		}
	}
	return Count > 0 && Count <= MaxCardTopChars;
}

bool UDRUpgradeUILibrary::IsValidCardValueLabel(const FString& Text)
{
	constexpr TCHAR MultiplySign = 0x00D7;	// '×'

	const int32 Len = Text.Len();
	int32 Index = 0;

	// [+ - × x]
	if (Index < Len && (Text[Index] == TEXT('+') || Text[Index] == TEXT('-')
		|| Text[Index] == MultiplySign || Text[Index] == TEXT('x')))
	{
		++Index;
	}

	// 숫자로 시작해야 한다
	if (Index >= Len || !IsAsciiDigit(Text[Index])) return false;

	// [숫자 . ,]*
	while (Index < Len && (IsAsciiDigit(Text[Index]) || Text[Index] == TEXT('.') || Text[Index] == TEXT(',')))
	{
		++Index;
	}

	// [% s m x] — 퍼센트 · 초 · 미터 · 배
	if (Index < Len && (Text[Index] == TEXT('%') || Text[Index] == TEXT('s')
		|| Text[Index] == TEXT('m') || Text[Index] == TEXT('x')))
	{
		++Index;
	}

	return Index == Len;
}

FText UDRUpgradeUILibrary::MakeModifierText(const FDRUpgradeModifier& Modifier)
{
	// 문구 생성 규칙은 FDRUpgradeModifier 가 이미 갖고 있다. 여기서 복제하지 않는다.
	return Modifier.GetDetailText();
}

FText UDRUpgradeUILibrary::MakeDeltaText(float DeltaFlat, float DeltaPercent)
{
	const bool bHasFlat = !FMath::IsNearlyZero(DeltaFlat);
	const bool bHasPercent = !FMath::IsNearlyZero(DeltaPercent);

	if (!bHasFlat && !bHasPercent)
	{
		return FText::GetEmpty();
	}

	if (bHasFlat && bHasPercent)
	{
		return FText::Format(
			LOCTEXT("DeltaBothFmt", "{0}, {1}"),
			FormatValue(DeltaFlat, EDRUpgradeOp::Flat),
			FormatValue(DeltaPercent, EDRUpgradeOp::Percent));
	}

	return bHasFlat
		? FormatValue(DeltaFlat, EDRUpgradeOp::Flat)
		: FormatValue(DeltaPercent, EDRUpgradeOp::Percent);
}

#undef LOCTEXT_NAMESPACE
