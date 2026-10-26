// Copyright DaeRune


#include "Player/DRPlayerController.h"
#include "UI/Widget/DRWardrobeScreenWidget.h"
#include "UI/Widget/DRStageSelectScreenWidget.h"
#include "Game/DRProgressionConfig.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DRGameplayTags.h"
#include "EnhancedInputSubsystems.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "Input/DRInputComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UI/Widget/DamageTextComponent.h"
#include "Actor/DRCleanserPart.h"
#include "Actor/DRCleanserSite.h"
#include "Actor/Stage2/DRS2InteractProp.h"
#include "Actor/Stage2/DRS2SlidePuzzle.h"
#include "UI/Widget/Stage2/DRS2SlidePuzzleWidget.h"
#include "Actor/Stage2/DRS2TrainCar.h"
#include "Actor/Stage2/DRS2Train.h"
#include "Interaction/DRInteractable.h"
#include "Actor/DRWaitingRoomCameraActor.h"
#include "Tutorial/DRTutorialManager.h"
#include "Character/DRCharacter.h"
#include "Character/DREnemy.h"
#include "Character/DRRobotVacuumCharacter.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Camera/CameraComponent.h"
#include "Game/DRStageGameMode.h"
#include "Phase/DRPhase3.h"
#include "Game/DRStageGameState.h"
#include "Game/DRLobbyGameState.h"
#include "UI/HUD/DRHUD.h"
#include "UI/Widget/DRUserWidget.h"
#include "Player/DRPlayerState.h"
#include "UI/WidgetController/DRWidgetController.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "Game/DRSettingsManager.h"
#include "Game/DRGameUserSettings.h"
#include "Actor/DRBGMActor.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Widget/DRWaitingRoomWidget.h"
#include "UI/DRUpgradeUILibrary.h"
#include "Game/DRLobbyGameMode.h"
#include "Game/DRGameInstance.h"
#include "Game/DRCosmeticCatalog.h"
#include "Game/DRChipCatalog.h"
#include "DaeRune/DRLogChannels.h"
#include "MultiplayerSessionsSubsystem.h"

ADRPlayerController::ADRPlayerController()
{
	// 멀티플레이 리플리케이션 활성화
	bReplicates = true;
	
	// 부품 시스템 초기화
	bPartDetectionEnabled = false;
	CurrentDetectedPart = nullptr;
	LineTraceTimer = 0.f;
}

void ADRPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter)
{
	// 내가 적을 타격한 경우 조준선 히트마커 표시
	// (타겟이 플레이어인 경우는 내가 피격당한 폴백 경로이므로 제외)
	if (IsValid(TargetCharacter) && IsLocalController() && Cast<ADREnemy>(TargetCharacter))
	{
		if (ADRHUD* DRHUD = GetHUD<ADRHUD>())
		{
			if (UOverlayWidgetController* OverlayWC = DRHUD->GetOverlayWidgetControllerCached())
			{
				OverlayWC->NotifyCrosshairEnemyHit();
			}
		}
	}

	if (IsValid(TargetCharacter) && DamageTextComponentClass && IsLocalController())
	{
		// 만료된(BP 애니메이션 종료 후 파괴된) 항목 정리
		ActiveDamageTexts.RemoveAll([](const TWeakObjectPtr<UDamageTextComponent>& Text)
		{
			return !Text.IsValid();
		});

		// 동시 표시 상한 - AoE 다중 타격 시 컴포넌트 생성 스파이크 방지 (코스메틱이라 스킵해도 무방)
		if (ActiveDamageTexts.Num() >= MaxConcurrentDamageTexts)
		{
			return;
		}

		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
		DamageText->RegisterComponent();

		// 타겟 캐릭터에 붙였다가 떼어 월드 위치 고정
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		DamageText->SetDamageText(DamageAmount);
		ActiveDamageTexts.Add(DamageText);
	}
}

void ADRPlayerController::SetPartDetectionEnabled(bool bEnabled, ADRCleanserPart* Part)
{
	if (!Part) return;

	if (bEnabled)
	{
		LineTraceTimer = 0.f;
	}
	SetInteractableDetectionEnabled(bEnabled, Part, OverlappedParts, bPartDetectionEnabled, CurrentDetectedPart);
}

void ADRPlayerController::SetInteractableDetectionEnabled(bool bEnabled, AActor* Interactable,
	TSet<TWeakObjectPtr<AActor>>& OverlapSet, bool& bDetectionFlag, TObjectPtr<AActor>& CurrentDetected)
{
	if (!Interactable) return;

	if (bEnabled)
	{
		OverlapSet.Add(Interactable);
		bDetectionFlag = true;
	}
	else
	{
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
}

void ADRPlayerController::SetCurrentDetectedInteractable(AActor* NewDetected, TObjectPtr<AActor>& CurrentDetected)
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
	if (&CurrentDetected == &CurrentOverlappedSite)
	{
		NotifyCrosshairSiteDetected(CurrentDetected != nullptr);
	}
}

void ADRPlayerController::NotifyCrosshairSiteDetected(bool bDetected)
{
	if (!IsLocalController()) return;

	if (ADRHUD* DRHUD = GetHUD<ADRHUD>())
	{
		if (UOverlayWidgetController* OverlayWC = DRHUD->GetOverlayWidgetControllerCached())
		{
			OverlayWC->SetCrosshairSiteDetected(bDetected);
		}
	}
}

ADRCleanserPart* ADRPlayerController::FindPartByLineTrace()
{
	// 캐릭터가 이미 부품을 들고 있으면 감지하지 않음
	const ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
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

void ADRPlayerController::ClientShowPartPickupUI_Implementation()
{
	OnPartPickedUp();
}

void ADRPlayerController::SetSiteDetectionEnabled(bool bEnabled, ADRCleanserSite* Site)
{
	SetInteractableDetectionEnabled(bEnabled, Site, OverlappedSites, bSiteDetectionEnabled, CurrentOverlappedSite);
}

void ADRPlayerController::SetMountDetectionEnabled(bool bEnabled, ADRRobotVacuumCharacter* Mount)
{
	if (!Mount) return;

	if (bEnabled)
	{
		LineTraceTimer = 0.f;
	}
	SetInteractableDetectionEnabled(bEnabled, Mount, OverlappedMounts, bMountDetectionEnabled, CurrentDetectedMount);
}

ADRRobotVacuumCharacter* ADRPlayerController::FindMountByLineTrace()
{
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
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

	GetWorld()->LineTraceMultiByObjectType(
		HitResults,
		Start,
		End,
		ObjectParams,
		QueryParams
	);

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

void ADRPlayerController::ServerRequestDismount_Implementation()
{
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter || !DRCharacter->IsMounted()) return;
	if (!IsValid(DRCharacter->MountedOn)) return;

	DRCharacter->MountedOn->DismountRider(true);
}

void ADRPlayerController::ServerSetMountedYaw_Implementation(float NewYaw)
{
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter || !DRCharacter->IsMounted()) return;

	// 서버 액터 요 갱신 → AttachmentReplication 상대 회전으로 다른 클라이언트에 전파
	DRCharacter->SetActorRotation(FRotator(0.f, NewYaw, 0.f));
}

void ADRPlayerController::ServerRequestDashBrake_Implementation()
{
	// 서버 재검증: 실제 지속 돌진 중인 청소기만 브레이크 이벤트 전달
	ADRRobotVacuumCharacter* Vacuum = GetPawn<ADRRobotVacuumCharacter>();
	if (!Vacuum || !Vacuum->bSustainedDash || Vacuum->bDead) return;

	FGameplayEventData EventData;
	EventData.Instigator = Vacuum;
	EventData.Target = Vacuum;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Vacuum, FDRGameplayTags::Get().Event_Dash_Brake, EventData);
}

ADRCleanserSite* ADRPlayerController::FindSiteByLineTrace()
{
	// 부품을 들고 있어야만 사이트 감지
	const ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter || !DRCharacter->IsCarryingPart()) return nullptr;

	// 박스 오버랩 중인 사이트인지 확인 (다른 사이트가 시점에 잡히는 케이스 차단)
	const FHitResult* Hit = GetInteractionViewBlockingHit();
	ADRCleanserSite* HitSite = Hit ? Cast<ADRCleanserSite>(Hit->GetActor()) : nullptr;
	return (HitSite && OverlappedSites.Contains(HitSite)) ? HitSite : nullptr;
}

void ADRPlayerController::ServerReportTutorialCharacterInfoOpened_Implementation()
{
	// 튜토리얼 매니저에 캐릭터 설명창이 열렸음을 보고 (서버 권한)
	if (ADRTutorialManager* TM = Cast<ADRTutorialManager>(
		UGameplayStatics::GetActorOfClass(GetWorld(), ADRTutorialManager::StaticClass())))
	{
		TM->ReportCharacterInfoOpened();
	}
}
void ADRPlayerController::ServerRequestInteract_Implementation(AActor* Interactable)
{
	if (!Interactable) return;

	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// 부품 획득 요청
	if (ADRCleanserPart* Part = Cast<ADRCleanserPart>(Interactable))
	{
		// 서버 측 검증: 클라이언트가 보낸 포인터를 그대로 신뢰하지 않고 상태/거리 확인
		// (거리 검증 없이는 맵 반대편 부품을 줍는 치트가 가능)
		if (!Part->CanBePickedUp()) return;

		const float DistSq = FVector::DistSquared(DRCharacter->GetActorLocation(), Part->GetActorLocation());
		if (DistSq > FMath::Square(MaxInteractDistance)) return;

		DRCharacter->PickupPart(Part);
		return;
	}

	// 사이트에 부품 설치 요청
	if (ADRCleanserSite* Site = Cast<ADRCleanserSite>(Interactable))
	{
		// 서버 측 검증: 캐릭터가 실제로 사이트(상호작용 박스)와 오버랩 중인지 확인
		if (!Site->IsOverlappingActor(DRCharacter)) return;

		Site->InstallPart(DRCharacter);
		return;
	}

	// 열차 좌석 탑승 요청 (Plan6 §5.5)
	if (ADRS2TrainCar* Car = Cast<ADRS2TrainCar>(Interactable))
	{
		// 서버 측 검증: 상태 + 거리
		if (!Car->CanBeBoardedBy(DRCharacter)) return;

		const float CarDistSq = FVector::DistSquared(DRCharacter->GetActorLocation(), Car->GetActorLocation());
		if (CarDistSq > FMath::Square(MaxInteractDistance)) return;

		Car->Board(DRCharacter);
		return;
	}

	// 스테이지2 상호작용 프롭 요청 (Plan6 §5.5-b)
	// 레버/버튼/단말 20여 개를 이 분기 하나로 처리한다 (실제 로직은 소유 퍼즐이 수행).
	if (ADRS2InteractProp* Prop = Cast<ADRS2InteractProp>(Interactable))
	{
		// 서버 측 검증: 클라이언트가 보낸 포인터를 그대로 신뢰하지 않고 상태/거리 확인
		if (!Prop->CanInteract(DRCharacter))
		{
			// 프롬프트는 떴는데 눌러도 반응이 없는 두 원인 중 하나다 (비활성 / 사망 / 부품 운반 중).
			UE_LOG(LogDR, Warning, TEXT("[S2Prop] %s 조작 거부: CanInteract=false"), *Prop->GetName());
			return;
		}

		// ★거리는 '보이는 물체' 기준으로 잰다 (2026-09-21)★
		// 프롭의 루트는 빈 SceneComponent 다 (눌림 연출 때문에 PropMesh 를 자식으로 내렸다).
		// 그래서 BP 에서 메시를 루트에서 떼어 두면 액터 원점과 실제로 보이는 물체가 갈라지고,
		// 원점 기준으로 재면 눈앞에 서 있어도 거부된다 — 프롬프트는 메시에 닿은 라인트레이스라
		// 멀쩡히 떠 있으므로 증상이 "F 를 눌러도 아무 반응이 없다"로만 나타나 원인을 찾기 어렵다.
		// 콜리전까지의 최단 거리로 재면 메시를 어디에 두든 플레이어가 보는 것과 판정이 일치한다.
		FVector ClosestPoint = FVector::ZeroVector;
		float Dist = Prop->ActorGetDistanceToCollision(DRCharacter->GetActorLocation(), ECC_Visibility, ClosestPoint);
		if (Dist < 0.f)
		{
			// 콜리전 프리미티브가 없는 프롭 (라인트레이스에도 안 잡히지만 방어적으로 둔다)
			Dist = FVector::Dist(DRCharacter->GetActorLocation(), Prop->GetActorLocation());
		}

		if (Dist > MaxInteractDistance)
		{
			UE_LOG(LogDR, Warning, TEXT("[S2Prop] %s 조작 거부: 거리 %.0f > %.0f"),
				*Prop->GetName(), Dist, MaxInteractDistance);
			return;
		}

		UE_LOG(LogDR, Log, TEXT("[S2Prop] %s 조작 수락 (서버)"), *Prop->GetName());

		Prop->ServerHandleInteract(DRCharacter);
		return;
	}

	// 로봇 청소기 탑승 요청 (Plan3 §5.2)
	if (ADRRobotVacuumCharacter* Mount = Cast<ADRRobotVacuumCharacter>(Interactable))
	{
		// 서버 측 검증: 라이더 유무/중복 탑승/순환/사망/부품/스턴 (CanBeMountedBy) + 거리
		if (!Mount->CanBeMountedBy(DRCharacter)) return;

		const float DistSq = FVector::DistSquared(DRCharacter->GetActorLocation(), Mount->GetActorLocation());
		if (DistSq > FMath::Square(MaxInteractDistance)) return;

		Mount->MountRider(DRCharacter);
	}
}

