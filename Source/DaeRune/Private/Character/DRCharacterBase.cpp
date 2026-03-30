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
		// ���� �������� Ȯ��
		if (AbilitySystemComponent && AbilitySystemComponent->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Corrupt))
		{
			// ���� ���¿��� ������ ��¥ ���
			Weapon->DetachFromComponent(FDetachmentTransformRules(EDetachmentRule::KeepWorld, true));
			MulticastHandleDeath(DeathImpulse);

			// �÷��̾ ��ǰ�� ��� ������ ����߸���
			if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(this))
			{
				if (DRCharacter->IsCarryingPart())
				{
					DRCharacter->DropCarriedPart();
				}
			}

			// GameMode�� �÷��̾� ��� �˸� (���� üũ)
			if (ADRGameModeBase* GameMode = GetWorld()->GetAuthGameMode<ADRGameModeBase>())
			{
				APlayerState* PS = GetPlayerState();
				if (PS)
				{
					GameMode->OnPlayerDied(PS);
				}
			}

			// ���� ����
			if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
			{
				// ���� ä���� ���� ���·� ������Ʈ
				DRPC->UpdateVoiceChannelForDeathState(true);

				// ������ �� ���� ��� ��ȯ
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
					4.0f,
					false
				);
			}

			// ĳ���� ���� �ı�
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
			   3.5f,
			   false
			);
		}
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
		CharMoveComp->SetComponentTickEnabled(false);
	}

	// �޽ø� ���� ��ġ�� ����
	if (USkeletalMeshComponent* MeshComp = GetMesh())
	{
		MeshComp->SetSimulatePhysics(false);
		MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
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

	// ��� �̺�Ʈ ��ε�ĳ��Ʈ
	OnDeathDelegate.Broadcast(this);

	if (UWorld* World = GetWorld())
	{
		if (APlayerController* LocalPC = World->GetFirstPlayerController())
		{
			if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(LocalPC))
			{
				DRPC->RefreshAllPlayerVoiceMutes();
			}
		}
	}
}

void ADRCharacterBase::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bIsStunned = NewCount > 0;
	GetCharacterMovement()->MaxWalkSpeed = bIsStunned ? StunnedMoveSpeed : GetMoveSpeed();
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



