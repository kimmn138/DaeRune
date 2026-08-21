// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2Barrier.generated.h"

class UBoxComponent;

/**
 * 전투 구간 차단벽 (Plan6 §4.9.6 / §14.6.5)
 *
 * 장애물이 보스 등장과 동시에 부서지므로 전방·후방 양쪽을 배리어로 막아 전투 구간을 한정한다.
 * (원안은 후방만 담당했으나 확정 사양에서 전방도 필요해졌다.)
 *
 * 콜리전은 Pawn 만 Block 한다. 열차(WorldDynamic)와 투사체는 통과하므로
 * 열차 주행과 사격을 방해하지 않는다.
 */
UCLASS()
class DAERUNE_API ADRS2Barrier : public AActor
{
	GENERATED_BODY()

public:
	ADRS2Barrier();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 서버 전용. 멱등.
	UFUNCTION(BlueprintCallable, Category = "S2|Barrier")
	void SetBarrierEnabled(bool bNewEnabled);

	UFUNCTION(BlueprintCallable, Category = "S2|Barrier")
	bool IsBarrierEnabled() const { return bEnabled; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Barrier")
	TObjectPtr<UBoxComponent> BlockBox;

	UPROPERTY(ReplicatedUsing = OnRep_bEnabled, BlueprintReadOnly, Category = "S2|Barrier")
	bool bEnabled = false;

	UFUNCTION()
	void OnRep_bEnabled();

	// 역장 시각화 등
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Barrier")
	void OnBarrierEnabledChanged(bool bNowEnabled);
};
