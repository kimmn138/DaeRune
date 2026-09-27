# Plan7 — 두더지 엘리트 보스 (스테이지2 방6 열차 구간) 제작 계획

> **작성일** 2026-08-09 · **C++ 구현 완료** 2026-08-11 (§15) · **남은 작업: 에디터 전용** (§16)
> **선행 문서** `Plan6.md` §14.6 / §4.9 / §15.9-H
> **범위** Plan6에서 "별도 작업"으로 분리해 둔 항목 **§14.6.7 두더지 보스의 스킬·공격 패턴·스탯**
> **비범위** 열차 주행·좌석·장애물·배리어·구간 루프 (Plan6 M6에서 **C++ 구현 완료**, 이 문서는 그 위에 얹는다)

---

## 0. 요약 (TL;DR)

| 항목 | 결론 |
|---|---|
| 상속 | `ADRS2MoleBoss : public ADREnemy` — 체력/물보상/디버프/사망/외곽선/회전을 전부 상속 |
| 이동 | **없음.** `MOVE_None` + BT에 MoveTo 없음. 부모 `ADREnemy::Tick`의 "ControlRotation 추종 회전"만 사용 |
| 자세 | 캡슐 중심을 지면 높이에 두어 **상반신만 노출**(반매몰). 하반신은 지형에 묻혀 자동으로 피격 불가 |
| 기본공격 | 전방 **120°**, 사거리 **200uu(2m)** 부채꼴 **2타**. 1타/2타 × 1·2·3차 구간 = **6개 데미지 값** |
| 스킬1 | 잠수(무적) → 무작위 플레이어 1명 지정 → 발밑 경고 **0.8초** → **0.4초** 후 융기 → 캡슐 반지름 AoE, **플레이어+타 적** 데미지 + 공중 띄우기 |
| 스킬1 스케일 ★ | **데미지는 인원수(1~4)별**, **쿨다운은 구간(1/2/3차)별**. 기본공격만 구간별 데미지 |
| 전기장 | **3차 구간 한정.** N번째 융기 시 (N-1)번 융기 지점에 생성 + (N-2) 지점 것 제거 = **항상 1개** |
| 전기장 데미지 | **플레이어 전용**, **인원수(1~4) 룩업 배열**. 스테이지1 `BP_PosionGas` 에셋(전기 데칼/나이아가라/사운드) 재사용 |
| 신규 C++ | 5쌍(보스 1, GA 2, 경고 액터 1, 전기장 1) |
| 기존 수정 | 5건 (`DRS2TrainPhase` 4곳, `DREnemy` 2줄, `DRGameplayAbility` 쿨다운 훅 4줄, `CharacterClassInfo` enum 1줄, `DRGameplayTags` 추가만) |
| 페이즈 영향 | **`UDRS2TrainPhase`의 3구간 루프 로직은 그대로.** 추가되는 것은 "구간 인덱스·인원수 주입" + "보관/재등장 통지" 4줄뿐 |

---

## 1. Plan6 기찻길(방6) 구간 정밀 분석 — 보스가 놓이는 자리

### 1.1 진행 시퀀스에서 보스가 개입하는 지점

Plan6 §14.6.1 확정 시퀀스를 **보스 관점**으로 다시 그리면:

```
[방5 클리어 → D5 하강 개방]
      │
      ▼
[Boarding]  4칸 열차, 생존자 전원 착석
      │                                              ← 보스: 존재하지 않음 (미스폰)
      ▼
[Traveling → 장애물0 앞 StopDistance 도달 → 정지]
      │  Obstacle[0]->BreakByBoss()                   ← 보스가 "장애물을 부수며" 등장하는 연출 시점
      │  ForwardBarrier[0]/RearBarrier[0] ON
      ▼
[BossFight 0]  ★보스 첫 스폰. 체력 100%
      │  SpawnEnemyAt(MoleBossClass, Obstacle[0]->GetBossSpawnTransform())
      │  MoleBoss->OnHealthChanged 바인딩
      │  ── 여기서부터 이 문서가 정의하는 전투 ──
      │  체력 67% 도달
      ▼
[ResolveSegment(0)]  UnbindBossHealth → StashBoss(숨김/콜리전OFF/Brain STOP)
      │  ForwardBarrier[0] OFF, 재탑승 목표
      ▼
[Traveling → 장애물1 정지] → ReappearBoss(Obstacle[1] 스폰지점)  ★체력 67%에서 이어짐
      │  ... 34% 도달 → ResolveSegment(1)
      ▼
[Traveling → 장애물2 정지] → ReappearBoss(Obstacle[2])          ★체력 34%에서 이어짐
      │  ★3구간에는 임계가 없다 — 사망만이 조건
      ▼
[보스 사망] → bBossDefeated → ValidatePhaseCompletion() → TriggerGameClear()
```

실측 코드 앵커: `Source/DaeRune/Private/Phase/Stage2/DRS2TrainPhase.cpp:117-167`(정지/등장), `:213-240`(구간 해제), `:242-281`(보관/재등장), `:285-319`(사망).

### 1.2 페이즈가 보스에게 요구하는 계약 (이미 코드에 박혀 있음 — 반드시 지켜야 함)

| # | 계약 | 근거 코드 | 위반 시 증상 |
|---|---|---|---|
| C1 | `TSubclassOf<ADREnemy>`로 스폰 가능해야 한다 | `DRS2TrainPhase.h:47` `MoleBossClass` | BP 지정 자체가 불가 |
| C2 | `OnHealthChanged` / `OnMaxHealthChanged` 델리게이트를 발화해야 한다 | `DRS2TrainPhase.cpp:177-178` · `DREnemy.h:74-78` | 67%/34% 임계가 영원히 안 걸려 구간이 끝나지 않음 |
| C3 | `MaxHealth`가 스폰 직후 1회 이상 브로드캐스트돼야 한다 | `CachedBossMaxHealth`가 0이면 `HandleBossHealthChanged`가 조기 return (`:201`) | 임계 판정 전면 무효 |
| C4 | `ICombatInterface::GetOnDeathDelegate()`로 사망을 통지해야 한다 | `DRS2PhaseBase.cpp:279-282` | 최종 처치해도 스테이지 클리어가 안 됨 |
| C5 | **`SetActorHiddenInGame`/`SetActorEnableCollision`/`SetActorTickEnabled`/`BrainComponent StopLogic`으로 완전히 정지 가능해야 한다** | `StashBoss()` `:242-259` | 보관 중에도 타이머·GA가 돌아 유령 공격 발생 ★ |
| C6 | `SetActorTransform`으로 순간이동시켜도 상태가 깨지지 않아야 한다 | `ReappearBoss()` `:261-281` | 재등장 위치 오류/클라 슬라이딩 |

> **C5가 이 문서의 최대 설계 제약이다.** 두더지의 스킬1은 "잠수 → 대기 → 융기"라는 **1.8초짜리 타이머 체인**을 갖는데, 그 도중에 임계가 걸려 `StashBoss()`가 불리면 타이머만 살아남아 **숨겨진 보스가 땅에서 솟아올라 데미지를 주는** 버그가 난다. → §5.1의 `NotifyStashed()` 훅으로 반드시 취소한다.

### 1.3 보스가 페이즈에게 요구하는 것 (신규 — 4줄 추가)

| # | 필요 | 이유 |
|---|---|---|
| R1 | **현재 구간 인덱스(0/1/2)** | 기본공격 1타·2타 데미지가 구간별로 다르다 |
| R2 | **전기장 활성 여부** | 3차 구간(index 2)에서만 전기장을 만든다 |
| R3 | **기준 인원수(1~4)** | **융기 데미지**와 **전기장 데미지**가 인원수에 따라 다르다 (§2.5-C) |
| R4 | **보관/재등장 통지** | 진행 중이던 스킬1 타이머 체인 취소 + 전기장 정리 |

R1·R2는 같은 값(구간 인덱스)에서 파생되므로 **주입 지점은 2곳**(`HandleTrainStopped` 1회, `OnPhaseStart` 1회)뿐이다.

### 1.4 "체력이 3구간에 이어진다"가 보스 설계에 거는 제약 ★

Plan6 §14.6.4의 핵심은 **단일 개체 재사용**이다. 여기서 파생되는 규칙:

- **어트리뷰트를 재초기화하면 안 된다.** → 보스의 `BeginPlay`/`InitializeDefaultAttributes`는 **1회만** 돈다. 재등장 경로에는 어떤 어트리뷰트 초기화도 넣지 않는다.
- **런타임 상태(쿨다운 GE, 디버프 GE, 융기 카운터, 전기장 참조)도 그대로 이어진다.** 이건 대부분 바람직하지만 **전기장만은 예외** — 구간이 바뀌면 이전 구간의 전기장은 물리적으로 다른 장소에 남는다. → `NotifyStashed()`에서 전기장 파괴 + 융기 카운터 리셋.
- **`bDead` 판정을 타면 안 된다.** 도망은 `Die()`를 타지 않으므로 물 보상도 없다(Plan6 §4.9.7 명시). 3구간 최종 처치에서만 `bIsBoss = true`에 의해 전역 물 지급이 일어난다.

### 1.5 전투 구간의 물리적 형태

```
        RearBarrier[i]                                 ForwardBarrier[i]
             ║                                                ║
   ══════════╬════════[열차 정지]════════[부서진 장애물]═══════╬══════════  선로
             ║          ┌──────────────────────────┐          ║
             ║          │   전투 가능 영역          │          ║
             ║          │   (하차한 플레이어 이동)  │          ║
             ║          │        ▲ 두더지 등장      │          ║
             ║          └──────────────────────────┘          ║
```

- 배리어는 **Pawn만 Block**(`DRS2Barrier.h` 주석) → 투사체·열차는 통과. 두더지의 융기 AoE는 오버랩 기반이라 배리어의 영향을 받지 않는다.
- **NavMesh는 이 구간을 덮어야 한다**(Plan6 §8-10 "선로변 전투 구간 3곳"). 두더지는 이동하지 않지만 **융기 착지점 검증(`ProjectPointToNavigation`)** 에 NavMesh를 쓴다.
- 플레이어가 **좌석에 앉은 채로도 사격 가능**(§14.6.8-2 확정: "허용(이동만 잠금)"). → 두더지가 좌석 위 플레이어를 스킬 타깃으로 고르면 열차 위로 솟아오르는 그림이 나온다. **§3.3-D에서 착석자를 타깃 후보에서 제외**한다.

---

## 2. 사양 확정 — 사용자 지시의 수치화

### 2.1 상시 상태 (Idle / 전투 대기)

| 항목 | 값 | 비고 |
|---|---|---|
| 위치 이동 | **없음** | `MOVE_None`. 스킬1의 융기만이 유일한 위치 변경 수단 |
| 노출 정도 | **몸의 절반** | 캡슐 중심 Z = 지면 Z (= 캡슐 상단 절반만 지면 위) |
| 회전 | **어그로 대상 방향으로 제자리 Yaw 회전** | 부모 `ADREnemy::Tick`의 `RInterpTo(ActorRot → ControlRot)` 재사용 |
| 회전 속도 | `RotationInterpSpeed` (기본 10.0, 보스는 **4.0** 권장) | 느린 회전이 "등 뒤로 돌아가기" 플레이를 만든다 |
| 넉백/월스턴 | **면역** | `MOVE_None`이면 `LaunchCharacter`가 무효. `MinSpeedForStun` 미달로 월스턴도 미발생 |

### 2.2 기본공격 — 전방 부채꼴 2타

| 항목 | 값 |
|---|---|
| 형태 | 전방 **120°** 부채꼴 (반각 60°) |
| 사거리 | **200uu (2m)** — 수평(XY) 거리 기준 |
| 타수 | **2타** (몽타주 1개 안의 AnimNotify 2개) |
| 데미지 | **구간별 × 타수별 = 3×2 = 6개 값** |
| 판정 시점 | 각 타의 AnimNotify 순간에 **독립 오버랩** → 1타 후 빠져나가면 2타는 안 맞는다 |
| 대상 | 플레이어만 (`IsNotFriend` 통과분) |

**데미지 테이블 (플레이스홀더 — 밸런싱 대상)**

| 구간 | 1타 | 2타 | 합 |
|---|---|---|---|
| 1차 (장애물0, 체력 100→67%) | 20 | 30 | 50 |
| 2차 (장애물1, 67→34%) | 26 | 39 | 65 |
| 3차 (장애물2, 34→0%) | 32 | 48 | 80 |

### 2.3 스킬1 — 굴착 강습 (Burrow Strike)

```
 t=0.00 ┌ GA 활성. AM_MoleBoss_Burrow 재생 (잠수 연출)
        │
 t=0.60 ├ [잠수 완료 AnimNotify] ★무적 진입
        │   · SetActorHiddenInGame(true) + 콜리전 OFF + HealthBar 숨김
        │   · (옵션) 자신에게 걸린 Debuff.* 전부 제거
        │   · 무작위 생존 플레이어 1명 선정 (착석자·거리초과 제외)
        │   · 착지점 계산: 타깃 발밑 → 하향 라인트레이스 → NavMesh 투영
        │   · ADRS2GroundWarning 스폰 (Radius = 두더지 캡슐 반지름, Duration = 0.8)
        │
 t=1.40 ├ 경고 소멸 (액터 LifeSpan 만료)
        │
 t=1.80 ├ ★융기 (EruptDelayAfterWarning = 0.4 경과)
        │   · 텔레포트(숨겨진 상태에서) → 언하이드 → 콜리전 ON  [순서 고정]
        │   · 즉시 AoE 판정: 반경 = 캡슐 반지름
        │       - 플레이어  → 데미지 + 공중 띄우기
        │       - 다른 적   → 데미지 + 공중 띄우기  ★IsNotFriend 우회
        │   · 융기 카운터 += 1
        │   · [3차 구간만] 전기장 링 버퍼 갱신 (§2.4)
        │   · AM_MoleBoss_Emerge 재생 (순수 연출)
        │
 t≈2.6  └ 몽타주 종료 → GA EndAbility → 쿨다운 GE 적용
```

| 항목 | 값 | 근거 |
|---|---|---|
| 잠수 연출 시간 `BurrowDuration` | 0.6s | 몽타주 길이에 맞춤 |
| 경고 표시 시간 `WarningDuration` | **0.8s** | 사용자 확정 |
| 경고 종료 → 융기 `EruptDelayAfterWarning` | **0.4s** | 사용자 확정 ("표시한 후 .4초 후에") |
| 융기 AoE 반경 `EruptRadius` | **캡슐 반지름** (`GetScaledCapsuleRadius()`) | 사용자 확정 |
| 무적 구간 | 잠수 완료 ~ 융기 순간 (≈1.2s) | 사용자 확정 |
| 타깃 최대 거리 `MaxTargetRange` | 4000uu | 전투 구간 길이 상한 |
| **쿨다운** | **구간별** (10 / 8 / 6s 플레이스홀더) | ★확정. 커브 `Cooldown.MoleBoss.BurrowStrike` (§8.0) |
| **융기 데미지** | **인원수별** (40 / 46 / 52 / 58 플레이스홀더) | ★확정. 커브 `Abilities.MoleBoss.BurrowStrike` (§8.0) |
| 띄우기 | `LaunchZ = 700`, `LaunchRadial = 300` | 중심에서 방사형 + 상향 (인원·구간 무관 고정) |

> **★스케일링 축이 기본공격과 다르다** — 의도된 설계다.
> · **기본공격**: 사거리 2m라 "붙어야만" 맞는다 → **구간이 진행될수록 아파진다**(구간별 데미지)
> · **스킬1**: 반드시 누군가에게 날아간다 → **인원이 많을수록 아파진다**(인원별 데미지) + **구간이 진행될수록 자주 온다**(구간별 쿨다운)
> 즉 후반 구간의 압박은 스킬1의 **빈도**로, 인원 압박은 스킬1의 **위력**으로 나눠 표현한다. 두 축이 곱해지지 않으므로 4인 3차에서 난이도가 폭발하지 않는다.

### 2.4 전기장 — 3차 구간 한정, 항상 1개 (링 버퍼)

사용자 예시를 그대로 상태 전이로 옮기면:

| 융기 # | 두더지 위치 | 전기장 생성 | 전기장 제거 | 결과 (살아있는 전기장) |
|---|---|---|---|---|
| 1 | P1 | — | — | 없음 |
| 2 | P2 | **P1** | — | {P1} |
| 3 | P3 | **P2** | P1 | {P2} |
| 4 | P4 | **P3** | P2 | {P3} |
| n≥2 | Pn | **P(n-1)** | P(n-2) | {P(n-1)} |

= **"직전 융기 지점 1곳에만 전기장이 존재한다"** 는 단일 규칙. 구현은 배열이 아니라 **포인터 1개(`ActiveField`) + 직전 위치 1개(`PreviousEruptLocation`)** 로 충분하다.

| 항목 | 값 |
|---|---|
| 적용 구간 | **3차(`SegmentIndex == 2`)에서만** |
| 대상 | **플레이어만** (`Cast<ADRCharacter>` 성공한 액터) — 두더지 자신·타 적·열차·설치물 제외 |
| 데미지 | **인원수 룩업**: `DamagePerPlayerCount[BasePlayerCount-1]` |
| 틱 주기 | 0.5s (Periodic Infinite GE) |
| 반경 | 기본 = **융기 AoE 반경과 동일** (그 자리에 남은 흔적) |
| 지속 | **다음 융기까지 무한** (수명 타이머 없음) |
| 정리 시점 | 보스 사망 / `NotifyStashed()` / 페이즈 종료 / 액터 `EndPlay` |
| 에셋 | 스테이지1 재사용: `MI_ElectricFieldWarningDecal`, `SC_ElectricField`, `BP_PosionGas`의 나이아가라 |

**인원수별 데미지 (틱당, 플레이스홀더)**

| 인원 | 1인 | 2인 | 3인 | 4인 |
|---|---|---|---|---|
| 틱당 데미지 | 6 | 8 | 10 | 12 |
| 초당 환산(0.5s 틱) | 12 | 16 | 20 | 24 |

### 2.5 해석이 갈렸던 지점 — **전부 확정됨** ★ (2026-08-09 사용자 확정)

초안에서 한 가지로 읽히지 않던 4곳. **A·B·D는 초안 채택안 그대로 확정**, **C는 사용자 지시로 변경**되었다.

| # | 쟁점 | **확정 사양** | 초안 대비 |
|---|---|---|---|
| A | "**.8초간 경고 후 .4초 후에 솟아오름**" — 총 1.2초인가 0.8초인가 | **총 1.2초** (경고 0.8 종료 → 0.4 공백 → 융기) | 채택안 유지 |
| B | 경고창이 **타깃을 따라다니는가** | **고정** — 지정 순간 위치에 못박는다 (걸어 나가면 회피 성공) | 채택안 유지 |
| C | 융기(스킬1)의 스케일링 축 | ★**데미지는 플레이어 수에 따라, 쿨다운은 구간에 따라 달라진다** | **변경** — 초안의 "구간별 데미지"를 폐기 |
| D | 전기장 데미지의 "**플레이하는 플레이어 수**" | **방6 시작 시점 생존자 수**로 1회 확정 (`ResolveBasePlayerCount()`) | 채택안 유지 |

**C 변경이 문서 전체에 미치는 영향**

| 항목 | 초안 | 확정 |
|---|---|---|
| 융기 데미지 축 | 구간 | **인원수** |
| 스킬1 쿨다운 축 | 단일값 8s | **구간** |
| 필요한 신규 훅 | 없음 | **`UDRGameplayAbility::GetBaseCooldownDuration(ActorInfo)` virtual 추가** (§5.4) |
| 기본공격 | 구간별 1타/2타 | **변경 없음** — 사용자 최초 지시 그대로 |

> **값 보관 방식은 그 뒤 한 번 더 바뀌었다** (2026-08-12): 배열 → **커브 테이블(`FScalableFloat`)**. 축 결정은 위 표 그대로이고 저장 위치만 옮겼다. 최종 형태는 **§8.0 · §15.4**.

> **왜 D가 "1회 확정"인가**: Plan6 §14.1.4 / `DRS2PhaseBase.cpp:62-76`이 "죽으면 쉬워지는 역인센티브 방지 + UI 분모 일관성"을 이유로 전 스테이지2 공통 규약으로 못박아 뒀다. 전기장만 예외를 두면 규약이 깨진다.
> **C의 인원수도 같은 값을 쓴다** — 전기장과 융기 데미지가 **동일한 `BasePlayerCount`** 를 참조하므로, 전투 중 한 명이 죽어도 두 수치가 어긋나지 않는다.

---

## 3. 아키텍처 결정

### 3.1 클래스 배치 한눈에

```
Source/DaeRune/
├─ Public|Private/Character/Stage2/
│   └─ DRS2MoleBoss.h/.cpp              ★신규  : ADREnemy 상속. 상태·무적·융기·전기장 오너십
├─ Public|Private/AbilitySystem/Abilities/Stage2/
│   ├─ DRS2MoleClawAttack.h/.cpp        ★신규  : 120° 2m 부채꼴 2타 (UDRDamageGameplayAbility 상속)
│   └─ DRS2MoleBurrowStrike.h/.cpp      ★신규  : 잠수→경고→융기 상태 머신 (UDRDamageGameplayAbility 상속)
├─ Public|Private/Actor/Stage2/
│   ├─ DRS2GroundWarning.h/.cpp         ★신규  : 원형 지면 경고 데칼 액터 (복제)
│   └─ DRS2ElectricField.h/.cpp         ★신규  : 플레이어 전용 전기장 (복제)
├─ Public|Private/Phase/Stage2/
│   └─ DRS2TrainPhase.h/.cpp            ○수정  : 구간 인덱스·인원수 주입 + 보관/재등장 통지 (4곳)
├─ Public|Private/Character/
│   └─ DREnemy.h/.cpp                   ○수정  : RotationInterpSpeed 를 UPROPERTY 로 (2줄, 기본값 동일)
├─ Public|Private/AbilitySystem/Abilities/
│   └─ DRGameplayAbility.h/.cpp         ○수정  : GetBaseCooldownDuration() virtual 훅 (4줄, 기본 동작 동일)
├─ Public/AbilitySystem/Data/
│   └─ CharacterClassInfo.h             ○수정  : ECharacterClass 에 MoleBoss 1줄 추가 (맨 뒤)
└─ Public|Private/
    └─ DRGameplayTags.h/.cpp            ○수정  : 태그 5개 추가 (기존 라인 무수정)
```

> **폴더 규약**: Plan6 §15.0은 스테이지2 C++을 `Phase/Stage2/`, `Actor/Stage2/`에 두라고 규정한다. 캐릭터·어빌리티는 목록에 없으므로 **같은 규약을 `Character/Stage2/`, `AbilitySystem/Abilities/Stage2/`로 확장**한다.
> **인코딩**: 신규 파일은 **UTF-8 (BOM)**. 기존 파일 수정 시 **수정 라인 외 무수정** — `DRGameplayTags.h`, `DREnemy.h`는 이미 한글 주석이 모지바케 상태라 전체 재저장하면 손상이 확산된다.

### 3.2 왜 `ADREnemy` 상속인가 — 공짜로 얻는 7가지

| # | 기능 | 코드 앵커 | 안 쓰면 직접 만들어야 하는 것 |
|---|---|---|---|
| 1 | ASC + `UDREnemyAttributeSet` + Minimal 복제 | `DREnemy.cpp:41-44,66` | 체력/물 파이프라인 전부 |
| 2 | **`OnHealthChanged`/`OnMaxHealthChanged`** | `DREnemy.h:74-78` | ★페이즈 계약 C2/C3 |
| 3 | `Die()` + `MulticastHandleDeath` + `OnDeathDelegate` | `DRCharacterBase.h:38-39,60-61` | ★페이즈 계약 C4 |
| 4 | ControlRotation 추종 회전 (서버 계산 + 클라 스무딩) | `DREnemy.cpp:80-96` | ★사양 "제자리 회전"이 그대로 충족됨 |
| 5 | 디버프 Niagara(화상/스턴) + RepNotify | `DRCharacterBase.h:84-98,194-198` | 플레이어 디버프가 보스에게 안 통함 |
| 6 | 물 보상 (`bIsBoss = true` → 전역 지급) | `DREnemy.h:194` | 처치 보상 |
| 7 | 히트박스 자동 수집(`"Hitbox"` 태그) + 웨이브 외곽선 | `DREnemy.h:242-251,274-283` | 반매몰 상반신 전용 히트박스 배치에 그대로 사용 |

특히 **#4는 이번 사양의 절반**이다. `ADREnemy::Tick`이 이미
```cpp
FRotator NewRotation = FMath::RInterpTo(GetActorRotation(), GetControlRotation(), DeltaTime, 10.0f);
SetActorRotation(FRotator(0.f, NewRotation.Yaw, 0.f));
```
을 서버에서 돌리고, 클라는 `NetworkSimulatedSmoothRotationTime = 0.1f` + Exponential 스무딩으로 따라온다(`DREnemy.cpp:56-58`). → **AI 컨트롤러가 `SetFocus(CombatTarget)`만 호출하면 "제자리에서 어그로 방향으로 회전"이 완성된다.** 별도 회전 복제 설계 불필요.

### 3.3 C++ / BP 역할 분담 — Armadillo 선례를 그대로 따른다

`ADRArmadilloEnemy`의 주석이 명시하는 원칙(`DRArmadilloEnemy.h:41-46`):
> *C++ handles form switching, charge movement, and collision detection only. Damage/stun effects are applied by GA Blueprint.*

이 프로젝트에서 검증된 분업이므로 그대로 채택한다.

| 담당 | 내용 |
|---|---|
| **C++ (보스 액터)** | 반매몰 Z 계산 · 무적 토글 · 융기 텔레포트 · 타깃 선정 · 착지점 검증 · 전기장 링 버퍼 · 구간 인덱스 보관 · 상태 복제 |
| **C++ (GA)** | 부채꼴 오버랩/각도 판정 · 데미지 파라미터 조립 · AoE 대상 수집(적 포함) · 띄우기 벡터 계산 |
| **BP (GA)** | 몽타주 재생 · AnimNotify 배선 · 쿨다운 GE 지정 · 데미지 수치 입력 |
| **BP (액터)** | 메시/머티리얼 · 나이아가라 · 사운드 · 잠수/융기 연출 타임라인 (`BlueprintImplementableEvent` 훅) |

### 3.4 데미지 파이프라인 진입점 — `ApplyDamageEffect` 단일 관문

세 종류의 데미지가 전부 `UDRAbilitySystemLibrary::ApplyDamageEffect(FDamageEffectParams)`를 통과한다 (`DRAbilitySystemLibrary.cpp:425-465`).

```
기본공격 2타 ──┐
융기 AoE     ──┼─→ MakeDamageEffectParamsFromClassDefaults(Target)
              │      → Params.BaseDamage 를 구간/타수별 값으로 덮어쓰기
              │      → Params.KnockbackForce 를 직접 계산해 주입 (융기만)
              └─→ ApplyDamageEffect(Params)
                     → GE_MoleBossDamage (ExecCalc_Damage)
                     → UDRPlayerAttributeSet / UDREnemyAttributeSet 가 LaunchCharacter 수행

전기장 ──────→ (별도) TargetASC->MakeOutgoingSpec(GE_S2ElectricFieldDamage)
                     + AssignTagSetByCallerMagnitude(Damage.Lightning, 인원별값)
                     ※ ADRPoisonGasActor 와 동일한 "타깃 ASC 가 스펙을 만든다" 관례
```

**핵심 2가지:**

1. **띄우기는 새 코드가 필요 없다.** `FDamageEffectParams::KnockbackForce`가 0이 아니면 어트리뷰트셋이 알아서 `LaunchCharacter(KnockbackForce, true, true)`를 호출한다 — 플레이어(`DRPlayerAttributeSet.cpp:322-326`)와 적(`DREnemyAttributeSet.cpp:169-172`) 양쪽 모두. **우리는 벡터만 잘 만들어 주면 된다.**
2. **전기장은 `Damage.Lightning`을 재사용하되 `Debuff.Chance`를 주입하지 않는다.** `ExecCalc_Damage::DetermineDebuff`는 `Debuff.Chance`가 없으면 `-1`을 받아 `RandRange(1,100) <= -1` = false가 되므로(`ExecCalc_Damage.cpp:26-29`) **스턴이 절대 걸리지 않는다.** → 신규 데미지 타입 태그를 만들지 않아도 되고, `DamageTypeTags` 배열 수정도 불필요하다.

---

## 4. 신규 C++ 클래스 상세

> **★이 장의 클래스 정의는 초안(2026-08-09) 기준이다.** 수치 보관 방식이 2026-08-12에 `TArray` → `FScalableFloat`(커브 테이블)로 바뀌었다. **최종 프로퍼티 이름과 값 출처는 §8.0 · §15.4 를 본다.** 나머지(구조·알고리즘·순서 제약)는 구현과 일치한다.

### 4.1 `ADRS2MoleBoss` — 보스 본체

**파일**: `Public/Character/Stage2/DRS2MoleBoss.h` · `Private/Character/Stage2/DRS2MoleBoss.cpp`

