// Copyright DaeRune


#include "Voice/DRVOIPTalker.h"
#include "Game/DRGameUserSettings.h"

void UDRVOIPTalker::OnTalkingBegin(UAudioComponent* AudioComponent)
{
    Super::OnTalkingBegin(AudioComponent);

    if (!AudioComponent) return;

	// 음성채팅 제거 예정 — 기본 볼륨 사용
	AudioComponent->SetVolumeMultiplier(1.0f);
}
