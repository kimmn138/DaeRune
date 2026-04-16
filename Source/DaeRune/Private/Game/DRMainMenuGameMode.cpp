// Copyright DaeRune


#include "Game/DRMainMenuGameMode.h"
#include "Game/DRGameInstance.h"
#include "Camera/CameraActor.h"
#include "Character/DRCharacter.h"
#include "UI/Widget/DRTutorialStartWidget.h"
#include "Menu.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LevelStreamingDynamic.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/TargetPoint.h"

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

	// 2. 디스플레이 캐릭터 스폰
	SpawnTutorialDisplayCharacter();

	// 3. 튜토리얼 시작 UI 표시
	ShowTutorialStartUI(PC);
}

void ADRMainMenuGameMode::OnLobbyPreviewLevelLoaded()
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (!PC) return;

	// 1. 로비 프리뷰 카메라 찾아서 ViewTarget 설정
	SetViewTargetByTag(PC, LobbyCameraTag);

	// 2. 기존 UMenu 위젯 표시
	ShowLobbyMenuUI(PC);
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

void ADRMainMenuGameMode::SpawnTutorialDisplayCharacter()
{
	if (!TutorialCharacterClass) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 스폰 위치 탐색 (TutorialCharacterSpawnPoint 태그)
	FVector SpawnLocation = FVector::ZeroVector;
	FRotator SpawnRotation = FRotator::ZeroRotator;

	for (TActorIterator<ATargetPoint> It(World); It; ++It)
	{
		if (It->ActorHasTag(TutorialCharacterSpawnTag))
		{
			SpawnLocation = It->GetActorLocation();
			SpawnRotation = It->GetActorRotation();
			break;
		}
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	DisplayCharacter = World->SpawnActor<ADRCharacter>(
		TutorialCharacterClass,
		FTransform(SpawnRotation, SpawnLocation),
		SpawnParams
	);

	if (DisplayCharacter)
	{
		// 이동 비활성화 (디스플레이용)
		if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(DisplayCharacter->GetMovementComponent()))
		{
			CMC->StopMovementImmediately();
			CMC->DisableMovement();
		}

		// 리플리케이트 이동 비활성화
		DisplayCharacter->SetReplicateMovement(false);
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
			Menu->MenuSetup(4, TEXT("/Game/Maps/LobbyMap"));
		}
		else
		{
			Widget->AddToViewport();
		}
		CurrentMenuWidget = Widget;
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

	// 2. 디스플레이 캐릭터가 있으면 카메라를 캐릭터 쪽으로 블렌드
	if (DisplayCharacter)
	{
		// 캐릭터 이동 활성화 (Possess 준비)
		if (UCharacterMovementComponent* CMC = Cast<UCharacterMovementComponent>(DisplayCharacter->GetMovementComponent()))
		{
			CMC->SetMovementMode(MOVE_Walking);
		}

		PC->Possess(DisplayCharacter);
		PC->SetViewTargetWithBlend(DisplayCharacter, CameraBlendTime);
	}

	// 3. 블렌드 완료 후 튜토리얼 맵으로 이동
	GetWorldTimerManager().SetTimer(
		TutorialTransitionTimerHandle,
		this,
		&ADRMainMenuGameMode::OnTutorialTransitionFinished,
		CameraBlendTime,
		false
	);
}

void ADRMainMenuGameMode::OnTutorialTransitionFinished()
{
	UWorld* World = GetWorld();
	if (!World) return;

	// 튜토리얼 맵으로 이동
	UGameplayStatics::OpenLevel(World, FName(*TutorialMapName));
}
