// Copyright DaeRune


#include "Sound/DRGameplayCue_Sound_LocalOnly.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/DRSoundManager.h"

bool UDRGameplayCue_Sound_LocalOnly::OnExecute_Implementation(AActor* MyTarget, const FGameplayCueParameters& Parameters) const
{
	if (!Sound) return false;

	// World를 먼저 안전하게 가져오기
	UWorld* World = MyTarget ? MyTarget->GetWorld() : nullptr;
	if (!World) return false;

	// 로컬 플레이어 체크
	APawn* Pawn = Cast<APawn>(MyTarget);
	if (!Pawn || !Pawn->IsLocallyControlled()) return false;

	// GameInstance에서 SoundManager 가져오기
	UGameInstance* GI = World->GetGameInstance();
	if (!GI) return false;

	UDRSoundManager* SoundManager = GI->GetSubsystem<UDRSoundManager>();
	if (!SoundManager) return false;

	// 위치 정보 안전하게 가져오기
	FVector Location = FVector::ZeroVector;

	if (!Parameters.Location.IsZero())
	{
		Location = Parameters.Location;
	}
	else if (IsValid(MyTarget) && !MyTarget->IsPendingKillPending())
	{
		Location = MyTarget->GetActorLocation();
	}
	else if (bIs3DSound)
	{
		// 3D 사운드인데 위치를 알 수 없으면 스킵
		return false;
	}

	// SoundManager를 통해 안전하게 재생
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
