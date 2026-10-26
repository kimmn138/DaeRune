// Copyright DaeRune


#include "Character/DRCharacterBase.h"
#include "AbilitySystemComponent.h"
#include "DRGameplayTags.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/Debuff/DebuffNiagaraComponent.h"
#include "DaeRune/DaeRune.h"
#include "Components/CapsuleComponent.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Game/DRGameModeBase.h" 
#include "Player/DRPlayerController.h"
#include "Character/DRCharacter.h"
#include "Components/AudioComponent.h"
#include "Character/DRFacialExpressionComponent.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Animation/AnimInstance.h"
#include "DaeRune/DRLogChannels.h"

namespace
{
	// 플레이어 사망 → 시체 정리 · 관전 시작까지의 시간 (사망 연출이 끝날 즈음)
	constexpr float PlayerBodyCleanupDelay = 2.2f;
	constexpr float PlayerSpectateDelay = 2.5f;

	// 무기가 붙는 3인칭 메시 소켓 (사망 시 서버에서 떼고, 부활 시 다시 붙인다)
	const FName WeaponAttachSocketName(TEXT("WeaponHandSocket"));
}

ADRCharacterBase::ADRCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get(); 

	// 화상 디버프 나이아가라 컴포넌트 생성
	BurnDebuffComponent = CreateDefaultSubobject<UDebuffNiagaraComponent>("BurnDebuffComponent");
	BurnDebuffComponent->SetupAttachment(GetRootComponent());
	BurnDebuffComponent->DebuffTag = GameplayTags.Debuff_Burn;

	// 스턴 디버프 나이아가라 컴포넌트 생성
	StunDebuffComponent = CreateDefaultSubobject<UDebuffNiagaraComponent>("StunDebuffComponent");
	StunDebuffComponent->SetupAttachment(GetRootComponent());
	StunDebuffComponent->DebuffTag = GameplayTags.Debuff_Stun;

	// 카메라 충돌 무시 설정
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	GetMesh()->SetGenerateOverlapEvents(false);

	// 무기 컴포넌트 생성 및 소켓 부착
	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(), WeaponAttachSocketName);
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Weapon->SetOwnerNoSee(true);
}

void ADRCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 디버프 상태들을 모든 클라이언트에 동기화
	DOREPLIFETIME(ADRCharacterBase, bIsStunned);
	DOREPLIFETIME(ADRCharacterBase, bIsBurned);
	DOREPLIFETIME(ADRCharacterBase, bIsBeingShocked);

	// 사망 상태 (RepNotify 안전망 - Relevancy 회복/늦은 조인 커버)
	DOREPLIFETIME(ADRCharacterBase, bDead);
}

UAbilitySystemComponent* ADRCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UAnimMontage* ADRCharacterBase::GetHitReactMontage_Implementation()
{
	// 배열이 비어있으면 nullptr 반환
	if (HitReactMontages.Num() == 0)
	{
		return nullptr;
	}

	// 배열에 하나만 있으면 그것을 반환
	if (HitReactMontages.Num() == 1)
	{
		return HitReactMontages[0];
	}

	// 여러 개 있으면 랜덤으로 선택
	const int32 RandomIndex = FMath::RandRange(0, HitReactMontages.Num() - 1);
	return HitReactMontages[RandomIndex];
}

void ADRCharacterBase::Die(const FVector& DeathImpulse)
{
	// 플레이어 캐릭터의 경우 특별 처리
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// 의도된 디자인: 플레이어는 corrupt 상태에서만 실제로 사망한다
		// (비-corrupt 상태의 체력 고갈은 corrupt 전환으로 이어질 뿐, 여기서 사망 처리하지 않음)
		const bool bIsCorrupt = AbilitySystemComponent &&
			AbilitySystemComponent->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Corrupt);
		if (!bIsCorrupt)
		{
			return;
		}

		// corrupt 상태에서 죽으면 진짜 사망 처리
		Weapon->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
		MulticastHandleDeath(DeathImpulse);

		// 플레이어가 부품을 들고 있었으면 떨어트리기
		if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(this))
		{
			if (DRCharacter->IsCarryingPart())
			{
				// 사망 시에는 드롭 쿨다운을 무시해야 부품이 캐릭터와 함께 파괴되지 않는다
				DRCharacter->ForceDropCarriedPart();
			}
		}

		// GameMode에 플레이어 사망 알림 (전멸 체크)
		if (ADRGameModeBase* GameMode = GetWorld()->GetAuthGameMode<ADRGameModeBase>())
		{
			APlayerState* PS = GetPlayerState();
			if (PS)
			{
				GameMode->OnPlayerDied(PS);
			}
		}

		// 관전 시작 예약 — 그 전에 부활하면 취소된다 (예약은 PC 가 들고 있다: 폰은 곧 정리된다)
		if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
		{
			DRPC->ScheduleSpectating(PlayerSpectateDelay);
		}

		// 시체 정리 — 그 전에 부활하면 Revive 가 취소한다.
		// 정리된 뒤의 부활은 게임모드가 새 폰을 만들어 처리한다 (ADRGameModeBase::RevivePlayer)
		GetWorldTimerManager().SetTimer(
			DeathCleanupTimerHandle,
			FTimerDelegate::CreateWeakLambda(this, [this]()
			{
				Destroy();
			}),
			PlayerBodyCleanupDelay,
			false
		);
	}
	else
	{
		// AI나 다른 캐릭터는 정상적으로 사망 처리
		Weapon->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
		MulticastHandleDeath(DeathImpulse);
	}
}

