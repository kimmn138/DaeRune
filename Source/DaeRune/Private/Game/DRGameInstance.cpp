// Copyright DaeRune


#include "Game/DRGameInstance.h"
#include "Game/DRSaveGame.h"
#include "Game/DRProgressionConfig.h"
#include "Game/DRChipCatalog.h"
#include "Game/DRGameUserSettings.h"
#include "Game/DRSettingsManager.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DRPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Internationalization/Internationalization.h"
#include "DaeRune/DRLogChannels.h"

void UDRGameInstance::Init()
{
	Super::Init();

	// Apply saved UI culture before any widget is constructed.
	// PreferredCulture holds a supported culture code (e.g. "ko", "en") persisted in GameUserSettings.ini.
	if (UDRGameUserSettings* UserSettings = UDRGameUserSettings::GetDRGameUserSettings())
	{
		const FString& Culture = UserSettings->PreferredCulture;
		if (UDRSettingsManager::IsSupportedCulture(Culture))
		{
			FInternationalization::Get().SetCurrentLanguageAndLocale(Culture);
		}
		else
		{
			// Unknown / empty culture -> fall back to the default language and persist it.
			const FString DefaultCulture = UDRSettingsManager::GetDefaultLanguage().CultureCode;
			UserSettings->PreferredCulture = DefaultCulture;
			UserSettings->SaveSettings();
			FInternationalization::Get().SetCurrentLanguageAndLocale(DefaultCulture);
		}
	}

	LoadProgress();
}

bool UDRGameInstance::HasCompletedTutorial() const
{
	if (!CurrentSaveGame) return false;
	return CurrentSaveGame->bHasCompletedTutorial;
}

void UDRGameInstance::SetTutorialCompleted()
{
	if (UDRSaveGame* Save = GetOrLoadSaveGame())
	{
		Save->bHasCompletedTutorial = true;
		SaveProgress();
	}
}

void UDRGameInstance::ResetTutorialProgress()
{
	if (UDRSaveGame* Save = GetOrLoadSaveGame())
	{
		Save->bHasCompletedTutorial = false;
		SaveProgress();
	}
}

void UDRGameInstance::LoadProgress()
{
	if (UGameplayStatics::DoesSaveGameExist(UDRSaveGame::SaveSlotName, UDRSaveGame::UserIndex))
	{
		CurrentSaveGame = Cast<UDRSaveGame>(
			UGameplayStatics::LoadGameFromSlot(UDRSaveGame::SaveSlotName, UDRSaveGame::UserIndex));
	}

	if (!CurrentSaveGame)
	{
		CurrentSaveGame = Cast<UDRSaveGame>(
			UGameplayStatics::CreateSaveGameObject(UDRSaveGame::StaticClass()));
	}

	EnsureProgressInitialized();
}

void UDRGameInstance::SaveProgress()
{
	if (CurrentSaveGame)
	{
		UGameplayStatics::SaveGameToSlot(CurrentSaveGame, UDRSaveGame::SaveSlotName, UDRSaveGame::UserIndex);
	}
}

UDRSaveGame* UDRGameInstance::GetOrLoadSaveGame()
{
	if (!CurrentSaveGame)
	{
		LoadProgress();
	}
	return CurrentSaveGame;
}