```cpp
UCLASS()
class DAERUNE_API ADRS2MoleBoss : public ADREnemy
{
    GENERATED_BODY()
public:
    ADRS2MoleBoss();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;

    // ===== 페이즈 → 보스 주입 (UDRS2TrainPhase 가 호출, 서버 전용) =====
    /** 현재 장애물 구간(0/1/2). 발톱 데미지 · 스킬1 쿨다운의 인덱스이자 전기장 활성 조건. */
    UFUNCTION(BlueprintCallable, Category = "MoleBoss|Phase")
    void SetSegmentIndex(int32 InIndex);

    UFUNCTION(BlueprintPure, Category = "MoleBoss|Phase")
    int32 GetSegmentIndex() const { return SegmentIndex; }

    /** 방6 시작 시점 생존자 수(1~4). **융기 데미지 · 전기장 데미지** 공통 룩업 인덱스. */
    UFUNCTION(BlueprintCallable, Category = "MoleBoss|Phase")
    void SetBasePlayerCount(int32 InCount);

    UFUNCTION(BlueprintPure, Category = "MoleBoss|Phase")
    int32 GetBasePlayerCount() const { return BasePlayerCount; }

    /** ★페이즈가 StashBoss() 직전에 호출. 진행 중 스킬 취소 + 전기장 정리 + 융기 카운터 리셋. */
    UFUNCTION(BlueprintCallable, Category = "MoleBoss|Phase")
    void NotifyStashed();

    /** ★페이즈가 ReappearBoss() 직후에 호출. 반매몰 Z 재정렬 + 상태 재개. */
    UFUNCTION(BlueprintCallable, Category = "MoleBoss|Phase")
    void NotifyReappeared();

    // ===== 반매몰 =====
    /** 지면을 찾아 캡슐 중심 Z 를 BuriedRatio 에 맞춰 재배치. 스폰/재등장/융기 후 호출. */
    UFUNCTION(BlueprintCallable, Category = "MoleBoss|Pose")
    void SnapToBuriedPose();

    /** 이 지점에 반매몰로 설 때의 액터 위치. 착지점 계산에 GA 가 사용. */
    UFUNCTION(BlueprintPure, Category = "MoleBoss|Pose")
    FVector MakeBuriedLocation(const FVector& GroundPoint) const;

    // ===== 무적 (스킬1 잠수 구간) =====
    UFUNCTION(BlueprintCallable, Category = "MoleBoss|Burrow")
    void EnterBurrowedState();      // 숨김 + 콜리전 OFF + 헬스바 OFF (+옵션: 디버프 제거)

    UFUNCTION(BlueprintCallable, Category = "MoleBoss|Burrow")
    void ExitBurrowedState();       // 역연산

    UFUNCTION(BlueprintPure, Category = "MoleBoss|Burrow")
    bool IsBurrowed() const { return bBurrowed; }

    // ===== 융기 =====
    /** 서버: 텔레포트 → 언하이드 → 콜리전 ON → 전기장 링 버퍼 갱신. 순서 고정. */
    UFUNCTION(BlueprintCallable, Category = "MoleBoss|Burrow")
    void EruptAt(const FVector& GroundPoint, const FRotator& FacingRotation);

    /** 융기 AoE 반경 = 캡슐 반지름 × 배율. 경고창 반경과 반드시 같은 함수를 쓴다. ★단일 진실원 */
    UFUNCTION(BlueprintPure, Category = "MoleBoss|Burrow")
    float GetEruptRadius() const;

    UFUNCTION(BlueprintPure, Category = "MoleBoss|Burrow")
    int32 GetEruptCount() const { return EruptCount; }

    // ===== 전기장 =====
    UFUNCTION(BlueprintCallable, Category = "MoleBoss|Field")
    void DestroyActiveField();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type) override;
    virtual void Die(const FVector& DeathImpulse) override;   // 사망 시 전기장 정리

    // ---------- 설정 (BP) ----------
    /** 0 = 완전 노출, 0.5 = 절반 매몰, 1 = 완전 매몰 */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Pose", meta=(ClampMin="0.0", ClampMax="1.0"))
    float BuriedRatio = 0.5f;

    /** 지면 탐색 하향 트레이스 길이 */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Pose")
    float GroundTraceDistance = 1000.f;

    /** 융기 AoE 반경 배율 (1.0 = 캡슐 반지름 그대로 — 사용자 확정값) */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow", meta=(ClampMin="0.1"))
    float EruptRadiusScale = 1.f;

    /** 잠수 시 자신에게 걸린 Debuff.* 를 제거할지 (무적 누수 차단) */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow")
    bool bClearDebuffsOnBurrow = true;

    /** 3차 구간에서 전기장을 쓰는 구간 인덱스 */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Field")
    int32 ElectricFieldSegmentIndex = 2;

    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Field")
    TSubclassOf<class ADRS2ElectricField> ElectricFieldClass;

    /** 인원(1~4)별 전기장 틱 데미지 */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Field")
    TArray<float> FieldDamagePerPlayerCount;   // 기본 {6,8,10,12}

    // ---------- 복제 상태 ----------
    /** 클라 연출(3차 전용 이펙트, 데미지 UI)용. 서버가 SetSegmentIndex 에서 설정. */
    UPROPERTY(ReplicatedUsing = OnRep_SegmentIndex, BlueprintReadOnly, Category="MoleBoss|Phase")
    int32 SegmentIndex = 0;

    UPROPERTY(ReplicatedUsing = OnRep_Burrowed, BlueprintReadOnly, Category="MoleBoss|Burrow")
    bool bBurrowed = false;

    UFUNCTION() void OnRep_SegmentIndex();
    UFUNCTION() void OnRep_Burrowed();

    // ---------- BP 연출 훅 ----------
    UFUNCTION(BlueprintImplementableEvent, Category="MoleBoss|FX") void OnBurrowVisual();
    UFUNCTION(BlueprintImplementableEvent, Category="MoleBoss|FX") void OnEruptVisual(const FVector& Location);
    UFUNCTION(BlueprintImplementableEvent, Category="MoleBoss|FX") void OnSegmentChangedVisual(int32 NewSegment);

private:
    UPROPERTY() int32 BasePlayerCount = 1;
    UPROPERTY() int32 EruptCount = 0;
    UPROPERTY() TWeakObjectPtr<ADRS2ElectricField> ActiveField;
    FVector PreviousEruptGround = FVector::ZeroVector;
    bool bHasPreviousErupt = false;

    void UpdateElectricFieldRing(const FVector& NewEruptGround);
};
```

**생성자 핵심**
```cpp
ADRS2MoleBoss::ADRS2MoleBoss()
{
    bIsBoss = true;                                   // 처치 시 전역 물 지급 (DREnemy.h:194)
    GetCharacterMovement()->bUseRVOAvoidance = false; // 부동체는 회피 계산에서 뺀다
    GetCharacterMovement()->GravityScale = 0.f;
    RotationInterpSpeed = 4.f;                        // 부모 신규 UPROPERTY. 느린 보스 회전
    NetUpdateFrequency = 60.f;                        // 보스는 부모 기본(30)보다 높인다
    FieldDamagePerPlayerCount = {6.f, 8.f, 10.f, 12.f};
}

void ADRS2MoleBoss::BeginPlay()
{
    Super::BeginPlay();
    if (HasAuthority())
    {
        GetCharacterMovement()->SetMovementMode(MOVE_None);  // ★이동/중력/넉백 전면 차단
        SnapToBuriedPose();
    }
}
```

> **`MOVE_None`을 쓰는 이유**: ① `MaxWalkSpeed = 0`은 어트리뷰트(`OnMoveSpeedChanged`)가 매번 덮어쓰므로 불안정하다. ② 중력이 살아 있으면 반매몰 위치에서 지면 판정과 싸운다. ③ `LaunchCharacter`가 무효화돼 **플레이어 넉백 스킬로 보스를 밀 수 없다** — 보스로서 올바른 동작이며 월스턴(`ADREnemy::OnHit`)도 자동으로 봉인된다.

---

### 4.2 `UDRS2MoleClawAttack` — 기본공격 (120° / 2m / 2타)

**파일**: `Public|Private/AbilitySystem/Abilities/Stage2/DRS2MoleClawAttack.h/.cpp`

```cpp
USTRUCT(BlueprintType)
struct FDRMoleSegmentDamage
{
    GENERATED_BODY()
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite) float Hit1 = 0.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadWrite) float Hit2 = 0.f;
};

UCLASS()
class DAERUNE_API UDRS2MoleClawAttack : public UDRDamageGameplayAbility
{
    GENERATED_BODY()
public:
    /** 몽타주 AnimNotify 에서 호출. HitIndex: 0 = 1타, 1 = 2타 */
    UFUNCTION(BlueprintCallable, Category = "MoleBoss|Claw")
    void PerformClawSweep(int32 HitIndex);

protected:
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Claw") float SweepRadius = 200.f;   // 2m
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Claw") float SweepAngle  = 120.f;   // 전방 120도

    /** 반매몰 보정: 오버랩 구는 크게 잡고 판정은 XY 거리로 한다 */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Claw") float VerticalTolerance = 400.f;

    /** 인덱스 = 구간(0/1/2). 3개 미만이면 마지막 항목으로 폴백. */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Claw")
    TArray<FDRMoleSegmentDamage> SegmentDamages;

private:
    float ResolveDamage(int32 HitIndex) const;
};
```

```cpp
void UDRS2MoleClawAttack::PerformClawSweep(int32 HitIndex)
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!Avatar || !Avatar->HasAuthority()) return;          // ★서버 전용

    const FVector Origin = Avatar->GetActorLocation();
    FVector Forward = Avatar->GetActorForwardVector(); Forward.Z = 0.f;
    if (!Forward.Normalize()) return;

    const float CosHalf = FMath::Cos(FMath::DegreesToRadians(SweepAngle * 0.5f));
    const float Damage  = ResolveDamage(HitIndex);

    // ★DREliteSweepAttack(:22-28) 과 달리 구를 크게 잡는다.
    //   두더지는 캡슐 중심이 지면 높이라 플레이어(중심 ≈ 지면+88)와 Z 차이가 크다.
    //   반경 200 구로 잡으면 유효 수평거리가 sqrt(200²-88²) ≈ 179 로 줄어 사양(2m)이 깨진다.
    TArray<AActor*> Candidates;
    TArray<AActor*> Ignore{ Avatar };
    UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(
        Avatar, Candidates, Ignore, SweepRadius + VerticalTolerance, Origin);

    for (AActor* Target : Candidates)
    {
        if (!UDRAbilitySystemLibrary::IsNotFriend(Avatar, Target)) continue;   // 기본공격은 플레이어만

        FVector ToTarget = Target->GetActorLocation() - Origin;
        ToTarget.Z = 0.f;                                    // ① XY 평면으로 눕히고
        if (ToTarget.SizeSquared() > FMath::Square(SweepRadius)) continue;     // ② XY 거리로 사거리 판정
        if (!ToTarget.Normalize()) continue;
        if (FVector::DotProduct(Forward, ToTarget) < CosHalf) continue;        // ③ 부채꼴 각도 판정

        FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(Target);
        Params.BaseDamage = Damage;                          // ★구간/타수별 값으로 덮어쓴다
        UDRAbilitySystemLibrary::ApplyDamageEffect(Params);
    }

    // 물 보상 감소 (DREnemy.h:81-82). 2타면 2회 호출되므로 1타에서만 부른다.
    if (HitIndex == 0)
        if (ADREnemy* Enemy = Cast<ADREnemy>(Avatar)) Enemy->OnAttackExecuted();
}

float UDRS2MoleClawAttack::ResolveDamage(int32 HitIndex) const
{
    if (SegmentDamages.Num() == 0) return GetUpgradedDamage();       // 폴백: 부모 Damage 값
    int32 Seg = 0;
    if (const ADRS2MoleBoss* Boss = Cast<ADRS2MoleBoss>(GetAvatarActorFromActorInfo()))
        Seg = Boss->GetSegmentIndex();
    const FDRMoleSegmentDamage& D = SegmentDamages[FMath::Clamp(Seg, 0, SegmentDamages.Num() - 1)];
    return (HitIndex == 0) ? D.Hit1 : D.Hit2;
}
```

> **`GetLiveObjectsWithinRadius` 실측 동작** (`DRAbilitySystemLibrary.cpp:311-332`):
> `OverlapMultiByObjectType(..., AllDynamicObjects, MakeSphere(Radius))` → `Implements<UCombatInterface>() && !IsDead()` 필터 → `Execute_GetAvatar()` 를 `AddUnique`. **채널이 아니라 오브젝트 타입 질의**이며, `ADRCleanserSite` 도 별도 분기로 함께 수집된다.
> 시사점 3가지: ① 캡슐이 반매몰이어도 **오버랩 구에 걸리기만 하면** 수집되므로 위 방식이 안전하다. ② 반환 배열에 CleanserSite·다른 적이 섞여 들어오므로 **호출부에서 반드시 타입/진영 필터를 건다**(기본공격은 `IsNotFriend`, 융기는 `Cast<ADRCharacter>` / `Cast<ADREnemy>`). ③ `SetActorEnableCollision(false)` 인 액터는 오버랩 자체에 안 걸리므로 잠수 무적이 여기서도 성립한다.

---

### 4.3 `UDRS2MoleBurrowStrike` — 스킬1 (잠수 → 경고 → 융기)

**파일**: `Public|Private/AbilitySystem/Abilities/Stage2/DRS2MoleBurrowStrike.h/.cpp`

```cpp
UCLASS()
class DAERUNE_API UDRS2MoleBurrowStrike : public UDRDamageGameplayAbility
{
    GENERATED_BODY()
public:
    virtual void ActivateAbility(...) override;
    virtual void EndAbility(...) override;      // ★타이머 전량 해제 (취소 경로 포함)

    /** ★구간별 쿨다운. 부모의 신규 훅을 오버라이드한다 (§5.4). */
    virtual float GetBaseCooldownDuration(const FGameplayAbilityActorInfo* ActorInfo) const override;

    /** 잠수 몽타주 끝 AnimNotify → 여기서 무적 진입 + 타깃 지정 + 경고 스폰 */
    UFUNCTION(BlueprintCallable, Category="MoleBoss|Burrow")
    void BeginBurrowPhase();

protected:
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow") float WarningDuration = 0.8f;
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow") float EruptDelayAfterWarning = 0.4f;
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow") float MaxTargetRange = 4000.f;
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow") bool  bWarningFollowsTarget = false;
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow") bool  bExcludeSeatedPlayers = true;

    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow") TSubclassOf<class ADRS2GroundWarning> WarningActorClass;

    /** ★인원(1~4)별 융기 데미지. index = BasePlayerCount - 1. 항목이 모자라면 마지막 값으로 폴백. */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow") TArray<float> EruptDamagePerPlayerCount;

    /** ★구간(0/1/2)별 쿨다운(초). 비어 있으면 부모 CooldownDuration 으로 폴백. */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow") TArray<float> CooldownPerSegment;

    /** 띄우기: 상향 성분 + 중심에서 밖으로 미는 성분 */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow") float LaunchZ = 700.f;
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow") float LaunchRadial = 300.f;

    /** 융기 AoE 가 다른 적에게도 적중하는지 (사용자 확정: true) */
    UPROPERTY(EditDefaultsOnly, Category="MoleBoss|Burrow") bool bHitOtherEnemies = true;

    UFUNCTION(BlueprintImplementableEvent, Category="MoleBoss|Burrow") void K2_OnEruptMontage();

private:
    ADRCharacter* PickRandomTarget() const;
    bool ResolveEruptGround(const AActor* Target, FVector& OutGround) const;
    void HandleWarningFinished();      // 경고 종료 → EruptDelay 타이머
    void HandleErupt();                // ★융기 본체
    void ApplyEruptDamage(const FVector& Center);

    TWeakObjectPtr<ADRCharacter>      CachedTarget;
    TWeakObjectPtr<ADRS2GroundWarning> WarningActor;
    FVector  PendingEruptGround = FVector::ZeroVector;
    FTimerHandle WarningTimer, EruptTimer;
};
```

**① 타깃 선정**
```cpp
ADRCharacter* UDRS2MoleBurrowStrike::PickRandomTarget() const
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    const ADRGameStateBase* GS = Avatar->GetWorld()->GetGameState<ADRGameStateBase>();
    if (!GS) return nullptr;

    TArray<ADRCharacter*> Pool;
    for (ADRCharacter* P : GS->GetAlivePlayers())      // DRGameStateBase.cpp:91-115 (IsDead 필터 포함)
    {
        if (!IsValid(P)) continue;
        if (bExcludeSeatedPlayers && P->IsSeatedOnTrain()) continue;   // ★열차 위로 솟지 않게
        if (FVector::Dist2D(P->GetActorLocation(), Avatar->GetActorLocation()) > MaxTargetRange) continue;
        Pool.Add(P);
    }
    if (Pool.Num() == 0) return nullptr;               // → 스킬 취소, 쿨다운 짧게 재시도
    return Pool[FMath::RandRange(0, Pool.Num() - 1)];
}
```

**② 착지점 계산 — 3중 검증**
```cpp
bool UDRS2MoleBurrowStrike::ResolveEruptGround(const AActor* Target, FVector& OutGround) const
{
    // (1) 타깃 발밑에서 하향 트레이스 → 실제 지면
    const FVector Start = Target->GetActorLocation();
    const FVector End   = Start - FVector(0, 0, 1500.f);
    FHitResult Hit; FCollisionQueryParams Q; Q.AddIgnoredActor(Target);
    if (!GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Q)) return false;

    // (2) NavMesh 투영 — 열차 지붕/난간 위 등 "솟아오를 수 없는 곳" 배제
    FNavLocation NavLoc;
    if (UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
        if (Nav->ProjectPointToNavigation(Hit.ImpactPoint, NavLoc, FVector(200.f)))
            { OutGround = NavLoc.Location; return true; }

    OutGround = Hit.ImpactPoint;   // NavMesh 없으면 트레이스 결과 사용 (Warning 로그)
    return true;
}
```

**③ 융기 — 순서가 곧 사양**
```cpp
void UDRS2MoleBurrowStrike::HandleErupt()
{
    ADRS2MoleBoss* Boss = Cast<ADRS2MoleBoss>(GetAvatarActorFromActorInfo());
    if (!Boss || !Boss->HasAuthority()) { EndAbility(...); return; }

    // 경고가 타깃 추종 모드면 마지막 위치로 갱신
    if (bWarningFollowsTarget && CachedTarget.IsValid())
        ResolveEruptGround(CachedTarget.Get(), PendingEruptGround);

    const FRotator Facing = CachedTarget.IsValid()
        ? (CachedTarget->GetActorLocation() - PendingEruptGround).Rotation()
        : Boss->GetActorRotation();

    // ★순서 고정: 텔레포트(숨겨진 상태) → 언하이드 → 콜리전 ON → 링버퍼 → 판정
    Boss->EruptAt(PendingEruptGround, FRotator(0.f, Facing.Yaw, 0.f));

    ApplyEruptDamage(Boss->GetActorLocation());
    K2_OnEruptMontage();                    // BP: AM_MoleBoss_Emerge 재생 + 나이아가라
}
```

**④ AoE 판정 — 플레이어 + 다른 적, 띄우기 포함**
```cpp
void UDRS2MoleBurrowStrike::ApplyEruptDamage(const FVector& Center)
{
    ADRS2MoleBoss* Boss = Cast<ADRS2MoleBoss>(GetAvatarActorFromActorInfo());
    const float R = Boss->GetEruptRadius();          // ★경고창과 같은 함수 = 같은 값

    TArray<AActor*> Targets; TArray<AActor*> Ignore{ Boss };
    UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(
        Boss, Targets, Ignore, R + VerticalTolerance, Center);

    // ★데미지 축 = 인원수 (구간이 아니다 — §2.5-C 확정)
    const float Dmg = EruptDamagePerPlayerCount.Num() > 0
        ? EruptDamagePerPlayerCount[FMath::Clamp(Boss->GetBasePlayerCount() - 1, 0, EruptDamagePerPlayerCount.Num() - 1)]
        : GetUpgradedDamage();

    for (AActor* T : Targets)
    {
        const bool bPlayer = (Cast<ADRCharacter>(T) != nullptr);
        const bool bEnemy  = (Cast<ADREnemy>(T)     != nullptr);
        // ★IsNotFriend 를 쓰지 않는다 — 아군(다른 적)에게도 맞아야 하는 사양이다
        if (!bPlayer && !(bEnemy && bHitOtherEnemies)) continue;

        FVector Flat = T->GetActorLocation() - Center; Flat.Z = 0.f;
        if (Flat.SizeSquared() > FMath::Square(R)) continue;      // XY 반경 판정

        // 띄우기 벡터: 중심에서 방사형 + 상향. 정중앙이면 순수 상향.
        const FVector Radial = Flat.IsNearlyZero() ? FVector::ZeroVector : Flat.GetSafeNormal();
        const FVector Knock  = Radial * LaunchRadial + FVector(0.f, 0.f, LaunchZ);

        FDamageEffectParams P = MakeDamageEffectParamsFromClassDefaults(T);
        P.BaseDamage        = Dmg;
        P.KnockbackForce    = Knock;      // ★어트리뷰트셋이 LaunchCharacter 를 수행한다
        P.DeathImpulse      = Knock;
        UDRAbilitySystemLibrary::ApplyDamageEffect(P);
    }
}
```

> **왜 `IsNotFriend`를 우회해도 되는가**: `ApplyDamageEffect`(`DRAbilitySystemLibrary.cpp:425`)는 진영 검사를 하지 않는다. 검사는 **호출부**(`DREliteSweepAttack.cpp:47` 등)의 관례일 뿐이다. 따라서 융기 AoE만 검사를 생략하면 되고, 다른 어떤 공격에도 영향이 없다.
> **주의**: 다른 적이 융기로 **죽으면** `UDREnemyAttributeSet`의 사망 경로를 타서 물 보상이 지급된다(`GrantWaterToPlayers`). 두더지가 잡몹을 죽여 플레이어에게 물을 주는 셈인데, §14.6.8-6에서 "전투 중 잡몹 동반 없음"으로 확정되어 실전에서는 거의 발생하지 않는다. 발생 시에도 이득 방향이라 무해.

**⑤ 취소 안전성 — `EndAbility` 오버라이드**
```cpp
void UDRS2MoleBurrowStrike::EndAbility(...)
{
    if (UWorld* W = GetWorld())
    {
        W->GetTimerManager().ClearTimer(WarningTimer);
        W->GetTimerManager().ClearTimer(EruptTimer);
    }
    if (WarningActor.IsValid()) WarningActor->Destroy();
    if (ADRS2MoleBoss* Boss = Cast<ADRS2MoleBoss>(GetAvatarActorFromActorInfo()))
        if (Boss->IsBurrowed()) Boss->ExitBurrowedState();     // 숨긴 채 끝나지 않게
    Super::EndAbility(...);
}
```
`ADRS2MoleBoss::NotifyStashed()`가 `ASC->CancelAbilityHandle` / `CancelAbilities(BurrowTag)`를 호출하면 이 경로를 타므로 **계약 C5가 지켜진다.**

**⑥ 구간별 쿨다운 — 부모 훅 오버라이드**
```cpp
float UDRS2MoleBurrowStrike::GetBaseCooldownDuration(const FGameplayAbilityActorInfo* ActorInfo) const
{
    if (CooldownPerSegment.Num() == 0) return Super::GetBaseCooldownDuration(ActorInfo);   // 폴백

    // ★ActorInfo 를 쓰는 이유: ApplyCooldown 은 CDO 에서 호출될 수 있어
    //   GetAvatarActorFromActorInfo()(= GetCurrentActorInfo 기반)가 비어 있을 수 있다.
    //   DRGameplayAbility.h:53-56 이 CheckCost/ApplyCost/ApplyCooldown 에 대해 명시한 함정이다.
    const AActor* Avatar = ActorInfo ? ActorInfo->AvatarActor.Get() : nullptr;
    const ADRS2MoleBoss* Boss = Cast<ADRS2MoleBoss>(Avatar);
    const int32 Seg = Boss ? Boss->GetSegmentIndex() : 0;

    return CooldownPerSegment[FMath::Clamp(Seg, 0, CooldownPerSegment.Num() - 1)];
}
```

- 반환값은 부모 `ApplyCooldown`이 `Data.Cooldown` SetByCaller 로 주입한다(`DRGameplayAbility.cpp:198-207`). 따라서 **`GE_Cooldown_MoleBoss_BurrowStrike` 의 Duration 은 반드시 `SetByCaller(Data.Cooldown)`** 로 만들어야 한다 — 고정 Duration 으로 만들면 구간값이 무시된다.
- 부모가 `MinCooldownDuration`(기본 0.05)으로 하한을 잡아 주므로 0 입력 사고가 나도 무한 연타는 발생하지 않는다.
- 적에게는 업그레이드 칩이 없어 `GetUpgradeRuntimeFor(ActorInfo)`가 항등 캐시를 돌려준다(`DRGameplayAbility.h:50-51` 주석). 즉 `EffectiveCooldown == 구간값` 이다.

---

### 4.4 `ADRS2GroundWarning` — 원형 지면 경고

**파일**: `Public|Private/Actor/Stage2/DRS2GroundWarning.h/.cpp`

```cpp
UCLASS()
class DAERUNE_API ADRS2GroundWarning : public AActor
{
    GENERATED_BODY()
public:
    ADRS2GroundWarning();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;

    /** 서버 전용. 스폰 직후 1회 호출. 반지름·지속시간을 복제하고 LifeSpan 을 건다. */
    UFUNCTION(BlueprintCallable, Category="S2|Warning")
    void InitWarning(float InRadius, float InDuration);

    /** 추종 모드에서 GA 가 매 틱 갱신 (bWarningFollowsTarget = true 일 때만) */
    UFUNCTION(BlueprintCallable, Category="S2|Warning")
    void UpdateLocation(const FVector& NewGroundLocation);

protected:
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent>  SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDecalComponent>  WarningDecal;   // -90 Pitch (PoisonGas 관례)
    UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> WarningFX;     // 선택

    UPROPERTY(EditDefaultsOnly, Category="S2|Warning") TObjectPtr<UMaterialInterface> DecalMaterial;

    UPROPERTY(ReplicatedUsing=OnRep_WarningParams) float Radius   = 100.f;
    UPROPERTY(ReplicatedUsing=OnRep_WarningParams) float Duration = 0.8f;

    UFUNCTION() void OnRep_WarningParams();

    /** BP 연출: 반경/지속시간을 받아 링 확장 타임라인 등을 돌린다 */
    UFUNCTION(BlueprintImplementableEvent, Category="S2|Warning")
    void OnWarningBegin(float InRadius, float InDuration);
};
```

- 서버가 `SetLifeSpan(Duration)` → 복제 삭제로 클라에서도 사라진다. 별도 정리 코드 불필요.
- `OnRep_WarningParams`와 서버 `InitWarning` **양쪽에서** `OnWarningBegin`을 호출한다 — **리슨 서버에서도 연출이 돌아야 하므로 RepNotify 수동 호출**(Plan6 §15.0 규약).
- 데칼 크기는 `DecalSize = FVector(300.f, Radius, Radius)` — `ADRPoisonGasActor.cpp:47` 관례 그대로.
- 머티리얼은 스테이지1 `MI_ElectricFieldWarningDecal` 재사용 가능(빨강 계열이 필요하면 파생 MI 1개 신규).

---

### 4.5 `ADRS2ElectricField` — 플레이어 전용 전기장

**파일**: `Public|Private/Actor/Stage2/DRS2ElectricField.h/.cpp`

**설계 판단: `ADRPoisonGasActor` 상속이 아니라 신규 액터로 만든다.**

| 상속했을 때 필요한 부모 수정 | 신규 액터일 때 |
|---|---|
| `BeginPlay`의 하드코딩 `3.0f` 경고 시간을 UPROPERTY 화 | 처음부터 파라미터 |
| `TransitionToActive()` 를 virtual 로 승격 | 불필요 |
| `static OverlapCountMap` 공유 → 스테이지1 가스와 카운트가 뒤섞임 ★ | 격리됨 |
| GE 를 SetByCaller 로 적용하도록 `ADREffectActor::ApplyEffectToTarget` 확장 | 자체 구현 |
| 슬로우 GE 경로 무력화 | 없음 |
| **스테이지1 회귀 위험** | **0** |

→ **코드는 새로 쓰고, 에셋(데칼 MI · 나이아가라 · `SC_ElectricField` 사운드)만 재사용**한다. 사용자가 말한 "스테이지1에 쓰이던 전기장"의 실체는 시각·청각 자산이며, 로직은 요구사항이 다르다(무한 지속·플레이어 전용·인원 스케일).

```cpp
UCLASS()
class DAERUNE_API ADRS2ElectricField : public AActor
{
    GENERATED_BODY()
public:
    ADRS2ElectricField();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>&) const override;

    /** 서버 전용. 스폰 직후 1회. 인스티게이터(보스)·반경·틱 데미지를 확정한다. */
    UFUNCTION(BlueprintCallable, Category="S2|Field")
    void InitField(AActor* InInstigatorActor, float InRadius, float InTickDamage);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type) override;

    UPROPERTY(VisibleAnywhere) TObjectPtr<USceneComponent>   SceneRoot;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent>  EffectSphere;   // ECC_Pawn Overlap 전용
    UPROPERTY(VisibleAnywhere) TObjectPtr<UDecalComponent>   GroundDecal;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UNiagaraComponent> FieldFX;
    UPROPERTY(VisibleAnywhere) TObjectPtr<UAudioComponent>   LoopAudio;      // SC_ElectricField

    /** Periodic Infinite. Period = TickInterval, Execution = ExecCalc_Damage,
        SetByCaller = Damage.Lightning (Debuff.Chance 는 주입하지 않아 스턴이 안 걸린다) */
    UPROPERTY(EditDefaultsOnly, Category="S2|Field") TSubclassOf<UGameplayEffect> FieldDamageEffectClass;

    UPROPERTY(EditDefaultsOnly, Category="S2|Field") float DefaultRadius = 150.f;
    UPROPERTY(EditDefaultsOnly, Category="S2|Field") float ActivationDelay = 0.f;  // 0 = 즉시 (경고 불필요)

    UPROPERTY(ReplicatedUsing=OnRep_FieldRadius) float FieldRadius = 150.f;
    UFUNCTION() void OnRep_FieldRadius();

    UFUNCTION(BlueprintImplementableEvent, Category="S2|Field") void OnFieldActivated(float InRadius);

    UFUNCTION() void OnSphereBegin(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32, bool, const FHitResult&);
    UFUNCTION() void OnSphereEnd(UPrimitiveComponent*, AActor*, UPrimitiveComponent*, int32);

private:
    void ApplyToPlayer(ADRCharacter*);
    void RemoveFromPlayer(ADRCharacter*);
    float TickDamage = 0.f;
    TWeakObjectPtr<AActor> FieldInstigator;
    TMap<TWeakObjectPtr<ADRCharacter>, FActiveGameplayEffectHandle> AppliedHandles;
};
```

**핵심 구현 포인트**

```cpp
void ADRS2ElectricField::ApplyToPlayer(ADRCharacter* Player)
{
    if (!HasAuthority() || !FieldDamageEffectClass) return;
    if (AppliedHandles.Contains(Player)) return;                       // 중복 방지
    if (ICombatInterface::Execute_IsDead(Player)) return;

    UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent();
    if (!ASC) return;

    // ADRPoisonGasActor.cpp:92-97 관례: 타깃 ASC 가 스펙을 만들고, 인스티게이터로 출처를 남긴다
    FGameplayEffectContextHandle Ctx = ASC->MakeEffectContext();
    Ctx.AddSourceObject(FieldInstigator.Get());
    Ctx.AddInstigator(FieldInstigator.Get(), this);

    FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(FieldDamageEffectClass, 1.f, Ctx);
    UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
        Spec, FDRGameplayTags::Get().Damage_Lightning, TickDamage);
    // ★Debuff.Chance 를 넣지 않는다 → ExecCalc_Damage.cpp:26-29 에서 -1 이 되어 스턴 미발생

    AppliedHandles.Add(Player, ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()));
}
```

| 규칙 | 구현 |
|---|---|
| **플레이어만** | `Cast<ADRCharacter>(OtherActor)` 성공 시에만 처리. **`"Player"` 액터 태그에 의존하지 않는다** — 그 태그는 C++ 어디에서도 부여하지 않고 BP 설정에만 존재하므로(`grep` 확인: 검사만 5곳, 부여 0곳) 레벨/BP 실수에 취약하다. `ADRRobotVacuumCharacter`도 `ADRCharacter` 자식이라 함께 잡힌다 |
| **두더지 자신·타 적 제외** | 위 캐스트로 자동 배제 |
| **사망 시 누수 방지** | `EndPlay` + `Destroy` 시 `AppliedHandles` 전량 `RemoveActiveGameplayEffect`. 추가로 각 플레이어의 `OnDeathDelegate`에 붙지 않고 **틱 GE 자체가 사망한 대상에 무해**하므로 `EndPlay` 일괄 정리로 충분 |
| **스테이지1 가스와 무간섭** | `static OverlapCountMap` 을 쓰지 않는다 |

