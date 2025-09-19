// Copyright DaeRune


#include "Character/DREnemy.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DREnemyAttributeSet.h"
#include "Components/WidgetComponent.h"
#include "UI/Widget/DRUserWidget.h"
#include "DRGameplayTags.h"
#include "AI/DRAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Character/DRCharacter.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Engine/OverlapResult.h"
#include "Components/CapsuleComponent.h"
#include "DRAbilityTypes.h"

ADREnemy::ADREnemy()
{
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	AbilitySystemComponent = CreateDefaultSubobject<UDRAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;

	AttributeSet = CreateDefaultSubobject<UDREnemyAttributeSet>("AttributeSet");

	HealthBar = CreateDefaultSubobject<UWidgetComponent>("HealthBar");
	HealthBar->SetupAttachment(GetRootComponent());

	BaseWalkSpeed = 250.f;
}

void ADREnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	if (!HasAuthority()) return;
	DRAIController = Cast<ADRAIController>(NewController);
	DRAIController->GetBlackboardComponent()->InitializeBlackboard(*BehaviorTree->BlackboardAsset);
	DRAIController->RunBehaviorTree(BehaviorTree);
	DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), false);
	DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("RangedAttacker"), CharacterClass != ECharacterClass::Warrior);
}

int32 ADREnemy::GetPlayerLevel_Implementation()
{
	return Level;
}

void ADREnemy::Die(const FVector& DeathImpulse)
{
	SetLifeSpan(LifeSpan);
	if (DRAIController) DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("Dead"), true);

	// ����� �� �ο�
	if (HasAuthority())
	{
		GrantWaterToPlayers();
	}

	Super::Die(DeathImpulse);
}

void ADREnemy::SetCombatTarget_Implementation(AActor* InCombatTarget)
{
	CombatTarget = InCombatTarget;
}

AActor* ADREnemy::GetCombatTarget_Implementation() const
{
	return CombatTarget;
}

void ADREnemy::OnAttackExecuted()
{
	if (!HasAuthority()) return;

	AttackCount++;
	ReduceWaterReward();
}

void ADREnemy::HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bHitReacting = NewCount > 0;
	if (bHitReacting)
	{
		// HitReact �߿��� �̵� ����
		GetCharacterMovement()->MaxWalkSpeed = 0.f;
	}
	else
	{
		// HitReact ���� �� GAS �Ӽ������� ����
		if (const UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet))
		{
			GetCharacterMovement()->MaxWalkSpeed = DRAS->GetMoveSpeed();
		}
	}

	if (DRAIController && DRAIController->GetBlackboardComponent())
	{
		DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), bHitReacting);
	}
}

void ADREnemy::ReduceWaterReward()
{
	if (!HasAuthority() || !WaterReductionEffectClass) return;

	const UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet);
	if (!DRAS || DRAS->GetWater() <= 0.f) return;

	FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
	ContextHandle.AddSourceObject(this);

	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		WaterReductionEffectClass,
		1.f,
		ContextHandle
	);

	if (SpecHandle.IsValid())
	{
		const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
		SpecHandle.Data.Get()->SetSetByCallerMagnitude(
			GameplayTags.Water_SetByCaller_Reduction,
			WaterReductionPerAttack
		);

		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}
}

void ADREnemy::SetKnockbackState(bool bInKnockback)
{
	bIsBeingKnockedBack = bInKnockback;
}

void ADREnemy::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	InitAbilityActorInfo();
	if (HasAuthority())
	{
		UDRAbilitySystemLibrary::GiveStartupAbilities(this, AbilitySystemComponent, CharacterClass);
	}

	if (UDRUserWidget* DRUserWidget = Cast<UDRUserWidget>(HealthBar->GetUserWidgetObject()))
	{
		DRUserWidget->SetWidgetController(this);
	}

	if (const UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet))
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(DRAS->GetHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnHealthChanged.Broadcast(Data.NewValue);
			}
		);
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(DRAS->GetMaxHealthAttribute()).AddLambda(
			[this](const FOnAttributeChangeData& Data)
			{
				OnMaxHealthChanged.Broadcast(Data.NewValue);
			}
		);

		AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().Effects_HitReact, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&ADREnemy::HitReactTagChanged
		);

		OnHealthChanged.Broadcast(DRAS->GetHealth());
		OnMaxHealthChanged.Broadcast(DRAS->GetMaxHealth());
	}

	// Physics Hit �̺�Ʈ ���ε�
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ADREnemy::OnHit);
		// ���� �浹 �˸� Ȱ��ȭ
		GetCapsuleComponent()->SetNotifyRigidBodyCollision(true);
	}
}

void ADREnemy::InitAbilityActorInfo()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	Cast<UDRAbilitySystemComponent>(AbilitySystemComponent)->AbilityActorInfoSet();
	AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().Debuff_Stun, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ADREnemy::StunTagChanged);

	if (HasAuthority())
	{
		InitializeDefaultAttributes();
	}
	OnAscRegistered.Broadcast(AbilitySystemComponent);
}

void ADREnemy::InitializeDefaultAttributes() const
{
	UDRAbilitySystemLibrary::InitializeDefaultAttributes(this, CharacterClass, Level, AbilitySystemComponent);
}

void ADREnemy::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	Super::StunTagChanged(CallbackTag, NewCount);

	if (DRAIController && DRAIController->GetBlackboardComponent())
	{
		DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("Stunned"), bIsStunned);
	}
}

