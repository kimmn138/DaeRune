// Copyright DaeRune


#include "AbilitySystem/Abilities/DRWaterPump.h"
#include "DaeRune/DRLogChannels.h"
#include "Camera/CameraComponent.h"
#include "DaeRune/DaeRune.h"
#include "DRGameplayTags.h"
#include "Character/DRCharacter.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Interaction/CombatInterface.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
// Plan5(물대포칩) — 업그레이드 조회 / 칩 튜닝 상수
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Game/DRProgressionConfig.h"

FVector UDRWaterPump::CalculateWaterBeamEndPoint(const FVector& WeaponSocketLocation, bool& bHitObstacle, FHitResult& OutHitResult)
{
    FVector AimStart, AimDirection;
    if (!GetAimDirection(AimStart, AimDirection))
    {
        bHitObstacle = false;
        return WeaponSocketLocation + FVector::ForwardVector * WeaponRange;
    }

    // ī�޶󿡼� �ִ� �����Ÿ������� ��ǥ ����
    FVector CameraTargetPoint = AimStart + (AimDirection * WeaponRange);

    // ���� ���Ͽ��� ��ǥ �������� LineTrace
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);
    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = false;

    // ★먼저 카메라 광선으로 **실제 조준 지점**을 찾는다 (2026-08-27).
    //
    //   이 단계가 없으면 소켓에서 "최대 사거리 지점"을 향해 직선을 쏘게 된다.
    //   무기 소켓이 지면에 가까운 캐릭터가 낮은 표적을 내려다보면 그 직선이
    //   목표에 닿기 훨씬 전에 바닥을 파고들어, 빔이 코앞에서 잘려 버린다.
    //
    //   조준 지점을 먼저 확정하면 소켓->조준점 직선이 목표를 향하므로 그 문제가 사라진다.
    //   벽 차단은 아래 소켓 트레이스가 그대로 담당하므로 관통 문제는 생기지 않는다.
    //
    //   ★Plan5(물대포칩)★ 관통 칩이 있으면 이 광선도 캐릭터를 건너뛰어야 한다.
    //   여기를 빠뜨리면 조준점이 첫 적에서 확정되어 아래 소켓 트레이스도 짧아지고,
    //   결국 관통이 "첫 적까지"만 된다.
    {
        FHitResult CameraHit;
        if (TraceBeamBlocker(AimStart, CameraTargetPoint, Resolved.bPierce, QueryParams, CameraHit))
        {
            CameraTargetPoint = CameraHit.ImpactPoint;
        }
    }

    // LineTrace ���� (관통이면 캐릭터를 건너뛴다 — 지형지물은 그대로 막는다)
    bHitObstacle = TraceBeamBlocker(
        WeaponSocketLocation, CameraTargetPoint, Resolved.bPierce, QueryParams, OutHitResult);

    // ★시작점이 이미 막힌 형상 안이면(BlockingVolume 등) 거리 0 으로 히트가 잡힌다.
    //   그대로 두면 빔 길이가 0 이 되어 판정 박스가 소켓 자리에 뭉개지고
    //   **아무 대상도 맞히지 못한다.** 이 경우는 히트가 아닌 것으로 취급한다. (2026-08-27)
    if (bHitObstacle && (OutHitResult.bStartPenetrating || OutHitResult.Distance < 1.f))
    {
        UE_LOG(LogDR, Warning,
            TEXT("[WaterPump] 무기 소켓이 막힌 형상(%s) 안에 있어 히트를 무시하고 최대 사거리를 사용한다."),
            *GetNameSafe(OutHitResult.GetActor()));

        bHitObstacle = false;
        OutHitResult = FHitResult();
    }

    FVector BeamEndPoint;
    if (bHitObstacle)
    {
        // �߰��� ��ֹ� ������ �� ����������
        BeamEndPoint = OutHitResult.ImpactPoint;
    }
    else
    {
        // ��ֹ� ������ �ִ� �Ÿ�����
        BeamEndPoint = CameraTargetPoint;
    }

    // ����� �ð�ȭ
#if ENABLE_DRAW_DEBUG
    if (bShowDebugVisualization)
    {
        const float DebugDuration = 0.1f;
        const FColor TraceColor = bHitObstacle ? FColor::Red : FColor::Green;

        DrawDebugLine(
            GetWorld(),
            WeaponSocketLocation,
            BeamEndPoint,
            TraceColor,
            false,
            DebugDuration,
            0,
            3.0f
        );

        if (bHitObstacle)
        {
            DrawDebugSphere(
                GetWorld(),
                OutHitResult.ImpactPoint,
                20.0f,
                12,
                FColor::Red,
                false,
                DebugDuration
            );
        }
    }
