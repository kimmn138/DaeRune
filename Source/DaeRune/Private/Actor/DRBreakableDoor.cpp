// Copyright DaeRune


#include "Actor/DRBreakableDoor.h"
#include "Character/DREnemy.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavArea_Default.h"

namespace
{
	// 벽 조각 전체 바운드의 중심 — 조각이 없으면 Fallback
	FVector GetPiecesCenter(const TArray<TObjectPtr<UStaticMeshComponent>>& Pieces, const FVector& Fallback)
	{
		FBox Box(ForceInit);
		for (const UStaticMeshComponent* Piece : Pieces)
		{
			if (Piece)
			{
				Box += Piece->Bounds.GetBox();
			}
		}
		return Box.IsValid ? Box.GetCenter() : Fallback;
	}
}

ADRBreakableDoor::ADRBreakableDoor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	bAlwaysRelevant = true;

	// Root Component
	RootSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("RootSceneComponent"));
	SetRootComponent(RootSceneComponent);

	// NavModifier Component 생성
	NavModifierComponent = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifierComponent"));
	// 초기에는 NavArea_Null로 설정
	NavModifierComponent->SetAreaClass(UNavArea_Null::StaticClass());
}

void ADRBreakableDoor::BeginPlay()
{
	Super::BeginPlay();

	// 모든 조각 수집
	CollectDoorPieces();
}

void ADRBreakableDoor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRBreakableDoor, bIsBroken);
}

void ADRBreakableDoor::CollectDoorPieces()
{
	DoorPieces.Empty();

	// 이 액터의 모든 StaticMeshComponent 수집
	TArray<UStaticMeshComponent*> MeshComponents;
	GetComponents<UStaticMeshComponent>(MeshComponents);

	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (MeshComp && MeshComp != RootComponent)
		{
			DoorPieces.Add(MeshComp);

			// 물리 시뮬레이션은 일단 비활성화
			MeshComp->SetSimulatePhysics(false);
			MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}
}

void ADRBreakableDoor::Break()
{
	if (!HasAuthority()) return;

	if (bIsBroken || bBreakPending) return;

	// 적을 먼저 벽 뒤에 스폰하고, 클라에 복제될 시간을 둔 뒤 벽을 부순다
	// (예전에는 벽을 부순 뒤 스폰해서 원격 클라에서는 벽이 먼저 깨지고 적이 나중에 나타났다)
	if (bSpawnEnemiesOnBreak)
	{
		SpawnEnemies();

		if (EnemySpawnLeadTime > 0.0f && WaitingEnemies.Num() > 0)
		{
			bBreakPending = true;
			GetWorld()->GetTimerManager().SetTimer(
				BreakDelayTimerHandle,
				this,
				&ADRBreakableDoor::ExecuteBreak,
				EnemySpawnLeadTime,
				false
			);
			return;
		}
	}

	ExecuteBreak();
}

void ADRBreakableDoor::BreakWithoutEnemies()
{
	if (!HasAuthority()) return;

	// 이미 적을 스폰해 둔 문은 예약된 파괴가 그대로 진행된다
	if (bIsBroken || bBreakPending) return;

	ExecuteBreak();
}

void ADRBreakableDoor::ExecuteBreak()
{
	bBreakPending = false;
	bIsBroken = true;

	// NavModifier 영역을 지나갈 수 있게 변경
	if (NavModifierComponent)
	{
		// NavArea_Default로 변경하여 AI가 지나갈 수 있게 함
		NavModifierComponent->SetAreaClass(UNavArea_Default::StaticClass());
	}

	// 서버에서 파괴 효과 실행
	PlayBreakSequence();

	// 모든 클라이언트에 파괴 효과 전파
	Multicast_PlayBreakEffect();

	// 벽 뒤에서 기다리던 적을 깨운다 (파편이 폰 충돌을 끈 뒤라 그대로 뚫고 나온다)
	ReleaseWaitingEnemies();

	// 파편 정리 타이머 시작
	GetWorld()->GetTimerManager().SetTimer(
		CleanupTimerHandle,
		this,
		&ADRBreakableDoor::CleanupPieces,
		PieceLifetime,
		false
	);

	// Delegate 브로드캐스트
	OnDoorBroken.Broadcast();
}

