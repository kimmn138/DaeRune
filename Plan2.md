# 신규 적 캐릭터 2종 구현 계획 (Plan2.md)

기존 `ADREnemy` 시스템을 기반으로, **DragonFly(공중 비행 적)**과 **Armadillo(폼 전환 적)** 2종의 새로운 적 캐릭터 구현 방법을 상세히 기술한다.

---

## 에셋 현황

### DragonFly (잠자리)
- **스켈레탈 메시**: `Content/DaeRuneAssets/Characters/Enemy/DragonFly/Dragonfly_v0_1_1.uasset`
- **애니메이션**: Idle, Walk, BasicAttack, Death, HitReact1~3, **Stun (1초, 스킬 사용 후 경직용 신규)**
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

DragonFly는 고정된 높이로 공중을 이동하며, 원거리 투사체로 플레이어를 공격하는 적이다.

#### 주요 특성
| 특성 | 설명 |
|------|------|
| 이동 방식 | **일정한 높이(Z 고정)**로 공중 비행 |
| 고도 유지 | 바닥의 **높낮이와 무관하게 일정한 고도**를 유지 (경사/언덕/계단 등에 영향 X) |
| 충돌 처리 | 지형지물(벽, 기둥, 천장, 장애물)과는 **정상적으로 충돌**함 |
| 기본 공격 | 타겟의 `Location`을 향한 **직선 투사체 발사** |
| 스킬 트리거 | 기본 공격 **5회마다** 자동 발동 |
| 스킬 효과 | **0.1초 간격으로 기본 공격 10발**을 1초간 연사 |
| 스킬 후 경직 | 스킬 사용 후 **3초간 이동 및 공격 불가** |
| 경직 애니메이션 | **1초짜리 Stun 애니메이션을 Loop 재생** (경직이 끝날 때까지 반복, 총 3초 = 약 3회 루프) |
| 사망 | Death 애니메이션은 **정지형**(애니 자체는 위치 변화 없음). 사망 시점에 **중력을 활성화**해 Death 애니메이션을 재생하면서 **자유낙하** |

#### 사용 애니메이션 에셋
- `Dragonfly_Idle` - 정지 호버링
- `Dragonfly_Walk` - 비행 이동
- `Dragonfly_BasicAttack` - 기본 공격 (투사체 발사 타이밍)
- `Dragonfly_HitReact1/2/3` - 피격 리액션
- `Dragonfly_Death` - 사망 (정지형)
- **`Dragonfly_Stun`** - **스킬 사용 후 경직 애니메이션 (1초, Loop 재생, 신규 추가)**
- `BS_IdleWalk` - Idle↔Walk 블렌드 스페이스

#### 경직 애니메이션 설계 (1초 애니 × 3회 Loop)

Stun 애니메이션은 1초 길이인데 경직 지속시간은 3초이므로 **Loop 재생**으로 경직이 끝날 때까지 자연스럽게 반복한다:

```
시간축: 0s ───────── 1s ───────── 2s ───────── 3s
         │           │           │           │
         │ Stun 1회  │ Stun 2회  │ Stun 3회  │
         │  (Loop)  │  (Loop)  │  (Loop)  │
         ▼           ▼           ▼           ▼
     스킬 종료                              경직 해제
     경직 시작                            Locomotion 복귀
```

- **0~3초**: `Dragonfly_Stun` 애니메이션이 **Loop=true**로 반복 재생 (지속적으로 지친 동작)
- **3초 시점**: 경직 GE 만료 → `bIsLockedDown = false` → AnimBP State가 Locomotion으로 Blend Out
- **이점**: 경직 해제 시점이 애니메이션의 어느 프레임에 위치하든 Transition Blend가 자연스럽게 처리됨. 경직 지속시간이 3초가 아닌 다른 값으로 조정되어도 별도 대응 불필요.

---

### 설계 결정: BTT_Attack 패턴 채택

#### 질문
> 기존 Dog는 `BTT_Attack`에서 확률 기반으로 기본 공격/스킬 태그를 선택해 `TryActivateAbilitiesByTag` 노드를 호출한다. DragonFly의 "5회마다 스킬"도 이 패턴으로 구현할지?

#### 판단: **채택 (YES)**

DragonFly의 공격 선택 로직을 **`BTT_Attack_DragonFly` Blueprint Task**에 통합한다. 그 근거는:

| 고려사항 | 비고 |
|----------|------|
| **아키텍처 일관성** | Dog가 BT에서 공격 결정을 하므로, DragonFly도 동일 패턴이면 유지보수/이해가 쉬움 |
| **책임 분리** | AI의 행동 결정은 BT가, 어빌리티 실행은 GA가 담당 (SoC 원칙) |
| **디자이너 접근성** | 5회 → N회로 조정하려면 BT/Blackboard 값만 바꾸면 됨 (C++ 재컴파일 불필요) |
| **상태 가시성** | Blackboard의 `BasicAttackCount` 값이 Unreal Editor에서 실시간으로 보임 (디버깅 용이) |
| **C++ 경량화** | 카운터 관리/자동 트리거 로직이 C++에서 사라져 `ADRFlyingEnemy`가 비행/경직에만 집중 |
| **Dog와의 차이** | Dog = 확률 기반, DragonFly = 결정론적 카운터 → 동일 패턴 내 변형만 다름 |

#### 결정 사항
- **카운터는 Blackboard**에 `BasicAttackCount` (int) 키로 보관
- **BTT_Attack_DragonFly**가 카운터를 읽고 기본 공격/스킬 중 선택
  - `BasicAttackCount < 5` → `Abilities.DragonFly.BasicAttack` 태그 활성화, 카운터 +1
  - `BasicAttackCount >= 5` → `Abilities.DragonFly.BurstSkill` 태그 활성화, 카운터 = 0
- **C++의 자동 트리거 로직 제거**: `NotifyBasicAttackExecuted()`, `TryActivateAbilitiesByTag()` 삭제
- **C++은 다음만 담당**: 비행 고도 유지, 충돌 처리, `StartSkillLockdown()` BP-Callable 함수 제공, 사망 처리

---

### 설계 결정: Gameplay Ability 구현 방식

#### 사용자 지정 요구사항
- **GA는 Blueprint 기반**으로 작성 (C++ 어빌리티 클래스 생성 불필요)
- **`PlayMontageAndWait` 노드** 사용 (Blueprint 그래프)
- **투사체 발사 시점은 AnimNotify**로 제어
- 코드로 해야만 하는 부분만 C++, 나머지는 Blueprint 노드로 작성

#### 채택 구조

| 어빌리티 | 구현 방식 | 부모 클래스 |
|----------|----------|-------------|
| `GA_DragonFly_BasicAttack` | **Blueprint** | `UDRDamageGameplayAbility` (기존 C++) |
| `GA_DragonFly_BurstSkill` | **Blueprint** | `UDRDamageGameplayAbility` (기존 C++) |

- **부모 C++ 클래스는 그대로 활용**: 기존 `MakeDamageEffectParamsFromClassDefaults()`, `GetDamageAtLevel()` 등 데미지 관련 유틸리티를 상속받아 Blueprint에서 노드로 호출
- **기본 공격 GA**: `PlayMontageAndWait` + `WaitGameplayEvent(Event.Montage.ProjectileShoot)` + 투사체 스폰 노드
- **스킬 GA**: `For Loop (0~9)` + `Delay 0.1s` + 투사체 스폰 노드 + 루프 완료 후 `StartSkillLockdown` 호출

---

### 설계 결정: GameplayTag 추가 범위

#### 사용자 지정 요구사항
- `DRGameplayTags`에 **새로 등록할 태그는 `State.LockedDown`과 `Effects.CannotAttack` 2개뿐**
- **어빌리티/이벤트/몽타주 태그는 기존 Dog 적 구현을 참고하여 사용자가 직접 관리**
  - 즉, `Abilities.DragonFly.BasicAttack`, `Abilities.DragonFly.BurstSkill`, `Event.Montage.ProjectileShoot` 등은 사용자가 정의/추가

#### 플랜 문서에서의 취급 방침

| 구분 | 처리 방식 |
|------|-----------|
| 경직/효과 태그 (`State.LockedDown`, `Effects.CannotAttack`) | **Step 10에서 상세 정의 (C++ 코드 포함)** |
| DragonFly 어빌리티/이벤트 태그 | **Step 10에서는 정의하지 않음**. 본 문서의 타 Step에서 인용되는 태그 이름(`Abilities.DragonFly.BasicAttack` 등)은 **참고용 예시** |
| 실제 태그명 일치성 | 사용자가 정한 실제 태그명이 `GA`, `BTT_Attack_DragonFly`, `GE_DragonFly_Lockdown`에서 동일하게 참조되도록 통일 |

> 본 문서는 가독성을 위해 예시 태그 이름을 `Abilities.DragonFly.BasicAttack` / `Abilities.DragonFly.BurstSkill` / `Event.Montage.ProjectileShoot`로 표기하지만, **이는 사용자가 실제로 선택한 태그명으로 1:1 치환하여 구현**하면 된다.

---

### 전체 아키텍처 (갱신)

```
┌─────────────────────────────────────────────────────────────┐
│                    ADRFlyingEnemy (C++)                     │
│   - Flying 이동 모드 + Z 좌표 고정 (FixedAltitude)          │
│   - 바닥 높낮이 무시 (일정 고도 유지, 지형지물 충돌 O)      │
│   - StartSkillLockdown() BP-Callable (경직 시작)            │
│   - EndSkillLockdown() 내부 타이머 콜백                     │
│   - IsLockedDown() BP-Pure (AnimBP/BT가 폴링)               │
│   - 사망 시 중력 추락 + Death 애니 재생 (MulticastHandleDeath) │
│   ※ 공격 카운터/자동 트리거는 C++에 없음 (BT에서 관리)      │
└─────────────────────────────────────────────────────────────┘
         ▲
         │ 상속
         │
   ┌──────────────┐
   │ BP_DragonFly │ ← 메시, AnimBP, CapsuleSize, 파라미터 값 설정
   └──────────────┘

┌─────────────────────────────────────────────────────────────┐
│             GA_DragonFly_BasicAttack (Blueprint GA)         │
│   부모: UDRDamageGameplayAbility (C++)                      │
│   - PlayMontageAndWait(Dragonfly_BasicAttack)               │
│   - WaitGameplayEvent(Event.Montage.ProjectileShoot)        │
│   - 이벤트 수신 시: 타겟 Location 방향 투사체 스폰          │
│   - OnCompleted/OnInterrupted → EndAbility                  │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│             GA_DragonFly_BurstSkill (Blueprint GA)          │
│   부모: UDRDamageGameplayAbility (C++)                      │
│   - For Loop (0..9) → Delay(0.1s) → Spawn Projectile        │
│   - 총 10발 발사 (0.0s / 0.1s / ... / 0.9s)                 │
│   - 루프 완료 후 Owner->StartSkillLockdown() 호출           │
│   - EndAbility                                              │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                   BP_DragonFlyProjectile                    │
│   부모: ADRProjectile (기존 C++)                            │
│   - ProjectileMovement: 직선 이동 (Homing=false, Gravity=0) │
│   - OnSphereOverlap 시 데미지 GE 적용 (부모 기능)           │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│              BT_EnemyBehaviorTree_DragonFly                 │
│   - Root Selector                                           │
│     ├─ [Dead/Stunned/LockedDown 체크 → Wait]                │
│     ├─ [타겟 있음]                                          │
│     │    ├─ BTT_RotateToFaceTarget                          │
│     │    └─ BTT_Attack_DragonFly ← 카운터 기반 선택         │
│     └─ [타겟 없음 → Patrol]                                 │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│           BTT_Attack_DragonFly (Blueprint BT Task)          │
│   - Get Blackboard "BasicAttackCount"                       │
│   - If < 5: TryActivateAbilitiesByTag(Basic), count++       │
│   - Else:   TryActivateAbilitiesByTag(Burst),  count = 0    │
│   - WaitForAbilityEnd → FinishExecute                       │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                       ABP_DragonFly                         │
│   - Locomotion State (BS_IdleWalk)                          │
│   - SkillStun State (Dragonfly_Stun, Loop=true로 반복 재생)│
│   - Death State (Dragonfly_Death)                           │
│   - bIsLockedDown 변수로 SkillStun 진입/이탈                │
└─────────────────────────────────────────────────────────────┘
```