void ADRPlayerController::ClientStartSpectating_Implementation()
{
	if (!IsLocalController()) return;
    
	bIsSpectating = true;
	CurrentSpectatedPlayerIndex = 0;

	ADRGameStateBase* GameStateBase = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!GameStateBase) return;

	TArray<ADRCharacter*> AlivePlayers = GameStateBase->GetAlivePlayers();
	
	if (AlivePlayers.Num() > 0)
	{
		SetSpectateTarget(AlivePlayers[0]);
	}
}

void ADRPlayerController::ClientStopSpectating_Implementation()
{
	if (!IsLocalController()) return;
	// 관전 상태 강제 초기화 (bIsSpectating 여부와 상관없이)
	bIsSpectating = false;
	CurrentSpectatedPlayerIndex = 0;

	UnbindSpectatedDeath();
	CurrentSpectatedCharacter.Reset();

	// 관전 중 HUD 가 다른 플레이어에게 묶여 있었다면 내 상태로 되돌린다 (부활)
	RestoreOwnOverlay();

	// 대기실이면 ViewTarget 복원 스킵 (ClientSetWaitingRoomView가 담당)
	if (bIsInWaitingRoom)
	{
		// FreeRoam에서 ClientStopSpectating이 호출된 경우 대기실 플래그 교정
		ADRLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<ADRLobbyGameState>() : nullptr;
		if (LGS && LGS->GetLobbyState() == ELobbyState::FreeRoam)
		{
			bIsInWaitingRoom = false;
			// 아래로 계속 진행하여 ViewTarget 복원
		}
		else
		{
			return;
		}
	}

	// ViewTarget 복원
	if (APawn* MyPawn = GetPawn())
	{
		SetViewTarget(MyPawn);
		RestoreDefaultInputMode();
	}
	else
	{
		// Pawn이 아직 복제되지 않았으면 OnPossessedPawnChanged 이벤트에서 복원
		bPendingViewTargetRestore = true;
	}
}

void ADRPlayerController::ScheduleSpectating(float Delay)
{
	if (!HasAuthority()) return;

	GetWorldTimerManager().SetTimer(PendingSpectateTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			// 예약과 실행 사이에 다른 경로로 새 폰을 받았으면(부활) 관전으로 넘어가지 않는다
			const APawn* MyPawn = GetPawn();
			if (MyPawn && !(MyPawn->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(MyPawn)))
			{
				return;
			}
			ClientStartSpectating();
		}),
		Delay, false);
}

void ADRPlayerController::CancelPendingSpectate()
{
	GetWorldTimerManager().ClearTimer(PendingSpectateTimerHandle);
}

void ADRPlayerController::RestoreOwnOverlay()
{
	ADRHUD* DRHUD = GetHUD<ADRHUD>();
	ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
	if (!DRHUD || !DRHUD->GetOverlayWidget() || !DRPS) return;

	UAbilitySystemComponent* ASC = DRPS->GetAbilitySystemComponent();
	UAttributeSet* AS = DRPS->GetAttributeSet();
	if (!ASC || !AS) return;

	DRHUD->RebindOverlay(this, DRPS, ASC, AS);
}

bool ADRPlayerController::SpectateOnlySurvivor(const TArray<ADRCharacter*>& AliveCharacters)
{
	if (AliveCharacters.Num() != 1) return false;

	// 생존자가 한 명뿐이면 그 사람으로 고정한다 — 보던 대상이 막 죽어 마지막 생존자로 넘어가야 하는 경우 포함
	if (CurrentSpectatedCharacter.Get() != AliveCharacters[0])
	{
		CurrentSpectatedPlayerIndex = 0;
		SetSpectateTarget(AliveCharacters[0]);
	}
	return true;
}

void ADRPlayerController::SpectateNextPlayer()
{
	if (!bIsSpectating || !IsLocalController()) return;

	ADRGameStateBase* GameStateBase = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!GameStateBase) return;

	TArray<ADRCharacter*> AliveCharacters = GameStateBase->GetAlivePlayers();
	if (AliveCharacters.Num() == 0)
	{
		// 모두 사망 - 관전 대상 없음
		CurrentSpectatedCharacter.Reset();
		return;
	}

	if (SpectateOnlySurvivor(AliveCharacters)) return;

	// 다음 인덱스 계산
	CurrentSpectatedPlayerIndex = (CurrentSpectatedPlayerIndex + 1) % AliveCharacters.Num();
    
	// 새 관전 대상 설정
	SetSpectateTarget(AliveCharacters[CurrentSpectatedPlayerIndex]);
}

void ADRPlayerController::SpectatePreviousPlayer()
{
	if (!bIsSpectating || !IsLocalController()) return;

	ADRGameStateBase* GameStateBase = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!GameStateBase) return;

	TArray<ADRCharacter*> AliveCharacters = GameStateBase->GetAlivePlayers();
	if (AliveCharacters.Num() == 0)
	{
		CurrentSpectatedCharacter.Reset();
		return;
	}

	if (SpectateOnlySurvivor(AliveCharacters)) return;

	// 이전 인덱스 계산
	CurrentSpectatedPlayerIndex--;
	if (CurrentSpectatedPlayerIndex < 0)
	{
		CurrentSpectatedPlayerIndex = AliveCharacters.Num() - 1;
	}

	// 새 관전 대상 설정
	SetSpectateTarget(AliveCharacters[CurrentSpectatedPlayerIndex]);
}

void ADRPlayerController::ClientStopAllAudio_Implementation()
{
	// BGM 정지
	TArray<AActor*> BGMActors;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADRBGMActor::StaticClass(), BGMActors);
	for (AActor* Actor : BGMActors)
	{
		if (ADRBGMActor* BGMActor = Cast<ADRBGMActor>(Actor))
		{
			BGMActor->StopBGM(0.0f);
		}
	}

}

void ADRPlayerController::CheatSkipToNextPhase()
{
// 개발 빌드에서만 동작하도록 체크
#if !UE_BUILD_SHIPPING
	ServerCheatSkipToNextPhase();
#endif
}

void ADRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Pawn 복제 도착 이벤트 바인딩 (타이머 재시도 대체)
	OnPossessedPawnChanged.AddDynamic(this, &ADRPlayerController::HandlePossessedPawnChanged);

	// Enhanced Input Context가 설정되어 있는지 확인
	check(DRContext);

	// Enhanced Input 서브시스템에 매핑 컨텍스트 추가
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(DRContext, 0);
	}

	// UI 설정
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	// 플레이어는 TeamId 0
	SetGenericTeamId(FGenericTeamId(0));

	// 카메라 피치 제한 설정
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -ViewPitchMin;  // 아래 (음수)
		PlayerCameraManager->ViewPitchMax = ViewPitchMax;   // 위 (양수)
	}

	// 로컬 플레이어만 오디오 설정 적용
	if (IsLocalController())
	{
		// Manager 통해서 오디오 설정 적용
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDRSettingsManager* Manager = GI->GetSubsystem<UDRSettingsManager>())
			{
				Manager->ApplyAudioSettings();

				// 마우스 감도 캐시 초기화 + 변경 델리게이트 구독 (Look()의 매 입력 Subsystem 조회 제거)
				if (UDRGameUserSettings* Settings = Manager->GetSettings())
				{
					CachedMouseSensitivity = Settings->MouseSensitivity;
				}
				Manager->OnMouseSensitivityChanged.AddUniqueDynamic(this, &ADRPlayerController::HandleMouseSensitivityChanged);
			}
		}
	}

	RestoreDefaultInputMode();
}

void ADRPlayerController::HandleMouseSensitivityChanged(float NewSensitivity)
{
	CachedMouseSensitivity = NewSensitivity;
}

void ADRPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);

	// Tab Hold 안전장치: 누른 채로 설정창이 열리거나 컨텍스트가 깨지면 Completed 이벤트가 유실될 수 있어 강제 숨김
	if (bIsCharacterInfoVisible && !CanShowCharacterInfo())
	{
		if (ADRHUD* DRHUD = GetHUD<ADRHUD>())
		{
			DRHUD->HideCharacterInfo();
		}
		bIsCharacterInfoVisible = false;
	}

	if (bIsSpectating) return;

	// 로컬 컨트롤러에서만 라인트레이스 실행
	if (!IsLocalController()) return;

	// 탑승 중 요 동기화: MOVE_None이라 CMC 무브가 요를 서버로 안 나르므로 주기적으로 직접 전송 (클라 전용)
	if (!HasAuthority())
	{
		const ADRCharacter* MountedChar = GetPawn<ADRCharacter>();
		if (MountedChar && MountedChar->IsMounted())
		{
			MountedYawSyncTimer += DeltaTime;
			const float ControlYaw = GetControlRotation().Yaw;
			if (MountedYawSyncTimer >= 0.1f && !FMath::IsNearlyEqual(ControlYaw, LastSentMountedYaw, 0.5f))
			{
				MountedYawSyncTimer = 0.f;
				LastSentMountedYaw = ControlYaw;
				ServerSetMountedYaw(ControlYaw);
			}
		}
	}

	// ★여기서 조기 리턴하면 안 된다.
	//   부품/사이트/청소기 감지는 오버랩으로 켜지는 방식이라 평소 전부 false 인데,
	//   스테이지2 프롭(레버·버튼·단말)과 열차 칸은 오버랩 게이트 없이 트레이스로만 판정한다.
	//   과거 이 자리에 있던 "전부 비활성화면 스킵" 가드가 그 둘까지 막아
	//   방2 레버에 다가가도 F 프롬프트가 뜨지 않았다.

	LineTraceTimer += DeltaTime;
	if (LineTraceTimer >= LineTraceUpdateInterval)
	{
		LineTraceTimer = 0.f;

		// 시점 광선 히트를 한 번 구해 아래 감지들이 나눠 쓴다
		RefreshInteractionViewHits();

		// 부품 라인트레이스
		if (bPartDetectionEnabled)
		{
			SetCurrentDetectedInteractable(FindPartByLineTrace(), CurrentDetectedPart);
		}

		// 사이트 라인트레이스
		if (bSiteDetectionEnabled)
		{
			SetCurrentDetectedInteractable(FindSiteByLineTrace(), CurrentOverlappedSite);
		}

		// 청소기 탑승 라인트레이스
		if (bMountDetectionEnabled)
		{
			SetCurrentDetectedInteractable(FindMountByLineTrace(), CurrentDetectedMount);
		}

		// 스테이지2 상호작용 프롭 라인트레이스 (Plan6 §5.5-b)
		// 레버/버튼/단말을 하나의 슬롯으로 처리한다. 오버랩 집합 없이 트레이스 거리로만 판정한다.
		SetCurrentDetectedInteractable(FindPropByLineTrace(), CurrentDetectedProp);

		// 열차 좌석 라인트레이스 (Plan6 §5.5)
		SetCurrentDetectedInteractable(FindTrainCarByLineTrace(), CurrentDetectedCar);
	}
}

ADRS2TrainCar* ADRPlayerController::FindTrainCarByLineTrace()
{
	// 이미 앉아 있거나 부품을 들고 있으면 감지하지 않는다
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter || DRCharacter->IsSeatedOnTrain() || DRCharacter->IsCarryingPart()) return nullptr;

	const FHitResult* Hit = GetInteractionViewBlockingHit();
	ADRS2TrainCar* HitCar = Hit ? Cast<ADRS2TrainCar>(Hit->GetActor()) : nullptr;
	return (HitCar && HitCar->CanBeBoardedBy(DRCharacter)) ? HitCar : nullptr;
}

bool ADRPlayerController::GetInteractionViewRay(FVector& OutStart, FVector& OutEnd) const
{
	const ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	const UCameraComponent* Camera = DRCharacter ? DRCharacter->GetFollowCamera() : nullptr;
	if (!Camera) return false;

	OutStart = Camera->GetComponentLocation();
	OutEnd = OutStart + Camera->GetForwardVector() * LineTraceDistance;
	return true;
}

