# Plan2 — 플레이어 사망 애니메이션 폴링 → 이벤트 기반 전이 (범위 축소판)

## 0. 한 줄 요약
**플레이어 캐릭터(`ADRCharacter`)에 한정** 하여, ABP가 `IsDead`를 폴링하는 방식을 Character가 AnimMontage를 명시적으로 푸시(push)하는 이벤트 기반 방식으로 바꾼다. 적 코드/적 ABP는 **건드리지 않는다**.

---

## 1. 작업 범위 (Scope Boundary)

### 1.1 포함 ✅
- `ADRCharacterBase` C++ — `bDead`를 리플리케이트 + RepNotify로 전환 (베이스 클래스라 적도 자동으로 안전성 개선을 얻지만 동작 변경은 없음).
- `ADRCharacter` C++ — 사망 몽타주 슬롯, 단일 재생 진입점, BP 이벤트, Mesh Tick 옵션 승격.
- 플레이어 캐릭터 BP(`BP_GardenRobot`, `BP_VendingMachine`) — `DeathMontages` 배열에 몽타주 등록.
- 플레이어 3P ABP(`ABP_Gardener`, `ABP_VendingMachine`) — EventGraph의 `IsDead` 폴링/Death State 전이 제거.

### 1.2 제외 ❌
- 적 C++ 코드(`DREnemy`, `DRFlyingEnemy`, `DRArmadilloEnemy`).
- 적 ABP(`ABP_Enemy`, `ABP_Armadillo_*`, `ABP_DragonFly`, `ABP_EliteBear`, `ABP_Dog_*`).
- 1P ABP(`ABP_FP_Gardener`, `ABP_FP_VendingMachine`) — 1P 카메라는 사망 시 3P로 전환되므로 1P 사망 애니메이션 불필요.
- 부활/리스폰 처리 — 현재 시스템은 사망 후 Destroy & 새 Pawn possess 패턴이라 1회성 사망 가정.

### 1.3 적이 코드 변경 영향 받는 부분 (무해함 검증)
| 변경 | 적에게 일어나는 일 |
|---|---|
| 베이스 `bDead`가 `ReplicatedUsing=OnRep_Dead`로 바뀜 | 자동 적용. `OnRep_Dead` 베이스 구현은 **빈 함수** → 적은 영향 없음 |
| 베이스에 `MulticastHandleDeath` 끝에서 `OnRep_Dead()`를 수동 호출하는 한 줄 추가 | 적의 `OnRep_Dead`는 빈 함수라 no-op |
| 베이스에 `VisibilityBasedAnimTickOption` 강제 변경 코드 추가 | **플레이어 분기 안에만 넣어** 적은 영향 없음 |

→ **적의 ABP/사망 처리는 한 글자도 안 건드린다.**

---

## 2. 아키텍처

### 2.1 책임 분리
```
ADRCharacterBase
  ├─ bDead (ReplicatedUsing=OnRep_Dead)         [공통 상태]
  ├─ OnRep_Dead() : virtual, empty default      [훅]
  └─ MulticastHandleDeath_Implementation():
       ... 기존 로직 ...
       bDead = true
       OnRep_Dead()    ← 서버에서 수동 호출 (RepNotify는 클라이언트에서만 자동 호출됨)
       ... 기존 로직 ...

ADRCharacter (플레이어 전용)
  ├─ DeathMontages : TArray<UAnimMontage*>       [BP에서 채움]
  ├─ DeathMontagePlayRate : float                [BP에서 조정]
  ├─ DeathMontageIndex : int32 (Replicated)      [서버가 결정, 모두 동일 인덱스 재생]
  ├─ bDeathMontagePlayed : bool                  [로컬 중복 방지]
  ├─ OnRep_Dead() override                       [bDead=true면 PlayDeathMontage]
  ├─ PlayDeathMontage_Internal()                 [실제 Montage_Play 호출]
  ├─ K2_OnCharacterDied (BIE)                    [BP 추가 연출 훅]
  └─ MulticastHandleDeath_Implementation() override:
       Super::Multicast... (베이스 본체 실행 + bDead=true + OnRep_Dead 호출됨)
       추가: 사망 몽타주 인덱스 결정(서버), Mesh Tick 옵션 승격
```

