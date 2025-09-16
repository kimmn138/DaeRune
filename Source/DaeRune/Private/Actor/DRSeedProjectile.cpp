// Copyright DaeRune


#include "Actor/DRSeedProjectile.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "DaeRune/DaeRune.h"
#include "Character/DRCharacter.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"

ADRSeedProjectile::ADRSeedProjectile()
{
    // 포물선 궤적을 위한 중력 활성화
    ProjectileMovement->ProjectileGravityScale = 1.0f;
    ProjectileMovement->InitialSpeed = 700.f;
    ProjectileMovement->MaxSpeed = 700.f;

    // 유도 기능 비활성화 (직선 포물선 궤적)
    ProjectileMovement->bIsHomingProjectile = false;
}

void ADRSeedProjectile::BeginPlay()
{
    Super::BeginPlay();

    // 시드 발사체는 이동 동기화 비활성화 (서버에서만 처리)
    SetReplicateMovement(false);
}

void ADRSeedProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
    bool bFromSweep, const FHitResult& SweepResult)
{
    // 중복 폭발 방지
    if (bHasExploded) return;

    // 자기 자신이거나 소유자면 무시
    if (!OtherActor || OtherActor == GetOwner()) return;

    // 발사한 캐릭터는 통과 (자폭 방지)
    if (DamageEffectParams.SourceAbilitySystemComponent)
    {
        AActor* SourceActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
        if (SourceActor == OtherActor) return;
    }

    if (OtherComp)
    {
        // 충돌 지점 계산 (정확한 폭발 위치 설정)
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
    // 중복 폭발 방지, 서버에서만 범위 데미지/힐 처리
    if (!HasAuthority() || bHasExploded) return;
    bHasExploded = true;

    // 폭발 이펙트 재생 (기존 OnHit 함수 활용)
    OnHit();

    // 디버그 시각화 (개발 빌드에서만)
#if !UE_BUILD_SHIPPING
    if (GEngine)
    {
        DrawDebugSphere(GetWorld(), ImpactLocation, InnerRadius, 16, FColor::Yellow, false, 3.0f, 0, 2.0f);
        DrawDebugSphere(GetWorld(), ImpactLocation, OuterRadius, 24, FColor::Orange, false, 3.0f, 0, 1.0f);
    }
#endif

    // 범위 내 모든 액터 검색
    TArray<FOverlapResult> OverlapResults;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);

    // 발사한 캐릭터는 제외
    if (DamageEffectParams.SourceAbilitySystemComponent)
    {
        AActor* SourceActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
        if (SourceActor)
        {
            QueryParams.AddIgnoredActor(SourceActor);
        }
    }

    // 구체 오버랩 체크
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

    // 각 액터에 대해 처리
    int32 AffectedCount = 0;
    int32 BlockedCount = 0;

    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* Target = Result.GetActor();
        if (!Target) continue;

        // 죽은 캐릭터는 무시
        if (Target->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Target))
        {
            continue;
        }

        // 거리 계산
        float Distance = FVector::Dist(ImpactLocation, Target->GetActorLocation());

        // 범위 밖이면 무시
        if (Distance > OuterRadius) continue;

        bool bCanApply = HasLineOfSight(ImpactLocation, Target->GetActorLocation(), Target);

        if (bCanApply)
        {
            ApplyEffectToActor(Target, Distance);
            AffectedCount++;
        }
        else
        {
            BlockedCount++;
        }
    }

    // 발사체 제거
    Destroy();
}

bool ADRSeedProjectile::HasLineOfSight(const FVector& StartLocation, const FVector& EndLocation, const AActor* TargetActor) const
{
    FHitResult HitResult;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(this);  // 발사체 자신은 무시
    QueryParams.AddIgnoredActor(TargetActor);  // 타겟 액터도 무시

    // 발사한 캐릭터도 무시
    if (DamageEffectParams.SourceAbilitySystemComponent)
    {
        if (AActor* SourceActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor())
        {
            QueryParams.AddIgnoredActor(SourceActor);
        }
    }

    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = false;

    // 시작점과 끝점을 조정 (폭발 중심과 타겟 중심)
    FVector AdjustedStart = StartLocation + FVector(0, 0, 50.f);  // 폭발 중심 높이
    FVector AdjustedEnd = EndLocation + FVector(0, 0, 50.f);  // 타겟 중심 높이

    // 벽 체크용 라인 트레이스 (Visibility 채널 사용)
    bool bHitResult = GetWorld()->LineTraceSingleByChannel(
        HitResult,
        AdjustedStart,
        AdjustedEnd,
        ECC_Visibility,  // 벽과 같은 Static 오브젝트 감지
        QueryParams
    );

    // 디버그 라인 그리기
#if !UE_BUILD_SHIPPING
    FColor DebugColor = bHitResult ? FColor::Red : FColor::Green;
    DrawDebugLine(GetWorld(), AdjustedStart, AdjustedEnd, DebugColor, false, 3.0f, 0, 2.0f);

    if (bHitResult)
    {
        // 충돌 지점에 X 표시
        DrawDebugBox(GetWorld(), HitResult.ImpactPoint, FVector(10.f), FColor::Red, false, 3.0f);
    }
#endif

    // 벽에 막히지 않았으면 true (시야 확보), 막혔으면 false
    return !bHitResult;
}

void ADRSeedProjectile::ApplyEffectToActor(AActor* Target, float Distance)
{
    if (!Target) return;

    // AbilitySystemComponent 가져오기
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    if (!TargetASC) return;

    // 발사한 캐릭터 정보
    AActor* SourceActor = nullptr;
    if (DamageEffectParams.SourceAbilitySystemComponent)
    {
        SourceActor = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
    }

    // 아군/적군 판별
    bool bIsAlly = !UDRAbilitySystemLibrary::IsNotFriend(SourceActor, Target);

    if (bIsAlly)
    {
        // 아군: 힐 적용
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
        // 적군: 데미지 적용
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
            // 기존 DamageEffectParams 복사 후 데미지 값만 변경
            FDamageEffectParams LocalDamageParams = DamageEffectParams;
            LocalDamageParams.BaseDamage = DamageAmount;
            LocalDamageParams.TargetAbilitySystemComponent = TargetASC;

            // 넉백은 폭발 중심에서 바깥으로
            FVector KnockbackDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
            KnockbackDirection.Z = 0.3f; // 살짝 위로
            LocalDamageParams.KnockbackForce = KnockbackDirection * LocalDamageParams.KnockbackForceMagnitude;

            // 데미지 적용
            UDRAbilitySystemLibrary::ApplyDamageEffect(LocalDamageParams);
        }
    }
}

void ADRSeedProjectile::ApplyHealToAlly(AActor* AllyActor, float HealAmount)
{
    if (!AllyActor || HealAmount <= 0.f) return;

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AllyActor);
    if (!TargetASC) return;

    // 힐 이펙트가 설정되어 있으면 사용
    if (HealEffectClass)
    {
        FGameplayEffectContextHandle ContextHandle = TargetASC->MakeEffectContext();
        ContextHandle.AddSourceObject(this);

        FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
            HealEffectClass,
            1.0f,
            ContextHandle
        );

        // 힐은 음수 데미지로 처리 가능
        UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
            SpecHandle,
            FGameplayTag::RequestGameplayTag("Heal"),
            HealAmount
        );

        TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
    }
}

