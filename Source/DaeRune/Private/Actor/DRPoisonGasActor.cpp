// Copyright DaeRune


#include "Actor/DRPoisonGasActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actor/DRCleanserSite.h"
#include "Materials/MaterialInterface.h"

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

	// NiagaraComponent 생성
	ActiveNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>("ActiveNiagaraEffect");
	ActiveNiagaraComponent->SetupAttachment(GetRootComponent());
	ActiveNiagaraComponent->bAutoActivate = false;

	// SphereComponent 생성 (Warning 중 비활성)
	EffectSphere = CreateDefaultSubobject<USphereComponent>("EffectSphere");
	EffectSphere->SetupAttachment(GetRootComponent());
	EffectSphere->SetSphereRadius(312.5f);
	EffectSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EffectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	EffectSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EffectSphere->SetGenerateOverlapEvents(true);
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

	// 나이아가라 에셋 할당 및 초기 비활성화
	if (ActiveNiagaraSystem)
	{
		ActiveNiagaraComponent->SetAsset(ActiveNiagaraSystem);
	}
	ActiveNiagaraComponent->Deactivate();

	// 구체 반지름 동기화
	EffectSphere->SetSphereRadius(EffectSphereRadius);

	// 오버랩 델리게이트 바인딩
	EffectSphere->OnComponentBeginOverlap.AddDynamic(
		this, &ADRPoisonGasActor::OnEffectSphereBeginOverlap);
	EffectSphere->OnComponentEndOverlap.AddDynamic(
		this, &ADRPoisonGasActor::OnEffectSphereEndOverlap);

	// 3초 후 Active로 전환
	CurrentPhase = EPoisonGasPhase::Warning;
	GetWorldTimerManager().SetTimer(
		PhaseTransitionTimerHandle, this,
		&ADRPoisonGasActor::TransitionToActive,
		3.0f, false
	);
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

	// 경고 데칼 숨기기
	GroundDecal->SetVisibility(false);

	const FVector GroundPos = FindGroundLocation();

	// 나이아가라 구체 활성화
	if (ActiveNiagaraComponent && ActiveNiagaraSystem)
	{
		ActiveNiagaraComponent->SetWorldLocation(GroundPos);
		ActiveNiagaraComponent->Activate(true);
	}

	// 효과 구체 콜리전 활성화 (같은 위치)
	EffectSphere->SetWorldLocation(GroundPos);
	EffectSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
}

void ADRPoisonGasActor::OnPoisonGasOverlap(AActor* TargetActor)
{
	// 서버에서만 GE 적용
	if (!HasAuthority()) return;

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
	// 서버에서만 GE 제거
	if (!HasAuthority()) return;

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

	if (ActiveNiagaraComponent)
	{
		ActiveNiagaraComponent->Deactivate();
	}

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

void ADRPoisonGasActor::OnEffectSphereBeginOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	if (OtherActor)
	{
		OnPoisonGasOverlap(OtherActor);
	}
}

void ADRPoisonGasActor::OnEffectSphereEndOverlap(
	UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
	if (OtherActor)
	{
		OnPoisonGasEndOverlap(OtherActor);
	}
}

FVector ADRPoisonGasActor::FindGroundLocation() const
{
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, 1000.f);

	FHitResult HitResult;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_WorldStatic, Params))
	{
		return HitResult.ImpactPoint;
	}

	return Start;
}
