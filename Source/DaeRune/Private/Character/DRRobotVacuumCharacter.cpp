// Copyright DaeRune


#include "Character/DRRobotVacuumCharacter.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "DRGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/DRPlayerController.h"
#include "Sound/SoundBase.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Game/DRProgressionConfig.h"
#include "DaeRune/DRLogChannels.h"
#include "EngineUtils.h"

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

	// ★Plan5(돌진칩)★ 돌진 중 파괴 · 레벨 전환 — 아군 캡슐에 내 이동 무시 항목이 남지 않게 (T-125)
	ApplyAllyPassThrough(false);

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
	//
	// ★서버(HasAuthority)에서 주입하면 원격 클라 폰은 제자리에 선다. 이유는 "덮어쓰기"가 아니라
	//   애초에 적용되지 않기 때문이다 (UE 5.5 CharacterMovementComponent.cpp 기준):
	//   - TickComponent:1588 이 ConsumeInputVector()로 입력을 먼저 꺼내 비운다.
	//   - :1677 의 게이트 bShouldPerformControlledCharMove = IsLocallyControlled() || ... 가 false 면
	//     ControlledCharacterMove()가 호출되지 않는다 → 꺼낸 입력은 쓰이지 않고 폐기된다.
	//   - 원격 클라 폰의 실제 이동은 오직 ServerMove RPC 경로에서만 일어나고
	//     (ServerMove_PerformMovement:"Perform actual movement" → MoveAutonomous),
	//     MoveAutonomous:10379 는 Acceleration 을 클라가 보낸 NewAccel 로 덮어쓴다.
	//   → 즉 서버가 밀었다가 되돌아가는 게 아니라, 서버 입력은 링에 오르지도 못하고
	//     클라가 보낸 "가속도 0"이 그대로 적용되어 정지가 확정된다(호스트 화면에서 떨림 없음).
	//
	// 리슨 호스트 폰은 서버=소유 클라라 IsLocallyControlled()가 true 이므로 혼자만 정상 동작했다.
	// IsLocallyControlled = 리슨 호스트 본인 + 원격 소유 클라 양쪽을 커버, 시뮬 프록시는 제외
	// (시뮬 프록시에서 주입하면 복제 위치와 싸운다 — 엔진이 이동 실행 여부를 판정할 때 쓰는 것과 같은 기준).
	// bSustainedDash는 복제 프로퍼티라 소유 클라에도 도착한다 (§4.2). 충돌/데미지/게이지는 여전히 서버 전용.
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
	DOREPLIFETIME(ADRRobotVacuumCharacter, bDashPassThroughAllies);   // ★Plan5(돌진칩)★ 모든 머신이 이동 무시를 맞춘다 (D-115)

	// 애님 전용 플래그 (ABP가 매 프레임 폴링 — RepNotify 불필요)
	DOREPLIFETIME(ADRRobotVacuumCharacter, bIsDashCharging);
	DOREPLIFETIME(ADRRobotVacuumCharacter, bIsJetJumping);
	DOREPLIFETIME(ADRRobotVacuumCharacter, JetJumpCounter);
}

