// Copyright DaeRune


#include "Game/DRGameInstance.h"
#include "Game/DRSaveGame.h"
#include "Game/DRProgressionConfig.h"
#include "Game/DRChipCatalog.h"
#include "Game/DRCosmeticCatalog.h"
#include "Game/DRGameUserSettings.h"
#include "Game/DRSettingsManager.h"
#include "UI/DRUpgradeUILibrary.h"
#include "UI/DRUpgradeUIStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Texture2D.h"	// ResolveSkinIcon 의 TSoftObjectPtr<UTexture2D>::LoadSynchronous 가 완전한 타입을 요구한다
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
	// v0 = 방금 생성된 새 세이브, v1 = 경험치/레벨(폐기), v2 = 랭크 구매형 업그레이드(폐기),
	// v3 = 슬롯 카테고리 분리(폐기 — 통합 6칸으로 대체).
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

		// 코스메틱도 같은 "깨끗한 초기 상태" 정책을 따른다.
		// (지금은 버전을 올리지 않으므로 이 분기는 타지 않는다 — 나중에 v5 가 생겨도 반쪽 상태가 남지 않게 함)
		CurrentSaveGame->EarnedAchievements.Reset();
		CurrentSaveGame->Cosmetics.Reset();
		CurrentSaveGame->PendingSteamAchievements.Reset();

		CurrentSaveGame->SaveVersion = UDRSaveGame::CurrentSaveVersion;
	}

	// 누락된 로봇 키를 빈 상태로 lazy 초기화 (enum 추가 시 자동 대응)
	const int32 ClassCount = static_cast<int32>(EPlayerCharacterClass::Count);
	for (int32 Index = 0; Index < ClassCount; ++Index)
	{
		const EPlayerCharacterClass CharacterClass = static_cast<EPlayerCharacterClass>(Index);

		CurrentSaveGame->ClassUpgrades.FindOrAdd(CharacterClass);

		// 코스메틱도 같은 방식. EnsureSize 가 카테고리 enum 확장까지 흡수한다.
		CurrentSaveGame->Cosmetics.FindOrAdd(CharacterClass).EnsureSize();
	}

	// 카탈로그에서 사라졌거나 소속이 어긋난 장착 스킨을 되돌린다.
	// 칩과 달리 재화가 오가지 않으므로 환급 처리가 없다.
	if (GetCosmeticCatalog())
	{
		bool bCosmeticChanged = false;

		for (auto& Pair : CurrentSaveGame->Cosmetics)
		{
			if (!SanitizeCosmeticState(Pair.Key, Pair.Value)) continue;

			bCosmeticChanged = true;
			UE_LOG(LogDR, Warning,
				TEXT("[Cosmetic] 카탈로그 변경 감지 — 클래스 %d 의 장착 스킨을 정화했습니다."),
				static_cast<int32>(Pair.Key));
		}

		if (bCosmeticChanged)
		{
			SaveProgress();
		}
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

	const int32 MaxSlots = ProgressionConfig->GetMaxSlotCount();

	TArray<FName>& Slots = State.SlotChips;
	if (Slots.Num() != MaxSlots)
	{
		Slots.SetNum(MaxSlots);
		bModified = true;
	}

	// 해금 수 클램프
	const int32 StoredUnlocked = State.UnlockedSlots;
	const int32 Unlocked = FMath::Clamp(StoredUnlocked, 0, MaxSlots);
	if (Unlocked != StoredUnlocked)
	{
		State.UnlockedSlots = Unlocked;
		bModified = true;
	}

	// 남은 해금 슬롯의 현재 정가 합계 (SpentCurrency 재산정 기준)
	for (int32 SlotNumber = 1; SlotNumber <= Unlocked; ++SlotNumber)
	{
		const int32 Cost = ProgressionConfig->GetSlotUnlockCost(SlotNumber);
		if (Cost > 0)
		{
			RebuiltSpent += Cost;
		}
	}

	// 1차 패스: 칸 단위 유효성 (정의 존재 / 소속 / 해금 단계 / 해금 범위).
	// 카테고리 일치 검사는 없다 — 어떤 칩이든 어느 칸에나 유효하다.
	for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
	{
		const FName ChipId = Slots[SlotIndex];
		if (ChipId.IsNone()) continue;

		const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
		const bool bValid = Chip
			&& Chip->OwnerClass == CharacterClass
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

void UDRGameInstance::DebugSetUpgradeSystemUnlocked(bool bUnlocked)
{
	UDRSaveGame* Save = GetOrLoadSaveGame();
	if (!Save || Save->bUpgradeSystemUnlocked == bUnlocked) return;

	Save->bUpgradeSystemUnlocked = bUnlocked;
	SaveProgress();

	UE_LOG(LogDR, Warning, TEXT("[Progression] (치트) 업그레이드 시스템 해금 상태를 %s 로 바꿨습니다."),
		bUnlocked ? TEXT("해금") : TEXT("잠김"));

	// 해금 방향일 때만 알린다 — 구독자(장치 프롬프트 등)는 "열렸다"만 처리한다
	if (bUnlocked)
	{
		OnUpgradeSystemUnlocked.Broadcast();
	}
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
		const int32 MaxSlots = ProgressionConfig->GetMaxSlotCount();
		if (State.SlotChips.Num() != MaxSlots)
		{
			State.SlotChips.SetNum(MaxSlots);
		}
	}

	return State;
}

const FDRClassUpgradeState* UDRGameInstance::FindClassState(EPlayerCharacterClass CharacterClass) const
{
	return CurrentSaveGame ? CurrentSaveGame->ClassUpgrades.Find(CharacterClass) : nullptr;
}

// ========================= 슬롯 =========================

int32 UDRGameInstance::GetMaxSlotCount() const
{
	return ProgressionConfig ? ProgressionConfig->GetMaxSlotCount() : 0;
}

int32 UDRGameInstance::GetUnlockedSlotCount(EPlayerCharacterClass CharacterClass) const
{
	const FDRClassUpgradeState* State = FindClassState(CharacterClass);
	if (!State) return 0;

	return FMath::Clamp(State->UnlockedSlots, 0, GetMaxSlotCount());
}

int32 UDRGameInstance::GetNextSlotUnlockCost(EPlayerCharacterClass CharacterClass) const
{
	if (!ProgressionConfig) return -1;

	const int32 Unlocked = GetUnlockedSlotCount(CharacterClass);
	return ProgressionConfig->GetSlotUnlockCost(Unlocked + 1);
}

int32 UDRGameInstance::GetClassSpentCurrency(EPlayerCharacterClass CharacterClass) const
{
	const FDRClassUpgradeState* State = FindClassState(CharacterClass);
	return State ? State->SpentCurrency : 0;
}

EDRUpgradeResult UDRGameInstance::CanUnlockSlot(EPlayerCharacterClass CharacterClass) const
{
	if (!ProgressionConfig || !GetChipCatalog() || !CurrentSaveGame) return EDRUpgradeResult::InvalidConfig;

	if (!IsUpgradeSystemUnlocked()) return EDRUpgradeResult::SystemLocked;

	const int32 Unlocked = GetUnlockedSlotCount(CharacterClass);
	if (Unlocked >= GetMaxSlotCount()) return EDRUpgradeResult::AllSlotsUnlocked;

	const int32 Cost = ProgressionConfig->GetSlotUnlockCost(Unlocked + 1);
	if (Cost < 0) return EDRUpgradeResult::AllSlotsUnlocked;

	if (CurrentSaveGame->Currency < Cost) return EDRUpgradeResult::NotEnoughCurrency;

	return EDRUpgradeResult::Success;
}

EDRUpgradeResult UDRGameInstance::UnlockSlot(EPlayerCharacterClass CharacterClass)
{
	if (!GetOrLoadSaveGame()) return EDRUpgradeResult::InvalidConfig;

	const EDRUpgradeResult CheckResult = CanUnlockSlot(CharacterClass);
	if (CheckResult != EDRUpgradeResult::Success) return CheckResult;

	FDRClassUpgradeState& State = FindOrAddClassState(CharacterClass);
	const int32 Unlocked = State.UnlockedSlots;
	const int32 Cost = ProgressionConfig->GetSlotUnlockCost(Unlocked + 1);

	CurrentSaveGame->Currency -= Cost;
	State.SpentCurrency += Cost;
	State.UnlockedSlots = Unlocked + 1;

	SaveProgress();

	OnCurrencyChanged.Broadcast(CurrentSaveGame->Currency);
	OnUpgradesChanged.Broadcast(CharacterClass);

	return EDRUpgradeResult::Success;
}

EDRUpgradeResult UDRGameInstance::RefundSlot(EPlayerCharacterClass CharacterClass, int32& OutRefunded)
{
	OutRefunded = 0;

	if (!GetOrLoadSaveGame() || !ProgressionConfig) return EDRUpgradeResult::InvalidConfig;

	if (!ProgressionConfig->bAllowSlotRefund) return EDRUpgradeResult::RefundDisabled;

	FDRClassUpgradeState& State = FindOrAddClassState(CharacterClass);
	const int32 Unlocked = State.UnlockedSlots;
	if (Unlocked <= 0) return EDRUpgradeResult::NoSlotToRefund;

	// 순차 해금의 역순 — 마지막으로 해금한 칸이 비어 있어야 환불할 수 있다
	const int32 SlotIndex = Unlocked - 1;
	if (State.SlotChips.IsValidIndex(SlotIndex) && !State.SlotChips[SlotIndex].IsNone())
	{
		return EDRUpgradeResult::SlotOccupied;
	}

	const int32 Cost = FMath::Max(0, ProgressionConfig->GetSlotUnlockCost(Unlocked));
	const int32 Deduct = FMath::Min(Cost, State.SpentCurrency);
	const int32 Refund = FMath::FloorToInt(Deduct * GetRefundRatio());

	State.SpentCurrency -= Deduct;
	State.UnlockedSlots = Unlocked - 1;
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

TArray<FName> UDRGameInstance::GetSlotAssignments(EPlayerCharacterClass CharacterClass) const
{
	TArray<FName> Result;
	Result.SetNum(GetMaxSlotCount());

	if (const FDRClassUpgradeState* State = FindClassState(CharacterClass))
	{
		const int32 Count = FMath::Min(Result.Num(), State->SlotChips.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			Result[Index] = State->SlotChips[Index];
		}
	}

	return Result;
}

int32 UDRGameInstance::GetFreeSlotCount(EPlayerCharacterClass CharacterClass) const
{
	const FDRClassUpgradeState* State = FindClassState(CharacterClass);
	return State ? State->CountFreeSlots() : 0;
}

UDRUpgradeUIStyle* UDRGameInstance::GetUpgradeUIStyle() const
{
	return UpgradeUIStyle;
}

UTexture2D* UDRGameInstance::ResolveChipIcon(UTexture2D* ChipIcon) const
{
	if (ChipIcon) return ChipIcon;

	// 카탈로그에 아이콘이 없으면 스타일의 기본 아이콘으로 대체한다.
	// 둘 다 없으면 nullptr — 위젯은 흰 사각형으로 그린다(설정 누락이 눈에 띄어야 한다).
	return UpgradeUIStyle ? UpgradeUIStyle->DefaultChipIcon : nullptr;
}

int32 UDRGameInstance::GetSlotUnlockCost(int32 SlotIndex) const
{
	if (!ProgressionConfig) return -1;

	// Config 는 1-base 슬롯 번호를 받는다
	return ProgressionConfig->GetSlotUnlockCost(SlotIndex + 1);
}

void UDRGameInstance::GetSlotViewModels(EPlayerCharacterClass CharacterClass,
	TArray<FDRSlotViewModel>& OutSlots) const
{
	OutSlots.Reset();

	const int32 MaxSlots = GetMaxSlotCount();
	if (MaxSlots <= 0) return;

	OutSlots.SetNum(MaxSlots);

	const FDRClassUpgradeState* State = FindClassState(CharacterClass);
	const int32 Unlocked = State ? FMath::Clamp(State->UnlockedSlots, 0, MaxSlots) : 0;

	// 다중 칸 칩 묶음 표시를 위해 "각 칩이 몇 칸을 먹는지"를 먼저 센다
	TMap<FName, int32> OccupiedCounts;
	if (State)
	{
		for (const FName& Id : State->SlotChips)
		{
			if (!Id.IsNone())
			{
				++OccupiedCounts.FindOrAdd(Id);
			}
		}
	}

	// 같은 칩이 몇 번째 칸으로 나왔는지 세는 카운터
	TMap<FName, int32> SeenCounts;

	// 잠긴 칸 중 "지금 살 수 있는" 칸의 사유는 한 번만 계산한다
	const EDRUpgradeResult NextUnlockResult = CanUnlockSlot(CharacterClass);

	for (int32 Index = 0; Index < MaxSlots; ++Index)
	{
		FDRSlotViewModel& ViewModel = OutSlots[Index];
		ViewModel.SlotIndex = Index;

		// ===== 잠긴 칸 (순차 해금이라 항상 뒤쪽에 모인다) =====
		if (Index >= Unlocked)
		{
			ViewModel.State = EDRSlotState::Locked;
			ViewModel.UnlockCost = GetSlotUnlockCost(Index);
			ViewModel.bIsNextUnlockable = (Index == Unlocked);
			ViewModel.UnlockBlockReason = ViewModel.bIsNextUnlockable
				? NextUnlockResult
				: EDRUpgradeResult::PreviousSlotLocked;
			continue;
		}

		const FName ChipId = (State && State->SlotChips.IsValidIndex(Index))
			? State->SlotChips[Index]
			: NAME_None;

		// ===== 해금된 빈 칸 =====
		if (ChipId.IsNone())
		{
			ViewModel.State = EDRSlotState::Empty;
			continue;
		}

		// ===== 점유된 칸 =====
		ViewModel.State = EDRSlotState::Occupied;
		ViewModel.ChipId = ChipId;
		ViewModel.GroupSize = OccupiedCounts.FindRef(ChipId);
		ViewModel.GroupOrder = SeenCounts.FindOrAdd(ChipId)++;

		// 카탈로그에서 삭제된 고아 칩이면 Id 만 채운 채로 남는다(다음 정화 때 비워진다)
		if (const FDRUpgradeChipDefinition* Chip = FindChip(ChipId))
		{
			ViewModel.ChipName = Chip->DisplayName;
			ViewModel.ChipIcon = ResolveChipIcon(Chip->Icon);
			ViewModel.bHasDrawback = Chip->HasDrawback();
			UDRUpgradeUILibrary::MakeShortLabel(*Chip, ViewModel.ShortLabelTop, ViewModel.ShortLabelBottom);
		}
		else
		{
			ViewModel.ChipName = FText::FromName(ChipId);
		}
	}
}

// ========================= 칸 배정 (드래그앤드롭의 토대) =========================

bool UDRGameInstance::ComputeAssignment(const FDRClassUpgradeState& State, int32 RequiredSlots,
	int32 PreferredSlotIndex, TArray<int32>& OutSlotIndices) const
{
	OutSlotIndices.Reset();

	if (RequiredSlots <= 0) return false;

	const int32 Unlocked = FMath::Min(State.UnlockedSlots, State.SlotChips.Num());

	// 1) 드롭한 칸을 우선 사용 (해금됐고 비어 있을 때)
	if (State.SlotChips.IsValidIndex(PreferredSlotIndex)
		&& PreferredSlotIndex < Unlocked
		&& State.SlotChips[PreferredSlotIndex].IsNone())
	{
		OutSlotIndices.Add(PreferredSlotIndex);
	}

	// 2) 모자란 만큼 앞에서부터 빈 칸을 채운다 (연속 칸을 요구하지 않는다 — Plan2.md 6.4)
	for (int32 Index = 0; Index < Unlocked && OutSlotIndices.Num() < RequiredSlots; ++Index)
	{
		if (State.SlotChips[Index].IsNone() && !OutSlotIndices.Contains(Index))
		{
			OutSlotIndices.Add(Index);
		}
	}

	if (OutSlotIndices.Num() < RequiredSlots)
	{
		OutSlotIndices.Reset();
		return false;
	}

	// UI 하이라이트가 칸 번호 순으로 켜지도록 정렬한다
	OutSlotIndices.Sort();
	return true;
}

int32 UDRGameInstance::GetChipSlotCount(const FDRClassUpgradeState& State, FName ChipId) const
{
	if (ChipId.IsNone()) return 0;

	if (const FDRUpgradeChipDefinition* Chip = FindChip(ChipId))
	{
		return FMath::Max(1, Chip->RequiredSlotCount);
	}

	// 카탈로그에서 사라진 고아 칩 — 지금 점유한 칸 수를 그대로 쓴다(이동해도 칸 수가 안 변하게)
	return FMath::Max(1, State.CountOccupiedSlots(ChipId));
}

// ========================= 칩 =========================

bool UDRGameInstance::IsChipUnlocked(EPlayerCharacterClass CharacterClass, FName ChipId) const
{
	const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
	if (!Chip || Chip->OwnerClass != CharacterClass) return false;

	// RequiredSlotTier 를 통합 해금 수와 비교한다 — 칩의 Category 는 보지 않는다.
	return GetUnlockedSlotCount(CharacterClass) >= FMath::Max(1, Chip->RequiredSlotTier);
}

bool UDRGameInstance::IsChipEquipped(EPlayerCharacterClass CharacterClass, FName ChipId) const
{
	const FDRClassUpgradeState* State = FindClassState(CharacterClass);
	if (!State) return false;

	return State->CountOccupiedSlots(ChipId) > 0;
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
	OutFreeSlots = GetFreeSlotCount(CharacterClass);

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

	// 칸 배정은 ComputeAssignment 하나로만 한다 (미리보기 하이라이트와 결과가 어긋나지 않게)
	TArray<int32> Assignment;
	if (!ComputeAssignment(State, RequiredSlots, PreferredSlotIndex, Assignment))
	{
		// CanEquipChip 이 통과했으면 도달할 수 없다. 방어적으로만 남긴다.
		return EDRUpgradeResult::NotEnoughSlots;
	}

	for (const int32 Index : Assignment)
	{
		State.SlotChips[Index] = ChipId;
	}

	SaveProgress();
	OnUpgradesChanged.Broadcast(CharacterClass);

	return EDRUpgradeResult::Success;
}

EDRUpgradeResult UDRGameInstance::UnequipChip(EPlayerCharacterClass CharacterClass, FName ChipId)
{
	if (!GetOrLoadSaveGame()) return EDRUpgradeResult::InvalidConfig;

	// NAME_None 은 "빈 칸"을 뜻하므로 그대로 비교하면 빈 칸 전부와 일치해 버린다.
	// 카탈로그 조회는 하지 않는다 — 카탈로그에서 삭제된 고아 칩도 뺄 수 있어야 한다.
	if (ChipId.IsNone()) return EDRUpgradeResult::NotEquipped;

	FDRClassUpgradeState& State = FindOrAddClassState(CharacterClass);

	bool bRemoved = false;
	for (FName& Slot : State.SlotChips)
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

EDRUpgradeResult UDRGameInstance::UnequipSlot(EPlayerCharacterClass CharacterClass, int32 SlotIndex)
{
	if (!GetOrLoadSaveGame()) return EDRUpgradeResult::InvalidConfig;

	const FDRClassUpgradeState& State = FindOrAddClassState(CharacterClass);

	if (!State.SlotChips.IsValidIndex(SlotIndex) || State.SlotChips[SlotIndex].IsNone())
	{
		return EDRUpgradeResult::NotEquipped;
	}

	// 여러 칸을 점유한 칩이면 전부 해제된다
	return UnequipChip(CharacterClass, State.SlotChips[SlotIndex]);
}

// ========================= 칸 단위 조작 (드래그앤드롭) =========================

EDRUpgradeResult UDRGameInstance::CanEquipChipAtSlot(EPlayerCharacterClass CharacterClass, FName ChipId,
	int32 SlotIndex, int32& OutRequiredSlots, int32& OutFreeSlots) const
{
	// 칩 자체의 자격(설정/해금/소속/중복)은 기존 판정을 그대로 쓴다.
	// NotEnoughSlots 만 "이 칸 기준"으로 다시 판단한다 — 점유 칸은 교체가 가능하기 때문.
	const EDRUpgradeResult BaseResult = CanEquipChip(CharacterClass, ChipId, OutRequiredSlots, OutFreeSlots);
	if (BaseResult != EDRUpgradeResult::Success && BaseResult != EDRUpgradeResult::NotEnoughSlots)
	{
		return BaseResult;
	}

	const FDRClassUpgradeState* State = FindClassState(CharacterClass);
	if (!State) return EDRUpgradeResult::InvalidConfig;

	if (!State->SlotChips.IsValidIndex(SlotIndex)) return EDRUpgradeResult::InvalidConfig;

	// 아직 해금하지 않은 칸에는 놓을 수 없다
	if (SlotIndex >= FMath::Min(State->UnlockedSlots, State->SlotChips.Num()))
	{
		return EDRUpgradeResult::PreviousSlotLocked;
	}

	// 점유 칸이면 "그 칩을 뺀 상태"를 가정하고 배정이 되는지 본다 (= 교체 가능 여부)
	FDRClassUpgradeState Work = *State;
	const FName Occupant = Work.SlotChips[SlotIndex];
	if (!Occupant.IsNone())
	{
		for (FName& Slot : Work.SlotChips)
		{
			if (Slot == Occupant)
			{
				Slot = NAME_None;
			}
		}
		OutFreeSlots = Work.CountFreeSlots();
	}

	TArray<int32> Assignment;
	if (!ComputeAssignment(Work, OutRequiredSlots, SlotIndex, Assignment))
	{
		return EDRUpgradeResult::NotEnoughSlots;
	}

	return EDRUpgradeResult::Success;
}

EDRUpgradeResult UDRGameInstance::SwapOrEquipChip(EPlayerCharacterClass CharacterClass, FName ChipId,
	int32 SlotIndex)
{
	if (!GetOrLoadSaveGame()) return EDRUpgradeResult::InvalidConfig;

	// 칸 배열 길이를 현재 설정에 맞춘 뒤 판정한다(판정과 커밋이 같은 배열을 보게)
	FDRClassUpgradeState& Live = FindOrAddClassState(CharacterClass);

	int32 RequiredSlots = 0;
	int32 FreeSlots = 0;
	const EDRUpgradeResult CheckResult = CanEquipChipAtSlot(CharacterClass, ChipId, SlotIndex,
		RequiredSlots, FreeSlots);
	if (CheckResult != EDRUpgradeResult::Success) return CheckResult;

	// 사본에서 작업하고 성공했을 때만 커밋한다 — 중간 실패로 상태가 깨지지 않게
	FDRClassUpgradeState Work = Live;

	// 목적지를 점유한 칩을 통째로 들어낸다 (다중 칸이면 그 칩의 모든 칸)
	const FName Occupant = Work.SlotChips[SlotIndex];
	if (!Occupant.IsNone())
	{
		for (FName& Slot : Work.SlotChips)
		{
			if (Slot == Occupant)
			{
				Slot = NAME_None;
			}
		}
	}

	TArray<int32> Assignment;
	if (!ComputeAssignment(Work, RequiredSlots, SlotIndex, Assignment))
	{
		// 사본을 버리는 것으로 롤백이 끝난다
		return EDRUpgradeResult::NotEnoughSlots;
	}

	for (const int32 Index : Assignment)
	{
		Work.SlotChips[Index] = ChipId;
	}

	Live = Work;

	SaveProgress();
	OnUpgradesChanged.Broadcast(CharacterClass);

	return EDRUpgradeResult::Success;
}

EDRUpgradeResult UDRGameInstance::MoveChip(EPlayerCharacterClass CharacterClass, int32 FromSlotIndex,
	int32 ToSlotIndex)
{
	if (!GetOrLoadSaveGame()) return EDRUpgradeResult::InvalidConfig;

	FDRClassUpgradeState& Live = FindOrAddClassState(CharacterClass);

	if (!Live.SlotChips.IsValidIndex(FromSlotIndex) || !Live.SlotChips.IsValidIndex(ToSlotIndex))
	{
		return EDRUpgradeResult::InvalidConfig;
	}

	// 제자리 드롭 — 조용히 성공 처리한다(토스트를 띄우면 오히려 시끄럽다)
	if (FromSlotIndex == ToSlotIndex) return EDRUpgradeResult::Success;

	const int32 Unlocked = FMath::Min(Live.UnlockedSlots, Live.SlotChips.Num());
	if (ToSlotIndex >= Unlocked) return EDRUpgradeResult::PreviousSlotLocked;

	const FName Moving = Live.SlotChips[FromSlotIndex];
	if (Moving.IsNone()) return EDRUpgradeResult::NotEquipped;

	const FName Target = Live.SlotChips[ToSlotIndex];

	// 같은 칩의 다른 칸으로 옮기는 경우 — 결과가 동일하므로 아무 것도 하지 않는다
	// (다중 칸 칩 내부에서의 이동. 저장/브로드캐스트를 아끼는 의미도 있다)
	if (Target == Moving) return EDRUpgradeResult::Success;

	const int32 MovingCount = GetChipSlotCount(Live, Moving);
	const int32 TargetCount = GetChipSlotCount(Live, Target);

	// 사본 작업 + 1회 커밋.
	// UnequipChip → EquipChip 으로 엮으면 SaveProgress 와 OnUpgradesChanged 가 두 번씩 나간다.
	FDRClassUpgradeState Work = Live;

	// 두 칩을 모두 들어낸 뒤 서로의 자리를 선호 칸으로 삼아 재배정한다
	for (FName& Slot : Work.SlotChips)
	{
		if (Slot == Moving || (!Target.IsNone() && Slot == Target))
		{
			Slot = NAME_None;
		}
	}

	TArray<int32> MovingAssignment;
	if (!ComputeAssignment(Work, MovingCount, ToSlotIndex, MovingAssignment))
	{
		return EDRUpgradeResult::NotEnoughSlots;
	}
	for (const int32 Index : MovingAssignment)
	{
		Work.SlotChips[Index] = Moving;
	}

	// 자리 교환 — 밀려난 칩을 출발 칸 쪽으로 되돌린다
	if (!Target.IsNone())
	{
		TArray<int32> TargetAssignment;
		if (!ComputeAssignment(Work, TargetCount, FromSlotIndex, TargetAssignment))
		{
			// 크기가 다른 칩끼리는 교환이 성립하지 않을 수 있다. 사본을 버려 원복한다.
			return EDRUpgradeResult::NotEnoughSlots;
		}
		for (const int32 Index : TargetAssignment)
		{
			Work.SlotChips[Index] = Target;
		}
	}

	Live = Work;

	SaveProgress();
	OnUpgradesChanged.Broadcast(CharacterClass);

	return EDRUpgradeResult::Success;
}

void UDRGameInstance::PreviewSlotAssignment(EPlayerCharacterClass CharacterClass, FName ChipId,
	int32 PreferredSlotIndex, TArray<int32>& OutSlotIndices) const
{
	OutSlotIndices.Reset();

	const FDRClassUpgradeState* State = FindClassState(CharacterClass);
	if (!State || ChipId.IsNone()) return;

	FDRClassUpgradeState Work = *State;

	// 이미 장착된 칩이면 자기 칸을 비우고 계산한다 (슬롯 → 슬롯 이동 미리보기)
	for (FName& Slot : Work.SlotChips)
	{
		if (Slot == ChipId)
		{
			Slot = NAME_None;
		}
	}

	// 점유 칸에 놓는 경우 그 칩을 비운다 (교체 미리보기)
	if (Work.SlotChips.IsValidIndex(PreferredSlotIndex) && !Work.SlotChips[PreferredSlotIndex].IsNone())
	{
		const FName Occupant = Work.SlotChips[PreferredSlotIndex];
		for (FName& Slot : Work.SlotChips)
		{
			if (Slot == Occupant)
			{
				Slot = NAME_None;
			}
		}
	}

	ComputeAssignment(Work, GetChipSlotCount(*State, ChipId), PreferredSlotIndex, OutSlotIndices);
}

void UDRGameInstance::MakeChipViewModel(EPlayerCharacterClass CharacterClass, const FDRUpgradeChipDefinition& Chip,
	FDRChipViewModel& OutViewModel) const
{
	OutViewModel = FDRChipViewModel();

	OutViewModel.ChipId = Chip.ChipId;
	OutViewModel.Category = Chip.Category;
	OutViewModel.DisplayName = Chip.DisplayName;
	OutViewModel.EffectSummary = Chip.EffectSummary;
	OutViewModel.Description = Chip.Description;
	OutViewModel.Icon = ResolveChipIcon(Chip.Icon);
	OutViewModel.RequiredSlotCount = FMath::Max(1, Chip.RequiredSlotCount);
	OutViewModel.RequiredSlotTier = FMath::Max(1, Chip.RequiredSlotTier);
	OutViewModel.bEquipped = IsChipEquipped(CharacterClass, Chip.ChipId);
	OutViewModel.bUnlocked = IsChipUnlocked(CharacterClass, Chip.ChipId);
	OutViewModel.bHasDrawback = Chip.HasDrawback();

	// 카드에 크게 찍을 2행 라벨. 슬롯 카드와 같은 규칙을 쓴다(같은 칩이 두 곳에서 다르게 보이지 않게).
	UDRUpgradeUILibrary::MakeShortLabel(Chip, OutViewModel.ShortLabelTop, OutViewModel.ShortLabelBottom);

	if (const UDRChipCatalog* Catalog = GetChipCatalog())
	{
		OutViewModel.TargetSkillName = Catalog->GetSkillDisplayName(Chip.TargetAbilityTag);
	}

	int32 RequiredSlots = 0;
	int32 FreeSlots = 0;
	const EDRUpgradeResult EquipResult = CanEquipChip(CharacterClass, Chip.ChipId, RequiredSlots, FreeSlots);
	OutViewModel.bCanEquipNow = (EquipResult == EDRUpgradeResult::Success);
	OutViewModel.EquipBlockReason = EquipResult;

	for (const FDRUpgradeModifier& Modifier : Chip.Modifiers)
	{
		const FText Detail = Modifier.GetDetailText();
		OutViewModel.DetailLines.Add(Detail);

		// 돌파 칩의 장점/단점은 세부 정보 토글과 무관하게 항상 표시한다
		if (Modifier.bIsDrawback)
		{
			OutViewModel.DrawbackLines.Add(Detail);
		}
		else
		{
			OutViewModel.BenefitLines.Add(Detail);
		}
	}
}

void UDRGameInstance::GetChipViewModels(EPlayerCharacterClass CharacterClass,
	TArray<FDRChipViewModel>& OutViewModels) const
{
	OutViewModels.Reset();

	const UDRChipCatalog* Catalog = GetChipCatalog();
	if (!Catalog) return;

	TArray<FDRUpgradeChipDefinition> ChipDefs;
	Catalog->GetChipsForClass(CharacterClass, ChipDefs);
	OutViewModels.Reserve(ChipDefs.Num());

	for (const FDRUpgradeChipDefinition& Chip : ChipDefs)
	{
		FDRChipViewModel ViewModel;
		MakeChipViewModel(CharacterClass, Chip, ViewModel);
		OutViewModels.Add(MoveTemp(ViewModel));
	}
}

void UDRGameInstance::GetChipViewModelsByCategory(EPlayerCharacterClass CharacterClass, EDRChipCategory Category,
	TArray<FDRChipViewModel>& OutViewModels) const
{
	GetChipViewModels(CharacterClass, OutViewModels);

	// Category 는 표시 필터일 뿐이다 — 장착 판정에는 관여하지 않는다.
	OutViewModels.RemoveAll([Category](const FDRChipViewModel& ViewModel)
	{
		return ViewModel.Category != Category;
	});
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

bool UDRGameInstance::MakeStatPreviewLine(EDRUpgradeStat Stat, const FGameplayTag& SkillTag,
	const FDRResolvedStat& Current, const FDRResolvedStat& Preview, FDRStatPreviewLine& OutLine) const
{
	const float DeltaFlat = Preview.Flat - Current.Flat;
	const float DeltaPercent = Preview.Percent - Current.Percent;

	// 변화가 없는 항목은 줄을 만들지 않는다 (UI 가 "안 변한 것"으로 도배되지 않게)
	if (FMath::IsNearlyZero(DeltaFlat) && FMath::IsNearlyZero(DeltaPercent))
	{
		return false;
	}

	OutLine = FDRStatPreviewLine();
	OutLine.Stat = Stat;
	OutLine.SkillTag = SkillTag;
	OutLine.CurrentFlat = Current.Flat;
	OutLine.CurrentPercent = Current.Percent;
	OutLine.PreviewFlat = Preview.Flat;
	OutLine.PreviewPercent = Preview.Percent;
	OutLine.DeltaText = UDRUpgradeUILibrary::MakeDeltaText(DeltaFlat, DeltaPercent);

	// 라벨: 전역이면 스탯명만, 스킬 대상이면 "스킬명 · 스탯명"
	const FText StatName = UDRUpgradeUILibrary::GetStatDisplayName(Stat);
	if (SkillTag.IsValid())
	{
		FText SkillName = FText::FromName(SkillTag.GetTagName());
		if (const UDRChipCatalog* Catalog = GetChipCatalog())
		{
			SkillName = Catalog->GetSkillDisplayName(SkillTag);
		}
		OutLine.Label = FText::Format(NSLOCTEXT("DRUpgradeUI", "PreviewSkillLabelFmt", "{0} · {1}"),
			SkillName, StatName);
	}
	else
	{
		OutLine.Label = StatName;
	}

	// 좋아지는 변화인지 나빠지는 변화인지.
	// 받는 피해/물 소모/쿨다운은 낮을수록 좋으므로 부호 해석을 뒤집는다.
	const float Direction = DeltaFlat + DeltaPercent;
	OutLine.bIsWorse = DRIsLowerBetter(Stat) ? (Direction > 0.f) : (Direction < 0.f);

	return true;
}

void UDRGameInstance::GetStatPreview(EPlayerCharacterClass CharacterClass, FName AddChipId, FName RemoveChipId,
	TArray<FDRStatPreviewLine>& OutLines) const
{
	OutLines.Reset();

	FDRUpgradeRuntime Current;
	FDRUpgradeRuntime Preview;
	BuildUpgradeRuntime(CharacterClass, Current);
	BuildPreviewRuntime(CharacterClass, AddChipId, RemoveChipId, Preview);

	Current.EnsureSize();
	Preview.EnsureSize();

	// 1) 캐릭터 단위 스탯 + "모든 스킬" 성분
	const int32 StatCount = static_cast<int32>(EDRUpgradeStat::Count);
	for (int32 StatIndex = 0; StatIndex < StatCount; ++StatIndex)
	{
		const EDRUpgradeStat Stat = static_cast<EDRUpgradeStat>(StatIndex);

		FDRStatPreviewLine Line;
		if (MakeStatPreviewLine(Stat, FGameplayTag(),
			Current.GlobalStats[StatIndex], Preview.GlobalStats[StatIndex], Line))
		{
			OutLines.Add(MoveTemp(Line));
		}
	}

	// 2) 스킬별 성분 — 양쪽 런타임의 태그 합집합을 훑는다
	//    (칩을 빼면 Preview 에서 태그가 통째로 사라지므로 Current 쪽도 봐야 한다)
	TSet<FGameplayTag> SkillTags;
	for (const TPair<FGameplayTag, FDRSkillStatBlock>& Pair : Current.SkillStats)
	{
		SkillTags.Add(Pair.Key);
	}
	for (const TPair<FGameplayTag, FDRSkillStatBlock>& Pair : Preview.SkillStats)
	{
		SkillTags.Add(Pair.Key);
	}

	for (const FGameplayTag& SkillTag : SkillTags)
	{
		const FDRSkillStatBlock* CurrentBlock = Current.SkillStats.Find(SkillTag);
		const FDRSkillStatBlock* PreviewBlock = Preview.SkillStats.Find(SkillTag);

		for (int32 StatIndex = 0; StatIndex < StatCount; ++StatIndex)
		{
			const EDRUpgradeStat Stat = static_cast<EDRUpgradeStat>(StatIndex);
			if (!DRIsSkillStat(Stat)) continue;

			const FDRResolvedStat CurrentStat = (CurrentBlock && CurrentBlock->Stats.IsValidIndex(StatIndex))
				? CurrentBlock->Stats[StatIndex] : FDRResolvedStat();
			const FDRResolvedStat PreviewStat = (PreviewBlock && PreviewBlock->Stats.IsValidIndex(StatIndex))
				? PreviewBlock->Stats[StatIndex] : FDRResolvedStat();

			FDRStatPreviewLine Line;
			if (MakeStatPreviewLine(Stat, SkillTag, CurrentStat, PreviewStat, Line))
			{
				OutLines.Add(MoveTemp(Line));
			}
		}
	}
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

	// 보상 반영 전의 해금 상태를 찍어 둔다 — 아래에서 "이번에 새로 열린 스킨"을 계산하는 기준이다.
	// FDRStageRewardResult 에는 "새로 달성한 업적" 필드가 없으므로 스냅샷 비교가 유일한 정확한 방법이다.
	const TSet<FName> UnlockedBefore = SnapshotUnlockedSkins();

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

		// ★해금 원장은 재화 지급 여부와 무관하게 항상 기록한다★
		// 아래 중복 방지 continue 보다 ★반드시 앞★ 이어야 한다 —
		// 뒤에 두면 이미 재화를 받은 업적이 코스메틱 해금 판정에서 누락된다. (Plan.md 4.5 / 5.1)
		Save->EarnedAchievements.AddUnique(AchievementId);

		// 스팀 미러링 큐. 연동(Plan.md M6) 전까지는 쌓아두기만 한다.
		if (!Def->SteamApiName.IsEmpty())
		{
			Save->PendingSteamAchievements.AddUnique(AchievementId);
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

	// 이번 보상으로 새로 열린 스킨을 알린다 (해금 토스트). SaveProgress 이후여야 한다.
	BroadcastNewlyUnlockedSkins(UnlockedBefore);

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

// ========================= 코스메틱 / 옷장 (Plan.md 5.1 / 15.11) =========================

UDRCosmeticCatalog* UDRGameInstance::GetCosmeticCatalog() const
{
	return ProgressionConfig ? ProgressionConfig->CosmeticCatalog : nullptr;
}

const FDRSkinDefinition* UDRGameInstance::FindSkinDef(FName SkinId) const
{
	const UDRCosmeticCatalog* Catalog = GetCosmeticCatalog();
	return Catalog ? Catalog->FindSkin(SkinId) : nullptr;
}

bool UDRGameInstance::IsSkinUnlocked(FName SkinId) const
{
	// NAME_None = "기본"(장착 해제) 칸. 언제나 고를 수 있어야 한다.
	if (SkinId.IsNone()) return true;

	const FDRSkinDefinition* Def = FindSkinDef(SkinId);
	if (!Def) return false;

	// 조건이 없으면 기본 제공
	if (Def->RequiredAchievements.Num() == 0) return true;

	if (!CurrentSaveGame) return false;

	// 판정 입력 = EarnedAchievements ∪ ClaimedRewards
	//   EarnedAchievements : 업적 달성 기록 (로컬 + 향후 스팀 역매핑)
	//   ClaimedRewards     : "StageClear.<StageId>" 류도 해금 조건으로 쓸 수 있게 함께 본다
	auto HasKey = [this](const FName& Id)
	{
		return CurrentSaveGame->EarnedAchievements.Contains(Id)
			|| CurrentSaveGame->ClaimedRewards.Contains(Id);
	};

	if (Def->bRequireAll)
	{
		for (const FName& Id : Def->RequiredAchievements)
		{
			if (!HasKey(Id)) return false;
		}
		return true;
	}

	for (const FName& Id : Def->RequiredAchievements)
	{
		if (HasKey(Id)) return true;
	}
	return false;
}

FDRClassCosmeticState& UDRGameInstance::FindOrAddCosmeticState(EPlayerCharacterClass CharacterClass)
{
	// 호출부가 GetOrLoadSaveGame() 으로 유효성을 먼저 확인한다 (FindOrAddClassState 와 같은 계약)
	FDRClassCosmeticState& State = CurrentSaveGame->Cosmetics.FindOrAdd(CharacterClass);
	State.EnsureSize();
	return State;
}

const FDRClassCosmeticState* UDRGameInstance::FindCosmeticState(EPlayerCharacterClass CharacterClass) const
{
	return CurrentSaveGame ? CurrentSaveGame->Cosmetics.Find(CharacterClass) : nullptr;
}

FName UDRGameInstance::GetEquippedSkin(EPlayerCharacterClass CharacterClass,
	EDRCosmeticCategory Category) const
{
	if (!DRIsValidCosmeticCategory(Category)) return NAME_None;

	const FDRClassCosmeticState* State = FindCosmeticState(CharacterClass);
	return State ? State->Get(Category) : NAME_None;
}

TArray<FName> UDRGameInstance::GetEquippedSkins(EPlayerCharacterClass CharacterClass) const
{
	// 길이는 항상 카테고리 수다 — 호출부(서버 보고/외형 적용)가 인덱스로 접근하기 때문이다.
	TArray<FName> Result;
	Result.SetNum(DRGetCosmeticCategoryCount());

	if (const FDRClassCosmeticState* State = FindCosmeticState(CharacterClass))
	{
		for (int32 Index = 0; Index < Result.Num(); ++Index)
		{
			if (State->EquippedByCategory.IsValidIndex(Index))
			{
				Result[Index] = State->EquippedByCategory[Index];
			}
		}
	}

	return Result;
}

bool UDRGameInstance::EquipSkin(EPlayerCharacterClass CharacterClass, EDRCosmeticCategory Category,
	FName SkinId)
{
	if (!DRIsValidCosmeticCategory(Category)) return false;

	UDRSaveGame* Save = GetOrLoadSaveGame();
	if (!Save) return false;

	// NAME_None 은 "기본 외형으로 되돌리기"라 검증 없이 통과시킨다
	if (!SkinId.IsNone())
	{
		const FDRSkinDefinition* Def = FindSkinDef(SkinId);
		if (!Def)
		{
			UE_LOG(LogDR, Warning, TEXT("[Cosmetic] 카탈로그에 없는 스킨 '%s' 장착 시도 — 무시합니다."),
				*SkinId.ToString());
			return false;
		}

		if (Def->OwnerClass != CharacterClass || Def->Category != Category) return false;
		if (!IsSkinUnlocked(SkinId)) return false;
	}

	FDRClassCosmeticState& State = FindOrAddCosmeticState(CharacterClass);

	const int32 Index = static_cast<int32>(Category);
	if (!State.EquippedByCategory.IsValidIndex(Index)) return false;

	// 이미 같은 상태면 저장도 브로드캐스트도 하지 않는다 (같은 칸 재클릭)
	if (State.EquippedByCategory[Index] == SkinId) return true;

	State.EquippedByCategory[Index] = SkinId;
	SaveProgress();

	OnCosmeticsChanged.Broadcast(CharacterClass);
	return true;
}

bool UDRGameInstance::ClearAllSkins(EPlayerCharacterClass CharacterClass)
{
	UDRSaveGame* Save = GetOrLoadSaveGame();
	if (!Save) return false;

	FDRClassCosmeticState& State = FindOrAddCosmeticState(CharacterClass);

	bool bChanged = false;
	for (FName& Id : State.EquippedByCategory)
	{
		if (!Id.IsNone())
		{
			Id = NAME_None;
			bChanged = true;
		}
	}

	if (!bChanged) return false;

	SaveProgress();
	OnCosmeticsChanged.Broadcast(CharacterClass);
	return true;
}

void UDRGameInstance::GetSkinViewModels(EPlayerCharacterClass CharacterClass,
	EDRCosmeticCategory Category, TArray<FDRSkinViewModel>& OutViewModels) const
{
	OutViewModels.Reset();

	if (!DRIsValidCosmeticCategory(Category)) return;

	const FName Equipped = GetEquippedSkin(CharacterClass, Category);

	// ★인덱스 0 은 항상 "기본"(장착 해제) 칸★ — 되돌릴 방법이 없으면 안 된다.
	{
		FDRSkinViewModel None;
		None.SkinId = NAME_None;
		None.Category = Category;
		None.DisplayName = NSLOCTEXT("DRCosmetic", "SkinNoneName", "기본");
		None.bUnlocked = true;
		None.bEquipped = Equipped.IsNone();
		None.bIsNoneSlot = true;
		OutViewModels.Add(MoveTemp(None));
	}

	const UDRCosmeticCatalog* Catalog = GetCosmeticCatalog();
	if (!Catalog) return;

	TArray<const FDRSkinDefinition*> Defs;
	Catalog->GetSkinsForCategory(CharacterClass, Category, Defs);

	for (const FDRSkinDefinition* Def : Defs)
	{
		if (!Def) continue;

		FDRSkinViewModel VM;
		VM.SkinId = Def->SkinId;
		VM.Category = Def->Category;
		VM.DisplayName = Def->DisplayName.IsEmpty() ? FText::FromName(Def->SkinId) : Def->DisplayName;
		VM.Description = Def->Description;
		VM.PreviewIcon = ResolveSkinIcon(Def->PreviewIcon);
		VM.bUnlocked = IsSkinUnlocked(Def->SkinId);
		VM.bEquipped = (Equipped == Def->SkinId);
		VM.bIsNoneSlot = false;

		if (!VM.bUnlocked)
		{
			VM.UnlockHint = Def->HowToUnlock.IsEmpty() ? MakeDefaultUnlockHint(*Def) : Def->HowToUnlock;
		}

		OutViewModels.Add(MoveTemp(VM));
	}
}

bool UDRGameInstance::HasAnySkinAvailable(EPlayerCharacterClass CharacterClass) const
{
	const UDRCosmeticCatalog* Catalog = GetCosmeticCatalog();
	if (!Catalog) return false;

	for (const FDRSkinDefinition& Skin : Catalog->Skins)
	{
		if (!Skin.SkinId.IsNone() && Skin.OwnerClass == CharacterClass) return true;
	}
	return false;
}

bool UDRGameInstance::SanitizeCosmeticState(EPlayerCharacterClass CharacterClass,
	FDRClassCosmeticState& State) const
{
	const UDRCosmeticCatalog* Catalog = GetCosmeticCatalog();

	// 카탈로그가 없으면 아무 것도 판단할 수 없다 — 데이터를 건드리지 않는다
	// (SanitizeClassState 가 ProgressionConfig 에 대해 하는 판단과 같다)
	if (!Catalog) return false;

	State.EnsureSize();
	return Catalog->SanitizeLoadout(State.EquippedByCategory, CharacterClass);
}

UTexture2D* UDRGameInstance::ResolveSkinIcon(const TSoftObjectPtr<UTexture2D>& Icon) const
{
	if (Icon.IsNull()) return nullptr;

	// 옷장은 로비 전용 화면이고 아이콘 수가 적어 동기 로드로 충분하다 (ResolveChipIcon 과 같은 판단).
	// 아이콘이 아직 없는 스킨은 nullptr 이 그대로 뷰모델에 실리고, 위젯이 이미지를 숨긴다.
	return Icon.LoadSynchronous();
}

FText UDRGameInstance::MakeDefaultUnlockHint(const FDRSkinDefinition& Def) const
{
	if (Def.RequiredAchievements.Num() == 0) return FText::GetEmpty();

	TArray<FText> Names;
	Names.Reserve(Def.RequiredAchievements.Num());

	for (const FName& Id : Def.RequiredAchievements)
	{
		const FDRAchievementDef* AchDef = ProgressionConfig ? ProgressionConfig->FindAchievement(Id) : nullptr;

		Names.Add(AchDef && !AchDef->DisplayName.IsEmpty()
			? AchDef->DisplayName
			: FText::FromName(Id));
	}

	const FText Delimiter = Def.bRequireAll
		? NSLOCTEXT("DRCosmetic", "UnlockJoinAll", ", ")
		: NSLOCTEXT("DRCosmetic", "UnlockJoinAny", " 또는 ");

	return FText::Format(
		NSLOCTEXT("DRCosmetic", "UnlockHintFormat", "달성 필요: {0}"),
		FText::Join(Delimiter, Names));
}

TSet<FName> UDRGameInstance::SnapshotUnlockedSkins() const
{
	TSet<FName> Unlocked;

	const UDRCosmeticCatalog* Catalog = GetCosmeticCatalog();
	if (!Catalog) return Unlocked;

	for (const FDRSkinDefinition& Skin : Catalog->Skins)
	{
		if (Skin.SkinId.IsNone()) continue;
		if (IsSkinUnlocked(Skin.SkinId))
		{
			Unlocked.Add(Skin.SkinId);
		}
	}

	return Unlocked;
}

void UDRGameInstance::BroadcastNewlyUnlockedSkins(const TSet<FName>& BeforeUnlocked)
{
	const UDRCosmeticCatalog* Catalog = GetCosmeticCatalog();
	if (!Catalog) return;

	for (const FDRSkinDefinition& Skin : Catalog->Skins)
	{
		if (Skin.SkinId.IsNone()) continue;
		if (BeforeUnlocked.Contains(Skin.SkinId)) continue;
		if (!IsSkinUnlocked(Skin.SkinId)) continue;

		UE_LOG(LogDR, Log, TEXT("[Cosmetic] 스킨 해금: %s"), *Skin.SkinId.ToString());
		OnSkinUnlocked.Broadcast(Skin.SkinId);
	}
}

void UDRGameInstance::DebugGrantAchievement(FName AchievementId)
{
	if (AchievementId.IsNone()) return;

	UDRSaveGame* Save = GetOrLoadSaveGame();
	if (!Save) return;

	const TSet<FName> Before = SnapshotUnlockedSkins();

	Save->EarnedAchievements.AddUnique(AchievementId);
	SaveProgress();

	UE_LOG(LogDR, Warning, TEXT("[Cheat] 업적 '%s' 달성 처리."), *AchievementId.ToString());

	BroadcastNewlyUnlockedSkins(Before);

	// 해금은 로봇을 가리지 않으므로 전 클래스에 갱신을 알린다 (옷장 화면이 자기 클래스만 걸러 쓴다)
	const int32 ClassCount = static_cast<int32>(EPlayerCharacterClass::Count);
	for (int32 Index = 0; Index < ClassCount; ++Index)
	{
		OnCosmeticsChanged.Broadcast(static_cast<EPlayerCharacterClass>(Index));
	}
}

void UDRGameInstance::DebugUnlockAllSkins()
{
	UDRSaveGame* Save = GetOrLoadSaveGame();
	const UDRCosmeticCatalog* Catalog = GetCosmeticCatalog();
	if (!Save || !Catalog)
	{
		UE_LOG(LogDR, Warning, TEXT("[Cheat] 코스메틱 카탈로그가 없어 전체 해금을 할 수 없습니다."));
		return;
	}

	const TSet<FName> Before = SnapshotUnlockedSkins();

	for (const FDRSkinDefinition& Skin : Catalog->Skins)
	{
		for (const FName& Id : Skin.RequiredAchievements)
		{
			Save->EarnedAchievements.AddUnique(Id);
		}
	}

	SaveProgress();

	UE_LOG(LogDR, Warning, TEXT("[Cheat] 코스메틱 전체 해금 (업적 %d개 보유)."),
		Save->EarnedAchievements.Num());

	BroadcastNewlyUnlockedSkins(Before);

	const int32 ClassCount = static_cast<int32>(EPlayerCharacterClass::Count);
	for (int32 Index = 0; Index < ClassCount; ++Index)
	{
		OnCosmeticsChanged.Broadcast(static_cast<EPlayerCharacterClass>(Index));
	}
}
