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
1. 현재 폼에서 전환 몽타주 재생 (예: BasicForm의 FormChange_BasicToBall)
2. AnimNotify(AN_FinishFormChange) 발동
3. 현재 폼 메시 Hidden + 새 폼 메시 Visible + 캡슐 크기 조정 + `bIsBallForm` 갱신
4. 새 폼에서 같은 전환의 이어지는 몽타주 재생 (예: BallForm의 FormChange_BasicToBall)
5. 몽타주 완료 후 다음 행동으로 진행

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
2. ★ 타겟 선택 (폼 전환 전에 먼저 수행):
   - 자신으로부터 3000 거리 내 플레이어 수집
   - LineTrace로 각 플레이어와 사이에 벽 차단 여부 확인 (ECC_WorldStatic)
   - 조건 만족 플레이어가 없으면 → 스킬 취소 (GA 즉시 EndAbility)
   - 폼 전환 없이 기본 폼 유지, 쿨타임 미적용 (재시도 가능)
   ↓
3. 조건 만족 플레이어 중 랜덤 1명 선택
   - 타겟팅 시점의 플레이어 위치 저장 (고정 목표점)
   ↓
4. 폼 전환: Basic → Ball (양쪽 FormChange 애니메이션)
   - BasicForm에서 FormChange_BasicToBall 몽타주 재생
   - AnimNotify(AN_FinishFormChange) → FinishFormChange() → 메시 교체
   - BallForm에서 FormChange_BasicToBall 몽타주 이어서 재생
   - 몽타주 완료 대기
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

**핵심 변경점**: 타겟 유효성 검사를 폼 전환 **이전**에 수행한다. 이렇게 하면 유효한 타겟이 없을 때 불필요한 폼 전환 애니메이션이 재생되지 않고, 기본 폼을 유지한 채 즉시 다른 행동(근접 공격, 추적 등)으로 전환할 수 있다.

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
 * 롤링 충돌 결과 데이터 — C++에서 충돌 감지 후 GA(Blueprint)에 전달
 * GA가 이 데이터를 받아 데미지/스턴 등 효과를 직접 처리한다.
 */
USTRUCT(BlueprintType)
struct FRollImpactResult
{
    GENERATED_BODY()

    /** 충돌 지점 (효과 범위의 중심) */
    UPROPERTY(BlueprintReadOnly)
    FVector ImpactLocation = FVector::ZeroVector;

    /** 충돌 범위(반지름 50) 내 액터 목록 (자기 자신 제외) */
    UPROPERTY(BlueprintReadOnly)
    TArray<AActor*> HitActors;

    /** 범위 내에 플레이어 또는 다른 적이 있었는지 (false = 지형지물만 충돌) */
    UPROPERTY(BlueprintReadOnly)
    bool bHitPlayerOrEnemy = false;
};

/** 롤링 충돌 발생 시 GA에 알리는 델리게이트 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRollImpact, const FRollImpactResult&, ImpactResult);

/** 롤링이 충돌 없이 목표점에 도달했을 때 GA에 알리는 델리게이트 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRollReachedTarget);

/**
 * 아르마딜로 적: 기본 폼(걷기/근접공격)과 볼 폼(구르기/돌진 스킬) 전환
 *
 * C++은 폼 전환, 돌진 이동, 충돌 감지만 담당한다.
 * 데미지/스턴 등 효과 적용은 GA Blueprint(DRDamageGameplayAbility)에서 처리한다.
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

    // ===== 롤링 충돌 이벤트 (GA가 바인딩) =====

    /** 충돌 발생 시 브로드캐스트 — GA가 이 이벤트를 받아 데미지/스턴 처리 */
    UPROPERTY(BlueprintAssignable, Category = "Armadillo|Roll")
    FOnRollImpact OnRollImpact;

    /** 충돌 없이 목표점 도달 시 브로드캐스트 — GA가 폼 복귀 처리 */
    UPROPERTY(BlueprintAssignable, Category = "Armadillo|Roll")
    FOnRollReachedTarget OnRollReachedTarget;

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

private:
    /** 돌진 중 Tick 처리 (이동 + 충돌 감지) */
    void TickRollCharge(float DeltaTime);

    /** 전방 충돌 감지 (SphereTrace) */
    bool DetectRollCollision(FHitResult& OutHit) const;

    /**
     * 충돌 시 범위 내 액터를 수집하여 FRollImpactResult를 구성하고
     * OnRollImpact 델리게이트를 브로드캐스트한다.
     * ※ 데미지/스턴 적용은 하지 않음 — GA가 델리게이트를 받아 처리
     */
    void BroadcastRollImpact(const FVector& ImpactLocation);

    /** 메시 가시성 업데이트 */
    void UpdateMeshVisibility();
};
```

**이전 설계와의 차이점**:
- `RollDamage`, `CleanserDamageMultiplier`, `PlayerStunDuration`, `SelfStunDuration`, `RollSkillCooldown`, `RollStunEffectClass`, `RollDamageEffectClass` 프로퍼티 **제거** — 이 값들은 GA Blueprint의 `DRDamageGameplayAbility` 프로퍼티(`Damage`, `DamageEffectClass` 등)로 설정
- `ApplyRollImpact()` → `BroadcastRollImpact()`로 변경 — 데미지를 직접 적용하지 않고 충돌 결과만 델리게이트로 전달
- `FRollImpactResult` 구조체 추가 — 충돌 위치, 범위 내 액터 목록, 플레이어/적 존재 여부를 GA에 전달
- `FOnRollImpact`, `FOnRollReachedTarget` 델리게이트 추가 — GA가 바인딩하여 충돌/도달 이벤트를 수신

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
    GetCharacterMovement()->Velocity = FVector(NewVelocity.X, NewVelocity.Y,
        GetCharacterMovement()->Velocity.Z);

    // 3. 목표점 도달 확인
    FVector ToTarget = RollTargetLocation - GetActorLocation();
    ToTarget.Z = 0;
    float Dot = FVector::DotProduct(ToTarget.GetSafeNormal(), RollDirection);
    if (Dot <= 0.f) // 목표점을 지나침
    {
        StopRollCharge();
        // ★ GA에 목표 도달 알림 → GA가 폼 복귀 처리
        OnRollReachedTarget.Broadcast();
        return;
    }

    // 4. 전방 충돌 감지
    FHitResult HitResult;
    if (DetectRollCollision(HitResult))
    {
        StopRollCharge();
        // ★ 충돌 결과 수집 후 GA에 알림 → GA가 데미지/스턴 처리
        BroadcastRollImpact(HitResult.ImpactPoint);
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

// ★ 핵심 변경: 데미지/스턴을 직접 적용하지 않고, 충돌 데이터만 수집하여 GA에 전달
void ADRArmadilloEnemy::BroadcastRollImpact(const FVector& ImpactLocation)
{
    FRollImpactResult Result;
    Result.ImpactLocation = ImpactLocation;
    Result.bHitPlayerOrEnemy = false;

    // 충돌 지점에서 반지름 50의 구 오버랩 검사
    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);

    GetWorld()->OverlapMultiByChannel(
        Overlaps, ImpactLocation, FQuat::Identity,
        ECC_Pawn,
        FCollisionShape::MakeSphere(RollImpactRadius),
        Params
    );

    for (const FOverlapResult& Overlap : Overlaps)
    {
        AActor* HitActor = Overlap.GetActor();
        if (!HitActor) continue;

        Result.HitActors.Add(HitActor);

        // 플레이어 또는 다른 적이 있는지만 판별 (데미지 적용은 GA에서)
        if (HitActor->IsA(ADRCharacter::StaticClass()) ||
            HitActor->IsA(ADREnemy::StaticClass()))
        {
            Result.bHitPlayerOrEnemy = true;
        }
    }

    // GA에 충돌 결과 전달 → GA가 데미지/스턴 처리
    OnRollImpact.Broadcast(Result);
}
```

**C++의 역할 요약** (데미지 로직 제거 후):
| 역할 | 함수 |
|------|------|
| 폼 전환 메시 교체 | `StartFormChange`, `FinishFormChange`, `UpdateMeshVisibility` |
| 돌진 이동 물리 | `StartRollCharge`, `StopRollCharge`, `TickRollCharge` |
| 전방 충돌 감지 | `DetectRollCollision` (SphereTrace) |
| 충돌 범위 수집 | `BroadcastRollImpact` (OverlapMulti → 액터 목록 수집) |
| GA에 이벤트 전달 | `OnRollImpact.Broadcast`, `OnRollReachedTarget.Broadcast` |

**C++이 하지 않는 것**:
- ❌ GE를 만들거나 적용하지 않음
- ❌ 데미지 계산하지 않음
- ❌ 스턴 적용하지 않음
- ❌ 클렌저 사이트 배율 계산하지 않음

---

## Phase 2: 게임플레이 태그 추가

### 2-1. `DRGameplayTags.h`에 태그 추가

```cpp
// 아르마딜로 태그
FGameplayTag Cooldown_Armadillo_RollCharge;
FGameplayTag State_BallForm;
```

