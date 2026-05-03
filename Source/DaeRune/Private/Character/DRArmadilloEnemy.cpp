// Copyright DaeRune

#include "Character/DRArmadilloEnemy.h"
#include "Character/DRCharacter.h"
#include "DRGameplayTags.h"
#include "AI/DRAIController.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Engine/OverlapResult.h"
#include "DrawDebugHelpers.h"

ADRArmadilloEnemy::ADRArmadilloEnemy()
{
	PrimaryActorTick.bCanEverTick = true;

	// Ball form mesh component
	BallFormMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BallFormMesh"));
	BallFormMesh->SetupAttachment(GetRootComponent());
	BallFormMesh->SetVisibility(false); // BasicForm is default
	BallFormMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRArmadilloEnemy::BeginPlay()
{
	Super::BeginPlay();

	// Save default capsule size
	DefaultCapsuleRadius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
	DefaultCapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
}

void ADRArmadilloEnemy::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority() && bIsRolling)
	{
		// Override parent's rotation to face roll direction instead of target
		FRotator RollRotation = RollDirection.Rotation();
		SetActorRotation(FRotator(0.f, RollRotation.Yaw, 0.f));

		TickRollCharge(DeltaTime);
	}
}

void ADRArmadilloEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRArmadilloEnemy, bIsBallForm);
	DOREPLIFETIME(ADRArmadilloEnemy, bIsRolling);
}

// ===== Death Handling =====

void ADRArmadilloEnemy::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
	// 1. Stop roll charge if rolling
	if (bIsRolling)
	{
		StopRollCharge();
	}

	// 2. Force revert to basic form (mesh swap only, no animation)
	if (bIsBallForm)
	{
		bIsBallForm = false;
		UpdateMeshVisibility();

		// Restore default capsule size
		GetCapsuleComponent()->SetCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight);
	}

	// 3. Parent handles death animation, dissolve, etc.
	Super::MulticastHandleDeath_Implementation(DeathImpulse);
}

// ===== Form Switching =====

void ADRArmadilloEnemy::StartFormChange(bool bToBallForm)
{
	if (!HasAuthority()) return;
	bPendingBallForm = bToBallForm;
	// Transition animation is played by GA -> AnimNotify calls FinishFormChange()
}

void ADRArmadilloEnemy::FinishFormChange()
{
	if (!HasAuthority()) return;
	bIsBallForm = bPendingBallForm;
	OnRep_BallForm(); // Update server local as well

	// Adjust capsule size
	if (bIsBallForm)
	{
		GetCapsuleComponent()->SetCapsuleSize(BallFormCapsuleRadius, BallFormCapsuleHalfHeight);
	}
	else
	{
		GetCapsuleComponent()->SetCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight);
	}
}

void ADRArmadilloEnemy::OnRep_BallForm()
{
	UpdateMeshVisibility();
}

void ADRArmadilloEnemy::UpdateMeshVisibility()
{
	// BasicForm mesh = GetMesh() (parent's SkeletalMeshComponent)
	GetMesh()->SetVisibility(!bIsBallForm);
	BallFormMesh->SetVisibility(bIsBallForm);
}

// ===== Stun Override =====

void ADRArmadilloEnemy::StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// If stunned while rolling, stop the charge
	if (NewCount > 0 && bIsRolling)
	{
		StopRollCharge();
	}

	// If stunned while in ball form, revert to basic form (mesh swap only, no animation)
	if (NewCount > 0 && bIsBallForm)
	{
		bIsBallForm = false;
		bPendingBallForm = false;
		OnRep_BallForm(); // Update visibility
		GetCapsuleComponent()->SetCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight);
	}

	// Call parent stun handling (updates BB, bIsStunned, etc.)
	Super::StunTagChanged(CallbackTag, NewCount);
}

// ===== Rolling Charge =====

AActor* ADRArmadilloEnemy::FindRollTarget() const
{
	// 1. Collect all players in world
	TArray<AActor*> AllPlayers;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), ADRCharacter::StaticClass(), AllPlayers);

	// 2. Filter by distance and line-of-sight
	TArray<AActor*> ValidTargets;
	const FVector MyLocation = GetActorLocation();

#if 0 // Temporarily disabled debug draw
	// Draw search range
	DrawDebugSphere(GetWorld(), MyLocation, RollTargetSearchRange, 24, FColor::Cyan, false, 2.f);