---

## 5. 기존 파일 수정 — 최소 침습

### 5.1 `UDRS2TrainPhase` (4곳)

```cpp
// ① OnPhaseStart() — 인원 확정 추가 (기존 코드 어디에도 호출이 없다)
void UDRS2TrainPhase::OnPhaseStart()
{
    Super::OnPhaseStart();
    ResolveBasePlayerCount();          // ★추가: 융기 데미지 + 전기장 데미지 공통 룩업 인덱스 확정
    ...
}

// ② HandleTrainStopped() — 보스 스폰/재등장 직후, BindBossHealth() 앞
    if (ADRS2MoleBoss* Mole = Cast<ADRS2MoleBoss>(MoleBoss.Get()))
    {
        Mole->SetBasePlayerCount(BasePlayerCount);
        Mole->SetSegmentIndex(ObstacleIndex);      // ★R1·R2
        Mole->NotifyReappeared();                  // ★R4 (첫 스폰에서도 안전 — 멱등)
    }

// ③ StashBoss() 맨 앞
    if (ADRS2MoleBoss* Mole = Cast<ADRS2MoleBoss>(Boss))
        Mole->NotifyStashed();                     // ★계약 C5: 스킬 취소 + 전기장 정리

// ④ OnPhaseEnd() — 배리어 정리 옆
    if (ADRS2MoleBoss* Mole = Cast<ADRS2MoleBoss>(MoleBoss.Get()))
        Mole->DestroyActiveField();
```

- 전부 `Cast<>` + null 가드라 **임시 `EliteBear` 보스로도 그대로 동작한다** (Plan6 §14.6.7 "보스 구현이 바뀌어도 페이즈 코드는 수정되지 않는다"의 정신 유지).
- 헤더에는 `class ADRS2MoleBoss;` 전방 선언 1줄만 추가.

### 5.2 `ADREnemy` (2줄)

```cpp
// DREnemy.h  protected 구역
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta=(ClampMin="0.1"))
float RotationInterpSpeed = 10.f;          // ★기존 하드코딩 10.0 을 그대로 기본값으로

// DREnemy.cpp:93  ── 리터럴만 교체
FRotator NewRotation = FMath::RInterpTo(CurrentActorRot, CurrentControlRot, DeltaTime, RotationInterpSpeed);
```
기본값이 동일하므로 **기존 모든 적의 동작이 바이트 단위로 같다.** 두더지 BP에서만 4.0으로 낮춘다.

### 5.3 `CharacterClassInfo.h` (1줄)

```cpp
enum class ECharacterClass : uint8
{
    Elementalist, Warrior, Ranger, Bear, PartEnemy,
    MoleBoss                    // ★맨 뒤에 추가 — 기존 값의 정수 인덱스가 변하지 않는다
};
```
`DA_EnemyCharacterClassInfo`에 `MoleBoss` 행을 추가하고 `GE_PrimaryAttributes_MoleBoss` / `GE_VitalAttributes_Enemy` / `StartupAbilities`(Claw, BurrowStrike, HitReact)를 넣는다.

### 5.4 `UDRGameplayAbility` — 쿨다운을 런타임 조건으로 바꿀 수 있게 (4줄) ★신규 요구

**왜 필요한가**: 스킬1의 쿨다운이 **구간별로 달라야 한다**(§2.5-C 확정). 그런데 현재 `ApplyCooldown`은 멤버 `CooldownDuration`을 직접 읽고(`DRGameplayAbility.cpp:185, 202`), `ApplyCooldown` 자체가 `const` 함수라 파생 클래스가 값을 갈아끼울 방법이 없다.

**최소 침습 해법 — virtual 훅 1개 추가**

```cpp
// DRGameplayAbility.h  (public, CooldownDuration 선언 아래)
/** 쿨다운 기본값 훅. 파생 클래스가 런타임 조건(보스 구간 등)에 따라 바꿀 수 있게 한다.
    ★ActorInfo 를 인자로 받는 이유는 위 CooldownDuration 주석(:53-56)과 동일하다 —
      ApplyCooldown 은 CDO 에서 호출될 수 있어 GetCurrentActorInfo() 가 비어 있을 수 있다. */
virtual float GetBaseCooldownDuration(const FGameplayAbilityActorInfo* ActorInfo) const { return CooldownDuration; }
```

```cpp
// DRGameplayAbility.cpp  ApplyCooldown() — 리터럴 참조 2곳만 교체
void UDRGameplayAbility::ApplyCooldown(...) const
{
    const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
    if (!CooldownGE) return;

    const float BaseCooldown = GetBaseCooldownDuration(ActorInfo);      // ★신규 1줄

    if (BaseCooldown <= 0.f)                                            // ★CooldownDuration → BaseCooldown
    { ... Super::ApplyCooldown(...); return; }

    FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());
    if (!SpecHandle.IsValid()) return;

    const float UpgradedCooldown = GetUpgradeRuntimeFor(ActorInfo)
        .ApplySkill(GetUpgradeKeyTag(), EDRUpgradeStat::SkillCooldown, BaseCooldown);   // ★교체
    ...
}
```

| 항목 | 평가 |
|---|---|
| 기존 어빌리티 영향 | **없음.** 기본 구현이 `return CooldownDuration;` 이라 모든 기존 GA 의 동작이 바이트 단위로 동일 |
| 업그레이드 칩 파이프라인 | 그대로 유지 — `ApplySkill(SkillCooldown, ...)` 이 훅 반환값에 적용된다. 플레이어 스킬에 이 훅을 오버라이드해도 칩이 정상 작동 |
| 하한 보호 | `MinCooldownDuration` 클램프가 그대로 살아 있어 0 입력 사고 시에도 무한 연타 불가 |
| 대안 (미채택) | ① 두더지 GA 에서 `ApplyCooldown` 전체를 복붙 오버라이드 → 업그레이드 로직이 이중화돼 향후 어긋남 ② `CooldownDuration` 을 `mutable` 로 → const 계약을 깨고 CDO 안전성 상실 |

### 5.5 `DRGameplayTags` (추가만, 5개)

| 태그 | 용도 |
|---|---|
| `Abilities.MoleBoss.Claw` | BT Task 의 `TryActivateAbilitiesByTag` 키 |
| `Abilities.MoleBoss.BurrowStrike` | 〃 + 취소 대상 지정 |
| `Cooldown.MoleBoss.BurrowStrike` | 쿨다운 GE / BT Service 의 준비 여부 판정 |
| `State.MoleBoss.Burrowed` | 잠수 중 `ActivationOwnedTags`. 기본공격 GA 의 `ActivationBlockedTags` |
| `Debuff.All` (선택) | `bClearDebuffsOnBurrow` 용 컨테이너를 코드에서 5개 태그로 조립하면 불필요 |

> `DRGameplayTags.h`는 한글 주석이 모지바케 상태다. **struct 끝부분에 5줄, `InitializeNativeGameplayTags()` 끝부분에 5블록만 추가**하고 나머지 라인은 절대 건드리지 않는다.
> `Debuff.All`을 만들지 않고 코드에서
> `Container.AddTag(Debuff_Burn); ... AddTag(Debuff_Bleed);` 로 조립한 뒤 `ASC->RemoveActiveEffectsWithGrantedTags(Container)` 를 호출하는 편이 안전하다(부모 태그 등록 여부에 의존하지 않음).

---

## 6. 핵심 알고리즘

### 6.1 반매몰 Z 계산 — 단일 공식

캡슐 중심 기준 좌표계에서, 지면 `Gz`, 캡슐 반높이 `H`, 매몰 비율 `r`:

```
              r = 0                r = 0.5              r = 1
            ┌───────┐
            │       │  H
Gz ─────────┤       ├──── ─────┌───────┐──── ────────────────────
            │       │          │       │          ┌───────┐
            └───────┘          └───────┘          └───────┘
        완전 노출            절반 매몰            완전 매몰

  CenterZ = Gz + H * (1 - 2r)
```

| r | CenterZ | 결과 |
|---|---|---|
| 0.0 | Gz + H | 바닥에 선 일반 캐릭터 |
| **0.5** | **Gz** | **사양: 몸의 절반만 노출** |
| 1.0 | Gz − H | 완전히 묻힘 |

```cpp
FVector ADRS2MoleBoss::MakeBuriedLocation(const FVector& GroundPoint) const
{
    const float H = GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
    return FVector(GroundPoint.X, GroundPoint.Y, GroundPoint.Z + H * (1.f - 2.f * BuriedRatio));
}

void ADRS2MoleBoss::SnapToBuriedPose()
{
    if (!HasAuthority()) return;
    const FVector Start = GetActorLocation();
    FHitResult Hit; FCollisionQueryParams Q; Q.AddIgnoredActor(this);
    const FVector Ground = GetWorld()->LineTraceSingleByChannel(
        Hit, Start, Start - FVector(0,0,GroundTraceDistance), ECC_WorldStatic, Q)
        ? Hit.ImpactPoint : Start;
    SetActorLocation(MakeBuriedLocation(Ground), false, nullptr, ETeleportType::TeleportPhysics);
}
```

**부수 효과 (전부 의도된 것)**
- 하반신은 지형 안이라 **투사체/트레이스가 도달할 수 없다** → 자연스러운 "상반신만 때릴 수 있는" 판정.
- `ADREnemy`의 히트박스 시스템(`"Hitbox"` 태그 자동 수집, `DREnemy.h:242-255`)으로 **상반신 전용 히트박스**를 BP에서 추가하면 판정이 더 정확해진다. 권장.
- `HealthBar` 빌보드는 캡슐 루트에 붙어 있으므로 `RelativeLocation.Z`를 `+H` 정도 올려야 머리 위에 뜬다.

### 6.2 부채꼴 판정 — 왜 XY 로 눕히는가

```
        측면도                      평면도 (판정)
                                        ▲ Forward
   플레이어 ●  중심 Z ≈ Gz+88            ╱ ╲   60°
            │                        ╱     ╲
   ─────────┼──── Gz              ╱   부채꼴  ╲
   두더지   ◍  중심 Z = Gz         ◍───────────  R = 200
                                두더지

   3D 거리 200 로 판정하면 유효 수평거리 = √(200² − 88²) ≈ 179  ← 사양 위반
   XY 거리 200 로 판정하면 정확히 2m
```
따라서 **오버랩 구는 `R + VerticalTolerance`(=600) 로 넓게, 판정은 `ToTarget.Z = 0` 후 `Size() <= R`** 로 한다. 이 2단 구조는 `UDREliteRoar::TickRoarAura`(`DREliteRoar.cpp:57-86`)가 이미 쓰고 있는 프로젝트 관례다.

### 6.3 굴착 강습 상태 머신 (취소 경로 포함)

```
                 ┌──────────────── ASC->CancelAbilities(BurrowStrike) ─────────────┐
                 │        (NotifyStashed / 사망 / 페이즈 종료 / 스턴)              │
                 ▼                                                                 │
   [Idle] ──활성──▶ [BurrowMontage] ──Notify──▶ [Burrowed]                          │
                       0.6s                     · 무적 ON                           │
                                                · 타깃 선정 ─(후보 0명)──▶ [Cancel]─┤
                                                · 경고 스폰                          │
                                                     │ WarningDuration 0.8s          │
                                                     ▼                               │
                                                [WarningGone]                        │
                                                     │ EruptDelay 0.4s               │
                                                     ▼                               │
                                                 [Erupt] ──▶ AoE + 링버퍼 + 몽타주   │
                                                     │                               │
                                                     ▼                               │
                                                 [EndAbility] ◀──────────────────────┘
                                                     │  타이머 해제 · 경고 파괴 · 무적 해제 보정
                                                     ▼
                                                  쿨다운 GE
```

**후보 0명일 때**: `[Cancel]`로 즉시 종료하고 **쿨다운을 적용하지 않는다**(`bWasCancelled = true`) → 다음 BT 틱에서 재시도. 전원이 열차에 앉아 있는 상황에서 보스가 영구 잠수하는 사태를 막는다.

### 6.4 전기장 링 버퍼

```cpp
void ADRS2MoleBoss::UpdateElectricFieldRing(const FVector& NewEruptGround)
{
    if (!HasAuthority()) return;
    if (SegmentIndex != ElectricFieldSegmentIndex) { PreviousEruptGround = NewEruptGround; bHasPreviousErupt = true; return; }

    // ① 이전 전기장 제거  (= P(n-2) 소멸)
    DestroyActiveField();

    // ② 직전 융기 지점에 새 전기장 생성  (= P(n-1) 생성). 첫 융기에는 직전 지점이 없다.
    if (bHasPreviousErupt && ElectricFieldClass)
    {
        const int32 Idx = FMath::Clamp(BasePlayerCount - 1, 0, FMath::Max(0, FieldDamagePerPlayerCount.Num() - 1));
        const float Dmg = FieldDamagePerPlayerCount.IsValidIndex(Idx) ? FieldDamagePerPlayerCount[Idx] : 8.f;

        ADRS2ElectricField* Field = GetWorld()->SpawnActorDeferred<ADRS2ElectricField>(
            ElectricFieldClass, FTransform(PreviousEruptGround));
        Field->InitField(this, GetEruptRadius(), Dmg);
        Field->FinishSpawning(FTransform(PreviousEruptGround));
        ActiveField = Field;
    }

    // ③ 다음 회차를 위해 현재 지점을 직전 지점으로 승격
    PreviousEruptGround = NewEruptGround;
    bHasPreviousErupt   = true;
}
```

> **1·2차 구간에서도 `PreviousEruptGround`를 계속 갱신**해 두면, 3차 진입 후 **첫 융기**에서 곧바로 전기장이 생긴다. 이게 맞는지가 §12-3 확인 항목이다. **기본 진행값: 구간이 바뀌면 `NotifyStashed()`에서 `bHasPreviousErupt = false`로 초기화** → 3차의 1번 융기에는 전기장이 없고 2번 융기부터 생긴다 (사용자 예시의 "1번 솟아오르고 2번째 솟아오를 때"와 정확히 일치).

### 6.5 무적 처리 — 3중 차단

| 경로 | 차단 방법 |
|---|---|
| 직격(투사체·근접 트레이스) | `SetActorEnableCollision(false)` — 트레이스가 통과 |
| AoE(`GetLiveObjectsWithinRadius`) | 위와 동일. `OverlapMultiByObjectType(AllDynamicObjects)` 에 잡히지 않음 |
| **지속 데미지(화상 등 이미 걸린 Debuff GE)** | ★콜리전과 무관하게 계속 틱한다. → `bClearDebuffsOnBurrow = true`면 잠수 시 `RemoveActiveEffectsWithGrantedTags({Burn, Stun, Arcane, Physical, Bleed})` |

세 번째를 놓치면 **"무적인데 체력이 줄어 임계가 걸리고, 페이즈가 `StashBoss()`를 호출해 잠수 중인 보스를 다시 숨기는"** 이중 상태가 만들어진다. `NotifyStashed()`가 스킬을 취소하므로 크래시는 없지만 연출이 깨진다.

### 6.6 융기 텔레포트와 클라이언트 스무딩

`ADREnemy`는 `NetworkSmoothingMode = Exponential`(`DREnemy.cpp:58`)이라 큰 위치 점프가 **슬라이딩**으로 보일 수 있다. 안전장치 3중:

1. **숨긴 상태에서 이동한다** — `EruptAt()`은 `SetActorLocation` → `SetActorHiddenInGame(false)` 순서. 슬라이딩이 일어나도 보이지 않는다.
2. `ETeleportType::TeleportPhysics` 사용.
3. UE의 `NetworkNoSmoothUpdateDistance`(기본 384uu)를 넘는 이동은 자동 스냅된다. 융기 거리는 대부분 이보다 크다.

---

## 7. 네트워크 설계 요약