### 2.2 호출 흐름 (Server / Client / Listen Server Host)
**서버 권위 발동 지점**: `UDRPlayerAttributeSet::ProcessCorruptedDamage` → `Die()` → `MulticastHandleDeath` (서버에서 트리거).

```
[Server / Listen Server Host]
  MulticastHandleDeath_Implementation (override in ADRCharacter)
    └ Super::Multicast... (베이스)
         ├ if (bDead) return;
         ├ bDead = true
         ├ (기존) Camera/Cue/1P↔3P/충돌/이동/물리/표정/Dissolve/디버프
         ├ OnRep_Dead()   ← 베이스에서 수동 호출
         │    └ override(ADRCharacter::OnRep_Dead): PlayDeathMontage_Internal()
         │         └ AnimInstance->Montage_Play(DeathMontages[DeathMontageIndex])
         └ OnDeathDelegate.Broadcast(this)
    └ (override 추가) DeathMontageIndex 결정 (서버)
                       Mesh->VisibilityBasedAnimTickOption = AlwaysTickPoseAndRefreshBones

[Remote Client] — 두 경로가 거의 동시에 들어옴, bDeathMontagePlayed로 중복 차단
  경로 ①: MulticastHandleDeath_Implementation 도착
    └ 동일한 흐름. bDead=true 직접 세팅, OnRep_Dead 수동 호출 → 몽타주 재생
  경로 ②: bDead 리플리케이션 도착 (ReplicatedUsing)
    └ OnRep_Dead 자동 호출 → bDeathMontagePlayed가 이미 true → no-op
```

### 2.3 왜 두 경로를 모두 두는가
1. **Multicast 경로**: 모든 정상 케이스 처리. 가장 빠름.
2. **RepNotify 경로(안전망)**: Relevancy 손실 후 회복, 늦은 조인, RPC drop, Channel close 같은 코너 케이스에서 `bDead` 상태를 일관되게 유지. `bDeathMontagePlayed` 플래그로 중복 호출 무해화.

---

## 3. C++ 구현 — 상세 코드

> 모든 코드는 그대로 붙여넣을 수 있도록 작성. 주석은 한국어 그대로 유지.

### 3.1 `Source/DaeRune/Public/Character/DRCharacterBase.h`

#### 변경 ①: `bDead` 필드 (line 110-112)
**기존**:
```cpp
protected:
    // 사망 상태
    bool bDead = false;
```
**변경 후**:
```cpp
public:
    // 사망 상태 (서버에서 MulticastHandleDeath로 변경, OnRep_Dead로 클라 동기화)
    UPROPERTY(ReplicatedUsing = OnRep_Dead, BlueprintReadOnly, Category = "Combat|Death")
    bool bDead = false;

    UFUNCTION()
    virtual void OnRep_Dead();
```

> 접근 제어자 변경(public)으로 한 단계 노출. `bDead`를 외부에서 변경하는 코드는 없으므로 안전.

#### 변경 ②: `OnRep_Stunned`/`OnRep_Burned` 선언 옆에 추가
이미 `OnRep_Stunned`, `OnRep_Burned`가 line 78-82에 있음. `OnRep_Dead`는 `bDead` 선언 바로 아래에 둠(위 변경 ①에 포함).

### 3.2 `Source/DaeRune/Private/Character/DRCharacterBase.cpp`

#### 변경 ③: `GetLifetimeReplicatedProps`
**기존** (line 49-57):
```cpp
void ADRCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 디버프 상태들을 모든 클라이언트에 동기화
    DOREPLIFETIME(ADRCharacterBase, bIsStunned);
    DOREPLIFETIME(ADRCharacterBase, bIsBurned);
    DOREPLIFETIME(ADRCharacterBase, bIsBeingShocked);
}
```
**변경 후**:
```cpp
void ADRCharacterBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 디버프 상태들을 모든 클라이언트에 동기화
    DOREPLIFETIME(ADRCharacterBase, bIsStunned);
    DOREPLIFETIME(ADRCharacterBase, bIsBurned);
    DOREPLIFETIME(ADRCharacterBase, bIsBeingShocked);

    // 사망 상태 (RepNotify 안전망)
    DOREPLIFETIME(ADRCharacterBase, bDead);
}
```

