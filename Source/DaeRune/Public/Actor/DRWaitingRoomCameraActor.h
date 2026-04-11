// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRWaitingRoomCameraActor.generated.h"

class UCameraComponent;

/**
 * 대기실의 고정 카메라. LobbyMap에 하나 배치한다.
 * 캐릭터들이 서 있는 위치를 한눈에 볼 수 있는 시점을 제공.
 */
UCLASS()
class DAERUNE_API ADRWaitingRoomCameraActor : public AActor
{
	GENERATED_BODY()

public:
	ADRWaitingRoomCameraActor();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> CameraComponent;

	// 카메라가 바라보는 방향으로 캐릭터가 회전해야 할 Rotation 반환
	UFUNCTION(BlueprintCallable, Category = "Waiting Room")
	FRotator GetCharacterFacingRotation() const;
};