void ADRPlayerController::RefreshInteractionViewHits()
{
	InteractionViewHits.Reset();

	FVector Start, End;
	if (!GetInteractionViewRay(Start, End)) return;

	// 부품 · 사이트 · 프롭 · 열차 칸이 같은 시점 광선을 쓴다 — Visibility 멀티 트레이스 한 번으로
	// 겹침 히트(부품)와 첫 막힘 히트(나머지)를 함께 얻는다 (예전에는 같은 광선을 최대 4번 쐈다)
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(GetPawn());
	GetWorld()->LineTraceMultiByChannel(InteractionViewHits, Start, End, ECC_Visibility, QueryParams);
}

const FHitResult* ADRPlayerController::GetInteractionViewBlockingHit() const
{
	// 멀티 트레이스는 첫 막힘 히트를 마지막에 둔다 (없으면 겹침만 있다)
	return (InteractionViewHits.Num() > 0 && InteractionViewHits.Last().bBlockingHit) ? &InteractionViewHits.Last() : nullptr;
}

void ADRPlayerController::ServerRequestTrainDeboard_Implementation()
{
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	ADRS2TrainCar* Car = DRCharacter->SeatedOn;
	if (!Car) return;

	// 이동 중 하차 거부 (Plan6 §14.6.5)
	if (ADRS2Train* Train = Car->GetOwningTrain())
	{
		if (!Train->CanDeboardNow()) return;
	}

	Car->Deboard();
}

ADRS2InteractProp* ADRPlayerController::FindPropByLineTrace()
{
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return nullptr;

	// ★프롭 메시에 **심플 콜리전**이 있어야 한다. 라인트레이스는 bTraceComplex=false 라
	//   콜리전 프리미티브가 없는 메시는 그대로 통과해 감지되지 않는다.
	const FHitResult* Hit = GetInteractionViewBlockingHit();
	ADRS2InteractProp* HitProp = Hit ? Cast<ADRS2InteractProp>(Hit->GetActor()) : nullptr;
	return (HitProp && HitProp->CanInteract(DRCharacter)) ? HitProp : nullptr;
}

void ADRPlayerController::Client_OpenSlidePuzzleUI_Implementation(ADRS2SlidePuzzle* Puzzle)
{
	OpenSlidePuzzleScreen(Puzzle);
}

void ADRPlayerController::OpenSlidePuzzleScreen(ADRS2SlidePuzzle* Puzzle)
{
	if (!IsLocalController() || !IsValid(Puzzle))
	{
		UE_LOG(LogDR, Warning, TEXT("[S2Puzzle] 창 열기 무시 (로컬 컨트롤러=%d, 퍼즐=%s)"),
			IsLocalController() ? 1 : 0, *GetNameSafe(Puzzle));
		return;
	}

	// 다른 전체화면 UI 와 입력 모드가 겹치지 않게 먼저 닫는다 (OpenUpgradeScreen 과 같은 가드)
	if (bIsSettingsMenuOpen)
	{
		CloseSettingsMenu();
	}

	// 이미 열려 있으면 아무것도 하지 않는다 (같은 단말을 두 번 눌렀을 때).
	if (bIsSlidePuzzleOpen)
	{
		UE_LOG(LogDR, Warning, TEXT("[S2Puzzle] 창이 이미 열린 상태로 기록돼 있다 - 열기를 건너뛴다"));
		return;
	}

	if (!SlidePuzzleWidgetClass)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2Puzzle] SlidePuzzleWidgetClass 미지정 - BP_DRPlayerController 의 UI|Stage2 에 WBP_S2SlidePuzzle 을 넣을 것"));
		return;
	}

	// ★창은 재사용한다★ 판 상태는 서버에 있으므로 새로 만들어도 동작하지만,
	// 재사용하면 등장 연출이 매번 다시 돌지 않고 조각 8개를 다시 만들 일도 줄어든다.
	if (!IsValid(SlidePuzzleWidget))
	{
		SlidePuzzleWidget = CreateWidget<UDRS2SlidePuzzleWidget>(this, SlidePuzzleWidgetClass);
		if (!SlidePuzzleWidget)
		{
			UE_LOG(LogDR, Error, TEXT("[S2Puzzle] 8퍼즐 창 생성 실패 (SlidePuzzleWidgetClass 확인)"));
			return;
		}
	}

	bIsSlidePuzzleOpen = true;

	// ★입력 모드는 여기서만 바꾼다★ (대기실 UI · 업그레이드 · 옷장과 같은 위치)
	//  포커스 위젯을 지정하지 않는 이유: 위젯이 NativeConstruct(=아래 AddToViewport) 에서
	//  SetKeyboardFocus 를 부르므로, 순서상 위젯이 마지막에 포커스를 가져간다.
	SetInputMode(FInputModeUIOnly());
	SetShowMouseCursor(true);

	// ★BindToPuzzle 이 AddToViewport 보다 먼저다★
	// 그래야 NativeConstruct 가 조각을 만든 직후 서버 보드를 그대로 그릴 수 있다.
	SlidePuzzleWidget->BindToPuzzle(Puzzle);
	SlidePuzzleWidget->AddToViewport(10);

	UE_LOG(LogDR, Log, TEXT("[S2Puzzle] 8퍼즐 창을 열었다 (%s)"), *GetNameSafe(SlidePuzzleWidget));

	OnSlidePuzzleScreenOpened(Puzzle);   // BP 연출 훅 (선택)
}

void ADRPlayerController::CloseSlidePuzzleScreen()
{
	if (!IsLocalController()) return;
	if (!bIsSlidePuzzleOpen) return;

	// ★플래그를 먼저 내린다★
	// 아래 RemoveFromParent 가 위젯의 NativeDestruct 를 부르고, 거기서 이 함수가 한 번 더 들어온다.
	bIsSlidePuzzleOpen = false;

	if (IsValid(SlidePuzzleWidget))
	{
		// 파괴하지 않는다. 다음에 열 때 같은 인스턴스를 다시 쓴다.
		SlidePuzzleWidget->RemoveFromParent();
	}

	OnSlidePuzzleScreenClosed();   // BP 연출 훅 (선택)

	// 레벨 컨텍스트(메인메뉴/튜토리얼/대기실/스테이지)에 맞는 모드로 되돌린다.
	RestoreDefaultInputMode();
}

void ADRPlayerController::Client_SlidePuzzleBusy_Implementation(ADRS2SlidePuzzle* Puzzle, APlayerState* Occupant)
{
	if (!IsLocalController() || !Puzzle) return;

	UE_LOG(LogDR, Log, TEXT("[S2Puzzle] 8퍼즐이 사용 중이다 (조작자: %s)"), *GetNameSafe(Occupant));

	OnSlidePuzzleScreenBusy(Occupant);
}

// ===== 8퍼즐 조작 요청 (2026-09-20) =====
// 넷 다 액터에 위임만 한다. 점유자 검사와 인접 검사는 ADRS2SlidePuzzle 안에서 하고,
// 거절은 조용히 무시된다 (보드가 안 바뀌므로 복제도 없다 - 위젯은 확정 타임아웃으로 잠금을 푼다).

void ADRPlayerController::Server_SlidePuzzleMove_Implementation(ADRS2SlidePuzzle* Puzzle, int32 TileId)
{
	if (!IsValid(Puzzle)) return;

	Puzzle->TryMove(this, TileId);
}

void ADRPlayerController::Server_SlidePuzzleUndo_Implementation(ADRS2SlidePuzzle* Puzzle)
{
	if (!IsValid(Puzzle)) return;

	Puzzle->TryUndo(this);
}

void ADRPlayerController::Server_SlidePuzzleReset_Implementation(ADRS2SlidePuzzle* Puzzle)
{
	if (!IsValid(Puzzle)) return;

	Puzzle->TryReset(this);
}

void ADRPlayerController::Server_SlidePuzzleRelease_Implementation(ADRS2SlidePuzzle* Puzzle)
{
	if (!IsValid(Puzzle)) return;

	Puzzle->ReleaseControl(this);
}
void ADRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// DRInputComponent로 캐스팅
	UDRInputComponent* DRInputComponent = CastChecked<UDRInputComponent>(InputComponent);
	// 기본 입력 액션 바인딩
	DRInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Move);
	DRInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Look);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ADRPlayerController::StartJump);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADRPlayerController::StopJump);
	// ★Started 여야 한다. Triggered 는 키를 누르고 있는 동안 **매 프레임** 발화해
	//   레버가 연속 토글되고 금고에 같은 숫자가 반복 입력된다 (2026-08-27 수정).
	//   상호작용은 전부 "한 번 누르면 한 번" 의미이므로 다른 단발 입력들과 동일하게 맞춘다.
	DRInputComponent->BindAction(InteractAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleInteract);
	DRInputComponent->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleSpectateNext);
	DRInputComponent->BindAction(SpectatePreviousAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleSpectatePrevious);
	DRInputComponent->BindAction(ToggleSettingsAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleToggleSettings);
	// Tab Hold: 누름/뗌 두 이벤트 모두 바인딩. Hold Trigger 없이 Started/Completed로 Hold 동작 구현.
	DRInputComponent->BindAction(CharacterInfoAction, ETriggerEvent::Started,   this, &ADRPlayerController::HandleCharacterInfoPressed);
	DRInputComponent->BindAction(CharacterInfoAction, ETriggerEvent::Completed, this, &ADRPlayerController::HandleCharacterInfoReleased);
	// 어빌리티 입력 바인딩 (InputConfig 사용)
	DRInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void ADRPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	// 로컬 컨트롤러만 처리
	if (!IsLocalController()) return;
// 레벨 진입 시 공통 초기화 수행
	OnLevelEntered();
}

void ADRPlayerController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	// 로컬 컨트롤러만 처리
	if (!IsLocalController()) return;
// SeamlessTravel 후 레벨 진입 시 공통 초기화 수행
	OnLevelEntered();
}

void ADRPlayerController::OnLevelEntered()
{
	// 레벨 컨텍스트 캐시 재판별 (ReceivedPlayer/PostSeamlessTravel에서 호출되므로 레벨 이동마다 갱신됨)
	CachedLevelContext = DetermineLevelContext();

	// 현재 레벨 확인
	UWorld* World = GetWorld();
	if (!World) return;

	// 게임오버 UI가 남아있으면 제거
	if (CurrentResultWidget)
	{
		CurrentResultWidget->RemoveFromParent();
		CurrentResultWidget = nullptr;
	}

	// 대기실 위젯 정리 (레벨 이동 시 무효화된 포인터 정리)
	DestroyWaitingRoomUI();

	// 설정 메뉴 상태 초기화 (레벨 이동 시)
	if (bIsSettingsMenuOpen)
	{
		CloseSettingsMenu();
	}
	bIsSettingsMenuOpen = false;

	// 업그레이드 화면 상태 초기화 (레벨 이동 시 위젯이 남지 않게)
	if (bIsUpgradeScreenOpen)
	{
		OnUpgradeScreenClosed();
		bIsUpgradeScreenOpen = false;
	}

	// 옷장 화면도 같은 처리 (위젯은 C++ 이 만들었으므로 C++ 이 지운다)
	if (bIsWardrobeScreenOpen)
	{
		OnWardrobeScreenClosed();
		if (WardrobeScreenWidget)
		{
			WardrobeScreenWidget->RemoveFromParent();
			WardrobeScreenWidget = nullptr;
		}
		bIsWardrobeScreenOpen = false;
	}

	// 스테이지 선택 화면 — ★출발할 때 열린 채로 이동하므로 도착한 레벨에서 반드시 여기서 정리된다★
	if (bIsStageSelectScreenOpen)
	{
		OnStageSelectScreenClosed();
		if (StageSelectScreenWidget)
		{
			StageSelectScreenWidget->RemoveFromParent();
			StageSelectScreenWidget = nullptr;
		}
		bIsStageSelectScreenOpen = false;
	}

	// 관전 상태 초기화 (델리게이트 정리 포함)
	if (CurrentSpectatedCharacter.IsValid())
	{
		if (ADRCharacterBase* OldTarget = Cast<ADRCharacterBase>(CurrentSpectatedCharacter.Get()))
		{
			OldTarget->OnDeathDelegate.RemoveDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
		}
	}
	bIsSpectating = false;
	CurrentSpectatedCharacter = nullptr;
	CurrentSpectatedPlayerIndex = 0;

	// 옷 보고는 ★로비 분기보다 앞★ 에서 한다 (Plan(옷장) M3) — 대기실이면 아래에서 일찍 return 하는데,
	// 리슨 서버 호스트는 OnRep_PlayerState 도 안 불려서 그러면 FreeRoam 에서 남에게 옷이 안 보인다.
	// (클라이언트는 OnRep_PlayerState 에서도 보고한다. 대기실 디스플레이 캐릭터도 이 목록을 입는다)
	ReportCosmeticLoadout();

	// 장착 칩 보고도 같은 이유로 ★로비 분기보다 앞★ 에서 한다.
	// 대기실이면 아래에서 일찍 return 하는데, 리슨 서버 호스트는 OnRep_PlayerState 가 안 불리고,
	// 저장된 클래스가 기본값(정원로봇)과 같으면 SetSelectedPlayerClass 도 일찍 return 해 재보고가 없다.
	// 그러면 정원로봇만 칩 기기를 열고 닫기 전까지 칩이 0개(HUD 모듈 칩 미적용 → HUD 숨김)였다.
	ReportUpgradeLoadout();

	// 대기실 감지 → ViewTarget 복원 차단
	ADRLobbyGameState* LGS = World->GetGameState<ADRLobbyGameState>();

	if (IsInLobby())
	{
		if (!LGS)
		{
			// GameState 아직 복제 안됨 → 대기실로 가정 (서버 RPC가 이후 교정)
			bIsInWaitingRoom = true;
			bAutoManageActiveCameraTarget = false; // Pawn 수신 시 ViewTarget 자동 설정 차단
			RestoreDefaultInputMode();
			// WaitingRoom UI는 ClientSetWaitingRoomView RPC에서 생성
			return;
		}

		if (LGS->GetLobbyState() == ELobbyState::WaitingRoom
			|| LGS->GetLobbyState() == ELobbyState::Transitioning)
		{
			bIsInWaitingRoom = true;
			bAutoManageActiveCameraTarget = false; // Pawn 수신 시 ViewTarget 자동 설정 차단
			RestoreDefaultInputMode();
			CreateWaitingRoomUI();
			return;  // ViewTarget은 ClientSetWaitingRoomView RPC가 설정
		}
	}

	bIsInWaitingRoom = false;

	// ViewTarget 즉시 복원 (잘못된 타겟 참조 방지)
	if (APawn* MyPawn = GetPawn())
	{
		SetViewTarget(MyPawn);
	}
	else
	{
		// Pawn이 아직 없으면 임시로 자기 자신을 보다가 OnPossessedPawnChanged 이벤트에서 복원
		SetViewTarget(this);
		bPendingViewTargetRestore = true;
	}

	// 레벨에 맞는 기본 입력 모드로 복원
	RestoreDefaultInputMode();

	// BGM은 레벨에 배치된 DRBGMActor가 담당
}

