// Copyright DaeRune

#include "Actor/DRBGMActor.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Net/UnrealNetwork.h"
#include "Engine/AssetManager.h"
#include "DRAssetManager.h"
#include "Sound/DRSoundDataAsset.h"

ADRBGMActor::ADRBGMActor()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	bAlwaysRelevant = true;

	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 메인 BGM (단일 트랙 또는 플레이리스트)
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);
	AudioComponent->bAutoActivate = false;
	AudioComponent->bIsUISound = true;
	AudioComponent->bAllowSpatialization = false;
	AudioComponent->SetVolumeMultiplier(1.0f);

	// 보스 BGM 전용 (크로스페이드용 분리)
	BossAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("BossAudioComponent"));
	BossAudioComponent->SetupAttachment(RootComponent);
	BossAudioComponent->bAutoActivate = false;
	BossAudioComponent->bIsUISound = true;
	BossAudioComponent->bAllowSpatialization = false;
	BossAudioComponent->SetVolumeMultiplier(1.0f);

	// BGM Sound Class (설정창 볼륨 슬라이더 연동)
	static ConstructorHelpers::FObjectFinder<USoundClass> BGMSoundClassFinder(
		TEXT("/Game/Blueprints/Audio/SoundClasses/SC_BGM.SC_BGM"));
	if (BGMSoundClassFinder.Succeeded())
	{
		AudioComponent->SoundClassOverride = BGMSoundClassFinder.Object;
		BossAudioComponent->SoundClassOverride = BGMSoundClassFinder.Object;
	}
}

void ADRBGMActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRBGMActor, CurrentTrackIndex);
	DOREPLIFETIME(ADRBGMActor, bBossBGMActive);
}

void ADRBGMActor::BeginPlay()
{
	Super::BeginPlay();

	LoadSoundsFromDataAsset();

	// 플레이리스트 종료 시 다음 트랙 진행 (서버에서만 바인딩)
	if (HasAuthority() && AudioComponent)
	{
		AudioComponent->OnAudioFinished.AddDynamic(this, &ADRBGMActor::OnAudioComponentFinished);
	}

	if (!bAutoPlay)
	{
		return;
	}

	if (HasAuthority())
	{
		PlayBGM();
	}
	else
	{
		// 클라: 짧은 딜레이 후 RepNotify가 아직 안 왔으면 로컬 재생
		FTimerHandle TimerHandle;
		GetWorld()->GetTimerManager().SetTimer(
			TimerHandle,
			[WeakThis = TWeakObjectPtr<ADRBGMActor>(this)]()
			{
				if (ADRBGMActor* StrongThis = WeakThis.Get())
				{
					if (StrongThis->AudioComponent && !StrongThis->AudioComponent->IsPlaying() && !StrongThis->bBossBGMActive)
					{
						if (StrongThis->BGMSlot == EDRBGMSlot::Stage)
						{
							const int32 Index = FMath::Max(0, StrongThis->CurrentTrackIndex);
							StrongThis->PlayPlaylistTrackLocal(Index, StrongThis->FadeInDuration);
						}
						else
						{
							StrongThis->PlaySingleTrackLocal();
						}
					}
				}
			},
			0.5f,
			false
		);
	}
}

void ADRBGMActor::LoadSoundsFromDataAsset()
{
	UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized());
	if (!AssetManager) return;

	UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset();
	if (!SoundData) return;

	switch (BGMSlot)
	{
	case EDRBGMSlot::MainMenu: SingleTrackSound = SoundData->BGM_MainMenu; break;
	case EDRBGMSlot::Lobby:    SingleTrackSound = SoundData->BGM_Lobby;    break;
	case EDRBGMSlot::Tutorial: SingleTrackSound = SoundData->BGM_Tutorial; break;
	case EDRBGMSlot::Stage:
		StagePlaylist = SoundData->BGM_StagePlaylist;
		BossSound = SoundData->BGM_Boss;
		break;
	default: break;
	}
}

void ADRBGMActor::PlayBGM()
{
	if (!HasAuthority()) return;

	bStoppedExplicitly = false;

	if (BGMSlot == EDRBGMSlot::Stage)
	{
		if (StagePlaylist.Num() == 0) return;
		CurrentTrackIndex = 0;
		PlayPlaylistTrackLocal(0, FadeInDuration);
	}
	else
	{
		if (!SingleTrackSound) return;
		PlaySingleTrackLocal();
	}
}

void ADRBGMActor::PlaySingleTrackLocal()
{
	if (!AudioComponent || !SingleTrackSound) return;
	if (AudioComponent->IsPlaying()) return;

	AudioComponent->SetSound(SingleTrackSound);
	AudioComponent->SetVolumeMultiplier(Volume);
	AudioComponent->FadeIn(FadeInDuration);
}

