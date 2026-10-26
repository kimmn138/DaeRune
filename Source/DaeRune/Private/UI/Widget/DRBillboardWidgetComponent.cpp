// Copyright DaeRune


#include "UI/Widget/DRBillboardWidgetComponent.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

UDRBillboardWidgetComponent::UDRBillboardWidgetComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = true;
}

void UDRBillboardWidgetComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// 회전이 필요 없는 경우는 일찍 끝낸다 — 적마다 붙어 있어(최대 100) 매 프레임 비용이 쌓인다.
	// 화면 공간 위젯은 늘 카메라를 향하고, 최근에 그려지지 않은(화면 밖 · 가려진) 위젯은 돌릴 필요가 없다
	if (!IsVisible() || GetWidgetSpace() == EWidgetSpace::Screen || !WasRecentlyRendered(0.2f))
	{
		return;
	}

	APlayerController* LocalPC = GetWorld()->GetFirstPlayerController();
	if (!LocalPC || !LocalPC->IsLocalController())
	{
		return;
	}

	APlayerCameraManager* CameraManager = LocalPC->PlayerCameraManager;
	if (!CameraManager)
	{
		return;
	}

	FVector CameraLocation = CameraManager->GetCameraLocation();
	FVector WidgetLocation = GetComponentLocation();

	FVector Direction = CameraLocation - WidgetLocation;
	Direction.Z = 0.0f;

	if (!Direction.IsNearlyZero())
	{
		// 거의 같은 방향이면 트랜스폼 갱신(자식 · 렌더 상태 갱신)을 건너뛴다
		const FRotator LookAtRotation = Direction.Rotation();
		if (!LookAtRotation.Equals(GetComponentRotation(), 0.5f))
		{
			SetWorldRotation(LookAtRotation);
		}
	}
}
