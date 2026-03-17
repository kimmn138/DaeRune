# 적 사망 후 회전 버그 수정 계획

## 참고 문서
- `Research.md` - 버그 원인 상세 분석

---

## 수정 전략

Research.md에서 분석한 4가지 원인을 모두 차단하기 위해 **다중 방어(Defense in Depth)** 접근 방식을 채택한다. 단일 지점 수정이 아니라 여러 계층에서 문제를 차단하여 안정성을 높인다.

### 선택한 수정 조합
1. `ADREnemy::Die()`에서 `ClearFocus()` 호출 — 근본 원인 제거
2. `ADREnemy::Die()`에서 `SetActorTickEnabled(false)` 호출 — 불필요한 Tick 실행 차단
3. `ADREnemy::Tick()`에 `bDead` 가드 추가 — 방어적 안전장치

### 채택하지 않는 방법
- **AI Controller UnPossess**: BehaviorTree의 Dead 키 기반 로직이나 Death Ability 등 사망 후에도 AI Controller 참조가 필요할 수 있는 코드가 존재하므로 채택하지 않음. `Die()` 내에서 `DRAIController->GetBlackboardComponent()`를 직접 호출하고 있어 UnPossess 시 null 참조 위험이 있음.

---

## 수정 단계

### Step 1: `ADREnemy::Tick()`에 사망 상태 가드 추가

**파일**: `Source/DaeRune/Private/Character/DREnemy.cpp`
**위치**: `Tick()` 함수 최상단 (69~89줄)

**현재 코드**:
```cpp
void ADREnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    if (HasAuthority())
    {
        // 서버 회전 처리...
    }
    else if (GetLocalRole() == ROLE_SimulatedProxy)
    {
        // 클라이언트 회전 처리...
    }
}
```

**수정 후 코드**:
```cpp
void ADREnemy::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    // 사망 상태에서는 회전 처리하지 않음
    if (bDead) return;

    if (HasAuthority())
    {
        // 서버 회전 처리... (기존 코드 동일)
    }
    else if (GetLocalRole() == ROLE_SimulatedProxy)
    {
        // 클라이언트 회전 처리... (기존 코드 동일)
    }
}
```

**변경 이유**:
- `SetActorTickEnabled(false)` 호출 이전 프레임이나, 어떤 이유로 Tick이 비활성화되지 않은 경우에도 회전을 차단하는 방어적 안전장치
- `bDead`는 `MulticastHandleDeath()`에서 가장 먼저 `true`로 설정되므로, 사망 직후부터 즉시 적용됨
- `bDead`는 부모 클래스 `ADRCharacterBase`에 이미 존재하는 멤버 변수이므로 추가 선언 불필요

**주의사항**:
- `bDead` 체크는 `Super::Tick(DeltaTime)` 호출 **이후**에 위치시킨다. 부모 클래스의 Tick 로직은 정상적으로 실행되어야 하며 (현재 `ADRCharacterBase`의 Tick은 비활성화 상태 `bCanEverTick = false`이므로 실질적 영향 없음), 차단 대상은 오직 적 전용 회전 로직뿐이다.

---

### Step 2: `ADREnemy::Die()`에서 AI Focus 해제 및 Tick 비활성화

**파일**: `Source/DaeRune/Private/Character/DREnemy.cpp`
**위치**: `Die()` 함수 (128~154줄)

**현재 코드**:
```cpp
void ADREnemy::Die(const FVector& DeathImpulse)
{
    // 죽을 때 부품 자동 드랍
    if (HasPart())
    {
        DropPart();
    }

    // Death Ability 발동
    if (HasAuthority())
    {
        ActivateDeathAbilities();
    }

    // 사망 처리 - 일정 시간 후 소멸
    SetLifeSpan(LifeSpan);
    // AI 상태 업데이트
    if (DRAIController) DRAIController->GetBlackboardComponent()->SetValueAsBool(DRBlackboardKeys::Dead, true);

    // 사망시 플레이어들에게 물 보상 지급
    if (HasAuthority())
    {
        GrantWaterToPlayers();
    }

    Super::Die(DeathImpulse);
}
```

**수정 후 코드**:
```cpp
void ADREnemy::Die(const FVector& DeathImpulse)
{
    // 죽을 때 부품 자동 드랍
    if (HasPart())
    {
        DropPart();
    }

    // Death Ability 발동
    if (HasAuthority())
    {
        ActivateDeathAbilities();
    }

    // 사망 처리 - 일정 시간 후 소멸
    SetLifeSpan(LifeSpan);
    // AI 상태 업데이트
    if (DRAIController)
    {
        DRAIController->GetBlackboardComponent()->SetValueAsBool(DRBlackboardKeys::Dead, true);
        // AI Focus 해제 - 사망 후 플레이어 방향 추적 중지
        DRAIController->ClearFocus(EAIFocusPriority::Gameplay);
    }

    // 사망 후 Tick 비활성화 - 불필요한 회전 처리 차단
    SetActorTickEnabled(false);

    // 사망시 플레이어들에게 물 보상 지급
    if (HasAuthority())
    {
        GrantWaterToPlayers();
    }

    Super::Die(DeathImpulse);
}
```

**변경 내용 상세**:

