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

ADRCharacterBase::ADRCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get(); 

	// ȭ�� ����� ���̾ư��� ������Ʈ ����
	BurnDebuffComponent = CreateDefaultSubobject<UDebuffNiagaraComponent>("BurnDebuffComponent");
	BurnDebuffComponent->SetupAttachment(GetRootComponent());
	BurnDebuffComponent->DebuffTag = GameplayTags.Debuff_Burn;

	// ���� ����� ���̾ư��� ������Ʈ ����
	StunDebuffComponent = CreateDefaultSubobject<UDebuffNiagaraComponent>("StunDebuffComponent");
	StunDebuffComponent->SetupAttachment(GetRootComponent());
	StunDebuffComponent->DebuffTag = GameplayTags.Debuff_Stun;

	// ī�޶� �浹 ���� ����
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	GetMesh()->SetGenerateOverlapEvents(false);

	// ���� ������Ʈ ���� �� ���� ����
	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(), FName("WeaponHandSocket"));
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
	// �迭�� ��������� nullptr ��ȯ
	if (HitReactMontages.Num() == 0)
	{
		return nullptr;
	}

	// �迭�� �ϳ��� ������ �װ��� ��ȯ
	if (HitReactMontages.Num() == 1)
	{
		return HitReactMontages[0];
	}

	// ���� �� ������ �������� ����
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

		// 관전 시작
		if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
		{
			// 딜레이 후 관전 모드 전환
			FTimerHandle SpectatorTimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
				SpectatorTimerHandle,
				FTimerDelegate::CreateWeakLambda(DRPC, [DRPC]()
				{
					DRPC->ClientStartSpectating();
				}),
				2.5f,
				false
			);
		}

		// 캐릭터 액터 파괴
		FTimerHandle DestroyTimerHandle;
		GetWorld()->GetTimerManager().SetTimer(
		   DestroyTimerHandle,
		   FTimerDelegate::CreateWeakLambda(this, [this]()
		   {
			  Destroy();
		   }),
		   2.2f,
		   false
		);
	}
	else
	{
		// AI�� �ٸ� ĳ���ʹ� ���������� ��� ó��
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

	// ���� ī�޶� ����
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

		// 1��Ī �޽� ����� 3��Ī �޽� ���̰�
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

	// ĸ�� �浹 ��Ȱ��ȭ
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

	// �޽ø� ���� ��ġ�� ����
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

	// Dissolve ȿ�� ����
	Dissolve();

	// ����� ������Ʈ ��Ȱ��ȭ
	if (BurnDebuffComponent)
	{
		BurnDebuffComponent->Deactivate();
	}
	if (StunDebuffComponent)
	{
		StunDebuffComponent->Deactivate();
	}

	// RepNotify는 클라이언트에서만 자동 호출됨. 서버에서도 동일 처리를 위해 수동 호출.
	// virtual이므로 ADRCharacter 인스턴스에서는 override가 호출되어 사망 몽타주 재생.
	OnRep_Dead();

	// ��� �̺�Ʈ ��ε�ĳ��Ʈ
	OnDeathDelegate.Broadcast(this);
}

void ADRCharacterBase::Revive(const FVector& ReviveLocation, float HealthRatio, float WaterRatio)
{
	if (!HasAuthority() || !bDead) return;

	// 부활 지점으로 이동 (물리 텔레포트 - 스윕 없이)
	SetActorLocation(ReviveLocation, false, nullptr, ETeleportType::TeleportPhysics);

	// 어트리뷰트 복원. 어트리뷰트 세트 종류에 무관하게 태그 기반으로 처리하지 않고
	// 기본 Health/MaxHealth 만 다루므로 UDRAttributeSet 공통 경로를 사용한다.
	if (AbilitySystemComponent)
	{
		if (const UDRAttributeSet* DRAttributes = Cast<UDRAttributeSet>(
			AbilitySystemComponent->GetAttributeSet(UDRAttributeSet::StaticClass())))
		{
			const float TargetHealth = FMath::Max(1.f, DRAttributes->GetMaxHealth() * HealthRatio);
			AbilitySystemComponent->SetNumericAttributeBase(DRAttributes->GetHealthAttribute(), TargetHealth);

			const float TargetWater = DRAttributes->GetMaxWater() * WaterRatio;
			AbilitySystemComponent->SetNumericAttributeBase(DRAttributes->GetWaterAttribute(), TargetWater);
		}
	}

	// 사망 상태 해제 -> 전 클라에서 상태/연출 복원
	bDead = false;
	MulticastHandleRevive();
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
	}

	// 디버프 VFX 컴포넌트는 사망 시 비활성화되었으나, 부활 시엔 디버프가 없는 상태이므로
	// 켜지 않는다 (디버프가 다시 적용되면 각 RepNotify 가 활성화한다).

	// BP 측 복원: Dissolve 머티리얼 파라미터 원복, 사망 카메라 애니메이션 해제, 표정 리셋 등
	K2_OnCharacterRevived();
}

void ADRCharacterBase::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bIsStunned = NewCount > 0;
	GetCharacterMovement()->MaxWalkSpeed = bIsStunned ? StunnedMoveSpeed : GetMoveSpeed();

	// 서버(Listen Server 호스트)에서는 RepNotify가 자동 호출되지 않으므로 수동 호출
	if (HasAuthority())
	{
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
	// ��� AudioComponent �����ϰ� ����
	TArray<UAudioComponent*> AudioComps;
	GetComponents<UAudioComponent>(AudioComps);

	for (UAudioComponent* AudioComp : AudioComps)
	{
		if (AudioComp && AudioComp->IsPlaying())
		{
			// ���̵� �ƿ����� �ε巴�� ����
			AudioComp->FadeOut(0.1f, 0.0f);
		}
	}

	Super::Destroyed();
}

FVector ADRCharacterBase::GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag)
{
	// ���� ���� ��ġ ��ȯ
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

void ADRCharacterBase::IncremenetMinionCount_Implementation(int32 Amount)
{
	MinionCount += Amount;
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
	check(IsValid(GetAbilitySystemComponent()));
	check(GameplayEffectClass);
	// ���ؽ�Ʈ ���� �� �ҽ� ����
	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	// ���� ���� �� ����
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

	// ��Ƽ�� �����Ƽ�� �нú� �����Ƽ �߰�
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
	// ĳ���� �޽� Dissolve
	if (IsValid(DissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicMatInst);
		StartDissolveTimeline(DynamicMatInst);
	}
	// ���� Dissolve
	if (IsValid(WeaponDissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(WeaponDissolveMaterialInstance, this);
		Weapon->SetMaterial(0, DynamicMatInst);
		StartWeaponDissolveTimeline(DynamicMatInst);
	}
}