---

### Step 1: C++ 서브클래스 `ADRFlyingEnemy` 생성

기존 `ADREnemy`를 상속하여 비행 관련 로직과 공격 카운터/경직 상태를 관리한다.

#### 1.1 새 파일 생성

**`Source/DaeRune/Public/Character/DRFlyingEnemy.h`**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Character/DREnemy.h"
#include "DRFlyingEnemy.generated.h"

class UGameplayAbility;

UCLASS()
class DAERUNE_API ADRFlyingEnemy : public ADREnemy
{
    GENERATED_BODY()

public:
    ADRFlyingEnemy();
    virtual void Tick(float DeltaTime) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    // ===== 경직(락다운) 시스템 =====
    // 스킬 GA(Blueprint)에서 마지막 투사체 발사 후 호출
    UFUNCTION(BlueprintCallable, Category = "FlyingEnemy|Combat")
    void StartSkillLockdown();

    // 경직 중 여부 (AnimBP / BT에서 폴링)
    UFUNCTION(BlueprintPure, Category = "FlyingEnemy|Combat")
    bool IsLockedDown() const { return bIsLockedDown; }

    /** Combat Interface Override */
    virtual void MulticastHandleDeath_Implementation(const FVector& DeathImpulse) override;

protected:
    virtual void BeginPlay() override;

    // ===== 비행 설정 =====

    // 비행 고정 높이 (World Z). -1이면 BeginPlay 시점의 Z를 사용
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlyingEnemy|Movement")
    float FixedAltitude = -1.f;

    // 비행 이동 속도
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlyingEnemy|Movement")
    float FlyingSpeed = 400.f;

    // 비행 감속
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "FlyingEnemy|Movement")
    float FlyingDeceleration = 800.f;

    // ===== 경직 파라미터 =====

    // 스킬 사용 후 경직 시간
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FlyingEnemy|Combat")
    float SkillLockdownDuration = 3.f;

    // 경직 중 GE (이동 속도 0, 공격 차단 태그)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FlyingEnemy|Combat")
    TSubclassOf<UGameplayEffect> LockdownEffectClass;

private:
    // 높이 고정 유지 (Tick에서 호출)
    void MaintainFixedAltitude();

    // 경직 종료 콜백
    void EndSkillLockdown();

    // ===== 상태 =====

    // 경직 상태 (복제)
    UPROPERTY(Replicated)
    bool bIsLockedDown = false;

    // 경직 중 적용된 GE 핸들
    FActiveGameplayEffectHandle LockdownEffectHandle;

    // 경직 종료 타이머
    FTimerHandle SkillLockdownTimerHandle;
};
```

**제거된 C++ 책임** (BT로 이관):
- ~~`NotifyBasicAttackExecuted()`~~ — BT가 직접 카운터 관리
- ~~`GetBasicAttackCount()` / `IsSkillReady()`~~ — Blackboard 직접 조회
- ~~`AttacksPerSkill` 프로퍼티~~ — BT Task 또는 Blackboard Default Value로 이관
- ~~`BasicAttackCount` Replicated 프로퍼티~~ — Blackboard에 존재 (서버 전용)
- ~~`TryActivateAbilitiesByTag` 자동 호출 로직~~ — `BTT_Attack_DragonFly`에서 호출

#### 1.2 생성자 구현 (`DRFlyingEnemy.cpp`)

```cpp
ADRFlyingEnemy::ADRFlyingEnemy()
{
    PrimaryActorTick.bCanEverTick = true;

    // === CharacterMovement 비행 설정 ===
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    MoveComp->DefaultLandMovementMode = EMovementMode::MOVE_Flying;
    MoveComp->SetMovementMode(MOVE_Flying);
    MoveComp->MaxFlySpeed = FlyingSpeed;
    MoveComp->BrakingDecelerationFlying = FlyingDeceleration;
    MoveComp->GravityScale = 0.f;
    MoveComp->bOrientRotationToMovement = false;
    MoveComp->SetCanEverFly(true);
    MoveComp->NavAgentProps.bCanFly = true;
    MoveComp->NavAgentProps.bCanWalk = false;
    MoveComp->NavAgentProps.bCanSwim = false;

    // === 지형지물 충돌 유지 ===
    // 중요: 지형지물(벽, 기둥, 천장 등)과의 충돌은 그대로 유지.
    // "바닥의 영향을 안 받는다" = 바닥 높낮이가 고도에 영향을 주지 않음을 의미.
    // → CapsuleComponent의 기본 Pawn 충돌 프로파일을 유지한다.
    // → GravityScale = 0 + Flying 모드 + Tick Z-보정으로 "고도 불변"을 달성.
    // → 수평 이동 시 지형지물에 부딪히면 자연스럽게 멈춤 (Sweep).
}
```

#### 1.3 BeginPlay 구현

```cpp
void ADRFlyingEnemy::BeginPlay()
{
    Super::BeginPlay();

    // FixedAltitude가 지정되지 않았다면 현재 Z를 기준으로 설정
    if (FixedAltitude < 0.f)
    {
        FixedAltitude = GetActorLocation().Z;
    }
    else
    {
        // 지정된 고도로 초기 위치 조정
        FVector Loc = GetActorLocation();
        Loc.Z = FixedAltitude;
        SetActorLocation(Loc);
    }
}
```

#### 1.4 Tick에서 높이 고정

**핵심**: GravityScale = 0 + Flying 모드만으로도 Z는 기본적으로 유지된다. 다만 AI MoveTo의 NavMesh 경로 추종 과정에서 Z가 미세하게 흔들릴 수 있고, 경사면 위에서 AI가 바닥을 따라가려 할 수 있다. Tick에서 Z를 고정값으로 보정한다.

**주의**: 지형지물(벽, 천장)과의 충돌은 유지해야 하므로 Z 보정 시 `bSweep = true`로 이동시켜 충돌 검사를 수행한다. 순간이동(Teleport)이 아닌 스윕 이동을 사용해야 위쪽에 천장이 있으면 막히고, 아래 지형에 붙어있어도 위로 올라갈 수 있다.

```cpp
void ADRFlyingEnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority() && !bDead)
    {
        MaintainFixedAltitude();
    }
}

void ADRFlyingEnemy::MaintainFixedAltitude()
{
    FVector Loc = GetActorLocation();
    if (!FMath::IsNearlyEqual(Loc.Z, FixedAltitude, 1.f))
    {
        // Z만 FixedAltitude로 스냅 (XY는 유지)
        FVector NewLoc = FVector(Loc.X, Loc.Y, FixedAltitude);

        // bSweep = true: 천장/장애물이 있으면 충돌로 막힘
        // 이렇게 해야 "지형지물 충돌은 유지" 요구사항을 만족
        SetActorLocation(NewLoc, /*bSweep=*/true, nullptr, ETeleportType::None);

        // Velocity의 Z 성분 제거 (이동 중 수직 튀어오름 방지)
        FVector Vel = GetCharacterMovement()->Velocity;
        Vel.Z = 0.f;
        GetCharacterMovement()->Velocity = Vel;
    }
}
```

**Sweep 충돌 처리**: 만약 FixedAltitude 위치로 이동할 때 천장/장애물이 가로막고 있다면 Sweep이 실패하여 Z가 그대로 유지된다. 이는 의도된 동작 — 장애물과 정상적으로 충돌하여 밀려나면서도 이후 장애물이 사라지면 다음 Tick에서 다시 FixedAltitude로 복귀한다.

#### 1.5 GetLifetimeReplicatedProps

```cpp
void ADRFlyingEnemy::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ADRFlyingEnemy, bIsLockedDown);
}
```

---

### Step 2: 고도 유지 및 충돌 처리 상세

#### 2.1 "바닥의 영향을 받지 않는다"의 정확한 의미

| 항목 | 동작 |
|------|------|
| **바닥 높낮이 변화** (언덕, 계단, 경사면) | **영향 없음** — DragonFly는 항상 `FixedAltitude` Z 값을 유지 |
| **바닥 자체와의 충돌** | 바닥이 DragonFly 고도까지 올라오지 않는 이상 **자연히 닿지 않음** |
| **벽, 기둥, 천장, 장애물** | **정상적으로 충돌** — 지형지물에 막히면 멈춤 |
| **중력** | 적용 안 됨 (`GravityScale = 0`) — 추락/부유 현상 없음 |

즉, CapsuleComponent의 충돌 응답은 **일반 Pawn과 동일하게 유지**한다. 고도 불변은 중력 제거 + Tick Z-보정으로 달성한다.

#### 2.2 CharacterMovementComponent 설정 (Flying 모드 특성)

Flying 모드는 다음 특성을 자연히 제공한다:
- **Floor 체크 없음**: Walking 모드와 달리 바닥을 따라가지 않음
- **수평 자유 이동**: XY 평면에서 자유로운 이동
- **중력 미적용**: `GravityScale = 0`이면 Z 속도 변화 없음

안전장치로 명시적 설정:

```cpp
// 생성자에 추가
MoveComp->MaxStepHeight = 0.f;            // 계단 오르기 비활성화
MoveComp->SetWalkableFloorAngle(0.f);     // 걷기 불가 (안전장치)
// MaxFlySpeed, BrakingDecelerationFlying은 이미 설정됨
```

#### 2.3 CollisionProfile 권장 설정

BP_DragonFly의 CapsuleComponent는 **기본 Pawn 충돌 프로파일을 그대로 유지**한다:

| Channel | Response | 이유 |
|---------|----------|------|
| WorldStatic (벽/기둥/천장/바닥) | **Block** | 지형지물과 정상 충돌 |
| WorldDynamic | Block | 동적 오브젝트와 충돌 |
| Pawn (플레이어/적) | Block | 유닛 간 충돌 유지 |
| Projectile | Block | 투사체 피격 판정 |
| Target (어빌리티 대상) | Block | 어빌리티 히트 판정 |

**중요**: 이전 수정에서 고려했던 `WorldStatic = Ignore` 설정은 **사용하지 않는다**. 지형지물에 부딪히는 것은 정상적인 동작이다.

커스텀 히트박스 시스템 활용 시:
- `Hitbox` 태그 컴포넌트로 피격 판정 이관 (기존 `SetupHitboxComponents` 시스템)
- 이 경우 CapsuleComponent의 Projectile/Target 응답은 Ignore로 변경 가능

#### 2.4 AI MoveTo 사용 시 주의

`NavAgentProps.bCanWalk = false` + `bCanFly = true`로 설정하지만, 간단한 구현을 위해 기존 NavMesh를 그대로 사용한다:

- **NavMesh는 바닥에 그대로 둠** (기본 NavMesh 사용)
- AI는 2D 경로(XY)를 찾고, 실제 이동 시 Flying 모드 + Z 고정으로 공중 이동
- Tick에서 Z를 `FixedAltitude`로 스냅 보정하므로 2D 경로라도 문제없음
- AI의 `MoveTo` 결과로 인한 미세한 Z 변동은 Tick 보정으로 즉시 교정

#### 2.5 고도 유지 매커니즘 정리

```
매 프레임 동작:
1. AI/BT가 MoveTo로 X,Y 이동 요청
2. CharacterMovement가 Flying 모드로 수평 이동 처리
3. GravityScale=0이므로 Z 속도는 발생하지 않음
4. 지형지물 충돌 시 자연히 멈춤 (Sweep)
5. Tick에서 Z != FixedAltitude면 bSweep=true로 스냅 복귀
6. 스냅 이동 중 천장이 있으면 막혀서 올라가지 못함 (의도된 동작)
```

---

### Step 3: 기본 공격 어빌리티 `GA_DragonFly_BasicAttack` (Blueprint)

**구현 방식**: Blueprint GA (부모 `UDRDamageGameplayAbility` C++) + `PlayMontageAndWait` + `WaitGameplayEvent`

#### 3.1 어빌리티 스펙

| 항목 | 값 |
|------|-----|
| 에셋 경로 | `Content/Blueprints/AbilitySystem/Enemy/DragonFly/GA_DragonFly_BasicAttack.uasset` |
| 부모 클래스 | `UDRDamageGameplayAbility` (기존 C++) |
| Ability Tag | `Abilities.DragonFly.BasicAttack` |
| Activation Tag Requirement | (없음) |
| Activation Blocked Tag | `State.LockedDown` (경직 중 활성 차단) |
| Net Execution Policy | `Server Only` |
| Instancing Policy | `Instanced Per Actor` |

#### 3.2 클래스 디폴트(Details 패널)에서 설정할 변수

부모 클래스 `UDRDamageGameplayAbility`가 제공하는 기존 프로퍼티를 BP Details에서 설정:

| 프로퍼티 | 값 | 비고 |
|----------|-----|------|
| `DamageEffectClass` | `GE_DragonFly_BasicAttackDamage` | 투사체 피격 시 적용될 데미지 GE |
| `DamageTypes` (Map) | `{ Damage.Physical: 15.0 }` | 타입별 데미지 값 |
| `AbilityTags` | `Abilities.DragonFly.BasicAttack` | 이 어빌리티 식별용 |

추가로 BP 변수(Variables 탭)에 아래를 선언:

| 변수명 | 타입 | 기본값 | 용도 |
|--------|------|--------|------|
| `ProjectileClass` | `TSubclassOf<ADRProjectile>` | `BP_DragonFlyProjectile` | 스폰할 투사체 |
| `ProjectileSpeed` | float | 2000.0 | 투사체 직선 속도 |
| `ProjectileSocketTag` | `FGameplayTag` | `CombatSocket.Weapon` | 스폰 위치 소켓 |
| `ProjectileEventTag` | `FGameplayTag` | `Event.Montage.ProjectileShoot` | AnimNotify 수신용 |
| `BasicAttackMontageTag` | `FGameplayTag` | `Montage.Attack.Basic` | `AttackMontages` 조회용 |

#### 3.3 Blueprint Graph: `EventActivateAbility`

노드 흐름:

```
[Event ActivateAbility]
        │
        ▼
