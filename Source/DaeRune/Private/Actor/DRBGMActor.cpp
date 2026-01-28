// Copyright DaeRune

#include "Actor/DRBGMActor.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundClass.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"

ADRBGMActor::ADRBGMActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// 서버에서 생성되고 클라이언트에 복제됨
	bReplicates = true;
	bAlwaysRelevant = true;

	// 루트 컴포넌트 생성
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	// 오디오 컴포넌트 생성
	AudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("AudioComponent"));
	AudioComponent->SetupAttachment(RootComponent);
	AudioComponent->bAutoActivate = false;
	AudioComponent->bIsUISound = true;  // 2D 사운드로 설정
	AudioComponent->bAllowSpatialization = false;  // 공간 음향 비활성화
	AudioComponent->SetVolumeMultiplier(1.0f);

	// BGM Sound Class 로드 및 설정 (설정창 볼륨 조절 적용)
	static ConstructorHelpers::FObjectFinder<USoundClass> BGMSoundClassFinder(
		TEXT("/Game/Blueprints/Audio/SoundClasses/SC_BGM.SC_BGM"));
	if (BGMSoundClassFinder.Succeeded())
	{
		AudioComponent->SoundClassOverride = BGMSoundClassFinder.Object;
	}
}

void ADRBGMActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRBGMActor, BGMStartServerTime);
}

void ADRBGMActor::BeginPlay()
{
	Super::BeginPlay();

	if (bAutoPlay && BGMSound && AudioComponent)
	{
		if (HasAuthority())
		{
			// 서버: BGM 시작 시간 설정 및 재생
			PlayBGM();
		}
		else
		{
			// 클라이언트: 짧은 딜레이 후 바로 재생 (복잡한 시간 동기화 제거)
			FTimerHandle TimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				TimerHandle,
				[WeakThis = TWeakObjectPtr<ADRBGMActor>(this)]()
				{
					if (ADRBGMActor* StrongThis = WeakThis.Get())
					{
						if (StrongThis->AudioComponent && !StrongThis->AudioComponent->IsPlaying())
						{
							// 단순하게 페이드인 재생
							StrongThis->AudioComponent->SetSound(StrongThis->BGMSound);
							StrongThis->AudioComponent->SetVolumeMultiplier(StrongThis->Volume);
							StrongThis->AudioComponent->FadeIn(StrongThis->FadeInDuration);
						}
					}
				},
				0.5f,
				false
			);
		}
	}
}

void ADRBGMActor::PlayBGM()
{
	if (!BGMSound)
	{
		return;
	}

	if (HasAuthority())
	{
		// 서버: 시작 시간 설정 (복제됨)
		if (AGameStateBase* GS = GetWorld()->GetGameState())
		{
			BGMStartServerTime = GS->GetServerWorldTimeSeconds();
		}
		else
		{
			BGMStartServerTime = GetWorld()->GetTimeSeconds();
		}
	}

	// 로컬에서 재생
	PlayBGMLocal();
}

void ADRBGMActor::PlayBGMLocal()
{
	if (!AudioComponent || !BGMSound)
	{
		return;
	}

	// 이미 재생 중이면 스킵
	if (AudioComponent->IsPlaying())
	{
		return;
	}

	AudioComponent->SetSound(BGMSound);
	AudioComponent->SetVolumeMultiplier(Volume);
	AudioComponent->FadeIn(FadeInDuration);
}

void ADRBGMActor::StopBGM(float FadeOutDuration)
{
	if (!AudioComponent)
	{
		return;
	}

	AudioComponent->FadeOut(FadeOutDuration, 0.0f);

	if (HasAuthority())
	{
		BGMStartServerTime = 0.0f;
	}
}

void ADRBGMActor::OnRep_BGMStartTime()
{
	// 클라이언트: 서버에서 BGM 시작 신호가 오면 재생
	// BeginPlay 타이머보다 이게 먼저 올 수도 있으므로 백업 역할
	if (BGMStartServerTime > 0.0f && BGMSound && AudioComponent)
	{
		PlayBGMLocal();  // 내부에서 IsPlaying() 체크함
	}
}

float ADRBGMActor::GetBGMDuration() const
{
	if (!BGMSound)
	{
		return 0.0f;
	}

	return BGMSound->GetDuration();
}