#endif

    return BeamEndPoint;
}

bool UDRWaterPump::TraceBeamBlocker(const FVector& Start, const FVector& End, bool bPierceCharacters,
    FCollisionQueryParams Params, FHitResult& OutHit) const
{
    UWorld* World = GetWorld();
    if (!World) return false;

    // 관통이 아니면 기존과 ★완전히 동일한★ 단일 트레이스다. 회귀 0.
    if (!bPierceCharacters)
    {
        return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params);
    }

    // ★Plan5(물대포칩)★ 관통 — 캐릭터형 액터를 만나면 무시 목록에 넣고 다시 쏜다.
    //
    // LineTraceMulti 로는 안 된다: Multi 도 ★첫 Block 에서 멈춘다★.
    // 적 커스텀 히트박스가 ECC_Visibility 를 Block 으로 잡고 있어서(DREnemy::SetupHitboxComponents)
    // Multi 를 써도 첫 적까지만 돌려준다.
    //
    // 판정 기준이 ICombatInterface 인 이유 (D-13):
    //   · Cast<ADREnemy>       — 홀로그램 두더지처럼 캐릭터가 아닌 유효 대상을 놓친다
    //   · IsValidDamageTarget  — 죽은 적을 false 로 돌려줘서 ★시체가 빔을 막는다★
    for (int32 Step = 0; Step <= MaxPierceTargets; ++Step)
    {
        if (!World->LineTraceSingleByChannel(OutHit, Start, End, ECC_Visibility, Params))
        {
            return false;                       // 끝까지 아무것도 안 막았다 = 최대 사거리
        }

        AActor* Blocker = OutHit.GetActor();
        if (!Blocker || !Blocker->Implements<UCombatInterface>())
        {
            return true;                        // ★지형지물★ — 여기서 끊긴다
        }

        Params.AddIgnoredActor(Blocker);        // 캐릭터는 통과시키고 다시
    }

    return true;   // 상한 도달: 마지막 히트를 그대로 쓴다 (무한 루프 방지)
}

AActor* UDRWaterPump::FindClosestTargetInBeam(const FVector& WeaponSocketLocation, const FVector& BeamEndPoint)
{
    // ★Plan5(물대포칩)★ 본체는 CollectTargetsInBeam 으로 옮겼다.
    // 이 함수는 BlueprintCallable 시그니처를 유지하기 위한 얇은 래퍼다.
    TArray<AActor*> Targets;
    CollectTargetsInBeam(WeaponSocketLocation, BeamEndPoint, Targets);
    return Targets.Num() > 0 ? Targets[0] : nullptr;
}

void UDRWaterPump::CollectTargetsInBeam(const FVector& WeaponSocketLocation, const FVector& BeamEndPoint,
    TArray<AActor*>& OutTargets) const
{
    OutTargets.Reset();

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter) return;

    // ������ ����� ���� ���
    FVector BeamDirection = (BeamEndPoint - WeaponSocketLocation).GetSafeNormal();
    float BeamLength = FVector::Distance(WeaponSocketLocation, BeamEndPoint);

    // 빔이 완전히 뭉개진 경우(소켓이 형상 안 등) 방향을 조준 방향으로 되살린다.
    if (BeamDirection.IsNearlyZero())
    {
        FVector AimStart, AimDirection;
        BeamDirection = GetAimDirection(AimStart, AimDirection)
            ? AimDirection
            : OwnerCharacter->GetActorForwardVector();
    }

    // 판정 박스는 시각 빔과 동일한 길이를 쓴다. 벽 너머를 때리면 안 되기 때문이다.
    FVector BoxCenter = WeaponSocketLocation + (BeamDirection * BeamLength * 0.5f);
    FVector BoxHalfExtent(BeamLength * 0.5f, BeamWidth * 0.5f, BeamHeight * 0.5f);

    // ȸ�� ��� (������ ��������)
    FRotator BoxRotation = BeamDirection.Rotation();
    FQuat BoxQuat = BoxRotation.Quaternion();

    // Overlap ��� ����
    TArray<FOverlapResult> OverlapResults;

    // �浹 ���� �Ķ����
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);
    QueryParams.bTraceComplex = false;

    // BoxOverlapMulti ����
    bool bHasOverlaps = GetWorld()->OverlapMultiByChannel(
        OverlapResults,
        BoxCenter,
        BoxQuat,
        ECC_Target,
        FCollisionShape::MakeBox(BoxHalfExtent),
        QueryParams
    );

    // ����� �ð�ȭ