void ADREnemy::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	// ���������� ó��
	if (!HasAuthority()) return;

	// �˹� ���°� �ƴϰų� ���� �鿪�̸� ����
	if (!bIsBeingKnockedBack || bIsStunImmune) return;

	// ������ Ȯ��
	if (!OtherActor) return;

	FString ActorName = OtherActor->GetName();

	// Floor�� ����
	if (ActorName.Contains(TEXT("Floor"))) return;

	// StaticMeshActor���� Ȯ�� (��)
	if (!ActorName.Contains(TEXT("StaticMeshActor"))) return;

	// �ӵ� üũ (Impact�� 0�̹Ƿ� �ӵ��� �Ǵ�)
	FVector Velocity = GetVelocity();
	float Speed = Velocity.Size();

	// �ӵ� �Ӱ谪 üũ
	if (Speed > MinSpeedForStun)  // MinSpeedForStun = 50.f
	{
		ApplyWallStun();
	}
}

void ADREnemy::ApplyWallStun()
{
	if (bIsStunImmune || !AbilitySystemComponent) return;

	// AttributeSet �ùٸ� ĳ����
	UDRAttributeSet* BaseAttributeSet = nullptr;

	// ���� DREnemyAttributeSet���� �õ� (Enemy�� �̰� ���)
	if (UDREnemyAttributeSet* EnemyAS = Cast<UDREnemyAttributeSet>(AttributeSet))
	{
		BaseAttributeSet = EnemyAS;  // DREnemyAttributeSet�� DRAttributeSet�� ���
	}
	else if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet))
	{
		BaseAttributeSet = DRAS;
	}

	if (!BaseAttributeSet)
	{
		return;
	}

	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	// EffectProperties ����
	FEffectProperties Props;
	Props.SourceASC = AbilitySystemComponent;
	Props.TargetASC = AbilitySystemComponent;
	Props.SourceAvatarActor = this;
	Props.TargetAvatarActor = this;
	Props.SourceCharacter = this;
	Props.TargetCharacter = this;

	// Context ����
	FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
	ContextHandle.AddSourceObject(this);

	// Ŀ���� ���ؽ�Ʈ ����
	if (FDRGameplayEffectContext* DRContext = static_cast<FDRGameplayEffectContext*>(ContextHandle.Get()))
	{
		DRContext->SetIsSuccessfulDebuff(true);
		DRContext->SetDebuffDamage(0.f);  // �� ������ �߰� ������ ����
		DRContext->SetDebuffDuration(WallStunDuration);
		DRContext->SetDebuffFrequency(0.1f);  // 0�� �ƴ� ���� �� (Period ���� ����)

		// Lightning Ÿ������ ���� (���� ����Ʈ)
		TSharedPtr<FGameplayTag> DamageType = MakeShareable(new FGameplayTag(GameplayTags.Damage_Lightning));
		DRContext->SetDamageType(DamageType);
	}

	Props.EffectContextHandle = ContextHandle;

	// ���� Debuff �ý��� ȣ��
	BaseAttributeSet->Debuff(Props);

	// �˹� ���� ����
	bIsBeingKnockedBack = false;

	// ���� �鿪 ����
	bIsStunImmune = true;

	// �鿪 Ÿ�̸�
	float TotalImmunityTime = WallStunDuration + StunImmunityDuration;
	GetWorld()->GetTimerManager().SetTimer(
		StunImmunityTimerHandle,
		this,
		&ADREnemy::EndStunImmunity,
		TotalImmunityTime,
		false
	);
}

void ADREnemy::EndStunImmunity()
{
	bIsStunImmune = false;
}

void ADREnemy::GrantWaterToPlayers()
{
	if (!HasAuthority() || !WaterGrantEffectClass) return;

	const UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet);
	if (!DRAS) return;

	const float CurrentWater = DRAS->GetWater();
	if (CurrentWater <= 0.f && !bIsBoss) return;

	TArray<AActor*> PlayersToGrant;

	if (bIsBoss)
	{
		// ����: ��ü �� �÷��̾�
		UGameplayStatics::GetAllActorsOfClass(
			GetWorld(),
			ADRCharacter::StaticClass(),
			PlayersToGrant
		);
	}
	else
	{
		// �Ϲ� ��: ���� �� �÷��̾�
		TArray<FOverlapResult> OverlapResults;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);

		GetWorld()->OverlapMultiByChannel(
			OverlapResults,
			GetActorLocation(),
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeSphere(WaterExplosionRadius),
			QueryParams
		);

		for (const FOverlapResult& Result : OverlapResults)
		{
			if (ADRCharacter* Player = Cast<ADRCharacter>(Result.GetActor()))
			{
				PlayersToGrant.Add(Player);
			}
		}
	}

	// �� �ο� ����
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	for (AActor* Player : PlayersToGrant)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Player);
		if (!TargetASC) continue;

		const UDRAttributeSet* TargetAS = Cast<UDRAttributeSet>(
			TargetASC->GetAttributeSet(UDRAttributeSet::StaticClass())
		);
		if (!TargetAS) continue;

		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(this);

		FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
			WaterGrantEffectClass,
			1.f,
			EffectContext
		);

		if (SpecHandle.IsValid())
		{
			float WaterAmount = CurrentWater;

			if (bIsBoss)
			{
				// ����: MaxWater���� ä���
				WaterAmount = TargetAS->GetMaxWater() - TargetAS->GetWater();
			}

			SpecHandle.Data.Get()->SetSetByCallerMagnitude(
				GameplayTags.Water_SetByCaller_Grant,
				WaterAmount
			);

			TargetASC->ApplyGameplayEffectSpecToTarget(
				*SpecHandle.Data.Get(),
				TargetASC
			);
		}
	}
}
