// Copyright DaeRune


#include "Game/DRChipCatalog.h"
#include "UI/DRUpgradeUILibrary.h"	// 칩 카드 라벨 규칙(IsValidCard*Label) — 검사와 생성이 같은 판정을 쓴다
#include "DaeRune/DRLogChannels.h"

#define LOCTEXT_NAMESPACE "DRUpgrade"

// ========================= FDRUpgradeModifier =========================

FText FDRUpgradeModifier::GetDetailText() const
{
	if (!DetailOverride.IsEmpty())
	{
		return DetailOverride;
	}

	// 열거형 DisplayName(에디터 전용 · 현지화 불가) 대신 LOCTEXT 이름
	const FText StatName = UDRUpgradeUILibrary::GetStatDisplayName(Stat);
	const FText Sign = Value < 0.f ? LOCTEXT("ModifierSignMinus", "-") : LOCTEXT("ModifierSignPlus", "+");

	FNumberFormattingOptions Options;
	Options.SetMinimumFractionalDigits(0);
	// 0.05 단위 칩이 생기면 1자리로는 0.1 로 반올림된다. 2자리로 둬도 MinimumFractionalDigits 가 0이라
	// 기존 표기(+20 / +0.5 / 12%)는 전혀 변하지 않는다. (Plan3(공용칩) §8.3)
	Options.SetMaximumFractionalDigits(2);

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

// ========================= UDRChipCatalog =========================

void UDRChipCatalog::PostLoad()
{
	Super::PostLoad();
	BuildIndex();
}

#if WITH_EDITOR
void UDRChipCatalog::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	BuildIndex();
}
#endif

void UDRChipCatalog::BuildIndex() const
{
	IdToIndex.Reset();
	IdToIndex.Reserve(Chips.Num());
	IndexedEntryCount = Chips.Num();
	for (int32 Index = 0; Index < Chips.Num(); ++Index)
	{
		const FName Id = Chips[Index].ChipId;
		if (Id.IsNone()) continue;

		// 중복 Id 는 먼저 정의된 쪽을 유지 (ValidateCatalog 가 경고를 낸다)
		if (!IdToIndex.Contains(Id))
		{
			IdToIndex.Add(Id, Index);
		}
	}
}

const FDRUpgradeChipDefinition* UDRChipCatalog::FindChip(FName ChipId) const
{
	if (ChipId.IsNone()) return nullptr;

	EnsureIndex();

	if (const int32* IndexPtr = IdToIndex.Find(ChipId))
	{
		if (Chips.IsValidIndex(*IndexPtr))
		{
			return &Chips[*IndexPtr];
		}
	}
	return nullptr;
}

void UDRChipCatalog::GetChipsForClass(EPlayerCharacterClass CharacterClass,
	TArray<FDRUpgradeChipDefinition>& OutChips) const
{
	OutChips.Reset();
	for (const FDRUpgradeChipDefinition& Chip : Chips)
	{
		// 공용 칩은 모든 로봇의 목록에 들어간다 (Plan3(공용칩) §5)
		if (Chip.IsUsableBy(CharacterClass))
		{
			OutChips.Add(Chip);
		}
	}

	// 해금 단계 → 공용 우선 → 이름 순으로 정렬해 UI 목록 순서를 안정화
	OutChips.Sort([](const FDRUpgradeChipDefinition& A, const FDRUpgradeChipDefinition& B)
	{
		if (A.RequiredSlotTier != B.RequiredSlotTier)
		{
			return A.RequiredSlotTier < B.RequiredSlotTier;
		}
		if (A.bCommonChip != B.bCommonChip)
		{
			return A.bCommonChip;
		}
		return A.ChipId.LexicalLess(B.ChipId);
	});
}

FText UDRChipCatalog::GetSkillDisplayName(const FGameplayTag& AbilityTag) const
{
	if (!AbilityTag.IsValid())
	{
		return LOCTEXT("SkillTargetAll", "전체");
	}

	if (const FText* Found = SkillDisplayNames.Find(AbilityTag))
	{
		if (!Found->IsEmpty())
		{
			return *Found;
		}
	}

	// 폴백: 태그의 마지막 조각 (Abilities.GardenRobot.SeedCannon → SeedCannon)
	FString TagString = AbilityTag.ToString();
	FString Left, Right;
	while (TagString.Split(TEXT("."), &Left, &Right))
	{
		TagString = Right;
	}
	return FText::FromString(TagString);
}

bool UDRChipCatalog::IsFeatureGated(FGameplayTag FeatureTag) const
{
	if (!FeatureTag.IsValid()) return false;

	// 칩 수가 많아야 수십 개라 선형 탐색으로 충분하다(HUD 갱신 시에만 호출된다).
	for (const FDRUpgradeChipDefinition& Chip : Chips)
	{
		if (Chip.GrantedTags.HasTag(FeatureTag))
		{
			return true;
		}
	}
	return false;
}