void UDRGameInstance::EnsureProgressInitialized()
{
	if (!CurrentSaveGame) return;

	// ===== 세이브 포맷 마이그레이션 =====
	// v0 = 방금 생성된 새 세이브, v1 = 경험치/레벨(폐기), v2 = 랭크 구매형 업그레이드(폐기).
	// 제거된 프로퍼티는 UE 세이브 역직렬화 규칙상 로드 시 자동으로 버려진다.
	// 미출시 단계라 구 진행도 → 신 진행도 환산은 하지 않고 깨끗한 초기 상태로 맞춘다.
	if (CurrentSaveGame->SaveVersion != UDRSaveGame::CurrentSaveVersion)
	{
		if (CurrentSaveGame->SaveVersion > 0)
		{
			UE_LOG(LogDR, Log, TEXT("[Progression] 세이브 v%d → v%d 마이그레이션. 진행도를 초기화합니다."),
				CurrentSaveGame->SaveVersion, UDRSaveGame::CurrentSaveVersion);
		}

		CurrentSaveGame->Currency = 0;
		CurrentSaveGame->LifetimeCurrency = 0;
		CurrentSaveGame->ClaimedRewards.Reset();
		CurrentSaveGame->bUpgradeSystemUnlocked = false;
		CurrentSaveGame->ClassUpgrades.Reset();

		CurrentSaveGame->SaveVersion = UDRSaveGame::CurrentSaveVersion;
	}

	// 누락된 로봇 키를 빈 상태로 lazy 초기화 (enum 추가 시 자동 대응)
	const int32 ClassCount = static_cast<int32>(EPlayerCharacterClass::Count);
	for (int32 Index = 0; Index < ClassCount; ++Index)
	{
		CurrentSaveGame->ClassUpgrades.FindOrAdd(static_cast<EPlayerCharacterClass>(Index));
	}

	// 저장된 상태가 현재 설정/카탈로그와 어긋나면(슬롯 수 변경, 칩 삭제 등) 정화한다.
	// 정화로 사라진 슬롯의 지불액은 지갑으로 되돌려 재화가 증발하지 않게 한다.
	if (ProgressionConfig)
	{
		bool bAnyChanged = false;
		int32 TotalReclaimed = 0;

		for (auto& Pair : CurrentSaveGame->ClassUpgrades)
		{
			int32 Reclaimed = 0;
			if (!SanitizeClassState(Pair.Key, Pair.Value, Reclaimed)) continue;

			bAnyChanged = true;
			TotalReclaimed += Reclaimed;

			UE_LOG(LogDR, Warning,
				TEXT("[Progression] 업그레이드 설정 변경 감지 — 클래스 %d 상태 정화, %d 재화 환급."),
				static_cast<int32>(Pair.Key), Reclaimed);
		}

		if (bAnyChanged)
		{
			CurrentSaveGame->Currency += TotalReclaimed;
			SaveProgress();
		}
	}
}

bool UDRGameInstance::SanitizeClassState(EPlayerCharacterClass CharacterClass, FDRClassUpgradeState& State,
	int32& OutReclaimedCurrency) const
{
	OutReclaimedCurrency = 0;

	// 설정이 없으면 아무 것도 판단할 수 없다 — 데이터를 건드리지 않는다.
	if (!ProgressionConfig) return false;

	bool bModified = false;
	int32 RebuiltSpent = 0;

	const int32 CategoryCount = static_cast<int32>(EDRChipCategory::Count);
	for (int32 CategoryIndex = 0; CategoryIndex < CategoryCount; ++CategoryIndex)
	{
		const EDRChipCategory Category = static_cast<EDRChipCategory>(CategoryIndex);
		const int32 MaxSlots = ProgressionConfig->GetMaxSlotCount(Category);

		TArray<FName>& Slots = State.GetSlotChips(Category);
		if (Slots.Num() != MaxSlots)
		{
			Slots.SetNum(MaxSlots);
			bModified = true;
		}

		// 해금 수 클램프
		const int32 StoredUnlocked = State.GetUnlockedSlotCount(Category);
		const int32 Unlocked = FMath::Clamp(StoredUnlocked, 0, MaxSlots);
		if (Unlocked != StoredUnlocked)
		{
			State.SetUnlockedSlotCount(Category, Unlocked);
			bModified = true;
		}

		// 남은 해금 슬롯의 현재 정가 합계 (SpentCurrency 재산정 기준)
		for (int32 SlotNumber = 1; SlotNumber <= Unlocked; ++SlotNumber)
		{
			const int32 Cost = ProgressionConfig->GetSlotUnlockCost(Category, SlotNumber);
			if (Cost > 0)
			{
				RebuiltSpent += Cost;
			}
		}

		// 1차 패스: 칸 단위 유효성 (정의 존재 / 소속 / 카테고리 / 해금 단계 / 해금 범위)
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			const FName ChipId = Slots[SlotIndex];
			if (ChipId.IsNone()) continue;

			const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
			const bool bValid = Chip
				&& Chip->OwnerClass == CharacterClass
				&& Chip->Category == Category
				&& Chip->RequiredSlotTier <= Unlocked
				&& SlotIndex < Unlocked;

			if (!bValid)
			{
				Slots[SlotIndex] = NAME_None;
				bModified = true;
			}
		}

		// 2차 패스: 점유 칸 수가 RequiredSlotCount 와 어긋난 칩은 완전히 비운다
		TMap<FName, int32> OccupiedCounts;
		for (const FName& ChipId : Slots)
		{
			if (!ChipId.IsNone())
			{
				++OccupiedCounts.FindOrAdd(ChipId);
			}
		}

		for (const TPair<FName, int32>& Pair : OccupiedCounts)
		{
			const FDRUpgradeChipDefinition* Chip = FindChip(Pair.Key);
			const int32 Required = Chip ? FMath::Max(1, Chip->RequiredSlotCount) : 0;
			if (Pair.Value == Required) continue;

			for (FName& Slot : Slots)
			{
				if (Slot == Pair.Key)
				{
					Slot = NAME_None;
				}
			}
			bModified = true;
		}
	}

	// 지불액 재산정: 정가 합계보다 많이 냈던 만큼은 지갑으로 환급 (재화 증발 방지)
	if (RebuiltSpent < State.SpentCurrency)
	{
		OutReclaimedCurrency = State.SpentCurrency - RebuiltSpent;
		State.SpentCurrency = RebuiltSpent;
		bModified = true;
	}

	return bModified;
}