[Branch: HasAuthority(Owner)]  ← 서버 전용 보호
   ├─ False → [End Ability]
   └─ True  ↓
        │
        ▼
[Get Avatar Actor From Actor Info]
        │
        ▼
[Cast to ADRFlyingEnemy]  → Store as "FlyingEnemy"
        │
        ▼
[GetTaggedMontageByTag (BasicAttackMontageTag)] → Store FTaggedMontage
        │
        ▼
┌─────────────────────────────────────────────────────────┐
│ PARALLEL: Play Montage + Wait Gameplay Event 2개 동시 실행│
└─────────────────────────────────────────────────────────┘
        │                                    │
        ▼                                    ▼
[Play Montage And Wait]              [Wait Gameplay Event]
  Montage: FTaggedMontage.Montage      EventTag: ProjectileEventTag
  PlayRate: 1.0                        OnlyMatchExact: true
                                       OnlyTriggerOnce: false
        │                                    │
        │                                    ▼
        │                             [Event Received]
        │                                    │
        │                                    ▼
        │                             [Spawn Projectile]  ← 3.4 참고
        │                                    │
        │                                    ▼
        │                             (루프 없이 1회만 발사;
        │                              OnlyTriggerOnce=true로 설정해도 동일)
        │
        ▼
[On Completed] / [On Interrupted] / [On Cancelled] / [On Blend Out]
        │
        ▼
[End Ability]
```

**핵심 포인트**:
- `PlayMontageAndWait`와 `WaitGameplayEvent`는 **병렬로 실행**. 몽타주가 재생되는 동안 AnimNotify가 발생하면 이벤트 콜백이 트리거됨.
- `WaitGameplayEvent`의 `Only Trigger Once` 체크박스를 **true**로 설정하여 몽타주 내에 노티파이가 실수로 여러 개 있어도 1발만 발사되게 안전장치.
- **카운터 증가는 BT가 담당**. 어빌리티는 투사체만 쏘고 종료.

#### 3.4 투사체 스폰 서브그래프 (`SpawnProjectileAtTarget`)

BP Function 또는 Macro로 분리:

```
Input: FlyingEnemy (ADRFlyingEnemy*)

[Get Combat Socket Location]  ← ICombatInterface::Execute_GetCombatSocketLocation
   Target: FlyingEnemy
   MontageTag: ProjectileSocketTag
        │
        ▼ SpawnLocation

[Get Combat Target (from FlyingEnemy)]  ← ADREnemy::CombatTarget
        │
        ▼
[Is Valid?]
   ├─ False → [Print Warning] → [Return]
   └─ True  ↓
[Target->GetActorLocation()]
        │
        ▼ TargetLocation

[Subtract] TargetLocation - SpawnLocation
        │
        ▼
[Get Safe Normal]
        │
        ▼ Direction (FVector)

[Rotation From X Vector]  (또는 MakeRotFromX)
        │
        ▼ SpawnRotation

[Make Transform]  (Location=SpawnLocation, Rotation=SpawnRotation)
        │
        ▼ SpawnTransform

[Spawn Actor From Class - Deferred]
   Class: ProjectileClass
   SpawnTransform: SpawnTransform
   Owner: FlyingEnemy
   Instigator: FlyingEnemy
   CollisionHandling: AlwaysSpawn
        │
        ▼ Projectile (ADRProjectile*)

[Set DamageEffectParams]
   Target: Projectile
   Value: MakeDamageEffectParamsFromClassDefaults()  ← 부모 C++ 노드 호출
        │
        ▼
[Get Projectile Movement Component]
        │
        ▼
[Set bIsHomingProjectile = false]
[Set ProjectileGravityScale = 0]
[Set Initial Speed = ProjectileSpeed]
[Set Max Speed = ProjectileSpeed]
[Set Velocity = Direction * ProjectileSpeed]
        │
        ▼
[Finish Spawning Actor]
   Actor: Projectile
   SpawnTransform: SpawnTransform
```

`MakeDamageEffectParamsFromClassDefaults`는 부모 C++ `UDRDamageGameplayAbility`에서 `UFUNCTION(BlueprintCallable)`로 노출되어 있으므로 Blueprint에서 직접 노드로 호출 가능 (기존 Dog 어빌리티와 동일).

#### 3.5 AnimNotify 재사용 안내

**신규 커스텀 AnimNotify를 생성하지 않는다.** 프로젝트에 이미 구현되어 있는 기존 `GameplayEvent` AnimNotify(또는 `SendGameplayEvent` 기능을 가진 노티파이)를 재사용한다.

- **사용 방식**: `Dragonfly_BasicAttack` 몽타주의 투사체 발사 타이밍 프레임에 **기존 AnimNotify**를 배치하고 `Event Tag`를 `Event.Montage.ProjectileShoot`로 설정한다.
- **수신 측**: `GA_DragonFly_BasicAttack` Blueprint의 `WaitGameplayEvent` Ability Task가 해당 태그를 수신하여 투사체를 스폰한다.
- **추가 C++ 작업 없음**: 이 단계에서는 별도의 클래스 추가나 코드 변경이 필요 없다.

> **주의**: 기존 AnimNotify의 정확한 클래스명/경로는 프로젝트 내 `Content/Blueprints/AnimNotifies/` 또는 C++ `UAnimNotify_...` 파일을 확인한다. `Dog` 계열 적이 이미 동일한 방식을 사용하고 있다면 그 노티파이를 그대로 재사용하면 된다.

---

### Step 4: `BTT_Attack_DragonFly` Blueprint BT Task (카운터 기반 공격 선택)

**구현 방식**: Blueprint BT Task Node (부모 `UBTTask_BlueprintBase`)

Dog의 `BTT_Attack` 패턴을 따르되, 확률 대신 **결정론적 카운터(BasicAttackCount)**를 사용한다.

#### 4.1 BT Task 에셋 스펙

| 항목 | 값 |
|------|-----|
| 에셋 경로 | `Content/Blueprints/AI/BTT/BTT_Attack_DragonFly.uasset` |
| 부모 클래스 | `BTTask_BlueprintBase` |
| Node Name | `Attack (DragonFly)` |

#### 4.2 Task 내부 변수 (Details)

| 변수명 | 타입 | 기본값 | 용도 |
|--------|------|--------|------|
| `BasicAttackTag` | `FGameplayTag` | `Abilities.DragonFly.BasicAttack` | 일반 공격 어빌리티 태그 |
| `BurstSkillTag` | `FGameplayTag` | `Abilities.DragonFly.BurstSkill` | 스킬 어빌리티 태그 |
| `AttacksPerSkill` | int | 5 | 몇 번의 기본 공격마다 스킬을 쓸지 |
| `BasicAttackCountKey` | `BlackboardKeySelector` | (BasicAttackCount) | 블랙보드 카운터 키 |

#### 4.3 Blackboard 키 선언

`BB_EnemyBlackboard_DragonFly` (또는 기존 BB에 추가):

| Key | Type | Default | 용도 |
|-----|------|---------|------|
| `BasicAttackCount` | Int | 0 | 누적된 기본 공격 횟수 |
| `IsLockedDown` | Bool | false | 경직 중 플래그 |
| (기존) `TargetToFollow` | Object (Actor) | - | 현재 추적 대상 |
| (기존) `HitReacting` | Bool | false | 피격 리액션 중 |

#### 4.4 `ReceiveExecuteAI` Graph

```
[Event Receive Execute AI]
   Inputs: OwnerController, ControlledPawn
        │
        ▼
[Cast ControlledPawn to ADRFlyingEnemy]  → FlyingEnemy
        │
        ▼
[Branch: FlyingEnemy.IsLockedDown()?]
   ├─ True → [Finish Execute (Success=false)]  ← 경직 중이면 종료
   └─ False ↓
        │
        ▼
[Get Blackboard Value as Int]
   Key: BasicAttackCountKey
        │
        ▼ Count (int)

[Get ASC from FlyingEnemy]
        │
        ▼ ASC

[Branch: Count < AttacksPerSkill]
   ├─ True  → [기본 공격 브랜치]
   │             │
   │             ▼
   │      [Try Activate Abilities By Tag]
   │         ASC: ASC
   │         Tag Container: MakeGameplayTagContainer(BasicAttackTag)
   │             │
   │             ▼ bSuccess
   │      [Branch: bSuccess]
   │         ├─ False → [Finish Execute (Success=false)]
   │         └─ True  ↓
   │      [Set Blackboard Value as Int]
   │         Key: BasicAttackCountKey
   │         Value: Count + 1
   │             │
   │             ▼
   │      [Wait for Ability End]  ← 4.5 참고
   │             │
   │             ▼
   │      [Finish Execute (Success=true)]
   │
   └─ False → [스킬 브랜치]
                 │
                 ▼
          [Try Activate Abilities By Tag]
             ASC: ASC
             Tag Container: MakeGameplayTagContainer(BurstSkillTag)
                 │
                 ▼ bSuccess
          [Branch: bSuccess]
             ├─ False → [Finish Execute (Success=false)]
             └─ True  ↓
          [Set Blackboard Value as Int]
             Key: BasicAttackCountKey
             Value: 0  ← 카운터 리셋
                 │
                 ▼
          [Wait for Ability End]
                 │
                 ▼
          [Finish Execute (Success=true)]