### 2-2. `DRGameplayTags.cpp`의 `InitializeNativeGameplayTags()`에 등록

```cpp
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

### 3-2. 돌진 데미지 GE — 별도 생성 불필요

돌진 데미지는 `GA_ArmadilloRollCharge`가 `DRDamageGameplayAbility`를 상속하므로, 부모 클래스의 `DamageEffectClass` 프로퍼티에 **기존 범용 데미지 GE**를 설정하면 된다. `CauseDamage()` / `MakeDamageEffectParamsFromClassDefaults()`가 GA에 설정된 `Damage`, `DamageType` 값을 자동으로 사용하므로 별도의 `GE_ArmadilloRollDamage`를 만들 필요가 없다.

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

**GA Blueprint 프로퍼티 설정** (부모 `DRDamageGameplayAbility`에서 상속):

| 프로퍼티 | 값 | 비고 |
|---------|-----|------|
| DamageEffectClass | 기존 범용 데미지 GE | `ExecCalc_Damage` 사용하는 기존 GE |
| DamageType | `Damage.Physical` | 물리 데미지 |
| Damage | 50 | 기본 돌진 데미지 |
| DebuffChance | 0 | 돌진은 별도 스턴 GE로 처리 |
| KnockbackChance | 0 | 돌진에 넉백 없음 |

**GA Blueprint 추가 변수** (GA 내부에서 선언):

| 변수 | 타입 | 값 | 용도 |
|------|------|-----|------|
| CleanserDamageMultiplier | Float | 1.5 | 클렌저 사이트 데미지 배율 |
| PlayerStunEffectClass | TSubclassOf\<UGameplayEffect\> | `GE_ArmadilloRollStun_Player` | 플레이어 1초 스턴 |
| SelfStunEffectClass | TSubclassOf\<UGameplayEffect\> | `GE_ArmadilloRollStun_Self` | 본인 1.5초 스턴 |
| SavedTargetLocation | FVector | - | 타겟팅 시점 위치 저장 |

**GA Blueprint 로직 (ActivateAbility)**:

```
[ActivateAbility]
    │
    ├─ 1. ★ FindRollTarget() 호출 (C++ 함수) — 폼 전환 전에 먼저 타겟 검증
    │     └─ 결과 null이면 → EndAbility (취소, 쿨타임 미적용)
    │        ※ 기본 폼 유지, 폼 전환 애니메이션 재생하지 않음
    │
    ├─ 2. ★ 타겟 위치 저장
    │     └─ SavedTargetLocation = TargetActor->GetActorLocation()
    │        (이 시점의 위치가 돌진 목표점으로 고정됨)
    │
    ├─ 3. C++ 델리게이트 바인딩
    │     ├─ OnRollImpact 바인딩 → HandleRollImpact (커스텀 이벤트)
    │     └─ OnRollReachedTarget 바인딩 → HandleRollReachedTarget (커스텀 이벤트)
    │
    ├─ 4. 폼 전환: Basic → Ball (양쪽 몽타주 재생)
    │     │
    │     ├─ StartFormChange(true) 호출
    │     │
    │     ├─ PlayMontageAndWait(AM_Armadillo_FormChange_BtoD) — BasicForm 메시에서 재생
    │     │   └─ 기본 폼이 볼로 말리기 시작하는 애니메이션
    │     │   └─ AN_FinishFormChange → FinishFormChange() → 메시 교체
    │     │
    │     ├─ PlayMontageAndWait(AM_ArmadilloBall_FormChange_BtoD) — BallForm 메시에서 이어서 재생
    │     │   └─ 볼 폼이 완전히 말리는 마무리 애니메이션
    │     │   └─ 몽타주 완료 대기
    │     │
    │     └─ 폼 전환 완료
    │
    ├─ 5. StartRollCharge(SavedTargetLocation) 호출
    │     └─ C++의 Tick에서 자동으로 이동 + 충돌 감지 수행
    │     └─ 충돌 또는 도달 시 → C++이 바인딩된 델리게이트 브로드캐스트
    │
    └─ 6. 이후 흐름은 델리게이트 콜백에서 처리 (아래 참조)
```

**HandleRollImpact (충돌 시 — 커스텀 이벤트)**:

```
HandleRollImpact(ImpactResult: FRollImpactResult)
    │
    ├─ ImpactResult.HitActors 배열을 순회:
    │   │
    │   ├─ [Cast to ADRCharacter 성공 — 플레이어]
    │   │   ├─ ★ CauseDamage(PlayerActor) ← 부모의 DamageEffectClass/Damage(50) 자동 사용
    │   │   │   └─ MakeDamageEffectParamsFromClassDefaults(PlayerActor)
    │   │   │       └─ ASC->ApplyGameplayEffectSpecToTarget()
    │   │   │           └─ ExecCalc_Damage → 플레이어 Health -50
    │   │   │
    │   │   └─ ★ 1초 스턴 GE 적용:
    │   │       ├─ 플레이어의 ASC 가져오기 (IAbilitySystemInterface)
    │   │       ├─ MakeOutgoingSpec(PlayerStunEffectClass)
    │   │       └─ ApplyGameplayEffectSpecToTarget(플레이어 ASC)
    │   │
    │   ├─ [Cast to ADREnemy 성공 — 다른 적]
    │   │   └─ ★ CauseDamage(EnemyActor) ← 동일하게 50 데미지
    │   │
    │   └─ [Cast to ADRCleanserSite 성공 — 클렌저 사이트]
    │       └─ ★ 1.5배 데미지 적용:
    │           ├─ MakeDamageEffectParamsFromClassDefaults(CleanserActor)
    │           ├─ Params.Damage = GetDamageAtLevel() * CleanserDamageMultiplier (50 * 1.5 = 75)
    │           └─ ApplyDamageEffect(Params) → ExecCalc_Damage → 클렌저 Health -75
    │
    ├─ ImpactResult.bHitPlayerOrEnemy 확인:
    │   │
    │   ├─ [true — 플레이어/적이 있었음]
    │   │   └─ 본인 스턴 없음
    │   │
    │   └─ [false — 지형지물만 충돌]
    │       └─ ★ 본인 스턴 GE 적용:
    │           ├─ GetAbilitySystemComponentFromActorInfo()
    │           ├─ MakeOutgoingSpec(SelfStunEffectClass)
    │           └─ ApplyGameplayEffectSpecToSelf() → 1.5초 스턴
    │
    ├─ 폼 복귀: Ball → Basic (양쪽 몽타주 재생)
    │   │
    │   ├─ StartFormChange(false) 호출
    │   │
    │   ├─ PlayMontageAndWait(AM_ArmadilloBall_FormChange_DtoB) — BallForm 메시에서 재생
    │   │   └─ 볼 폼이 펴지기 시작하는 애니메이션
    │   │   └─ AN_FinishFormChange → FinishFormChange() → 메시 교체
    │   │
    │   ├─ PlayMontageAndWait(AM_Armadillo_FormChange_DtoB) — BasicForm 메시에서 이어서 재생
    │   │   └─ 기본 폼이 완전히 펴지는 마무리 애니메이션
    │   │   └─ 몽타주 완료 대기
    │   │
    │   └─ 폼 복귀 완료
    │
    ├─ CommitAbilityCooldown() → 쿨타임 10초 시작
    │
    └─ EndAbility()
```

**HandleRollReachedTarget (충돌 없이 목표 도달 시 — 커스텀 이벤트)**:

```
HandleRollReachedTarget()
    │
    ├─ (데미지/스턴 없음 — 충돌이 발생하지 않았으므로)
    │
    ├─ 폼 복귀: Ball → Basic (양쪽 몽타주 재생)
    │   │
    │   ├─ StartFormChange(false) 호출
    │   │
    │   ├─ PlayMontageAndWait(AM_ArmadilloBall_FormChange_DtoB) — BallForm 메시에서 재생
    │   │   └─ 볼 폼이 펴지기 시작하는 애니메이션
    │   │   └─ AN_FinishFormChange → FinishFormChange() → 메시 교체
    │   │
    │   ├─ PlayMontageAndWait(AM_Armadillo_FormChange_DtoB) — BasicForm 메시에서 이어서 재생
    │   │   └─ 기본 폼이 완전히 펴지는 마무리 애니메이션
    │   │   └─ 몽타주 완료 대기
    │   │
    │   └─ 폼 복귀 완료
    │
    ├─ CommitAbilityCooldown() → 쿨타임 10초 시작
    │
    └─ EndAbility()
