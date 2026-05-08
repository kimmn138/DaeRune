// Copyright DaeRune


#include "Game/DRTutorialGameMode.h"
#include "Game/DRGameInstance.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Character/DRCharacter.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Kismet/GameplayStatics.h"

ADRTutorialGameMode::ADRTutorialGameMode()
{
	WipeoutDelayTime = 2.0f;
}

void ADRTutorialGameMode::BeginPlay()
{
	Super::BeginPlay();
}

UClass* ADRTutorialGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
	if (APlayerController* PC = Cast<APlayerController>(InController))
	{
		if (ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
		{
			EPlayerCharacterClass SelectedClass = PS->GetSelectedPlayerClass();

			if (UPlayerCharacterClassInfo* ClassInfo = UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(this))
			{
				TSubclassOf<ADRCharacter>* BPClassPtr = ClassInfo->CharacterBPClasses.Find(SelectedClass);
				if (BPClassPtr && *BPClassPtr)
				{
					return *BPClassPtr;
				}
			}
		}
	}

	return Super::GetDefaultPawnClassForController_Implementation(InController);
}

void ADRTutorialGameMode::TriggerTutorialComplete()
{
	if (!HasAuthority()) return;
	if (bIsWipeoutInProgress) return;

	bIsWipeoutInProgress = true;

	// SaveGame에 튜토리얼 완료 플래그 저장
	UDRGameInstance* GI = Cast<UDRGameInstance>(GetGameInstance());
	if (GI)
	{
		GI->SetTutorialCompleted();
	}

	// 모든 플레이어에게 완료 알림
	NotifyAllPlayersTutorialComplete();

	// 딜레이 후 메인 메뉴로 복귀
	GetWorldTimerManager().SetTimer(
		TutorialCompleteTimerHandle,
		this,
		&ADRTutorialGameMode::ReturnToMainMenu,
		TutorialCompleteDelay,
		false
	);
}

void ADRTutorialGameMode::HandleWipeout()
{
	if (!HasAuthority()) return;

	// 튜토리얼 실패 시 메인 메뉴로 복귀 (SaveGame 저장하지 않음)
	ReturnToMainMenu();
}

void ADRTutorialGameMode::ReturnToMainMenu()
{
	if (!HasAuthority()) return;

	PrepareForTravel();

	UWorld* World = GetWorld();
	if (World)
	{
		UGameplayStatics::OpenLevel(World, FName(*MainMenuMapName));
	}

	bIsWipeoutInProgress = false;
}

void ADRTutorialGameMode::NotifyAllPlayersTutorialComplete()
{
	if (!HasAuthority()) return;

	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
		{
			PC->Client_ShowGameClearUI();
		}
	}
}
