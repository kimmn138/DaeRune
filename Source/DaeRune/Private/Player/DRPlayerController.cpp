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
	NearbyPart = nullptr;
	LineTraceTimer = 0.f;
}

void ADRPlayerController::CorruptedStateChanged(bool bIsStateChanged)
{
	bIsCorrupted = bIsStateChanged;
}

void ADRPlayerController::ShowDamageNumber_Implementation(float DamageAmount, ACharacter* TargetCharacter)
{
	// 占쏙옙占쏙옙 占쏙옙트占싼뤄옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쌔쏙옙트 표占쏙옙
	if (IsValid(TargetCharacter) && DamageTextComponentClass && IsLocalController())
	{
		// 占쏙옙占쏙옙占쏙옙 占쌔쏙옙트 占쏙옙占쏙옙占쏙옙트 占쏙옙占쏙옙 占쏙옙 占쏙옙占쏙옙
		UDamageTextComponent* DamageText = NewObject<UDamageTextComponent>(TargetCharacter, DamageTextComponentClass);
		DamageText->RegisterComponent();

		// 타占쏙옙 캐占쏙옙占싶울옙 占싹쏙옙占쏙옙占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙 占싻몌옙 (占쏙옙占쏙옙 占쏙옙치 占쏙옙占쏙옙)
		DamageText->AttachToComponent(TargetCharacter->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DamageText->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);

		// 占쏙옙占쏙옙占쏙옙 占쏙옙치 占쏙옙占쏙옙 占쏙옙 占쌍니몌옙占싱쇽옙 占쏙옙占쏙옙
		DamageText->SetDamageText(DamageAmount);
	}
}

