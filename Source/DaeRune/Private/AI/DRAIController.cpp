// Copyright DaeRune


#include "AI/DRAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

ADRAIController::ADRAIController()
{
	// �������� ������Ʈ ���� - AI ���� ������ �����
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>("BlackboardComponent");
	check(Blackboard);
	// �����̺�� Ʈ�� ������Ʈ ���� - AI �ൿ ���� ����
	BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>("BehaviorTreeComponent");
	check(BehaviorTreeComponent);

	// AI Perception ������Ʈ ����
	AIPerceptionComponent = CreateDefaultSubobject<UAIPerceptionComponent>("AIPerceptionComponent");
	SetPerceptionComponent(*AIPerceptionComponent);

	// �þ� ���� ����
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>("SightConfig");
	SightConfig->SightRadius = 2000.f;  // �þ� �Ÿ�
	SightConfig->LoseSightRadius = SightConfig->SightRadius + 500.f;  // �þ� �Ҵ� �Ÿ�
	SightConfig->PeripheralVisionAngleDegrees = 360.f;  // �þ߰�
	SightConfig->SetMaxAge(3.f);  // ��� ���� �ð�

	// ������ ��ġ 1000 ���� �̳��� �ڵ� ����
	SightConfig->AutoSuccessRangeFromLastSeenLocation = 1000.f;

	// ���� ��� ���� - �÷��̾ ����
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = false;

	SetGenericTeamId(FGenericTeamId(1));

	// Perception�� ���� ���� �߰�
	AIPerceptionComponent->ConfigureSense(*SightConfig);
	AIPerceptionComponent->SetDominantSense(SightConfig->GetSenseImplementation());
}

ETeamAttitude::Type ADRAIController::GetTeamAttitudeTowards(const AActor& Other) const
{
	// �÷��̾� ��Ʈ�ѷ� üũ
	if (const APawn* OtherPawn = Cast<APawn>(&Other))
	{
		if (const IGenericTeamAgentInterface* TeamAgent = Cast<const IGenericTeamAgentInterface>(OtherPawn->GetController()))
		{
			FGenericTeamId OtherTeamId = TeamAgent->GetGenericTeamId();

			// TeamId 0 = �÷��̾� = ��
			if (OtherTeamId == 0)
			{
				return ETeamAttitude::Hostile;
			}
			// TeamId 1 = �ٸ� AI = �Ʊ�
			else if (OtherTeamId == 1)
			{
				return ETeamAttitude::Friendly;
			}
		}
	}

	return ETeamAttitude::Neutral;
}

void ADRAIController::UpdateCombatTime()
{
	if (!Blackboard) return;
	
	// ���� ���� �ð��� �������忡 ����
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	Blackboard->SetValueAsFloat(FName("LastCombatTime"), CurrentTime);
}

bool ADRAIController::HasCombatTimedOut(float TimeoutSeconds) const
{
	if (!Blackboard) return true; // �������� ������ ���� ����� ����
	
	const float LastCombatTime = Blackboard->GetValueAsFloat(FName("LastCombatTime"));
	
	// ���� ������ �� ���� (�ʱⰪ 0)
	if (LastCombatTime <= 0.0f) return true;
	
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float ElapsedTime = CurrentTime - LastCombatTime;
	
	return ElapsedTime > TimeoutSeconds;
}

void ADRAIController::BeginPlay()
{
	Super::BeginPlay();

	// Perception �̺�Ʈ ���ε�
	if (AIPerceptionComponent)
	{
		AIPerceptionComponent->OnTargetPerceptionUpdated.AddDynamic(this, &ADRAIController::OnTargetPerceptionUpdated);
	}

	// [DEBUG] 매 1초마다 AI 상태 로깅 (서버 + Possess 후에만)
	GetWorld()->GetTimerManager().SetTimer(DebugStateLogTimerHandle, this,
		&ADRAIController::DebugStateLog, 1.0f, true, 1.0f);
}

void ADRAIController::DebugStateLog()
{
	if (!HasAuthority()) return;
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return;

	ACharacter* MyChar = Cast<ACharacter>(MyPawn);
	UCharacterMovementComponent* CMC = MyChar ? MyChar->GetCharacterMovement() : nullptr;

	FString TargetName = TEXT("NULL");
	FVector TargetLoc = FVector::ZeroVector;
	bool bHasPlayerInRange = false;
	FString BTNodeName = TEXT("?");
	if (Blackboard)
	{
		if (UObject* TargetObj = Blackboard->GetValueAsObject(FName("TargetCleanserSite")))
		{
			TargetName = TargetObj->GetName();
		}
		TargetLoc = Blackboard->GetValueAsVector(FName("TargetCleanserSiteLocation"));
		bHasPlayerInRange = Blackboard->GetValueAsBool(FName("HasPlayerInRange"));
	}
	if (BehaviorTreeComponent)
	{
		BTNodeName = BehaviorTreeComponent->DescribeActiveTasks();
	}

	UE_LOG(LogTemp, Warning,
		TEXT("[AITick] %s | Loc=%s | Vel=%.0f | MoveMode=%d | Target=%s | TargetLoc=%s | HasPlayer=%d | BT=%s"),
		*MyPawn->GetName(),
		*MyPawn->GetActorLocation().ToString(),
		MyPawn->GetVelocity().Size(),
		CMC ? static_cast<int32>(CMC->MovementMode.GetValue()) : -1,
		*TargetName,
		*TargetLoc.ToString(),
		bHasPlayerInRange ? 1 : 0,
		*BTNodeName);
}

void ADRAIController::OnTargetPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	// ���������� ó��
	if (!HasAuthority()) return;

	APawn* PerceivedPawn = Cast<APawn>(Actor);
	if (!PerceivedPawn) return;

	// �÷��̾� ��Ʈ�ѷ����� Ȯ��
	if (!PerceivedPawn->IsPlayerControlled()) return;

	if (Stimulus.WasSuccessfullySensed())
	{
		// �÷��̾� ������ - �ʿ� �߰�
		UpdatePlayer(Actor);
	}
	else
	{
		// �÷��̾� �þ߿��� ��� - �ʿ��� ����
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
