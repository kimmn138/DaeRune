# 신규 적 캐릭터 2종 구현 계획 (Plan2.md)

기존 `ADREnemy` 시스템을 기반으로, **DragonFly(공중 비행 적)**과 **Armadillo(폼 전환 적)** 2종의 새로운 적 캐릭터 구현 방법을 상세히 기술한다.

---

## 에셋 현황

### DragonFly (잠자리)
- **스켈레탈 메시**: `Content/DaeRuneAssets/Characters/Enemy/DragonFly/Dragonfly_v0_1_1.uasset`
- **애니메이션**: Idle, Walk, BasicAttack, Death, HitReact1~3
- **블렌드 스페이스**: BS_IdleWalk
- **머티리얼**: Black, Gray01, Red, White
- **BP/ABP**: `Content/Blueprints/Character/Enemy/DragonFly/BP_DragonFly.uasset`, `ABP_DragonFly.uasset` (이미 생성됨)

### Armadillo (아르마딜로)
- **BasicForm 스켈레탈 메시**: `Content/DaeRuneAssets/Characters/Enemy/Armadilo/BasicForm/Armadillo_v0_1_2.uasset`
- **BasicForm 애니메이션**: Idle, Walk, BasicAttack, Death, HitReact1~3, Stun, FormChangeBtoD, FormChangeDtoB
- **RollForm 스켈레탈 메시**: `Content/DaeRuneAssets/Characters/Enemy/Armadilo/RollForm/Armadillo_Ball_v0_1_0.uasset`
- **RollForm 애니메이션**: Idle, Roll, FormChangeBtoD, FormChangeDtoB
- **별도 스켈레톤**: BasicForm과 RollForm이 서로 다른 Skeleton을 사용 (리깅이 다름)
- **BP/ABP**: 미생성 (새로 만들어야 함)

---

## 적 1: DragonFly (공중 비행 적)

### 개요
- 죽을 때를 제외하고 **항상 공중에 떠다니며 이동**하는 적
- 기존 `ADREnemy`를 C++ 상속 없이, **블루프린트 설정 + CharacterMovementComponent 비행 모드**로 구현
- 사망 시 비행 모드 해제 → 중력에 의해 바닥으로 추락

### 핵심 구현 방식: CharacterMovementComponent Flying Mode

UE5의 `UCharacterMovementComponent`는 `EMovementMode::MOVE_Flying`을 기본 지원한다. 이를 활용하면 별도의 C++ 서브클래스 없이도 공중 이동이 가능하다.

---

### Step 1: BP_DragonFly 블루프린트 설정

#### 1.1 CharacterMovement 컴포넌트 설정

**BP_DragonFly** 블루프린트에서 CharacterMovement 컴포넌트를 다음과 같이 설정:

| 프로퍼티 | 값 | 설명 |
|---------|-----|------|
| `DefaultLandMovementMode` | `Flying` | 기본 이동 모드를 비행으로 설정 |
| `MaxFlySpeed` | `400.0` | 비행 최대 속도 |
| `BrakingDecelerationFlying` | `600.0` | 비행 감속 |
| `NavAgentProps.bCanFly` | `true` | NavMesh 비행 에이전트 활성화 |
| `GravityScale` | `0.0` | 비행 중 중력 무시 |
| `bOrientRotationToMovement` | `false` | 기존 적 회전 방식 유지 (수동 회전) |

#### 1.2 CapsuleComponent 설정

- CapsuleComponent의 크기를 DragonFly 메시에 맞게 조정 (반지름, 높이)
- 비행체이므로 캡슐이 지면에 닿지 않아야 함

#### 1.3 스켈레탈 메시 설정

- Mesh 컴포넌트에 `Dragonfly_v0_1_1` 스켈레탈 메시 할당
- AnimBlueprint에 `ABP_DragonFly` 할당

---

### Step 2: NavMesh 공중 이동 지원

DragonFly가 AI로 공중 이동하려면 NavMesh 위를 걷는 대신 **공중 경로탐색**이 필요하다.

#### 방법 A: NavMesh 기반 + 고도 오프셋 (권장)

가장 간단한 방식. NavMesh 위에서 경로를 탐색하되, 캐릭터가 실제로는 일정 높이에서 비행하도록 한다.

1. **BP_DragonFly에서 CapsuleComponent의 위치를 위로 오프셋** (또는 Mesh를 아래로 오프셋)
2. NavMesh는 바닥에 그대로 두고, AI MoveTo는 NavMesh 경로를 따라가되 Flying 모드이므로 캐릭터가 해당 높이에서 이동
3. **장점**: 기존 NavMesh 인프라 그대로 사용 가능, BehaviorTree 수정 최소화
4. **단점**: 복잡한 3D 경로탐색은 불가 (수직 장애물 회피 등)

구체적 설정:
```
BP_DragonFly:
  - CharacterMovement.DefaultLandMovementMode = Flying
  - CharacterMovement.MaxFlySpeed = 400
  - CharacterMovement.GravityScale = 0
  - CharacterMovement.NavAgentProps.bCanFly = true
  - CapsuleComponent 기본 위치 유지
  - Mesh Z offset = -적절한 값 (메시가 공중에 떠 보이도록)
```

