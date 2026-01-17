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
	if (!Sound || !MyTarget) return false;

	if (bIs3DSound)
	{
		// 3D 사운드: 타겟 위치에서 재생
		FVector Location = MyTarget->GetActorLocation();

		// Parameters에 위치 정보가 있으면 사용
		if (!Parameters.Location.IsZero())
		{
			Location = Parameters.Location;
		}

		UGameplayStatics::PlaySoundAtLocation(
			MyTarget->GetWorld(),
			Sound,
			Location
		);
	}
	else
	{
		// 2D 사운드: 전역 재생
		UGameplayStatics::PlaySound2D(MyTarget->GetWorld(), Sound);
	}

	return false;
}
