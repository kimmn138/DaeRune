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
	//
	// ★3번째 자리만 예외다 (2026-09-16). CCTV 는 "타깃 캐릭터가 사진에 몇 명 나오는가"가 답인데,
	//   그 수는 그림에 이미 그려져 있어 서버가 정할 수 없다. 그래서 CCTV 보드가 후보 중 하나를
	//   뽑아 자릿수를 **결정하고**, 여기서 그것을 읽어 온다 (§14.2.4).
	ADRS2CctvBoard* CctvBoard = Director->Room2CctvBoard;

	int32 CctvDigit = INDEX_NONE;
	if (CctvBoard)
	{
		CctvBoard->ChooseTarget();          // 후보 선정 + 화면 시퀀스 재생 시작
		CctvDigit = CctvBoard->GetSecretDigit();
	}

	if (CctvDigit < 0 || CctvDigit > 9)
	{
		// 보드 미배선이거나 후보가 비었을 때. 금고는 열려야 하므로 난수로 대체한다.
		// (이 경우 CCTV 를 아무리 세어도 답이 안 나오므로 개발 중에만 벌어져야 한다)
		UE_LOG(LogDR, Error,
			TEXT("[S2P2] CCTV 보드에서 3번째 자리를 얻지 못했습니다(%d). ")
			TEXT("Director 의 Room2CctvBoard 배선과 TargetCandidates 를 확인하세요. 난수로 대체합니다."),
			CctvDigit);
		CctvDigit = FMath::RandRange(0, 9);
	}

	SecretCode = {
		static_cast<uint8>(FMath::RandRange(0, 9)),
		static_cast<uint8>(FMath::RandRange(0, 9)),
		static_cast<uint8>(CctvDigit)
	};

#if !UE_BUILD_SHIPPING
	// ★개발용 임시 로그. 퍼즐을 다 풀지 않고도 금고를 테스트할 수 있게 한다.
	//   패키징(Shipping)에서는 컴파일되지 않으므로 지우지 않아도 안전하다.
	UE_LOG(LogDR, Warning, TEXT("[S2P2] ★금고 비밀번호 = %d %d %d   (1번 8퍼즐 / 2번 스위치 / 3번 CCTV)"),
		static_cast<int32>(SecretCode[0]),
		static_cast<int32>(SecretCode[1]),
		static_cast<int32>(SecretCode[2]));
#endif

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

	// ★D2(방1<->방3) 개방. 닫힌 채로 시작해 여기서 열린다 (Plan6 §14.2.7).
	//   부품을 얻기 전에는 방3으로 갈 수 없고, 열린 뒤 전원 방3 입장 시 다시 봉쇄된다.
	if (ADRS2StageDirector* Director = GetDirector())
	{
		SetBlockerBlocked(Director->Blocker_Room1ToRoom3, false);
	}

	// 픽업만으로는 완료가 아니다. 부품을 들고 방2를 나가야 한다.
	SetupPhaseObjectiveByRow(TEXT("S2P2_Return"));

	UE_LOG(LogDR, Log, TEXT("[S2P2] 부품 획득 - D2 개방, 방1 복귀 대기"));
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