```

**이 설계의 이점**:
- `CauseDamage()`, `MakeDamageEffectParamsFromClassDefaults()` 등 **부모 클래스의 인프라를 그대로 활용**
- `DamageEffectClass`, `Damage`, `DamageType` 등 **GA 프로퍼티에서 한 곳에서 설정** → 밸런싱 조정이 쉬움
- C++에 데미지/GE 관련 코드가 없어 **책임 분리 명확** (C++ = 물리/충돌, GA = 효과/데미지)
- 클렌저 1.5배 데미지도 GA Blueprint에서 `GetDamageAtLevel() * 1.5`로 간단히 처리
- 스턴 GE도 GA Blueprint에서 직접 ASC에 적용 → 별도 C++ 프로퍼티 불필요

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
│     ├─ Decorator: Blackboard — "Dead" == true
│     │   ├─ Key Query: Is Set
│     │   ├─ Notify Observer: On Value Change
│     │   └─ Observer Aborts: Both (사망 시 하위 트리 모두 즉시 중단)
│     └─ Task: BTT_StopBehavior
│
├─ [2] 스턴 체크 (Sequence)
│     ├─ Decorator: Blackboard — "Stunned" == true
│     │   ├─ Key Query: Is Set
│     │   ├─ Notify Observer: On Value Change
│     │   └─ Observer Aborts: Both (스턴 진입/해제 시 즉시 반응)
│     └─ Task: Wait (스턴 해제될 때까지)
│
├─ [3] 히트 리액트 (Sequence)
│     ├─ Decorator: Blackboard — "HitReacting" == true
│     │   ├─ Key Query: Is Set
│     │   ├─ Notify Observer: On Value Change
│     │   └─ Observer Aborts: Both
│     └─ Task: Wait (히트 리액트 종료될 때까지)
│
├─ [4] 전투 분기 — 타겟 있음 (Selector)
│     │
│     │  ★ 이 Selector에 Decorator를 걸어서 타겟 존재 여부를 검사
│     ├─ Decorator: Blackboard — "TargetToFollow"
│     │   ├─ Key Query: Is Set
│     │   ├─ Notify Observer: On Value Change
│     │   └─ Observer Aborts: Lower Priority
│     │      (타겟이 새로 생기면 [5] 순찰을 중단하고 여기로 복귀)
│     │
│     ├─ [4-1] 롤링 스킬 (Sequence)
│     │     │
│     │     ├─ Decorator: Blackboard — "RollSkillReady" == true
│     │     │   ├─ Key Query: Is Set
│     │     │   ├─ Notify Observer: On Value Change
│     │     │   └─ Observer Aborts: None
│     │     │      (쿨타임이 돌아와도 현재 기본 공격을 중단하지 않음.
│     │     │       기본 공격 완료 후 자연스럽게 다음 Tick에서 스킬 시도)
│     │     │
│     │     └─ Task: BTT_ArmadilloRollAttack (★ 신규)
│     │           └─ TryActivateAbilitiesByTag("Abilities.Armadillo.RollCharge")
│     │              ※ GA 내부에서 폼 전환 전에 FindRollTarget() 실행
│     │              ※ 유효 타겟 없으면 GA가 즉시 종료 → BTT는 Failure 반환
│     │              ※ 유효 타겟 있으면 타겟 위치 저장 → 폼 전환 → 돌진
│     │              ※ 스킬 성공 시 쿨타임 적용 → RollSkillReady = false
│     │
│     └─ [4-2] 접근 + 기본 공격 (Sequence)
│           │
│           │  ★ Decorator 없음 — [4-1]이 실패(쿨다운 중)하면 자동으로 여기로 폴스루
│           │
│           ├─ Task: MoveTo (TargetToFollow)
│           │   └─ Acceptable Radius: 공격 사거리 (예: 150)
│           │
│           ├─ Task: BTT_RotateToFaceTarget
│           │
│           └─ Task: BTT_Attack_Armadillo (★ 신규)
│                 └─ TryActivateAbilitiesByTag("Abilities.Armadillo.BasicAttack")
│
└─ [5] 순찰 (Sequence)
      │
      │  ★ Decorator 없음 — [4]가 실패(타겟 없음)하면 자동으로 여기로 폴스루
      │
      └─ Task: MoveTo (HomeLocation 주변 랜덤)
```

**데코레이터 설정 상세 설명**:

| 노드 | 데코레이터 | Key Query | Observer Aborts | 이유 |
|------|-----------|-----------|-----------------|------|
| [1] 사망 | BB "Dead" | Is Set | **Both** | 사망은 최우선 — 어떤 행동 중이든 즉시 중단 |
| [2] 스턴 | BB "Stunned" | Is Set | **Both** | 스턴 진입 시 하위 행동 중단, 해제 시 자기도 중단 |
| [3] 히트리액트 | BB "HitReacting" | Is Set | **Both** | 위와 동일 |
| [4] 전투 분기 | BB "TargetToFollow" | Is Set | **Lower Priority** | 타겟 생기면 순찰[5]을 중단하고 전투로 복귀 |
| [4-1] 롤링 스킬 | BB "RollSkillReady" | Is Set | **None** | 쿨타임 복귀 시 기본 공격을 중간에 끊지 않음 |
| [4-2] 기본 공격 | (없음) | - | - | 폴스루 전용 — 스킬 실패 시 자동 진입 |
| [5] 순찰 | (없음) | - | - | 폴스루 전용 — 타겟 없을 때 자동 진입 |

**Observer Aborts 해설**:
- **None**: 값이 변해도 현재 실행 중인 노드를 중단하지 않음. 다음 자연 재평가 때 반영
- **Self**: 조건이 **거짓**이 되면 자기 서브트리를 중단
- **Lower Priority**: 조건이 **참**이 되면 자기보다 **아래(오른쪽)** 형제 노드를 중단하고 자기 실행
- **Both**: Self + Lower Priority 합친 것

**흐름 예시**:

```
[타겟 없음 → 순찰 중 → 타겟 감지]
  순찰[5] 실행 중
  → TargetToFollow 값 Set됨
  → [4]의 Decorator(Lower Priority) 발동
  → [5] 순찰 즉시 중단
  → [4] 전투 분기 진입
  → [4-1] RollSkillReady 확인 → true면 스킬, false면 [4-2] 기본 공격

[스킬 쿨다운 중 → 기본 공격 중 → 쿨다운 해제]
  [4-2] 기본 공격 실행 중
  → BTS_CheckRollSkillReady가 RollSkillReady = true로 설정
  → [4-1]의 Decorator(None) → 현재 공격을 중단하지 않음
  → 기본 공격 완료 → Selector[4] 재평가 → [4-1] 스킬 시도

[스킬 실행 → 쿨다운 적용 → 다음 행동]
  [4-1] 스킬 성공 → RollSkillReady = false
  → Selector[4] 재평가 → [4-1] 데코레이터 실패
  → [4-2] 폴스루 → 접근 + 기본 공격
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
    3. GA 결과 확인:
       - GA가 유효 타겟을 찾지 못해 즉시 EndAbility한 경우:
         → 쿨타임 미적용 상태, Return Failure (BT가 다음 분기로 이동)
       - GA가 정상적으로 돌진을 수행하고 종료한 경우:
         → 쿨타임 적용됨, Set BB "RollSkillReady" = false
         → Return Success
  ```
- **GA 실패 판별**: GA 내부에서 FindRollTarget() 실패 시 쿨타임을 적용하지 않으므로,
  BTT는 GA 종료 후 ASC에 `Cooldown.Armadillo.RollCharge` 태그가 있는지 확인하여
  성공/실패를 판별할 수 있다.

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
| **ADRCharacterBase** | | |
| CharacterClass | Warrior | 근접 공격 기반 |
| Level | 1 (기본) | 스폰 시 조정 |
| BaseWalkSpeed | 250 | 기본 폼 이동 속도 |
| **ADREnemy** | | |
| BehaviorTree | `BT_EnemyBehaviorTree_Armadillo` | |
| WaterReductionEffectClass | 기존 적 GE | |
| WaterGrantEffectClass | 기존 적 GE | |
| EnrageMovementSpeedGE | 기존 적 GE | |
| **ADRArmadilloEnemy** | | |
| RollInitialSpeed | 200 | 돌진 초기 속도 |
| RollMaxSpeed | 2000 | 돌진 최대 속도 |
| RollAcceleration | 400 | 초당 가속량 |
| RollTargetSearchRange | 3000 | 유닛 |
| RollDetectionRadius | 30 | 전방 감지 구 반지름 |
| RollImpactRadius | 50 | 충돌 효과 구 반지름 |

**참고**: 데미지(`RollDamage`), 클렌저 배율(`CleanserDamageMultiplier`), 스턴 GE 클래스(`RollStunEffectClass`), 데미지 GE 클래스(`RollDamageEffectClass`), 스턴 지속시간, 쿨타임 등은 C++이 아닌 **GA_ArmadilloRollCharge Blueprint의 프로퍼티**에서 설정한다.

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
- [ ] 7. `GE_ArmadilloRollStun_Player` 생성 (1초)
- [ ] 8. `GE_ArmadilloRollStun_Self` 생성 (1.5초)
- [ ] 9. `GE_Cooldown_ArmadilloRollCharge` 생성 (10초)
- ※ 돌진 데미지 GE는 별도 생성 불필요 (GA의 DamageEffectClass에 기존 범용 GE 사용)

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

