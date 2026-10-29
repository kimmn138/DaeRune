// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DRLocalInteractStation.h"
#include "DRUpgradeStation.generated.h"

/**
 * 로비의 업그레이드 전용 장치. 상호작용하면 업그레이드 화면(로컬 UI)으로 전환한다.
 * 오버랩 · 상호작용 처리는 ADRLocalInteractStation 이 한다 (옷장과 같다). (Plan2.md 8.8 참조)
 */
UCLASS()
class DAERUNE_API ADRUpgradeStation : public ADRLocalInteractStation
{
	GENERATED_BODY()

public:
	ADRUpgradeStation();

	// 업그레이드 시스템이 해금됐는지 (프롬프트 위젯이 잠김 표시를 하는 데 사용)
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	bool IsUpgradeSystemUnlocked() const;

protected:
	// 업그레이드 시스템은 스테이지1 최초 클리어로 해금된다 (Plan2.md 5.1)
	virtual bool CanUseStation() const override { return IsUpgradeSystemUnlocked(); }
	virtual void OpenStationScreen(ADRPlayerController* PC) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StationMesh;
};
