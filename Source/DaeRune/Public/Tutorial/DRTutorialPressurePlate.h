// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTutorialPressurePlate.generated.h"

class UBoxComponent;
class ADRTutorialManager;

/**
 * 튜토리얼 발판(Pressure Plate) 액터
 * 플레이어가 위에 올라서면 해당 구간의 클리어 여부를 체크하고,
 * 클리어된 상태라면 TutorialManager에 보고한다.
 */
UCLASS(Blueprintable)
class DAERUNE_API ADRTutorialPressurePlate : public AActor
{
	GENERATED_BODY()

public:
	ADRTutorialPressurePlate();

	// 이 발판이 담당하는 구간 인덱스 (1, 2, 3)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "PressurePlate")
	int32 SectionIndex = 1;

	// 튜토리얼 매니저 참조 (레벨에서 할당)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "PressurePlate")
	TObjectPtr<ADRTutorialManager> TutorialManager;

	// 외부에서 활성화 완료 표시
	void MarkActivated() { bActivated = true; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PlateMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> TriggerBox;

	UFUNCTION()
	void OnTriggerBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

private:
	bool bActivated = false;
};
