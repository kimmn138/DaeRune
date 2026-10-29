// Copyright DaeRune


#include "Player/DRInteractionComponent.h"
#include "Player/DRPlayerController.h"
#include "Character/DRCharacter.h"
#include "Character/DRRobotVacuumCharacter.h"
#include "Actor/DRCleanserPart.h"
#include "Actor/DRCleanserSite.h"
#include "Actor/Stage2/DRS2InteractProp.h"
#include "Actor/Stage2/DRS2TrainCar.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Interaction/DRInteractable.h"
#include "Camera/CameraComponent.h"
#include "UI/HUD/DRHUD.h"
#include "UI/WidgetController/OverlayWidgetController.h"

UDRInteractionComponent::UDRInteractionComponent()
{
	// 틱은 PlayerController::PlayerTick 이 TickDetection 으로 준다 (로컬 컨트롤러 · 관전 아닐 때만)
	PrimaryComponentTick.bCanEverTick = false;
}

ADRPlayerController* UDRInteractionComponent::GetOwnerController() const
{
	return Cast<ADRPlayerController>(GetOwner());
}

// ========================= 오버랩으로 켜지는 감지 =========================

void UDRInteractionComponent::SetPartDetectionEnabled(bool bEnabled, ADRCleanserPart* Part)
{
	if (!Part) return;

	if (bEnabled)
	{
		LineTraceTimer = 0.f;
	}
	SetInteractableDetectionEnabled(bEnabled, Part, OverlappedParts, bPartDetectionEnabled, CurrentDetectedPart);
}

void UDRInteractionComponent::SetSiteDetectionEnabled(bool bEnabled, ADRCleanserSite* Site)
{
	SetInteractableDetectionEnabled(bEnabled, Site, OverlappedSites, bSiteDetectionEnabled, CurrentDetectedSite);
}

void UDRInteractionComponent::SetMountDetectionEnabled(bool bEnabled, ADRRobotVacuumCharacter* Mount)
{
	if (!Mount) return;

	if (bEnabled)
	{
		LineTraceTimer = 0.f;
	}
	SetInteractableDetectionEnabled(bEnabled, Mount, OverlappedMounts, bMountDetectionEnabled, CurrentDetectedMount);
}

void UDRInteractionComponent::SetInteractableDetectionEnabled(bool bEnabled, AActor* Interactable,
	TSet<TWeakObjectPtr<AActor>>& OverlapSet, bool& bDetectionFlag, TObjectPtr<AActor>& CurrentDetected)
{
	if (!Interactable) return;

	if (bEnabled)
	{
		OverlapSet.Add(Interactable);
		bDetectionFlag = true;
		return;
	}

	OverlapSet.Remove(Interactable);

	// 파괴된 대상의 잔여 엔트리 정리
	for (auto It = OverlapSet.CreateIterator(); It; ++It)
	{
		if (!It->IsValid())
		{
			It.RemoveCurrent();
		}
	}

	// 오버랩 중인 대상이 하나도 없을 때만 감지 비활성화
	if (OverlapSet.IsEmpty())
	{
		bDetectionFlag = false;
	}

	// 이탈한 대상을 감지 중이었거나 감지가 꺼졌으면 UI 제거
	if (CurrentDetected == Interactable || !bDetectionFlag)
	{
		SetCurrentDetectedInteractable(nullptr, CurrentDetected);
	}
}

void UDRInteractionComponent::SetCurrentDetectedInteractable(AActor* NewDetected, TObjectPtr<AActor>& CurrentDetected)
{
	if (CurrentDetected == NewDetected) return;

	if (IDRInteractable* OldInteractable = Cast<IDRInteractable>(CurrentDetected))
	{
		OldInteractable->SetInteractionUIVisible(false);
	}

	CurrentDetected = NewDetected;

	if (IDRInteractable* NewInteractable = Cast<IDRInteractable>(CurrentDetected))
	{
		NewInteractable->SetInteractionUIVisible(true);
	}

	// 사이트 감지 슬롯의 변화면 조준선 상태 갱신 (설치 가능 조준선 표시/해제)
	// 라인트레이스 갱신/오버랩 이탈 등 모든 감지 변경 경로가 이 함수를 지나므로 여기서 일괄 처리
	if (&CurrentDetected == &CurrentDetectedSite)
	{
		NotifyCrosshairSiteDetected(CurrentDetected != nullptr);
	}
}