| 데이터 | 방식 | 근거 |
|---|---|---|
| 보스 위치/회전 | 기존 `ADREnemy` 경로 (ReplicatedMovement + CMC 스무딩) | 신규 설계 불필요 (§3.2-#4) |
| `SegmentIndex` | `UPROPERTY(ReplicatedUsing)` | 3차 전용 클라 연출(전기 오라 등) |
| `bBurrowed` | `UPROPERTY(ReplicatedUsing)` | 클라에서 잠수/융기 나이아가라 재생 |
| 경고창 | **복제 액터** + `SetLifeSpan` | 스폰/삭제가 자동 동기화. 0.8초 텔레그래프에 1틱 지연은 무해 |
| 전기장 | **복제 액터**, GE 적용은 서버 전용 | `ADRPoisonGasActor` 관례 |
| 공격 몽타주 | GA 의 `PlayMontageAndWait` | ★아래 주의 |
| 데미지/띄우기 | 서버 전용 → 어트리뷰트 복제 + `LaunchCharacter` | 기존 파이프라인 |

> **몽타주 주의 (프로젝트 기존 이슈)**: `ADRArmadilloEnemy`는 *"GA의 PlayMontage 노드는 서버(호스트)에서만 재생되므로, 원격 클라이언트에는 `MulticastPlayFormChangeMontage()`로 동일 몽타주를 재생해준다"* 는 주석과 함께 멀티캐스트를 병행한다(`DRArmadilloEnemy.h:128-151`). 두더지도 **원격 클라에서 공격/잠수/융기 모션이 안 보이면 같은 패턴을 적용**한다. M1 검증 항목에 넣어 둔다.
>
> **RepNotify 수동 호출**: 서버에서 `bBurrowed`/`SegmentIndex`를 바꾼 뒤 `OnRep_*()`를 **직접 호출**한다 (리슨 서버 연출 — Plan6 §15.0 규약).

---

## 8. 데이터 · 밸런스 표 (플레이스홀더, 전부 BP 노출)

> **★2026-08-12 개정** — 모든 전투 수치를 **커브 테이블**로 옮겼다. 근거와 전환 내역은 §15.4.

### 8.0 값 출처 한눈에 ★

| 값 | 축 | 커브 테이블 | 행 이름 | 조회 레벨 | C++ 필드 |
|---|---|---|---|---|---|
| **MaxHealth** | 인원 | `CT_EnemyAttributes` | `Mole.MaxHealth` | `Level` (= 인원) | — (엔진이 처리) |
| **융기 데미지** | 인원 | `CT_Damage` | `Abilities.MoleBoss.BurrowStrike` | `GetAbilityLevel()` (= 인원) | — (상속받은 `Damage`) |
| 발톱 1타 | **구간** | `CT_Damage` | `Abilities.MoleBoss.Claw.Hit1` | `Segment + 1` | `Hit1Damage` |
| 발톱 2타 | **구간** | `CT_Damage` | `Abilities.MoleBoss.Claw.Hit2` | `Segment + 1` | `Hit2Damage` |
| 스킬1 쿨다운 | **구간** | `CT_Damage` | `Cooldown.MoleBoss.BurrowStrike` | `Segment + 1` | `BurrowCooldown` |
| 전기장 틱 데미지 | 인원 | `CT_Damage` | `Abilities.MoleBoss.ElectricField` | GE 스펙 레벨 (= 인원) | — (**GE 의 Modifier**) |

**핵심 두 가지**

1. **인원 축은 `Level` 하나에서 전부 파생된다.** 페이즈가 지연 스폰으로 `Level = BasePlayerCount` 를 심으면, 같은 값이 `CT_EnemyAttributes`(체력)와 어빌리티 스펙 레벨(융기 데미지)을 동시에 인덱싱한다. **이 두 값에는 C++ 코드가 한 줄도 없다.**
2. **구간 축은 `Level` 에 실을 수 없다** (레벨은 스칼라 하나인데 축이 두 개). 대신 `FScalableFloat::GetValueAtLevel(Segment + 1)` 로 **어빌리티 레벨과 무관하게** 커브를 직접 조회한다.
3. **전기장은 GE 스펙 레벨을 쓴다.** 액터라 어빌리티 레벨이 없는 대신, `MakeOutgoingSpec(GE, 인원수, Ctx)` 로 스펙을 만들면 GE 의 **Modifier(ScalableFloat)** 가 그 레벨로 커브를 읽는다 — 스테이지1 `GE_PoisonDamage` 와 동일한 구조이며, **C++ 에 데미지 숫자가 아예 없다**.

**GE 구성 방식 두 가지 — 왜 다른가**

| | 발톱 · 융기 | 전기장 |
|---|---|---|
| GE | `GE_Damage` | `GE_S2ElectricFieldDamage` |
| 구성 | **Executions = `ExecCalc_Damage`** | **Modifier = `IncomingDamage`(ScalableFloat)** |
| 수치 전달 | SetByCaller (C++ 계산) | **스펙 레벨 → 커브** (C++ 계산 없음) |
| 왜 | 어빌리티라 **넉백 벡터·사망 임펄스**를 컨텍스트에 실어야 한다 | 지역 위험물이라 넉백이 없다. `IncomingDamage` 에 값만 넣으면 `HandleIncomingDamage` 가 컨테이너 체력·부식·업그레이드 칩·피격 큐를 전부 처리한다 |
| 선례 | 모든 어빌리티 | **`GE_PoisonDamage`** (같은 패턴) |

> ★두 방식 모두 **`IncomingDamage` 에 도달한다**는 점이 핵심이다. `Health` 를 직접 깎는 것과 혼동하면 안 된다 — 그건 컨테이너 체력 시스템을 통째로 우회한다.
> 전기장은 컨텍스트에 데미지 타입 태그가 없으므로 **디버프(스턴)가 원천적으로 걸리지 않는다.** 사양과 정확히 일치한다.

> ★**`Value` 는 반드시 1 로 둔다.** `FScalableFloat` 의 결과는 `Value × Curve[Level]` 이다. 커브를 걸어 놓고 `Value` 를 0으로 두면 **조용히 항상 0**이 된다 — 커브 테이블 도입 시 제일 많이 나는 사고라, 스폰/어빌리티 부여 시점에 Error 로그로 잡도록 방어 코드를 넣어 두었다.

### 8.1 스탯 (`GE_PrimaryAttributes_MoleBoss`)

| 어트리뷰트 | 출처 | 비고 |
|---|---|---|
| MaxHealth | **`CT_EnemyAttributes / Mole.MaxHealth`, 레벨 1~4 = 인원수** | ★인원수로 스케일된다 |
| MaxWater | GE 에서 Override 0 | 물 소모 없음 |
| MoveSpeed | GE 에서 Override 0 | `MOVE_None`이라 무의미하나 2차 안전장치 |

> **★체력이 인원수로 스케일되면서 밸런스 전제가 바뀌었다** (초안 §8.1 은 고정 체력을 가정했다)
>
> 체력과 파티 DPS가 함께 늘면 **구간 클리어 시간이 인원수와 거의 무관해진다.** 따라서 초안이 기대했던
> "1인은 스킬을 여러 번 받고 4인은 적게 받는" 자동 보정은 **사라진다** — 인원과 무관하게 노출 횟수가 비슷해진다.
>
> | 체력 커브를 이렇게 두면 | 구간 클리어 시간 | 스킬 노출 횟수 |
> |---|---|---|
> | 인원수에 **정비례** (1500 / 3000 / 4500 / 6000) | 인원 무관 ≈ 일정 | 인원 무관 ≈ 일정 |
> | 인원수보다 **완만하게** (1500 / 2600 / 3600 / 4500) | 인원 많을수록 짧아짐 | 인원 많을수록 적음 |
>
> **결과적으로 융기 데미지 커브가 인원 난이도를 조절하는 유일한 손잡이가 된다.** 체력 커브와 데미지 커브가
> 같은 방향으로 가파르면 4인 체감이 두 배로 커지므로, 둘 중 하나만 세우는 편이 튜닝하기 쉽다.
> 권장 출발점: **체력은 완만하게, 융기 데미지는 완만하게** 두고 플레이테스트로 한쪽만 조인다.

### 8.2 커브 행 입력값 (플레이스홀더) ★

**(a) 구간 축 — `CT_Damage`, 커브 레벨 = 구간 + 1**

| 행 이름 | Lv1 (1차) | Lv2 (2차) | Lv3 (3차) |
|---|---|---|---|
| `Abilities.MoleBoss.Claw.Hit1` | 20 | 26 | 32 |
| `Abilities.MoleBoss.Claw.Hit2` | 30 | 39 | 48 |
| `Cooldown.MoleBoss.BurrowStrike` | **10.0** | **8.0** | **6.0** |

**(b) 인원 축 — 커브 레벨 = 인원수**

| 테이블 / 행 이름 | Lv1 (1인) | Lv2 | Lv3 | Lv4 (4인) |
|---|---|---|---|---|
| `CT_EnemyAttributes / Mole.MaxHealth` | *(이미 작성됨)* | | | |
| `CT_Damage / Abilities.MoleBoss.BurrowStrike` | 40 | 46 | 52 | 58 |
| `CT_Damage / Abilities.MoleBoss.ElectricField` | 6 | 8 | 10 | 12 |
| └ 초당 환산 (0.5s 틱) | 12 | 16 | 20 | 24 |

> 모든 커브의 Interp / Extrap 은 **Constant** 로 둔다 (`RCIM_Constant` / `RCCE_Constant`) — 프로젝트의 기존 `CT_Damage` 행들과 동일하다. 정수 레벨만 조회하므로 보간이 끼어들 여지를 없앤다.
> 쿨다운 행이 `CT_Damage` 에 있는 게 어색해 보일 수 있으나, 커브 테이블은 용도 제약이 없고 **행 이름 접두사로 구분**된다. "MoleBoss"로 검색하면 보스 튜닝 값 5개가 한 화면에 모이는 편이 낫다.

**(c) 축과 무관한 고정값**

| 항목 | 값 |
|---|---|
| 발톱 사거리 / 각도 | 200uu(2m) / 120° 부채꼴 |
| 융기 반경 / 전기장 반경 | 캡슐 반지름 (`GetEruptRadius()`) |
| 띄우기 | `LaunchZ = 700`, `LaunchRadial = 300` |
| 융기 대상 | 플레이어 + **다른 적** |
| 전기장 대상 | **플레이어 전용** |

> **왜 두 축을 곱하지 않는가**: 융기 데미지를 구간·인원 양쪽으로 스케일하면 4인 3차에서 58 × 1.6 ≈ 93 이 되어 컨테이너 하나(100HP)를 한 방에 날린다. 축을 나눠 두면 **3차의 위협은 "10초에 한 번 → 6초에 한 번"이라는 빈도**로, **4인의 위협은 "40 → 58"이라는 위력**으로 각각 독립 조정된다. 밸런싱 시 한 축을 건드려도 다른 축이 오염되지 않는다.
>
> ★단 **체력이 인원 축에 올라탔으므로**, 인원 난이도는 이제 "체력 커브 × 융기 데미지 커브" 두 개가 함께 만든다. 위 검산 박스를 참고해 둘 중 하나만 세우는 것을 권한다.

### 8.3 타이밍

| 항목 | 값 |
|---|---|
| 발톱 몽타주 총 길이 | 1.4s (1타 notify 0.45s, 2타 notify 0.95s) |
| 발톱 쿨다운 / BT 재시도 간격 | 1.0s |
| 잠수 몽타주 | 0.6s |
| 경고 | 0.8s |
| 경고→융기 공백 | 0.4s |
| 융기 몽타주 | 0.8s |
| 스킬1 총 시전 시간 (활성→융기) | **1.8s** (잠수 0.6 + 경고 0.8 + 공백 0.4) |
| **스킬1 쿨다운** | **구간별 10 / 8 / 6s** — `CooldownPerSegment` |
| 스킬1 실효 주기 (쿨다운 + 시전) | 1차 ≈11.8s · 2차 ≈9.8s · 3차 ≈7.8s |

---

## 9. 에셋 · BP 체크리스트

### 9.1 신규 BP / 데이터

| # | 에셋 | 부모 | 경로 |
|---|---|---|---|
| 1 | `BP_S2MoleBoss` | `ADRS2MoleBoss` | `Content/Blueprints/Character/Enemy/MoleBoss/` |
| 2 | `ABP_S2MoleBoss` | AnimInstance | 〃 |
| 3 | `AM_MoleBoss_Claw` | AnimMontage | 〃 `/Montage/` — **AnimNotify 2개** |
| 4 | `AM_MoleBoss_Burrow` | AnimMontage | 〃 — 끝에 `Notify_BurrowDone` |
| 5 | `AM_MoleBoss_Emerge` | AnimMontage | 〃 |
| 6 | `GA_S2MoleBoss_Claw` | `UDRS2MoleClawAttack` | `Content/Blueprints/AbilitySystem/Enemy/MoleBoss/Abilities/` |
| 7 | `GA_S2MoleBoss_BurrowStrike` | `UDRS2MoleBurrowStrike` | 〃 |
| 8 | `GE_PrimaryAttributes_MoleBoss` | GameplayEffect | 〃 `/Effects/` |
| 9 | `GE_Cooldown_MoleBoss_BurrowStrike` | GameplayEffect | 〃 — ★**Duration 을 반드시 `SetByCaller(Data.Cooldown)`** 로. 고정 Duration 이면 구간별 쿨다운이 무시된다 |
| 10 | `GE_MoleBossDamage` | GameplayEffect (`ExecCalc_Damage`) | 〃 — 또는 기존 적 데미지 GE 재사용 |
| 11 | `GE_S2ElectricFieldDamage` | GameplayEffect | `Content/Blueprints/Actor/Stage2/Field/` — **Infinite + Period 0.5 + SetByCaller(Damage.Lightning)** |
| 12 | `BP_S2GroundWarning` | `ADRS2GroundWarning` | `Content/Blueprints/Actor/Stage2/` |
| 13 | `BP_S2ElectricField` | `ADRS2ElectricField` | 〃 |
| 14 | `MI_S2MoleWarningDecal` | `M_ElectricFieldWarningDecal` 파생 | `Content/Blueprints/Actor/Area/Material/` |
| 15 | `BT_S2MoleBoss` | BehaviorTree | `Content/Blueprints/AI/BehaviorTree/` |
| 16 | `BTT_S2MoleBoss_Claw` | BTTask (BP) | `Content/Blueprints/AI/Tasks/` |
| 17 | `BTT_S2MoleBoss_BurrowStrike` | BTTask (BP) | 〃 |
| 18 | `BTS_S2MoleBoss_Target` | BTService (BP) | `Content/Blueprints/AI/Services/` — `BTS_FindNearestPlayer` 복제 후 수정 |

### 9.2 재사용 에셋 (신규 제작 불필요)

| 에셋 | 용도 |
|---|---|
| `Content/Blueprints/Actor/Area/Material/MI_ElectricFieldWarningDecal` | 경고 데칼 · 전기장 데칼 |
| `Content/Blueprints/Sound/SoundCue/Electric/SC_ElectricField` | 전기장 루프 |
| `Content/Blueprints/Sound/SoundCue/Electric/SC_ElectricWarning` | 경고 사운드 |
| `BP_PosionGas`의 `ActiveNiagaraSystem` | 전기장 본체 VFX |
| `Content/Blueprints/AI/BehaviorTree/BB_EnemyBlackboard` | 블랙보드 그대로 사용 |
| `Content/Blueprints/AI/AIController/BP_DRAIController` | AI 컨트롤러 그대로 사용 |

### 9.3 데이터 테이블 / 데이터 에셋 수정

| 에셋 | 수정 |
|---|---|
| `DA_EnemyCharacterClassInfo` | `MoleBoss` 행 추가 (PrimaryAttributes / VitalAttributes / StartupAbilities 3종) |
| `DT_S2PhaseObjective` | 변경 없음 (`S2P5_Boss` 행은 Plan6 §15.9-H-1에서 이미 예정) |
| `BP_S2TrainPhase` | `MoleBossClass` 를 임시 `BP_ElteBear` → **`BP_S2MoleBoss`** 로 교체 |

### 9.4 BehaviorTree 구조

```
BT_S2MoleBoss  (BB_EnemyBlackboard)
└─ Selector
   ├─ [Service] BTS_S2MoleBoss_Target        · 최근접 생존 플레이어 → TargetToFollow
   │                                          · AIController->SetFocus(Target)   ★회전의 근원
   │                                          · Cooldown.MoleBoss.BurrowStrike 보유 여부 → bSkillReady
   │
   ├─ Sequence  [Decorator: Blackboard "TargetToFollow" IsSet]
   │  ├─ Selector
   │  │  ├─ Sequence  [Decorator: bSkillReady == true]
   │  │  │  └─ BTT_S2MoleBoss_BurrowStrike    · TryActivateAbilitiesByTag(Abilities.MoleBoss.BurrowStrike)
   │  │  │                                     · 어빌리티 종료까지 InProgress 유지
   │  │  └─ Sequence  [Decorator: Distance(XY) <= 200]
   │  │     └─ BTT_S2MoleBoss_Claw            · TryActivateAbilitiesByTag(Abilities.MoleBoss.Claw)
   │  └─ Wait 0.2
   │
   └─ Wait 0.5                                 · 타깃 없음 (제자리 대기)
```

- **`MoveTo` 노드가 하나도 없다** — 이것이 "위치를 이동하지 않는다"의 최종 보증.
- 회전은 BT가 아니라 **`SetFocus` → ControlRotation → `ADREnemy::Tick`** 경로다. BTT_RotateToFaceTarget 은 쓰지 않는다.
- `[Decorator: State.MoleBoss.Burrowed 없음]`을 Claw 시퀀스에 걸어 잠수 중 기본공격이 끼어들지 않게 한다(GA `ActivationBlockedTags`와 이중 안전).

---

## 10. 마일스톤 & 검증

| 단계 | 규모 | 내용 | 검증 기준 (전부 2인 PIE 리슨서버) |
|---|---|---|---|
| **B1** 골격 | S | `ADRS2MoleBoss` + BP + `DA_EnemyCharacterClassInfo` 행 + `MOVE_None` + 반매몰 | 레벨에 배치 시 **상반신만 노출** · 밀어도 안 밀림 · 총 맞으면 체력 감소 · **클라에서도 같은 위치/자세** |
| **B2** 회전 | S | `RotationInterpSpeed` UPROPERTY + BT/Service + `SetFocus` | 플레이어가 원을 그리며 돌 때 **부드럽게 추적 회전** · 이동 0 · **클라에서 회전이 끊기지 않음** |
| **B3** 기본공격 | M | `UDRS2MoleClawAttack` + 몽타주 2 notify + BTT | 2m 안 → 2타 피격 · **2m 밖 무피격** · 등 뒤(120° 밖) 무피격 · 1타 후 뒤로 빠지면 2타 회피 · **원격 클라에서 모션 보임** |
| **B4** 구간 스케일 | M | `SegmentDamages` + `CooldownPerSegment` + `GetBaseCooldownDuration` 훅 + `UDRS2TrainPhase` 주입 ②③ | **발톱 데미지가 구간1/2/3에서 다름** · **스킬1 쿨다운이 10/8/6초로 다름**(로그 또는 `WaitCooldownChange` 확인) · 구간 전환 직후 첫 발동부터 새 값 적용 · **기존 플레이어 스킬 쿨다운 회귀 없음**(파이어볼트·제트점프로 확인) ★ |
| **B5** 스킬1 | **L** | `UDRS2MoleBurrowStrike` + `ADRS2GroundWarning` + 무적 + `EruptDamagePerPlayerCount` | 잠수 중 **어떤 공격도 안 들어감** · 경고 0.8s 표시 · 0.4s 후 융기 · **경고 원과 실제 피격 범위 일치** · 경고 밖으로 걸어 나가면 회피 성공 · 범위 안 플레이어 **공중에 뜸** · **1인 PIE 와 2인 PIE 에서 융기 데미지가 다름**(40 vs 46) ★ · **구간이 바뀌어도 융기 데미지는 안 바뀜** ★ |
| **B6** 아군 오사 | S | 융기 AoE 에 타 적 포함 | 두더지 근처에 잡몹 스폰 후 융기 → **잡몹도 피해+띄워짐** |
| **B7** 취소 안전성 | **M** | `NotifyStashed()` 배선 | 잠수 중 임계 도달 → 보관 → **유령 융기 없음** · 잠수 중 처치 → 정상 사망 · 잠수 중 페이즈 종료 → 경고 액터 잔존 없음 |
| **B8** 전기장 | **L** | `ADRS2ElectricField` + 링 버퍼 + 인원 스케일 | 1·2차 구간 **전기장 없음** · 3차 1번 융기 **없음** → 2번 융기에 P1 생성 → 3번 융기에 P1 소멸·P2 생성 · **동시에 2개 존재하지 않음** · **적은 안 아픔** · 1인/2인에서 틱 데미지가 다름 |
| **B9** 통합 | M | 3구간 완주 | 체력 100→67→34→0 이어짐 · 각 구간에서 스킬·전기장 사양대로 동작 · 최종 처치 즉시 스테이지 클리어 · **전기장 잔존 없음** |
| **B10** 폴리시 | M | 몽타주·VFX·사운드·보스 BGM·헬스바 위치 | 데모 품질 |

**의존 관계**

```
B1 ──▶ B2 ──▶ B3 ──▶ B4 ──┐
        │                  ├──▶ B9 ──▶ B10
        └──▶ B5 ──▶ B6 ────┤
             │             │
             └──▶ B7 ──────┤
                  │        │
                  └─▶ B8 ──┘
```

---

## 11. 리스크와 대응

| 리스크 | 심각도 | 대응 |
|---|---|---|
| **잠수 중 `StashBoss()` → 유령 융기** | **높음** | §5.1-③ `NotifyStashed()` → `CancelAbilities` → `EndAbility`에서 타이머 전량 해제. **B7에서 단독 검증** |
| **경고 원과 실제 피격 범위 불일치** | 높음 | 반경 산출을 `ADRS2MoleBoss::GetEruptRadius()` **단 하나의 함수**로 통일. 경고 액터·AoE 양쪽이 이 값만 쓴다. 데칼 크기 = `FVector(300, R, R)` |
| **캡슐 반지름 AoE 가 너무 작아 스킬이 안 맞음** | 중 | 사용자 확정 사양이므로 기본 1.0 유지. `EruptRadiusScale` 로 즉시 조정 가능. 플레이테스트에서 1.5~2.0 필요 가능성 있음 (§12-2) |
| **전원 착석 시 타깃 후보 0명 → 보스 영구 잠수** | 중 | 후보 0명이면 쿨다운 없이 즉시 취소 후 재시도(§6.3). 추가로 `bExcludeSeatedPlayers = false` 로 전환 가능 |
| **원격 클라에서 몽타주 미재생** | 중 | Armadillo 선례(`MulticastPlayFormChangeMontage`) 그대로 병행. B3/B5 검증 항목 |
| **화상 DoT 가 무적을 뚫음** | 중 | `bClearDebuffsOnBurrow = true` (§6.5) |
| **전기장 GE 가 사망/디스커넥트 시 누수** | 중 | `EndPlay` 일괄 `RemoveActiveGameplayEffect`. `TWeakObjectPtr` 키로 널 액터 자동 스킵 |
| **`DRGameplayAbility::ApplyCooldown` 수정이 전 어빌리티에 영향** | 중 | 기본 구현이 `return CooldownDuration;` 이라 동작 동일. 그래도 **공통 경로**이므로 B4에 스테이지1 플레이어 스킬(파이어볼트·제트점프·대시) **쿨다운 회귀 검증**을 명시적으로 넣었다 |
| **`GE_Cooldown_MoleBoss_BurrowStrike` 를 고정 Duration 으로 만들어 구간별 쿨다운이 무시됨** | 중 | GE Duration 을 `SetByCaller(Data.Cooldown)` 로 만들지 않으면 부모가 **Warning 로그를 남긴다**(`DRGameplayAbility.cpp:189-191`). 에셋 체크리스트 #9에 ★ 표기 |
| **`ECharacterClass` enum 추가로 기존 데이터 깨짐** | 낮 | **맨 뒤에 추가**하므로 기존 항목의 정수값 불변. `DA_EnemyCharacterClassInfo`는 TMap 키라 안전 |
| **`DREnemy.h` 수정 시 모지바케 확산** | 낮 | 수정 라인 외 무수정 원칙(Plan6 §15.0). 추가 2줄은 새 UTF-8 라인 |
| **아트 에셋 미제작** | 낮(논블로킹) | Plan6 §9.3 트랙 A 그대로 — B1~B9는 임시 프리미티브(원뿔/구)로 진행, B10에서 교체 |
| **`MOVE_None` 이 BT/NavMesh 와 충돌** | 낮 | BT에 `MoveTo`가 없다. NavMesh는 융기 착지점 투영에만 쓰고 이동에는 안 쓴다 |

---

## 12. 잔여 확인 항목 (전부 기본 진행값 지정 — 논블로킹)

| # | 항목 | 기본 진행값 |
|---|---|---|
| 1 | 경고 0.8s **후** 0.4s 인가, 0.8s **안에** 포함인가 | **후** (총 1.2s). `EruptDelayAfterWarning = 0` 으로 즉시 전환 가능 |
| 2 | 융기 AoE 반경 = 캡슐 반지름 그대로면 너무 작지 않은가 | 캡슐 반지름 90 가정, `EruptRadiusScale = 1.0` 유지. 테스트 후 조정 |
| 3 | 3차 진입 후 **첫 융기**에도 전기장이 생기는가 | **아니오** — 구간 전환 시 `bHasPreviousErupt = false`. 사용자 예시("2번째 솟아오를 때")와 일치 |
| 4 | 경고창이 타깃을 추종하는가 | **고정** (지정 순간 위치) |
| 5 | 전기장 데미지 인원수 = 방6 시작 시점 생존자 수인가, 실시간인가 | **방6 시작 시점 1회 확정** (프로젝트 규약) |
| 6 | 보스 체력이 인원수에 따라 달라지는가 | **아니오** (고정 6000). §8.1 검산 표가 근거 |
| 7 | 융기 데미지·쿨다운의 스케일 축 | **확정(§2.5-C)** — 데미지는 인원별(40/46/52/58), 쿨다운은 구간별(10/8/6s) |
| 7b | **기본공격 간격**도 구간별로 달라지는가 | **아니오** — 발톱은 데미지만 구간별. 간격은 BT `Wait 1.0` 고정. 필요해지면 `CooldownPerSegment` 와 같은 패턴을 Claw GA 에 복제 |
| 7c | 전기장 데미지가 **구간별**로도 달라지는가 | **아니오** — 3차 전용이라 구간 축이 무의미 |
| 8 | 융기가 **다른 적도 띄우는가**, 데미지만 주는가 | **둘 다** (사용자 명시: "데미지를 입히고 공중에 띄워") |
| 9 | 전기장 반경 | **융기 AoE 반경과 동일**. 대안: 스테이지1 기본 312.5 |
| 10 | 전기장이 **보스 도망(1·2차)** 시에도 남는가 | 3차에만 생기므로 무의미. 안전상 `NotifyStashed()`에서 항상 제거 |
| 11 | 잠수 중 디버프(화상) 제거 여부 | **제거** (`bClearDebuffsOnBurrow = true`) |
| 12 | 보스 전용 체력바 UI (화면 상단 보스 바) | **미제작** — 기존 머리 위 `HealthBar` 사용. 필요 시 별도 작업 |
| 13 | 보스 BGM 전환 | Plan6 §14.6.8-10 그대로 `DRPhase3.cpp:1314-1328` 패턴 재사용 |
| 14 | 두더지가 잡몹을 죽였을 때 플레이어 물 지급 | **허용** (§14.6.8-6 "보스 단독"이라 거의 미발생) |
| 15 | 상반신 전용 히트박스(`"Hitbox"` 태그) 배치 여부 | **배치 권장** — 캡슐만 쓰면 지면 아래 절반이 판정에서 낭비됨 |
| 16 | 발톱 공격에 부위(왼발/오른발) 구분 소켓 필요 여부 | 불필요 (부채꼴 판정이 액터 기준) |

---

## 13. 신규/수정 파일 체크리스트 (작업 순서)

| # | 파일 | 종류 | 단계 |
|---|---|---|---|
| 1 | `Public/Character/Stage2/DRS2MoleBoss.h` | ★신규 | B1 |
| 2 | `Private/Character/Stage2/DRS2MoleBoss.cpp` | ★신규 | B1 |
| 3 | `Public/AbilitySystem/Data/CharacterClassInfo.h` | ○수정 (enum 1줄) | B1 |
| 4 | `Public/Character/DREnemy.h` | ○수정 (UPROPERTY 1개) | B2 |
| 5 | `Private/Character/DREnemy.cpp` | ○수정 (리터럴 1개) | B2 |
| 6 | `Public/DRGameplayTags.h` | ○수정 (태그 5개, 추가만) | B3 |
| 7 | `Private/DRGameplayTags.cpp` | ○수정 (등록 5블록, 추가만) | B3 |
| 8 | `Public/AbilitySystem/Abilities/Stage2/DRS2MoleClawAttack.h` | ★신규 | B3 |
| 9 | `Private/AbilitySystem/Abilities/Stage2/DRS2MoleClawAttack.cpp` | ★신규 | B3 |
| 10 | `Public/Phase/Stage2/DRS2TrainPhase.h` | ○수정 (전방선언 1줄) | B4 |
| 11 | `Private/Phase/Stage2/DRS2TrainPhase.cpp` | ○수정 (4곳) | B4 · B7 |
| 11a | `Public/AbilitySystem/Abilities/DRGameplayAbility.h` | ○수정 (virtual 훅 1개) | B4 |
| 11b | `Private/AbilitySystem/Abilities/DRGameplayAbility.cpp` | ○수정 (`ApplyCooldown` 3줄) | B4 |
| 12 | `Public/Actor/Stage2/DRS2GroundWarning.h` | ★신규 | B5 |
| 13 | `Private/Actor/Stage2/DRS2GroundWarning.cpp` | ★신규 | B5 |
| 14 | `Public/AbilitySystem/Abilities/Stage2/DRS2MoleBurrowStrike.h` | ★신규 | B5 |
| 15 | `Private/AbilitySystem/Abilities/Stage2/DRS2MoleBurrowStrike.cpp` | ★신규 | B5 |
| 16 | `Public/Actor/Stage2/DRS2ElectricField.h` | ★신규 | B8 |
| 17 | `Private/Actor/Stage2/DRS2ElectricField.cpp` | ★신규 | B8 |

**신규 C++ 10개 파일(5쌍) · 기존 수정 9개 파일 — 전부 완료 (2026-08-11, 빌드 통과). 상세는 §15.**

> 기존 수정 9개 중 **7개는 "추가만" 또는 "기본값 동일"** 이라 회귀 위험이 없다. 실제 로직이 바뀌는 것은 `DRS2TrainPhase.cpp`(4곳, 전부 `Cast<>` 널 가드) 하나뿐이다.
> 단 `DRGameplayAbility.cpp`는 **모든 어빌리티의 공통 경로**라 기본값 동일성에도 불구하고 B4에서 **스테이지1 플레이어 스킬 쿨다운 회귀 검증**을 반드시 넣는다.

---

## 14. Plan6 문서 갱신 필요 항목

이 계획이 완료되면 Plan6의 다음 항목이 해소된다. 해당 절에 "→ Plan7" 링크를 남긴다.

| Plan6 위치 | 현재 서술 | 갱신 |
|---|---|---|
| §14.6.7 | "보스의 스킬·공격 패턴은 이번 범위 밖" | **Plan7에서 확정** |
| §14.6.8-4 | "보스 최대 체력·스킬·공격 패턴 = 별도 작업, 임시 `EliteBear` 스탯" | Plan7 §8 |
| §14.6.8-5 | "도망/재등장 연출 = BP 훅" | Plan7 §4.1 `OnBurrowVisual` / `OnEruptVisual` 로 통합 |
| §9.3 트랙 B | "두더지 보스 스킬·패턴 (논블로킹)" | Plan7 B1~B10 |
| §11.B-15 | "두더지 보스의 스킬·공격 패턴·스탯" 미정 | 해소 |
| §15.9-H-7 | "`MoleBossClass`에 임시로 EliteBear 계열 지정" | **`BP_S2MoleBoss` 로 교체** |
| §9.2 M7 | 폴리시 대상 목록 | 두더지 보스 VFX/사운드 추가 |

---

## 15. 구현 결과 — C++ 전량 완료 (2026-08-11) ★

**`DaeRuneEditor Win64 Development` 빌드 통과.** 아래 19개 파일이 계획대로 반영되었다.

### 15.1 생성/수정 결과

| 구분 | 파일 | 상태 |
|---|---|---|
| ★신규 | `Public\|Private/Character/Stage2/DRS2MoleBoss.h/.cpp` | 완료 |
| ★신규 | `Public\|Private/AbilitySystem/Abilities/Stage2/DRS2MoleClawAttack.h/.cpp` | 완료 |
| ★신규 | `Public\|Private/AbilitySystem/Abilities/Stage2/DRS2MoleBurrowStrike.h/.cpp` | 완료 |
| ★신규 | `Public\|Private/Actor/Stage2/DRS2GroundWarning.h/.cpp` | 완료 |
| ★신규 | `Public\|Private/Actor/Stage2/DRS2ElectricField.h/.cpp` | 완료 |
| ○수정 | `DRGameplayTags.h/.cpp` | 태그 4개 추가 (기존 라인 무수정) |
| ○수정 | `CharacterClassInfo.h` | `ECharacterClass::MoleBoss` 맨 뒤 추가 |
| ○수정 | `DREnemy.h/.cpp` | `RotationInterpSpeed` UPROPERTY 화 (기본값 10.0 유지) |
| ○수정 | `DRGameplayAbility.h/.cpp` | `GetBaseCooldownDuration(ActorInfo)` virtual 훅 |
| ○수정 | `DRS2TrainPhase.h/.cpp` | 4곳 배선 (인원 확정 / 구간·인원 주입 / 보관 통지 / 페이즈 종료 정리) |

> 계획(§5.5)의 `State.MoleBoss.Burrowed` 를 포함해 태그 4개를 등록했다. `Debuff.All` 은 만들지 않았다 — 부모 태그 등록 여부에 의존하지 않도록 5개 디버프 태그를 코드에서 직접 조립한다.

### 15.2 구현 중 확정한 사항 (계획서와 다른 지점)

| # | 지점 | 계획 | 구현 | 이유 |
|---|---|---|---|---|
| 1 | **쿨다운 시작 시점** | §8.3 이 "쿨다운 + 시전"을 실효 주기로 표기 | **활성 시점에 `CommitAbility`** → 실효 주기 = 쿨다운 그 자체 | GAS 표준. 쿨다운(6~10s)이 시전(1.8s)보다 길어 결과가 동일하고, 설계 의도("6초에 한 번")가 값에 그대로 드러난다 |
| 2 | **타깃 후보 0명 처리** | "쿨다운 없이 취소" | **활성 직전에 후보 유무를 먼저 검사**하고, 없으면 `CommitAbility` 전에 취소 | 커밋 후 취소하면 쿨다운이 이미 소모된다. 잠수 후 재선정에 실패한 드문 경우만 쿨다운을 잃는다 |
| 3 | **잠수 진입 트리거** | "잠수 몽타주 끝 AnimNotify" | **C++ 타이머(`BurrowDuration`)가 주** + AnimNotify 도 호출 가능(래치로 중복 무시) | 0.8/0.4초 타이밍이 사양이라 몽타주 배선 실수에 좌우되면 안 된다 |
| 4 | **경고/전기장 활성 타이밍** | 명시 없음 | **`BeginPlay` 이후로 지연** (`bPendingActivation`) | `SpawnActorDeferred` 경로에서 `InitField`/`InitWarning` 이 `BeginPlay` 보다 먼저 불린다. 그 시점엔 오버랩 델리게이트가 미바인딩이고 BP 타임라인도 못 돈다 |
| 5 | **융기 시 ControlRotation** | 명시 없음 | `EruptAt` 이 **ControlRotation 도 함께 설정** | 부모 `ADREnemy::Tick` 이 ControlRotation 을 추종하므로, 안 맞추면 융기 직후 이전 방향으로 홱 돌아간다 |
| 6 | **`EmergeRecoverDuration`** | 없음 | 신규 프로퍼티(0.8s) | 융기 몽타주가 도는 동안 GA 를 살려 둬 재발동을 막는다 |
| 7 | 지역 변수명 | `Damage` / `Tags` | `HitDamage` / `EruptDamage` / `DRTags` | 프로젝트가 `-WarningsAsErrors` 라 부모 멤버 섀도잉(C4458)이 컴파일 에러다 |

### 15.3 계획서에 없던 방어 코드

- `ADRS2MoleBoss::Die()` 에서 **전기장 파괴 + 굴착 취소 + 잠수 해제**를 수행한다. 잠수 중 즉사(과도한 폭딜)해도 숨겨진 시체가 남지 않는다.
- `UDRS2MoleBurrowStrike::EndAbility()` 가 **타이머 5개 전량 해제 + 경고 액터 파괴 + 잠수 강제 해제**를 한다. 취소·정상 종료·사망 어느 경로든 같은 함수를 지난다.
- `UDRS2TrainPhase::OnPhaseEnd()` 에서도 `NotifyStashed()` 를 불러 **플레이어에게 걸린 전기장 주기 GE 까지** 회수한다.
- `ElectricFieldClass` / `WarningActorClass` / `FieldDamageEffectClass` 미설정 시 **Error 로그**를 남긴다 (조용한 실패 방지).

---

### 15.4 커브 테이블 전환 (2026-08-12) ★2차 개정

초안은 인원별·구간별 수치를 **`TArray` 하드코딩**으로 들고 있었다. `CT_EnemyAttributes` 에 이미 `Mole.MaxHealth` 행이 **인원수 축(레벨 1~4)** 으로 작성되어 있는 것을 확인하고, 나머지 수치도 커브 테이블로 통일했다.

#### 전환 판단 근거

| 근거 | 내용 |
|---|---|
| **프로젝트 일관성** | `CT_Damage` 가 이미 `Abilities.Bear.Sweep` 처럼 어빌리티 태그로 키잉되어 있고 모든 적이 거기 있다. 두더지만 배열이면 밸런싱 때 두 군데를 봐야 한다 |
| **인원 축이 이미 커브** | `Mole.MaxHealth` 가 인원 축으로 작성된 이상, 융기 데미지만 배열이면 같은 축의 값 두 개가 다른 곳에 흩어진다 |
| **전환 비용 0** | BP 에 아직 값을 넣지 않은 시점이라 마이그레이션할 데이터가 없다 |

#### ★"레벨 = 인원수"는 절반만 쓴다

`Level` 을 인원수로 쓰면 인원 축은 엔진이 공짜로 처리한다. 그러나 **구간 축은 실을 자리가 없다** — 레벨은 스칼라 하나인데 축이 두 개고, 융기 어빌리티는 자기 안에서 데미지=인원 / 쿨다운=구간이라 어빌리티별로 스펙 레벨을 나눠도 해결되지 않는다.

**해법**: `FScalableFloat::GetValueAtLevel(float Level)` 의 인자는 **어빌리티 레벨일 의무가 없다.** 구간 축 값은 `GetValueAtLevel(Segment + 1)` 로 직접 조회한다 → 커브 테이블의 이점을 얻으면서 레벨 충돌이 없다.

#### 변경 내역

| 대상 | 초안 | 개정 |
|---|---|---|
| `UDRS2MoleClawAttack` | `TArray<FDRMoleSegmentDamage> SegmentDamages` + `FDRMoleSegmentDamage` USTRUCT | `FScalableFloat Hit1Damage` / `Hit2Damage` — USTRUCT **삭제** |
| `UDRS2MoleBurrowStrike` | `TArray<float> EruptDamagePerPlayerCount` | **삭제** — 상속받은 `Damage` 를 어빌리티 레벨(=인원)로 조회. `Params.BaseDamage` 덮어쓰기도 제거 |
| `UDRS2MoleBurrowStrike` | `TArray<float> CooldownPerSegment` | `FScalableFloat BurrowCooldown` (`GetValueAtLevel(Segment+1)`) |
| `ADRS2MoleBoss` | `TArray<float> FieldDamagePerPlayerCount` | **삭제** — 전기장 수치는 GE 의 Modifier 커브가 갖는다 (아래 15.5) |
| `ADRS2MoleBoss` | `int32 BasePlayerCount` 필드 + `SetBasePlayerCount()` | **삭제** — `GetBasePlayerCount()` 이 `Level` 을 직접 읽는다 (단일 진실원) |
| `UDRS2PhaseBase::SpawnEnemyAt` | 인자 2개 | `int32 EnemyLevel = INDEX_NONE` 추가 |

#### ★타이밍 버그를 함께 고쳤다

`ADREnemy::BeginPlay()` 안에서 `InitializeDefaultAttributes(Level)` 와 `GiveStartupAbilities(Level)` 가 **모두 끝난다.** 그런데 초안의 페이즈 코드는 평범한 `SpawnActor` 로 스폰한 뒤 값을 주입했다 — **BeginPlay 가 이미 다 돈 뒤**라 레벨이 반영되지 않는다.

증상이 "커브를 아무리 고쳐도 항상 1인 수치"여서 알아채기 어렵다. `SpawnEnemyAt` 에 `EnemyLevel > 0` 경로를 추가해 **`SpawnActorDeferred` → `SetLevel` → `FinishSpawning`** 순서로 처리한다. `EnemyLevel` 을 넘기지 않는 기존 호출부(방1/방3/방5)는 예전 경로를 그대로 탄다.

#### `Value = 0` 함정 방어

`FScalableFloat` 의 결과는 `Value × Curve[Level]` 이고 기본 생성자의 `Value` 는 **0** 이다. 커브만 걸고 `Value` 를 1로 안 바꾸면 조용히 전부 0이 된다.

- C++ 필드 3개(`Hit1Damage` / `Hit2Damage` / `BurrowCooldown`) 전부 생성자에서 `FScalableFloat(1.f)` 로 초기화
- 두 GA 의 `OnGiveAbility` 에서 1회 검사
  - 커브 있음 + `Value ≈ 0` → **Error**
  - 커브 없음 → **Warning** (상수로 동작한다고 알림)
- 보스 스폰 시 `Level` 과 실제 적용된 `MaxHealth` 를 Log 로 남겨 커브 반영 여부를 즉시 확인할 수 있게 했다
- ※ GE 안의 ScalableFloat(체력·전기장)은 C++ 에서 검사할 수 없다 — 에디터에서 **Value = 1** 인지 직접 확인해야 한다

---

### 15.5 전기장을 Modifier 방식으로 되돌림 (2026-08-12) ★3차 개정

초안은 전기장 GE 도 `ExecCalc_Damage` + SetByCaller 로 잡았다. 이유는 "데미지는 ExecCalc 를 통과시킨다"는 관례였는데, **`GE_PoisonDamage` 를 열어 보니 그 관례에 반례가 있었다.**

```
GE_PoisonDamage  →  Modifier: DRAttributeSet.IncomingDamage, ScalableFloat, Period   ← 지역 위험물
GE_Damage        →  Executions: ExecCalc_Damage                                       ← 어빌리티
```

**핵심은 둘 다 `IncomingDamage` 에 도달한다는 것이다.** 내가 우려했던 "파이프라인 우회"는 `Health` 를 직접 깎을 때의 문제지, `IncomingDamage` 를 쓰면 `HandleIncomingDamage` 가 컨테이너 체력·부식·업그레이드 칩·전투 상태·피격 큐를 전부 처리한다.

| | ExecCalc + SetByCaller (초안) | **Modifier + ScalableFloat (채택)** |
|---|---|---|
| 보스 쪽 커브 필드 | `FieldTickDamage` 필요 | **불필요** |
| C++ 데미지 계산 | `GetValueAtLevel()` 호출 | **없음** — 스펙 레벨만 전달 |
| 커브 참조 위치 | 보스 BP | **GE 안** (디자이너가 먼저 보는 곳) |
| 디버프 차단 | `Debuff.Chance` 를 안 넣어서 우회 | **타입 태그 자체가 없어 원천 차단** |
| 선례 | 없음 | **`GE_PoisonDamage`** 동일 구조 |

**변경**: `ADRS2ElectricField::InitField(Instigator, Radius, **int32 PlayerCount**)` — 인원수를 GE 스펙 레벨로 받는다. `TickDamage` 멤버와 `AssignTagSetByCallerMagnitude` 호출을 제거했고, 보스의 `FieldTickDamage` 필드도 삭제했다.

**어빌리티 쪽(발톱·융기)은 그대로 ExecCalc 를 쓴다** — 넉백 벡터와 사망 임펄스를 컨텍스트에 실어야 하기 때문이다. 두 방식이 섞인 게 아니라 **어빌리티 / 지역 위험물이라는 카테고리별 관례**를 각각 따른 것이다.

---

### 15.6 잠수·융기 몽타주를 C++ 소유로 (2026-08-12) ★4차 개정

초안은 몽타주 재생을 BP(`K2_OnBurrowMontage` / `K2_OnEruptMontage`)에 맡겼다. 검토해 보니 **연출 흐름은 잡혀 있었지만 세 가지가 보장되지 않았다.**

| # | 문제 | 증상 |
|---|---|---|
| ① | `BurrowDuration`(C++ 상수)와 잠수 몽타주 길이(에셋)가 **이원화** | 몽타주가 1.2초인데 상수가 0.6이면 **파고드는 중간에 그대로 사라진다** |
| ② | `EruptAt` 이 **언하이드를 먼저** 하고 몽타주는 그 뒤에 BP 가 재생 | 새 위치에 기본 포즈로 한 프레임 튄 뒤 몽타주가 땅속으로 끌어내린다. 클라에서는 언하이드(프로퍼티 복제)와 몽타주(`RepAnimMontageInfo`)가 **다른 경로**라 틈이 더 벌어진다 |
| ③ | GA 가 재생한 몽타주가 **원격 클라에 도달하지 않을 수 있음** | 원격 플레이어에게는 파고드는 동작도 솟는 동작도 없이 **그냥 사라졌다 나타난다** — 연출이 통째로 소실 |

③은 추측이 아니라 이 프로젝트의 기록된 선례다 (`DRArmadilloEnemy.h:128-151` 의 `MulticastPlayFormChangeMontage` 주석).

#### 해결 — 몽타주를 `ADRS2MoleBoss` 가 소유한다

```cpp
UPROPERTY(EditDefaultsOnly) TObjectPtr<UAnimMontage> BurrowMontage;
UPROPERTY(EditDefaultsOnly) TObjectPtr<UAnimMontage> EmergeMontage;

float PlayBurrowMontage();            // 재생 + 멀티캐스트, ★길이를 돌려준다
float GetEmergeMontageLength() const;

UFUNCTION(NetMulticast, Reliable) void Multicast_PlayMoleMontage(UAnimMontage*);
```

| 문제 | 해결 |
|---|---|
| ① | GA 가 `PlayBurrowMontage()` 의 **반환 길이를 그대로 타이머로 쓴다** → 어긋날 수가 없다. `BurrowDuration` / `EmergeRecoverDuration` 은 **몽타주 미지정 시 폴백**으로 격하 |
| ② | `EruptAt` 순서를 **텔레포트 → 몽타주 → 언하이드**로 고정. 숨겨진 메시도 포즈는 계속 틱하므로(`SkeletalMeshComponent` 기본값 `AlwaysTickPoseAndRefreshBones` — 엔진 소스 확인) 첫 프레임(땅속 포즈)이 적용된 뒤 보이기 시작한다 |
| ③ | `Multicast_PlayMoleMontage` 를 **처음부터 내장**. 서버는 로컬 재생 후 RPC 내부에서 `HasAuthority()` 로 걸러 이중 재생을 막는다 |

#### BP 역할 변경

| | 초안 | 개정 |
|---|---|---|
| 잠수 몽타주 | GA BP 의 `Play Montage and Wait` | **C++ 자동** |
| 융기 몽타주 | GA BP 의 `Play Montage and Wait` | **C++ 자동** |
| 잠수 FX (흙먼지·사운드) | 〃 | `K2_OnBurrowStarted()` (GA, t=0) |
| 융기 FX | 〃 | `OnEruptVisual(Location)` (보스, 융기 순간) |

**에디터 작업이 오히려 줄었다** — GA BP 에서 몽타주 배선이 사라지고, `BP_S2MoleBoss` 에 몽타주 에셋 2개만 지정하면 된다.

#### 애니메이션 저작 요건 ★

| 몽타주 | 요건 |
|---|---|
| `AM_MoleBoss_Burrow` | **끝 포즈 = 완전히 땅속.** 이 시점에 메시가 숨겨진다 |
| `AM_MoleBoss_Emerge` | ★**첫 프레임 = 완전히 땅속.** 가시화가 이 프레임에서 시작하므로, 첫 프레임이 지상 포즈면 튀어 보인다 |
| 공통 | 두더지의 **평상시(Idle)는 반매몰**이다. 잠수는 "반매몰 → 완전 매몰", 융기는 "완전 매몰 → 반매몰"이다. 지상에 완전히 선 포즈를 기준으로 만들면 안 된다 |

---

## 16. 에디터 작업 — 상세 절차 ★

> **전제**: C++ 은 전부 끝났고 빌드가 통과한 상태다. 아래는 **에디터에서만** 할 수 있는 작업이다.
> **원칙**: E1 → E10 **순서대로** 진행한다. 뒤 단계가 앞 단계의 에셋을 참조하므로 순서를 바꾸면 참조를 비워 둔 채 저장하게 된다.
> **임시 에셋 허용**: 아트가 아직 없다(Plan6 §9.3 트랙 A). **E1~E9 는 전부 엔진 기본 프리미티브(Cylinder/Cube/Sphere)로 진행**하고 M7 에서 교체한다.

### 16.0 시작 전 확인 (2분)

| # | 확인 | 방법 |
|---|---|---|
| 1 | C++ 클래스가 에디터에 보이는가 | 콘텐츠 브라우저 → 설정 → **"C++ 클래스 표시"** 체크 → `DaeRune/Character/Stage2/` 에 `DRS2MoleBoss` 가 보여야 한다 |
| 2 | 신규 태그가 등록되었는가 | 프로젝트 설정 → GameplayTags → **`Abilities.MoleBoss.Claw` / `.BurrowStrike` / `Cooldown.MoleBoss.BurrowStrike` / `State.MoleBoss.Burrowed`** 4개가 목록에 있어야 한다. 없으면 에디터를 재시작한다(네이티브 태그는 모듈 로드 시 등록된다) |
| 3 | `ECharacterClass` 에 MoleBoss 가 있는가 | `DA_EnemyCharacterClassInfo` 를 열어 CharacterClassInformation 맵의 키 드롭다운에 **MoleBoss** 가 있어야 한다 |

**폴더를 먼저 만든다**

```
Content/Blueprints/Character/Enemy/MoleBoss/
Content/Blueprints/Character/Enemy/MoleBoss/Montage/
Content/Blueprints/AbilitySystem/Enemy/MoleBoss/Abilities/
Content/Blueprints/AbilitySystem/Enemy/MoleBoss/Effects/
Content/Blueprints/Actor/Stage2/Field/
```

---

### 16.1 [E1] GameplayEffect 4종

#### E1-0. ★`CT_Damage` 에 커브 행 5개 추가 — **가장 먼저 한다**

`Content/Blueprints/AbilitySystem/Data/CT_Damage` 를 열고 아래 행을 추가한다. 기존 `Abilities.Bear.Sweep` 행을 복사해 이름만 바꾸는 게 빠르다.

| 행 이름 | 축 | Lv1 | Lv2 | Lv3 | Lv4 |
|---|---|---|---|---|---|
| `Abilities.MoleBoss.Claw.Hit1` | 구간 | 20 | 26 | 32 | — |
| `Abilities.MoleBoss.Claw.Hit2` | 구간 | 30 | 39 | 48 | — |
| `Cooldown.MoleBoss.BurrowStrike` | 구간 | 10 | 8 | 6 | — |
| `Abilities.MoleBoss.BurrowStrike` | 인원 | 40 | 46 | 52 | 58 |
| `Abilities.MoleBoss.ElectricField` | 인원 | 6 | 8 | 10 | 12 |

- 모든 키의 **Interp Mode = Constant**, **Pre/Post Infinity Extrap = Constant**
- `CT_EnemyAttributes / Mole.MaxHealth` 는 **이미 작성되어 있다** (레벨 1~4 = 인원수). 값만 §8.1 검산 박스를 참고해 재검토한다

#### E1-1. `GE_PrimaryAttributes_MoleBoss`
`Content/Blueprints/AbilitySystem/Enemy/MoleBoss/Effects/`

**가장 빠른 방법**: `GE_PrimaryAttributes_EliteEnemy` 를 **복제(Ctrl+D)** 해서 이름만 바꾼 뒤 값을 고친다.

| 항목 | 값 |
|---|---|
| Duration Policy | **Instant** |
| Modifier 1 | Attribute = `DRAttributeSet.MaxHealth`, Op = **Override**, Magnitude = **Scalable Float** → Curve Table `CT_EnemyAttributes`, Row **`Mole.MaxHealth`**, **Value = 1** ★ |
| Modifier 2 | Attribute = `DRAttributeSet.MaxWater`, Op = Override, Magnitude = Scalable Float, Value = **0** (커브 없음) |
| Modifier 3 | Attribute = `DRAttributeSet.MoveSpeed`, Op = Override, Magnitude = Scalable Float, Value = **0** (커브 없음) |

> ★MaxHealth 를 **고정 숫자가 아니라 커브 참조**로 둬야 인원수 스케일이 걸린다. 이 GE 는 `InitializeDefaultAttributes(..., Level, ...)` 가 **Level = 인원수**로 적용하므로, 커브가 자동으로 인원별 체력이 된다.
> `GE_PrimaryAttributes_EliteEnemy` 등 기존 적 GE 가 어떻게 되어 있는지 열어 보고 같은 형태로 맞추면 된다 (`CT_EnemyAttributes` 에 `Bear.MaxHealth` 등이 이미 있는 걸 보면 커브 참조 방식일 가능성이 높다).
> MoveSpeed 0 은 `MOVE_None` 이라 어차피 무의미하지만, 혹시 이동 봉인이 풀렸을 때 보스가 걸어 다니지 않게 하는 2차 안전장치다.

#### E1-2. `GE_Cooldown_MoleBoss_BurrowStrike` ★가장 실수하기 쉬운 에셋
같은 폴더. `GE_Cooldown_SweepAttack` 을 복제해서 시작한다.

| 항목 | 값 |
|---|---|
| Duration Policy | **Has Duration** |
| Duration Magnitude | ★**Set By Caller** |
| └ Data Tag | ★**`Data.Cooldown`** |
| GameplayEffectComponent 추가 | **Target Tags Gameplay Effect Component** |
| └ Add Granted Tags | **`Cooldown.MoleBoss.BurrowStrike`** |

> ★**Duration 을 "Scalable Float"(고정값)로 만들면 구간별 쿨다운 10/8/6초가 통째로 무시된다.**
> 반드시 Set By Caller + `Data.Cooldown` 이어야 `UDRGameplayAbility::ApplyCooldown` 이 주입한 값이 먹는다.
> 실수하면 출력 로그에 `[Upgrade] ...: 쿨다운 GE 가 SetByCaller Duration 인데...` 계열 경고가 뜨거나, 쿨다운이 GE 고정값으로 굳는다.

#### E1-3. `GE_S2ElectricFieldDamage` ★
`Content/Blueprints/Actor/Stage2/Field/`

**가장 빠른 방법**: `Content/Blueprints/Actor/Area/GE_PoisonDamage` 를 **복제**해서 커브 행만 바꾼다. 구조가 완전히 같다.

| 항목 | 값 |
|---|---|
| Duration Policy | **Infinite** |
| Period | **0.5** |
| Execute Periodic Effect on Application | **체크** (진입 즉시 1틱) |
| **Modifier 1 → Attribute** | ★**`DRAttributeSet.IncomingDamage`** |
| └ Modifier Op | **Add** |
| └ Magnitude Calculation Type | **Scalable Float** |
| └ Scalable Float → Curve Table | `CT_Damage` |
| └ Scalable Float → Row Name | ★**`Abilities.MoleBoss.ElectricField`** |
| └ Scalable Float → **Value** | ★**1** |
| Executions | **없음** |

> **`Health` 가 아니라 `IncomingDamage` 다.** `Health` 를 직접 깎으면 컨테이너 체력·부식·업그레이드 칩·피격 큐를 전부 우회한다. `IncomingDamage` 에 넣으면 `UDRPlayerAttributeSet::HandleIncomingDamage` 가 알아서 처리한다 — `GE_PoisonDamage` 가 이미 그렇게 되어 있다.
> **인원수 스케일이 붙는 원리**: `ADRS2ElectricField` 가 `MakeOutgoingSpec(GE, 인원수, Ctx)` 로 스펙을 만든다. Modifier 의 ScalableFloat 이 그 레벨로 커브를 읽으므로 **C++ 에는 숫자가 없다.**
> **스턴이 안 걸리는 이유**: `ExecCalc_Damage` 를 쓰지 않아 컨텍스트에 데미지 타입 태그 자체가 없다. 디버프 판정에 진입할 수 없다.
> ★**Executions 에 `ExecCalc_Damage` 를 추가하지 말 것.** 추가하면 SetByCaller 값이 없어 데미지가 0이 되고, 디버프 경로도 열린다.

#### E1-4. 데미지 GE — 신규 제작 불필요
발톱·융기가 쓸 `DamageEffectClass` 는 **기존 `Content/Blueprints/AbilitySystem/Player/Effects/GE_Damage`** 를 그대로 쓴다. `ExecCalc_Damage` 가 타깃 어트리뷰트셋 종류를 보고 올바른 `IncomingDamage` 를 고르므로 플레이어/적 공용이다.

---

### 16.2 [E2] `BP_S2ElectricField`

부모 클래스 **`DRS2ElectricField`** → `Content/Blueprints/Actor/Stage2/Field/`

| 컴포넌트 | 설정 |
|---|---|
| `GroundDecal` | 자동 생성됨. 크기는 C++ 이 `FieldRadius` 로 덮어쓰므로 건드리지 않는다 |
| `FieldFX` | 자동 생성됨. 에셋은 아래 클래스 디폴트에서 지정 |
| `LoopAudio` | 자동 생성됨 |
| `EffectSphere` | **반경을 손대지 않는다** — `InitField` 가 설정한다 |

**클래스 디폴트**

| 프로퍼티 | 값 |
|---|---|
| `Field Damage Effect Class` | ★**`GE_S2ElectricFieldDamage`** (E1-3) |
| `Field Decal Material` | `MI_ElectricFieldWarningDecal` (스테이지1 재사용) |
| `Field Niagara System` | `BP_PosionGas` 를 열어 `ActiveNiagaraSystem` 에 들어 있는 에셋을 그대로 지정 |
| `Field Loop Sound` | `SC_ElectricField` |
| `Default Radius` | 150 (실제로는 보스가 캡슐 반지름을 넘겨준다) |
| `Activation Delay` | **0** (융기 자체가 이미 텔레그래프라 별도 경고가 필요 없다) |
| `Decal Projection Depth` | 300 |

**BP 이벤트 그래프** — `Event On Field Activated(In Radius)` 를 구현해 링 스케일 조정/사운드 페이드인 등을 넣는다. 비워 둬도 동작한다.

---

### 16.3 [E3] `BP_S2GroundWarning`

부모 클래스 **`DRS2GroundWarning`** → `Content/Blueprints/Actor/Stage2/`

| 프로퍼티 | 값 |
|---|---|
| `Decal Material` | `MI_ElectricFieldWarningDecal` (또는 빨강 계열 파생 MI 신규 제작) |
| `Warning Niagara System` | (선택) 링 파티클 |
| `Decal Projection Depth` | 300 |

**BP 이벤트 그래프** — `Event On Warning Begin(In Radius, In Duration)`

권장 연출 (0.8초 안에 "곧 터진다"가 읽혀야 한다):
1. `SC_ElectricWarning` 재생 (`Play Sound at Location`)
2. **Timeline (Length = In Duration)** → Alpha 0→1
3. `Set Scalar Parameter Value on Materials` 로 데칼 MI 의 채움 비율/점멸 속도를 Alpha 로 구동

> ★**데칼 크기를 BP 에서 직접 바꾸지 말 것.** `Radius` 는 C++ 이 보스의 `GetEruptRadius()` 를 그대로 넘긴 값이며, 경고 원과 실제 피격 범위가 어긋나면 회피 불가능한 스킬이 된다.

---

### 16.4 [E4] 어빌리티 2종

#### E4-1. `GA_S2MoleBoss_Claw`
부모 **`DRS2MoleClawAttack`** → `Content/Blueprints/AbilitySystem/Enemy/MoleBoss/Abilities/`

**클래스 디폴트 — GAS 공통**

| 항목 | 값 |
|---|---|
| Asset Tags (구 Ability Tags) | ★**`Abilities.MoleBoss.Claw`** |
| Instancing Policy | `Instanced Per Actor` (C++ 기본값, 확인만) |
| Net Execution Policy | `Server Only` (C++ 기본값, 확인만) |
| Damage Effect Class | ★**`GE_Damage`** |
| Damage Type | ★**`Damage.Physical`** |
| Cooldown Gameplay Effect Class | (비워 둠 — BT 의 Wait 로 간격을 준다) |

**클래스 디폴트 — MoleBoss\|Claw**

| 프로퍼티 | 값 |
|---|---|
| `Sweep Radius` | **200** (2m) |
| `Sweep Angle` | **120** |
| `Vertical Tolerance` | 400 |
| `Hit1Damage` | Curve Table = `CT_Damage`, Row = **`Abilities.MoleBoss.Claw.Hit1`**, **Value = 1** ★ |
| `Hit2Damage` | Curve Table = `CT_Damage`, Row = **`Abilities.MoleBoss.Claw.Hit2`**, **Value = 1** ★ |

> ★`Value` 를 1 이 아닌 값으로 두면 커브값에 곱해진다. **0 이면 데미지가 항상 0** — 이 경우 어빌리티 부여 시점에 Error 로그가 뜬다.

**이벤트 그래프** — ★`GA_Bear_SweepAttack` 의 구조를 그대로 미러링한다 (프로젝트 기존 패턴)

```
Event ActivateAbility
  ├─→ Wait Gameplay Event  (Event Tag = Montage.Attack.1)
  │      └ EventReceived → Perform Claw Sweep (Hit Index = 0)
  │
  ├─→ Wait Gameplay Event  (Event Tag = Montage.Attack.2)
  │      └ EventReceived → Perform Claw Sweep (Hit Index = 1)
  │
  └─→ Play Montage and Wait  (Montage = AM_MoleBoss_Claw)
         ├ OnCompleted   → End Ability
         ├ OnBlendOut    → End Ability
         ├ OnInterrupted → End Ability
         └ OnCancelled   → End Ability
```

> **`Play Montage and Wait` 에는 노티파이 출력 핀이 없다.** 스톡 `UAbilityTask_PlayMontageAndWait` 의 핀은 OnCompleted / OnBlendOut / OnInterrupted / OnCancelled 넷뿐이다.
> 그래서 이 프로젝트는 **몽타주에 `AN_MontageEvent` 를 심어 게임플레이 이벤트를 쏘고, GA 가 `Wait Gameplay Event` 로 받는** 방식을 쓴다 (`GA_Bear_SweepAttack` 실측: `AbilityTask_WaitGameplayEvent` + `Montage.Attack.1` → `PerformSweepAttack`).
> **`Wait Gameplay Event` 를 몽타주 재생보다 먼저 연결**해야 한다. 나중에 걸면 첫 노티파이를 놓친다.

#### E4-2. `GA_S2MoleBoss_BurrowStrike`
부모 **`DRS2MoleBurrowStrike`**

**클래스 디폴트 — GAS 공통**

| 항목 | 값 |
|---|---|
| Asset Tags | ★**`Abilities.MoleBoss.BurrowStrike`** — 이 태그로 `NotifyStashed()` 가 취소를 건다. **틀리면 도망 시 유령 융기 버그가 난다** |
| Activation Owned Tags | **`State.MoleBoss.Burrowed`** |
| Cooldown Gameplay Effect Class | ★**`GE_Cooldown_MoleBoss_BurrowStrike`** (E1-2) |
| Damage Effect Class | ★**`GE_Damage`** |
| Damage Type | **`Damage.Physical`** |
| **`Damage`** (상속 프로퍼티, Damage 카테고리) | ★Curve Table = `CT_Damage`, Row = **`Abilities.MoleBoss.BurrowStrike`**, **Value = 1** — ★**이게 융기 데미지다.** 어빌리티 레벨(= 인원수)로 조회되므로 인원 축이 자동으로 붙는다 |
| Knockback Chance | 100 (참고용. 실제 넉백 벡터는 C++ 이 직접 넣는다) |

**클래스 디폴트 — MoleBoss\|Burrow**

| 프로퍼티 | 값 | 비고 |
|---|---|---|
| `Warning Actor Class` | ★**`BP_S2GroundWarning`** | 미설정 시 경고 없이 융기(Error 로그) |
| `Burrow Duration` | 0.6 | **몽타주 미지정 시에만 쓰이는 폴백.** 몽타주가 있으면 그 길이가 우선한다 |
| `Warning Duration` | **0.8** | ★사용자 확정 |
| `Erupt Delay After Warning` | **0.4** | ★사용자 확정 |
| `Emerge Recover Duration` | 0.8 | **폴백.** 몽타주가 있으면 그 길이가 우선한다 |
| `Burrow Cooldown` | Curve = `CT_Damage` / **`Cooldown.MoleBoss.BurrowStrike`**, **Value = 1** ★ | 구간 축 |
| `Launch Z` / `Launch Radial` | 700 / 300 | |
| `Max Target Range` | 4000 | |
| `b Exclude Seated Players` | **체크** | 열차 위로 솟지 않게 |
| `b Warning Follows Target` | **해제** | ★확정: 고정(회피 가능) |
| `b Hit Other Enemies` | **체크** | ★확정: 다른 적도 맞는다 |

**이벤트 그래프** — ★**거의 비어 있다.** 몽타주도 타이밍도 전부 C++ 이 처리한다.

```
Event K2 On Burrow Started   → (선택) 흙먼지 Niagara / 사운드 / 카메라 셰이크
                                ※ 몽타주는 재생하지 않는다 — 보스가 이미 재생했다
```

> 융기 순간의 FX 는 이 GA 가 아니라 **`BP_S2MoleBoss` 의 `Event On Erupt Visual`** 에서 처리한다 (E6-g).
> ★**`Play Montage and Wait` 를 넣지 않는다.** 몽타주는 `BP_S2MoleBoss` 의 `BurrowMontage`/`EmergeMontage` 에 지정만 하면 C++ 이 재생하고 **길이까지 읽어 타이밍을 맞춘다**.
> ★**`End Ability` 를 그래프에서 부르지 않는다.** 융기 몽타주 길이만큼 유지한 뒤 C++ 이 종료시킨다.

---

### 16.5 [E5] 애니메이션 — 몽타주 3종 + `AN_MontageEvent`

`Content/Blueprints/Character/Enemy/MoleBoss/Montage/`

임시 진행이라면 `ABP_EliteBear` 가 쓰는 스켈레톤과 몽타주를 복제해 이름만 바꿔도 된다.

#### 5-a. 기존 노티파이 자산을 그대로 쓴다 ★신규 제작 불필요

`Content/Blueprints/AnimNotifies/AN_MontageEvent` 가 이미 있다. 구조는:
- `EventTag` (GameplayTag) 프로퍼티
- `Received_Notify` 에서 `SendGameplayEventToActor(MeshComp 소유자, EventTag)` 호출

즉 **몽타주에 이 노티파이를 심고 `EventTag` 만 지정하면** GA 가 `Wait Gameplay Event` 로 받는다.

#### 5-b. 사용할 태그 — ★신규 태그 불필요

`FDRGameplayTags` 에 이미 `Montage.Attack.1 ~ .4` 가 등록되어 있다. 발톱 2타에 그대로 재사용한다.

| 몽타주 | 길이 | 심을 노티파이 | 위치 | `EventTag` | 저작 요건 |
|---|---|---|---|---|---|
| `AM_MoleBoss_Claw` | ~1.4s | `AN_MontageEvent` ×2 | ≈0.45s | **`Montage.Attack.1`** | 반매몰 상태에서 상반신으로 휘두른다 |
| | | | ≈0.95s | **`Montage.Attack.2`** | |
| `AM_MoleBoss_Burrow` | 자유 | 없음 | | | ★**끝 포즈 = 완전히 땅속** |
| `AM_MoleBoss_Emerge` | 자유 | 없음 | | | ★**첫 프레임 = 완전히 땅속** |

> **잠수/융기 몽타주는 노티파이가 필요 없다.** C++ 이 직접 재생하고 **길이를 읽어 타이밍을 맞춘다**(§15.6) — 몇 초짜리로 만들든 코드를 고칠 필요가 없다.
> ★**두더지의 평상시는 반매몰**이다. 잠수 = "반매몰 → 완전 매몰", 융기 = "완전 매몰 → 반매몰". 지상에 완전히 선 포즈 기준으로 만들면 융기 후 몸이 지면 위로 다 나와 버린다.

#### 5-c. `ABP_S2MoleBoss`

몽타주 슬롯(`DefaultSlot`)만 있으면 되고, **AnimNotify 이벤트를 ABP 에서 받을 필요가 없다.** 노티파이가 게임플레이 이벤트로 GA 에 직접 도달하기 때문이다.

#### 5-d. ★원격 클라이언트에서 모션이 안 보이면

Armadillo 선례(`DRArmadilloEnemy.h:128-151`)와 같은 증상이다. `BP_S2MoleBoss` 에 `Multicast_PlayMontage(UAnimMontage*)` 커스텀 이벤트(Replicates = **NetMulticast**, **Reliable**)를 만들고 GA 에서 `Play Montage and Wait` 과 **함께** 호출한다. 이벤트 내부에서 `Switch Has Authority → Remote` 만 재생해 서버 이중 재생을 피한다.

> **주의**: 노티파이는 몽타주가 재생되는 머신에서만 발화한다. 데미지 판정은 서버에서만 일어나야 하므로 **서버 재생 경로(GA 의 `Play Montage and Wait`)를 없애면 안 된다.** Multicast 는 어디까지나 원격 클라의 시각용 추가다.

**★원격 클라이언트에서 모션이 안 보이면** (Armadillo 선례 — `DRArmadilloEnemy.h:128-151`):
`BP_S2MoleBoss` 에 `Multicast_PlayMontage(Montage)` 커스텀 이벤트(Replicates = **NetMulticast**, Reliable)를 만들고 GA 에서 `Play Montage and Wait` 과 **함께** 호출한다. 서버에서는 이중 재생을 피하도록 `Switch Has Authority` 로 원격만 태운다.

---

### 16.6 [E6] `BP_S2MoleBoss` ★핵심

부모 클래스 **`DRS2MoleBoss`** → `Content/Blueprints/Character/Enemy/MoleBoss/`

#### 6-a. 루트 캡슐 — ★세 가지를 동시에 결정한다

루트 `CapsuleComponent` 는 물리 충돌만이 아니라 **스킬 범위와 매몰 높이의 기준**이다.

| 쓰임 | 계산식 |
|---|---|
| 물리 충돌 · 피격(히트박스가 없을 때) | Pawn 채널 |
| ★**융기 AoE 반경 = 경고 원 반경** | `GetScaledCapsuleRadius() × EruptRadiusScale` |
| ★**반매몰 Z** | `GroundZ + GetScaledCapsuleHalfHeight() × (1 - 2 × BuriedRatio)` |

**메시에 맞추는 절차**

```
1. Mesh 에 스켈레탈메시 지정 + Rotation Yaw = -90 (UE 캐릭터 관례)
2. 뷰포트에서 메시 전체 높이 H_mesh, 가장 넓은 폭 W_mesh 를 잰다
3. Capsule Half Height = H_mesh / 2
   Capsule Radius      = W_mesh / 2
4. Mesh Relative Location Z = -(Capsule Half Height)   ← 발이 캡슐 바닥에 닿는다
5. Anim Class = ABP_S2MoleBoss
```

3~4를 지키면 `BuriedRatio = 0.5` 에서 **메시의 정확히 위쪽 절반**이 지면 위로 나온다.

> ★"몸의 절반"은 **캡슐 기준**이다. 메시가 캡슐보다 짧으면(뚱뚱한 두더지를 긴 캡슐에 넣으면) 절반보다 많이 나온다. `Half Height = H_mesh / 2` 를 지키는 것이 핵심.
> 임시 프리미티브로 진행할 때는 Cylinder 를 쓰고 위 규칙을 그대로 적용한다.

#### 6-b. 세 가지를 독립적으로 조절하는 법 ★

캡슐을 밸런싱 손잡이로 쓰면 안 된다 — 세 값이 함께 움직인다. 축을 나눠 쓴다.

| 조절 대상 | 손잡이 | 다른 값에 미치는 영향 |
|---|---|---|
| 몸 크기 · 피격 범위 | 캡슐 Radius / Half Height | ★AoE 와 매몰 Z 가 함께 변한다 |
| **융기 AoE 반경만** | **`Erupt Radius Scale`** | 없음 |
| **노출 정도만** | **`Buried Ratio`** | 없음 |

#### 6-c. 히트박스 — ★대부분 불필요하다

`ADREnemy::SetupHitboxComponents()` 는 `"Hitbox"` 태그가 붙은 ShapeComponent 를 자동 수집하는데, **하나라도 있으면 캡슐과 메시의 피격 판정을 꺼버린다**:

```cpp
Capsule->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
Capsule->SetCollisionResponseToChannel(ECC_Target,     ECR_Ignore);
GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
```

전부 아니면 전무다. 그리고 **반매몰 상태에서는 히트박스가 없어도 상반신만 맞는다** — 하단 절반은 지형 안이라 투사체가 애초에 도달하지 못하기 때문이다.

**히트박스가 실제로 필요한 유일한 경우**: 캡슐 반지름을 AoE 때문에 몸보다 크게 잡았을 때. 그러면 보이는 몸 바깥에서도 총알이 맞는다. 하지만 그 상황은 **`Erupt Radius Scale` 로 AoE 를 키우면 애초에 생기지 않는다.**

> **결론: 기본은 히트박스 없이 간다.** 캡슐을 몸에 맞추고 AoE 는 `Erupt Radius Scale` 로 조절한다.
> 그래도 필요하면: `Capsule Collision` 추가 → **Component Tags 에 `Hitbox`** → 노출된 상반신을 덮도록 Z/크기 조정.

#### 6-d. 헬스바
`HealthBar` 컴포넌트의 Relative Location **Z = +160** 정도로 올린다. 기본 위치면 반매몰 탓에 지면에 파묻힌다.

#### 6-e. 클래스 디폴트

| 카테고리 | 프로퍼티 | 값 |
|---|---|---|
| MoleBoss\|Pose | `Buried Ratio` | **0.5** (몸의 절반). 노출 정도는 캡슐이 아니라 이 값으로 조절 |
| MoleBoss\|Pose | `Ground Trace Distance` | 1000 |
| MoleBoss\|Burrow | `Erupt Radius Scale` | **1.0** (캡슐 반지름 그대로 — 확정 사양). AoE 조절은 캡슐이 아니라 이 값으로 |
| MoleBoss\|Burrow | `b Clear Debuffs On Burrow` | **체크** |
| MoleBoss\|Field | `Electric Field Segment Index` | **2** (3차) |
| MoleBoss\|Field | `Electric Field Class` | ★**`BP_S2ElectricField`** |
| MoleBoss\|Field | (데미지 수치 없음) | 전기장 데미지는 **`GE_S2ElectricFieldDamage` 의 Modifier 커브**에 있다 (E1-3) |
| MoleBoss\|Anim | `Burrow Montage` | ★**`AM_MoleBoss_Burrow`** — 길이가 자동으로 잠수 시간이 된다 |
| MoleBoss\|Anim | `Emerge Montage` | ★**`AM_MoleBoss_Emerge`** — 길이가 자동으로 융기 후 유지 시간이 된다 |
| Combat | `Rotation Interp Speed` | **4.0** (느린 보스 회전) |
| Combat | `Life Span` | 5.0 정도 (사망 후 시체 유지) |
| Character Class Defaults | `Character Class` | ★**MoleBoss** (C++ 기본값, 확인만) |
| AI | `Behavior Tree` | ★**`BT_S2MoleBoss`** (E8 이후에 채운다) |
| Water System | `b Is Boss` | **체크** (C++ 기본값, 확인만) |
| Pawn | `AI Controller Class` | **`BP_DRAIController`** |
| Pawn | `Auto Possess AI` | **Placed in World or Spawned** |

#### 6-f. ★액터 태그 — 빠뜨리면 아무 공격도 안 통한다
**Class Defaults → Actor → Tags** 에 **`Enemy`** 를 추가한다.
`IsNotFriend` 가 액터 태그 기반이라(`DRAbilitySystemLibrary.cpp:416-423`), 이게 없으면 **플레이어의 모든 공격이 보스를 아군으로 판정해 통과**한다.

#### 6-g. BP 연출 훅 (선택, 나중에 채워도 됨)
| 이벤트 | 용도 |
|---|---|
| `Event On Burrow Visual` | 잠수 순간 흙 폭발 Niagara + 사운드 |
| `Event On Erupt Visual (Erupt Location)` | 융기 흙/파편 + 카메라 셰이크 |
| `Event On Segment Changed Visual (New Segment)` | 3차(=2)에서 전기 오라 켜기 등 |

---

### 16.7 [E7] `DA_EnemyCharacterClassInfo` 에 MoleBoss 행 추가

`Content/Blueprints/AbilitySystem/Data/DA_EnemyCharacterClassInfo` 를 열고 `Character Class Information` 맵에 **키 = MoleBoss** 항목을 추가한다.

| 필드 | 값 |
|---|---|
| Primary Attributes | ★**`GE_PrimaryAttributes_MoleBoss`** (E1-1) |
| Vital Attributes | **`GE_VitalAttributes_Enemy`** (기존 재사용) |
| Startup Abilities | ★**`GA_S2MoleBoss_Claw`**, ★**`GA_S2MoleBoss_BurrowStrike`** |
| Death Abilities | (비움) |

> `GA_HitReact` 는 `Common Abilities` 에 이미 들어 있으므로 여기 추가하지 않는다 (중복 부여 방지). 목록을 열어 확인만 한다.

---

### 16.8 [E8] AI — BT / Service / Task

#### 8-a. `BTS_S2MoleBoss_Target` (BTService, BP)
`BTS_FindNearestPlayer` 를 **복제**해서 시작한다. `Content/Blueprints/AI/Services/`

수행할 일:
1. 최근접 생존 플레이어를 찾아 블랙보드 **`TargetToFollow`** 에 저장
2. ★**`AI Controller → Set Focus (Target)`** 호출 ← **회전의 근원.** 이게 없으면 보스가 돌지 않는다
3. 타깃이 없으면 `Clear Value (TargetToFollow)` + `Clear Focus`
4. 쿨다운 준비 여부를 블랙보드 Bool **`bSkillReady`** 에 저장
   `ASC → Has Matching Gameplay Tag(Cooldown.MoleBoss.BurrowStrike)` 의 **NOT**

> `BB_EnemyBlackboard` 에 **`bSkillReady` (Bool) 키를 추가**해야 한다. `TargetToFollow` 는 기존 키를 그대로 쓴다.

#### 8-b. `BTT_S2MoleBoss_Claw` (BTTask, BP)
`BTT_Attack_EliteBear` 를 복제해서 시작한다.

```
Event Receive Execute AI
  → Controlled Pawn → Get Ability System Component
  → Try Activate Abilities by Tag (Abilities.MoleBoss.Claw)
  → (성공) Finish Execute (Success)  ※ 몽타주 종료 대기는 BT 의 Wait 로 처리
  → (실패) Finish Execute (Fail)
```

#### 8-c. `BTT_S2MoleBoss_BurrowStrike` (BTTask, BP)
동일 패턴, 태그만 **`Abilities.MoleBoss.BurrowStrike`**.
스킬은 1.8초 이상 걸리므로 **어빌리티 종료를 기다려야 한다**:
- 간단한 방법: Task 는 즉시 Success 하고, BT 시퀀스 뒤에 **`Wait 2.6`** 노드를 둔다
- 정확한 방법: `Wait Gameplay Tag Removed (State.MoleBoss.Burrowed)` 후 Finish

#### 8-d. `BT_S2MoleBoss` (BehaviorTree)
`Content/Blueprints/AI/BehaviorTree/` · Blackboard = **`BB_EnemyBlackboard`**

```
Root
└ Selector
   [Service: BTS_S2MoleBoss_Target  (Interval 0.2)]
   │
   ├ Sequence                         [Decorator: Blackboard "Dead"  Is Not Set]
   │  │                               [Decorator: Blackboard "Stunned" Is Not Set]
   │  ├ Selector
   │  │  ├ Sequence                   [Decorator: Blackboard "bSkillReady" Is Set(true)]
   │  │  │  ├ BTT_S2MoleBoss_BurrowStrike
   │  │  │  └ Wait 2.6
   │  │  │
   │  │  └ Sequence                   [Decorator: Distance To (TargetToFollow) <= 200]
   │  │     │                         [Decorator: Blackboard "State.MoleBoss.Burrowed" 상태 아님 — 선택]
   │  │     ├ BTT_S2MoleBoss_Claw
   │  │     └ Wait 1.0
   │  │
   │  └ Wait 0.2
   │
   └ Wait 0.5
```

> ★**`Move To` 노드를 하나도 넣지 않는다.** 이것이 "위치를 이동하지 않는다"의 최종 보증이다.
> 거리 판정은 `Distance To` 데코레이터가 없으면 Service 에서 거리 계산 후 Bool 키(`bInClawRange`)로 넘겨도 된다.

E8 을 마친 뒤 **E6 으로 돌아가 `BP_S2MoleBoss` 의 `Behavior Tree` 에 `BT_S2MoleBoss` 를 지정**한다.

---

### 16.9 [E9] 페이즈 BP 배선

`Content/Blueprints/Phase/Stage2/BP_S2TrainPhase` (Plan6 §15.9-H-7 에서 만든 것)

| 프로퍼티 | 기존 | 변경 |
|---|---|---|
| `Mole Boss Class` | `BP_ElteBear` (임시) | ★**`BP_S2MoleBoss`** |
| `Retreat Health Ratios` | {0.67, 0.34} | 그대로 |
| `Train Speed` | 600 | 그대로 |

> 이것 하나만 바꾸면 페이즈가 두더지 보스를 스폰하고, C++ 이 구간 인덱스·인원수·보관 통지를 자동으로 넣는다. **페이즈 BP 에 추가할 프로퍼티는 없다.**

---

### 16.10 [E10] 레벨 작업

> **지면 판정이 어떻게 동작하는지** — 아래 항목들의 근거
>
> `SnapToBuriedPose()` 는 **하향 라인 트레이스 한 번**으로 지면을 찾는다:
> ```
> Start = ActorLocation + (0,0,HalfHeight)     // 캡슐 위에서 시작 (이미 반매몰이어도 지면을 맞히도록)
> End   = ActorLocation - (0,0,1000)           // GroundTraceDistance
> LineTraceSingleByChannel(..., ECC_WorldStatic, [자신 무시])
> ```
> 호출 시점은 **스폰(BeginPlay)** 과 **구간 재등장(`NotifyReappeared`)**. 융기 시에는 `ResolveEruptGround()` 가 **타깃 발밑**에서 따로 쏘고 NavMesh 에 투영한다.
> 실패하면 `ActorLocation.Z - HalfHeight` 로 폴백하고 Warning 로그를 남긴다.
> ★**중력이 없다**(`GravityScale = 0` + `MOVE_None`). 한 번 스냅하면 끝이고 자동 보정이 없다.

| # | 작업 | 상세 |
|---|---|---|
| 1 | **NavMesh 확장** | 선로변 전투 구간 3곳(전방~후방 배리어 사이)을 `NavMeshBoundsVolume` 이 덮어야 한다. 보스는 이동하지 않지만 **융기 착지점 검증(`ProjectPointToNavigation`)** 에 NavMesh 를 쓴다. 없으면 라인트레이스 결과로 폴백하지만, 열차 지붕 위로 솟아오를 수 있다 |
| 2 | **BossSpawnPoint Z** | `BP_S2TrainObstacle` ×3 의 `BossSpawnPoint` 를 **반드시 바닥 위**에 둔다. 바닥보다 아래면 트레이스가 **그 아래쪽 지형**을 잡아 보스가 지하에 박힌다 |
| 3 | **지면이 `WorldStatic` Block 인지** | 반매몰 계산과 착지점 트레이스가 `ECC_WorldStatic` 을 쓴다. 선로변 바닥을 WorldDynamic 으로 만들었으면 지면을 못 찾는다 |
| 3b | **경사면/가장자리 주의** | 중심점 **한 점만** 샘플링한다. 보스가 설 만한 자리는 평평하게 — 경사면이면 한쪽이 뜨거나 묻힌다 |
| 4 | **전투 구간 폭 확보** | 융기가 플레이어 발밑으로 오므로 도망칠 공간(전후 배리어 간격 최소 1500uu 권장)이 필요하다 |
| 5 | **천장 높이** | 띄우기 `LaunchZ = 700` 이라 플레이어가 700uu 이상 떠오른다. 터널 구간이면 천장에 부딪힌다 |

---

### 16.11 검증 시나리오 — 순서대로

**모두 PIE, Net Mode = Play As Listen Server, Number of Players = 2 이상**

| # | 확인 | 기대 결과 | 실패 시 |
|---|---|---|---|
| V1 | 레벨에 `BP_S2MoleBoss` 직접 배치 후 플레이 | **상반신만 노출** · 클라에서도 같은 자세 | `Buried Ratio` / 캡슐 Half Height / 지면 WorldStatic 확인 |
| V2 | 보스를 총으로 쏘기 | 체력 감소 + 데미지 텍스트 | **액터 태그 `Enemy`** 누락 (16.6-f) |
| V3 | 넉백 스킬로 밀기 | **밀리지 않음** | `MOVE_None` 이 안 걸림 — BeginPlay 로그 확인 |
| V4 | 주위를 원을 그리며 돌기 | 부드럽게 추적 회전, 이동 0 | Service 의 `Set Focus` 누락 (16.8-a) |
| V5 | 2m 안으로 접근 | 발톱 2타 피격 · **원격 클라에서도 모션 보임** | 모션 안 보이면 Multicast 몽타주 추가 (16.5) |
| V6 | 2m 밖 / 등 뒤에서 대기 | **무피격** | `Sweep Radius` 200 / `Sweep Angle` 120 확인 |
| V7 | 1타 직후 뒤로 빠지기 | **2타 회피 성공** | 정상 (각 타가 독립 오버랩) |
| V8 | 스킬 발동 대기 | 잠수 → **0.8초 경고** → 0.4초 후 융기 | `Warning Actor Class` 누락이면 Error 로그 |
| V8a | ★**원격 클라에서 잠수/융기 모션** | 파고드는 동작과 솟는 동작이 **둘 다 보임** | 안 보이면 몽타주 미지정(Warning 로그) 또는 멀티캐스트 실패 |
| V8b | ★잠수 몽타주 길이를 2배로 늘려 재테스트 | **끝까지 다 파고든 뒤** 사라진다 (중간에 끊기지 않음) | 끊기면 C++ 이 길이를 못 읽은 것 — `Burrow Montage` 지정 확인 |
| V8c | ★융기 첫 프레임 | 땅속에서 시작해 올라온다. **지상 포즈로 튀지 않는다** | 튀면 몽타주 첫 프레임이 지상 포즈 |
| V9 | 잠수 중 총 쏘기 | **데미지 0** | `SetActorEnableCollision` 확인 |
| V10 | 화상 걸고 잠수 유도 | 잠수 순간 화상 해제 | `b Clear Debuffs On Burrow` 확인 |
| V11 | 경고 원 밖으로 걸어 나가기 | **회피 성공** | 경고 반경 ≠ 실제 반경이면 `GetEruptRadius` 배선 확인 |
| V12 | 경고 원 안에 서 있기 | 데미지 + **공중에 뜸** | `Launch Z` / `Knockback Force` 확인 |
| V13 | 보스 옆에 잡몹 소환 후 융기 | **잡몹도 피해+띄워짐** | `b Hit Other Enemies` 체크 확인 |
| V14a | ★**보스 스폰 로그 확인** | `[MoleBoss] 스폰 — Level(=기준 인원) N, MaxHealth ...` 에서 **Level 이 실제 인원수**이고 MaxHealth 가 커브값과 일치 | Level 이 1로 고정이면 지연 스폰 경로 미적용 — `SpawnEnemyAt(..., BasePlayerCount)` 확인 |
| V14b | ★**커브 미설정 경고 확인** | 출력 로그에 `Value 가 0입니다` Error 가 **없어야** 한다 | 뜨면 해당 `FScalableFloat` 의 Value 를 1 로 |
| V14 | 1인 PIE vs 2인 PIE 융기 데미지 비교 | **40 vs 46** | `ResolveBasePlayerCount` 로그(`[S2Phase] 기준 인원 확정`) + V14a 확인 |
| V14c | 1인 vs 2인 **보스 체력** 비교 | `Mole.MaxHealth` 커브대로 다름 | V14a 의 MaxHealth 로그로 즉시 판별 |
| V15 | 구간 1→2→3 진행하며 발톱 데미지 | **20/30 → 26/39 → 32/48** | `Set Segment Index` 로그(`[MoleBoss] 구간 인덱스 = N`) 확인 |
| V16 | 구간별 스킬 재사용 간격 측정 | **10 → 8 → 6초** | 쿨다운 GE 가 SetByCaller 인지 확인 (16.1 E1-2) |
| V17 | **구간이 바뀌어도 융기 데미지 불변** | 40/46/52/58 그대로 | 축을 잘못 배선 |
| V18 | 1·2차 구간에서 융기 반복 | **전기장 없음** | `Electric Field Segment Index` = 2 확인 |
| V19 | 3차 1번 융기 | **전기장 없음** | 정상 (직전 지점이 없다) |
| V20 | 3차 2번 융기 | 1번 자리에 **전기장 1개** 생성 | `Electric Field Class` 누락이면 Error 로그 |
| V21 | 3차 3번 융기 | 1번 전기장 소멸 + 2번 자리에 생성 · **동시에 2개 없음** | 링 버퍼 로그 확인 |
| V22 | 전기장 안에 적 세우기 | **적은 무피해** | 정상 (`Cast<ADRCharacter>` 필터) |
| V23 | 전기장 안에 플레이어 서 있기 | 0.5초마다 피해 · **스턴 안 걸림** | 스턴이 걸리면 GE 에 디버프 설정을 넣은 것 |
| V24 | **잠수 중 임계 도달** (67%/34%) | 보스 도망 · **유령 융기 없음** · 경고 액터 잔존 없음 | ★Asset Tag 오타 확인 (16.4 E4-2) |
| V25 | 3차 최종 처치 | 즉시 스테이지 클리어 · **전기장 사라짐** | `OnPhaseEnd` 정리 확인 |
| V26 | **스테이지1 회귀** | 파이어볼트·제트점프·대시 쿨다운 정상 | `DRGameplayAbility` 수정 영향 — ★반드시 확인 |

---

### 16.12 자주 나는 실수 — 증상 → 원인 → 조치

| 증상 | 원인 | 조치 |
|---|---|---|
| 보스를 때려도 아무 반응 없음 | 액터 태그 `Enemy` 누락 | 16.6-f |
| 보스가 안 돎 | Service 의 `Set Focus` 누락 | 16.8-a |
| 보스가 걸어 다님 | BT 에 `Move To` 가 있음 | 16.8-d |
| 몸이 지면에 완전히 파묻힘 / 공중에 뜸 | `Buried Ratio` 오설정, 메시 Z 오프셋 미조정 | 16.6-a |
| **절반보다 훨씬 많이/적게 나옴** | 캡슐 Half Height ≠ 메시 높이/2 | 16.6-a 절차 3~4 |
| **보이는 몸 바깥에서도 총알이 맞음** | AoE 때문에 캡슐을 뚱뚱하게 잡음 | 캡슐은 몸에 맞추고 `Erupt Radius Scale` 로 AoE 조절 (16.6-b) |
| **보스가 지하에 박힘** | `BossSpawnPoint` 가 바닥보다 아래 → 그 아래 지형을 잡음 | 16.10-2 |
| 헬스바가 안 보임 | 반매몰 탓에 지면 아래 | 16.6-d (Z +160) |
| 경고 원과 실제 피격 범위가 다름 | BP 에서 데칼 크기를 직접 수정 | 16.3 (건드리지 말 것) |
| 구간이 바뀌어도 쿨다운이 같음 | 쿨다운 GE 가 고정 Duration | 16.1 E1-2 (SetByCaller) |
| 전기장이 플레이어를 계속 스턴시킴 | GE 에 Debuff 설정을 추가함 | 16.1 E1-3 (디버프 설정 금지) |
| 전기장이 적도 때림 | 구현상 불가능 — 다른 원인 | 로그 확인 |
| **데미지/쿨다운이 전부 0 또는 1** | `FScalableFloat` 의 `Value` 가 0(또는 커브 미지정) | 출력 로그의 `Value 가 0입니다` Error 확인 → Value = 1 |
| **인원수를 바꿔도 체력·융기 데미지가 그대로** | 지연 스폰 경로를 안 탐 (Level 이 1로 고정) | V14a 로그로 Level 확인 |
| 커브를 고쳐도 반영이 안 됨 | 에디터가 커브를 캐시함 | 커브 테이블 저장 후 PIE 재시작 |
| **도망 후 허공에서 데미지가 들어옴** | GA Asset Tag 가 `Abilities.MoleBoss.BurrowStrike` 와 불일치 | 16.4 E4-2 ★최우선 확인 |
| 융기 후 보스가 홱 돌아감 | 구현상 방지됨 (`EruptAt` 이 ControlRotation 동기화) | 발생 시 Service 가 매 틱 Focus 를 덮는지 확인 |
| 스킬을 영원히 안 씀 | 전원 착석 → 후보 0명 | `b Exclude Seated Players` 해제 또는 하차 유도 |
| 원격 클라에서 발톱 모션 안 보임 | GA 몽타주 복제 한계 (Armadillo 선례) | 16.5-d Multicast 병행 |
| **잠수/융기 모션이 안 보임** | `BP_S2MoleBoss` 의 `Burrow/Emerge Montage` 미지정 | 출력 로그에 `Montage 미설정` Warning 확인 |
| **파고드는 중간에 사라짐** | 몽타주 미지정 → 폴백 `BurrowDuration`(0.6s)이 쓰임 | 몽타주를 지정하면 길이가 자동 적용된다 |
| **융기 시작에 지상 포즈가 한 프레임 튐** | 융기 몽타주 첫 프레임이 지상 포즈 | 첫 프레임을 완전 매몰 포즈로 |
| 열차 지붕 위로 솟아오름 | NavMesh 미커버 | 16.10-1 |

---

### 16.13 에디터 작업 요약 체크리스트

```
[ ] 16.0  태그 4개 · MoleBoss enum · C++ 클래스 표시 확인
[ ] E1-0  CT_Damage 에 커브 행 5개 (+ Mole.MaxHealth 값 재검토)  ★먼저
[ ] E1-1  GE_PrimaryAttributes_MoleBoss           ★MaxHealth = Scalable Float(Mole.MaxHealth)
[ ] E1-2  GE_Cooldown_MoleBoss_BurrowStrike       ★SetByCaller(Data.Cooldown)
[ ] E1-3  GE_S2ElectricFieldDamage                ★Infinite + Period 0.5 + ExecCalc_Damage
[ ] E2    BP_S2ElectricField                      (GE / 데칼 / 나이아가라 / 사운드)
[ ] E3    BP_S2GroundWarning                      (데칼 MI + 0.8초 타임라인)
[ ] E4-1  GA_S2MoleBoss_Claw                      ★Asset Tag + Hit1/Hit2Damage 커브 (Value=1)
[ ] E4-2  GA_S2MoleBoss_BurrowStrike              ★Asset Tag + Damage/BurrowCooldown 커브 + 쿨다운 GE + 경고 클래스
[ ] E5    몽타주 3종 + AN_MontageEvent 2개 (Montage.Attack.1 / .2)
[ ] E6    BP_S2MoleBoss                           ★캡슐 90/120 + 액터 태그 Enemy
[ ] E7    DA_EnemyCharacterClassInfo → MoleBoss 행
[ ] E8    BB 키 추가 + Service + Task 2종 + BT_S2MoleBoss
[ ] E6b   BP_S2MoleBoss 에 BT 지정 (E8 이후)
[ ] E9    BP_S2TrainPhase → MoleBossClass = BP_S2MoleBoss
[ ] E10   NavMesh 확장 + BossSpawnPoint Z + 구간 폭/천장
[ ] V1~V26 검증
```

---

## 17. ★기본공격 원거리화 — 대각 검기(Slash Wave) 전환 계획 (2026-09-09 신규 · 사양 확정본)

> **한 줄 요약**: 두더지 보스의 기본공격을 **"전방 120° / 캡슐 표면+150uu 즉발 부채꼴 2타"** 에서
> **"전방으로 날아가는 45° 대각 관통 검기 2발 (1타 `/` · 2타 `\`)"** 로 바꾼다.
> **판정 형태만 바뀌고, 데미지 축(구간별 1타/2타 커브)·몽타주 노티파이·GA 자산·태그·AI 태스크는 전부 그대로 쓴다.**

### 17.1 왜 바꾸나 — 이 변경이 실제로 고치는 문제

현재 보스는 **`MOVE_None` 으로 절대 이동하지 않는다**(§2.1). 그 결과 기본공격의 사거리가 곧 "보스의 위협 반경 전부"인데, 실효 사거리가 캡슐 표면에서 150uu 밖에 안 된다.

| 현재의 문제 | 검기 전환이 주는 변화 |
|---|---|
| 플레이어가 **2m 밖에 서 있기만 하면 기본공격이 영원히 무의미**하다. 위협이 스킬1(굴착 강습, 쿨다운 10/8/6초)뿐이라 그 사이가 통째로 무위험 구간이 된다 | 원거리 압박이 상시로 깔린다. **"멈춰서 쏘면 검기에 맞는다"** 는 이동 강제력이 생긴다 |
| 이동하지 않는 보스 + 근접 기본공격 = 설계상 **서로 부정하는 조합** | 이동하지 않는 보스 + 원거리 기본공격 = 포탑형 보스의 정석. 회피는 이동으로 한다 |
| 등 뒤로 돌아가는 플레이(§2.1 의 느린 회전 4.0)가 **보상받지 못한다** — 어차피 안 맞으니 | 검기는 전방으로만 날아가므로 **측면·후면이 진짜 안전지대**가 된다. 느린 회전 설정이 비로소 의미를 갖는다 |
| 2타가 1타와 **기능적으로 완전히 같다** (같은 부채꼴을 두 번 훑을 뿐) | ★**1타 `/` · 2타 `\`** 로 기울기가 반대라, 2타가 1타의 사각을 메운다. **"같은 방향으로만 피하면 2타에 맞는" 구조**가 생긴다 (17.8-F) |
| 반매몰 자세(§6.1) 때문에 근접 판정에 Z 보정 코드가 잔뜩 붙어 있다 | 투사체는 발사 Z 를 한 번만 정하면 끝난다 |

### 17.2 ★사용자 확정 사양 (2026-09-09)

§2.5 와 같은 형식. **아래는 전부 확정이며 추가 질의 없이 진행한다.**

| # | 쟁점 | **확정** |
|---|---|---|
| A | 조준 방식 | **전방 기준 + ±25° 조준 보정** (`MaxAimYawFromForward = 25`) |
| B | 관통 | ★**인원수 무제한 관통** (`MaxPierceCount = 0`). **단 지형지물(벽·열차)은 관통하지 못하고 그 자리에서 소멸** (`bStopOnWorldGeometry = true`) |
| C | 좌석 착석 플레이어 | **검기에 맞는다** (조준 대상 후보에서만 제외) |
| D | 지형 차단 | **막힌다** — B 와 동일 결론 |
| E | 근접 사각지대 | **없다** — 몸통 안에서 발사해 밀착해도 맞는다 (17.8-C) |
| F | 데미지 재조정 | **1차 플레이테스트까지 현행 커브 유지** (20/30 → 26/39 → 32/48) |
| G | BT 재시도 간격 | **1.0 → 1.8초** |
| ★H | **검기 형태** | ★**수평이 아니라 45° 대각선.** 판정 캡슐을 진행축(X) 기준 45° 롤 시킨다. **1타와 2타는 서로 반대 방향의 대각선** (`/` 와 `\`) |
| ★H-1 | **판정과 이펙트의 관계** | ★**보이는 참격 이펙트와 실제 판정이 똑같이 `/` `\` 로 기운다.** 둘을 따로 만들지 않고 **액터를 통째로 롤** 시켜 항상 일치시킨다 (17.6-C) |

> **H 의 해석 확정**: "대각선으로 날아온다" = **칼날(이펙트 + 판정)의 단면이 `/` `\` 로 기울어 있다**는 뜻이다. **비행 궤적 자체는 여전히 수평·고도 고정**이다.
> 이 구분이 중요한 이유: 궤적까지 기울이면 **벽 판정용 수평 선분 트레이스의 전제(17.8-D)가 깨지고**, 지면과의 관계가 매 발 달라져 "어떤 발은 바닥에 박히고 어떤 발은 머리 위로 지나가는" 통제 불가 상태가 된다. 기울기는 **롤(Roll) 하나로만** 준다.
>
> ★**`/` `\` 의 기준 시점**: **플레이어가 검기를 정면으로 마주 본 화면** 기준이다. 보스 뒤에서 보면 좌우가 뒤집혀 보이므로, 스폰 회전의 롤 부호는 **반드시 플레이어 시점에서 눈으로 확인**한다 (17.13-V45). 반대로 나오면 `Slash Roll Angles` 의 두 값을 맞바꾼다 — 코드는 손대지 않는다.

### 17.3 바뀌는 것 / 그대로인 것 ★ (이 표가 작업 범위 전부다)

| 영역 | 상태 |
|---|---|
| **판정 방식** | ★변경 — 즉발 부채꼴 오버랩 → **45° 기울어진 캡슐 투사체** 비행 + 무제한 관통 |
| **신규 액터** | ★추가 — `ADRS2MoleSlashWave` (`ADRProjectile` 상속) + `BP_Mole_SlashWave` |
| **GA C++ 클래스** | ★내부 수정 — `UDRS2MoleClawAttack` 의 `PerformClawSweep` 본문 교체 (클래스 자체는 유지) |
| **GA 블루프린트 (`GA_Mole_Claw`)** | 유지 — 이벤트 그래프 **수정 없음**. 클래스 디폴트 프로퍼티만 재설정 |
| **Asset Tag `Abilities.MoleBoss.Claw`** | 유지 — BT 태스크가 이 태그로 활성화하므로 건드리지 않는다 |
| **몽타주 `AM_MoleBoss_Claw` + `Montage.Attack.1/.2`** | 유지 (노티파이 2개가 그대로 1타/2타 발사 시점) · ★**애니메이션 궤적만 좌우 반대로 저작 권장** (17.12-E14) |
| **데미지 커브 행** (`Abilities.MoleBoss.Claw.Hit1/.Hit2`) | 유지 — ★**`CT_Damage` 에 추가할 행이 하나도 없다** |
| **데미지 축 (구간 1/2/3차)** | 유지 — `ResolveDamage()` 로직 무수정 |
| **물 보상 감소 (`OnAttackExecuted`)** | 유지 — 1타에서만 1회 |
| **BT 태스크 `BTT_S2MoleBoss_Claw`** | 유지 |
| **BT 거리 데코레이터 (`<= 200`)** | ★변경 — 검기 사거리에 맞춘다 (17.12-E13) |
| **보스 본체 `ADRS2MoleBoss`** | ★**수정 없음** — 반매몰·잠수·융기·전기장 전부 무관 |
| **페이즈 `UDRS2TrainPhase`** | ★**수정 없음** |
| **신규 게임플레이 태그** | ★**불필요** |

### 17.4 기준선 — 현재 구현 실측 (2026-09-09 코드 기준)

**호출 체인** — 이 사슬의 **마지막 한 칸만 갈아끼운다**:

```
BT_S2MoleBoss
 └ [Decorator] Distance To (TargetToFollow) <= 200          ← ★여기 (17.12-E13)
   └ BTT_S2MoleBoss_Claw
      └ TryActivateAbilitiesByTag(Abilities.MoleBoss.Claw)
         └ GA_Mole_Claw (UDRS2MoleClawAttack, ServerOnly / InstancedPerActor)
            ├ Wait Gameplay Event(Montage.Attack.1) → PerformClawSweep(0)   → 검기 `/`
            ├ Wait Gameplay Event(Montage.Attack.2) → PerformClawSweep(1)   → 검기 `\`
            └ Play Montage and Wait(AM_MoleBoss_Claw)
                 └ PerformClawSweep() 내부:
                    GetLiveObjectsWithinRadius → IsNotFriend → XY거리 → 부채꼴 각도 → ApplyDamageEffect
                    ▲ ★이 5줄만 "검기 스폰"으로 교체된다
```

| 파일 | 현재 역할 | 이번 변경 |
|---|---|---|
| `Public/AbilitySystem/Abilities/Stage2/DRS2MoleClawAttack.h` | 부채꼴 파라미터 + Hit1/Hit2 커브 | ★수정 (부채꼴 필드 → 검기 필드) |
| `Private/.../DRS2MoleClawAttack.cpp` | 오버랩 판정 + 데미지 | ★수정 (스폰으로 교체) |
| `Public/Actor/DRProjectile.h/.cpp` | 공용 투사체 (스피어 + PMC + `FDamageEffectParams`) | 무수정 — **상속만 한다** |
| `Public/Actor/DRVacuumAirProjectile.h/.cpp` | 루트 스피어 외 별도 판정 컴포넌트를 다는 선례 | 무수정 — **패턴만 차용** |
| `Public/Character/Stage2/DRS2MoleBoss.h/.cpp` | 보스 본체 | ★무수정 |

> **★문서 드리프트 경고**: §16.4 E4-1 의 표에 적힌 `Sweep Radius = 200` / `Sweep Angle = 120` 은 **현재 코드와 다르다.** 실제 구현은 `ClawReach = 150` (캡슐 **표면** 기준, 실효 반경 = `GetScaledCapsuleRadius() + ClawReach`) 으로 이미 한 번 개정됐고 그 개정이 §16.4 에 반영되지 않았다. 이번 전환으로 **해당 표 자체가 폐기**되므로 드리프트도 함께 해소된다 (17.16).

### 17.5 사양 확정 — 대각 검기 수치

| 항목 | 값 | 근거 |
|---|---|---|
| 형태 | 진행축 기준 **45° 기울어진 캡슐 칼날** (중력 0, 고도 고정) | 확정 H |
| 1타 기울기 | **Roll = +45°** (`/` 모양) | 확정 H |
| 2타 기울기 | **Roll = −45°** (`\` 모양) | ★두 발이 X 자로 교차해 사각을 메운다 (17.8-F) |
| 발사 수 | **타당 1발** (총 2발) | §2.2 "2타" 사양 유지 |
| 속도 | **1200 uu/s** | 최대 사거리까지 ≈1.2초. 옆으로 굴러 회피 가능한 하한 |
| 최대 사거리 | **1400 uu (14m)** | 전투 구간(§1.5) 을 대략 덮되 배리어 너머까지는 못 가는 길이 |
| 수명 | ★**자동** = 최대 사거리 ÷ 속도 (≈1.17s) | 두 값이 어긋날 여지를 코드가 없앤다 |
| 판정 캡슐 | **Half Height 200 / Radius 50** | 바운딩 ≈ **312 × 312**, 진행방향 두께 100 (17.8-E 계산) |
| 발사 고도 | **보스 액터 Z + 100uu** | 반매몰이라 보스 액터 Z = 지면 Z (§6.1). 칼날 중심이 플레이어 가슴 높이 |
| 발사 원점 | 캡슐 중심 + 전방 × (보스 캡슐반지름 × 0.6) | ★몸통 **안**에서 출발 — 밀착 플레이어도 통과 경로에 들어온다 (17.8-C) |
| 관통 | ★**무제한** (`MaxPierceCount = 0`) · 동일 대상 재타격 없음 | 확정 B |
| 지형 | ★**벽·열차에 막혀 소멸** · 배리어·바닥은 통과 | 확정 B/D. 배리어는 Pawn 만 Block (`DRS2Barrier.cpp:20-22` 실측) |
| 대상 | **플레이어 전용** (`IsNotFriend` + `Cast<ADRCharacter>`) | 기존과 동일 — 융기와 달리 아군 오사 없음 |
| 데미지 | ★**변경 없음** — 구간별 Hit1/Hit2 커브 (20/30 → 26/39 → 32/48) | 확정 F |
| 넉백 | 부모 `ADRProjectile` 의 `KnockbackChance` 경로, **기본 0** | 원거리 상시 공격이 계속 띄우면 조작감이 무너진다 |

### 17.6 아키텍처 결정

#### A. 왜 `ADRProjectile` 상속인가 — 공짜로 얻는 것

| 얻는 것 | 부모 코드 위치 |
|---|---|
| 복제 + 이동 복제 (`bReplicates`, `SetReplicateMovement`) | `DRProjectile.cpp:19, 45` |
| `UProjectileMovementComponent` (중력 0 기본) | `DRProjectile.cpp:32-35` |
| `ECC_Projectile` 오브젝트 타입 + Pawn/World 오버랩 프로파일 | `DRProjectile.cpp:22-30` |
| `FDamageEffectParams` 운반 + `ApplyDamageEffect` 파이프라인 | `DRProjectile.cpp:113-116` |
| 아군 필터 (`IsNotFriend`) · 자기 자신 제외 | `DRProjectile.cpp:87-90` |
| 임팩트 나이아가라 / 사운드 / 루핑 사운드 | `DRProjectile.cpp:53-67` |
| 클라 소멸 시 이펙트 재생 보정 (`Destroyed` 의 `!bHit` 분기) | `DRProjectile.cpp:76` |

#### B. ★왜 Box 가 아니라 Capsule 인가 — 확정 H 가 형태를 결정한다

초안은 `ADRVacuumAirProjectile` 을 따라 **가로형 Box** 를 쓰려 했다. 확정 H(45° 대각)로 **캡슐이 정답이 된다**:

| 후보 | 판정 |
|---|---|
| `UBoxComponent` | 회전은 되지만 모서리가 각져 **대각선 칼날의 끝단이 뭉툭**하다. 45° 회전 시 코너가 판정 밖으로 삐져나와 "안 맞았는데 맞음"이 잘 난다 |
| `USphereComponent` 확대 | 비균등 스케일 불가 — 얇고 긴 칼날 자체가 표현 불가 |
| ★**`UCapsuleComponent`** | **길고 얇은 형태가 기본형**이고, 축(로컬 Z)을 롤로 눕히면 그대로 대각 칼날이 된다. 끝단이 둥글어 시각(참격 나이아가라)과 판정이 잘 맞는다 |

> 캡슐도 비균등 스케일은 안 되지만 **필요가 없다** — 길이는 `HalfHeight`, 두께는 `Radius` 로 각각 따로 조절된다. Box 를 쓸 때의 "루트 스피어는 비균등 스케일 불가" 문제 자체가 사라진다.

#### C. ★기울기를 컴포넌트가 아니라 **액터 스폰 회전**으로 주는 이유

기울기를 주는 방법은 둘이다:

| 방법 | 결과 |
|---|---|
| 캡슐 컴포넌트의 상대 회전을 타마다 바꾼다 | 판정만 기울고 **메시/나이아가라는 그대로** → 보이는 것과 맞는 것이 어긋난다. BP 에서 연출을 맞추려면 같은 값을 두 번 관리해야 한다 |
| ★**액터 스폰 회전의 Roll 에 ±45 를 넣는다** | **액터 전체(판정 + 메시 + 나이아가라 + 트레일)가 통째로 기운다.** BP 는 "수평 칼날"만 만들면 되고 방향 전환에 추가 작업이 0 이다 |

**롤은 비행에 아무 영향이 없다.** `UProjectileMovementComponent` 의 속도는 `Velocity = Forward(X) * Speed` 이고 롤은 X 축 자체를 회전시키지 않는다. 즉 **궤적은 수평 그대로, 칼날 단면만 기운다** — 확정 H 의 요구와 정확히 일치한다.

> ★**반드시 확인할 함정**: `ProjectileMovement->bRotationFollowsVelocity` 가 **true 이면 매 프레임 회전이 속도 방향으로 덮어써져 롤이 지워진다** (칼날이 항상 수평으로 돌아온다). UE 기본값은 false 이며 `ADRProjectile` 도 건드리지 않지만, **`BP_Mole_SlashWave` 에서 실수로 체크하면 대각선이 통째로 사라진다.** 17.13-V45 가 이걸 잡는다.

#### D. 왜 GA 클래스를 새로 만들지 않는가 ★

새 GA 클래스를 만들면 **`GA_Mole_Claw` 를 리페어런트하거나 새 BP 를 만들어야 하고**, 그러면 Asset Tag·몽타주 배선·`DA_EnemyCharacterClassInfo` 의 StartupAbilities·BT 태스크가 전부 재검증 대상이 된다. §16.12 의 "자주 나는 실수" 표에서 **Asset Tag 불일치가 최상위 함정**으로 지목된 자산이다.

반면 기존 클래스의 `PerformClawSweep` **본문만** 교체하면:
- BP 이벤트 그래프가 부르는 **함수 시그니처가 그대로**라 컴파일 깨짐이 없다
- ★**`HitIndex` 가 이미 1타/2타를 구분해 넘어온다** — 대각선 방향을 고르는 인덱스로 **그대로 재사용**된다. 새 배선이 필요 없다
- 태그·몽타주·AI·데이터 에셋이 **한 줄도 안 바뀐다**
- 롤백이 체크박스 하나로 끝난다 (아래 F)

> 클래스 **이름**(`ClawAttack`)이 내용(검기)과 어긋나는 것은 감수한다. 이름을 바꾸면 `CoreRedirects` 를 쓰더라도 BP 재저장·리페어런트 리스크가 생기고 얻는 것은 가독성뿐이다. **이름 변경은 검기가 최종 확정된 뒤 별도 커밋으로 분리**한다 (17.15-11, 선택).

#### E. C++ / BP 역할 분담 (§3.3 관례 유지)

| 층 | 담당 |
|---|---|
| C++ (`ADRS2MoleSlashWave`) | 비행·관통·중복 방지·벽 정지·데미지 적용 — **판정 전부** |
| C++ (`UDRS2MoleClawAttack`) | 발사 원점/방향/**타별 롤 각도** 계산, 데미지 값 결정, 스폰 |
| BP (`BP_Mole_SlashWave`) | 메시/나이아가라/사운드/**캡슐 치수** — **연출과 크기 전부** |
| BP (`GA_Mole_Claw`) | 몽타주 재생 + 노티파이 대기 — **수정 없음** |

#### F. 롤백 스위치 ★

`UDRS2MoleClawAttack` 에 `bUseMeleeSweep` (기본 **false**) 를 남기고 **기존 부채꼴 코드를 `PerformMeleeSweep_Legacy()` 로 살려 둔다.**
플레이테스트에서 근접 버전과 A/B 비교가 가능하고, 문제 발생 시 BP 체크박스 하나로 즉시 되돌아간다.
검기가 확정되면 **다음 마일스톤에서 레거시 경로를 삭제**한다 (17.15-11).

### 17.7 신규 C++ — `ADRS2MoleSlashWave`

**파일**: `Public|Private/Actor/Stage2/DRS2MoleSlashWave.h/.cpp`

```cpp
UCLASS()
class DAERUNE_API ADRS2MoleSlashWave : public ADRProjectile
{
    GENERATED_BODY()
public:
    ADRS2MoleSlashWave();
    virtual void Tick(float DeltaSeconds) override;

    /** 서버 전용. GA 가 SpawnActorDeferred 직후 FinishSpawning 전에 호출한다. */
    void InitWave(float InSpeed, float InMaxRange);

protected:
    virtual void BeginPlay() override;

    /** ★부모를 호출하지 않는다 — 부모는 첫 히트에 Destroy 하므로 관통이 불가능하다. */
    virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
        const FHitResult& SweepResult) override;

    /**
     * ★대각 칼날 판정.
     *
     * 캡슐의 축은 로컬 Z 다. 여기에 상대 회전 Roll = 90 을 주어 축을 **액터의 Y(좌우)** 로 눕힌다.
     * 그 뒤 액터 자체의 스폰 회전 Roll(±45) 이 이 칼날을 진행축 기준으로 기울인다 (17.6-C).
     *
     * ★부호는 에디터에서 눈으로 확인하고 뒤집는다 — UE 의 롤 부호 관례를 문서로 단정하지 않는다.
     *   확인 방법은 17.13-V45.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SlashWave")
    TObjectPtr<UCapsuleComponent> BladeCapsule;

    /** ★0 = 무제한 관통 (확정 B). 0 보다 크면 그 수만큼만 뚫는다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SlashWave", meta = (ClampMin = "0"))
    int32 MaxPierceCount = 0;

    /** ★지형지물(벽·열차) 관통 금지 (확정 B/D). 배리어는 Visibility 를 Ignore 하므로 통과한다. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SlashWave")
    bool bStopOnWorldGeometry = true;

    /** 관통 순간 연출 (BP 훅) — 무제한 관통이라 여러 번 불릴 수 있다 */
    UFUNCTION(BlueprintImplementableEvent, Category = "SlashWave")
    void OnPierceVisual(const FVector& HitLocation);

private:
    /** ★동일 대상 재타격 금지. 오버랩에서 빠졌다가 다시 들어와도 두 번 맞지 않는다. */
    TSet<TWeakObjectPtr<AActor>> HitActors;

    int32 PierceCount = 0;
    FVector LastTickLocation = FVector::ZeroVector;
};
```

```cpp
ADRS2MoleSlashWave::ADRS2MoleSlashWave()
{
    // ★부모는 Tick 을 끈다. 벽 판정을 위해 다시 켠다 (17.8-D)
    PrimaryActorTick.bCanEverTick = true;

    // ---- 대각 칼날 판정 캡슐 ----
    BladeCapsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("BladeCapsule"));
    BladeCapsule->SetupAttachment(GetRootComponent());

    // ★축(로컬 Z)을 액터 Y 로 눕힌다. 기울기는 액터 스폰 회전의 Roll 이 준다 (17.6-C).
    BladeCapsule->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
    BladeCapsule->SetCapsuleSize(50.f, 200.f);          // Radius 50 / HalfHeight 200

    BladeCapsule->SetCollisionObjectType(ECC_Projectile);
    BladeCapsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    BladeCapsule->SetCollisionResponseToAllChannels(ECR_Ignore);
    BladeCapsule->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    BladeCapsule->OnComponentBeginOverlap.AddDynamic(this, &ADRS2MoleSlashWave::OnSphereOverlap);
}