// ========================= 재화 =========================

int32 UDRGameInstance::GetCurrency() const
{
	return CurrentSaveGame ? CurrentSaveGame->Currency : 0;
}

int32 UDRGameInstance::GetLifetimeCurrency() const
{
	return CurrentSaveGame ? CurrentSaveGame->LifetimeCurrency : 0;
}

void UDRGameInstance::AddCurrency(int32 Amount)
{
	if (Amount <= 0) return;

	UDRSaveGame* Save = GetOrLoadSaveGame();
	if (!Save) return;

	Save->Currency += Amount;
	Save->LifetimeCurrency += Amount;
	SaveProgress();

	OnCurrencyChanged.Broadcast(Save->Currency);
}

// ========================= 시스템 해금 =========================

bool UDRGameInstance::IsUpgradeSystemUnlocked() const
{
	return CurrentSaveGame ? CurrentSaveGame->bUpgradeSystemUnlocked : false;
}

void UDRGameInstance::UnlockUpgradeSystem()
{
	UDRSaveGame* Save = GetOrLoadSaveGame();
	if (!Save || Save->bUpgradeSystemUnlocked) return;

	Save->bUpgradeSystemUnlocked = true;
	SaveProgress();

	OnUpgradeSystemUnlocked.Broadcast();
}

// ========================= 카탈로그 / 공용 헬퍼 =========================

UDRChipCatalog* UDRGameInstance::GetChipCatalog() const
{
	return ProgressionConfig ? ProgressionConfig->ChipCatalog : nullptr;
}

const FDRUpgradeChipDefinition* UDRGameInstance::FindChip(FName ChipId) const
{
	const UDRChipCatalog* Catalog = GetChipCatalog();
	return Catalog ? Catalog->FindChip(ChipId) : nullptr;
}

float UDRGameInstance::GetRefundRatio() const
{
	return ProgressionConfig ? FMath::Clamp(ProgressionConfig->RefundRatio, 0.f, 1.f) : 1.f;
}

FDRClassUpgradeState& UDRGameInstance::FindOrAddClassState(EPlayerCharacterClass CharacterClass)
{
	check(CurrentSaveGame);

	FDRClassUpgradeState& State = CurrentSaveGame->ClassUpgrades.FindOrAdd(CharacterClass);

	// 칸 배열 길이를 항상 현재 설정에 맞춰 둔다 (인덱스 접근 안전성 확보)
	if (ProgressionConfig)
	{
		const int32 CategoryCount = static_cast<int32>(EDRChipCategory::Count);
		for (int32 CategoryIndex = 0; CategoryIndex < CategoryCount; ++CategoryIndex)
		{
			const EDRChipCategory Category = static_cast<EDRChipCategory>(CategoryIndex);
			TArray<FName>& Slots = State.GetSlotChips(Category);
			const int32 MaxSlots = ProgressionConfig->GetMaxSlotCount(Category);
			if (Slots.Num() != MaxSlots)
			{
				Slots.SetNum(MaxSlots);
			}
		}
	}

	return State;
}

const FDRClassUpgradeState* UDRGameInstance::FindClassState(EPlayerCharacterClass CharacterClass) const
{
	return CurrentSaveGame ? CurrentSaveGame->ClassUpgrades.Find(CharacterClass) : nullptr;
}

