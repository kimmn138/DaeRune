// Copyright DaeRune

#include "Phase/Stage2/DRS2PuzzlePhase.h"

#include "Actor/DRCleanserPart.h"
#include "Actor/Stage2/DRS2StageDirector.h"
#include "Actor/Stage2/DRS2TeleportGate.h"
#include "Actor/Stage2/DRS2SlidePuzzle.h"
#include "Actor/Stage2/DRS2SwitchPuzzle.h"
#include "Actor/Stage2/DRS2CctvBoard.h"
#include "Actor/Stage2/DRS2Safe.h"
#include "Game/DRStageGameMode.h"
#include "Game/DRStageGameState.h"
#include "DaeRune/DRLogChannels.h"

void UDRS2PuzzlePhase::OnPhaseStart()
{
	Super::OnPhaseStart();

	if (!GameMode || !GameState) return;

	ADRS2StageDirector* Director = GetDirector();
	if (!Director) return;

	// 방1 클리어 보상으로 D1 게이트를 발광시킨다.
	// 이 처리를 방1 페이즈의 OnPhaseEnd 가 아니라 여기서 하는 이유:
	// TriggerGameOver 도 OnPhaseEnd 를 호출하므로 게임오버 시 게이트가 켜지는 부수효과를 막기 위함.
	SetGateActive(Director->Gate_Room1ToRoom2, true);

	SetupPhaseObjectiveByRow(TEXT("S2P2_Move"));

	// ===== 비밀번호 생성 및 배분 (Plan6 §14.2.6) =====
	// 반드시 한곳에서 만들어 각 액터에 주입한다. 각자 난수를 뽑으면 값이 어긋난다.
	SecretCode = {
		static_cast<uint8>(FMath::RandRange(0, 9)),
		static_cast<uint8>(FMath::RandRange(0, 9)),
		static_cast<uint8>(FMath::RandRange(0, 9))
	};

	if (ADRS2SlidePuzzle* SlidePuzzle = Director->Room2SlidePuzzle)
	{
		SlidePuzzle->SetRevealDigit(SecretCode[0]);
		SlidePuzzle->OnPuzzleSolved.AddDynamic(this, &UDRS2PuzzlePhase::HandlePuzzleSolved);
	}

	if (ADRS2SwitchPuzzle* SwitchPuzzle = Director->Room2SwitchPuzzle)
	{
		SwitchPuzzle->SetRevealDigit(SecretCode[1]);
		SwitchPuzzle->OnPuzzleSolved.AddDynamic(this, &UDRS2PuzzlePhase::HandlePuzzleSolved);
	}

	if (ADRS2CctvBoard* CctvBoard = Director->Room2CctvBoard)
	{
		// CCTV는 해결 상태가 없다. 타깃 이미지 등장 횟수가 곧 마지막 자리다.
		CctvBoard->SetTargetImageCount(SecretCode[2]);
	}

	if (ADRS2Safe* Safe = Director->Room2Safe)
	{
		Safe->SetSecretCode(SecretCode);
		Safe->OnSafeOpened.AddDynamic(this, &UDRS2PuzzlePhase::HandleSafeOpened);
	}

	if (ADRS2TeleportGate* ExitGate = Director->Gate_Room2Exit)
	{
		ExitGate->OnTeamRecalled.AddDynamic(this, &UDRS2PuzzlePhase::HandleTeamRecalled);
	}

	UE_LOG(LogDR, Log, TEXT("[S2P2] 금고 코드 생성: %d%d%d"), SecretCode[0], SecretCode[1], SecretCode[2]);

	// 진행도 분모 2 = 스크린에 숫자가 뜨는 퍼즐 수 (8퍼즐 + 스위치). CCTV는 제외한다.
	SetupPhaseObjectiveByRow(TEXT("S2P2_Puzzle"), 2);
	GameState->UpdatePhaseObjectiveProgress(0);
}

void UDRS2PuzzlePhase::HandlePuzzleSolved(AActor* /*Puzzle*/)
{
	if (!GameState) return;

	++DigitsRevealed;
	GameState->UpdatePhaseObjectiveProgress(DigitsRevealed);

	UE_LOG(LogDR, Log, TEXT("[S2P2] 비밀번호 공개 %d/2"), DigitsRevealed);
}

void UDRS2PuzzlePhase::HandleSafeOpened(AActor* SpawnedPart)
{
	if (!GameState) return;

	bSafeOpened = true;

	SetupPhaseObjectiveByRow(TEXT("S2P2_Part"));

	// 금고에서 나온 부품의 획득을 감지한다 (Plan6 §5.6)
	if (ADRCleanserPart* Part = Cast<ADRCleanserPart>(SpawnedPart))
	{
		Part->OnPartPickedUp.AddDynamic(this, &UDRS2PuzzlePhase::HandlePartPickedUp);
	}

	UE_LOG(LogDR, Log, TEXT("[S2P2] 금고 개방 - 부품 획득 대기"));
}

void UDRS2PuzzlePhase::HandlePartPickedUp(ADRCleanserPart* /*Part*/, ADRCharacter* /*Character*/)
{
	if (bPartPickedUp) return;

	bPartPickedUp = true;

	// 픽업만으로는 완료가 아니다. 부품을 들고 방2를 나가야 한다.
	SetupPhaseObjectiveByRow(TEXT("S2P2_Return"));

	UE_LOG(LogDR, Log, TEXT("[S2P2] 부품 획득 - 방1 복귀 대기"));
}

void UDRS2PuzzlePhase::HandleTeamRecalled()
{
	if (!GameMode || bCarrierExited) return;

	if (ADRS2StageDirector* Director = GetDirector())
	{
		// 방2 재입장 불가
		SetGateActive(Director->Gate_Room1ToRoom2, false);
	}

	UE_LOG(LogDR, Log, TEXT("[S2P2] 부품 소지자 퇴장 - 전원 방1 회수 완료"));

	bCarrierExited = true;
	GameMode->ValidatePhaseCompletion();
}

bool UDRS2PuzzlePhase::IsCompleted() const
{
	return bCarrierExited;
}

void UDRS2PuzzlePhase::OnPhaseEnd()
{
	if (ADRS2StageDirector* Director = GetDirector())
	{
		if (ADRS2SlidePuzzle* SlidePuzzle = Director->Room2SlidePuzzle)
		{
			SlidePuzzle->OnPuzzleSolved.RemoveDynamic(this, &UDRS2PuzzlePhase::HandlePuzzleSolved);
		}
		if (ADRS2SwitchPuzzle* SwitchPuzzle = Director->Room2SwitchPuzzle)
		{
			SwitchPuzzle->OnPuzzleSolved.RemoveDynamic(this, &UDRS2PuzzlePhase::HandlePuzzleSolved);
		}
		if (ADRS2Safe* Safe = Director->Room2Safe)
		{
			Safe->OnSafeOpened.RemoveDynamic(this, &UDRS2PuzzlePhase::HandleSafeOpened);
		}
		if (ADRS2TeleportGate* ExitGate = Director->Gate_Room2Exit)
		{
			ExitGate->OnTeamRecalled.RemoveDynamic(this, &UDRS2PuzzlePhase::HandleTeamRecalled);
		}
	}

	// 주의: 금고에서 스폰한 부품은 SpawnedEnemies 가 아니므로 Super 의 정리 대상이 아니다.
	// 방3~방4에서 계속 사용하므로 파괴해서는 안 된다.

	Super::OnPhaseEnd();
}
