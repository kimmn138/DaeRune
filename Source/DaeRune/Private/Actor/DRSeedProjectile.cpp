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
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/DRSoundDataAsset.h"
#include "DRAssetManager.h"
#include "Tutorial/DRTutorialManager.h"
#include "DRGameplayTags.h"
#include "Game/DRProgressionConfig.h"
#include "DaeRune/DRLogChannels.h"

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

    // ★Plan5(씨앗칩)★ 업그레이드 칩 해석은 서버에서만 필요하다 —
    // 폭발 판정/피해/회복/환급이 전부 서버 권한이다.
    // DamageEffectParams 는 FinishSpawning() ★전★ 에 대입되므로 여기서 이미 유효하다.
    if (HasAuthority())
    {
        ResolveUpgradedParams();
    }
}

void ADRSeedProjectile::ResolveUpgradedParams()
{
    const UAbilitySystemComponent* SourceASC = DamageEffectParams.SourceAbilitySystemComponent;
    const FGameplayTagContainer& AbilityTags = DamageEffectParams.SourceAbilityTags;

    auto Upgraded = [SourceASC, &AbilityTags](EDRUpgradeStat Stat, float Base)
    {
        return UDRAbilitySystemLibrary::ApplySkillUpgrade(SourceASC, AbilityTags, Stat, Base);
    };

    // 범위는 1uu 하한 (0 이면 OverlapMulti 가 아무도 못 잡는다)
    Resolved.InnerRadius = FMath::Max(1.f, Upgraded(EDRUpgradeStat::SkillAreaRadius, InnerRadius));
    Resolved.OuterRadius = FMath::Max(Resolved.InnerRadius,
                                      Upgraded(EDRUpgradeStat::SkillAreaRadius, OuterRadius));

    // 피해는 0 하한. 칩 배율은 곱연산이라 뒤집힐 수 없지만,
    // 칩 하나가 -100% 미만으로 작성된 경우에 대비한 이중 안전장치다. (Plan5(씨앗칩))
    Resolved.InnerDamage = FMath::Max(0.f, Upgraded(EDRUpgradeStat::SkillDamage, InnerDamage));
    Resolved.OuterDamage = FMath::Max(0.f, Upgraded(EDRUpgradeStat::SkillDamage, OuterDamage));

    Resolved.InnerHeal = FMath::Max(0.f, Upgraded(EDRUpgradeStat::SkillHealing, InnerHeal));
    Resolved.OuterHeal = FMath::Max(0.f, Upgraded(EDRUpgradeStat::SkillHealing, OuterHeal));

    // 이름을 Tags 로 두면 AActor::Tags 를 가려 C4458(=에러)가 난다.
    const FDRGameplayTags& DRTags = FDRGameplayTags::Get();
    Resolved.bRefundOnDirectHit = UDRAbilitySystemLibrary::HasUpgradeFeature(
        SourceASC, DRTags.Upgrade_Feature_Skill_SeedBomb_Refund);
    Resolved.bSlowEnemies = UDRAbilitySystemLibrary::HasUpgradeFeature(
        SourceASC, DRTags.Upgrade_Feature_Skill_SeedBomb_Slow);
    Resolved.bHasteAllies = UDRAbilitySystemLibrary::HasUpgradeFeature(
        SourceASC, DRTags.Upgrade_Feature_Skill_SeedBomb_Haste);
}

