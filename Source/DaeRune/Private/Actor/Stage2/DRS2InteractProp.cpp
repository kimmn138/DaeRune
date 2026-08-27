// Copyright DaeRune

#include "Actor/Stage2/DRS2InteractProp.h"

#include "Actor/Stage2/DRS2SwitchPuzzle.h"
#include "Actor/Stage2/DRS2Safe.h"
#include "Actor/Stage2/DRS2SlidePuzzle.h"
#include "Character/DRCharacter.h"
#include "Components/WidgetComponent.h"
#include "Interaction/CombatInterface.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"

ADRS2InteractProp::ADRS2InteractProp()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	// ★프롭은 이동을 복제하지 않는다.
	//   레버 자세는 LeverBits 에서, 버튼 눌림 연출은 BP 타임라인에서 각 머신이 로컬로 계산한다.
	//   이동 복제를 켜두면 서버의 연출 좌표가 클라로 흘러가 로컬 연출과 충돌해 떨린다.
	SetReplicateMovement(false);

	// ★루트를 빈 SceneComponent 로 둔다. PropMesh 를 루트로 삼으면
	//   PropMesh->SetRelativeLocation 이 월드 좌표가 되어 눌림 연출을 만들 수 없다.
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	PropMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PropMesh"));
	PropMesh->SetupAttachment(SceneRoot);

	// 시점 라인트레이스(ECC_Visibility)에 걸려야 감지된다.
	PropMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	PropMesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	InteractionWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("InteractionWidget"));
	InteractionWidget->SetupAttachment(SceneRoot);
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

void ADRS2InteractProp::PlayInteractedVisual()
{
	OnInteractedVisual();
}

void ADRS2InteractProp::Multicast_PlayInteractedVisual_Implementation()
{
	PlayInteractedVisual();
}

// ================= ADRS2Lever =================

ADRS2Lever::ADRS2Lever()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 연타로 전구가 정신없이 깜빡이는 것을 막는다. 자세 전환(0.15초)보다 넉넉하게 잡았다.
	InteractCooldown = 0.4f;

	// 이동 복제 해제는 베이스 생성자가 처리한다 (자세를 각 머신이 로컬 계산하므로).
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

ADRS2SafeButton::ADRS2SafeButton()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
}

void ADRS2SafeButton::BeginPlay()
{
	Super::BeginPlay();

	// 눌림 연출의 기준점. BP 에서 PropMesh 를 옮겨 배치했어도 그 위치에서 눌린다.
	if (PropMesh)
	{
		PressBaseLocation = PropMesh->GetRelativeLocation();
	}

	// 금고에 자기를 등록한다 (서버·클라 공통).
	// 금고는 이 목록으로 ① 문에 부착 ② 개방 시 조작 차단 두 가지를 처리한다.
	if (ADRS2Safe* Safe = Cast<ADRS2Safe>(OwnerPuzzle))
	{
		Safe->RegisterButton(this);
	}
}

void ADRS2SafeButton::PlayInteractedVisual()
{
	StartPressAnimation();

	// BP 훅(사운드 등)도 그대로 호출한다.
	Super::PlayInteractedVisual();
}

void ADRS2SafeButton::StartPressAnimation()
{
	PressElapsed = 0.f;
	bPressing = true;
	SetActorTickEnabled(true);
}

void ADRS2SafeButton::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!bPressing || !PropMesh) return;

	PressElapsed += DeltaSeconds;

	const float InTime = FMath::Max(PressInDuration, KINDA_SMALL_NUMBER);
	const float OutTime = FMath::Max(PressOutDuration, KINDA_SMALL_NUMBER);

	// 0 -> 1 (들어감) -> 0 (돌아옴)
	float Alpha;
	if (PressElapsed < InTime)
	{
		Alpha = PressElapsed / InTime;
	}
	else if (PressElapsed < InTime + OutTime)
	{
		Alpha = 1.f - (PressElapsed - InTime) / OutTime;
	}
	else
	{
		// ★끝나면 반드시 원위치로 스냅한다. 중간값에 멈추는 일이 없다.
		Alpha = 0.f;
		bPressing = false;
		SetActorTickEnabled(false);
	}

	PropMesh->SetRelativeLocation(PressBaseLocation + PressOffset * Alpha);
}

void ADRS2SafeButton::ExecuteInteract(ADRCharacter* /*Character*/)
{
	ADRS2Safe* Safe = Cast<ADRS2Safe>(OwnerPuzzle);
	if (!Safe)
	{
		UE_LOG(LogDR, Error, TEXT("[S2SafeButton] %s: OwnerPuzzle 이 금고가 아닙니다. 배선을 확인하세요."), *GetName());
		return;
	}

	switch (ButtonType)
	{
	case ES2SafeButtonType::Digit:
		Safe->PushDigit(static_cast<uint8>(FMath::Clamp(PropIndex, 0, 9)));
		break;

	case ES2SafeButtonType::Delete:
		Safe->DeleteLastDigit();
		break;

	case ES2SafeButtonType::Reset:
		Safe->ClearInput();
		break;

	case ES2SafeButtonType::Enter:
		Safe->SubmitCode();
		break;
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