void ADRS2MoleSlashWave::BeginPlay()
{
    Super::BeginPlay();

    // ★루트 스피어의 판정을 완전히 끈다 — 판정 형태를 캡슐 하나로 단일화한다.
    //   ① 스피어가 살아 있으면 대각선의 "빈 구석"(17.8-F)이 중앙 구에 메워져 설계가 무의미해진다
    //   ② 지면 +100uu 를 나는 물체라 스피어가 바닥(WorldStatic)과 상시 오버랩 → 즉시 소멸한다
    //   (부모 BeginPlay 가 Sphere 에 건 델리게이트는 콜백이 안 오므로 그대로 둬도 무해하다)
    Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

    LastTickLocation = GetActorLocation();
}

void ADRS2MoleSlashWave::InitWave(float InSpeed, float InMaxRange)
{
    ProjectileMovement->InitialSpeed = InSpeed;
    ProjectileMovement->MaxSpeed     = InSpeed;
    ProjectileMovement->ProjectileGravityScale = 0.f;   // 고도 고정

    // ★롤(대각 기울기)이 매 프레임 지워지는 것을 코드로도 막는다 (17.6-C 함정)
    ProjectileMovement->bRotationFollowsVelocity = false;

    // ★수명 = 사거리 / 속도. 두 값이 어긋날 여지를 없앤다 (부모의 LifeSpan 15초를 덮어쓴다)
    SetLifeSpan(InSpeed > KINDA_SMALL_NUMBER ? (InMaxRange / InSpeed) : 1.f);
}

