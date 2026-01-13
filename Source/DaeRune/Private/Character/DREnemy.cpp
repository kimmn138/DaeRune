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
	PrimaryActorTick.bCanEverTick = false;
	PrimaryActorTick.bStartWithTickEnabled = false;

	// 메시 가시성 충돌 설정
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// GAS 컴포넌트 초기화 - 리슨서버용 최소 리플리케이션
	AbilitySystemComponent = CreateDefaultSubobject<UDRAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// AI 회전 설정 - 컨트롤러 기반 부드러운 회전
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;

	// 적 전용 어트리뷰트셋
	AttributeSet = CreateDefaultSubobject<UDREnemyAttributeSet>("AttributeSet");

	// 부품 메시 컴포넌트 생성 (선택적)
	PartMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>("PartMesh");
	PartMeshComponent->SetupAttachment(GetMesh(), "PartSocket");
	PartMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PartMeshComponent->SetVisibility(false); // 기본적으로 숨김
	PartMeshComponent->SetIsReplicated(true);

	BaseWalkSpeed = 250.f;
}

void ADREnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버에서만 AI 초기화
	if (!HasAuthority()) return;
	DRAIController = Cast<ADRAIController>(NewController);

	if (!DRAIController || !BehaviorTree || !BehaviorTree->BlackboardAsset) return;

	// 블랙보드 초기화 및 비헤이비어 트리 실행
	DRAIController->GetBlackboardComponent()->InitializeBlackboard(*BehaviorTree->BlackboardAsset);
	DRAIController->RunBehaviorTree(BehaviorTree);
	// 초기 AI 상태 설정
	DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), false);
	DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("RangedAttacker"), CharacterClass != ECharacterClass::Warrior);
	DRAIController->GetBlackboardComponent()->SetValueAsVector(FName("HomeLocation"), GetActorLocation());
}

int32 ADREnemy::GetPlayerLevel_Implementation()
{
	return Level;
}

void ADREnemy::Die(const FVector& DeathImpulse)
{
	// 죽을 때 부품 자동 드랍
	if (HasPart())
	{
		DropPart();
	}

	// Death Ability 발동
	if (HasAuthority())
	{
		ActivateDeathAbilities();
	}

	// 사망 처리 - 일정 시간 후 소멸
	SetLifeSpan(LifeSpan);
	// AI 상태 업데이트
	if (DRAIController) DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("Dead"), true);

	// 사망시 플레이어들에게 물 보상 지급
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

UBlackboardComponent* ADREnemy::GetBlackboardComponent() const
{
	if (DRAIController)
	{
		return DRAIController->GetBlackboardComponent();
	}
	return nullptr;
}

void ADREnemy::OnAttackExecuted()
{
	if (!HasAuthority()) return;

	// 공격 횟수 증가 및 물 보상 감소
	AttackCount++;
	ReduceWaterReward();
}

void ADREnemy::HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bHitReacting = NewCount > 0;
	GetCharacterMovement()->MaxWalkSpeed = bHitReacting ? 200.f : GetMoveSpeed();

	// AI 블랙보드 상태 업데이트
	if (DRAIController && DRAIController->GetBlackboardComponent())
	{
		DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), bHitReacting);
	}
}

void ADREnemy::ActivateDeathAbilities()
{
	if (!AbilitySystemComponent) return;

	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	// "Ability.Death" 태그를 가진 모든 어빌리티 발동
	FGameplayTagContainer DeathTags;
	DeathTags.AddTag(GameplayTags.Abilities_Death);

	// 발동 시도
	bool bActivated = AbilitySystemComponent->TryActivateAbilitiesByTag(DeathTags);
}

void ADREnemy::ReduceWaterReward()
{
	if (!HasAuthority() || !WaterReductionEffectClass) return;

	// 현재 물이 없으면 감소시키지 않음
	const UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet);
	if (!DRAS || DRAS->GetWater() <= 0.f) return;

	// GameplayEffect로 물 감소 적용
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
		// SetByCaller로 감소량 설정
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

