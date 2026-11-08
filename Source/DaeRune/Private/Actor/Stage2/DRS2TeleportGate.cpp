// Copyright DaeRune

#include "Actor/Stage2/DRS2TeleportGate.h"

#include "Components/BoxComponent.h"
#include "Character/DRCharacter.h"
#include "Game/DRGameStateBase.h"
#include "Interaction/CombatInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

ADRS2TeleportGate::ADRS2TeleportGate()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	USceneComponent* SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	GateMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GateMesh"));
	GateMesh->SetupAttachment(SceneRoot);
	GateMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GateMesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	// 길 안내 강조는 오버레이 머티리얼인데, UE 5.5 Nanite 프록시는 오버레이를 그리지 않는다 → 일반 메시로 그린다
	// (문 메시가 수십 개 삼각형이라 비용 차이는 없다)
	GateMesh->bDisallowNanite = true;

	TriggerBox = CreateDefaultSubobject<UBoxComponent>(TEXT("TriggerBox"));
	TriggerBox->SetupAttachment(SceneRoot);
	TriggerBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	TriggerBox->SetCollisionObjectType(ECC_WorldStatic);
	TriggerBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	TriggerBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	TriggerBox->SetBoxExtent(FVector(120.f, 120.f, 120.f));
	TriggerBox->SetGenerateOverlapEvents(true);

	DefaultDestination = CreateDefaultSubobject<USceneComponent>(TEXT("DefaultDestination"));
	DefaultDestination->SetupAttachment(SceneRoot);
}

void ADRS2TeleportGate::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2TeleportGate, bActive);
	DOREPLIFETIME(ADRS2TeleportGate, bGuideHighlight);
}

void ADRS2TeleportGate::BeginPlay()
{
	Super::BeginPlay();

	if (HasAuthority())
	{
		bActive = bStartActive;
		TriggerBox->OnComponentBeginOverlap.AddDynamic(this, &ADRS2TeleportGate::HandleBeginOverlap);
	}

	// 강조 오버레이는 여기서 한 번만 걸고 이후엔 파라미터(0/1)로만 켜고 끈다.
	// 켤 때 SetOverlayMaterial 을 하면 프록시가 다시 만들어지는데, 패키지 빌드는 새 PSO 가 컴파일될 때까지
	// 프록시 생성을 미뤄(r.PSOPrecache.ProxyCreationWhenPSOReady) 그동안 문이 사라진다. 지금은 로딩 화면 중이다.
	if (GuideHighlightMaterial)
	{
		GuideHighlightMID = UMaterialInstanceDynamic::Create(GuideHighlightMaterial, this);
		GateMesh->SetOverlayMaterial(GuideHighlightMID);
	}

	// 초기 발광 상태 반영 (서버/클라 공통)
	OnGateActiveChanged(bActive);
	RefreshGuideHighlight();
}

void ADRS2TeleportGate::SetGateActive(bool bNewActive)
{
	if (!HasAuthority() || bActive == bNewActive) return;

	bActive = bNewActive;

	// 리슨 서버에서도 연출이 돌도록 수동 호출
	OnRep_bActive();
}

void ADRS2TeleportGate::OnRep_bActive()
{
	OnGateActiveChanged(bActive);
	RefreshGuideHighlight();
}

void ADRS2TeleportGate::SetGuideHighlight(bool bNewHighlight)
{
	if (!HasAuthority() || bGuideHighlight == bNewHighlight) return;

	bGuideHighlight = bNewHighlight;

	// 리슨 서버에서도 보이도록 수동 호출
	OnRep_bGuideHighlight();

	UE_LOG(LogDR, Log, TEXT("[S2Gate] %s 길 안내 강조 %s"), *GetName(), bGuideHighlight ? TEXT("켬") : TEXT("끔"));
}

void ADRS2TeleportGate::OnRep_bGuideHighlight()
{
	RefreshGuideHighlight();
}

void ADRS2TeleportGate::RefreshGuideHighlight()
{
	// 꺼진 게이트가 빛나면 못 쓰는 문으로 안내하게 되므로 활성일 때만 켠다
	if (GuideHighlightMID)
	{
		GuideHighlightMID->SetScalarParameterValue(GuideHighlightParameterName, (bActive && bGuideHighlight) ? 1.f : 0.f);
	}
}