FOnDeathSignature& ADRCharacterBase::GetOnDeathDelegate()
{
	return OnDeathDelegate;
}

void ADRCharacterBase::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
	// 중복 사망 방지
	if (bDead) return;

	bDead = true;

	// 죽음 카메라 연출
	if (ADRCharacter* PlayerCharacter = Cast<ADRCharacter>(this))
	{
		PlayerCharacter->PlayDeathCameraAnimation();

		if (AbilitySystemComponent)
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = GetActorLocation();

			AbilitySystemComponent->ExecuteGameplayCue(
				FDRGameplayTags::Get().GameplayCue_Player_Death,
				CueParams
			);
		}

		// 1인칭 메쉬 숨기고 3인칭 메쉬 보이게
		if (PlayerCharacter->IsLocallyControlled())
		{
			if (PlayerCharacter->FirstPersonMesh)
			{
				PlayerCharacter->FirstPersonMesh->SetVisibility(false);
			}

			GetMesh()->SetOwnerNoSee(false);
			GetMesh()->SetVisibility(true);
			if (Weapon)
			{
				Weapon->SetOwnerNoSee(false);
				Weapon->SetVisibility(true);
			}
		}
	}

	// 캡슐 충돌 비활성화
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (UCharacterMovementComponent* CharMoveComp = GetCharacterMovement())
	{
		CharMoveComp->StopMovementImmediately();
		CharMoveComp->DisableMovement();
		// NOTE: SetComponentTickEnabled(false)는 호출하지 않음.
		// ACharacter는 Mesh->AddTickPrerequisiteComponent(CharacterMovement)로 mesh tick을 CMC에 종속시키는데,
		// 서버의 Autonomous Proxy(원격 클라이언트의 캐릭터)에 한해 CMC tick을 끄면 mesh tick까지 함께 멈춰
		// 호스트 시점에서 사망 몽타주가 보이지 않음.
		// MovementMode가 MOVE_None이고 StopMovementImmediately가 호출됐으므로 CMC가 계속 tick해도 부하 미미.
	}

	// 메시를 현재 위치에 고정
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	}

	// Death 표정 적용 (Dissolve 전에 실행)
	if (ADRCharacter* PC = Cast<ADRCharacter>(this))
	{
		if (PC->FacialExpressionComponent)
		{
			PC->FacialExpressionComponent->OnDeath();
		}
	}

	// Dissolve 효과 시작
	Dissolve();

	// 디버프 컴포넌트 비활성화
	if (BurnDebuffComponent)
	{
		BurnDebuffComponent->Deactivate();
	}
	if (StunDebuffComponent)
	{
		StunDebuffComponent->Deactivate();
	}

	// 태그로 켜는 몸 이펙트(공속 버프 · 도발 표시)도 끈다 — bDead 를 보고 꺼진다
	RefreshTagDrivenVFX();

	// RepNotify는 클라이언트에서만 자동 호출됨. 서버에서도 동일 처리를 위해 수동 호출.
	// virtual이므로 ADRCharacter 인스턴스에서는 override가 호출되어 사망 몽타주 재생.
	OnRep_Dead();

	// 사망 이벤트 브로드캐스트
	OnDeathDelegate.Broadcast(this);
}

