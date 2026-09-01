// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRWardrobe.generated.h"

class UBoxComponent;
class UWidgetComponent;
class ADRPlayerController;

/**
 * 로비의 옷장. 상호작용하면 옷장 화면(로컬 UI)으로 전환한다.
 *
 * ADRUpgradeStation 과 ★구조가 동일하다★ — 옷장 화면도 순수 로컬 UI(로컬 세이브만 다룸)이므로
 * 서버 왕복이 없고, 따라서 오버랩/상호작용 판정을 각 머신에서 로컬로 처리한다.
 * (ADRStageSelectActor 같은 호스트 전용 포털과 다른 점이 이것이다)
 *
 * 배치 위치는 로비의 ★FreeRoam 구역★ 이다. 대기실(WaitingRoom)에는 플레이어 폰 자체가 없어
 * 걸어가서 쓰는 장치를 놓을 수 없다. (Plan.md 1.4 / 5.5)
 */
UCLASS()
class DAERUNE_API ADRWardrobe : public AActor
{
	GENERATED_BODY()

public:
	ADRWardrobe();

	// 이 로봇에 (잠긴 것 포함) 정의된 옷이 하나라도 있는지.
	// 프롬프트 위젯이 "준비 중" 표시를 하는 데 쓴다.
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	bool HasAnySkinAvailable() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WardrobeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> InteractionWidget;

	// 로컬 플레이어가 범위에 들어왔을 때 (BP: 프롬프트 문구 갱신 / 문 열림 연출)
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnLocalPlayerEnteredRange(bool bHasAnySkin);

	// 로컬 플레이어가 범위를 벗어났을 때
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnLocalPlayerLeftRange();

	// 고를 옷이 하나도 없을 때 상호작용을 시도했을 때 (BP: 안내 문구)
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
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
