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
	GetMesh()->SetGenerateOverlapEvents(true);

	// 무기 컴포넌트 생성 및 소켓 부착
	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(), FName("WeaponHandSocket"));
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	// 디버프 상태들을 모든 클라이언트에 동기화
	DOREPLIFETIME(ADRCharacterBase, bIsStunned);
	DOREPLIFETIME(ADRCharacterBase, bIsBurned);
	DOREPLIFETIME(ADRCharacterBase, bIsBeingShocked);
}

UAbilitySystemComponent* ADRCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UAnimMontage* ADRCharacterBase::GetHitReactMontage_Implementation()
{
	return HitReactMontage;
}

void ADRCharacterBase::Die(const FVector& DeathImpulse)
{
	// 플레이어 캐릭터인 경우 특별 처리
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		// 부패 상태인지 확인
		if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Corrupt))
		{
			// 부패 상태에서 죽으면 진짜 사망
			Weapon->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
			MulticastHandleDeath(DeathImpulse);

			// 플레이어가 부품을 들고 있으면 떨어뜨리기
			if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(this))
			{
				if (DRCharacter->IsCarryingPart())
				{
					DRCharacter->DropCarriedPart();
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
				// 약간의 딜레이 후 관전 모드 전환
				FTimerHandle SpectatorTimerHandle;
				GetWorld()->GetTimerManager().SetTimer(
					SpectatorTimerHandle,
					[DRPC]()
					{
						if (IsValid(DRPC))
						{
							DRPC->ClientStartSpectating();
						}
					},
					3.0f,
					false
				);
			}

			// 캐릭터 액터는 바로 파괴
			FTimerHandle DestroyTimerHandle;
			GetWorld()->GetTimerManager().SetTimer(
			   DestroyTimerHandle,
			   [this]()
			   {
				  if (IsValid(this))
				  {
					 Destroy();
				  }
			   },
			   2.5f,
			   false
			);
		}
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

	// 사망 사운드 재생
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation(), GetActorRotation());
	}

	// 무기에 물리 시뮬레이션 적용
	if (Weapon)
	{
		Weapon->SetSimulatePhysics(true);
		Weapon->SetEnableGravity(true);
		Weapon->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		Weapon->AddImpulse(DeathImpulse * 0.1f, NAME_None, true);
	}

	// 캐릭터 메시에 물리 시뮬레이션 적용
	if (GetMesh())
	{
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->SetEnableGravity(true);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		GetMesh()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		GetMesh()->AddImpulse(DeathImpulse, NAME_None, true);
	}

	// 캡슐 충돌 비활성화
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
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

	// 사망 이벤트 브로드캐스트
	OnDeathDelegate.Broadcast(this);
}

void ADRCharacterBase::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bIsStunned = NewCount > 0;
	GetCharacterMovement()->MaxWalkSpeed = bIsStunned ? 0.f : GetMoveSpeed();
}

void ADRCharacterBase::OnRep_Stunned()
{
}

void ADRCharacterBase::OnRep_Burned()
{
}

void ADRCharacterBase::BeginPlay()
{
	Super::BeginPlay();
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
	for (FTaggedMontage TaggedMontage : AttackMontages)
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



