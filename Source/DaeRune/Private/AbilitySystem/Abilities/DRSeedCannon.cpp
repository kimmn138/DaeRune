// Copyright DaeRune


#include "AbilitySystem/Abilities/DRSeedCannon.h"
#include "Actor/DRSeedProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"

float UDRSeedCannon::GetAttackSpeedPlayRate() const
{
	// ★Plan5(공속칩)★ 버프가 없으면 GetAttackSpeedMultiplier() 가 1.0 이라 BaseMontagePlayRate 그대로 (D-68)
	return BaseMontagePlayRate * GetAttackSpeedMultiplier();
}

void UDRSeedCannon::SpawnSeedProjectile(const FVector& ForwardVector, const FVector& SocketLocation)
{
    const bool bIsServer = GetAvatarActorFromActorInfo()->HasAuthority();
    if (!bIsServer) return;

    // 카메라 포워드 벡터를 받아서 60도 위로 회전
    // 포워드 벡터를 수평으로 만들기
    FVector CameraForward = ForwardVector;
    CameraForward.Normalize();

    // Right 벡터 계산
    FVector CameraUp = FVector::UpVector;
    FVector RightVector = FVector::CrossProduct(CameraUp, CameraForward);
    RightVector.Normalize();

    // LaunchAngle만큼 위로 회전
    FVector LaunchDirection = CameraForward.RotateAngleAxis(LaunchAngle, RightVector);
    LaunchDirection.Normalize();

    // Transform 설정
    FTransform SpawnTransform;
    SpawnTransform.SetLocation(SocketLocation);
    SpawnTransform.SetRotation(LaunchDirection.Rotation().Quaternion());

    // 발사체 생성 (Deferred Spawn)
    ADRSeedProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRSeedProjectile>(
        SeedProjectileClass,
        SpawnTransform,
        GetOwningActorFromActorInfo(),
        Cast<APawn>(GetOwningActorFromActorInfo()),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    // 데미지 파라미터 설정
    Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();

    // ★Plan5(씨앗칩)★ 직격 환급 칩(GR.Asc.SeedRefund)이 돌려줄 금액의 근거.
    // GetEffectiveWaterCostFor 는 CheckCost/ApplyCost 가 쓰는 것과 ★같은 함수★ 라
    // 실제 지불액과 어긋나지 않는다.
    Projectile->SpentWaterCost = GetEffectiveWaterCostFor(GetCurrentActorInfo());

    // 속도 벡터 직접 설정
    Projectile->ProjectileMovement->Velocity = LaunchDirection * LaunchSpeed;

    // 중력 확실히 활성화
    Projectile->ProjectileMovement->ProjectileGravityScale = 1.0f;

    // 유도 기능 비활성화
    Projectile->ProjectileMovement->bIsHomingProjectile = false;
    Projectile->ProjectileMovement->HomingTargetComponent = nullptr;

    // 회전도 속도 방향으로 설정
    Projectile->ProjectileMovement->bRotationFollowsVelocity = true;
    Projectile->ProjectileMovement->bInitialVelocityInLocalSpace = false;

    // 스폰 완료
    Projectile->FinishSpawning(SpawnTransform);

//    // 디버그 시각화 (개발 빌드에서만)
//#if !UE_BUILD_SHIPPING
//    // 발사 방향 화살표 그리기
//    DrawDebugDirectionalArrow(
//        GetWorld(),
//        SocketLocation,
//        SocketLocation + (LaunchDirection * 200.f),
//        50.f,
//        FColor::Green,
//        false,
//        3.0f,
//        0,
//        5.f
//    );
//
//    // 예상 궤적 그리기 (간단한 포물선)
//    FVector PrevPoint = SocketLocation;
//    float TimeStep = 0.05f;
//    for (int i = 1; i <= 20; i++)
//    {
//        float Time = TimeStep * i;
//        FVector Point = SocketLocation + (LaunchDirection * LaunchSpeed * Time) +
//            (0.5f * FVector(0, 0, -980.f) * Time * Time); // 중력 가속도
//
//        DrawDebugLine(GetWorld(), PrevPoint, Point, FColor::Yellow, false, 3.0f, 0, 2.f);
//        PrevPoint = Point;
//    }
//#endif
}
