// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Components/WidgetComponent.h"
#include "DRBillboardWidgetComponent.generated.h"

/**
 * 로컬 플레이어 카메라를 향해 자동 회전하는 빌보드 위젯 컴포넌트.
 * 멀티플레이어에서 각 클라이언트가 독립적으로 자신의 카메라 방향으로 회전합니다.
 */
UCLASS(ClassGroup=(UI), meta=(BlueprintSpawnableComponent))
class DAERUNE_API UDRBillboardWidgetComponent : public UWidgetComponent
{
	GENERATED_BODY()

public:
	UDRBillboardWidgetComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
};