void UDRChipCatalog::ResolveLoadout(const TArray<FName>& EquippedChips, EPlayerCharacterClass CharacterClass,
	FDRUpgradeRuntime& OutRuntime) const
{
	OutRuntime.Reset();

	for (const FName& ChipId : EquippedChips)
	{
		const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
		if (!Chip) continue;

		// 이 로봇이 쓸 수 없는 칩(다른 로봇 전용)이 섞여 들어온 경우 무시
		if (!Chip->IsUsableBy(CharacterClass)) continue;

		for (const FDRUpgradeModifier& Modifier : Chip->Modifiers)
		{
			OutRuntime.AddModifier(Modifier);
		}

		// 수치가 아닌 기능 스위치 (Plan3(공용칩) §4.3)
		OutRuntime.GrantedTags.AppendTags(Chip->GrantedTags);
	}
}

FName UDRChipCatalog::FindSkillConflict(const TArray<FName>& EquippedChips, FName ChipId) const
{
	const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
	if (!Chip) return NAME_None;

	// 공용 칩 · 로봇 전체 대상 칩(칩 레벨 태그 없음)은 제한이 없다
	const FGameplayTag Key = Chip->GetExclusiveSkillTag();
	if (!Key.IsValid()) return NAME_None;

	for (const FName& OtherId : EquippedChips)
	{
		// 자기 자신(다중 칸 칩의 다른 칸 포함)은 건너뛴다
		if (OtherId.IsNone() || OtherId == ChipId) continue;

		if (const FDRUpgradeChipDefinition* Other = FindChip(OtherId))
		{
			if (Other->GetExclusiveSkillTag().MatchesTagExact(Key))
			{
				return OtherId;
			}
		}
	}
	return NAME_None;
}

bool UDRChipCatalog::SanitizeLoadout(TArray<FName>& InOutChips, EPlayerCharacterClass CharacterClass,
	int32 MaxSlots) const
{
	bool bModified = false;

	TArray<FName> Sanitized;
	Sanitized.Reserve(InOutChips.Num());

	// 남은 슬롯 예산 — 슬롯에 종류 구분이 없으므로 단일 합산 하나로 끝난다.
	int32 RemainingSlots = FMath::Max(0, MaxSlots);

	TSet<FName> SeenIds;
	for (const FName& ChipId : InOutChips)
	{
		const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
		if (!Chip || !Chip->IsUsableBy(CharacterClass))
		{
			bModified = true;
			continue;
		}

		// 동일 칩 중복 장착 금지
		if (SeenIds.Contains(ChipId))
		{
			bModified = true;
			continue;
		}

		// ★Plan5(스킬당1칩)★ 같은 스킬 칩은 먼저 온 것 하나만 남긴다 (클라 조작·옛 세이브 방어)
		if (!FindSkillConflict(Sanitized, ChipId).IsNone())
		{
			bModified = true;
			continue;
		}

		const int32 Required = FMath::Max(1, Chip->RequiredSlotCount);
		if (RemainingSlots < Required)
		{
			// 슬롯 총량 초과 → 위조이거나 카탈로그 개편 잔재
			bModified = true;
			continue;
		}

		RemainingSlots -= Required;
		SeenIds.Add(ChipId);
		Sanitized.Add(ChipId);
	}

	if (bModified)
	{
		InOutChips = MoveTemp(Sanitized);
	}

	return bModified;
}

