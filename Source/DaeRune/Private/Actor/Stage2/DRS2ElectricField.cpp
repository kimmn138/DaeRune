// Copyright DaeRune

#include "Actor/Stage2/DRS2ElectricField.h"

#include "AbilitySystemComponent.h"
#include "Character/DRCharacter.h"
#include "Components/AudioComponent.h"
#include "Components/DecalComponent.h"
#include "Components/SphereComponent.h"
#include "DaeRune/DRLogChannels.h"
#include "Interaction/CombatInterface.h"
#include "NiagaraComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADRS2ElectricField::ADRS2ElectricField()
{
	PrimaryActorTick.bCanEverTick = false;

	bReplicates = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	EffectSphere = CreateDefaultSubobject<USphereComponent>(TEXT("EffectSphere"));
	EffectSphere->SetupAttachment(SceneRoot);
	EffectSphere->SetSphereRadius(150.f);
	EffectSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	EffectSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	EffectSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	EffectSphere->SetGenerateOverlapEvents(true);

	GroundDecal = CreateDefaultSubobject<UDecalComponent>(TEXT("GroundDecal"));
	GroundDecal->SetupAttachment(SceneRoot);
	GroundDecal->SetRelativeRotation(FRotator(-90.f, 0.f, 0.f));
	GroundDecal->DecalSize = FVector(300.f, 150.f, 150.f);

	FieldFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("FieldFX"));
	FieldFX->SetupAttachment(SceneRoot);
	FieldFX->bAutoActivate = false;

	LoopAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("LoopAudio"));
	LoopAudio->SetupAttachment(SceneRoot);
	LoopAudio->bAutoActivate = false;
}

void ADRS2ElectricField::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ADRS2ElectricField, FieldRadius);
}

void ADRS2ElectricField::BeginPlay()
{
	Super::BeginPlay();

	if (FieldDecalMaterial)
	{
		GroundDecal->SetDecalMaterial(FieldDecalMaterial);
	}
	if (FieldNiagaraSystem)
	{
		FieldFX->SetAsset(FieldNiagaraSystem);
	}
	if (FieldLoopSound)
	{
		LoopAudio->SetSound(FieldLoopSound);
	}

	EffectSphere->OnComponentBeginOverlap.AddDynamic(this, &ADRS2ElectricField::OnSphereBegin);
	EffectSphere->OnComponentEndOverlap.AddDynamic(this, &ADRS2ElectricField::OnSphereEnd);

	// 클라이언트는 초기 복제로 FieldRadius 를 이미 받은 상태다.
	ApplyVisual();

	// ★델리게이트 바인딩이 끝난 지금이 활성화 가능한 최초 시점이다 (헤더 StartActivation 주석 참고)
	bBegunPlay = true;
	if (bPendingActivation)
	{
		StartActivation();
	}
}

void ADRS2ElectricField::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// GE 누수 차단 — 파괴/레벨 전환/플레이어 사망 어느 경로로 끝나도 전부 회수한다
	RemoveFromAll();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ActivationTimer);
	}

	if (LoopAudio && LoopAudio->IsPlaying())
	{
		LoopAudio->Stop();
	}

	Super::EndPlay(EndPlayReason);
}

void ADRS2ElectricField::InitField(AActor* InInstigatorActor, float InRadius, int32 InPlayerCount)
{
	if (!HasAuthority()) return;

	FieldInstigator = InInstigatorActor;
	FieldRadius = (InRadius > 0.f) ? InRadius : DefaultRadius;
	FieldLevel = FMath::Max(1, InPlayerCount);

	if (!FieldDamageEffectClass)
	{
		UE_LOG(LogDR, Error,
			TEXT("[S2Field] FieldDamageEffectClass 미설정 — 전기장이 데미지를 주지 않습니다. BP_S2ElectricField 를 확인하세요."));
	}

	// 리슨 서버에서도 연출이 돌아야 하므로 RepNotify 수동 호출 (Plan6 §15.0 규약).
	// BeginPlay 전이라 컴포넌트는 이미 등록되어 있으므로 데칼/나이아가라 갱신은 안전하다.
	OnRep_FieldRadius();

	// 실제 활성화(콜리전 ON)는 BeginPlay 에서 델리게이트가 바인딩된 뒤에 한다
	bPendingActivation = true;
	if (bBegunPlay)
	{
		StartActivation();
	}
}

void ADRS2ElectricField::StartActivation()
{
	if (!HasAuthority() || bActivated) return;

	bPendingActivation = false;

	if (ActivationDelay > 0.f)
	{
		GetWorld()->GetTimerManager().SetTimer(
			ActivationTimer, this, &ADRS2ElectricField::ActivateField, ActivationDelay, false);
	}
	else
	{
		ActivateField();
	}
}

