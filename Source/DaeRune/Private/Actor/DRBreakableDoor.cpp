// Copyright DaeRune


#include "Actor/DRBreakableDoor.h"
#include "Character/DREnemy.h"
#include "Components/StaticMeshComponent.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NavModifierComponent.h"
#include "NavAreas/NavArea_Null.h"
#include "NavAreas/NavArea_Default.h"

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
	InitialTransforms.Empty();

	// 이 액터의 모든 StaticMeshComponent 수집
	TArray<UStaticMeshComponent*> MeshComponents;
	GetComponents<UStaticMeshComponent>(MeshComponents);

	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (MeshComp && MeshComp != RootComponent)
		{
			DoorPieces.Add(MeshComp);

			// 초기 Transform 저장
			FPieceInitialTransform InitialTransform;
			InitialTransform.Location = MeshComp->GetRelativeLocation();
			InitialTransform.Rotation = MeshComp->GetRelativeRotation();
			InitialTransforms.Add(InitialTransform);

			// 물리 시뮬레이션은 일단 비활성화
			MeshComp->SetSimulatePhysics(false);
			MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
	}
}

void ADRBreakableDoor::Break()
{
	if (!HasAuthority()) return;

	if (bIsBroken) return;

	bShouldSpawnEnemies = true;
	ExecuteBreak();
}

void ADRBreakableDoor::BreakWithoutEnemies()
{
	if (!HasAuthority()) return;

	if (bIsBroken) return;

	bShouldSpawnEnemies = false;
	ExecuteBreak();
}

void ADRBreakableDoor::ExecuteBreak()
{
	bIsBroken = true;

	// NavModifier 영역을 지나갈 수 있게 변경
	if (NavModifierComponent)
	{
		// NavArea_Default로 변경하여 AI가 지나갈 수 있게 함
		NavModifierComponent->SetAreaClass(UNavArea_Default::StaticClass());
	}

	// 서버에서 파괴 효과 실행
	ApplyImpulseToPieces();
	PlayBreakEffects();

	// 모든 클라이언트에 파괴 효과 전파
	Multicast_PlayBreakEffect();

	// 몬스터 스폰
	if (bSpawnEnemiesOnBreak && bShouldSpawnEnemies)
	{
		SpawnEnemies();
	}

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
	if (bIsBroken)
	{
		// 늦게 참여한 클라이언트를 위한 상태 동기화
		for (UStaticMeshComponent* Piece : DoorPieces)
		{
			if (Piece)
			{
				Piece->SetVisibility(false);
			}
		}
	}
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
	ApplyImpulseToPieces();
	PlayBreakEffects();
}

void ADRBreakableDoor::PlayBreakEffects()
{
	// 사운드 재생
	if (BreakSound)
	{
		FVector SoundLocation = GetActorLocation();
		if (EnemySpawnOffsets.IsValidIndex(1))
		{
			SoundLocation += GetActorRotation().RotateVector(EnemySpawnOffsets[1]);
		}

		UGameplayStatics::PlaySoundAtLocation(this, BreakSound, SoundLocation);
	}

	// 파티클 이펙트
	if (BreakParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			BreakParticle,
			GetActorLocation(),
			GetActorRotation()
		);
	}

	// 나이아가라 이펙트
	if (BreakNiagara)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			BreakNiagara,
			GetActorLocation(),
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

	// Launch 방향 계산
	FVector LaunchDirection = BreakDirection.GetSafeNormal();
	LaunchDirection.Z += EnemyLaunchUpwardRatio;
	LaunchDirection = LaunchDirection.GetSafeNormal();
	FVector LaunchVelocity = LaunchDirection * EnemyLaunchStrength;

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
			// 적을 BreakDirection 방향으로 밀어주기
			SpawnedEnemy->LaunchCharacter(LaunchVelocity, true, true);
		}
	}
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

