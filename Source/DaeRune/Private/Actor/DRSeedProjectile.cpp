// Copyright DaeRune


#include "Actor/DRSeedProjectile.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Character/DRCharacter.h"
#include "Character/DREnemy.h"
#include "DrawDebugHelpers.h"
#include "Actor/DRCleanserSite.h"
#include "Engine/OverlapResult.h"
#include "Sound/DRSoundManager.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/DRSoundDataAsset.h"
#include "DRAssetManager.h"
#include "Tutorial/DRTutorialManager.h"
#include "DRGameplayTags.h"

ADRSeedProjectile::ADRSeedProjectile()
{
    // ������ ������ ���� �߷� Ȱ��ȭ
    ProjectileMovement->ProjectileGravityScale = 1.0f;
    ProjectileMovement->InitialSpeed = 700.f;
    ProjectileMovement->MaxSpeed = 700.f;

    // ���� ��� ��Ȱ��ȭ (���� ������ ����)
    ProjectileMovement->bIsHomingProjectile = false;
}

void ADRSeedProjectile::BeginPlay()
{
    Super::BeginPlay();

    // �õ� �߻�ü�� �̵� ����ȭ ��Ȱ��ȭ (���������� ó��)
    SetReplicateMovement(false);
}

void ADRSeedProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // �ߺ� ���� ����
    if (bHasExploded) return;

    // �ڱ� �ڽ��̰ų� �����ڸ� ����
    if (!OtherActor || OtherActor == GetOwner()) return;

    // �߻��� ĳ���ʹ� ��� (���� ����)
    if (DamageEffectParams.SourceAbilitySystemComponent)
    {
        AActor* SourceActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
        if (SourceActor == OtherActor) return;
    }

    if (OtherComp)
    {
        // �浹 ���� ��� (��Ȯ�� ���� ��ġ ����)
        FVector ImpactPoint = SweepResult.Location;
        if (ImpactPoint.IsZero())
        {
            ImpactPoint = GetActorLocation();
        }

        ExplodeAtLocation(ImpactPoint);
        return;
    }
}

void ADRSeedProjectile::ExplodeAtLocation(const FVector& ImpactLocation)
{
    // �ߺ� ���� ����, ���������� ���� ������/�� ó��
    if (!HasAuthority() || bHasExploded) return;
    bHasExploded = true;

    // ���� ����Ʈ ��� (���� OnHit �Լ� Ȱ��)
    OnHit();

    MulticastPlayExplosionSound(ImpactLocation);

    // ��� Ŭ���̾�Ʈ���� ���� ��ġ ���� (����� ��ο��)
    MulticastExplodeAtLocation(ImpactLocation);

    // ���� �� ��� ���� �˻�
    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    // �߻��� ĳ���ʹ� ����
    if (DamageEffectParams.SourceAbilitySystemComponent)
    {
        AActor* SourceActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
        if (SourceActor)
        {
            QueryParams.AddIgnoredActor(SourceActor);
        }
    }

    // ��ü ������ üũ
    bool bOverlapSuccess = GetWorld()->OverlapMultiByChannel(
        OverlapResults,
        ImpactLocation,
        FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(OuterRadius),
        QueryParams
    );

    if (!bOverlapSuccess)
    {
        Destroy();
        return;
    }

    // �� ���Ϳ� ���� ó��
    int32 AffectedCount = 0;
    int32 BlockedCount = 0;
    int32 EnemyHitCount = 0;
    TSet<AActor*> ProcessedTargets;

    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* Target = Result.GetActor();
        if (!Target) continue;
        if (ProcessedTargets.Contains(Target)) continue;
        ProcessedTargets.Add(Target);

        // ���� ĳ���ʹ� ����
        if (Target->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Target))
        {
            continue;
        }

        // Ŭ���� ����Ʈ�� ����
        if (ADRCleanserSite* CleanserSite = Cast<ADRCleanserSite>(Target))
        {
            continue;
        }

        // �Ÿ� ���
        float Distance = FVector::Dist(ImpactLocation, Target->GetActorLocation());

        // ���� ���̸� ����
        if (Distance > OuterRadius) continue;

        bool bCanApply = HasLineOfSight(ImpactLocation, Target->GetActorLocation(), Target);

        if (bCanApply)
        {
            ApplyEffectToActor(Target, Distance);
            AffectedCount++;

            if (Cast<ADREnemy>(Target))
            {
                EnemyHitCount++;
            }
        }
        else
        {
            BlockedCount++;
        }
    }

    // 튜토리얼 매니저에 적중 수 보고 (튜토리얼 맵에서만 동작, SeedCannon 어빌리티만 인정)
    if (EnemyHitCount > 0 &&
        DamageEffectParams.SourceAbilityTags.HasTag(FDRGameplayTags::Get().Abilities_GardenRobot_SeedCannon))
    {
        if (ADRTutorialManager* TM = Cast<ADRTutorialManager>(
            UGameplayStatics::GetActorOfClass(GetWorld(), ADRTutorialManager::StaticClass())))
        {
            TM->ReportSeedCannonHits(EnemyHitCount);
        }
    }

    // �߻�ü ����
    Destroy();
}

