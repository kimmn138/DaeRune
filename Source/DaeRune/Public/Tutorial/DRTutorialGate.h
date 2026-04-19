// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTutorialGate.generated.h"

/**
 * 튜토리얼 문(Gate) 액터
 * 닫힌 상태에서 물리적으로 통행을 차단하고,
 * OpenGate() 호출 시 양쪽 문이 +X / -X 방향으로 슬라이드하며 열린다.
 */
UCLASS(Blueprintable)
class DAERUNE_API ADRTutorialGate : public AActor
{
	GENERATED_BODY()

public:
	ADRTutorialGate();

	// 문 열기
	UFUNCTION(BlueprintCallable, Category = "Tutorial|Gate")
	void OpenGate();

	// 문이 열려있는지 확인
	UFUNCTION(BlueprintPure, Category = "Tutorial|Gate")
	bool IsOpen() const { return bIsOpen; }

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// 문 루트 (씬 컴포넌트)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootScene;

	// 왼쪽 문 메시 (+X 방향으로 열림)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GateMeshLeft;

	// 오른쪽 문 메시 (-X 방향으로 열림)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> GateMeshRight;

	// 열림 거리 (각 문이 좌우로 이동할 거리)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gate|Config")
	float OpenDistance = 200.f;

	// 열림 속도 (유닛/초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Gate|Config")
	float OpenSpeed = 200.f;

private:
	bool bIsOpen = false;
	bool bIsOpening = false;
	FVector LeftClosedLocation;
	FVector LeftOpenLocation;
	FVector RightClosedLocation;
	FVector RightOpenLocation;
};
