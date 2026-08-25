// Copyright DaeRune


#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/Data/GameBalanceConfig.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "DRAbilityTypes.h"
#include "DRGameplayTags.h"
#include "Actor/DRCleanserSite.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "Game/DRGameModeBase.h"
#include "Game/DRGameInstance.h"
#include "Interaction/CombatInterface.h"
#include "Interaction/DRProximityHitOnly.h"
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
			if (!PS) return false; // 클라이언트 초기화 중 PlayerState 복제 전이면 실패 처리

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
{
	AActor* AvatarActor = ASC->GetAvatarActor();

	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	FCharacterClassDefaultInfo ClassDefaultInfo = CharacterClassInfo->GetClassDefaultInfo(CharacterClass);

	// 헬퍼 함수로 간소화
	CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.PrimaryAttributes, AvatarActor, Level);
	CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.VitalAttributes, AvatarActor, Level);
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
	for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultInfo.DeathAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, CharacterLevel);
		ASC->GiveAbility(AbilitySpec);
	}
}

UCharacterClassInfo* UDRAbilitySystemLibrary::GetCharacterClassInfo(const UObject* WorldContextObject)
{
	const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (DRGameMode == nullptr) return nullptr;
	return DRGameMode->EnemyCharacterClassInfo;
}

UPlayerCharacterClassInfo* UDRAbilitySystemLibrary::GetPlayerCharacterClassInfo(const UObject* WorldContextObject)
{
	// GameInstance는 서버/클라이언트 모두에 존재하므로 양쪽에서 안전하게 접근 가능
	const UDRGameInstance* DRGameInstance = Cast<UDRGameInstance>(UGameplayStatics::GetGameInstance(WorldContextObject));
	if (DRGameInstance == nullptr) return nullptr;
	return DRGameInstance->PlayerCharacterClassInfo;
}

TSubclassOf<UUserWidget> UDRAbilitySystemLibrary::GetCharacterInfoWidgetClass(const UObject* WorldContextObject, EPlayerCharacterClass PlayerClass)
{
	UPlayerCharacterClassInfo* ClassInfo = GetPlayerCharacterClassInfo(WorldContextObject);
	if (ClassInfo == nullptr) return nullptr;

	FCharacterClassDefaultInfo Info = ClassInfo->GetClassDefaultInfo(PlayerClass);
	return Info.CharacterInfoWidgetClass;
}

void UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(
	const UObject* WorldContextObject, EPlayerCharacterClass PlayerClass, float Level, UAbilitySystemComponent* ASC)
{
	AActor* AvatarActor = ASC->GetAvatarActor();

	UPlayerCharacterClassInfo* ClassInfo = GetPlayerCharacterClassInfo(WorldContextObject);
	if (!ClassInfo) return;

	// Step 1: 기존 활성 GE 제거 (Infinite/HasDuration GE 대비)
	for (auto& Pair : ClassInfo->CharacterClassInformation)
	{
		FCharacterClassDefaultInfo& Info = Pair.Value;
		if (Info.PrimaryAttributes)
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(Info.PrimaryAttributes, ASC);
		}
		if (Info.VitalAttributes)
		{
			ASC->RemoveActiveGameplayEffectBySourceEffect(Info.VitalAttributes, ASC);
		}
	}

	// Step 2: 속성 BaseValue 초기화 (Instant GE에 의한 누적값 제거)
	if (const UDRAttributeSet* DRAS = ASC->GetSet<UDRAttributeSet>())
	{
		ASC->SetNumericAttributeBase(DRAS->GetMaxHealthAttribute(), 0.f);
		ASC->SetNumericAttributeBase(DRAS->GetMaxWaterAttribute(), 0.f);
		ASC->SetNumericAttributeBase(DRAS->GetMoveSpeedAttribute(), 0.f);
		ASC->SetNumericAttributeBase(DRAS->GetHealthAttribute(), 0.f);
		ASC->SetNumericAttributeBase(DRAS->GetWaterAttribute(), 0.f);
	}

	// Step 3: 선택된 클래스의 GE 적용 (깨끗한 상태에서)
	FCharacterClassDefaultInfo ClassDefaultInfo = ClassInfo->GetClassDefaultInfo(PlayerClass);
	CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.PrimaryAttributes, AvatarActor, Level);
	CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.VitalAttributes, AvatarActor, Level);
}