AI가 `MoveToActor`/`MoveToLocation`을 호출하면 CharacterMovement의 Flying 모드가 자동으로 처리한다.

#### 방법 B: NavMesh 없이 직접 이동 (대안)

NavMesh를 사용하지 않고 비헤이비어 트리에서 직접 위치를 계산하여 이동:

1. 커스텀 BTTask에서 타겟 위치 + 고도를 계산
2. `UAIBlueprintHelperLibrary::SimpleMoveToLocation()` 또는 직접 `AddMovementInput()` 사용
3. **장점**: 완전한 3D 이동 가능
4. **단점**: 장애물 회피 직접 구현 필요

**권장**: 현재 게임의 스테이지 구조상 방법 A로 충분할 가능성이 높음.

---

### Step 3: ABP_DragonFly 애니메이션 블루프린트

#### 3.1 상태 머신 구조

```
[Entry] → Locomotion ─────→ Attack
              ↑                  │
              └──────────────────┘

Locomotion 내부:
  BlendSpace(BS_IdleWalk) 사용
  - Speed 변수로 Idle ↔ Walk 블렌딩

별도 슬롯:
  - HitReact (Montage, DefaultSlot)
  - Death (Montage 또는 상태)
```

#### 3.2 핵심 변수

| 변수 | 타입 | 소스 | 용도 |
|------|------|------|------|
| `Speed` | float | `GetVelocity().Size()` | Locomotion 블렌드 |
| `bIsAggroed` | bool | Enemy→bIsAggroed | 전투 상태 전환 |
| `bIsDead` | bool | Blackboard "Dead" | 사망 상태 |

#### 3.3 비행 애니메이션 특성

- **Idle**: 공중에서 날개를 퍼덕이며 호버링 (Dragonfly_Idle)
- **Walk**: 실제로는 "비행 이동" 애니메이션 (Dragonfly_Walk)
- BS_IdleWalk 블렌드 스페이스가 Speed에 따라 Idle↔Walk 블렌딩
- 별도의 착지/이륙 애니메이션 불필요 (항상 비행 상태)

---

### Step 4: 사망 처리 - 비행 해제 및 추락

DragonFly의 핵심 특징: **사망 시 비행 모드를 해제하여 바닥으로 추락**

#### 4.1 구현 방식 (BP_DragonFly 이벤트 그래프 또는 C++)

사망 시 `MulticastHandleDeath`가 호출되면:

```
사망 시퀀스:
1. CharacterMovement->SetMovementMode(MOVE_Falling)
2. CharacterMovement->GravityScale = 1.0  (중력 복원)
3. Death 애니메이션 몽타주 재생
4. Dissolve 이펙트 시작 (기존 시스템 활용)
5. LifeSpan 후 액터 파괴
```

#### 4.2 C++ 구현이 필요한 경우

만약 블루프린트만으로 사망 처리가 깔끔하지 않다면, `ADREnemy`를 상속하는 `ADRFlyingEnemy` 서브클래스를 만들 수 있다:

```cpp
// DRFlyingEnemy.h
UCLASS()
class DAERUNE_API ADRFlyingEnemy : public ADREnemy
{
    GENERATED_BODY()

public:
    ADRFlyingEnemy();

    virtual void MulticastHandleDeath_Implementation(const FVector& DeathImpulse) override;

protected:
    // 비행 높이 (NavMesh 대비 오프셋)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flying")
    float FlyingAltitude = 300.f;

    // 호버링 진폭 (위아래 떠다니는 효과)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flying")
    float HoverAmplitude = 20.f;

    // 호버링 주기
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Flying")
    float HoverFrequency = 2.f;
};
```

```cpp
// DRFlyingEnemy.cpp
ADRFlyingEnemy::ADRFlyingEnemy()
{
    // 비행 모드 기본 설정
    GetCharacterMovement()->DefaultLandMovementMode = EMovementMode::MOVE_Flying;
    GetCharacterMovement()->MaxFlySpeed = 400.f;
    GetCharacterMovement()->BrakingDecelerationFlying = 600.f;
    GetCharacterMovement()->GravityScale = 0.f;
    GetCharacterMovement()->SetCanEverFly(true);
}

void ADRFlyingEnemy::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
    // 비행 모드 해제 → 추락
    GetCharacterMovement()->SetMovementMode(MOVE_Falling);
    GetCharacterMovement()->GravityScale = 1.f;

    // 부모 사망 처리 (Dissolve, 콜리전 비활성화 등)
    Super::MulticastHandleDeath_Implementation(DeathImpulse);
}
```

#### 4.3 권장 방식

**C++ 서브클래스(`ADRFlyingEnemy`) 생성 권장**. 이유:
- `MulticastHandleDeath`의 override가 깔끔
- 비행 관련 프로퍼티를 편집 가능하게 노출
- 향후 다른 비행 적 추가 시 재사용 가능
- 호버링 효과 등 추가 기능 확장 용이

