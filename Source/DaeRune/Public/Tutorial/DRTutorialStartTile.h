// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTutorialStartTile.generated.h"

class UBoxComponent;
class ADRTutorialManager;

/**
 * 구간 3 시작 타일
 * 플레이어가 밟으면 부품 적의 AI를 활성화한다.
 */
UCLASS(Blueprintable)
class DAERUNE_API ADRTutorialStartTile : public AActor
{
	GENERATED_BODY()

public:
	ADRTutorialStartTile();

	// 튜토리얼 매니저 참조 (레벨에서 할당)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "StartTile")
	TObjectPtr<ADRTutorialManager> TutorialManager;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> TileMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bActivated = false;
};