#if ENABLE_DRAW_DEBUG
    if (bShowDebugVisualization)
    {
        const float DebugDuration = 0.1f;
        DrawDebugBox(
            GetWorld(),
            BoxCenter,
            BoxHalfExtent,
            BoxQuat,
            bHasOverlaps ? FColor::Yellow : FColor::Blue,
            false,
            DebugDuration,
            0,
            2.0f
        );
    }
#endif

    if (!bHasOverlaps) return;

    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* OverlappedActor = Result.GetActor();
        if (!OverlappedActor) continue;

        // CombatInterface üũ
        if (!OverlappedActor->Implements<UCombatInterface>()) continue;

        // ���� �� ����
        if (ICombatInterface::Execute_IsDead(OverlappedActor)) continue;

        // ★같은 액터의 히트박스가 여러 개면 오버랩도 여러 번 잡힌다★ — 중복을 제거한다.
        // 기존에는 "가장 가까운 하나"만 골라서 드러나지 않던 문제다.
        OutTargets.AddUnique(OverlappedActor);
    }

    // 가까운 순으로 정렬한다. 관통이 아닐 때 [0] 이 곧 기존의 "가장 가까운 대상"이다.
    OutTargets.Sort([&WeaponSocketLocation](const AActor& A, const AActor& B)
    {
        return FVector::DistSquared(WeaponSocketLocation, A.GetActorLocation())
             < FVector::DistSquared(WeaponSocketLocation, B.GetActorLocation());
    });
}

void UDRWaterPump::NarrowTargetsToChipRules(TArray<AActor*>& Targets) const
{
    // 관통이 아니면 가장 가까운 하나만 = ★기존 동작과 완전히 동일★ (회귀 0)
    if (!Resolved.bPierce)
    {
        if (Targets.Num() > 1)
        {
            Targets.SetNum(1);
        }
        return;
    }

    AActor* Avatar = GetAvatarActorFromActorInfo();

    // ★Plan5(물대포칩) / D-18★ 관통은 ★적에게만★ 적용된다.
    //   아군은 관통 여부와 무관하게 ★가장 가까운 한 명★ 만 급수 대상이다.
    //   Targets 는 거리순이므로 앞에서부터 훑으며 두 번째 이후의 아군만 버리면 된다.
    //   안전 상한(MaxPierceTargets)도 같은 루프에서 처리한다.
    TArray<AActor*> Narrowed;
    Narrowed.Reserve(FMath::Min(Targets.Num(), MaxPierceTargets));

    bool bAllyTaken = false;
    for (AActor* Target : Targets)
    {
        if (!UDRAbilitySystemLibrary::IsValidDamageTarget(Avatar, Target))
        {
            if (bAllyTaken) continue;      // 두 번째 이후의 아군은 버린다
            bAllyTaken = true;
        }

        Narrowed.Add(Target);
        if (Narrowed.Num() >= MaxPierceTargets) break;
    }

    Targets = MoveTemp(Narrowed);
}

