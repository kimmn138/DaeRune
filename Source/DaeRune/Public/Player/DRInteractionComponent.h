// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRInteractionComponent.generated.h"

class ADRPlayerController;
class ADRCharacter;
class ADRCleanserPart;
class ADRCleanserSite;
class ADRRobotVacuumCharacter;
class ADRS2InteractProp;
class ADRS2TrainCar;

/**
 * PlayerController 에 붙는 상호작용 대상 감지 (로컬 컨트롤러 전용).
 *
 * 카메라 시점 광선을 주기마다 한 번 쏴서 지금 F 로 상호작용할 대상을 고르고, 대상이 바뀌면
 * IDRInteractable 프롬프트 UI 를 켜고 끈다. 부품 · 사이트 · 청소기는 그 액터의 오버랩 범위에 들어왔을 때만
 * 감지를 켜고(Set*DetectionEnabled), 스테이지2 프롭 · 열차 칸은 오버랩 없이 광선 거리로만 판정한다.
 *
 * 서버 요청(ServerRequestInteract) · F 키 처리는 PlayerController 에 남는다 — RPC 는 소유 연결이 있는 PC 가 보낸다.
 */
UCLASS(ClassGroup = (DaeRune))
class DAERUNE_API UDRInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRInteractionComponent();

	// 오버랩으로 켜지는 감지 — 부품 · 사이트 · 청소기의 감지 범위 오버랩 콜백이 부른다
	void SetPartDetectionEnabled(bool bEnabled, ADRCleanserPart* Part);
	void SetSiteDetectionEnabled(bool bEnabled, ADRCleanserSite* Site);
	void SetMountDetectionEnabled(bool bEnabled, ADRRobotVacuumCharacter* Mount);

	// PlayerTick(로컬 컨트롤러)이 매 프레임 부른다 — LineTraceUpdateInterval 마다 감지 대상을 갱신한다
	void TickDetection(float DeltaTime);

	// 지금 감지 중인 대상 (없으면 null)
	AActor* GetDetectedPart() const { return CurrentDetectedPart; }
	AActor* GetDetectedSite() const { return CurrentDetectedSite; }
	AActor* GetDetectedMount() const { return CurrentDetectedMount; }
	AActor* GetDetectedProp() const { return CurrentDetectedProp; }
	AActor* GetDetectedTrainCar() const { return CurrentDetectedCar; }

protected:
	// 시점 광선 길이
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	float LineTraceDistance = 250.f;

	// 감지 갱신 간격 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	float LineTraceUpdateInterval = 0.1f;

private:
	ADRPlayerController* GetOwnerController() const;

	// 오버랩 집합 등록/해제 + 감지 플래그 갱신 + 이탈 시 UI 정리 (부품 · 사이트 · 청소기 공용)
	void SetInteractableDetectionEnabled(bool bEnabled, AActor* Interactable,
		TSet<TWeakObjectPtr<AActor>>& OverlapSet, bool& bDetectionFlag, TObjectPtr<AActor>& CurrentDetected);

	// 현재 감지 대상 전환 + 상호작용 UI 위젯 토글 (IDRInteractable)
	void SetCurrentDetectedInteractable(AActor* NewDetected, TObjectPtr<AActor>& CurrentDetected);

	// 사이트 감지 변화를 조준선 UI(OverlayWidgetController)에 전달
	void NotifyCrosshairSiteDetected(bool bDetected) const;

	// 시점 광선 히트에서 대상 찾기
	ADRCleanserPart* FindPart() const;
	ADRCleanserSite* FindSite() const;
	ADRRobotVacuumCharacter* FindMount() const;
	ADRS2InteractProp* FindProp() const;
	ADRS2TrainCar* FindTrainCar() const;

	// ===== 시점 광선 (감지 주기마다 1회) =====
	bool GetInteractionViewRay(FVector& OutStart, FVector& OutEnd) const;
	void RefreshInteractionViewHits();
	const FHitResult* GetInteractionViewBlockingHit() const;
	TArray<FHitResult> InteractionViewHits;

	float LineTraceTimer = 0.f;

	// 오버랩으로 켜지는 감지 — 범위 안 후보 집합과 활성 여부
	bool bPartDetectionEnabled = false;
	bool bSiteDetectionEnabled = false;
	bool bMountDetectionEnabled = false;
	TSet<TWeakObjectPtr<AActor>> OverlappedParts;
	TSet<TWeakObjectPtr<AActor>> OverlappedSites;
	TSet<TWeakObjectPtr<AActor>> OverlappedMounts;

	// 지금 감지 중인 대상 (IDRInteractable 구현 액터)
	UPROPERTY()
	TObjectPtr<AActor> CurrentDetectedPart;

	UPROPERTY()
	TObjectPtr<AActor> CurrentDetectedSite;

	UPROPERTY()
	TObjectPtr<AActor> CurrentDetectedMount;

	// 스테이지2 상호작용 프롭 — 레버/버튼/단말을 하나의 슬롯으로 처리한다 (Plan6 §5.5-b)
	UPROPERTY()
	TObjectPtr<AActor> CurrentDetectedProp;

	// 열차 좌석 (Plan6 §5.5)
	UPROPERTY()
	TObjectPtr<AActor> CurrentDetectedCar;
};
