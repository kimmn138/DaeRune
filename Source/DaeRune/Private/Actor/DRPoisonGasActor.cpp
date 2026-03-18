// Copyright DaeRune


#include "Actor/DRPoisonGasActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actor/DRCleanserSite.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"

TMap<TWeakObjectPtr<AActor>, int32> ADRPoisonGasActor::OverlapCountMap;

ADRPoisonGasActor::ADRPoisonGasActor()
{
	InfiniteEffectApplicationPolicy = EEffectApplicationPolicy::ApplyOnOverlap;
	InfiniteEffectRemovalPolicy = EEffectRemovalPolicy::RemoveOnEndOverlap;

	bApplyEffectsToEnemies = false;
	bReplicates = true;

	// DecalComponent 생성
	GroundDecal = CreateDefaultSubobject<UDecalComponent>("GroundDecal");
	GroundDecal->SetupAttachment(GetRootComponent());
	GroundDecal->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	GroundDecal->DecalSize = FVector(300.0f, 312.5f, 312.5f);
}

void ADRPoisonGasActor::BeginPlay()
{
	Super::BeginPlay();

	// Decal 크기 동기화
	GroundDecal->DecalSize = FVector(300.0f, DecalRadius, DecalRadius);

	// 경고 머티리얼 설정
	if (WarningDecalMaterial)
	{
		GroundDecal->SetDecalMaterial(WarningDecalMaterial);
	}

	// 3초 후 Active로 전환
	CurrentPhase = EPoisonGasPhase::Warning;
	GetWorldTimerManager().SetTimer(
		PhaseTransitionTimerHandle, this,
		&ADRPoisonGasActor::TransitionToActive,
		3.0f, false
	);

	// 효과 판정 타이머 시작 (서버에서만)
	if (HasAuthority())
	{
		GetWorldTimerManager().SetTimer(
			EffectCheckTimerHandle, this,
			&ADRPoisonGasActor::CheckNearbyTargets,
			EffectCheckInterval,
			true,    // 반복
			3.0f     // 첫 실행 지연 = 경고 시간과 동일
		);
	}
}

void ADRPoisonGasActor::ApplySlowEffectToTarget(AActor* TargetActor)
{
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 클렌저사이트 필터링
	if (Cast<ADRCleanserSite>(TargetActor)) return;
	
	if (!SlowGameplayEffectClass) return;

	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC) return;

	FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(this);
	EffectContextHandle.AddInstigator(this, this);
	
	const FGameplayEffectSpecHandle EffectSpecHandle = TargetASC->MakeOutgoingSpec(SlowGameplayEffectClass, ActorLevel, EffectContextHandle);
	const FActiveGameplayEffectHandle ActiveEffectHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());

	// Infinite인 경우에만 핸들 저장
	const bool bIsInfinite = EffectSpecHandle.Data.Get()->Def.Get()->DurationPolicy == EGameplayEffectDurationType::Infinite;
	if (bIsInfinite && SlowEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
	{
		SlowEffectHandles.Add(ActiveEffectHandle, TargetASC);
	}
}

void ADRPoisonGasActor::TransitionToActive()
{
	CurrentPhase = EPoisonGasPhase::Active;

	// 활성화 머티리얼로 교체
	if (ActiveDecalMaterial)
	{
		GroundDecal->SetDecalMaterial(ActiveDecalMaterial);
	}

	// 타이머가 이미 돌고 있으므로, 다음 CheckNearbyTargets()에서 자동 감지됨
	// (첫 실행 지연 3.0초 = 경고 시간이므로, Active 전환 직후에 체크 시작)
}

void ADRPoisonGasActor::OnPoisonGasOverlap(AActor* TargetActor)
{
	// 적 필터링
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 클렌저사이트 필터링
	if (Cast<ADRCleanserSite>(TargetActor)) return;

	// 경고 페이즈면 효과 미적용
	if (CurrentPhase != EPoisonGasPhase::Active) return;

	// 이미 이 액터에서 효과 적용 중이면 스킵
	if (ActiveEffectTargets.Contains(TargetActor)) return;

	int32& Count = OverlapCountMap.FindOrAdd(TargetActor);
	Count++;

	// 처음 진입할 때만 GE 적용
	if (Count == 1)
	{
		OnOverlap(TargetActor);
		ApplySlowEffectToTarget(TargetActor);
	}

	// 추적
	ActiveEffectTargets.Add(TargetActor);
}

void ADRPoisonGasActor::OnPoisonGasEndOverlap(AActor* TargetActor)
{
	// 적 필터링
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return;

	// 클렌저사이트 필터링
	if (Cast<ADRCleanserSite>(TargetActor)) return;

	// 추적 제거
	ActiveEffectTargets.Remove(TargetActor);

	int32* CountPtr = OverlapCountMap.Find(TargetActor);
	if (!CountPtr) return;

	(*CountPtr)--;

	// 모든 가스 영역에서 나갔을 때만 GE 제거
	if (*CountPtr <= 0)
	{
		OnEndOverlap(TargetActor);

		// 슬로우 Effect 제거
		if (SlowEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
		{
			UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
			if (!IsValid(TargetASC)) return;

			TArray<FActiveGameplayEffectHandle> HandlesToRemove;
			for (const TTuple<FActiveGameplayEffectHandle, UAbilitySystemComponent*>& HandlePair : SlowEffectHandles)
			{
				if (TargetASC == HandlePair.Value)
				{
					TargetASC->RemoveActiveGameplayEffect(HandlePair.Key, 1);
					HandlesToRemove.Add(HandlePair.Key);
				}
			}

			for (const FActiveGameplayEffectHandle& Handle : HandlesToRemove)
			{
				SlowEffectHandles.FindAndRemoveChecked(Handle);
			}
		}

		// EndOverlap 시 슬로우 적용하는 케이스 (일반적이진 않지만)
		if (SlowEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnEndOverlap)
		{
			ApplySlowEffectToTarget(TargetActor);
		}

		OverlapCountMap.Remove(TargetActor);
	}
}

void ADRPoisonGasActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	RemoveAllPoisonEffects();
	GetWorldTimerManager().ClearTimer(PhaseTransitionTimerHandle);
	GetWorldTimerManager().ClearTimer(EffectCheckTimerHandle);
	Super::EndPlay(EndPlayReason);
}

