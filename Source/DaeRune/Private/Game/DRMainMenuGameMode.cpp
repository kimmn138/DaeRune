// Copyright DaeRune


#include "Game/DRMainMenuGameMode.h"
#include "Game/DRGameInstance.h"
#include "UI/Loading/DRLoadingScreenSubsystem.h"
#include "UI/Loading/DRLoadingScreenWidget.h"
#include "DaeRune/DRLogChannels.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/Level.h"
#include "Engine/LevelStreaming.h"
#include "UI/Widget/DRTutorialStartWidget.h"
#include "Menu.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "TimerManager.h"

ADRMainMenuGameMode::ADRMainMenuGameMode()
{
	// 메인 메뉴에서는 기본 폰 사용하지 않음 (카메라만 사용)
	DefaultPawnClass = nullptr;
}

void ADRMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	bIsTutorialMode = !HasCompletedTutorial();

	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = UGameplayStatics::GetPlayerController(World, 0);
	if (!PC) return;

	// 입력 모드: UI Only
	PC->SetInputMode(FInputModeUIOnly());
	PC->SetShowMouseCursor(true);

	// 서브레벨 메시가 그릴 준비를 마칠 때까지 검게 덮는다 — RevealMenu 에서 페이드 인
	if (PC->PlayerCameraManager)
	{
		PC->PlayerCameraManager->SetManualCameraFade(1.f, FLinearColor::Black, false);
	}

	if (bIsTutorialMode)
	{
		// 튜토리얼 프리뷰 서브레벨 로드
		FLatentActionInfo LatentInfo;
		LatentInfo.CallbackTarget = this;
		LatentInfo.ExecutionFunction = TEXT("OnTutorialPreviewLevelLoaded");
		LatentInfo.Linkage = 0;
		LatentInfo.UUID = 1;

		UGameplayStatics::LoadStreamLevel(World, TutorialPreviewLevelName, true, false, LatentInfo);
	}
	else
	{
		// 로비 프리뷰 서브레벨 로드
		FLatentActionInfo LatentInfo;
		LatentInfo.CallbackTarget = this;
		LatentInfo.ExecutionFunction = TEXT("OnLobbyPreviewLevelLoaded");
		LatentInfo.Linkage = 0;
		LatentInfo.UUID = 2;

		UGameplayStatics::LoadStreamLevel(World, LobbyPreviewLevelName, true, false, LatentInfo);
	}
}

bool ADRMainMenuGameMode::HasCompletedTutorial() const
{
	UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	if (GI)
	{
		return GI->HasCompletedTutorial();
	}
	return false;
}

void ADRMainMenuGameMode::OnTutorialPreviewLevelLoaded()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	// 1. 튜토리얼 프리뷰 카메라 찾아서 ViewTarget 설정
	SetViewTargetByTag(PC, TutorialCameraTag);

	// 2. 메시가 모두 그릴 준비를 마치면 페이드 인 + 튜토리얼 시작 UI
	//    (보이는 정원로봇은 서브레벨에 놓인 메시다 — 예전의 캐릭터 BP 스폰은 카메라 뒤에 생겨 보이지 않았다)
	BeginRevealWhenReady(TutorialPreviewLevelName);
}

void ADRMainMenuGameMode::OnLobbyPreviewLevelLoaded()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	// 1. 로비 프리뷰 카메라 찾아서 ViewTarget 설정
	SetViewTargetByTag(PC, LobbyCameraTag);

	// 2. 메시가 모두 그릴 준비를 마치면 페이드 인 + 기존 UMenu 위젯
	BeginRevealWhenReady(LobbyPreviewLevelName);
}

void ADRMainMenuGameMode::BeginRevealWhenReady(FName LevelName)
{
	RevealLevelName = LevelName;
	RevealWaitStartTime = FPlatformTime::Seconds();
	GetWorldTimerManager().SetTimer(RevealTimerHandle, this, &ThisClass::CheckRevealReady, 0.05f, true);
}

