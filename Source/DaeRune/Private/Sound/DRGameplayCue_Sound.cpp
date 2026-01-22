// Copyright DaeRune


#include "Sound/DRGameplayCue_Sound.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/DRSoundManager.h"

UDRGameplayCue_Sound::UDRGameplayCue_Sound()
{
	// Static Cue는 인스턴스 생성 안 함
	IsOverride = true;
}

bool UDRGameplayCue_Sound::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
    if (!Sound) return false;

    UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
    if (!World) return false;

    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return false;

    UDRSoundManager* SoundManager = GI->GetSubsystem<UDRSoundManager>();
    if (!SoundManager) return false;

    FVector Location = FVector::ZeroVector;
    if (!Parameters.Location.IsZero())
    {
        Location = Parameters.Location;
    }
    else if (IsValid(MyTarget) && !MyTarget->IsPendingKillPending())
    {
        Location = MyTarget->GetActorLocation();
    }
    else
    {
        return false; // 위치를 알 수 없으면 스킵
    }

    if (bIs3DSound)
    {
        SoundManager->PlaySoundAtLocation(Sound, Location);
    }
    else
    {
        SoundManager->PlaySound2D(Sound);
    }

    return false;
}