// ========================= 슬롯 =========================

int32 UDRGameInstance::GetMaxSlotCount(EDRChipCategory Category) const
{
	return ProgressionConfig ? ProgressionConfig->GetMaxSlotCount(Category) : 0;
}

int32 UDRGameInstance::GetUnlockedSlotCount(EPlayerCharacterClass CharacterClass, EDRChipCategory Category) const
{
	const FDRClassUpgradeState* State = FindClassState(CharacterClass);
	if (!State) return 0;

	return FMath::Clamp(State->GetUnlockedSlotCount(Category), 0, GetMaxSlotCount(Category));
}

int32 UDRGameInstance::GetNextSlotUnlockCost(EPlayerCharacterClass CharacterClass, EDRChipCategory Category) const
{
	if (!ProgressionConfig) return -1;

	const int32 Unlocked = GetUnlockedSlotCount(CharacterClass, Category);
	return ProgressionConfig->GetSlotUnlockCost(Category, Unlocked + 1);
}

int32 UDRGameInstance::GetClassSpentCurrency(EPlayerCharacterClass CharacterClass) const
{
	const FDRClassUpgradeState* State = FindClassState(CharacterClass);
	return State ? State->SpentCurrency : 0;
}

EDRUpgradeResult UDRGameInstance::CanUnlockSlot(EPlayerCharacterClass CharacterClass, EDRChipCategory Category) const
{
	if (!ProgressionConfig || !GetChipCatalog() || !CurrentSaveGame) return EDRUpgradeResult::InvalidConfig;

	if (!IsUpgradeSystemUnlocked()) return EDRUpgradeResult::SystemLocked;

	const int32 Unlocked = GetUnlockedSlotCount(CharacterClass, Category);
	if (Unlocked >= GetMaxSlotCount(Category)) return EDRUpgradeResult::AllSlotsUnlocked;

	const int32 Cost = ProgressionConfig->GetSlotUnlockCost(Category, Unlocked + 1);
	if (Cost < 0) return EDRUpgradeResult::AllSlotsUnlocked;

	if (CurrentSaveGame->Currency < Cost) return EDRUpgradeResult::NotEnoughCurrency;

	return EDRUpgradeResult::Success;
}

EDRUpgradeResult UDRGameInstance::UnlockSlot(EPlayerCharacterClass CharacterClass, EDRChipCategory Category)
{
	if (!GetOrLoadSaveGame()) return EDRUpgradeResult::InvalidConfig;

	const EDRUpgradeResult CheckResult = CanUnlockSlot(CharacterClass, Category);
	if (CheckResult != EDRUpgradeResult::Success) return CheckResult;

	FDRClassUpgradeState& State = FindOrAddClassState(CharacterClass);
	const int32 Unlocked = State.GetUnlockedSlotCount(Category);
	const int32 Cost = ProgressionConfig->GetSlotUnlockCost(Category, Unlocked + 1);

	CurrentSaveGame->Currency -= Cost;
	State.SpentCurrency += Cost;
	State.SetUnlockedSlotCount(Category, Unlocked + 1);

	SaveProgress();

	OnCurrencyChanged.Broadcast(CurrentSaveGame->Currency);
	OnUpgradesChanged.Broadcast(CharacterClass);

	return EDRUpgradeResult::Success;
}

