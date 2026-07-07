// Copyright DaeRune


#include "Player/DRPlayerController.h"
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
#include "Interaction/DRInteractable.h"
#include "Actor/DRWaitingRoomCameraActor.h"
#include "Tutorial/DRTutorialManager.h"
#include "Character/DRCharacter.h"
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
#include "Sound/DRSoundManager.h"
#include "Actor/DRBGMActor.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Widget/DRWaitingRoomWidget.h"
#include "Game/DRLobbyGameMode.h"
#include "MultiplayerSessionsSubsystem.h"

ADRPlayerController::ADRPlayerController()
{
	// 占쏙옙티占시뤄옙占쏙옙 占쏙옙占시몌옙占쏙옙占싱쇽옙 활占쏙옙화
	bReplicates = true;
	
	// 占쏙옙품 占시쏙옙占쏙옙 占십깍옙화
	bPartDetectionEnabled = false;
	CurrentDetectedPart = nullptr;
	LineTraceTimer = 0.f;
}

void ADRPlayerController::CorruptedStateChanged(bool bIsStateChanged)
{
	bIsCorrupted = bIsStateChanged;
}

void ADRPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter)
{
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
}

ADRCleanserPart* ADRPlayerController::FindPartByLineTrace()
{
	// 캐占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return nullptr;

	// 캐占쏙옙占싶곤옙 占싱뱄옙 占쏙옙품占쏙옙 占쏙옙占?占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙
	if (DRCharacter->IsCarryingPart()) return nullptr;

	// 카占쌨띰옙 占쏙옙占쏙옙占쏙옙트 占쏙옙占쏙옙占쏙옙占쏙옙
	UCameraComponent* Camera = DRCharacter->GetFollowCamera();
	if (!Camera) return nullptr;

	FVector Start = Camera->GetComponentLocation();
	FVector End = Start + Camera->GetForwardVector() * LineTraceDistance;

	// 멀티 라인트레이스: 사이트 메시 등에 가려져도 부품을 찾을 수 있도록 모든 히트를 검사
	TArray<FHitResult> HitResults;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(DRCharacter);

	GetWorld()->LineTraceMultiByChannel(
		HitResults,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	for (const FHitResult& Hit : HitResults)
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

ADRCleanserSite* ADRPlayerController::FindSiteByLineTrace()
{
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return nullptr;

	// 부품을 들고 있어야만 사이트 감지
	if (!DRCharacter->IsCarryingPart()) return nullptr;

	UCameraComponent* Camera = DRCharacter->GetFollowCamera();
	if (!Camera) return nullptr;

	const FVector Start = Camera->GetComponentLocation();
	const FVector End = Start + Camera->GetForwardVector() * LineTraceDistance;

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(DRCharacter);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	if (bHit)
	{
		ADRCleanserSite* HitSite = Cast<ADRCleanserSite>(HitResult.GetActor());
		// 박스 오버랩 중인 사이트인지 확인 (다른 사이트가 시점에 잡히는 케이스 차단)
		if (HitSite && OverlappedSites.Contains(HitSite))
		{
			return HitSite;
		}
	}

	return nullptr;
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
	// 愿???곹깭 媛뺤젣 珥덇린??(bIsSpectating ?щ?? ?곴??놁씠)
	bIsSpectating = false;
	CurrentSpectatedPlayerIndex = 0;

	// ?몃━寃뚯씠???댁젣
	if (CurrentSpectatedCharacter.IsValid())
	{
		if (ADRCharacterBase* OldTarget = Cast<ADRCharacterBase>(CurrentSpectatedCharacter.Get()))
		{
			OldTarget->OnDeathDelegate.RemoveDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
		}
	}
	CurrentSpectatedCharacter.Reset();

	// ?湲곗떎?대㈃ ViewTarget 蹂듭썝 ?ㅽ궢 (ClientSetWaitingRoomView媛 ?대떦)
	if (bIsInWaitingRoom)
	{
		// FreeRoam?먯꽌 ClientStopSpectating???몄텧??寃쎌슦 ?湲곗떎 ?뚮옒洹?援먯젙
		ADRLobbyGameState* LGS = GetWorld() ? GetWorld()->GetGameState<ADRLobbyGameState>() : nullptr;
		if (LGS && LGS->GetLobbyState() == ELobbyState::FreeRoam)
		{
			bIsInWaitingRoom = false;
			// ?꾨옒濡?怨꾩냽 吏꾪뻾?섏뿬 ViewTarget 蹂듭썝
		}
		else
		{
			return;
		}
	}

	// ViewTarget 蹂듭썝
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

void ADRPlayerController::SpectateNextPlayer()
{
	if (!bIsSpectating || !IsLocalController()) return;

	ADRGameStateBase* GameStateBase = GetWorld()->GetGameState<ADRGameStateBase>();
	if (!GameStateBase) return;

	TArray<ADRCharacter*> AliveCharacters = GameStateBase->GetAlivePlayers();
	if (AliveCharacters.Num() == 0)
	{
		// 紐⑤몢 ?щ쭩 - 愿??????놁쓬
		CurrentSpectatedCharacter.Reset();
		return;
	}
	
	if(AliveCharacters.Num() == 1) return;

	// ?ㅼ쓬 ?몃뜳??怨꾩궛
	CurrentSpectatedPlayerIndex = (CurrentSpectatedPlayerIndex + 1) % AliveCharacters.Num();
    
	// ??愿??????ㅼ젙
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

	if(AliveCharacters.Num() == 1) return;

	// ?댁쟾 ?몃뜳??怨꾩궛
	CurrentSpectatedPlayerIndex--;
	if (CurrentSpectatedPlayerIndex < 0)
	{
		CurrentSpectatedPlayerIndex = AliveCharacters.Num() - 1;
	}

	// ??愿??????ㅼ젙
	SetSpectateTarget(AliveCharacters[CurrentSpectatedPlayerIndex]);
}

void ADRPlayerController::ClientStopAllAudio_Implementation()
{
	// BGM ?뺤?
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
// 媛쒕컻 鍮뚮뱶?먯꽌留??숈옉?섎룄濡?泥댄겕
#if !UE_BUILD_SHIPPING
	ServerCheatSkipToNextPhase();
#endif
}

void ADRPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Pawn 복제 도착 이벤트 바인딩 (타이머 재시도 대체)
	OnPossessedPawnChanged.AddDynamic(this, &ADRPlayerController::HandlePossessedPawnChanged);

	// Enhanced Input Context占쏙옙 占쏙옙占쏙옙占실억옙 占쌍댐옙占쏙옙 확占쏙옙
	check(DRContext);

	// Enhanced Input 占쏙옙占쏙옙첵占쏙옙謗占?占쏙옙占쏙옙 占쏙옙占쌔쏙옙트 占쌩곤옙
	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(DRContext, 0);
	}

	// UI 占쏙옙占쏙옙
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());

	// 占시뤄옙占싱억옙占?TeamId 0
	SetGenericTeamId(FGenericTeamId(0));

	// 移대찓???쇱튂 ?쒗븳 ?ㅼ젙
	if (PlayerCameraManager)
	{
		PlayerCameraManager->ViewPitchMin = -ViewPitchMin;  // ?꾨옒 (?뚯닔)
		PlayerCameraManager->ViewPitchMax = ViewPitchMax;   // ??(?묒닔)
	}

	// 濡쒖뺄 ?뚮젅?댁뼱留??ㅻ뵒???ㅼ젙 ?곸슜
	if (IsLocalController())
	{
		// Manager ?듯빐???ㅻ뵒???ㅼ젙 ?곸슜
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

	// 둘 다 비활성화면 스킵
	if (!bPartDetectionEnabled && !bSiteDetectionEnabled) return;

	LineTraceTimer += DeltaTime;
	if (LineTraceTimer >= LineTraceUpdateInterval)
	{
		LineTraceTimer = 0.f;

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
	}
}
void ADRPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	// DRInputComponent占쏙옙 캐占쏙옙占쏙옙
	UDRInputComponent* DRInputComponent = CastChecked<UDRInputComponent>(InputComponent);
	// 占썩본 占쌉뤄옙 占쌓쇽옙 占쏙옙占싸듸옙
	DRInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Move);
	DRInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ADRPlayerController::Look);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Started, this, &ADRPlayerController::StartJump);
	DRInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ADRPlayerController::StopJump);
	DRInputComponent->BindAction(InteractAction, ETriggerEvent::Triggered, this, &ADRPlayerController::HandleInteract);
	DRInputComponent->BindAction(SpectateNextAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleSpectateNext);
	DRInputComponent->BindAction(SpectatePreviousAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleSpectatePrevious);
	DRInputComponent->BindAction(ToggleSettingsAction, ETriggerEvent::Started, this, &ADRPlayerController::HandleToggleSettings);
	// Tab Hold: 누름/뗌 두 이벤트 모두 바인딩. Hold Trigger 없이 Started/Completed로 Hold 동작 구현.
	DRInputComponent->BindAction(CharacterInfoAction, ETriggerEvent::Started,   this, &ADRPlayerController::HandleCharacterInfoPressed);
	DRInputComponent->BindAction(CharacterInfoAction, ETriggerEvent::Completed, this, &ADRPlayerController::HandleCharacterInfoReleased);
	// 占쏙옙占쏙옙占싣?占쌉뤄옙 占쏙옙占싸듸옙 (InputConfig 占쏙옙占?
	DRInputComponent->BindAbilityActions(InputConfig, this, &ThisClass::AbilityInputTagPressed, &ThisClass::AbilityInputTagReleased, &ThisClass::AbilityInputTagHeld);
}

