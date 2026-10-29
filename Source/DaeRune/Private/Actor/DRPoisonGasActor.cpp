// Copyright DaeRune


#include "Actor/DRPoisonGasActor.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Actor/DRCleanserSite.h"
#include "Game/DRStageGameState.h"
#include "Materials/MaterialInterface.h"

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

bool ADRPoisonGasActor::CanAffect(const AActor* TargetActor) const
{
	if (!TargetActor) return false;
	if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) return false;
	return !TargetActor->IsA<ADRCleanserSite>();
}

TArray<FActiveGameplayEffectHandle> ADRPoisonGasActor::ApplyPoisonEffects(UAbilitySystemComponent* TargetASC)
{
	TArray<FActiveGameplayEffectHandle> Handles;
	if (!TargetASC) return Handles;

	FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(this);
	EffectContextHandle.AddInstigator(this, this);

	for (const TSubclassOf<UGameplayEffect>& EffectClass : { InfiniteGameplayEffectClass, SlowGameplayEffectClass })
	{
		if (!EffectClass) continue;

		const FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(EffectClass, ActorLevel, EffectContextHandle);
		if (!SpecHandle.IsValid()) continue;

		const FActiveGameplayEffectHandle ActiveHandle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		if (ActiveHandle.IsValid())
		{
			Handles.Add(ActiveHandle);
		}
	}
	return Handles;
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

	// 서버에서만 GameState에 활성 카운트 증가를 알림 (멀티캐스트로 루프 사운드 재생)
	if (HasAuthority())
	{
		if (ADRStageGameState* GS = GetWorld()->GetGameState<ADRStageGameState>())
		{
			GS->NotifyPoisonGasActivated();
			bNotifiedActive = true;
		}
	}
}

void ADRPoisonGasActor::OnPoisonGasOverlap(AActor* TargetActor)
{
	// 서버에서만, 활성 페이즈에서만
	if (!HasAuthority() || CurrentPhase != EPoisonGasPhase::Active || !CanAffect(TargetActor)) return;

	if (UDRPoisonGasSubsystem* Registry = GetWorld()->GetSubsystem<UDRPoisonGasSubsystem>())
	{
		Registry->Enter(this, TargetActor);
	}
}

void ADRPoisonGasActor::OnPoisonGasEndOverlap(AActor* TargetActor)
{
	if (!HasAuthority() || !TargetActor) return;

	if (UDRPoisonGasSubsystem* Registry = GetWorld()->GetSubsystem<UDRPoisonGasSubsystem>())
	{
		Registry->Leave(this, TargetActor);
	}
}

void ADRPoisonGasActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (HasAuthority())
	{
		if (UDRPoisonGasSubsystem* Registry = GetWorld() ? GetWorld()->GetSubsystem<UDRPoisonGasSubsystem>() : nullptr)
		{
			Registry->LeaveAll(this);
		}
	}
	GetWorldTimerManager().ClearTimer(PhaseTransitionTimerHandle);

	if (ActiveNiagaraComponent)
	{
		ActiveNiagaraComponent->Deactivate();
	}

	// 활성 카운트를 줄여 마지막 액터면 루프 사운드 정지
	if (bNotifiedActive && HasAuthority())
	{
		if (UWorld* World = GetWorld())
		{
			if (ADRStageGameState* GS = World->GetGameState<ADRStageGameState>())
			{
				GS->NotifyPoisonGasDeactivated();
			}
		}
		bNotifiedActive = false;
	}

	Super::EndPlay(EndPlayReason);
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

// ========================= UDRPoisonGasSubsystem =========================

void UDRPoisonGasSubsystem::Enter(ADRPoisonGasActor* Gas, AActor* Target)
{
	if (!Gas || !Target) return;

	FTargetState& State = Targets.FindOrAdd(Target);
	const bool bWasOutside = State.Gases.Num() == 0;
	State.Gases.Add(Gas);

	// 첫 구역에 들어설 때만 GE 를 건다
	if (bWasOutside)
	{
		UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
		State.ASC = TargetASC;
		State.Handles = Gas->ApplyPoisonEffects(TargetASC);
	}
}

void UDRPoisonGasSubsystem::Leave(ADRPoisonGasActor* Gas, AActor* Target)
{
	FTargetState* State = Targets.Find(Target);
	if (!State) return;

	State->Gases.Remove(Gas);

	// 사라진 가스가 남긴 항목도 함께 정리한다
	for (auto It = State->Gases.CreateIterator(); It; ++It)
	{
		if (!It->IsValid()) It.RemoveCurrent();
	}

	// 마지막 구역을 벗어났을 때만 뗀다
	if (State->Gases.Num() == 0)
	{
		RemoveEffects(*State);
		Targets.Remove(Target);
	}
}

void UDRPoisonGasSubsystem::LeaveAll(ADRPoisonGasActor* Gas)
{
	TArray<TWeakObjectPtr<AActor>> Inside;
	for (const TPair<TWeakObjectPtr<AActor>, FTargetState>& Pair : Targets)
	{
		if (Pair.Value.Gases.Contains(Gas))
		{
			Inside.Add(Pair.Key);
		}
	}

	for (const TWeakObjectPtr<AActor>& Target : Inside)
	{
		FTargetState* State = Targets.Find(Target);
		if (!State) continue;

		State->Gases.Remove(Gas);
		if (State->Gases.Num() == 0)
		{
			RemoveEffects(*State);
			Targets.Remove(Target);
		}
	}
}

void UDRPoisonGasSubsystem::RemoveEffects(FTargetState& State)
{
	// 폰이 파괴돼도 ASC 는 PlayerState 에 남아 있다 — 대상 액터가 아니라 ASC 로 뗀다
	if (UAbilitySystemComponent* ASC = State.ASC.Get())
	{
		for (const FActiveGameplayEffectHandle& Handle : State.Handles)
		{
			ASC->RemoveActiveGameplayEffect(Handle, 1);
		}
	}
	State.Handles.Reset();
}