void ADRS2MoleSlashWave::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!HasAuthority() || bHit || !bStopOnWorldGeometry)
    {
        LastTickLocation = GetActorLocation();
        return;
    }

    // ★수평 선분 트레이스 — 고도가 고정이라 바닥은 절대 안 걸리고 수직 지오메트리만 걸린다 (17.8-D)
    const FVector Now = GetActorLocation();
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(this);
    if (DamageEffectParams.SourceAbilitySystemComponent)
    {
        Params.AddIgnoredActor(DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor());
    }

    if (GetWorld()->LineTraceSingleByChannel(Hit, LastTickLocation, Now, ECC_Visibility, Params))
    {
        SetActorLocation(Hit.ImpactPoint);
        OnHit();          // 임팩트 연출 + bHit = true
        Destroy();
        return;
    }
    LastTickLocation = Now;
}

void ADRS2MoleSlashWave::OnSphereOverlap(UPrimitiveComponent*, AActor* OtherActor,
    UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (DamageEffectParams.SourceAbilitySystemComponent == nullptr) return;
    AActor* Source = DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor();
    if (!IsValid(Source) || !IsValid(OtherActor) || Source == OtherActor) return;
    if (!UDRAbilitySystemLibrary::IsNotFriend(Source, OtherActor)) return;

    // ★플레이어만. 벽/소품이 들어와도 여기서 걸러진다 (지형 판정은 Tick 담당)
    if (Cast<ADRCharacter>(OtherActor) == nullptr) return;

    if (HitActors.Contains(OtherActor)) return;     // 동일 대상 재타격 금지
    HitActors.Add(OtherActor);

    OnPierceVisual(OtherActor->GetActorLocation());

    if (HasAuthority())
    {
        if (UAbilitySystemComponent* TargetASC =
            UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
        {
            DamageEffectParams.DeathImpulse =
                GetActorForwardVector() * DamageEffectParams.DeathImpulseMagnitude;
            DamageEffectParams.TargetAbilitySystemComponent = TargetASC;
            UDRAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);
        }

        // ★MaxPierceCount = 0 이면 이 블록에 절대 안 들어간다 = 무제한 관통 (확정 B).
        //   검기를 멈추는 것은 오직 벽(Tick)과 수명뿐이다.
        if (MaxPierceCount > 0 && ++PierceCount >= MaxPierceCount)
        {
            OnHit();
            Destroy();
        }
    }
}
```

> **부모 `OnSphereOverlap` 을 호출하지 않는 이유**: 부모는 `IsNotFriend` 를 통과한 **첫 액터에서 무조건 `Destroy()`** 한다(`DRProjectile.cpp:120`). 그런데 `IsNotFriend` 는 **양쪽 다 `Player`/`Enemy` 태그가 아니면 "적이 아닌 것도 적으로" 통과시킨다**(`DRAbilitySystemLibrary.cpp` 실측 — 벽·소품이 전부 통과). 즉 부모 로직으로는 무제한 관통은커녕 **바닥에 스치기만 해도 사라진다.** 그래서 데미지 파이프라인만 그대로 베끼고 소멸 조건을 갈아끼운다.

### 17.8 핵심 알고리즘

#### A. 발사 원점 — 반매몰 보정이 여기서 끝난다

보스는 캡슐 중심 Z 가 곧 지면 Z 다(`BuriedRatio = 0.5`, §6.1). 따라서:

```
SpawnLocation.XY = Boss.XY + Forward2D * (GetScaledCapsuleRadius() * MuzzleForwardRatio)   // 기본 0.6
SpawnLocation.Z  = Boss.Z + MuzzleHeight                                                   // 기본 100
```

기존 근접 판정의 `VerticalTolerance = 400` 같은 보정이 **전부 필요 없어진다.** 검기는 처음부터 플레이어 가슴 높이를 난다.

> 연출상 손/무기 소켓에서 나오게 하고 싶다면 `ICombatInterface::Execute_GetCombatSocketLocation(Boss, CombatSocket.RightHand)` 를 쓸 수 있지만 **기본값은 위 계산식**이다. 이유: `ADRCharacterBase::GetCombatSocketLocation_Implementation` 은 **태그가 안 맞으면 `FVector()`(월드 원점)을 돌려준다** — 소켓 이름을 BP 에 안 넣으면 검기가 맵 원점에서 발사되는 무증상 버그가 난다.
> ★대각 칼날에서는 **원점 Z 가 곧 칼날의 회전 중심**이라 소켓을 쓰면 1타/2타의 대각선 중심 높이가 손 위치에 따라 흔들린다. 계산식 고정을 권한다.

#### B. 방향 — 전방 + 조준 클램프 + ★타별 롤

```
Desired  = (Target.XY - Spawn.XY) 의 Yaw                  // 타깃이 없으면 Forward.Yaw
Delta    = NormalizedDeltaYaw(Desired, Forward.Yaw)
FireYaw  = Forward.Yaw + Clamp(Delta, -MaxAimYawFromForward, +MaxAimYawFromForward)

