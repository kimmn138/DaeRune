// Copyright DaeRune

#include "Actor/Stage2/DRS2Mole.h"

#include "Interaction/CombatInterface.h"

#include "DaeRune/DaeRune.h"

#include "AbilitySystemComponent.h"
#include "Actor/Stage2/DRS2MoleGame.h"
#include "Components/CapsuleComponent.h"
#include "Net/UnrealNetwork.h"
#include "DaeRune/DRLogChannels.h"
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

	// ★공격 판정 채널 두 개에 반드시 응답해야 한다 (2026-08-27 수정).
	//   오버랩은 양쪽 응답이 모두 필요한데, 이 줄들이 없으면 두더지가 해당 채널을 Ignore 해
	//   공격이 그냥 통과한다. 그 결과 때려도 아무 반응이 없었다.
	//     ECC_Projectile : 씨앗폭탄 등 투사체
	//     ECC_Target     : 물대포·기본 공격 등 근접 판정 (OverlapMultiByChannel)
	HitBox->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	HitBox->SetCollisionResponseToChannel(ECC_Target, ECR_Overlap);

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

	// ★공격 판정 채널 응답을 BeginPlay 에서 다시 강제한다 (2026-08-27).
	//   생성자에서만 설정하면 BP(BP_DRMole)가 컴포넌트 콜리전을 직렬화해 갖고 있을 때
	//   그 저장값이 C++ 생성자 설정을 덮어써 채널 응답이 Ignore 로 남는다.
	//   런타임 설정은 직렬화값보다 뒤에 적용되므로 항상 이긴다.
	if (HitBox)
	{
		HitBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
		HitBox->SetCollisionObjectType(ECC_Pawn);
		HitBox->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
		HitBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
		HitBox->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
		HitBox->SetCollisionResponseToChannel(ECC_Target, ECR_Overlap);

		// [임시 진단]
		UE_LOG(LogDR, Warning,
			TEXT("[S2Mole] CombatInterface=%d / 응답: Visibility=%d Target=%d Projectile=%d (0=Ignore 1=Overlap 2=Block) / CollisionEnabled=%d / 액터위치=%s / 히트박스위치=%s 반지름=%.0f 반높이=%.0f"),
			Implements<UCombatInterface>() ? 1 : 0,
			static_cast<int32>(HitBox->GetCollisionResponseToChannel(ECC_Visibility)),
			static_cast<int32>(HitBox->GetCollisionResponseToChannel(ECC_Target)),
			static_cast<int32>(HitBox->GetCollisionResponseToChannel(ECC_Projectile)),
			static_cast<int32>(HitBox->GetCollisionEnabled()),
			*GetActorLocation().ToCompactString(),
			*HitBox->GetComponentLocation().ToCompactString(),
			HitBox->GetScaledCapsuleRadius(),
			HitBox->GetScaledCapsuleHalfHeight());
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

	// [임시 진단]
	UE_LOG(LogDR, Warning, TEXT("[MoleDiag] C. AcceptsHitFrom: 거리 %.0f / 허용 %.0f -> %s"),
		FMath::Sqrt(DistSq), ProximityRadius,
		DistSq <= FMath::Square(ProximityRadius) ? TEXT("통과") : TEXT("투과(거리 초과)"));

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

void ADRS2Mole::Die(const FVector& /*DeathImpulse*/)
{
	// ICombatInterface 계약상 필요하지만, 정상 경로는 HandleProximityHit 이다.
	// 외부에서 직접 호출되더라도 같은 소멸 처리를 태워 상태가 어긋나지 않게 한다.
	if (!HasAuthority() || bVanishing) return;

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
