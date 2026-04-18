# 아르마딜로(Armadillo) 적 캐릭터 구현 계획 (Plan3.md)

기존 `ADREnemy` 시스템을 기반으로 **폼 전환(BasicForm ↔ BallForm)** 메커니즘을 가진 아르마딜로 적 캐릭터 구현 방법을 상세히 기술한다.

---

## 에셋 현황

### 스켈레탈 메시
| 폼 | 경로 | 비고 |
|----|------|------|
| BasicForm | `Content/DaeRuneAssets/Characters/Enemy/Armadilo/BasicForm/Armadillo_v0_1_2.uasset` | 걸어다니는 기본 형태 |
| BallForm | `Content/DaeRuneAssets/Characters/Enemy/Armadilo/RollForm/Armadillo_Ball_v0_1_0.uasset` | 둥글게 말린 볼 형태 |

**중요**: BasicForm과 BallForm은 **서로 다른 Skeleton/리깅**을 사용한다. 하나의 AnimBP로 처리할 수 없으므로 **2개의 SkeletalMeshComponent + 2개의 AnimBP**가 필요하다.

### 애니메이션
| 폼 | 애니메이션 | 용도 |
|----|-----------|------|
| BasicForm | Idle | 기본 대기 |
| BasicForm | Walk | 걷기 이동 |
| BasicForm | BasicAttack | 근접 공격 |
| BasicForm | Death | 사망 |
| BasicForm | HitReact 1, 2, 3 | 피격 리액션 |
| BasicForm | Stun | 스턴 상태 |
| BasicForm | FormChange_BasicToBall | 기본→볼 폼 전환 |
| BasicForm | FormChange_BallToBasic | 볼→기본 폼 전환 |
| BallForm | Idle | 볼 대기 |
| BallForm | Roll | 구르기 이동 |
| BallForm | FormChange_BasicToBall | 기본→볼 폼 전환 |
| BallForm | FormChange_BallToBasic | 볼→기본 폼 전환 |

---

## 핵심 설계 결정

### 1. 듀얼 메시 시스템 (리깅이 다른 2개의 폼)

BasicForm과 BallForm의 Skeleton이 다르므로, **하나의 Actor에 2개의 SkeletalMeshComponent**를 배치한다.

```
ADRArmadilloEnemy
├── CapsuleComponent (Root)
├── BasicFormMesh (SkeletalMeshComponent) ← 기본 폼 메시, 평시 Visible
│   └── ABP_Armadillo_Basic (AnimInstance)
├── BallFormMesh (SkeletalMeshComponent) ← 볼 폼 메시, 평시 Hidden
│   └── ABP_Armadillo_Ball (AnimInstance)
├── RollCollisionSphere (SphereComponent, R=30) ← 구르기 전방 충돌 감지용
└── RollImpactSphere (SphereComponent, R=50) ← 충돌 시 효과 적용 범위
```

**폼 전환 시 처리 순서**:
1. 전환 애니메이션 재생 (현재 폼의 AnimBP에서)
2. 애니메이션 완료 시점에 AnimNotify 발동
3. 현재 폼 메시 Hidden + 새 폼 메시 Visible
4. CapsuleComponent 크기 조정 (볼 폼은 더 작을 수 있음)
5. 내부 상태 플래그 갱신 (`bIsBallForm`)

### 2. 공격 시스템 설계

| 구분 | 기본 폼 (BasicForm) | 볼 폼 (BallForm) |
|------|---------------------|-------------------|
| 공격 유형 | 근접 공격 (Dog의 GA_DogBite 복사) | 롤링 돌진 스킬 |
| 발동 조건 | BT에서 플레이어 감지 시 | 현재 타겟이 플레이어 + 쿨타임 10초 경과 |
| GA 클래스 | `GA_ArmadilloBite` (BP) | `GA_ArmadilloRollCharge` (BP) |
| 데미지 | 기본 근접 데미지 | 50 (대상 무관) + 클렌저 1.5배 |
| 부가 효과 | 없음 | 플레이어 1초 스턴, 지형 충돌 시 본인 1.5초 스턴 |

### 3. 볼 폼 롤링 스킬 상세

```
[스킬 발동 흐름]

1. BT에서 스킬 조건 확인 (타겟=플레이어, 쿨타임 10초)
   ↓
2. 폼 전환: Basic → Ball (FormChange 애니메이션)
   ↓
3. 타겟 선택: 3000 거리 내 + 벽 없는 랜덤 플레이어 1명
   - LineTrace로 벽 차단 여부 확인
   - 조건 만족 플레이어 없으면 스킬 취소 → 기본 폼 복귀
   ↓
4. 타겟팅 시점의 플레이어 위치 저장 (고정 목표점)
   ↓
5. 일직선 돌진 시작
   - 방향: 아르마딜로 → 저장된 목표점
   - 속도: 시간에 따라 점진적 가속 (초기 200 → 최대 2000)
   - 전방 충돌 감지: 반지름 30 구(Sphere Sweep)
   ↓
6-A. 충돌 감지 (첫 번째 대상)
   → 충돌 지점에서 반지름 50 구 범위 판정
   → 범위 내 대상에 따라 분기:

   [6-A-1] 플레이어 또는 다른 적이 범위 내에 있음:
     - 범위 내 모든 플레이어/적에게 50 데미지
     - 플레이어에게 1초 스턴
     - 클렌저 사이트가 범위 내이면 75 데미지 (1.5배)
     - 아르마딜로: 스턴 없음, 즉시 기본 폼 복귀

   [6-A-2] 범위 내에 지형지물만 있음 (플레이어/적 없음):
     - 아르마딜로 본인 1.5초 스턴
     - 이후 기본 폼 복귀

6-B. 목표점 도달 (충돌 없이)
   → 돌진 종료, 기본 폼 복귀

7. 스킬 쿨타임 10초 시작
```

