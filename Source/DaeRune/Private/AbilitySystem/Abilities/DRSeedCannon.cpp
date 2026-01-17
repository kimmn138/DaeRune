// Copyright DaeRune


#include "AbilitySystem/Abilities/DRSeedCannon.h"
#include "Actor/DRSeedProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Interaction/CombatInterface.h"
#include "Kismet/KismetMathLibrary.h"

void UDRSeedCannon::SpawnSeedProjectile(const FVector& ForwardVector, const FGameplayTag& SocketTag)
{
    const bool bIsServer = GetAvatarActorFromActorInfo()->HasAuthority();
    if (!bIsServer) return;

    // 발사 위치 가져오기
    const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
        GetAvatarActorFromActorInfo(),
        SocketTag
    );

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

    // 디버그 시각화 (개발 빌드에서만)
#if !UE_BUILD_SHIPPING
    // 발사 방향 화살표 그리기
    DrawDebugDirectionalArrow(
        GetWorld(),
        SocketLocation,
        SocketLocation + (LaunchDirection * 200.f),
        50.f,
        FColor::Green,
        false,
        3.0f,
        0,
        5.f
    );

    // 예상 궤적 그리기 (간단한 포물선)
    FVector PrevPoint = SocketLocation;
    float TimeStep = 0.05f;
    for (int i = 1; i <= 20; i++)
    {
        float Time = TimeStep * i;
        FVector Point = SocketLocation + (LaunchDirection * LaunchSpeed * Time) +
            (0.5f * FVector(0, 0, -980.f) * Time * Time); // 중력 가속도

        DrawDebugLine(GetWorld(), PrevPoint, Point, FColor::Yellow, false, 3.0f, 0, 2.f);
        PrevPoint = Point;
    }
#endif

    UE_LOG(LogTemp, Log, TEXT("SeedCannon: Projectile spawned successfully"));
}
