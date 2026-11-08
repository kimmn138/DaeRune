// Copyright DaeRune


#include "Player/DRInteractionComponent.h"
#include "Player/DRPlayerController.h"
#include "Character/DRCharacter.h"
#include "Character/DRRobotVacuumCharacter.h"
#include "Actor/DRCleanserPart.h"
#include "Actor/DRCleanserSite.h"
#include "Actor/Stage2/DRS2InteractProp.h"
#include "Actor/Stage2/DRS2Train.h"
#include "Actor/Stage2/DRS2TrainCar.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Interaction/DRInteractable.h"
#include "Camera/CameraComponent.h"
#include "Engine/BlockingVolume.h"
#include "GameFramework/HUD.h"
#include "UI/HUD/DRHUD.h"
#include "UI/Widget/DRInteractionPromptWidget.h"
#include "UI/WidgetController/OverlayWidgetController.h"

UDRInteractionComponent::UDRInteractionComponent()
{
	// 틱은 PlayerController::PlayerTick 이 TickDetection 으로 준다 (로컬 컨트롤러 · 관전 아닐 때만)
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (PromptWidget)
	{
		PromptWidget->RemoveFromParent();
		PromptWidget = nullptr;
	}

	Super::EndPlay(EndPlayReason);
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

	// 이탈한 대상을 감지 중이었거나 감지가 꺼졌으면 감지 해제 (화면 안내는 다음 감지 주기에 따라온다)
	if (CurrentDetected == Interactable || !bDetectionFlag)
	{
		SetCurrentDetectedInteractable(nullptr, CurrentDetected);
	}
}

