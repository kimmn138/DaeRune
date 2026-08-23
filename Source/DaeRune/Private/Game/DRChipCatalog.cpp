// Copyright DaeRune


#include "Game/DRChipCatalog.h"
#include "DaeRune/DRLogChannels.h"

#define LOCTEXT_NAMESPACE "DRUpgrade"

// ========================= FDRUpgradeModifier =========================

FText FDRUpgradeModifier::GetDetailText() const
{
	if (!DetailOverride.IsEmpty())
	{
		return DetailOverride;
	}

	const FText StatName = UEnum::GetDisplayValueAsText(Stat);
	const FText Sign = Value < 0.f ? LOCTEXT("ModifierSignMinus", "-") : LOCTEXT("ModifierSignPlus", "+");

	FNumberFormattingOptions Options;
	Options.SetMinimumFractionalDigits(0);
	Options.SetMaximumFractionalDigits(1);

	if (Op == EDRUpgradeOp::Percent)
	{
		return FText::Format(
			LOCTEXT("ModifierPercentFmt", "{0} {1}{2}%"),
			StatName, Sign, FText::AsNumber(FMath::Abs(Value) * 100.f, &Options));
	}

	return FText::Format(
		LOCTEXT("ModifierFlatFmt", "{0} {1}{2}"),
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

void UDRChipCatalog::GetChipsForClass(EPlayerCharacterClass CharacterClass, EDRChipCategory Category,
	TArray<FDRUpgradeChipDefinition>& OutChips) const
{
	OutChips.Reset();
	for (const FDRUpgradeChipDefinition& Chip : Chips)
	{
		if (Chip.OwnerClass == CharacterClass && Chip.Category == Category)
		{
			OutChips.Add(Chip);
		}
	}

	// 해금 단계 → 이름 순으로 정렬해 UI 목록 순서를 안정화
	OutChips.Sort([](const FDRUpgradeChipDefinition& A, const FDRUpgradeChipDefinition& B)
	{
		if (A.RequiredSlotTier != B.RequiredSlotTier)
		{
			return A.RequiredSlotTier < B.RequiredSlotTier;
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

void UDRChipCatalog::ResolveLoadout(const TArray<FName>& EquippedChips, EPlayerCharacterClass CharacterClass,
	FDRUpgradeRuntime& OutRuntime) const
{
	OutRuntime.Reset();

	for (const FName& ChipId : EquippedChips)
	{
		const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
		if (!Chip) continue;

		// 다른 로봇의 칩이 섞여 들어온 경우 무시
		if (Chip->OwnerClass != CharacterClass) continue;

		for (const FDRUpgradeModifier& Modifier : Chip->Modifiers)
		{
			OutRuntime.AddModifier(Modifier);
		}
	}
}

bool UDRChipCatalog::SanitizeLoadout(TArray<FName>& InOutChips, EPlayerCharacterClass CharacterClass,
	int32 MaxStatSlots, int32 MaxAscensionSlots) const
{
	bool bModified = false;

	TArray<FName> Sanitized;
	Sanitized.Reserve(InOutChips.Num());

	// 카테고리별 남은 슬롯 예산 (서버는 클라의 해금 단계를 모르지만 구조적 상한은 안다)
	int32 RemainingSlots[static_cast<int32>(EDRChipCategory::Count)] = { 0 };
	RemainingSlots[static_cast<int32>(EDRChipCategory::Stat)] = FMath::Max(0, MaxStatSlots);
	RemainingSlots[static_cast<int32>(EDRChipCategory::Ascension)] = FMath::Max(0, MaxAscensionSlots);

	TSet<FName> SeenIds;
	for (const FName& ChipId : InOutChips)
	{
		const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
		if (!Chip || Chip->OwnerClass != CharacterClass)
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

		const int32 CategoryIndex = static_cast<int32>(Chip->Category);
		if (!ensure(CategoryIndex >= 0 && CategoryIndex < static_cast<int32>(EDRChipCategory::Count)))
		{
			bModified = true;
			continue;
		}

		const int32 Required = FMath::Max(1, Chip->RequiredSlotCount);
		if (RemainingSlots[CategoryIndex] < Required)
		{
			// 슬롯 총량 초과 → 위조이거나 카탈로그 개편 잔재
			bModified = true;
			continue;
		}

		RemainingSlots[CategoryIndex] -= Required;
		SeenIds.Add(ChipId);
		Sanitized.Add(ChipId);
	}

	if (bModified)
	{
		InOutChips = MoveTemp(Sanitized);
	}

	return bModified;
}

bool UDRChipCatalog::ValidateCatalog(int32 MaxStatSlots, int32 MaxAscensionSlots, TArray<FString>& OutErrors) const
{
	OutErrors.Reset();

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

		const int32 MaxSlots = (Chip.Category == EDRChipCategory::Ascension) ? MaxAscensionSlots : MaxStatSlots;

		if (Chip.RequiredSlotCount < 1 || Chip.RequiredSlotCount > MaxSlots)
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: RequiredSlotCount(%d) 가 1~%d 범위를 벗어나 장착이 불가능합니다."),
				*IdString, Chip.RequiredSlotCount, MaxSlots));
		}

		if (Chip.RequiredSlotTier < 1 || Chip.RequiredSlotTier > MaxSlots)
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: RequiredSlotTier(%d) 가 1~%d 범위를 벗어나 영구히 잠깁니다."),
				*IdString, Chip.RequiredSlotTier, MaxSlots));
		}

		// 필요 슬롯 수는 해금 단계 이하여야 한다 (단계 3에서만 열리는 3칸 칩은 정상)
		if (Chip.RequiredSlotCount > Chip.RequiredSlotTier)
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s: RequiredSlotCount(%d) > RequiredSlotTier(%d) — 해금 직후에는 장착할 수 없습니다."),
				*IdString, Chip.RequiredSlotCount, Chip.RequiredSlotTier));
		}

		if (Chip.Modifiers.Num() == 0)
		{
			OutErrors.Add(FString::Printf(TEXT("%s: Modifiers 가 비어 있어 아무 효과도 없습니다."), *IdString));
		}

		if (Chip.DisplayName.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: DisplayName 이 비어 있습니다."), *IdString));
		}

		if (Chip.EffectSummary.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: EffectSummary 가 비어 있습니다(기본 표시 문구)."), *IdString));
		}

		// 돌파 칩은 단점을 동반하는 것이 설계 규칙
		if (Chip.Category == EDRChipCategory::Ascension && !Chip.HasDrawback())
		{
			OutErrors.Add(FString::Printf(TEXT("%s: 돌파 칩인데 단점(bIsDrawback) 모디파이어가 없습니다."), *IdString));
		}

		for (const FDRUpgradeModifier& Modifier : Chip.Modifiers)
		{
			if (!DRIsSkillStat(Modifier.Stat) && Modifier.TargetAbilityTag.IsValid())
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: 캐릭터 단위 스탯에 TargetAbilityTag(%s) 가 지정돼 무시됩니다."),
					*IdString, *Modifier.TargetAbilityTag.ToString()));
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
