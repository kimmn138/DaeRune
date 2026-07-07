// Copyright DaeRune


#include "AbilitySystem/Abilities/DRWaterPump.h"
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
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"

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

    // LineTrace ����
    bHitObstacle = GetWorld()->LineTraceSingleByChannel(
        OutHitResult,
        WeaponSocketLocation,
        CameraTargetPoint,
        ECC_Visibility,
        QueryParams
    );

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

AActor* UDRWaterPump::FindClosestTargetInBeam(const FVector& WeaponSocketLocation, const FVector& BeamEndPoint)
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter) return nullptr;

    // ������ ����� ���� ���
    FVector BeamDirection = (BeamEndPoint - WeaponSocketLocation).GetSafeNormal();
    float BeamLength = FVector::Distance(WeaponSocketLocation, BeamEndPoint);

    // �ڽ� �߽��� (���� ���ϰ� ������ �߰�)
    FVector BoxCenter = WeaponSocketLocation + (BeamDirection * BeamLength * 0.5f);

    // �ڽ� ũ�� (���� �� ����)
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

    if (!bHasOverlaps) return nullptr;

    // ���� ����� �� ã��
    AActor* ClosestTarget = nullptr;
    float ClosestDistanceSq = FLT_MAX;

    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* OverlappedActor = Result.GetActor();
        if (!OverlappedActor) continue;

        // CombatInterface üũ
        if (!OverlappedActor->Implements<UCombatInterface>()) continue;

        // ���� �� ����
        if (ICombatInterface::Execute_IsDead(OverlappedActor)) continue;

        // �Ÿ� ���
        float DistanceSq = FVector::DistSquared(WeaponSocketLocation, OverlappedActor->GetActorLocation());

        if (DistanceSq < ClosestDistanceSq)
        {
            ClosestDistanceSq = DistanceSq;
            ClosestTarget = OverlappedActor;
        }
    }

    return ClosestTarget;
}

void UDRWaterPump::StartWaterPumpLoop()
{
    UWorld* World = GetWorld();
    if (!World) return;

    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter) return;

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

    // BoxOverlap으로 타겟 감지
    AActor* NewTarget = FindClosestTargetInBeam(WeaponSocketLocation, BeamEndPoint);

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

            // ��������Ʈ���� ȿ�� ����
            OnDamageTickReached(CurrentTarget.Get());
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


