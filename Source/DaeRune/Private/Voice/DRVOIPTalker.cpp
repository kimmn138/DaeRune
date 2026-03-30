// Copyright DaeRune


#include "Voice/DRVOIPTalker.h"
#include "Game/DRGameUserSettings.h"

void UDRVOIPTalker::OnTalkingBegin(UAudioComponent* AudioComponent)
{
    Super::OnTalkingBegin(AudioComponent);

    if (!AudioComponent) return;

	// 로컬 플레이어의 설정 가져오기
	if (UDRGameUserSettings* UserSettings = UDRGameUserSettings::GetDRGameUserSettings())
	{
		// 볼륨 적용 (0.0 ~ 2.0)
		AudioComponent->SetVolumeMultiplier(UserSettings->VoiceVolume);
	}
}