#### 변경 ④: `OnRep_Dead` 베이스 정의 (빈 구현)
`OnRep_Burned` 정의(line 265-267) 바로 아래에 추가:
```cpp
void ADRCharacterBase::OnRep_Dead()
{
    // 기본 구현은 비어있음. 파생 클래스(ADRCharacter)에서 사망 애니메이션 재생.
}
```

#### 변경 ⑤: `MulticastHandleDeath_Implementation` 끝에 수동 호출 + Mesh Tick 옵션
**기존** (line 162-247) 핵심 부분:
```cpp
void ADRCharacterBase::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
    if (bDead) return;
    bDead = true;
    ... (기존 로직 그대로) ...

    // 사망 이벤트 브로드캐스트
    OnDeathDelegate.Broadcast(this);
}
```
**변경 후** — `OnDeathDelegate.Broadcast(this);` 직전에 두 줄 삽입:
```cpp
void ADRCharacterBase::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
    if (bDead) return;
    bDead = true;
    ... (기존 로직 그대로) ...

    // ★ RepNotify는 클라이언트에서만 자동 호출됨. 서버에서도 동일한 처리를 위해 수동 호출.
    //    파생 클래스의 override가 호출되므로(virtual), ADRCharacter::OnRep_Dead가 몽타주 재생.
    OnRep_Dead();

    // ★ 사망 후 ABP가 계속 평가되도록 Mesh Tick 옵션 승격.
    //    플레이어/적 모두 적용해도 무해(곧 소멸).
    if (USkeletalMeshComponent* MeshComp = GetMesh())
    {
        MeshComp->VisibilityBasedAnimTickOption =
            EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }

    // 사망 이벤트 브로드캐스트
    OnDeathDelegate.Broadcast(this);
}
```

> `OnRep_Dead()`는 `virtual`이므로 `ADRCharacter` 인스턴스에서는 override가 호출됨. 적 인스턴스에서는 빈 베이스 구현이 호출되어 영향 없음.

### 3.3 `Source/DaeRune/Public/Character/DRCharacter.h`

#### 변경 ⑥: 헤더에 포워드 선언 추가
파일 상단의 `class UNiagaraSystem;` 인근에 추가:
```cpp
class UAnimMontage;
```
(이미 있을 가능성 높음. grep으로 확인 후 없으면 추가.)

#### 변경 ⑦: 클래스 public 영역에 사망 몽타주 관련 필드/함수 추가
`ADRCharacter` 클래스 안, "표정 컴포넌트 생성" 같은 다른 카테고리들 끝부분에 새 섹션 추가:
```cpp
public:
    // ========== 사망 애니메이션 ==========

    // 사망 시 재생할 몽타주들. 비어있으면 사망 몽타주 미재생(기존 동작).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Death")
    TArray<TObjectPtr<UAnimMontage>> DeathMontages;

    // 사망 몽타주 재생 속도
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Death", meta = (ClampMin = "0.1"))
    float DeathMontagePlayRate = 1.0f;

    // 서버가 결정한 사망 몽타주 인덱스 (모든 머신에서 동일한 몽타주 재생)
    UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat|Death")
    int32 DeathMontageIndex = INDEX_NONE;

    // BP에서 사망 시점에 추가 연출을 붙이고 싶을 때 사용 (선택)
    UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Death", meta = (DisplayName = "On Character Died"))
    void K2_OnCharacterDied();

    /** Combat Interface / 사망 처리 override */
    virtual void OnRep_Dead() override;
    virtual void MulticastHandleDeath_Implementation(const FVector& DeathImpulse) override;

protected:
    // 사망 몽타주가 이미 재생되었는지 (로컬 중복 방지 — 멀티캐스트와 RepNotify가 둘 다 도착해도 1회만 재생)
    bool bDeathMontagePlayed = false;

    // 사망 몽타주 재생 실제 구현 (모든 머신에서 호출 가능)
    virtual void PlayDeathMontage_Internal();
```

