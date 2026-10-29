// Copyright DaeRune


#include "Player/DRCheatManager.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Game/DRGameInstance.h"
#include "Game/DRCosmeticCatalog.h"
#include "UI/DRUpgradeUILibrary.h"
#include "DaeRune/DRLogChannels.h"

// 치트 매니저는 쉬핑에서 만들어지지 않으므로(UE_WITH_CHEAT_MANAGER) 아래 함수들에 따로 쉬핑 가드를 두지 않는다.

ADRPlayerController* UDRCheatManager::GetDRPlayerController() const
{
	return Cast<ADRPlayerController>(GetOuterAPlayerController());
}

UDRGameInstance* UDRCheatManager::GetDRGameInstance() const
{
	UDRGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance<UDRGameInstance>() : nullptr;
	if (!GI)
	{
		UE_LOG(LogDR, Warning, TEXT("[Cheat] UDRGameInstance 를 찾지 못했습니다."));
	}
	return GI;
}

EPlayerCharacterClass UDRCheatManager::GetViewedUpgradeClass() const
{
	// 업그레이드 화면과 같은 기준으로 클래스를 고른다(UDRUpgradeScreenWidget::GetViewedClass 와 동일)
	const ADRPlayerController* PC = GetDRPlayerController();
	if (const ADRPlayerState* PS = PC ? PC->GetPlayerState<ADRPlayerState>() : nullptr)
	{
		return PS->GetSelectedPlayerClass();
	}
	return EPlayerCharacterClass::Gardener;
}

// ========================= 업그레이드 =========================

void UDRCheatManager::DRUnlockUpgrade()
{
	if (UDRGameInstance* GI = GetDRGameInstance())
	{
		GI->UnlockUpgradeSystem();
		UE_LOG(LogDR, Warning, TEXT("[Cheat] 업그레이드 시스템 해금됨 (현재 재화 %d)"), GI->GetCurrency());
	}
}

void UDRCheatManager::DRAddCurrency(int32 Amount)
{
	if (UDRGameInstance* GI = GetDRGameInstance())
	{
		GI->AddCurrency(Amount);
		UE_LOG(LogDR, Warning, TEXT("[Cheat] 재화 %d 지급 → 현재 %d"), Amount, GI->GetCurrency());
	}
}

void UDRCheatManager::DRLockUpgrade()
{
	if (UDRGameInstance* GI = GetDRGameInstance())
	{
		GI->DebugSetUpgradeSystemUnlocked(false);
	}
}

void UDRCheatManager::DRUnlockSlot(int32 Count)
{
	UDRGameInstance* GI = GetDRGameInstance();
	if (!GI) return;

	// 시스템이 잠겨 있으면 CanUnlockSlot 이 SystemLocked 로 막는다 — 먼저 열어 준다
	GI->UnlockUpgradeSystem();

	const EPlayerCharacterClass Class = GetViewedUpgradeClass();

	for (int32 Index = 0; Index < FMath::Max(1, Count); ++Index)
	{
		// 비용이 모자라면 그만큼 채워 넣고 다시 시도한다 (치트니까 지갑은 신경 쓰지 않는다)
		if (GI->CanUnlockSlot(Class) == EDRUpgradeResult::NotEnoughCurrency)
		{
			GI->AddCurrency(FMath::Max(1000, GI->GetNextSlotUnlockCost(Class)));
		}

		const EDRUpgradeResult Result = GI->UnlockSlot(Class);
		if (Result != EDRUpgradeResult::Success)
		{
			UE_LOG(LogDR, Warning, TEXT("[Cheat] 슬롯 해금 중단 (%d번째): %s"),
				Index + 1, *UDRUpgradeUILibrary::GetUpgradeResultText(Result).ToString());
			break;
		}
	}

	DRDumpUpgrade();
}

void UDRCheatManager::DRDumpUpgrade()
{
	const UDRGameInstance* GI = GetDRGameInstance();
	if (!GI) return;

	const EPlayerCharacterClass Class = GetViewedUpgradeClass();

	UE_LOG(LogDR, Warning,
		TEXT("[Cheat] 진행도: 시스템=%s / 재화=%d / 클래스=%d / 해금슬롯=%d/%d / 빈칸=%d"),
		GI->IsUpgradeSystemUnlocked() ? TEXT("해금") : TEXT("잠김"),
		GI->GetCurrency(), static_cast<int32>(Class),
		GI->GetUnlockedSlotCount(Class), GI->GetMaxSlotCount(),
		GI->GetFreeSlotCount(Class));
}