void ADRBGMActor::PlayPlaylistTrackLocal(int32 Index, float FadeIn)
{
	if (!AudioComponent) return;
	if (!StagePlaylist.IsValidIndex(Index)) return;

	USoundBase* Track = StagePlaylist[Index];
	if (!Track) return;

	// 현재 재생 중이면 페이드아웃 후 새 트랙 페이드인 (간단히 즉시 교체 + 페이드인)
	if (AudioComponent->IsPlaying())
	{
		AudioComponent->FadeOut(TrackCrossfadeDuration, 0.0f);
	}

	AudioComponent->SetSound(Track);
	AudioComponent->SetVolumeMultiplier(Volume);
	AudioComponent->FadeIn(FadeIn);
}

void ADRBGMActor::StopBGM(float FadeOutDuration)
{
	// FadeOut 완료 시 OnAudioFinished가 발화되어 다음 트랙으로 넘어가는 것을 막기 위한 플래그
	bStoppedExplicitly = true;

	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->FadeOut(FadeOutDuration, 0.0f);
	}
	if (BossAudioComponent && BossAudioComponent->IsPlaying())
	{
		BossAudioComponent->FadeOut(FadeOutDuration, 0.0f);
	}

	if (HasAuthority())
	{
		bBossBGMActive = false;
	}
}

void ADRBGMActor::StartBossBGM()
{
	if (!HasAuthority()) return;
	if (BGMSlot != EDRBGMSlot::Stage) return;
	if (!BossSound) return;
	if (bBossBGMActive) return;

	bBossBGMActive = true;
	PlayBossBGMLocal();
}

void ADRBGMActor::EndBossBGM()
{
	if (!HasAuthority()) return;
	if (!bBossBGMActive) return;

	bBossBGMActive = false;
	StopBossBGMLocal();
}

void ADRBGMActor::PlayBossBGMLocal()
{
	if (!BossAudioComponent || !BossSound) return;

	// 메인 페이드아웃
	if (AudioComponent && AudioComponent->IsPlaying())
	{
		AudioComponent->FadeOut(BossCrossfadeDuration, 0.0f);
	}

	// 보스 페이드인
	BossAudioComponent->SetSound(BossSound);
	BossAudioComponent->SetVolumeMultiplier(Volume);
	BossAudioComponent->FadeIn(BossCrossfadeDuration);
}

void ADRBGMActor::StopBossBGMLocal()
{
	if (!BossAudioComponent) return;

	// 보스 페이드아웃
	if (BossAudioComponent->IsPlaying())
	{
		BossAudioComponent->FadeOut(BossCrossfadeDuration, 0.0f);
	}

	// 플레이리스트 현재 인덱스 트랙을 처음부터 재생
	if (BGMSlot == EDRBGMSlot::Stage && StagePlaylist.Num() > 0)
	{
		const int32 Index = FMath::Max(0, CurrentTrackIndex);
		PlayPlaylistTrackLocal(Index, BossCrossfadeDuration);
	}
}

void ADRBGMActor::OnAudioComponentFinished()
{
	// 서버에서만 호출. 클라는 RepNotify로 동기화.
	if (!HasAuthority()) return;
	if (BGMSlot != EDRBGMSlot::Stage) return;
	if (bBossBGMActive) return;  // 보스 중에는 메인 페이드아웃 종료가 정상이므로 진행 안함
	if (bStoppedExplicitly) return;  // StopBGM으로 정지된 경우 다음 트랙으로 넘어가지 않음
	if (StagePlaylist.Num() == 0) return;

	const int32 NextIndex = (CurrentTrackIndex + 1) % StagePlaylist.Num();
	CurrentTrackIndex = NextIndex;
	PlayPlaylistTrackLocal(NextIndex, TrackCrossfadeDuration);
}

void ADRBGMActor::OnRep_CurrentTrackIndex()
{
	if (BGMSlot != EDRBGMSlot::Stage) return;
	if (bBossBGMActive) return;
	if (!StagePlaylist.IsValidIndex(CurrentTrackIndex)) return;

	// 같은 트랙이 이미 재생 중이면 스킵
	if (AudioComponent && AudioComponent->IsPlaying() && AudioComponent->Sound == StagePlaylist[CurrentTrackIndex])
	{
		return;
	}

	const float FadeIn = (CurrentTrackIndex == 0 && !AudioComponent->IsPlaying()) ? FadeInDuration : TrackCrossfadeDuration;
	PlayPlaylistTrackLocal(CurrentTrackIndex, FadeIn);
}

void ADRBGMActor::OnRep_BossBGMActive()
{
	if (bBossBGMActive)
	{
		PlayBossBGMLocal();
	}
	else
	{
		StopBossBGMLocal();
	}
}