### 3.4 `Source/DaeRune/Private/Character/DRCharacter.cpp`

#### 변경 ⑧: 헤더 include
파일 상단의 include 블록에 추가(없을 경우):
```cpp
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
```

#### 변경 ⑨: `GetLifetimeReplicatedProps`에 `DeathMontageIndex` 등록
**기존**:
```cpp
void ADRCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADRCharacter, bIsCarryingPart);
    DOREPLIFETIME(ADRCharacter, CarriedPart);
    DOREPLIFETIME(ADRCharacter, PlayerCharacterClass);

    DOREPLIFETIME(ADRCharacter, bWaterPumpActive);
    DOREPLIFETIME_CONDITION(ADRCharacter, WaterPumpBeamEndPoint, COND_SkipOwner);
}
```
**변경 후** — 끝에 한 줄 추가:
```cpp
    DOREPLIFETIME(ADRCharacter, DeathMontageIndex);
```

#### 변경 ⑩: `MulticastHandleDeath_Implementation` override 추가
파일 끝 또는 다른 함수 정의 근처에 추가:
```cpp
void ADRCharacter::MulticastHandleDeath_Implementation(const FVector& DeathImpulse)
{
    // 서버에서만 인덱스 결정 (Super 호출 전이어야 OnRep_Dead 수동 호출 시점에 인덱스가 채워져 있음)
    if (HasAuthority() && DeathMontages.Num() > 0)
    {
        DeathMontageIndex = FMath::RandRange(0, DeathMontages.Num() - 1);
    }

    // 베이스 본체 실행: bDead=true, 충돌/이동/Dissolve/표정/디버프 정리, OnRep_Dead 수동 호출, Mesh Tick 옵션 승격
    Super::MulticastHandleDeath_Implementation(DeathImpulse);

    // 추가 BP 훅
    K2_OnCharacterDied();
}
```

#### 변경 ⑪: `OnRep_Dead` override
```cpp
void ADRCharacter::OnRep_Dead()
{
    Super::OnRep_Dead();

    if (bDead)
    {
        PlayDeathMontage_Internal();
    }
}
```

#### 변경 ⑫: `PlayDeathMontage_Internal` 구현
```cpp
void ADRCharacter::PlayDeathMontage_Internal()
{
    // 중복 재생 방지: 멀티캐스트와 RepNotify가 둘 다 호출돼도 1회만 재생.
    if (bDeathMontagePlayed) return;

    if (DeathMontages.Num() == 0) return;

    USkeletalMeshComponent* MeshComp = GetMesh();
    if (!MeshComp) return;

    UAnimInstance* AnimInst = MeshComp->GetAnimInstance();
    if (!AnimInst) return;

    // 서버 인덱스 결정이 아직 안 됐을 경우(이론상 RepNotify가 인덱스 도착보다 먼저 도착) 0번으로 폴백.
    int32 Idx = DeathMontageIndex;
    if (!DeathMontages.IsValidIndex(Idx))
    {
        Idx = 0;
    }

    UAnimMontage* Montage = DeathMontages[Idx];
    if (!Montage) return;

    // HitReact 같은 다른 몽타주가 진행 중이면 즉시 중단
    AnimInst->StopAllMontages(0.1f);

    const float PlayedLength = AnimInst->Montage_Play(Montage, DeathMontagePlayRate);
    if (PlayedLength > 0.f)
    {
        bDeathMontagePlayed = true;
    }
}
```

---

## 4. Blueprint / 자산 작업 (편집기에서 수동)

### 4.1 사망 몽타주 자산 준비
- 위치: `Content/Blueprints/Character/PlayerCharacter/<Class>/Animations/AM_Death_<Class>.uasset`.
- Slot Group: `DefaultGroup.DefaultSlot` (대상 ABP의 AnimGraph가 사용하는 슬롯과 일치).
- Blend In: 0.1~0.25s, Blend Out: 0(또는 매우 길게).
- 단일 Section 권장(`Default`만).
- 임시로는 기존 캐릭터의 idle 몽타주 한 개로 동작 확인 가능.

