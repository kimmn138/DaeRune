// Copyright DaeRune


#include "Sound/DRSoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/DRSoundDataAsset.h"
#include "Game/DRGameInstance.h"
#include "DRAssetManager.h"

void UDRSoundManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogTemp, Log, TEXT("DRSoundManager::Initialize - Starting..."));

	// AssetManager에서 로드된 SoundData 가져오기
	if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		UE_LOG(LogTemp, Log, TEXT("DRSoundManager: AssetManager is initialized"));
		if (UDRAssetManager* DRAssetManager = Cast<UDRAssetManager>(AssetManager))
		{
			SoundData = DRAssetManager->GetSoundDataAsset();
			UE_LOG(LogTemp, Log, TEXT("DRSoundManager: Got SoundData from AssetManager: %s"), SoundData ? *SoundData->GetName() : TEXT("NULL"));
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("DRSoundManager: AssetManager is not DRAssetManager!"));
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager: AssetManager not initialized yet"));
	}

	// AssetManager에서 못 가져왔으면 직접 로드 시도
	if (!SoundData)
	{
		UE_LOG(LogTemp, Log, TEXT("DRSoundManager: Trying direct load..."));
		static const TCHAR* SoundDataPath = TEXT("/Game/Blueprints/Sound/Data/DA_SoundData.DA_SoundData");
		SoundData = LoadObject<UDRSoundDataAsset>(nullptr, SoundDataPath);
		UE_LOG(LogTemp, Log, TEXT("DRSoundManager: Direct load result: %s"), SoundData ? *SoundData->GetName() : TEXT("NULL"));
	}

	// 마지막으로 GameInstance에서 시도 (에디터 폴백)
	if (!SoundData)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDRGameInstance* DRGI = Cast<UDRGameInstance>(GI))
			{
				SoundData = DRGI->SoundDataAsset;
				UE_LOG(LogTemp, Log, TEXT("DRSoundManager: Got SoundData from GameInstance: %s"), SoundData ? *SoundData->GetName() : TEXT("NULL"));
			}
		}
	}

	if (!SoundData)
	{
		UE_LOG(LogTemp, Error, TEXT("DRSoundManager: SoundDataAsset is null! BGM will not play."));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("DRSoundManager: SoundDataAsset loaded successfully: %s"), *SoundData->GetName());
		// BGM 참조 확인
		UE_LOG(LogTemp, Log, TEXT("DRSoundManager: BGM_Stage: %s"), SoundData->BGM_Stage ? *SoundData->BGM_Stage->GetName() : TEXT("NULL"));
		UE_LOG(LogTemp, Log, TEXT("DRSoundManager: BGM_Lobby: %s"), SoundData->BGM_Lobby ? *SoundData->BGM_Lobby->GetName() : TEXT("NULL"));
		UE_LOG(LogTemp, Log, TEXT("DRSoundManager: BGM_MainMenu: %s"), SoundData->BGM_MainMenu ? *SoundData->BGM_MainMenu->GetName() : TEXT("NULL"));
	}
}

void UDRSoundManager::Deinitialize()
{
	if (CurrentBGMComponent)
	{
		CurrentBGMComponent->Stop();
		CurrentBGMComponent = nullptr;
	}

	if (PreviousBGMComponent)
	{
		PreviousBGMComponent->Stop();
		PreviousBGMComponent = nullptr;
	}

	Super::Deinitialize();
}

void UDRSoundManager::PlaySound2D(USoundBase* Sound)
{
	if (!Sound) return;

	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		UGameplayStatics::PlaySound2D(World, Sound);
	}
}

void UDRSoundManager::PlayUIClickSound()
{
	if (SoundData)
	{
		PlaySound2D(SoundData->UIButtonClickSound);
	}
}

void UDRSoundManager::PlayPhaseStartSound()
{
	if (SoundData)
	{
		PlaySound2D(SoundData->PhaseStartSound);
	}
}

void UDRSoundManager::PlayWaveStartSound()
{
	if (SoundData)
	{
		PlaySound2D(SoundData->WaveStartSound);
	}
}

void UDRSoundManager::PlayGameClearSound()
{
	if (SoundData)
	{
		PlaySound2D(SoundData->GameClearSound);
	}
}

void UDRSoundManager::PlayGameOverSound()
{
	if (SoundData)
	{
		PlaySound2D(SoundData->GameOverSound);
	}
}

void UDRSoundManager::PlaySoundAtLocation(USoundBase* Sound, const FVector& Location)
{
	if (!Sound) return;

	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(World, Sound, Location);
	}
}

void UDRSoundManager::PlayPartPickupSound(const FVector& Location)
{
	if (SoundData)
	{
		PlaySoundAtLocation(SoundData->PartPickupSound, Location);
	}
}

void UDRSoundManager::PlayPartInstallSound(const FVector& Location, bool bIsComplete)
{
	if (bIsComplete)
	{
		if (SoundData)
		{
			PlaySoundAtLocation(SoundData->PartInstallCompleteSound, Location);
		}
	}
	else
	{
		if (SoundData)
		{
			PlaySoundAtLocation(SoundData->PartInstallSound, Location);
		}
	}
}

void UDRSoundManager::PlaySeedExplosionSound(const FVector& Location)
{
	if (SoundData)
	{
		PlaySoundAtLocation(SoundData->SeedExplosionSound, Location);
	}
}