#### 2-1. `DRAIController->ClearFocus(EAIFocusPriority::Gameplay)` 추가
- **위치**: `DRAIController` null 체크 블록 안, BB Dead 설정 직후
- **매개변수**: `EAIFocusPriority::Gameplay`를 사용하여 게임플레이 레벨의 Focus만 정확히 해제. `SetFocus()`의 기본 Priority가 `Gameplay`이므로 동일한 레벨로 해제해야 한다.
- **효과**: AI Controller가 더 이상 Focus 대상의 위치를 추적하지 않으므로 `GetControlRotation()`이 마지막 방향에서 고정됨
- **기존 코드 영향**: `DRAIController` null 체크가 이미 존재하므로 안전. 기존 `if (DRAIController)` 한 줄 조건문을 중괄호 블록으로 확장 필요

#### 2-2. `SetActorTickEnabled(false)` 추가
- **위치**: DRAIController 블록 바로 다음, `GrantWaterToPlayers()` 호출 전
- **효과**: 사망 후 `Tick()` 자체가 호출되지 않으므로 회전 로직이 완전히 차단됨. 불필요한 CPU 사이클도 절약.
- **안전성**: 이 시점에서 적의 Tick에서 실행되는 유일한 로직은 회전 처리뿐이므로, Tick 비활성화에 의한 부작용 없음. Dissolve 타임라인은 Blueprint에서 독립적으로 실행되므로 영향 없음.
- **주의**: `Super::Die()` 호출 **전**에 배치하여, 부모 클래스의 사망 처리가 진행되기 전에 Tick을 차단함. `Super::Die()` 내부의 `MulticastHandleDeath()`는 별도의 RPC로 실행되므로 이 시점에서 Tick을 끄는 것이 안전함.

---

## 수정하지 않는 파일

### `DREnemyAttributeSet.cpp`
- `SetFocus()` 호출 자체는 정상적인 전투 중 기능이므로 수정 불필요
- Focus 해제는 사망 시점인 `Die()`에서 처리하는 것이 적절

### `DRCharacterBase.cpp` (`MulticastHandleDeath`)
- 이미 `CharacterMovement` 비활성화, 충돌 비활성화 등을 수행하고 있음
- 회전 중지 로직은 적(`ADREnemy`) 전용이므로 부모 클래스가 아닌 자식 클래스에서 처리하는 것이 올바름
- `bDead = true` 설정이 여기서 이루어지므로 Step 1의 가드가 자동으로 작동

### `DRAIController.cpp`
- AI Controller 자체에는 수정할 내용 없음
- Focus 해제는 호출하는 쪽(`ADREnemy::Die()`)에서 책임짐

### `DREnemy.h`
- 새로운 멤버 변수나 함수 선언이 필요 없음
- `bDead`는 부모 클래스에 이미 존재, `ClearFocus()`와 `SetActorTickEnabled()`는 엔진 함수

---

## 수정 파일 요약

| 파일 | 수정 내용 | 변경 줄 수 |
|------|----------|-----------|
| `Source/DaeRune/Private/Character/DREnemy.cpp` - `Tick()` | `if (bDead) return;` 가드 추가 | +2줄 |
| `Source/DaeRune/Private/Character/DREnemy.cpp` - `Die()` | `ClearFocus()` 호출 + `SetActorTickEnabled(false)` 추가 + 기존 if문 블록 확장 | +8줄 |

**총 수정 파일**: 1개 (`DREnemy.cpp`)
**총 추가 코드**: 약 10줄

---

## 수정 후 예상 실행 흐름

```
1. 적이 스폰됨
2. 플레이어가 적을 공격
3. HandleIncomingDamage() → SetFocus(플레이어) ← 정상 작동
4. AI가 플레이어를 추적하며 전투 ← 정상 작동
5. 적의 체력이 0 이하 → Die() 호출
   → BB에 Dead = true 설정
   → ★ ClearFocus(Gameplay) → AI Controller가 플레이어 추적 중지
   → ★ SetActorTickEnabled(false) → Tick 호출 자체가 중단
   → Super::Die() → MulticastHandleDeath()
     → bDead = true ← Step 1 가드의 백업 트리거
     → CharacterMovement 비활성화
     → Dissolve 시작
6. 사망 후: Tick이 호출되지 않음 → 회전 없음
   (만약 Tick이 호출되더라도 bDead 체크에 의해 회전 차단)
7. LifeSpan 경과 후 적 소멸
```

---

## 테스트 시나리오

### 기본 테스트
1. PIE에서 적 1마리와 전투
2. 적을 죽인 후 플레이어가 적 주변을 360도 돌아다님
3. **기대 결과**: 사망한 적이 마지막 방향에서 고정되어 회전하지 않음

### 멀티플레이어 테스트
1. 2인 플레이 환경에서 테스트
2. 플레이어 A가 적을 공격하여 Focus 대상이 됨
3. 플레이어 B가 마지막 타격으로 적을 죽임
4. 플레이어 A가 이동
5. **기대 결과**: 서버/클라이언트 양쪽 모두에서 적이 회전하지 않음

### 사망 몽타주 테스트
1. 사망 몽타주가 정상적으로 재생되는지 확인
2. Dissolve 이펙트가 정상 작동하는지 확인
3. LifeSpan 후 적이 정상적으로 소멸되는지 확인

### 회귀 테스트
1. 적의 생존 중 회전이 정상 작동하는지 확인 (플레이어 추적)
2. 적이 스턴에서 풀린 후 회전이 정상 복구되는지 확인
3. 부품 드랍이 사망 시 정상 작동하는지 확인
4. Death Ability가 정상 발동되는지 확인
5. 물 보상이 정상 지급되는지 확인
