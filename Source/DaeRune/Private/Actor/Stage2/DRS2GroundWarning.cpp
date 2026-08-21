// Copyright DaeRune

#include "Actor/Stage2/DRS2GroundWarning.h"

#include "Components/DecalComponent.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"

ADRS2GroundWarning::ADRS2GroundWarning()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;
	// 경고는 위치가 바뀔 수 있으므로(추종 모드) 이동 복제를 켠다.
	SetReplicateMovement(true);

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	WarningDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("WarningDecal"));
	WarningDecal->SetupAttachment(SceneRoot);
	WarningDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	WarningDecal->DecalSize = FVector(300.f, 100.f, 100.f);

	WarningFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("WarningFX"));
	WarningFX->SetupAttachment(SceneRoot);
	WarningFX->bAutoActivate = false;
}

void ADRS2GroundWarning::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2GroundWarning, Radius);
	DOREPLIFETIME(ADRS2GroundWarning, Duration);
}

void ADRS2GroundWarning::BeginPlay()
{
	Super::BeginPlay();

	if (DecalMaterial)
	{
		WarningDecal->SetDecalMaterial(DecalMaterial);
	}

	if (WarningNiagaraSystem)
	{
		WarningFX->SetAsset(WarningNiagaraSystem);
	}

	// ★InitWarning 은 SpawnActorDeferred 경로에서 BeginPlay 보다 먼저 불린다.
	//   BP 타임라인이 BeginPlay 전에 시작되면 안 되므로, 연출 발화는 여기서 한다.
	//   클라이언트는 초기 복제로 Radius/Duration 을 이미 받은 상태다.
	bBegunPlay = true;
	ApplyWarningVisual();
}

void ADRS2GroundWarning::InitWarning(float InRadius, float InDuration)
{
	if (!HasAuthority()) return;

	Radius = FMath::Max(1.f, InRadius);
	Duration = FMath::Max(0.05f, InDuration);

	// 리슨 서버에서도 연출이 돌아야 하므로 RepNotify 를 수동 호출한다 (Plan6 §15.0 규약)
	OnRep_WarningParams();

	// 수명이 다하면 스스로 파괴되고, 복제로 클라에서도 함께 사라진다
	SetLifeSpan(Duration);
}

void ADRS2GroundWarning::UpdateGroundLocation(const FVector& NewGroundLocation)
{
	if (!HasAuthority()) return;

	SetActorLocation(NewGroundLocation);
}

void ADRS2GroundWarning::OnRep_WarningParams()
{
	ApplyWarningVisual();
}

void ADRS2GroundWarning::ApplyWarningVisual()
{
	// 데칼은 X 가 투영 깊이, YZ 가 평면 반경이다 (ADRPoisonGasActor.cpp:47 관례)
	if (WarningDecal)
	{
		WarningDecal->DecalSize = FVector(DecalProjectionDepth, Radius, Radius);
		WarningDecal->MarkRenderStateDirty();
	}

	// BeginPlay 전에는 BP 연출을 시작하지 않는다 (타임라인/사운드가 동작하지 않는 시점)
	if (!bBegunPlay) return;

	if (WarningFX && WarningFX->GetAsset() && !WarningFX->IsActive())
	{
		WarningFX->Activate(true);
	}

	if (!bVisualStarted)
	{
		bVisualStarted = true;
		OnWarningBegin(Radius, Duration);
	}
}