---

## 제작 순서

---

## Phase 1: C++ 클래스 — `ADRArmadilloEnemy`

### 1-1. 헤더 파일 생성 (`Source/DaeRune/Public/Character/DRArmadilloEnemy.h`)

```cpp
// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DREnemy.h"
#include "DRArmadilloEnemy.generated.h"

class USphereComponent;

/**
 * 아르마딜로 적: 기본 폼(걷기/근접공격)과 볼 폼(구르기/돌진 스킬) 전환
 */
UCLASS()
class DAERUNE_API ADRArmadilloEnemy : public ADREnemy
{
    GENERATED_BODY()

public:
    ADRArmadilloEnemy();
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    /** Combat Interface Override */
    virtual void MulticastHandleDeath_Implementation(const FVector& DeathImpulse) override;

    // ===== 폼 전환 시스템 =====

    /** 현재 볼 폼 여부 */
    UFUNCTION(BlueprintPure, Category = "Armadillo|Form")
    bool IsBallForm() const { return bIsBallForm; }

    /** 폼 전환 요청 (서버) — 전환 애니메이션 끝에 AnimNotify에서 FinishFormChange 호출 */
    UFUNCTION(BlueprintCallable, Category = "Armadillo|Form")
    void StartFormChange(bool bToBallForm);

    /** 폼 전환 완료 (AnimNotify에서 호출) — 메시 교체 + 상태 갱신 */
    UFUNCTION(BlueprintCallable, Category = "Armadillo|Form")
    void FinishFormChange();

    // ===== 롤링 돌진 스킬 =====

    /** 롤링 돌진 중 여부 */
    UFUNCTION(BlueprintPure, Category = "Armadillo|Roll")
    bool IsRolling() const { return bIsRolling; }

    /** 돌진 시작 (GA에서 호출) */
    UFUNCTION(BlueprintCallable, Category = "Armadillo|Roll")
    void StartRollCharge(FVector TargetLocation);

    /** 돌진 종료 (충돌 또는 목표 도달 시) */
    UFUNCTION(BlueprintCallable, Category = "Armadillo|Roll")
    void StopRollCharge();

    /** 유효한 돌진 타겟 찾기: 3000 내 벽 없는 랜덤 플레이어 */
    UFUNCTION(BlueprintCallable, Category = "Armadillo|Roll")
    AActor* FindRollTarget() const;

    // ===== 볼 폼 메시 참조 =====

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Armadillo|Mesh")
    TObjectPtr<USkeletalMeshComponent> BallFormMesh;

protected:
    virtual void BeginPlay() override;
    virtual void StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount) override;

    // ===== 폼 전환 설정 =====

    /** 볼 폼 전환 중 목표 상태 (true=볼 전환 중, false=기본 전환 중) */
    UPROPERTY(BlueprintReadOnly, Category = "Armadillo|Form")
    bool bPendingBallForm = false;

    /** 볼 폼 상태 (복제) */
    UPROPERTY(ReplicatedUsing = OnRep_BallForm, BlueprintReadOnly, Category = "Armadillo|Form")
    bool bIsBallForm = false;

    UFUNCTION()
    void OnRep_BallForm();

    /** 볼 폼 캡슐 반지름 */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Form")
    float BallFormCapsuleRadius = 40.f;

    /** 볼 폼 캡슐 반높이 */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Form")
    float BallFormCapsuleHalfHeight = 40.f;

    /** 기본 폼 캡슐 반지름 (BeginPlay에서 저장) */
    float DefaultCapsuleRadius;

    /** 기본 폼 캡슐 반높이 (BeginPlay에서 저장) */
    float DefaultCapsuleHalfHeight;

    // ===== 롤링 돌진 설정 =====

    /** 돌진 상태 (복제) */
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Armadillo|Roll")
    bool bIsRolling = false;

    /** 돌진 목표 위치 */
    UPROPERTY(BlueprintReadOnly, Category = "Armadillo|Roll")
    FVector RollTargetLocation;

    /** 돌진 방향 (정규화) */
    UPROPERTY(BlueprintReadOnly, Category = "Armadillo|Roll")
    FVector RollDirection;

    /** 현재 돌진 속도 */
    UPROPERTY(BlueprintReadOnly, Category = "Armadillo|Roll")
    float CurrentRollSpeed = 0.f;

    /** 돌진 초기 속도 */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    float RollInitialSpeed = 200.f;

    /** 돌진 최대 속도 */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    float RollMaxSpeed = 2000.f;

    /** 돌진 가속도 (초당 속도 증가량) */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    float RollAcceleration = 400.f;

    /** 전방 충돌 감지 구 반지름 */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    float RollDetectionRadius = 30.f;

    /** 충돌 효과 적용 구 반지름 */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    float RollImpactRadius = 50.f;

    /** 타겟 검색 최대 거리 */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    float RollTargetSearchRange = 3000.f;

    /** 돌진 데미지 */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    float RollDamage = 50.f;

    /** 클렌저 사이트 데미지 배율 */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    float CleanserDamageMultiplier = 1.5f;

    /** 플레이어 스턴 지속시간 (초) */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    float PlayerStunDuration = 1.0f;

    /** 지형 충돌 시 본인 스턴 지속시간 (초) */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    float SelfStunDuration = 1.5f;

    /** 스킬 쿨타임 (초) */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    float RollSkillCooldown = 10.f;

    /** 스턴 GE 클래스 (플레이어/본인에게 적용) */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    TSubclassOf<UGameplayEffect> RollStunEffectClass;

    /** 롤링 데미지 GE 클래스 */
    UPROPERTY(EditDefaultsOnly, Category = "Armadillo|Roll")
    TSubclassOf<UGameplayEffect> RollDamageEffectClass;

private:
    /** 돌진 중 Tick 처리 (이동 + 충돌 감지) */
    void TickRollCharge(float DeltaTime);

    /** 전방 충돌 감지 (SphereTrace) */
    bool DetectRollCollision(FHitResult& OutHit) const;

    /** 충돌 시 효과 적용 */
    void ApplyRollImpact(const FVector& ImpactLocation);

    /** 메시 가시성 업데이트 */
    void UpdateMeshVisibility();
};
```