void ADRPlayerController::ReceivedPlayer()
{
	Super::ReceivedPlayer();

	// 濡쒖뺄 而⑦듃濡ㅻ윭留?泥섎━
	if (!IsLocalController()) return;
// ?덈꺼 吏꾩엯 ??怨듯넻 珥덇린???섑뻾
	OnLevelEntered();
}

void ADRPlayerController::PostSeamlessTravel()
{
	Super::PostSeamlessTravel();

	// 濡쒖뺄 而⑦듃濡ㅻ윭留?泥섎━
	if (!IsLocalController()) return;
// SeamlessTravel ???덈꺼 吏꾩엯 ??怨듯넻 珥덇린???섑뻾
	OnLevelEntered();
}

void ADRPlayerController::OnLevelEntered()
{
	// 레벨 컨텍스트 캐시 재판별 (ReceivedPlayer/PostSeamlessTravel에서 호출되므로 레벨 이동마다 갱신됨)
	CachedLevelContext = DetermineLevelContext();

	// ?꾩옱 ?덈꺼 ?뺤씤
	UWorld* World = GetWorld();
	if (!World) return;

	// 寃뚯엫?ㅻ쾭 UI媛 ?⑥븘?덉쑝硫??쒓굅
	if (CurrentResultWidget)
	{
		CurrentResultWidget->RemoveFromParent();
		CurrentResultWidget = nullptr;
	}

	// ?湲곗떎 ?꾩젽 ?뺣━ (?덈꺼 ?대룞 ??臾댄슚?붾맂 ?ъ씤???뺣━)
	DestroyWaitingRoomUI();

	// ?ㅼ젙 硫붾돱 ?곹깭 珥덇린??(?덈꺼 ?대룞 ??
	if (bIsSettingsMenuOpen)
	{
		CloseSettingsMenu();
	}
	bIsSettingsMenuOpen = false;

	// 愿???곹깭 珥덇린??(?몃━寃뚯씠???뺣━ ?ы븿)
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

	// ?湲곗떎 媛먯? ??ViewTarget 蹂듭썝 李⑤떒
	ADRLobbyGameState* LGS = World->GetGameState<ADRLobbyGameState>();

	if (IsInLobby())
	{
		if (!LGS)
		{
			// GameState ?꾩쭅 蹂듭젣 ?덈맖 ???湲곗떎濡?媛??(?쒕쾭 RPC媛 ?댄썑 援먯젙)
			bIsInWaitingRoom = true;
			bAutoManageActiveCameraTarget = false; // Pawn ?섏떊 ??ViewTarget ?먮룞 ?ㅼ젙 李⑤떒
			RestoreDefaultInputMode();
			// WaitingRoom UI??ClientSetWaitingRoomView RPC?먯꽌 ?앹꽦
			return;
		}

		if (LGS->GetLobbyState() == ELobbyState::WaitingRoom
			|| LGS->GetLobbyState() == ELobbyState::Transitioning)
		{
			bIsInWaitingRoom = true;
			bAutoManageActiveCameraTarget = false; // Pawn ?섏떊 ??ViewTarget ?먮룞 ?ㅼ젙 李⑤떒
			RestoreDefaultInputMode();
			CreateWaitingRoomUI();
			return;  // ViewTarget? ClientSetWaitingRoomView RPC媛 ?ㅼ젙
		}
	}

	bIsInWaitingRoom = false;

	// ViewTarget 利됱떆 蹂듭썝 (?섎せ???寃?李몄“ 諛⑹?)
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

	// ?덈꺼??留욌뒗 湲곕낯 ?낅젰 紐⑤뱶濡?蹂듭썝
	RestoreDefaultInputMode();

	// BGM? ?덈꺼??諛곗튂??DRBGMActor媛 ?대떦
}