void UDRWaterPump::StartWaterPumpLoop()
{
    UWorld* World = GetWorld();
    if (!World) return;

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter) return;

    // ★Plan5(물대포칩) / D-14★ 칩 스위치 해석은 ★서버/클라 분기보다 먼저★ 한다.
    //   관통은 소유 클라이언트의 1P 빔 길이 계산에도 필요하기 때문이다.
    //   (FDRUpgradeRuntime 은 각 머신이 복제된 EquippedChips 로 각자 만들므로 클라에서도 유효하다)
    ResolveUpgradedParams();

    // 공통 초기화
    // 첫 틱에서 즉시 데미지가 적용되도록 카운터를 임계값 직전으로 초기화
    DamageTickCounter = FMath::Max(0, DamageApplicationInterval - 1);
    CurrentTarget = nullptr;
    PreviousTarget = nullptr;
    CachedBeamEndPoint = FVector::ZeroVector;

    // ══ SERVER: 게임 로직 (데미지 틱 + GameplayCue) ══
    if (OwnerCharacter->HasAuthority())
    {
        // GameplayCue는 서버에서만 관리 → ASC가 자동으로 클라이언트에 리플리케이트
        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
        {
            // AddGameplayCue는 카운트 누적 방식 → 이전 Stop 누락 시 잔존하므로 먼저 제거
            const FGameplayTag& WaterPumpCue = FDRGameplayTags::Get().GameplayCue_Skill_WaterPump;
            ASC->RemoveGameplayCue(WaterPumpCue);
            ASC->AddGameplayCue(WaterPumpCue, FGameplayCueParameters());

            // 채널링 슬로우 GE 적용 (이전 핸들이 살아있다면 먼저 제거하여 스택 누적 방지)
            if (SlowSelfEffectClass)
            {
                if (ActiveSlowSelfHandle.IsValid())
                {
                    ASC->RemoveActiveGameplayEffect(ActiveSlowSelfHandle);
                    ActiveSlowSelfHandle.Invalidate();
                }

                FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
                ContextHandle.AddSourceObject(this);
                const FGameplayEffectSpecHandle SpecHandle =
                    ASC->MakeOutgoingSpec(SlowSelfEffectClass, GetAbilityLevel(), ContextHandle);
                if (SpecHandle.IsValid())
                {
                    ActiveSlowSelfHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
                }
            }
        }

        // 서버 전용 데미지 틱 타이머 (타겟 감지 + 데미지 적용)
        World->GetTimerManager().SetTimer(
            WaterPumpTimerHandle,
            this,
            &UDRWaterPump::PerformWaterPumpTick,
            TickInterval,
            true,
            0.0f
        );

        // 3P 빔 리플리케이트 상태 활성화
        if (ADRCharacter* DRChar = Cast<ADRCharacter>(OwnerCharacter))
        {
            // 3P 빔 길이 정규화가 1P와 같은 사거리를 쓰도록 동기화
            DRChar->WaterPumpWeaponRange = WeaponRange;

            // 활성화 전에 초기 끝지점 계산 (비소유 클라이언트에서 유효한 끝지점으로 3P 빔 생성)
            if (OwnerCharacter->Implements<UCombatInterface>())
            {
                FVector WeaponSocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
                    OwnerCharacter,
                    FDRGameplayTags::Get().CombatSocket_RightHand
                );

                bool bHitObstacle = false;
                FHitResult HitResult;
                DRChar->WaterPumpBeamEndPoint = CalculateWaterBeamEndPoint(
                    WeaponSocketLocation, bHitObstacle, HitResult);
            }

            DRChar->bWaterPumpActive = true;

            // 리슨 서버에서는 OnRep이 자동 호출되지 않으므로 수동 호출
            if (World->GetNetMode() != NM_DedicatedServer)
            {
                DRChar->OnRep_WaterPumpActive();
            }
        }
    }

    // ══ CLIENT: 1P 빔 전용 ══
    if (OwnerCharacter->IsLocallyControlled())
    {
        // VFX 생성 전에 초기 끝지점 계산 (유효한 HitEffectPosition으로 빔 생성)
        if (ADRCharacter* DRChar = Cast<ADRCharacter>(OwnerCharacter))
        {
            if (DRChar->FirstPersonMesh)
            {
                const FVector WeaponSocketLocation =
                    DRChar->FirstPersonMesh->GetSocketLocation(MuzzleSocketName);

                bool bHitObstacle = false;
                FHitResult HitResult;
                CachedBeamEndPoint = CalculateWaterBeamEndPoint(
                    WeaponSocketLocation, bHitObstacle, HitResult);
            }
        }

        StartBeamEffect();

        // 즉시 한 번 업데이트하여 회전/위치 보정 (33ms 갭 제거)
        UpdateBeamEndpoint();

        World->GetTimerManager().SetTimer(
            BeamUpdateTimer,
            this,
            &UDRWaterPump::UpdateBeamEndpoint,
            0.033f,
            true
        );
    }
}

void UDRWaterPump::StopWaterPumpLoop()
{
    UWorld* World = GetWorld();
    if (!World) return;

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());

    // ══ SERVER 정리 ══
    if (OwnerCharacter && OwnerCharacter->HasAuthority())
    {
        World->GetTimerManager().ClearTimer(WaterPumpTimerHandle);

        if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
        {
            ASC->RemoveGameplayCue(
                FDRGameplayTags::Get().GameplayCue_Skill_WaterPump
            );

            // 채널링 슬로우 GE 제거
            if (ActiveSlowSelfHandle.IsValid())
            {
                ASC->RemoveActiveGameplayEffect(ActiveSlowSelfHandle);
                ActiveSlowSelfHandle.Invalidate();
            }
        }

        DamageTickCounter = 0;
        CurrentTarget = nullptr;
        PreviousTarget = nullptr;

        // ★Plan5(물대포칩)★ 물대포를 끄면 가압 스택을 초기화한다(요구 ①-3).
        // EndAbility 도 StopWaterPumpLoop 를 부르므로 강제 취소 경로까지 한 줄로 덮인다.
        ClearRampStacks();

        // 3P 빔 리플리케이트 상태 비활성화
        if (ADRCharacter* DRChar = Cast<ADRCharacter>(OwnerCharacter))
        {
            DRChar->bWaterPumpActive = false;
            DRChar->WaterPumpBeamEndPoint = FVector::ZeroVector;

            if (World->GetNetMode() != NM_DedicatedServer)
            {
                DRChar->OnRep_WaterPumpActive();
            }
        }
    }

    // ══ CLIENT 정리 (1P 빔 전용) ══
    if (!OwnerCharacter || OwnerCharacter->IsLocallyControlled())
    {
        World->GetTimerManager().ClearTimer(BeamUpdateTimer);
        StopBeamEffect();
        CachedBeamEndPoint = FVector::ZeroVector;
    }
}

