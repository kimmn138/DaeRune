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
	// 틱 비활성 설정
	PrimaryActorTick.bCanEverTick = false;
	// 복제 활성 설정
	bReplicates = true;

	// 스피어 콜리전 컴포넌트 생성 및 루트 설정 처리
	Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
	SetRootComponent(Sphere);
	// 충돌 채널 및 응답 설정 처리
	Sphere->SetCollisionObjectType(ECC_Projectile);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// 이동 컴포넌트 생성 및 속도 설정 처리
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovement");
	ProjectileMovement->InitialSpeed = 550.f;
	ProjectileMovement->MaxSpeed = 550.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void ADRProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	// 생존 시간 설정 처리
	SetLifeSpan(LifeSpan);
	// 위치 복제 활성 설정
	SetReplicateMovement(true);
	// 오버랩 이벤트 바인딩 처리
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &ADRProjectile::OnSphereOverlap);

	// 루핑 사운드 컴포넌트 생성 처리
	LoopingSoundComponent = UGameplayStatics::SpawnSoundAttached(LoopingSound, GetRootComponent());
}

// 히트 처리 메서드 정의
void ADRProjectile::OnHit()
{
	// 임팩트 사운드 재생 처리
	UGameplayStatics::PlaySoundAtLocation(this, ImpactSound, GetActorLocation(), FRotator::ZeroRotator); 
	// 임팩트 이펙트 생성 처리
	UNiagaraFunctionLibrary::SpawnSystemAtLocation(this, ImpactEffect, GetActorLocation());
	// 루핑 사운드 정지 및 컴포넌트 제거 처리
	if (LoopingSoundComponent)
	{
		LoopingSoundComponent->Stop();
		LoopingSoundComponent->DestroyComponent();
	}
	// 히트 플래그 설정 처리
	bHit = true;
}

// 액터 파괴 시 처리 메서드 정의
void ADRProjectile::Destroyed()
{
	// 루핑 사운드 정지 및 컴포넌트 제거 처리
	if (LoopingSoundComponent)
	{
		LoopingSoundComponent->Stop();
		LoopingSoundComponent->DestroyComponent();
	}
	// 미히트 상태에서 클라이언트 권한 시 히트 처리 호출 처리
	if (!bHit && !HasAuthority()) OnHit();
	Super::Destroyed();
}

// 스피어 오버랩 이벤트 처리 메서드 정의
void ADRProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 소스 ASC 유효성 검사 처리
	if (DamageEffectParams.SourceAbilitySystemComponent == nullptr) return;
	// 소스 아바타 액터 가져오기 처리
	AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor(); 
	// 자기 자신 무시 처리
	if (SourceAvatarActor == OtherActor) return;
	// 우호 관계 검사 처리
	if (!UDRAbilitySystemLibrary::IsNotFriend(SourceAvatarActor, OtherActor)) return;
	// 히트 플래그 체크 후 OnHit 호출 처리
	if (!bHit) OnHit();

	// 서버 권한 검사 후 데미지 적용 및 파괴 처리
	if (HasAuthority())
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
		{
			// 데스 임펄스 계산 처리
			const FVector DeathImpulse = GetActorForwardVector() * DamageEffectParams.DeathImpulseMagnitude; 
			DamageEffectParams.DeathImpulse = DeathImpulse;
			// 넉백 확률 계산 처리
			const bool bKnockback = FMath::RandRange(1, 100) < DamageEffectParams.KnockbackChance;
			if (bKnockback)
			{
				FRotator Rotation = GetActorRotation();
				Rotation.Pitch = 45.f;

				const FVector KnockbackDirection = Rotation.Vector();
				const FVector KnockbackForce = KnockbackDirection * DamageEffectParams.KnockbackForceMagnitude;
				DamageEffectParams.KnockbackForce = KnockbackForce;
			}

			// 타겟 ASC 설정 및 데미지 효과 적용 처리
			DamageEffectParams.TargetAbilitySystemComponent = TargetASC; 
			UDRAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);
		}

		// 투사체 파괴 처리
		Destroy();
	}
	else bHit = true; // 클라이언트 권한 시 플래그 설정 처리
} 