void UDRAbilitySystemLibrary::GivePlayerStartupAbilities(
	const UObject* WorldContextObject, UAbilitySystemComponent* ASC, EPlayerCharacterClass PlayerClass)
{
	UPlayerCharacterClassInfo* ClassInfo = GetPlayerCharacterClassInfo(WorldContextObject);
	if (!ClassInfo) return;

	UDRAbilitySystemComponent* DRASC = Cast<UDRAbilitySystemComponent>(ASC);
	if (!DRASC) return;

	// 공통 + 클래스별 어빌리티를 하나의 배열로 수집
	TArray<TSubclassOf<UGameplayAbility>> AllAbilities;
	AllAbilities.Append(ClassInfo->CommonAbilities);

	const FCharacterClassDefaultInfo& DefaultInfo = ClassInfo->GetClassDefaultInfo(PlayerClass);
	AllAbilities.Append(DefaultInfo.StartupAbilities);

	// AddCharacterAbilities가 DynamicAbilityTags, InputTagToAbilityMap,
	// bStartupAbilitiesGiven, AbilitiesGivenDelegate를 모두 처리
	DRASC->AddCharacterAbilities(AllAbilities);
}

UAbilityInfo* UDRAbilitySystemLibrary::GetAbilityInfo(const UObject* WorldContextObject)
{
	const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (DRGameMode == nullptr) return nullptr;
	return DRGameMode->AbilityInfo;
}

UGameBalanceConfig* UDRAbilitySystemLibrary::GetGameBalanceConfig(const UObject* WorldContextObject)
{
	const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (DRGameMode == nullptr) return nullptr;
	return DRGameMode->GameBalanceConfig;
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

FGameplayTagContainer UDRAbilitySystemLibrary::GetSourceAbilityTags(const FGameplayEffectContextHandle& EffectContextHandle)
{
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetSourceAbilityTags();
	}
	return FGameplayTagContainer();
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
		AActor* ClosestActor = nullptr;
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

	const TArray<TObjectPtr<ADRCleanserSite>>& CleanserSites = GameState->GetCleanserSites();
	if (CleanserSites.Num() == 0) return nullptr;

	const FVector PawnLocation = ControlledPawn->GetActorLocation();
	ADRCleanserSite* Closest = nullptr;
	float ClosestDistSq = TNumericLimits<float>::Max();

	for (ADRCleanserSite* Site : CleanserSites)
	{
		if (!Site) continue;
		const float DistSq = FVector::DistSquared(Site->GetActorLocation(), PawnLocation);
		if (DistSq < ClosestDistSq)
		{
			ClosestDistSq = DistSq;
			Closest = Site;
		}
	}

	return Closest;
}

bool UDRAbilitySystemLibrary::IsActorReachable(APawn* Asker, AActor* Target)
{
	if (!IsValid(Asker) || !IsValid(Target)) return false;

	UWorld* World = Asker->GetWorld();
	if (!World) return false;

	// 벽/지형(WorldStatic)만 차단으로 간주. 다른 캐릭터/적은 무시.
	const FVector Start = Asker->GetActorLocation();
	const FVector End = Target->GetActorLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(IsActorReachable), /*bTraceComplex=*/false);
	Params.AddIgnoredActor(Asker);
	Params.AddIgnoredActor(Target);

	FHitResult Hit;
	const bool bBlocked = World->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params);
	return !bBlocked;
}