void ADRSeedProjectile::GrantWaterRefund()
{
    UAbilitySystemComponent* SourceASC = DamageEffectParams.SourceAbilitySystemComponent;
    if (!SourceASC || SpentWaterCost <= 0.f) return;

    const FDRSeedBombChipTuning& Tuning =
        UDRAbilitySystemLibrary::GetChipTuning(SourceASC->GetAvatarActor()).SeedBomb;

    const float RefundAmount = SpentWaterCost * FMath::Max(0.f, Tuning.DirectHitRefundRatio);
    if (RefundAmount <= 0.f) return;

    UClass* RefundClass = Tuning.RefundEffect.LoadSynchronous();
    if (!RefundClass)
    {
        UE_LOG(LogDR, Warning,
            TEXT("[Upgrade] ChipTuning.SeedBomb.RefundEffect 미지정 — 직격 환급이 적용되지 않습니다."));
        return;
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(this);

    FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(RefundClass, 1.f, Context);
    if (!SpecHandle.IsValid()) return;

    // 적 처치 물 지급과 같은 SetByCaller 를 쓴다 (GE_WaterGrant 재사용 가능)
    SpecHandle.Data->SetSetByCallerMagnitude(
        FDRGameplayTags::Get().Water_SetByCaller_Grant, RefundAmount);

    SourceASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
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

        // ★Plan5(씨앗칩)★ "정확히 직접 적에게 명중" 판정.
        // IsNotFriend() 를 쓰지 않는 이유: 그 함수는 Player/Enemy 액터 태그가 ★둘 다 없는★
        // 클렌저 사이트·중립 액터에도 true 를 돌려준다. 모든 적은 ADREnemy 파생이다. (D-3)
        // ★ExplodeAtLocation 호출 전에★ 세워야 한다 — 그 안에서 환급을 처리한다.
        if (ADREnemy* HitEnemy = Cast<ADREnemy>(OtherActor))
        {
            bDirectHitOnEnemy = !ICombatInterface::Execute_IsDead(HitEnemy);
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
    // ★Plan5(씨앗칩)★ 범위 칩이 붙으면 VFX 도 같이 커져야 한다.
    // Resolved 는 서버에만 있으므로 "기본 대비 몇 배인가"를 인자로 넘긴다.
    // 칩이 없으면 정확히 1.0 이라 기존과 동일하다.
    const float ExplosionRadiusScale = (OuterRadius > KINDA_SMALL_NUMBER)
        ? Resolved.OuterRadius / OuterRadius
        : 1.f;

    MulticastExplodeAtLocation(ImpactLocation, ExplosionRadiusScale);

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
        FCollisionShape::MakeSphere(Resolved.OuterRadius),
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
        if (Distance > Resolved.OuterRadius) continue;

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

    // ★Plan5(씨앗칩)★ 직격 환급.
    // 폭발 처리(피해/회복)가 전부 끝난 뒤에 준다 — 로그 순서가 "맞음 → 환급" 이어야 디버깅이 쉽다.
    if (bDirectHitOnEnemy && Resolved.bRefundOnDirectHit)
    {
        GrantWaterRefund();
    }

    // 발사체 제거
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
        // 아군: 힐 적용
        float HealAmount = 0.f;
        if (Distance <= Resolved.InnerRadius)
        {
            HealAmount = Resolved.InnerHeal;
        }
        else if (Distance <= Resolved.OuterRadius)
        {
            HealAmount = Resolved.OuterHeal;
        }

        if (HealAmount > 0.f)
        {
            ApplyHealToAlly(Target, HealAmount);

            // ★Plan5(씨앗칩)★ 회복을 실제로 받은 아군에게만 가속을 건다.
            if (Resolved.bHasteAllies)
            {
                const FDRTimedSpeedEffect& Cfg =
                    UDRAbilitySystemLibrary::GetChipTuning(this).SeedBomb.AllyHaste;

                UDRAbilitySystemLibrary::ApplyTimedSpeedEffect(
                    Target, DamageEffectParams.SourceAbilitySystemComponent,
                    Cfg, 1.f + Cfg.Magnitude);
            }
        }
    }
    else
    {
        // 적: 데미지 적용
        float DamageAmount = 0.f;
        if (Distance <= Resolved.InnerRadius)
        {
            DamageAmount = Resolved.InnerDamage;
        }
        else if (Distance <= Resolved.OuterRadius)
        {
            DamageAmount = Resolved.OuterDamage;
        }

        if (DamageAmount > 0.f)
        {
            FDamageEffectParams LocalDamageParams = DamageEffectParams;

            // ★Plan5(씨앗칩)★ 폭발 피해는 발사체가 들고 있는 상수라
            // MakeDamageEffectParamsFromClassDefaults 가 넣어 둔 BaseDamage 를 덮어쓴다.
            // 그대로 덮어쓰면 SkillDamage 칩이 통째로 사라지므로
            // Resolved 에 이미 칩이 반영된 값을 넣는다.
            LocalDamageParams.BaseDamage = DamageAmount;
            LocalDamageParams.TargetAbilitySystemComponent = TargetASC;

            // 넣백은 폭발 중심에서 바깥쪽으로
            FVector KnockbackDirection = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();
            KnockbackDirection.Z = 0.3f; // 살짝 위로
            LocalDamageParams.KnockbackForce = KnockbackDirection * LocalDamageParams.KnockbackForceMagnitude;

            UDRAbilitySystemLibrary::ApplyDamageEffect(LocalDamageParams);

            // ★Plan5(씨앗칩)★ 피해를 실제로 입은 적에게만 감속을 건다.
            // 칩 배율은 곱연산이라 피해 감소 칩을 겹쳐도 DamageAmount 가
            // 0 이 되지 않는다 — 그래서 이 분기 안에 두어도 감속이 사라지지 않는다.
            if (Resolved.bSlowEnemies)
            {
                const FDRTimedSpeedEffect& Cfg =
                    UDRAbilitySystemLibrary::GetChipTuning(this).SeedBomb.EnemySlow;

                UDRAbilitySystemLibrary::ApplyTimedSpeedEffect(
                    Target, DamageEffectParams.SourceAbilitySystemComponent,
                    Cfg, FMath::Max(0.05f, 1.f - Cfg.Magnitude));
            }
        }
    }
}

void ADRSeedProjectile::ApplyHealToAlly(AActor* AllyActor, float HealAmount)
{
    // ★Plan5(발톱칩)★ 공용 헬퍼로 통합했다. 동작은 이전과 완전히 같다 —
    // SourceASC 를 nullptr 로 넘기면 대상 ASC 로 스펙을 만들고(자가 회복 형태),
    // SourceObject 로 이 발사체를 넘겨 이펙트 컨텍스트의 소스도 그대로 유지된다.
    UDRAbilitySystemLibrary::ApplyHealToActor(
        AllyActor, /*SourceASC=*/nullptr, HealEffectClass, HealAmount, /*SourceObject=*/this);
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

void ADRSeedProjectile::MulticastExplodeAtLocation_Implementation(const FVector& ImpactLocation, float RadiusScale)
{
    // ★Plan5(씨앗칩)★ 이 함수는 모든 머신에서 돌아간다.
    // Resolved 는 서버에서만 채워지므로 ★여기서 Resolved 를 사용하면 안 된다★
    // (아래 주석 처리된 디버그 드로우를 되살릴 때 특히 주의 — 범위 칩이 반영되지 않는다).
    // 0 이하 스케일은 나이아가라가 사라지거나 뒤집힌다 — 방어적으로 하한을 둔다.
    const float SafeRadiusScale = FMath::Max(0.01f, RadiusScale);

    if (ExplosionEffect)
    {
        const FVector SpawnScale = bScaleExplosionVFX
            ? FVector(SafeRadiusScale)
            : FVector(1.f, 1.f, 1.f);

        UNiagaraComponent* ExplosionComp = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
            this,
            ExplosionEffect,
            ImpactLocation,
            FRotator::ZeroRotator,
            SpawnScale,
            true,
            true,
            ENCPoolMethod::None,
            true
        );

        // 나이아가라 시스템이 float User 파라미터 `RadiusScale` 을 선언해 두면
        // 아티스트가 반경·입자 수를 직접 제어할 수 있다.
        // 선언하지 않았으면 그저 쓰이지 않는 오버라이드라 경고 없이 무시된다.
        if (ExplosionComp)
        {
            // 세터는 UNiagaraComponent 의 멤버다(함수 라이브러리에 없다).
            // FString 버전(SetNiagaraVariableFloat)은 5.3 에서 deprecated 돼 FName 버전을 쓴다.
            // 사용자 파라미터 스토어가 "User." 접두사를 알아서 붙여준다.
            static const FName RadiusScaleParamName(TEXT("RadiusScale"));
            ExplosionComp->SetVariableFloat(RadiusScaleParamName, SafeRadiusScale);
        }
    }

//    #if !UE_BUILD_SHIPPING
//    // ���� ���� ǥ��
//    // ★Plan5(씨앗칩)★ Resolved 는 서버 전용이라 여기서는 쓸 수 없다.
//    // 대신 서버가 넘겨준 SafeRadiusScale 을 기본 반경에 곱해 그린다.
//    DrawDebugSphere(GetWorld(), ImpactLocation, InnerRadius * SafeRadiusScale, 16, FColor::Yellow, false, 1.0f, 0, 3.0f);
//    DrawDebugSphere(GetWorld(), ImpactLocation, OuterRadius * SafeRadiusScale, 24, FColor::Orange, false, 1.0f, 0, 2.0f);
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

