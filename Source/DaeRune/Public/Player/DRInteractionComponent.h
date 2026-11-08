// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Interaction/DRInteractable.h"
#include "DRInteractionComponent.generated.h"

class APawn;
class ADRPlayerController;
class ADRCharacter;
class ADRCleanserPart;
class ADRCleanserSite;
class ADRRobotVacuumCharacter;
class ADRS2InteractProp;
class ADRS2TrainCar;
class UDRInteractionPromptWidget;

/**
 * PlayerController 에 붙는 상호작용 대상 감지 + 화면 안내 (로컬 컨트롤러 전용).
 *
 * 카메라 시점 광선을 주기마다 한 번 쏴서 지금 F 로 상호작용할 대상을 고르고, 그 결과로 화면 안내
 * (UDRInteractionPromptWidget — 조준선 아래 "[F] 부품 줍기" + 들고 있는 부품 아이콘)를 갱신한다.
 * 부품 · 사이트 · 청소기는 그 액터의 오버랩 범위에 들어왔을 때만 감지를 켜고(Set*DetectionEnabled),
 * 스테이지2 프롭 · 열차 칸은 오버랩 없이 광선 거리로만 판정한다. 로비 장치(업그레이드 · 옷장 · 스테이지 선택 포털)는
 * 광선 없이 범위에 들어와 있는 동안 안내만 띄운다(SetStationInRange) — 그 입력은 OnInteractPressed 로 받는다.
 *
 * 서버 요청(ServerRequestInteract) · F 키 처리는 PlayerController 에 남는다 — RPC 는 소유 연결이 있는 PC 가 보낸다.
 * ★안내를 고르는 순서(BuildPrompt)는 ADRPlayerController::HandleInteract 와 같아야 한다★ — 안내와 F 가 할 일이 어긋나지 않게.
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

	// 범위로 켜지는 장치(IDRInteractable — 업그레이드 · 옷장 · 스테이지 선택 포털). 범위 안에 있는 동안 안내를 띄운다
	void SetStationInRange(AActor* Station, bool bInRange);

	// PlayerTick(로컬 컨트롤러)이 매 프레임 부른다 — LineTraceUpdateInterval 마다 감지 대상과 화면 안내를 갱신한다
	void TickDetection(float DeltaTime);

	// 화면 안내를 지금 상태로 바로 다시 정한다 (전체 화면을 열고 닫을 때 · 장치 범위 출입)
	void RefreshPrompt();

	// 화면 안내를 내린다 (관전 · 로비 연출처럼 감지를 쉬는 동안)
	void HidePrompt();

	// 지금 감지 중인 대상 (없으면 null)
	AActor* GetDetectedPart() const { return CurrentDetectedPart; }
	AActor* GetDetectedSite() const { return CurrentDetectedSite; }
	AActor* GetDetectedMount() const { return CurrentDetectedMount; }
	AActor* GetDetectedProp() const { return CurrentDetectedProp; }
	AActor* GetDetectedTrainCar() const { return CurrentDetectedCar; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 시점 광선 길이
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	float LineTraceDistance = 250.f;

	// 감지 갱신 간격 (초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction")
	float LineTraceUpdateInterval = 0.1f;

	// 화면 상호작용 안내 위젯 (BP_DRPlayerController 의 InteractionComponent 에서 WBP_InteractionPrompt 지정). 비면 안내를 띄우지 않는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|UI")
	TSubclassOf<UDRInteractionPromptWidget> PromptWidgetClass;

	// 안내 위젯의 뷰포트 Z 순서 — HUD 오버레이(0) 위, 결과창(100) 아래
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Interaction|UI")
	int32 PromptZOrder = 5;

private:
	ADRPlayerController* GetOwnerController() const;

	// 오버랩 집합 등록/해제 + 감지 플래그 갱신 + 이탈 시 감지 해제 (부품 · 사이트 · 청소기 공용)
	void SetInteractableDetectionEnabled(bool bEnabled, AActor* Interactable,
		TSet<TWeakObjectPtr<AActor>>& OverlapSet, bool& bDetectionFlag, TObjectPtr<AActor>& CurrentDetected);

	// 현재 감지 대상 전환 (사이트 슬롯이면 조준선에도 알린다)
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

	// 지난 감지 주기의 폰 — 바뀌면(첫 조종 · 리스폰) 주변 오버랩을 다시 본다
	TWeakObjectPtr<APawn> LastDetectionPawn;

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

	// ===== 화면 안내 =====

	// 지금 F(또는 점프키)가 할 일에 맞는 안내 — 순서는 ADRPlayerController::HandleInteract 와 같다
	FDRInteractionPrompt BuildPrompt() const;

	// 안내를 위젯에 넘긴다 (처음 띄울 때 위젯을 만든다)
	void ApplyPrompt(const FDRInteractionPrompt& Prompt);

	// 범위 안에 있는 장치 중 마지막에 들어온 것 — 폰이 지금 겹쳐 있는 것만 (없으면 null)
	AActor* GetStationInRange(const APawn* Pawn) const;

	// 범위 안에 있는 장치 (나중에 들어온 것이 뒤)
	TArray<TWeakObjectPtr<AActor>> StationsInRange;

	UPROPERTY(Transient)
	TObjectPtr<UDRInteractionPromptWidget> PromptWidget;
};
