// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DRInteractable.generated.h"

UINTERFACE(MinimalAPI, meta = (CannotImplementInterfaceInBlueprint))
class UDRInteractable : public UInterface
{
	GENERATED_BODY()
};

/**
 * 시점 라인트레이스 기반 상호작용 대상 (CleanserPart/CleanserSite 공용)
 * PlayerController가 감지 대상 전환 시 상호작용 UI 위젯 표시를 토글하는 데 사용
 */
class DAERUNE_API IDRInteractable
{
	GENERATED_BODY()

public:
	// 상호작용 UI 위젯 표시/숨김 (로컬 코스메틱 - 판정 근거가 전부 복제 프로퍼티라 RPC 불필요)
	virtual void SetInteractionUIVisible(bool bShow) = 0;
};
