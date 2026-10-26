// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "Components/PanelWidget.h"	// GetParent() 반환형(UPanelWidget)을 UWidget 으로 업캐스트한다

namespace DRWidgetTree
{
	/**
	 * From 이 들어 있는 ScreenType 화면을 찾는다 (업그레이드 · 옷장 화면 공용 탐색 규칙).
	 * 1) 디자이너 배치 위젯 — 화면의 WidgetTree 안에 있으므로 Outer 체인에서 바로 잡힌다
	 * 2) 런타임 생성 위젯 — CreateWidget 의 Owner 가 PlayerController 라 Outer 로는 못 찾는다.
	 *    부모 패널은 화면의 WidgetTree 소속이므로 부모를 따라 올라가며 찾는다.
	 */
	template <typename ScreenType>
	ScreenType* FindOwnerScreen(UWidget* From)
	{
		if (!From) return nullptr;

		if (ScreenType* Screen = From->GetTypedOuter<ScreenType>())
		{
			return Screen;
		}

		for (UWidget* Cursor = From->GetParent(); Cursor; Cursor = Cursor->GetParent())
		{
			if (ScreenType* Screen = Cursor->GetTypedOuter<ScreenType>())
			{
				return Screen;
			}
		}
		return nullptr;
	}
}
