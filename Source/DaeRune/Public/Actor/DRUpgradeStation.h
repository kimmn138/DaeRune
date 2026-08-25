// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRUpgradeStation.generated.h"

class UBoxComponent;
class UWidgetComponent;
class ADRPlayerController;

/**
 * 로비의 업그레이드 전용 장치.
 * 상호작용하면 업그레이드 화면(로컬 UI)으로 전환한다.
 *
 * 업그레이드 화면은 순수 로컬 UI(로컬 세이브만 다룸)이므로 서버 왕복이 없다.
 * 따라서 오버랩/상호작용 판정을 각 머신에서 로컬로 처리한다 — ADRStageSelectActor(호스트 전용 포털)와
 * 다른 점이 이것이다.
 * (Plan2.md 8.8 참조)
 */
UCLASS()
class DAERUNE_API ADRUpgradeStation : public AActor
{
	GENERATED_BODY()

public:
	ADRUpgradeStation();

	// 업그레이드 시스템이 해금됐는지 (프롬프트 위젯이 잠김 표시를 하는 데 사용)
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	bool IsUpgradeSystemUnlocked() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StationMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> InteractionWidget;

	// 로컬 플레이어가 범위에 들어왔을 때 (BP: 프롬프트 문구 갱신)
	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade")
	void OnLocalPlayerEnteredRange(bool bUnlocked);

	// 로컬 플레이어가 범위를 벗어났을 때
	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade")
	void OnLocalPlayerLeftRange();

	// 해금 전에 상호작용을 시도했을 때 (BP: "스테이지1을 클리어하세요" 안내)
	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade")
	void OnInteractBlocked();

private:
	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 상호작용 키 입력 처리 (로컬)
	UFUNCTION()
	void OnLocalInteract();

	// 델리게이트 바인딩 해제 + 프롬프트 숨김
	void ClearLocalController();

	// 범위 안에 있는 로컬 플레이어의 컨트롤러
	UPROPERTY()
	TObjectPtr<ADRPlayerController> OverlappingLocalController;
};