void ADRMainMenuGameMode::CheckRevealReady()
{
	const double Waited = FPlatformTime::Seconds() - RevealWaitStartTime;
	const bool bReady = IsRevealLevelReady();
	if (!bReady && Waited < MaxRevealWaitTime)
	{
		return;
	}

	if (bReady)
	{
		UE_LOG(LogDR, Log, TEXT("[MainMenu] %s 메시 준비까지 %.2f초 — %.1f초 뒤 화면을 엽니다"), *RevealLevelName.ToString(), Waited, RevealHoldTime);
	}
	else
	{
		UE_LOG(LogDR, Warning, TEXT("[MainMenu] %s 메시가 %.1f초 안에 그릴 준비를 마치지 못해 화면을 엽니다"), *RevealLevelName.ToString(), Waited);
	}

	// 마지막 PSO 가 끝난 프레임에 프록시가 다시 만들어진다 — 첫 그리기와 조명이 자리 잡을 시간을 조금 더 준다
	if (RevealHoldTime > 0.f)
	{
		GetWorldTimerManager().SetTimer(RevealTimerHandle, this, &ThisClass::RevealMenu, RevealHoldTime, false);
	}
	else
	{
		RevealMenu();
	}
}

bool ADRMainMenuGameMode::IsRevealLevelReady() const
{
	const ULevelStreaming* Streaming = UGameplayStatics::GetStreamingLevel(this, RevealLevelName);
	const ULevel* Level = Streaming ? Streaming->GetLoadedLevel() : nullptr;
	if (!Level)
	{
		return true;
	}

	for (const AActor* Actor : Level->Actors)
	{
		if (!Actor || Actor->IsHidden())
		{
			continue;
		}

		// PSO 를 컴파일하는 동안은 프록시가 없어 그려지지 않는다 (에디터 바이너리는 프리캐싱이 꺼져 있어 항상 false)
		const TInlineComponentArray<UPrimitiveComponent*> Primitives(Actor);
		for (const UPrimitiveComponent* Primitive : Primitives)
		{
			if (Primitive->IsRegistered() && Primitive->IsVisible() && Primitive->IsPSOPrecaching())
			{
				return false;
			}
		}
	}
	return true;
}

void ADRMainMenuGameMode::RevealMenu()
{
	GetWorldTimerManager().ClearTimer(RevealTimerHandle);

	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	if (PC->PlayerCameraManager)
	{
		if (RevealFadeInTime > 0.f)
		{
			PC->PlayerCameraManager->StartCameraFade(1.f, 0.f, RevealFadeInTime, FLinearColor::Black, false, false);
		}
		else
		{
			PC->PlayerCameraManager->StopCameraFade();
		}
	}

	if (bIsTutorialMode)
	{
		ShowTutorialStartUI(PC);
	}
	else
	{
		ShowLobbyMenuUI(PC);
	}
}

void ADRMainMenuGameMode::SetViewTargetByTag(APlayerController* PC, const FName& Tag)
{
	if (!PC) return;

	UWorld* World = GetWorld();
	if (!World) return;

	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(Tag))
		{
			PC->SetViewTargetWithBlend(*It, 0.f);
			return;
		}
	}

	// 태그로 찾지 못하면 첫 번째 CameraActor를 사용 (폴백)
	for (TActorIterator<ACameraActor> It(World); It; ++It)
	{
		PC->SetViewTargetWithBlend(*It, 0.f);
		return;
	}
}

void ADRMainMenuGameMode::ShowTutorialStartUI(APlayerController* PC)
{
	if (!PC || !TutorialStartWidgetClass) return;

	UDRTutorialStartWidget* Widget = CreateWidget<UDRTutorialStartWidget>(PC, TutorialStartWidgetClass);
	if (Widget)
	{
		Widget->AddToViewport();
		CurrentMenuWidget = Widget;
	}
}