void ADRPlayerController::HandleToggleSettings()
{
	// 스테이지 선택 화면은 위젯이 ESC 를 받는다(선택 취소 → 닫기). 포커스를 잃었을 때만 여기로 온다.
	if (bIsStageSelectScreenOpen)
	{
		CloseStageSelectScreen();
		return;
	}
	// 옷장/업그레이드 화면이 열려 있으면 ESC 로 그 화면을 먼저 닫는다.
	// (옷장은 위젯의 NativeOnKeyDown 도 ESC 를 받지만, 포커스를 잃은 상태를 대비해 여기도 둔다)
	if (bIsWardrobeScreenOpen)
	{
		CloseWardrobeScreen();
		return;
	}
	if (bIsUpgradeScreenOpen)
	{
		CloseUpgradeScreen();
		return;
	}

	ToggleSettingsMenu();
}

bool ADRPlayerController::CanShowCharacterInfo() const
{
	// 로컬 컨트롤러만 UI 토글
	if (!IsLocalController()) return false;

	// 게임 레벨/튜토리얼/로비에서만 허용 (메인메뉴 차단)
	if (!IsInGameLevel() && !IsInTutorial() && !IsInLobby()) return false;

	// 설정창 열려있으면 차단
	if (bIsSettingsMenuOpen) return false;

	return true;
}

void ADRPlayerController::HandleCharacterInfoPressed()
{
	if (!CanShowCharacterInfo()) return;

	ADRHUD* DRHUD = GetHUD<ADRHUD>();
	if (DRHUD == nullptr) return;

	// 입력 모드는 변경하지 않음 — GameOnly 유지로 Tab Hold 중에도 이동/조작 가능.
	// 위젯 자체도 IsFocusable=false로 두어 Tab 키 이벤트가 컨트롤러로 그대로 흐르게 함.
	// CachedSelectedClass는 서버에서만 갱신되는 비복제 캐시이므로 클라이언트 UI 소스로 쓸 수 없음.
	// 복제되는 PlayerState 값을 우선 사용하고, PlayerState 미도착 시에만 캐시로 폴백.
	EPlayerCharacterClass SelectedClass = GetCachedSelectedClass();
	if (const ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>())
	{
		SelectedClass = DRPS->GetSelectedPlayerClass();
	}
	DRHUD->ShowCharacterInfo(SelectedClass);
	bIsCharacterInfoVisible = true;

	// 튜토리얼 중에는 매니저에 보고
	if (IsInTutorial())
	{
		ServerReportTutorialCharacterInfoOpened();
	}
}

void ADRPlayerController::HandleCharacterInfoReleased()
{
	if (!bIsCharacterInfoVisible) return;

	if (ADRHUD* DRHUD = GetHUD<ADRHUD>())
	{
		DRHUD->HideCharacterInfo();
	}
	bIsCharacterInfoVisible = false;
}

void ADRPlayerController::ToggleFirstPersonMeshAndHUDVisibility()
{
	if (!IsLocalController()) return;

	bool bNewVisible = true;
	bool bFoundVisibilitySource = false;

	if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetPawn()))
	{
		if (DRCharacter->FirstPersonMesh)
		{
			bNewVisible = !DRCharacter->FirstPersonMesh->IsVisible();
			bFoundVisibilitySource = true;
			DRCharacter->FirstPersonMesh->SetVisibility(bNewVisible, false);
		}
	}

	if (AHUD* BaseHUD = GetHUD())
	{
		if (!bFoundVisibilitySource)
		{
			bNewVisible = !BaseHUD->bShowHUD;
		}
		BaseHUD->bShowHUD = bNewVisible;
	}

	if (ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD()))
	{
		if (UDRUserWidget* OverlayWidget = DRHUD->GetOverlayWidget())
		{
			OverlayWidget->SetVisibility(bNewVisible ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		}
	}
}

void ADRPlayerController::HandleSpectateNext()
{
	if (bIsSpectating)
	{
		SpectateNextPlayer();
	}
}

void ADRPlayerController::HandleSpectatePrevious()
{
	if (bIsSpectating)
	{
		SpectatePreviousPlayer();
	}
}

void ADRPlayerController::SetSpectateTarget(ACharacter* NewTarget)
{
	// ★사망 델리게이트는 로컬 컨트롤러에서 묶는다★ — 관전 여부(bIsSpectating)는 로컬에만 있고,
	// 사망 멀티캐스트가 모든 머신에서 OnDeathDelegate 를 방송하므로 원격 클라도 직접 받는다.
	// (서버 쪽 PC 객체에 묶으면 bIsSpectating 이 늘 false 라 원격 클라는 다음 대상으로 넘어가지 못했다)
	if (IsLocalController())
	{
		BindSpectatedDeath(NewTarget);
	}
	CurrentSpectatedCharacter = NewTarget;

	// 클라이언트: 서버에도 알려 서버 쪽 ViewTarget(관련성 판정 기준)을 맞추고, 로컬 ViewTarget 은 바로 바꾼다
	if (!HasAuthority())
	{
		ServerSetSpectateTarget(NewTarget);
		if (NewTarget)
		{
			SetViewTarget(NewTarget);
		}
		return;
	}

	if (NewTarget)
	{
		SetViewTarget(NewTarget);

		// HUD 를 관전 대상에게 묶는다
		ClientUpdateSpectatorUI(NewTarget);
	}
}

void ADRPlayerController::BindSpectatedDeath(ACharacter* NewTarget)
{
	UnbindSpectatedDeath();

	if (ADRCharacterBase* DRTarget = Cast<ADRCharacterBase>(NewTarget))
	{
		DRTarget->OnDeathDelegate.AddUniqueDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
	}
}

void ADRPlayerController::UnbindSpectatedDeath()
{
	if (ADRCharacterBase* OldTarget = Cast<ADRCharacterBase>(CurrentSpectatedCharacter.Get()))
	{
		OldTarget->OnDeathDelegate.RemoveDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
	}
}

void ADRPlayerController::ServerSetSpectateTarget_Implementation(ACharacter* NewTarget)
{
	// 서버에서 SetSpectateTarget 실행
	SetSpectateTarget(NewTarget);
}

void ADRPlayerController::ClientUpdateSpectatorUI_Implementation(ACharacter* SpectatedTarget)
{
	// 클라이언트에서만 실행
	if (!IsLocalController()) return;

	UpdateSpectatorUI(SpectatedTarget);
}

void ADRPlayerController::UpdateSpectatorUI(ACharacter* SpectatedTarget)
{
	if (!SpectatedTarget) return;

	// HUD 가져오기
	ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD());
	if (!DRHUD) return;

	// 관전 대상의 PlayerState 가져오기
	ADRPlayerState* SpectatedPS = SpectatedTarget->GetPlayerState<ADRPlayerState>();
	if (!SpectatedPS) return;

	// 관전 대상의 GAS 컴포넌트들 가져오기
	UAbilitySystemComponent* SpectatedASC = SpectatedPS->GetAbilitySystemComponent();
	UAttributeSet* SpectatedAS = SpectatedPS->GetAttributeSet();

	if (!SpectatedASC || !SpectatedAS) return;

	// HUD 를 관전 대상의 PlayerState 에 다시 묶는다
	DRHUD->RebindOverlay(this, SpectatedPS, SpectatedASC, SpectatedAS);
}

void ADRPlayerController::OnSpectatedPlayerDied(AActor* DeadActor)
{
	if (!bIsSpectating) return;

	// 약간의 딜레이 후 다음 플레이어로 전환
	FTimerHandle SwitchTimerHandle;
	GetWorld()->GetTimerManager().SetTimer(
		SwitchTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{
			if (bIsSpectating)
			{
				SpectateNextPlayer();
			}
		}),
		1.0f,
		false
	);
}

void ADRPlayerController::Move(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// 입력 블록 상태 확인
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed)) return;

	// 2D 입력을 월드 좌표계로 변환
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// 폰에 이동 입력 전달
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		// 탑승 중 이동 차단 (시점 회전은 Look에서 계속 허용, Plan3 §5.2)
		if (ADRCharacter* DRChar = Cast<ADRCharacter>(ControlledPawn))
		{
			if (DRChar->IsMounted()) return;
		}

		// 지속 돌진 중 후진 입력 = 브레이크 요청 (Plan3 §8.2 — 서버가 재검증 후 GA에 이벤트 전달)
		if (ADRRobotVacuumCharacter* Vacuum = Cast<ADRRobotVacuumCharacter>(ControlledPawn))
		{
			// ★Reliable RPC 를 매 프레임 보내지 않는다★ — 서버가 돌진을 끝내 복제될 때까지 S 를 누르고 있으면
			// 프레임마다 쌓였다. 짧은 간격으로 한 번만 보낸다 (응답이 늦어 유실되어도 다음 간격에 다시 간다)
			if (Vacuum->bSustainedDash && InputAxisVector.Y < -0.5f)
			{
				const double Now = GetWorld()->GetTimeSeconds();
				if (Now - LastDashBrakeRequestTime >= DashBrakeRequestInterval)
				{
					LastDashBrakeRequestTime = Now;
					ServerRequestDashBrake();
				}
			}
		}

		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}

void ADRPlayerController::Look(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// 마우스 시선 처리
	const FVector2D Axis = InputActionValue.Get<FVector2D>();

	// Manager에서 감도 가져오기
	// 감도는 캐시 사용 (설정 변경 시 OnMouseSensitivityChanged 델리게이트로 갱신)
	const float Sensitivity = CachedMouseSensitivity;

	// 감도 적용
	AddYawInput(Axis.X * Sensitivity);
	AddPitchInput(Axis.Y * Sensitivity);
}

void ADRPlayerController::StartJump(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// 탑승 중이면 점프키 = 하차 (Plan3 §5.3)
	if (ADRCharacter* DRChar = GetPawn<ADRCharacter>())
	{
		if (DRChar->IsMounted())
		{
			ServerRequestDismount();
			return;
		}

		// 열차 좌석에 앉아 있으면 점프키 = 하차 (Plan6 §14.6.5)
		if (DRChar->IsSeatedOnTrain())
		{
			ServerRequestTrainDeboard();
			return;
		}
	}

	// 점프 시작
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>()))
	{
		ControlledCharacter->Jump();
	}
}