### 5. FindRollTarget 실패 (폼 전환 전 검증)
- 3000 내에 벽 없는 플레이어가 없으면:
  - GA의 ActivateAbility 진입 직후, **폼 전환 전에** FindRollTarget()이 nullptr 반환
  - 즉시 EndAbility 호출 → GA 종료
  - **폼 전환 애니메이션이 전혀 재생되지 않음** (기본 폼 유지)
  - 쿨타임 미적용 (CommitAbilityCooldown 호출 전에 종료되므로)
  - BT의 다음 Tick에서 즉시 다른 행동(근접 공격, 추적 등)으로 전환 가능

---

## 부록: 상황별 호출 흐름 상세

아르마딜로가 겪는 모든 주요 상황에 대해, **어디(C++/BP/BT/GA/GE/AnimBP)의 어떤 함수/노드가 어떤 순서로 호출되는지**를 시간 순서대로 기술한다.

---

### A. 스폰 및 초기화

```
[1] 월드에 BP_Armadillo 스폰 (GameMode 또는 Phase에서 SpawnActor)
    │
    │ ── C++: ADRArmadilloEnemy::ADRArmadilloEnemy() (생성자)
    │    ├─ BallFormMesh 컴포넌트 생성 (CreateDefaultSubobject)
    │    ├─ BallFormMesh->SetVisibility(false)
    │    └─ BallFormMesh->SetCollisionEnabled(NoCollision)
    │
    │ ── C++: ADREnemy::ADREnemy() (부모 생성자)
    │    ├─ PartMeshComponent 생성
    │    ├─ HealthBar 위젯 컴포넌트 생성
    │    └─ HitboxComponents 초기화
    │
    │ ── C++: ADRCharacterBase::ADRCharacterBase() (조부모 생성자)
    │    ├─ AbilitySystemComponent 생성
    │    ├─ AttributeSets (UDREnemyAttributeSet) 생성
    │    ├─ BurnDebuffComponent, StunDebuffComponent 생성
    │    └─ Weapon SkeletalMeshComponent 생성
    │
    ▼
[2] ADRArmadilloEnemy::BeginPlay()
    │
    ├─ Super::BeginPlay() 호출
    │   │
    │   ├─ ADREnemy::BeginPlay()
    │   │   ├─ SetupHitboxComponents() — "Hitbox" 태그 컴포넌트 수집
    │   │   ├─ CapsuleComponent->OnComponentHit 바인딩 → OnHit() (벽 스턴용)
    │   │   └─ 체력 변화 델리게이트 바인딩 (OnHealthChanged, OnMaxHealthChanged)
    │   │
    │   └─ ADRCharacterBase::BeginPlay()
    │       └─ Debuff 태그 콜백 등록 (Stun, Burn)
    │
    ├─ DefaultCapsuleRadius 저장 ← GetCapsuleComponent()->GetUnscaledCapsuleRadius()
    └─ DefaultCapsuleHalfHeight 저장 ← GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()
    │
    ▼
[3] ADREnemy::PossessedBy(ADRAIController) — AI 컨트롤러 소유
    │
    ├─ DRAIController 참조 저장
    ├─ InitAbilityActorInfo()
    │   ├─ ASC->InitAbilityActorInfo(this, this)
    │   ├─ ASC 태그 콜백 등록:
    │   │   ├─ Effects.HitReact → HitReactTagChanged()
    │   │   └─ Debuff.Stun → StunTagChanged()
    │   └─ OnAscRegistered 델리게이트 브로드캐스트
    │
    ├─ InitializeDefaultAttributes()
    │   ├─ UDRAbilitySystemLibrary::InitializeDefaultAttributes()
    │   │   ├─ CharacterClassInfo에서 Warrior 클래스 정보 조회
    │   │   ├─ GE_PrimaryAttributes_Armadillo 적용 (MaxHealth, MoveSpeed 등)
    │   │   └─ GE_VitalAttributes_Enemy 적용 (현재 Health, Water 초기화)
    │   └─ MoveSpeed 어트리뷰트 → CharacterMovement->MaxWalkSpeed 반영
    │
    ├─ AddCharacterAbilities() — 어빌리티 부여
    │   ├─ GA_ArmadilloBite → ASC->GiveAbility()
    │   ├─ GA_ArmadilloRollCharge → ASC->GiveAbility()
    │   └─ GA_HitReact (공용) → ASC->GiveAbility()
    │
    ├─ Blackboard 초기화
    │   ├─ BB->InitializeBlackboard(*BehaviorTree->BlackboardAsset)
    │   ├─ BB->SetValueAsBool("RangedAttacker", false) — 근접 타입
    │   ├─ BB->SetValueAsVector("HomeLocation", GetActorLocation())
    │   └─ BB->SetValueAsBool("RollSkillReady", true) — 초기 스킬 사용 가능
    │
    └─ DRAIController->RunBehaviorTree(BT_EnemyBehaviorTree_Armadillo) — BT 실행 시작
    │
    ▼
[4] AnimBP 초기화 (BP_Armadillo의 Mesh 컴포넌트에 설정)
    │
    ├─ ABP_Armadillo_Basic — BasicForm 메시의 AnimInstance
    │   └─ Event Blueprint Initialize Animation
    │       └─ Owner 캐스팅 → ADRArmadilloEnemy 참조 저장
    │
    └─ ABP_Armadillo_Ball — BallForm 메시의 AnimInstance (Hidden 상태지만 초기화됨)
        └─ Event Blueprint Initialize Animation
            └─ Owner 캐스팅 → ADRArmadilloEnemy 참조 저장
```

---

### B. 평시 행동 (기본 폼 — 순찰 / 추적 / 대기)

```
[매 BT Tick]
    │
    ├─ BT: Root Selector 평가
    │   ├─ [1] Dead == false → 스킵
    │   ├─ [2] Stunned == false → 스킵
    │   ├─ [3] HitReacting == false → 스킵
    │   ├─ [4] RollSkillReady 확인 (BTS_CheckRollSkillReady에서 매 0.5초 갱신)
    │   │     └─ ASC에 Cooldown.Armadillo.RollCharge 태그 없음 → RollSkillReady = true
    │   │     └─ 하지만 TargetToFollow 미설정 또는 타겟이 플레이어 아님 → [4] 스킵
    │   ├─ [5] 기본 공격: TargetToFollow 미설정 또는 범위 밖 → 스킵
    │   ├─ [6] 추적: TargetToFollow 설정됨 → MoveTo 실행
    │   └─ [7] 순찰: TargetToFollow 미설정 → HomeLocation 주변 이동
    │
    ▼
[BTS_FindNearestPlayer (BT Service, 매 Tick)]
    │
    ├─ AIPerceptionComponent에서 감지된 플레이어 목록 조회
    ├─ 가장 가까운 플레이어 → BB "TargetToFollow"에 설정
    └─ 감지된 플레이어 없으면 → BB "TargetToFollow" 클리어
    │
    ▼
[이동 중 — CharacterMovement]
    │
    ├─ C++: CharacterMovementComponent가 NavMesh 경로를 따라 이동
    ├─ AnimBP: ABP_Armadillo_Basic → State Machine
    │   ├─ Event Blueprint Update Animation (매 프레임)
    │   │   └─ Speed = GetVelocity().Size()
    │   └─ Locomotion 상태:
    │       └─ BS_Armadillo_IdleWalk 블렌드 스페이스
    │           ├─ Speed ≈ 0 → Idle 애니메이션
    │           └─ Speed ≈ 250 → Walk 애니메이션
    └─ (BallFormMesh는 Hidden이므로 ABP_Armadillo_Ball은 시각적 영향 없음)
```

---

### C. 기본 공격 (BasicForm 근접 공격)

```
[1] BT: [5] 기본 공격 분기 진입
    │
    ├─ Decorator: BB "TargetToFollow" IsSet ✓
    ├─ Decorator: 공격 범위 내 ✓
    │
    ├─ BTT_RotateToFaceTarget 실행
    │   └─ C++/BP: 아르마딜로를 타겟 방향으로 회전
    │
    └─ BTT_Attack_Armadillo 실행
        │
        ▼
[2] BTT_Attack_Armadillo (BT Task Blueprint)
    │
    └─ ASC->TryActivateAbilitiesByTag("Abilities.Armadillo.BasicAttack")
        │
        ▼
[3] GA_ArmadilloBite::ActivateAbility() (GA Blueprint — UDRMeleeAttack 상속)
    │
    ├─ CommitAbility() — 코스트/쿨다운 확인
    │
    ├─ PlayMontageAndWait(AM_Armadillo_BasicAttack)
    │   │
    │   │ ── AnimBP: ABP_Armadillo_Basic
    │   │    └─ DefaultSlot에 AM_Armadillo_BasicAttack 몽타주 재생
    │   │    └─ BasicAttack 애니메이션 시작
    │   │
    │   ▼
    ├─ [몽타주 재생 중 — 데미지 타이밍]
    │   │
    │   └─ AnimNotify 발동 (데미지 Notify)
    │       │
    │       └─ GA Blueprint: 이벤트 수신
    │           ├─ GetAttackMontages() → FTaggedMontage 조회
    │           ├─ GetCombatSocketLocation(CombatSocket.RightHand) → 소켓 위치
    │           ├─ SphereTrace / BoxTrace 실행 (공격 범위)
    │           ├─ 히트된 액터에 대해:
    │           │   ├─ MakeDamageEffectParamsFromClassDefaults(HitActor)
    │           │   │   └─ DamageType: Damage.Physical (또는 Damage.Bite)
    │           │   │   └─ Damage: 설정된 수치
    │           │   └─ CauseDamage(HitActor)
    │           │       └─ ASC->ApplyGameplayEffectSpecToTarget()
    │           │           │
    │           │           └─ ExecCalc_Damage 실행 (서버)
    │           │               ├─ 기본 데미지 계산
    │           │               ├─ 디버프 확률 체크 (Physical → Debuff.Physical)
    │           │               └─ 최종 데미지 적용 → 대상 Health 감소
    │           │
    │           └─ OnAttackExecuted() — 물 보상 감소 카운터 증가
    │
    ▼
[4] 몽타주 완료
    │
    └─ GA_ArmadilloBite: OnCompleted 콜백
        └─ EndAbility()
            │
            └─ BTT_Attack_Armadillo: OnAbilityEnded
                └─ FinishExecute(true) → BT로 제어권 반환
```

