// Copyright DaeRune

#include "Actor/Stage2/DRS2PassageBlocker.h"

#include "Components/BoxComponent.h"
#include "Character/DRCharacter.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

ADRS2PassageBlocker::ADRS2PassageBlocker()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	bReplicates = true;
	// 애니메이션은 로컬 시뮬로 처리한다. 이동 복제를 켜면 소스가 이중이 되어 떨림이 생긴다.
	SetReplicateMovement(false);

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	// 실제 차단은 이 박스가 담당한다 (메시 형상과 무관하게 통로 폭을 확실히 덮기 위함).
	// 문이 움직여도 이 박스는 제자리에서 콜리전만 토글된다.
	PawnBlock = CreateDefaultSubobject<UBoxComponent>(TEXT("PawnBlock"));
	PawnBlock->SetupAttachment(SceneRoot);
	PawnBlock->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PawnBlock->SetCollisionObjectType(ECC_WorldStatic);
	PawnBlock->SetCollisionResponseToAllChannels(ECR_Ignore);
	PawnBlock->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	PawnBlock->SetBoxExtent(FVector(200.f, 200.f, 300.f));
}

void ADRS2PassageBlocker::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2PassageBlocker, bBlocked);
}

void ADRS2PassageBlocker::BeginPlay()
{
	Super::BeginPlay();

	// 파생 클래스가 컴포넌트 수집·원점 캐시를 먼저 수행한다
	InitializePose();

	// 초기 포즈는 연출 없이 즉시 적용한다.
	// 서버에서 bBlocked 를 세팅하면 복제로 클라에 전달되지만, 클라도 자기 기본값으로
	// 동일한 포즈를 잡아두어야 복제 도착 전 한 프레임 어긋남이 없다.
	if (HasAuthority())
	{
		bBlocked = bStartBlocked;
	}

	CurrentAlpha = bStartBlocked ? 1.f : 0.f;
	bMoving = false;
	SetActorTickEnabled(false);

	ApplyPose(CurrentAlpha);
	PawnBlock->SetCollisionEnabled(bStartBlocked ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
}

void ADRS2PassageBlocker::SetBlocked(bool bNewBlocked)
{
	if (!HasAuthority()) return;

	// 멱등: 같은 값이면 아무것도 하지 않는다.
	if (bBlocked == bNewBlocked) return;

	bBlocked = bNewBlocked;

	// 리슨 서버에서도 연출이 돌도록 RepNotify 수동 호출
	OnRep_bBlocked();
}

void ADRS2PassageBlocker::OnRep_bBlocked()
{
	BeginMove();
}

void ADRS2PassageBlocker::BeginMove()
{
	MoveFromAlpha = CurrentAlpha;
	MoveToAlpha = bBlocked ? 1.f : 0.f;
	MoveElapsed = 0.f;
	bMoving = true;
	SetActorTickEnabled(true);

	// 열릴 때는 이동 "시작" 시점에 콜리전을 해제한다.
	// 움직이는 동안에도 벽이 남아 있으면 플레이어가 갇힌다.
	if (!bBlocked)
	{
		PawnBlock->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	OnMoveStarted(bBlocked);
}

void ADRS2PassageBlocker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bMoving) return;

	MoveElapsed += DeltaSeconds;
	const float T = FMath::Clamp(MoveElapsed / FMath::Max(MoveDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

	CurrentAlpha = FMath::Lerp(MoveFromAlpha, MoveToAlpha, T);
	ApplyPose(CurrentAlpha);

	if (T >= 1.f)
	{
		FinishMove();
	}
}

void ADRS2PassageBlocker::FinishMove()
{
	bMoving = false;
	SetActorTickEnabled(false);

	CurrentAlpha = MoveToAlpha;
	ApplyPose(CurrentAlpha);

	if (bBlocked)
	{
		// 막힐 때는 이동 "완료" 후에 콜리전을 켠다.
		// 움직이는 중에 켜면 캐릭터를 밀어 올리거나 벽에 끼우는 사고가 난다.
		PawnBlock->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

		if (HasAuthority())
		{
			PushOutTrappedPawns();
		}
	}

	OnMoveFinished(bBlocked);
}

void ADRS2PassageBlocker::PushOutTrappedPawns()
{
	UWorld* World = GetWorld();
	if (!PushOutPoint || !World || !PawnBlock) return;

	// ★PawnBlock 은 Pawn 에 Block 응답이라 오버랩 이벤트를 만들지 않는다.
	//   따라서 GetOverlappingActors 는 항상 비어 있다. 명시적 형상 질의로 찾아야 한다.
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(S2BlockerPushOut), false, this);
	Params.bIgnoreTouches = false;

	World->OverlapMultiByObjectType(
		Overlaps,
		PawnBlock->GetComponentLocation(),
		PawnBlock->GetComponentQuat(),
		FCollisionObjectQueryParams(ECC_Pawn),
		FCollisionShape::MakeBox(PawnBlock->GetScaledBoxExtent()),
		Params);

	TSet<AActor*> Pushed;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		ADRCharacter* Character = Cast<ADRCharacter>(Overlap.GetActor());
		if (!IsValid(Character) || Pushed.Contains(Character)) continue;

		Pushed.Add(Character);
		Character->SetActorLocation(PushOutPoint->GetActorLocation(), false, nullptr, ETeleportType::TeleportPhysics);
		UE_LOG(LogDR, Warning, TEXT("[S2Blocker] %s: 통로에 남은 %s 를 밀어냈습니다."),
			*GetName(), *Character->GetName());
	}
}
