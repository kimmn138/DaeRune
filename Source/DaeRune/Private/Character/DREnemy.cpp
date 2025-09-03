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
	if (bHitReacting)
	{
		// HitReact 중에는 이동 정지
		GetCharacterMovement()->MaxWalkSpeed = 0.f;
	}
	else
	{
		// HitReact 종료 시 GAS 속성값으로 복구
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

		UE_LOG(LogTemp, Log, TEXT("%s - Attack #%d, Water reduced by %f (Current: %f)"),
			*GetName(), AttackCount, WaterReductionPerAttack, DRAS->GetWater());
	}
}

void ADREnemy::SetKnockbackState(bool bInKnockback)
{
	bIsBeingKnockedBack = bInKnockback;

	if (bInKnockback)
	{
		UE_LOG(LogTemp, Log, TEXT("%s: Knockback started"), *GetName());
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("%s: Knockback ended"), *GetName());
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

	// Physics Hit 이벤트 바인딩
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ADREnemy::OnHit);
		// 물리 충돌 알림 활성화
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
	// 서버에서만 처리
	if (!HasAuthority()) return;

	// 넉백 상태가 아니거나 스턴 면역이면 무시
	if (!bIsBeingKnockedBack || bIsStunImmune) return;

	// 벽인지 확인
	if (!OtherActor) return;

	FString ActorName = OtherActor->GetName();

	// Floor는 무시
	if (ActorName.Contains(TEXT("Floor"))) return;

	// StaticMeshActor인지 확인 (벽)
	if (!ActorName.Contains(TEXT("StaticMeshActor"))) return;

	// 속도 체크 (Impact가 0이므로 속도로 판단)
	FVector Velocity = GetVelocity();
	float Speed = Velocity.Size();

	UE_LOG(LogTemp, Warning, TEXT("%s hit wall - Speed: %f"), *GetName(), Speed);

	// 속도 임계값 체크
	if (Speed > MinSpeedForStun)  // MinSpeedForStun = 50.f
	{
		UE_LOG(LogTemp, Warning, TEXT("Applying wall stun!"));
		ApplyWallStun();
	}
}

void ADREnemy::ApplyWallStun()
{
	if (bIsStunImmune || !AbilitySystemComponent) return;

	// AttributeSet 올바른 캐스팅
	UDRAttributeSet* BaseAttributeSet = nullptr;

	// 먼저 DREnemyAttributeSet으로 시도 (Enemy는 이걸 사용)
	if (UDREnemyAttributeSet* EnemyAS = Cast<UDREnemyAttributeSet>(AttributeSet))
	{
		BaseAttributeSet = EnemyAS;  // DREnemyAttributeSet은 DRAttributeSet을 상속
	}
	else if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet))
	{
		BaseAttributeSet = DRAS;
	}

	if (!BaseAttributeSet)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to cast AttributeSet"));
		return;
	}

	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	// EffectProperties 구성
	FEffectProperties Props;
	Props.SourceASC = AbilitySystemComponent;
	Props.TargetASC = AbilitySystemComponent;
	Props.SourceAvatarActor = this;
	Props.TargetAvatarActor = this;
	Props.SourceCharacter = this;
	Props.TargetCharacter = this;

	// Context 생성
	FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
	ContextHandle.AddSourceObject(this);

	// 커스텀 컨텍스트 설정
	if (FDRGameplayEffectContext* DRContext = static_cast<FDRGameplayEffectContext*>(ContextHandle.Get()))
	{
		DRContext->SetIsSuccessfulDebuff(true);
		DRContext->SetDebuffDamage(0.f);  // 벽 스턴은 추가 데미지 없음
		DRContext->SetDebuffDuration(WallStunDuration);
		DRContext->SetDebuffFrequency(0.1f);  // 0이 아닌 작은 값 (Period 문제 방지)

		// Lightning 타입으로 설정 (기절 이펙트)
		TSharedPtr<FGameplayTag> DamageType = MakeShareable(new FGameplayTag(GameplayTags.Damage_Lightning));
		DRContext->SetDamageType(DamageType);
	}

	Props.EffectContextHandle = ContextHandle;

	// 기존 Debuff 시스템 호출
	BaseAttributeSet->Debuff(Props);

	UE_LOG(LogTemp, Warning, TEXT("Wall stun applied using existing Debuff system"));

	// 넉백 상태 해제
	bIsBeingKnockedBack = false;

	// 스턴 면역 설정
	bIsStunImmune = true;

	// 면역 타이머
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
	UE_LOG(LogTemp, Log, TEXT("%s: Stun immunity ended, can be stunned again"), *GetName());
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
