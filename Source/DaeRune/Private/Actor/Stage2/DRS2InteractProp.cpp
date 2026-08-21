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

void ADRS2Lever::ServerHandleInteract(ADRCharacter* /*Character*/)
{
	if (ADRS2SwitchPuzzle* Puzzle = Cast<ADRS2SwitchPuzzle>(OwnerPuzzle))
	{
		Puzzle->ToggleLever(PropIndex);
		Multicast_PlayInteractedVisual();
	}
}

// ================= ADRS2SafeButton =================

void ADRS2SafeButton::ServerHandleInteract(ADRCharacter* /*Character*/)
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

void ADRS2PuzzleTerminal::ServerHandleInteract(ADRCharacter* Character)
{
	if (ADRS2SlidePuzzle* Puzzle = Cast<ADRS2SlidePuzzle>(OwnerPuzzle))
	{
		Puzzle->RequestOpenUI(Character);
		Multicast_PlayInteractedVisual();
	}
}