void UDRInteractionComponent::NotifyCrosshairSiteDetected(bool bDetected) const
{
	const ADRPlayerController* PC = GetOwnerController();
	if (!PC || !PC->IsLocalController()) return;

	if (ADRHUD* DRHUD = PC->GetHUD<ADRHUD>())
	{
		if (UOverlayWidgetController* OverlayWC = DRHUD->GetOverlayWidgetControllerCached())
		{
			OverlayWC->SetCrosshairSiteDetected(bDetected);
		}
	}
}

// ========================= 주기 감지 =========================

void UDRInteractionComponent::TickDetection(float DeltaTime)
{
	// ★여기서 "전부 비활성화면 스킵" 같은 조기 리턴을 두면 안 된다.
	//   부품/사이트/청소기 감지는 오버랩으로 켜지는 방식이라 평소 전부 false 인데,
	//   스테이지2 프롭(레버·버튼·단말)과 열차 칸은 오버랩 게이트 없이 트레이스로만 판정한다.
	//   과거 그런 가드가 그 둘까지 막아 방2 레버에 다가가도 F 프롬프트가 뜨지 않았다.
	LineTraceTimer += DeltaTime;
	if (LineTraceTimer < LineTraceUpdateInterval) return;
	LineTraceTimer = 0.f;

	// 시점 광선 히트를 한 번 구해 아래 감지들이 나눠 쓴다
	RefreshInteractionViewHits();

	if (bPartDetectionEnabled)
	{
		SetCurrentDetectedInteractable(FindPart(), CurrentDetectedPart);
	}
	if (bSiteDetectionEnabled)
	{
		SetCurrentDetectedInteractable(FindSite(), CurrentDetectedSite);
	}
	if (bMountDetectionEnabled)
	{
		SetCurrentDetectedInteractable(FindMount(), CurrentDetectedMount);
	}

	// 스테이지2 상호작용 프롭 · 열차 좌석 — 오버랩 집합 없이 광선 거리로만 판정한다 (Plan6 §5.5 · §5.5-b)
	SetCurrentDetectedInteractable(FindProp(), CurrentDetectedProp);
	SetCurrentDetectedInteractable(FindTrainCar(), CurrentDetectedCar);
}

bool UDRInteractionComponent::GetInteractionViewRay(FVector& OutStart, FVector& OutEnd) const
{
	const ADRPlayerController* PC = GetOwnerController();
	const ADRCharacter* DRCharacter = PC ? PC->GetPawn<ADRCharacter>() : nullptr;
	const UCameraComponent* Camera = DRCharacter ? DRCharacter->GetFollowCamera() : nullptr;
	if (!Camera) return false;

	OutStart = Camera->GetComponentLocation();
	OutEnd = OutStart + Camera->GetForwardVector() * LineTraceDistance;
	return true;
}

void UDRInteractionComponent::RefreshInteractionViewHits()
{
	InteractionViewHits.Reset();

	FVector Start, End;
	if (!GetInteractionViewRay(Start, End)) return;

	// 부품 · 사이트 · 프롭 · 열차 칸이 같은 시점 광선을 쓴다 — Visibility 멀티 트레이스 한 번으로
	// 겹침 히트(부품)와 첫 막힘 히트(나머지)를 함께 얻는다
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetOwnerController()->GetPawn());
	GetWorld()->LineTraceMultiByChannel(InteractionViewHits, Start, End, ECC_Visibility, QueryParams);
}

const FHitResult* UDRInteractionComponent::GetInteractionViewBlockingHit() const
{
	// 멀티 트레이스는 첫 막힘 히트를 마지막에 둔다 (없으면 겹침만 있다)
	return (InteractionViewHits.Num() > 0 && InteractionViewHits.Last().bBlockingHit) ? &InteractionViewHits.Last() : nullptr;
}

// ========================= 대상 고르기 =========================

ADRCleanserPart* UDRInteractionComponent::FindPart() const
{
	// 캐릭터가 이미 부품을 들고 있으면 감지하지 않음
	const ADRCharacter* DRCharacter = GetOwnerController()->GetPawn<ADRCharacter>();
	if (!DRCharacter || DRCharacter->IsCarryingPart()) return nullptr;

	// 사이트 메시 등에 가려져도 부품을 찾을 수 있도록 겹침 히트까지 모두 본다
	for (const FHitResult& Hit : InteractionViewHits)
	{
		ADRCleanserPart* HitPart = Cast<ADRCleanserPart>(Hit.GetActor());
		if (HitPart && HitPart->CanBePickedUp())
		{
			return HitPart;
		}
	}
	return nullptr;
}

