// Copyright DaeRune

#include "Actor/Stage2/DRS2InteractProp.h"

#include "Actor/Stage2/DRS2SwitchPuzzle.h"
#include "Actor/Stage2/DRS2Safe.h"
#include "Actor/Stage2/DRS2SlidePuzzle.h"
#include "Character/DRCharacter.h"
#include "Components/WidgetComponent.h"
#include "Interaction/CombatInterface.h"
#include "Net/UnrealNetwork.h"

ADRS2InteractProp::ADRS2InteractProp()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	PropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropMesh"));
	SetRootComponent(PropMesh);

	// 시점 라인트레이스(ECC_Visibility)에 걸려야 감지된다.
	PropMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PropMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetupAttachment(PropMesh);
	InteractionWidget->SetWidgetSpace(EWidgetSpace::Screen);
	InteractionWidget->SetVisibility(false);
}

void ADRS2InteractProp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2InteractProp, bPropEnabled);
}

bool ADRS2InteractProp::CanInteract(const ADRCharacter* Character) const
{
	if (!bPropEnabled || !IsValid(Character)) return false;

	// IsDead 는 ICombatInterface 의 BlueprintNativeEvent 이므로 Execute_ 로 호출한다.
	if (ICombatInterface::Execute_IsDead(const_cast<ADRCharacter*>(Character))) return false;

	// 부품 운반 중에는 퍼즐을 조작할 수 없다 (한 손에 부품을 들고 있는 상태)
	if (Character->IsCarryingPart()) return false;

	return true;
}

void ADRS2InteractProp::ServerHandleInteract(ADRCharacter* Character)
{
	if (!HasAuthority()) return;

	// 연타 방지. 프롭 단위 쿨다운이라 누가 눌렀든 함께 적용된다.
	if (InteractCooldown > 0.f)
	{
		const float Now = GetWorld()->GetTimeSeconds();
		if (Now - LastInteractTime < InteractCooldown) return;

		LastInteractTime = Now;
	}

	ExecuteInteract(Character);
}

void ADRS2InteractProp::SetPropEnabled(bool bNewEnabled)
{
	if (!HasAuthority() || bPropEnabled == bNewEnabled) return;

	bPropEnabled = bNewEnabled;
	OnRep_bPropEnabled();
}

void ADRS2InteractProp::OnRep_bPropEnabled()
{
	if (!bPropEnabled)
	{
		InteractionWidget->SetVisibility(false);
	}

	OnPropEnabledChanged(bPropEnabled);
}

void ADRS2InteractProp::SetInteractionUIVisible(bool bShow)
{
	// 로컬 코스메틱 (판정 근거가 전부 복제 프로퍼티라 RPC 불필요)
	InteractionWidget->SetVisibility(bShow && bPropEnabled);
}

void ADRS2InteractProp::Multicast_PlayInteractedVisual_Implementation()
{
	OnInteractedVisual();
}

// ================= ADRS2Lever =================

ADRS2Lever::ADRS2Lever()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 연타로 전구가 정신없이 깜빡이는 것을 막는다. 자세 전환(0.15초)보다 넉넉하게 잡았다.
	InteractCooldown = 0.4f;

	// 자세는 LeverBits 로부터 서버·클라가 각자 계산한다. 회전을 복제하면 소스가 이중이 되어 떨린다.
	SetReplicateMovement(false);
}

void ADRS2Lever::BeginPlay()
{
	Super::BeginPlay();

	// ★배치 회전이 기준점이다. 여기에 OnRotation/OffRotation 을 로컬 합성한다.
	const FQuat BaseQuat = GetActorQuat();
	OffQuat = BaseQuat * OffRotation.Quaternion();
	OnQuat = BaseQuat * OnRotation.Quaternion();

	// 일단 OFF 자세로 두고, 등록 직후 퍼즐이 실제 상태를 밀어준다.
	ApplyPose(0.f);

	if (ADRS2SwitchPuzzle* Puzzle = Cast<ADRS2SwitchPuzzle>(OwnerPuzzle))
	{
		Puzzle->RegisterLever(this);
	}
}

void ADRS2Lever::SetLeverOn(bool bNewOn)
{
	// 첫 호출(등록 시점)에는 값이 같아도 자세를 적용해야 한다.
	if (bPoseInitialized && bLeverOn == bNewOn) return;

	const bool bFirstApply = !bPoseInitialized;
	bPoseInitialized = true;
	bLeverOn = bNewOn;

	// 최초 적용과 즉시 모드는 보간 없이 스냅한다.
	if (bFirstApply || ToggleDuration <= 0.f)
	{
		bAnimating = false;
		SetActorTickEnabled(false);
		CurrentAlpha = bLeverOn ? 1.f : 0.f;
		ApplyPose(CurrentAlpha);
		return;
	}

	FromAlpha = CurrentAlpha;
	ToAlpha = bLeverOn ? 1.f : 0.f;
	Elapsed = 0.f;
	bAnimating = true;
	SetActorTickEnabled(true);
}

void ADRS2Lever::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bAnimating) return;

	Elapsed += DeltaSeconds;
	const float T = FMath::Clamp(Elapsed / FMath::Max(ToggleDuration, KINDA_SMALL_NUMBER), 0.f, 1.f);

	CurrentAlpha = FMath::Lerp(FromAlpha, ToAlpha, T);
	ApplyPose(CurrentAlpha);

	if (T >= 1.f)
	{
		bAnimating = false;
		SetActorTickEnabled(false);
	}
}

void ADRS2Lever::ApplyPose(float Alpha)
{
	SetActorRotation(FQuat::Slerp(OffQuat, OnQuat, FMath::Clamp(Alpha, 0.f, 1.f)));
}

void ADRS2Lever::ExecuteInteract(ADRCharacter* /*Character*/)
{
	if (ADRS2SwitchPuzzle* Puzzle = Cast<ADRS2SwitchPuzzle>(OwnerPuzzle))
	{
		// 자세는 퍼즐이 LeverBits 갱신 후 SetLeverOn 으로 밀어준다 (여기서 토글하지 않는다).
		Puzzle->ToggleLever(PropIndex);
		Multicast_PlayInteractedVisual();
	}
}

// ================= ADRS2SafeButton =================

void ADRS2SafeButton::ExecuteInteract(ADRCharacter* /*Character*/)
{
	ADRS2Safe* Safe = Cast<ADRS2Safe>(OwnerPuzzle);
	if (!Safe) return;

	if (bIsClearButton)
	{
		Safe->ClearInput();
	}
	else
	{
		Safe->PushDigit(static_cast<uint8>(FMath::Clamp(PropIndex, 0, 9)));
	}

	Multicast_PlayInteractedVisual();
}

// ================= ADRS2PuzzleTerminal =================

void ADRS2PuzzleTerminal::ExecuteInteract(ADRCharacter* Character)
{
	if (ADRS2SlidePuzzle* Puzzle = Cast<ADRS2SlidePuzzle>(OwnerPuzzle))
	{
		Puzzle->RequestOpenUI(Character);
		Multicast_PlayInteractedVisual();
	}
}
