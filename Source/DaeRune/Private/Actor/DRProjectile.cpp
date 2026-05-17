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
	// ��Ƽ�÷��� ����ȭ Ȱ��ȭ
	bReplicates = true;

	// �浹 ������ ��ü ������Ʈ ����
	Sphere = CreateDefaultSubobject<USphereComponent>("Sphere");
	SetRootComponent(Sphere);
	Sphere->SetCollisionObjectType(ECC_Projectile);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Overlap);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	// �߻�ü ���� �̵� ����
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>("ProjectileMovement");
	ProjectileMovement->InitialSpeed = 550.f;
	ProjectileMovement->MaxSpeed = 550.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void ADRProjectile::BeginPlay()
{
	Super::BeginPlay();
	
	// �ڵ� �Ҹ� Ÿ�̸� ����
	SetLifeSpan(LifeSpan);
	// �̵� ����ȭ Ȱ��ȭ
	SetReplicateMovement(true);
	// �浹 �̺�Ʈ ���ε�
	Sphere->OnComponentBeginOverlap.AddDynamic(this, &ADRProjectile::OnSphereOverlap);

	// ���� �� �ݺ� ���� ���
	LoopingSoundComponent = UGameplayStatics::SpawnSoundAttached(LoopingSound, GetRootComponent());
}

void ADRProjectile::OnHit()
{
	// �浹 �� ����Ʈ �� ���� ���
	if (UWorld* World = GetWorld())
	{
		UGameplayStatics::PlaySoundAtLocation(World, ImpactSound, GetActorLocation(), FRotator::ZeroRotator);
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(World, ImpactEffect, GetActorLocation());
	}
	// �ݺ� ���� ����
	if (LoopingSoundComponent)
	{
		LoopingSoundComponent->Stop();
		LoopingSoundComponent->DestroyComponent();
	}
	bHit = true;
}

void ADRProjectile::Destroyed()
{
	// ���� �ı� �� ���� ���ҽ� ����
	if (LoopingSoundComponent)
	{
		LoopingSoundComponent->Stop();
		LoopingSoundComponent->DestroyComponent();
	}
	// Ŭ���̾�Ʈ���� �ı��� �� ����Ʈ ��� (������ OnHit���� ó����)
	if (!bHit && !HasAuthority()) OnHit();
	Super::Destroyed();
}

void ADRProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// ������ �ҽ� ����
	if (DamageEffectParams.SourceAbilitySystemComponent == nullptr) return;
	AActor* SourceAvatarActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
	if (!IsValid(SourceAvatarActor) || !IsValid(OtherActor)) return;
	// �ڱ� �ڽ� ����
	if (SourceAvatarActor == OtherActor) return;
	// �Ʊ� ����
	if (!UDRAbilitySystemLibrary::IsNotFriend(SourceAvatarActor, OtherActor)) return;
	// ����Ʈ ���
	if (!bHit) OnHit();

	// ���������� ������ ó��
	if (HasAuthority())
	{
		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
		{
			// ��� �� �˹� ���� ����
			const FVector DeathImpulse = GetActorForwardVector() * DamageEffectParams.DeathImpulseMagnitude; 
			DamageEffectParams.DeathImpulse = DeathImpulse;
			// �˹� Ȯ�� ��� �� ����
			const bool bKnockback = FMath::RandRange(1, 100) < DamageEffectParams.KnockbackChance;
			if (bKnockback)
			{
				FRotator Rotation = GetActorRotation();
				// �������� �˹�
				Rotation.Pitch = 45.f;

				const FVector KnockbackDirection = Rotation.Vector();
				const FVector KnockbackForce = KnockbackDirection * DamageEffectParams.KnockbackForceMagnitude;
				DamageEffectParams.KnockbackForce = KnockbackForce;
			}

			// GAS�� ���� ������ ����
			DamageEffectParams.TargetAbilitySystemComponent = TargetASC; 
			UDRAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);
		}

		// �������� �߻�ü �ı�
		Destroy();
	}
	// Ŭ���̾�Ʈ������ Ÿ�� �÷��׸� ����
	else bHit = true;
}