---

### Step 5: BehaviorTree 설정

#### 5.1 새 비헤이비어 트리: BT_EnemyBehaviorTree_DragonFly

기존 Dog의 BT를 기반으로 하되, 비행 적 특성 반영:

```
Root
├── Selector
│   ├── Sequence [Dead Check]
│   │   └── Decorator: Blackboard "Dead" == true
│   │       └── Task: StopMovement
│   │
│   ├── Sequence [Stunned]
│   │   └── Decorator: Blackboard "Stunned" == true
│   │       └── Task: Wait
│   │
│   ├── Sequence [Combat - Has Target]
│   │   └── Decorator: Blackboard "TargetToFollow" IsSet
│   │       ├── Service: BTS_FindNearestPlayer
│   │       ├── Task: MoveToTarget (공중에서 접근)
│   │       └── Task: BTT_Attack (근접 or 원거리)
│   │
│   └── Sequence [Patrol - No Target]
│       ├── Service: BTS_FindNearestPlayer
│       └── Task: BTT_FindPatrolLocation → MoveTo
```

#### 5.2 비행 적 공격 패턴 옵션

DragonFly의 공격 방식에 따라 BTTask를 조정:

**옵션 A: 근접 급강하 공격**
- 타겟 위치로 빠르게 하강 → 공격 → 다시 상승
- 커스텀 BTTask 필요 (`BTT_DiveAttack`)

**옵션 B: 공중 원거리 공격**
- 일정 거리를 유지하며 공중에서 투사체 발사
- 기존 Elementalist BT 패턴 활용 가능

**옵션 C: 공중 근접 공격**
- 비행 높이에서 직접 타겟에 접근하여 공격
- 가장 간단한 구현

---

### Step 6: 호버링(Hovering) 효과 (선택적)

비행 적이 정지 상태에서도 자연스럽게 위아래로 떠다니는 효과:

#### 방법 1: 애니메이션으로 처리 (권장)
- Idle 애니메이션 자체에 위아래 움직임이 포함되어 있다면 추가 작업 불필요
- Dragonfly_Idle 애니메이션에 이미 호버링이 포함되어 있을 가능성 높음

#### 방법 2: C++ Tick에서 사인파 오프셋
```cpp
void ADRFlyingEnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (!bDead)
    {
        // 메시에 사인파 Z 오프셋 적용
        float HoverOffset = FMath::Sin(GetWorld()->GetTimeSeconds() * HoverFrequency) * HoverAmplitude;
        GetMesh()->SetRelativeLocation(FVector(0.f, 0.f, HoverOffset + DefaultMeshZOffset));
    }
}
```

#### 방법 3: ABP에서 Bone Offset
- 애니메이션 블루프린트의 AnimGraph에서 루트 본에 사인파 Z 오프셋 적용
- `Modify Bone` 노드 사용

---

### Step 7: 히트박스 설정

DragonFly는 공중에 떠있으므로 히트박스를 메시에 맞게 조정해야 한다.

#### 7.1 기존 히트박스 시스템 활용

`ADREnemy`의 커스텀 히트박스 시스템 활용:
1. BP_DragonFly에서 `BoxComponent` 또는 `SphereComponent` 추가
2. 컴포넌트 태그에 **"Hitbox"** 추가
3. `SetupHitboxComponents()`가 BeginPlay에서 자동 수집
4. 기본 CapsuleComponent 히트 판정 비활성화됨

#### 7.2 히트박스 배치
- 몸통 중심에 주요 히트박스 (SphereComponent)
- 날개를 포함할지는 밸런스 판단에 따라 결정

---

### DragonFly 구현 요약

| 단계 | 작업 | 방식 | 파일 |
|------|------|------|------|
| 1 | C++ 서브클래스 생성 | `ADRFlyingEnemy` : `ADREnemy` | DRFlyingEnemy.h/cpp |
| 2 | BP_DragonFly 설정 | 부모를 FlyingEnemy로 변경 | BP_DragonFly.uasset |
| 3 | ABP_DragonFly 구성 | BS_IdleWalk + 상태머신 | ABP_DragonFly.uasset |
| 4 | BehaviorTree 생성 | BT_EnemyBehaviorTree_DragonFly | Blueprints/AI/ |
| 5 | CharacterClassInfo 등록 | DA_EnemyCharacterClassInfo에 추가 | Data Asset |
| 6 | 사망 추락 처리 | MulticastHandleDeath override | DRFlyingEnemy.cpp |
| 7 | 히트박스 조정 | 커스텀 히트박스 컴포넌트 | BP_DragonFly |

---

## 적 2: Armadillo (폼 전환 적)

### 개요
- **BasicForm (기본 폼)**: 걸어다니며 일반 공격, 히트 리액션 있음
- **RollForm (볼 폼)**: 공처럼 굴러다니며 돌진 공격, 히트 리액션 없음 (또는 감소)
- 두 폼은 **서로 다른 스켈레탈 메시와 스켈레톤**을 사용 (리깅 자체가 다름)
- 일정 조건에 따라 폼을 전환하며 전투