### 1-2. 소스 파일 생성 (`Source/DaeRune/Private/Character/DRArmadilloEnemy.cpp`)

구현 핵심 로직:

```cpp
ADRArmadilloEnemy::ADRArmadilloEnemy()
{
    PrimaryActorTick.bCanEverTick = true;

    // 볼 폼 메시 컴포넌트 생성
    BallFormMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("BallFormMesh"));
    BallFormMesh->SetupAttachment(GetRootComponent());
    BallFormMesh->SetVisibility(false); // 기본 폼이 초기 상태
    BallFormMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRArmadilloEnemy::BeginPlay()
{
    Super::BeginPlay();

    // 기본 캡슐 크기 저장
    DefaultCapsuleRadius = GetCapsuleComponent()->GetUnscaledCapsuleRadius();
    DefaultCapsuleHalfHeight = GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight();
}

void ADRArmadilloEnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority() && bIsRolling)
    {
        TickRollCharge(DeltaTime);
    }
}

// ===== 폼 전환 =====

void ADRArmadilloEnemy::StartFormChange(bool bToBallForm)
{
    if (!HasAuthority()) return;
    bPendingBallForm = bToBallForm;
    // 전환 애니메이션은 GA에서 재생 → AnimNotify가 FinishFormChange() 호출
}

void ADRArmadilloEnemy::FinishFormChange()
{
    if (!HasAuthority()) return;
    bIsBallForm = bPendingBallForm;
    OnRep_BallForm(); // 서버 로컬도 갱신

    // 캡슐 크기 조정
    if (bIsBallForm)
    {
        GetCapsuleComponent()->SetCapsuleSize(BallFormCapsuleRadius, BallFormCapsuleHalfHeight);
    }
    else
    {
        GetCapsuleComponent()->SetCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight);
    }
}

void ADRArmadilloEnemy::OnRep_BallForm()
{
    UpdateMeshVisibility();
}

void ADRArmadilloEnemy::UpdateMeshVisibility()
{
    // 기본 메시 = GetMesh() (부모의 SkeletalMeshComponent)
    GetMesh()->SetVisibility(!bIsBallForm);
    BallFormMesh->SetVisibility(bIsBallForm);
}

// ===== 롤링 돌진 =====

AActor* ADRArmadilloEnemy::FindRollTarget() const
{
    // 1. 월드 내 모든 플레이어 수집
    // 2. 거리 3000 이내 필터링
    // 3. LineTrace로 벽 차단 여부 확인 (ECC_WorldStatic 채널)
    // 4. 조건 만족 플레이어 중 랜덤 1명 반환
    // 5. 없으면 nullptr 반환
}

void ADRArmadilloEnemy::StartRollCharge(FVector TargetLocation)
{
    if (!HasAuthority()) return;

    bIsRolling = true;
    RollTargetLocation = TargetLocation;
    RollDirection = (TargetLocation - GetActorLocation()).GetSafeNormal2D();
    CurrentRollSpeed = RollInitialSpeed;

    // AI 이동 중지 (CharacterMovement는 직접 제어)
    if (DRAIController)
    {
        DRAIController->StopMovement();
    }
}

void ADRArmadilloEnemy::StopRollCharge()
{
    if (!HasAuthority()) return;

    bIsRolling = false;
    CurrentRollSpeed = 0.f;
    GetCharacterMovement()->Velocity = FVector::ZeroVector;
}

void ADRArmadilloEnemy::TickRollCharge(float DeltaTime)
{
    // 1. 가속
    CurrentRollSpeed = FMath::Min(CurrentRollSpeed + RollAcceleration * DeltaTime, RollMaxSpeed);

    // 2. 이동
    FVector NewVelocity = RollDirection * CurrentRollSpeed;
    GetCharacterMovement()->Velocity = FVector(NewVelocity.X, NewVelocity.Y, GetCharacterMovement()->Velocity.Z);

    // 3. 목표점 도달 확인
    FVector ToTarget = RollTargetLocation - GetActorLocation();
    ToTarget.Z = 0;
    float DotProduct = FVector::DotProduct(ToTarget.GetSafeNormal(), RollDirection);
    if (DotProduct <= 0.f) // 목표점을 지나침
    {
        StopRollCharge();
        // 기본 폼으로 복귀 (GA에서 처리)
        return;
    }

    // 4. 전방 충돌 감지
    FHitResult HitResult;
    if (DetectRollCollision(HitResult))
    {
        ApplyRollImpact(HitResult.ImpactPoint);
        StopRollCharge();
    }
}

bool ADRArmadilloEnemy::DetectRollCollision(FHitResult& OutHit) const
{
    FVector Start = GetActorLocation();
    FVector End = Start + RollDirection * CurrentRollSpeed * GetWorld()->GetDeltaSeconds();

    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    // SphereTrace 반지름 30으로 전방 감지
    return GetWorld()->SweepSingleByChannel(
        OutHit, Start, End, FQuat::Identity,
        ECC_Pawn, // 또는 커스텀 채널
        FCollisionShape::MakeSphere(RollDetectionRadius),
        Params
    );
}

void ADRArmadilloEnemy::ApplyRollImpact(const FVector& ImpactLocation)
{
    // 1. 충돌 지점에서 반지름 50의 구 오버랩 검사
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByChannel(
        Overlaps, ImpactLocation, FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(RollImpactRadius),
        Params
    );

    bool bHitPlayerOrEnemy = false;

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* HitActor = Overlap.GetActor();
        if (!HitActor) continue;

        // 플레이어 확인
        if (ADRCharacter* Player = Cast<ADRCharacter>(HitActor))
        {
            bHitPlayerOrEnemy = true;
            // 50 데미지 적용 (GE)
            // 1초 스턴 적용 (GE)
        }
        // 다른 적 확인
        else if (ADREnemy* Enemy = Cast<ADREnemy>(HitActor))
        {
            bHitPlayerOrEnemy = true;
            // 50 데미지 적용 (GE)
        }
        // 클렌저 사이트 확인
        else if (ADRCleanserSite* Cleanser = Cast<ADRCleanserSite>(HitActor))
        {
            // 75 데미지 (50 * 1.5) 적용 (GE)
        }
    }

    // 2. 범위 내에 플레이어/적이 없고 지형지물만 있으면 → 본인 스턴
    if (!bHitPlayerOrEnemy)
    {
        // 본인에게 1.5초 스턴 GE 적용
    }
}
```