void ADRPoisonGasActor::RemoveAllPoisonEffects()
{
	TArray<TWeakObjectPtr<AActor>> TargetsToRemove;
	for (const TWeakObjectPtr<AActor>& ActorPtr : ActiveEffectTargets)
	{
		TargetsToRemove.Add(ActorPtr);
	}
	for (const TWeakObjectPtr<AActor>& ActorPtr : TargetsToRemove)
	{
		if (AActor* Actor = ActorPtr.Get())
		{
			OnPoisonGasEndOverlap(Actor);
		}
	}
	ActiveEffectTargets.Empty();
}

bool ADRPoisonGasActor::IsTargetInEffectZone(AActor* Target) const
{
	if (!Target || !IsValid(Target)) return false;

	// 1. 2D 거리 체크 (XY 평면에서 원형 범위)
	const FVector GasLocation = GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Distance2D = FVector::Dist2D(GasLocation, TargetLocation);

	if (Distance2D > EffectRadius)
	{
		return false;
	}

	// 2. 착지 여부 체크 (점프 중이면 효과 미적용)
	if (const ACharacter* Character = Cast<ACharacter>(Target))
	{
		if (const UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
		{
			if (!MovementComp->IsMovingOnGround())
			{
				return false;  // 점프 중 → 효과 미적용
			}
		}
	}

	return true;
}

void ADRPoisonGasActor::CheckNearbyTargets()
{
	// 서버에서만 실행
	if (!HasAuthority()) return;

	// 경고 페이즈면 효과 미적용
	if (CurrentPhase != EPoisonGasPhase::Active) return;

	UWorld* World = GetWorld();
	if (!World) return;

	// 현재 프레임에 효과 범위 안에 있는 대상 수집
	TSet<TWeakObjectPtr<AActor>> CurrentFrameTargets;

	// 모든 Character를 순회 (플레이어 + 적)
	for (TActorIterator<ACharacter> It(World); It; ++It)
	{
		ACharacter* Character = *It;
		if (!Character || !IsValid(Character)) continue;

		// 적 필터링 (기존 로직과 동일)
		if (Character->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) continue;

		// 클렌저사이트 필터링
		if (Cast<ADRCleanserSite>(Character)) continue;

		// 효과 범위 판정
		if (IsTargetInEffectZone(Character))
		{
			CurrentFrameTargets.Add(Character);
		}
	}

	// --- 신규 진입: 이번 프레임에 있는데 이전에 없었던 대상 → 효과 적용 ---
	for (const TWeakObjectPtr<AActor>& TargetPtr : CurrentFrameTargets)
	{
		if (!ActiveEffectTargets.Contains(TargetPtr))
		{
			AActor* Target = TargetPtr.Get();
			if (Target)
			{
				// 기존 OnPoisonGasOverlap의 효과 적용 로직 재사용
				int32& Count = OverlapCountMap.FindOrAdd(Target);
				Count++;
				if (Count == 1)
				{
					OnOverlap(Target);                // 부모 ADREffectActor의 GE 적용
					ApplySlowEffectToTarget(Target);  // 슬로우 GE 적용
				}
				ActiveEffectTargets.Add(Target);
			}
		}
	}

	// --- 이탈: 이전에 있었는데 이번 프레임에 없는 대상 → 효과 제거 ---
	TArray<TWeakObjectPtr<AActor>> TargetsToRemove;
	for (const TWeakObjectPtr<AActor>& TargetPtr : ActiveEffectTargets)
	{
		if (!CurrentFrameTargets.Contains(TargetPtr))
		{
			TargetsToRemove.Add(TargetPtr);
		}
	}

	for (const TWeakObjectPtr<AActor>& TargetPtr : TargetsToRemove)
	{
		AActor* Target = TargetPtr.Get();
		if (Target)
		{
			// 기존 OnPoisonGasEndOverlap의 효과 제거 로직 재사용
			ActiveEffectTargets.Remove(Target);

			int32* CountPtr = OverlapCountMap.Find(Target);
			if (CountPtr)
			{
				(*CountPtr)--;
				if (*CountPtr <= 0)
				{
					OnEndOverlap(Target);              // 부모 ADREffectActor의 GE 제거

					// 슬로우 GE 제거
					if (SlowEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnEndOverlap)
					{
						UAbilitySystemComponent* TargetASC =
							UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
						if (IsValid(TargetASC))
						{
							TArray<FActiveGameplayEffectHandle> HandlesToRemove;
							for (const auto& HandlePair : SlowEffectHandles)
							{
								if (TargetASC == HandlePair.Value)
								{
									TargetASC->RemoveActiveGameplayEffect(HandlePair.Key, 1);
									HandlesToRemove.Add(HandlePair.Key);
								}
							}
							for (const auto& Handle : HandlesToRemove)
							{
								SlowEffectHandles.FindAndRemoveChecked(Handle);
							}
						}
					}

					OverlapCountMap.Remove(Target);
				}
			}
		}
		else
		{
			// 이미 파괴된 액터 → 추적에서 제거만
			ActiveEffectTargets.Remove(TargetPtr);
		}
	}
}