void ADRPlayerController::StopJump(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// 점프 종료
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>()))
	{
		ControlledCharacter->StopJumping();
	}
}

void ADRPlayerController::HandleInteract()
{
	if (bIsSpectating) return;

	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// 이미 탑승 중이면 F키 무시 (하차는 점프키, Plan3 §5.1)
	if (DRCharacter->IsMounted()) return;

	// 부품을 들고 있지 않을 때만 부품 획득 시도
	if (!DRCharacter->IsCarryingPart() && CurrentDetectedPart)
	{
		ServerRequestInteract(CurrentDetectedPart);
		return;
	}

	// 부품을 들고 있고 클렌저 사이트 오버랩 중이면 설치
	if (DRCharacter->IsCarryingPart())
	{
		// 클렌저 사이트 범위 안이면 설치
		if (CurrentOverlappedSite)
		{
			ServerRequestInteract(CurrentOverlappedSite);
			return;
		}
		// 클렌저 사이트 범위 밖이면 떨어트리기
		else
		{
			ServerRequestDropPart();
			return;
		}
	}

	// 부품 미소지 && 탑승 후보 감지 → 탑승 요청 (Plan3 §5.1)
	if (!DRCharacter->IsCarryingPart() && CurrentDetectedMount)
	{
		ServerRequestInteract(CurrentDetectedMount);
		return;
	}

	// 열차 좌석 탑승 (Plan6 §5.5)
	if (CurrentDetectedCar)
	{
		ServerRequestInteract(CurrentDetectedCar);
		return;
	}

	// 스테이지2 상호작용 프롭 (레버/버튼/단말) — Plan6 §5.5-b
	if (CurrentDetectedProp)
	{
		UE_LOG(LogDR, Log, TEXT("[S2Prop] F -> %s 조작 요청 (클라)"), *CurrentDetectedProp->GetName());

		ServerRequestInteract(CurrentDetectedProp);
		return;
	}

	OnInteractPressed.Broadcast();
}

void ADRPlayerController::ToggleSettingsMenu()
{
	if (bIsSettingsMenuOpen)
	{
		CloseSettingsMenu();
	}
	else
	{
		OpenSettingsMenu();
	}
}

void ADRPlayerController::OpenSettingsMenu()
{
	// 로컬 컨트롤러에서만 실행
	if (!IsLocalController()) return;

	// 이미 열려있으면 무시
	if (bIsSettingsMenuOpen) return;

	bIsSettingsMenuOpen = true;

	// 입력 모드 변경
	SetInputMode(FInputModeUIOnly());
	SetShowMouseCursor(true);

	// Blueprint에서 위젯 생성
	OnSettingsMenuOpened();
}

void ADRPlayerController::CloseSettingsMenu()
{
	// 로컬 컨트롤러에서만 실행
	if (!IsLocalController()) return;

	// 이미 닫혀있으면 무시
	if (!bIsSettingsMenuOpen) return;

	// Blueprint에서 위젯 제거
	OnSettingsMenuClosed();

	bIsSettingsMenuOpen = false;
	RestoreDefaultInputMode();
}

void ADRPlayerController::ClientCloseSettingsMenu_Implementation()
{
	CloseSettingsMenu();
}

// ========================= 업그레이드 칩 (Plan2.md 7.1) =========================

void ADRPlayerController::ReportUpgradeLoadout()
{
	if (!IsLocalController()) return;

	const ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
	if (!DRPS) return;

	const UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	if (!GI) return;

	const EPlayerCharacterClass SelectedClass = DRPS->GetSelectedPlayerClass();
	ServerReportUpgradeLoadout(SelectedClass, GI->GetEquippedChips(SelectedClass));
}

void ADRPlayerController::ServerReportUpgradeLoadout_Implementation(EPlayerCharacterClass ForClass,
	const TArray<FName>& Chips)
{
	ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
	if (!DRPS) return;

	// 경합/스푸핑 방어: 서버가 확정한 선택 클래스와 다른 보고는 폐기한다
	if (ForClass != DRPS->GetSelectedPlayerClass()) return;

	const UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	UDRChipCatalog* Catalog = GI ? GI->GetChipCatalog() : nullptr;
	if (!Catalog)
	{
		// 카탈로그가 없으면 검증할 수 없다 — 아무 것도 싣지 않는다(잘못된 강화 방지)
		DRPS->SetEquippedChips(TArray<FName>());
		return;
	}

	TArray<FName> Sanitized = Chips;
	const bool bModified = Catalog->SanitizeLoadout(Sanitized, ForClass, GI->GetMaxSlotCount());

	if (bModified)
	{
		UE_LOG(LogDR, Warning, TEXT("[Upgrade] %s 의 장착 목록을 정화했습니다 (%d → %d)."),
			*DRPS->GetPlayerName(), Chips.Num(), Sanitized.Num());
	}

	DRPS->SetEquippedChips(Sanitized);
}

// ========================= 코스메틱 (Plan(옷장) M3 · D-26) =========================

void ADRPlayerController::ReportCosmeticLoadout()
{
	if (!IsLocalController()) return;

	const ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
	if (!DRPS) return;

	const UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	if (!GI) return;

	const EPlayerCharacterClass SelectedClass = DRPS->GetSelectedPlayerClass();
	ServerReportCosmeticLoadout(SelectedClass, GI->GetEquippedSkins(SelectedClass));
}

void ADRPlayerController::ServerReportCosmeticLoadout_Implementation(EPlayerCharacterClass ForClass,
	const TArray<FName>& SkinIds)
{
	ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
	if (!DRPS) return;

	// 경합/스푸핑 방어: 서버가 확정한 선택 클래스와 다른 보고는 폐기한다 (칩과 같은 가드)
	if (ForClass != DRPS->GetSelectedPlayerClass()) return;

	const UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	const UDRCosmeticCatalog* Catalog = GI ? GI->GetCosmeticCatalog() : nullptr;
	if (!Catalog)
	{
		// 검증할 수 없으면 기본 외형 — 없는 에셋을 가리키는 Id 로 다른 클라가 헤매지 않게 한다
		DRPS->SetEquippedSkinIds(TArray<FName>());
		return;
	}

	// 길이를 카테고리 수로 맞추고, 카탈로그에 없는 Id · 다른 로봇 · 다른 부위를 None 으로.
	// ★해금 여부는 보지 않는다★ — 협동 PvE 라 잠긴 옷을 보고해도 남에게 피해가 없고, 서버는 남의 해금 상태를 모른다.
	TArray<FName> Sanitized = SkinIds;
	if (Catalog->SanitizeLoadout(Sanitized, ForClass))
	{
		UE_LOG(LogDR, Warning, TEXT("[Cosmetic] %s 의 옷 목록을 정화했습니다."), *DRPS->GetPlayerName());
	}

	DRPS->SetEquippedSkinIds(Sanitized);
}

void ADRPlayerController::Client_GrantStageReward_Implementation(const FDRStageRewardReport& Report)
{
	UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	if (!GI) return;

	// 지갑 반영을 먼저 끝낸 뒤 결과창이 그 결과를 읽는다
	LastStageRewardResult = GI->ApplyStageReward(Report);

	UE_LOG(LogDR, Log, TEXT("[Progression] 스테이지 보상 반영: +%d (보유 %d → %d)"),
		LastStageRewardResult.TotalGained,
		LastStageRewardResult.CurrencyBefore,
		LastStageRewardResult.CurrencyAfter);
}

// ========================= 로비 업그레이드 화면 =========================

void ADRPlayerController::OpenUpgradeScreen()
{
	if (!IsLocalController()) return;
	if (bIsUpgradeScreenOpen) return;

	// 로비에서만 슬롯 해금/칩 교체가 가능하다.
	// 화면을 닫을 때 서버로 재보고되고 그 즉시 스탯 GE 가 재적용되므로,
	// 이 가드가 없으면 스테이지 도중 칩 교체가 실제로 반영돼 버린다.
	if (!IsInLobby())
	{
		UE_LOG(LogDR, Warning, TEXT("[Upgrade] 로비가 아닌 곳에서 업그레이드 화면 열기를 시도했습니다 — 무시합니다."));
		return;
	}

	// 설정창이 열려 있으면 먼저 닫는다 (입력 모드 충돌 방지)
	if (bIsSettingsMenuOpen)
	{
		CloseSettingsMenu();
	}

	bIsUpgradeScreenOpen = true;

	SetInputMode(FInputModeUIOnly());
	SetShowMouseCursor(true);

	// 블루프린트에서 위젯 생성 (설정 메뉴와 같은 구조 — OpenSettingsMenu 참조)
	OnUpgradeScreenOpened();
}

void ADRPlayerController::DRUnlockUpgrade()
{
#if !UE_BUILD_SHIPPING   // 로컬 세이브를 바꾸는 치트 — 쉬핑에서는 아무 일도 하지 않는다
	UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	if (!GI)
	{
		UE_LOG(LogDR, Warning, TEXT("[Cheat] UDRGameInstance 를 찾지 못했습니다."));
		return;
	}

	GI->UnlockUpgradeSystem();
	UE_LOG(LogDR, Warning, TEXT("[Cheat] 업그레이드 시스템 해금됨 (현재 재화 %d)"), GI->GetCurrency());
#endif
}

void ADRPlayerController::DRLockUpgrade()
{
#if !UE_BUILD_SHIPPING   // 로컬 세이브를 바꾸는 치트 — 쉬핑에서는 아무 일도 하지 않는다
	if (UDRGameInstance* GI = GetGameInstance<UDRGameInstance>())
	{
		GI->DebugSetUpgradeSystemUnlocked(false);
	}
#endif
}

void ADRPlayerController::DRUnlockSlot(int32 Count)
{
#if !UE_BUILD_SHIPPING   // 로컬 세이브를 바꾸는 치트 — 쉬핑에서는 아무 일도 하지 않는다
	UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	if (!GI)
	{
		UE_LOG(LogDR, Warning, TEXT("[Cheat] UDRGameInstance 를 찾지 못했습니다."));
		return;
	}

	// 시스템이 잠겨 있으면 CanUnlockSlot 이 SystemLocked 로 막는다 — 먼저 열어 준다
	GI->UnlockUpgradeSystem();

	const EPlayerCharacterClass Class = GetViewedUpgradeClass();

	for (int32 Index = 0; Index < FMath::Max(1, Count); ++Index)
	{
		// 비용이 모자라면 그만큼 채워 넣고 다시 시도한다 (치트니까 지갑은 신경 쓰지 않는다)
		if (GI->CanUnlockSlot(Class) == EDRUpgradeResult::NotEnoughCurrency)
		{
			GI->AddCurrency(FMath::Max(1000, GI->GetNextSlotUnlockCost(Class)));
		}

		const EDRUpgradeResult Result = GI->UnlockSlot(Class);
		if (Result != EDRUpgradeResult::Success)
		{
			UE_LOG(LogDR, Warning, TEXT("[Cheat] 슬롯 해금 중단 (%d번째): %s"),
				Index + 1, *UDRUpgradeUILibrary::GetUpgradeResultText(Result).ToString());
			break;
		}
	}

	DRDumpUpgrade();
#endif
}

void ADRPlayerController::DRDumpUpgrade()
{
	UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	if (!GI)
	{
		UE_LOG(LogDR, Warning, TEXT("[Cheat] UDRGameInstance 를 찾지 못했습니다."));
		return;
	}

	const EPlayerCharacterClass Class = GetViewedUpgradeClass();

	UE_LOG(LogDR, Warning,
		TEXT("[Cheat] 진행도: 시스템=%s / 재화=%d / 클래스=%d / 해금슬롯=%d/%d / 빈칸=%d"),
		GI->IsUpgradeSystemUnlocked() ? TEXT("해금") : TEXT("잠김"),
		GI->GetCurrency(), static_cast<int32>(Class),
		GI->GetUnlockedSlotCount(Class), GI->GetMaxSlotCount(),
		GI->GetFreeSlotCount(Class));
}

EPlayerCharacterClass ADRPlayerController::GetViewedUpgradeClass() const
{
	// 업그레이드 화면과 같은 기준으로 클래스를 고른다(UDRUpgradeScreenWidget::GetViewedClass 와 동일)
	if (const ADRPlayerState* PS = GetPlayerState<ADRPlayerState>())
	{
		return PS->GetSelectedPlayerClass();
	}
	return EPlayerCharacterClass::Gardener;
}

