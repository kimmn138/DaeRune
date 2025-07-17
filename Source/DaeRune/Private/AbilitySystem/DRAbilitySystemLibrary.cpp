// Copyright DaeRune


#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DRAbilityTypes.h"
#include "DRGameplayTags.h"
#include "Game/DRGameModeBase.h"
#include "Interaction/CombatInterface.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DRPlayerState.h"
#include "UI/HUD/DRHUD.h"
#include "UI/WidgetController/DRWidgetController.h"
#include "Engine/OverlapResult.h"

// 위젯 컨트롤러 파라미터 생성 기능
bool UDRAbilitySystemLibrary::MakeWidgetControllerParams(const UObject* WorldContextObject, FWidgetControllerParams& OutWCParams, ADRHUD*& OutDRHUD)
{
	// 플레이어 컨트롤러 획득 단계
	if (APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0))
	{
		// HUD 객체 캐스팅 단계
		OutDRHUD = Cast<ADRHUD>(PC->GetHUD()); 
		if (OutDRHUD)
		{
			// 플레이어 상태 및 컴포넌트 추출 단계
			ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>();
			UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
			UAttributeSet* AS = PS->GetAttributeSet();
			
			// 위젯 컨트롤러 파라미터 설정 단계
			OutWCParams.AttributeSet = AS;
			OutWCParams.AbilitySystemComponent = ASC;
			OutWCParams.PlayerState = PS;
			OutWCParams.PlayerController = PC;
			return true; // 성공 여부 반환
		}
	}
	return false; // 실패 여부 반환
}

// 오버레이 위젯 컨트롤러 반환 기능
UOverlayWidgetController* UDRAbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	// 파라미터 생성 시도 단계
	FWidgetControllerParams WCParams;
	ADRHUD* DRHUD = nullptr;
	if (MakeWidgetControllerParams(WorldContextObject, WCParams, DRHUD))
	{
		// 오버레이 컨트롤러 반환 단계
		return DRHUD->GetOverlayWidgetController(WCParams);
	}
	return nullptr; // 반환 실패 처리
}

// 플레이어 기본 속성 초기화 기능
void UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(const UObject* WorldContextObject, EPlayerCharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC)
{
	// 아바타 액터 참조 단계
	AActor* AvatarActor = ASC->GetAvatarActor();

	// 클래스 정보 조회 단계
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	FCharacterClassDefaultInfo ClassDefaultInfo = CharacterClassInfo->GetPlayerClassDefaultInfo(CharacterClass);

	// 생명력 속성 효과 컨텍스트 생성 단계
	FGameplayEffectContextHandle VitalAttributesContextHandle = ASC->MakeEffectContext();
	VitalAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle VitalAttributesSpecHandle = ASC->MakeOutgoingSpec(ClassDefaultInfo.VitalAttributes, Level, VitalAttributesContextHandle);
	// 자체 적용 단계
	ASC->ApplyGameplayEffectSpecToSelf(*VitalAttributesSpecHandle.Data.Get());
}

// 적 기본 속성 초기화 기능
void UDRAbilitySystemLibrary::InitializeEnemyDefaultAttributes(const UObject* WorldContextObject, EEnemyCharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC)
{
	// 아바타 액터 참조 단계
	AActor* AvatarActor = ASC->GetAvatarActor();

	// 클래스 정보 조회 단계
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	FCharacterClassDefaultInfo ClassDefaultInfo = CharacterClassInfo->GetEnemyClassDefaultInfo(CharacterClass);

	// 생명력 속성 효과 컨텍스트 생성 단계
	FGameplayEffectContextHandle VitalAttributesContextHandle = ASC->MakeEffectContext();
	VitalAttributesContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle VitalAttributesSpecHandle = ASC->MakeOutgoingSpec(ClassDefaultInfo.VitalAttributes, Level, VitalAttributesContextHandle);
	// 자체 적용 단계
	ASC->ApplyGameplayEffectSpecToSelf(*VitalAttributesSpecHandle.Data.Get());
}

// 플레이어 시작 능력 부여 기능
void UDRAbilitySystemLibrary::GivePlayerStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, EPlayerCharacterClass CharacterClass)
{
	// 클래스 정보 조회 단계
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (CharacterClassInfo == nullptr) return; // 예외 처리 단계
	// 공통 능력 부여 단계
	for (TSubclassOf<UGameplayAbility> AbilityClass : CharacterClassInfo->CommonAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		ASC->GiveAbility(AbilitySpec);
	}
	// 클래스별 시작 능력 부여 단계
	const FCharacterClassDefaultInfo& DefaultInfo = CharacterClassInfo->GetPlayerClassDefaultInfo(CharacterClass);
	for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultInfo.StartupAbilities)
	{
		if (ASC->GetAvatarActor()->Implements<UCombatInterface>())
		{
			FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, ICombatInterface::Execute_GetPlayerLevel(ASC->GetAvatarActor()));
			ASC->GiveAbility(AbilitySpec);
		}
	}
}

