// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DRLocalInteractStation.h"
#include "DRWardrobe.generated.h"

/**
 * 로비의 옷장. 상호작용하면 옷장 화면(로컬 UI)으로 전환한다.
 * 오버랩 · 상호작용 처리는 ADRLocalInteractStation 이 한다 (업그레이드 장치와 같다).
 *
 * 배치 위치는 로비의 ★FreeRoam 구역★ 이다. 대기실(WaitingRoom)에는 플레이어 폰 자체가 없어
 * 걸어가서 쓰는 장치를 놓을 수 없다. (Plan(옷장) D-9)
 */
UCLASS()
class DAERUNE_API ADRWardrobe : public ADRLocalInteractStation
{
	GENERATED_BODY()

public:
	ADRWardrobe();

	// 이 로봇에 (잠긴 것 포함) 정의된 옷이 하나라도 있는지.
	// 프롬프트 위젯이 "준비 중" 표시를 하는 데 쓴다.
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	bool HasAnySkinAvailable() const;

protected:
	// 카탈로그에 이 로봇의 옷이 하나도 없으면 빈 화면만 뜬다 — 열지 않고 안내한다.
	// ("기본" 칸만 있는 화면은 아무 것도 할 수 없어 버그처럼 보인다)
	virtual bool CanUseStation() const override { return HasAnySkinAvailable(); }
	virtual void OpenStationScreen(ADRPlayerController* PC) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WardrobeMesh;
};