void ADRPlayerController::DRAddCurrency(int32 Amount)
{
#if !UE_BUILD_SHIPPING   // 로컬 세이브를 바꾸는 치트 — 쉬핑에서는 아무 일도 하지 않는다
	UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	if (!GI)
	{
		UE_LOG(LogDR, Warning, TEXT("[Cheat] UDRGameInstance 를 찾지 못했습니다."));
		return;
	}

	GI->AddCurrency(Amount);
	UE_LOG(LogDR, Warning, TEXT("[Cheat] 재화 %d 지급 → 현재 %d"), Amount, GI->GetCurrency());
#endif
}

void ADRPlayerController::DRResetDefaultChips()
{
#if !UE_BUILD_SHIPPING   // 로컬 세이브를 바꾸는 치트 — 쉬핑에서는 아무 일도 하지 않는다
	UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	if (!GI)
	{
		UE_LOG(LogDR, Warning, TEXT("[Cheat] UDRGameInstance 를 찾지 못했습니다."));
		return;
	}

	GI->DebugResetDefaultLoadout();

	// 세이브만 바꿔서는 이미 스폰된 캐릭터에 반영되지 않는다 — 서버에 다시 보고해야
	// PlayerState 의 EquippedChips 가 갱신되고 HUD/스탯이 따라온다.
	ReportUpgradeLoadout();

	UE_LOG(LogDR, Warning, TEXT("[Cheat] 기본 장착 칩을 다시 적용하고 서버에 재보고했습니다."));
#endif
}

void ADRPlayerController::CloseUpgradeScreen()
{
	if (!IsLocalController()) return;
	if (!bIsUpgradeScreenOpen) return;

	// 블루프린트에서 위젯 제거 (설정 메뉴와 같은 구조 — CloseSettingsMenu 참조)
	OnUpgradeScreenClosed();

	bIsUpgradeScreenOpen = false;
	RestoreDefaultInputMode();

	// 변경된 장착 상태를 서버에 반영
	ReportUpgradeLoadout();
}

// ========================= 로비 옷장 화면 (Plan(옷장)) =========================

void ADRPlayerController::OpenWardrobeScreen()
{
	if (!IsLocalController()) return;
	if (bIsWardrobeScreenOpen) return;

	// 옷장은 로비 전용이다. 스테이지 도중 외형을 바꿔 끼우는 경로를 만들지 않는다.
	// (OpenUpgradeScreen 과 같은 가드)
	if (!IsInLobby())
	{
		UE_LOG(LogDR, Warning, TEXT("[Cosmetic] 로비가 아닌 곳에서 옷장 열기를 시도했습니다 — 무시합니다."));
		return;
	}

	// 다른 전체화면 UI 와 입력 모드가 겹치지 않게 먼저 닫는다
	if (bIsSettingsMenuOpen)
	{
		CloseSettingsMenu();
	}
	if (bIsUpgradeScreenOpen)
	{
		CloseUpgradeScreen();
	}

	bIsWardrobeScreenOpen = true;

	SetInputMode(FInputModeUIOnly());
	SetShowMouseCursor(true);

	// ★위젯은 C++ 이 만든다★ — BP 의 OnWardrobeScreenOpened 는 연출용이다 (거기서 또 만들면 두 장이 뜬다)
	if (!WardrobeScreenWidget && WardrobeScreenWidgetClass)
	{
		WardrobeScreenWidget = CreateWidget<UDRWardrobeScreenWidget>(this, WardrobeScreenWidgetClass);
		if (WardrobeScreenWidget)
		{
			// 키보드 포커스는 화면의 NativeConstruct 가 직접 잡는다 (ESC / C 키)
			WardrobeScreenWidget->AddToViewport();
		}
	}
	else if (!WardrobeScreenWidgetClass)
	{
		UE_LOG(LogDR, Warning, TEXT("[Cosmetic] WardrobeScreenWidgetClass 가 지정되지 않았습니다 (BP_DRPlayerController)."));
	}

	OnWardrobeScreenOpened();
}

void ADRPlayerController::CloseWardrobeScreen()
{
	if (!IsLocalController()) return;
	if (!bIsWardrobeScreenOpen) return;

	OnWardrobeScreenClosed();

	if (WardrobeScreenWidget)
	{
		WardrobeScreenWidget->RemoveFromParent();
		WardrobeScreenWidget = nullptr;
	}

	bIsWardrobeScreenOpen = false;
	RestoreDefaultInputMode();

	// 고른 옷을 서버에 보고 → PlayerState 복제 → 내 캐릭터와 남의 화면에 입혀진다 (Plan(옷장) M3).
	// 화면이 열려 있는 동안의 변경은 프리뷰에만 보이고, 닫는 순간 한 번에 반영된다.
	ReportCosmeticLoadout();
}

// ========================= 스테이지 선택 화면 =========================

bool ADRPlayerController::CanUseStageSelectScreen() const
{
	if (!StageSelectScreenWidgetClass) return false;

	const UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	return GI && GI->ProgressionConfig && GI->ProgressionConfig->StageSelectList.Num() > 0;
}

void ADRPlayerController::OpenStageSelectScreen()
{
	if (!IsLocalController()) return;
	if (bIsStageSelectScreenOpen) return;

	// 리슨 서버의 호스트만 — 클라이언트는 스테이지를 고를 수 없다
	if (!HasAuthority())
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] 호스트가 아닌 플레이어가 스테이지 선택을 열려고 했습니다 — 무시합니다."));
		return;
	}

	if (!IsInLobby())
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] 로비가 아닌 곳에서 스테이지 선택 열기를 시도했습니다 — 무시합니다."));
		return;
	}

	if (!StageSelectScreenWidgetClass)
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] StageSelectScreenWidgetClass 가 지정되지 않았습니다 (BP_DRPlayerController)."));
		return;
	}

	// 다른 전체화면 UI 와 입력 모드가 겹치지 않게 먼저 닫는다
	if (bIsSettingsMenuOpen)
	{
		CloseSettingsMenu();
	}
	if (bIsUpgradeScreenOpen)
	{
		CloseUpgradeScreen();
	}
	if (bIsWardrobeScreenOpen)
	{
		CloseWardrobeScreen();
	}

	bIsStageSelectScreenOpen = true;

	SetInputMode(FInputModeUIOnly());
	SetShowMouseCursor(true);

	// ★위젯은 C++ 이 만든다★ — 키보드 포커스는 화면의 NativeConstruct 가 직접 잡는다
	StageSelectScreenWidget = CreateWidget<UDRStageSelectScreenWidget>(this, StageSelectScreenWidgetClass);
	if (StageSelectScreenWidget)
	{
		// 전체화면 모달이라 HUD(0) · 퍼즐(10) 위, 결과창/캐릭터 정보(100) · 로딩(10000) 아래.
		// 0 이면 화면을 연 ★뒤에★ 만들어지는 HUD 위젯(대기실 → 자유이동 전환 직후의 크로스헤어 등)이 위에 덮인다.
		StageSelectScreenWidget->AddToViewport(50);
	}

	OnStageSelectScreenOpened();
}

void ADRPlayerController::CloseStageSelectScreen()
{
	if (!IsLocalController()) return;
	if (!bIsStageSelectScreenOpen) return;

	OnStageSelectScreenClosed();

	if (StageSelectScreenWidget)
	{
		StageSelectScreenWidget->RemoveFromParent();
		StageSelectScreenWidget = nullptr;
	}

	bIsStageSelectScreenOpen = false;
	RestoreDefaultInputMode();
}

bool ADRPlayerController::RequestStageTravel(FName StageId)
{
	if (!IsLocalController() || !HasAuthority())
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] 스테이지 이동은 호스트만 요청할 수 있습니다."));
		return false;
	}

	const UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	const FDRStageSelectDef* Def = (GI && GI->ProgressionConfig) ? GI->ProgressionConfig->FindStageSelectDef(StageId) : nullptr;
	if (!Def || Def->Map.IsNull())
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] '%s' 의 정의 또는 Map 이 없습니다 (DA_ProgressionConfig > StageSelectList)."),
			*StageId.ToString());
		return false;
	}

	// 화면을 거치지 않는 호출(BP · 치트)도 같은 규칙을 따른다
	if (GI->GetStageSelectState(StageId) == EDRStageSelectState::Locked)
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] '%s' 는 잠긴 스테이지입니다 — 이동하지 않습니다."), *StageId.ToString());
		return false;
	}

	ADRLobbyGameMode* LobbyGameMode = GetWorld() ? GetWorld()->GetAuthGameMode<ADRLobbyGameMode>() : nullptr;
	if (!LobbyGameMode)
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] 로비 게임모드가 아닙니다 — 이동하지 않습니다."));
		return false;
	}

	// "/Game/Maps/Stage1" 형태. ExecuteTravel 이 "?listen" 을 붙여 ServerTravel 한다.
	LobbyGameMode->TravelToStage(Def->Map.GetLongPackageName(), this);
	return true;
}

void ADRPlayerController::DROpenStageSelect()
{
	OpenStageSelectScreen();
}

void ADRPlayerController::DRStageState(const FString& StageId, const FString& State)
{
#if !UE_BUILD_SHIPPING   // 로컬 세이브를 바꾸는 치트 — 쉬핑에서는 아무 일도 하지 않는다
	UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	if (!GI) return;

	const FName Id = StageId.Equals(TEXT("all"), ESearchCase::IgnoreCase) ? NAME_None : FName(*StageId);

	if (State.Equals(TEXT("reset"), ESearchCase::IgnoreCase) || State.IsEmpty())
	{
		GI->DebugClearStageStateOverride(Id);
	}
	else if (Id.IsNone())
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] 'all' 은 reset 과만 쓸 수 있습니다."));
		return;
	}
	else if (State.Equals(TEXT("locked"), ESearchCase::IgnoreCase))
	{
		GI->DebugSetStageStateOverride(Id, EDRStageSelectState::Locked);
	}
	else if (State.Equals(TEXT("normal"), ESearchCase::IgnoreCase) || State.Equals(TEXT("available"), ESearchCase::IgnoreCase))
	{
		GI->DebugSetStageStateOverride(Id, EDRStageSelectState::Available);
	}
	else if (State.Equals(TEXT("cleared"), ESearchCase::IgnoreCase) || State.Equals(TEXT("clear"), ESearchCase::IgnoreCase))
	{
		GI->DebugSetStageStateOverride(Id, EDRStageSelectState::Cleared);
	}
	else
	{
		UE_LOG(LogDR, Warning, TEXT("[StageSelect] 사용법: DRStageState <StageId|all> <locked|normal|cleared|reset>"));
		return;
	}

	UE_LOG(LogDR, Log, TEXT("[StageSelect] %s → %s (디버그 덮어쓰기, 저장 안 함)"), *StageId, *State);
#endif
}

void ADRPlayerController::DRUnlockSkins()
{
#if !UE_BUILD_SHIPPING   // 로컬 세이브를 바꾸는 치트 — 쉬핑에서는 아무 일도 하지 않는다
	if (UDRGameInstance* GI = GetGameInstance<UDRGameInstance>())
	{
		GI->DebugUnlockAllSkins();
	}
#endif
}

void ADRPlayerController::DROpenWardrobe()
{
	OpenWardrobeScreen();
}

void ADRPlayerController::DRDumpCosmetic()
{
	const UDRGameInstance* GI = GetGameInstance<UDRGameInstance>();
	if (!GI)
	{
		UE_LOG(LogDR, Warning, TEXT("[Cheat] UDRGameInstance 를 찾지 못했습니다."));
		return;
	}

	const EPlayerCharacterClass Class = GetViewedUpgradeClass();
	const TArray<FName> Equipped = GI->GetEquippedSkins(Class);

	FString Line;
	for (int32 Index = 0; Index < Equipped.Num(); ++Index)
	{
		Line += FString::Printf(TEXT("%s=%s "),
			*StaticEnum<EDRCosmeticCategory>()->GetNameStringByValue(Index),
			*Equipped[Index].ToString());
	}

	UE_LOG(LogDR, Warning, TEXT("[Cheat] 코스메틱: 클래스=%d / 카탈로그=%s / %s"),
		static_cast<int32>(Class),
		GI->GetCosmeticCatalog() ? TEXT("지정됨") : TEXT("★미지정★"),
		*Line);
}

void ADRPlayerController::DRDie()
{
#if !UE_BUILD_SHIPPING
	ServerCheatDie();
#endif
}

void ADRPlayerController::DRRevive()
{
#if !UE_BUILD_SHIPPING
	ServerCheatRevive();
#endif
}

void ADRPlayerController::ServerCheatDie_Implementation()
{
#if !UE_BUILD_SHIPPING
	// Die() 는 오염 상태가 아니면 사망을 막는다 — 확인용이라 사망 처리만 바로 부른다
	ADRCharacterBase* CheatTarget = Cast<ADRCharacterBase>(GetPawn());
	if (!CheatTarget || ICombatInterface::Execute_IsDead(CheatTarget)) return;

	CheatTarget->MulticastHandleDeath(FVector::ZeroVector);
	UE_LOG(LogDR, Warning, TEXT("[Cheat] %s 사망 처리"), *CheatTarget->GetName());
#endif
}

