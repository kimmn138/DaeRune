// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTutorialFallZone.generated.h"

class UBoxComponent;

/**
 * 튜토리얼 점프 구간 낙하 리셋 액터
 * 플레이어가 오버랩하면 점프 구간 시작 지점으로 텔레포트시킨다.
 */
UCLASS(Blueprintable)
class DAERUNE_API ADRTutorialFallZone : public AActor
{
	GENERATED_BODY()

public:
	ADRTutorialFallZone();

	// 플레이어가 리스폰될 위치/회전 (인스턴스별 지정)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "FallZone")
	FTransform RespawnTransform;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
};
