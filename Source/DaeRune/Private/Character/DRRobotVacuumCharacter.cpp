// Copyright DaeRune


#include "Character/DRRobotVacuumCharacter.h"
#include "AbilitySystemComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "DRGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/DRPlayerController.h"
#include "Sound/SoundBase.h"

ADRRobotVacuumCharacter::ADRRobotVacuumCharacter()
{
	// 지속 돌진 자동 전진(Tick) 필요
	PrimaryActorTick.bCanEverTick = true;

	// 근접 감지 스피어 (부품 DetectionSphere 패턴)
	MountDetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("MountDetectionSphere"));
	MountDetectionSphere->SetupAttachment(GetCapsuleComponent());
	MountDetectionSphere->SetSphereRadius(300.f);
	MountDetectionSphere->bMultiBodyOverlap = false;
	MountDetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	MountDetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	MountDetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 탑승 프롬프트 위젯 (부품 InteractionWidget 패턴 — 로컬 코스메틱)
	MountPromptWidget = CreateDefaultSubobject<UWidgetComponent>(TEXT("MountPromptWidget"));
	MountPromptWidget->SetupAttachment(GetCapsuleComponent());
	MountPromptWidget->SetRelativeLocation(FVector(0.f, 0.f, 120.f));
	MountPromptWidget->SetWidgetSpace(EWidgetSpace::Screen);
	MountPromptWidget->SetDrawSize(FVector2D(300.f, 100.f));
	MountPromptWidget->SetVisibility(false);
}

void ADRRobotVacuumCharacter::BeginPlay()
{
	Super::BeginPlay();

	// 돌진 충돌 감지 바인딩 (판정 자체는 OnCapsuleHit에서 서버 전용으로 수행)
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		Capsule->SetNotifyRigidBodyCollision(true);
		Capsule->OnComponentHit.AddDynamic(this, &ADRRobotVacuumCharacter::OnCapsuleHit);
	}

	// 탑승 감지 오버랩 바인딩 (로컬 PC 감지 후보 등록 — 부품 패턴)
	if (MountDetectionSphere)
	{
		// BP에 저장된 오버라이드가 생성자 값을 덮어 오버랩 이벤트가 죽는 사고 방지 (실제 발생했던 버그)
		MountDetectionSphere->SetGenerateOverlapEvents(true);
		MountDetectionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		MountDetectionSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
		MountDetectionSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

		MountDetectionSphere->OnComponentBeginOverlap.AddDynamic(this, &ADRRobotVacuumCharacter::OnMountDetectionBeginOverlap);
		MountDetectionSphere->OnComponentEndOverlap.AddDynamic(this, &ADRRobotVacuumCharacter::OnMountDetectionEndOverlap);
	}
}

void ADRRobotVacuumCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// 레벨 전환/파괴 시 내 위의 라이더 링크 정리 (라이더 측 정리는 ADRCharacter::EndPlay)
	if (HasAuthority() && RiderOnTop)
	{
		DismountRider(false);
	}

	Super::EndPlay(EndPlayReason);
}

void ADRRobotVacuumCharacter::OnMountDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                                           UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
                                                           bool bFromSweep, const FHitResult& SweepResult)
{
	RefreshMountOverlapStateFor(Cast<ADRCharacter>(OtherActor));
}

void ADRRobotVacuumCharacter::OnMountDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
                                                         UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	RefreshMountOverlapStateFor(Cast<ADRCharacter>(OtherActor));
}

void ADRRobotVacuumCharacter::RefreshMountOverlapStateFor(ADRCharacter* Character)
{
	if (!Character || Character == this || !MountDetectionSphere) return;

	ADRPlayerController* PC = Cast<ADRPlayerController>(Character->GetController());
	if (!PC || !PC->IsLocalController()) return;

	// 부품 소지/탑승 여부 같은 동적 조건은 FindMountByLineTrace가 매 주기 재검사하므로
	// 여기서는 순수 오버랩 여부만으로 후보 등록/해제한다
	const bool bShouldDetect = MountDetectionSphere->IsOverlappingActor(Character) && !bDead;
	PC->SetMountDetectionEnabled(bShouldDetect, this);
}

void ADRRobotVacuumCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// 지속 돌진: "입력 소유" 머신에서 전진 입력 주입 (Plan3 §15.2).
	// CMC의 클라 예측 구조상 원격 클라 폰은 자기 ServerMove 입력이 서버 값을 덮으므로,
	// 서버(HasAuthority)에서만 주입하면 리슨 호스트 폰만 움직이고 클라 폰은 제자리에 선다.
	// IsLocallyControlled = 리슨 호스트 본인 + 원격 소유 클라 양쪽을 커버, 시뮬 프록시는 제외.
	// bSustainedDash는 복제 프로퍼티라 소유 클라에도 도착한다 (§4.2).
	// 조향은 컨트롤러 회전(bUseControllerRotationYaw)을 따르므로 마우스로 방향 조절 가능 (Plan3 §10-4)
	if (bSustainedDash && !bDead && IsLocallyControlled())
	{
		AddMovementInput(GetActorForwardVector(), 1.f);
	}

	// 라이더 좌석 추적: 본 소켓의 '위치'만 매 프레임 따라간다 (회전은 라이더 컨트롤러 소유).
	// 라이더가 본이 아닌 캡슐에 attach돼 있어 애니메이션 기울기는 전달되지 않고, 높낮이 움직임만 반영된다.
	// RiderOnTop은 복제되고 메시는 각 머신에서 로컬로 애니메이트되므로 서버/클라 모두에서 수행.
	if (IsValid(RiderOnTop) && RiderOnTop->GetAttachParentActor() == this && GetMesh())
	{
		RiderOnTop->SetActorLocation(
			GetMesh()->GetSocketLocation(RideSocketName) + FVector(0.f, 0.f, RiderOnTop->GetMountZOffset()));
	}
}

void ADRRobotVacuumCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRRobotVacuumCharacter, RiderOnTop);
	DOREPLIFETIME(ADRRobotVacuumCharacter, bSustainedDash);

	// 애님 전용 플래그 (ABP가 매 프레임 폴링 — RepNotify 불필요)
	DOREPLIFETIME(ADRRobotVacuumCharacter, bIsDashCharging);
	DOREPLIFETIME(ADRRobotVacuumCharacter, bIsJetJumping);
}

void ADRRobotVacuumCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// 공중 Q 사용 플래그 리셋 (판정은 서버 값만 사용)
	if (HasAuthority())
	{
		bAirJumpUsed = false;
		SetJetJumping(false);
	}
}

void ADRRobotVacuumCharacter::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
	// 사망 시 탑승 링크 정리 (서버 경로에서만)
	if (HasAuthority())
	{
		// 내 위의 라이더 강제 하차
		if (RiderOnTop)
		{
			DismountRider(false);
		}

		// 내가 다른 청소기에 타고 있었다면 그쪽 링크 해제
		if (MountedOn)
		{
			MountedOn->DismountRider(false);
		}
	}

	Super::MulticastHandleDeath_Implementation(DeathImpulse);
}

// ========== 탑승 (마운트 측) ==========

bool ADRRobotVacuumCharacter::CanBeMountedBy(ADRCharacter* Candidate) const
{
	if (!Candidate || Candidate == this) return false;

	// 이미 위에 라이더가 있으면 거부 → "라이더를 얹은 청소기에는 새로 못 탄다" (탑쌓기 순서 규칙)
	if (RiderOnTop != nullptr) return false;

	// 이미 어딘가에 타고 있는 캐릭터는 거부
	if (Candidate->IsMounted()) return false;

	// 사망/부품 소지/스턴 상태 거부
	if (bDead || Candidate->bDead) return false;
	if (Candidate->IsCarryingPart()) return false;
	if (Candidate->bIsStunned) return false;

	// 순환 방지 1: 내가 타고 있는 체인(아래 방향)에 후보가 있으면 거부 (상호 탑승 차단)
	for (const ADRCharacter* Below = MountedOn; Below; Below = Below->MountedOn)
	{
		if (Below == Candidate) return false;
	}

	// 순환 방지 2: 후보 위의 라이더 체인(위 방향)에 내가 있으면 거부
	if (const ADRRobotVacuumCharacter* CandidateVac = Cast<ADRRobotVacuumCharacter>(Candidate))
	{
		const ADRCharacter* Above = CandidateVac->RiderOnTop;
		while (Above)
		{
			if (Above == this) return false;
			const ADRRobotVacuumCharacter* AboveVac = Cast<ADRRobotVacuumCharacter>(Above);
			Above = AboveVac ? AboveVac->RiderOnTop.Get() : nullptr;
		}
	}

	return true;
}

void ADRRobotVacuumCharacter::MountRider(ADRCharacter* Rider)
{
	if (!HasAuthority() || !Rider) return;

	RiderOnTop = Rider;
	Rider->MountedOn = this;

	// 라이더 이동 정지 + 마운트와 상호 밀림 방지
	Rider->GetCharacterMovement()->SetMovementMode(MOVE_None);
	Rider->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	Rider->AttachToMountSocket(this);

	if (UAbilitySystemComponent* RiderASC = Rider->GetAbilitySystemComponent())
	{
		const FGameplayTag& RidingTag = FDRGameplayTags::Get().State_Riding;
		RiderASC->AddLooseGameplayTag(RidingTag);
		RiderASC->AddReplicatedLooseGameplayTag(RidingTag);
	}

	// 리슨 서버 parity (클라는 RepNotify 경로)
	OnRep_RiderOnTop();
}