bool ADRSeedProjectile::HasLineOfSight(const FVector& StartLocation, const FVector& EndLocation, const AActor* TargetActor) const
{
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);  // �߻�ü �ڽ��� ����
    QueryParams.AddIgnoredActor(TargetActor);  // Ÿ�� ���͵� ����

    // �߻��� ĳ���͵� ����
    if (DamageEffectParams.SourceAbilitySystemComponent)
    {
        if (AActor* SourceActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor())
        {
            QueryParams.AddIgnoredActor(SourceActor);
        }
    }

    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = false;

    // �������� ������ ���� (���� �߽ɰ� Ÿ�� �߽�)
    FVector AdjustedStart = StartLocation + FVector(0, 0, 50.f);  // ���� �߽� ����
    FVector AdjustedEnd = EndLocation + FVector(0, 0, 50.f);  // Ÿ�� �߽� ����

    // �� üũ�� ���� Ʈ���̽� (Visibility ä�� ���)
    bool bHitResult = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        AdjustedStart,
        AdjustedEnd,
        ECC_Visibility,  // ���� ���� Static ������Ʈ ����
        QueryParams
    );

    // ���� ������ �ʾ����� true (�þ� Ȯ��), �������� false
    return !bHitResult;
}

void ADRSeedProjectile::ApplyEffectToActor(AActor* Target, float Distance)
{
    if (!Target) return;

    // AbilitySystemComponent ��������
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    if (!TargetASC) return;

    // �߻��� ĳ���� ����
    AActor* SourceActor = nullptr;
    if (DamageEffectParams.SourceAbilitySystemComponent)
    {
        SourceActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
    }

    // �Ʊ�/���� �Ǻ�
    bool bIsAlly = !UDRAbilitySystemLibrary::IsNotFriend(SourceActor, Target);

    if (bIsAlly)
    {
        // �Ʊ�: �� ����
        float HealAmount = 0.f;
        if (Distance <= InnerRadius)
        {
            HealAmount = InnerHeal;
        }
        else if (Distance <= OuterRadius)
        {
            HealAmount = OuterHeal;
        }

        if (HealAmount > 0.f)
        {
            ApplyHealToAlly(Target, HealAmount);
        }
    }
    else
    {
        // ����: ������ ����
        float DamageAmount = 0.f;
        if (Distance <= InnerRadius)
        {
            DamageAmount = InnerDamage;
        }
        else if (Distance <= OuterRadius)
        {
            DamageAmount = OuterDamage;
        }

        if (DamageAmount > 0.f)
        {
            // ���� DamageEffectParams ���� �� ������ ���� ����
            FDamageEffectParams LocalDamageParams = DamageEffectParams;
            LocalDamageParams.BaseDamage = DamageAmount;
            LocalDamageParams.TargetAbilitySystemComponent = TargetASC;

            // �˹��� ���� �߽ɿ��� �ٱ�����
            FVector KnockbackDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
            KnockbackDirection.Z = 0.3f; // ��¦ ����
            LocalDamageParams.KnockbackForce = KnockbackDirection * LocalDamageParams.KnockbackForceMagnitude;

            // ������ ����
            UDRAbilitySystemLibrary::ApplyDamageEffect(LocalDamageParams);
        }
    }
}

