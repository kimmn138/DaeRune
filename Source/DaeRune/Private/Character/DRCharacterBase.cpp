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

ADRCharacterBase::ADRCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get(); 

	BurnDebuffComponent = CreateDefaultSubobject<UDebuffNiagaraComponent>("BurnDebuffComponent");
	BurnDebuffComponent->SetupAttachment(GetRootComponent());
	BurnDebuffComponent->DebuffTag = GameplayTags.Debuff_Burn;

	StunDebuffComponent = CreateDefaultSubobject<UDebuffNiagaraComponent>("StunDebuffComponent");
	StunDebuffComponent->SetupAttachment(GetRootComponent());
	StunDebuffComponent->DebuffTag = GameplayTags.Debuff_Stun;

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	GetMesh()->SetGenerateOverlapEvents(true);

	Weapon = CreateDefaultSubobject<USkeletalMeshComponent>("Weapon");
	Weapon->SetupAttachment(GetMesh(), FName("WeaponHandSocket"));
	Weapon->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

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

			// 관전자 모드로 전환
			if (PC)
			{
				// 약간의 딜레이 후 관전 모드 전환 (안전을 위해)
				FTimerHandle SpectatorTimerHandle;
				GetWorld()->GetTimerManager().SetTimer(
					SpectatorTimerHandle,
					[PC]()
					{
						PC->StartSpectatingOnly();
					},
					0.5f,
					false
				);
			}
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
	// 이미 죽은 상태면 무시
	if (bDead) return;

	bDead = true;

	// 사운드 재생
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation(), GetActorRotation());
	}

	// 무기 물리 적용
	if (Weapon)
	{
		Weapon->SetSimulatePhysics(true);
		Weapon->SetEnableGravity(true);
		Weapon->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		Weapon->AddImpulse(DeathImpulse * 0.1f, NAME_None, true);
	}

	// 메시 물리 적용
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

	// Dissolve 효과
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

	// Death 델리게이트 브로드캐스트
	OnDeathDelegate.Broadcast(this);
}

void ADRCharacterBase::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bIsStunned = NewCount > 0;
	GetCharacterMovement()->MaxWalkSpeed = bIsStunned ? 0.f : BaseWalkSpeed;
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

void ADRCharacterBase::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const
{
	check(IsValid(GetAbilitySystemComponent()));
	check(GameplayEffectClass);
	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
	ContextHandle.AddSourceObject(this);
	const FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponent()->MakeOutgoingSpec(GameplayEffectClass, Level, ContextHandle);
	GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), GetAbilitySystemComponent());
}

void ADRCharacterBase::InitializeDefaultAttributes() const
{
	ApplyEffectToSelf(DefaultPrimaryAttributes, 1.f);
	ApplyEffectToSelf(DefaultVitalAttributes, 1.f);
}

void ADRCharacterBase::AddCharacterAbilities()
{
	UDRAbilitySystemComponent* DRASC = CastChecked<UDRAbilitySystemComponent>(AbilitySystemComponent);
	if (!HasAuthority()) return;

	DRASC->AddCharacterAbilities(StartupAbilities);
	DRASC->AddCharacterPassiveAbilities(StartupPassiveAbilities);
}

void ADRCharacterBase::Dissolve()
{
	if (IsValid(DissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicMatInst);
		StartDissolveTimeline(DynamicMatInst);
	}
	if (IsValid(WeaponDissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(WeaponDissolveMaterialInstance, this);
		Weapon->SetMaterial(0, DynamicMatInst);
		StartWeaponDissolveTimeline(DynamicMatInst);
	}
}