ADRCleanserSite* UDRInteractionComponent::FindSite() const
{
	// 부품을 들고 있어야만 사이트 감지
	const ADRCharacter* DRCharacter = GetOwnerController()->GetPawn<ADRCharacter>();
	if (!DRCharacter || !DRCharacter->IsCarryingPart()) return nullptr;

	// 박스 오버랩 중인 사이트인지 확인 (다른 사이트가 시점에 잡히는 케이스 차단)
	const FHitResult* Hit = GetInteractionViewBlockingHit();
	ADRCleanserSite* HitSite = Hit ? Cast<ADRCleanserSite>(Hit->GetActor()) : nullptr;
	return (HitSite && OverlappedSites.Contains(HitSite)) ? HitSite : nullptr;
}

ADRRobotVacuumCharacter* UDRInteractionComponent::FindMount() const
{
	ADRCharacter* DRCharacter = GetOwnerController()->GetPawn<ADRCharacter>();
	if (!DRCharacter) return nullptr;

	// 부품 소지 중에는 탑승 불가(F키 의미 충돌), 이미 탑승 중이면 감지 불필요
	if (DRCharacter->IsCarryingPart() || DRCharacter->IsMounted()) return nullptr;

	FVector Start, End;
	if (!GetInteractionViewRay(Start, End)) return nullptr;

	// 멀티 라인트레이스: 다른 액터에 일부 가려져도 청소기를 찾을 수 있도록 모든 히트 검사.
	// 채널(Visibility) 트레이스는 캐릭터를 못 맞춘다 — 캡슐(Pawn 프리셋)은 Visibility를 Ignore하고,
	// 스켈레탈 메시 히트는 피직스 에셋 유무에 좌우된다. Pawn "오브젝트 타입" 쿼리는 응답 설정과
	// 무관하게 캡슐을 맞추므로 청소기 감지가 메시 콜리전 세팅에 의존하지 않는다.
	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(DRCharacter);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);

	GetWorld()->LineTraceMultiByObjectType(HitResults, Start, End, ObjectParams, QueryParams);

	for (const FHitResult& Hit : HitResults)
	{
		ADRRobotVacuumCharacter* HitVacuum = Cast<ADRRobotVacuumCharacter>(Hit.GetActor());
		if (HitVacuum &&
			OverlappedMounts.Contains(HitVacuum) &&
			!HitVacuum->bDead &&
			HitVacuum->RiderOnTop == nullptr && // 이미 라이더가 있으면 프롬프트도 안 띄움 (서버가 최종 검증)
			// ★Plan5(탑승칩)★ ③ 이 청소기에서 비상 탈출한 뒤 20초는 프롬프트도 안 띄운다 (서버도 같은 함수로 거부 — D-90)
			!UDRAbilitySystemLibrary::IsRideBannedFrom(DRCharacter, HitVacuum))
		{
			return HitVacuum;
		}
	}
	return nullptr;
}

ADRS2InteractProp* UDRInteractionComponent::FindProp() const
{
	ADRCharacter* DRCharacter = GetOwnerController()->GetPawn<ADRCharacter>();
	if (!DRCharacter) return nullptr;

	// ★프롭 메시에 **심플 콜리전**이 있어야 한다. 라인트레이스는 bTraceComplex=false 라
	//   콜리전 프리미티브가 없는 메시는 그대로 통과해 감지되지 않는다.
	const FHitResult* Hit = GetInteractionViewBlockingHit();
	ADRS2InteractProp* HitProp = Hit ? Cast<ADRS2InteractProp>(Hit->GetActor()) : nullptr;
	return (HitProp && HitProp->CanInteract(DRCharacter)) ? HitProp : nullptr;
}

ADRS2TrainCar* UDRInteractionComponent::FindTrainCar() const
{
	// 이미 앉아 있거나 부품을 들고 있으면 감지하지 않는다
	ADRCharacter* DRCharacter = GetOwnerController()->GetPawn<ADRCharacter>();
	if (!DRCharacter || DRCharacter->IsSeatedOnTrain() || DRCharacter->IsCarryingPart()) return nullptr;

	const FHitResult* Hit = GetInteractionViewBlockingHit();
	ADRS2TrainCar* HitCar = Hit ? Cast<ADRS2TrainCar>(Hit->GetActor()) : nullptr;
	return (HitCar && HitCar->CanBeBoardedBy(DRCharacter)) ? HitCar : nullptr;
}