void ADRPlayerController::ServerCheatRevive_Implementation()
{
#if !UE_BUILD_SHIPPING
	ADRGameModeBase* GM = GetWorld()->GetAuthGameMode<ADRGameModeBase>();
	if (!GM) return;

	// 시체가 남아 있으면 그 자리, 이미 정리됐으면 시작 지점에서 되살린다
	FTransform At = GetPawn() ? GetPawn()->GetActorTransform() : FTransform::Identity;
	if (!GetPawn())
	{
		if (const AActor* Start = GM->FindPlayerStart(this))
		{
			At = Start->GetActorTransform();
		}
	}

	if (GM->RevivePlayer(this, At, 1.0f, 1.0f))
	{
		UE_LOG(LogDR, Warning, TEXT("[Cheat] %s 부활"), *GetName());
	}
#endif
}

void ADRPlayerController::RestoreDefaultInputMode()
{
	if (!IsLocalController()) return;

	if (IsInMainMenu())
	{
		// 메인메뉴: UI 모드
		SetInputMode(FInputModeUIOnly());
		SetShowMouseCursor(true);
	}
	else if (IsInTutorial())
	{
		// 튜토리얼: 게임 모드
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
	else if (IsInLobby())
	{
		// 로비 상태에 따라 분기
		ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
		if (LGS && (LGS->GetLobbyState() == ELobbyState::WaitingRoom
				  || LGS->GetLobbyState() == ELobbyState::Transitioning))
		{
			// TEST: 대기실은 GameAndUI로 통일 (ClientSetWaitingRoomView와 일치)
			FInputModeGameAndUI InputMode;
			InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			InputMode.SetHideCursorDuringCapture(false);
			SetInputMode(InputMode);
			SetShowMouseCursor(true);
		}
		else
		{
			SetInputMode(FInputModeGameOnly());
			SetShowMouseCursor(false);
		}
	}
	else
	{
		// 스테이지: 게임 모드
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
}

EDRLevelContext ADRPlayerController::DetermineLevelContext() const
{
	UWorld* World = GetWorld();
	if (!World) return EDRLevelContext::Unknown;

	FString CurrentLevelName = World->GetMapName();
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	if (CurrentLevelName.Contains(TEXT("MainMenu"))) return EDRLevelContext::MainMenu;
	if (CurrentLevelName.Contains(TEXT("Lobby"))) return EDRLevelContext::Lobby;
	if (CurrentLevelName.Contains(TEXT("Tutorial"))) return EDRLevelContext::Tutorial;

	// 스테이지 레벨은 맵 이름 하드코딩("Stage1") 대신 GameState 타입으로 판별 (Stage2가 생겨도 동작)
	if (World->GetGameState<ADRStageGameState>()) return EDRLevelContext::GameLevel;

	return EDRLevelContext::Unknown;
}

EDRLevelContext ADRPlayerController::GetLevelContext() const
{
	// 초기 프레임에 GameState 복제가 늦어 판별에 실패(Unknown)했으면 재시도
	if (CachedLevelContext == EDRLevelContext::Unknown)
	{
		CachedLevelContext = DetermineLevelContext();
	}
	return CachedLevelContext;
}

bool ADRPlayerController::IsInMainMenu() const
{
	return GetLevelContext() == EDRLevelContext::MainMenu;
}

bool ADRPlayerController::IsInLobby() const
{
	return GetLevelContext() == EDRLevelContext::Lobby;
}

bool ADRPlayerController::IsInGameLevel() const
{
	return GetLevelContext() == EDRLevelContext::GameLevel;
}

bool ADRPlayerController::IsInTutorial() const
{
	return GetLevelContext() == EDRLevelContext::Tutorial;
}

void ADRPlayerController::ShowResultWidget(TSubclassOf<UUserWidget> WidgetClass)
{
	// 이미 결과 UI 가 떠 있거나 위젯 클래스가 없으면 무시
	if (CurrentResultWidget || !WidgetClass) return;

	CurrentResultWidget = CreateWidget<UUserWidget>(this, WidgetClass);
	if (CurrentResultWidget)
	{
		CurrentResultWidget->AddToViewport(100);
	}
}

void ADRPlayerController::Client_ShowGameOverUI_Implementation()
{
	ShowResultWidget(GameOverWidgetClass);
}

void ADRPlayerController::Client_ShowGameClearUI_Implementation()
{
	ShowResultWidget(GameClearWidgetClass);
}

void ADRPlayerController::Client_ShowTutorialClearUI_Implementation()
{
	ShowResultWidget(TutorialGameClearWidgetClass);
}

void ADRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	// 홀드 추적은 아래 차단 게이트보다 먼저 — Pressed 만 걸리고 Released 는 통과하는 경우에도
	// 집합이 어긋나지 않도록 (Released 쪽 Remove 도 동일하게 최상단에서 수행)
	if (InputTag.IsValid())
	{
		HeldInputTags.Add(InputTag);
	}

	if (bIsSpectating) return;

	// 입력 블록 확인 후 어빌리티 입력 처리
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed)) return;

	if (GetASC())
	{
		GetASC()->AbilityInputTagPressed(InputTag);
	}

	// 스킬 아이콘 UI 피드백 브로드캐스트 (로컬 컨트롤러만, 차단 중이면 발화 안 함)
	if (IsLocalController() && !IsAbilityInputBlocked(InputTag))
	{
		if (ADRHUD* HUD = Cast<ADRHUD>(GetHUD()))
		{
			if (UOverlayWidgetController* WC = HUD->GetOverlayWidgetControllerCached())
			{
				WC->OnAbilityInputPressed.Broadcast(InputTag);
			}
		}
	}
}

void ADRPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	// 반드시 아래 early return 들보다 먼저 — 관전/Block.InputReleased 에 걸려 리턴하면
	// 태그가 "눌린 채"로 영구히 남아 이후 GA 종료발 Released 재방송이 계속 억제된다
	HeldInputTags.Remove(InputTag);

	if (bIsSpectating) return;

	// 입력 해제 블록 확인
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputReleased)) return;

	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagReleased(InputTag);

	// 스킬 아이콘 UI 피드백 브로드캐스트 (로컬 컨트롤러만, 차단 중이면 발화 안 함)
	if (IsLocalController() && !IsAbilityInputBlocked(InputTag))
	{
		if (ADRHUD* HUD = Cast<ADRHUD>(GetHUD()))
		{
			if (UOverlayWidgetController* WC = HUD->GetOverlayWidgetControllerCached())
			{
				WC->OnAbilityInputReleased.Broadcast(InputTag);
			}
		}
	}
}

void ADRPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	if (bIsSpectating) return;
		
	// 입력 홀드 블록 확인
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputHeld)) return;

	if (GetASC() == nullptr) return;
	GetASC()->AbilityInputTagHeld(InputTag);
}

UDRAbilitySystemComponent* ADRPlayerController::GetASC() const
{
	return Cast<UDRAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
}

bool ADRPlayerController::IsAbilityInputBlocked(const FGameplayTag& InputTag) const
{
	UDRAbilitySystemComponent* ASC = GetASC();
	if (!ASC) return false;

	// Carrying 중에는 모든 스킬 입력이 차단된 것으로 간주 (UI 피드백도 막음)
	if (ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Carrying))
	{
		return true;
	}

	// InputTag → AbilitySpec → AbilityTag → 차단 카운터 검사
	if (FGameplayAbilitySpec* Spec = ASC->FindAbilitySpecByInputTag(InputTag))
	{
		const FGameplayTag AbilityTag = UDRAbilitySystemComponent::GetAbilityTagFromSpec(*Spec);
		if (AbilityTag.IsValid() && ASC->IsAbilityTagBlocked(AbilityTag))
		{
			return true;
		}
	}

	return false;
}

void ADRPlayerController::ServerCheatSkipToNextPhase_Implementation()
{
#if !UE_BUILD_SHIPPING
	// GameMode 가져오기
	ADRStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<ADRStageGameMode>();
	if (!StageGameMode) return;

	// 페이즈 전환
	if (UDRPhase3* Phase3 = Cast<UDRPhase3>(StageGameMode->GetCurrentPhase()))
	{
		Phase3->SkipToNextWave();
		return;
	}

	StageGameMode->TransitionToNextPhase();
#endif
}

void ADRPlayerController::ServerRequestDropPart_Implementation()
{
	// 캐릭터 가져오기
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// 부품 떨어트리기
	DRCharacter->DropCarriedPart();
}

void ADRPlayerController::RequestChangeClass(bool bNext)
{
	ServerRequestChangeClass(bNext);
}

void ADRPlayerController::ServerRequestChangeClass_Implementation(bool bNext)
{
	// 대기실 상태에서만 가능
	ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
	if (!LGS || LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

	ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
	if (!PS) return;

	// 현재 클래스 가져오기
	int32 CurrentIndex = static_cast<int32>(PS->GetSelectedPlayerClass());
	constexpr int32 ClassCount = static_cast<int32>(EPlayerCharacterClass::Count);

	// 순환
	int32 NewIndex;
	if (bNext)
		NewIndex = (CurrentIndex + 1) % ClassCount;
	else
		NewIndex = (CurrentIndex - 1 + ClassCount) % ClassCount;

	EPlayerCharacterClass NewClass = static_cast<EPlayerCharacterClass>(NewIndex);
	PS->SetSelectedPlayerClass(NewClass);

	// PlayerController 에도 캐싱 (Seamless Travel 시 PlayerState 값 유실 방지)
	CachedSelectedClass = NewClass;
}

void ADRPlayerController::ClientTeleportToSlot_Implementation(FVector SlotLocation, FRotator SlotRotation)
{
	if (APawn* MyPawn = GetPawn())
	{
		MyPawn->TeleportTo(SlotLocation, SlotRotation);

		if (UCharacterMovementComponent* MovementComp =
			Cast<UCharacterMovementComponent>(MyPawn->GetMovementComponent()))
		{
			MovementComp->Velocity = FVector::ZeroVector;
			MovementComp->DisableMovement();
		}
	}
}

void ADRPlayerController::ClientSetWaitingRoomView_Implementation(
	ADRWaitingRoomCameraActor* CameraActor)
{
	if (!CameraActor) return;

	// 카메라 캐시 저장 (ClientRestart에서 재고정에 사용)
	CachedWaitingRoomCamera = CameraActor;

	bIsInWaitingRoom = true;
	bAutoManageActiveCameraTarget = false; // Pawn 변경 시 자동 ViewTarget 전환 차단

	// 잘못 생성된 LobbyOverlay가 있으면 제거
	if (ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD()))
	{
		DRHUD->RemoveOverlay();
	}

	// 즉시 고정 카메라로 전환
	SetViewTargetWithBlend(CameraActor, 0.f);

	// UI Only 모드 (마우스 커서 ON)
	// TEST: GameAndUI mode for waiting room (Tab/ESC focus issue test)
	// Revert: change block back to SetInputMode(FInputModeUIOnly());
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	SetShowMouseCursor(true);

	// 소유 Pawn의 메시 가시성 전환 (1P 숨기기, 3P 보이기)
	if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetPawn()))
	{
		DRCharacter->SetWaitingRoomVisibility(true);
	}

	// 대기실 UI 생성 (아직 없으면)
	CreateWaitingRoomUI();
}

void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
	bIsInWaitingRoom = false;

	APawn* MyPawn = GetPawn();
	if (!MyPawn)
	{
		// Pawn이 아직 복제되지 않았으면 OnPossessedPawnChanged 이벤트에서 전환 실행 (5초 폴백)
		bPendingCameraTransition = true;
		GetWorldTimerManager().SetTimer(
			CameraTransitionRetryHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				if (!bPendingCameraTransition) return;
				bPendingCameraTransition = false;

				// 최소한 InputMode 전환으로 완전 고착 방지
				bAutoManageActiveCameraTarget = true;
				SetInputMode(FInputModeGameOnly());
				SetShowMouseCursor(false);
				InitOverlayForFreeRoam();
			}),
			5.0f,
			false
		);
		return;
	}

	// Pawn이 이미 있으면 즉시 실행 (호스트 또는 빠른 리플리케이션)
	ExecuteCameraTransitionToCharacter();
}

