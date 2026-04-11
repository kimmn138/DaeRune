// Copyright DaeRune


#include "Game/DRMainMenuGameMode.h"
#include "Camera/CameraActor.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"

ADRMainMenuGameMode::ADRMainMenuGameMode()
{
	// 메인 메뉴에서는 기본 폰 사용하지 않음 (카메라만 사용)
	DefaultPawnClass = nullptr;
}

void ADRMainMenuGameMode::BeginPlay()
{
	Super::BeginPlay();

	// 레벨에 배치된 CameraActor를 찾아 ViewTarget으로 설정
	APlayerController* PC = UGameplayStatics::GetPlayerController(GetWorld(), 0);
	if (PC)
	{
		for (TActorIterator<ACameraActor> It(GetWorld()); It; ++It)
		{
			PC->SetViewTargetWithBlend(*It, 0.f);
			break;
		}
	}
}

bool ADRMainMenuGameMode::HasCompletedTutorial() const
{
	// TODO: SaveGame에서 튜토리얼 완료 플래그 읽기
	// 현재는 항상 true (로비 배경 사용)
	return true;
}