---

### D. 롤링 돌진 스킬 (성공 — 플레이어/적 충돌)

```
[1] BT: [4] 롤링 스킬 분기 진입
    │
    ├─ Decorator: BB "RollSkillReady" == true ✓
    │   └─ (BTS_CheckRollSkillReady가 ASC에 Cooldown 태그 없음 확인)
    ├─ Decorator: BB "TargetToFollow" IsSet ✓
    ├─ Decorator: Target이 플레이어 ✓
    │
    └─ BTT_ArmadilloRollAttack 실행
        │
        └─ ASC->TryActivateAbilitiesByTag("Abilities.Armadillo.RollCharge")
            │
            ▼
[2] GA_ArmadilloRollCharge::ActivateAbility() (GA Blueprint)
    │
    ├─ ★ Step 1: 타겟 검색 (폼 전환 전)
    │   │
    │   └─ C++: ADRArmadilloEnemy::FindRollTarget()
    │       ├─ UGameplayStatics::GetAllActorsOfClass(ADRCharacter) — 플레이어 수집
    │       ├─ 각 플레이어에 대해:
    │       │   ├─ 거리 계산 (GetDistanceTo) — 3000 이내 필터링
    │       │   ├─ LineTrace (자신 → 플레이어, ECC_WorldStatic)
    │       │   │   └─ 히트 있으면 = 벽 차단 → 제외
    │       │   │   └─ 히트 없으면 = 시야 확보 → 후보 목록에 추가
    │       │   └─ (플레이어 사망 여부도 확인 — IsDead)
    │       ├─ 후보 목록에서 랜덤 1명 선택
    │       └─ 반환: 선택된 플레이어 Actor (또는 nullptr)
    │
    ├─ ★ Step 2: 타겟 유효성 확인
    │   └─ FindRollTarget 결과 != nullptr → 계속 진행 ✓
    │
    ├─ ★ Step 3: 타겟 위치 저장
    │   └─ RollTargetLocation = TargetActor->GetActorLocation()
    │       (이 시점의 위치가 돌진 목표점으로 고정)
    │
    ▼
[3] 폼 전환: Basic → Ball
    │
    ├─ C++: ADRArmadilloEnemy::StartFormChange(true)
    │   └─ bPendingBallForm = true
    │
    ├─ GA Blueprint: PlayMontageAndWait(AM_Armadillo_FormChange_BtoD) — BasicForm 메시에서 재생
    │   │
    │   │ ── AnimBP: ABP_Armadillo_Basic
    │   │    └─ DefaultSlot에 FormChange_BasicToBall 몽타주 재생
    │   │    └─ 기본 폼이 볼로 말리는 애니메이션 시작
    │   │
    │   ├─ [몽타주 재생 중...]
    │   │
    │   └─ AnimNotify: AN_FinishFormChange 발동
    │       │
    │       └─ C++: ADRArmadilloEnemy::FinishFormChange()
    │           ├─ bIsBallForm = true (bPendingBallForm 값)
    │           ├─ OnRep_BallForm() 호출 (서버 로컬)
    │           │   └─ UpdateMeshVisibility()
    │           │       ├─ GetMesh()->SetVisibility(false) — BasicForm 숨김
    │           │       └─ BallFormMesh->SetVisibility(true) — BallForm 표시
    │           ├─ 캡슐 크기 조정:
    │           │   └─ SetCapsuleSize(BallFormCapsuleRadius, BallFormCapsuleHalfHeight)
    │           │
    │           └─ [리플리케이션] bIsBallForm = true → 클라이언트로 전파
    │               └─ 클라이언트: OnRep_BallForm()
    │                   └─ UpdateMeshVisibility() — 동일하게 메시 교체
    │
    ├─ GA Blueprint: PlayMontageAndWait(AM_ArmadilloBall_FormChange_BtoD) — BallForm 메시에서 이어서 재생
    │   │
    │   │ ── AnimBP: ABP_Armadillo_Ball
    │   │    └─ BallForm의 FormChange_BasicToBall 몽타주 재생
    │   │    └─ 볼 폼이 완전히 말리는 마무리 애니메이션
    │   │
    │   └─ [몽타주 완료 대기]
    │
    ▼
[4] 돌진 시작
    │
    ├─ C++: ADRArmadilloEnemy::StartRollCharge(RollTargetLocation)
    │   ├─ bIsRolling = true
    │   ├─ RollDirection = (TargetLocation - GetActorLocation()).GetSafeNormal2D()
    │   ├─ CurrentRollSpeed = RollInitialSpeed (200)
    │   └─ DRAIController->StopMovement() — AI 경로 이동 중지
    │
    │ ── AnimBP: ABP_Armadillo_Ball
    │    └─ State Machine: bIsRolling == true → Roll 상태 진입
    │        └─ Roll 애니메이션 Loop 재생
    │
    ▼
[5] 돌진 중 (매 서버 Tick)
    │
    └─ C++: ADRArmadilloEnemy::Tick(DeltaTime)
        └─ HasAuthority() && bIsRolling → TickRollCharge(DeltaTime)
            │
            ├─ 5-1. 가속
            │   └─ CurrentRollSpeed += RollAcceleration * DeltaTime
            │       (200 → 600 → 1000 → ... → 최대 2000)
            │       └─ FMath::Min으로 RollMaxSpeed 클램프
            │
            ├─ 5-2. 이동
            │   └─ CharacterMovement->Velocity = RollDirection * CurrentRollSpeed
            │       └─ CharacterMovement가 리플리케이션으로 클라이언트에 위치 동기화
            │
            ├─ 5-3. 목표점 도달 확인
            │   └─ DotProduct(ToTarget, RollDirection) > 0 → 아직 진행 중
            │
            └─ 5-4. 전방 충돌 감지
                └─ DetectRollCollision(HitResult)
                    └─ SweepSingleByChannel(반지름 30 구, ECC_Pawn)
                        ├─ Start: 현재 위치
                        ├─ End: 현재 위치 + 이동 방향 * 속도 * DeltaTime
                        └─ 히트 감지됨! (플레이어 또는 적 또는 지형지물)
    │
    ▼
[6] 충돌 감지 → C++이 결과를 GA에 전달
    │
    ├─ C++: StopRollCharge()
    │   ├─ bIsRolling = false
    │   ├─ CurrentRollSpeed = 0
    │   └─ CharacterMovement->Velocity = Zero
    │
    ├─ C++: BroadcastRollImpact(HitResult.ImpactPoint)
    │   │
    │   ├─ 6-1. 충돌 지점에서 반지름 50 구 오버랩 검사
    │   │   └─ OverlapMultiByChannel(ImpactPoint, 반지름 50, ECC_Pawn)
    │   │       └─ 겹치는 액터 목록 수집 → FRollImpactResult.HitActors에 저장
    │   │
    │   ├─ 6-2. 각 액터 타입 확인 → bHitPlayerOrEnemy 플래그 설정
    │   │   └─ ADRCharacter 또는 ADREnemy가 있으면 true
    │   │
    │   └─ 6-3. ★ OnRollImpact.Broadcast(Result) → GA에 충돌 결과 전달
    │       └─ (C++은 데미지/스턴을 적용하지 않음)
    │
    │ ── AnimBP: ABP_Armadillo_Ball
    │    └─ bIsRolling == false → Roll에서 Idle로 전환
    │
    ▼
[7] ★ GA Blueprint: HandleRollImpact(ImpactResult) — 데미지/스턴 처리
    │
    ├─ ImpactResult.HitActors 배열 순회:
    │   │
    │   ├─ [Cast to ADRCharacter 성공 — 플레이어]
    │   │   ├─ ★ CauseDamage(PlayerActor)
    │   │   │   └─ 부모 DRDamageGameplayAbility의 인프라 사용:
    │   │   │       ├─ MakeDamageEffectParamsFromClassDefaults(PlayerActor)
    │   │   │       │   └─ DamageEffectClass, Damage(50), DamageType(Physical) 자동 적용
    │   │   │       └─ ASC->ApplyGameplayEffectSpecToTarget(플레이어 ASC)
    │   │   │           └─ ExecCalc_Damage 실행 → 플레이어 Health -50
    │   │   │
    │   │   └─ ★ 1초 스턴 GE 적용 (GA Blueprint에서 직접):
    │   │       ├─ 플레이어 ASC 가져오기 (IAbilitySystemInterface)
    │   │       ├─ MakeOutgoingSpec(PlayerStunEffectClass = GE_ArmadilloRollStun_Player)
    │   │       └─ ApplyGameplayEffectSpecToTarget(플레이어 ASC)
    │   │           ├─ 플레이어: Debuff.Stun 태그 부여
    │   │           ├─ 플레이어: StunTagChanged() → bIsStunned = true
    │   │           ├─ 플레이어: OnRep_Stunned() → StunDebuffComponent 활성화
    │   │           └─ 1초 후 GE 만료 → 스턴 해제
    │   │
    │   ├─ [Cast to ADREnemy 성공 — 다른 적]
    │   │   └─ ★ CauseDamage(EnemyActor) ← 동일하게 50 데미지
    │   │       └─ MakeDamageEffectParamsFromClassDefaults → ExecCalc_Damage
    │   │
    │   └─ [Cast to ADRCleanserSite 성공 — 클렌저 사이트]
    │       └─ ★ 1.5배 데미지 적용 (GA Blueprint에서 계산):
    │           ├─ Params = MakeDamageEffectParamsFromClassDefaults(CleanserActor)
    │           ├─ Params의 Damage를 GetDamageAtLevel() * CleanserDamageMultiplier로 변경
    │           │   └─ 50 * 1.5 = 75
    │           └─ ApplyDamageEffect(Params) → ExecCalc_Damage → 클렌저 Health -75
    │
    ├─ ImpactResult.bHitPlayerOrEnemy 확인:
    │   ├─ [true] → 본인 스턴 없음
    │   └─ [false — 지형지물만 충돌] → 본인 스턴 (GA Blueprint에서 적용):
    │       ├─ GetAbilitySystemComponentFromActorInfo()
    │       ├─ MakeOutgoingSpec(SelfStunEffectClass = GE_ArmadilloRollStun_Self)
    │       └─ ApplyGameplayEffectSpecToSelf() → 1.5초 스턴
    │
    ▼
[8] 기본 폼 복귀 + 스킬 완료 (GA Blueprint HandleRollImpact 계속)
    │
    ├─ C++: StartFormChange(false)
    │   └─ bPendingBallForm = false
    │
    ├─ GA Blueprint: PlayMontageAndWait(AM_ArmadilloBall_FormChange_DtoB) — BallForm 메시에서 재생
    │   │
    │   │ ── AnimBP: ABP_Armadillo_Ball
    │   │    └─ FormChange_BallToBasic 몽타주 재생
    │   │
    │   └─ AnimNotify: AN_FinishFormChange 발동
    │       │
    │       └─ C++: FinishFormChange()
    │           ├─ bIsBallForm = false
    │           ├─ OnRep_BallForm()
    │           │   └─ UpdateMeshVisibility()
    │           │       ├─ GetMesh()->SetVisibility(true) — BasicForm 표시
    │           │       └─ BallFormMesh->SetVisibility(false) — BallForm 숨김
    │           └─ 캡슐 크기 복원:
    │               └─ SetCapsuleSize(DefaultCapsuleRadius, DefaultCapsuleHalfHeight)
    │
    ├─ GA Blueprint: PlayMontageAndWait(AM_Armadillo_FormChange_DtoB) — BasicForm 메시에서 이어서 재생
    │   │
    │   │ ── AnimBP: ABP_Armadillo_Basic
    │   │    └─ BasicForm의 FormChange_BallToBasic 몽타주 재생
    │   │    └─ 기본 폼이 완전히 펴지는 마무리 애니메이션
    │   │
    │   └─ [몽타주 완료 대기]
    │
    ├─ GA Blueprint: CommitAbilityCooldown()
    │   └─ GE_Cooldown_ArmadilloRollCharge 적용 → ASC에 Cooldown 태그 10초
    │
    ├─ GA Blueprint: EndAbility()
    │
    └─ BTT_ArmadilloRollAttack: OnAbilityEnded
        ├─ ASC에 Cooldown 태그 확인 → 있음 → 성공
        ├─ BB "RollSkillReady" = false
        └─ FinishExecute(true) → BT로 제어권 반환
            │
            └─ [이후] BTS_CheckRollSkillReady가 매 0.5초마다 Cooldown 태그 확인
                └─ 10초 후 쿨타임 만료 → BB "RollSkillReady" = true
```