---

## Phase 2: 게임플레이 태그 추가

### 2-1. `DRGameplayTags.h`에 태그 추가

```cpp
// 아르마딜로 태그
FGameplayTag Abilities_Armadillo_BasicAttack;
FGameplayTag Abilities_Armadillo_RollCharge;
FGameplayTag Cooldown_Armadillo_RollCharge;
FGameplayTag State_BallForm;
```

### 2-2. `DRGameplayTags.cpp`의 `InitializeNativeGameplayTags()`에 등록

```cpp
GameplayTags.Abilities_Armadillo_BasicAttack = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("Abilities.Armadillo.BasicAttack"),
    FString("아르마딜로 기본 근접 공격"));

GameplayTags.Abilities_Armadillo_RollCharge = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("Abilities.Armadillo.RollCharge"),
    FString("아르마딜로 볼 폼 돌진 스킬"));

GameplayTags.Cooldown_Armadillo_RollCharge = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("Cooldown.Armadillo.RollCharge"),
    FString("아르마딜로 돌진 스킬 쿨타임"));

GameplayTags.State_BallForm = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("State.BallForm"),
    FString("아르마딜로 볼 폼 상태"));
```

---

## Phase 3: 게임플레이 이펙트 (GE) — Blueprint

### 3-1. `GE_PrimaryAttributes_Armadillo` (적 기본 스탯)

- **위치**: `Content/Blueprints/AbilitySystem/GE/Enemy/GE_PrimaryAttributes_Armadillo`
- **부모 클래스**: `UGameplayEffect`
- **Duration**: Instant
- **설정**:
  - MaxHealth: (밸런싱 수치, 예: 200)
  - MoveSpeed: 250 (기본 폼 걷기 속도)
- **참고**: 기존 `GE_PrimaryAttributes_Enemy`를 복제하여 수치만 조정

### 3-2. `GE_ArmadilloRollDamage` (돌진 데미지)

- **위치**: `Content/Blueprints/AbilitySystem/GE/Enemy/GE_ArmadilloRollDamage`
- **Duration**: Instant
- **Execution Calculation**: `ExecCalc_Damage` (기존 것 재사용)
- **Set By Caller**: `Damage` 태그로 데미지 값 전달 (기본 50, 클렌저는 75)
- **Damage Type**: `Damage.Physical`

### 3-3. `GE_ArmadilloRollStun_Player` (플레이어 스턴)

- **위치**: `Content/Blueprints/AbilitySystem/GE/Enemy/GE_ArmadilloRollStun_Player`
- **Duration**: Has Duration = 1.0초
- **Granted Tags**: `Debuff.Stun`
- **효과**: 플레이어 이동 불가 + 공격 불가

### 3-4. `GE_ArmadilloRollStun_Self` (본인 스턴 — 지형 충돌 시)

- **위치**: `Content/Blueprints/AbilitySystem/GE/Enemy/GE_ArmadilloRollStun_Self`
- **Duration**: Has Duration = 1.5초
- **Granted Tags**: `Debuff.Stun`
- **효과**: 아르마딜로 이동 불가 + 공격 불가

### 3-5. `GE_Cooldown_ArmadilloRollCharge` (돌진 쿨타임)

- **위치**: `Content/Blueprints/AbilitySystem/GE/Enemy/GE_Cooldown_ArmadilloRollCharge`
- **Duration**: Has Duration = 10.0초
- **Granted Tags**: `Cooldown.Armadillo.RollCharge`
- **용도**: GA의 `CooldownGameplayEffectClass`에 설정

---

## Phase 4: 게임플레이 어빌리티 (GA) — Blueprint

### 4-1. `GA_ArmadilloBite` (기본 폼 근접 공격)

- **위치**: `Content/Blueprints/AbilitySystem/GA/Enemy/GA_ArmadilloBite`
- **부모 클래스**: `UDRMeleeAttack` (C++)
- **제작 방법**: 기존 `GA_DogBite`를 **복제(Duplicate)**하여 생성
- **AbilityTag**: `Abilities.Armadillo.BasicAttack`
- **설정**:
  - DamageEffectClass: 기존 적 데미지 GE 또는 새로 생성
  - DamageType: `Damage.Physical` 또는 `Damage.Bite`
  - Damage: (밸런싱 수치)
  - 공격 몽타주: BasicForm의 BasicAttack 애니메이션
