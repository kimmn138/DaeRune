// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Game/DRUpgradeTypes.h"
#include "DRUpgradeUILibrary.generated.h"

/**
 * 업그레이드 화면의 표시 문구 생성기. (Plan2.md 21.5 참조)
 *
 * 목적은 하나다 — ★실패 사유/스탯 이름/수치 포맷을 위젯이 각자 만들지 않게 하는 것★.
 * 위젯이 자체 문자열을 만들기 시작하면 현지화 대상이 흩어지고, 같은 코드가 다른 문구로 보인다.
 * 모든 문구는 LOCTEXT 로 작성한다 (Plan4 현지화 규칙).
 */
UCLASS()
class DAERUNE_API UDRUpgradeUILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * 슬롯/칩 조작 실패 사유 → 사용자 문구.
	 * Arg0 / Arg1 은 사유별 포맷 인자다:
	 *   NotEnoughSlots    → Arg0 = 필요한 슬롯 수, Arg1 = 남은 빈 칸 수
	 *   NotEnoughCurrency → Arg0 = 필요한 재화
	 *   ChipLocked        → Arg0 = 필요한 슬롯 해금 단계(RequiredSlotTier)
	 */
	UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
	static FText GetUpgradeResultText(EDRUpgradeResult Result, int32 Arg0 = 0, int32 Arg1 = 0);

	/** 스탯 정식 표시명 ("컨테이너 체력" 등). enum 의 DisplayName 을 그대로 쓴다. 툴팁·상세 문구용. */
	UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
	static FText GetStatDisplayName(EDRUpgradeStat Stat);

	/**
	 * 카드에 찍는 ★짧은★ 스탯 이름 (한글 5자 이내).
	 *
	 * 칩 카드의 텍스트 폭은 100px 이고 이름 폰트가 20pt 라 한 줄에 한글 5자가 한계다
	 * (Plan2.md §22 STEP 5-5). 정식 명칭은 "스킬 물 소모량"처럼 7자짜리가 있어 그대로 넣으면 넘친다.
	 *
	 * enum 의 DisplayName 자체를 줄이지 않고 이 함수를 따로 둔 이유:
	 * 정식 명칭은 툴팁·상세 수치 문구(FDRUpgradeModifier::GetDetailText)에서 그대로 필요하다.
	 * 하나를 줄이면 양쪽 다 줄어든다.
	 */
	UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
	static FText GetStatShortName(EDRUpgradeStat Stat);

	/** "1,250" 처럼 천 단위 구분 (현재 컬처 적용) */
	UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
	static FText FormatCurrency(int32 Amount);

	/**
	 * 칩 정의 → 칩/슬롯 카드용 짧은 2행 라벨 ("ATK" / "+5").
	 *
	 * 규칙:
	 *  0) CardLabelTop 이 있으면 CardLabelTop / CardLabelBottom 을 그대로 쓴다(디자이너가 직접 정한 표기).
	 *  1) EffectSummary 가 "ATK +5" 처럼 [이름 + 수치] 2토막이고 ★카드에 들어가면★ 쪼개 쓴다 —
	 *     이름은 IsValidCardTopLabel, 수치는 IsValidCardValueLabel 을 통과해야 한다.
	 *     문장형 요약("근처 팀원과 함께 공격 +10%, 방어 +10%")이나 한글 단위("2배")는 여기서 걸러진다.
	 *  2) 아니면 첫 번째 ★장점★ 모디파이어를 대표로 삼아 [스탯명] / [±수치] 로 만든다.
	 *  3) 장점 모디파이어가 없으면 DisplayName 을 위쪽에 넣고 아래는 비운다.
	 *     효과가 코드 구동이라 모디파이어가 단점뿐인 칩에서 단점 수치가 대표로 뜨지 않게 한다.
	 */
	UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
	static void MakeShortLabel(const FDRUpgradeChipDefinition& Chip, FText& OutTop, FText& OutBottom);

	/**
	 * 카드 윗줄에 한 줄로 들어가는가 — 공백 빼고 5자 이내.
	 * 칩 카드 텍스트 폭이 100px 이고 이름 폰트(Pretendard 15)의 한글 1자가 약 20px 이다.
	 */
	static bool IsValidCardTopLabel(const FString& Text);

	/**
	 * 카드 아랫줄(수치) 표기인가 — [+ - × x] 숫자 [숫자 . ,]* [% s m x].
	 * 수치 폰트 BrunoAceSC 에는 한글이 없다(숫자 · 기호 · 영문만). 한글이 섞이면 깨진 상자로 그려진다.
	 */
	static bool IsValidCardValueLabel(const FString& Text);

	/** 모디파이어 1건의 상세 문구 (FDRUpgradeModifier::GetDetailText() 위임) */
	UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
	static FText MakeModifierText(const FDRUpgradeModifier& Modifier);

	/**
	 * 미리보기 증분 문구. "+12%" / "-15" / "+20, +12%"
	 * 두 성분이 모두 0이면 빈 텍스트를 돌려준다.
	 */
	UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
	static FText MakeDeltaText(float DeltaFlat, float DeltaPercent);

	/** 수치 1건 포맷 (Percent 는 0.12 → "+12%", Flat 은 20 → "+20") */
	UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
	static FText FormatValue(float Value, EDRUpgradeOp Op);

	/** 수치 표기 공통 숫자 포맷 — FormatValue 와 FDRUpgradeModifier::GetDetailText 가 같이 쓴다 */
	static FNumberFormattingOptions MakeValueFormatOptions();
};
