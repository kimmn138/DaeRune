// Copyright DaeRune


#include "Game/DRGameInstance.h"
#include "Game/DRSaveGame.h"
#include "Game/DRProgressionConfig.h"
#include "Game/DRGameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DRPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "Internationalization/Internationalization.h"

void UDRGameInstance::Init()
{
	Super::Init();

	// Apply saved UI culture before any widget is constructed.
	// PreferredCulture is "ko" or "en" persisted in GameUserSettings.ini.
	if (UDRGameUserSettings* UserSettings = UDRGameUserSettings::GetDRGameUserSettings())
	{
		const FString& Culture = UserSettings->PreferredCulture;
		if (Culture == TEXT("ko") || Culture == TEXT("en"))
		{
			FInternationalization::Get().SetCurrentLanguageAndLocale(Culture);
		}
		else
		{
			// Unknown / empty culture -> force Korean default and persist it.
			UserSettings->PreferredCulture = TEXT("ko");
			UserSettings->SaveSettings();
			FInternationalization::Get().SetCurrentLanguageAndLocale(TEXT("ko"));
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
	if (!CurrentSaveGame)
	{
		LoadProgress();
	}
	CurrentSaveGame->bHasCompletedTutorial = true;
	SaveProgress();
}

void UDRGameInstance::ResetTutorialProgress()
{
	if (!CurrentSaveGame)
	{
		LoadProgress();
	}
	CurrentSaveGame->bHasCompletedTutorial = false;
	SaveProgress();
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

void UDRGameInstance::EnsureProgressInitialized()
{
	if (!CurrentSaveGame) return;

	// SaveVersion 마이그레이션 (현재는 v1 하나뿐 — 향후 포맷 변경 시 분기 추가).
	if (CurrentSaveGame->SaveVersion != UDRSaveGame::CurrentSaveVersion)
	{
		// TODO: 버전별 변환 로직. 현재는 버전 스탬프만 최신으로 갱신.
		CurrentSaveGame->SaveVersion = UDRSaveGame::CurrentSaveVersion;
	}

	// 누락된 클래스 키를 기본값 {Level=1, XP=0}으로 lazy 초기화.
	const int32 ClassCount = static_cast<int32>(EPlayerCharacterClass::Count);
	for (int32 Index = 0; Index < ClassCount; ++Index)
	{
		CurrentSaveGame->CharacterProgress.FindOrAdd(static_cast<EPlayerCharacterClass>(Index));
	}
}

FDRCharacterProgress UDRGameInstance::GetCharacterProgress(EPlayerCharacterClass CharacterClass) const
{
	if (CurrentSaveGame)
	{
		if (const FDRCharacterProgress* Found = CurrentSaveGame->CharacterProgress.Find(CharacterClass))
		{
			return *Found;
		}
	}
	return FDRCharacterProgress();
}

int32 UDRGameInstance::GetCharacterLevel(EPlayerCharacterClass CharacterClass) const
{
	return GetCharacterProgress(CharacterClass).Level;
}

TArray<int32> UDRGameInstance::GetAllClassLevels() const
{
	const int32 ClassCount = static_cast<int32>(EPlayerCharacterClass::Count);
	TArray<int32> Levels;
	Levels.Init(1, ClassCount);

	if (CurrentSaveGame)
	{
		for (int32 Index = 0; Index < ClassCount; ++Index)
		{
			if (const FDRCharacterProgress* Found = CurrentSaveGame->CharacterProgress.Find(static_cast<EPlayerCharacterClass>(Index)))
			{
				Levels[Index] = Found->Level;
			}
		}
	}
	return Levels;
}

FDRStageProgressResult UDRGameInstance::ApplyStageResult(EPlayerCharacterClass CharacterClass, int32 ClearedPhaseCount, bool bGameClear)
{
	FDRStageProgressResult Result;

	if (!CurrentSaveGame)
	{
		LoadProgress();
	}
	if (!CurrentSaveGame)
	{
		return Result; // 세이브 확보 실패 — 기본값 반환.
	}

	FDRCharacterProgress& Progress = CurrentSaveGame->CharacterProgress.FindOrAdd(CharacterClass);
	Result.StartLevel = Progress.Level;

	// 지급량 계산은 클라 권위: ProgressionConfig가 없으면 지급 0.
	const int32 XpGained = ProgressionConfig ? ProgressionConfig->CalcStageXp(ClearedPhaseCount, bGameClear) : 0;
	Result.XpGained = XpGained;
	Progress.CurrentXP += XpGained;
	Progress.TotalXP += XpGained;

	// 다중 레벨업 루프 (MaxLevel 상한).
	if (ProgressionConfig)
	{
		const int32 MaxLevel = FMath::Max(1, ProgressionConfig->MaxLevel);
		while (Progress.Level < MaxLevel)
		{
			const int32 Needed = ProgressionConfig->GetXpToNextLevel(Progress.Level);
			if (Needed <= 0 || Progress.CurrentXP < Needed)
			{
				break;
			}
			Progress.CurrentXP -= Needed;
			Progress.Level++;
		}

		// MAX 레벨 도달 시 잉여 XP는 캡(누적 표시 방지).
		if (Progress.Level >= MaxLevel)
		{
			Progress.CurrentXP = 0;
		}

		Result.XpForNext = (Progress.Level < MaxLevel) ? ProgressionConfig->GetXpToNextLevel(Progress.Level) : 0;
	}

	Result.EndLevel = Progress.Level;
	Result.bLeveledUp = Result.EndLevel > Result.StartLevel;
	Result.XpIntoCurrent = Progress.CurrentXP;

	SaveProgress();
	return Result;
}

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

void UDRGameInstance::SaveProgress()
{
	if (CurrentSaveGame)
	{
		UGameplayStatics::SaveGameToSlot(CurrentSaveGame, UDRSaveGame::SaveSlotName, UDRSaveGame::UserIndex);
	}
}