```

#### 4.5 어빌리티 종료 대기 방식

BT Task가 Tick 기반으로 어빌리티 종료를 폴링하는 것을 피하기 위해 **델리게이트 바인딩** 방식을 사용:

```
[Try Activate Abilities By Tag] 직후:
   Get ASC's AbilitySpecs matching BasicAttackTag
     → For Each Spec → Get Active Instance → Bind to OnGameplayAbilityEnded
        → Custom Event "OnAbilityEnded" → FinishExecute(true)
```

**간소화 대안**: Dog의 `BTT_Attack`에서 사용하는 방식 그대로 — 몽타주 길이만큼 `Delay` 후 `FinishExecute`. DragonFly는 기본 공격 ~1초, 스킬 ~1초이므로:

```
기본 공격: Delay 1.2s → FinishExecute
스킬:     Delay 1.2s → StartSkillLockdown이 BT에서 별도 처리
```

**권장**: 기존 Dog `BTT_Attack`에서 검증된 Delay 방식을 그대로 사용 (일관성 확보).

#### 4.6 BT에서의 사용

```
Combat Sequence:
  ├─ BTT_RotateToFaceTarget
  └─ BTT_Attack_DragonFly (이 Task가 기본/스킬 자동 선택)
```

BT는 **단 하나의 Task**만 호출하면 되므로 BT 트리 구조가 단순해진다 (Dog와 동일 패턴).

---

### Step 5: 스킬 어빌리티 `GA_DragonFly_BurstSkill` (Blueprint)

**구현 방식**: Blueprint GA (부모 `UDRDamageGameplayAbility` C++) + Blueprint `Delay` 노드 기반 연사 + 완료 후 `StartSkillLockdown` 호출

#### 5.1 어빌리티 스펙

| 항목 | 값 |
|------|-----|
| 에셋 경로 | `Content/Blueprints/AbilitySystem/Enemy/DragonFly/GA_DragonFly_BurstSkill.uasset` |
| 부모 클래스 | `UDRDamageGameplayAbility` (기존 C++) |
| Ability Tag | `Abilities.DragonFly.BurstSkill` |
| Activation Blocked Tag | `State.LockedDown` |
| Net Execution Policy | `Server Only` |
| Instancing Policy | `Instanced Per Actor` |

#### 5.2 클래스 디폴트 + 변수

부모 프로퍼티 설정:

| 프로퍼티 | 값 | 비고 |
|----------|-----|------|
| `DamageEffectClass` | `GE_DragonFly_BasicAttackDamage` | 기본 공격과 동일 GE 재사용 |
| `DamageTypes` (Map) | `{ Damage.Physical: 15.0 }` | 기본 공격과 동일 데미지 |
| `AbilityTags` | `Abilities.DragonFly.BurstSkill` | - |

BP Variables:

| 변수명 | 타입 | 기본값 | 용도 |
|--------|------|--------|------|
| `ProjectileClass` | `TSubclassOf<ADRProjectile>` | `BP_DragonFlyProjectile` | 기본 공격과 동일 |
| `ProjectileSpeed` | float | 2000.0 | - |
| `ProjectileSocketTag` | `FGameplayTag` | `CombatSocket.Weapon` | - |
| `TotalShots` | int | 10 | 연사 발수 |
| `ShotInterval` | float | 0.1 | 발사 간격(s) |

#### 5.3 Blueprint Graph: `EventActivateAbility`

**기본 공격 GA와 중복되는 "투사체 스폰" 로직은 Blueprint Function Library에 공통 함수로 뽑거나, 두 GA가 동일 Macro를 참조하도록 구성한다.** 여기서는 인라인으로 기술.

```
[Event ActivateAbility]
        │
        ▼
[Branch: HasAuthority]
   ├─ False → [End Ability]
   └─ True  ↓
        │
        ▼
[Get Avatar Actor From Actor Info]
        │
        ▼
[Cast to ADRFlyingEnemy]  → FlyingEnemy
        │
        ▼
[For Loop]
   First Index: 0
   Last Index:  TotalShots - 1    (= 9 when 10발)
   │
   ├─ Loop Body ─┐
   │             ▼
   │      [Branch: FlyingEnemy IsValid?]
   │         ├─ False → [Break Loop]
   │         └─ True  ↓
   │      [SpawnProjectileAtTarget (공유 매크로/함수)]
   │             │
   │             ▼
   │      [Branch: Index < TotalShots - 1]    ← 마지막 발 뒤에는 대기 불필요
   │         ├─ False → (Continue to Completed)
   │         └─ True  ↓
   │      [Delay ShotInterval (0.1s)]
   │             │
   │             ▼
   │      (다음 Loop Body로)
   │
   └─ Completed ↓
        │
        ▼
[Call StartSkillLockdown on FlyingEnemy]
   (서버 권한에서 실행, C++ UFUNCTION(BlueprintCallable))
        │
        ▼
[End Ability]
```

#### 5.4 타이밍 산출

- 0번째 발: 0.0s (즉시 발사)
- 1번째 발: 0.1s
- 2번째 발: 0.2s
- ...
- 9번째 발: 0.9s
- **총 발사 시간**: 약 0.9s → 10번째 발 후 즉시 `StartSkillLockdown` 호출
- **경직 시작 시간**: 약 1.0s (요구사항 "1초간 10발" 충족)
- **경직 종료 시간**: 1.0s + 3.0s = 4.0s (GA 활성화 시점 기준)

#### 5.5 타겟 소실 처리 (엣지 케이스)

`SpawnProjectileAtTarget` 매크로에서 타겟이 nullptr이면:
- **옵션 A (권장)**: 첫 발 발사 시 타겟 위치를 로컬 변수 `TargetSnapshot`에 저장. 이후 발은 이 스냅샷 위치로 계속 발사 (예측 불가한 공격 연출).
- **옵션 B**: 매 발마다 최신 타겟 위치 재조회, nullptr 시 `Break Loop` → 조기 `StartSkillLockdown` 호출.

BP 구현 시 매크로 입력에 `bUseSnapshot` 플래그를 두어 기본공격은 실시간, 스킬은 스냅샷 방식을 사용.

#### 5.6 `WaitGameplayEvent` 미사용 이유

- 스킬은 몽타주가 없거나(또는 `Dragonfly_BasicAttack`을 반복 사용), 10발 각각에 노티파이를 배치하는 것보다 **코드로 타이밍 제어가 단순**.
- 몽타주 재생이 필요하다면 **선택적으로** `PlayMontageAndWait`를 병렬 실행하여 시각 효과만 덧붙임 (루프 제어와 분리).

#### 5.7 `EndAbility` 보호

BP GA의 `EventEndAbility`에서 타이머/Delay가 남아있지 않도록 처리:
- `Delay` 노드는 `EndAbility`로 그래프가 끊겨도 자동으로 정리됨 (Latent Action이 GA Instance 기준).
- 추가 안전장치로 GA의 `Instancing Policy = Instanced Per Actor`를 확인 (재활성화 시 상태 꼬임 방지).

---

### Step 6: 경직(Lockdown) 시스템 구현

경직은 3가지 레이어로 구성된다:
1. **Gameplay Effect** (`GE_DragonFly_Lockdown`): MoveSpeed 0, 어빌리티 차단 태그
2. **Blackboard 키** (`IsLockedDown`): BT 행동 차단
3. **Stun 애니메이션**: 시각적 경직 표현 → **AnimBP State의 Sequence Player로만 처리** (Multicast 몽타주 불필요)

> **설계 원칙**: `bIsLockedDown`이 UPROPERTY(Replicated)이므로, 서버에서 true가 되는 순간 모든 클라이언트의 AnimBP가 자동으로 `SkillStun` State로 전환된다. 별도의 Multicast RPC로 몽타주를 재생할 필요가 없다.

#### 6.1 `StartSkillLockdown` 구현 (간소화)

```cpp
void ADRFlyingEnemy::StartSkillLockdown()
{
    if (!HasAuthority()) return;
    if (bDead) return;

    bIsLockedDown = true;  // ← 복제됨, AnimBP가 자동 감지

    // Blackboard 업데이트 (BT가 즉시 공격 중단)
    if (UBlackboardComponent* BB = GetBlackboardComponent())
    {
        BB->SetValueAsBool(TEXT("IsLockedDown"), true);
    }

    // Lockdown GE 적용 (이동 속도 0 + 공격 차단 태그)
    if (LockdownEffectClass && AbilitySystemComponent)
    {
        FGameplayEffectContextHandle Context = AbilitySystemComponent->MakeEffectContext();
        Context.AddSourceObject(this);
        FGameplayEffectSpecHandle Spec = AbilitySystemComponent->MakeOutgoingSpec(
            LockdownEffectClass, 1.f, Context);

        // 지속시간을 SkillLockdownDuration으로 오버라이드
        Spec.Data->SetDuration(SkillLockdownDuration, true);

        LockdownEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*Spec.Data);
    }

    // 경직 종료 타이머 (3초)
    GetWorldTimerManager().SetTimer(
        SkillLockdownTimerHandle,
        this,
        &ADRFlyingEnemy::EndSkillLockdown,
        SkillLockdownDuration,
        false);

    // AI StopMovement
    if (DRAIController)
    {
        DRAIController->StopMovement();
    }
}