#endif

	for (AActor* Player : AllPlayers)
	{
		if (!Player || Player->IsHidden()) continue;

		// Check if player is dead
		if (const ICombatInterface* CombatInterface = Cast<ICombatInterface>(Player))
		{
			if (CombatInterface->Execute_IsDead(Player)) continue;
		}

		const float Distance = FVector::Dist(MyLocation, Player->GetActorLocation());
		if (Distance > RollTargetSearchRange)
		{
#if 0 // Temporarily disabled debug draw
			// Out of range — gray line
			DrawDebugLine(GetWorld(), MyLocation, Player->GetActorLocation(), FColor::Silver, false, 2.f);
			DrawDebugString(GetWorld(), Player->GetActorLocation() + FVector(0, 0, 50), TEXT("OUT OF RANGE"), nullptr, FColor::Silver, 2.f);
#endif
			continue;
		}

		// LineTrace for wall obstruction check (ECC_WorldStatic)
		FHitResult HitResult;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(this);
		Params.AddIgnoredActor(Player);

		bool bBlocked = GetWorld()->LineTraceSingleByChannel(
			HitResult,
			MyLocation,
			Player->GetActorLocation(),
			ECC_WorldStatic,
			Params
		);

		if (!bBlocked)
		{
			ValidTargets.Add(Player);
#if 0 // Temporarily disabled debug draw
			// Valid target — green line
			DrawDebugLine(GetWorld(), MyLocation, Player->GetActorLocation(), FColor::Green, false, 2.f);
			DrawDebugSphere(GetWorld(), Player->GetActorLocation(), 40.f, 8, FColor::Green, false, 2.f);
#endif
		}
		else
		{
#if 0 // Temporarily disabled debug draw
			// Blocked by wall — red line to wall hit, then red dashed to player
			DrawDebugLine(GetWorld(), MyLocation, HitResult.ImpactPoint, FColor::Red, false, 2.f);
			DrawDebugLine(GetWorld(), HitResult.ImpactPoint, Player->GetActorLocation(), FColor::Orange, false, 2.f);
			DrawDebugPoint(GetWorld(), HitResult.ImpactPoint, 10.f, FColor::Red, false, 2.f);
			DrawDebugString(GetWorld(), HitResult.ImpactPoint + FVector(0, 0, 30), TEXT("WALL"), nullptr, FColor::Red, 2.f);
#endif
		}
	}

	// 3. Return random valid target, or nullptr
	if (ValidTargets.Num() == 0)
	{
#if 0 // Temporarily disabled debug draw
		DrawDebugString(GetWorld(), MyLocation + FVector(0, 0, 100), TEXT("NO VALID TARGET"), nullptr, FColor::Red, 2.f);
#endif
		return nullptr;
	}

	const int32 RandomIndex = FMath::RandRange(0, ValidTargets.Num() - 1);
	AActor* ChosenTarget = ValidTargets[RandomIndex];

#if 0 // Temporarily disabled debug draw
	// Highlight chosen target
	DrawDebugSphere(GetWorld(), ChosenTarget->GetActorLocation(), 60.f, 12, FColor::Magenta, false, 3.f);
	DrawDebugString(GetWorld(), ChosenTarget->GetActorLocation() + FVector(0, 0, 80), TEXT("CHOSEN TARGET"), nullptr, FColor::Magenta, 3.f);
#endif

	return ChosenTarget;
}

void ADRArmadilloEnemy::StartRollCharge(FVector TargetLocation)
{
	if (!HasAuthority()) return;

	bIsRolling = true;
	RollTargetLocation = TargetLocation;
	RollDirection = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
	CurrentRollSpeed = RollInitialSpeed;

	// Stop AI movement (CharacterMovement is controlled directly)
	if (DRAIController)
	{
		DRAIController->StopMovement();
	}
}

void ADRArmadilloEnemy::StopRollCharge()
{
	if (!HasAuthority()) return;

	bIsRolling = false;
	CurrentRollSpeed = 0.f;
	GetCharacterMovement()->Velocity = FVector::ZeroVector;
}