void UDRSoundManager::PlayWaterGainSound(const FVector& Location)
{
	if (SoundData)
	{
		PlaySoundAtLocation(SoundData->WaterGainSound, Location);
	}
}

UAudioComponent* UDRSoundManager::StartCleanserOperatingSound(const FVector& Location)
{
	if(!SoundData) return nullptr;

	if (!SoundData->CleanserOperatingSound) return nullptr;

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return nullptr;

	return UGameplayStatics::SpawnSoundAtLocation(
		World,
		SoundData->CleanserOperatingSound,
		Location,
		FRotator::ZeroRotator,
		1.0f, 1.0f, 0.0f,
		nullptr, nullptr,
		false  
	);
}

void UDRSoundManager::PlayBGM(USoundBase* NewBGM, float FadeInDuration)
{
	UE_LOG(LogTemp, Log, TEXT("DRSoundManager::PlayBGM called with: %s"), NewBGM ? *NewBGM->GetName() : TEXT("NULL"));

	if (!NewBGM)
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager::PlayBGM - NewBGM is NULL!"));
		return;
	}

	if (CurrentBGMComponent && CurrentBGMComponent->IsPlaying())
	{
		UE_LOG(LogTemp, Log, TEXT("DRSoundManager::PlayBGM - Fading out current BGM"));
		CurrentBGMComponent->FadeOut(FadeInDuration, 0.0f);
	}

	CurrentBGMComponent = CreateBGMComponent(NewBGM);
	if (CurrentBGMComponent)
	{
		UE_LOG(LogTemp, Log, TEXT("DRSoundManager::PlayBGM - Starting FadeIn"));
		CurrentBGMComponent->FadeIn(FadeInDuration);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager::PlayBGM - Failed to create BGM component!"));
	}
}

void UDRSoundManager::PlayStageBGM()
{
	UE_LOG(LogTemp, Log, TEXT("DRSoundManager::PlayStageBGM called"));

	if (!SoundData)
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager::PlayStageBGM - SoundData is NULL!"));
		return;
	}

	if (!SoundData->BGM_Stage)
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager::PlayStageBGM - BGM_Stage is NULL!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DRSoundManager::PlayStageBGM - Playing: %s"), *SoundData->BGM_Stage->GetName());
	PlayBGM(SoundData->BGM_Stage, 2.0f);
}

void UDRSoundManager::PlayLobbyBGM()
{
	UE_LOG(LogTemp, Log, TEXT("DRSoundManager::PlayLobbyBGM called"));

	if (!SoundData)
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager::PlayLobbyBGM - SoundData is NULL!"));
		return;
	}

	if (!SoundData->BGM_Lobby)
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager::PlayLobbyBGM - BGM_Lobby is NULL!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DRSoundManager::PlayLobbyBGM - Playing: %s"), *SoundData->BGM_Lobby->GetName());
	PlayBGM(SoundData->BGM_Lobby, 2.0f);
}

void UDRSoundManager::PlayMainMenuBGM()
{
	UE_LOG(LogTemp, Log, TEXT("DRSoundManager::PlayMainMenuBGM called"));

	if (!SoundData)
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager::PlayMainMenuBGM - SoundData is NULL!"));
		return;
	}

	if (!SoundData->BGM_MainMenu)
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager::PlayMainMenuBGM - BGM_MainMenu is NULL!"));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("DRSoundManager::PlayMainMenuBGM - Playing: %s"), *SoundData->BGM_MainMenu->GetName());
	PlayBGM(SoundData->BGM_MainMenu, 2.0f);
}

void UDRSoundManager::StopBGM(float FadeOutDuration)
{
	if (CurrentBGMComponent && CurrentBGMComponent->IsPlaying())
	{
		CurrentBGMComponent->FadeOut(FadeOutDuration, 0.0f);
	}
}

void UDRSoundManager::CrossfadeBGM(USoundBase* NewBGM, float CrossfadeDuration)
{
	if (!NewBGM) return;

	if (PreviousBGMComponent)
	{
		PreviousBGMComponent->Stop();
	}
	PreviousBGMComponent = CurrentBGMComponent;

	if (PreviousBGMComponent && PreviousBGMComponent->IsPlaying())
	{
		PreviousBGMComponent->FadeOut(CrossfadeDuration, 0.0f);
	}

	CurrentBGMComponent = CreateBGMComponent(NewBGM);
	if (CurrentBGMComponent)
	{
		CurrentBGMComponent->FadeIn(CrossfadeDuration);
	}
}

UAudioComponent* UDRSoundManager::CreateBGMComponent(USoundBase* Sound)
{
	UE_LOG(LogTemp, Log, TEXT("DRSoundManager::CreateBGMComponent called"));

	if (!Sound)
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager::CreateBGMComponent - Sound is NULL!"));
		return nullptr;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager::CreateBGMComponent - World is NULL!"));
		return nullptr;
	}

	UE_LOG(LogTemp, Log, TEXT("DRSoundManager::CreateBGMComponent - Spawning Sound2D for: %s"), *Sound->GetName());
	UAudioComponent* AudioComp = UGameplayStatics::SpawnSound2D(World, Sound, 1.0f, 1.0f, 0.0f, nullptr, false, false);

	if (AudioComp)
	{
		UE_LOG(LogTemp, Log, TEXT("DRSoundManager::CreateBGMComponent - AudioComponent created successfully"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager::CreateBGMComponent - SpawnSound2D returned NULL!"));
	}

	return AudioComp;
}
