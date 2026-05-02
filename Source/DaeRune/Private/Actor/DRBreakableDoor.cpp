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

	// NavModifier Component ����
	NavModifierComponent = CreateDefaultSubobject<UNavModifierComponent>(TEXT("NavModifierComponent"));
	// �ʱ⿡�� NavArea_Null�� ����
	NavModifierComponent->SetAreaClass(UNavArea_Null::StaticClass());
}

void ADRBreakableDoor::BeginPlay()
{
	Super::BeginPlay();

	// ��� ���� ����
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

	// �� ������ ��� StaticMeshComponent ����
	TArray<UStaticMeshComponent*> MeshComponents;
	GetComponents<UStaticMeshComponent>(MeshComponents);

	for (UStaticMeshComponent* MeshComp : MeshComponents)
	{
		if (MeshComp && MeshComp != RootComponent)
		{
			DoorPieces.Add(MeshComp);

			// �ʱ� Transform ����
			FPieceInitialTransform InitialTransform;
			InitialTransform.Location = MeshComp->GetRelativeLocation();
			InitialTransform.Rotation = MeshComp->GetRelativeRotation();
			InitialTransforms.Add(InitialTransform);

			// ���� �ùķ��̼��� �ϴ� ��Ȱ��ȭ
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

	// NavModifier ������ ������ �� �ְ� ����
	if (NavModifierComponent)
	{
		// NavArea_Default�� �����Ͽ� AI�� ������ �� �ְ� ��
		NavModifierComponent->SetAreaClass(UNavArea_Default::StaticClass());
	}

	// �������� �ı� ȿ�� ����
	ApplyImpulseToPieces();
	PlayBreakEffects();

	// ��� Ŭ���̾�Ʈ�� �ı� ȿ�� ����
	Multicast_PlayBreakEffect();

	// ���� ����
	if (bSpawnEnemiesOnBreak && bShouldSpawnEnemies)
	{
		SpawnEnemies();
	}

	// ���� ���� Ÿ�̸� ����
	GetWorld()->GetTimerManager().SetTimer(
		CleanupTimerHandle,
		this,
		&ADRBreakableDoor::CleanupPieces,
		PieceLifetime,
		false
	);

	// Delegate ��ε�ĳ��Ʈ
	OnDoorBroken.Broadcast();
}

void ADRBreakableDoor::OnRep_IsBroken()
{
	if (bIsBroken)
	{
		// �ʰ� ������ Ŭ���̾�Ʈ�� ���� ���� ����ȭ
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

		// ���� �ùķ��̼� Ȱ��ȭ
		Piece->SetSimulatePhysics(true);
		Piece->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Piece->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore); // �÷��̾�� �浹 ����
		Piece->SetCanEverAffectNavigation(false); // ������ NavMesh�� ������ ���� ����

		// Impulse ��� �� ����
		FVector Impulse = CalculatePieceImpulse(Piece, i);
		Piece->AddImpulse(Impulse, NAME_None, true); // bVelChange = true (���� ����)
	}
}

FVector ADRBreakableDoor::CalculatePieceImpulse(UStaticMeshComponent* Piece, int32 PieceIndex)
{
	// �⺻ ����
	FVector BaseDirection = BreakDirection.GetSafeNormal();

	// ������ ��� ��ġ ��������
	FVector PieceLocation = Piece->GetRelativeLocation();

	// �¿� �л� ��� (������ Y ��ġ�� ����)
	// ���� �߽��� �������� ����/���������� �������
	float SideOffset = PieceLocation.Y; // ���� Y ��ǥ
	FVector SidewaysDirection = FVector(0.0f, FMath::Sign(SideOffset), 0.0f);
	float SidewaysStrength = FMath::Abs(SideOffset) / 100.0f; // �߽ɿ��� �ּ��� �� ���� �����
	SidewaysStrength = FMath::Clamp(SidewaysStrength, 0.0f, 1.0f);

	// ���� ��
	FVector UpwardDirection = FVector(0.0f, 0.0f, 1.0f);

	// ���� ����
	FVector RandomOffset = FVector(
		FMath::FRandRange(-1.0f, 1.0f),
		FMath::FRandRange(-1.0f, 1.0f),
		FMath::FRandRange(0.0f, 1.0f) // Z�� ����� (���θ�)
	) * RandomVariation;

	// ���� ���� �ռ�
	FVector FinalDirection = BaseDirection
		+ (SidewaysDirection * SidewaysStrength * SidewaysSpreadRatio)
		+ (UpwardDirection * UpwardForceRatio)
		+ RandomOffset;

	FinalDirection = FinalDirection.GetSafeNormal();

	// ���� Impulse
	FVector FinalImpulse = FinalDirection * ImpulseStrength;

	return FinalImpulse;
}

void ADRBreakableDoor::Multicast_PlayBreakEffect_Implementation()
{
	if (HasAuthority()) return;

	// Ŭ���̾�Ʈ���� ���� �ùķ��̼� �� ����Ʈ ����
	ApplyImpulseToPieces();
	PlayBreakEffects();
}

void ADRBreakableDoor::PlayBreakEffects()
{
	// ���� ���
	if (BreakSound)
	{
		FVector SoundLocation = GetActorLocation();
		if (EnemySpawnOffsets.IsValidIndex(1))
		{
			SoundLocation += GetActorRotation().RotateVector(EnemySpawnOffsets[1]);
		}

		UGameplayStatics::PlaySoundAtLocation(this, BreakSound, SoundLocation);
	}

	// ��ƼŬ ����Ʈ
	if (BreakParticle)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(),
			BreakParticle,
			GetActorLocation(),
			GetActorRotation()
		);
	}

	// ���̾ư��� ����Ʈ
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

	// SpawnOffsets�� ��������� �⺻ ��ġ���� ����
	if (EnemySpawnOffsets.Num() == 0)
	{
		SpawnCount = EnemySpawnCount;
	}

	// Launch ���� ���
	FVector LaunchDirection = BreakDirection.GetSafeNormal();
	LaunchDirection.Z += EnemyLaunchUpwardRatio;
	LaunchDirection = LaunchDirection.GetSafeNormal();
	FVector LaunchVelocity = LaunchDirection * EnemyLaunchStrength;

	for (int32 i = 0; i < SpawnCount; i++)
	{
		FVector SpawnLocation;

		if (EnemySpawnOffsets.IsValidIndex(i))
		{
			// ���� �������� ���� ��ǥ�� ��ȯ
			SpawnLocation = GetActorLocation() + GetActorRotation().RotateVector(EnemySpawnOffsets[i]);
		}
		else
		{
			// �� ���ʿ��� ����
			FVector Offset = -BreakDirection.GetSafeNormal() * 100.0f;
			Offset.Y += (i - SpawnCount / 2.0f) * 150.0f; // �¿�� �л�
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
			// ���� BreakDirection �������� �о��ֱ�
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
			// ���� �ùķ��̼� ����
			Piece->SetSimulatePhysics(false);
			// �����
			Piece->SetVisibility(false);
			// �浹�� ����
			Piece->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}
}

