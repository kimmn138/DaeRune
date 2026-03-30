// Copyright DaeRune


#include "Actor/DRProjectile.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "DaeRune/DaeRune.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"

ADRProjectile::ADRProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	// 멀티플레이 동기화 활성화
	bReplicates = true;

	// 충돌 감지용 구체 컴포넌트 설정
	Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
	SetRootComponent(Sphere);
	Sphere->SetCollisionObjectType(ECC_Projectile);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 발사체 물리 이동 설정
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovement");
	ProjectileMovement->InitialSpeed = 550.f;
	ProjectileMovement->MaxSpeed = 550.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void ADRProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	// 자동 소멸 타이머 설정
	SetLifeSpan(LifeSpan);
	// 이동 동기화 활성화
	SetReplicateMovement(true);
	// 충돌 이벤트 바인딩
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &ADRProjectile::OnSphereOverlap);

	// 비행 중 반복 사운드 재생
	LoopingSoundComponent = UGameplayStatics::SpawnSoundAttached(LoopingSound, GetRootComponent());
}

void ADRProjectile::OnHit()
{
	// 충돌 시 이펙트 및 사운드 재생
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::PlaySoundAtLocation(World, ImpactSound, GetActorLocation(), FRotator::ZeroRotator);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ImpactEffect, GetActorLocation());
	}
	// 반복 사운드 정리
	if (LoopingSoundComponent)
	{
		LoopingSoundComponent->Stop();
		LoopingSoundComponent->DestroyComponent();
	}
	bHit = true;
}

void ADRProjectile::Destroyed()
{
	// 액터 파괴 시 사운드 리소스 정리
	if (LoopingSoundComponent)
	{
		LoopingSoundComponent->Stop();
		LoopingSoundComponent->DestroyComponent();
	}
	// 클라이언트에서 파괴될 때 이펙트 재생 (서버는 OnHit에서 처리됨)
	if (!bHit && !HasAuthority()) OnHit();
	Super::Destroyed();
}

void ADRProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 데미지 소스 검증
	if (DamageEffectParams.SourceAbilitySystemComponent == nullptr) return;
	AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor(); 
	// 자기 자신 무시
	if (SourceAvatarActor == OtherActor) return;
	// 아군 무시
	if (!UDRAbilitySystemLibrary::IsNotFriend(SourceAvatarActor, OtherActor)) return;
	// 이펙트 재생
	if (!bHit) OnHit();

	// 서버에서만 데미지 처리
	if (HasAuthority())
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
		{
			// 사망 시 넉백 벡터 설정
			const FVector DeathImpulse = GetActorForwardVector() * DamageEffectParams.DeathImpulseMagnitude; 
			DamageEffectParams.DeathImpulse = DeathImpulse;
			// 넉백 확률 계산 및 적용
			const bool bKnockback = FMath::RandRange(1, 100) < DamageEffectParams.KnockbackChance;
			if (bKnockback)
			{
				FRotator Rotation = GetActorRotation();
				// 위쪽으로 넉백
				Rotation.Pitch = 45.f;

				const FVector KnockbackDirection = Rotation.Vector();
				const FVector KnockbackForce = KnockbackDirection * DamageEffectParams.KnockbackForceMagnitude;
				DamageEffectParams.KnockbackForce = KnockbackForce;
			}

			// GAS를 통한 데미지 적용
			DamageEffectParams.TargetAbilitySystemComponent = TargetASC; 
			UDRAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);
		}

		// 서버에서 발사체 파괴
		Destroy();
	}
	// 클라이언트에서는 타격 플래그만 설정
	else bHit = true;
}