---

### E. 롤링 돌진 스킬 — 지형 충돌 (본인 스턴)

```
[1~5] D의 [1]~[5]와 동일 (타겟 검색 → 폼 전환 → 돌진 시작 → Tick 이동)
    │
    ▼
[6] 충돌 감지 → C++이 결과를 GA에 전달
    │
    ├─ C++: DetectRollCollision() — SphereTrace가 지형(WorldStatic) 히트
    │
    ├─ C++: StopRollCharge()
    │   ├─ bIsRolling = false
    │   ├─ CurrentRollSpeed = 0
    │   └─ Velocity = Zero
    │
    ├─ C++: BroadcastRollImpact(ImpactPoint)
    │   │
    │   ├─ OverlapMultiByChannel(반지름 50) 실행
    │   │   └─ 결과: ADRCharacter 없음, ADREnemy 없음
    │   │   └─ (지형지물은 Pawn 채널이 아니므로 오버랩에 잡히지 않음)
    │   │
    │   ├─ FRollImpactResult 구성:
    │   │   ├─ HitActors: 빈 배열 (또는 지형 액터만)
    │   │   └─ bHitPlayerOrEnemy = false
    │   │
    │   └─ ★ OnRollImpact.Broadcast(Result) → GA에 전달
    │
    ▼
[7] ★ GA Blueprint: HandleRollImpact(ImpactResult) — 본인 스턴 처리
    │
    ├─ ImpactResult.HitActors 순회 → 플레이어/적/클렌저 없음
    │
    ├─ ImpactResult.bHitPlayerOrEnemy == false
    │   └─ ★ 본인 스턴 적용 (GA Blueprint에서):
    │       ├─ GetAbilitySystemComponentFromActorInfo()
    │       ├─ MakeOutgoingSpec(SelfStunEffectClass = GE_ArmadilloRollStun_Self)
    │       └─ ApplyGameplayEffectSpecToSelf() → 1.5초 스턴
    │           │
    │           └─ C++: StunTagChanged(Debuff.Stun, 1)
    │               ├─ bIsStunned = true
    │               ├─ BB "Stunned" = true
    │               ├─ OnRep_Stunned() → StunDebuffComponent 활성화 (VFX)
    │               └─ CharacterMovement->MaxWalkSpeed = StunnedMoveSpeed (0)
    │
    ▼
[8] 스턴 상태 (1.5초간)
    │
    ├─ BT: [2] Stunned == true → Wait 분기 진입
    │   └─ 다른 모든 행동 차단
    │
    ├─ AnimBP: ABP_Armadillo_Ball
    │   └─ bIsStunned == true → Idle 상태 (볼 폼에 Stun 애니가 없으므로)
    │
    ▼
[9] 스턴 해제 후 기본 폼 복귀 (GA Blueprint HandleRollImpact 계속)
    │
    ├─ 1.5초 후: GE_ArmadilloRollStun_Self 만료
    │   └─ C++: StunTagChanged(Debuff.Stun, 0)
    │       ├─ bIsStunned = false
    │       ├─ BB "Stunned" = false
    │       ├─ OnRep_Stunned() → StunDebuffComponent 비활성화
    │       └─ CharacterMovement->MaxWalkSpeed 복원
    │
    ├─ GA Blueprint: 폼 복귀 진행 (양쪽 몽타주 재생)
    │   ├─ StartFormChange(false)
    │   │
    │   ├─ PlayMontageAndWait(AM_ArmadilloBall_FormChange_DtoB) — BallForm 메시에서 재생
    │   │   └─ AN_FinishFormChange → FinishFormChange() → 메시 교체 + 캡슐 복원
    │   │
    │   ├─ PlayMontageAndWait(AM_Armadillo_FormChange_DtoB) — BasicForm 메시에서 이어서 재생
    │   │   └─ 몽타주 완료 대기
    │   │
    │   ├─ CommitAbilityCooldown() → 10초 쿨타임
    │   └─ EndAbility()
    │
    └─ BTT → FinishExecute(true) → BT 정상 재개
```

---

### F. 롤링 돌진 스킬 — 타겟 없음 (즉시 취소)

