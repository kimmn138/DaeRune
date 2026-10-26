// Copyright DaeRune


#include "Game/DRGameModeBase.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Character/DRCharacterBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"

ADRGameModeBase::ADRGameModeBase()
{
	WipeoutDelayTime = 3.0f;
	bIsWipeoutInProgress = false;
}

void ADRGameModeBase::Logout(AController* Exiting)
{
	// 접속 종료를 현재 페이즈에 먼저 통지한다 (Plan6 §5.10).
	// Super 호출 후에는 PlayerState 가 정리될 수 있으므로 순서가 중요하다.
	if (HasAuthority() && Exiting && Exiting->PlayerState)
	{
		NotifyPhasePlayerLeft(Exiting->PlayerState);
	}

	Super::Logout(Exiting);
}

void ADRGameModeBase::OnPlayerDied(APlayerState* DeadPlayer)
{
	if (!HasAuthority()) return;

	if (!DeadPlayer) return;

	// 현재 페이즈에 사망 통지 (Plan6 §14.3.5-A)
	// 전멸 처리와 무관하게 항상 전달한다.
	NotifyPhasePlayerDied(DeadPlayer);

	// 이미 전멸 처리 중이면 무시
	if (bIsWipeoutInProgress) return;

	// 전멸 체크
	if (CheckTeamWipeout())
	{
		bIsWipeoutInProgress = true;

		// 모든 플레이어에게 게임 오버 UI 표시
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
			{
				PC->Client_ShowGameOverUI();
			}
		}

		// 일정 시간 후 전멸 처리
		GetWorldTimerManager().SetTimer(
			WipeoutTimerHandle,
			this,
			&ADRGameModeBase::HandleWipeout,
			WipeoutDelayTime,
			false
		);
	}
}

APawn* ADRGameModeBase::RespawnPlayerPawn(AController* Controller, TSubclassOf<APawn> PawnClass, const FTransform& SpawnTransform)
{
	if (!HasAuthority() || !Controller || !PawnClass) return nullptr;

	if (APawn* OldPawn = Controller->GetPawn())
	{
		Controller->UnPossess();
		OldPawn->Destroy();
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	APawn* NewPawn = GetWorld()->SpawnActor<APawn>(PawnClass, SpawnTransform, SpawnParams);
	if (!NewPawn) return nullptr;

	Controller->Possess(NewPawn);

	// 소유 클라의 시선도 스폰 방향으로 맞춘다 (엔진 FinishRestartPlayer 와 같은 처리)
	Controller->ClientSetRotation(NewPawn->GetActorRotation(), true);

	return NewPawn;
}

bool ADRGameModeBase::RevivePlayer(APlayerController* PC, const FTransform& At, float HealthRatio, float WaterRatio)
{
	if (!HasAuthority() || !PC || !PC->PlayerState || PC->PlayerState->IsOnlyASpectator()) return false;

	// 사망 직후(시체 정리 전) — 같은 폰을 그 자리에서 되살린다 (관전 예약 취소 · 관전 해제도 Revive 가 한다)
	if (ADRCharacterBase* Character = Cast<ADRCharacterBase>(PC->GetPawn()))
	{
		if (!ICombatInterface::Execute_IsDead(Character)) return false;

		Character->SetActorRotation(At.GetRotation());
		Character->Revive(At.GetLocation(), HealthRatio, WaterRatio);
		return true;
	}

	// 시체가 이미 정리됐다 — 같은 클래스로 새 폰을 만든다.
	// ★오염을 먼저 푼다★ — 빙의가 기본 속성을 다시 채울 때 오염(최대 체력 100) 기준이 남지 않게
	if (const ADRPlayerState* DRPS = Cast<ADRPlayerState>(PC->PlayerState))
	{
		if (UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(DRPS->GetAttributeSet()))
		{
			PlayerAS->ExitCorruptedState();
		}
	}

	ADRCharacterBase* NewCharacter = Cast<ADRCharacterBase>(
		RespawnPlayerPawn(PC, GetDefaultPawnClassForController(PC), At));
	if (!NewCharacter) return false;

	// 빙의가 속성을 기본값(가득)으로 채웠다 — 부활 비율로 맞춘다
	NewCharacter->SetReviveAttributes(HealthRatio, WaterRatio);

	if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
	{
		DRPC->CancelPendingSpectate();
		DRPC->ClientStopSpectating();
	}
	return true;
}

bool ADRGameModeBase::CheckTeamWipeout()
{
	if (!HasAuthority()) return false;

	AGameStateBase* GameStateBase = GetGameState<AGameStateBase>();
	if (!GameStateBase) return false;

	int32 AlivePlayerCount = 0;
	int32 TotalPlayerCount = 0;

	// 모든 PlayerState 순회	
	for (APlayerState* PlayerState : GameStateBase->PlayerArray)
	{
		if (!PlayerState) continue;

		TotalPlayerCount++;

		// Character 가져오기
		ADRCharacterBase* Character = Cast<ADRCharacterBase>(PlayerState->GetPawn());
		if (!Character) continue;

		// CombatInterface로 죽음 체크
		if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Character))
		{
			if (!CombatInterface->Execute_IsDead(Character)) 
			{
				AlivePlayerCount++;
			}
		}
	}

	// 플레이어가 1명 이상 있고, 생존자가 0명이면 전멸
	return (TotalPlayerCount > 0) && (AlivePlayerCount == 0);
}

void ADRGameModeBase::BeginPlay()
{
	Super::BeginPlay();
}

void ADRGameModeBase::HandleWipeout()
{
	if (!HasAuthority()) return;

	// 플래그 리셋
	bIsWipeoutInProgress = false;
}

void ADRGameModeBase::PrepareForTravel()
{
	if (!HasAuthority()) return;

	// 모든 클라이언트에게 설정창 닫기 및 오디오 정리 요청
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
		{
			PC->ClientCloseSettingsMenu();
			PC->ClientStopAllAudio();
		}
	}
}