bool ADREnemy::DropPart()
{
	if (!HasAuthority() || !bCarriesPart || bPartDropped || !PartActorClass)
		return false;

	// 부품 메시 숨기기
	if (PartMeshComponent)
	{
		PartMeshComponent->SetVisibility(false);
	}

	// 실제 부품 액터 스폰
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

	FVector SpawnLocation = GetActorLocation() + GetActorUpVector() * 10.f;
	if (PartMeshComponent && PartMeshComponent->IsVisible())
	{
		SpawnLocation = PartMeshComponent->GetComponentLocation();
	}

	AActor* DroppedPart = GetWorld()->SpawnActor<AActor>(
		PartActorClass,
		SpawnLocation,
		GetActorRotation(),
		SpawnParams);

	bPartDropped = true;
	bCarriesPart = false;

	// 블랙보드 업데이트
	if (DRAIController && DRAIController->GetBlackboardComponent())
	{
		DRAIController->GetBlackboardComponent()->SetValueAsBool("HasPart", false);
	}

	return true;
}

void ADREnemy::TriggerEnrage()
{
	if (!HasAuthority() || bIsEnraged || !bIsPhase3Enemy) return;
    
	bIsEnraged = true;
    
	// 블랙보드에 광폭화 상태 설정
	if (ADRAIController* AIController = Cast<ADRAIController>(GetController()))
	{
		if (UBlackboardComponent* BB = AIController->GetBlackboardComponent())
		{
			BB->SetValueAsBool(FName("bIsEnraged"), true);

			float CurrentAttackSpeed = BB->GetValueAsFloat(FName("AttackSpeed"));
			BB->SetValueAsFloat(FName("AttackSpeed"), CurrentAttackSpeed / 2.f);

			float CurrentEliteAttackSpeed = BB->GetValueAsFloat(FName("EliteAttackSpeed"));
			BB->SetValueAsFloat(FName("EliteAttackSpeed"), CurrentEliteAttackSpeed / 2.f);
		}
	}
    
	// 이동속도 증가 GE 적용
	if (EnrageMovementSpeedGE && AbilitySystemComponent)
	{
		FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
		EffectContext.AddSourceObject(this);
        
		FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(EnrageMovementSpeedGE, 1, EffectContext);
            
		if (SpecHandle.IsValid())
		{
			AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
}

void ADREnemy::BeginPlay()
{
	Super::BeginPlay();
	
	// GAS 초기화
	InitAbilityActorInfo();
	// 서버에서만 시작 어빌리티 부여
	if (HasAuthority())
	{
		UDRAbilitySystemLibrary::GiveStartupAbilities(this, AbilitySystemComponent, CharacterClass);

		// CharacterClassInfo에서 DeathAbilities 정보 가져오기
		if (UCharacterClassInfo* CharacterClassInfo = UDRAbilitySystemLibrary::GetCharacterClassInfo(this))
		{
			FCharacterClassDefaultInfo ClassInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);
			DeathAbilities = ClassInfo.DeathAbilities;
		}
	}

	// 어트리뷰트 변화 이벤트 바인딩
	if (const UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet))
	{
		// 체력 변화 델리게이트 바인딩
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

		// 히트 리액션 태그 이벤트 바인딩
		AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().Effects_HitReact, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&ADREnemy::HitReactTagChanged
		);

		// 초기값 브로드캐스트
		OnHealthChanged.Broadcast(DRAS->GetHealth());
		OnMaxHealthChanged.Broadcast(DRAS->GetMaxHealth());
	}

	// 물리 충돌 이벤트 바인딩 (넉백 처리용)
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->OnComponentHit.AddDynamic(this, &ADREnemy::OnHit);
		GetCapsuleComponent()->SetNotifyRigidBodyCollision(true);
	}

	// 부품 운반자인 경우 설정
	if (bCarriesPart)
	{
		if (PartMeshComponent)
		{
			PartMeshComponent->SetVisibility(true);
		}

		// 블랙보드에 부품 보유 상태 설정
		if (HasAuthority() && DRAIController && DRAIController->GetBlackboardComponent())
		{
			DRAIController->GetBlackboardComponent()->SetValueAsBool("HasPart", true);
		}
	}
}