```
[1] BT: [4] 롤링 스킬 분기 진입
    │
    ├─ Decorator: RollSkillReady == true ✓
    ├─ Decorator: TargetToFollow IsSet ✓
    ├─ Decorator: Target이 플레이어 ✓
    │
    └─ BTT_ArmadilloRollAttack 실행
        └─ ASC->TryActivateAbilitiesByTag("Abilities.Armadillo.RollCharge")
            │
            ▼
[2] GA_ArmadilloRollCharge::ActivateAbility()
    │
    ├─ FindRollTarget() 호출
    │   ├─ 3000 내 플레이어 수집
    │   ├─ 각 플레이어에 LineTrace → 모두 벽에 차단됨
    │   └─ 반환: nullptr (유효 타겟 없음)
    │
    ├─ ★ nullptr 확인 → 즉시 EndAbility()
    │   └─ 폼 전환 없음 (StartFormChange 호출하지 않음)
    │   └─ 쿨타임 없음 (CommitAbilityCooldown 호출하지 않음)
    │
    └─ BTT_ArmadilloRollAttack: OnAbilityEnded
        ├─ ASC에 Cooldown 태그 확인 → 없음 → 실패로 판별
        └─ FinishExecute(false) → BT Selector가 다음 분기([5] 기본 공격)로 이동
            │
            └─ 아르마딜로는 기본 폼 유지, 즉시 근접 공격이나 추적 수행
```

---

### G. 롤링 돌진 — 목표점 도달 (충돌 없이 통과)

```
[1~5] D의 [1]~[5]와 동일 (타겟 검색 → 폼 전환 → 돌진)
    │
    ▼
[6] 목표점 도달
    │
    └─ C++: TickRollCharge(DeltaTime)
        ├─ ToTarget = RollTargetLocation - GetActorLocation()
        ├─ DotProduct(ToTarget, RollDirection) <= 0 — 목표점을 지나침
        │
        ├─ StopRollCharge()
        │   ├─ bIsRolling = false
        │   ├─ CurrentRollSpeed = 0
        │   └─ Velocity = Zero
        │
        └─ ★ OnRollReachedTarget.Broadcast() → GA에 목표 도달 알림
    │
    ▼
[7] ★ GA Blueprint: HandleRollReachedTarget() — 폼 복귀 처리
    │
    ├─ (데미지/스턴 없음 — 충돌이 발생하지 않았으므로)
    │
    ├─ 폼 복귀: Ball → Basic (양쪽 몽타주 재생)
    │   ├─ StartFormChange(false)
    │   ├─ PlayMontageAndWait(AM_ArmadilloBall_FormChange_DtoB) — BallForm 메시에서 재생
    │   │   └─ AN_FinishFormChange → FinishFormChange() → 메시 교체
    │   ├─ PlayMontageAndWait(AM_Armadillo_FormChange_DtoB) — BasicForm 메시에서 이어서 재생
    │   │   └─ 몽타주 완료 대기
    │   └─ 폼 복귀 완료
    │
    ├─ CommitAbilityCooldown() → 10초 쿨타임
    │
    └─ EndAbility() → BTT 복귀
```

---

### H. 피격 (HitReact)

```
[1] 플레이어 공격이 아르마딜로에 적중
    │
    ├─ C++: ExecCalc_Damage 실행 (서버)
    │   ├─ 데미지 계산 → Health 감소
    │   ├─ 디버프 확률 체크 (데미지 타입에 따라)
    │   └─ HitReact GE 적용 시도
    │
    ▼
[2] GE_HitReact 적용
    │
    └─ ASC에 Effects.HitReact 태그 부여
        │
        └─ C++: ADREnemy::HitReactTagChanged(Effects.HitReact, 1)
            ├─ bHitReacting = true
            ├─ BB "HitReacting" = true
            └─ HitReactingMoveSpeed 적용 (200)
    │
    ▼
[3] GA_HitReact 발동
    │
    ├─ ASC->TryActivateAbilitiesByTag(Abilities.HitReact)
    │
    └─ GA_HitReact::ActivateAbility()
        │
        ├─ ★ 볼 폼 중이면:
        │   └─ 볼 폼에서는 HitReact 몽타주가 없음
        │   └─ GA가 실패하거나 빈 몽타주로 처리
        │   └─ (돌진 중이면 돌진은 계속됨 — HitReact로 중단하지 않음)
        │
        └─ ★ 기본 폼이면:
            ├─ HitReactMontages 배열에서 랜덤 선택
            │   └─ AM_Armadillo_HitReact1 / 2 / 3 중 하나
            │
            ├─ PlayMontageAndWait(선택된 HitReact 몽타주)
            │   └─ AnimBP: ABP_Armadillo_Basic → 몽타주 재생
            │
            ├─ [몽타주 완료]
            │   └─ EndAbility()
            │
            └─ HitReactTagChanged(Effects.HitReact, 0)
                ├─ bHitReacting = false
                ├─ BB "HitReacting" = false
                └─ 이동 속도 복원
    │
    ▼
[4] BT 재개
    └─ HitReacting == false → 정상 분기 진행
```

---

### I. 사망

```
[1] Health가 0 이하로 감소
    │
    └─ C++: ADREnemy::PostGameplayEffectExecute() 또는 AttributeSet에서 감지
        └─ Die(DeathImpulse) 호출
    │
    ▼
[2] ADREnemy::Die(DeathImpulse)
    │
    ├─ bDead = true
    ├─ OnDeathDelegate 브로드캐스트 (Phase 시스템이 수신 → 적 카운트 감소)
    ├─ ActivateDeathAbilities() — 사망 시 발동 어빌리티 (있으면)
    ├─ DropPart() — 부품 드롭 (bCarriesPart이면)
    └─ MulticastHandleDeath(DeathImpulse) 호출 — RPC
    │
    ▼
[3] ADRArmadilloEnemy::MulticastHandleDeath_Implementation(DeathImpulse)
    │  (모든 클라이언트 + 서버에서 실행)
    │
    ├─ ★ 돌진 중이면:
    │   └─ StopRollCharge()
    │       ├─ bIsRolling = false
    │       └─ Velocity = Zero
    │
    ├─ ★ 볼 폼이면:
    │   └─ 즉시 기본 폼으로 전환 (애니메이션 없이)
    │       ├─ bIsBallForm = false
    │       └─ UpdateMeshVisibility()
    │           ├─ GetMesh()->SetVisibility(true)
    │           └─ BallFormMesh->SetVisibility(false)
    │
    ├─ Super::MulticastHandleDeath_Implementation(DeathImpulse) — 부모 사망 처리
    │   │
    │   ├─ AI 정지:
    │   │   ├─ DRAIController->StopMovement()
    │   │   └─ BrainComponent->StopLogic("Dead")
    │   │
    │   ├─ BB "Dead" = true
    │   │
    │   ├─ ASC에 모든 활성 GA 취소
    │   │   └─ GA_ArmadilloRollCharge 실행 중이면 취소됨
    │   │
    │   ├─ 캡슐 충돌 비활성화
    │   │   └─ SetCollisionEnabled(NoCollision)
    │   │
    │   ├─ Dissolve 시작
    │   │   └─ DissolveMaterialInstance 적용 → 타임라인 시작
    │   │
    │   └─ SetLifeSpan(LifeSpan) — 일정 시간 후 액터 소멸
    │
    │ ── AnimBP: ABP_Armadillo_Basic
    │    └─ bIsDead == true → Death 상태 진입
    │        └─ Death 애니메이션 재생 (1회)
    │
    ▼
[4] LifeSpan 만료
    │
    └─ 액터 Destroy
        └─ ADRCharacterBase::Destroyed()
            └─ 정리 작업
```

---

### J. 스턴 (외부 요인 — 기본 폼)

```
[1] 플레이어의 Lightning 공격 적중
    │
    └─ ExecCalc_Damage
        ├─ Damage.Lightning → Debuff.Stun 매핑
        ├─ 디버프 확률 성공
        └─ Debuff Stun GE 적용
    │
    ▼
[2] C++: ADRArmadilloEnemy::StunTagChanged(Debuff.Stun, 1)
    │
    ├─ Super::StunTagChanged() — ADREnemy::StunTagChanged()
    │   ├─ bIsStunned = true
    │   ├─ BB "Stunned" = true
    │   ├─ BB "FirstAttacker" 클리어
    │   ├─ BB "HasFirstAttacker" = false
    │   ├─ BB "TargetToFollow" 클리어
    │   └─ OnRep_Stunned() → StunDebuffComponent 활성화 (VFX)
    │
    ├─ CharacterMovement->MaxWalkSpeed = StunnedMoveSpeed (0)
    │
    └─ ★ bIsRolling 확인 → false (기본 폼이므로) → 추가 처리 없음
    │
    ▼
[3] 스턴 상태
    │
    ├─ BT: [2] Stunned == true → Wait 분기
    │
    ├─ AnimBP: ABP_Armadillo_Basic
    │   └─ State Machine: bIsStunned == true → Stun 상태 진입
    │       └─ Stun 애니메이션 Loop 재생
    │
    └─ [스턴 지속시간 경과]
    │
    ▼
[4] 스턴 해제
    │
    └─ GE 만료 → StunTagChanged(Debuff.Stun, 0)
        ├─ bIsStunned = false
        ├─ BB "Stunned" = false
        ├─ OnRep_Stunned() → StunDebuffComponent 비활성화
        ├─ CharacterMovement->MaxWalkSpeed 복원
        │
        └─ AnimBP: bIsStunned == false → Locomotion으로 Blend Out
            └─ 정상 행동 재개
```

