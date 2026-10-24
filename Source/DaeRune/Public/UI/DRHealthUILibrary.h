// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRHealthUILibrary.generated.h"

class UUserWidget;

/**
 * 체력 HUD(WBP_HealthWaterBar) 계산.
 *
 * 체력 칸은 화면에 ★항상 4칸★ 이고, 칸 크기 = 최대 체력 / 4 다.
 * 서버 쪽(UDRPlayerAttributeSet)도 같은 규칙(최대 체력 / 칸 수)이라 회복이 멈추는 칸 경계가 화면과 일치한다.
 */
UCLASS()
class DAERUNE_API UDRHealthUILibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// UI 의 체력 칸 수(고정)와 위젯 그래프가 가정하는 칸 하나의 크기.
	static constexpr int32 DisplayContainerCount = 4;
	static constexpr double DisplayContainerSize = 100.0;

	/**
	 * 실제 체력 → "4칸 × 100(= 400)" 기준 표시값.
	 * WBP_HealthWaterBar 는 칸 표시(300/200/100/0 분기) · 칸 감소 연출 · 링 재질(CurrentHP × 1/400)이 전부
	 * 최대 체력 400 을 가정하고 짜여 있다. 그 그래프에 이 값을 넣으면 칸 크기 = 최대 체력 / 4 로 동작한다.
	 * 예) 최대 800 에 체력 600 → 300 (3칸) · 최대 200 에 체력 150 → 300 (3칸).
	 * 오염 중에는 그대로 돌려준다(최대 체력 100 = 예전처럼 한 칸).
	 * 숫자 표시(Text_Hp)에는 쓰지 않는다 — 숫자는 실제 체력이다.
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Health", meta = (DefaultToSelf = "Widget"))
	static double ToContainerDisplayHealth(const UUserWidget* Widget, double Health, double MaxHealth);

	/**
	 * 위젯 주인(로컬 플레이어)이 오염 상태인지.
	 * 서버는 AttributeSet 의 플래그(최대 체력을 바꾸기 전에 켜지고 꺼진다), 클라이언트는 복제된 PlayerState 플래그를 본다.
	 * (예전 판정 "최대 체력 ≤ 100" 은 기본 체력이 100 인 청소기를 늘 오염으로 칠했다)
	 */
	UFUNCTION(BlueprintPure, Category = "UI|Health", meta = (DefaultToSelf = "Widget"))
	static bool IsOwningPlayerCorrupted(const UUserWidget* Widget);
};
