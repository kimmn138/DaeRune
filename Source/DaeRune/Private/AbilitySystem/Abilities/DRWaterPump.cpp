// Copyright DaeRune


#include "AbilitySystem/Abilities/DRWaterPump.h"
#include "Camera/CameraComponent.h"
#include "Character/DRCharacterBase.h"
#include "DaeRune/DaeRune.h"
#include "DRGameplayTags.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/KismetSystemLibrary.h"

bool UDRWaterPump::TraceFromWeaponToAim(const FVector& WeaponSocketLocation, FHitResult& OutHitResult)
{
    // 소유자 캐릭터 확인
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter)
    {
        return false;
    }

    // 플레이어 컨트롤러 확인
    APlayerController* PC = Cast<APlayerController>(OwnerCharacter->GetController());
    if (!PC)
    {
        return false;
    }

    // 카메라 위치와 방향 구하기
    FVector CameraLocation;
    FRotator CameraRotation;
    PC->GetPlayerViewPoint(CameraLocation, CameraRotation);

    // 카메라에서 사정거리만큼 떨어진 지점 계산 (벡터 계산)
    FVector AimDirection = CameraRotation.Vector();
    FVector TargetLocation = CameraLocation + (AimDirection * WeaponRange);

    // 무기 소켓에서 목표 지점으로 라인 트레이스
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(OwnerCharacter);
    QueryParams.bTraceComplex = false;
    QueryParams.bReturnPhysicalMaterial = false;

    // 라인 트레이스 실행
    bool bHit = GetWorld()->LineTraceSingleByChannel(
        OutHitResult,
        WeaponSocketLocation,
        TargetLocation,
        ECC_Target,
        QueryParams
    );

    // 디버그 시각화 (옵션)
#if ENABLE_DRAW_DEBUG
    const float DebugDuration = 2.0f;
    const FColor TraceColor = bHit ? FColor::Red : FColor::Green;

    DrawDebugLine(
        GetWorld(),
        WeaponSocketLocation,
        bHit ? OutHitResult.ImpactPoint : TargetLocation,
        TraceColor,
        false,
        DebugDuration,
        0,
        2.0f
    );

    if (bHit)
    {
        DrawDebugSphere(
            GetWorld(),
            OutHitResult.ImpactPoint,
            10.0f,
            12,
            FColor::Red,
            false,
            DebugDuration
        );
    }
#endif

    return bHit;
}

void UDRWaterPump::StartWaterPumpLoop()
{
    // 타이머 시작
    if (UWorld* World = GetWorld())
    {
        DamageTickCounter = 0;
        CurrentTarget = nullptr;
        PreviousTarget = nullptr;

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
    // 타이머 정지
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(WaterPumpTimerHandle);

        DamageTickCounter = 0;
        CurrentTarget = nullptr;
        PreviousTarget = nullptr;
    }
}

void UDRWaterPump::PerformWaterPumpTick()
{
    // 무기 소켓 위치 가져오기
    ACharacter* OwnerCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
    if (!OwnerCharacter || !OwnerCharacter->Implements<UCombatInterface>())
    {
        return;
    }

    FVector WeaponSocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
        OwnerCharacter,
        FDRGameplayTags::Get().CombatSocket_LeftHand
    );

    // 트레이싱 수행
    FHitResult HitResult;
    bool bHit = TraceFromWeaponToAim(WeaponSocketLocation, HitResult);

    if (bHit && HitResult.GetActor())
    {
        // 새로운 타겟인지 확인
        if (CurrentTarget != HitResult.GetActor())
        {
            PreviousTarget = CurrentTarget;
            CurrentTarget = HitResult.GetActor();
            DamageTickCounter = 0; // 새 타겟이면 카운터 리셋

            // 블루프린트에서 타겟 변경 이벤트 처리 가능
            OnTargetChanged(PreviousTarget.Get(), CurrentTarget.Get());
        }

        // 틱 카운터 증가
        DamageTickCounter++;

        // 데미지/효과 적용 시점 체크 (1초마다)
        if (DamageTickCounter >= DamageApplicationInterval)
        {
            DamageTickCounter = 0;

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

FGameplayAbilityTargetDataHandle UDRWaterPump::MakeTargetDataHandleFromActor(AActor* TargetActor)
{
    FGameplayAbilityTargetDataHandle TargetDataHandle;

    if (!TargetActor)
    {
        return TargetDataHandle;
    }

    FGameplayAbilityTargetData_ActorArray* TargetData = new FGameplayAbilityTargetData_ActorArray();
    TargetData->TargetActorArray.Add(TargetActor);

    TargetDataHandle.Add(TargetData);

    return TargetDataHandle;
}