// 적 시작 능력 부여 기능
void UDRAbilitySystemLibrary::GiveEnemyStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, EEnemyCharacterClass CharacterClass)
{
	// 클래스 정보 조회 단계
	UCharacterClassInfo* CharacterClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (CharacterClassInfo == nullptr) return; // 예외 처리 단계
	// 공통 능력 부여 단계
	for (TSubclassOf<UGameplayAbility> AbilityClass : CharacterClassInfo->CommonAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		ASC->GiveAbility(AbilitySpec);
	}
	// 클래스별 시작 능력 부여 단계
	const FCharacterClassDefaultInfo& DefaultInfo = CharacterClassInfo->GetEnemyClassDefaultInfo(CharacterClass);
	for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultInfo.StartupAbilities)
	{
		if (ASC->GetAvatarActor()->Implements<UCombatInterface>())
		{
			FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, ICombatInterface::Execute_GetPlayerLevel(ASC->GetAvatarActor()));
			ASC->GiveAbility(AbilitySpec);
		}
	}
}

// 게임모드 내 클래스 정보 참조 기능
UCharacterClassInfo* UDRAbilitySystemLibrary::GetCharacterClassInfo(const UObject* WorldContextObject)
{
	// 게임모드 캐스팅 단계
	const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (DRGameMode == nullptr) return nullptr; // 반환 예외 처리
	return DRGameMode->CharacterClassInfo; // 캐릭터 클래스 정보 반환
}

// 게임모드 내 능력 정보 참조 기능
UAbilityInfo* UDRAbilitySystemLibrary::GetAbilityInfo(const UObject* WorldContextObject)
{
	// 게임모드 캐스팅 단계
	const ADRGameModeBase* DRGameMode = Cast<ADRGameModeBase>(UGameplayStatics::GetGameMode(WorldContextObject));
	if (DRGameMode == nullptr) return nullptr; // 반환 예외 처리
	return DRGameMode->AbilityInfo; // 능력 정보 반환
}

// 디버프 성공 여부 검사 기능
bool UDRAbilitySystemLibrary::IsSuccessfulDebuff(const FGameplayEffectContextHandle& EffectContextHandle)
{
	// 컨텍스트 캐스팅 단계
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->IsSuccessfulDebuff(); // 디버프 성공 여부 반환
	}
	return false; // 기본 반환 값
}

// 디버프 피해량 조회 기능
float UDRAbilitySystemLibrary::GetDebuffDamage(const FGameplayEffectContextHandle& EffectContextHandle)
{
	// 컨텍스트 캐스팅 단계
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetDebuffDamage(); // 피해량 반환
	}
	return 0.f; // 기본 반환 값
}

// 디버프 지속 시간 조회 기능
float UDRAbilitySystemLibrary::GetDebuffDuration(const FGameplayEffectContextHandle& EffectContextHandle)
{
	// 컨텍스트 캐스팅 단계
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetDebuffDuration(); // 지속 시간 반환
	}
	return 0.f; // 기본 반환 값
}

// 디버프 빈도 조회 기능
float UDRAbilitySystemLibrary::GetDebuffFrequency(const FGameplayEffectContextHandle& EffectContextHandle)
{
	// 컨텍스트 캐스팅 단계
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetDebuffFrequency(); // 빈도 반환
	}
	return 0.f; // 기본 반환 값
}

// 데미지 타입 태그 조회 기능
FGameplayTag UDRAbilitySystemLibrary::GetDamageType(const FGameplayEffectContextHandle& EffectContextHandle)
{
	// 컨텍스트 캐스팅 및 유효성 검사 단계
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		if (DREffectContext->GetDamageType().IsValid())
		{
			return *DREffectContext->GetDamageType(); // 태그 반환
		}
	}
	return FGameplayTag(); // 기본 태그 반환
}

// 사망 임펄스 조회 기능
FVector UDRAbilitySystemLibrary::GetDeathImpulse(const FGameplayEffectContextHandle& EffectContextHandle)
{
	// 컨텍스트 캐스팅 단계
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetDeathImpulse(); // 임펄스 벡터 반환
	}
	return FVector::ZeroVector; // 기본 벡터 반환
}

