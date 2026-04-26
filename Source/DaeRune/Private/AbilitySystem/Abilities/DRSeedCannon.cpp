// Copyright DaeRune


#include "AbilitySystem/Abilities/DRSeedCannon.h"
#include "Actor/DRSeedProjectile.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Interaction/CombatInterface.h"

void UDRSeedCannon::SpawnSeedProjectile(const FVector& ForwardVector, const FGameplayTag& SocketTag)
{
    const bool bIsServer = GetAvatarActorFromActorInfo()->HasAuthority();
    if (!bIsServer) return;

    // 3인칭 메시에서 발사 위치 가져오기
    FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
        GetAvatarActorFromActorInfo(), SocketTag);

    // ī�޶� ������ ���͸� �޾Ƽ� 60�� ���� ȸ��
    // ������ ���͸� �������� �����
    FVector CameraForward = ForwardVector;
    CameraForward.Normalize();

    // Right ���� ���
    FVector CameraUp = FVector::UpVector;
    FVector RightVector = FVector::CrossProduct(CameraUp, CameraForward);
    RightVector.Normalize();

    // LaunchAngle��ŭ ���� ȸ��
    FVector LaunchDirection = CameraForward.RotateAngleAxis(LaunchAngle, RightVector);
    LaunchDirection.Normalize();

    // Transform ����
    FTransform SpawnTransform;
    SpawnTransform.SetLocation(SocketLocation);
    SpawnTransform.SetRotation(LaunchDirection.Rotation().Quaternion());

    // �߻�ü ���� (Deferred Spawn)
    ADRSeedProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRSeedProjectile>(
        SeedProjectileClass,
        SpawnTransform,
        GetOwningActorFromActorInfo(),
        Cast<APawn>(GetOwningActorFromActorInfo()),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    // ������ �Ķ���� ����
    Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();

    // �ӵ� ���� ���� ����
    Projectile->ProjectileMovement->Velocity = LaunchDirection * LaunchSpeed;

    // �߷� Ȯ���� Ȱ��ȭ
    Projectile->ProjectileMovement->ProjectileGravityScale = 1.0f;

    // ���� ��� ��Ȱ��ȭ
    Projectile->ProjectileMovement->bIsHomingProjectile = false;
    Projectile->ProjectileMovement->HomingTargetComponent = nullptr;

    // ȸ���� �ӵ� �������� ����
    Projectile->ProjectileMovement->bRotationFollowsVelocity = true;
    Projectile->ProjectileMovement->bInitialVelocityInLocalSpace = false;

    // ���� �Ϸ�
    Projectile->FinishSpawning(SpawnTransform);

//    // ����� �ð�ȭ (���� ���忡����)
//#if !UE_BUILD_SHIPPING
//    // �߻� ���� ȭ��ǥ �׸���
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
//    // ���� ���� �׸��� (������ ������)
//    FVector PrevPoint = SocketLocation;
//    float TimeStep = 0.05f;
//    for (int i = 1; i <= 20; i++)
//    {
//        float Time = TimeStep * i;
//        FVector Point = SocketLocation + (LaunchDirection * LaunchSpeed * Time) +
//            (0.5f * FVector(0, 0, -980.f) * Time * Time); // �߷� ���ӵ�
//
//        DrawDebugLine(GetWorld(), PrevPoint, Point, FColor::Yellow, false, 3.0f, 0, 2.f);
//        PrevPoint = Point;
//    }
//#endif
}