### 4.2 `BP_GardenRobot` 설정
1. 더블클릭 열기 → Class Defaults.
2. 검색창에 "death" 입력.
3. **Death Montages** 배열에 `AM_Death_GardenRobot` 추가.
4. **Death Montage Play Rate** 필요 시 조정(기본 1.0).

### 4.3 `BP_VendingMachine` 설정
- 위와 동일하게 `AM_Death_VendingMachine` 추가.

### 4.4 `ABP_Gardener` (3P, `Content/Blueprints/Character/PlayerCharacter/GardenRobot/`) 수정
1. **EventGraph 정리**
   - `Update Animation` 그래프에서 `Try Get Pawn Owner → ICombatInterface::IsDead → bool 변수 캐싱` 노드 체인 **삭제**.
   - 만약 다른 곳에서 `IsDead` 캐시 변수를 참조하면 그 참조부터 끊고 변수도 삭제.
2. **AnimGraph 정리**
   - 최종 출력 직전에 `Slot 'DefaultSlot'` 노드가 있는지 확인. 없으면 추가하고 기존 State Machine 출력을 그 슬롯의 Source로 연결.
   - 기존 State Machine에서 "Death" 상태로의 전이/Death 상태 자체를 **삭제**(또는 ResearchOnly로 비활성). 사망은 이제 DefaultSlot이 위에서 오버라이드함.
3. **컴파일 + 저장**.

### 4.5 `ABP_VendingMachine` (3P) 수정
- 위와 동일하게 처리.

### 4.6 1P ABP는 손대지 않음
- `ABP_FP_Gardener`, `ABP_FP_VendingMachine`은 1P 카메라용. 사망 시 1P 메시는 `SetVisibility(false)` 되고 3P로 전환(`DRCharacterBase.cpp:186-200`)이므로 1P ABP는 손댈 필요 없음.

### 4.7 적 ABP는 손대지 않음 ✋
- 적의 ABP가 현재 폴링 방식이라도 호스트에서 잘 보이므로 그대로 유지.

---

## 5. 테스트 플랜

### 5.1 PIE 시나리오 (Listen Server + Client 2)
| # | 시나리오 | 확인 포인트 |
|---|---|---|
| 1 | 클라이언트 A가 부패 상태에서 사망 | 호스트/클라B에서 A의 3P 사망 몽타주 재생 |
| 2 | 클라이언트 B가 사망 | 호스트/클라A에서 B의 3P 사망 몽타주 재생 |
| 3 | 호스트가 사망 | 클라A/B에서 호스트의 3P 사망 몽타주 재생, 호스트 본인 카메라는 3P 전환 후 사망 몽타주 재생 |
| 4 | 사망 직전 HitReact 진행 중 | HitReact 즉시 중단, 사망 몽타주로 전환 |
| 5 | 사망 후 3.5초 → Destroy | 메시 정상 제거, 메모리 누수 없음 |
| 6 | 적이 사망하는 경우 | 기존과 동일하게 동작(회귀 없음) |

### 5.2 임시 로그(작업 중에만)
디버깅용으로 다음 위치에 한시적 로그 추가 가능:
```cpp
UE_LOG(LogTemp, Warning, TEXT("[Death] %s HasAuth=%d bDead=%d Idx=%d Played=%d Path=%s"),
       *GetName(), HasAuthority(), bDead, DeathMontageIndex, bDeathMontagePlayed, TEXT(__FUNCTION__));
```
- `ADRCharacterBase::MulticastHandleDeath_Implementation` 진입 시
- `ADRCharacterBase::OnRep_Dead` 진입 시
- `ADRCharacter::PlayDeathMontage_Internal`의 각 early-return 직전

### 5.3 회귀 테스트
- `Beam Spell` (`DRBeamSpell.cpp:62`)의 `OnDeathDelegate` 바인딩
- `DRPhase1/2/3`의 `OnEnemyDeath` 호출 (적 사망에 의존)
- `DRPlayerController::OnSpectatedPlayerDied` (관전 카메라 전환)
- `ADREnemyAttributeSet::PostGameplayEffectExecute` → 적 사망 흐름

