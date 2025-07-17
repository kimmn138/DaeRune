// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DRBeamSpell.generated.h"

/**
 * 빔 스펠 어빌리티 클래스 선언
 */
UCLASS()
class DAERUNE_API UDRBeamSpell : public UDRDamageGameplayAbility
{
	GENERATED_BODY()
	
public:
	// 카메라 히트 정보 저장 함수 선언
	UFUNCTION(BlueprintCallable)
	void StoreCameraDataInfo(const FHitResult& HitResult);

	// 소유자 변수 저장 함수 선언
	UFUNCTION(BlueprintCallable)
	void StoreOwnerVariables();

	// 첫 번째 대상 트레이스 함수 선언
	UFUNCTION(BlueprintCallable)
	void TraceFirstTarget(const FVector& BeamTargetLocation);

	// 추가 대상 저장 함수 선언
	UFUNCTION(BlueprintCallable)
	void StoreAdditionalTargets(TArray<AActor*>& OutAdditionalTargets);

	// 주 대상 사망 이벤트 선언
	UFUNCTION(BlueprintImplementableEvent)
	void PrimaryTargetDied(AActor* DeadActor);

	// 추가 대상 사망 이벤트 선언
	UFUNCTION(BlueprintImplementableEvent)
	void AdditionalTargetDied(AActor* DeadActor);

protected:
	// 카메라 히트 위치 변수 선언
	UPROPERTY(BlueprintReadWrite, Category = "Beam")
	FVector CameraHitLocation;

	// 카메라 히트 액터 변수 선언
	UPROPERTY(BlueprintReadWrite, Category = "Beam")
	TObjectPtr<AActor> CameraHitActor;

	// 소유자 캐릭터 변수 선언
	UPROPERTY(BlueprintReadWrite, Category = "Beam")
	TObjectPtr<ACharacter> OwnerCharacter;

	// 최대 충격 대상 수 변수 선언
	UPROPERTY(EditDefaultsOnly, Category = "Beam")
	int32 MaxNumShockTargets = 5;
};