### 핵심 과제: 서로 다른 스켈레톤의 메시 전환

BasicForm과 RollForm이 **서로 다른 Skeleton**을 사용하므로, 단순히 AnimBP 상태 전환만으로는 불가능하다. `SetSkeletalMesh()` 또는 **듀얼 메시 컴포넌트** 방식이 필요하다.

---

### 폼 전환 구현 방식 비교

#### 방법 A: 듀얼 SkeletalMeshComponent (권장)

2개의 SkeletalMeshComponent를 가지고, 현재 활성 폼의 메시만 보이게 전환:

```
BP_Armadillo
├── CapsuleComponent (루트)
├── BasicFormMesh (SkeletalMeshComponent) - Armadillo_v0_1_2 + ABP_Armadillo_Basic
├── RollFormMesh (SkeletalMeshComponent) - Armadillo_Ball_v0_1_0 + ABP_Armadillo_Roll
├── HealthBar (BillboardWidget)
└── ...기타 컴포넌트
```

**장점**:
- 각 폼에 독립적인 AnimBP 할당 가능
- 전환 시 애니메이션 상태 보존 용이
- 메시 전환이 즉시 발생 (로딩 없음)
- 각 폼의 PhysicsAsset 독립 관리

**단점**:
- 메모리 사용량 약간 증가 (비활성 메시도 로드됨)
- 히트 판정 메시 동기화 필요

#### 방법 B: SetSkeletalMeshAsset() 동적 전환

하나의 SkeletalMeshComponent에서 런타임에 메시와 AnimBP를 교체:

```cpp
GetMesh()->SetSkeletalMeshAsset(NewMesh);
GetMesh()->SetAnimInstanceClass(NewAnimBP);
```

**장점**: 메모리 효율적, 컴포넌트 1개

**단점**:
- 메시 교체 시 순간적으로 T-Pose 발생 가능
- AnimBP 재초기화로 상태 리셋
- PhysicsAsset 재설정 필요
- 전환 애니메이션 구현이 복잡

**결론: 방법 A (듀얼 메시) 권장** - 안정적이고 전환 애니메이션 처리가 깔끔

---

### Step 1: C++ 서브클래스 ADRArmadilloEnemy 생성

```cpp
// DRArmadilloEnemy.h
#pragma once

#include "CoreMinimal.h"
#include "Character/DREnemy.h"
#include "DRArmadilloEnemy.generated.h"

UENUM(BlueprintType)
enum class EArmadilloForm : uint8
{
    BasicForm,
    RollForm
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFormChanged, EArmadilloForm, NewForm);

UCLASS()
class DAERUNE_API ADRArmadilloEnemy : public ADREnemy
{
    GENERATED_BODY()

public:
    ADRArmadilloEnemy();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // 현재 폼 상태
    UPROPERTY(ReplicatedUsing = OnRep_CurrentForm, BlueprintReadOnly, Category = "Form")
    EArmadilloForm CurrentForm = EArmadilloForm::BasicForm;

    // 폼 전환 요청 (서버에서만 호출)
    UFUNCTION(BlueprintCallable, Category = "Form")
    void SwitchForm(EArmadilloForm NewForm);

    // 폼 전환 완료 이벤트
    UPROPERTY(BlueprintAssignable, Category = "Form")
    FOnFormChanged OnFormChanged;

    // 현재 폼 확인
    UFUNCTION(BlueprintPure, Category = "Form")
    bool IsInRollForm() const { return CurrentForm == EArmadilloForm::RollForm; }

    /** Combat Interface Override */
    virtual void Die(const FVector& DeathImpulse) override;

protected:
    virtual void BeginPlay() override;

    // 듀얼 메시 컴포넌트
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Form")
    TObjectPtr<USkeletalMeshComponent> BasicFormMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Form")
    TObjectPtr<USkeletalMeshComponent> RollFormMesh;

    // 폼 전환 지속시간 (전환 애니메이션 길이와 맞춤)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Form")
    float FormTransitionDuration = 1.0f;

    // 볼 폼 유지 시간
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Form")
    float RollFormDuration = 5.0f;

    // 볼 폼 이동 속도 배율
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Form")
    float RollFormSpeedMultiplier = 2.0f;

    // 볼 폼 진입 쿨다운
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Form")
    float FormSwitchCooldown = 8.0f;

    // 폼 전환 중 여부
    UPROPERTY(BlueprintReadOnly, Category = "Form")
    bool bIsTransitioning = false;

    // 폼 전환 몽타주
    UPROPERTY(EditDefaultsOnly, Category = "Form|Animation")
    TObjectPtr<UAnimMontage> BasicToRollMontage;   // Armadillo_FormChangeBtoD

    UPROPERTY(EditDefaultsOnly, Category = "Form|Animation")
    TObjectPtr<UAnimMontage> RollToBasicMontage;   // Armadillo_FormChangeDtoB

    virtual float GetMoveSpeed() override;

private:
    UFUNCTION()
    void OnRep_CurrentForm();

    // 실제 메시 전환 처리
    void ApplyFormSwitch(EArmadilloForm NewForm);

    // 전환 애니메이션 완료 콜백
    void OnFormTransitionComplete();

    // 타이머 핸들
    FTimerHandle FormDurationTimerHandle;
    FTimerHandle FormCooldownTimerHandle;
    bool bFormSwitchOnCooldown = false;
};
```

