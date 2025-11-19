// Copyright DaeRune


#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DRAbilityTypes.h"
#include "DRGameplayTags.h"
#include "Actor/DRCleanserSite.h"
#include "Game/DRGameModeBase.h"
#include "Interaction/CombatInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DRPlayerState.h"
#include "UI/HUD/DRHUD.h"
#include "UI/WidgetController/DRWidgetController.h"
#include "Engine/OverlapResult.h"
#include "Game/DRStageGameState.h"
#include "GameFramework/Character.h"

bool UDRAbilitySystemLibrary::MakeWidgetControllerParams(const UObject* WorldContextObject, FWidgetControllerParams& OutWCParams, ADRHUD*& OutDRHUD)
{
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		OutDRHUD = Cast<ADRHUD>(PC->GetHUD()); 
		if (OutDRHUD)
		{
			ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>();
			UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
			UAttributeSet* AS = PS->GetAttributeSet();
			
			OutWCParams.AttributeSet = AS;
			OutWCParams.AbilitySystemComponent = ASC;
			OutWCParams.PlayerState = PS;
			OutWCParams.PlayerController = PC;
			return true;
		}
	}
	return false;
}

UOverlayWidgetController* UDRAbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	FWidgetControllerParams WCParams;
	ADRHUD* DRHUD = nullptr;
	if (MakeWidgetControllerParams(WorldContextObject, WCParams, DRHUD))
	{
		return DRHUD->GetOverlayWidgetController(WCParams);
	}
	return nullptr;
}

void UDRAbilitySystemLibrary::InitializeDefaultAttributes(const UObject* WorldContextObject, ECharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC)
{AActor* AvatarActor = ASC->GetAvatarActor();

	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	FCharacterClassDefaultInfo ClassDefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);

	FGameplayEffectContextHandle PrimaryAttributesContextHandle = ASC->MakeEffectContext();
	PrimaryAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle PrimaryAttributesSpecHandle = ASC->MakeOutgoingSpec(ClassDefaultInfo.PrimaryAttributes, Level, PrimaryAttributesContextHandle);
	ASC->ApplyGameplayEffectSpecToSelf(*PrimaryAttributesSpecHandle.Data.Get());

	FGameplayEffectContextHandle VitalAttributesContextHandle = ASC->MakeEffectContext();
	VitalAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle VitalAttributesSpecHandle = ASC->MakeOutgoingSpec(ClassDefaultInfo.VitalAttributes, Level, VitalAttributesContextHandle);
	ASC->ApplyGameplayEffectSpecToSelf(*VitalAttributesSpecHandle.Data.Get());
}

void UDRAbilitySystemLibrary::GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ECharacterClass CharacterClass)
{
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (CharacterClassInfo == nullptr) return;

	int32 CharacterLevel = 1;
	if (ASC->GetAvatarActor()->Implements<UCombatInterface>())
	{
		CharacterLevel = ICombatInterface::Execute_GetPlayerLevel(ASC->GetAvatarActor());
	}
	
	for (TSubclassOf<UGameplayAbility> AbilityClass : CharacterClassInfo->CommonAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, CharacterLevel);
		ASC->GiveAbility(AbilitySpec);
	}
	const FCharacterClassDefaultInfo& DefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);
	for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultInfo.StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, CharacterLevel);
		ASC->GiveAbility(AbilitySpec);
	}
}

UCharacterClassInfo* UDRAbilitySystemLibrary::GetCharacterClassInfo(const UObject* WorldContextObject)
{
	const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (DRGameMode == nullptr) return nullptr;
	return DRGameMode->CharacterClassInfo;
}

UAbilityInfo* UDRAbilitySystemLibrary::GetAbilityInfo(const UObject* WorldContextObject)
{
	const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (DRGameMode == nullptr) return nullptr;
	return DRGameMode->AbilityInfo;
}

bool UDRAbilitySystemLibrary::IsSuccessfulDebuff(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->IsSuccessfulDebuff();
	}
	return false;
}

float UDRAbilitySystemLibrary::GetDebuffDamage(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetDebuffDamage();
	}
	return 0.f;
}

float UDRAbilitySystemLibrary::GetDebuffDuration(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetDebuffDuration();
	}
	return 0.f;
}