void ADRPlayerController::HandleToggleSettings()
{
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
	DRHUD->ShowCharacterInfo(GetCachedSelectedClass());
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
	// ?대씪?댁뼵?? ?쒕쾭???붿껌 + 濡쒖뺄?먯꽌 ViewTarget ?ㅼ젙
	if (!HasAuthority())
	{
		ServerSetSpectateTarget(NewTarget);

		// ?대씪?댁뼵?몄뿉?쒕룄 ViewTarget怨?罹먮┃??罹먯떆 ?ㅼ젙
		CurrentSpectatedCharacter = NewTarget;
		if (NewTarget)
		{
			SetViewTarget(NewTarget);
		}
		return;
	}

	// ?쒕쾭: ?꾩껜 濡쒖쭅 ?ㅽ뻾

	// ?댁쟾 ??곸쓽 ?щ쭩 ?몃━寃뚯씠???댁젣
	if (CurrentSpectatedCharacter.IsValid())
	{
		if (ADRCharacterBase* OldTarget = Cast<ADRCharacterBase>(CurrentSpectatedCharacter.Get()))
		{
			OldTarget->OnDeathDelegate.RemoveDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
		}
	}

	CurrentSpectatedCharacter = NewTarget;

	if (NewTarget)
	{
		// ViewTarget ?ㅼ젙
		SetViewTarget(NewTarget);

		// UI ?낅뜲?댄듃
		ClientUpdateSpectatorUI(NewTarget);

		// ????곸쓽 ?щ쭩 ?몃━寃뚯씠??諛붿씤??
		if (ADRCharacterBase* DRTarget = Cast<ADRCharacterBase>(NewTarget))
		{
			DRTarget->OnDeathDelegate.AddDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
		}
	}
}

