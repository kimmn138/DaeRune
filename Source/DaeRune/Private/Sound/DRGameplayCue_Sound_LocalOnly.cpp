// Copyright DaeRune


#include "Sound/DRGameplayCue_Sound_LocalOnly.h"
#include "Kismet/GameplayStatics.h"

bool UDRGameplayCue_Sound_LocalOnly::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!Sound) return false;
	if (!IsValid(MyTarget)) return false;

	// 로컬 플레이어만 재생
	APawn* Pawn = Cast<APawn>(MyTarget);
	if (!Pawn || !Pawn->IsLocallyControlled()) return false;

	// 위치 결정
	FVector Location = FVector::ZeroVector;

	if (!Parameters.Location.IsZero())
	{
		Location = Parameters.Location;
	}
	else if (!MyTarget->IsPendingKillPending())
	{
		Location = MyTarget->GetActorLocation();
	}
	else if (bIs3DSound)
	{
		// 3D 사운드인데 위치를 알 수 없으면 스킵
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