EDRUpgradeResult UDRGameInstance::RefundSlot(EPlayerCharacterClass CharacterClass, EDRChipCategory Category,
	int32& OutRefunded)
{
	OutRefunded = 0;

	if (!GetOrLoadSaveGame() || !ProgressionConfig) return EDRUpgradeResult::InvalidConfig;

	if (!ProgressionConfig->bAllowSlotRefund) return EDRUpgradeResult::RefundDisabled;

	FDRClassUpgradeState& State = FindOrAddClassState(CharacterClass);
	const int32 Unlocked = State.GetUnlockedSlotCount(Category);
	if (Unlocked <= 0) return EDRUpgradeResult::NoSlotToRefund;

	// 순차 해금의 역순 — 마지막으로 해금한 칸이 비어 있어야 환불할 수 있다
	const int32 SlotIndex = Unlocked - 1;
	const TArray<FName>& Slots = State.GetSlotChips(Category);
	if (Slots.IsValidIndex(SlotIndex) && !Slots[SlotIndex].IsNone())
	{
		return EDRUpgradeResult::SlotOccupied;
	}

	const int32 Cost = FMath::Max(0, ProgressionConfig->GetSlotUnlockCost(Category, Unlocked));
	const int32 Deduct = FMath::Min(Cost, State.SpentCurrency);
	const int32 Refund = FMath::FloorToInt(Deduct * GetRefundRatio());

	State.SpentCurrency -= Deduct;
	State.SetUnlockedSlotCount(Category, Unlocked - 1);
	CurrentSaveGame->Currency += Refund;

	// 해금 단계가 내려가면서 잠긴 칩이 장착 상태로 남을 수 있다 → 자동 해제
	int32 Reclaimed = 0;
	SanitizeClassState(CharacterClass, State, Reclaimed);
	CurrentSaveGame->Currency += Reclaimed;

	SaveProgress();

	OnCurrencyChanged.Broadcast(CurrentSaveGame->Currency);
	OnUpgradesChanged.Broadcast(CharacterClass);

	OutRefunded = Refund + Reclaimed;
	return EDRUpgradeResult::Success;
}

TArray<FName> UDRGameInstance::GetSlotAssignments(EPlayerCharacterClass CharacterClass, EDRChipCategory Category) const
{
	TArray<FName> Result;
	Result.SetNum(GetMaxSlotCount(Category));

	if (const FDRClassUpgradeState* State = FindClassState(CharacterClass))
	{
		const TArray<FName>& Slots = State->GetSlotChips(Category);
		const int32 Count = FMath::Min(Result.Num(), Slots.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Result[Index] = Slots[Index];
		}
	}

	return Result;
}

int32 UDRGameInstance::GetFreeSlotCount(EPlayerCharacterClass CharacterClass, EDRChipCategory Category) const
{
	const FDRClassUpgradeState* State = FindClassState(CharacterClass);
	return State ? State->CountFreeSlots(Category) : 0;
}

// ========================= 칩 =========================

bool UDRGameInstance::IsChipUnlocked(EPlayerCharacterClass CharacterClass, FName ChipId) const
{
	const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
	if (!Chip || Chip->OwnerClass != CharacterClass) return false;

	return GetUnlockedSlotCount(CharacterClass, Chip->Category) >= FMath::Max(1, Chip->RequiredSlotTier);
}

bool UDRGameInstance::IsChipEquipped(EPlayerCharacterClass CharacterClass, FName ChipId) const
{
	const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
	const FDRClassUpgradeState* State = FindClassState(CharacterClass);
	if (!Chip || !State) return false;

	return State->CountOccupiedSlots(Chip->Category, ChipId) > 0;
}

EDRUpgradeResult UDRGameInstance::CanEquipChip(EPlayerCharacterClass CharacterClass, FName ChipId,
	int32& OutRequiredSlots, int32& OutFreeSlots) const
{
	OutRequiredSlots = 0;
	OutFreeSlots = 0;

	if (!ProgressionConfig || !GetChipCatalog() || !CurrentSaveGame) return EDRUpgradeResult::InvalidConfig;

	if (!IsUpgradeSystemUnlocked()) return EDRUpgradeResult::SystemLocked;

	const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
	if (!Chip) return EDRUpgradeResult::UnknownChip;

	if (Chip->OwnerClass != CharacterClass) return EDRUpgradeResult::WrongClass;

	OutRequiredSlots = FMath::Max(1, Chip->RequiredSlotCount);
	OutFreeSlots = GetFreeSlotCount(CharacterClass, Chip->Category);

	if (!IsChipUnlocked(CharacterClass, ChipId)) return EDRUpgradeResult::ChipLocked;

	if (IsChipEquipped(CharacterClass, ChipId)) return EDRUpgradeResult::AlreadyEquipped;

	if (OutFreeSlots < OutRequiredSlots) return EDRUpgradeResult::NotEnoughSlots;

	return EDRUpgradeResult::Success;
}

