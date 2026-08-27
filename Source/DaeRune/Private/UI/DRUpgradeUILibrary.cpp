// Copyright DaeRune


#include "UI/DRUpgradeUILibrary.h"

#define LOCTEXT_NAMESPACE "DRUpgradeUI"

namespace
{
	// 수치 표기 공통 포맷 (소수점은 필요할 때만 1자리)
	FNumberFormattingOptions MakeValueFormat()
	{
		FNumberFormattingOptions Options;
		Options.SetMinimumFractionalDigits(0);
		Options.SetMaximumFractionalDigits(1);
		return Options;
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

	default:
		break;
	}

	// 최대 물 / 이동 속도 / 받는 피해 / 물 획득량 은 이미 5자 이내다
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

	// 1) 디자이너가 EffectSummary 를 "ATK +5" 형태로 써 뒀으면 그 표기를 존중한다.
	//    마지막 공백을 기준으로 쪼개고, 뒤쪽이 수치처럼 보일 때만 채택한다.
	if (!Chip.EffectSummary.IsEmpty())
	{
		const FString Summary = Chip.EffectSummary.ToString().TrimStartAndEnd();

		int32 SpaceIndex = INDEX_NONE;
		if (Summary.FindLastChar(TEXT(' '), SpaceIndex) && SpaceIndex > 0)
		{
			const FString Head = Summary.Left(SpaceIndex).TrimStartAndEnd();
			const FString Tail = Summary.Mid(SpaceIndex + 1).TrimStartAndEnd();

			// 뒤쪽 토막이 +/-/숫자로 시작하면 "이름 + 수치" 로 본다
			if (!Head.IsEmpty() && !Tail.IsEmpty())
			{
				const TCHAR First = Tail[0];
				if (First == TEXT('+') || First == TEXT('-') || FChar::IsDigit(First))
				{
					OutTop = FText::FromString(Head);
					OutBottom = FText::FromString(Tail);
					return;
				}
			}
		}
	}

	// 2) 대표 모디파이어(첫 번째 비-단점)로 자동 생성
	const FDRUpgradeModifier* Representative = nullptr;
	for (const FDRUpgradeModifier& Modifier : Chip.Modifiers)
	{
		if (!Modifier.bIsDrawback)
		{
			Representative = &Modifier;
			break;
		}
	}

	// 장점이 하나도 없는 칩(전부 단점)이면 첫 번째 모디파이어를 쓴다
	if (!Representative && Chip.Modifiers.Num() > 0)
	{
		Representative = &Chip.Modifiers[0];
	}

	if (Representative)
	{
		// 카드에 들어가야 하므로 정식 명칭이 아니라 짧은 이름을 쓴다
		OutTop = GetStatShortName(Representative->Stat);
		OutBottom = FormatValue(Representative->Value, Representative->Op);
		return;
	}

	// 3) 효과 정의가 아예 없는 칩 — 이름만이라도 보여준다
	OutTop = Chip.DisplayName;
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