void ADRArmadilloEnemy::TickRollCharge(float DeltaTime)
{
	// 1. Accelerate
	CurrentRollSpeed = FMath::Min(CurrentRollSpeed + RollAcceleration * DeltaTime, RollMaxSpeed);

	// 2. Move
	FVector NewVelocity = RollDirection * CurrentRollSpeed;
	GetCharacterMovement()->Velocity = FVector(NewVelocity.X, NewVelocity.Y,
		GetCharacterMovement()->Velocity.Z);

	// 3. Check if target reached
	FVector ToTarget = RollTargetLocation - GetActorLocation();
	ToTarget.Z = 0;
	float Dot = FVector::DotProduct(ToTarget.GetSafeNormal(), RollDirection);
	if (Dot <= 0.f) // Passed the target point
	{
		StopRollCharge();
		// Notify GA that target was reached -> GA handles form revert
		OnRollReachedTarget.Broadcast();
		return;
	}

	// 4. Forward collision detection
	FHitResult HitResult;
	if (DetectRollCollision(HitResult))
	{
		StopRollCharge();

#if 0 // Temporarily disabled debug draw
		// Draw impact radius sphere at collision point
		DrawDebugSphere(GetWorld(), HitResult.ImpactPoint, RollImpactRadius, 12, FColor::Red, false, 3.f);
#endif

		// Collect collision results and notify GA -> GA handles damage/stun
		BroadcastRollImpact(HitResult.ImpactPoint);
	}

#if 0 // Temporarily disabled debug draw
	// Draw forward detection sphere
	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	FVector TraceStart = GetActorLocation() + RollDirection * CapsuleRadius;
	FVector TraceEnd = TraceStart + RollDirection * CurrentRollSpeed * DeltaTime;
	DrawDebugSphere(GetWorld(), TraceStart, RollDetectionRadius, 8, FColor::Green, false, 0.f);
	DrawDebugLine(GetWorld(), TraceStart, TraceEnd, FColor::Green, false, 0.f);
	// Draw target location
	DrawDebugSphere(GetWorld(), RollTargetLocation, 30.f, 8, FColor::Yellow, false, 0.f);
#endif
}

bool ADRArmadilloEnemy::DetectRollCollision(FHitResult& OutHit) const
{
	// Start from the front edge of the capsule, not the center
	const float CapsuleRadius = GetCapsuleComponent()->GetScaledCapsuleRadius();
	FVector Start = GetActorLocation() + RollDirection * CapsuleRadius;
	FVector End = Start + RollDirection * CurrentRollSpeed * GetWorld()->GetDeltaSeconds();

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	FCollisionObjectQueryParams ObjectParams;
	ObjectParams.AddObjectTypesToQuery(ECC_Pawn);        // Players, enemies
	ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);  // Walls, floors, terrain
	ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic); // Cleanser sites, etc.

	// SphereTrace with radius 30 for forward detection (multi-channel)
	bool bHit = GetWorld()->SweepSingleByObjectType(
		OutHit, Start, End, FQuat::Identity,
		ObjectParams,
		FCollisionShape::MakeSphere(RollDetectionRadius),
		Params
	);

	// Skip trigger volumes (QueryOnly components like door triggers)
	// SweepByObjectType treats all matching object types as blocking regardless of response settings
	if (bHit && OutHit.GetComponent() && OutHit.GetComponent()->GetCollisionEnabled() == ECollisionEnabled::QueryOnly)
	{
		return false;
	}

	return bHit;
}

void ADRArmadilloEnemy::BroadcastRollImpact(const FVector& ImpactLocation)
{
	FRollImpactResult Result;
	Result.ImpactLocation = ImpactLocation;
	Result.bHitPlayerOrEnemy = false;

	// Overlap check at impact location with radius 50
	TArray<FOverlapResult> Overlaps;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	GetWorld()->OverlapMultiByChannel(
		Overlaps, ImpactLocation, FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(RollImpactRadius),
		Params
	);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor) continue;

		Result.HitActors.Add(HitActor);

		// Only check if player or other enemy exists (damage is applied by GA)
		if (HitActor->IsA(ADRCharacter::StaticClass()) ||
			HitActor->IsA(ADREnemy::StaticClass()))
		{
			Result.bHitPlayerOrEnemy = true;
		}
	}

	// Pass collision result to GA -> GA handles damage/stun
	OnRollImpact.Broadcast(Result);
}
