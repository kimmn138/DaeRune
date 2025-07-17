// Copyright DaeRune


#include "AbilitySystem/Abilities/DRSummonAbility.h"

// 스폰 위치 계산 함수 구현부
TArray<FVector> UDRSummonAbility::GetSpawnLocations()
{
	// 아바타 정방향 벡터 계산
	const FVector Forward = GetAvatarActorFromActorInfo()->GetActorForwardVector(); 
	// 아바타 위치 계산
	const FVector Location = GetAvatarActorFromActorInfo()->GetActorLocation();
	// 스프레드 간격 계산
	const float DeltaSpread = SpawnSpread / NumMinions;

	// 최대 스프레드 방향 벡터 계산
	const FVector LeftOfSpread = Forward.RotateAngleAxis(-SpawnSpread / 2.f, FVector::UpVector);
	// 스폰 위치 배열 생성
	TArray<FVector> SpawnLocations;
	// 소환 위치 반복 생성
	for (int32 i = 0; i < NumMinions; i++)
	{
		// 회전된 방향 벡터 계산
		const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, FVector::UpVector);
		// 랜덤 거리 적용 위치 계산
		FVector ChosenSpawnLocation = Location + Direction * FMath::FRandRange(MinSpawnDistance, MaxSpawnDistance);

		// 충돌 검사 결과 저장
		FHitResult Hit;
		// 지면 충돌 검사 수행
		GetWorld()->LineTraceSingleByChannel(Hit, ChosenSpawnLocation + FVector(0.f, 0.f, 400.f), ChosenSpawnLocation - FVector(0.f, 0.f, 400.f), ECC_Visibility);
		// 충돌 시 임팩트 위치 적용
		if (Hit.bBlockingHit)
		{
			ChosenSpawnLocation = Hit.ImpactPoint;
		}
		// 계산된 위치 배열 추가
		SpawnLocations.Add(ChosenSpawnLocation);
	}

	// 스폰 위치 배열 반환
	return SpawnLocations;
}

// 랜덤 미니언 클래스 선택 함수 구현부
TSubclassOf<APawn> UDRSummonAbility::GetRandomMinionClass()
{
	// 랜덤 인덱스 생성
	const int32 Selection = FMath::RandRange(0, MinionClasses.Num() - 1); 
	// 선택된 미니언 클래스 반환
	return MinionClasses[Selection];
}
