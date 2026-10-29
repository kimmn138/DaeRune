// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "DRAttributeChangedDelegate.generated.h"

// 어트리뷰트 값 변경 → UI 갱신용 델리게이트 (HUD 오버레이 · 적 체력바 · 클렌저 사이트 체력바 공용).
// 액터 헤더가 이 델리게이트 하나 때문에 OverlayWidgetController.h 전체를 끌어오지 않도록 따로 둔다.
UDELEGATE()
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeChangedSignature, float, NewValue);