---

### K. 스턴 (외부 요인 — 돌진 중)

```
[1] 돌진 중 플레이어의 스턴 공격 적중
    │
    └─ Debuff Stun GE 적용
    │
    ▼
[2] C++: ADRArmadilloEnemy::StunTagChanged(Debuff.Stun, 1)
    │
    ├─ Super::StunTagChanged()
    │   ├─ bIsStunned = true
    │   ├─ BB 업데이트 (Stunned, 타겟 클리어)
    │   └─ OnRep_Stunned() → VFX
    │
    ├─ ★ bIsRolling == true 확인 → 돌진 중단 필요!
    │   │
    │   └─ StopRollCharge()
    │       ├─ bIsRolling = false
    │       ├─ CurrentRollSpeed = 0
    │       └─ Velocity = Zero
    │
    ├─ CharacterMovement->MaxWalkSpeed = 0
    │
    └─ GA_ArmadilloRollCharge: 외부에서 CancelAbility() 호출
        └─ GA 즉시 종료 (쿨타임은 적용 여부 결정 필요)
    │
    ▼
[3] 스턴 상태 (볼 폼 유지)
    │
    ├─ AnimBP: ABP_Armadillo_Ball
    │   └─ bIsStunned == true → Idle (볼 폼 Stun = Idle)
    │
    └─ [스턴 지속시간 경과]
    │
    ▼
[4] 스턴 해제 후 기본 폼 복귀
    │
    ├─ StunTagChanged(Debuff.Stun, 0) → bIsStunned = false
    │
    ├─ ★ 스턴 진입 시점(StunTagChanged)에서 이미 즉시 기본 폼으로 강제 전환 완료
    │   └─ 애니메이션 없이 메시 교체 + 캡슐 복원 (C++ StunTagChanged 내부 처리)
    │   └─ 스턴 해제 시점에는 이미 기본 폼 → 추가 복귀 불필요
    │
    └─ BT 정상 재개
```

---

### L. 벽 스턴 (넉백으로 벽에 충돌)

```
[1] 플레이어의 넉백 공격 적중
    │
    ├─ ExecCalc_Damage에서 KnockbackForce 적용
    │   └─ LaunchCharacter() 또는 Velocity 직접 설정
    │
    └─ C++: ADREnemy::SetKnockbackState(true)
        └─ bIsBeingKnockedBack = true
    │
    ▼
[2] 넉백 이동 중 벽에 충돌
    │
    └─ C++: ADREnemy::OnHit(HitComponent, OtherActor, ...)
        │
        ├─ 조건 확인:
        │   ├─ bIsBeingKnockedBack == true ✓
        │   ├─ NormalImpulse의 속도 >= MinSpeedForStun (50) ✓
        │   ├─ OtherActor가 StaticMeshActor (벽) ✓
        │   ├─ OtherActor가 Floor 아님 ✓
        │   └─ bIsStunImmune == false ✓
        │
        └─ ApplyWallStun()
            ├─ Stun GE 적용 (WallStunDuration = 5초)
            │   └─ StunTagChanged → bIsStunned = true (J와 동일)
            │
            ├─ bIsStunImmune = true
            └─ SetTimer(EndStunImmunity, StunImmunityDuration = 5초)
                └─ 5초 후: bIsStunImmune = false
    │
    ▼
[3~4] J의 [3]~[4]와 동일 (스턴 상태 → 해제 → 복귀)
    │
    └─ ★ 돌진 중 벽 스턴이면 K의 흐름을 따름
```

---

### M. 광폭화 (Enrage — Phase 3)

```
[1] Health가 EnrageHealthThreshold (20%) 이하로 감소
    │
    └─ C++: PostGameplayEffectExecute에서 체력 비율 확인
        └─ Health / MaxHealth <= 0.2
            └─ TriggerEnrage()
    │
    ▼
[2] C++: ADREnemy::TriggerEnrage()
    │
    ├─ bIsEnraged = true
    ├─ BB "IsEnraged" = true
    ├─ BB "AttackSpeed" = EnrageAttackSpeedMultiplier (0.5 = 2배 빠름)
    │
    ├─ EnrageMovementSpeedGE 적용
    │   └─ 이동 속도 증가 → CharacterMovement->MaxWalkSpeed 증가
    │
    └─ AnimBP: 애니메이션 재생 속도에 AttackSpeed 반영
        └─ (몽타주 PlayRate에 적용)
    │
    ▼
[3] 이후 행동
    │
    └─ BT는 동일하게 실행되지만:
        ├─ 이동 속도 증가 → 추적/순찰이 빨라짐
        └─ 공격 속도 증가 → 근접 공격 몽타주가 빠르게 재생
```

---

### N. 호출 흐름 요약 (주체별)

| 주체 | 담당 역할 | 호출되는 주요 함수/이벤트 |
|------|----------|------------------------|
| **BT** | AI 행동 결정 | Selector 분기 평가, BTT 실행, BTS 주기적 갱신 |
| **BTT_Attack_Armadillo** | 기본 공격 트리거 | TryActivateAbilitiesByTag → GA_ArmadilloBite |
| **BTT_ArmadilloRollAttack** | 돌진 스킬 트리거 | TryActivateAbilitiesByTag → GA_ArmadilloRollCharge |
| **BTS_CheckRollSkillReady** | 쿨타임 확인 | ASC 태그 조회 → BB 갱신 |
| **BTS_FindNearestPlayer** | 타겟 갱신 | AIPerception → BB "TargetToFollow" |
| **GA_ArmadilloBite** | 근접 공격 실행 | 몽타주 재생, AnimNotify에서 Trace, CauseDamage |
| **GA_ArmadilloRollCharge** | 돌진 스킬 실행 + **데미지/스턴 처리** | FindRollTarget, 델리게이트 바인딩, StartFormChange, StartRollCharge, **HandleRollImpact에서 CauseDamage/스턴 GE 적용**, 폼 복귀, 쿨타임 |
| **GA_HitReact** | 피격 반응 | 랜덤 HitReact 몽타주 재생 |
| **C++ ADRArmadilloEnemy** | 폼 전환 + 돌진 물리 + **충돌 결과 전달** | StartFormChange, FinishFormChange, TickRollCharge, DetectRollCollision, **BroadcastRollImpact**, FindRollTarget, **OnRollImpact/OnRollReachedTarget 브로드캐스트** |
| **C++ ADREnemy** | 적 공통 로직 | Die, StunTagChanged, HitReactTagChanged, OnHit(벽 스턴), TriggerEnrage |
| **C++ ADRCharacterBase** | 캐릭터 공통 | MulticastHandleDeath, Dissolve, RepNotify(Stun/Burn) |
| **ABP_Armadillo_Basic** | 기본 폼 애니메이션 | Locomotion(BS), Stun(Loop), Death, 몽타주 재생 |
| **ABP_Armadillo_Ball** | 볼 폼 애니메이션 | Idle, Roll(Loop), 몽타주 재생 |
| **AN_FinishFormChange** | 폼 전환 완료 알림 | FinishFormChange() 호출 → 메시 교체 |
| **GE (DamageEffectClass)** | 돌진 데미지 | GA의 DamageEffectClass에 설정된 기존 범용 GE → ExecCalc_Damage |
| **GE_ArmadilloRollStun_Player** | 플레이어 스턴 | Debuff.Stun 1초 (GA에서 적용) |
| **GE_ArmadilloRollStun_Self** | 본인 스턴 | Debuff.Stun 1.5초 (GA에서 적용) |
| **GE_Cooldown_ArmadilloRollCharge** | 스킬 쿨타임 | Cooldown 태그 10초 (GA에서 적용) |
| **ExecCalc_Damage** | 데미지 계산 | 기본 데미지 + 디버프 확률 판정 |

**핵심 책임 분리 원칙**:
```
C++ (ADRArmadilloEnemy)              GA Blueprint (GA_ArmadilloRollCharge)
──────────────────────────          ──────────────────────────────────────
폼 전환 메시 교체                    타겟 검색 요청 (FindRollTarget 호출)
돌진 이동/가속 물리                  타겟 위치 저장
전방 SphereTrace 충돌 감지           델리게이트 바인딩
충돌 범위 OverlapMulti 수집          HandleRollImpact에서:
FRollImpactResult 구성                ├─ CauseDamage() (부모 인프라)
OnRollImpact 브로드캐스트             ├─ 스턴 GE 적용
OnRollReachedTarget 브로드캐스트      ├─ 클렌저 1.5배 데미지 계산
                                      └─ 본인 스턴 GE 적용
❌ GE 생성/적용하지 않음             폼 복귀 요청 (StartFormChange 호출)
❌ 데미지 계산하지 않음              쿨타임 적용 (CommitAbilityCooldown)
❌ 스턴 적용하지 않음                EndAbility
```