- **로직**: `GA_DogBite`와 동일 (몽타주 재생 → AnimNotify에서 Trace/데미지 적용)

### 4-2. `GA_ArmadilloRollCharge` (볼 폼 돌진 스킬)

- **위치**: `Content/Blueprints/AbilitySystem/GA/Enemy/GA_ArmadilloRollCharge`
- **부모 클래스**: `UDRDamageGameplayAbility` (C++)
- **AbilityTag**: `Abilities.Armadillo.RollCharge`
- **CooldownGameplayEffectClass**: `GE_Cooldown_ArmadilloRollCharge`
- **Cooldown Tags**: `Cooldown.Armadillo.RollCharge`

**GA Blueprint 로직 (ActivateAbility)**:

```
[ActivateAbility]
    │
    ├─ 1. FindRollTarget() 호출 (C++ 함수)
    │     └─ 결과 null이면 → EndAbility (취소)
    │
    ├─ 2. StartFormChange(true) 호출 → Basic→Ball 전환 시작
    │     └─ BasicForm에서 FormChange_BasicToBall 몽타주 재생
    │     └─ AnimNotify → FinishFormChange() → 메시 교체
    │
    ├─ 3. Wait for AnimNotify (FinishFormChange 완료 대기)
    │
    ├─ 4. 타겟 위치 저장 (FindRollTarget의 GetActorLocation)
    │
    ├─ 5. StartRollCharge(TargetLocation) 호출
    │     └─ C++의 Tick에서 자동으로 이동 + 충돌 감지 수행
    │
    ├─ 6. Wait until bIsRolling == false (충돌 또는 도달 시)
    │
    ├─ 7. StartFormChange(false) 호출 → Ball→Basic 전환 시작
    │     └─ BallForm에서 FormChange_BallToBasic 몽타주 재생
    │     └─ AnimNotify → FinishFormChange() → 메시 교체
    │
    ├─ 8. Wait for AnimNotify (FinishFormChange 완료 대기)
    │
    └─ 9. CommitAbilityCooldown() → 쿨타임 10초 시작
         └─ EndAbility
```

**충돌 시 효과 적용은 C++의 `ApplyRollImpact()`에서 처리**:
- GA에서 GE 클래스를 C++ 프로퍼티에 세팅
- C++에서 ASC를 통해 GE 적용

---

## Phase 5: 애니메이션 블루프린트 (ABP) — 2개

### 5-1. `ABP_Armadillo_Basic` (기본 폼)

- **위치**: `Content/Blueprints/Character/Enemy/Armadillo/ABP_Armadillo_Basic`
- **Skeleton**: BasicForm의 Skeleton 사용

**State Machine 구조**:
```
[Entry] → Locomotion
              │
              ├─ (bIsStunned == true) → Stun (Loop)
              │     └─ (bIsStunned == false) → Locomotion
              │
              ├─ (bIsDead == true) → Death
              │
              └─ Locomotion 내부:
                    ├─ Idle (Speed < 10)
                    └─ Walk (Speed >= 10)
                    (BlendSpace BS_IdleWalk로 통합 가능)
```

**사용 애니메이션**:
- `Idle` → Locomotion 대기
- `Walk` → Locomotion 이동
- `Stun` → 스턴 상태 (Loop 재생)
- `Death` → 사망
- `FormChange_BasicToBall` → 몽타주로 재생 (GA에서 트리거)
- `FormChange_BallToBasic` → 몽타주로 재생 (GA에서 트리거)
- `BasicAttack` → 몽타주로 재생 (GA_ArmadilloBite에서 트리거)
- `HitReact 1/2/3` → 몽타주로 재생 (GA_HitReact에서 트리거)

**AnimNotify 설정**:
- `BasicAttack` 몽타주: 데미지 타이밍에 `AN_AttackTrace` 또는 커스텀 Notify
- `FormChange_BasicToBall` 몽타주: 전환 완료 시점에 `AN_FinishFormChange` Notify
- `FormChange_BallToBasic` 몽타주: 전환 완료 시점에 `AN_FinishFormChange` Notify

### 5-2. `ABP_Armadillo_Ball` (볼 폼)

- **위치**: `Content/Blueprints/Character/Enemy/Armadillo/ABP_Armadillo_Ball`
- **Skeleton**: BallForm(RollForm)의 Skeleton 사용

**State Machine 구조**:
```
[Entry] → Idle
              │
              ├─ (bIsRolling == true) → Roll (Loop)
              │     └─ (bIsRolling == false) → Idle
              │
              └─ (bIsStunned == true) → Idle (볼 폼에서 스턴 = Idle)
```

**사용 애니메이션**:
- `Idle` → 볼 대기 상태
- `Roll` → 구르기 (Loop, 돌진 중 재생)
- `FormChange_BasicToBall` → 몽타주 (전환 시작 시 재생)
- `FormChange_BallToBasic` → 몽타주 (전환 종료 시 재생)

**참고**: 볼 폼에서는 HitReact/Death 애니메이션이 없으므로, 볼 폼 중 사망 시 기본 폼으로 전환 후 Death 재생.

---

## Phase 6: 애니메이션 몽타주 생성

### 6-1. 기본 폼 몽타주