void ADRFlyingEnemy::EndSkillLockdown()
{
    if (!HasAuthority()) return;

    bIsLockedDown = false;  // ← 복제됨, AnimBP가 Locomotion으로 자동 복귀

    if (UBlackboardComponent* BB = GetBlackboardComponent())
    {
        BB->SetValueAsBool(TEXT("IsLockedDown"), false);
    }

    // GE는 Duration 기반이므로 자동 제거되지만, 조기 해제 시 명시적 제거
    if (AbilitySystemComponent && LockdownEffectHandle.IsValid())
    {
        AbilitySystemComponent->RemoveActiveGameplayEffect(LockdownEffectHandle);
        LockdownEffectHandle = FActiveGameplayEffectHandle();
    }
}
```

**제거된 항목** (AnimBP State 방식 채택으로 불필요):
- ~~`SkillStunMontage` / `StunMontageDuration` 프로퍼티~~
- ~~`MulticastPlayStunMontage()` RPC~~
- ~~`AM_Dragonfly_Stun` 몽타주 에셋~~

대신 `ABP_DragonFly`의 `SkillStun` State에서 `Dragonfly_Stun` **애니메이션 시퀀스를 직접 참조**한다 (Step 8.2 참고).

#### 6.2 Lockdown GameplayEffect 블루프린트

**`GE_DragonFly_Lockdown`** 생성:

| 필드 | 값 |
|------|-----|
| DurationPolicy | HasDuration |
| Duration Magnitude | 3.0 (코드에서 오버라이드) |
| Modifiers → MoveSpeed | Override to 0 (또는 Multiplier -∞) |
| Granted Tags | `State.LockedDown`, `Effects.CannotAttack` |
| Blocked Ability Tags | `Abilities.DragonFly.BasicAttack`, `Abilities.DragonFly.BurstSkill` |

`Blocked Ability Tags`에 DragonFly의 공격 어빌리티 태그를 추가하면, GAS가 자동으로 어빌리티 활성화를 차단한다. 이로써 BTT_Attack_DragonFly에서 `TryActivateAbilitiesByTag`를 호출해도 실패하여 무리 없이 대기한다.

#### 6.3 이동 차단

GE의 `MoveSpeed Override to 0`과 더불어, AI 쪽에서도 BT가 `IsLockedDown` 블랙보드 값으로 행동을 정지하도록 한다. 삼중 방어로 확실하게 이동/공격 차단 (GE + BT Blackboard Decorator + `BTT_Attack_DragonFly`의 C++ 플래그 체크).

#### 6.4 Stun 애니메이션 에셋 준비 (몽타주 불필요)

- `Dragonfly_Stun` 애니메이션 시퀀스를 그대로 사용
- **몽타주 변환 불필요** — AnimBP State의 Sequence Player 노드가 직접 시퀀스를 참조
- **재생 방식**: **Loop=true** — 경직이 끝날 때까지(`bIsLockedDown=false`가 될 때까지) 1초짜리 시퀀스를 반복 재생
- 이 방식의 이점:
  - Multicast RPC 호출 불필요 (네트워크 트래픽 절감)
  - `bIsLockedDown` Replication만으로 자동 동기화
  - 몽타주 슬롯 충돌/BlendOut 이슈 회피
  - 1초 애니메이션이 3초 경직 동안 3회 루프되어 "지속적으로 지치는" 시각적 표현이 자연스러움
  - 애니메이션 중간에 경직이 해제되어도 State 탈출 시 자연스러운 Blend Out

---

### Step 7: 사망 처리 - Death 애니메이션 + 중력 추락

Death 애니메이션 자체는 **위치 이동이 없는 정지형**이지만, 사망 시점에 **CharacterMovement의 중력을 다시 활성화**하여 액터가 떨어지면서 Death 애니메이션이 재생되도록 한다. 이를 통해 "공중에서 힘없이 떨어지는" 시각적 임팩트를 연출한다.

#### 7.1 연출 설계: 애니메이션 + 중력 추락 (Ragdoll 아님)

**중요 구분**: 본 설계는 **Ragdoll 물리 시뮬레이션이 아니다**. Mesh의 `SimulatePhysics`는 활성화하지 않는다.
- **유지되는 것**: `Dragonfly_Death` 애니메이션이 AnimBP의 Death State에서 정상 재생됨 (정지형 포즈).
- **변경되는 것**: CharacterMovement가 `MOVE_Flying` → `MOVE_Falling`으로 전환되어 **캡슐이 중력에 의해 자유낙하**.
- **결과**: 잠자리가 "죽은 포즈 그대로" 공중을 가르며 떨어지는 시각 효과.

이 방식의 장점:
| 항목 | 설명 |
|------|------|
| 애니메이션 보존 | Death 시퀀스가 온전히 재생됨 (Ragdoll로 덮어쓰지 않음) |
| 물리 비용 최소 | PhysicsAsset 기반 다관절 시뮬레이션 불필요 → 성능 경제적 |
| 동기화 간단 | 부모 `ADRCharacterBase`의 Death 복제 시스템 그대로 활용 |
| 랜딩 처리 용이 | 바닥에 닿으면 `MOVE_Falling` → `MOVE_None`으로 자연 정지 |

#### 7.2 사망 연출 옵션 비교 및 채택

| 옵션 | 내용 | 채택 |
|------|------|------|
| 옵션 A | 제자리 Death + 공중 Dissolve (위치 고정 유지) | ❌ |
| 옵션 B | 제자리 Death 후 하강 Dissolve (애니 종료 후 Z 수동 감소) | ❌ |
| **옵션 C** | **Death 재생과 동시에 중력 추락 (CharacterMovement MOVE_Falling 전환)** | **✅ 채택** |

**옵션 C 채택 근거**:
- 사망 시 추락하는 연출이 **시각적 임팩트가 가장 강함** (공중 적의 격추감 표현).
- `Dragonfly_Death`가 정지형 애니메이션이므로, 그 포즈 그대로 낙하하는 모습이 자연스럽게 "힘이 빠진 상태로 떨어지는" 느낌을 준다.
- Ragdoll 물리가 아닌 **애니메이션 + CharacterMovement 중력**으로 구현하므로 PhysicsAsset 튜닝 불필요.

#### 7.3 `MulticastHandleDeath_Implementation` 오버라이드 (옵션 C 구현)

```cpp
void ADRFlyingEnemy::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
    // 1. 타이머 정리 (경직 타이머 등)
    GetWorldTimerManager().ClearTimer(SkillLockdownTimerHandle);

    // 2. 경직 상태 해제 (안전장치)
    if (HasAuthority())
    {
        bIsLockedDown = false;
        if (UBlackboardComponent* BB = GetBlackboardComponent())
        {
            BB->SetValueAsBool(TEXT("IsLockedDown"), false);
        }
        if (AbilitySystemComponent && LockdownEffectHandle.IsValid())
        {
            AbilitySystemComponent->RemoveActiveGameplayEffect(LockdownEffectHandle);
            LockdownEffectHandle = FActiveGameplayEffectHandle();
        }
    }

    // 3. ★ 중력 추락 활성화 ★
    //    Flying → Falling 모드로 전환하여 중력 적용
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (MoveComp)
    {
        // 수평 속도는 즉시 정지 (부자연스러운 전진 방지)
        FVector CurrentVel = MoveComp->Velocity;
        CurrentVel.X = 0.f;
        CurrentVel.Y = 0.f;
        CurrentVel.Z = 0.f;    // 수직도 초기화 (중력이 다시 가속시킴)
        MoveComp->Velocity = CurrentVel;

        // 중력 활성화
        MoveComp->GravityScale = DeathGravityScale;   // ex: 1.0

        // Falling 모드로 전환 → CharacterMovement가 중력을 자동 적용
        MoveComp->SetMovementMode(MOVE_Falling);

        // Falling 시 공기 저항 (살짝 느린 낙하로 연출감 향상)
        MoveComp->FallingLateralFriction = 0.f;
        MoveComp->AirControl = 0.f;   // 입력 없이 순수 중력만
    }

    // 4. AI 정지 (BT 중단)
    if (DRAIController)
    {
        DRAIController->StopMovement();
        DRAIController->BrainComponent->StopLogic(TEXT("Dead"));
    }

    // 5. 캡슐 충돌은 유지 (바닥에 닿아 착지하기 위함)
    //    하지만 Pawn/Projectile 채널은 Ignore로 변경 (시체에 부딪히거나 히트되는 것 방지)
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);  // 바닥 착지용
        Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Ignore);
        Capsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
        Capsule->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
        // 투사체/타겟 채널 등도 필요 시 Ignore
    }

    // 6. 부모 Die 처리
    //    - Death 애니메이션 재생 (AnimBP가 bIsDead=true를 읽고 Death State 전환)
    //    - Dissolve 시작, LifeSpan 설정, 기타 종료 처리
    //    주의: 부모가 DisableMovement()를 호출하지 않는지 확인 필요.
    //          호출한다면 Super 호출 후 다시 MOVE_Falling 재설정해야 함.
    Super::MulticastHandleDeath_Implementation(DeathImpulse);

    // 7. Super 이후 MovementMode 재확인 (부모가 None으로 바꿨다면 복원)
    if (MoveComp && MoveComp->MovementMode != MOVE_Falling)
    {
        MoveComp->SetMovementMode(MOVE_Falling);
        MoveComp->GravityScale = DeathGravityScale;
    }

    // 8. 착지 감지 바인딩 (선택적)
    //    Landed 이벤트 또는 LandedDelegate를 이용해 착지 후 처리
    LandedDelegate.AddDynamic(this, &ADRFlyingEnemy::OnDeathLanded);

    // 9. Tick에서 Z 고정 로직은 bDead==true로 자동 비활성화됨
    //    (MaintainFixedAltitude는 `!bDead` 체크 필요)
}
```

#### 7.4 헤더에 추가할 프로퍼티 및 함수

`DRFlyingEnemy.h`에 추가:

```cpp
// ===== 사망 추락 설정 =====

/** 사망 시 적용할 중력 스케일 (1.0 = 정상 중력). 0.5~1.0 권장 */
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FlyingEnemy|Death")
float DeathGravityScale = 1.0f;

/** 착지 후 액터가 완전히 소멸되기까지의 대기 시간 (초). 부모 LifeSpan과 별도 운용 시 사용 */
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "FlyingEnemy|Death")
float PostLandingLifeSpan = 2.0f;

protected:
    /** 착지 시 호출되어 추가 처리 수행 (Dissolve 트리거 등) */
    UFUNCTION()
    void OnDeathLanded(const FHitResult& Hit);
```

#### 7.5 착지 처리 (`OnDeathLanded`)

```cpp
void ADRFlyingEnemy::OnDeathLanded(const FHitResult& Hit)
{
    if (!bDead) return;  // 사망 상태에서만 처리

    // 중복 호출 방지
    LandedDelegate.RemoveDynamic(this, &ADRFlyingEnemy::OnDeathLanded);

    // 1. 낙하 정지 - Falling → None으로 전환
    UCharacterMovementComponent* MoveComp = GetCharacterMovement();
    if (MoveComp)
    {
        MoveComp->StopMovementImmediately();
        MoveComp->SetMovementMode(MOVE_None);
        MoveComp->DisableMovement();
    }

    // 2. 바닥에 완전히 붙이기 (선택)
    //    필요시 FHitResult의 ImpactPoint를 기반으로 Z를 살짝 올려 Mesh가 지면에 묻히지 않게 함

    // 3. LifeSpan 재설정 (착지 후 추가 체류 시간)
    SetLifeSpan(PostLandingLifeSpan);

    // 4. 캡슐 충돌 완전 비활성화 (시체를 밟고 지나가지 않도록)
    if (UCapsuleComponent* Capsule = GetCapsuleComponent())
    {
        Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Ignore);
    }

    // 5. 먼지/임팩트 VFX 재생 가능 (선택적)
    //    - Niagara System 스폰
    //    - 사운드: 착지 효과음 재생
}
```

#### 7.6 Tick의 Z 고정 로직에 `bDead` 체크 보강

`MaintainFixedAltitude`가 사망 중에도 호출되면 추락이 막힌다. Step 1.4의 Tick 코드에 `!bDead` 조건이 이미 있지만, 명시적으로 재확인:

```cpp
void ADRFlyingEnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // bDead일 때는 Z 고정 로직 skip → 중력 추락 허용
    if (HasAuthority() && !bDead && !bIsLockedDown)
    {
        MaintainFixedAltitude();
    }
}
```

> **주의**: 경직 상태에서도 고도는 유지되어야 하므로 `!bIsLockedDown`은 굳이 필요 없다. 경직은 수평 이동만 막을 뿐 Z는 그대로. 위 코드는 선택적이며, **`!bDead`만 체크**하는 것이 요구사항에 충분.

#### 7.7 GravityScale 네트워크 복제 주의

`GravityScale`은 `CharacterMovementComponent`의 내부 프로퍼티로, 서버에서 변경하면 클라이언트에 자동 복제되지 않을 수 있다. 해결책:
- `MulticastHandleDeath_Implementation`이 **Multicast**이므로 **모든 클라이언트와 서버에서 동일하게 실행**된다. 따라서 각 머신이 자체 `GravityScale`을 수동 설정하게 된다. → 별도 처리 불필요.
- 만약 결함이 발견되면 `OnRep_IsDead` 콜백에서 클라이언트 측 `GravityScale` 재설정.

#### 7.8 멀티플레이 동기화 확인

| 항목 | 서버 | 클라이언트 |
|------|------|-----------|
| `bDead` | 서버에서 true | Replicated → 클라에서 RepNotify로 인지 |
| `GravityScale`, `MovementMode` | `MulticastHandleDeath`에서 설정 | 동일 Multicast 실행으로 설정 |
| 실제 낙하 위치 | CharacterMovement가 서버에서 시뮬레이션 | 서버 Location을 복제받아 보간 |
| Death 애니메이션 | AnimBP가 `bIsDead`로 판단하여 Death State 진입 | 동일 |
| 착지 이벤트 | 서버에서 `OnDeathLanded` 발동 | 클라이언트는 복제된 Velocity=0으로 자연 정지 |

**권장**: 서버에서의 낙하 위치를 권위로 삼고, 클라이언트는 CharacterMovement의 보간만 받는 구조 유지 (기존 네트워크 모델 그대로).

#### 7.9 엣지 케이스

| 케이스 | 처리 |
|--------|------|
| 바닥이 없는 절벽 아래에서 죽음 | `PostLandingLifeSpan` 대신 부모 기본 `LifeSpan`이 만료되어 자동 파괴됨 |
| 천장에 붙어 죽음 | 중력으로 즉시 아래로 떨어짐 (천장과의 상단 충돌은 문제 없음) |
| 경직 중 사망 | Step 7.3의 단계 2에서 경직 상태 해제, Step 8의 SkillStun → Death 전환 규칙으로 애니메이션 전환 |
| 이미 사망한 상태에서 추가 데미지 | 부모 `ADRCharacterBase`가 `bDead` 체크로 중복 Die 방지 |
| 추락 중 벽에 부딪힘 | Falling 모드라서 `CharacterMovement`가 수평 충돌 처리 — 벽을 따라 미끄러지거나 그 자리에 멈춰 자유낙하 계속 |

---

### Step 8: AnimBlueprint (`ABP_DragonFly`) 구성

#### 8.1 상태 머신 (Stun 상태 포함)

```
                    ┌──────────────────────────────┐
                    │                              ▼
