// Copyright DaeRune


#include "Sound/DRGameplayCue_Sound.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UDRGameplayCue_Sound::UDRGameplayCue_Sound()
{
	// Static Cue는 인스턴스 생성 안 함
	IsOverride = true;
}

bool UDRGameplayCue_Sound::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!Sound || !IsValid(MyTarget) || !ShouldPlayFor(MyTarget)) return false;

	// MyTarget을 WorldContextObject로 사용해서 올바른 World에서 사운드 재생
	if (!bIs3DSound)
	{
		UGameplayStatics::PlaySound2D(MyTarget, Sound);
		return false;
	}

	// 3D: 큐 위치 → 대상 위치 순. 대상이 사라지는 중이고 큐 위치도 없으면 재생하지 않는다
	if (!Parameters.Location.IsZero())
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Sound, Parameters.Location);
	}
	else if (!MyTarget->IsPendingKillPending())
	{
		UGameplayStatics::PlaySoundAtLocation(MyTarget, Sound, MyTarget->GetActorLocation());
	}

	return false;
}