void ADRPlayerController::ServerSetSpectateTarget_Implementation(ACharacter* NewTarget)
{
	// ?쒕쾭?먯꽌 SetSpectateTarget ?ㅽ뻾
	SetSpectateTarget(NewTarget);
}

void ADRPlayerController::ClientUpdateSpectatorUI_Implementation(ACharacter* SpectatedTarget)
{
	// ?대씪?댁뼵?몄뿉?쒕쭔 ?ㅽ뻾
	if (!IsLocalController()) return;

	UpdateSpectatorUI(SpectatedTarget);
}

void ADRPlayerController::UpdateSpectatorUI(ACharacter* SpectatedTarget)
{
	if (!SpectatedTarget) return;

	// HUD 媛?몄삤湲?
	ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD());
	if (!DRHUD) return;

	// 愿????곸쓽 PlayerState 媛?몄삤湲?
	ADRPlayerState* SpectatedPS = SpectatedTarget->GetPlayerState<ADRPlayerState>();
	if (!SpectatedPS) return;

	// 愿????곸쓽 GAS 而댄룷?뚰듃??媛?몄삤湲?
	UAbilitySystemComponent* SpectatedASC = SpectatedPS->GetAbilitySystemComponent();
	UAttributeSet* SpectatedAS = SpectatedPS->GetAttributeSet();

	if (!SpectatedASC || !SpectatedAS) return;

	// 湲곗〈 WidgetController瑜??뚭눼?섍퀬 ?덈줈 留뚮뱾?댁쨲
	DRHUD->UpdateOverlayForSpectating(this, SpectatedPS, SpectatedASC, SpectatedAS);
}

void ADRPlayerController::OnSpectatedPlayerDied(AActor* DeadActor)
{
	if (!bIsSpectating) return;

	// ?쎄컙???쒕젅?????ㅼ쓬 ?뚮젅?댁뼱濡??꾪솚
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
	
	// 占쌉뤄옙 占쏙옙占?占쏙옙占쏙옙 확占쏙옙
	if (GetASC() && GetASC()->HasMatchingGameplayTag(FDRGameplayTags::Get().Player_Block_InputPressed)) return;

	// 2D 占쌉뤄옙占쏙옙 占쏙옙占쏙옙 占쏙옙표占쏙옙占?占쏙옙환
	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();
	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	// 占쏙옙占쏙옙 占싱듸옙 占쌉뤄옙 占쏙옙占쏙옙
	if (APawn* ControlledPawn = GetPawn<APawn>())
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}

