// Copyright DaeRune


#include "AI/DRAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Character/DRCharacter.h"
#include "Character/DREnemy.h"
#include "Engine/World.h"
#include "TimerManager.h"

ADRAIController::ADRAIController()
{
	// 블랙보드 컴포넌트 생성 - AI 상태 데이터 저장소
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>("BlackboardComponent");
	check(Blackboard);
	// 비헤이비어 트리 컴포넌트 생성 - AI 행동 로직 실행
	BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>("BehaviorTreeComponent");
	check(BehaviorTreeComponent);

	// AI Perception 컴포넌트 생성
	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>("AIPerceptionComponent");
	SetPerceptionComponent(*AIPerceptionComponent);

	// 시야 감지 설정
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>("SightConfig");
	SightConfig->SightRadius = 2100.f;  // 시야 거리
	SightConfig->LoseSightRadius = SightConfig->SightRadius + 500.f;  // 시야 잃는 거리
	SightConfig->PeripheralVisionAngleDegrees = 360.f;  // 시야각
	SightConfig->SetMaxAge(5.f);  // 기억 유지 시간

	// 마지막 위치 1000 유닛 이내면 자동 성공
	SightConfig->AutoSuccessRangeFromLastSeenLocation = 1000.f;

	// 감지 대상 설정 - 플레이어만 감지
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;

	SetGenericTeamId(FGenericTeamId(1));

	// Perception에 감지 설정 추가
	AIPerceptionComponent->ConfigureSense(*SightConfig);
	AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
}

ETeamAttitude::Type ADRAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// 플레이어 컨트롤러 체크
	if (const APawn* OtherPawn = Cast<APawn>(&Other))
	{
		if (const IGenericTeamAgentInterface* TeamAgent = Cast<const IGenericTeamAgentInterface>(OtherPawn->GetController()))
		{
			FGenericTeamId OtherTeamId = TeamAgent->GetGenericTeamId();

			// TeamId 0 = 플레이어 = 적
			if (OtherTeamId == 0)
			{
				return ETeamAttitude::Hostile;
			}
			// TeamId 1 = 다른 AI = 아군
			else if (OtherTeamId == 1)
			{
				return ETeamAttitude::Friendly;
			}
		}
	}

	return ETeamAttitude::Neutral;
}

void ADRAIController::BeginPlay()
{
	Super::BeginPlay();

	// Perception 이벤트 바인딩
	if (AIPerceptionComponent)
	{
		AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ADRAIController::OnTargetPerceptionUpdated);
	}
}

void ADRAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// 서버에서만 처리
	if (!HasAuthority()) return;

	APawn* PerceivedPawn = Cast<APawn>(Actor);
	if (!PerceivedPawn) return;

	// 플레이어 컨트롤러인지 확인
	if (!PerceivedPawn->IsPlayerControlled()) return;

	if (Stimulus.WasSuccessfullySensed())
	{
		// 플레이어 감지됨 - 맵에 추가
		UpdatePlayer(Actor);
	}
	else
	{
		// 플레이어 시야에서 벗어남 - 맵에서 제거
		RemovePlayer(Actor);
	}
}

void ADRAIController::UpdatePlayer(AActor* Player)
{
	if (!Player || !GetPawn()) return;

	PerceivedPlayers.Add(Player);

	if (PerceivedPlayers.Num() > 0)
	{
		Blackboard->SetValueAsBool("HasPlayerInRange", true);
	}
}

void ADRAIController::RemovePlayer(AActor* Player)
{
	PerceivedPlayers.Remove(Player);

	if (PerceivedPlayers.Num() == 0)
	{
		Blackboard->SetValueAsBool("HasPlayerInRange", false);
	}
}
