// Copyright DaeRune


#include "Actor/DRWaitingRoomCameraActor.h"
#include "Camera/CameraComponent.h"

ADRWaitingRoomCameraActor::ADRWaitingRoomCameraActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Client RPC 파라미터로 안전하게 전달되도록 리플리케이션 활성화
	bReplicates = true;
	bAlwaysRelevant = true;

	CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("WaitingRoomCamera"));
	RootComponent = CameraComponent;
}

FRotator ADRWaitingRoomCameraActor::GetCharacterFacingRotation() const
{
	// 카메라를 향하는 방향 = 카메라 전방 벡터의 반대
	FVector CameraForward = CameraComponent->GetForwardVector();
	FRotator FacingRotation = (-CameraForward).Rotation();
	FacingRotation.Pitch = 0.f; // 수평 유지
	FacingRotation.Roll = 0.f;
	return FacingRotation;
}