void ADRRobotVacuumCharacter::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	// 공중 Q 사용 횟수 리셋 (판정은 서버 값만 사용)
	if (HasAuthority())
	{
		AirJumpsUsed = 0;   // ★Plan5(물점프칩)★ (예전 bAirJumpUsed = false — D-107)
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

	// ★Plan5(탑승칩)★ ③ 비상 탈출 뒤 이 청소기 탑승 금지 (D-90) — 클라 프롬프트와 같은 함수
	if (UDRAbilitySystemLibrary::IsRideBannedFrom(Candidate, this)) return false;

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

	// ★Plan5(탑승칩)★ ① 탑 인원이 바뀌었다 — 감속 다시 맞추기 (D-81)
	RefreshRideLoadsAfterLinkChange(Rider);

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
		RefreshRideLoadsAfterLinkChange(nullptr);   // ★Plan5(탑승칩)★ (D-81)
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

	// ★Plan5(탑승칩)★ ① 탑 인원이 바뀌었다 — 내린 청소기는 맨 아래가 됐다 (D-81)
	RefreshRideLoadsAfterLinkChange(Rider);

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

// ========== 탑승 칩 (Plan5(탑승칩)) ==========

int32 ADRRobotVacuumCharacter::GetRiderCountAbove() const
{
	// 상한 8 은 순환 방어용 (CanBeMountedBy 가 이미 막는다)
	int32 Count = 0;
	const ADRCharacter* Above = RiderOnTop;
	while (IsValid(Above) && Count < 8)
	{
		++Count;
		const ADRRobotVacuumCharacter* AboveVacuum = Cast<ADRRobotVacuumCharacter>(Above);
		Above = AboveVacuum ? AboveVacuum->RiderOnTop.Get() : nullptr;
	}
	return Count;
}

const ADRRobotVacuumCharacter* ADRRobotVacuumCharacter::GetStackBottom() const
{
	const ADRRobotVacuumCharacter* Bottom = this;
	for (int32 Steps = 0; IsValid(Bottom->MountedOn) && Steps < 8; ++Steps)
	{
		Bottom = Bottom->MountedOn;
	}
	return Bottom;
}

void ADRRobotVacuumCharacter::RefreshRideLoad()
{
	if (!HasAuthority()) return;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;

	const FDRGameplayTags& DRTags = FDRGameplayTags::Get();

	// ★핸들이 아니라 태그로 지운다★ — ASC 는 PlayerState 소유라 리스폰 · 클래스 변경 뒤에도 GE 가 남는다 (T-84)
	ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(DRTags.Debuff_RobotVacuum_RideLoad));

	if (bDead || MountedOn) return;   // ① 은 맨 아래(직접 달리는) 청소기만 (D-79)
	if (!UDRAbilitySystemLibrary::HasUpgradeFeature(ASC, DRTags.Upgrade_Feature_Skill_Ride_Power)) return;

	const int32 Riders = GetRiderCountAbove();
	if (Riders <= 0) return;

	const FDRRideChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(this).Ride;
	UClass* EffectClass = Tuning.PowerLoadEffect.LoadSynchronous();
	if (!EffectClass)
	{
		UE_LOG(LogDR, Warning, TEXT("[RideLoad] ChipTuning.Ride.PowerLoadEffect 미지정 — 칩①이 감속 없이 동작"));
		return;
	}

	const float SpeedMult = FMath::Clamp(1.f - Tuning.PowerSpeedPenaltyPerRider * Riders, 0.05f, 1.f);
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
	if (!Spec.IsValid()) return;
	Spec.Data->SetSetByCallerMagnitude(DRTags.Data_MoveSpeed_Mult, SpeedMult);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	UE_LOG(LogDR, Log, TEXT("[RideLoad] %s 위 %d명 → 이속 ×%.2f"), *GetName(), Riders, SpeedMult);
}

void ADRRobotVacuumCharacter::RefreshRideLoadsAfterLinkChange(ADRCharacter* Rider)
{
	// 탑승한 청소기는 맨 아래가 아니게 됐고, 내린 청소기는 맨 아래가 됐다
	if (ADRRobotVacuumCharacter* RiderVacuum = Cast<ADRRobotVacuumCharacter>(Rider))
	{
		RiderVacuum->RefreshRideLoad();
	}
	// 인원이 바뀐 것은 나부터 맨 아래까지 전부다 (T-85)
	int32 Steps = 0;
	for (ADRRobotVacuumCharacter* Vacuum = this; IsValid(Vacuum) && Steps++ < 8; Vacuum = Vacuum->MountedOn)
	{
		Vacuum->RefreshRideLoad();
	}
}

void ADRRobotVacuumCharacter::EmergencyEjectRider()
{
	if (!HasAuthority() || bDead || !IsValid(RiderOnTop)) return;

	ADRCharacter* Rider = RiderOnTop;
	UAbilitySystemComponent* SelfASC = GetAbilitySystemComponent();
	const FDRRideChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(this).Ride;

	// ① 먼저 내린다 — 이후 회복 · 금지는 탑 상태와 무관해진다 (D-89). 감속 갱신도 DismountRider 가 한다
	DismountRider(/*bLaunchOff*/ true);

	// ② 둘 다 회복. 대상은 방금까지 내 위에 있던 아군과 나 자신이라 팀 판정이 필요 없다 (T-33)
	UDRAbilitySystemLibrary::ApplyHealToActor(Rider, SelfASC, Tuning.EjectHealEffect, Tuning.EjectHealAmount, this);
	UDRAbilitySystemLibrary::ApplyHealToActor(this, SelfASC, Tuning.EjectHealEffect, Tuning.EjectHealAmount, this);

	// ③ 이 청소기 탑승 금지 — 소스 = 내 ASC 로 "어느 청소기"인지 구분한다 (D-90)
	const FActiveGameplayEffectHandle Ban =
		UDRAbilitySystemLibrary::ApplyTimedTagEffect(Rider, SelfASC, Tuning.EjectBanEffect, Tuning.EjectBanDuration);

	UE_LOG(LogDR, Log, TEXT("[RideEject] %s → %s 비상 탈출 · 회복 %.0f · 금지 %.0f초%s"),
		*GetName(), *GetNameSafe(Rider), Tuning.EjectHealAmount, Tuning.EjectBanDuration,
		Ban.IsValid() ? TEXT("") : TEXT(" ★EjectBanEffect 미지정 — 금지 없음★"));
}