[Entry] → Locomotion ──[bIsLockedDown=true]──→ SkillStun ──[bIsLockedDown=false]──→ Locomotion
              │                                    │
              │                                    └──[bIsDead=true]──→ Death
              │
              └──[bIsDead=true]──→ Death

Locomotion:
  BlendSpace(BS_IdleWalk): Speed 기반 Idle↔Walk

SkillStun (스킬 사용 후 경직 - 신규):
  State 내부에서 Dragonfly_Stun 애니메이션 재생
  - Loop=true로 1초 시퀀스를 반복 재생
  - bIsLockedDown=false가 될 때까지 State 유지 (총 3초 = 3회 루프)
  - 전환 인: bIsLockedDown=true AND !bIsDead
  - 전환 아웃: bIsLockedDown=false (Locomotion으로 Blend Out)
  - 우선순위 높음: Death로도 전환 가능

HitReact (슬롯 오버레이):
  DefaultSlot 몽타주 재생 (Dragonfly_HitReact1/2/3)
  - bHitReacting 상태에 따라 재생 (랜덤 선택)
  - **SkillStun 상태에서는 히트리액트 무시**

Death:
  Dragonfly_Death 애니메이션 (정지형)
  - 마지막 프레임에서 Hold (Loop false, Final Pose)
  - State로 구성하여 되감기 방지
  - 어느 상태(Locomotion/SkillStun)에서도 bIsDead=true면 전환
  - ★ 애니메이션 재생 중 C++에서 중력을 활성화해 캡슐이 자유낙하 ★
  - Mesh는 애니메이션 포즈를 유지한 채 World Z 위치만 감소 (Ragdoll 아님)
  - 착지 후에도 State는 그대로 유지 (마지막 프레임 홀드)
```

#### 8.2 SkillStun 상태 상세 구성 (채택 방식)

**핵심**: 1초짜리 애니메이션을 경직 종료 시점까지 루프 재생하는 방법 → **AnimBP State 내 Sequence Player (Loop=true)** 사용.

State Machine의 `SkillStun` State 내부에 `Play Sequence` (또는 State Asset에서 시퀀스 드롭) 노드를 배치하고, 다음 설정을 적용:

| Sequence Player 속성 | 값 | 설명 |
|---------------------|-----|------|
| Sequence | `Dragonfly_Stun` | 1초 애니메이션 |
| **Loop Animation** | **true** | **경직이 끝날 때까지 반복 재생 (핵심!)** |
| Play Rate | 1.0 | 정상 속도 |
| Start Position | 0.0 | 처음부터 재생 |

**동작 결과**:
- State 진입 시 애니메이션 재생 시작
- 1초 시퀀스가 계속 루프 (3초 경직 동안 약 3회 반복)
- `bIsLockedDown=false`가 되는 순간 Transition 조건 충족 → Locomotion으로 전환
- State 이탈 시 Locomotion으로 블렌드 (Transition Blend Duration ~0.25s)
- 애니메이션의 어느 프레임에서 전환되어도 Blend가 자연스러운 처리

**네트워크 동기화**: `bIsLockedDown`이 Replicated 프로퍼티이므로 서버 → 클라이언트 자동 전파. 모든 클라이언트의 AnimBP가 동시에 SkillStun State로 전환 (Multicast RPC 불필요).

**몽타주 방식이 제외된 이유**: 몽타주는 `bIsLockedDown` 상태와 별도로 수명을 관리해야 하므로 State 기반 동기화와 충돌할 수 있다. State 머신 + Loop Sequence는 `bIsLockedDown` 하나의 변수로 재생/정지가 완전히 제어되어 로직이 단순하다.

**Loop=true 선택 이유 (Loop=false 대비)**:
| 항목 | Loop=true (채택) | Loop=false |
|------|------------------|------------|
| 경직 중 시각 표현 | 지속적으로 움직이는 지친 모션 → 생동감 | 1초 후 정지 → "얼어붙은 듯한" 느낌 |
| 경직 해제 시 전환 | 어느 프레임에서든 자연스러운 Blend | 최종 프레임에서 탈출 → 경직적 느낌 |
| 경직 지속시간 변동 대응 | 어떤 지속시간에도 자연스러움 | 정확히 1초 이상이어야 자연스러움 |
| 구현 단순성 | Sequence Player 기본 Loop 플래그 | 최종 프레임 Hold 처리 필요 |

#### 8.3 AnimBP 변수

| 변수 | 타입 | 소스 | 용도 |
|------|------|------|------|
| `Speed` | float | `GetVelocity().Size2D()` | BS_IdleWalk 블렌드 |
| `bIsDead` | bool | `IsDead_Implementation()` | Death 상태 전환 |
| **`bIsLockedDown`** | **bool** | **`FlyingEnemy->IsLockedDown()`** | **SkillStun 상태 전환 (신규)** |
| `bHitReacting` | bool | `Enemy->bHitReacting` | 히트 리액션 시 이동 애니메이션 보류 |

#### 8.4 Event Graph - 변수 갱신

`BlueprintUpdateAnimation` 이벤트에서 매 프레임 변수 갱신:

```
Event Blueprint Update Animation (DeltaTime)
├── TryGetPawnOwner → Cast to ADRFlyingEnemy → (Set bIsLockedDown = IsLockedDown())
├── TryGetPawnOwner → GetVelocity → VectorLength → (Set Speed)
├── TryGetPawnOwner → IsDead → (Set bIsDead)
└── TryGetPawnOwner → Cast to ADREnemy → Get bHitReacting → (Set bHitReacting)
```

#### 8.5 상태 전환 규칙 (Transition Rules)

| From | To | 조건 | Blend Duration |
|------|-----|------|----------------|
| Locomotion | SkillStun | `bIsLockedDown == true && !bIsDead` | 0.15s |
| SkillStun | Locomotion | `bIsLockedDown == false && !bIsDead` | 0.25s |
| Locomotion | Death | `bIsDead == true` | 0.1s |
| SkillStun | Death | `bIsDead == true` | 0.1s (Priority: SkillStun → Death가 Locomotion 전환보다 우선) |

**Priority Order**: Death > SkillStun > Locomotion → 사망이 항상 최우선, 경직이 그 다음.

#### 8.6 Attack 몽타주의 투사체 발사 타이밍

**채택 방식**: 프로젝트에 **기존에 구현된 GameplayEvent AnimNotify**를 재사용 (Step 3.5 참고).
- `Dragonfly_BasicAttack` 몽타주의 발사 타이밍 프레임에 기존 AnimNotify 배치
- Notify 설정: `EventTag = Event.Montage.ProjectileShoot`
- GA Blueprint의 `WaitGameplayEvent` Ability Task가 수신 → 투사체 스폰
- **신규 AnimNotify 클래스 생성 없음** (기존 Dog 계열 적이 사용하는 노티파이와 동일하게 재사용)

**대안 (권장하지 않음)**: 노티파이가 직접 어빌리티 로직을 실행 → 어빌리티/AI 책임 경계가 흐려짐.

#### 8.7 히트 리액션 차단 (경직 중)

`SkillStun` 상태에서는 히트리액트를 무시해야 한다. 이미 Step 15.3 (엣지 케이스)에서 `Effects.HitReactImmune` 태그로 차단하는 방안이 있으며, AnimBP 측에서도 이중 방어:

```
HitReact Slot 실행 조건:
  bHitReacting == true AND bIsLockedDown == false
  → 경직 중이면 HitReact 슬롯 무시
```

이는 HitReact 슬롯을 연결하는 Blend Poses by Bool 노드로 구현.

---

### Step 9: BehaviorTree 설정 (`BT_EnemyBehaviorTree_DragonFly`)

#### 9.1 트리 구조

```
Root
└── Selector [루트 Selector]
    │
    ├── Sequence [1. Dead]
    │   └── Decorator: BB_Dead == true
    │       └── Task: StopMovement
    │
    ├── Sequence [2. Stunned (벽 스턴 / 외부 CC)]
    │   └── Decorator: BB_Stunned == true
    │       └── Task: StopMovement + Wait
    │
    ├── Sequence [3. LockedDown - 스킬 후 경직 중]
    │   └── Decorator: BB_IsLockedDown == true
    │       └── Task: StopMovement + Wait (BB 리셋까지)
    │
    ├── Sequence [4. Combat - 타겟 있음]
    │   ├── Service: BTS_FindNearestPlayer (0.5s 간격 갱신)
    │   └── Selector
    │       │
    │       ├── Sequence [공격 사거리 이내]
    │       │   ├── Decorator: Distance to Target < AttackRange (ex: 1500)
    │       │   ├── Task: BTT_RotateToFaceTarget
    │       │   └── Task: BTT_Attack_DragonFly  ← 카운터 기반 자동 선택
    │       │       → BasicAttackCount < 5 → BasicAttack 활성화, 카운터 +1
    │       │       → BasicAttackCount >= 5 → BurstSkill 활성화, 카운터 = 0
    │       │       → BurstSkill GA 내부에서 StartSkillLockdown 호출
    │       │
    │       └── Sequence [사거리 밖 접근]
    │           └── Task: MoveToTarget (FocusOnTarget=true)
    │
    └── Sequence [5. No Target - 대기]
        └── Task: BTT_Idle 또는 Patrol
```

#### 9.2 핵심 Blackboard Keys

`BB_EnemyBlackboard_DragonFly` 블랙보드 에셋을 새로 생성 (또는 기존 `BB_EnemyBlackboard`에 추가):

| Key | Type | Default | 용도 | 갱신 주체 |
|-----|------|---------|------|----------|
| `BasicAttackCount` | Int | 0 | 누적 기본 공격 횟수 | `BTT_Attack_DragonFly` |
| `IsLockedDown` | Bool | false | 스킬 경직 중 | `ADRFlyingEnemy::StartSkillLockdown` / `EndSkillLockdown` |
| `AttackRange` | Float | 1500.0 | 공격 사거리 | BP_DragonFly 디폴트 |
| `TargetToFollow` | Object | - | 현재 타겟 | `BTS_FindNearestPlayer` |
| `HitReacting` | Bool | false | 피격 리액션 중 | `ADREnemy::HitReactTagChanged` |
| `Dead` | Bool | false | 사망 여부 | `Die` 경로 |
| `Stunned` | Bool | false | 일반 스턴 | `ADRCharacterBase::OnRep_Stunned` |

`DRBlackboardKeys` 네임스페이스에도 추가:

```cpp
// Source/DaeRune/Public/Character/DREnemy.h (DRBlackboardKeys 네임스페이스)
inline const FName IsLockedDown = TEXT("IsLockedDown");
inline const FName BasicAttackCount = TEXT("BasicAttackCount");
inline const FName AttackRange = TEXT("AttackRange");
```

#### 9.3 주의사항 및 설계 근거

- **공격 선택 로직은 BT에 완전히 위임됨**. C++에는 자동 트리거 로직이 없음.
- `BTT_Attack_DragonFly` 하나의 Task가 기본/스킬을 결정론적으로 분기 (Dog의 `BTT_Attack`과 동일 패턴, 확률 → 카운터 차이만).
- 경직 상태(`IsLockedDown=true`) Decorator가 Dead/Stunned 다음에 위치하여 Combat 분기를 완전히 차단.
- 스킬 GA 내부에서 마지막 발사 후 `StartSkillLockdown`을 호출 → `bIsLockedDown=true` + Blackboard `IsLockedDown=true` → BT가 자연히 LockedDown 분기로 이동.
- 스킬 GA의 `Abilities.DragonFly.BurstSkill` 태그가 `GE_DragonFly_Lockdown`의 Blocked Ability Tags에 포함되어 있어, 경직 중 BTT가 재활성화를 시도해도 GAS가 자동으로 차단.

---

### Step 10: Gameplay Tags 추가

#### 10.1 신규 추가 태그 (C++ 필수)

본 플랜에서 **`DRGameplayTags.h/.cpp`에 새로 추가해야 하는 태그는 다음 2개뿐**이다:

```cpp
// DRGameplayTags.h (기존 선언부에 2줄 추가)
FGameplayTag State_LockedDown;        // "State.LockedDown"      - 경직 상태 표시
FGameplayTag Effects_CannotAttack;    // "Effects.CannotAttack"  - 공격 차단 효과 태그
```

```cpp
// DRGameplayTags.cpp의 InitializeNativeGameplayTags()에 2줄 추가
GameplayTags.State_LockedDown = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("State.LockedDown"),
    FString("경직 상태 - 스킬 사용 후 일정 시간 동안 행동 불가"));