bool UDRChipCatalog::ValidateCatalog(int32 MaxSlots, TArray<FString>& OutErrors) const
{
	OutErrors.Reset();

	const int32 SlotCap = FMath::Max(0, MaxSlots);

	TSet<FName> SeenIds;
	for (const FDRUpgradeChipDefinition& Chip : Chips)
	{
		if (Chip.ChipId.IsNone())
		{
			OutErrors.Add(TEXT("ChipId 가 비어 있는 칩이 있습니다."));
			continue;
		}

		const FString IdString = Chip.ChipId.ToString();

		if (SeenIds.Contains(Chip.ChipId))
		{
			OutErrors.Add(FString::Printf(TEXT("중복 ChipId: %s"), *IdString));
		}
		SeenIds.Add(Chip.ChipId);

		if (Chip.RequiredSlotCount < 1 || Chip.RequiredSlotCount > SlotCap)
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: RequiredSlotCount(%d) 가 1~%d 범위를 벗어나 장착이 불가능합니다."),
				*IdString, Chip.RequiredSlotCount, SlotCap));
		}

		if (Chip.RequiredSlotTier < 1 || Chip.RequiredSlotTier > SlotCap)
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: RequiredSlotTier(%d) 가 1~%d 범위를 벗어나 영구히 잠깁니다."),
				*IdString, Chip.RequiredSlotTier, SlotCap));
		}

		// 필요 슬롯 수는 해금 단계 이하여야 한다 (단계 3에서만 열리는 3칸 칩은 정상)
		if (Chip.RequiredSlotCount > Chip.RequiredSlotTier)
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: RequiredSlotCount(%d) > RequiredSlotTier(%d) — 해금 직후에는 장착할 수 없습니다."),
				*IdString, Chip.RequiredSlotCount, Chip.RequiredSlotTier));
		}

		// HUD 표시 칩처럼 Modifiers 가 비고 GrantedTags 만 있는 칩이 정상이므로 둘 다 본다.
		if (!Chip.HasAnyEffect())
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: Modifiers 와 GrantedTags 가 모두 비어 있어 아무 효과도 없습니다."), *IdString));
		}

		// 기능 스위치 태그는 Upgrade.Feature 하위여야 한다 (엉뚱한 태그를 넣어도 조용히 무시되는 것 방지)
		{
			static const FGameplayTag FeatureRoot =
				FGameplayTag::RequestGameplayTag(FName("Upgrade.Feature"), /*ErrorIfNotFound*/ false);

			for (const FGameplayTag& FeatureTag : Chip.GrantedTags)
			{
				if (!FeatureRoot.IsValid() || !FeatureTag.MatchesTag(FeatureRoot))
				{
					OutErrors.Add(FString::Printf(
						TEXT("%s: GrantedTags 의 %s 가 Upgrade.Feature 하위가 아닙니다."),
						*IdString, *FeatureTag.ToString()));
				}
			}

			// ★Plan5(스킬당1칩)★ 스킬 전용 기능 스위치(Upgrade.Feature.Skill.*)를 주는 칩은
			// 칩 레벨 대상 스킬이 있어야 한다 — 없으면 스킬당 1칩 제한을 조용히 빠져나간다 (Plan5 D-51).
			static const FGameplayTag SkillFeatureRoot =
				FGameplayTag::RequestGameplayTag(FName("Upgrade.Feature.Skill"), /*ErrorIfNotFound*/ false);

			if (!Chip.bCommonChip && !Chip.TargetAbilityTag.IsValid() && SkillFeatureRoot.IsValid()
				&& Chip.GrantedTags.HasTag(SkillFeatureRoot))
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: 스킬 기능 스위치(Upgrade.Feature.Skill.*)를 주는데 칩의 TargetAbilityTag 가 비어 있어 스킬당 1칩 제한을 빠져나갑니다."),
					*IdString));
			}
		}

		if (Chip.DisplayName.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: DisplayName 이 비어 있습니다."), *IdString));
		}

		if (Chip.EffectSummary.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: EffectSummary 가 비어 있습니다(기본 표시 문구)."), *IdString));
		}

		// 칩 카드 라벨을 직접 정했으면 카드에 들어가는지 본다.
		// 자동 라벨은 MakeShortLabel 이 같은 판정으로 걸러서 만들므로 따로 보지 않는다.
		if (!Chip.CardLabelTop.IsEmpty() && !UDRUpgradeUILibrary::IsValidCardTopLabel(Chip.CardLabelTop.ToString()))
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: CardLabelTop '%s' 이 공백 빼고 5자를 넘어 칩 카드에 한 줄로 들어가지 않습니다."),
				*IdString, *Chip.CardLabelTop.ToString()));
		}

		if (!Chip.CardLabelBottom.IsEmpty())
		{
			if (Chip.CardLabelTop.IsEmpty())
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: CardLabelTop 이 비어 있어 CardLabelBottom 이 무시됩니다(자동 라벨이 쓰입니다)."), *IdString));
			}
			else if (!UDRUpgradeUILibrary::IsValidCardValueLabel(Chip.CardLabelBottom.ToString()))
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: CardLabelBottom '%s' 에는 숫자 표기만 쓸 수 있습니다(+ - × x 숫자 . , %% s m) — 수치 폰트에 한글이 없어 깨집니다."),
					*IdString, *Chip.CardLabelBottom.ToString()));
			}
		}

		// 돌파 칩은 단점을 동반하는 것이 설계 규칙이다.
		// 단, 단점이 ★코드 구동★ 인 칩(회복 불가 / 피격 시 컨테이너 소모 / 체력 코스트)은
		// bIsDrawback 모디파이어로 표현되지 않는다 — GrantedTags 로 식별하되,
		// 플레이어가 무엇을 잃는지 알 수 있도록 Description 을 필수로 요구한다.
		// (Plan3(공용칩) §17.4.5)
		if (Chip.Category == EDRChipCategory::Ascension)
		{
			if (!Chip.HasDrawback() && Chip.GrantedTags.Num() == 0)
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: 돌파 칩인데 단점(bIsDrawback) 모디파이어도 GrantedTags 도 없습니다 — 단점이 없는 돌파 칩입니다."),
					*IdString));
			}

			if (!Chip.HasDrawback() && Chip.Description.IsEmpty())
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: 단점이 코드 구동인 돌파 칩은 Description 에 단점을 반드시 적어야 합니다."),
					*IdString));
			}
		}

		for (const FDRUpgradeModifier& Modifier : Chip.Modifiers)
		{
			if (!DRIsSkillStat(Modifier.Stat) && Modifier.TargetAbilityTag.IsValid())
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: 캐릭터 단위 스탯에 TargetAbilityTag(%s) 가 지정돼 무시됩니다."),
					*IdString, *Modifier.TargetAbilityTag.ToString()));
			}

			// ★Plan5(스킬당1칩)★ 전용 칩의 스킬 단위 모디파이어는 칩 레벨 대상 스킬과 ★같은 태그★ 여야 한다.
			//  · 모디파이어 태그가 비면 그 줄이 그 로봇의 ★모든 스킬★ 에 걸린다(GR.Asc.SeedSupport 사고).
			//  · 칩 태그와 다르면 "적용 대상 스킬" 표기·스킬당 1칩 규칙과 실제 효과가 어긋난다.
			if (!Chip.bCommonChip && DRIsSkillStat(Modifier.Stat))
			{
				if (!Chip.TargetAbilityTag.IsValid())
				{
					OutErrors.Add(FString::Printf(
						TEXT("%s: 스킬 단위 모디파이어(%s)가 있는데 칩의 TargetAbilityTag 가 비어 있어 스킬당 1칩 제한을 빠져나갑니다."),
						*IdString, *UEnum::GetValueAsString(Modifier.Stat)));
				}
				else if (!Modifier.TargetAbilityTag.MatchesTagExact(Chip.TargetAbilityTag))
				{
					OutErrors.Add(FString::Printf(
						TEXT("%s: 스킬 단위 모디파이어(%s)의 TargetAbilityTag(%s)가 칩의 대상 스킬(%s)과 다릅니다 — 비어 있으면 이 로봇의 모든 스킬에 적용됩니다."),
						*IdString, *UEnum::GetValueAsString(Modifier.Stat),
						*Modifier.TargetAbilityTag.ToString(), *Chip.TargetAbilityTag.ToString()));
				}
			}

			// 공용 칩에 특정 로봇의 스킬 태그를 걸면 다른 로봇에서는 조용히 효과가 0이 된다
			if (Chip.bCommonChip && Modifier.TargetAbilityTag.IsValid())
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: 공용 칩에 특정 스킬 태그(%s)가 지정돼 다른 로봇에서는 효과가 없습니다."),
					*IdString, *Modifier.TargetAbilityTag.ToString()));
			}

			// MaxHealth 는 "총량" 스탯이라 Flat 만 의미가 있다.
			// 비율은 ContainerHealth Percent 와 수학적으로 동일하므로 그쪽으로 몰아 이중 정의를 막는다.
			if (Modifier.Stat == EDRUpgradeStat::MaxHealth && Modifier.Op == EDRUpgradeOp::Percent)
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: MaxHealth 는 Flat 전용입니다. 비율 증가는 ContainerHealth Percent 로 작성하세요."),
					*IdString));
			}

			// ★Plan5(씨앗칩)★ Percent 는 곱연산 인수 (1 + Value) 로 쓰인다.
			// Value < -1 이면 인수가 음수가 되어 부호가 뒤집힌다(피해 → 회복).
			// 정확히 -1.0(효과를 0 으로 만든다)은 정당한 값이라 허용한다.
			if (Modifier.Op == EDRUpgradeOp::Percent && Modifier.Value < -1.f)
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: Percent %.2f 는 -100%% 미만이라 부호가 뒤집힙니다. -1.0 이상으로 쓰세요."),
					*IdString, Modifier.Value));
			}

			if (FMath::IsNearlyZero(Modifier.Value))
			{
				OutErrors.Add(FString::Printf(TEXT("%s: Value 가 0인 모디파이어가 있습니다."), *IdString));
			}
		}
	}

	if (OutErrors.Num() > 0)
	{
		UE_LOG(LogDR, Warning, TEXT("[ChipCatalog] %s: 정합성 오류 %d건"), *GetName(), OutErrors.Num());
	}

	return OutErrors.Num() == 0;
}

#undef LOCTEXT_NAMESPACE