FGameplayTag UDRAbilitySystemLibrary::GetDamageType(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		if (DREffectContext->GetDamageType().IsValid())
		{
			return *DREffectContext->GetDamageType();
		}
	}
	return FGameplayTag();
}

FVector UDRAbilitySystemLibrary::GetDeathImpulse(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetDeathImpulse();
	}
	return FVector::ZeroVector;
}

FVector UDRAbilitySystemLibrary::GetKnockbackForce(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetKnockbackForce();
	}
	return FVector::ZeroVector;
}

void UDRAbilitySystemLibrary::SetIsSuccessfulDebuff(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, bool bInSuccessfulDebuff)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetIsSuccessfulDebuff(bInSuccessfulDebuff);
	}
}

void UDRAbilitySystemLibrary::SetDebuffDamage(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, float InDamage)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetDebuffDamage(InDamage);
	}
}

void UDRAbilitySystemLibrary::SetDebuffDuration(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, float InDuration)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetDebuffDuration(InDuration);
	}
}

void UDRAbilitySystemLibrary::SetDamageType(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, const FGameplayTag& InDamageType)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		const TSharedPtr<FGameplayTag> DamageType = MakeShared<FGameplayTag>(InDamageType);
		DREffectContext->SetDamageType(DamageType);
	}
}

void UDRAbilitySystemLibrary::SetDeathImpulse(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, const FVector& InImpulse)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetDeathImpulse(InImpulse);
	}
}

void UDRAbilitySystemLibrary::SetKnockbackForce(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, const FVector& InForce)
{
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{ 
		DREffectContext->SetKnockbackForce(InForce);
	}
}

void UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(const UObject* WorldContextObject, TArray<AActor*>& OutOverlappingActors, const TArray<AActor*>& ActorsToIgnore, float Radius, const FVector& SphereOrigin)
{
	FCollisionQueryParams SphereParams;
	SphereParams.AddIgnoredActors(ActorsToIgnore);

	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, SphereOrigin, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects), FCollisionShape::MakeSphere(Radius), SphereParams);
		for (FOverlapResult& Overlap : Overlaps)
		{
			if (Overlap.GetActor()->Implements<UCombatInterface>() && !ICombatInterface::Execute_IsDead(Overlap.GetActor()))
			{
				OutOverlappingActors.AddUnique(ICombatInterface::Execute_GetAvatar(Overlap.GetActor()));
			}
			else if (ADRCleanserSite* CleanserSite = Cast<ADRCleanserSite>(Overlap.GetActor()))
			{
				OutOverlappingActors.AddUnique(CleanserSite);
			}
		}
	}
}

void UDRAbilitySystemLibrary::GetClosestTargets(int32 MaxTargets, const TArray<AActor*>& Actors, TArray<AActor*>& OutClosestTargets, const FVector& Origin)
{
	if (Actors.Num() <= MaxTargets)
	{
		OutClosestTargets = Actors;
		return;
	}

	TArray<AActor*> ActorsToCheck = Actors;
	int32 NumTargetsFound = 0;

	while (NumTargetsFound < MaxTargets)
	{
		if (ActorsToCheck.Num() == 0) break;
		double ClosestDistance = TNumericLimits<double>::Max();
		AActor* ClosestActor;
		for (AActor* PotentialTarget : ActorsToCheck)
		{
			const double Distance = (PotentialTarget->GetActorLocation() - Origin).Length();
			if (Distance < ClosestDistance)
			{
				ClosestDistance = Distance;
				ClosestActor = PotentialTarget;
			}
		}
		ActorsToCheck.Remove(ClosestActor);
		OutClosestTargets.AddUnique(ClosestActor); 
		++NumTargetsFound;
	}
}

AActor* UDRAbilitySystemLibrary::GetClosestCleanserSite(APawn* ControlledPawn)
{
	if (!ControlledPawn) return nullptr;

	UWorld* World = ControlledPawn->GetWorld();
	if (!World) return nullptr;

	ADRStageGameState* GameState = Cast<ADRStageGameState>(World->GetGameState());
	if (!GameState) return nullptr;

	TArray<ADRCleanserSite*> CleanserSites = GameState->GetCleanserSites();
	
	const float Dist0 = FVector::Dist(CleanserSites[0]->GetActorLocation(), ControlledPawn->GetActorLocation());
	const float Dist1 = FVector::Dist(CleanserSites[1]->GetActorLocation(), ControlledPawn->GetActorLocation());

	AActor* ClosestCleanserSite = Dist0 < Dist1 ? CleanserSites[0] : CleanserSites[1];

	return ClosestCleanserSite;
}