GameplayTags.Effects_CannotAttack = UGameplayTagsManager::Get().AddNativeGameplayTag(
    FName("Effects.CannotAttack"),
    FString("공격 불가 효과 - 경직 중 어빌리티 활성화 차단"));
```

#### 10.2 기존 Dog 패턴을 그대로 재사용하는 태그 (사용자 처리)

다음 태그들은 **본 플랜에서 별도 정의하지 않으며**, 사용자가 기존 Dog 계열 적의 구현 패턴을 참고하여 직접 프로젝트의 태그 구조에 맞게 추가한다:

| 태그 (예상 이름) | 용도 | Dog 참고 패턴 |
|-----------------|------|---------------|
| `Abilities.DragonFly.BasicAttack` (또는 사용자가 정한 이름) | 기본 공격 GA 식별 태그 | Dog의 `Abilities.Dog.XXX`와 동일 명명 규칙 |
| `Abilities.DragonFly.BurstSkill` (또는 사용자가 정한 이름) | 스킬 GA 식별 태그 | Dog의 `Abilities.Dog.XXX`와 동일 명명 규칙 |
| `Event.Montage.ProjectileShoot` (또는 기존 이벤트 태그) | AnimNotify → GA 투사체 스폰 이벤트 | Dog가 사용 중인 몽타주 이벤트 태그 재사용 가능 |
| `Montage.Attack.Basic` (또는 기존 몽타주 태그) | `AttackMontages` 조회용 태그 | Dog가 사용 중인 몽타주 태그 재사용 가능 |
| `CombatSocket.XXX` (소켓 태그) | 투사체 스폰 소켓 위치 | 기존 `CombatSocket.Weapon` 등 그대로 사용 가능 |

**사용자 처리 방침**:
- Dog 계열 적(`BP_Dog`, `GA_Dog_*`, `BT_Dog_*`)의 기존 태그 구조/명명 규칙을 확인하고 일관된 방식으로 추가
- 본 플랜의 각 Step에서 언급되는 태그 문자열(예: `Abilities.DragonFly.BasicAttack`)은 **참고용 예시**이며, 사용자의 실제 태그명으로 대체 가능
- 어빌리티/이벤트 태그는 `FGameplayTagContainer`로 묶어 `TryActivateAbilitiesByTag`나 `WaitGameplayEvent`에 전달되므로, **태그 식별자 자체가 중요한 게 아니라 서로 일치하기만 하면 된다**

#### 10.3 요약

| 태그 종류 | 처리 주체 |
|----------|-----------|
| `State.LockedDown` | **플랜 필수** — C++ 2줄 추가 |
| `Effects.CannotAttack` | **플랜 필수** — C++ 2줄 추가 |
| DragonFly 어빌리티 태그 | **사용자 처리** — Dog 패턴 따라 추가 |
| 몽타주 이벤트/태그 | **사용자 처리** — 기존 태그 재사용 또는 신규 추가 |

> **중요**: 이후 Step 3/5/6/9/13 등에서 어빌리티 태그 이름이 `Abilities.DragonFly.BasicAttack` / `BurstSkill`로 표기되지만, 사용자가 다른 이름을 채택해도 무방하다. 어빌리티 BP Class Defaults의 `AbilityTags`, `BTT_Attack_DragonFly`의 Tag 변수, `GE_DragonFly_Lockdown`의 `Blocked Ability Tags`가 **서로 동일 태그를 가리키도록 통일**만 하면 된다.

---

### Step 11: BP_DragonFly 블루프린트 설정

#### 11.1 기본 설정
- 부모 클래스: **`ADRFlyingEnemy`** (C++ 변경 후 BP 리페어런트)
- Mesh 컴포넌트:
  - SkeletalMesh: `Dragonfly_v0_1_1`
  - AnimClass: `ABP_DragonFly`
  - Collision: NoCollision (히트박스는 별도 컴포넌트로)
- CapsuleComponent:
  - Size: DragonFly 크기에 맞게 (ex: Radius 50, HalfHeight 50)
  - Collision Profile: `Pawn` (기본값 유지) — 지형지물과 정상 충돌
  - **주의**: WorldStatic 응답을 Ignore로 바꾸지 말 것. 벽/천장 충돌이 필요함

#### 11.2 파라미터 설정 (Details 패널)

| 카테고리 | 프로퍼티 | 기본값 | 비고 |
|----------|----------|--------|------|
| FlyingEnemy\|Movement | FixedAltitude | -1 | BeginPlay에서 자동 설정 |
| FlyingEnemy\|Movement | FlyingSpeed | 400 | - |
| FlyingEnemy\|Movement | FlyingDeceleration | 800 | - |
| FlyingEnemy\|Combat | SkillLockdownDuration | 3.0 | 경직 지속 시간 (초) |
| FlyingEnemy\|Combat | LockdownEffectClass | `GE_DragonFly_Lockdown` | - |
| Enemy\|AI | BehaviorTree | `BT_EnemyBehaviorTree_DragonFly` | - |
| Combat | AttackMontages | [{Montage.Attack.Basic, Dragonfly_BasicAttack, SocketTag, ImpactSound}] | `GA_DragonFly_BasicAttack`에서 태그로 조회 |
| Combat | HitReactMontages | [Dragonfly_HitReact1, 2, 3] | 랜덤 재생 |

**참고**:
- `AttacksPerSkill`은 C++에서 제거됨. 대신 `BTT_Attack_DragonFly`의 변수(기본 5) 또는 Blackboard Default Value로 관리.
- `SkillStunMontage`/`StunMontageDuration`은 불필요 (AnimBP State의 Sequence Player가 `Dragonfly_Stun` 시퀀스를 직접 참조).
- `BasicAttackCount`는 Blackboard 키(Int, Default 0)로 존재하며 `BTT_Attack_DragonFly`가 갱신.

#### 11.3 소켓 설정

Skeletal Mesh의 Skeleton(`Dragonfly_v0_1_1_Skeleton`)에 투사체 스폰용 소켓 추가:
- 소켓 이름: `MuzzleSocket` (입 또는 머리 부위)
- `DRCharacterBase`의 소켓 이름 프로퍼티에 설정
  - 예: `WeaponTipSocketName = "MuzzleSocket"` 또는 별도 소켓 이름 추가

---

### Step 12: 투사체 블루프린트

#### 12.1 `BP_DragonFlyProjectile` 생성
- 부모: `ADRProjectile` (기존)
- Sphere Collision: 적절한 반경
- Niagara VFX: 비행 이펙트
- ProjectileMovement:
  - InitialSpeed: 2000 (어빌리티에서 오버라이드)
  - bIsHomingProjectile: false
  - ProjectileGravityScale: 0 (직선 이동)
  - bRotationFollowsVelocity: true

#### 12.2 데미지 GE
- `GE_DragonFly_BasicAttackDamage` 생성
- DamageType: `Damage.Physical` (또는 원하는 타입)
- Damage: 10~20 (밸런스 조정)

---

### Step 13: CharacterClassInfo 등록

`DA_EnemyCharacterClassInfo`에 DragonFly 항목 추가:

- **CharacterClass**: `ECharacterClass::Ranger` (원거리 적) 또는 신규 추가
- **PrimaryAttributes GE**: `GE_DragonFly_PrimaryAttributes` (체력, 이동속도 등)
- **VitalAttributes GE**: 기본 Vital GE 재사용
- **StartupAbilities** (Blueprint GA 클래스):
  - `GA_DragonFly_BasicAttack` (어빌리티 태그: `Abilities.DragonFly.BasicAttack`)
  - `GA_DragonFly_BurstSkill` (어빌리티 태그: `Abilities.DragonFly.BurstSkill`)
- **DeathAbilities**: 필요 시 사망 이펙트 어빌리티

**중요**: 두 GA 모두 Blueprint로 작성되며, 부모 클래스 `UDRDamageGameplayAbility`의 `AbilityTags`에 각각의 태그가 포함되도록 Class Defaults 설정 필수. AI BT가 `TryActivateAbilitiesByTag`로 호출하므로 태그 매칭이 정확해야 한다.

---

### Step 14: 네트워크 복제 요약

| 프로퍼티/이벤트 | 복제 방식 | 비고 |
|----------------|----------|------|
| `bIsLockedDown` | Replicated | AnimBP의 SkillStun State 자동 동기화 |
| `bIsAggroed` | (기존 ADREnemy) Replicated | - |
| Death Multicast | NetMulticast Reliable | 기존 시스템 (부모 `ADREnemy`) |
| Lockdown GE | GAS 자동 복제 | MoveSpeed, Blocked Tags 동기화 |
| 투사체 스폰 | 서버 권한, 자동 복제 | BP GA의 `SpawnActorDeferred` |
| Z 고정 | 서버 권한 | Tick에서 서버만 실행, 클라는 복제된 위치 수신 |
| `BasicAttackCount` (Blackboard) | **복제 안 함** | Blackboard는 서버 전용, BT 실행이 서버에서만 일어남 |

**제거된 복제 항목**:
- ~~`BasicAttackCount` UPROPERTY(Replicated)~~ → Blackboard로 이동 (서버 전용이므로 복제 불필요)
- ~~`MulticastPlayStunMontage` RPC~~ → AnimBP State가 `bIsLockedDown` 복제로 자동 동기화

---

### Step 15: 엣지 케이스 및 주의사항

#### 15.1 AI 이동 중 Z 튕김 방지 및 지형지물 충돌
NavMesh 기반 이동 시 AvoidanceManager가 수직 성분을 만들거나, 경사 NavMesh를 따라가면서 Z가 변동될 수 있음:
- Tick에서 매 프레임 Z를 `FixedAltitude`로 보정 (bSweep=true로 이동하여 충돌 유지)
- `GetCharacterMovement()->Velocity.Z = 0` 지속 유지
- **지형지물 충돌**: 천장이 FixedAltitude보다 낮으면 Z 보정 시 Sweep에 막혀 올라가지 못함 → 의도된 동작
- **고도 위에 장애물**: DragonFly가 수평 이동 중 벽/기둥에 부딪히면 자연히 멈춤 (CharacterMovement가 처리)

#### 15.2 스킬 중 타겟 사망
- `FireOneShot`에서 `CombatTarget`이 nullptr 또는 사망 체크
- 타겟 소실 시 현재 방향 유지 또는 스킬 조기 종료 (디자인 선택)
- **권장**: 스킬 시작 시 타겟 위치 스냅샷 저장 → 해당 방향으로만 발사 (예측 불가능 공격 연출)

#### 15.3 HitReact vs 스킬
- 스킬 사용 중 히트 리액션이 오면 스킬 중단 여부 결정:
  - **옵션 A**: 스킬 중 히트리액트 면역 (`Effects.HitReactImmune` 태그)
  - **옵션 B**: 스킬 중에도 히트리액트 허용 (시각적으로만 재생, 어빌리티는 계속)
- **권장 A**: 스킬 연사는 "각성 모드" 느낌이라 중단되지 않는 것이 자연스러움

#### 15.4 스킬 대신 경직 중 피격
- 경직(LockedDown) 중에도 데미지는 받아야 함 (정지 표적)
- Lockdown GE는 MoveSpeed와 Attack만 차단하고 데미지 수령은 정상 처리

#### 15.5 월 스턴 메카닉
- DragonFly도 `ADREnemy`의 월 스턴 시스템 상속
- 공중에서 벽에 부딪혀 스턴되는 경우 → 현재 위치에서 스턴 애니메이션 재생
- FixedAltitude 유지되므로 공중 스턴

#### 15.6 NavMesh 시작점
- AI MoveTo 호출 시 NavMesh 위 경로가 필요
- 바닥 NavMesh를 DragonFly의 FixedAltitude로 투영하여 이동
- Tick에서 Z 보정하므로 실제 이동은 올바른 높이에서 발생

---

### Step 16: 구현 체크리스트 및 순서

#### Phase A: C++ 기반 (최우선, 최소화됨)
1. ☐ `DRGameplayTags`에 **`State.LockedDown`, `Effects.CannotAttack` 2개 태그만** 추가 (Step 10.1 참고)
   - 기타 어빌리티/이벤트 태그는 **사용자가 Dog 패턴 참고하여 별도 처리** (Step 10.2)
2. ☐ `ADRFlyingEnemy.h/.cpp` 생성 (비행 설정 + 경직 시스템만, 카운터 로직 없음)
3. ☐ ~~커스텀 AnimNotify 생성~~ → **기존 AnimNotify 재사용 (신규 클래스 생성 불필요)**
4. ☐ `DRBlackboardKeys` 네임스페이스에 `IsLockedDown`, `BasicAttackCount`, `AttackRange` 추가
5. ☐ 컴파일 및 로그 확인

#### Phase B: Gameplay Effect 및 투사체 (C++ 완료 후)
6. ☐ `GE_DragonFly_Lockdown` 생성 (Blocked Ability Tags 설정)
7. ☐ `GE_DragonFly_BasicAttackDamage` 생성
8. ☐ `GE_DragonFly_PrimaryAttributes` 생성
9. ☐ `BP_DragonFlyProjectile` 생성 (부모 `ADRProjectile`, 직선 이동)

#### Phase C: Blueprint Gameplay Abilities (신규 - Blueprint 전환)
10. ☐ **`GA_DragonFly_BasicAttack` 블루프린트 생성 (부모 `UDRDamageGameplayAbility`)**
    - Ability Tag: `Abilities.DragonFly.BasicAttack`
    - 그래프: `PlayMontageAndWait` + `WaitGameplayEvent(Event.Montage.ProjectileShoot)` + 투사체 스폰
    - Activation Blocked Tag: `State.LockedDown`
11. ☐ **`GA_DragonFly_BurstSkill` 블루프린트 생성 (부모 `UDRDamageGameplayAbility`)**
    - Ability Tag: `Abilities.DragonFly.BurstSkill`
    - 그래프: `For Loop (0..9)` + `Delay 0.1s` + 투사체 스폰 + 완료 후 `StartSkillLockdown` 호출
    - Activation Blocked Tag: `State.LockedDown`
12. ☐ 투사체 스폰 로직을 Blueprint Macro/Function으로 공용화 (두 GA 공유)

#### Phase D: AnimBP 및 몽타주
13. ☐ `BP_DragonFly` 부모 클래스를 `ADRFlyingEnemy`로 리페어런트 + 파라미터 설정
14. ☐ `ABP_DragonFly` 상태 머신 구성:
    - Locomotion State (`BS_IdleWalk`)
    - **SkillStun State** (`Dragonfly_Stun` Sequence Player, **Loop=true**)
    - Death State (`Dragonfly_Death`, Final Pose Hold)
    - 전환 규칙: Death > SkillStun > Locomotion
15. ☐ `bIsLockedDown` 변수 추가 + `EventBlueprintUpdateAnimation`에서 `FlyingEnemy->IsLockedDown()` 폴링
16. ☐ `Dragonfly_BasicAttack` 애니메이션 → 몽타주 변환
17. ☐ `Dragonfly_BasicAttack` 몽타주의 발사 타이밍에 **기존 AnimNotify(GameplayEvent 전송)** 배치 + `EventTag = Event.Montage.ProjectileShoot` 설정
18. ☐ `AttackMontages` 배열에 기본 공격 몽타주 등록

#### Phase E: AI - Blackboard, BT, BTT
19. ☐ `BB_EnemyBlackboard_DragonFly` 생성 (또는 기존 BB 확장)
    - `BasicAttackCount` (Int, Default 0)
    - `IsLockedDown` (Bool, Default false)
    - `AttackRange` (Float, Default 1500)
    - 기존 `TargetToFollow`, `HitReacting`, `Dead`, `Stunned` 포함
20. ☐ **`BTT_Attack_DragonFly` Blueprint BT Task 생성 (부모 `BTTask_BlueprintBase`)**
    - 변수: `BasicAttackTag`, `BurstSkillTag`, `AttacksPerSkill(5)`, `BasicAttackCountKey` Selector
    - Graph: Count < 5 → BasicAttack 활성화 + Count+1 / 그 외 → BurstSkill 활성화 + Count=0
    - Delay 기반 종료 (Dog BTT_Attack 패턴 일치)
21. ☐ `BT_EnemyBehaviorTree_DragonFly` 생성:
    - Dead / Stunned / **LockedDown** / Combat / Patrol Selector 분기
    - Combat 분기에서 `BTT_Attack_DragonFly` 사용

#### Phase F: 통합 및 테스트
22. ☐ `DA_EnemyCharacterClassInfo`에 DragonFly 등록 (Blueprint GA 클래스 참조)
23. ☐ 테스트 맵에 배치 후 플레이
24. ☐ 공중 Z 고정 확인 (바닥 경사/언덕 위에서도 동일 고도 유지, 지형지물 벽에는 부딪힘)
25. ☐ 기본 공격 5회 → 6번째에 스킬 발동 확인 (BasicAttackCount=5 → BurstSkill)
26. ☐ 스킬 약 1초간 10발 발사 확인 (0.0s/0.1s/.../0.9s)
27. ☐ 스킬 후 3초 경직 확인 (이동/공격 모두 불가)
28. ☐ **경직 3초간 `Dragonfly_Stun` 애니메이션이 Loop 재생되는지 확인 (약 3회 반복)**
29. ☐ **경직 해제 시점(어느 루프 프레임이든) Locomotion으로 자연스러운 Blend Out 확인**
30. ☐ **경직 중 HitReact 발생 시 Stun 상태 유지 확인**
31. ☐ **경직 도중 사망 시 Stun → Death 전환 확인**
32. ☐ **사망 시 Death 애니메이션 재생 + 중력 추락 확인** (수평 정지, 수직 가속 낙하)
33. ☐ **착지 시 `OnDeathLanded` 호출, 수평/수직 정지 확인**
34. ☐ **착지 후 `PostLandingLifeSpan`만큼 대기 후 Dissolve/파괴 확인**
35. ☐ **바닥이 없는 절벽에서 사망 시 무한 낙하 방지 (기본 LifeSpan으로 자동 파괴) 확인**
36. ☐ 멀티플레이 테스트 (서버/클라이언트 `bIsLockedDown`, `MovementMode=Falling` 복제 → AnimBP/낙하 동기화)
37. ☐ 스킬 GA가 `Blocked Ability Tags`에 의해 경직 중 재활성화되지 않는지 확인

---

### DragonFly 구현 요약 (최종)

| 단계 | 작업 | 파일/에셋 | 유형 |
|------|------|-----------|------|
| 1 | GameplayTags 추가 (**`State.LockedDown`, `Effects.CannotAttack` 2개만**; 나머지는 사용자가 Dog 패턴 재사용) | `DRGameplayTags.h/.cpp` | C++ |
| 2 | C++ 클래스: 비행 적 (비행/경직만) | `DRFlyingEnemy.h/.cpp` | C++ |
| 3 | Blackboard 키 상수 추가 | `DREnemy.h` (`DRBlackboardKeys`) | C++ |
| 4 | 경직 GE | `GE_DragonFly_Lockdown.uasset` | BP/Asset |
| 5 | 데미지 GE | `GE_DragonFly_BasicAttackDamage.uasset` | BP/Asset |
| 6 | 속성 GE | `GE_DragonFly_PrimaryAttributes.uasset` | BP/Asset |
| 7 | 투사체 BP | `BP_DragonFlyProjectile.uasset` | Blueprint |
| 8 | **기본 공격 GA (Blueprint)** | **`GA_DragonFly_BasicAttack.uasset`** | **Blueprint GA** |
| 9 | **스킬 GA (Blueprint)** | **`GA_DragonFly_BurstSkill.uasset`** | **Blueprint GA** |
| 10 | BP_DragonFly 리페어런트 + 파라미터 | `BP_DragonFly.uasset` | Blueprint |
| 11 | ABP_DragonFly 구성 (Locomotion + Death + **SkillStun State with Dragonfly_Stun, Loop=true**) | `ABP_DragonFly.uasset` | Blueprint |
| 12 | Attack 몽타주 변환 + **기존 AnimNotify 재사용 배치** | `Dragonfly_BasicAttack` 몽타주 | Animation |
| 13 | Stun 애니메이션 에셋 (1초, 몽타주 변환 불필요, Loop 재생) | `Dragonfly_Stun.uasset` | Animation |
| 14 | Blackboard 에셋 (`BasicAttackCount`, `IsLockedDown` 등) | `BB_EnemyBlackboard_DragonFly.uasset` | Asset |
| 15 | **BT Task: 공격 선택 Blueprint Task** | **`BTT_Attack_DragonFly.uasset`** | **Blueprint BTT** |
| 16 | BehaviorTree | `BT_EnemyBehaviorTree_DragonFly.uasset` | Asset |
| 17 | CharacterClassInfo 등록 (Blueprint GA 참조) | `DA_EnemyCharacterClassInfo.uasset` | Asset |

**핵심 변화**:
- ~~C++ 어빌리티 클래스 2개~~ → Blueprint GA 2개
- ~~C++에서 공격 카운터/자동 스킬 트리거~~ → `BTT_Attack_DragonFly` Blueprint Task가 Blackboard 기반으로 관리
- ~~커스텀 `AnimNotify_GameplayEvent` C++ 클래스 신규 생성~~ → **기존 AnimNotify(GameplayEvent 전송 기능) 재사용**
- ~~MulticastPlayStunMontage RPC + AM_Dragonfly_Stun 몽타주~~ → AnimBP SkillStun State의 Sequence Player (`bIsLockedDown` 복제 기반 자동 동기화, **Loop=true**)
- ~~DRGameplayTags에 5개 태그 신규 등록~~ → **필수 태그는 `State.LockedDown`, `Effects.CannotAttack` 2개만 C++ 추가**; 어빌리티/이벤트 태그는 사용자가 Dog 패턴 따라 기존 구조 재사용
- **C++ 책임 최소화**: 비행/고도 유지/충돌/경직 GE 적용/타이머만 C++, 나머지는 전부 Blueprint

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