EDRUpgradeResult UDRGameInstance::EquipChip(EPlayerCharacterClass CharacterClass, FName ChipId,
	int32 PreferredSlotIndex)
{
	if (!GetOrLoadSaveGame()) return EDRUpgradeResult::InvalidConfig;

	// 칸 배열 길이를 먼저 현재 설정에 맞춘다.
	// 판정(CanEquipChip)과 배정이 같은 배열 크기를 보게 해야 빈 칸 계산이 어긋나지 않는다.
	FDRClassUpgradeState& State = FindOrAddClassState(CharacterClass);

	int32 RequiredSlots = 0;
	int32 FreeSlots = 0;
	const EDRUpgradeResult CheckResult = CanEquipChip(CharacterClass, ChipId, RequiredSlots, FreeSlots);
	if (CheckResult != EDRUpgradeResult::Success) return CheckResult;

	const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
	const EDRChipCategory Category = Chip->Category;

	TArray<FName>& Slots = State.GetSlotChips(Category);
	const int32 Unlocked = FMath::Min(State.GetUnlockedSlotCount(Category), Slots.Num());

	int32 Remaining = RequiredSlots;

	// 드래그앤드롭한 칸을 우선 사용 (해금됐고 비어 있을 때)
	if (Slots.IsValidIndex(PreferredSlotIndex) && PreferredSlotIndex < Unlocked && Slots[PreferredSlotIndex].IsNone())
	{
		Slots[PreferredSlotIndex] = ChipId;
		--Remaining;
	}

	// 남은 점유 칸은 앞에서부터 빈 칸을 채운다 (연속 칸을 요구하지 않는다 — Plan2.md 6.4)
	for (int32 Index = 0; Index < Unlocked && Remaining > 0; ++Index)
	{
		if (Slots[Index].IsNone())
		{
			Slots[Index] = ChipId;
			--Remaining;
		}
	}

	// 빈 칸 수를 미리 확인했으므로 여기서 남는 일은 없어야 한다
	checkf(Remaining == 0, TEXT("[Progression] 칩 %s 장착 중 칸 배정 실패 (남은 %d)"), *ChipId.ToString(), Remaining);

	SaveProgress();
	OnUpgradesChanged.Broadcast(CharacterClass);

	return EDRUpgradeResult::Success;
}

EDRUpgradeResult UDRGameInstance::UnequipChip(EPlayerCharacterClass CharacterClass, FName ChipId)
{
	if (!GetOrLoadSaveGame()) return EDRUpgradeResult::InvalidConfig;

	const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
	if (!Chip) return EDRUpgradeResult::UnknownChip;

	FDRClassUpgradeState& State = FindOrAddClassState(CharacterClass);
	TArray<FName>& Slots = State.GetSlotChips(Chip->Category);

	bool bRemoved = false;
	for (FName& Slot : Slots)
	{
		if (Slot == ChipId)
		{
			Slot = NAME_None;
			bRemoved = true;
		}
	}

	if (!bRemoved) return EDRUpgradeResult::NotEquipped;

	SaveProgress();
	OnUpgradesChanged.Broadcast(CharacterClass);

	return EDRUpgradeResult::Success;
}

EDRUpgradeResult UDRGameInstance::UnequipSlot(EPlayerCharacterClass CharacterClass, EDRChipCategory Category,
	int32 SlotIndex)
{
	if (!GetOrLoadSaveGame()) return EDRUpgradeResult::InvalidConfig;

	FDRClassUpgradeState& State = FindOrAddClassState(CharacterClass);
	const TArray<FName>& Slots = State.GetSlotChips(Category);

	if (!Slots.IsValidIndex(SlotIndex) || Slots[SlotIndex].IsNone()) return EDRUpgradeResult::NotEquipped;

	// 여러 칸을 점유한 칩이면 전부 해제된다
	return UnequipChip(CharacterClass, Slots[SlotIndex]);
}