---

### Step 2: C++ 구현 - 핵심 로직

```cpp
// DRArmadilloEnemy.cpp

ADRArmadilloEnemy::ADRArmadilloEnemy()
{
    // === BasicForm 메시 컴포넌트 생성 ===
    // 기본 GetMesh()는 사용하지 않거나 BasicFormMesh로 대체
    // 방법: GetMesh()를 BasicForm으로 사용하고, RollFormMesh를 추가 생성

    RollFormMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RollFormMesh"));
    RollFormMesh->SetupAttachment(GetRootComponent());
    RollFormMesh->SetVisibility(false);               // 기본적으로 숨김
    RollFormMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    // BasicFormMesh는 기본 GetMesh()를 그대로 사용
    // BP에서 GetMesh()에 Armadillo_v0_1_2 할당
    // BP에서 RollFormMesh에 Armadillo_Ball_v0_1_0 할당
}

void ADRArmadilloEnemy::SwitchForm(EArmadilloForm NewForm)
{
    if (!HasAuthority()) return;              // 서버만
    if (NewForm == CurrentForm) return;       // 같은 폼이면 무시
    if (bIsTransitioning) return;             // 전환 중이면 무시
    if (bDead) return;                        // 사망 상태면 무시

    bIsTransitioning = true;

    // 전환 애니메이션 재생 (현재 활성 메시에서)
    UAnimMontage* TransitionMontage = (NewForm == EArmadilloForm::RollForm)
        ? BasicToRollMontage
        : RollToBasicMontage;

    if (TransitionMontage)
    {
        // 현재 활성 메시의 AnimInstance에서 몽타주 재생
        USkeletalMeshComponent* ActiveMesh = (CurrentForm == EArmadilloForm::BasicForm)
            ? GetMesh() : RollFormMesh;

        if (UAnimInstance* AnimInstance = ActiveMesh->GetAnimInstance())
        {
            float Duration = AnimInstance->Montage_Play(TransitionMontage);

            // 전환 애니메이션의 특정 시점에 메시 스왑
            // 애니메이션 중간 지점(약 50%)에서 실제 메시 전환
            FTimerHandle TransitionTimer;
            GetWorldTimerManager().SetTimer(TransitionTimer, [this, NewForm]()
            {
                ApplyFormSwitch(NewForm);
            }, Duration * 0.5f, false);

            // 전환 완료 처리
            FTimerHandle CompletionTimer;
            GetWorldTimerManager().SetTimer(CompletionTimer, [this]()
            {
                OnFormTransitionComplete();
            }, Duration, false);
        }
    }
    else
    {
        // 몽타주 없으면 즉시 전환
        ApplyFormSwitch(NewForm);
        OnFormTransitionComplete();
    }
}

void ADRArmadilloEnemy::ApplyFormSwitch(EArmadilloForm NewForm)
{
    CurrentForm = NewForm;

    if (NewForm == EArmadilloForm::RollForm)
    {
        // BasicForm 숨기고 RollForm 보이기
        GetMesh()->SetVisibility(false);
        GetMesh()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        RollFormMesh->SetVisibility(true);
        RollFormMesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }
    else
    {
        // RollForm 숨기고 BasicForm 보이기
        RollFormMesh->SetVisibility(false);
        RollFormMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

        GetMesh()->SetVisibility(true);
        GetMesh()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
    }

    // 이동 속도 업데이트
    // GetMoveSpeed()에서 폼에 따라 속도 배율 적용

    OnFormChanged.Broadcast(NewForm);
}

void ADRArmadilloEnemy::OnRep_CurrentForm()
{
    // 클라이언트에서 메시 전환 동기화
    ApplyFormSwitch(CurrentForm);
}

float ADRArmadilloEnemy::GetMoveSpeed()
{
    float BaseSpeed = Super::GetMoveSpeed();
    if (CurrentForm == EArmadilloForm::RollForm)
    {
        return BaseSpeed * RollFormSpeedMultiplier;
    }
    return BaseSpeed;
}

void ADRArmadilloEnemy::Die(const FVector& DeathImpulse)
{
    // 사망 시 반드시 BasicForm으로 복귀 (사망 애니메이션이 BasicForm에만 있으므로)
    if (CurrentForm == EArmadilloForm::RollForm)
    {
        // 즉시 BasicForm으로 전환 (전환 애니메이션 없이)
        ApplyFormSwitch(EArmadilloForm::BasicForm);
    }

    Super::Die(DeathImpulse);
}
```

---

### Step 3: 듀얼 AnimBP 시스템

각 폼에 독립적인 AnimBP가 필요하다.

#### 3.1 ABP_Armadillo_Basic (기본 폼 AnimBP)