void ADRPlayerController::Look(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// 占쏙옙占쎌스 占시쇽옙 처占쏙옙
	const FVector2D Axis = InputActionValue.Get<FVector2D>();

	// Manager?먯꽌 媛먮룄 媛?몄삤湲?
	// 감도는 캐시 사용 (설정 변경 시 OnMouseSensitivityChanged 델리게이트로 갱신)
	const float Sensitivity = CachedMouseSensitivity;

	// 媛먮룄 ?곸슜
	AddYawInput(Axis.X * Sensitivity);
	AddPitchInput(Axis.Y * Sensitivity);
}

void ADRPlayerController::StartJump(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// 占쏙옙占쏙옙 占쏙옙占쏙옙
	if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn<APawn>()))
	{
		ControlledCharacter->Jump();
	}
}

void ADRPlayerController::StopJump(const FInputActionValue& InputActionValue)
{
	if (bIsSpectating) return;
	
	// 占쏙옙占쏙옙 占쏙옙占쏙옙
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
	
	// 遺?덉쓣 ?ㅺ퀬 ?덉? ?딆쓣 ?뚮쭔 遺???띾뱷 ?쒕룄
	if (!DRCharacter->IsCarryingPart() && CurrentDetectedPart)
	{
		ServerRequestInteract(CurrentDetectedPart);
		return;
	}
	
	// 遺?덉쓣 ?ㅺ퀬 ?덇퀬 ?대젋? ?ъ씠???ㅻ쾭??以묒씠硫??ㅼ튂
	if (DRCharacter->IsCarryingPart())
	{
		// ?대젋? ?ъ씠??踰붿쐞 ?덉씠硫??ㅼ튂
		if (CurrentOverlappedSite)
		{
			ServerRequestInteract(CurrentOverlappedSite);
			return;
		}
		// ?대젋? ?ъ씠??踰붿쐞 諛뽰씠硫??⑥뼱?몃━湲?
		else
		{
			ServerRequestDropPart();
			return;
		}
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
	// 濡쒖뺄 而⑦듃濡ㅻ윭?먯꽌留??ㅽ뻾
	if (!IsLocalController()) return;

	// ?대? ?대젮?덉쑝硫?臾댁떆
	if (bIsSettingsMenuOpen) return;

	bIsSettingsMenuOpen = true;

	// ?낅젰 紐⑤뱶 蹂寃?
	SetInputMode(FInputModeUIOnly());
	SetShowMouseCursor(true);

	// Blueprint?먯꽌 ?꾩젽 ?앹꽦
	OnSettingsMenuOpened();
}

void ADRPlayerController::CloseSettingsMenu()
{
	// 濡쒖뺄 而⑦듃濡ㅻ윭?먯꽌留??ㅽ뻾
	if (!IsLocalController()) return;

	// ?대? ?ロ??덉쑝硫?臾댁떆
	if (!bIsSettingsMenuOpen) return;

	// Blueprint?먯꽌 ?꾩젽 ?쒓굅
	OnSettingsMenuClosed();

	bIsSettingsMenuOpen = false;
	RestoreDefaultInputMode();
}

void ADRPlayerController::ClientCloseSettingsMenu_Implementation()
{
	CloseSettingsMenu();
}

void ADRPlayerController::RestoreDefaultInputMode()
{
	if (!IsLocalController()) return;

	if (IsInMainMenu())
	{
		// 硫붿씤硫붾돱: UI 紐⑤뱶
		SetInputMode(FInputModeUIOnly());
		SetShowMouseCursor(true);
	}
	else if (IsInTutorial())
	{
		// ?쒗넗由ъ뼹: 寃뚯엫 紐⑤뱶
		SetInputMode(FInputModeGameOnly());
		SetShowMouseCursor(false);
	}
	else if (IsInLobby())
	{
		// 濡쒕퉬 ?곹깭???곕씪 遺꾧린
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
		// ?ㅽ뀒?댁?: 寃뚯엫 紐⑤뱶
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

void ADRPlayerController::Client_ShowGameOverUI_Implementation()
{
	// ?대? UI媛 ?쒖떆 以묒씠硫?臾댁떆
	if (CurrentResultWidget) return;

	// ?꾩젽 ?대옒?ㅺ? ?ㅼ젙?섏? ?딆븯?쇰㈃ 由ы꽩
	if (!GameOverWidgetClass) return;

	// 寃뚯엫 ?ㅻ쾭 ?꾩젽 ?앹꽦
	CurrentResultWidget = CreateWidget<UUserWidget>(this, GameOverWidgetClass);
	if (CurrentResultWidget)
	{
		// 酉고룷?몄뿉 異붽?
		CurrentResultWidget->AddToViewport(100);
	}
}

void ADRPlayerController::Client_ShowGameClearUI_Implementation()
{
	// ?대? UI媛 ?쒖떆 以묒씠硫?臾댁떆
	if (CurrentResultWidget) return;

	// ?꾩젽 ?대옒?ㅺ? ?ㅼ젙?섏? ?딆븯?쇰㈃ 由ы꽩
	if (!GameClearWidgetClass) return;

	// 寃뚯엫 ?대━???꾩젽 ?앹꽦
	CurrentResultWidget = CreateWidget<UUserWidget>(this, GameClearWidgetClass);
	if (CurrentResultWidget)
	{
		// 酉고룷?몄뿉 異붽?
		CurrentResultWidget->AddToViewport(100);
	}
}

void ADRPlayerController::Client_ShowTutorialClearUI_Implementation()
{
	if (CurrentResultWidget) return;

	if (!TutorialGameClearWidgetClass) return;

	CurrentResultWidget = CreateWidget<UUserWidget>(this, TutorialGameClearWidgetClass);
	if (CurrentResultWidget)
	{
		CurrentResultWidget->AddToViewport(100);
	}
}

void ADRPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	if (bIsSpectating) return;

	// 占쌉뤄옙 占쏙옙占?확占쏙옙 占쏙옙 占쏙옙占쏙옙占싣?占쌉뤄옙 처占쏙옙
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
	if (bIsSpectating) return;

	// 占쌉뤄옙 占쏙옙占쏙옙 占쏙옙占?확占쏙옙
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
		
	// 占쌉뤄옙 홀占쏙옙 占쏙옙占?확占쏙옙
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
	// GameMode 媛?몄삤湲?
	ADRStageGameMode* StageGameMode = GetWorld()->GetAuthGameMode<ADRStageGameMode>();
	if (!StageGameMode) return;

	// ?섏씠利??꾪솚
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
	// 罹먮┃??媛?몄삤湲?
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// 遺???⑥뼱?몃━湲?
	DRCharacter->DropCarriedPart();
}

void ADRPlayerController::RequestChangeClass(bool bNext)
{
	ServerRequestChangeClass(bNext);
}

void ADRPlayerController::ServerRequestChangeClass_Implementation(bool bNext)
{
	// ?湲곗떎 ?곹깭?먯꽌留?媛??
	ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
	if (!LGS || LGS->GetLobbyState() != ELobbyState::WaitingRoom) return;

	ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
	if (!PS) return;

	// ?꾩옱 ?대옒??媛?몄삤湲?
	int32 CurrentIndex = static_cast<int32>(PS->GetSelectedPlayerClass());
	constexpr int32 ClassCount = static_cast<int32>(EPlayerCharacterClass::Count);

	// ?쒗솚
	int32 NewIndex;
	if (bNext)
		NewIndex = (CurrentIndex + 1) % ClassCount;
	else
		NewIndex = (CurrentIndex - 1 + ClassCount) % ClassCount;

	EPlayerCharacterClass NewClass = static_cast<EPlayerCharacterClass>(NewIndex);
	PS->SetSelectedPlayerClass(NewClass);

	// PlayerController??罹먯떛 (Seamless Travel ??PlayerState 媛??좎떎 諛⑹?)
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

	// 移대찓??罹먯떆 ???(ClientRestart?먯꽌 ?ш퀬?뺤뿉 ?ъ슜)
	CachedWaitingRoomCamera = CameraActor;

	bIsInWaitingRoom = true;
	bAutoManageActiveCameraTarget = false; // Pawn 蹂寃????먮룞 ViewTarget ?꾪솚 李⑤떒

	// ?섎せ ?앹꽦??LobbyOverlay媛 ?덉쑝硫??쒓굅
	if (ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD()))
	{
		DRHUD->RemoveOverlay();
	}

	// 利됱떆 怨좎젙 移대찓?쇰줈 ?꾪솚
	SetViewTargetWithBlend(CameraActor, 0.f);

	// UI Only 紐⑤뱶 (留덉슦??而ㅼ꽌 ON)
	// TEST: GameAndUI mode for waiting room (Tab/ESC focus issue test)
	// Revert: change block back to SetInputMode(FInputModeUIOnly());
	{
		FInputModeGameAndUI InputMode;
		InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
		InputMode.SetHideCursorDuringCapture(false);
		SetInputMode(InputMode);
	}
	SetShowMouseCursor(true);

	// ?뚯쑀 Pawn??硫붿떆 媛?쒖꽦 ?꾪솚 (1P ?④린湲? 3P 蹂댁씠湲?
	if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetPawn()))
	{
		DRCharacter->SetWaitingRoomVisibility(true);
	}

	// ?湲곗떎 UI ?앹꽦 (?꾩쭅 ?놁쑝硫?
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

	// Pawn???대? ?덉쑝硫?利됱떆 ?ㅽ뻾 (?몄뒪???먮뒗 鍮좊Ⅸ 由ы뵆由ъ??댁뀡)
	ExecuteCameraTransitionToCharacter();
}