void ADRBreakableDoor::OnRep_IsBroken()
{
	// ★연출을 이미 본 머신은 건드리지 않는다★ — 예전에는 파괴 멀티캐스트 직후 도착한 이 복제가
	// 날아가는 파편을 곧바로 숨겨 원격 클라에서는 문이 그냥 사라졌다.
	// 연출을 못 본 늦은 참가자만 숨긴다 (멀티캐스트가 더 늦게 오면 PlayBreakSequence 가 다시 보이게 한다).
	if (bIsBroken && !bBreakEffectPlayed)
	{
		for (UStaticMeshComponent* Piece : DoorPieces)
		{
			if (Piece)
			{
				Piece->SetVisibility(false);
			}
		}
	}
}

void ADRBreakableDoor::PlayBreakSequence()
{
	if (bBreakEffectPlayed) return;
	bBreakEffectPlayed = true;

	for (UStaticMeshComponent* Piece : DoorPieces)
	{
		if (Piece)
		{
			Piece->SetVisibility(true);
		}
	}

	// 이펙트 위치를 조각이 날아가기 전에 잰다
	PlayBreakEffects();
	ApplyImpulseToPieces();
}

void ADRBreakableDoor::ApplyImpulseToPieces()
{
	for (int32 i = 0; i < DoorPieces.Num(); i++)
	{
		UStaticMeshComponent* Piece = DoorPieces[i];
		if (!Piece) continue;

		// 물리 시뮬레이션 활성화
		Piece->SetSimulatePhysics(true);
		Piece->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Piece->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // 플레이어와 충돌 무시
		Piece->SetCanEverAffectNavigation(false); // 파편이 NavMesh 에 영향을 주지 않게

		// Impulse 계산 및 적용
		FVector Impulse = CalculatePieceImpulse(Piece, i);
		Piece->AddImpulse(Impulse, NAME_None, true); // bVelChange = true (질량 무시)
	}
}

FVector ADRBreakableDoor::CalculatePieceImpulse(UStaticMeshComponent* Piece, int32 PieceIndex)
{
	// 기본 방향
	FVector BaseDirection = BreakDirection.GetSafeNormal();

	// 조각의 상대 위치 가져오기
	FVector PieceLocation = Piece->GetRelativeLocation();

	// 좌우 분산 계산 (조각의 Y 위치에 따라)
	// 문의 중심을 기준으로 왼쪽/오른쪽으로 흩어지게
	float SideOffset = PieceLocation.Y; // 로컬 Y 좌표
	FVector SidewaysDirection = FVector(0.0f, FMath::Sign(SideOffset), 0.0f);
	float SidewaysStrength = FMath::Abs(SideOffset) / 100.0f; // 중심에서 멀수록 더 많이 흩어짐
	SidewaysStrength = FMath::Clamp(SidewaysStrength, 0.0f, 1.0f);

	// 위쪽 힘
	FVector UpwardDirection = FVector(0.0f, 0.0f, 1.0f);

	// 랜덤 변동
	FVector RandomOffset = FVector(
		FMath::FRandRange(-1.0f, 1.0f),
		FMath::FRandRange(-1.0f, 1.0f),
		FMath::FRandRange(0.0f, 1.0f) // Z는 양수만 (위로만)
	) * RandomVariation;

	// 최종 방향 합성
	FVector FinalDirection = BaseDirection
		+ (SidewaysDirection * SidewaysStrength * SidewaysSpreadRatio)
		+ (UpwardDirection * UpwardForceRatio)
		+ RandomOffset;

	FinalDirection = FinalDirection.GetSafeNormal();

	// 최종 Impulse
	FVector FinalImpulse = FinalDirection * ImpulseStrength;

	return FinalImpulse;
}

void ADRBreakableDoor::Multicast_PlayBreakEffect_Implementation()
{
	if (HasAuthority()) return;

	// 클라이언트에서 물리 시뮬레이션 및 이펙트 실행
	PlayBreakSequence();
}