void ADRPlayerController::ExecuteCameraTransitionToCharacter()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return;

	// 메시 가시성 복원 (3P 숨기기, 1P 보이기 — 일반 FPS 모드)
	if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(MyPawn))
	{
		DRCharacter->SetWaitingRoomVisibility(false);
	}

	// 고정 카메라 → 캐릭터 카메라로 1.5초간 부드럽게 블렌드
	SetViewTargetWithBlend(MyPawn, 1.5f, EViewTargetBlendFunction::VTBlend_EaseInOut);

	// 전환 완료 후 인풋 모드 변경 + HUD 오버레이 초기화
	FTimerHandle InputTimerHandle;
	GetWorldTimerManager().SetTimer(
		InputTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{

			// (1) 블렌드 완료 후에야 자동 카메라 관리 활성화
			bAutoManageActiveCameraTarget = true;

			// (2) ViewTarget을 Pawn으로 확정 (블렌드 잔여 상태 정리)
			if (APawn* FinalPawn = GetPawn())
			{
				SetViewTarget(FinalPawn);

				// (3) ControlRotation을 Pawn의 현재 회전으로 동기화
				SetControlRotation(FinalPawn->GetActorRotation());
			}

			// (4) 입력 모드 전환
			SetInputMode(FInputModeGameOnly());
			SetShowMouseCursor(false);

			// (5) HUD 오버레이 초기화 (대기실에서 스킵했으므로)
			InitOverlayForFreeRoam();
		}),
		1.5f,
		false
	);
}

void ADRPlayerController::HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn)
{
	// 폰 교체(사망/리스폰/클래스 변경) 시 홀드 상태 초기화 —
	// 키를 누른 채 폰이 바뀌면 Released 입력이 유실돼 태그가 눌린 채 남을 수 있다
	HeldInputTags.Reset();

	if (!NewPawn) return;

	// 캐릭터별 상하 시야각 제한 — 청소기만 ±ViewPitchLimit(기본 30도), 그 외 캐릭터는 기본값 복원.
	// OnPossessedPawnChanged는 서버·소유 클라 양쪽에서 발화하고, 카메라 클램프는 로컬 카메라 매니저에만 의미가 있다.
	if (PlayerCameraManager)
	{
		if (const ADRRobotVacuumCharacter* Vacuum = Cast<ADRRobotVacuumCharacter>(NewPawn))
		{
			PlayerCameraManager->ViewPitchMin = -Vacuum->ViewPitchLimit;
			PlayerCameraManager->ViewPitchMax = Vacuum->ViewPitchLimit;
		}
		else
		{
			PlayerCameraManager->ViewPitchMin = -ViewPitchMin;
			PlayerCameraManager->ViewPitchMax = ViewPitchMax;
		}
	}

	// ViewTarget 복원 대기 중이었으면 즉시 복원 (기존 0.5초 재시도 타이머 대체)
	if (bPendingViewTargetRestore)
	{
		bPendingViewTargetRestore = false;

		// 대기실이면 ViewTarget 복원 스킵 (ClientSetWaitingRoomView가 담당)
		if (!bIsInWaitingRoom)
		{
			SetViewTarget(NewPawn);
			RestoreDefaultInputMode();
		}
	}

	// 카메라 전환 대기 중이었으면 즉시 실행 (기존 0.1초 폴링 대체)
	if (bPendingCameraTransition)
	{
		bPendingCameraTransition = false;
		GetWorldTimerManager().ClearTimer(CameraTransitionRetryHandle);
		ExecuteCameraTransitionToCharacter();
	}
}

void ADRPlayerController::OnRep_Pawn()
{
	// 엔진 기본 처리 (AcknowledgePossession 등)
	Super::OnRep_Pawn();

	// 대기실에서는 엔진이 변경한 ViewTarget을 즉시 복원
	if (bIsInWaitingRoom && CachedWaitingRoomCamera.IsValid())
	{
		bAutoManageActiveCameraTarget = false;
		SetViewTargetWithBlend(CachedWaitingRoomCamera.Get(), 0.f);
	}
}

void ADRPlayerController::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	// 클라이언트가 자기 PlayerState 를 처음 알게 되는 시점 — 여기서 장착 칩을 보고한다.
	// (PlayerState 의 BeginPlay 시점에는 컨트롤러 연결이 아직 없을 수 있다. Plan2.md 7.1)
	ReportUpgradeLoadout();
	ReportCosmeticLoadout();	// 옷도 같은 이유로 (Plan(옷장) M3)
}

void ADRPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	// 대기실에서는 ViewTarget 자동 전환만 차단하고, 나머지 엔진 초기화는 정상 수행
	if (bIsInWaitingRoom)
	{
		// bAutoManageActiveCameraTarget = false로 Super 내부의 ViewTarget 자동 전환 차단
		bAutoManageActiveCameraTarget = false;

		// Super 호출: ResetIgnoreInputFlags, AcknowledgePossession, PawnClientRestart 등
		// 엔진 초기화를 정상 수행 (호스트에서 Look/Move 입력이 무시되는 버그 방지)
		Super::ClientRestart_Implementation(NewPawn);

		// 새 폰에 대기실 가시성 적용
		if (NewPawn)
		{
			if (ADRCharacter* DRChar = Cast<ADRCharacter>(NewPawn))
			{
				DRChar->SetWaitingRoomVisibility(true);
			}
		}

		// Super가 ViewTarget을 변경했을 수 있으므로 캐시된 카메라로 즉시 복원
		if (CachedWaitingRoomCamera.IsValid())
		{
			SetViewTargetWithBlend(CachedWaitingRoomCamera.Get(), 0.f);
		}

		return;
	}

	Super::ClientRestart_Implementation(NewPawn);
}

void ADRPlayerController::ClientKicked_Implementation(const FString& Reason)
{
	// 대기실 UI 제거
	DestroyWaitingRoomUI();

	// 세션 떠나기 → 메인 메뉴로 이동
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMultiplayerSessionsSubsystem* Subsystem = GI->GetSubsystem<UMultiplayerSessionsSubsystem>())
		{
			Subsystem->LeaveServer();
		}
	}
}

// ========== 대기실 UI ==========

void ADRPlayerController::CreateWaitingRoomUI()
{
	if (!IsLocalController()) return;
	if (WaitingRoomWidget) return; // 이미 존재
	if (!WaitingRoomWidgetClass) return;

	WaitingRoomWidget = CreateWidget<UDRWaitingRoomWidget>(this, WaitingRoomWidgetClass);
	if (WaitingRoomWidget)
	{
		WaitingRoomWidget->AddToViewport();

		// 호스트 여부 설정 (Listen Server의 로컬 컨트롤러 = 호스트)
		bool bIsHost = HasAuthority();
		WaitingRoomWidget->SetIsHost(bIsHost);

		// LobbyState 변경 구독
		if (ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>())
		{
			LGS->OnLobbyStateChanged.AddDynamic(this, &ADRPlayerController::OnLobbyStateChangedForUI);
		}

		RefreshWaitingRoomUI();
	}
}

void ADRPlayerController::DestroyWaitingRoomUI()
{
	// 델리게이트 해제
	if (UWorld* World = GetWorld())
	{
		if (ADRLobbyGameState* LGS = World->GetGameState<ADRLobbyGameState>())
		{
			LGS->OnLobbyStateChanged.RemoveDynamic(this, &ADRPlayerController::OnLobbyStateChangedForUI);
		}
	}

	if (WaitingRoomWidget)
	{
		WaitingRoomWidget->RemoveFromParent();
		WaitingRoomWidget = nullptr;
	}
}

void ADRPlayerController::ClientRefreshWaitingRoomUI_Implementation()
{
	RefreshWaitingRoomUI();
}

void ADRPlayerController::RefreshWaitingRoomUI()
{
	if (!WaitingRoomWidget) return;

	ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
	ADRGameStateBase* GS = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!LGS || !GS) return;

	// 로컬 PlayerState 유효성 검사 + 재시도
	APlayerState* MyPS = GetPlayerState<APlayerState>();
	if (!MyPS)
	{
		FTimerHandle RetryTimer;
		GetWorldTimerManager().SetTimer(
			RetryTimer,
			[WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
			{
				if (ADRPlayerController* PC = WeakThis.Get())
				{
					PC->RefreshWaitingRoomUI();
				}
			},
			0.2f, false
		);
		return;
	}

	TArray<FWaitingRoomPlayerInfo> Infos;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS) continue;
		ADRPlayerState* DRPS = Cast<ADRPlayerState>(PS);
		if (!DRPS) continue;

		FWaitingRoomPlayerInfo Info;
		Info.PlayerName = PS->GetPlayerName();
		Info.SelectedClass = DRPS->GetSelectedPlayerClass();
		Info.bIsHost = GS->IsPlayerHost(PS);
		Info.bIsReady = Info.bIsHost ? true : DRPS->IsReady(); // 호스트는 항상 준비 상태로 표시
		Info.OwningPlayerState = PS;

		// 서버 권위 슬롯 인덱스 사용
		Info.SlotIndex = DRPS->GetWaitingRoomSlotIndex();

		// bIsLocalPlayer 판정 (PlayerId 비교)
		Info.bIsLocalPlayer = (PS->GetPlayerId() == MyPS->GetPlayerId());

		Infos.Add(Info);
	}

	// SlotIndex 기준으로 정렬하여 UI에 전달
	Infos.Sort([](const FWaitingRoomPlayerInfo& A, const FWaitingRoomPlayerInfo& B)
	{
		return A.SlotIndex < B.SlotIndex;
	});

	WaitingRoomWidget->RefreshPlayerSlots(Infos);
}

void ADRPlayerController::OnLobbyStateChangedForUI(ELobbyState NewState)
{
	if (NewState == ELobbyState::Transitioning || NewState == ELobbyState::FreeRoam)
	{
		DestroyWaitingRoomUI();
	}
}

void ADRPlayerController::InitOverlayForFreeRoam()
{
	ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD());
	if (!DRHUD) return;

	ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
	if (!PS) return;

	UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
	UAttributeSet* AS = PS->GetAttributeSet();
	if (!ASC || !AS) return;

	DRHUD->InitOverlay(this, PS, ASC, AS);

	// 위젯 트리 완전 초기화 후 어빌리티 아이콘 강제 갱신
	// 위젯 트리 구성이 끝난 다음 틱에 갱신 (매직 딜레이 대신 프레임 확정 타이밍)
	GetWorldTimerManager().SetTimerForNextTick(
		[WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
		{
			ADRPlayerController* PC = WeakThis.Get();
			if (!PC) return;

			ADRHUD* HUD = Cast<ADRHUD>(PC->GetHUD());
			if (!HUD) return;

			ADRPlayerState* PlayerState = PC->GetPlayerState<ADRPlayerState>();
			if (!PlayerState) return;

			UAbilitySystemComponent* AbilitySystem = PlayerState->GetAbilitySystemComponent();
			UAttributeSet* Attributes = PlayerState->GetAttributeSet();
			if (!AbilitySystem || !Attributes) return;

			const FWidgetControllerParams Params(PC, PlayerState, AbilitySystem, Attributes);
			if (UOverlayWidgetController* WC = HUD->GetOverlayWidgetController(Params))
			{
				WC->BroadcastAbilityInfo();
			}
		}
	);
}

void ADRPlayerController::ServerRequestPowerOn_Implementation()
{
	// 호스트 검증: Server RPC이므로 서버에서 실행됨
	// IsLocalController()로 호스트(Listen Server) 확인
	if (!IsLocalController() || !HasAuthority()) return;

	ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
	if (LobbyGM)
	{
		LobbyGM->PowerOn(this);
	}
}

void ADRPlayerController::OnPowerOnButtonPressed()
{
	// 버튼 클릭 시 로컬에서 호출됨. 호스트(리슨 서버)면 PowerOn 시도, 원격 클라이언트면 준비 토글.
	if (HasAuthority())
	{
		ServerRequestPowerOn();
	}
	else
	{
		ServerToggleReady();
	}
}

void ADRPlayerController::ServerToggleReady_Implementation()
{
	// 클라이언트 요청 → 서버에서 실행. 리슨 서버 호스트는 준비 토글 대상이 아님.
	// (Server RPC 안이므로 HasAuthority()는 항상 true - 실질 판별은 로컬 컨트롤러 여부)
	if (IsListenServerHost()) return;

	ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
	if (!LobbyGM) return;

	ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
	if (!PS) return;

	LobbyGM->SetPlayerReady(this, !PS->IsReady());
}

void ADRPlayerController::ServerRequestKickPlayer_Implementation(APlayerState* TargetPlayerState)
{
	// 호스트 검증
	if (!IsLocalController() || !HasAuthority()) return;
	if (!TargetPlayerState) return;

	// TargetPlayerState → PlayerController 찾기
	APlayerController* TargetPC = Cast<APlayerController>(TargetPlayerState->GetOwner());
	ADRPlayerController* TargetDRPC = Cast<ADRPlayerController>(TargetPC);

	ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
	if (LobbyGM && TargetDRPC)
	{
		LobbyGM->KickPlayer(this, TargetDRPC);
	}
}