void ADRPlayerController::ExecuteCameraTransitionToCharacter()
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return;

	// 硫붿떆 媛?쒖꽦 蹂듭썝 (3P ?④린湲? 1P 蹂댁씠湲????쇰컲 FPS 紐⑤뱶)
	if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(MyPawn))
	{
		DRCharacter->SetWaitingRoomVisibility(false);
	}

	// 怨좎젙 移대찓????罹먮┃??移대찓?쇰줈 1.5珥덇컙 遺?쒕읇寃?釉붾젋??
	SetViewTargetWithBlend(MyPawn, 1.5f, EViewTargetBlendFunction::VTBlend_EaseInOut);

	// ?꾪솚 ?꾨즺 ???명뭼 紐⑤뱶 蹂寃?+ HUD ?ㅻ쾭?덉씠 珥덇린??
	FTimerHandle InputTimerHandle;
	GetWorldTimerManager().SetTimer(
		InputTimerHandle,
		FTimerDelegate::CreateWeakLambda(this, [this]()
		{

			// (1) 釉붾젋???꾨즺 ?꾩뿉???먮룞 移대찓??愿由??쒖꽦??
			bAutoManageActiveCameraTarget = true;

			// (2) ViewTarget??Pawn?쇰줈 ?뺤젙 (釉붾젋???붿뿬 ?곹깭 ?뺣━)
			if (APawn* FinalPawn = GetPawn())
			{
				SetViewTarget(FinalPawn);

				// (3) ControlRotation??Pawn???꾩옱 ?뚯쟾?쇰줈 ?숆린??
				SetControlRotation(FinalPawn->GetActorRotation());
			}

			// (4) ?낅젰 紐⑤뱶 ?꾪솚
			SetInputMode(FInputModeGameOnly());
			SetShowMouseCursor(false);

			// (5) HUD ?ㅻ쾭?덉씠 珥덇린??(?湲곗떎?먯꽌 ?ㅽ궢?덉쑝誘濡?
			InitOverlayForFreeRoam();
		}),
		1.5f,
		false
	);
}