void ADRRobotVacuumCharacter::DismountRider(bool bLaunchOff)
{
	if (!HasAuthority() || !RiderOnTop) return;

	ADRCharacter* Rider = RiderOnTop;
	RiderOnTop = nullptr;

	// 라이더가 이미 파괴 중이면 링크만 해제 (EndPlay 경로 대비)
	if (!IsValid(Rider))
	{
		OnRep_RiderOnTop();
		return;
	}

	// 분리 + 겹침 방지 위치 (마운트 위 60 + 전방 80)
	Rider->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	const FVector DismountLocation = GetActorLocation()
		+ FVector(0.f, 0.f, 60.f)
		+ GetActorForwardVector() * 80.f;
	Rider->SetActorLocation(DismountLocation, false, nullptr, ETeleportType::TeleportPhysics);

	Rider->GetCharacterMovement()->SetMovementMode(MOVE_Falling);
	if (bLaunchOff)
	{
		// 폴짝 뛰어내리는 연출
		Rider->LaunchCharacter(FVector(0.f, 0.f, 300.f) + GetActorForwardVector() * 200.f, false, false);
	}

	// Pawn 콜리전 응답 복원
	Rider->GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);

	if (UAbilitySystemComponent* RiderASC = Rider->GetAbilitySystemComponent())
	{
		const FGameplayTag& RidingTag = FDRGameplayTags::Get().State_Riding;
		RiderASC->RemoveLooseGameplayTag(RidingTag);
		RiderASC->RemoveReplicatedLooseGameplayTag(RidingTag);
	}

	Rider->MountedOn = nullptr;

	OnRep_RiderOnTop();
}

void ADRRobotVacuumCharacter::OnRep_RiderOnTop()
{
	// TODO(Plan3 Phase B): 탑승 프롬프트 갱신 등 클라 코스메틱 처리
}

void ADRRobotVacuumCharacter::SetInteractionUIVisible(bool bShow)
{
	if (MountPromptWidget)
	{
		MountPromptWidget->SetVisibility(bShow);
	}
}

// ========== 돌진 ==========

void ADRRobotVacuumCharacter::SetDashCollisionEnabled(bool bEnabled)
{
	if (!HasAuthority()) return;
	bDashCollisionArmed = bEnabled;
}

void ADRRobotVacuumCharacter::SetSustainedDash(bool bEnabled)
{
	if (!HasAuthority() || bSustainedDash == bEnabled) return;

	bSustainedDash = bEnabled;

	// 강화 공기탄 반동 예외 판정용 태그 동기화 (Plan3 §8.2)
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		const FGameplayTag& DashTag = FDRGameplayTags::Get().State_RobotVacuum_SustainedDash;
		if (bEnabled)
		{
			ASC->AddLooseGameplayTag(DashTag);
			ASC->AddReplicatedLooseGameplayTag(DashTag);
		}
		else
		{
			ASC->RemoveLooseGameplayTag(DashTag);
			ASC->RemoveReplicatedLooseGameplayTag(DashTag);
		}
	}

	// 리슨 서버 parity (Armadillo OnRep_IsRolling 패턴)
	OnRep_SustainedDash();
}

void ADRRobotVacuumCharacter::OnRep_SustainedDash()
{
	// TODO(Plan3 Phase F): 지속 돌진 루프 사운드/VFX 시작·정지
}

void ADRRobotVacuumCharacter::MulticastPlayDashImpactSound_Implementation(FVector_NetQuantize Location)
{
	if (GetNetMode() == NM_DedicatedServer) return;
	if (!DashImpactSound) return;

	UGameplayStatics::PlaySoundAtLocation(this, DashImpactSound, Location);
}

void ADRRobotVacuumCharacter::OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
                                           UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	// 판정은 전부 서버 + 돌진 버프 활성 중에만
	if (!HasAuthority() || !bDashCollisionArmed) return;
	if (!OtherActor || OtherActor == this) return;

	// 탑 상태에서 서로 충돌 판정 금지
	if (OtherActor == RiderOnTop || OtherActor == MountedOn) return;

	// 기본 이속보다 확실히 빠를 때만 (벽 스침 오판 방지)
	if (GetVelocity().Size2D() < DashImpactMinSpeed) return;

	// Pawn(적/타 플레이어) 또는 수직면(벽/지형지물 — 바닥 착지는 노말 Z가 커서 제외)
	const bool bHitPawn = Cast<APawn>(OtherActor) != nullptr;
	const bool bHitVerticalSurface = !bHitPawn && Hit.ImpactNormal.Z < 0.7f;
	if (!bHitPawn && !bHitVerticalSurface) return;

	// 중복 발화 방지: 1회 브로드캐스트 후 해제 (GA가 FinishDash에서 재확인)
	bDashCollisionArmed = false;
	OnDashImpact.Broadcast(OtherActor, Hit);
}

// ========== 애님 전용 플래그 ==========

void ADRRobotVacuumCharacter::SetDashCharging(bool bNew)
{
	if (!HasAuthority()) return;
	bIsDashCharging = bNew;
}

void ADRRobotVacuumCharacter::SetJetJumping(bool bNew)
{
	if (!HasAuthority()) return;
	bIsJetJumping = bNew;
}