---

## 6. 작업 순서 (Step-by-Step)

1. **C++ 헤더 변경** — §3.1, §3.3 (5분).
2. **C++ 구현 변경** — §3.2, §3.4 (10분).
3. **Build** — Visual Studio에서 Development Editor 빌드. 컴파일 에러 0 확인.
4. **에디터 실행 + Hot Reload**.
5. **임시 몽타주 등록** — §4.1 임시판으로 일단 `AM_Death_*` 자리에 기존 임의 몽타주(예: idle pose 멈춤형)를 채워서 코드 동작 검증.
6. **ABP의 폴링 제거** — §4.4, §4.5. 이 단계에서 사망 시 임시 몽타주가 재생됨을 PIE에서 확인.
7. **정식 사망 몽타주 제작/등록** — 디자이너 협업 후 §4.2, §4.3.
8. **테스트 §5.1 전체 수행**.
9. **Dissolve 타이밍 조정 (필요 시)** — §7.
10. **임시 로그 제거 + 커밋**.

---

## 7. Dissolve 타이밍 결정

현재 `MulticastHandleDeath`는 `Dissolve()`를 즉시 호출(`DRCharacterBase.cpp:233`). 사망 몽타주 길이가 1.5~2초인 경우 메시가 사망 도중 사라질 수 있음.

**옵션**:
- **(A) 추천**: Dissolve 시작을 타이머로 지연. 사망 몽타주 평균 길이만큼.
  ```cpp
  // MulticastHandleDeath에서 Dissolve() 호출을 다음으로 교체:
  FTimerHandle DissolveDelayTimer;
  GetWorld()->GetTimerManager().SetTimer(DissolveDelayTimer,
      [WeakThis = TWeakObjectPtr<ADRCharacterBase>(this)]()
      {
          if (WeakThis.IsValid()) WeakThis->Dissolve();
      }, 1.5f, false);
  ```
- (B) Dissolve 타임라인 자체 duration을 늘림(BP 작업).
- (C) 현재대로 둠(빠른 페이드).

본 1차 작업에선 **(C) 현재대로** 유지하고, 테스트 후 시각적으로 어색하면 (A)로 조정.

---

## 8. 롤백 전략

- 작업 브랜치: `feat/death-anim-event-based` (별도 분기).
- 회귀 발생 시 단계별 롤백:
  1. **ABP 변경만 롤백** → 폴링 복귀, C++ 변경은 안전망으로 남음.
  2. **C++의 `MulticastHandleDeath` 끝에 추가한 두 줄 제거** → 기존 동작 완전 복원.
  3. **헤더의 `UPROPERTY` 데코레이션만 제거** → `bDead`를 plain bool로 되돌림.
- 최소 안전판: §3의 변경 ①(bDead 리플리케이트) + 변경 ⑤의 Mesh Tick 옵션 두 줄만 적용해도 현재 증상의 상당 부분 완화 가능성 있음.

---

## 9. 미해결 / 차후 결정 사항
- [ ] **정식 사망 몽타주 자산** — 디자이너 확정 필요.
- [ ] **Dissolve 타이밍** — §7 옵션 결정.
- [ ] **사망 카메라 애니메이션과 몽타주 동기화** — `PlayDeathCameraAnimation()`(`DRCharacter.h:74`)이 BP 이벤트로 사망 카메라를 재생. 몽타주 길이와 맞지 않으면 BP에서 조정.
- [ ] **본인 사망 시점 1P→3P 전환의 1프레임 깜빡임** — 사망 직전 1P 시점이 갑자기 3P로 바뀌어 어색할 수 있음. 카메라 트랜지션 부드럽게 처리(향후 별도 작업).
- [ ] **`State_Corrupt` 미진입 사망 케이스** — 현재 `ADRCharacterBase::Die`는 부패 상태가 아닌 플레이어 사망에서 `MulticastHandleDeath`를 호출하지 않음(`DRCharacterBase.cpp:86-148`). 이 경로의 의도된 설계인지 확인 필요(범위 외).