void ADRCharacterBase::Revive(const FVector& ReviveLocation, float HealthRatio, float WaterRatio)
{
	if (!HasAuthority() || !bDead) return;

	// 사망 처리가 예약해 둔 시체 정리를 취소한다 — 이 폰을 그대로 되살린다
	GetWorldTimerManager().ClearTimer(DeathCleanupTimerHandle);

	// 부활 지점으로 이동 (물리 텔레포트 - 스윕 없이)
	SetActorLocation(ReviveLocation, false, nullptr, ETeleportType::TeleportPhysics);

	PrepareRevive();
	SetReviveAttributes(HealthRatio, WaterRatio);

	// 사망 상태 해제 -> 전 클라에서 상태/연출 복원
	bDead = false;
	MulticastHandleRevive();

	// 관전 예약 취소 + 관전 중이었으면 내 시점 · HUD 로 복귀
	if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(GetController()))
	{
		DRPC->CancelPendingSpectate();
		DRPC->ClientStopSpectating();
	}
}

void ADRCharacterBase::SetReviveAttributes(float HealthRatio, float WaterRatio)
{
	if (!HasAuthority() || !AbilitySystemComponent) return;

	// 어트리뷰트 세트 종류(플레이어 · 적)와 무관하게 공통 Health / Water 만 다룬다
	const UDRAttributeSet* DRAttributes = AbilitySystemComponent->GetSet<UDRAttributeSet>();
	if (!DRAttributes) return;

	const float TargetHealth = FMath::Max(1.f, DRAttributes->GetMaxHealth() * HealthRatio);
	AbilitySystemComponent->SetNumericAttributeBase(DRAttributes->GetHealthAttribute(), TargetHealth);

	const float TargetWater = DRAttributes->GetMaxWater() * WaterRatio;
	AbilitySystemComponent->SetNumericAttributeBase(DRAttributes->GetWaterAttribute(), TargetWater);
}

void ADRCharacterBase::MulticastHandleRevive_Implementation()
{
	// ===== MulticastHandleDeath 의 역연산 (Plan6 §5.9) =====

	// 캡슐 콜리전 복원
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// 이동 복원
	if (UCharacterMovementComponent* CharMoveComp = GetCharacterMovement())
	{
		CharMoveComp->SetMovementMode(MOVE_Walking);
	}

	// 메시 복원 (사망 시 물리 시뮬을 끄고 QueryOnly 로 바꿔둔 상태)
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

		// 사망 몽타주 정지
		if (UAnimInstance* AnimInstance = MeshComp->GetAnimInstance())
		{
			AnimInstance->StopAllMontages(0.1f);
		}

		// 사망 시(서버) 떼어 둔 무기를 다시 손에 붙인다 — 클라에서는 떼지 않았으므로 그대로다
		if (Weapon && Weapon->GetAttachParent() != MeshComp)
		{
			Weapon->AttachToComponent(MeshComp, FAttachmentTransformRules::SnapToTargetNotIncludingScale, WeaponAttachSocketName);
		}
	}

	// 디버프 VFX 컴포넌트는 사망 시 비활성화되었으나, 부활 시엔 디버프가 없는 상태이므로
	// 켜지 않는다 (디버프가 다시 적용되면 각 RepNotify 가 활성화한다).
	// 태그로 켜는 몸 이펙트는 남은 태그 기준으로 다시 맞춘다.
	RefreshTagDrivenVFX();

	// BP 측 복원: Dissolve 머티리얼 파라미터 원복, 사망 카메라 애니메이션 해제, 표정 리셋 등
	K2_OnCharacterRevived();
}

void ADRCharacterBase::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// 이동 속도는 태그를 받은 머신마다 바로 맞춘다
	const bool bStunnedNow = NewCount > 0;
	GetCharacterMovement()->MaxWalkSpeed = bStunnedNow ? StunnedMoveSpeed : GetMoveSpeed();

	// ★복제 변수는 서버만 쓴다★ — 클라가 먼저 도착한 태그로 값을 바꿔 두면 이후 복제가 같은 값이라
	// OnRep_Stunned 가 불리지 않아 입력 차단 · 스턴 VFX 가 빠진다.
	// 서버(Listen Server 호스트)에서는 RepNotify가 자동 호출되지 않으므로 수동 호출
	if (HasAuthority())
	{
		bIsStunned = bStunnedNow;
		OnRep_Stunned();
	}
}

void ADRCharacterBase::OnRep_Stunned()
{
}

void ADRCharacterBase::OnRep_Burned()
{
}

void ADRCharacterBase::OnRep_Dead()
{
	// 기본 구현은 비어있음. 파생 클래스(ADRCharacter)에서 사망 애니메이션 재생.
}

void ADRCharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

