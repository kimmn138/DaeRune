// Copyright DaeRune


#include "Sound/DRGameplayCue_Sound.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

UDRGameplayCue_Sound::UDRGameplayCue_Sound()
{
	// Static Cue�� �ν��Ͻ� ���� �� ��
	IsOverride = true;
}

bool UDRGameplayCue_Sound::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
    if (!Sound) return false;
    if (!IsValid(MyTarget)) return false;

    FVector Location = FVector::ZeroVector;
    if (!Parameters.Location.IsZero())
    {
        Location = Parameters.Location;
    }
    else if (!MyTarget->IsPendingKillPending())
    {
        Location = MyTarget->GetActorLocation();
    }
    else
    {
        return false;
    }

    // MyTarget을 WorldContextObject로 사용해서 올바른 World에서 사운드 재생
    if (bIs3DSound)
    {
        UGameplayStatics::PlaySoundAtLocation(MyTarget, Sound, Location);
    }
    else
    {
        UGameplayStatics::PlaySound2D(MyTarget, Sound);
    }

    return false;
}
