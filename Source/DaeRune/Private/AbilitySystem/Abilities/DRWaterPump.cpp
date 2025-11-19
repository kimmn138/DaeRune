// Copyright DaeRune


#include "AbilitySystem/Abilities/DRWaterPump.h"
#include "Camera/CameraComponent.h"
#include "Character/DRCharacterBase.h"
#include "DaeRune/DaeRune.h"
#include "DRGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Interaction/CombatInterface.h"
#include "DrawDebugHelpers.h"
#include "Engine/OverlapResult.h"

FVector UDRWaterPump::CalculateWaterBeamEndPoint(const FVector& WeaponSocketLocation, bool& bHitObstacle, FHitResult& OutHitResult)
{
    FVector AimStart, AimDirection;
    if (!GetAimDirection(AimStart, AimDirection))
    {
        bHitObstacle = false;
        return WeaponSocketLocation + FVector::ForwardVector * WeaponRange;
    }

    // 카메라에서 최대 사정거리까지의 목표 지점
    FVector CameraTargetPoint = AimStart + (AimDirection * WeaponRange);

    // 무기 소켓에서 목표 지점으로 LineTrace
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);
    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = false;

    // LineTrace 실행 (장애물 감지)
    bHitObstacle = GetWorld()->LineTraceSingleByChannel(
        OutHitResult,
        WeaponSocketLocation,
        CameraTargetPoint,
        ECC_Visibility, // 벽/장애물 감지용 채널
        QueryParams
    );

    FVector BeamEndPoint;
    if (bHitObstacle)
    {
        // 중간에 장애물 있으면 그 지점까지만
        BeamEndPoint = OutHitResult.ImpactPoint;
    }
    else
    {
        // 장애물 없으면 최대 거리까지
        BeamEndPoint = CameraTargetPoint;
    }

    // 디버그 시각화
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

    // 물대포 방향과 길이 계산
    FVector BeamDirection = (BeamEndPoint - WeaponSocketLocation).GetSafeNormal();
    float BeamLength = FVector::Distance(WeaponSocketLocation, BeamEndPoint);

    // 박스 중심점 (무기 소켓과 끝점의 중간)
    FVector BoxCenter = WeaponSocketLocation + (BeamDirection * BeamLength * 0.5f);

    // 박스 크기 (좁고 긴 형태)
    FVector BoxHalfExtent(BeamLength * 0.5f, BeamWidth * 0.5f, BeamHeight * 0.5f);

    // 회전 계산 (물대포 방향으로)
    FRotator BoxRotation = BeamDirection.Rotation();
    FQuat BoxQuat = BoxRotation.Quaternion();

    // Overlap 결과 저장
    TArray<FOverlapResult> OverlapResults;

    // 충돌 쿼리 파라미터
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);
    QueryParams.bTraceComplex = false;

    // BoxOverlapMulti 실행
    bool bHasOverlaps = GetWorld()->OverlapMultiByChannel(
        OverlapResults,
        BoxCenter,
        BoxQuat,
        ECC_Target,
        FCollisionShape::MakeBox(BoxHalfExtent),
        QueryParams
    );

    // 디버그 시각화
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

    // 가장 가까운 적 찾기
    AActor* ClosestTarget = nullptr;
    float ClosestDistanceSq = FLT_MAX;

    for (const FOverlapResult& Result : OverlapResults)
    {
        AActor* OverlappedActor = Result.GetActor();
        if (!OverlappedActor) continue;

        // CombatInterface 체크
        if (!OverlappedActor->Implements<UCombatInterface>()) continue;

        // 죽은 적 제외
        if (ICombatInterface::Execute_IsDead(OverlappedActor)) continue;

        // 거리 계산
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
    if (UWorld* World = GetWorld())
    {
        // 초기화
        DamageTickCounter = 0;
        CurrentTarget = nullptr;
        PreviousTarget = nullptr;
        CachedBeamEndPoint = FVector::ZeroVector;

        // 타이머 시작
        World->GetTimerManager().SetTimer(
            WaterPumpTimerHandle,
            this,
            &UDRWaterPump::PerformWaterPumpTick,
            TickInterval,
            true, // Looping
            0.0f  // 즉시 시작
        );
    }
}

void UDRWaterPump::StopWaterPumpLoop()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(WaterPumpTimerHandle);

        // 정리
        DamageTickCounter = 0;
        CurrentTarget = nullptr;
        PreviousTarget = nullptr;
        CachedBeamEndPoint = FVector::ZeroVector;
    }
}

void UDRWaterPump::PerformWaterPumpTick()
{
    // 무기 소켓 위치 가져오기
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter || !OwnerCharacter->Implements<UCombatInterface>()) return;

    FVector WeaponSocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
        OwnerCharacter,
        FDRGameplayTags::Get().CombatSocket_RightHand
    );

    // 1단계: LineTrace로 물대포 끝점 계산
    bool bHitObstacle = false;
    FHitResult HitResult;
    FVector BeamEndPoint = CalculateWaterBeamEndPoint(WeaponSocketLocation, bHitObstacle, HitResult);

    // 끝점 캐시 (블루프린트에서 이펙트 위치로 사용)
    CachedBeamEndPoint = BeamEndPoint;
    OnBeamEndPointUpdated(BeamEndPoint);

    // 2단계: BoxOverlap으로 가장 가까운 적 1명 찾기
    AActor* NewTarget = FindClosestTargetInBeam(WeaponSocketLocation, BeamEndPoint);

    // 3단계: 타겟 변경 체크
    if (NewTarget)
    {
        // 새로운 타겟인지 확인
        if (CurrentTarget != NewTarget)
        {
            PreviousTarget = CurrentTarget;
            CurrentTarget = NewTarget;
            DamageTickCounter = 0; // 새 타겟이면 카운터 리셋

            // 블루프린트 이벤트
            OnTargetChanged(PreviousTarget.Get(), CurrentTarget.Get());
        }

        // 틱 카운터 증가
        DamageTickCounter++;
        
        // 데미지 적용 시점 체크 (1초마다)
        if (DamageTickCounter >= DamageApplicationInterval)
        {
            DamageTickCounter = 0; // 카운터 리셋

            // 블루프린트에서 효과 적용
            OnDamageTickReached(CurrentTarget.Get());
        }
    }
    else
    {
        // 타겟을 잃음
        if (CurrentTarget.IsValid())
        {
            PreviousTarget = CurrentTarget;
            CurrentTarget = nullptr;
            DamageTickCounter = 0;

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

bool UDRWaterPump::GetAimDirection(FVector& OutAimStart, FVector& OutAimDirection) const
{
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter) return false;

    APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (!PC) return false;

    // 카메라 위치와 방향 구하기
    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    OutAimStart = CameraLocation;
    OutAimDirection = CameraRotation.Vector();

    return true;
}