스켈레톤: `Armadillo_v0_1_2_Skeleton`

```
상태 머신:
[Entry] → Idle/Walk ──→ Attack ──→ Idle/Walk
              │                        ↑
              ├── HitReact ────────────┘
              │
              └── FormChange (BtoD) ──→ [폼 전환 시 정지]

Idle/Walk:
  - Speed 기반 Idle↔Walk 블렌딩 (BS 없으면 직접 블렌드)
  - 사용 애니메이션: Armadillo_Idle, Armadillo_Walk

Attack:
  - Armadillo_BasicAttack 몽타주

HitReact:
  - Armadillo_HitReact1~3 중 랜덤

Stun:
  - Armadillo_Stun 애니메이션

FormChange:
  - Armadillo_FormChangeBtoD (Basic→Roll 전환 시작)
  - Armadillo_FormChangeDtoB (Roll→Basic 전환 완료 후 재생)
```

#### 3.2 ABP_Armadillo_Roll (볼 폼 AnimBP)

스켈레톤: `Armadillo_Ball_v0_1_0_Skeleton`

```
상태 머신:
[Entry] → Idle ──→ Roll ──→ Idle
              │
              └── FormChange (DtoB) ──→ [폼 전환 시 정지]

Idle:
  - Armadillo_Ball_Idle

Roll (이동 중):
  - Armadillo_Ball_Roll
  - Speed > 0 일 때 활성

FormChange:
  - Armadillo_Ball_FormChangeBtoD (Roll 폼 진입 시 초기 애니메이션)
  - Armadillo_Ball_FormChangeDtoB (Roll→Basic 전환 시작)
```

---

### Step 4: 폼 전환 시퀀스 상세

전환 과정은 매끄러운 시각적 연출이 중요하다.

#### 4.1 BasicForm → RollForm 전환 시퀀스

```
시간축: ──────────────────────────────────────────→

[1] BasicForm 메시에서 "FormChangeBtoD" 몽타주 재생
    (아르마딜로가 몸을 웅크리는 애니메이션)

[2] 몽타주 50% 지점에서:
    - BasicForm 메시 숨김 (SetVisibility false)
    - RollForm 메시 표시 (SetVisibility true)
    - CapsuleComponent 크기 조정 (볼 형태에 맞게 축소)

[3] RollForm 메시에서 "Ball_FormChangeBtoD" 애니메이션 재생
    (볼 형태가 완성되는 마무리 애니메이션)

[4] 전환 완료 → RollForm 이동 시작
    - 이동 속도 x2.0 적용
    - 히트 리액션 비활성화 (또는 무시)
    - Blackboard에 "IsRollForm" = true 설정
```

#### 4.2 RollForm → BasicForm 전환 시퀀스

```
[1] RollForm 메시에서 "Ball_FormChangeDtoB" 애니메이션 재생
    (볼이 풀리기 시작하는 애니메이션)

[2] 애니메이션 50% 지점에서:
    - RollForm 메시 숨김
    - BasicForm 메시 표시
    - CapsuleComponent 크기 복원

[3] BasicForm 메시에서 "FormChangeDtoB" 몽타주 재생
    (아르마딜로가 일어서는 애니메이션)

[4] 전환 완료 → BasicForm 이동 시작
```

#### 4.3 CapsuleComponent 크기 조정

```cpp
void ADRArmadilloEnemy::UpdateCapsuleForForm(EArmadilloForm Form)
{
    UCapsuleComponent* Capsule = GetCapsuleComponent();
    if (Form == EArmadilloForm::RollForm)
    {
        // 볼 폼: 구형에 가까운 캡슐
        Capsule->SetCapsuleSize(RollFormCapsuleRadius, RollFormCapsuleHalfHeight);
    }
    else
    {
        // 기본 폼: 기본 캡슐 크기
        Capsule->SetCapsuleSize(BasicFormCapsuleRadius, BasicFormCapsuleHalfHeight);
    }
}
```

---

### Step 5: BehaviorTree - 폼 전환 AI 로직

#### 5.1 새 비헤이비어 트리: BT_EnemyBehaviorTree_Armadillo

```
Root
├── Selector
│   ├── Sequence [Dead]
│   │   └── Decorator: BB "Dead" == true
│   │       └── Task: StopMovement
│   │
│   ├── Sequence [Stunned]
│   │   └── Decorator: BB "Stunned" == true
│   │       └── Task: Wait
│   │
│   ├── Sequence [Form Transitioning]
│   │   └── Decorator: BB "IsTransitioning" == true
│   │       └── Task: Wait (전환 중 행동 정지)
│   │
│   ├── Sequence [RollForm Behavior]
│   │   └── Decorator: BB "IsRollForm" == true
│   │       ├── Service: BTS_FindNearestPlayer
│   │       ├── Task: BTT_RollTowardTarget (타겟 방향으로 굴러감)
│   │       └── Task: BTT_RollAttack (충돌 시 데미지)
│   │
│   ├── Sequence [BasicForm Combat]
│   │   └── Decorator: BB "TargetToFollow" IsSet
│   │       ├── Service: BTS_FindNearestPlayer
│   │       ├── Service: BTS_CheckFormSwitchCondition (폼 전환 조건 체크)
│   │       ├── Task: MoveToTarget
│   │       └── Task: BTT_Attack
│   │
│   └── Sequence [Patrol]
│       ├── Service: BTS_FindNearestPlayer
│       └── Task: BTT_FindPatrolLocation → MoveTo
```

