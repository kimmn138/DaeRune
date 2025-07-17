// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "DRSummonAbility.generated.h"

/**
 * 소환 어빌리티 클래스 선언
 */
UCLASS()
class DAERUNE_API UDRSummonAbility : public UDRGameplayAbility
{
	GENERATED_BODY()
	
public:
	// 스폰 위치 반환 함수 선언
	UFUNCTION(BlueprintCallable)
	TArray<FVector> GetSpawnLocations();

	// 랜덤 미니언 클래스 반환 함수 선언
	UFUNCTION(BlueprintPure, Category = "Summoning")
	TSubclassOf<APawn> GetRandomMinionClass();

	// 생성 미니언 개수 변수
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	int32 NumMinions = 5;

	// 미니언 클래스 배열 변수
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
    TArray<TSubclassOf<APawn>> MinionClasses;

	// 최소 스폰 거리 변수
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	float MinSpawnDistance = 50.f;

	// 최대 스폰 거리 변수
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	float MaxSpawnDistance = 250.f;
	
	// 스폰 스프레드 변수
	UPROPERTY(EditDefaultsOnly, Category = "Summoning")
	float SpawnSpread = 90.f;
};
