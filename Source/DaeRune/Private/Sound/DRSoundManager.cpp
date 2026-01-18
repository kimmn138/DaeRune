// Copyright DaeRune


#include "Sound/DRSoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/DRSoundDataAsset.h"

void UDRSoundManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	SoundData = LoadObject<UDRSoundDataAsset>(nullptr, SOUND_DATA_PATH);

	if (!SoundData)
	{
		UE_LOG(LogTemp, Warning, TEXT("DRSoundManager: DA_SoundData not found at %s"), SOUND_DATA_PATH);
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
	if (!NewBGM) return;

	if (CurrentBGMComponent && CurrentBGMComponent->IsPlaying())
	{
		CurrentBGMComponent->FadeOut(FadeInDuration, 0.0f);
	}

	CurrentBGMComponent = CreateBGMComponent(NewBGM);
	if (CurrentBGMComponent)
	{
		CurrentBGMComponent->FadeIn(FadeInDuration);
	}
}

void UDRSoundManager::PlayStageBGM()
{
	if (SoundData && SoundData->BGM_Stage)
	{
		PlayBGM(SoundData->BGM_Stage, 2.0f);
	}
}

void UDRSoundManager::PlayLobbyBGM()
{
	if (SoundData && SoundData->BGM_Lobby)
	{
		PlayBGM(SoundData->BGM_Lobby, 2.0f);
	}
}

void UDRSoundManager::PlayMainMenuBGM()
{
	if (SoundData && SoundData->BGM_MainMenu)
	{
		PlayBGM(SoundData->BGM_MainMenu, 2.0f);
	}
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
	if (!Sound) return nullptr;

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World) return nullptr;

	return UGameplayStatics::SpawnSound2D(World, Sound, 1.0f, 1.0f, 0.0f, nullptr, false, false);
}
