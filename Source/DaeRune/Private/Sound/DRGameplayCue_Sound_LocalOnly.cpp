// Copyright DaeRune


#include "Sound/DRGameplayCue_Sound_LocalOnly.h"
#include "Kismet/GameplayStatics.h"

bool UDRGameplayCue_Sound_LocalOnly::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!Sound || !MyTarget) return false;

	// 로컬 플레이어 체크
	APawn* Pawn = Cast<APawn>(MyTarget);
	if (!Pawn) return false;

	// 로컬 컨트롤러가 아니면 재생 안 함
	if (!Pawn->IsLocallyControlled()) return false;

	if (bIs3DSound)
	{
		FVector Location = MyTarget->GetActorLocation();
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
		UGameplayStatics::PlaySound2D(MyTarget->GetWorld(), Sound);
	}

	return false;
}