void UDRWaterPump::PerformWaterPumpTick()
{
    // 이 함수는 서버에서만 호출됨 (HasAuthority 타이머에 의해)
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter || !OwnerCharacter->Implements<UCombatInterface>()) return;

    FVector WeaponSocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
        OwnerCharacter,
        FDRGameplayTags::Get().CombatSocket_RightHand
    );

    // 서버에서 빔 끝점 계산 (리플리케이트된 ControlRotation 사용)
    bool bHitObstacle = false;
    FHitResult HitResult;
    FVector BeamEndPoint = CalculateWaterBeamEndPoint(WeaponSocketLocation, bHitObstacle, HitResult);

    // 캐릭터의 리플리케이트 빔 끝점 갱신 (비소유 클라이언트에서 3P 빔 위치 업데이트에 사용)
    // 일정 거리 이상 움직였을 때만 대입해서 정지 조준 시 프로퍼티가 dirty되지 않도록 함
    if (ADRCharacter* DRChar = Cast<ADRCharacter>(OwnerCharacter))
    {
        constexpr float BeamEndPointUpdateThresholdSq = 25.f * 25.f;
        if (FVector::DistSquared(DRChar->WaterPumpBeamEndPoint, BeamEndPoint) > BeamEndPointUpdateThresholdSq)
        {
            DRChar->WaterPumpBeamEndPoint = BeamEndPoint;
        }
    }

    // BoxOverlap으로 타겟 감지 — ★Plan5(물대포칩)★ 빔 안의 유효 대상을 거리순으로 전부 모은 뒤
    // 칩 규칙(관통 여부 + 아군 1명 제한 + 안전 상한)으로 좁힌다.
    TArray<AActor*> HitTargets;
    CollectTargetsInBeam(WeaponSocketLocation, BeamEndPoint, HitTargets);
    NarrowTargetsToChipRules(HitTargets);

    AActor* NewTarget = HitTargets.Num() > 0 ? HitTargets[0] : nullptr;

    // Ÿ�� ���� üũ
    if (NewTarget)
    {
        // ���ο� Ÿ������ Ȯ��
        if (CurrentTarget != NewTarget)
        {
            PreviousTarget = CurrentTarget;
            CurrentTarget = NewTarget;
            // 타겟 변경 시 카운터를 리셋하지 않음 (활성화 기준 주기 유지 → 스왑 익스플로잇 방지)

            OnTargetChanged(PreviousTarget.Get(), CurrentTarget.Get());
        }

        // ƽ ī���� ����
        DamageTickCounter++;

        // ������ ���� ���� üũ (1�ʸ���)
        if (DamageTickCounter >= DamageApplicationInterval)
        {
            DamageTickCounter = 0; // ī���� ����

            // ★이번 틱에 적을 맞혔는지는 ★피해를 넣기 전에★ 판정한다.★
            // 피해로 적이 죽으면 IsValidDamageTarget 이 false 가 되어
            // "마지막 일격을 넣으면 가압 스택이 끊기는" 이상한 동작이 된다.
            const bool bTickHitEnemy = HasEnemyTarget(HitTargets);

            // ★D-16★ ① 디버프를 먼저 건다 — 같은 틱에 들어오는 아군의 피해가 곧바로
            //   취약 배율을 받도록. (물대포 자신의 피해는 칩 ②에서 0 이라 순서가 결과를 바꾸진 않지만,
            //   조합이 늘었을 때 원인 불명의 차이가 생기지 않게 순서를 고정해 둔다)
            ApplyChipDebuffs(HitTargets);

            // ★D-15★ ② 대상 수만큼 BP 이벤트를 부른다.
            //   관통 칩이 없으면 목록이 1개라 호출 횟수도 기존과 같다(회귀 0).
            //   ⚠️ 이벤트 안에서 대상이 죽을 수 있으므로 매 반복 IsValid 를 다시 본다.
            for (AActor* Target : HitTargets)
            {
                if (IsValid(Target))
                {
                    OnDamageTickReached(Target);
                }
            }

            // ★③ 스택은 마지막★ — 이번 틱의 피해는 아직 강화되지 않은 값이다.
            AdvanceRampStack(bTickHitEnemy);
        }
    }
    else
    {
        // Ÿ���� ����
        if (CurrentTarget.IsValid())
        {
            PreviousTarget = CurrentTarget;
            CurrentTarget = nullptr;
            // 카운터 유지 (활성화 기준 주기)

            OnTargetChanged(PreviousTarget.Get(), nullptr);
        }
    }
}