| 몽타주 이름 | 원본 애니메이션 | 슬롯 | AnimNotify |
|------------|----------------|------|------------|
| `AM_Armadillo_BasicAttack` | BasicAttack | DefaultSlot | 데미지 타이밍에 Notify 추가 |
| `AM_Armadillo_HitReact1` | HitReact1 | DefaultSlot | - |
| `AM_Armadillo_HitReact2` | HitReact2 | DefaultSlot | - |
| `AM_Armadillo_HitReact3` | HitReact3 | DefaultSlot | - |
| `AM_Armadillo_FormChange_BtoD` | FormChange_BasicToBall | DefaultSlot | 끝에 `AN_FinishFormChange` |
| `AM_Armadillo_FormChange_DtoB` | FormChange_BallToBasic | DefaultSlot | 끝에 `AN_FinishFormChange` |

### 6-2. 볼 폼 몽타주

| 몽타주 이름 | 원본 애니메이션 | 슬롯 | AnimNotify |
|------------|----------------|------|------------|
| `AM_ArmadilloBall_FormChange_BtoD` | FormChange_BasicToBall | DefaultSlot | 끝에 `AN_FinishFormChange` |
| `AM_ArmadilloBall_FormChange_DtoB` | FormChange_BallToBasic | DefaultSlot | 끝에 `AN_FinishFormChange` |

---

## Phase 7: AnimNotify — `AN_FinishFormChange`

### 7-1. AnimNotify 생성

- **위치**: `Content/Blueprints/Character/Enemy/Armadillo/AN_FinishFormChange`
- **부모 클래스**: `UAnimNotify` (Blueprint)
- **로직**:
  ```
  Received_Notify →
    Get Owning Actor →
    Cast to ADRArmadilloEnemy →
    Call FinishFormChange()
  ```

이 Notify는 기본 폼과 볼 폼 양쪽의 FormChange 몽타주에 배치한다.

---

## Phase 8: Behavior Tree (BT) — Blueprint

### 8-1. Blackboard 에셋: `BB_Armadillo`

- **위치**: `Content/Blueprints/AI/Armadillo/BB_Armadillo`
- **기존 키 상속** (DRBlackboardKeys에 정의된 것들 모두 포함):
  - `HitReacting` (Bool)
  - `Dead` (Bool)
  - `Stunned` (Bool)
  - `HomeLocation` (Vector)
  - `FirstAttacker` (Object)
  - `HasFirstAttacker` (Bool)
  - `TargetToFollow` (Object)
  - `IsEnraged` (Bool)
  - `AttackSpeed` (Float)
  - `RangedAttacker` (Bool) → **false** (근접 공격자)

- **아르마딜로 전용 키 추가**:
  - `IsBallForm` (Bool) — 현재 폼 상태
  - `RollSkillReady` (Bool) — 쿨타임 완료 여부
  - `RollTarget` (Object) — 돌진 타겟 플레이어

### 8-2. Behavior Tree: `BT_EnemyBehaviorTree_Armadillo`

- **위치**: `Content/Blueprints/AI/Armadillo/BT_EnemyBehaviorTree_Armadillo`

**BT 구조**:

```
Root (Selector)
│
├─ [1] 사망 체크 (Sequence)
│     ├─ Decorator: BB "Dead" == true
│     └─ Task: BTT_StopBehavior
│
├─ [2] 스턴 체크 (Sequence)
│     ├─ Decorator: BB "Stunned" == true
│     └─ Task: Wait (스턴 해제될 때까지)
│
├─ [3] 히트 리액트 (Sequence)
│     ├─ Decorator: BB "HitReacting" == true
│     └─ Task: Wait (히트 리액트 종료될 때까지)
│
├─ [4] 롤링 스킬 분기 (Sequence)
│     ├─ Decorator: BB "RollSkillReady" == true
│     ├─ Decorator: BB "TargetToFollow" IsSet
│     ├─ Decorator: Target이 플레이어인지 확인
│     ├─ Task: BTT_ArmadilloRollAttack (★ 신규)
│     │     └─ TryActivateAbilitiesByTag("Abilities.Armadillo.RollCharge")
│     └─ (스킬 실행 후 쿨타임 동안 RollSkillReady = false)
│
├─ [5] 기본 공격 분기 (Sequence)
│     ├─ Decorator: BB "TargetToFollow" IsSet
│     ├─ Decorator: 공격 범위 내 확인
│     ├─ Task: BTT_RotateToFaceTarget
│     └─ Task: BTT_Attack_Armadillo (★ 신규)
│           └─ TryActivateAbilitiesByTag("Abilities.Armadillo.BasicAttack")
│
├─ [6] 추적 (Sequence)
│     ├─ Decorator: BB "TargetToFollow" IsSet
│     └─ Task: MoveTo (TargetToFollow)
│
└─ [7] 순찰 (Sequence)
      └─ Task: MoveTo (HomeLocation 주변 랜덤)
```

### 8-3. BT Service: `BTS_CheckRollSkillReady`

- **위치**: `Content/Blueprints/AI/Armadillo/BTS_CheckRollSkillReady`
- **기능**: 매 Tick마다 ASC에서 `Cooldown.Armadillo.RollCharge` 태그 여부를 확인
  - 쿨타임 태그 없음 → `RollSkillReady = true`
  - 쿨타임 태그 있음 → `RollSkillReady = false`
- **Tick Interval**: 0.5초

### 8-4. BT Service: `BTS_FindNearestPlayer_Armadillo`

- **기존 `BTS_FindNearestPlayer`를 복제**하여 사용
- 또는 기존 것을 그대로 재사용 (동작이 동일하므로)

### 8-5. BT Task: `BTT_Attack_Armadillo` (기본 공격)

