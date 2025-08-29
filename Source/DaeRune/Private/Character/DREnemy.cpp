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

	// 사망시 물 부여
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
	GetCharacterMovement()->MaxWalkSpeed = bHitReacting ? 0.f : BaseWalkSpeed;
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

		UE_LOG(LogTemp, Log, TEXT("%s - Attack #%d, Water reduced by %f (Current: %f)"),
			*GetName(), AttackCount, WaterReductionPerAttack, DRAS->GetWater());
	}
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
		// 보스: 전체 맵 플레이어
		UGameplayStatics::GetAllActorsOfClass(
			GetWorld(),
			ADRCharacter::StaticClass(),
			PlayersToGrant
		);

		UE_LOG(LogTemp, Log, TEXT("Boss %s died - Granting MAX water to all %d players"),
			*GetName(), PlayersToGrant.Num());
	}
	else
	{
		// 일반 적: 범위 내 플레이어
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

		UE_LOG(LogTemp, Log, TEXT("Enemy %s died - Granting %f water to %d players (Radius: %f)"),
			*GetName(), CurrentWater, PlayersToGrant.Num(), WaterExplosionRadius);
	}

	// 물 부여 적용
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
				// 보스: MaxWater까지 채우기
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

			UE_LOG(LogTemp, Verbose, TEXT("Granted %f water to %s"),
				WaterAmount, *Player->GetName());
		}
	}
}