FireRot  = FRotator(
             Pitch = 0,                       // ★고도 고정 (17.2-H)
             Yaw   = FireYaw,
             Roll  = SlashRollAngles[HitIndex] // ★1타 +45 / 2타 -45  ← 대각선의 정체
           )
```

- 타깃은 `ICombatInterface::Execute_GetCombatTarget(Boss)` → 없으면 `ADRGameStateBase::GetAlivePlayers()` 중 최근접 (스킬1 의 `PickRandomTarget()` 과 같은 소스: `DRS2MoleBurrowStrike.cpp:333-363`)
- ★**Yaw 와 Roll 은 완전히 독립**이다. 조준 보정(Yaw)이 어떤 값이든 칼날 기울기(Roll)는 그대로 유지된다
- ★`Pitch = 0` 고정이 **대각선 사양의 전제**다. Pitch 가 들어가면 롤 축(=진행축)이 위아래로 기울어 대각선 각도가 거리마다 달라지고, 17.8-D 의 수평 선분 트레이스도 깨진다

#### C. 근접 사각지대를 만들지 않는 법

이 보스는 캡슐 반지름이 커서(§16.6-a) **플레이어가 아무리 붙어도 캡슐 중심에서 `보스반지름 + 플레이어반지름` 안쪽으로 들어올 수 없다.**
발사 원점을 `CapsuleRadius * 0.6` 으로 잡으면 검기는 **몸통 안에서 출발해 캡슐 표면을 뚫고 나간다.** 따라서 밀착한 플레이어도 정상적으로 통과 경로에 들어온다.
(원점을 캡슐 밖 `CapsuleRadius + α` 로 잡으면 밀착 플레이어가 원점보다 뒤에 놓여 **영구 안전지대**가 생긴다 — 근접판이 겪었던 "부채꼴이 몸통 안에 갇히는" 버그의 거울상이다.)

#### D. 지형 정지를 오버랩이 아니라 선분 트레이스로 하는 이유 ★

| 방식 | 문제 |
|---|---|
| 캡슐 오버랩으로 `WorldStatic` 감지 | 검기는 지면 +100uu 를 나는데 **대각 칼날의 아래쪽 끝이 지면 밑으로 내려간다**(17.8-E) → **바닥과 상시 오버랩** → 스폰 즉시 소멸 |
| PMC 의 `bSweepCollision` | Block 응답이 필요한데 `ECC_Projectile` 프로파일은 전부 Overlap 이다. 프로파일을 바꾸면 다른 투사체에 영향 |
| ★**프레임 간 수평 선분 트레이스** | **고도가 고정이라 선분이 수평**이다 → 바닥과 절대 교차하지 않고 **수직 지오메트리(벽·열차)만** 잡는다. 빠른 이동에서도 터널링이 없다 |

- 트레이스는 **칼날의 기울기와 무관**하다 — 액터 중심점의 이동 궤적만 보므로 1타/2타가 동일하게 동작한다
- 배리어는 `ECC_Pawn` 만 Block 이고 나머지는 Ignore 이므로(`DRS2Barrier.cpp:20-22`) **Visibility 트레이스에 안 걸린다** = 검기가 통과한다. §1.5 의 "배리어는 투사체를 통과시킨다" 사양과 일치
- ★대각 칼날은 **중심선보다 옆으로 150uu 씩 삐져나온다.** 중심선이 벽 모서리를 아슬아슬하게 비켜 가면 **칼날 끝이 벽을 뚫고 지나가 보인다.** 허용 오차로 두되, 거슬리면 트레이스를 `SweepSingleByChannel(구 반경 100)` 으로 바꾼다 (17.14)

#### E. ★대각 칼날의 실제 판정 크기 — 숫자로 확인

`HalfHeight = 200`, `Radius = 50`, 45° 기울기일 때 (캡슐의 `HalfHeight` 는 **반구 캡을 포함**한다):

```
원통부 반길이 = HalfHeight - Radius = 150
축 방향 45° 성분 = 150 × cos45° ≈ 106
좌우 반폭 = 106 + Radius = 156        →  ★총 폭 ≈ 312uu
상하 반높이 = 106 + Radius = 156      →  ★총 높이 ≈ 312uu
진행방향 두께 = Radius × 2 = 100uu
```

즉 **바운딩 박스는 312 × 312 로 기존 초안의 "폭 320" 과 사실상 같고**, 그 안에서 **대각선 띠만 판정**된다는 점이 다르다. 발사 고도 100 기준 칼날은 **지면 −56 ~ +256** 범위를 지나므로:
- **아래쪽 끝이 살짝 땅에 잠긴다** → 바닥을 긁는 연출로 자연스럽고, 판정상 바닥은 무시된다(17.7 `BeginPlay`)
- **위쪽 끝(+256)은 플레이어 키(≈176)보다 높다** → 위로는 못 피한다 (점프 회피 불가)

#### F. ★1타 `/` · 2타 `\` 가 만드는 회피 구조 — 이 변경의 핵심 재미

칼날이 대각선이면 **좌우 회피의 유불리가 비대칭**이 된다. `/` 칼날(오른쪽이 올라가는 방향) 기준:

```
      z                     칼날 `/`                   ┌ 플레이어 키 176
  256 ┤                              ╱                 │
      │                          ╱                     │
  176 ┼─────────────────────╱───────────────  ← 이 위로는 칼날이 지나가도 안 맞는다
      │                 ╱      ┌──┐                    │
  100 ┤  칼날 중심 ╱          │  │ ← 오른쪽으로 피한 플레이어: 칼날이 머리 위로 지나감 = 회피 O
      │      ╱                └──┘
    0 ┼──╱────────────────────────────────────  지면
      │╱   ┌──┐
  -56 ┘    │  │ ← 왼쪽으로 피한 플레이어: 칼날이 다리 높이로 지나감 = 회피 X
           └──┘
      ←─────────  y (좌)          (우)  ─────────→
```

기본 수치에서의 대략적인 유효 회피 폭 (플레이어 캡슐 0~176 기준):

| 칼날 | 왼쪽으로 회피 | 오른쪽으로 회피 |
|---|---|---|
| 1타 `/` | **약 ±160 까지 맞는다** (칼날이 낮게 깔림) | ★**약 110 밖이면 회피 성공** (칼날이 머리 위) |
| 2타 `\` | ★**약 110 밖이면 회피 성공** | **약 160 까지 맞는다** |

→ ★**1타를 오른쪽으로 피한 자리가 2타의 정타 자리**가 된다. **한 방향으로만 계속 피하면 반드시 2타에 맞고, 1타와 2타 사이에 방향을 바꿔야 둘 다 피한다.**
0.5초(노티파이 0.45s → 0.95s) 안에 좌우를 바꿔야 하므로 **"검기를 보고 반응하는" 플레이**가 성립한다.

> 위 숫자는 `HalfHeight 200 / Radius 50 / MuzzleHeight 100` 에서 나온 **파생값**이다. 셋 중 하나라도 바꾸면 **표를 다시 계산해야 한다.**
> - `MuzzleHeight` ↑ → 위쪽 사각이 넓어지고 아래쪽은 "무조건 맞음"이 된다
> - `HalfHeight` ↑ → 전체 폭이 넓어져 좌우 회피 자체가 어려워진다
> - **비대칭을 없애고 싶다면** 롤을 ±45 대신 0/90 으로 두면 되지만, 그러면 확정 H 가 깨진다

#### G. 중복 타격 방지가 필요한 이유

무제한 관통이라 검기는 **대상을 지나쳐도 계속 산다.** 오버랩 이벤트는 컴포넌트가 겹칠 때마다 오므로, 플레이어가 칼날 옆면을 스치듯 들락거리면 `BeginOverlap` 이 여러 번 발생할 수 있다. `HitActors` TSet 이 **한 검기당 한 대상 1회**를 보장한다.

### 17.9 기존 파일 수정 — `UDRS2MoleClawAttack`

**헤더 변경** (필드 교체, 나머지 유지):

| 필드 | 처리 |
|---|---|
| `ClawReach`, `SweepAngle`, `VerticalTolerance` | ★`bUseMeleeSweep == true` 일 때만 쓰이는 레거시로 강등 (카테고리 `MoleBoss\|Claw (Legacy)`) |
| `Hit1Damage`, `Hit2Damage` (`FScalableFloat`) | ★**그대로** — 커브 행·`OnGiveAbility` 검증 로그까지 전부 유지 |
| `GetEffectiveSweepRadius()` | 유지 (레거시 경로 + 디버그용) |
| **신규** `TSubclassOf<ADRS2MoleSlashWave> SlashWaveClass` | ★필수. 미지정 시 `OnGiveAbility` 에서 Error 로그 (§15.3 방어 코드 관례) |
| **신규** `float WaveSpeed = 1200.f` | |
| **신규** `float WaveMaxRange = 1400.f` | |
| **신규** `float MuzzleForwardRatio = 0.6f` / `float MuzzleHeight = 100.f` | ★`MuzzleHeight` 는 대각선 회피 구조에 직결된다 (17.8-F) |
| **신규** `float MaxAimYawFromForward = 25.f` | 0 = 순수 전방, 180 = 완전 조준 |
| ★**신규** `TArray<float> SlashRollAngles = { 45.f, -45.f }` | **인덱스 = HitIndex.** 1타 `/`, 2타 `\`. 배열이 짧으면 마지막 값으로 폴백, 비어 있으면 0(수평) |
| **신규** `int32 WavesPerHit = 1` / `float FanSpreadAngle = 20.f` | ★2발 이상일 때만 부채 분산. **기본 1이라 분산 코드는 잠들어 있다** |
| **신규** `bool bUseMeleeSweep = false` | 롤백 스위치 (17.6-F) |

> ★**`SlashRollAngles` 를 배열로 두는 이유**: 3타 이상으로 늘리거나 "1타만 대각, 2타는 수평" 같은 변형을 **코드 수정 없이** 시험할 수 있다. 값 셋(`{45,-45}` / `{-45,45}` / `{30,-30}`)을 바꿔가며 회피 난이도를 조율하는 것이 17.11 의 3순위 조정 손잡이다.

**`PerformClawSweep` 본문**:

```cpp
void UDRS2MoleClawAttack::PerformClawSweep(int32 HitIndex)
{
    AActor* Avatar = GetAvatarActorFromActorInfo();
    if (!IsValid(Avatar) || !Avatar->HasAuthority()) return;      // ★서버 전용 (기존과 동일)

    if (bUseMeleeSweep) { PerformMeleeSweep_Legacy(HitIndex); }   // 롤백 경로
    else                { FireSlashWaves(HitIndex); }             // ★신규 기본 경로

    // 물 보상 감소는 "공격 1회"당 1번 (기존 로직 그대로)
    if (HitIndex == 0)
    {
        if (ADREnemy* Enemy = Cast<ADREnemy>(Avatar)) Enemy->OnAttackExecuted();
    }
}

/** ★HitIndex 로 대각선 방향을 고른다. 범위를 벗어나면 마지막 값, 비어 있으면 수평(0). */
float UDRS2MoleClawAttack::ResolveSlashRoll(int32 HitIndex) const
{
    if (SlashRollAngles.Num() == 0) return 0.f;
    return SlashRollAngles[FMath::Clamp(HitIndex, 0, SlashRollAngles.Num() - 1)];
}