bool UDRAbilitySystemLibrary::IsNotFriend(AActor* FirstActor, AActor* SecondActor)
{
	const bool bBothArePlayers = FirstActor->ActorHasTag(FName("Player")) && SecondActor->ActorHasTag(FName("Player"));
	const bool bBothAreEnemies = FirstActor->ActorHasTag(FName("Enemy")) && SecondActor->ActorHasTag(FName("Enemy"));
	const bool bFriends = bBothArePlayers || bBothAreEnemies;
	return !bFriends;
}

FGameplayEffectContextHandle UDRAbilitySystemLibrary::ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams)
{
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
	const AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();

	FGameplayEffectContextHandle EffectContexthandle = DamageEffectParams.SourceAbilitySystemComponent->MakeEffectContext();
	EffectContexthandle.AddSourceObject(SourceAvatarActor);
	SetDeathImpulse(EffectContexthandle, DamageEffectParams.DeathImpulse);
	SetKnockbackForce(EffectContexthandle, DamageEffectParams.KnockbackForce);
	const FGameplayEffectSpecHandle SpecHandle = DamageEffectParams.SourceAbilitySystemComponent->MakeOutgoingSpec(DamageEffectParams.DamageGameplayEffectClass, DamageEffectParams.AbilityLevel, EffectContexthandle);

	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, DamageEffectParams.DamageType, DamageEffectParams.BaseDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Chance, DamageEffectParams.DebuffChance);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Damage, DamageEffectParams.DebuffDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Duration, DamageEffectParams.DebuffDuration);

	DamageEffectParams.TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	return EffectContexthandle;
}

TArray<FRotator> UDRAbilitySystemLibrary::EvenlySpacedRotators(const FVector& Forward, const FVector& Axis, float Spread, int32 NumRotators)
{
	TArray<FRotator> Rotators; 

	const FVector LeftOfSpread = Forward.RotateAngleAxis(-Spread / 2.f, Axis);
	if (NumRotators > 1)
	{
		const float DeltaSpread = Spread / (NumRotators - 1);
		for (int32 i = 0; i < NumRotators; i++)
		{
			const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, Axis);
			Rotators.Add(Direction.Rotation());
		}
	}
	else
	{
		Rotators.Add(Forward.Rotation());
	}
	return Rotators;
}

TArray<FVector> UDRAbilitySystemLibrary::EvenlyRotatedVectors(const FVector& Forward, const FVector& Axis, float Spread, int32 NumVectors)
{
	TArray<FVector> Vectors; 

	const FVector LeftOfSpread = Forward.RotateAngleAxis(-Spread / 2.f, Axis);
	if (NumVectors > 1)
	{
		const float DeltaSpread = Spread / (NumVectors - 1);
		for (int32 i = 0; i < NumVectors; i++)
		{
			const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, Axis);
			Vectors.Add(Direction);
		}
	}
	else
	{
		Vectors.Add(Forward);
	}
	return Vectors;
}

bool UDRAbilitySystemLibrary::CheckActorWallCollision(AActor* Target, float CheckDistance)
{
	if (!Target)
	{
		return false;
	}

	// 넉백 방향 계산 (타겟의 현재 속도 방향)
	FVector Velocity = FVector::ZeroVector;
	if (ACharacter* Character = Cast<ACharacter>(Target))
	{
		Velocity = Character->GetVelocity();
	}

	if (Velocity.IsNearlyZero())
	{
		return false;
	}

	// 벽 체크를 위한 트레이스
	FVector StartLocation = Target->GetActorLocation();
	FVector EndLocation = StartLocation + (Velocity.GetSafeNormal() * CheckDistance);

	FHitResult WallHitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Target);

	bool bHitWall = Target->GetWorld()->LineTraceSingleByChannel(
		WallHitResult,
		StartLocation,
		EndLocation,
		ECC_WorldStatic,
		QueryParams
	);

	if (bHitWall && WallHitResult.bBlockingHit)
	{
		return true;
	}

	return false;
}