#### 5.2 폼 전환 조건 (BTS_CheckFormSwitchCondition)

AI 서비스에서 폼 전환 시점을 결정:

```
전환 조건 (BasicForm → RollForm):
- 타겟과의 거리가 일정 범위 이상 (예: 800 units 이상)
- 쿨다운 완료
- 현재 공격/히트리액션 중이 아님
- 체력이 일정 이상 (예: 50% 이상)

전환 조건 (RollForm → BasicForm):
- 볼 폼 지속시간 만료
- 타겟에 충분히 접근 (예: 200 units 이내)
- 스턴 상태에 빠짐
```

#### 5.3 커스텀 BTTask: BTT_RollTowardTarget

볼 폼에서의 돌진 이동 태스크:

```
BTT_RollTowardTarget:
1. 타겟 방향으로 빠른 속도로 이동 (MaxWalkSpeed * RollFormSpeedMultiplier)
2. 이동 경로는 직선 (NavMesh 기반이지만 빠른 속도)
3. 타겟에 도달하거나 벽에 충돌 시 완료
4. 충돌 시 데미지 적용 + 넉백
```

---

### Step 6: 볼 폼 전투 메카닉

#### 6.1 볼 폼 공격: 돌진 데미지

볼 폼에서의 공격은 **접촉 데미지** 방식:

```cpp
// 볼 폼 충돌 감지 (OverlapComponent 또는 OnHit)
UPROPERTY(VisibleAnywhere, Category = "Form|Combat")
TObjectPtr<USphereComponent> RollDamageCollision;

// RollDamageCollision 설정:
// - 볼 폼일 때만 활성화
// - ECC_Pawn과 Overlap 감지
// - 플레이어와 접촉 시 데미지 GE 적용
```

#### 6.2 볼 폼 방어: 데미지 감소

볼 폼에서는 일정 데미지 감소 효과:

```
옵션 A: GameplayEffect로 방어력 버프
  - 볼 폼 진입 시 데미지 감소 GE 적용
  - 볼 폼 해제 시 GE 제거

옵션 B: AttributeSet에서 폼 체크
  - PostGameplayEffectExecute에서 CurrentForm 체크
  - 볼 폼이면 데미지에 감소 배율 적용
```

#### 6.3 히트 리액션 처리

```
BasicForm: 일반적인 히트 리액션 (HitReact1~3)
RollForm:
  - 히트 리액션 무시 (볼 상태에서는 경직 없음)
  - 또는 약간 튕겨나는 효과만 적용
  - Blackboard "HitReacting" 설정하지 않음
```

---

### Step 7: 네트워크 복제

#### 7.1 복제 프로퍼티

```cpp
void ADRArmadilloEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION_NOTIFY(ADRArmadilloEnemy, CurrentForm, COND_None, REPNOTIFY_Always);
}
```

#### 7.2 복제 흐름

```
서버:
  1. AI가 폼 전환 결정
  2. SwitchForm() 호출
  3. 전환 애니메이션 재생 (Multicast)
  4. CurrentForm 변경 → 자동 복제

클라이언트:
  1. OnRep_CurrentForm() 호출
  2. ApplyFormSwitch()로 메시 전환
  3. 전환 애니메이션은 Multicast로 동기화됨
```

---

### Step 8: 사망 처리

```
사망 시:
1. 현재 RollForm이면 → 즉시 BasicForm으로 전환 (애니메이션 없이)
2. BasicForm의 Death 애니메이션 재생 (Armadillo_Death)
3. 기존 Dissolve 시스템 적용
4. LifeSpan 후 파괴
```

---

### Armadillo 구현 요약

| 단계 | 작업 | 방식 | 파일 |
|------|------|------|------|
| 1 | C++ 서브클래스 생성 | `ADRArmadilloEnemy` : `ADREnemy` | DRArmadilloEnemy.h/cpp |
| 2 | BP_Armadillo 블루프린트 생성 | 듀얼 메시 설정 | Content/Blueprints/Character/Enemy/Armadillo/ |
| 3 | ABP_Armadillo_Basic 생성 | BasicForm 전용 AnimBP | Content/Blueprints/Character/Enemy/Armadillo/ |
| 4 | ABP_Armadillo_Roll 생성 | RollForm 전용 AnimBP | Content/Blueprints/Character/Enemy/Armadillo/ |
| 5 | BehaviorTree 생성 | BT_EnemyBehaviorTree_Armadillo | Blueprints/AI/ |
| 6 | BTTask/Service 생성 | 폼 전환 AI 로직 | Blueprints/AI/ |
| 7 | 볼 폼 데미지 시스템 | 접촉 데미지 + 방어 GE | GE 블루프린트 |
| 8 | CharacterClassInfo 등록 | DA에 추가 | Data Asset |
| 9 | 네트워크 복제 | CurrentForm RepNotify | DRArmadilloEnemy.cpp |

