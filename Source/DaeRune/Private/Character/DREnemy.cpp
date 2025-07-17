// Copyright DaeRune


#include "Character/DREnemy.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Components/WidgetComponent.h"
#include "UI/Widget/DRUserWidget.h"
#include "DRGameplayTags.h"
#include "AI/DRAIController.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

ADREnemy::ADREnemy()
{
	// 메시 가시성 충돌 응답 설정
	GetMesh()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	// 어빌리티 시스템 컴포넌트 생성 및 복제 설정 처리
	AbilitySystemComponent = CreateDefaultSubobject<UDRAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);

	// 컨트롤러 회전 사용 설정
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bUseControllerDesiredRotation = true;

	// 어트리뷰트 세트 컴포넌트 생성
	AttributeSet = CreateDefaultSubobject<UDRAttributeSet>("AttributeSet");

	// 체력바 위젯 컴포넌트 생성 및 부모 연결
	HealthBar = CreateDefaultSubobject<UWidgetComponent>("HealthBar");
	HealthBar->SetupAttachment(GetRootComponent());

	// 기본 이동 속도 설정
	BaseWalkSpeed = 250.f;
}

// 소유 시 AI 초기화 및 행동 트리 실행 처리
void ADREnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// 서버 권한 검사 처리
	if (!HasAuthority()) return;
	// AI 컨트롤러 캐스팅 처리
	DRAIController = Cast<ADRAIController>(NewController);
	// 블랙보드 초기화
	DRAIController->GetBlackboardComponent()->InitializeBlackboard(*BehaviorTree->BlackboardAsset);
	// 행동 트리 실행 처리
	DRAIController->RunBehaviorTree(BehaviorTree);
	// 블랙보드 변수 초기 설정 처리
	DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), false);
	DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("RangedAttacker"), CharacterClass != EEnemyCharacterClass::Warrior);
}

// 레벨 반환 구현
int32 ADREnemy::GetPlayerLevel_Implementation()
{
	return Level;
}

// 클래스 반환 구현
EEnemyCharacterClass ADREnemy::GetEnemyCharacterClass_Implementation()
{
	return CharacterClass;
}

// 사망 처리 구현
void ADREnemy::Die(const FVector& DeathImpulse)
{
	// 수명 설정 처리
	SetLifeSpan(LifeSpan);
	// 블랙보드 사망 상태 설정 처리
	if (DRAIController) DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("Dead"), true);

	Super::Die(DeathImpulse);
}

// 전투 대상 설정 구현
void ADREnemy::SetCombatTarget_Implementation(AActor* InCombatTarget)
{
	CombatTarget = InCombatTarget;
}

// 전투 대상 반환 구현
AActor* ADREnemy::GetCombatTarget_Implementation() const
{
	return CombatTarget;
}

// 피격 반응 태그 변경 처리
void ADREnemy::HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// 반응 상태 업데이트 처리
	bHitReacting = NewCount > 0;
	// 이동 속도 업데이트 처리
	GetCharacterMovement()->MaxWalkSpeed = bHitReacting ? 0.f : BaseWalkSpeed;
	// 블랙보드 피격 반응 변수 설정 처리
	if (DRAIController && DRAIController->GetBlackboardComponent())
	{
		DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("HitReacting"), bHitReacting);
	}
}

void ADREnemy::BeginPlay()
{
	Super::BeginPlay();

	// 이동 속도 초기화 처리
	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	// 어빌리티 초기화 호출
	InitAbilityActorInfo();
	// 서버 권한 검사 처리 및 초기 어빌리티 부여 처리
	if (HasAuthority())
	{
		UDRAbilitySystemLibrary::GiveEnemyStartupAbilities(this, AbilitySystemComponent, CharacterClass);
	}

	// 위젯 컨트롤러 설정 처리
	if (UDRUserWidget* DRUserWidget = Cast<UDRUserWidget>(HealthBar->GetUserWidgetObject()))
	{
		DRUserWidget->SetWidgetController(this);
	}

	// 체력 및 최대 체력 델리게이트 바인딩 처리
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

		// 피격 반응 태그 이벤트 리스너 등록 처리
		AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().Effects_HitReact, EGameplayTagEventType::NewOrRemoved).AddUObject(
			this,
			&ADREnemy::HitReactTagChanged
		);

		// 초기 체력 및 최대 체력 값 브로드캐스트 처리
		OnHealthChanged.Broadcast(DRAS->GetHealth());
		OnMaxHealthChanged.Broadcast(DRAS->GetMaxHealth());
	}
}

// 어빌리티 및 태그 이벤트 초기화 처리
void ADREnemy::InitAbilityActorInfo()
{
	// ASC 액터 정보 초기화 처리
	AbilitySystemComponent->InitAbilityActorInfo(this, this);
	// 액터 정보 설정 콜백 호출 처리
	Cast<UDRAbilitySystemComponent>(AbilitySystemComponent)->AbilityActorInfoSet();
	// 기절 태그 이벤트 리스너 등록 처리
	AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().Debuff_Stun, EGameplayTagEventType::NewOrRemoved).AddUObject(this, &ADREnemy::StunTagChanged);

	// 서버 권한 검사 처리 및 기본 특성 초기화 호출
	if (HasAuthority())
	{
		InitializeDefaultAttributes();
	}
	// ASC 등록 이벤트 브로드캐스트 처리
	OnAscRegistered.Broadcast(AbilitySystemComponent);
}

// 기본 특성 초기화 호출
void ADREnemy::InitializeDefaultAttributes() const
{
	UDRAbilitySystemLibrary::InitializeEnemyDefaultAttributes(this, CharacterClass, Level, AbilitySystemComponent);
}

// 기절 태그 변경 처리
void ADREnemy::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	Super::StunTagChanged(CallbackTag, NewCount);

	// 블랙보드 상태 업데이트 처리
	if (DRAIController && DRAIController->GetBlackboardComponent())
	{
		DRAIController->GetBlackboardComponent()->SetValueAsBool(FName("Stunned"), bIsStunned);
	}
}