void ADRSeedProjectile::ApplyHealToAlly(AActor* AllyActor, float HealAmount)
{
    if (!AllyActor || HealAmount <= 0.f) return;

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AllyActor);
    if (!TargetASC) return;

    // �� ����Ʈ�� �����Ǿ� ������ ���
    if (HealEffectClass)
    {
        FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
        ContextHandle.AddSourceObject(this);

        FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
            HealEffectClass,
            1.0f,
            ContextHandle
        );

        // ���� ���� �������� ó�� ����
        UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
            SpecHandle,
            FGameplayTag::RequestGameplayTag("Heal"),
            HealAmount
        );

        TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }
}

void ADRSeedProjectile::MulticastPlayExplosionSound_Implementation(const FVector& Location)
{
    // Actor의 World를 직접 사용해서 사운드 재생 (클라이언트에서 확실히 동작)
    if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
    {
        if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
        {
            if (SoundData->SeedExplosionSound)
            {
                UGameplayStatics::PlaySoundAtLocation(this, SoundData->SeedExplosionSound, Location);
            }
        }
    }
}

void ADRSeedProjectile::MulticastExplodeAtLocation_Implementation(const FVector& ImpactLocation)
{
    if (ExplosionEffect)
    {
        UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,                           
            ExplosionEffect,                
            ImpactLocation,                 
            FRotator::ZeroRotator,          
            FVector(1.f, 1.f, 1.f),
            true,                           
            true,                           
            ENCPoolMethod::None,           
            true                            
        );
    }

//    #if !UE_BUILD_SHIPPING
//    // ���� ���� ǥ��
//    DrawDebugSphere(GetWorld(), ImpactLocation, InnerRadius, 16, FColor::Yellow, false, 1.0f, 0, 3.0f);
//    DrawDebugSphere(GetWorld(), ImpactLocation, OuterRadius, 24, FColor::Orange, false, 1.0f, 0, 2.0f);
//
//    // Ŭ���̾�Ʈ�� �ڱⰡ ���� ���!
//    TArray<FOverlapResult> OverlapResults;
//    FCollisionQueryParams QueryParams;
//    QueryParams.AddIgnoredActor(this);
//
//    if (DamageEffectParams.SourceAbilitySystemComponent)
//    {
//        if (AActor* SourceActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor())
//        {
//            QueryParams.AddIgnoredActor(SourceActor);
//        }
//    }
//
//    GetWorld()->OverlapMultiByChannel(
//        OverlapResults,
//        ImpactLocation,
//        FQuat::Identity,
//        ECC_Pawn,
//        FCollisionShape::MakeSphere(OuterRadius),
//        QueryParams
//    );
//
//    // �� Ÿ�ٿ� ���� ���� �׸���
//    for (const FOverlapResult& Result : OverlapResults)
//    {
//        AActor* Target = Result.GetActor();
//        if (!Target) continue;
//
//        if (Target->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Target))
//        {
//            continue;
//        }
//
//        float Distance = FVector::Dist(ImpactLocation, Target->GetActorLocation());
//        if (Distance > OuterRadius) continue;
//
//        // Ŭ���̾�Ʈ�� ���� LOS üũ
//        FVector AdjustedStart = ImpactLocation + FVector(0, 0, 50.f);
//        FVector AdjustedEnd = Target->GetActorLocation() + FVector(0, 0, 50.f);
//
//        FHitResult HitResult;
//        FCollisionQueryParams LOSParams;
//        LOSParams.AddIgnoredActor(this);
//        LOSParams.AddIgnoredActor(Target);
//        if (DamageEffectParams.SourceAbilitySystemComponent)
//        {
//            if (AActor* SourceActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor())
//            {
//                LOSParams.AddIgnoredActor(SourceActor);
//            }
//        }
//
//        bool bBlocked = GetWorld()->LineTraceSingleByChannel(
//            HitResult,
//            AdjustedStart,
//            AdjustedEnd,
//            ECC_Visibility,
//            LOSParams
//        );
//
//        // ���� �׸���
//        FColor LineColor = bBlocked ? FColor::Red : FColor::Green;
//        DrawDebugLine(GetWorld(), AdjustedStart, AdjustedEnd, LineColor, false, 1.0f, 0, 1.0f);
//
//        if (bBlocked)
//        {
//            DrawDebugBox(GetWorld(), HitResult.ImpactPoint, FVector(15.f), FColor::Red, false, 1.0f, 0, 2.0f);
//        }
//    }
//#endif
}