---

## 공통 작업

### DA_EnemyCharacterClassInfo 업데이트

두 적 모두 CharacterClassInfo 데이터 에셋에 등록해야 한다:

```
DragonFly:
  - CharacterClass: 신규 Enum 추가 또는 기존 Warrior/Ranger 활용
  - PrimaryAttributes GE: DragonFly 전용 (체력, 이동속도 등)
  - StartupAbilities: 비행 공격 어빌리티

Armadillo:
  - CharacterClass: 신규 Enum 추가 또는 기존 활용
  - PrimaryAttributes GE: Armadillo 전용 (높은 체력, 중간 이동속도)
  - StartupAbilities: 기본 공격 + 돌진 공격 어빌리티
```

### Gameplay Tags 추가

```cpp
// DRGameplayTags.h/cpp에 추가할 태그들

// DragonFly
GameplayTags.AddTag(FDRGameplayTags::State_Flying, "State.Flying", "비행 상태");

// Armadillo
GameplayTags.AddTag(FDRGameplayTags::State_RollForm, "State.RollForm", "볼 폼 상태");
GameplayTags.AddTag(FDRGameplayTags::State_FormTransitioning, "State.FormTransitioning", "폼 전환 중");
GameplayTags.AddTag(FDRGameplayTags::Abilities_RollAttack, "Abilities.RollAttack", "돌진 공격");
GameplayTags.AddTag(FDRGameplayTags::Damage_Roll, "Damage.Roll", "돌진 데미지");
```

### Blackboard 키 추가

```cpp
// DRBlackboardKeys 네임스페이스에 추가
namespace DRBlackboardKeys
{
    // Armadillo 폼 시스템
    inline const FName IsRollForm = TEXT("IsRollForm");
    inline const FName IsTransitioning = TEXT("IsTransitioning");
    inline const FName FormSwitchCooldownReady = TEXT("FormSwitchCooldownReady");
}
```

---

## 구현 우선순위

### Phase 1: 기본 틀 (높은 우선순위)
1. `ADRFlyingEnemy` C++ 클래스 생성
2. `ADRArmadilloEnemy` C++ 클래스 생성
3. BP_DragonFly 부모 클래스 변경 및 비행 설정
4. BP_Armadillo 블루프린트 생성 및 듀얼 메시 설정

### Phase 2: 애니메이션 (높은 우선순위)
5. ABP_DragonFly 상태 머신 구성
6. ABP_Armadillo_Basic 생성
7. ABP_Armadillo_Roll 생성
8. 폼 전환 애니메이션 연결

### Phase 3: AI (중간 우선순위)
9. BT_EnemyBehaviorTree_DragonFly 생성
10. BT_EnemyBehaviorTree_Armadillo 생성
11. 커스텀 BTTask/Service 생성 (폼 전환, 돌진)

### Phase 4: 전투 시스템 (중간 우선순위)
12. DragonFly 공격 어빌리티
13. Armadillo 볼 폼 접촉 데미지
14. GameplayEffect 설정 (속성, 버프)
15. 히트박스 조정

### Phase 5: 폴리싱 (낮은 우선순위)
16. 사망 연출 (DragonFly 추락, Armadillo 폼 복귀)
17. VFX/SFX 추가
18. 밸런싱 (체력, 데미지, 속도, 쿨다운)
19. 네트워크 테스트

---

## 파일 생성 목록

### C++ 파일 (새로 생성)
| 파일 | 설명 |
|------|------|
| `Source/DaeRune/Public/Character/DRFlyingEnemy.h` | 비행 적 서브클래스 헤더 |
| `Source/DaeRune/Private/Character/DRFlyingEnemy.cpp` | 비행 적 서브클래스 구현 |
| `Source/DaeRune/Public/Character/DRArmadilloEnemy.h` | 아르마딜로 적 서브클래스 헤더 |
| `Source/DaeRune/Private/Character/DRArmadilloEnemy.cpp` | 아르마딜로 적 서브클래스 구현 |

### 블루프린트 파일 (새로 생성/수정)
| 파일 | 설명 |
|------|------|
| `BP_DragonFly` | 부모 클래스를 ADRFlyingEnemy로 변경 |
| `ABP_DragonFly` | 비행 상태 머신 구성 (이미 존재, 수정) |
| `BP_Armadillo` | 아르마딜로 블루프린트 (새로 생성) |
| `ABP_Armadillo_Basic` | 기본 폼 AnimBP (새로 생성) |
| `ABP_Armadillo_Roll` | 볼 폼 AnimBP (새로 생성) |
| `BT_EnemyBehaviorTree_DragonFly` | 비행 적 비헤이비어 트리 |
| `BT_EnemyBehaviorTree_Armadillo` | 아르마딜로 비헤이비어 트리 |