- **위치**: `Content/Blueprints/AI/Armadillo/BTT_Attack_Armadillo`
- **기존 `BTT_Attack_Dog`를 복제**
- `TryActivateAbilitiesByTag`로 `Abilities.Armadillo.BasicAttack` 활성화
- **로직**: 타겟 방향 회전 → 어빌리티 활성화 → 어빌리티 종료 대기 → Task 완료

### 8-6. BT Task: `BTT_ArmadilloRollAttack` (돌진 스킬)

- **위치**: `Content/Blueprints/AI/Armadillo/BTT_ArmadilloRollAttack`
- **로직**:
  ```
  Execute:
    1. TryActivateAbilitiesByTag("Abilities.Armadillo.RollCharge")
    2. Wait for ability to end
    3. Set BB "RollSkillReady" = false
    4. Return Success/Failure
  ```

---

## Phase 9: Blueprint 캐릭터 — `BP_Armadillo`

### 9-1. Blueprint 생성

- **위치**: `Content/Blueprints/Character/Enemy/Armadillo/BP_Armadillo`
- **부모 클래스**: `ADRArmadilloEnemy` (C++)

### 9-2. 컴포넌트 설정

| 컴포넌트 | 설정 |
|----------|------|
| **Mesh (기본 폼)** | SkeletalMesh = `Armadillo_v0_1_2`, AnimClass = `ABP_Armadillo_Basic` |
| **BallFormMesh** | SkeletalMesh = `Armadillo_Ball_v0_1_0`, AnimClass = `ABP_Armadillo_Ball`, Visibility = Hidden |
| **CapsuleComponent** | 아르마딜로 체형에 맞게 조정 (예: Radius=34, HalfHeight=60) |

### 9-3. 프로퍼티 설정

| 프로퍼티 | 값 | 비고 |
|---------|-----|------|
| CharacterClass | Warrior | 근접 공격 기반 |
| Level | 1 (기본) | 스폰 시 조정 |
| BehaviorTree | `BT_EnemyBehaviorTree_Armadillo` | |
| BaseWalkSpeed | 250 | 기본 폼 이동 속도 |
| RollInitialSpeed | 200 | 돌진 초기 속도 |
| RollMaxSpeed | 2000 | 돌진 최대 속도 |
| RollAcceleration | 400 | 초당 가속량 |
| RollDamage | 50 | 돌진 데미지 |
| CleanserDamageMultiplier | 1.5 | 클렌저 추가 데미지 |
| PlayerStunDuration | 1.0 | 초 |
| SelfStunDuration | 1.5 | 초 |
| RollSkillCooldown | 10.0 | 초 |
| RollTargetSearchRange | 3000 | 유닛 |
| RollDetectionRadius | 30 | 전방 감지 구 반지름 |
| RollImpactRadius | 50 | 충돌 효과 구 반지름 |
| RollStunEffectClass | GE별로 할당 | |
| RollDamageEffectClass | `GE_ArmadilloRollDamage` | |
| WaterReductionEffectClass | 기존 적 GE | |
| WaterGrantEffectClass | 기존 적 GE | |
| EnrageMovementSpeedGE | 기존 적 GE | |

### 9-4. 공격 몽타주 설정 (AttackMontages 배열)

```
AttackMontages[0]:
  Montage = AM_Armadillo_BasicAttack
  MontageTag = Montage.Attack.1
  SocketTag = CombatSocket.RightHand (또는 Tail)
```

### 9-5. 히트 리액트 몽타주 설정 (HitReactMontages 배열)

```
HitReactMontages[0] = AM_Armadillo_HitReact1
HitReactMontages[1] = AM_Armadillo_HitReact2
HitReactMontages[2] = AM_Armadillo_HitReact3
```

---

## Phase 10: CharacterClassInfo 데이터 에셋 업데이트

### 10-1. 기존 `DA_CharacterClassInfo` 업데이트

**새 항목 추가 또는 기존 Warrior 항목에 아르마딜로 전용 설정**:

아르마딜로는 `ECharacterClass::Warrior`를 사용하므로, `BP_Armadillo` Blueprint에서 직접 StartupAbilities를 설정하거나, 별도의 CharacterClass enum 값을 추가할 수 있다.

**추천 방법**: `BP_Armadillo`에서 직접 StartupAbilities 오버라이드
- `GA_ArmadilloBite` (기본 공격)
- `GA_ArmadilloRollCharge` (돌진 스킬)
- `GA_HitReact` (공용 히트 리액트)

---

## Phase 11: 블렌드 스페이스 생성

### 11-1. `BS_Armadillo_IdleWalk` (기본 폼)

- **위치**: `Content/Blueprints/Character/Enemy/Armadillo/BS_Armadillo_IdleWalk`
- **축**: Speed (0 ~ 300)
- **포인트**:
  - Speed = 0: `Idle`
  - Speed = 250: `Walk`

---

## Phase 12: 멀티플레이어 리플리케이션 확인

### 리플리케이트 프로퍼티

| 프로퍼티 | 리플리케이션 | RepNotify |
|---------|-------------|-----------|
| `bIsBallForm` | `DOREPLIFETIME` | `OnRep_BallForm` (메시 가시성 갱신) |
| `bIsRolling` | `DOREPLIFETIME` | 없음 (시각적으로는 이동으로 표현) |
| `bIsStunned` | 부모에서 처리 | `OnRep_Stunned` (부모) |
| `bIsBurned` | 부모에서 처리 | `OnRep_Burned` (부모) |

### 서버 권한 로직

- 폼 전환 결정: **서버**
- 돌진 이동/충돌: **서버** (Tick에서 처리)
- 데미지/스턴 GE 적용: **서버**
- 타겟 선택: **서버**

### 클라이언트 처리