void UDRS2MoleClawAttack::FireSlashWaves(int32 HitIndex)
{
    if (!SlashWaveClass)
    {
        UE_LOG(LogDR, Error, TEXT("[MoleBoss] SlashWaveClass 미지정 — 검기가 발사되지 않습니다."));
        return;
    }

    const FVector Muzzle    = ResolveMuzzleLocation();          // 17.8-A
    const float   FireYaw   = ResolveFireYaw(Muzzle);           // 17.8-B (Yaw 만 계산)
    const float   Roll      = ResolveSlashRoll(HitIndex);       // ★1타 +45 / 2타 -45
    const float   HitDamage = ResolveDamage(HitIndex);          // ★기존 함수 그대로 (구간 축)

    const int32 Count = FMath::Max(1, WavesPerHit);
    for (int32 i = 0; i < Count; ++i)
    {
        float Yaw = FireYaw;
        if (Count > 1)
        {
            Yaw += FanSpreadAngle * (static_cast<float>(i) / (Count - 1) - 0.5f);
        }

        // ★Pitch = 0 고정, Roll = 대각 기울기 (17.8-B)
        const FRotator  Aim(0.f, Yaw, Roll);
        const FTransform SpawnTM(Aim.Quaternion(), Muzzle);

        ADRS2MoleSlashWave* Wave = GetWorld()->SpawnActorDeferred<ADRS2MoleSlashWave>(
            SlashWaveClass, SpawnTM, GetOwningActorFromActorInfo(),
            Cast<APawn>(GetOwningActorFromActorInfo()),
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
        if (!Wave) continue;

        Wave->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();
        Wave->DamageEffectParams.BaseDamage = HitDamage;       // ★구간/타수별 값으로 덮어쓴다
        Wave->InitWave(WaveSpeed, WaveMaxRange);
        Wave->FinishSpawning(SpawnTM);
    }
}
```

> ★**`FRotator` 구성 순서 주의**: `FRotator(Pitch, Yaw, Roll)` 이다. `FRotator(0, Yaw, Roll)` 을 `FRotator(0, Roll, Yaw)` 로 쓰면 **검기가 엉뚱한 방향으로 날아가면서 수평으로 눕는다** — 증상이 "대각선이 안 나온다"로 보여 원인을 엉뚱한 곳에서 찾게 된다.
> **`MakeDamageEffectParamsFromClassDefaults()` 를 타깃 없이 부른다**: 근접판은 대상별로 `(Target)` 을 넘겨 `TargetAbilitySystemComponent` 를 채웠지만, 투사체는 **맞는 순간에** 채운다(`DRProjectile.cpp:113`). `UDRProjectileSpell::SpawnProjectile` 과 동일한 관례다.
> **디버프 설정은 자동으로 따라온다** — 확률·데미지·지속시간이 `DamageEffectParams` 안에 들어 있고 `ExecCalc_Damage` 가 읽는다.

### 17.10 네트워크

| 항목 | 처리 |
|---|---|
| 발사 권한 | **서버 전용** — GA 가 `ServerOnly`, `PerformClawSweep` 에 `HasAuthority()` 가드 (기존 유지) |
| 검기 액터 | `bReplicates` + `SetReplicateMovement(true)` (부모 제공) → 클라는 복제된 액터를 본다 |
| ★**대각 기울기(Roll)** | **스폰 회전에 들어 있으므로 복제된 초기 트랜스폼에 그대로 실려 간다.** 별도 복제 프로퍼티가 필요 없다 |
| 데미지 | 서버에서만 `ApplyDamageEffect` |
| 클라 예측 | **없음** — 적 공격이라 필요 없다 |
| 클라 소멸 연출 | 부모 `Destroyed()` 의 `!bHit && !HasAuthority()` 분기가 임팩트를 재생 |
| 몽타주 | ★**변경 없음** — 기존 §16.5-d 의 Multicast 보완책이 그대로 유효 |
| ★유의 | 클라에서 **`bRotationFollowsVelocity` 가 켜져 있으면 클라에서만 칼날이 수평으로 돌아온다**(이동 복제 보간이 회전을 다시 계산). 서버는 정상이라 **"내 화면엔 수평인데 대각선 판정으로 맞는"** 최악의 증상이 된다 → 17.13-V45 를 **반드시 클라이언트에서도** 확인한다 |

### 17.11 데이터 · 밸런스

**커브 테이블 변경: 없음.** `Abilities.MoleBoss.Claw.Hit1/.Hit2` 를 그대로 쓴다 (§8.2-a).

| 구간 | 1타 `/` | 2타 `\` | 합 | 비고 |
|---|---|---|---|---|
| 1차 | 20 | 30 | 50 | 현행 유지 |
| 2차 | 26 | 39 | 65 | 현행 유지 |
| 3차 | 32 | 48 | 80 | 현행 유지 |

> ★**2타가 1타보다 아픈 것이 이제 의미를 갖는다.** 1타는 "회피 방향을 유도하는 견제", 2타는 "잘못 피하면 크게 맞는 본타"가 된다 (17.8-F). 재조정 시 **이 비율(20:30)을 뒤집지 않는 편이 좋다.**

**★재조정 가이드** (확정 F — 1차 플레이테스트 후): 근접판은 사실상 명중률이 0 에 가까웠으므로 **이 값들은 실전 검증된 적이 없다.** 조정 순서:

1. **BT `Wait` 간격**(1.8초) — 빈도가 가장 크게 체감된다
2. **`WaveSpeed`** — 반응 시간. 1200 → 900 이면 눈에 띄게 쉬워진다
3. ★**`SlashRollAngles` / `MuzzleHeight` / 캡슐 치수** — **회피 난이도의 본체.** 롤을 ±30 으로 낮추면 칼날이 눕고(=좌우 회피가 어려워짐), ±60 으로 올리면 서고(=좌우 회피가 쉬워짐). `MuzzleHeight` 를 올리면 위쪽 사각이 넓어진다 (17.8-F 표를 다시 계산할 것)
4. **커브 값** — 마지막에 손댄다. 구간 스케일 비율(20:26:32)을 깨지 않도록 **비례 축소**

> ★**관통 수는 조정 손잡이가 아니다** — 확정 B 로 무제한이며, 4인이 일렬로 서면 전원이 맞는 것이 사양이다. 총 피해가 과하다면 **커브 값(4순위)** 으로 내린다.

§8.2-c 의 고정값 표는 다음으로 대체된다:

| 항목 | 기존 | ★변경 |
|---|---|---|
| 기본공격 사거리 / 각도 | 200uu(2m) / 120° 부채꼴 | **1400uu 직선 / 45° 대각 칼날 (바운딩 312×312) / 무제한 관통** |
| 융기 반경 / 전기장 / 띄우기 / 대상 | — | **전부 변경 없음** |

### 17.12 에디터 작업 — [E11] ~ [E14]

#### E11. `BP_Mole_SlashWave` (신규)
부모 **`DRS2MoleSlashWave`** → `Content/Blueprints/AbilitySystem/Enemy/EliteMole/Abilities/`
(DragonFly 의 `BP_Lazer` 가 같은 성격의 선례다 — 참고용으로 열어 보면 좋다)

| 항목 | 값 |
|---|---|
| `BladeCapsule` Capsule Radius | **50** |
| `BladeCapsule` Capsule Half Height | **200** |
| `BladeCapsule` **Relative Rotation** | ★**Roll 90 (C++ 기본값) — 건드리지 말 것.** 기울기는 스폰 회전이 준다 (17.6-C) |
| `BladeCapsule` Collision | **Query Only** · Pawn = Overlap, 나머지 Ignore (C++ 설정, 확인만) |
| `Sphere` (루트) | ★**Collision 을 켜지 말 것** — C++ `BeginPlay` 가 끈다. 켜면 대각선의 사각이 메워진다 |
| `ProjectileMovement → b Rotation Follows Velocity` | ★**반드시 체크 해제** (기본값 false). 체크되면 대각선이 사라진다 (17.6-C) |
| 메시 / 나이아가라 | ★**에셋 원본은 수평 참격 하나만** 만든다. **게임에서는 액터 롤에 의해 `/` `\` 로 기울어 보인다** — 에셋 자체를 기울여 만들면 이중 회전이 되어 90°(수직)가 된다 |
| 메시 정렬 | 칼날의 긴 축이 **액터 로컬 Y(좌우)** 를 향하도록. `BladeCapsule` 과 겹쳐 보며 맞춘다 → ★이렇게 맞춰 두면 **이펙트와 판정이 영원히 같이 기운다** |
| `Impact Effect` / `Impact Sound` | 벽 타격 연출 |
| `Looping Sound` | 비행음 (선택) |
| `Max Pierce Count` | ★**0 (무제한)** |
| `b Stop On World Geometry` | ★**체크** |
| `On Pierce Visual` (이벤트) | 피격 스파크 (선택) — 무제한 관통이라 여러 번 불릴 수 있다 |

> ★`Life Span`(부모, EditDefaultsOnly)은 **건드리지 않는다.** `InitWave` 가 사거리÷속도로 덮어쓴다.
> ★**캡슐 치수를 바꾸면 17.8-F 의 회피 표가 무효**가 된다. 크기 조정은 밸런스 변경이지 연출 조정이 아니다.

#### E12. `GA_Mole_Claw` — 클래스 디폴트만 수정 (★그래프 무수정)

| 프로퍼티 | 값 |
|---|---|
| `Slash Wave Class` | ★**`BP_Mole_SlashWave`** — 비우면 Error 로그 + 무공격 |
| `Wave Speed` | **1200** |
| `Wave Max Range` | **1400** |
| `Muzzle Forward Ratio` | 0.6 |
| `Muzzle Height` | **100** |
| `Max Aim Yaw From Forward` | **25** |
| ★`Slash Roll Angles` | **[0] = 45 · [1] = −45** ← 1타 `/`, 2타 `\`. **부호가 반대로 보이면 두 값을 맞바꾼다** |
| `Waves Per Hit` | **1** |
| `Fan Spread Angle` | 20 (`WavesPerHit = 1` 이면 무시됨) |
| `b Use Melee Sweep` | ★**체크 해제** (체크하면 근접 부채꼴로 되돌아간다) |
| `Hit1Damage` / `Hit2Damage` | ★**변경 금지** — 커브 행 그대로 |
| Asset Tag / Damage Effect Class / Damage Type | ★**변경 금지** |

#### E13. `BT_S2MoleBoss` — 거리 데코레이터 (★필수)

```
├ Sequence   [Decorator: Distance To (TargetToFollow) <= 200]      ← 기존
                                                    ↓
├ Sequence   [Decorator: Distance To (TargetToFollow) <= 1300]     ← ★변경
   ├ BTT_S2MoleBoss_Claw
   └ Wait 1.8                                                     ← ★1.0 에서 상향 (확정 G)
```

- **1300 = 검기 사거리 1400 − 100(여유)**. 사거리 끝에서 발사해 도달 전에 소멸하는 헛발질을 막는다
- ★`WaveMaxRange` 를 바꾸면 **이 숫자도 같이 바꿔야 한다** (BT 는 C++ 값을 모른다). 조정이 잦을 것 같으면 `BTS_S2MoleBoss_Target` 에서 사거리를 읽어 Bool 키 `bInClawRange` 로 넘기는 방식이 안전하다 (§16.8-d 의 대안과 동일)
- 최소 사거리 데코레이터는 **넣지 않는다** (17.8-C — 근접 사각지대가 없다)
- ★**1타만 쏘고 태스크가 끝나지 않도록** 주의: `BTT_S2MoleBoss_Claw` 는 즉시 Success 하고 몽타주가 계속 돌며 2타를 쏘는 구조다. `Wait 1.8` 이 **몽타주 길이 1.4초보다 길어야** 2타가 잘리지 않는다

#### E14. 몽타주 `AM_MoleBoss_Claw` — ★연출을 대각선에 맞춘다

노티파이 위치·태그·개수는 **그대로여도 동작**하지만, 대각선 방향과 팔 궤적이 어긋나면 "왜 저기서 저게 나오지" 하는 위화감이 크다.

| 타 | 검기 | 권장 애니메이션 궤적 |
|---|---|---|
| 1타 (`Montage.Attack.1`, ≈0.45s) | `/` (Roll +45) | **왼쪽 아래 → 오른쪽 위** 로 올려 베기 |
| 2타 (`Montage.Attack.2`, ≈0.95s) | `\` (Roll −45) | **오른쪽 위 → 왼쪽 아래** 로 내려 베기 |

- 노티파이는 팔이 **최고 속도로 지나가는 프레임**에 맞춘다
- 무기 궤적 트레일이 있다면 검기 기울기와 같은 방향으로
- ★애니메이션 방향이 반대로 만들어졌다면 **애니메이션을 고치지 말고 `Slash Roll Angles` 두 값을 맞바꾼다** (E12) — 훨씬 싸다

### 17.13 검증 시나리오 (V27~) — §16.11 에 이어서

| # | 절차 | 기대 | 실패 시 |
|---|---|---|---|
| V27 | 보스 정면 **10m** 에 서 있기 | 검기 2발이 날아와 **맞는다** (기존엔 아무 일도 안 일어났다) | `Slash Wave Class` / BT 거리 데코레이터 |
| V28 | 보스 정면 **밀착** | ★**맞는다** — 근접 사각지대 없음 | `Muzzle Forward Ratio` 를 낮춘다 (17.8-C) |
| V29 | 보스 **바로 옆(90°)** | **안 맞는다** | 조준 클램프가 너무 넓음 |
| V30 | 보스 **등 뒤** | **안 맞는다** · 크래시 없음 | |
| ★V45 | **1타/2타를 옆에서 관찰** (서버 화면) | 1타는 `/`, 2타는 `\` 로 **서로 반대 기울기** | 기울기가 아예 없으면 → ★`b Rotation Follows Velocity` 체크 여부 / `FRotator` 인자 순서 (17.9) |
| ★V46 | **V45 를 원격 클라이언트에서 재확인** | 클라에서도 **동일한 기울기** | 클라만 수평이면 `b Rotation Follows Velocity` (17.10) |
| ★V47 | 1타를 **오른쪽으로** 피하고 **그 자리에 그대로 서 있기** | ★**2타에 맞는다** (`\` 의 정타 위치) | 안 맞으면 롤 부호가 두 타 모두 같은 방향 — `Slash Roll Angles` 확인 |
| ★V48 | 1타를 오른쪽, 2타를 **왼쪽으로** 피하기 | ★**둘 다 회피 성공** | 다 맞으면 칼날이 너무 큼 → `HalfHeight` 하향 |
| ★V49 | 칼날의 **높은 쪽 끝** 방향으로 멀찍이 서기 (1타 기준 오른쪽 ~150uu) | **머리 위로 지나간다 = 회피** | 맞으면 `MuzzleHeight` 가 너무 낮거나 캡슐이 큼 (17.8-F) |
| ★V50 | 칼날의 **낮은 쪽 끝** 방향으로 서기 (1타 기준 왼쪽 ~150uu) | **맞는다** (다리 높이로 깔림) | 안 맞으면 `MuzzleHeight` 과다 |
| ★V51 | **점프해서 회피 시도** | **맞는다** — 위로는 못 피한다 (칼날 상단 ≈ +256) | |
| V31 | 발사 직후 **옆으로 구르기** | **회피 성공** | `WaveSpeed` 하향 |
| V32 | **1타 맞고 2타 전에 이탈** | 2타는 안 맞는다 (§2.2 "독립 판정" 유지) | |
| ★V33 | **4인이 일렬로** 정면에 서기 | ★**전원 피격** (무제한 관통, 확정 B) | 앞사람만 맞으면 `Max Pierce Count` 가 0 이 아님 |
| V34 | 한 명이 칼날 옆면을 스치며 들락거리기 | **1회만** 피해 | `HitActors` 중복 방지 (17.8-G) |
| V35 | 보스와 플레이어 사이에 **벽/열차** | ★**검기가 벽에서 소멸** + 임팩트 연출 (확정 B/D) | Tick 트레이스 (17.8-D) |
| V36 | **배리어** 너머 플레이어 | 검기가 배리어를 **통과** (§1.5 사양) | 배리어 BP 의 Visibility 응답 확인 |
| ★V37 | **경사로/난간 위**에서 발사 | 스폰 즉시 소멸하지 **않는다** (칼날 아래끝이 땅에 잠겨도 무해) | 루트 `Sphere` 의 Collision 이 켜져 있음 (17.7 `BeginPlay`) |
| V38 | 구간 1→2→3 진행하며 데미지 측정 | **20/30 → 26/39 → 32/48** (V15 와 동일 기대치) | `ResolveDamage` 무수정 확인 |
| V39 | 검기 비행 중 **보스 사망/도망(구간 전환)** | 날아간 검기는 끝까지 날아가 피해를 준다(허용) · **크래시 없음** | `SourceASC` 널 가드 |
| V40 | **잠수 중** BT 가 발톱을 시도 | 발사되지 않는다 (`State.MoleBoss.Burrowed` 차단 — 기존 동작) | |
| V41 | 원격 클라이언트 관전 | 검기가 **보인다** · 서버 판정과 체감 오차 수용 범위 | `WaveSpeed` 하향 |
| V42 | `b Use Melee Sweep` 체크 후 PIE | ★**근접 부채꼴로 되돌아간다** | 롤백 경로 생존 확인 |
| V43 | `Slash Wave Class` 비우고 PIE | Error 로그 1줄 + **크래시 없음** | |
| V44 | **스테이지1 회귀** | 파이어볼트·씨앗캐논 등 정상 (`ADRProjectile` 무수정) | |

> ★**V45~V51 은 이번 개정(대각선)의 핵심 검증**이다. V45 가 실패하면 나머지 대각선 시나리오는 전부 무의미하므로 **가장 먼저** 한다.
> V45 검증 팁: PIE 를 일시정지(`Pause`)하고 검기 액터를 선택하면 캡슐 기즈모로 기울기를 직접 볼 수 있다. 또는 `BladeCapsule` 의 `Hidden In Game` 을 임시로 해제한다.

### 17.14 리스크와 대응

| 리스크 | 확률 | 영향 | 대응 |
|---|---|---|---|
| ★**롤이 매 프레임 지워져 칼날이 수평이 됨** | 중 | **대각선 사양 전체 무효** | `b Rotation Follows Velocity` = false (BP + `InitWave` 양쪽에서 보장) · V45/V46 |
| ★**메시를 이미 기울여 만들어 이중 회전(90°)** | 중 | 칼날이 수직으로 섬 | 에셋은 **수평**으로 만든다 (17.12-E11) |
| ★**롤 부호가 예상과 반대** | 높음 | 1타/2타 방향이 뒤바뀜 | 코드가 아니라 `Slash Roll Angles` 두 값을 **맞바꾼다** (E12) — 문서가 UE 롤 부호를 단정하지 않는 이유 |
| **바닥/경사로에 걸려 스폰 즉시 소멸** | 중 | 공격 전무 | 루트 `Sphere` Collision Off (17.7 `BeginPlay`) + V37 |
| ★**칼날 끝이 벽을 뚫고 보임** | 중 | 시각적 위화감 | 중심선 트레이스라 발생 가능 (17.8-D). 거슬리면 `SweepSingleByChannel(반경 100)` 으로 승격 |
| **지형 정지가 빡빡해 작은 소품에도 막힘** | 중 | 공격 무력화 | Visibility 채널이라 소품도 잡는다 → 전용 트레이스 채널 신설 또는 소품 콜리전 조정 |
| ★**무제한 관통으로 4인 총 피해 과다** | 중 | 밸런스 | 확정 B 라 관통은 유지 — **커브 값**으로 내린다 (17.11-4순위) |
| 원거리화로 **체감 난이도 급상승** | 중 | 밸런스 붕괴 | 17.11 의 조정 순서 |
| 조준 클램프가 좁아 **여전히 안 맞음** | 중 | 변경 효과 없음 | `MaxAimYawFromForward` 를 45~90 으로 올려 재테스트. **회전 속도 4.0(§2.1)과 한 세트로 조정** |
| ★**대각선 때문에 회피가 너무 어렵/쉬움** | 중 | 재미 저하 | 롤 각도(±30 ~ ±60) → `MuzzleHeight` → 캡슐 치수 순으로 (17.11-3순위) |
| `GA_Mole_Claw` 그래프를 실수로 건드림 | 저 | 전체 무공격 | ★그래프는 **열지 않는다.** 클래스 디폴트 패널만 수정 |
| 검기가 **좌석 위 플레이어**를 노려 그림이 이상함 | 저 | 연출 | Pitch 0 고정이라 좌석 높이로 올라가지 않는다 (17.8-B). 착석자를 조준 후보에서 빼려면 스킬1 의 `bExcludeSeatedPlayers` 와 같은 필터를 조준 함수에 추가 |
| 성능 | 저 | — | 수명 1.2초 · 발사 간격 1.8초라 동시 존재 수 ≤ 2 |

### 17.15 작업 순서 체크리스트

```
[ ] 1.  ADRS2MoleSlashWave.h/.cpp 신규 작성                        (17.7)
        - BladeCapsule (Radius 50 / HalfHeight 200 / 상대 Roll 90)
        - MaxPierceCount 기본 0 (무제한) · bStopOnWorldGeometry 기본 true
        - BeginPlay 에서 루트 Sphere Collision Off
        - InitWave 에서 bRotationFollowsVelocity = false
[ ] 2.  DRS2MoleClawAttack.h 필드 개편                             (17.9)
        - 검기 프로퍼티 추가 (★SlashRollAngles = {45, -45})
        - 부채꼴 필드는 Legacy 로 강등
[ ] 3.  DRS2MoleClawAttack.cpp
        - 기존 본문을 PerformMeleeSweep_Legacy() 로 이동 (삭제 아님)
        - FireSlashWaves / ResolveMuzzleLocation / ResolveFireYaw / ResolveSlashRoll 추가
        - ★FRotator(0, Yaw, Roll) 인자 순서 확인
        - ★ResolveDamage · OnAttackExecuted · OnGiveAbility 검증 로그는 그대로 둔다
[ ] 4.  OnGiveAbility 에 SlashWaveClass 널 체크 Error 로그 추가      (§15.3 관례)
[ ] 5.  컴파일 → 에디터 재시작
[ ] 6.  E11  BP_Mole_SlashWave 생성 (★수평 메시 · 캡슐 치수 · 관통 0)
[ ] 7.  E12  GA_Mole_Claw 클래스 디폴트 재설정  ★그래프 무수정
[ ] 8.  E13  BT_S2MoleBoss 거리 200 → 1300, Wait 1.0 → 1.8
[ ] 9.  ★V45 먼저 검증 (대각선이 실제로 나오는가) → 실패 시 여기서 멈추고 원인 제거
[ ] 10. V27~V51 전체 검증
[ ] 11. E14 몽타주 궤적을 대각선 방향에 맞춰 재저작 (연출)
[ ] 12. 밸런스 튜닝 (17.11 순서)
[ ] 13. (다음 마일스톤) 레거시 부채꼴 경로 + bUseMeleeSweep 제거,
        원한다면 클래스명 ClawAttack → SlashWave 리네임 (CoreRedirects 필요)
```

### 17.16 이 변경으로 갱신해야 할 문서 항목 ★

구현 완료 후 아래를 수정한다. **지금은 §17 이 최신 사양**이며, 아래 절들은 근접판 기준으로 남아 있다:

| 절 | 현재 내용 | 갱신 방향 |
|---|---|---|
| §0 요약 | "전방 부채꼴 2타" | "전방 45° 대각 관통 검기 2발 (`/` + `\`)" |
| §2.2 | 기본공격 사양표 전체 | §17.5 로 대체 |
| §2.5 | 확정 사항 A~D | ★확정 A~H (§17.2) 를 이어붙인다 |
| §4.2 | `UDRS2MoleClawAttack` 코드 스케치 | §17.9 로 대체 |
| §8.2-c | "발톱 사거리/각도 200uu / 120°" | "1400uu 직선 / 45° 대각 칼날 312×312 / 무제한 관통" |
| §8.3 | 발톱 BT 재시도 간격 1.0s | 1.8s |
| §9.1 | 신규 BP 목록 | `BP_Mole_SlashWave` 추가 |
| §16.4 E4-1 | `Sweep Radius/Angle` 표 (★이미 코드와 어긋나 있었음) | §17.12-E12 로 대체 |
| §16.5 | 몽타주 저작 요건 | ★1타/2타 궤적을 좌우 반대로 (§17.12-E14) |
| §16.8-d | BT 거리 데코레이터 `<= 200` | `<= 1300` |
| §16.12 실수 표 | — | ★"대각선이 안 나옴 → `b Rotation Follows Velocity`" 행 추가 |
| §16.13 체크리스트 | — | E11~E14 · V27~V51 추가 |

### 17.17 구현 결과 — C++ 전량 완료 (2026-09-09) ★

`Build.bat DaeRuneEditor Win64 Development` **성공**. 신규 2파일 컴파일 + 링크 확인 (경고는 전부 기존 파일의 UE5.5 deprecation, 이번 변경과 무관).

#### 17.17.1 생성 / 수정 결과

| 파일 | 상태 | 내용 |
|---|---|---|
| `Public/Actor/Stage2/DRS2MoleSlashWave.h` | ★신규 | 대각 검기 액터 |
| `Private/Actor/Stage2/DRS2MoleSlashWave.cpp` | ★신규 | 관통 · 지형 정지 · 수명 |
| `Public/AbilitySystem/Abilities/Stage2/DRS2MoleClawAttack.h` | ★수정 | 검기 프로퍼티 추가 · 부채꼴 필드 Legacy 강등 |
| `Private/.../DRS2MoleClawAttack.cpp` | ★수정 | `FireSlashWaves` 신설 · 기존 본문은 `PerformMeleeSweep_Legacy` 로 보존 |
| 그 외 | **무수정** | 보스 본체 · 페이즈 · `ADRProjectile` · 태그 · 커브 — 계획대로 한 줄도 안 건드렸다 |

#### 17.17.2 계획서 코드 스케치와 달라진 지점 ★

구현 중 발견해 고친 것들. **스케치(§17.7 / §17.9)보다 이쪽이 실제 코드다.**

**① ★`SetLifeSpan` 이 부모 `BeginPlay` 에 덮어써지는 문제 (실제 버그였다)**

스케치는 `InitWave` 에서 `SetLifeSpan(사거리/속도)` 를 부르고 끝냈다. 그런데 호출 순서가

```
SpawnActorDeferred → (생성자) → InitWave [수명 1.17초 설정] → FinishSpawning → BeginPlay
                                                                                  └ Super::BeginPlay()
                                                                                     └ SetLifeSpan(LifeSpan=15초)  ★덮어쓴다
```

라서 **검기가 사거리를 넘어 15초간 날아간다.** 부모의 `LifeSpan` 은 private 이라 상속으로 못 막는다.
→ `PendingLifeSpan` 에 저장해 두고 **`BeginPlay` 의 `Super` 호출 뒤에 다시 적용**한다.
(`ADRVacuumAirProjectile` 이 `SetLifeSpan(0.f)` 로 같은 문제를 처리하는 선례가 있었다 — 그 선례를 놓쳤던 것.)

**② `ResolveFireRotation` → `ResolveFireYaw` + `ResolveSlashRoll` 분리**

스케치는 회전 하나를 통째로 만들었지만, **Yaw(조준)와 Roll(칼날 기울기)은 완전히 독립**이라 함수를 나누는 편이 맞다.
다발 발사(`WavesPerHit ≥ 2`)에서 **Yaw 만 분산하고 Roll 은 유지**해야 하는데, 합쳐 두면 부채 분산 코드가 롤까지 건드릴 여지가 생긴다.

**③ ★넉백 방향에서 Roll 을 제거했다 (스케치에 없던 버그)**

부모 `ADRProjectile::OnSphereOverlap` 은 넉백 벡터를 `GetActorRotation()` 에서 만든다. 그대로 베끼면
**칼날 기울기(±45)가 넉백 방향에 섞여** 1타와 2타가 서로 다른 쪽으로 플레이어를 날린다.

```cpp
FRotator Rotation = GetActorRotation();
Rotation.Pitch = 45.f;
Rotation.Roll  = 0.f;   // ★칼날 기울기는 넉백 방향과 무관하다
```

> 현재 `Knockback Chance` 기본값이 0 이라 당장은 드러나지 않지만, 나중에 넉백을 켜는 순간 원인 찾기 어려운 증상이 된다.

**④ 조준 대상 결정을 `ResolveAimTarget()` 으로 분리 + 착석자 제외 구현**

확정 C("착석자는 맞기는 하되 겨누지는 않는다")를 코드로 옮겼다:
- ① `IEnemyInterface::Execute_GetCombatTarget` (AI 서비스가 잡아 둔 타깃) — **착석자면 건너뛴다**
- ② 폴백: `ADRGameStateBase::GetAlivePlayers()` 중 **착석자를 뺀** 최근접
- 프로퍼티 `bExcludeSeatedPlayersForAim` (기본 true) 로 껐다 켤 수 있다

> ★`GetCombatTarget` 은 `ICombatInterface` 가 아니라 **`IEnemyInterface`** 에 있다 (`EnemyInterface.h:31`). 스케치가 틀렸다.

**⑤ `OnGiveAbility` 검증 로그 2개 추가**

- `SlashWaveClass` 미지정 → **Error** ("기본공격이 아무것도 발사하지 않습니다")
- `SlashRollAngles` 비어 있음 → **Warning** ("검기가 수평으로 나갑니다")

무증상 실패를 부여 시점에 잡는 §15.3 방어 코드 관례를 따랐다.

**⑥ `MuzzleForwardRatio` 에 `ClampMax = 0.95`**

1.0 이상이면 발사 원점이 캡슐 밖으로 나가 **근접 영구 안전지대**가 생긴다(17.8-C). 에디터에서 아예 못 넣게 막았다.

**⑦ `bRotationFollowsVelocity = false` 를 `InitWave` 에서도 강제**

BP 체크박스 하나로 대각선 사양 전체가 사라지는 것을 막기 위해 **코드에서도 한 번 더** 끈다. BP 와 코드 양쪽에서 보장한다.

#### 17.17.3 확정 사양 → 코드 대응표 (검수용)

| 확정 | 구현 위치 |
|---|---|
| A 조준 ±25° | `MaxAimYawFromForward = 25` · `ResolveFireYaw()` 의 `FindDeltaAngleDegrees` + `Clamp` |
| B 무제한 관통 | `MaxPierceCount = 0` — `if (MaxPierceCount > 0 && ...)` 블록에 진입하지 않는다 |
| B/D 지형 정지 | `bStopOnWorldGeometry` + `Tick()` 의 수평 선분 트레이스 |
| C 착석자 | 피격 O (필터 없음) / 조준 X (`bExcludeSeatedPlayersForAim`) |
| E 근접 사각 없음 | `MuzzleForwardRatio = 0.6` · `ResolveMuzzleLocation()` |
| F 데미지 유지 | `ResolveDamage()` **무수정** — 커브 행 그대로 |
| G BT 간격 | ★**BT 에디터 작업 (E13)** — C++ 아님 |
| H 대각선 | `SlashRollAngles = {45, -45}` · `FRotator(0, Yaw, Roll)` · 캡슐 상대 Roll 90 |
| H-1 이펙트=판정 | 액터 스폰 회전으로 통째 롤 → 메시/나이아가라/캡슐이 함께 기운다 |

#### 17.17.4 ★남은 작업 — 전부 에디터 작업이다

C++ 은 끝났고, **지금 PIE 를 돌리면 검기가 나가지 않는다** (`SlashWaveClass` 가 비어 있어 Error 로그만 뜬다). 아래를 해야 동작한다:

```
[ ] E11  BP_Mole_SlashWave 생성        ← ★이것부터. 없으면 아무것도 안 나간다
[ ] E12  GA_Mole_Claw 클래스 디폴트    ← Slash Wave Class 지정 (나머지는 C++ 기본값으로 이미 맞다)
[ ] E13  BT 거리 200 → 1300, Wait 1.0 → 1.8
[ ] V45  ★대각선이 실제로 `/` `\` 로 나오는지 먼저 확인 → 실패 시 여기서 멈춘다
[ ] V27~V51 전체 검증
[ ] E14  몽타주 궤적을 대각선 방향에 맞춰 재저작 (연출, 나중에 해도 됨)
```

> ★**E12 에서 실제로 지정할 것은 `Slash Wave Class` 하나뿐이다.** 속도 1200 · 사거리 1400 · 롤 ±45 · 관통 0 · 조준 25° 는 전부 **C++ 생성자 기본값**으로 이미 들어가 있다. 값을 바꿀 때만 BP 에서 건드리면 된다.