void UDRGameInstance::GetChipViewModels(EPlayerCharacterClass CharacterClass, EDRChipCategory Category,
	TArray<FDRChipViewModel>& OutViewModels) const
{
	OutViewModels.Reset();

	const UDRChipCatalog* Catalog = GetChipCatalog();
	if (!Catalog) return;

	TArray<FDRUpgradeChipDefinition> ChipDefs;
	Catalog->GetChipsForClass(CharacterClass, Category, ChipDefs);
	OutViewModels.Reserve(ChipDefs.Num());

	for (const FDRUpgradeChipDefinition& Chip : ChipDefs)
	{
		FDRChipViewModel ViewModel;
		ViewModel.ChipId = Chip.ChipId;
		ViewModel.Category = Chip.Category;
		ViewModel.DisplayName = Chip.DisplayName;
		ViewModel.TargetSkillName = Catalog->GetSkillDisplayName(Chip.TargetAbilityTag);
		ViewModel.EffectSummary = Chip.EffectSummary;
		ViewModel.Description = Chip.Description;
		ViewModel.Icon = Chip.Icon;
		ViewModel.RequiredSlotCount = FMath::Max(1, Chip.RequiredSlotCount);
		ViewModel.RequiredSlotTier = FMath::Max(1, Chip.RequiredSlotTier);
		ViewModel.bEquipped = IsChipEquipped(CharacterClass, Chip.ChipId);
		ViewModel.bUnlocked = IsChipUnlocked(CharacterClass, Chip.ChipId);
		ViewModel.bHasDrawback = Chip.HasDrawback();

		int32 RequiredSlots = 0;
		int32 FreeSlots = 0;
		const EDRUpgradeResult EquipResult = CanEquipChip(CharacterClass, Chip.ChipId, RequiredSlots, FreeSlots);
		ViewModel.bCanEquipNow = (EquipResult == EDRUpgradeResult::Success);
		ViewModel.EquipBlockReason = EquipResult;

		for (const FDRUpgradeModifier& Modifier : Chip.Modifiers)
		{
			const FText Detail = Modifier.GetDetailText();
			ViewModel.DetailLines.Add(Detail);

			// 돌파 칩의 장점/단점은 세부 정보 토글과 무관하게 항상 표시한다
			if (Modifier.bIsDrawback)
			{
				ViewModel.DrawbackLines.Add(Detail);
			}
			else
			{
				ViewModel.BenefitLines.Add(Detail);
			}
		}

		OutViewModels.Add(MoveTemp(ViewModel));
	}
}

// ========================= 서버 보고 / 미리보기 =========================

TArray<FName> UDRGameInstance::GetEquippedChips(EPlayerCharacterClass CharacterClass) const
{
	TArray<FName> Chips;
	if (const FDRClassUpgradeState* State = FindClassState(CharacterClass))
	{
		State->GetEquippedChips(Chips);
	}
	return Chips;
}

void UDRGameInstance::BuildUpgradeRuntime(EPlayerCharacterClass CharacterClass, FDRUpgradeRuntime& OutRuntime) const
{
	OutRuntime.Reset();

	if (const UDRChipCatalog* Catalog = GetChipCatalog())
	{
		Catalog->ResolveLoadout(GetEquippedChips(CharacterClass), CharacterClass, OutRuntime);
	}
}

void UDRGameInstance::BuildPreviewRuntime(EPlayerCharacterClass CharacterClass, FName AddChipId, FName RemoveChipId,
	FDRUpgradeRuntime& OutRuntime) const
{
	OutRuntime.Reset();

	const UDRChipCatalog* Catalog = GetChipCatalog();
	if (!Catalog) return;

	TArray<FName> Chips = GetEquippedChips(CharacterClass);

	if (!RemoveChipId.IsNone())
	{
		Chips.Remove(RemoveChipId);
	}
	if (!AddChipId.IsNone())
	{
		Chips.AddUnique(AddChipId);
	}

	Catalog->ResolveLoadout(Chips, CharacterClass, OutRuntime);
}

// ========================= 스테이지 보상 =========================

bool UDRGameInstance::IsRewardClaimed(FName RewardId) const
{
	if (!CurrentSaveGame || RewardId.IsNone()) return false;
	return CurrentSaveGame->ClaimedRewards.Contains(RewardId);
}