void ADRS2ElectricField::ActivateField()
{
	if (!HasAuthority() || bActivated) return;
	bActivated = true;

	EffectSphere->SetSphereRadius(FieldRadius);

	// 콜리전을 켜는 순간 UpdateOverlaps 가 돌아 BeginOverlap 이 발화한다.
	// 그래도 "이미 서 있던" 플레이어를 확실히 잡기 위해 한 번 더 훑는다 (ApplyToPlayer 는 멱등).
	EffectSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

	TArray<AActor*> Overlapping;
	EffectSphere->GetOverlappingActors(Overlapping, ADRCharacter::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		if (ADRCharacter* Player = Cast<ADRCharacter>(Actor))
		{
			ApplyToPlayer(Player);
		}
	}
}

void ADRS2ElectricField::OnRep_FieldRadius()
{
	ApplyVisual();
}

void ADRS2ElectricField::ApplyVisual()
{
	if (GroundDecal)
	{
		GroundDecal->DecalSize = FVector(DecalProjectionDepth, FieldRadius, FieldRadius);
		GroundDecal->MarkRenderStateDirty();
	}

	if (FieldFX && FieldFX->GetAsset() && !FieldFX->IsActive())
	{
		FieldFX->Activate(true);
	}

	if (LoopAudio && LoopAudio->Sound && !LoopAudio->IsPlaying())
	{
		LoopAudio->Play();
	}

	OnFieldActivated(FieldRadius);
}

void ADRS2ElectricField::OnSphereBegin(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/, bool /*bFromSweep*/, const FHitResult& /*SweepResult*/)
{
	// ★플레이어만. 두더지 자신·다른 적·열차·설치물은 캐스트 실패로 자동 배제된다.
	if (ADRCharacter* Player = Cast<ADRCharacter>(OtherActor))
	{
		ApplyToPlayer(Player);
	}
}

void ADRS2ElectricField::OnSphereEnd(UPrimitiveComponent* /*OverlappedComponent*/, AActor* OtherActor,
	UPrimitiveComponent* /*OtherComp*/, int32 /*OtherBodyIndex*/)
{
	if (ADRCharacter* Player = Cast<ADRCharacter>(OtherActor))
	{
		RemoveFromPlayer(Player);
	}
}

void ADRS2ElectricField::ApplyToPlayer(ADRCharacter* Player)
{
	if (!HasAuthority() || !bActivated) return;
	if (!IsValid(Player) || !FieldDamageEffectClass) return;
	if (AppliedHandles.Contains(Player)) return;
	if (ICombatInterface::Execute_IsDead(Player)) return;

	UAbilitySystemComponent* TargetASC = Player->GetAbilitySystemComponent();
	if (!TargetASC) return;

	// ADRPoisonGasActor.cpp:92-97 관례 — 타깃 ASC 가 스펙을 만들고 인스티게이터로 출처를 남긴다
	FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
	ContextHandle.AddSourceObject(FieldInstigator.Get());
	ContextHandle.AddInstigator(FieldInstigator.Get(), this);

	// ★스펙 레벨 = 인원수. GE 의 Modifier(ScalableFloat)가 이 레벨로 커브를 읽어 틱 데미지를 정한다.
	//   코드에는 숫자가 없다 — 전부 CT_Damage / Abilities.MoleBoss.ElectricField 에 있다.
	const FGameplayEffectSpecHandle SpecHandle =
		TargetASC->MakeOutgoingSpec(FieldDamageEffectClass, static_cast<float>(FieldLevel), ContextHandle);
	if (!SpecHandle.IsValid()) return;

	const FActiveGameplayEffectHandle Handle = TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
	AppliedHandles.Add(Player, Handle);
}

void ADRS2ElectricField::RemoveFromPlayer(ADRCharacter* Player)
{
	if (!HasAuthority() || !IsValid(Player)) return;

	FActiveGameplayEffectHandle Handle;
	if (!AppliedHandles.RemoveAndCopyValue(Player, Handle)) return;

	if (UAbilitySystemComponent* TargetASC = Player->GetAbilitySystemComponent())
	{
		TargetASC->RemoveActiveGameplayEffect(Handle);
	}
}

void ADRS2ElectricField::RemoveFromAll()
{
	if (!HasAuthority())
	{
		AppliedHandles.Empty();
		return;
	}

	for (const TPair<TWeakObjectPtr<ADRCharacter>, FActiveGameplayEffectHandle>& Pair : AppliedHandles)
	{
		ADRCharacter* Player = Pair.Key.Get();
		if (!IsValid(Player)) continue;

		if (UAbilitySystemComponent* TargetASC = Player->GetAbilitySystemComponent())
		{
			TargetASC->RemoveActiveGameplayEffect(Pair.Value);
		}
	}

	AppliedHandles.Empty();
}