- `OnRep_BallForm()`으로 메시 교체 동기화
- 이동은 CharacterMovement의 기본 리플리케이션으로 처리
- 스턴 VFX는 `OnRep_Stunned()`로 동기화

---

## Phase 13: 최종 제작 체크리스트 (순서대로)

### C++ 작업
- [ ] 1. `DRGameplayTags.h/.cpp`에 아르마딜로 관련 태그 추가
- [ ] 2. `DRArmadilloEnemy.h` 헤더 파일 생성
- [ ] 3. `DRArmadilloEnemy.cpp` 소스 파일 생성
- [ ] 4. `DaeRune.Build.cs`에 필요 모듈 확인 (보통 추가 불필요)
- [ ] 5. 컴파일 및 오류 수정

### Blueprint — GE 작업
- [ ] 6. `GE_PrimaryAttributes_Armadillo` 생성
- [ ] 7. `GE_ArmadilloRollDamage` 생성
- [ ] 8. `GE_ArmadilloRollStun_Player` 생성 (1초)
- [ ] 9. `GE_ArmadilloRollStun_Self` 생성 (1.5초)
- [ ] 10. `GE_Cooldown_ArmadilloRollCharge` 생성 (10초)

### Blueprint — 애니메이션 작업
- [ ] 11. `BS_Armadillo_IdleWalk` 블렌드 스페이스 생성
- [ ] 12. 기본 폼 몽타주 6개 생성 (BasicAttack, HitReact1~3, FormChange x2)
- [ ] 13. 볼 폼 몽타주 2개 생성 (FormChange x2)
- [ ] 14. `AN_FinishFormChange` AnimNotify 생성
- [ ] 15. `ABP_Armadillo_Basic` AnimBP 생성
- [ ] 16. `ABP_Armadillo_Ball` AnimBP 생성

### Blueprint — GA 작업
- [ ] 17. `GA_ArmadilloBite` 생성 (GA_DogBite 복제)
- [ ] 18. `GA_ArmadilloRollCharge` 생성

### Blueprint — AI 작업
- [ ] 19. `BB_Armadillo` Blackboard 생성
- [ ] 20. `BTS_CheckRollSkillReady` BT Service 생성
- [ ] 21. `BTT_Attack_Armadillo` BT Task 생성 (BTT_Attack_Dog 복제)
- [ ] 22. `BTT_ArmadilloRollAttack` BT Task 생성
- [ ] 23. `BT_EnemyBehaviorTree_Armadillo` Behavior Tree 생성

### Blueprint — 캐릭터 작업
- [ ] 24. `BP_Armadillo` Blueprint 생성
- [ ] 25. 컴포넌트 설정 (메시, AnimBP, 캡슐 등)
- [ ] 26. 프로퍼티 설정 (데미지, 속도, 쿨타임 등)
- [ ] 27. CharacterClassInfo에 StartupAbilities 설정

### 테스트
- [ ] 28. 에디터에서 스폰 후 기본 폼 이동/대기 확인
- [ ] 29. 기본 공격 (근접) 동작 확인
- [ ] 30. 폼 전환 (Basic→Ball→Basic) 시각적 확인
- [ ] 31. 롤링 돌진 — 플레이어 타겟팅 + 일직선 이동 확인
- [ ] 32. 롤링 돌진 — 가속도 동작 확인
- [ ] 33. 롤링 돌진 — 플레이어/적 충돌 시 데미지+스턴 확인
- [ ] 34. 롤링 돌진 — 지형 충돌 시 본인 스턴 확인
- [ ] 35. 롤링 돌진 — 클렌저 사이트 충돌 시 1.5배 데미지 확인
- [ ] 36. 롤링 돌진 — 벽 뒤 플레이어 미타겟팅 확인
- [ ] 37. 쿨타임 10초 동작 확인
- [ ] 38. 사망 처리 확인
- [ ] 39. 멀티플레이어 동기화 테스트 (폼 전환, 돌진, 스턴)
- [ ] 40. Phase 1/3 스폰 통합 테스트

---

## 주의사항 및 엣지 케이스

### 1. 돌진 중 사망 처리
- 돌진 중(`bIsRolling == true`) 사망 시:
  1. `StopRollCharge()` 즉시 호출
  2. 볼 폼이면 기본 폼으로 전환 (메시 교체만, 애니메이션 없이)
  3. 기본 폼의 Death 애니메이션 재생

### 2. 돌진 중 스턴 (외부 요인)
- 플레이어의 스턴 공격으로 돌진 중 스턴될 경우:
  1. `StunTagChanged`에서 `bIsRolling`이면 `StopRollCharge()` 호출
  2. GA_ArmadilloRollCharge를 CancelAbility
  3. 볼 폼에서 스턴 → 기본 폼 전환 후 스턴 상태 진입

### 3. 폼 전환 중 피격
- FormChange 애니메이션 재생 중 피격 시:
  - HitReact는 무시 (전환 애니메이션 우선)
  - 데미지는 정상 적용
  - 이를 위해 FormChange 몽타주에 `Effects.CannotAttack` 태그를 부여하지 않되, HitReact GA의 `BlockAbilitiesWithTag`에 폼 전환 관련 태그 추가

### 4. 목표점 도달 후 복귀
- 돌진이 충돌 없이 목표점에 도달하면:
  - 즉시 정지 (데미지/스턴 없음)
  - Ball → Basic 폼 전환
  - BT로 제어권 반환

### 5. FindRollTarget 실패
- 3000 내에 벽 없는 플레이어가 없으면:
  - GA 즉시 취소
  - 폼 전환 없이 기본 폼 유지
  - 쿨타임 적용하지 않음 (재시도 가능)