FGameplayAbilityTargetDataHandle UDRWaterPump::MakeTargetDataHandleFromActors(AActor* TargetActor)
{
    FGameplayAbilityTargetDataHandle TargetDataHandle;

    if (!TargetActor) return TargetDataHandle;

    FGameplayAbilityTargetData_ActorArray* TargetData = new FGameplayAbilityTargetData_ActorArray();
    TargetData->TargetActorArray.Add(TargetActor);

    TargetDataHandle.Add(TargetData);

    return TargetDataHandle;
}

void UDRWaterPump::StartBeamEffect()
{
if (!WaterCannonEffect)
    {
return;
    }

    ADRCharacter* Character = Cast<ADRCharacter>(GetAvatarActorFromActorInfo());
    if (!Character)
    {
return;
    }

    USkeletalMeshComponent* FPMesh = Character->FirstPersonMesh;
    if (!FPMesh)
    {
return;
    }

    if (!FPMesh->DoesSocketExist(MuzzleSocketName))
    {
return;
    }

    // 직전 스폰이 정리되지 않은 경우(빠른 토글로 Stop 누락) 누수 방지
    if (FirstPersonBeam)
    {
        StopBeamEffect();
    }

    // 1P 빔만 생성 (IsLocallyControlled 분기에서 소유 클라이언트에서만 호출됨)
    // 3P 빔은 DRCharacter::OnRep_WaterPumpActive()에서 관리
    FirstPersonBeam = UNiagaraFunctionLibrary::SpawnSystemAttached(
        WaterCannonEffect,
        FPMesh,
        MuzzleSocketName,
        FVector::ZeroVector,
        FRotator::ZeroRotator,
        EAttachLocation::SnapToTarget,
        false
    );

    if (FirstPersonBeam)
    {
        FirstPersonBeam->SetOnlyOwnerSee(true);
        FirstPersonBeam->SetVectorParameter(FName("HitEffectPosition"), CachedBeamEndPoint);
}
    else
    {
}
}

void UDRWaterPump::UpdateBeamEndpoint()
{
    // 이 함수는 클라이언트에서만 호출됨 (IsLocallyControlled 타이머에 의해)
    ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetAvatarActorFromActorInfo());
    if (!DRCharacter) return;

    // 1P 메시 소켓에서 직접 위치를 가져옴 (3P 메시가 아닌 1P 메시 사용)
    USkeletalMeshComponent* FPMesh = DRCharacter->FirstPersonMesh;
    if (!FPMesh) return;

    const FVector WeaponSocketLocation = FPMesh->GetSocketLocation(MuzzleSocketName);

    // 로컬 카메라로 빔 끝점 계산 (1P 소켓 위치를 트레이스 시작점으로 사용)
    bool bHitObstacle = false;
    FHitResult HitResult;
    CachedBeamEndPoint = CalculateWaterBeamEndPoint(WeaponSocketLocation, bHitObstacle, HitResult);

    // 방향 벡터 및 길이 계산
    FVector BeamVector = CachedBeamEndPoint - WeaponSocketLocation;
    float BeamLength = BeamVector.Size();

    FVector BeamDir = BeamVector;

    if (!BeamDir.IsNearlyZero())
    {
        BeamDir.Normalize();
    }
    const FRotator BeamRotation = BeamDir.Rotation();

    // 1P Niagara 컴포넌트만 업데이트 (3P는 DRCharacter::UpdateWaterPumpThirdPersonBeam에서 관리)

    float NormalizedLength = BeamLength / WeaponRange;

    // 물 맞는 효과 위치 0~1000 사이로 조절
    float XValue = FMath::Clamp(NormalizedLength * 1000.0f, 0.0f, 1000.0f);

    // 물대포 이펙트 길이 0~5 사이로 조절
    float ZScale = FMath::Clamp(NormalizedLength * 5.0f, 0.0f, 5.0f);

    if (FirstPersonBeam)
    {
        FirstPersonBeam->SetWorldLocation(WeaponSocketLocation);
        FirstPersonBeam->SetWorldRotation(BeamRotation);
        FirstPersonBeam->SetVectorParameter(
            FName("WaterCannonScale"),
            FVector(0.2f, 0.2f, ZScale)

        );
        FirstPersonBeam->SetVectorParameter(
            FName("HitEffectPosition"),
            FVector(XValue, 0.0f, 0.0f)
        );
    }

    OnBeamEndPointUpdated(CachedBeamEndPoint);
}


