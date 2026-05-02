// Copyright DaeRune


#include "Sound/DRSoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/DRSoundDataAsset.h"
#include "Game/DRGameInstance.h"
#include "DRAssetManager.h"
#include "Engine/Engine.h"

void UDRSoundManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// AssetManager?먯꽌 濡쒕뱶??SoundData 媛?몄삤湲?
	if (UAssetManager* AssetManager = UAssetManager::GetIfInitialized())
	{
		if (UDRAssetManager* DRAssetManager = Cast<UDRAssetManager>(AssetManager))
		{
			SoundData = DRAssetManager->GetSoundDataAsset();
		}
	}

	// AssetManager?먯꽌 紐?媛?몄솕?쇰㈃ 吏곸젒 濡쒕뱶 ?쒕룄
	if (!SoundData)
	{
		static const TCHAR* SoundDataPath = TEXT("/Game/Blueprints/Sound/Data/DA_SoundData.DA_SoundData");
		SoundData = LoadObject<UDRSoundDataAsset>(nullptr, SoundDataPath);
	}

	// 留덉?留됱쑝濡?GameInstance?먯꽌 ?쒕룄 (?먮뵒???대갚)
	if (!SoundData)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDRGameInstance* DRGI = Cast<UDRGameInstance>(GI))
			{
				SoundData = DRGI->SoundDataAsset;
			}
		}
	}

	if (!SoundData)
	{
}
}

void UDRSoundManager::PlaySound2D(USoundBase* Sound)
{
	if (!Sound) return;

	UWorld* World = nullptr;
	if (GEngine)
	{
		World = GEngine->GetCurrentPlayWorld();
	}

	if (!World && GetGameInstance())
	{
		World = GetGameInstance()->GetWorld();
	}

	if (World)
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

	UWorld* World = nullptr;
	if (GEngine)
	{
		World = GEngine->GetCurrentPlayWorld();
	}

	if (!World && GetGameInstance())
	{
		World = GetGameInstance()->GetWorld();
	}

	if (World)
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
	if (!SoundData) return nullptr;

	if (!SoundData->CleanserOperatingSound) return nullptr;

	UWorld* World = nullptr;
	if (GEngine)
	{
		World = GEngine->GetCurrentPlayWorld();
	}

	if (!World && GetGameInstance())
	{
		World = GetGameInstance()->GetWorld();
	}

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