void ADRMainMenuGameMode::ShowLobbyMenuUI(APlayerController* PC)
{
	if (!PC || !LobbyMenuWidgetClass) return;

	UUserWidget* Widget = CreateWidget<UUserWidget>(PC, LobbyMenuWidgetClass);
	if (Widget)
	{
		// UMenu인 경우 MenuSetup 호출 (세션 연결 설정)
		if (UMenu* Menu = Cast<UMenu>(Widget))
		{
			Menu->MenuSetup(MaxPublicConnections, LobbyMapPath);

			// 로딩 화면은 메뉴가 입력을 검증하고 요청을 보낼 때 띄우고, 실패하면 닫는다
			// (버튼 클릭에 걸면 룸코드가 비어도 뜨고 방을 못 찾아도 닫히지 않는다)
			Menu->OnHostRequestStarted.AddUniqueDynamic(this, &ThisClass::HandleHostRequestStarted);
			Menu->OnJoinRequestStarted.AddUniqueDynamic(this, &ThisClass::HandleJoinRequestStarted);
			Menu->OnSessionRequestFailed.AddUniqueDynamic(this, &ThisClass::HandleSessionRequestFailed);
		}
		else
		{
			Widget->AddToViewport();
		}
		CurrentMenuWidget = Widget;
	}
}

UDRLoadingScreenSubsystem* ADRMainMenuGameMode::GetLoadingScreenSubsystem() const
{
	const UGameInstance* GI = GetGameInstance();
	return GI ? GI->GetSubsystem<UDRLoadingScreenSubsystem>() : nullptr;
}

void ADRMainMenuGameMode::HandleHostRequestStarted()
{
	if (UDRLoadingScreenSubsystem* LoadingSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingSubsystem->BeginLoadingScreen(CreateRoomLoadingWidgetClass);
	}
}

void ADRMainMenuGameMode::HandleJoinRequestStarted()
{
	if (UDRLoadingScreenSubsystem* LoadingSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingSubsystem->BeginLoadingScreen(JoinRoomLoadingWidgetClass);
	}
}

void ADRMainMenuGameMode::HandleSessionRequestFailed()
{
	if (UDRLoadingScreenSubsystem* LoadingSubsystem = GetLoadingScreenSubsystem())
	{
		LoadingSubsystem->CancelLoadingScreen();
	}
}

void ADRMainMenuGameMode::StartTutorial()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	// 1. UI 제거
	if (CurrentMenuWidget)
	{
		CurrentMenuWidget->RemoveFromParent();
		CurrentMenuWidget = nullptr;
	}

	// 2. 카메라 블렌드 없이 즉시 로딩 화면 등장 + 맵 이동
	//    (로딩 화면이 전체를 덮으므로 블렌드 연출은 보이지 않아 생략)
	OnTutorialTransitionFinished();
}

void ADRMainMenuGameMode::OnTutorialTransitionFinished()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 로딩 화면 위젯이 지정되어 있으면: 등장 애니 → 로드 → (도착 맵에서) 퇴장 애니
	if (TutorialLoadingWidgetClass)
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UDRLoadingScreenSubsystem* LoadingSubsystem = GI->GetSubsystem<UDRLoadingScreenSubsystem>())
			{
				LoadingSubsystem->OpenLevelWithLoadingScreen(TutorialLoadingWidgetClass, FName(*TutorialMapName), FString());
				return;
			}
		}
	}

	// 폴백: 로딩 화면 없이 바로 이동
	UGameplayStatics::OpenLevel(World, FName(*TutorialMapName));
}

void ADRMainMenuGameMode::SkipTutorialAndReloadMenu()
{
	UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	if (!GI) return;

	GI->SetTutorialCompleted();
	ReloadMenu();   // 로비 프리뷰로 진입
}

void ADRMainMenuGameMode::ResetTutorialAndReloadMenu()
{
	UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	if (!GI) return;

	GI->ResetTutorialProgress();
	ReloadMenu();
}

void ADRMainMenuGameMode::ReloadMenu()
{
	if (CurrentMenuWidget)
	{
		CurrentMenuWidget->RemoveFromParent();
		CurrentMenuWidget = nullptr;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	const FName CurrentMapName(*UGameplayStatics::GetCurrentLevelName(World, true));
	UGameplayStatics::OpenLevel(World, CurrentMapName);
}
