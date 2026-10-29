// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DRHealthBarEditorTools.generated.h"

/**
 * 체력 HUD(WBP_HealthWaterBar — Lobby/Stage/Tutorial 오버레이가 쓰는 실제 위젯) 그래프 수정 — ★에디터 전용★.
 * 게임 빌드에서는 본문이 비어 있다.
 *
 * 예전 그래프는 최대 체력 400(= 4칸 × 100)을 가정했다:
 *   SetContainerSize / ShowContainerReduceEffectAnimation 의 300·200·100·0 분기, 링 재질 CurrentHP(× 1/400),
 *   IsCorrupted? = 최대 체력 ≤ 100.
 * 고치는 방법 — 그래프의 숫자는 그대로 두고 ★들어가는 체력을 400 기준 값으로 바꾼다★(UDRHealthUILibrary::ToContainerDisplayHealth):
 *   - 위 두 함수 안의 Health / PreviousHealth 읽기 → ToContainerDisplayHealth(…, MaxHealth)
 *   - 링 재질 SetScalarParameterValue(CurrentHP, DisplayedHealth) 의 값 → ToContainerDisplayHealth(DisplayedHealth, MaxHealth)
 *   - IsCorrupted? 의 "≤ 100" → UDRHealthUILibrary::IsOwningPlayerCorrupted
 * 숫자 표시(Text_Hp)는 실제 체력 그대로 둔다. 몇 번을 돌려도 같은 결과(멱등). 컴파일까지 하고, 저장은 부른 쪽(Python)이 한다.
 *
 *   log = unreal.DRHealthBarEditorTools.fix_health_water_bar_widget()
 */
UCLASS()
class DAERUNEEDITOR_API UDRHealthBarEditorTools : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	// 성공하면 로그 첫 줄이 "OK", 실패하면 "ERROR" 로 시작한다.
	UFUNCTION(BlueprintCallable, Category = "DaeRune|Editor")
	static FString FixHealthWaterBarWidget();
};