void UDRInteractionComponent::SetCurrentDetectedInteractable(AActor* NewDetected, TObjectPtr<AActor>& CurrentDetected)
{
	if (CurrentDetected == NewDetected) return;

	CurrentDetected = NewDetected;

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

// ========================= 범위로 켜지는 장치 =========================

void UDRInteractionComponent::SetStationInRange(AActor* Station, bool bInRange)
{
	if (!Station) return;

	StationsInRange.Remove(Station);
	StationsInRange.RemoveAll([](const TWeakObjectPtr<AActor>& Entry) { return !Entry.IsValid(); });

	// 들어올 때만 바로 띄운다 — 나갈 때는 장치 EndPlay(레벨 종료)에서도 불리므로 다음 감지 주기에 맡긴다
	if (bInRange)
	{
		StationsInRange.Add(Station);
		RefreshPrompt();
	}
}

AActor* UDRInteractionComponent::GetStationInRange(const APawn* Pawn) const
{
	// 장치가 범위 이탈을 놓쳐도(폰이 사라질 때) 안내가 남지 않게 지금 겹쳐 있는지도 본다
	for (int32 Index = StationsInRange.Num() - 1; Index >= 0; --Index)
	{
		AActor* Station = StationsInRange[Index].Get();
		if (Station && Pawn && Station->IsOverlappingActor(Pawn))
		{
			return Station;
		}
	}
	return nullptr;
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

	// 새 폰을 조종하게 된 첫 주기 — 스폰 중(컨트롤러가 붙기 전)에 시작된 오버랩은 감지를 켜지 못했으므로 다시 본다.
	// 클라는 폰(OnRep_Pawn)이 폰의 컨트롤러(OnRep_Controller)보다 먼저 올 수 있어 로컬 조종이 확인될 때까지 미룬다
	APawn* Pawn = GetOwnerController()->GetPawn();
	if (Pawn != LastDetectionPawn.Get() && (!Pawn || Pawn->IsLocallyControlled()))
	{
		LastDetectionPawn = Pawn;
		if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(Pawn))
		{
			DRCharacter->RefreshNearbyInteractions();
		}
	}

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

	// 감지 결과 · 부품 운반 · 탑승 상태로 화면 안내를 다시 정한다
	RefreshPrompt();
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

	// ★보이지 않는 이동 차단 볼륨(BlockingVolume)은 시선을 가리지 않는다★ — 스테이지 메시에는 충돌이 없고 볼륨이 충돌을 맡는데
	// 볼륨이 Visibility 도 막아서, 볼륨 면 바로 뒤에 있는 대상(스테이지2 금고 키패드)이 감지되지 않았다. 걸리면 빼고 다시 쏜다
	constexpr int32 MaxSkippedVolumes = 4;
	for (int32 Attempt = 0; Attempt <= MaxSkippedVolumes; ++Attempt)
	{
		InteractionViewHits.Reset();
		GetWorld()->LineTraceMultiByChannel(InteractionViewHits, Start, End, ECC_Visibility, QueryParams);

		const FHitResult* BlockingHit = GetInteractionViewBlockingHit();
		AActor* BlockingActor = BlockingHit ? BlockingHit->GetActor() : nullptr;
		if (!BlockingActor || !BlockingActor->IsA<ABlockingVolume>()) return;

		QueryParams.AddIgnoredActor(BlockingActor);
	}
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

	// 박스 오버랩 중인 사이트인지 확인 (다른 사이트가 시점에 잡히는 케이스 차단).
	// 다른 사람이 마지막 부품을 넣어 다 찼으면 고르지 않는다 — 그때 F 는 떨어뜨리기이고 안내도 그렇게 바뀐다
	const FHitResult* Hit = GetInteractionViewBlockingHit();
	ADRCleanserSite* HitSite = Hit ? Cast<ADRCleanserSite>(Hit->GetActor()) : nullptr;
	return (HitSite && OverlappedSites.Contains(HitSite) && HitSite->CanAcceptPart()) ? HitSite : nullptr;
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

// ========================= 화면 안내 =========================

namespace DRInteractionComponent
{
	// IDRInteractable 대상의 문구를 안내로 — 대상이 "지금은 못 쓴다"고 하면 키 없이 문구만
	void FillPromptFrom(const AActor* Target, const ADRCharacter* Interactor, const FKey& Key, FDRInteractionPrompt& OutPrompt)
	{
		const IDRInteractable* Interactable = Cast<IDRInteractable>(Target);
		if (!Interactable) return;

		bool bUsable = true;
		OutPrompt.Text = Interactable->GetInteractionPromptText(Interactor, bUsable);
		OutPrompt.Key = bUsable ? Key : FKey();
	}
}

FDRInteractionPrompt UDRInteractionComponent::BuildPrompt() const
{
	FDRInteractionPrompt Prompt;

	const ADRPlayerController* PC = GetOwnerController();
	if (!PC || !PC->IsLocalController()) return Prompt;

	// 전체 화면(설정 · 업그레이드 · 옷장 · 스테이지 선택 · 8퍼즐)이 떠 있거나 HUD 를 숨겼으면(9 키) 안내도 숨긴다
	if (PC->GetActiveScreen() != EDRScreen::None) return Prompt;
	if (const AHUD* HUD = PC->GetHUD(); HUD && !HUD->bShowHUD) return Prompt;

	const ADRCharacter* DRCharacter = PC->GetPawn<ADRCharacter>();
	if (!DRCharacter || DRCharacter->bDead) return Prompt;

	// ★아래 순서는 ADRPlayerController::HandleInteract 와 같다★
	const FKey InteractKey = PC->GetInteractKey();

	// 청소기 위 — F 는 막히고 점프키가 하차다
	if (const ADRRobotVacuumCharacter* Mount = DRCharacter->MountedOn)
	{
		Prompt.Text = Mount->GetDismountPromptText();
		Prompt.Key = PC->GetJumpKey();
		return Prompt;
	}

	// 부품을 든 채 — 사이트를 보면 설치, 아니면 떨어뜨리기. F 는 둘 중 하나로 끝난다
	if (DRCharacter->IsCarryingPart())
	{
		Prompt.bShowCarriedPart = true;

		if (CurrentDetectedSite)
		{
			DRInteractionComponent::FillPromptFrom(CurrentDetectedSite, DRCharacter, InteractKey, Prompt);
		}
		// 주운 직후(PartDropCooldown)에는 서버가 떨어뜨리기를 거절한다 — 그동안은 부품 아이콘만 띄운다
		else if (DRCharacter->CanDropCarriedPartNow())
		{
			if (const ADRCleanserPart* Part = DRCharacter->GetCarriedPart())
			{
				Prompt.Text = Part->GetDropPromptText();
				Prompt.Key = InteractKey;
			}
		}
		return Prompt;
	}

	// 부품 줍기 → 청소기 탑승 → 열차 칸 탑승 → 스테이지2 프롭 → 범위 장치(OnInteractPressed)
	const AActor* Target = CurrentDetectedPart ? CurrentDetectedPart.Get()
		: CurrentDetectedMount ? CurrentDetectedMount.Get()
		: CurrentDetectedCar ? CurrentDetectedCar.Get()
		: CurrentDetectedProp ? CurrentDetectedProp.Get()
		: GetStationInRange(DRCharacter);
	if (Target)
	{
		DRInteractionComponent::FillPromptFrom(Target, DRCharacter, InteractKey, Prompt);
		return Prompt;
	}

	// 열차 칸에 앉아 있으면 점프키가 하차다 — 열차가 서 있을 때만 내릴 수 있다 (Plan6 §14.6.5)
	if (const ADRS2TrainCar* Car = DRCharacter->SeatedOn)
	{
		const ADRS2Train* Train = Car->GetOwningTrain();
		if (!Train || Train->CanDeboardNow())
		{
			Prompt.Text = Car->GetDeboardPromptText();
			Prompt.Key = PC->GetJumpKey();
		}
	}
	return Prompt;
}

void UDRInteractionComponent::ApplyPrompt(const FDRInteractionPrompt& Prompt)
{
	if (Prompt.IsEmpty())
	{
		HidePrompt();
		return;
	}

	const UWorld* World = GetWorld();
	if (!PromptWidgetClass || !World || World->bIsTearingDown) return;

	// 처음 띄울 때 만든다. 심리스 이동은 PC 를 넘기지만 뷰포트 위젯은 이전 월드와 함께 내려가므로 그때도 새로 만든다
	if (!PromptWidget || !PromptWidget->IsInViewport())
	{
		ADRPlayerController* PC = GetOwnerController();
		if (!PC) return;

		PromptWidget = CreateWidget<UDRInteractionPromptWidget>(PC, PromptWidgetClass);
		if (!PromptWidget) return;

		PromptWidget->AddToViewport(PromptZOrder);
	}

	PromptWidget->SetPrompt(Prompt);
}

void UDRInteractionComponent::RefreshPrompt()
{
	ApplyPrompt(BuildPrompt());
}

void UDRInteractionComponent::HidePrompt()
{
	if (PromptWidget)
	{
		PromptWidget->HidePrompt();
	}
}
