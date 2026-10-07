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

    // ★Plan5(씨앗칩)★ 직격 환급 칩(GR.Asc.SeedRefund)이 돌려줄 금액의 근거.
    // GetEffectiveWaterCostFor 는 CheckCost/ApplyCost 가 쓰는 것과 ★같은 함수★ 라
    // 실제 지불액과 어긋나지 않는다.
    Projectile->SpentWaterCost = GetEffectiveWaterCostFor(GetCurrentActorInfo());

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