// 넉백 포스 조회 기능
FVector UDRAbilitySystemLibrary::GetKnockbackForce(const FGameplayEffectContextHandle& EffectContextHandle)
{
	// 컨텍스트 캐스팅 단계
	if (const FDRGameplayEffectContext* DREffectContext = static_cast<const FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		return DREffectContext->GetKnockbackForce(); // 포스 벡터 반환
	} 
	return FVector::ZeroVector; // 기본 벡터 반환
}

// 디버프 성공 여부 설정 기능
void UDRAbilitySystemLibrary::SetIsSuccessfulDebuff(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, bool bInSuccessfulDebuff)
{
	// 컨텍스트 캐스팅 단계
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetIsSuccessfulDebuff(bInSuccessfulDebuff); // 성공 여부 설정 단계
	}
}

// 디버프 피해량 설정 기능
void UDRAbilitySystemLibrary::SetDebuffDamage(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, float InDamage)
{
	// 컨텍스트 캐스팅 단계
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetDebuffDamage(InDamage); // 피해량 설정 단계
	}
}

// 디버프 지속 시간 설정 기능
void UDRAbilitySystemLibrary::SetDebuffDuration(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, float InDuration)
{
	// 컨텍스트 캐스팅 단계
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetDebuffDuration(InDuration); // 지속 시간 설정 단계
	}
}

// 디버프 빈도 설정 기능
void UDRAbilitySystemLibrary::SetDebuffFrequency(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, float InFrequency)
{
	// 컨텍스트 캐스팅 단계
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetDebuffFrequency(InFrequency); // 빈도 설정 단계
	}
}

// 데미지 타입 태그 설정 기능
void UDRAbilitySystemLibrary::SetDamageType(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, const FGameplayTag& InDamageType)
{
	// 컨텍스트 캐스팅 단계
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		// 태그 복제 및 설정 단계
		const TSharedPtr<FGameplayTag> DamageType = MakeShared<FGameplayTag>(InDamageType);
		DREffectContext->SetDamageType(DamageType);
	}
}

// 사망 임펄스 설정 기능
void UDRAbilitySystemLibrary::SetDeathImpulse(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, const FVector& InImpulse)
{
	// 컨텍스트 캐스팅 단계
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{
		DREffectContext->SetDeathImpulse(InImpulse); // 임펄스 설정 단계
	}
}

// 넉백 포스 설정 기능
void UDRAbilitySystemLibrary::SetKnockbackForce(UPARAM(ref)FGameplayEffectContextHandle& EffectContextHandle, const FVector& InForce)
{
	// 컨텍스트 캐스팅 단계
	if (FDRGameplayEffectContext* DREffectContext = static_cast<FDRGameplayEffectContext*>(EffectContextHandle.Get()))
	{ 
		DREffectContext->SetKnockbackForce(InForce); // 포스 설정 단계
	}
}

// 반경 내 살아있는 플레이어 검색 기능
void UDRAbilitySystemLibrary::GetLivePlayersWithinRadius(const UObject* WorldContextObject, TArray<AActor*>& OutOverlappingActors, const TArray<AActor*>& ActorsToIgnore, float Radius, const FVector& SphereOrigin)
{
	// 충돌 쿼리 파라미터 구성 단계
	FCollisionQueryParams SphereParams;
	SphereParams.AddIgnoredActors(ActorsToIgnore);

	// 월드 컨텍스트 획득 단계
	if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		// 오버랩 결과 수집 단계
		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByObjectType(Overlaps, SphereOrigin, FQuat::Identity, FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects), FCollisionShape::MakeSphere(Radius), SphereParams);
		for (FOverlapResult& Overlap : Overlaps)
		{
			// 전투 인터페이스 구현 및 생존 검증 단계
			if (Overlap.GetActor()->Implements<UCombatInterface>() && !ICombatInterface::Execute_IsDead(Overlap.GetActor()))
			{
				// 아바타 객체 추출 단계
				OutOverlappingActors.AddUnique(ICombatInterface::Execute_GetAvatar(Overlap.GetActor()));
			}
		}
	}
}