void ADRPlayerController::HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn)
{
	if (!NewPawn) return;

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
	// ?붿쭊 湲곕낯 泥섎━ (AcknowledgePossession ??
	Super::OnRep_Pawn();

	// ?湲곗떎?먯꽌???붿쭊??蹂寃쏀븳 ViewTarget??利됱떆 蹂듭썝
	if (bIsInWaitingRoom && CachedWaitingRoomCamera.IsValid())
	{
		bAutoManageActiveCameraTarget = false;
		SetViewTargetWithBlend(CachedWaitingRoomCamera.Get(), 0.f);
	}
}

void ADRPlayerController::ClientRestart_Implementation(APawn* NewPawn)
{
	// ?湲곗떎?먯꽌??ViewTarget ?먮룞 ?꾪솚留?李⑤떒?섍퀬, ?섎㉧吏 ?붿쭊 珥덇린?붾뒗 ?뺤긽 ?섑뻾
	if (bIsInWaitingRoom)
	{
		// bAutoManageActiveCameraTarget = false濡?Super ?대???ViewTarget ?먮룞 ?꾪솚 李⑤떒
		bAutoManageActiveCameraTarget = false;

		// Super ?몄텧: ResetIgnoreInputFlags, AcknowledgePossession, PawnClientRestart ??
		// ?붿쭊 珥덇린?붾? ?뺤긽 ?섑뻾 (?몄뒪?몄뿉??Look/Move ?낅젰??臾댁떆?섎뒗 踰꾧렇 諛⑹?)
		Super::ClientRestart_Implementation(NewPawn);

		// ???곗뿉 ?湲곗떎 媛?쒖꽦 ?곸슜
		if (NewPawn)
		{
			if (ADRCharacter* DRChar = Cast<ADRCharacter>(NewPawn))
			{
				DRChar->SetWaitingRoomVisibility(true);
			}
		}

		// Super媛 ViewTarget??蹂寃쏀뻽?????덉쑝誘濡?罹먯떆??移대찓?쇰줈 利됱떆 蹂듭썝
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
	// ?湲곗떎 UI ?쒓굅
	DestroyWaitingRoomUI();

	// ?몄뀡 ?좊굹湲???硫붿씤 硫붾돱濡??대룞
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UMultiplayerSessionsSubsystem* Subsystem = GI->GetSubsystem<UMultiplayerSessionsSubsystem>())
		{
			Subsystem->LeaveServer();
		}
	}
}