void UDRCheatManager::DRResetDefaultChips()
{
	UDRGameInstance* GI = GetDRGameInstance();
	if (!GI) return;

	GI->DebugResetDefaultLoadout();

	// 세이브만 바꿔서는 이미 스폰된 캐릭터에 반영되지 않는다 — 서버에 다시 보고해야
	// PlayerState 의 EquippedChips 가 갱신되고 HUD/스탯이 따라온다.
	if (ADRPlayerController* PC = GetDRPlayerController())
	{
		PC->ReportUpgradeLoadout();
	}

	UE_LOG(LogDR, Warning, TEXT("[Cheat] 기본 장착 칩을 다시 적용하고 서버에 재보고했습니다."));
}

// ========================= 코스메틱 =========================

void UDRCheatManager::DRUnlockSkins()
{
	if (UDRGameInstance* GI = GetDRGameInstance())
	{
		GI->DebugUnlockAllSkins();
	}
}

void UDRCheatManager::DROpenWardrobe()
{
	if (ADRPlayerController* PC = GetDRPlayerController())
	{
		PC->OpenWardrobeScreen();
	}
}

void UDRCheatManager::DRDumpCosmetic()
{
	const UDRGameInstance* GI = GetDRGameInstance();
	if (!GI) return;

	const EPlayerCharacterClass Class = GetViewedUpgradeClass();
	const TArray<FName> Equipped = GI->GetEquippedSkins(Class);

	FString Line;
	for (int32 Index = 0; Index < Equipped.Num(); ++Index)
	{
		Line += FString::Printf(TEXT("%s=%s "),
			*StaticEnum<EDRCosmeticCategory>()->GetNameStringByValue(Index),
			*Equipped[Index].ToString());
	}

	UE_LOG(LogDR, Warning, TEXT("[Cheat] 코스메틱: 클래스=%d / 카탈로그=%s / %s"),
		static_cast<int32>(Class),
		GI->GetCosmeticCatalog() ? TEXT("지정됨") : TEXT("★미지정★"),
		*Line);
}

// ========================= 스테이지 선택 =========================

void UDRCheatManager::DROpenStageSelect()
{
	if (ADRPlayerController* PC = GetDRPlayerController())
	{
		PC->OpenStageSelectScreen();
	}
}

void UDRCheatManager::DRStageState(const FString& StageId, const FString& State)
{
	UDRGameInstance* GI = GetDRGameInstance();
	if (!GI) return;

	const FName Id = StageId.Equals(TEXT("all"), ESearchCase::IgnoreCase) ? NAME_None : FName(*StageId);

	if (State.Equals(TEXT("reset"), ESearchCase::IgnoreCase) || State.IsEmpty())
	{
		GI->DebugClearStageStateOverride(Id);
	}
	else if (Id.IsNone())
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] 'all' 은 reset 과만 쓸 수 있습니다."));
		return;
	}
	else if (State.Equals(TEXT("locked"), ESearchCase::IgnoreCase))
	{
		GI->DebugSetStageStateOverride(Id, EDRStageSelectState::Locked);
	}
	else if (State.Equals(TEXT("normal"), ESearchCase::IgnoreCase) || State.Equals(TEXT("available"), ESearchCase::IgnoreCase))
	{
		GI->DebugSetStageStateOverride(Id, EDRStageSelectState::Available);
	}
	else if (State.Equals(TEXT("cleared"), ESearchCase::IgnoreCase) || State.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
	{
		GI->DebugSetStageStateOverride(Id, EDRStageSelectState::Cleared);
	}
	else
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] 사용법: DRStageState <StageId|all> <locked|normal|cleared|reset>"));
		return;
	}

	UE_LOG(LogDR, Log, TEXT("[StageSelect] %s → %s (디버그 덮어쓰기, 저장 안 함)"), *StageId, *State);
}

// ========================= 사망 · 부활 · 페이즈 =========================

void UDRCheatManager::DRDie()
{
	if (ADRPlayerController* PC = GetDRPlayerController())
	{
		PC->ServerCheatDie();
	}
}

void UDRCheatManager::DRRevive()
{
	if (ADRPlayerController* PC = GetDRPlayerController())
	{
		PC->ServerCheatRevive();
	}
}

void UDRCheatManager::DRSkipPhase()
{
	if (ADRPlayerController* PC = GetDRPlayerController())
	{
		PC->CheatSkipToNextPhase();
	}
}