// 가장 가까운 타겟 선정 기능
void UDRAbilitySystemLibrary::GetClosestTargets(int32 MaxTargets, const TArray<AActor*>& Actors, TArray<AActor*>& OutClosestTargets, const FVector& Origin)
{
	// 대상 수 검사 단계
	if (Actors.Num() <= MaxTargets)
	{
		OutClosestTargets = Actors;
		return;
	}

	// 후보 리스트 복사 단계
	TArray<AActor*> ActorsToCheck = Actors;
	int32 NumTargetsFound = 0;

	// 거리 기반 선별 루프 단계
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
		// 선정 및 제거 단계
		ActorsToCheck.Remove(ClosestActor);
		OutClosestTargets.AddUnique(ClosestActor); 
		++NumTargetsFound;
	}
}

// 아군 여부 판별 기능
bool UDRAbilitySystemLibrary::IsNotFriend(AActor* FirstActor, AActor* SecondActor)
{
	// 플레이어 태그 검사 단계
	const bool bBothArePlayers = FirstActor->ActorHasTag(FName("Player")) && SecondActor->ActorHasTag(FName("Player"));
	// 적 태그 검사 단계
	const bool bBothAreEnemies = FirstActor->ActorHasTag(FName("Enemy")) && SecondActor->ActorHasTag(FName("Enemy"));
	// 우정 여부 판단 단계
	const bool bFriends = bBothArePlayers || bBothAreEnemies;
	return !bFriends; // 비우군 여부 반환
}

// 데미지 효과 적용 기능
FGameplayEffectContextHandle UDRAbilitySystemLibrary::ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams)
{
	// 태그 싱글톤 획득 단계
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
	// 소스 아바타 참조 단계
	const AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();

	// 효과 컨텍스트 생성 단계
	FGameplayEffectContextHandle EffectContexthandle = DamageEffectParams.SourceAbilitySystemComponent->MakeEffectContext();
	EffectContexthandle.AddSourceObject(SourceAvatarActor);
	// 임펄스 및 넉백 설정 단계
	SetDeathImpulse(EffectContexthandle, DamageEffectParams.DeathImpulse);
	SetKnockbackForce(EffectContexthandle, DamageEffectParams.KnockbackForce);
	// 스펙 생성 단계
	const FGameplayEffectSpecHandle SpecHandle = DamageEffectParams.SourceAbilitySystemComponent->MakeOutgoingSpec(DamageEffectParams.DamageGameplayEffectClass, DamageEffectParams.AbilityLevel, EffectContexthandle);

	// 태그 기반 값 할당 단계
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, DamageEffectParams.DamageType, DamageEffectParams.BaseDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Chance, DamageEffectParams.DebuffChance);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Damage, DamageEffectParams.DebuffDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Duration, DamageEffectParams.DebuffDuration);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, GameplayTags.Debuff_Frequency, DamageEffectParams.DebuffFrequency);

	// 대상 적용 단계
	DamageEffectParams.TargetAbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
	return EffectContexthandle; // 컨텍스트 반환
}

// 균등 회전된 로테이터 배열 생성 기능
TArray<FRotator> UDRAbilitySystemLibrary::EvenlySpacedRotators(const FVector& Forward, const FVector& Axis, float Spread, int32 NumRotators)
{
	TArray<FRotator> Rotators; 

	// 시작 방향 계산 단계
	const FVector LeftOfSpread = Forward.RotateAngleAxis(-Spread / 2.f, Axis);
	if (NumRotators > 1)
	{
		const float DeltaSpread = Spread / (NumRotators - 1);
		for (int32 i = 0; i < NumRotators; i++)
		{
			// 방향 회전 단계
			const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, Axis);
			Rotators.Add(Direction.Rotation()); // 회전자 변환 단계
		}
	}
	else
	{
		Rotators.Add(Forward.Rotation()); // 단일 회전자 추가 단계
	}
	return Rotators; // 배열 반환
}

// 균등 회전된 벡터 배열 생성 기능
TArray<FVector> UDRAbilitySystemLibrary::EvenlyRotatedVectors(const FVector& Forward, const FVector& Axis, float Spread, int32 NumVectors)
{
	TArray<FVector> Vectors; 

	// 시작 방향 계산 단계
	const FVector LeftOfSpread = Forward.RotateAngleAxis(-Spread / 2.f, Axis);
	if (NumVectors > 1)
	{
		const float DeltaSpread = Spread / (NumVectors - 1);
		for (int32 i = 0; i < NumVectors; i++)
		{
			// 방향 회전 단계
			const FVector Direction = LeftOfSpread.RotateAngleAxis(DeltaSpread * i, Axis);
			Vectors.Add(Direction); // 벡터 추가 단계
		}
	}
	else
	{
		Vectors.Add(Forward); // 단일 벡터 추가 단계
	}
	return Vectors; // 배열 반환
}
