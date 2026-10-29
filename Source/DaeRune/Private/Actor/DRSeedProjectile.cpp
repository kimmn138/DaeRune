// Copyright DaeRune


#include "Actor/DRSeedProjectile.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Character/DRCharacter.h"
#include "Character/DREnemy.h"
#include "Actor/DRCleanserSite.h"
#include "Engine/OverlapResult.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/DRSoundDataAsset.h"
#include "DRAssetManager.h"
#include "Tutorial/DRTutorialManager.h"
#include "Game/DRTutorialGameMode.h"
#include "DRGameplayTags.h"
#include "Game/DRProgressionConfig.h"
#include "DaeRune/DRLogChannels.h"

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
    // 중복 폭발 방지, 서버에서만 범위 데미지/힐 처리
    if (!HasAuthority() || bHasExploded) return;
    bHasExploded = true;

    // 폭발 이펙트 재생 (기존 OnHit 함수 활용)
    OnHit();

    MulticastPlayExplosionSound(ImpactLocation);

    // 모든 클라이언트에게 폭발 위치 전달 (디버그 드로우용)
    // ★Plan5(씨앗칩)★ 범위 칩이 붙으면 VFX 도 같이 커져야 한다.
    // Resolved 는 서버에만 있으므로 "기본 대비 몇 배인가"를 인자로 넘긴다.
    // 칩이 없으면 정확히 1.0 이라 기존과 동일하다.
    const float ExplosionRadiusScale = (OuterRadius > KINDA_SMALL_NUMBER)
        ? Resolved.OuterRadius / OuterRadius
        : 1.f;

    MulticastExplodeAtLocation(ImpactLocation, ExplosionRadiusScale);

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
        FCollisionShape::MakeSphere(Resolved.OuterRadius),
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
    int32 EnemyHitCount = 0;
    TSet<AActor*> ProcessedTargets;

    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* Target = Result.GetActor();
        if (!Target) continue;
        if (ProcessedTargets.Contains(Target)) continue;
        ProcessedTargets.Add(Target);

        // 죽은 캐릭터는 무시
        if (Target->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Target))
        {
            continue;
        }

        // 클렌저 사이트는 무시
        if (ADRCleanserSite* CleanserSite = Cast<ADRCleanserSite>(Target))
        {
            continue;
        }

        // 거리 계산
        float Distance = FVector::Dist(ImpactLocation, Target->GetActorLocation());

        // 범위 밖이면 무시
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
    // (튜토리얼 게임모드일 때만 찾는다 — 예전에는 모든 맵에서 폭발마다 전 액터를 순회했다)
    if (EnemyHitCount > 0 &&
        DamageEffectParams.SourceAbilityTags.HasTag(FDRGameplayTags::Get().Abilities_GardenRobot_SeedCannon) &&
        GetWorld()->GetAuthGameMode<ADRTutorialGameMode>())
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

    // 벽 체크 — 지형 · 벽(WorldStatic)만 본다. 다른 적 · 아군 몸은 폭발을 가리지 않는다
    const bool bHitResult = UDRAbilitySystemLibrary::LineTraceWorldGeometry(
        GetWorld(), HitResult, AdjustedStart, AdjustedEnd, QueryParams);

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
    UDRSoundDataAsset::PlayAtLocation(this, &UDRSoundDataAsset::SeedExplosionSound, Location);
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
}

