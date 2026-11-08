// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DRInteractable.h"
#include "DRLocalInteractStation.generated.h"

class UBoxComponent;
class ADRPlayerController;

/**
 * 로비에서 걸어가 상호작용하면 로컬 화면을 여는 장치의 공통 부분 (업그레이드 장치 · 옷장).
 *
 * 화면이 순수 로컬 UI(로컬 세이브만 다룸)라 서버 왕복이 없다 — 그래서 오버랩/상호작용 판정을
 * 각 머신에서 로컬로 처리한다. ADRStageSelectActor(호스트 전용 포털)와 다른 점이 이것이다.
 *
 * 파생은 생성자에서 CreateStationRootMesh() 로 장치 메시를 만들고(메시 이름은 장치마다 달라
 * BP · 레벨의 기존 설정이 그대로 붙도록 파생이 정한다), CanUseStation / OpenStationScreen 두 개와 안내 문구를 채운다.
 *
 * 범위에 들어온 동안 화면 상호작용 안내를 띄운다 (UDRInteractionComponent::SetStationInRange).
 * 쓸 수 없는 상태면 키 없이 UnavailablePromptText 만 띄운다.
 */
UCLASS(Abstract)
class DAERUNE_API ADRLocalInteractStation : public AActor, public IDRInteractable
{
	GENERATED_BODY()

public:
	ADRLocalInteractStation();

	// IDRInteractable — 쓸 수 있으면 PromptText, 아니면 키 없이 UnavailablePromptText
	virtual FText GetInteractionPromptText(const ADRCharacter* Interactor, bool& bOutUsable) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 장치 메시를 루트로 만들고 상호작용 박스를 그 아래에 붙인다 (파생 생성자에서 1회)
	UStaticMeshComponent* CreateStationRootMesh(FName MeshName);

	// 지금 쓸 수 있나 — 화면 안내(문구 · 키 표시)와 상호작용 허용에 같이 쓴다
	virtual bool CanUseStation() const PURE_VIRTUAL(ADRLocalInteractStation::CanUseStation, return false;);

	// 화면을 연다 (CanUseStation 이 true 일 때만 불린다)
	virtual void OpenStationScreen(ADRPlayerController* PC) PURE_VIRTUAL(ADRLocalInteractStation::OpenStationScreen, );

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	// 화면 상호작용 안내 — 쓸 수 있을 때 (F = 화면 열기)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station")
	FText PromptText;

	// 화면 상호작용 안내 — 지금 쓸 수 없을 때 (키 없이 이 문구만)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Station")
	FText UnavailablePromptText;

	// 로컬 플레이어가 범위에 들어왔을 때 (BP: 연출). bCanUse = CanUseStation()
	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnLocalPlayerEnteredRange(bool bCanUse);

	// 로컬 플레이어가 범위를 벗어났을 때
	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnLocalPlayerLeftRange();

	// 쓸 수 없는 상태에서 상호작용을 시도했을 때 (BP: 안내 문구)
	UFUNCTION(BlueprintImplementableEvent, Category = "Station")
	void OnInteractBlocked();

	// 범위 안에 있는 로컬 플레이어의 컨트롤러
	UPROPERTY()
	TObjectPtr<ADRPlayerController> OverlappingLocalController;

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

	// 델리게이트 바인딩 해제 + 화면 안내 해제
	void ClearLocalController();
};