void ADRBreakableDoor::PlayBreakEffects()
{
	// 소리 · 이펙트는 벽 조각들의 중심에서 — 배치된 문은 액터가 월드 원점에 있고 조각이 월드 좌표에 놓여 있다
	// (예전에는 두 번째 스폰 오프셋에서 냈는데, 오프셋이 하나뿐인 문은 원점에서 나서 방1 에서는 감쇠 거리 밖이었다)
	const FVector EffectLocation = GetPiecesCenter(DoorPieces, GetActorLocation());

	// 사운드 재생
	if (BreakSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, BreakSound, EffectLocation);
	}

	// 파티클 이펙트
	if (BreakParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			BreakParticle,
			EffectLocation,
			GetActorRotation()
		);
	}

	// 나이아가라 이펙트
	if (BreakNiagara)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			BreakNiagara,
			EffectLocation,
			GetActorRotation()
		);
	}
}

void ADRBreakableDoor::SpawnEnemies()
{
	if (!HasAuthority()) return;
	if (!EnemyClassToSpawn) return;

	int32 SpawnCount = FMath::Min(EnemySpawnCount, EnemySpawnOffsets.Num());

	// SpawnOffsets이 비어있으면 기본 위치에서 스폰
	if (EnemySpawnOffsets.Num() == 0)
	{
		SpawnCount = EnemySpawnCount;
	}

	for (int32 i = 0; i < SpawnCount; i++)
	{
		FVector SpawnLocation;

		if (EnemySpawnOffsets.IsValidIndex(i))
		{
			// 로컬 오프셋을 월드 좌표로 변환
			SpawnLocation = GetActorLocation() + GetActorRotation().RotateVector(EnemySpawnOffsets[i]);
		}
		else
		{
			// 문 뒤쪽에서 스폰
			FVector Offset = -BreakDirection.GetSafeNormal() * 100.0f;
			Offset.Y += (i - SpawnCount / 2.0f) * 150.0f; // 좌우로 분산
			SpawnLocation = GetActorLocation() + Offset;
		}

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ADREnemy* SpawnedEnemy = GetWorld()->SpawnActor<ADREnemy>(
			EnemyClassToSpawn,
			SpawnLocation,
			BreakDirection.Rotation(),
			SpawnParams
		);

		if (SpawnedEnemy)
		{
			// 벽이 부서질 때까지 제자리에서 기다리게 AI 를 멈춘다 (스폰 때 빙의되며 BT 가 이미 돌고 있다) — 벽 너머 공격 방지
			if (AAIController* AIC = Cast<AAIController>(SpawnedEnemy->GetController()))
			{
				if (UBrainComponent* Brain = AIC->GetBrainComponent())
				{
					Brain->StopLogic(TEXT("BreakableDoor: Waiting for break"));
				}
			}

			WaitingEnemies.Add(SpawnedEnemy);
		}
	}
}

void ADRBreakableDoor::ReleaseWaitingEnemies()
{
	// Launch 방향 계산
	FVector LaunchDirection = BreakDirection.GetSafeNormal();
	LaunchDirection.Z += EnemyLaunchUpwardRatio;
	LaunchDirection = LaunchDirection.GetSafeNormal();
	const FVector LaunchVelocity = LaunchDirection * EnemyLaunchStrength;

	for (const TWeakObjectPtr<ADREnemy>& EnemyPtr : WaitingEnemies)
	{
		ADREnemy* Enemy = EnemyPtr.Get();
		if (!Enemy) continue;

		if (AAIController* AIC = Cast<AAIController>(Enemy->GetController()))
		{
			if (UBrainComponent* Brain = AIC->GetBrainComponent())
			{
				Brain->RestartLogic();
			}
		}

		// 적을 BreakDirection 방향으로 밀어주기
		Enemy->LaunchCharacter(LaunchVelocity, true, true);
	}

	WaitingEnemies.Empty();
}

void ADRBreakableDoor::CleanupPieces()
{
	for (UStaticMeshComponent* Piece : DoorPieces)
	{
		if (Piece)
		{
			// 물리 시뮬레이션 중지
			Piece->SetSimulatePhysics(false);
			// 숨기기
			Piece->SetVisibility(false);
			// 충돌도 끄기
			Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