void UDRWaterPump::StopBeamEffect()
{
    if (FirstPersonBeam)
    {
        FirstPersonBeam->DeactivateImmediate();
        FirstPersonBeam->DestroyComponent();
        FirstPersonBeam = nullptr;
    }
}

void UDRWaterPump::EndAbility(
    const FGameplayAbilitySpecHandle Handle,
    const FGameplayAbilityActorInfo* ActorInfo,
    const FGameplayAbilityActivationInfo ActivationInfo,
    bool bReplicateEndAbility,
    bool bWasCancelled)
{
    // BP의 EndAbility 이벤트가 실행되지 않는 캔슬 경로(다른 어빌리티의 강제 캔슬,
    // 인풋 시퀀스 꼬임 등)에서도 cleanup이 보장되도록 C++에서 직접 호출
    StopWaterPumpLoop();

    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

// ========================= Plan5(물대포칩) — 업그레이드 칩 =========================

void UDRWaterPump::ResolveUpgradedParams()
{
    const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    const FDRGameplayTags& Tags = FDRGameplayTags::Get();

    Resolved.bRamp = UDRAbilitySystemLibrary::HasUpgradeFeature(
        ASC, Tags.Upgrade_Feature_Skill_WaterPump_Ramp);
    Resolved.bSlow = UDRAbilitySystemLibrary::HasUpgradeFeature(
        ASC, Tags.Upgrade_Feature_Skill_WaterPump_Slow);
    Resolved.bVulnerable = UDRAbilitySystemLibrary::HasUpgradeFeature(
        ASC, Tags.Upgrade_Feature_Skill_WaterPump_Vulnerable);
    Resolved.bPierce = UDRAbilitySystemLibrary::HasUpgradeFeature(
        ASC, Tags.Upgrade_Feature_Skill_WaterPump_Pierce);
}

void UDRWaterPump::ApplyChipDebuffs(const TArray<AActor*>& Targets)
{
    if (!Resolved.bSlow && !Resolved.bVulnerable) return;

    AActor* Avatar = GetAvatarActorFromActorInfo();
    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    if (!Avatar || !SourceASC) return;

    const FDRWaterPumpChipTuning& Tuning =
        UDRAbilitySystemLibrary::GetChipTuning(Avatar).WaterPump;

    for (AActor* Target : Targets)
    {
        // ★아군에게는 걸지 않는다★ — 대상 목록에는 아군도 들어 있다(BP 가 분기한다).
        if (!UDRAbilitySystemLibrary::IsValidDamageTarget(Avatar, Target)) continue;

        // ②-1 감속 — Plan5(씨앗칩) 의 헬퍼를 ★그대로★ 쓴다.
        if (Resolved.bSlow)
        {
            UDRAbilitySystemLibrary::ApplyTimedSpeedEffect(
                Target, SourceASC, Tuning.EnemySlow,
                FMath::Max(0.05f, 1.f - Tuning.EnemySlow.Magnitude));
        }

        // ②-2 취약 — 태그만 거는 시한 GE. 배율은 피해 시점에 Config 에서 읽는다.
        if (Resolved.bVulnerable)
        {
            UDRAbilitySystemLibrary::ApplyTimedTagEffect(
                Target, SourceASC, Tuning.VulnerableEffect, Tuning.VulnerableDuration);
        }
    }
}

bool UDRWaterPump::HasEnemyTarget(const TArray<AActor*>& Targets) const
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!Avatar) return false;

    for (AActor* Target : Targets)
    {
        if (UDRAbilitySystemLibrary::IsValidDamageTarget(Avatar, Target))
        {
            return true;
        }
    }
    return false;
}

