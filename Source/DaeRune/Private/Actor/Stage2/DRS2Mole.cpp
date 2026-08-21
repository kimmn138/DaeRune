// Copyright DaeRune

#include "Actor/Stage2/DRS2Mole.h"

#include "AbilitySystemComponent.h"
#include "Actor/Stage2/DRS2MoleGame.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADRS2Mole::ADRS2Mole()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	MoleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MoleMesh"));
	MoleMesh->SetupAttachment(SceneRoot);
	MoleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// 투사체 오버랩 / 시점 트레이스에 걸리는 판정체
	HitBox = CreateDefaultSubobject<UCapsuleComponent>(TEXT("HitBox"));
	HitBox->SetupAttachment(SceneRoot);
	HitBox->SetCapsuleSize(34.f, 60.f);
	HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	HitBox->SetCollisionObjectType(ECC_Pawn);
	HitBox->SetCollisionResponseToAllChannels(ECR_Ignore);
	HitBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	HitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	HitBox->SetGenerateOverlapEvents(true);

	// 최소 ASC. 어트리뷰트는 두지 않는다 (피격은 IDRProximityHitOnly 로 직접 처리).
	// ASC 자체가 없으면 투사체가 대상을 찾지 못해 ApplyDamageEffect 가 호출되지 않는다.
	AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void ADRS2Mole::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2Mole, bVanishing);
}

void ADRS2Mole::BeginPlay()
{
	Super::BeginPlay();

	// ★IsNotFriend 가 액터 태그 기반이므로(DRAbilitySystemLibrary.cpp:415-422)
	//   이 태그 하나로 기존 플레이어 공격 전부의 유효 대상이 된다.
	Tags.AddUnique(FName("Enemy"));

	if (AbilitySystemComponent)
	{
		AbilitySystemComponent->InitAbilityActorInfo(this, this);
	}

	OnEmergeVisual();
}

void ADRS2Mole::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(LifetimeTimer);
		World->GetTimerManager().ClearTimer(VanishTimer);
	}

	Super::EndPlay(EndPlayReason);
}

void ADRS2Mole::InitFromGame(ADRS2MoleGame* InGame, float InLifetime)
{
	if (!HasAuthority()) return;

	OwningGame = InGame;

	if (InLifetime > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			LifetimeTimer, this, &ADRS2Mole::HandleLifetimeExpired, InLifetime, false);
	}
}

bool ADRS2Mole::AcceptsHitFrom(const AActor* Attacker) const
{
	if (bVanishing || !IsValid(Attacker)) return false;

	// 2m 이내에서의 공격만 유효하다. 그 밖은 투과된다.
	const float DistSq = FVector::DistSquared(GetActorLocation(), Attacker->GetActorLocation());
	return DistSq <= FMath::Square(ProximityRadius);
}

void ADRS2Mole::HandleProximityHit(AActor* /*Attacker*/)
{
	if (!HasAuthority() || bVanishing) return;

	// 데미지 수치와 무관하게 1히트로 사라진다 (홀로그램)
	BeginVanish(/*bKilled=*/true);

	if (ADRS2MoleGame* Game = OwningGame.Get())
	{
		Game->OnMoleKilled(this);
	}
}

void ADRS2Mole::HandleLifetimeExpired()
{
	if (!HasAuthority() || bVanishing) return;

	// 공격받지 않고 시간이 지나면 스스로 사라진다 (패널티 없음)
	BeginVanish(/*bKilled=*/false);

	if (ADRS2MoleGame* Game = OwningGame.Get())
	{
		Game->OnMoleExpired(this);
	}
}

void ADRS2Mole::BeginVanish(bool bKilled)
{
	bVanishing = true;
	bKilledByPlayer = bKilled;

	// 더 이상 피격되지 않도록 판정체를 끈다
	HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetWorld()->GetTimerManager().ClearTimer(LifetimeTimer);

	OnRep_bVanishing();

	// 연출 후 제거
	if (VanishDuration > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			VanishTimer, this, &ADRS2Mole::FinishVanish, VanishDuration, false);
	}
	else
	{
		FinishVanish();
	}
}

void ADRS2Mole::OnRep_bVanishing()
{
	if (bVanishing)
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		OnVanishVisual(bKilledByPlayer);
	}
}

void ADRS2Mole::FinishVanish()
{
	if (HasAuthority())
	{
		Destroy();
	}
}
