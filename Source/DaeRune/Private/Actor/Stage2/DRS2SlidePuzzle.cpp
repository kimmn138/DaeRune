// Copyright DaeRune

#include "Actor/Stage2/DRS2SlidePuzzle.h"

#include "Actor/Stage2/DRS2CodeScreen.h"
#include "Character/DRCharacter.h"
#include "Net/UnrealNetwork.h"
#include "Player/DRPlayerController.h"
#include "DaeRune/DRLogChannels.h"

ADRS2SlidePuzzle::ADRS2SlidePuzzle()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
}

void ADRS2SlidePuzzle::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2SlidePuzzle, RevealedDigit);
	DOREPLIFETIME(ADRS2SlidePuzzle, bSolved);
}

void ADRS2SlidePuzzle::BeginPlay()
{
	Super::BeginPlay();

	// 해결 전에는 '미공개'로 표시
	if (CodeScreen)
	{
		CodeScreen->SetDigits({ -1 });
	}
}

void ADRS2SlidePuzzle::SetRevealDigit(int32 InDigit)
{
	if (!HasAuthority()) return;

	SecretDigit = InDigit;
}

void ADRS2SlidePuzzle::RequestOpenUI(ADRCharacter* Character)
{
	if (!HasAuthority() || !IsValid(Character)) return;

	// 이미 풀린 퍼즐은 다시 열지 않는다
	if (bSolved) return;

	ADRPlayerController* PlayerController = Cast<ADRPlayerController>(Character->GetController());
	if (!PlayerController) return;

	// 요청한 플레이어에게만 UI를 띄운다.
	// 실제 위젯 생성은 BP(HUD)가 OnSlidePuzzleUIRequested 델리게이트를 받아 처리한다.
	PlayerController->Client_OpenSlidePuzzleUI(this);
}

void ADRS2SlidePuzzle::NotifySolved()
{
	if (!HasAuthority() || bSolved) return;

	bSolved = true;

	// 이 시점에만 정답 숫자가 복제된다 (해결 전에는 클라에 정답 정보가 없다 - Plan6 §14.2.6)
	RevealedDigit = SecretDigit;
	OnRep_RevealedDigit();

	UE_LOG(LogDR, Log, TEXT("[S2Puzzle] 8퍼즐 해결 - 첫 자리 공개"));

	OnPuzzleSolved.Broadcast(this);
}

void ADRS2SlidePuzzle::OnRep_RevealedDigit()
{
	if (CodeScreen)
	{
		CodeScreen->SetDigits({ RevealedDigit });
	}

	if (RevealedDigit >= 0)
	{
		OnPuzzleSolvedVisual(RevealedDigit);
	}
}