void ADRCharacterBase::Destroyed()
{
	// 모든 AudioComponent 안전하게 정리
	TArray<UAudioComponent*> AudioComps;
	GetComponents<UAudioComponent>(AudioComps);

	for (UAudioComponent* AudioComp : AudioComps)
	{
		if (AudioComp && AudioComp->IsPlaying())
		{
			// 페이드 아웃으로 부드럽게 정지
			AudioComp->FadeOut(0.1f, 0.0f);
		}
	}

	Super::Destroyed();
}

FVector ADRCharacterBase::GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag)
{
	// 전투 소켓 위치 반환
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_Weapon) && IsValid(Weapon))
	{
		return Weapon->GetSocketLocation(WeaponTipSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_LeftHand))
	{
		return GetMesh()->GetSocketLocation(LeftHandSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_RightHand))
	{
		return GetMesh()->GetSocketLocation(RightHandSocketName);
	}
	if (MontageTag.MatchesTagExact(GameplayTags.CombatSocket_Tail))
	{
		return GetMesh()->GetSocketLocation(TailSocketName);
	}
	return FVector();
}

bool ADRCharacterBase::IsDead_Implementation() const
{
	return bDead;
}

AActor* ADRCharacterBase::GetAvatar_Implementation()
{
	return this;
}

TArray<FTaggedMontage> ADRCharacterBase::GetAttackMontages_Implementation()
{
	return AttackMontages;
}

UNiagaraSystem* ADRCharacterBase::GetBloodEffect_Implementation()
{
	return BloodEffect;
}

FTaggedMontage ADRCharacterBase::GetTaggedMontageByTag_Implementation(const FGameplayTag& MontageTag)
{
	for (const FTaggedMontage& TaggedMontage : AttackMontages)
	{
		if (TaggedMontage.MontageTag == MontageTag)
		{
			return TaggedMontage;
		}
	}
	return FTaggedMontage();
}

int32 ADRCharacterBase::GetMinionCount_Implementation()
{
	return MinionCount;
}

ECharacterClass ADRCharacterBase::GetCharacterClass_Implementation()
{
	return CharacterClass;
}

FOnASCRegistered& ADRCharacterBase::GetOnASCRegisteredDelegate()
{
	return OnAscRegistered;
}

USkeletalMeshComponent* ADRCharacterBase::GetWeapon_Implementation()
{
	return Weapon;
}

void ADRCharacterBase::SetIsBeingShocked_Implementation(bool bInShock)
{
	bIsBeingShocked = bInShock;
}

bool ADRCharacterBase::IsBeingShocked_Implementation() const
{
	return bIsBeingShocked;
}

void ADRCharacterBase::InitAbilityActorInfo()
{
}

void ADRCharacterBase::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass) const
{
	// BP 에서 기본 속성 GE 를 비워 두면 크래시 대신 경고
	if (!IsValid(GetAbilitySystemComponent()) || !GameplayEffectClass)
	{
		UE_LOG(LogDR, Warning, TEXT("[Character] %s: ASC 또는 GE 클래스가 없어 속성 효과를 적용하지 못했습니다."), *GetName());
		return;
	}
	// 컨텍스트 생성 및 소스 설정
	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	// 스펙 생성 및 적용
	const FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponent()->MakeOutgoingSpec(GameplayEffectClass, Level, ContextHandle);
	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), GetAbilitySystemComponent());
}

void ADRCharacterBase::InitializeDefaultAttributes() const
{
	ApplyEffectToSelf(DefaultPrimaryAttributes);
	ApplyEffectToSelf(DefaultVitalAttributes);
}

void ADRCharacterBase::AddCharacterAbilities()
{
	UDRAbilitySystemComponent* DRASC = CastChecked<UDRAbilitySystemComponent>(AbilitySystemComponent);
	if (!HasAuthority()) return;

	// 액티브 어빌리티와 패시브 어빌리티 추가
	DRASC->AddCharacterAbilities(StartupAbilities);
	DRASC->AddCharacterPassiveAbilities(StartupPassiveAbilities);
}

void ADRCharacterBase::OnMoveSpeedChanged(const FOnAttributeChangeData& Data)
{
	GetCharacterMovement()->MaxWalkSpeed = Data.NewValue;
}

float ADRCharacterBase::GetMoveSpeed()
{
	return BaseWalkSpeed;
}

void ADRCharacterBase::Dissolve()
{
	// 캐릭터 메시 Dissolve
	if (IsValid(DissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicMatInst);
		StartDissolveTimeline(DynamicMatInst);
	}
	// 무기 Dissolve
	if (IsValid(WeaponDissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(WeaponDissolveMaterialInstance, this);
		Weapon->SetMaterial(0, DynamicMatInst);
		StartWeaponDissolveTimeline(DynamicMatInst);
	}
}