void ADRPlayerController::SetPartDetectionEnabled(bool bEnabled, ADRCleanserPart* Part)
{
	if (bEnabled)
	{
		// 占쏙옙占쏙옙트占쏙옙占싱쏙옙 활占쏙옙화
		bPartDetectionEnabled = true;
		NearbyPart = Part;
		LineTraceTimer = 0.f;
	}
	else
	{
		// 占쌔댐옙 占쏙옙품占쏙옙 占쏙옙占쏙옙 占쏙옙처 占쏙옙품占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙활占쏙옙화
		if (NearbyPart == Part)
		{
			bPartDetectionEnabled = false;
			NearbyPart = nullptr;
			
			// 占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙 占쏙옙품占쏙옙 占쏙옙占쏙옙占쏙옙 UI 占쏙옙占쏙옙 占싯몌옙
			if (CurrentDetectedPart)
			{
				ServerNotifyLineTraceLost(CurrentDetectedPart);
				CurrentDetectedPart = nullptr;
			}
		}
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
	UCameraComponent* Camera = DRCharacter->FindComponentByClass<UCameraComponent>();
	if (!Camera) return nullptr;

	// 占쏙옙占쏙옙트占쏙옙占싱쏙옙 占쏙옙占쏙옙/占쏙옙 占쏙옙치 占쏙옙占?
	FVector Start = Camera->GetComponentLocation();
	FVector End = Start + Camera->GetForwardVector() * LineTraceDistance;

	// 占쏙옙占쏙옙트占쏙옙占싱쏙옙 占쏙옙占쏙옙
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(DRCharacter);

	bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		Start,
		End,
		ECC_Visibility,
		QueryParams
	);

	// 占쏙옙품占쏙옙 占쏙옙트占쌩댐옙占쏙옙 확占쏙옙
	if (bHit)
	{
		ADRCleanserPart* HitPart = Cast<ADRCleanserPart>(HitResult.GetActor());
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

void ADRPlayerController::ServerNotifyLineTraceDetected_Implementation(ADRCleanserPart* Part)
{
	if (!Part) return;

	// ?쒕쾭?먯꽌 泥섎━ (UI 媛깆떊? 硫?곗틦?ㅽ듃濡?
	Part->MulticastShowInteractionUI(this, true);
}

void ADRPlayerController::ServerNotifyLineTraceLost_Implementation(ADRCleanserPart* Part)
{
	if (!Part) return;

	Part->MulticastShowInteractionUI(this, false);
}

void ADRPlayerController::SetSiteDetectionEnabled(bool bEnabled, ADRCleanserSite* Site)
{
	if (bEnabled)
	{
		bSiteDetectionEnabled = true;
		NearbySite = Site;
	}
	else
	{
		// 진입한 사이트가 같을 때만 비활성화 (다른 사이트 진입 우선)
		if (NearbySite == Site)
		{
			bSiteDetectionEnabled = false;
			NearbySite = nullptr;

			// 현재 라인트레이스로 잡혀있던 사이트 UI 제거
			if (CurrentOverlappedSite)
			{
				ServerNotifySiteLost(CurrentOverlappedSite);
				CurrentOverlappedSite = nullptr;
			}
		}
	}
}

ADRCleanserSite* ADRPlayerController::FindSiteByLineTrace()
{
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return nullptr;

	// 부품을 들고 있어야만 사이트 감지
	if (!DRCharacter->IsCarryingPart()) return nullptr;

	UCameraComponent* Camera = DRCharacter->FindComponentByClass<UCameraComponent>();
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
		// 박스 오버랩 중인 사이트와 동일한지 확인 (다른 사이트가 시점에 잡히는 케이스 차단)
		if (HitSite && HitSite == NearbySite)
		{
			return HitSite;
		}
	}

	return nullptr;
}

void ADRPlayerController::ServerNotifySiteDetected_Implementation(ADRCleanserSite* Site)
{
	if (!Site) return;
	Site->MulticastShowInteractionUI(this, true);
}

void ADRPlayerController::ServerNotifySiteLost_Implementation(ADRCleanserSite* Site)
{
	if (!Site) return;
	Site->MulticastShowInteractionUI(this, false);
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
void ADRPlayerController::ServerRequestInstallPartToSite_Implementation(ADRCleanserSite* Site)
{
	if (!Site) return;
	// 罹먮┃??媛?몄삤湲?
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// ?대젋? ?ъ씠?몄뿉 遺???ㅼ튂
	Site->InstallPart(DRCharacter);
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
		// Pawn???꾩쭅 ?놁쑝硫??쒕젅?????ъ떆??
if (UWorld* World = GetWorld())
		{
			FTimerHandle RetryTimer;
			World->GetTimerManager().SetTimer(
				RetryTimer,
				[WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
				{
					if (ADRPlayerController* PC = WeakThis.Get())
					{
						// ?湲곗떎?대㈃ ViewTarget 蹂듭썝 ?ㅽ궢
						if (PC->bIsInWaitingRoom) return;

						if (APawn* MyPawn = PC->GetPawn())
						{
							PC->SetViewTarget(MyPawn);
}
						PC->RestoreDefaultInputMode();
					}
				},
				0.5f,
				false
			);
		}
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
	ServerCheatSkipToNextPhase();
}

void ADRPlayerController::BeginPlay()
{
	Super::BeginPlay();

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
			}
		}
	}

	RestoreDefaultInputMode();
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
			ADRCleanserPart* DetectedPart = FindPartByLineTrace();

			if (DetectedPart != CurrentDetectedPart)
			{
				if (CurrentDetectedPart)
				{
					ServerNotifyLineTraceLost(CurrentDetectedPart);
				}

				CurrentDetectedPart = DetectedPart;

				if (CurrentDetectedPart)
				{
					ServerNotifyLineTraceDetected(CurrentDetectedPart);
				}
			}
		}

		// 사이트 라인트레이스
		if (bSiteDetectionEnabled)
		{
			ADRCleanserSite* DetectedSite = FindSiteByLineTrace();

			if (DetectedSite != CurrentOverlappedSite)
			{
				if (CurrentOverlappedSite)
				{
					ServerNotifySiteLost(CurrentOverlappedSite);
				}

				CurrentOverlappedSite = DetectedSite;

				if (CurrentOverlappedSite)
				{
					ServerNotifySiteDetected(CurrentOverlappedSite);
				}
			}
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
		// Pawn???꾩쭅 ?놁쑝硫??먭린 ?먯떊?쇰줈 ?ㅼ젙 ???쒕젅???ъ떆??
		SetViewTarget(this);

		FTimerHandle ViewTargetRetryTimer;
		World->GetTimerManager().SetTimer(
			ViewTargetRetryTimer,
			[WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
			{
				if (ADRPlayerController* PC = WeakThis.Get())
				{
					// ?湲곗떎?대㈃ ViewTarget 蹂듭썝 ?ㅽ궢
					if (PC->bIsInWaitingRoom) return;

					if (APawn* MyPawn = PC->GetPawn())
					{
						PC->SetViewTarget(MyPawn);
}
				}
			},
			0.5f,
			false
		);
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
		[this]()
		{
			if (IsValid(this) && bIsSpectating)
			{
				SpectateNextPlayer();
			}
		},
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
	float Sensitivity = 1.0f;
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UDRSettingsManager* Manager = GI->GetSubsystem<UDRSettingsManager>())
		{
			if (UDRGameUserSettings* Settings = Manager->GetSettings())
			{
				Sensitivity = Settings->MouseSensitivity;
			}
		}
	}

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
		ServerRequestPickupPart(CurrentDetectedPart);
		return;
	}
	
	// 遺?덉쓣 ?ㅺ퀬 ?덇퀬 ?대젋? ?ъ씠???ㅻ쾭??以묒씠硫??ㅼ튂
	if (DRCharacter->IsCarryingPart())
	{
		// ?대젋? ?ъ씠??踰붿쐞 ?덉씠硫??ㅼ튂
		if (CurrentOverlappedSite)
		{
			ServerRequestInstallPartToSite(CurrentOverlappedSite);
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

bool ADRPlayerController::IsInMainMenu() const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	FString CurrentLevelName = World->GetMapName();
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	return CurrentLevelName.Contains(TEXT("MainMenu"));
}

bool ADRPlayerController::IsInLobby() const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	FString CurrentLevelName = World->GetMapName();
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	return CurrentLevelName.Contains(TEXT("Lobby"));
}