FDRStageRewardResult UDRGameInstance::ApplyStageReward(const FDRStageRewardReport& Report)
{
	FDRStageRewardResult Result;

	UDRSaveGame* Save = GetOrLoadSaveGame();
	if (!Save) return Result;

	Result.CurrencyBefore = Save->Currency;
	Result.CurrencyAfter = Save->Currency;

	if (!ProgressionConfig)
	{
		UE_LOG(LogDR, Warning, TEXT("[Progression] ProgressionConfig 미지정 — 스테이지 보상을 지급하지 않습니다."));
		return Result;
	}

	// 재화는 클리어 시에만 지급한다 (게임오버 보상 없음 — Plan2.md 5.1)
	if (!Report.bGameClear) return Result;

	// ===== 1) 스테이지 최초 클리어 =====
	if (const FDRStageRewardDef* StageDef = ProgressionConfig->FindStageReward(Report.StageId))
	{
		const FName RewardId = UDRProgressionConfig::MakeStageClearRewardId(Report.StageId);
		if (!RewardId.IsNone() && !Save->ClaimedRewards.Contains(RewardId))
		{
			Save->ClaimedRewards.Add(RewardId);
			Result.FirstClearCurrency = FMath::Max(0, StageDef->FirstClearCurrency);

			FDRRewardLineItem Line;
			Line.RewardId = RewardId;
			Line.Kind = EDRRewardKind::StageFirstClear;
			Line.DisplayName = StageDef->DisplayName.IsEmpty()
				? FText::FromName(Report.StageId)
				: StageDef->DisplayName;
			Line.Currency = Result.FirstClearCurrency;
			Result.Lines.Add(MoveTemp(Line));
		}

		// 해금은 재화 지급과 별개로 판정한다 (원장은 있는데 해금이 안 된 이상 상태 복구)
		if (StageDef->bUnlocksUpgradeSystem && !Save->bUpgradeSystemUnlocked)
		{
			Save->bUpgradeSystemUnlocked = true;
			Result.bUpgradeSystemNewlyUnlocked = true;
		}
	}
	else if (!Report.StageId.IsNone())
	{
		UE_LOG(LogDR, Warning, TEXT("[Progression] ProgressionConfig 에 StageId '%s' 정의가 없어 최초 클리어 보상이 없습니다."),
			*Report.StageId.ToString());
	}

	// ===== 2) 업적 / 클리어 조건 =====
	for (const FName& AchievementId : Report.AchievementIds)
	{
		const FDRAchievementDef* Def = ProgressionConfig->FindAchievement(AchievementId);
		if (!Def)
		{
			UE_LOG(LogDR, Warning, TEXT("[Progression] 정의되지 않은 업적 '%s' 보고 — 무시합니다."),
				*AchievementId.ToString());
			continue;
		}

		// 계정당 1회 — 이미 받았으면 결과창에도 표시하지 않는다
		if (Save->ClaimedRewards.Contains(AchievementId)) continue;

		Save->ClaimedRewards.Add(AchievementId);

		const int32 Currency = FMath::Max(0, Def->Currency);
		Result.AchievementCurrency += Currency;

		FDRRewardLineItem Line;
		Line.RewardId = AchievementId;
		Line.Kind = Def->Kind;
		Line.DisplayName = Def->DisplayName.IsEmpty() ? FText::FromName(AchievementId) : Def->DisplayName;
		Line.Currency = Currency;
		Result.Lines.Add(MoveTemp(Line));
	}

	Result.TotalGained = Result.FirstClearCurrency + Result.AchievementCurrency;

	Save->Currency += Result.TotalGained;
	Save->LifetimeCurrency += Result.TotalGained;
	Result.CurrencyAfter = Save->Currency;

	SaveProgress();

	if (Result.TotalGained > 0)
	{
		OnCurrencyChanged.Broadcast(Save->Currency);
	}
	if (Result.bUpgradeSystemNewlyUnlocked)
	{
		OnUpgradeSystemUnlocked.Broadcast();
	}

	return Result;
}

// ========================= 캐릭터 선택 보존 =========================

void UDRGameInstance::SavePlayerClassSelection(const FString& PlayerName, EPlayerCharacterClass SelectedClass)
{
	PlayerClassSelections.Add(PlayerName, SelectedClass);
}

EPlayerCharacterClass UDRGameInstance::LoadPlayerClassSelection(const FString& PlayerName) const
{
	const EPlayerCharacterClass* Found = PlayerClassSelections.Find(PlayerName);
	return Found ? *Found : EPlayerCharacterClass::Gardener;
}

void UDRGameInstance::SaveAllPlayerSelections(UWorld* World)
{
	if (!World) return;

	AGameStateBase* GS = World->GetGameState<AGameStateBase>();
	if (!GS) return;

	PlayerClassSelections.Empty();
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (ADRPlayerState* DRPS = Cast<ADRPlayerState>(PS))
		{
			PlayerClassSelections.Add(DRPS->GetPlayerName(), DRPS->GetSelectedPlayerClass());
		}
	}
}

void UDRGameInstance::ClearPlayerClassSelections()
{
	PlayerClassSelections.Empty();
}