// ========== ?湲곗떎 UI ==========

void ADRPlayerController::CreateWaitingRoomUI()
{
	if (!IsLocalController()) return;
	if (WaitingRoomWidget) return; // ?대? 議댁옱
	if (!WaitingRoomWidgetClass) return;

	WaitingRoomWidget = CreateWidget<UDRWaitingRoomWidget>(this, WaitingRoomWidgetClass);
	if (WaitingRoomWidget)
	{
		WaitingRoomWidget->AddToViewport();

		// ?몄뒪???щ? ?ㅼ젙 (Listen Server??濡쒖뺄 而⑦듃濡ㅻ윭 = ?몄뒪??
		bool bIsHost = HasAuthority();
		WaitingRoomWidget->SetIsHost(bIsHost);

		// LobbyState 蹂寃?援щ룆
		if (ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>())
		{
			LGS->OnLobbyStateChanged.AddDynamic(this, &ADRPlayerController::OnLobbyStateChangedForUI);
		}

		RefreshWaitingRoomUI();
	}
}

void ADRPlayerController::DestroyWaitingRoomUI()
{
	// ?몃━寃뚯씠???댁젣
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

	// 濡쒖뺄 PlayerState ?좏슚??寃??+ ?ъ떆??
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

		// ?쒕쾭 沅뚯쐞 ?щ’ ?몃뜳???ъ슜
		Info.SlotIndex = DRPS->GetWaitingRoomSlotIndex();

		// bIsLocalPlayer ?먯젙 (PlayerId 鍮꾧탳)
		Info.bIsLocalPlayer = (PS->GetPlayerId() == MyPS->GetPlayerId());

		Infos.Add(Info);
	}

	// SlotIndex 湲곗??쇰줈 ?뺣젹?섏뿬 UI???꾨떖
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

	// ?꾩젽 ?몃━ ?꾩쟾 珥덇린?????대퉴由ы떚 ?꾩씠肄?媛뺤젣 媛깆떊
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
	// ?몄뒪??寃利? Server RPC?대?濡??쒕쾭?먯꽌 ?ㅽ뻾??
	// IsLocalController()濡??몄뒪??Listen Server) ?뺤씤
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
	// ?몄뒪??寃利?
	if (!IsLocalController() || !HasAuthority()) return;
	if (!TargetPlayerState) return;

	// TargetPlayerState ??PlayerController 李얘린
	APlayerController* TargetPC = Cast<APlayerController>(TargetPlayerState->GetOwner());
	ADRPlayerController* TargetDRPC = Cast<ADRPlayerController>(TargetPC);

	ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
	if (LobbyGM && TargetDRPC)
	{
		LobbyGM->KickPlayer(this, TargetDRPC);
	}
}