void ADREnemy::InitAbilityActorInfo()
{
	// GAS 초기화
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	Cast<UDRAbilitySystemComponent>(AbilitySystemComponent)->AbilityActorInfoSet();
	// 스턴 태그 이벤트 바인딩
	AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().Debuff_Stun, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ADREnemy::StunTagChanged);

	// 서버에서만 기본 어트리뷰트 초기화
	if (HasAuthority())
	{
		InitializeDefaultAttributes();
	}
	OnAscRegistered.Broadcast(AbilitySystemComponent);
}

void ADREnemy::InitializeDefaultAttributes() const
{
	// 캐릭터 클래스와 레벨에 따른 기본 어트리뷰트 초기화
	UDRAbilitySystemLibrary::InitializeDefaultAttributes(this, CharacterClass, Level, AbilitySystemComponent);
}

void ADREnemy::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	Super::StunTagChanged(CallbackTag, NewCount);

	// AI 블랙보드에 스턴 상태 업데이트
	if (DRAIController && DRAIController->GetBlackboardComponent())
	{
		UBlackboardComponent* BB = DRAIController->GetBlackboardComponent();
		BB->SetValueAsBool(FName("Stunned"), bIsStunned);

		// 스턴 시 타겟 정보 초기화 (어그로 리셋)
		if (bIsStunned)
		{
			BB->ClearValue("FirstAttacker");
			BB->SetValueAsBool("HasFirstAttacker", false);
			BB->ClearValue("TargetToFollow");
		}
	}
}

float ADREnemy::GetMoveSpeed()
{
	if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet))
	{
		return DRAS->GetMoveSpeed();
	}
	return Super::GetMoveSpeed();
}

void ADREnemy::OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit)
{
	// 서버에서만 넉백 처리
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

	// 속도 임계값 체크
	if (Speed > MinSpeedForStun)  // MinSpeedForStun = 50.f
	{
		ApplyWallStun();
	}
}

void ADREnemy::ApplyWallStun()
{
	if (bIsStunImmune || !AbilitySystemComponent) return;

	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
    
	// AttributeSet에서 GE 클래스 가져오기
	UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet);
	if (!DRAS) return;

	TSubclassOf<UGameplayEffect>* StunEffectClass = DRAS->DebuffEffectMap.Find(GameplayTags.Debuff_Stun);
	if (!StunEffectClass || !(*StunEffectClass)) return;

	// Context 생성
	FGameplayEffectContextHandle ContextHandle = AbilitySystemComponent->MakeEffectContext();
	ContextHandle.AddSourceObject(this);

	// Spec 생성
	FGameplayEffectSpecHandle SpecHandle = AbilitySystemComponent->MakeOutgoingSpec(
		*StunEffectClass,
		1.f,
		ContextHandle
	);

	if (!SpecHandle.IsValid()) return;

	SpecHandle.Data->SetDuration(WallStunDuration, true);

	// 적용
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	// 넉백 해제 및 면역 설정
	bIsBeingKnockedBack = false;
	bIsStunImmune = true;

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
	// 일반 적은 물이 없으면 지급하지 않음, 보스는 항상 지급
	if (CurrentWater <= 0.f && !bIsBoss) return;

	TArray<AActor*> PlayersToGrant;

	if (bIsBoss)
	{
		// 보스: 맵 전체 플레이어에게 지급
		UGameplayStatics::GetAllActorsOfClass(
			GetWorld(),
			ADRCharacter::StaticClass(),
			PlayersToGrant
		);
	}
	else
	{
		// 일반 적: 폭발 반경 내 플레이어에게만 지급
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

	// 각 플레이어에게 물 지급 GameplayEffect 적용
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
				// 보스는 MaxWater까지 채워주기
				WaterAmount = TargetAS->GetMaxWater() - TargetAS->GetWater();
			}

			// SetByCaller로 지급량 설정
			SpecHandle.Data.Get()->SetSetByCallerMagnitude(
				GameplayTags.Water_SetByCaller_Grant,
				WaterAmount
			);

			// 타겟에게 효과 적용
			TargetASC->ApplyGameplayEffectSpecToTarget(
				*SpecHandle.Data.Get(),
				TargetASC
			);
		}
	}
}