bool UDRAbilitySystemLibrary::IsNotFriend(AActor* FirstActor, AActor* SecondActor)
{
	if (!IsValid(FirstActor) || !IsValid(SecondActor)) return false;
	const bool bBothArePlayers = FirstActor->ActorHasTag(FName("Player")) && SecondActor->ActorHasTag(FName("Player"));
	const bool bBothAreEnemies = FirstActor->ActorHasTag(FName("Enemy")) && SecondActor->ActorHasTag(FName("Enemy"));
	const bool bFriends = bBothArePlayers || bBothAreEnemies;
	return !bFriends;
}

FGameplayEffectContextHandle UDRAbilitySystemLibrary::ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams)
{
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
	AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();

	// ===== 근접 전용 피격 대상 가로채기 (Plan6 §5.11) =====
	// 홀로그램 두더지처럼 "2m 안에서의 공격만 유효하고 그 밖은 투과"하는 대상을 여기서 처리한다.
	// 모든 공격이 이 함수를 지나므로 공격 종류마다 손대지 않아도 된다.
	// GE 는 적용하지 않는다 - 어트리뷰트를 쓰지 않는 대상이라 PostGameplayEffectExecute 를 태우면 위험하다.
	if (DamageEffectParams.TargetAbilitySystemComponent)
	{
		AActor* TargetAvatarActor = DamageEffectParams.TargetAbilitySystemComponent->GetAvatarActor();
		if (IDRProximityHitOnly* ProximityTarget = Cast<IDRProximityHitOnly>(TargetAvatarActor))
		{
			if (ProximityTarget->AcceptsHitFrom(SourceAvatarActor))
			{
				ProximityTarget->HandleProximityHit(SourceAvatarActor);
			}
			// 범위 밖이면 아무 일도 하지 않는다 (투과)
			return FGameplayEffectContextHandle();
		}
	}

	FGameplayEffectContextHandle EffectContexthandle = DamageEffectParams.SourceAbilitySystemComponent->MakeEffectContext();
	EffectContexthandle.AddSourceObject(SourceAvatarActor);
	SetDeathImpulse(EffectContexthandle, DamageEffectParams.DeathImpulse);
	SetKnockbackForce(EffectContexthandle, DamageEffectParams.KnockbackForce);
	if (FDRGameplayEffectContext* DRContext = static_cast<FDRGameplayEffectContext*>(EffectContexthandle.Get()))
	{
		DRContext->SetSourceAbilityTags(DamageEffectParams.SourceAbilityTags);
	}
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

	// 넉백 방향 계산 (타겟의 현재 속도 사용)
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

FGameplayEffectSpecHandle UDRAbilitySystemLibrary::CreateAndApplyEffectSpec(
	UAbilitySystemComponent* ASC,
	TSubclassOf<UGameplayEffect> EffectClass,
	AActor* SourceObject,
	float Level)
{
	FGameplayEffectSpecHandle SpecHandle = CreateEffectSpec(ASC, EffectClass, SourceObject, Level);

	if (SpecHandle.IsValid())
	{
		ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	}

	return SpecHandle;
}

FGameplayEffectSpecHandle UDRAbilitySystemLibrary::CreateEffectSpec(
	UAbilitySystemComponent* ASC,
	TSubclassOf<UGameplayEffect> EffectClass,
	AActor* SourceObject,
	float Level)
{
	if (!ASC || !EffectClass)
	{
		return FGameplayEffectSpecHandle();
	}

	FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
	if (SourceObject)
	{
		ContextHandle.AddSourceObject(SourceObject);
	}

	return ASC->MakeOutgoingSpec(EffectClass, Level, ContextHandle);
}

void UDRAbilitySystemLibrary::ApplyEffectSpecWithSetByCaller(
	UAbilitySystemComponent* ASC,
	FGameplayEffectSpecHandle& SpecHandle,
	const FGameplayTag& Tag,
	float Magnitude)
{
	if (!ASC || !SpecHandle.IsValid())
	{
		return;
	}

	SpecHandle.Data.Get()->SetSetByCallerMagnitude(Tag, Magnitude);
	ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}