bool ADRPlayerController::IsInGameLevel() const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	FString CurrentLevelName = World->GetMapName();
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	return CurrentLevelName.Contains(TEXT("Stage1"));
}

bool ADRPlayerController::IsInTutorial() const
{
	UWorld* World = GetWorld();
	if (!World) return false;

	FString CurrentLevelName = World->GetMapName();
	CurrentLevelName.RemoveFromStart(World->StreamingLevelsPrefix);

	return CurrentLevelName.Contains(TEXT("Tutorial"));
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

UDRAbilitySystemComponent* ADRPlayerController::GetASC()
{
	return Cast<UDRAbilitySystemComponent>(UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>()));
}

bool ADRPlayerController::IsAbilityInputBlocked(const FGameplayTag& InputTag) const
{
	UDRAbilitySystemComponent* ASC = const_cast<ADRPlayerController*>(this)->GetASC();
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
}

void ADRPlayerController::ServerRequestPickupPart_Implementation(ADRCleanserPart* Part)
{
	if (!Part) return;

	// 캐占쏙옙占쏙옙 占쏙옙占쏙옙占쏙옙占쏙옙
	ADRCharacter* DRCharacter = GetPawn<ADRCharacter>();
	if (!DRCharacter) return;

	// 占쏙옙품 획占쏙옙 占시듸옙
	DRCharacter->PickupPart(Part);
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
	constexpr int32 ClassCount = 2; // GardenRobot, VendingMachineRobot

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
		// Pawn???꾩쭅 由ы뵆由ъ??댄듃?섏? ?딆쓬 ??0.1珥?媛꾧꺽?쇰줈 ?ъ떆??
		int32 MaxRetries = 50; // 5珥??쒗븳
		GetWorldTimerManager().SetTimer(
			CameraTransitionRetryHandle,
			[this, MaxRetries, RetryCount = 0]() mutable
			{
				if (!IsValid(this)) return;

				if (++RetryCount > MaxRetries)
				{
GetWorldTimerManager().ClearTimer(CameraTransitionRetryHandle);
					// 理쒖냼??InputMode ?꾪솚?쇰줈 ?꾩쟾 怨좎갑 諛⑹?
					bAutoManageActiveCameraTarget = true;
					SetInputMode(FInputModeGameOnly());
					SetShowMouseCursor(false);
					InitOverlayForFreeRoam();
					return;
				}

				APawn* Pawn = GetPawn();
				if (!Pawn) return; // ?꾩쭅 ?놁쑝硫??ㅼ쓬 諛섎났?먯꽌 ?ъ떆??

				// Pawn ?꾩갑 ?꾨즺 ????대㉧ 以묒? + 移대찓???꾪솚 ?ㅽ뻾
				GetWorldTimerManager().ClearTimer(CameraTransitionRetryHandle);
				ExecuteCameraTransitionToCharacter();
			},
			0.1f,
			true
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
		[this]()
		{
			if (!IsValid(this)) return;

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
		},
		1.5f,
		false
	);
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
	FTimerHandle AbilityIconTimerHandle;
	GetWorldTimerManager().SetTimer(
		AbilityIconTimerHandle,
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
		},
		0.1f,
		false
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