void ADRRobotVacuumCharacter::OnUpgradeEffectsRefreshed()
{
	RefreshRideLoad();   // 서버에서만 실제로 동작. 빙의 때 리스폰 잔류 감속도 지운다
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

// ========== 돌진칩② 아군 통과 (Plan5(돌진칩)) ==========

void ADRRobotVacuumCharacter::SetDashPassThroughAllies(bool bEnabled)
{
	if (!HasAuthority() || bDashPassThroughAllies == bEnabled) return;
	bDashPassThroughAllies = bEnabled;
	OnRep_DashPassThroughAllies();   // 리슨 서버 parity (OnRep_SustainedDash 와 같은 관례)
}

void ADRRobotVacuumCharacter::OnRep_DashPassThroughAllies()
{
	ApplyAllyPassThrough(bDashPassThroughAllies);
}

void ADRRobotVacuumCharacter::ApplyAllyPassThrough(bool bEnable)
{
	UCapsuleComponent* MyCapsule = GetCapsuleComponent();
	if (!MyCapsule) return;

	// 먼저 이 머신에서 넣었던 것을 전부 되돌린다 (켜기 · 끄기 공통 — 중복 등록 방지)
	for (const TWeakObjectPtr<ADRCharacter>& Weak : PassThroughAllies)
	{
		if (ADRCharacter* Ally = Weak.Get())
		{
			MyCapsule->IgnoreActorWhenMoving(Ally, false);
			if (UCapsuleComponent* AllyCapsule = Ally->GetCapsuleComponent())
			{
				AllyCapsule->IgnoreActorWhenMoving(this, false);
			}
		}
	}
	PassThroughAllies.Reset();

	if (!bEnable || !GetWorld()) return;

	// ★양쪽 다★ — 내가 움직일 때도, 아군이 움직일 때도 서로 막지 않는다 (T-124).
	// 이동 무시는 복제되지 않아 이 함수가 ★모든 머신★ 에서 돈다 (T-123)
	for (TActorIterator<ADRCharacter> It(GetWorld()); It; ++It)
	{
		ADRCharacter* Ally = *It;
		if (Ally == this || UDRAbilitySystemLibrary::IsNotFriend(this, Ally)) continue;   // 플레이어끼리만 (D-116)

		MyCapsule->IgnoreActorWhenMoving(Ally, true);
		if (UCapsuleComponent* AllyCapsule = Ally->GetCapsuleComponent())
		{
			AllyCapsule->IgnoreActorWhenMoving(this, true);
		}
		PassThroughAllies.Add(Ally);
	}
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

	// ★Plan5(돌진칩)★ ② 아군 통과 중이면 아군 충돌은 무시 — 무시 목록에 없던 아군(돌진 중 부활 · 입장)의 안전망.
	//   피해 · 돌진 종료가 절대 없다 (물리적으로는 막힐 수 있다 — §9-69) (D-115)
	if (bDashPassThroughAllies && !UDRAbilitySystemLibrary::IsNotFriend(this, OtherActor)) return;

	// Pawn(적/타 플레이어) 또는 수직면(벽/지형지물 — 바닥 착지는 노말 Z가 커서 제외)
	const bool bHitPawn = Cast<APawn>(OtherActor) != nullptr;
	const bool bHitVerticalSurface = !bHitPawn && Hit.ImpactNormal.Z < 0.7f;
	if (!bHitPawn && !bHitVerticalSurface) return;

	// 속도 게이트는 벽/지형(수직면)에만 적용. Pawn 충돌은 armed(돌진 중)만으로 유효 판정 —
	// 충돌 프레임에서 CMC가 블로킹을 해소하며 GetVelocity가 순항 속도보다 저평가돼
	// 적/플레이어 타격이 간헐적으로 누락되던 문제를 제거한다.
	if (!bHitPawn && GetVelocity().Size2D() < DashImpactMinSpeed) return;

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

	if (bNew)
	{
		// 발동 1회당 1 증가 — bIsJetJumping이 이미 true여도 변화가 생겨
		// FP ABP가 "지상 Q → 공중 Q" 2연속에서도 엣지를 감지할 수 있다 (Plan5 §4.6.2)
		++JetJumpCounter;
	}
}

// ========== FP 애니메이션 (Plan5 §5.2) ==========

void ADRRobotVacuumCharacter::ClientPlayDashEndFPMontage_Implementation(bool bFromImpact)
{
	// 소유 클라(리슨 호스트 포함)에서만 도착. FP 메시가 안 보이는 상태(사망 등)면 생략
	UAnimMontage* Montage = bFromImpact ? FPDashCrushMontage.Get() : FPDashStopMontage.Get();
	if (!Montage || !FirstPersonMesh || !FirstPersonMesh->IsVisible()) return;

	if (UAnimInstance* AnimInst = FirstPersonMesh->GetAnimInstance())
	{
		AnimInst->Montage_Play(Montage);
	}
}

// ========== FP 기준 발사 (Plan5 §6) ==========

FVector ADRRobotVacuumCharacter::GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag)
{
	if (MontageTag.MatchesTagExact(FDRGameplayTags::Get().CombatSocket_LeftHand)
		&& FirstPersonMesh && FirstPersonMesh->DoesSocketExist(FPMuzzleSocketName))
	{
		return FirstPersonMesh->GetSocketLocation(FPMuzzleSocketName);
	}
	return Super::GetCombatSocketLocation_Implementation(MontageTag);
}