void ADRS2TeleportGate::SetEntryRule(ES2GateEntryRule NewRule)
{
	if (!HasAuthority()) return;

	EntryRule = NewRule;

	UE_LOG(LogDR, Verbose, TEXT("[S2Gate] %s 진입 규칙 변경: %s"),
		*GetName(), NewRule == ES2GateEntryRule::CarrierOnly ? TEXT("CarrierOnly") : TEXT("Anyone"));
}

void ADRS2TeleportGate::HandleBeginOverlap(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	// 위치 이동은 서버 권한으로만. 비활성 상태에서는 오버랩을 무시한다.
	if (!HasAuthority() || !bActive) return;

	ADRCharacter* Character = Cast<ADRCharacter>(OtherActor);
	if (!Character) return;

	// IsDead 는 ICombatInterface 의 BlueprintNativeEvent 이므로 Execute_ 로 호출한다.
	if (ICombatInterface::Execute_IsDead(Character)) return;

	// 진입 자격 검사
	if (EntryRule == ES2GateEntryRule::CarrierOnly && !Character->IsCarryingPart())
	{
		OnEntryDenied(Character);
		return;
	}

	if (Mode == ES2GateMode::TeamOnCarrier && Character->IsCarryingPart())
	{
		// 부품 소지자가 통과하면 생존자 전원을 목적지로 회수한다.
		const ADRGameStateBase* DRGameState = GetWorld() ? GetWorld()->GetGameState<ADRGameStateBase>() : nullptr;
		TArray<ADRCharacter*> AlivePlayers;
		if (DRGameState)
		{
			AlivePlayers = DRGameState->GetAlivePlayers();
		}

		// 회수 대상에 진입자가 빠지는 일이 없도록 방어
		AlivePlayers.AddUnique(Character);

		for (int32 Index = 0; Index < AlivePlayers.Num(); ++Index)
		{
			TeleportOne(AlivePlayers[Index], Index, AlivePlayers.Num());
		}

		UE_LOG(LogDR, Log, TEXT("[S2Gate] %s: 부품 소지자 통과 -> 생존자 %d명 회수"), *GetName(), AlivePlayers.Num());

		OnTeamRecalled.Broadcast();
	}
	else
	{
		TeleportOne(Character, 0, 1);
		OnGateUsed.Broadcast(Character);
	}

	if (bDeactivateOnUse)
	{
		SetGateActive(false);
	}
}

void ADRS2TeleportGate::GetDestinationTransform(FVector& OutLocation, FRotator& OutRotation) const
{
	if (IsValid(DestinationOverride))
	{
		OutLocation = DestinationOverride->GetActorLocation();
		OutRotation = DestinationOverride->GetActorRotation();
		return;
	}

	OutLocation = DefaultDestination->GetComponentLocation();
	OutRotation = DefaultDestination->GetComponentRotation();
}

void ADRS2TeleportGate::TeleportOne(ADRCharacter* Character, int32 SlotIndex, int32 SlotTotal)
{
	if (!IsValid(Character)) return;

	FVector DestLocation;
	FRotator DestRotation;
	GetDestinationTransform(DestLocation, DestRotation);

	// 여러 명이 동시에 이동하면 같은 지점에 겹치므로 원형으로 분산한다.
	if (SlotTotal > 1 && DestinationSpreadRadius > 0.f)
	{
		const float Angle = (2.f * PI * SlotIndex) / SlotTotal;
		DestLocation += FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * DestinationSpreadRadius;
	}

	Character->SetActorLocationAndRotation(DestLocation, DestRotation, false, nullptr, ETeleportType::TeleportPhysics);

	if (AController* Controller = Character->GetController())
	{
		Controller->SetControlRotation(DestRotation);
	}

	Multicast_PlayTeleportFX(Character);
}

void ADRS2TeleportGate::Multicast_PlayTeleportFX_Implementation(ADRCharacter* Who)
{
	OnTeleportFX(Who);
}