void UDRWaterPump::AdvanceRampStack(bool bTickHitEnemy)
{
    // ★요구 ①-1★ "동일 적이 아니어도 된다" = 대상이 누구든 상관없다.
    // ★단 한 틱에 1스택★ — 관통으로 여러 명을 맞혀도 1이다. 아군만 맞힌 틱은 오르지 않는다.
    if (!Resolved.bRamp || !bTickHitEnemy) return;

    AActor* Avatar = GetAvatarActorFromActorInfo();
    UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!Avatar || !ASC) return;

    const FDRWaterPumpChipTuning& Tuning =
        UDRAbilitySystemLibrary::GetChipTuning(Avatar).WaterPump;

    UClass* StackClass = Tuning.RampStackEffect.LoadSynchronous();
    if (!StackClass) return;   // 미지정이면 조용히 — 매 초 경고를 쏟지 않는다

    FGameplayEffectSpecHandle SpecHandle =
        MakeOutgoingGameplayEffectSpec(StackClass, GetAbilityLevel());
    if (!SpecHandle.IsValid()) return;

    // 지속시간을 매번 실어 보낸다 → 상한에 도달한 뒤에도 ★갱신★ 이 계속된다.
    // ★GE 의 Deny Overflow Application 을 체크하면 이 갱신이 막힌다★ (Plan5 함정표 T-4)
    SpecHandle.Data->SetSetByCallerMagnitude(
        FDRGameplayTags::Get().Data_Duration, FMath::Max(0.1f, Tuning.RampResetGrace));

    // ★핸들을 보관한다★ — 스택 수는 이 핸들로만 읽을 수 있다 (헤더의 RampStackHandle 주석 참고).
    RampStackHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UDRWaterPump::ClearRampStacks()
{
    if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
    {
        // 핸들이 아니라 태그로 지운다 — 강제 취소 등으로 핸들을 잃어도 확실히 정리된다.
        FGameplayTagContainer PressureTags;
        PressureTags.AddTag(FDRGameplayTags::Get().Buff_WaterPump_Pressure);
        ASC->RemoveActiveEffectsWithGrantedTags(PressureTags);
    }

    // 핸들도 같이 비운다. (안 비워도 GetCurrentStackCount 가 0 을 돌려주지만,
    //  "끈 뒤에도 핸들이 살아 있는 것처럼 보이는" 상태를 남기지 않는다)
    RampStackHandle.Invalidate();
}

float UDRWaterPump::GetRuntimeDamageMultiplier() const
{
    const UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
    if (!ASC) return 1.f;

    // ★스택 수는 액티브 GE 의 StackCount 로 읽는다 — ★태그 카운트가 아니다★.★
    // GAS 는 Stacking 이 켜진 GE 를 하나의 액티브 GE 로 합치고 StackCount 만 올리므로,
    // 부여 태그의 카운트는 스택이 몇 개든 항상 1 이다. (헤더 RampStackHandle 주석 참고)
    // 핸들이 만료/무효면 0 을 돌려주므로 별도 분기가 필요 없다.
    const int32 Stacks = ASC->GetCurrentStackCount(RampStackHandle);
    if (Stacks <= 0) return 1.f;

    const FDRWaterPumpChipTuning& Tuning =
        UDRAbilitySystemLibrary::GetChipTuning(GetAvatarActorFromActorInfo()).WaterPump;

    // ★상한은 Config 가 최종 권위다★ — GE 의 Stack Limit 이 더 크게 잡혀 있어도 여기서 자른다.
    const float Bonus = FMath::Min(Tuning.RampMaxDamageBonus,
                                   Tuning.RampDamageBonusPerStack * Stacks);

    return 1.f + FMath::Max(0.f, Bonus);
}

void UDRWaterPump::GrantWaterToAlly(AActor* AllyActor)
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!IsValid(AllyActor) || !Avatar || !Avatar->HasAuthority()) return;
    if (!AllyWaterGrantEffect) return;

    UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
    UAbilitySystemComponent* TargetASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AllyActor);
    if (!SourceASC || !TargetASC) return;

    // ★칩 반영 지점★ — 칩 ① 이면 ×0, 칩 ② 면 ×2, 둘 다면 0 × 2 = 0.
    const float Amount =
        FMath::Max(0.f, GetUpgradedFloat(EDRUpgradeStat::SkillWaterGrant, AllyWaterGrantAmount));

    // 0 이면 GE 자체를 얹지 않는다 — 0 짜리 이펙트는 큐/로그만 더럽힌다. (칩 ①의 단점 경로)
    if (Amount <= 0.f) return;

    FGameplayEffectSpecHandle SpecHandle =
        MakeOutgoingGameplayEffectSpec(AllyWaterGrantEffect, GetAbilityLevel());
    if (!SpecHandle.IsValid()) return;

    // 적 처치 물 지급(ADREnemy::GrantWaterToPlayers)과 ★같은 SetByCaller 태그★ 를 쓴다.
    SpecHandle.Data->SetSetByCallerMagnitude(
        FDRGameplayTags::Get().Water_SetByCaller_Grant, Amount);

    SourceASC->ApplyGameplayEffectSpecToTarget(*SpecHandle.Data.Get(), TargetASC);
}

bool UDRWaterPump::GetAimDirection(FVector& OutAimStart, FVector& OutAimDirection) const
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter) return false;

    APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (!PC) return false;

    // ī�޶� ��ġ�� ���� ���ϱ�
    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    OutAimStart = CameraLocation;
    OutAimDirection = CameraRotation.Vector();

    return true;
}


