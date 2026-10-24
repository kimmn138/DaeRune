# Plan3 — 전 로봇 공용 업그레이드 칩 (Common Chips)

> 작성일: 2026-09-21
> **1차 개정: 2026-09-21** — 공격 선딜/후딜 칩(구 ⑥번)을 **범위에서 제외**. 칩 6종 → **5종**.
> **2차 확장: 2026-09-22** — **돌파 칩 3종(⑥⑦⑧) 추가**. 총 **8종**. → **§17**
> 대상 브랜치: `feat/upgradeskill`
>
> **문서 구조**
> - **§0 ~ §16 + 부록 A** — 기본 세트 **5종**(①~⑤). ★C++ 완료·에셋 완료·위젯 배선만 남음★
> - **§17** — 돌파 세트 **3종**(⑥~⑧). ★전부 미착수★
>   ①~⑤ 가 "수치 하나 바꾸는 칩"이라면 ⑥~⑧ 은 **전투 규칙 자체를 조건부로 갈아끼우는 칩**이라
>   필요한 배관의 성격이 다르다. §17.0 참조.
> 선행 문서: **Plan2.md** (재화 · 슬롯 · 칩 시스템). 이 문서는 Plan2 위에 **콘텐츠 1세트 + 그걸 태우기 위한 4가지 배관**을 얹는다.
> 작성 목적: 정원로봇 / 자판기 / 청소기 **어느 로봇으로 플레이해도 쓸 수 있는 공용 칩 5종**을 정의하고,
> 그중 2종을 **기본 장착 상태**로 제공하는 구조를 만든다.
>
> ⚠️ **이름 충돌 주의** — 코드 주석에 있는 `Plan3 §5.5`, `Plan3 §6.1`, `Plan3 §13.5-③` 같은 참조는
> **이 문서가 아니다.** 예전에 존재했다가 삭제된 "청소기/탑승 계획서"를 가리킨다
> (`git log --diff-filter=D -- Plan3.md` → `dd452343`). 헷갈리지 말 것.
> 이 문서를 가리키는 새 주석은 반드시 **`Plan3(공용칩) §x`** 로 쓴다.

---

## 0. 한눈에 보기

| 칩 | 효과 | 새 배관이 필요한가 | 난이도 |
|---|---|---|---|
| ① 최대 체력 +100 | `MaxHealth` Flat +100 | **신규 스탯 1개** (컨테이너 불변식 유지) | 하 |
| ② 체력·물·스킬 아이콘 UI 표시 | 피처 태그 부여 | **신규 축: 피처 태그 + HUD 스위치** | 중 |
| ③ 최대 물 +50% | `MaxWater` Percent +0.5 | **없음** (Plan2 M2 에서 이미 배선됨) | 하 |
| ④ 이동 속도 +10% | `MoveSpeed` Percent +0.1 | **없음** (〃) | 하 |
| ⑤ 자동 체력 회복 타이머 −0.5초 | `HealthRegenDelay` Flat −0.5 | **신규 스탯 1개** + PlayerState 훅 | 하 |
| **⑥ 광폭화** (2차 확장) | 주는 피해 ×2~3, 피격 시 칸 1개 소모, 회복 불가 | 주는 피해 축 + 피격/회복 분기 | **상** → §17.6 |
| **⑦ 시너지 오라** (2차 확장) | 근접 팀원과 공수 ±10% (중첩) | **신규 액터 컴포넌트** | **상** → §17.7 |
| **⑧ 피의 계약** (2차 확장) | 최대 체력 ×3, 스킬이 물 대신 체력 소모 | 코스트 분기 2곳 | 중 → §17.8 |

그리고 ①~⑤ 전부에 공통으로 필요한 것:

| 공통 배관 | 왜 필요한가 |
|---|---|
| **A. 공용 칩** (`bCommonChip`) | 지금 칩은 `OwnerClass` 하나에만 속한다. "모든 로봇이 쓸 수 있는 칩"이 표현 불가능하다. |
| **B. 기본 장착** (`DefaultEquippedChips` + `InitialFreeSlots`) | ①②는 "처음부터 끼워져 있다". 지금은 새 세이브의 슬롯이 0칸이고 장착도 0개다. |
| **C. 피처 태그** (`GrantedTags`) | ②는 수치가 아니다. 현재 칩은 `FDRUpgradeModifier`(숫자)밖에 못 준다. |
| **D. 시간 단위 표기** | ⑤가 `−0.5` 인데 UI 에 단위가 없어 비율처럼 읽힌다. "초"를 붙인다. |

> **핵심 판단**: ③④는 **코드 0줄** (카탈로그 항목만), ①⑤는 각 10줄 안쪽, **②가 유일한 UI 작업**이다.
> 배관 A·B 가 전체 작업의 절반이고, 그건 칩이 몇 개든 한 번만 깔면 된다.

### 이번 개정에서 제외된 것

**구 ⑥번 "모든 공격 선딜 −0.05초 / 후딜 −0.05초" 칩을 범위에서 뺐다.**

- 그 칩 하나가 전체 작업의 8할이었다 — 몽타주 배속 파이프라인, 노티파이 시각 측정,
  어빌리티별 타이머 보정, GA 블루프린트 7곳 재배선, 멀티 동기화 검증이 전부 그것 때문이었다.
- 빠지면서 **신규 스탯 2개(`AttackStartupDelay` / `AttackRecoveryDelay`), C++ 파일 4개,
  BP 7곳, 테스트 5항목, 리스크 4건**이 같이 빠졌다. 예상 작업량이 **5.7일 → 2.2일**로 줄었다.
- 조사해 둔 내용(선딜/후딜의 실제 구현 위치, 해결 방향, 엔진 API 확인 결과)은
  나중에 되살릴 수 있도록 **부록 A** 에 요약만 남겼다.

### 구현 현황

> 마지막 갱신: 2026-09-21 — **C++ 전량 완료(빌드 통과). 남은 작업은 전부 에디터 작업이다.**

| 마일스톤 | 상태 | 내용 |
|---|---|---|
| **N1. 배관 A·B·C·D** | ✅ 코드 완료 | 공용 칩 / 기본 장착 / 피처 태그 / 시간 표기 |
| **N2. 칩 ①③④** | ✅ 코드 완료 | ①은 `RefreshUpgradeEffects()` 분배 로직. ③④는 원래 코드가 필요 없다 |
| **N3. 칩 ②** | 🟨 코드 완료 / **위젯 미착수** | C++ 신호 경로 완성. `WBP_StageOverlay` 배선만 남음 (§7.2.4) |
| **N4. 칩 ⑤** | ✅ 코드 완료 | `GetEffectiveCombatExitDelay()` + `EnterCombat` 2곳 |
| **N5. 에셋 + 밸런스 + 테스트** | 🟨 에셋 완료 / 검증 미착수 | `DA_ChipCatalog` 5종 + `DA_ProgressionConfig` 작성됨. PIE 검증 남음 |
| **N6~N10. 돌파 칩 3종 (§17)** | ⬜ 미착수 | 공통 배관 → ⑧ → ⑥ → ⑦ → 에셋. 약 3.2일 (§17.11) |

> ⚠️ **"코드 완료"는 "컴파일된다"까지의 의미다.** `DA_ChipCatalog` 에 공용 칩 5종이 아직 없고
> `DA_ProgressionConfig.DefaultEquippedChips` 도 비어 있어 **어떤 칩도 실제로 동작한 적이 없다.**
> §12 STEP 2~4 를 끝내야 처음으로 경로가 돌아간다.

#### 구현 중 계획에 추가된 안전장치 2건

계획에 없었지만 구현하면서 필요하다고 판단해 넣은 것들이다.

| # | 문제 | 대응 |
|---|---|---|
| **S-1** | **코드가 에셋보다 먼저 들어가면 HUD 가 통째로 사라진다.** 칩 ②가 카탈로그에 없으면 아무도 피처 태그를 안 갖는데, 위젯은 그걸 "빼는 걸 선택했다"로 해석한다. 화면에서 원인을 볼 수 없어 가장 찾기 어려운 회귀가 된다. | `UDRChipCatalog::IsFeatureGated()` 신설. **"그 기능을 가두는 칩이 카탈로그에 있을 때만 잠근다."** 칩을 지웠을 때도 자동 복구된다. |
| **S-2** | 기본 칩을 꽂지 못했는데(카탈로그 미작성 / 빈 칸 없음) `bDefaultsApplied` 를 세워 버리면 **에셋을 다 만든 뒤에도 영영 안 꽂힌다.** | `ApplyDefaultLoadout()` 이 **전부 꽂았을 때만** 마커를 세운다. 실패하면 다음 실행에 재시도(변경이 없으면 저장도 안 한다). |

---

## 1. 요구사항 (원문 → 해석)

| # | 원문 | 해석 (이 문서가 구현하는 것) |
|---|---|---|
| 1 | 최대체력 100 증가 (기본으로 장착된 상태) | `MaxHealth` **총량 +100**. 컨테이너 4칸 × 100 = 400 → 500. 새 세이브에서 자동 장착. |
| 2 | 체력, 물, 스킬 아이콘 UI 표시 (기본 장착, 빼면 안 보임) | 이 칩이 **꽂혀 있을 때만** 스테이지 HUD 의 체력·물 링과 스킬 아이콘이 보인다. 빼면 숨는다. |
| 3 | 최대 물 보유량 50% 증가 | `MaxWater` × 1.5 |
| 4 | 이동속도 10% 증가 | `MoveSpeed` × 1.1 |
| 5 | 자동 체력 회복 타이머가 0.5초 빨라짐 | **전투 이탈 대기 시간**(`CombatExitDelay`, 기본 5초)이 4.5초로 줄어 회복이 0.5초 빨리 시작된다. → §7.5 (다른 해석이면 §7.5.5 의 대체안) |
| ~~6~~ | ~~모든 공격 선딜/후딜 0.05초 감소~~ | **제외됨** (1차 개정). 부록 A 참조. |

### 1.1 이 문서가 확정하는 설계 결정

| 결정 | 내용 | 근거 |
|---|---|---|
| **D-1** | 공용 칩은 **모든 로봇의 목록에 등장**하지만, **장착 상태는 로봇별로 따로** 관리한다. | 세이브가 `TMap<Class, FDRClassUpgradeState>` 구조다(Plan2 §4.2). "공용"은 *사용 가능 여부*이지 *장착 공유*가 아니다. 정원로봇에 끼운 체력칩이 자판기에도 자동으로 끼워지면, 자판기 슬롯을 해금하지 않았는데 칩이 꽂혀 있는 모순이 생긴다. |
| **D-2** | 기본 장착 칩은 **6칸 중 앞 2칸**을 실제로 점유한다. 그 2칸은 **무료로 해금된 상태**로 시작한다. | 요구사항이 "빼면 UI 가 안 보인다"라고 명시했다 = **뺄 수 있어야 한다**. 뺄 수 있으려면 일반 슬롯에 있어야 하고, 빼서 생긴 빈칸을 다른 칩에 쓸 수 있어야 트레이드오프가 성립한다. |
| **D-3** | 칩 ①은 `MaxHealth` **총량** 스탯으로 정의한다 (컨테이너당 분배는 코드가 계산). | `ContainerHealth Flat +25` 로 쓰면 컨테이너 개수가 4가 아닌 순간 "+100"이 거짓말이 된다. §7.1 |
| **D-4** | `EDRUpgradeStat` 의 새 값은 **반드시 `Count` 바로 앞**에 추가한다. 기존 값 사이에 끼워 넣지 않는다. | `ECharacterClass` 주석과 같은 규칙. 에셋에 직렬화된 값이 밀리는 사고를 막는다. |
| **D-5** | 기본 장착은 `EquipChip()` 을 **거치지 않고** `SlotChips` 에 직접 쓴다. | `EquipChip()` 은 `IsUpgradeSystemUnlocked()` 가 false 면 `SystemLocked` 로 막힌다. 기본 장착은 스테이지1 클리어 전에도 동작해야 한다. §6.3 |

---

## 2. 현재 코드 기준 사실 (각 칩이 무엇을 건드려야 하는가)

### 2.1 칩은 로봇 1대에만 속한다 — "공용"을 표현할 수단이 없다

`FDRUpgradeChipDefinition::OwnerClass` 는 **단일 값**이고, 코드 6곳이 `== CharacterClass` 로 직접 비교한다.

| # | 파일:줄 | 함수 | 비교 형태 | 빠뜨리면 생기는 일 |
|---|---|---|---|---|
| 1 | `DRChipCatalog.cpp:92` | `GetChipsForClass` | `Chip.OwnerClass == CharacterClass` | 목록에 안 보임 |
| 2 | `DRChipCatalog.cpp:145` | `ResolveLoadout` | `!=` → continue | 장착해도 **효과가 0** |
| 3 | `DRChipCatalog.cpp:169` | `SanitizeLoadout` (서버 검증) | `!Chip \|\| !=` | 서버가 **칩을 버림** → 멀티에서만 효과 없음 |
| 4 | `DRGameInstance.cpp:247` | `SanitizeClassState` (세이브 정화) | `Chip->OwnerClass == CharacterClass` | 다음 실행 때 **조용히 빠져 있음** |
| 5 | `DRGameInstance.cpp:692` | `IsChipUnlocked` | `!=` → false | 항상 잠김 |
| 6 | `DRGameInstance.cpp:719` | `CanEquipChip` | `!=` → `WrongClass` | 장착 자체가 거부됨 |

→ **여섯 곳을 전부 한 헬퍼로 바꾸지 않으면 반드시 어긋난다.** (§5)
특히 3·4번은 증상이 "나중에" 나타나서 디버깅이 가장 어렵다.

### 2.2 "기본 장착"이라는 개념이 아예 없다

- 새 세이브의 `FDRClassUpgradeState` 는 `UnlockedSlots = 0`, `SlotChips` 빈 배열, `SpentCurrency = 0`.
- `UDRGameInstance::EnsureProgressInitialized()` 가 로봇 키만 `FindOrAdd` 하고 끝난다.
- `EquipChip()` 은 `IsUpgradeSystemUnlocked()` 가 false 면 `SystemLocked` 로 막힌다. (→ D-5)

### 2.3 플레이어 체력은 컨테이너 시스템이다 (Plan2 §2.2 재확인)

- `MaxHealth = NumContainers × ContainerHealth` 가 **불변식**이다.
  `UDRPlayerAttributeSet::GetCurrentContainerIndex() = Floor(Health / ContainerHealth)`,
  `ExitCorruptedState()` 는 `NumContainers * ContainerHealth` 로 MaxHealth 를 되돌린다.
- 그래서 Plan2 는 `EDRUpgradeStat::ContainerHealth`(칸 용량)만 노출하고,
  `ADRCharacter::RefreshUpgradeEffects()` 에서
  `MaxHealthFlat = (Upgraded − Base) × NumContainers` 를 SetByCaller 로 넣는다.
- `NumContainers` / `ContainerHealth` 는 **복제되지 않는다**. 서버는 `DA_GameBalanceConfig`,
  클라는 BP CDO 기본값에서 읽는다(양쪽 4 / 100 으로 일치).

### 2.4 최대 물 / 이동 속도는 이미 끝까지 배선돼 있다

`GE_Upgrade_Stats` 에 `MaxWater Add/Multiply`, `MoveSpeed Add/Multiply` SetByCaller 가 있고,
`RefreshUpgradeEffects()` 가 `Data.Upgrade.MaxWater.Mult = 1 + Percent` 식으로 채운다.
이동 속도는 `InitializeMoveSpeedBinding()` 이 어트리뷰트 → `CharacterMovement->MaxWalkSpeed` 로 잇는다.

→ **칩 ③④는 `DA_ChipCatalog` 에 줄 두 개 넣는 것이 전부다. C++ 변경 0줄.**

### 2.5 "자동 체력 회복 타이머"의 실체

`ADRPlayerState` 에 2단 구조로 있다.

```
피격 → EnterCombat()
        ├ bIsInCombat = true, State.InCombat 태그
        ├ StopHealthRegen()                ← 회복 GE 제거
        └ SetTimer(CombatTimerHandle, CheckCombatExit, CombatExitDelay = 5.0, false)
                                              ↑ ★이것이 "자동 회복 타이머"★
(5초 무피격) → CheckCombatExit() → ExitCombat()
        └ CheckAndStartHealthRegen() → CheckHealthRegenStatus() → StartHealthRegen()
                                                                   └ GE_HealthRegen (Infinite, 주기 틱)
                                                                      MMC_HealthRegen: 틱당 10, 현재 컨테이너 상한까지
```

- `CombatExitDelay` 는 `BeginPlay()` 에서 `DA_GameBalanceConfig.PlayerCombat.CombatExitDelay` 로 1회 덮어쓴다.
- 피격이 이어지면 타이머만 리셋된다(`EnterCombat` 의 "이미 전투 중" 분기).
- 회복 **주기**(틱 간격)는 `GE_HealthRegen` 에셋의 Period 이고, **코드에 보정 훅이 없다.**
- 회복은 **서버 권한 전용**이다 (`StartHealthRegen()` 첫 줄이 `if (!HasAuthority()) return;`).

→ 해석 A(권장): `CombatExitDelay` 5.0 → 4.5. 해석 B: GE Period 단축. §7.5

### 2.6 HUD 표시를 껐다 켤 스위치가 없다

- 스테이지 HUD = `WBP_StageOverlay` (`Content/Blueprints/UI/Overlay/`)
  - `WBP_HealthWaterBar` — **체력 링 + 물 게이지가 한 위젯 안에 같이 있다** (`Overlay_Health`, `WaterProgress`, `HealthRing`…)
  - `SkillIconContainer` — `OnSkillIconClassChanged` 로 받은 클래스별 스킬아이콘 위젯이 들어가는 패널
- `UOverlayWidgetController` 는 값 변경 델리게이트뿐이고 **표시/숨김 델리게이트가 없다.**
  다만 선례는 있다 — `OnObjectiveProgressVisibilityChanged`, `FOnPhase3TimerVisibilitySignature`, `FOnToxicGasWarningUISignature`.

→ 체력/물은 **한 위젯이라 따로 숨길 수 없다.** 피처 태그를 `Vitals`(체력+물) / `SkillIcons` **2종**으로 정의한다. (§7.2)

---

## 3. 설계 개요

```
[DA_ChipCatalog]  ← 칩 5종 추가 (bCommonChip = true)
   │
   ├─ FDRUpgradeModifier[]              (숫자)   ──────┐
   └─ FGameplayTagContainer GrantedTags (스위치) ★C★ ──┼─┐
                                                       │ │
[DA_ProgressionConfig]                                 │ │
   ├─ InitialFreeSlots = 2          ★B★               │ │
   └─ DefaultEquippedChips[2]       ★B★               │ │
   │                                                   │ │
[UDRGameInstance::EnsureProgressInitialized]           │ │
   └─ ApplyDefaultLoadout()   (로봇당 세이브당 1회)      │ │
        → SlotChips[0] = CM.Stat.MaxHealth             │ │
          SlotChips[1] = CM.Util.HudDisplay            │ │
          UnlockedSlots = max(현재, 2)                  │ │
          bDefaultsApplied = true     ★B★              │ │
   │                                                   │ │
   ▼ (Plan2 의 기존 경로 그대로)                        │ │
ADRPlayerController::ReportUpgradeLoadout()
   → ServerReportUpgradeLoadout → SanitizeLoadout
   → ADRPlayerState::EquippedChips (Replicated)
   │
   ▼ 각 머신이 로컬 Resolve (복제 안 함)                 │ │
FDRUpgradeRuntime                                      ▼ ▼
   ├─ GlobalStats[EDRUpgradeStat]   ←── 숫자 스탯 2종 추가 ★신규★
   └─ GrantedTags                   ←── 피처 태그         ★C★
   │
   ├─→ ①  ADRCharacter::RefreshUpgradeEffects()   : MaxHealth 총량 → 컨테이너 분배
   ├─→ ③④ GE_Upgrade_Stats (무변경)
   ├─→ ②  ADRPlayerState::OnUpgradeRuntimeChanged
   │        → UOverlayWidgetController::RefreshHudFeatureVisibility()
   │        → WBP_StageOverlay (Visibility 토글)
   └─→ ⑤  ADRPlayerState::GetEffectiveCombatExitDelay()
```

**설계 원칙 (Plan2 계승)**

- 업그레이드 수치는 **`FDRUpgradeRuntime` 단일 해석 경로**로만 조회한다. 계산식을 흩뿌리지 않는다.
- `FDRUpgradeRuntime` 은 **복제하지 않는다.** 각 머신이 복제된 `EquippedChips` 로 각자 만든다.
- 칩이 하나도 없으면 **항등**이다. 배관을 넣어도 칩을 안 끼면 오늘과 수치가 100% 같다.

### 3.1 칩 5종 최종 스펙

모두 `bCommonChip = true`, `RequiredSlotCount = 1`, `Category = Stat`.

| # | `ChipId` | `DisplayName`(예시) | `EffectSummary` | Modifier / GrantedTags | `Tier` | 기본 장착 |
|---|---|---|---|---|---|---|
| ① | `CM.Stat.MaxHealth` | 예비 전원 셀 | 최대 체력 +100 | `MaxHealth` Flat **+100** | **1** | **✔ 슬롯 0** |
| ② | `CM.Util.HudDisplay` | 계기판 모듈 | 체력 · 물 · 스킬 아이콘 표시 | GrantedTags:<br>`Upgrade.Feature.HUD.Vitals`<br>`Upgrade.Feature.HUD.SkillIcons` | **1** | **✔ 슬롯 1** |
| ③ | `CM.Stat.MaxWater` | 확장 물탱크 | 최대 물 +50% | `MaxWater` Percent **+0.5** | 2 | |
| ④ | `CM.Stat.MoveSpeed` | 경량 구동계 | 이동 속도 +10% | `MoveSpeed` Percent **+0.1** | 2 | |
| ⑤ | `CM.Stat.RegenDelay` | 응급 수복기 | 체력 회복 시작 0.5초 단축 | `HealthRegenDelay` Flat **−0.5** | 3 | |

- `RequiredSlotTier` 는 **튜닝값**이다. 다만 **기본 장착 2종은 반드시 1** 이어야 한다
  — `SanitizeClassState()` 가 `RequiredSlotTier <= UnlockedSlots` 를 검사하는데, 시작 해금 수가 2이기 때문이다. (§6.4)
- `ValidateCatalog()` 규칙 `RequiredSlotCount <= RequiredSlotTier` 는 전부 `1 <= N` 이라 자동 통과.
- ②는 `Modifiers` 가 비어 있다 → **현재 `ValidateCatalog()` 가 "효과 없음" 오류를 낸다. 규칙을 고쳐야 한다.** (§4.2)

> ### ⚠️ 칩 ⑥ 제외로 생긴 콘텐츠 공백
>
> 슬롯은 6칸인데 공용 칩은 **5종**이고 그중 2종은 기본 장착이다.
> 3~6번 칸(4칸)을 채울 후보가 **③④⑤ 세 개뿐**이라 **"고민 없이 다 끼면 끝"** 인 상태가 된다.
> 슬롯을 사는 재화 소비의 동기도 약해진다 — 4번째 칸을 사도 꽂을 게 없다.
>
> 선택의 긴장감은 두 가지 중 하나로 메운다:
> 1. **로봇 전용 칩**을 추가한다 (Plan2 §6.2 의 `GR.*` / `VM.*` / `RV.*` 계열 — 스킬 피해량 / 쿨다운 / 투사체 수).
>    ★권장★ — 배관이 이미 전부 있고 카탈로그 항목만 쓰면 된다.
> 2. **`MaxSlots` 를 6 → 4 로 줄인다.** 배관은 `MaxSlots` 를 전부 Config 에서 읽으므로 코드 변경이 없다.
>    다만 `WBP_UpgradeScreen` 의 슬롯 6칸 레이아웃을 손봐야 한다.
>
> 이 문서는 공용 세트만 다루므로 판단을 §14 O-6 로 남긴다.

### 3.2 기존 정원로봇 전용 칩 3종의 처리

`DA_ChipCatalog` 에 이미 `GR.Stat.Container` / `GR.Stat.Water` / `GR.Stat.Speed` 가 들어 있고, 이번 ①③④와 **효과가 겹친다.**

→ **권장: 기존 3종을 삭제하고 공용 칩으로 흡수한다.**

- 삭제해도 세이브는 안전하다 — `SanitizeClassState()` 가 고아 칩을 비우고 재화를 환급한다(Plan2 §6.6).
- 남겨두면 "정원로봇만 물탱크 칩이 2개" 라는 이상한 목록이 된다.
- 삭제 대신 남기려면 **수치를 확실히 차등**해야 한다(예: 공용 +50% / 정원 전용 +80%, 그리고 정원 전용은 Tier 4 이상).

---

## 4. 데이터 모델 변경

### 4.1 `EDRUpgradeStat` — 2개 추가 (`DRUpgradeTypes.h`)

```cpp
UENUM(BlueprintType)
enum class EDRUpgradeStat : uint8
{
    // ===== 기존 (순서 절대 변경 금지) =====
    ContainerHealth      UMETA(DisplayName = "컨테이너 체력"),
    MaxWater             UMETA(DisplayName = "최대 물"),
    MoveSpeed            UMETA(DisplayName = "이동 속도"),
    DamageTaken          UMETA(DisplayName = "받는 피해"),
    WaterGain            UMETA(DisplayName = "물 획득량"),
    SkillDamage          UMETA(DisplayName = "스킬 피해량"),
    SkillWaterCost       UMETA(DisplayName = "스킬 물 소모량"),
    SkillCooldown        UMETA(DisplayName = "스킬 딜레이"),
    SkillProjectileCount UMETA(DisplayName = "스킬 투사체 수"),

    // ===== Plan3(공용칩) 추가 — ★반드시 Count 바로 앞★ (D-4) =====

    // 캐릭터 단위. ★Flat 전용★ — 총 최대 체력(HP) 증가량이다.
    // 컨테이너 1칸 용량이 아니라 "총량"이라서, 코드가 NumContainers 로 나눠 칸에 분배한다.
    // Percent 성분이 필요하면 ContainerHealth 쪽에 쓴다(수학적으로 동일하고 이중 정의를 피한다).
    MaxHealth            UMETA(DisplayName = "최대 체력"),

    // 캐릭터 단위. 전투 이탈 → 자동 체력 회복이 시작되기까지의 대기 시간(초). 음수 = 단축.
    HealthRegenDelay     UMETA(DisplayName = "회복 시작 시간"),

    Count UMETA(Hidden)
};
```

동반 수정 (같은 헤더):

```cpp
// DRIsSkillStat() 은 ★변경하지 않는다★ — 이번에 추가한 두 스탯은 모두 캐릭터 단위(전역)다.

FORCEINLINE bool DRIsLowerBetter(EDRUpgradeStat Stat)
{
    return Stat == EDRUpgradeStat::DamageTaken
        || Stat == EDRUpgradeStat::SkillWaterCost
        || Stat == EDRUpgradeStat::SkillCooldown
        || Stat == EDRUpgradeStat::HealthRegenDelay;        // 추가
}

// ★신규★ 값의 단위가 "초"인가 — UI 표기에 "초"를 붙인다 (§8.3)
FORCEINLINE bool DRIsTimeStat(EDRUpgradeStat Stat)
{
    return Stat == EDRUpgradeStat::SkillCooldown
        || Stat == EDRUpgradeStat::HealthRegenDelay;
}
```

### 4.2 `FDRUpgradeChipDefinition` — 2개 필드 추가 (`DRUpgradeTypes.h`)

```cpp
    // ★Plan3(공용칩)★ true 면 OwnerClass 를 무시하고 ★모든 로봇★의 목록에 나타난다.
    // 장착 상태 자체는 로봇별로 따로다 (D-1) — 정원로봇에 끼워도 자판기에는 안 끼워진다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
    bool bCommonChip = false;

    // ★Plan3(공용칩)★ 수치가 아닌 "기능 스위치". 장착 중이면 FDRUpgradeRuntime::GrantedTags 에 합쳐진다.
    // 예: Upgrade.Feature.HUD.Vitals — 이 태그가 있어야 체력/물 UI 가 보인다.
    // GAS 의 어빌리티/이펙트 태그와는 무관하다. ASC 에 부여되지 않는다.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Effect",
              meta = (Categories = "Upgrade.Feature"))
    FGameplayTagContainer GrantedTags;

    // ===== 헬퍼 =====

    // ★모든 소속 판정의 단일 진입점★ (§5). Chip->OwnerClass == X 로 직접 비교하는 코드를 남기지 말 것.
    bool IsUsableBy(EPlayerCharacterClass InClass) const
    {
        return bCommonChip || OwnerClass == InClass;
    }

    // 아무 효과도 없는 칩인가 (ValidateCatalog 용)
    bool HasAnyEffect() const
    {
        return Modifiers.Num() > 0 || GrantedTags.Num() > 0;
    }
```

**`ValidateCatalog()` 수정** (`DRChipCatalog.cpp`) — 지금은 `Modifiers.Num() == 0` 을 무조건 오류로 본다.
칩 ②는 `Modifiers` 가 비었으므로 규칙을 바꾼다.

```cpp
-   if (Chip.Modifiers.Num() == 0)
+   if (!Chip.HasAnyEffect())
    {
-       OutErrors.Add(... "Modifiers 가 비어 있어 아무 효과도 없습니다.");
+       OutErrors.Add(... "Modifiers 와 GrantedTags 가 모두 비어 있어 아무 효과도 없습니다.");
    }
```

추가할 검사 3개:

```cpp
// (a) MaxHealth 는 Flat 전용이다 (D-3)
if (Modifier.Stat == EDRUpgradeStat::MaxHealth && Modifier.Op == EDRUpgradeOp::Percent)
{
    OutErrors.Add(FString::Printf(
        TEXT("%s: MaxHealth 는 Flat 전용입니다. 비율 증가는 ContainerHealth Percent 로 작성하세요."), *IdString));
}

// (b) 공용 칩인데 특정 로봇 스킬 태그를 노린 모디파이어가 있으면 다른 로봇에서 조용히 무효가 된다
if (Chip.bCommonChip && Modifier.TargetAbilityTag.IsValid())
{
    OutErrors.Add(FString::Printf(
        TEXT("%s: 공용 칩에 특정 스킬 태그(%s)가 지정돼 다른 로봇에서는 효과가 없습니다."),
        *IdString, *Modifier.TargetAbilityTag.ToString()));
}

// (c) 피처 태그는 Upgrade.Feature 하위여야 한다 (오타 방지)
static const FGameplayTag FeatureRoot = FGameplayTag::RequestGameplayTag(TEXT("Upgrade.Feature"));
for (const FGameplayTag& Tag : Chip.GrantedTags)
{
    if (!Tag.MatchesTag(FeatureRoot))
    {
        OutErrors.Add(FString::Printf(
            TEXT("%s: GrantedTags 의 %s 가 Upgrade.Feature 하위가 아닙니다."), *IdString, *Tag.ToString()));
    }
}
```

### 4.3 `FDRUpgradeRuntime` — 피처 태그 채널 추가 (`DRUpgradeTypes.h`)

```cpp
USTRUCT(BlueprintType)
struct FDRUpgradeRuntime
{
    GENERATED_BODY()

    UPROPERTY() TArray<FDRResolvedStat> GlobalStats;
    UPROPERTY() TMap<FGameplayTag, FDRSkillStatBlock> SkillStats;

    // ★Plan3(공용칩)★ 장착 칩들이 부여한 기능 스위치의 합집합.
    UPROPERTY() FGameplayTagContainer GrantedTags;

    void Reset()
    {
        GlobalStats.Reset();
        GlobalStats.SetNum(static_cast<int32>(EDRUpgradeStat::Count));
        SkillStats.Reset();
        GrantedTags.Reset();          // ★추가★ — 빠뜨리면 칩을 빼도 UI 가 계속 보인다
    }

    // 피처 스위치 조회. 호출부가 GrantedTags 를 직접 만지지 않게 한다.
    bool HasFeature(const FGameplayTag& FeatureTag) const
    {
        return FeatureTag.IsValid() && GrantedTags.HasTag(FeatureTag);
    }

    // ... Apply / ApplySkill / GetGlobalStat / IsEmpty 는 그대로 ...
};
```

> ⚠️ **`IsEmpty()` 는 손대지 않는다.** 이 함수의 의미는 "**수치상** 항등인가"이고,
> `ADRCharacter::RefreshUpgradeEffects()` 가 "GE 를 얹을 필요가 있나"를 판단하는 데 쓴다.
> 칩 ②만 끼운 상태는 수치가 안 바뀌므로 GE 를 안 얹는 게 **맞다.**
> 대신 헤더 주석에 "GrantedTags 는 보지 않는다"를 명시해 오해를 막는다.

`ResolveLoadout()` 에 한 줄 추가 (`DRChipCatalog.cpp:145` 근처):

```cpp
 for (const FName& ChipId : EquippedChips)
 {
     const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
     if (!Chip) continue;
-    if (Chip->OwnerClass != CharacterClass) continue;
+    if (!Chip->IsUsableBy(CharacterClass)) continue;

     for (const FDRUpgradeModifier& Modifier : Chip->Modifiers)
     {
         OutRuntime.AddModifier(Modifier);
     }
+    OutRuntime.GrantedTags.AppendTags(Chip->GrantedTags);   // ★추가★
 }
```

### 4.4 `FDRClassUpgradeState` — 기본 장착 1회 마커 (`DRProgressionTypes.h`)

```cpp
    // ★Plan3(공용칩)★ 기본 장착을 이미 적용했는가.
    // 이 플래그가 없으면 플레이어가 기본 칩을 뺀 다음 게임을 껐다 켤 때마다 다시 꽂힌다.
    // "한 번만 꽂아준다"를 자료구조로 보장하는 유일한 수단이다. (§6.2)
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade")
    bool bDefaultsApplied = false;
```

### 4.5 `UDRProgressionConfig` — 기본 장착 설정 (`DRProgressionConfig.h`)

```cpp
    // ========== 기본 제공 (Plan3(공용칩) §6) ==========

    // 새 세이브에서 ★무료로★ 열어 두는 슬롯 수. 기본 장착 칩이 들어갈 자리다.
    // DefaultEquippedChips.Num() 보다 작으면 넘치는 칩은 장착되지 않는다(경고 로그).
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot", meta = (ClampMin = "0"))
    int32 InitialFreeSlots = 2;

    // 새 세이브에서 앞 칸부터 순서대로 자동 장착할 칩 Id.
    // ★모든 로봇에 공통 적용★ 된다 — 로봇별로 다르게 주고 싶어지면 TMap 으로 바꿀 것.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Slot")
    TArray<FName> DefaultEquippedChips;
```

**`GetSlotUnlockCostForClass()` 수정** — 무료 슬롯은 예산에서 빼야 Plan2 §5.4 예산 검증이 맞는다.

```cpp
 int32 UDRProgressionConfig::GetSlotUnlockCostForClass() const
 {
     int32 Total = 0;
-    for (int32 SlotNumber = 1; SlotNumber <= GetMaxSlotCount(); ++SlotNumber)
+    // 무료 시작 슬롯은 재화를 쓰지 않으므로 필요 예산에서 제외한다
+    const int32 FirstPaid = FMath::Clamp(InitialFreeSlots, 0, GetMaxSlotCount()) + 1;
+    for (int32 SlotNumber = FirstPaid; SlotNumber <= GetMaxSlotCount(); ++SlotNumber)
     {
         const int32 Cost = GetSlotUnlockCost(SlotNumber);
         if (Cost > 0) Total += Cost;
     }
     return Total;
 }
```

> **슬롯 가격표는 번호 기준을 유지한다.** 3번 칸은 여전히 `SlotCosts[2]`(=1100) 다.
> 무료 2칸이 생겼다고 인덱스를 당기지 않는다 — 당기면 에셋의 의미가 조용히 바뀐다.
> 대신 `SlotCosts` 앞 두 값(400/700)이 **쓰이지 않게 되므로**, 필요하면 배열을 재튜닝한다.
> 로봇 1대 비용: 1100 + 1500 + 1900 + 2400 = **6900**, 3대 = **20700**.

### 4.6 게임플레이 태그 추가 (`DRGameplayTags.h` / `.cpp`)

```cpp
    // ===== Plan3(공용칩) 피처 스위치 =====
    FGameplayTag Upgrade_Feature_HUD_Vitals;      // 체력 링 + 물 게이지 (한 위젯이라 묶인다 — §2.6)
    FGameplayTag Upgrade_Feature_HUD_SkillIcons;  // 스킬 아이콘 패널
```

```cpp
    GameplayTags.Upgrade_Feature_HUD_Vitals = UGameplayTagsManager::Get().AddNativeGameplayTag(
        FName("Upgrade.Feature.HUD.Vitals"),
        FString("장착 시 체력/물 HUD 를 표시한다"));

    GameplayTags.Upgrade_Feature_HUD_SkillIcons = UGameplayTagsManager::Get().AddNativeGameplayTag(
        FName("Upgrade.Feature.HUD.SkillIcons"),
        FString("장착 시 스킬 아이콘 HUD 를 표시한다"));
```

> 체력/물을 따로 끄고 싶어지면 `WBP_HealthWaterBar` 를 두 위젯으로 쪼갠 뒤
> `Upgrade.Feature.HUD.Health` / `.Water` 를 추가한다. **지금은 쪼개지 않는다** (§14 열린 결정 O-3).

### 4.7 세이브 버전 — **올리지 않는다**

`UDRSaveGame::CurrentSaveVersion` 은 **4 그대로 둔다.**

- 이번 변경은 `FDRClassUpgradeState::bDefaultsApplied` **순수 필드 추가**뿐이다.
  UE 역직렬화가 기본값(false)으로 채워주므로 버전을 올릴 이유가 없다.
- 올리면 `EnsureProgressInitialized()` 의 마이그레이션 분기가 돌아
  **테스터 전원의 재화·슬롯·칩이 초기화된다.** (`DRSaveGame.h` 의 기존 경고 주석과 동일한 판단)
- `bDefaultsApplied == false` 인 **기존 세이브도 자동으로 기본 장착을 1회 받는다.**
  이건 의도한 동작이다 — 기존 테스터도 HUD 칩을 갖게 된다.
  단 **빈 칸이 없으면 못 꽂는다**(이미 6칸을 다 채워 쓰던 세이브). §6.2 의 배치 규칙 참조.

### 4.8 `FDRChipViewModel` — 공용 표시용 1필드 (`DRProgressionTypes.h`)

```cpp
    // 모든 로봇 공용 칩인가 (카드에 "공용" 뱃지)
    UPROPERTY(BlueprintReadOnly, Category = "Chip")
    bool bCommonChip = false;
```

`UDRGameInstance::MakeChipViewModel()` 에서 `OutViewModel.bCommonChip = Chip.bCommonChip;` 한 줄.

> **`EDRChipCategory::Common` 은 만들지 않는다.** `Category` 는 "스탯/돌파" 축이고
> "공용/전용"은 직교하는 다른 축이다. 하나의 enum 에 섞으면 "공용 돌파 칩"을 표현할 수 없다.

---

## 5. 배관 A — 공용 칩 (`IsUsableBy`)

**규칙: `Chip->OwnerClass == / != CharacterClass` 를 코드에 남기지 않는다.** 전부 `IsUsableBy()` 로 바꾼다.

| # | 파일:줄 | 변경 |
|---|---|---|
| 1 | `DRChipCatalog.cpp:92` | `if (Chip.OwnerClass == CharacterClass)` → `if (Chip.IsUsableBy(CharacterClass))` |
| 2 | `DRChipCatalog.cpp:145` | `if (Chip->OwnerClass != CharacterClass) continue;` → `if (!Chip->IsUsableBy(CharacterClass)) continue;` |
| 3 | `DRChipCatalog.cpp:169` | `if (!Chip \|\| Chip->OwnerClass != CharacterClass)` → `if (!Chip \|\| !Chip->IsUsableBy(CharacterClass))` |
| 4 | `DRGameInstance.cpp:247` | `&& Chip->OwnerClass == CharacterClass` → `&& Chip->IsUsableBy(CharacterClass)` |
| 5 | `DRGameInstance.cpp:692` | `if (!Chip \|\| Chip->OwnerClass != CharacterClass) return false;` → `IsUsableBy` |
| 6 | `DRGameInstance.cpp:719` | `if (Chip->OwnerClass != CharacterClass) return WrongClass;` → `if (!Chip->IsUsableBy(CharacterClass)) return WrongClass;` |

**검증 방법** — 작업 후 아래 grep 결과가 **코스메틱(스킨) 경로만** 남아야 한다:

```
grep -rn "OwnerClass ==\|OwnerClass !=" Source/DaeRune --include=*.cpp
```

기대 결과: `DRCosmeticCatalog.cpp:50`, `DRCosmeticCatalog.cpp:75`, `DRGameInstance.cpp:1522`, `DRGameInstance.cpp:1622`
(전부 스킨 경로 — 칩과 무관하므로 건드리지 않는다).

### 5.1 정렬 순서 — 공용 칩을 목록 위로

`GetChipsForClass()` 의 정렬에 공용 여부를 추가하면 목록이 읽기 쉬워진다 (선택).

```cpp
 OutChips.Sort([](const FDRUpgradeChipDefinition& A, const FDRUpgradeChipDefinition& B)
 {
     if (A.RequiredSlotTier != B.RequiredSlotTier) return A.RequiredSlotTier < B.RequiredSlotTier;
+    if (A.bCommonChip != B.bCommonChip) return A.bCommonChip;   // 같은 단계면 공용이 먼저
     return A.ChipId.LexicalLess(B.ChipId);
 });
```

### 5.2 `SanitizeLoadout()` 의 서버 검증은 그대로 유효하다

서버는 클라의 해금 단계를 모르지만 **총 칸 수(6)는 안다.** 공용 칩이 섞여도
"존재하는 칩인가 / 이 로봇이 쓸 수 있는가 / 중복인가 / 총 칸 수를 넘는가" 네 검사는 그대로 성립한다.
`IsUsableBy()` 로 바꾸는 것 외에 로직 변경이 필요 없다.

---

## 6. 배관 B — 기본 장착 파이프라인

### 6.1 진입점

`UDRGameInstance::EnsureProgressInitialized()` 의 **로봇 키 lazy 초기화 직후, 정화(`SanitizeClassState`) 직전**에 넣는다.

```cpp
void UDRGameInstance::EnsureProgressInitialized()
{
    if (!CurrentSaveGame) return;

    // ... (기존 세이브 버전 마이그레이션) ...

    const int32 ClassCount = static_cast<int32>(EPlayerCharacterClass::Count);
    for (int32 Index = 0; Index < ClassCount; ++Index)
    {
        const EPlayerCharacterClass CharacterClass = static_cast<EPlayerCharacterClass>(Index);
        CurrentSaveGame->ClassUpgrades.FindOrAdd(CharacterClass);
        CurrentSaveGame->Cosmetics.FindOrAdd(CharacterClass).EnsureSize();
    }

    // ... (기존 코스메틱 정화) ...

    // ★Plan3(공용칩)★ 기본 장착 — 반드시 SanitizeClassState 보다 ★먼저★.
    // 순서가 바뀌면 배열 길이가 0인 상태에서 배치하려다 조용히 실패한다.
    bool bDefaultsChanged = false;
    if (ProgressionConfig)
    {
        for (auto& Pair : CurrentSaveGame->ClassUpgrades)
        {
            bDefaultsChanged |= ApplyDefaultLoadout(Pair.Key, Pair.Value);
        }
    }

    // ... (기존 SanitizeClassState 루프 — 환급 포함) ...

    if (bDefaultsChanged /* || bAnyChanged */)
    {
        SaveProgress();
    }
}
```

### 6.2 `ApplyDefaultLoadout()` 구현 (신규 private 멤버)

```cpp
/**
 * 새 세이브(또는 기본 장착을 한 번도 받지 않은 세이브)에 시작 슬롯과 기본 칩을 꽂는다.
 *
 * ★세이브당 로봇당 딱 1회만★ 동작한다 (bDefaultsApplied 마커).
 * 그래야 플레이어가 기본 칩을 뺀 선택이 다음 실행에서 되돌려지지 않는다.
 *
 * ★EquipChip() 을 쓰지 않는다★ (D-5) — 그 함수는 IsUpgradeSystemUnlocked() 게이트를 통과해야 하는데,
 * 기본 장착은 스테이지1 클리어 전에도 동작해야 한다.
 *
 * @return 세이브를 건드렸으면 true
 */
bool UDRGameInstance::ApplyDefaultLoadout(EPlayerCharacterClass CharacterClass, FDRClassUpgradeState& State)
{
    if (!ProgressionConfig) return false;
    if (State.bDefaultsApplied) return false;

    const int32 MaxSlots = ProgressionConfig->GetMaxSlotCount();

    // 1) 칸 배열 길이 보장 (SanitizeClassState 보다 먼저 돌기 때문에 여기서 해야 한다)
    if (State.SlotChips.Num() != MaxSlots)
    {
        State.SlotChips.SetNum(MaxSlots);
    }

    // 2) 무료 시작 슬롯 해금 — ★SpentCurrency 는 건드리지 않는다★ (무료니까)
    const int32 FreeSlots = FMath::Clamp(ProgressionConfig->InitialFreeSlots, 0, MaxSlots);
    State.UnlockedSlots = FMath::Max(State.UnlockedSlots, FreeSlots);

    // 3) 기본 칩 배치 — 앞에서부터 ★빈 칸에만★ 꽂는다.
    //    기존 세이브(이미 슬롯을 쓰고 있던 플레이어)의 장착을 덮어쓰지 않기 위함이다.
    for (const FName& ChipId : ProgressionConfig->DefaultEquippedChips)
    {
        if (ChipId.IsNone()) continue;

        const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
        if (!Chip)
        {
            UE_LOG(LogDR, Warning,
                TEXT("[Progression] DefaultEquippedChips 의 %s 가 카탈로그에 없습니다 — 건너뜁니다."),
                *ChipId.ToString());
            continue;
        }
        if (!Chip->IsUsableBy(CharacterClass)) continue;   // 공용이 아니면 그 로봇만 건너뛴다
        if (State.CountOccupiedSlots(ChipId) > 0) continue; // 이미 꽂혀 있음

        const int32 Required = FMath::Max(1, Chip->RequiredSlotCount);

        TArray<int32> Assignment;
        if (!ComputeAssignment(State, Required, /*PreferredSlotIndex*/ INDEX_NONE, Assignment))
        {
            UE_LOG(LogDR, Warning,
                TEXT("[Progression] 기본 칩 %s 를 꽂을 빈 칸이 없습니다 (클래스 %d)."),
                *ChipId.ToString(), static_cast<int32>(CharacterClass));
            continue;
        }

        for (const int32 SlotIndex : Assignment)
        {
            State.SlotChips[SlotIndex] = ChipId;
        }
    }

    State.bDefaultsApplied = true;
    return true;
}
```

> **`ComputeAssignment()` 를 재사용하는 이유**: 칸 배정 규칙이 두 군데로 갈라지면
> "드래그앤드롭으로 꽂은 칩"과 "기본으로 꽂힌 칩"의 배치 규칙이 달라진다. Plan2 §6.4 의 원칙을 지킨다.

### 6.3 `SystemLocked` 게이트와의 관계

| 경로 | 해금 전(스테이지1 클리어 전) | 해금 후 |
|---|---|---|
| `ApplyDefaultLoadout()` | ✔ 동작 (게이트를 거치지 않음) | ✔ (이미 적용됐으면 no-op) |
| `ResolveLoadout()` → 효과 발동 | ✔ 동작 (해금 여부를 보지 않는다) | ✔ |
| `EquipChip()` / `UnequipChip()` (플레이어 조작) | ✘ `SystemLocked` | ✔ |
| 업그레이드 화면 열기 | ✘ `ADRUpgradeStation` 이 막음 | ✔ |

→ **결과: 게임을 처음 켠 플레이어는 HUD 가 보이고 체력이 500 이며, 그 칩을 뺄 수는 없다.**
스테이지1 을 클리어해야 비로소 "HUD 를 포기하고 다른 칩을 끼는" 선택지가 열린다. 의도한 진행 곡선이다.

### 6.4 `SanitizeClassState()` 와 충돌하지 않는지 검증

정화 루프의 유효성 판정은 다음과 같다 (`DRGameInstance.cpp:245` 부근):

```cpp
const bool bValid = Chip
    && Chip->IsUsableBy(CharacterClass)          // ← §5 에서 교체됨
    && Chip->RequiredSlotTier <= Unlocked
    && SlotIndex < Unlocked;
```

기본 장착 칩이 살아남으려면:

| 조건 | 기본 칩이 만족하는가 |
|---|---|
| 카탈로그에 존재 | ✔ (없으면 `ApplyDefaultLoadout` 이 경고하고 안 꽂음) |
| `IsUsableBy` | ✔ (`bCommonChip = true`) |
| `RequiredSlotTier <= UnlockedSlots(=2)` | ✔ **Tier 를 1 로 둔 이유가 이것이다** |
| `SlotIndex < UnlockedSlots(=2)` | ✔ (슬롯 0, 1 에 꽂힘) |

→ **기본 장착 칩의 `RequiredSlotTier` 를 3 이상으로 올리면 다음 실행에서 조용히 사라진다.**
`UDRProgressionConfig::ValidateCurrencyBudget()` 에 이 검사를 추가해 두면 사고를 막는다
(Config 가 `InitialFreeSlots` 를 알아야 하므로 `ValidateCatalog` 이 아니라 이쪽이다):

```cpp
for (const FName& DefaultId : DefaultEquippedChips)
{
    const FDRUpgradeChipDefinition* Chip = ChipCatalog ? ChipCatalog->FindChip(DefaultId) : nullptr;
    if (!Chip)
    {
        OutErrors.Add(FString::Printf(
            TEXT("기본 장착 칩 %s 가 카탈로그에 없습니다."), *DefaultId.ToString()));
        continue;
    }
    if (Chip->RequiredSlotTier > InitialFreeSlots)
    {
        OutErrors.Add(FString::Printf(
            TEXT("기본 장착 칩 %s 의 RequiredSlotTier(%d) 가 InitialFreeSlots(%d) 보다 커서 "
                 "세이브 정화 시 자동으로 해제됩니다."),
            *DefaultId.ToString(), Chip->RequiredSlotTier, InitialFreeSlots));
    }
}
```

### 6.5 `RefundSlot()` — 무료 슬롯은 환불 불가

무료 슬롯을 환불하면 **재화는 0원 받고 칸만 잃는다.** 함정이므로 막는다.

```cpp
 EDRUpgradeResult UDRGameInstance::RefundSlot(EPlayerCharacterClass CharacterClass, int32& OutRefunded)
 {
     ...
-    if (State.UnlockedSlots <= 0) return EDRUpgradeResult::NoSlotToRefund;
+    // ★Plan3(공용칩)★ 무료로 받은 시작 슬롯은 환불 대상이 아니다 (돌려받을 재화가 0인데 칸만 잃는다)
+    const int32 FreeSlots = ProgressionConfig
+        ? FMath::Clamp(ProgressionConfig->InitialFreeSlots, 0, ProgressionConfig->GetMaxSlotCount()) : 0;
+    if (State.UnlockedSlots <= FreeSlots) return EDRUpgradeResult::NoSlotToRefund;
     ...
 }
```

`GetSlotViewModels()` 의 `bIsNextUnlockable` / `UnlockBlockReason` 은 손댈 필요 없다
(무료 슬롯은 이미 `Empty` 또는 `Occupied` 상태라 잠금 UI 를 타지 않는다).

### 6.6 치트 훅 (테스트 편의)

```cpp
// ★치트/디버그 전용★ — 기본 장착을 다시 적용한다 (bDefaultsApplied 를 내리고 재실행)
UFUNCTION(BlueprintCallable, Exec, Category = "Progression|Upgrade")
void DebugResetDefaultLoadout();
```

`ADRPlayerController` 의 기존 `DRUnlockUpgrade()` 패턴을 따라 Exec 콘솔 명령으로도 노출한다.
"HUD 칩을 빼서 UI 를 없앴는데 되돌릴 방법이 없다"는 상황을 만들지 않기 위해 **반드시 넣는다.**

---

## 7. 칩별 구현 상세

### 7.1 칩 ① — 최대 체력 +100

#### 문제

컨테이너 시스템 때문에 `MaxHealth` 를 직접 올릴 수 없다 (§2.3).
Plan2 는 `ContainerHealth`(칸 용량)만 노출했는데, 그걸로 "+100"을 표현하면 `+25` 를 적어야 하고
**컨테이너 개수가 4가 아닌 순간 "+100"이 거짓말이 된다.**

#### 해결 — `MaxHealth` 총량 스탯을 코드가 칸에 나눠 넣는다

`ADRCharacter::RefreshUpgradeEffects()` 의 **①번 블록만** 3줄 고친다.

```cpp
void ADRCharacter::RefreshUpgradeEffects()
{
    if (!AbilitySystemComponent) return;

    const ADRPlayerState* DRPS = GetPlayerState<ADRPlayerState>();
    if (!DRPS) return;

    const FDRUpgradeRuntime& Runtime = DRPS->GetUpgradeRuntime();

    // ===== 1) 컨테이너 체력 (모든 머신) =====
    const float BaseContainerHealth = ContainerHealth;

+   // ★Plan3(공용칩)★ MaxHealth 는 "총 HP" 단위 스탯이다.
+   // 컨테이너 불변식(MaxHealth = NumContainers × ContainerHealth)을 지키려면
+   // 총량을 칸 수로 나눠 "칸 용량 증가분"으로 환산해 넣어야 한다.
+   // 이렇게 해야 칸 수가 4에서 바뀌어도 카탈로그의 "+100" 이 계속 +100 이다.
+   const float MaxHealthBonusTotal = Runtime.GetGlobalStat(EDRUpgradeStat::MaxHealth).Flat;
+   const float PerContainerBonus   = (NumContainers > 0)
+       ? MaxHealthBonusTotal / static_cast<float>(NumContainers) : 0.f;

    const float UpgradedContainerHealth = FMath::Max(
-       1.f, Runtime.Apply(EDRUpgradeStat::ContainerHealth, BaseContainerHealth));
+       1.f, Runtime.Apply(EDRUpgradeStat::ContainerHealth, BaseContainerHealth + PerContainerBonus));

    if (UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSets))
    {
        PlayerAS->SetContainerInfo(NumContainers, UpgradedContainerHealth);
    }

    // ===== 2) 스탯 GE (서버 전용) ===== ... 이하 전부 무변경 ...
}
```

**나머지가 저절로 맞는 이유**: 아래 기존 줄이 이미 총량을 다시 계산한다.

```cpp
const float MaxHealthFlat = (UpgradedContainerHealth - BaseContainerHealth) * (float)NumContainers;
```
`UpgradedContainerHealth - BaseContainerHealth = 25` → `MaxHealthFlat = 100`. ✔

#### 계산 검증

| 상태 | ContainerHealth | NumContainers | MaxHealth | 컨테이너 인덱스 경계 |
|---|---|---|---|---|
| 칩 없음 | 100 | 4 | 400 | 100 / 200 / 300 / 400 |
| 칩 ① | **125** | 4 | **500** | 125 / 250 / 375 / 500 |
| 칩 ① + `ContainerHealth Percent +20%` | (100+25)×1.2 = **150** | 4 | **600** | 150 / 300 / 450 / 600 |

- `GetCurrentContainerIndex() = Floor(Health / 125)` → 여전히 0..3. ✔
- `ExitCorruptedState()` 의 `NumContainers * ContainerHealth` = 4 × 125 = 500. ✔ **오염 해제로 업그레이드가 증발하지 않는다.**
- `MMC_HealthRegen` 의 `ContainerMax = (Index+1) * ContainerHealth` 도 125 단위로 자동 추종. ✔
- 클라이언트도 같은 계산을 한다 (`RefreshUpgradeEffects` 의 ①번 블록은 `HasAuthority()` 앞에 있다). ✔

#### 주의: 컨테이너 UI

`WBP_ContainerHealthBar` 는 컨테이너 **개수**(4)로 아이콘을 그리므로 레이아웃이 안 바뀐다.
칸당 용량만 100 → 125 로 커진다. **UI 작업 0.**

#### 대안 — "컨테이너를 1칸 더 준다"

`NumContainers 4 → 5` 로 만들면 역시 MaxHealth 500 이고 "체력통이 하나 늘었다"는 시각적 임팩트가 크다.
다만 비용이 훨씬 크다:

- `EDRUpgradeStat::ContainerCount` 신설 + `SetContainerInfo(NumContainers + N, ...)`
- `WBP_ContainerHealthBar` 의 아이콘 개수를 **동적 생성**으로 바꿔야 한다 (지금은 `Container100ReduceEffectAnim` ~ `Container400ReduceEffectAnim` 처럼 **4개 고정 애니메이션**이 하드코딩돼 있다)
- 데미지 오버플로우 규칙(현재 칸 체력의 10%)과 오염 진입 판정 재검증

→ **이번 범위에서는 하지 않는다.** 필요해지면 별도 칩으로 추가한다 (§16).

---

### 7.2 칩 ② — 체력 · 물 · 스킬 아이콘 UI 표시

#### 7.2.1 신호 경로

```
칩 장착/해제 (로비)
  → GI::EquipChip/UnequipChip → SaveProgress + OnUpgradesChanged
  → (업그레이드 화면 닫기) ADRPlayerController::CloseUpgradeScreen → ReportUpgradeLoadout()
  → Server RPC → SanitizeLoadout → PS::SetEquippedChips → 복제
  → ★모든 머신★ PS::OnRep_EquippedChips → RebuildUpgradeRuntime()
        ├ CachedUpgradeRuntime.GrantedTags 갱신
        ├ Pawn->RefreshUpgradeEffects()            (기존)
        └ OnUpgradeRuntimeChanged.Broadcast()      ★신규★
  → UOverlayWidgetController::RefreshHudFeatureVisibility()
  → OnHudFeatureVisibilityChanged.Broadcast(bShowVitals, bShowSkillIcons)
  → WBP_StageOverlay: WBP_HealthWaterBar / SkillIconContainer 의 Visibility 토글
```

#### 7.2.2 `ADRPlayerState` — 변경 알림 델리게이트 추가

```cpp
// DRPlayerState.h
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpgradeRuntimeChangedSignature);

    // 장착 칩 수치 캐시가 다시 만들어졌다. UI 가 피처 태그를 다시 읽어야 한다.
    UPROPERTY(BlueprintAssignable, Category = "Upgrade")
    FOnUpgradeRuntimeChangedSignature OnUpgradeRuntimeChanged;

    // 피처 스위치 조회 (UI 가 FDRUpgradeRuntime 내부를 직접 뒤지지 않게 하는 단일 경로)
    UFUNCTION(BlueprintPure, Category = "Upgrade")
    bool HasUpgradeFeature(FGameplayTag FeatureTag) const
    {
        return CachedUpgradeRuntime.HasFeature(FeatureTag);
    }
```

```cpp
// DRPlayerState.cpp — RebuildUpgradeRuntime() 끝에 1줄
void ADRPlayerState::RebuildUpgradeRuntime()
{
    CachedUpgradeRuntime.Reset();
    // ... 기존 ResolveLoadout ...
    if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetPawn()))
    {
        DRCharacter->RefreshUpgradeEffects();
    }
+   OnUpgradeRuntimeChanged.Broadcast();   // ★추가★
}
```

> **왜 GameInstance 가 아니라 PlayerState 를 읽는가**
> HUD 는 로컬 UI 라서 `UDRGameInstance` 세이브를 바로 읽어도 "동작은" 한다. 그러나
> ① 스테이지 진행 중에 세이브가 바뀌면(치트/다른 창) HUD 만 따로 바뀌어 **실제 전투 효과와 어긋난다.**
> ② 관전 모드(`UpdateOverlayForSpectating`)에서는 **보고 있는 대상의** PlayerState 를 읽어야 맞다.
> → **SSOT 는 "서버가 승인해 복제한 `EquippedChips`" 다.** Plan2 §7.1 과 같은 판단.

#### 7.2.3 `UOverlayWidgetController` — 표시 델리게이트

```cpp
// OverlayWidgetController.h
// 업그레이드 칩에 따른 HUD 모듈 표시/숨김 (Plan3(공용칩) §7.2)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnHudFeatureVisibilityChangedSignature,
    bool, bShowVitals, bool, bShowSkillIcons);

    UPROPERTY(BlueprintAssignable, Category = "GAS|Upgrade")
    FOnHudFeatureVisibilityChangedSignature OnHudFeatureVisibilityChanged;

protected:
    // 현재 PlayerState 의 피처 태그를 읽어 방송한다. 초기 1회 + 칩 변경 시.
    UFUNCTION()
    void RefreshHudFeatureVisibility();
```

```cpp
// OverlayWidgetController.cpp
void UOverlayWidgetController::RefreshHudFeatureVisibility()
{
    const ADRPlayerState* DRPS = Cast<ADRPlayerState>(PlayerState);

    // ★PlayerState 가 아직 없으면 "보이는" 쪽으로 간다★
    // 실패했을 때 UI 가 사라지는 것보다 남아 있는 편이 훨씬 안전하다(디버깅 가능).
    if (!DRPS)
    {
        OnHudFeatureVisibilityChanged.Broadcast(true, true);
        return;
    }

    const FDRGameplayTags& Tags = FDRGameplayTags::Get();
    OnHudFeatureVisibilityChanged.Broadcast(
        DRPS->HasUpgradeFeature(Tags.Upgrade_Feature_HUD_Vitals),
        DRPS->HasUpgradeFeature(Tags.Upgrade_Feature_HUD_SkillIcons));
}
```

호출 지점 3곳:

| 위치 | 이유 |
|---|---|
| `BroadcastInitialValues()` 끝 | 위젯 최초 생성 시 1회 |
| `BindCallbacksToDependencies()` 에서 `DRPS->OnUpgradeRuntimeChanged.AddDynamic(this, &...::RefreshHudFeatureVisibility)` | 칩 변경 추종 |
| `UnbindAllDelegates()` 에서 `RemoveDynamic` | 관전 전환 시 댕글링 방지 (기존 패턴과 동일) |

#### 7.2.4 `WBP_StageOverlay` (BP 작업)

1. `Event Construct` / 기존 컨트롤러 바인딩 지점에서 `OnHudFeatureVisibilityChanged` 에 이벤트 바인딩.
2. 핸들러:
   ```
   Set Visibility (WBP_HealthWaterBar,  bShowVitals     ? Visible : Collapsed)
   Set Visibility (SkillIconContainer,  bShowSkillIcons ? Visible : Collapsed)
   ```
   - **`Hidden` 이 아니라 `Collapsed`** 를 쓴다. 레이아웃 공간까지 없애야 "빠진" 느낌이 난다.
3. ⚠️ **스킬 아이콘은 나중에 생성된다.** `OnSkillIconClassChanged` 가 오면 `SkillIconContainer` 에
   위젯을 만들어 넣는데, 그 시점에 **마지막 표시 상태를 다시 적용**해야 한다.
   → BP 변수 `bLastShowSkillIcons` 를 두고, 아이콘 생성 직후 그 값으로 Visibility 를 세팅한다.
4. ⚠️ 관전/리스폰으로 `UpdateOverlayForSpectating()` 이 돌면 컨트롤러가 재바인딩된다 →
   `BroadcastInitialValues()` 가 다시 방송하므로 자동 복구된다. (테스트 항목 T-9)

#### 7.2.5 튜토리얼 / 로비 오버레이는 건드리지 않는다

- `WBP_TutorialOverlay` 는 `WBP_SkillIcon_Tutorial` 을 쓰고, 튜토리얼은 업그레이드 시스템 해금 이전이다.
  → **항상 보이게 둔다.** 바인딩 자체를 하지 않는다.
- `WBP_LobbyOverlay` / `WBP_WaitingRoomOverlay` 도 대상 아님.

#### 7.2.6 "UI 를 끄는 대신 무엇을 얻는가"

이 칩은 **단점만 있는 칩이 아니라 "칸을 돌려받는 선택"** 이다.
슬롯이 6칸뿐이므로, 빌드를 짤 때 HUD 를 포기하면 칩 하나를 더 끼울 수 있다.
→ UI 표기는 **돌파 칩처럼 경고색을 쓰지 않는다.** `bIsDrawback` 모디파이어가 없으므로 자동으로 그렇게 된다.
다만 **해제 시 확인 토스트**를 띄우는 편이 친절하다 (§8.4).

> ⚠️ **칩 ⑥ 제외의 부작용**: 꽂을 칩이 5종뿐이라 "HUD 를 빼서 칸을 벌" 이유가 약하다.
> 이 트레이드오프가 의미를 가지려면 §3.1 의 콘텐츠 공백 메모대로 **칩 종류를 늘리거나 슬롯을 줄여야** 한다.

---

### 7.3 칩 ③ — 최대 물 +50%

**C++ 변경 없음.** `DA_ChipCatalog` 항목 1개.

```
ChipId        : CM.Stat.MaxWater
bCommonChip   : true
Category      : Stat
DisplayName   : 확장 물탱크
EffectSummary : 최대 물 +50%
RequiredSlotCount : 1
RequiredSlotTier  : 2
Modifiers[0]  : { Stat = MaxWater, Op = Percent, Value = 0.5, bIsDrawback = false }
```

동작 확인 경로: `RefreshUpgradeEffects()` → `Data.Upgrade.MaxWater.Mult = 1 + 0.5 = 1.5`
→ `GE_Upgrade_Stats` 의 `MaxWater Multiply`.
GAS 애그리게이터 평가식은 `(Base + ΣAdd) × ΠMultiply` 라 Plan2 §4.3 의 합산 규칙과 일치한다.

⚠️ **현재 물 보유량은 비율 유지된다** (`bPreserveVitalRatioOnUpgradeApply`).
스폰 시 만수위였다면 그대로 만수위(150%)가 된다. 칩 목록이 늦게 도착해 재적용돼도 공짜 회복은 없다.

---

### 7.4 칩 ④ — 이동 속도 +10%

**C++ 변경 없음.** `DA_ChipCatalog` 항목 1개.

```
ChipId        : CM.Stat.MoveSpeed
bCommonChip   : true
EffectSummary : 이동 속도 +10%
RequiredSlotTier : 2
Modifiers[0]  : { Stat = MoveSpeed, Op = Percent, Value = 0.1 }
```

확인 항목:

- `InitializeMoveSpeedBinding()` 이 `MoveSpeed` 어트리뷰트 → `CharacterMovement->MaxWalkSpeed` 를 잇는다. ✔
- **부품 운반 중**에는 `ADRCharacter::SetCarryingState()` 가 이동 속도를 따로 제한한다.
  운반 속도가 **고정값**이면 칩이 무효가 되고, **배율**이면 칩이 곱해진다.
  → 테스트 항목 T-5 로 실측한다. 고정값이면 "운반 중에는 적용되지 않음"을 칩 설명에 쓰거나,
  운반 속도도 어트리뷰트 배율로 바꾼다.
- 청소기 `Buff.RobotVacuum.DashSpeed` 등 다른 이속 버프와는 **GE 애그리게이터에서 자연 합산**된다.

---

### 7.5 칩 ⑤ — 자동 체력 회복 타이머 −0.5초

#### 7.5.1 해석 (§2.5 기준)

**해석 A (이 문서가 구현하는 것)**: 전투 이탈 대기 시간 `CombatExitDelay` 5.0초 → 4.5초.
피격이 멈춘 뒤 회복이 **0.5초 빨리 시작**된다.

#### 7.5.2 `ADRPlayerState` 변경

```cpp
// DRPlayerState.h (private 섹션)
    // 업그레이드로 줄어든 전투 이탈 대기의 하한. 0에 가까우면 "전투 중"이 의미를 잃는다.
    UPROPERTY(EditDefaultsOnly, Category = "Combat", meta = (ClampMin = "0.1"))
    float MinCombatExitDelay = 1.0f;

public:
    /**
     * 업그레이드가 반영된 전투 이탈 대기 시간.
     * ★타이머를 거는 시점에 계산한다★ — BeginPlay 때 캐시하면 칩 목록이 나중에 도착했을 때 반영되지 않는다.
     */
    UFUNCTION(BlueprintPure, Category = "Combat")
    float GetEffectiveCombatExitDelay() const;
```

```cpp
// DRPlayerState.cpp
float ADRPlayerState::GetEffectiveCombatExitDelay() const
{
    const float Upgraded = CachedUpgradeRuntime.Apply(EDRUpgradeStat::HealthRegenDelay, CombatExitDelay);
    return FMath::Max(MinCombatExitDelay, Upgraded);
}
```

`EnterCombat()` 의 `SetTimer` **2곳**을 교체한다 (둘 다 고쳐야 한다 — 하나만 고치면 연속 피격 시에만 구버전이 적용된다).

```cpp
 void ADRPlayerState::EnterCombat()
 {
     if (!HasAuthority()) return;
+    const float ExitDelay = GetEffectiveCombatExitDelay();

     if (bIsInCombat)
     {
-        GetWorld()->GetTimerManager().SetTimer(CombatTimerHandle, this, &ADRPlayerState::CheckCombatExit, CombatExitDelay, false);
+        GetWorld()->GetTimerManager().SetTimer(CombatTimerHandle, this, &ADRPlayerState::CheckCombatExit, ExitDelay, false);
         return;
     }
     ...
-    GetWorld()->GetTimerManager().SetTimer(CombatTimerHandle, this, &ADRPlayerState::CheckCombatExit, CombatExitDelay, false);
+    GetWorld()->GetTimerManager().SetTimer(CombatTimerHandle, this, &ADRPlayerState::CheckCombatExit, ExitDelay, false);
 }
```

#### 7.5.3 권한 / 복제

- 회복은 **서버 전용** (`StartHealthRegen()` 의 `HasAuthority()` 가드).
- `CachedUpgradeRuntime` 은 서버에도 존재한다 (`SetEquippedChips()` → `RebuildUpgradeRuntime()`).
- 클라이언트의 `CombatExitDelay` 는 `BeginPlay` 의 `HasAuthority()` 블록 안에서만 덮어써지므로
  **클라에서는 기본값 5.0 그대로**다. 하지만 클라는 이 값을 타이머에 쓰지 않으므로 무해하다.
  (UI 에 "회복까지 N초"를 표시하게 되면 그때 `CombatExitDelay` 를 복제해야 한다 — 지금은 불필요.)

#### 7.5.4 이미 돌고 있는 타이머

칩 목록이 전투 도중에 바뀌면(정상 플레이에서는 불가능 — 로비에서만 교체 가능),
이미 걸린 타이머는 **다음 `EnterCombat()` 부터** 새 값을 쓴다. 의도적으로 단순하게 둔다.

#### 7.5.5 ★만약 "회복 틱 주기"를 뜻한 것이었다면★ (해석 B)

"자동 체력 회복 타이머"가 **회복이 도는 간격**(예: 1초마다 10 회복 → 0.5초마다 10 회복)을 뜻했다면,
위 §7.5.2 대신 아래로 바꾼다. **스탯 이름과 칩 정의는 그대로 쓸 수 있다.**

1. `GE_HealthRegen` 의 **Period 를 SetByCaller** 로 바꾼다 (`Data.HealthRegen.Period` 태그 신설).
2. `ADRPlayerState::InitializeHealthRegenSpec()` 이 캐시하는 Spec 에 값을 주입한다.
   ⚠️ **현재 Spec 을 BeginPlay 에서 1회만 캐시**하므로, 칩이 나중에 도착하면 반영되지 않는다.
   → `StartHealthRegen()` 에서 매번 Spec 을 만들거나, `RebuildUpgradeRuntime()` 에서
     `InitializeHealthRegenSpec()` 을 다시 부르고 활성 GE 를 재적용해야 한다.

```cpp
void ADRPlayerState::InitializeHealthRegenSpec()
{
    if (!HealthRegenEffectClass || !AbilitySystemComponent) return;

    FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
    EffectContext.AddSourceObject(this);
    CachedHealthRegenSpec = AbilitySystemComponent->MakeOutgoingSpec(HealthRegenEffectClass, 1.f, EffectContext);

    if (CachedHealthRegenSpec.IsValid())
    {
        const float Period = FMath::Max(MinRegenPeriod,
            CachedUpgradeRuntime.Apply(EDRUpgradeStat::HealthRegenDelay, BaseRegenPeriod));
        CachedHealthRegenSpec.Data->SetSetByCallerMagnitude(
            FDRGameplayTags::Get().Data_HealthRegen_Period, Period);
    }
}
```

3. 밸런스 주의: 주기를 0.5초 줄이면 **초당 회복량이 2배**가 될 수도 있다
   (1.0 → 0.5 는 −50%). 해석 A 의 "5.0 → 4.5"(−10%)보다 훨씬 강하다.
   틱당 회복량(`NORMAL_HEAL_PER_TICK = 10`)을 함께 조정해야 한다.

→ **이래서 해석 A 를 권장한다.** 값이 강하지 않고, 코드 변경이 3줄이며, GE 에셋을 안 건드린다.

---

## 8. UI / 문구

### 8.1 스탯 표시명 (`UDRUpgradeUILibrary`)

`GetStatDisplayName()` 은 enum 의 `UMETA(DisplayName)` 을 그대로 쓰므로 §4.1 만으로 끝난다.
`GetStatShortName()`(카드용 5자 이내)에 추가할 것은 **1건뿐**이다.

```cpp
    case EDRUpgradeStat::HealthRegenDelay:
        return LOCTEXT("StatShort_RegenDelay", "회복 시작");       // 정식: 회복 시작 시간 (7자 → 초과)

    // MaxHealth("최대 체력")는 5자라 기본 폴백(GetStatDisplayName)으로 충분하다 — 항목 추가 불필요
```

### 8.2 `EDRUpgradeResult` — 변경 없음

`WrongClass` 는 공용 칩에서는 발생하지 않고, 전용 칩에서는 그대로 유효하다. 새 결과 코드가 필요 없다.

### 8.3 시간 단위 표기 (배관 D)

⑤의 값 `−0.5` 는 화면에 `회복 시작 시간 -0.5` 로만 나와서 **"-0.5%"인지 "-0.5초"인지 구분이 안 된다.**
`DRIsTimeStat()` 으로 "초"를 붙인다.

```cpp
// DRChipCatalog.cpp - FDRUpgradeModifier::GetDetailText()
 return FText::Format(
-    LOCTEXT("ModifierFlatFmt", "{0} {1}{2}"),
-    StatName, Sign, FText::AsNumber(FMath::Abs(Value), &Options));
+    DRIsTimeStat(Stat)
+        ? LOCTEXT("ModifierFlatSecFmt", "{0} {1}{2}초")
+        : LOCTEXT("ModifierFlatFmt",    "{0} {1}{2}"),
+    StatName, Sign, FText::AsNumber(FMath::Abs(Value), &Options));
```

결과: `회복 시작 시간 -0.5초`, `최대 체력 +100`, `최대 물 +50%`.

> #### 소수 자릿수는 이번에는 안 건드려도 된다 (예전 ⑥번 때문에 필요했던 항목)
>
> `MakeValueFormat()` 과 `GetDetailText()` 가 둘 다 `SetMaximumFractionalDigits(1)` 이라
> **0.05 단위 값은 0.1 로 반올림된다.** 칩 ⑥(−0.05초)이 있을 때는 곧바로 오표기였지만,
> 이번 5종은 최소 단위가 **0.1**(⑤의 0.5)이라 **1자리로 정확히 표시된다.**
>
> 다만 **0.05 스케일 칩을 나중에 하나라도 추가하면 즉시 터진다.** 두 곳을 2자리로 올려두는 것은
> 기존 표기(`+20`, `+0.5`, `12%`)를 **전혀 바꾸지 않으므로**(`MinimumFractionalDigits = 0` 이라 후행 0 이 안 붙는다)
> 지금 같이 고쳐두는 편을 권장한다. 선택 사항이다.
>
> ```cpp
> // DRUpgradeUILibrary.cpp 의 MakeValueFormat() 과
> // DRChipCatalog.cpp 의 FDRUpgradeModifier::GetDetailText() ★둘 다★
> - Options.SetMaximumFractionalDigits(1);
> + Options.SetMaximumFractionalDigits(2);
> ```

### 8.4 칩 카드 표기

| 칩 | `ShortLabelTop` / `Bottom` (자동 생성) | 비고 |
|---|---|---|
| ① | 최대 체력 / +100 | `MakeShortLabel()` 의 2번 규칙(첫 비-단점 모디파이어)으로 자동 |
| ② | — | **모디파이어가 없어 3번 규칙(DisplayName)으로 폴백한다.** `EffectSummary` 를 `"UI 표시"` 처럼 2토막(`이름 + 수치`)으로 쓰거나, 카드가 허전하면 전용 아이콘을 쓴다 |
| ③ | 최대 물 / +50% | 자동 |
| ④ | 이동 속도 / +10% | 자동 |
| ⑤ | 회복 시작 / -0.5초 | 자동 (§8.3 적용 후) |

**"공용" 뱃지** (`FDRChipViewModel::bCommonChip`) — `WBP_UpgradeChip` 에 작은 텍스트/아이콘을 추가한다.
없어도 동작하지만, 로봇을 바꿔도 같은 칩이 보이는 이유를 알려주는 편이 좋다.

**칩 ② 해제 확인 토스트** — `WBP_UpgradeToast` 로
"계기판 모듈을 제거하면 전투 중 체력·물·스킬 UI 가 보이지 않습니다" 를 띄운다.
(`WBP_UpgradeScreen::HandleDropOnSlot` 의 실패 토스트 경로를 재사용)

### 8.5 현지화

모든 신규 문구는 **LOCTEXT** 로 작성한다 (Plan4 규칙).
`DA_ChipCatalog` 의 `DisplayName` / `EffectSummary` / `Description` 은 에셋의 FText 라
**한국어로 입력하고 영어는 `Localization Dashboard` 수집 대상**으로 남긴다.
새 LOCTEXT 키 접두사: `StatShort_`, `Modifier*Fmt`, `ChipToast_`.

---

## 9. 멀티플레이어 검증 포인트

| 항목 | 위험 | 확인 방법 |
|---|---|---|
| 칩 목록 복제 | 이미 검증된 경로 (Plan2 §7.1) | PIE 2인, 클라가 칩 장착 → 서버 로그에 `SanitizeLoadout` 결과 |
| ① 컨테이너 체력 | 서버/클라가 **각자** 계산 → 값이 어긋나면 UI 와 실제 체력이 다름 | 양쪽에서 `MaxHealth` 출력. `DA_GameBalanceConfig.NumContainers` 와 `BP_DRCharacter` CDO 가 **둘 다 4** 인지 확인 |
| ② HUD 표시 | 클라 전용 UI. PlayerState 미도착 시 깜빡임 | 클라 입장 직후 HUD 가 한 번 사라졌다 나타나지 않는지. `RefreshHudFeatureVisibility()` 의 "PS 없으면 true" 폴백이 이걸 막는다 |
| ③④ 스탯 GE | 서버가 GE 를 적용하고 어트리뷰트가 복제된다 | 클라에서 `MaxWater` / `MaxWalkSpeed` 확인 |
| ⑤ 회복 타이머 | 서버 전용이라 안전 | 서버 로그로 `GetEffectiveCombatExitDelay()` 출력 |
| 리슨 서버 호스트 | 호스트는 서버이자 클라 | 호스트/클라 양쪽에서 5개 칩 전부 1회씩 검증 |

> **칩 ⑥이 빠지면서 멀티 리스크가 크게 줄었다.** 몽타주 배속은 머신마다 애니메이션이 갈릴 수 있는
> 유일한 항목이었고, 나머지 5종은 전부 **기존 복제 경로(어트리뷰트 / PlayerState)** 위에 얹힌다.

---

## 10. 마일스톤

| 마일스톤 | 내용 | 산출물 | 예상 |
|---|---|---|---|
| **N1** | 배관 A·B·C·D | `DRUpgradeTypes.h`, `DRProgressionTypes.h`, `DRProgressionConfig.h/.cpp`, `DRChipCatalog.cpp`, `DRGameInstance.h/.cpp`, `DRGameplayTags.h/.cpp`, `DRUpgradeUILibrary.cpp` | 0.5일 |
| **N2** | 칩 ①③④ | `DRCharacter.cpp` 3줄 + 카탈로그 3항목 | 0.5일 |
| **N3** | 칩 ② | `DRPlayerState.h/.cpp`, `OverlayWidgetController.h/.cpp`, `WBP_StageOverlay` | 0.5일 |
| **N4** | 칩 ⑤ | `DRPlayerState.h/.cpp` | 0.2일 |
| **N5** | 에셋 + 밸런스 + 테스트 | `DA_ChipCatalog`, `DA_ProgressionConfig`, §13 전 항목 | 0.5일 |

**합계 ≈ 2.2일** (칩 ⑥ 포함 시 5.7일이었다).
**의존 관계**: N1 → (N2 ∥ N3 ∥ N4) → N5.
**최소 컷(시연 우선)**: N1 → N2 → N3 → N5. ⑤ 없이도 "공용 칩 + 기본 장착 + HUD 트레이드오프"가 성립한다.

---

## 11. 변경 파일 체크리스트

### C++ — 신규
없음. (칩 ⑥ 제외로 `DRAnimNotify_MontageEvent` 신설이 불필요해졌다.)

### C++ — 수정

| 파일 | 변경 | 마일스톤 |
|---|---|---|
| `Public/Game/DRUpgradeTypes.h` | `EDRUpgradeStat` +2, `DRIsLowerBetter` 갱신, `DRIsTimeStat` 신설, `FDRUpgradeChipDefinition::{bCommonChip, GrantedTags, IsUsableBy, HasAnyEffect}`, `FDRUpgradeRuntime::{GrantedTags, HasFeature, Reset}` | N1 |
| `Public/Game/DRProgressionTypes.h` | `FDRClassUpgradeState::bDefaultsApplied`, `FDRChipViewModel::bCommonChip` | N1 |
| `Public/Game/DRProgressionConfig.h` / `.cpp` | `InitialFreeSlots`, `DefaultEquippedChips`, `GetSlotUnlockCostForClass()` 수정, 기본칩 Tier 검증 | N1 |
| `Public/Game/DRChipCatalog.h` / `.cpp` | `IsUsableBy` 3곳 교체, `ResolveLoadout` 의 `GrantedTags` 누적, `ValidateCatalog` 규칙 3건, `GetDetailText` 시간 단위 | N1 |
| `Public/Game/DRGameInstance.h` / `.cpp` | `IsUsableBy` 3곳 교체, `ApplyDefaultLoadout()` 신설, `EnsureProgressInitialized()` 훅, `RefundSlot()` 무료칸 가드, `MakeChipViewModel` 1줄, `DebugResetDefaultLoadout()` | N1 |
| `Public/DRGameplayTags.h` / `.cpp` | `Upgrade.Feature.HUD.Vitals` / `.SkillIcons` | N1 |
| `Private/UI/DRUpgradeUILibrary.cpp` | `GetStatShortName` +1, (선택) `MakeValueFormat` 소수 2자리 | N1 |
| `Private/Character/DRCharacter.cpp` | `RefreshUpgradeEffects()` — MaxHealth 총량 → 칸 분배 (3줄) | N2 |
| `Public/Player/DRPlayerState.h` / `.cpp` | `OnUpgradeRuntimeChanged`, `HasUpgradeFeature()`, `GetEffectiveCombatExitDelay()`, `MinCombatExitDelay`, `EnterCombat` 2곳 | N3 · N4 |
| `Public/UI/WidgetController/OverlayWidgetController.h` / `.cpp` | `OnHudFeatureVisibilityChanged`, `RefreshHudFeatureVisibility()`, 바인딩/언바인딩 | N3 |

### 콘텐츠 / 에셋

| 에셋 | 변경 | 마일스톤 |
|---|---|---|
| `DA_ChipCatalog` | 공용 칩 5종 추가, 기존 `GR.Stat.*` 3종 삭제(§3.2) | N5 |
| `DA_ProgressionConfig` | `InitialFreeSlots = 2`, `DefaultEquippedChips = [CM.Stat.MaxHealth, CM.Util.HudDisplay]`, `SlotCosts` 재튜닝 | N5 |
| `WBP_StageOverlay` | `OnHudFeatureVisibilityChanged` 바인딩, Visibility 토글, 스킬아이콘 생성 후 재적용 | N3 |
| `WBP_UpgradeChip` | "공용" 뱃지 (선택) | N5 |
| 칩 아이콘 5종 (`UTexture2D`) | `Content/DaeRuneAssets/UI/Upgrade/` — 기존 `SpeedIcon` 옆에 추가 | N5 |

> **어빌리티 블루프린트는 전혀 건드리지 않는다.** (칩 ⑥ 제외의 가장 큰 이득)

---

## 12. 에디터 작업 순서 (STEP)

### STEP 0. 선행 확인
- [ ] `DA_GameBalanceConfig.PlayerContainer` 의 `NumContainers` / `ContainerHealth` 가 **4 / 100**
- [ ] `BP_DRCharacter` (및 파생 3종) CDO 의 `NumContainers` / `ContainerHealth` 도 **4 / 100**
      → 서버/클라가 다른 값을 쓰면 칩 ①이 머신마다 다른 체력을 만든다
- [ ] `DA_ProgressionConfig` 가 `UDRGameInstance` BP 에 지정돼 있는지
- [ ] `GE_Upgrade_Stats` 가 `BP_DRCharacter.UpgradeStatEffectClass` 에 지정돼 있는지

### STEP 1. C++ 컴파일 (N1~N4)
§11 의 C++ 항목을 순서대로 적용하고 빌드 통과를 확인한다.

### STEP 2. 칩 아이콘 임포트
`Content/DaeRuneAssets/UI/Upgrade/` 에 5종. 기존 텍스처 설정을 따른다
(Texture Group = UI, sRGB = on, Compression = UserInterface2D, Mip Gen = NoMipmaps).

### STEP 3. `DA_ChipCatalog` 작성
1. 기존 `GR.Stat.Container` / `GR.Stat.Water` / `GR.Stat.Speed` **삭제**
2. §3.1 표대로 5종 추가. `bCommonChip = true` 를 **빠뜨리지 말 것**
3. 우클릭 → `Validate Catalog` (MaxSlots = 6) → 오류 0 확인

### STEP 4. `DA_ProgressionConfig`
```
InitialFreeSlots      = 2
DefaultEquippedChips  = [ CM.Stat.MaxHealth, CM.Util.HudDisplay ]
SlotCosts             = [ 0, 0, 1100, 1500, 1900, 2400 ]   ← 앞 2칸은 무료라 미사용
```
→ 저장 시 `PostEditChangeProperty` 가 예산 검증 로그를 남긴다. **경고 0** 확인.

### STEP 5. `WBP_StageOverlay` 배선 (§7.2.4)

### STEP 6. 세이브 초기화 후 최초 실행 검증
프로젝트의 `Saved/SaveGames/DaeRunePlayerProgress.sav` 를 삭제하고 PIE.
- [ ] 슬롯 1·2 가 열려 있고 칩 ①② 가 꽂혀 있다
- [ ] HUD 가 보인다
- [ ] 최대 체력이 500 이다
- [ ] 업그레이드 장치는 여전히 잠겨 있다 (스테이지1 미클리어)

### STEP 7. 치트로 전체 흐름 확인
콘솔: `DRUnlockUpgrade` → 로비 장치 → 칩 ② 제거 → 스테이지 입장 → **HUD 가 없다** → 로비 복귀 → 재장착

---

## 13. 테스트 계획

### 13.1 단일 / 로컬

| ID | 항목 | 기대 |
|---|---|---|
| T-1 | 세이브 삭제 후 첫 실행 | 슬롯 2칸 해금, 칩 ①② 장착, `bDefaultsApplied = true`, 재화 0 |
| T-2 | 칩 ① 장착/해제 | MaxHealth 500 ↔ 400. 컨테이너 경계 125 ↔ 100. **현재 체력 비율 유지**(공짜 회복 없음) |
| T-3 | 오염 진입 → 해제 | 해제 후 MaxHealth 가 **500 으로 복귀** (업그레이드 증발 없음) |
| T-4 | 칩 ③ | MaxWater ×1.5. 물 비율 유지 |
| T-5 | 칩 ④ | `MaxWalkSpeed` ×1.1. **부품 운반 중에도 적용되는가** 실측 (§7.4) |
| T-6 | 칩 ⑤ | 피격 후 회복 시작까지 4.5초 (스톱워치 or 로그) |
| T-7 | 칩 ② 제거 | 스테이지 HUD 의 체력·물·스킬아이콘이 **Collapsed**. 목표/웨이브 UI 는 남는다 |
| T-8 | 칩 ② 제거 후 사망 → 관전 | 관전 대상 HUD 가 **그 대상의 칩 상태**를 따른다 |
| T-9 | 기본 칩 제거 후 재실행 | **다시 꽂히지 않는다** (`bDefaultsApplied` 동작) |
| T-10 | `DebugResetDefaultLoadout` | 기본 칩이 빈 칸에 다시 꽂힌다 |
| T-11 | 무료 슬롯 환불 시도 | `NoSlotToRefund`. 유료 슬롯만 환불됨 |
| T-12 | 로봇 3종 전환 | 세 로봇 모두 목록에 공용 칩 5종이 보인다. 장착 상태는 **각자 따로** |
| T-13 | 카탈로그에서 칩 삭제 후 재실행 | `SanitizeClassState` 가 비우고 재화 환급, 크래시 없음 |
| T-14 | 칩 ⑤ 툴팁 표기 | `회복 시작 시간 -0.5초` (단위 "초" 포함, §8.3) |

### 13.2 멀티플레이어 (PIE 3인, 리슨 서버)

| ID | 항목 | 기대 |
|---|---|---|
| M-1 | 클라가 칩 5종 장착 후 스테이지 입장 | 서버 `SanitizeLoadout` 로그에 "정화" 없음 |
| M-2 | 칩 ① | 클라/서버 양쪽 `MaxHealth` = 500 일치 |
| M-3 | 칩 ③④ | 클라에서 `MaxWater` / 이동속도가 서버와 일치 |
| M-4 | 칩 ② | 자기 HUD 만 사라진다. 다른 플레이어 HUD 는 영향 없음 |
| M-5 | 호스트(리슨 서버) | 5종 전부 클라와 동일 동작 |
| M-6 | 스테이지 도중 칩 교체 시도 | `OpenUpgradeScreen()` 이 `IsInLobby()` 가드로 차단 |

### 13.3 회귀 (칩을 하나도 안 낀 상태)

| ID | 항목 | 기대 |
|---|---|---|
| R-1 | 전 칩 해제 후 플레이 | MaxHealth 400, MaxWater/이속 원본, 회복 5초 — **개편 전과 완전히 동일** |
| R-2 | 기존 정원로봇 전용 칩 세이브 | 삭제된 칩이 정화되고 재화 환급, 경고 로그 1회 |
| R-3 | 튜토리얼 | HUD 가 항상 보인다 (칩 무관) |
| R-4 | 기존 칩(Plan2 의 `SkillDamage` 등) 표기 | `MaxValueFormat` 을 건드렸다면 기존 수치 표기가 그대로인지 (§8.3) |

---

## 14. 열린 결정 사항 (확인 필요)

| ID | 질문 | 이 문서의 가정 | 다르면 바뀌는 것 |
|---|---|---|---|
| **O-1** | "자동 체력 회복 타이머"가 **전투 이탈 대기(5초)** 인가, **회복 틱 주기** 인가? | 전투 이탈 대기 (§7.5) | §7.5.5 의 대체안으로 교체. GE 에셋 수정 + 밸런스 재조정 필요 |
| **O-2** | 기본 장착 2종이 6칸 중 2칸을 먹는 것이 맞는가? (= 무료 2칸 시작) | 맞다 (D-2) | "빼도 칸이 안 생긴다"면 별도 고정 슬롯 UI 가 필요하고, 트레이드오프 설계가 사라진다 |
| **O-3** | 체력 UI 와 물 UI 를 **따로** 끌 수 있어야 하는가? | 아니다 — 한 위젯이라 묶는다 (§2.6) | `WBP_HealthWaterBar` 를 둘로 쪼개고 태그 1개 추가 |
| **O-4** | 칩 ①로 **최대 체력이 500** 이 되는 게 맞는가? (기본 장착이라 사실상 기본값 상승) | 맞다. 빼면 오늘과 같은 400 | 기본값을 낮춰 400 을 유지하려면 `GE_DRPrimaryAttributes` / `DA_GameBalanceConfig` 재튜닝 |
| **O-5** | 기존 `GR.Stat.*` 3종을 삭제하는가? | 삭제 (§3.2) | 남기려면 수치 차등 + Tier 상향 |
| ~~**O-6**~~ | ~~★칩 ⑥ 제외로 생긴 공백★ — 슬롯 6칸에 꽂을 칩이 5종(기본 2 제외 시 3종)뿐이다~~ | **✅ 해소됨 (2026-09-22)** — §17 의 돌파 칩 3종이 들어오면서 유료 4칸에 고를 칩이 **6종**이 됐다. 게다가 §17.9 의 상호작용 매트릭스가 "아무거나 다 끼면 됨"을 막는다 | `MaxSlots` 축소는 더 이상 검토하지 않는다 |
| **O-7** | 칩 5종의 `RequiredSlotTier` (1,1,2,2,3) | 기본 2종만 1 필수, 나머지는 튜닝값 | 자유 조정. 단 기본 2종은 반드시 ≤ `InitialFreeSlots` |
| **O-8** | 소수 표기를 2자리로 미리 올려둘 것인가? | 권장하되 **선택** (§8.3) | 안 올리면 향후 0.05 스케일 칩 추가 시 즉시 오표기 |

---

## 15. 리스크 / 알려진 함정

| # | 함정 | 증상 | 방지 |
|---|---|---|---|
| 1 | `IsUsableBy` 교체를 6곳 중 일부만 함 | 장착은 되는데 **다음 실행에 사라짐** / **멀티에서만 효과 없음** | §5 의 grep 검증을 반드시 실행 |
| 2 | `FDRUpgradeRuntime::Reset()` 에 `GrantedTags.Reset()` 누락 | 칩 ②를 빼도 HUD 가 계속 보인다 | §4.3 코드 그대로 사용 |
| 3 | `EDRUpgradeStat` 에 값을 **중간에** 삽입 | `DA_ChipCatalog` 의 기존 칩 효과가 다른 스탯으로 밀린다 | D-4 — 항상 `Count` 바로 앞 |
| 4 | 기본 칩의 `RequiredSlotTier > InitialFreeSlots` | 다음 실행에 조용히 해제됨 | §6.4 의 `ValidateCurrencyBudget` 검사 추가 |
| 5 | `ApplyDefaultLoadout` 을 `SanitizeClassState` **뒤에** 호출 | 배열 길이 0 상태라 배치 실패 | §6.1 의 순서 고정 |
| 6 | `SaveVersion` 을 올림 | **테스터 전원의 재화·슬롯·칩 초기화** | §4.7 — 올리지 않는다 |
| 7 | `NumContainers` 가 서버/클라 불일치 | 칩 ① 적용 후 체력 바와 실제 체력이 다름 | §12 STEP 0 |
| 8 | 칩 ② 제거 후 되돌릴 방법이 없다고 착각 | 테스트 중 "UI 가 사라져서 못 고침" | §6.6 `DebugResetDefaultLoadout` + 업그레이드 화면은 HUD 와 무관하게 열린다 |
| 9 | `EnterCombat()` 의 `SetTimer` 를 한 곳만 수정 | 첫 피격만 4.5초, 연속 피격 시 5.0초 | §7.5.2 — 두 곳 모두 |
| 10 | 칩 종류 부족 | 슬롯을 사도 꽂을 게 없어 재화 시스템이 무의미해짐 | §14 O-6 을 먼저 결정 |

---

## 16. 향후 확장 (Out of Scope, 메모)

- **로봇 전용 칩 세트** — Plan2 §6.2 의 `GR.*` / `VM.*` / `RV.*` 계열.
  `SkillDamage` / `SkillWaterCost` / `SkillCooldown` / `SkillProjectileCount` 배관이 **이미 전부 완성**돼 있어
  카탈로그 항목만 쓰면 된다. **O-6 의 1순위 해법.**
- **돌파 칩(Ascension)** — 장점 + 단점(`bIsDrawback`) 조합. 배관 완성 상태.
- **컨테이너 1칸 추가 칩** — `EDRUpgradeStat::ContainerCount` + `WBP_ContainerHealthBar` 동적 생성 (§7.1 대안)
- **HUD 세분화 칩** — 미니맵 / 조준선 / 데미지 텍스트 표시도 피처 태그로 (`Upgrade.Feature.HUD.*` 가 이미 확장 가능)
- **공용 돌파 칩** — `bCommonChip` + `Category = Ascension` 조합이 이미 성립한다
- **로봇별 기본 장착 차등** — `DefaultEquippedChips` 를 `TMap<EPlayerCharacterClass, ...>` 로 승격
- **공격 선딜/후딜 칩** — 부록 A

---

## 부록 A. 보류된 "공격 선딜/후딜" 칩 조사 결과

> 1차 개정에서 범위 밖으로 뺐지만, **조사 내용은 되살릴 때 그대로 쓸 수 있으므로** 결론만 남긴다.
> 되살리려면 이 부록을 §7.6 으로 승격하고 `EDRUpgradeStat` 에 스탯 2개를 추가하면 된다.

### A.1 "애니메이션 길이로 만든 딜레이"의 실체

`.uasset` 노드 구성을 직접 확인한 결과다.

| 스킬 | 선딜의 실체 | 후딜의 실체 |
|---|---|---|
| `GA_ClawSwipe` / `GA_WaterPump` / `GA_SeedCannon` (정원) | 몽타주 시작 → `AN_MontageEvent` 노티파이(`Event.Montage.*`) | 노티파이 → `PlayMontageAndWait::OnCompleted` → `EndAbility` |
| `GA_VendingMachineBasicAttack` (자판기) | **없음** (`ExecuteShot()` 즉시 발사) | **`BaseFireInterval` = 0.3초 타이머** (`GetCurrentFireInterval()`) |
| `GA_VacuumAirShot` (청소기) | **없음** (차지 해제 즉시 `FireShot`) | **없음** (즉시 `EndAbility`) |
| `GA_VacuumDash` / `GA_VacuumJetJump` | — | 쿨다운 GE (`SkillCooldown` 칩이 이미 담당) |

→ **"모든 공격"이라 해도 실제로 몽타주 길이가 딜레이인 것은 정원로봇 3종뿐이다.**
후딜 구간에는 `BlockAbilitiesWithTag` 로 다른 스킬도 막혀 있어(`OnGiveAbility` 가 `AbilityTags` 를
`ActivationOwnedTags` 에 머지) **진짜 경직 시간**이 맞다.

### A.2 결론 — 답은 "타이머 단축"이 아니라 "배속"

애니메이션을 그대로 두고 타이머만 줄이면 **투사체가 팔을 휘두르기 전에 나간다.**
딜레이를 줄이려면 **해당 구간의 몽타주 재생 속도를 올려야** 한다.

| 안 | 방법 | 정확도 | 작업량 |
|---|---|---|---|
| **A. 단일 배속** | 몽타주 전체를 `Rate = L / (L − 0.1)` 로 재생 | 총합 0.1초 정확, 구간 배분은 비례 (실측 오차 ≈ 11ms, 프레임보다 작음) | **작음** — `PlayRate` 핀 2개 × 스킬 수 |
| **B. 구간 배속** | 선딜은 `R_pre`, 타격 노티파이 수신 후 `ASC->CurrentMontageSetPlayRate(R_post)` | 정확히 0.05 / 0.05 | 중간 |
| **C. 타이밍 데이터 분리** | 선딜/후딜을 데이터로 선언하고 애니가 따라감 | 완전 제어 | 큼 (전 스킬 재작업) |

→ **안 A 로 시작하고, 부족하면 안 B 로 올린다.** 자판기는 몽타주가 아니므로
`GetCurrentFireInterval()` 에서 직접 차감한다(`0.3 → 0.25`). 청소기는 대상이 아니다.

### A.3 확인해 둔 엔진 사실 (UE 5.5 소스 실측)

| 항목 | 위치 | 결론 |
|---|---|---|
| `UAnimSequenceBase::Notifies` | `Classes/Animation/AnimSequenceBase.h:42` | `UPROPERTY()` **public**, 주석에 "sorted by time (earliest first)" 명시. 런타임 데이터라 패키징 빌드에서도 접근 가능 |
| `FAnimNotifyEvent::GetTriggerTime()` | `Public/Animation/AnimTypes.h:422` | `ENGINE_API float GetTriggerTime() const;` **public** |
| `FGameplayAbilityRepAnimMontage::PlayRate` | `GameplayAbilityRepAnimMontage.h:35` | **복제됨** (`bSkipPlayRate` 를 켜지 않는 한). 시뮬레이티드 프록시의 3인칭 애니도 같은 속도로 재생 |
| `UAbilitySystemComponent::CurrentMontageSetPlayRate(float)` | `AbilitySystemComponent.h:1477` | public. 소유 클라면 `ServerCurrentMontageSetPlayRate` RPC, 서버면 RepAnimMontageInfo 갱신까지 엔진이 처리 |
| `AnimMontage_UpdateReplicatedData()` | `AbilitySystemComponent_Abilities.cpp:3134` | `RepAnimMontageInfo.PlayRate = Montage_GetPlayRate(...)` 를 매번 채운다 |

→ **배속은 각 머신이 복제된 `EquippedChips` 로 각자 계산하면 일치한다** (Plan2 §4.6 의 설계 배당금).
별도 RPC 가 필요 없고, 엔진의 `PlayRate` 복제가 이중 안전망이 된다.

### A.4 `AN_MontageEvent` 를 C++ 에서 식별하는 법

타격 시각을 자동 측정하려면 노티파이를 찾아야 하는데, `AN_MontageEvent` 는 BP 클래스다.

- **싼 방법**: `FAnimNotifyEvent::NotifyName == "MontageEvent"` 로 매칭 (`AN_` 접두사와 `_C` 접미사가 제거된 이름).
  에셋을 전혀 건드리지 않는다. **권장.**
- **튼튼한 방법**: `UDRAnimNotify_MontageEvent` (C++ 베이스)를 만들어 `AN_MontageEvent` 를 리페어런트하고 `IsA<>` 검사.
  ⚠️ 리페어런트 시 **몽타주에 배치된 노티파이 인스턴스의 `EventTag` 값이 초기화될 수 있다** —
  10개 이상의 몽타주를 다시 채워야 할 수 있으므로 신중히.

### A.5 되살릴 때 주의할 밸런스

절대값 차감(−0.05초)은 **원래 간격이 짧을수록 상대 효과가 커진다.**

| 대상 | 원본 | 칩 적용 | 이득 |
|---|---|---|---|
| 정원로봇 (몽타주 0.9초) | 0.900 | 0.800 | +12% |
| 자판기 (버프 0스택) | 0.300 | 0.250 | **+20%** |
| 자판기 (버프 5스택) | 0.150 | 0.100 | **+50%** |

→ 어빌리티별 `MinRecoveryDelay` 하한(자판기는 0.15 등)으로 극단값을 막는 것이 권장안이었다.

---

# §17. 2차 확장 — 돌파 칩 3종 (⑥ ⑦ ⑧)

> 추가일: 2026-09-22
> §1~§16 의 공용 칩 5종(기본 세트)은 **코드 완료** 상태다. 이 절은 그 위에 얹는 **돌파 칩(Ascension) 3종**이다.
> 앞의 5종이 "수치 하나 바꾸는 칩"이었다면, 이 3종은 **전투 파이프라인 자체를 조건부로 갈아끼우는 칩**이다.
> 그래서 필요한 배관의 성격이 완전히 다르다 — §17.3 을 먼저 읽을 것.

## 17.0 이 3종이 앞의 5종과 다른 점

| | 공용 칩 ①~⑤ | 돌파 칩 ⑥~⑧ |
|---|---|---|
| 효과 형태 | **정적 수치** (`FDRUpgradeModifier` 로 전부 표현 가능) | **조건부·동적 로직** (남은 컨테이너 수, 주변 팀원 수에 따라 매 순간 변함) |
| 적용 지점 | 스폰 시 GE 1장 (`GE_Upgrade_Stats`) | **피해 계산 시점마다** (`ExecCalc_Damage` / `HandleIncomingDamage` / `ExecCalc_WaterCost`) |
| 단점 | 없음 (②만 "칸을 먹는다"가 비용) | **본질적 단점 동반** — 회복 불가 / 즉사급 피격 / 체력 소모 |
| 배관 | 스탯 enum + GE SetByCaller | **주는 피해 축 신설 + 피처 태그 분기 + 오라 컴포넌트** |

> **핵심 판단**: ⑥⑧은 "전투 규칙 분기"라 **코드가 8할**이고 카탈로그는 거들 뿐이다.
> ⑦은 유일하게 **신규 액터 컴포넌트**가 필요하다(주기적 근접 탐지 + GE 부여/해제).
> 이 3종이 들어오면 §14 **O-6(콘텐츠 공백)이 해소된다** — 유료 슬롯 4칸에 고를 칩이 3종 → **6종**이 된다.

---

## 17.1 요구사항 (원문 → 해석)

### 칩 ⑥ — 광폭화 (가칭)

| 원문 | 해석 |
|---|---|
| 입히는 모든 피해량이 200% | **주는** 피해 ×2.0. 스킬뿐 아니라 투사체·DoT 등 **모든 경로** 포함 |
| 체력 컨테이너를 1개 잃을 때마다 피해량이 추가로 증가 (1개당 25%, 최대 300%) | 잃은 칸 수 × 25%p 가산, 상한 ×3.0 → **⚠️ 4칸 구성에서는 상한에 도달할 수 없다 (§17.6.5)** |
| 적의 공격을 받으면 피해량과 관계없이 체력 컨테이너 1칸이 즉시 소모됨 | 피해량 무시. 현재 칸을 **통째로 비운다** (= 칸 바닥까지) |
| 모든 체력 회복이 불가함 | 힐 GE · 자동 재생 · (결정 필요) 오염 정화까지 차단 |

### 칩 ⑦ — 시너지 오라 (가칭)

| 원문 | 해석 |
|---|---|
| 반경 3미터 이내에서 가장 가까운 팀원 1명을 감지 | 300uu 구체 오버랩 → 살아 있는 다른 플레이어 중 **최근접 1명만** |
| 범위 내 팀원과 자신에게 효과 부여 | 버프 대상 = **{자신, 최근접 팀원}** 2명 |
| 받는 피해량 10% 감소, 주는 피해량 10% 증가 | 각각 `HandleIncomingDamage` / `ExecCalc_Damage` 에서 적용 |
| 범위 안에 팀원이 없으면 효과가 해제됨 | **자신 것도 같이 해제된다** (혼자면 아무 효과 없음) |
| 중첩 가능 | 여러 명이 이 칩을 끼고 뭉치면 스택이 쌓인다 (10% × 스택 수) |

### 칩 ⑧ — 피의 계약 (가칭)

| 원문 | 해석 |
|---|---|
| 최대 체력이 200% 증가 | ×3 (400 → 1200). 컨테이너 **용량**을 3배로 (칸 수는 그대로 4) |
| 스킬이 더 이상 물을 소모하지 않음 | `ExecCalc_WaterCost` 의 물 차감분 = 0 |
| 스킬 사용 시 물 대신 체력을 소모 (기존 50% 가 아닌 **물의 100%**) | 체력 차감 = `RequiredWater × 1.0` (기존 부족분 보전은 `× 0.5`) |

---

## 17.2 현재 코드 기준 사실 — 전투 파이프라인 지도

### 17.2.1 `ExecCalc_Damage` = 모든 피해가 지나는 단 하나의 관문

`Source/DaeRune/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp` (104줄, 전부 읽음)

```
SetByCaller(DamageTypeTags) 합산 → Damage
  ↓  [소스측 배율 구간 — 선례가 이미 2개 있다]
  ├ SourceASC 에 Buff.Elite.Roar → Damage *= 1.2      (69~73행)
  └ SourceASC 에 State.Enemy.Phase1 → Damage *= 0.7   (94~99행)
  ↓
IncomingDamage 어트리뷰트에 Additive 출력
```

→ **칩 ⑥의 200%/컨테이너 보너스, 칩 ⑦의 +10% 는 전부 여기에 들어간다.**

> **왜 `EDRUpgradeStat::SkillDamage` 를 안 쓰는가**
> 그건 `UDRDamageGameplayAbility::GetUpgradedDamage()` 에서 **어빌리티 단위로** 적용된다.
> Plan2 §8.5 가 이미 겪었듯이 `UDRVacuumAirShot::Stages[].Damage` 처럼 그 경로를 안 타는 피해원이 있고,
> 디버프 DoT·투사체 직접 주입 등도 빠진다. **"입히는 모든 피해량"** 을 지키려면 ExecCalc 여야 한다.

### 17.2.2 받는 피해 = `UDRPlayerAttributeSet::HandleIncomingDamage()`

현재 처리 순서 (`DRPlayerAttributeSet.cpp`):

| # | 처리 | 비고 |
|---|---|---|
| 1 | `GameplayCue.Player.Damage` 큐 | |
| 2 | `NotifyEnterCombat(Props)` | 전투 진입 → 회복 중단 |
| 3 | 소스가 적이면 AI 전투 시각 갱신 | |
| 4 | **부품 운반 중이면 ×1.5 + 부품 드롭** | |
| 5 | **`EDRUpgradeStat::DamageTaken` 칩 배율** (0 하한) | ★칩 ⑦의 감소가 들어갈 자리 |
| 6 | `ShouldPreventDeath()` → 로비/튜토리얼 클램프 | 체력 1 미만 방지 |
| 7 | `bCorrupted ? ProcessCorruptedDamage : ProcessNormalDamage` | |
| 8 | `ShowFloatingText(Props, LocalIncomingDamage)` | |
| 9 | 디버프 / 탑승 피해 공유 | |

`ProcessNormalDamage()` → `CalculateContainerDamage()` 가 컨테이너 오버플로우(현재 칸 체력의 10% 초과분만 다음 칸으로)를 계산한다.
→ **칩 ⑥의 "칸 통째로 소모"는 이 함수를 우회한다.**

### 17.2.3 체력 회복 경로가 **4개** 있다 — 하나라도 빠뜨리면 칩 ⑥이 반쯤 동작한다

| # | 경로 | 진입점 |
|---|---|---|
| 1 | 힐 GE (`GE_Heal` → `ExecCalc_Heal`) | `UDRPlayerAttributeSet::HandleIncomingHealing()` |
| 2 | 전투 이탈 후 자동 재생 | `ADRPlayerState::StartHealthRegen()` → `GE_HealthRegen` |
| 3 | 재생 틱 계산 | `UMMC_HealthRegen::CalculateBaseMagnitude_Implementation()` |
| 4 | 오염 정화 (초과 힐) | `HandleCorruptionPurification()` / `ExitCorruptedState()` |

> ⚠️ 2번을 막아도 3번이 살아 있으면 GE 가 이미 붙어 있던 상황에서 틱이 계속 돈다.
> **2·3 둘 다** 막는 것이 정석이다(진입 차단 + 계산 차단).

### 17.2.4 `ExecCalc_WaterCost` = 물/체력 코스트의 단일 지점

```cpp
if (CurrentWater >= RequiredWater) { ActualWaterCost = RequiredWater; HealthCost = 0; }
else { ActualWaterCost = CurrentWater;
       HealthCost = FloorToFloat((RequiredWater - CurrentWater) * 0.5f); }   // ★기존 50% 규칙
```

- 체력 차감은 `Health` 어트리뷰트에 **Additive 직접 적용**이다 → `IncomingDamage` 를 타지 않는다.
  즉 **컨테이너 오버플로우·전투 진입·피격 리액션이 발생하지 않는다.** (기존 동작이며 칩 ⑧도 이를 그대로 따른다 — §17.8.3)
- 짝이 되는 사전 검사는 `UDRGameplayAbility::CheckCost()` 에 있다:
  `RequiredHealth = FloorToFloat(WaterShortage * 0.5f); return CurrentHealth > RequiredHealth;`
  → **두 곳의 비율을 같이 바꾸지 않으면 "쓸 수 있다고 했는데 실제로는 못 내는"** 불일치가 난다.

### 17.2.5 오라 선례 — `UDREliteRoar::TickRoarAura()`

`Source/DaeRune/Private/AbilitySystem/Abilities/DREliteRoar.cpp` 에 **그대로 베낄 수 있는 구현**이 있다:

```
타이머(AuraTickInterval) →
  Step 1: OverlapMultiByChannel(ECC_Pawn, 반경+여유) → XY 거리로 재판정해 InRange 집합
  Step 2: BuffedEnemies(TMap<TWeakObjectPtr, FActiveGameplayEffectHandle>) 순회 →
          범위 밖/사망이면 RemoveActiveGameplayEffect + 맵에서 제거
  Step 3: 새로 들어온 대상에 GE 적용 + 핸들 저장
```

칩 ⑦은 이 구조를 **대상만 바꿔서**(적 → 아군 플레이어, 전원 → 최근접 1명) 재사용한다.

### 17.2.6 팀 판정 = 액터 태그

```cpp
bool UDRAbilitySystemLibrary::IsNotFriend(AActor* A, AActor* B)
{
    const bool bBothArePlayers = A->ActorHasTag("Player") && B->ActorHasTag("Player");
    ...
}
```
→ 팀원 판정은 **`ActorHasTag("Player")`** 하나면 된다. `IGenericTeamAgentInterface` 를 끌어올 필요가 없다.

---

## 17.3 설계 개요 — 신규 축 3개

```
[칩 정의]
   ├─ FDRUpgradeModifier[]   숫자 ──────────────┐
   └─ FGameplayTagContainer GrantedTags  스위치 ─┼──┐
                                                 │  │
[FDRUpgradeRuntime]  (복제 안 함, 각 머신이 로컬 구축)
   ├─ GlobalStats[EDRUpgradeStat]  ←── ★축 E★ DamageDealt 추가
   └─ GrantedTags                  ←── ★축 F★ 조건부 로직 스위치
         │
         ├─→ ⑥ ExecCalc_Damage (소스측)          : ×2.0 + 잃은 칸 × 25%p, 상한 ×3.0
         ├─→ ⑥ HandleIncomingDamage / ProcessNormalDamage : 칸 통째로 소모
         ├─→ ⑥ HandleIncomingHealing / StartHealthRegen / MMC : 회복 전면 차단
         ├─→ ⑧ ContainerHealth Percent +2.0       (기존 GE 경로, 코드 0줄)
         └─→ ⑧ CheckCost / ExecCalc_WaterCost     : 물 0, 체력 100%

[★축 G★ 오라]  UDRSynergyAuraComponent (신규 ActorComponent, 서버 전용)
   타이머 → 최근접 팀원 탐지 → GE_Upgrade_Synergy 부여/해제
         │
         └─→ ASC 의 Buff.Upgrade.Synergy ★태그 카운트★ = 스택 수
                ├─→ ExecCalc_Damage (소스측)        : ×(1 + 0.10 × 스택)
                └─→ HandleIncomingDamage (타겟측)   : ×(1 − 0.10 × 스택)
```

### 17.3.1 ★중요★ 두 종류의 태그를 섞지 않는다

| | **칩 피처 태그** (`Upgrade.Feature.*`) | **오라 버프 태그** (`Buff.Upgrade.Synergy`) |
|---|---|---|
| 어디에 있나 | `FDRUpgradeRuntime::GrantedTags` — **ASC 에 없다** | **ASC 에 있다** (GE 의 `GrantedTags`) |
| 조회 | `PlayerState->HasUpgradeFeature(Tag)` | `ASC->GetGameplayTagCount(Tag)` |
| 왜 분리하나 | 칩 태그를 ASC 에 Loose 로 얹으면 어빌리티의 `ActivationBlockedTags` / `BlockAbilitiesWithTag` 와 **의도치 않게 상호작용**한다. 칩은 "내가 무엇을 끼고 있나"이지 "지금 어떤 상태인가"가 아니다 | 오라는 **실제 런타임 상태**이고 스택 수를 세야 하므로 GAS 태그 카운트가 정확히 맞는 도구다 |

---

## 17.4 데이터 모델 변경

### 17.4.1 `EDRUpgradeStat` — 1개 추가 (`DRUpgradeTypes.h`)

```cpp
    // ... 기존 + MaxHealth, HealthRegenDelay ...

    // 캐릭터 단위. 이 캐릭터가 ★주는★ 모든 피해의 배율.
    // ExecCalc_Damage 소스측에서 적용되므로 스킬/투사체/DoT 을 전부 포함한다.
    // (SkillDamage 는 어빌리티 단위라 일부 경로가 새는 것과 대비 — §17.2.1)
    DamageDealt          UMETA(DisplayName = "주는 피해"),

    Count UMETA(Hidden)
```

- `DRIsSkillStat` — **변경 없음** (캐릭터 단위)
- `DRIsLowerBetter` — **변경 없음** (높을수록 좋다)
- `DRIsTimeStat` — **변경 없음**
- `UDRUpgradeUILibrary::GetStatShortName` — "주는 피해"가 5자라 **추가 불필요** (폴백으로 충분)

> `Count` 바로 앞에 넣는다 (D-4). 기존 값 사이에 끼우면 `DA_ChipCatalog` 의 직렬화 값이 밀린다.

### 17.4.2 게임플레이 태그 — 4개 추가 (`DRGameplayTags.h/.cpp`)

```cpp
    // 칩 피처 스위치 (ASC 에 부여되지 않는다 — §17.3.1)
    FGameplayTag Upgrade_Feature_Combat_Berserk;     // ⑥ 광폭화 규칙 일괄
    FGameplayTag Upgrade_Feature_Combat_NoHealing;   // ⑥ 회복 차단 (분리해 두면 단독 재사용 가능)
    FGameplayTag Upgrade_Feature_Combat_Synergy;     // ⑦ 오라 구동 스위치
    FGameplayTag Upgrade_Feature_Combat_BloodCost;   // ⑧ 물 → 체력 코스트

    // 오라 버프 (★이건 GE 가 ASC 에 실제로 부여한다★)
    FGameplayTag Buff_Upgrade_Synergy;
```

```cpp
    GameplayTags.Upgrade_Feature_Combat_Berserk = UGameplayTagsManager::Get().AddNativeGameplayTag(
        FName("Upgrade.Feature.Combat.Berserk"),
        FString("주는 피해 증폭 + 피격 시 컨테이너 1칸 즉시 소모"));

    GameplayTags.Upgrade_Feature_Combat_NoHealing = UGameplayTagsManager::Get().AddNativeGameplayTag(
        FName("Upgrade.Feature.Combat.NoHealing"),
        FString("모든 체력 회복 차단"));

    GameplayTags.Upgrade_Feature_Combat_Synergy = UGameplayTagsManager::Get().AddNativeGameplayTag(
        FName("Upgrade.Feature.Combat.Synergy"),
        FString("근접 팀원 시너지 오라 활성화"));

    GameplayTags.Upgrade_Feature_Combat_BloodCost = UGameplayTagsManager::Get().AddNativeGameplayTag(
        FName("Upgrade.Feature.Combat.BloodCost"),
        FString("스킬 코스트를 물 대신 체력으로 지불"));

    GameplayTags.Buff_Upgrade_Synergy = UGameplayTagsManager::Get().AddNativeGameplayTag(
        FName("Buff.Upgrade.Synergy"),
        FString("시너지 오라 적용 중 (태그 카운트 = 스택 수)"));
```

> **⑥을 태그 2개로 쪼개는 이유**: "회복 불가"는 나중에 다른 칩(예: 흡혈 칩의 단점)에서 단독으로 쓸 수 있다.
> 한 태그에 4가지 규칙을 묶으면 재사용이 불가능해진다.

### 17.4.3 `UDRProgressionConfig` — 튜닝 상수 (`FDRChipTuning`)

동적 계산에 쓰이는 수치는 카탈로그의 `Modifiers` 로 표현할 수 없다(잃은 칸 수·팀원 수에 따라 변하므로).
그렇다고 C++ 하드코딩하면 밸런스 조정 때마다 재컴파일해야 한다.

```cpp
// DRProgressionConfig.h
USTRUCT(BlueprintType)
struct FDRChipTuning
{
    GENERATED_BODY()

    // ===== ⑥ 광폭화 =====
    // 컨테이너 1칸 잃을 때마다 더해지는 주는 피해 배율(가산)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Berserk", meta = (ClampMin = "0.0"))
    float BerserkBonusPerLostContainer = 0.25f;

    // 주는 피해 배율 상한 (기본 200% + 보너스의 합에 걸린다)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Berserk", meta = (ClampMin = "1.0"))
    float BerserkMaxMultiplier = 3.0f;

    // ===== ⑦ 시너지 오라 =====
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "0.0"))
    float SynergyRadius = 300.f;             // 3m

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "0.05"))
    float SynergyTickInterval = 0.25f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "0.0"))
    float SynergyDamageDealtPerStack = 0.10f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SynergyDamageTakenPerStack = 0.10f;

    // 받는 피해 감소의 하한 (0.2 = 아무리 뭉쳐도 최소 20% 는 들어온다)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SynergyMinDamageTakenRatio = 0.2f;

    // 오라 버프 GE (GrantedTags = Buff.Upgrade.Synergy, Infinite, 모디파이어 없음)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Synergy")
    TSoftClassPtr<UGameplayEffect> SynergyBuffEffect;

    // ===== ⑧ 피의 계약 =====
    // 물 소모량 대비 체력 소모 비율 (1.0 = 물의 100%)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BloodCost", meta = (ClampMin = "0.0"))
    float BloodCostHealthRatio = 1.0f;
};
```

`UDRProgressionConfig` 에 필드 하나 추가:

```cpp
    // 동적 칩 효과의 튜닝 상수 (Plan3(공용칩) §17.4.3)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade|Chip Tuning")
    FDRChipTuning ChipTuning;
```

### 17.4.4 칩 정의 3종 (`DA_ChipCatalog`)

전부 `bCommonChip = true`, `RequiredSlotCount = 1`, **`Category = Ascension`**.

| # | `ChipId` | `DisplayName`(예시) | Modifiers | GrantedTags | `Tier` |
|---|---|---|---|---|---|
| ⑥ | `CM.Asc.Berserk` | 과부하 코어 | `DamageDealt` Percent **+1.0** | `Upgrade.Feature.Combat.Berserk`<br>`Upgrade.Feature.Combat.NoHealing` | 5 |
| ⑦ | `CM.Asc.Synergy` | 동조 링크 | (없음) | `Upgrade.Feature.Combat.Synergy` | 4 |
| ⑧ | `CM.Asc.BloodCost` | 피의 계약 | `ContainerHealth` Percent **+2.0** | `Upgrade.Feature.Combat.BloodCost` | 5 |

**문구 예시**

| # | `EffectSummary` | `Description` (단점을 반드시 명시) |
|---|---|---|
| ⑥ | 주는 피해 +100%, 잃은 칸마다 +25% | 체력 회복이 불가능해지고, 피격 시 피해량과 무관하게 체력 컨테이너 1칸을 통째로 잃습니다. |
| ⑦ | 근처 팀원과 함께 공수 +10% | 3m 안에 팀원이 없으면 효과가 사라집니다. 여러 명이 장착하면 중첩됩니다. |
| ⑧ | 최대 체력 +200%, 스킬이 물 대신 체력 소모 | 스킬이 물을 전혀 쓰지 않는 대신, 물 소모량과 **같은 양의 체력**을 소모합니다. |

### 17.4.5 ★`ValidateCatalog()` 수정 필수 — 지금 그대로면 3종 모두 오류가 뜬다★

현재 규칙 (§4.2 에서 추가한 것):

```cpp
if (Chip.Category == EDRChipCategory::Ascension && !Chip.HasDrawback())
{
    OutErrors.Add("돌파 칩인데 단점(bIsDrawback) 모디파이어가 없습니다.");
}
```

⑥⑦⑧의 단점은 **전부 코드 구동**이라 `bIsDrawback` 모디파이어가 하나도 없다 → **셋 다 오류로 잡힌다.**

```cpp
-   if (Chip.Category == EDRChipCategory::Ascension && !Chip.HasDrawback())
-   {
-       OutErrors.Add(... "돌파 칩인데 단점(bIsDrawback) 모디파이어가 없습니다.");
-   }
+   // 돌파 칩은 단점을 동반하는 것이 설계 규칙이다.
+   // 단, 단점이 ★코드 구동★ 인 칩(회복 불가 / 체력 코스트 등)은 모디파이어로 표현되지 않는다.
+   // 그런 칩은 GrantedTags 로 식별하되, 플레이어가 알 수 있도록 Description 을 필수로 요구한다.
+   if (Chip.Category == EDRChipCategory::Ascension)
+   {
+       if (!Chip.HasDrawback() && Chip.GrantedTags.Num() == 0)
+       {
+           OutErrors.Add(FString::Printf(
+               TEXT("%s: 돌파 칩인데 단점 모디파이어도 GrantedTags 도 없습니다 — 단점이 없는 돌파 칩입니다."),
+               *IdString));
+       }
+       if (!Chip.HasDrawback() && Chip.Description.IsEmpty())
+       {
+           OutErrors.Add(FString::Printf(
+               TEXT("%s: 단점이 코드 구동인 돌파 칩은 Description 에 단점을 반드시 적어야 합니다."),
+               *IdString));
+       }
+   }
```

**`FDRChipViewModel::bHasDrawback` 도 같이 고친다** (`UDRGameInstance::MakeChipViewModel`):

```cpp
-   OutViewModel.bHasDrawback = Chip.HasDrawback();
+   // 돌파 칩은 단점이 코드 구동이라 모디파이어가 없을 수 있다 — 분류만으로도 경고 아이콘을 띄운다
+   OutViewModel.bHasDrawback = Chip.HasDrawback() || Chip.Category == EDRChipCategory::Ascension;
```

---

## 17.5 공통 배관 — 소스/타겟측 업그레이드 조회 헬퍼

`ExecCalc_Damage` 는 static 컨텍스트라 `UDRGameplayAbility::GetUpgradeRuntimeFor()` 를 쓸 수 없다.
같은 조회 로직이 **ExecCalc 2곳 + AttributeSet 1곳 + 컴포넌트 1곳** 에 복제될 판이므로 라이브러리로 올린다.

```cpp
// DRAbilitySystemLibrary.h
public:
    /**
     * ASC 소유자의 업그레이드 수치 캐시. 플레이어가 아니면(적/사이트) ★빈 캐시★ 를 돌려준다.
     * 호출부에 분기가 필요 없도록 항상 유효한 참조를 반환한다.
     *
     * 플레이어는 ASC 를 PlayerState 가 소유하므로 OwnerActor 가 곧 PlayerState 다.
     * 아바타 경유 폴백까지 여기서 한 번에 처리한다.
     */
    static const FDRUpgradeRuntime& GetUpgradeRuntimeForASC(const UAbilitySystemComponent* ASC);

    /** 칩 피처 스위치 보유 여부 (Upgrade.Feature.*). ★ASC 태그가 아니다★ — §17.3.1 */
    UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|Upgrade")
    static bool HasUpgradeFeature(const UAbilitySystemComponent* ASC, FGameplayTag FeatureTag);

    /** 컨테이너 정보를 가진 플레이어 어트리뷰트셋. 플레이어가 아니면 nullptr. */
    static const UDRPlayerAttributeSet* GetPlayerAttributeSetFromASC(const UAbilitySystemComponent* ASC);

    /** 튜닝 상수 접근 (Config 미지정 시 기본값 구조체). */
    static const FDRChipTuning& GetChipTuning(const UObject* WorldContextObject);
```

```cpp
// DRAbilitySystemLibrary.cpp
const FDRUpgradeRuntime& UDRAbilitySystemLibrary::GetUpgradeRuntimeForASC(const UAbilitySystemComponent* ASC)
{
    static const FDRUpgradeRuntime EmptyRuntime;
    if (!ASC) return EmptyRuntime;

    // 플레이어: ASC 소유자 = PlayerState
    if (const ADRPlayerState* DRPS = Cast<ADRPlayerState>(ASC->GetOwnerActor()))
    {
        return DRPS->GetUpgradeRuntime();
    }
    // 폴백: 아바타의 PlayerState (ASC 소유 구조가 다른 경우 대비)
    if (const APawn* AvatarPawn = Cast<APawn>(ASC->GetAvatarActor()))
    {
        if (const ADRPlayerState* DRPS = AvatarPawn->GetPlayerState<ADRPlayerState>())
        {
            return DRPS->GetUpgradeRuntime();
        }
    }
    return EmptyRuntime;
}

bool UDRAbilitySystemLibrary::HasUpgradeFeature(const UAbilitySystemComponent* ASC, FGameplayTag FeatureTag)
{
    return GetUpgradeRuntimeForASC(ASC).HasFeature(FeatureTag);
}
```

**중복 제거**: `UDRGameplayAbility::GetUpgradeRuntimeFor(ActorInfo)` 는 이 함수로 위임하게 바꾼다.

```cpp
const FDRUpgradeRuntime& UDRGameplayAbility::GetUpgradeRuntimeFor(const FGameplayAbilityActorInfo* ActorInfo) const
{
    static const FDRUpgradeRuntime EmptyRuntime;
    if (!ActorInfo) return EmptyRuntime;
    return UDRAbilitySystemLibrary::GetUpgradeRuntimeForASC(ActorInfo->AbilitySystemComponent.Get());
}
```

> ⚠️ `ActorInfo->AbilitySystemComponent` 가 비어 있는 CDO 호출 경로가 있다(Plan2 §8.5 의 ★ 주석).
> 위 구현은 null ASC 에 대해 빈 캐시를 돌려주므로 **기존 동작과 동일하다.** 회귀 없음.

---

## 17.6 칩 ⑥ — 광폭화

### 17.6.1 주는 피해 (ExecCalc_Damage 소스측)

기존 소스측 배율 구간(69~73행) **바로 뒤**에 삽입한다. 순서는 아래 표를 따른다.

```cpp
// ExecCalc_Damage.cpp — Execute_Implementation, SourceASC 를 얻은 직후

// ===== 업그레이드 칩: 주는 피해 배율 (Plan3(공용칩) §17.6.1) =====
if (SourceASC)
{
    const FDRGameplayTags& Tags = FDRGameplayTags::Get();
    const FDRUpgradeRuntime& SourceRuntime = UDRAbilitySystemLibrary::GetUpgradeRuntimeForASC(SourceASC);

    // (a) 정적 배율 — DamageDealt 스탯. 칩이 없으면 Apply 가 항등이라 정확히 1.0 이다.
    float OutgoingMultiplier = SourceRuntime.Apply(EDRUpgradeStat::DamageDealt, 1.f);

    // (b) 광폭화 — 잃은 컨테이너 수만큼 가산한 뒤 상한을 건다.
    //     ★상한은 (a)+(b) 합에만 건다.★ 시너지(c)는 별개 축이라 상한 밖에서 곱한다.
    if (SourceRuntime.HasFeature(Tags.Upgrade_Feature_Combat_Berserk))
    {
        if (const UDRPlayerAttributeSet* SourcePlayerAS =
                UDRAbilitySystemLibrary::GetPlayerAttributeSetFromASC(SourceASC))
        {
            const FDRChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(SourceASC->GetAvatarActor());

            // GetCurrentContainerIndex() 는 사망 시 -1 을 돌려준다 → 0 으로 바닥을 친다.
            const int32 ContainerIndex = FMath::Max(0, SourcePlayerAS->GetCurrentContainerIndex());
            const int32 LostContainers =
                FMath::Max(0, SourcePlayerAS->GetNumContainers() - 1 - ContainerIndex);

            OutgoingMultiplier += Tuning.BerserkBonusPerLostContainer * LostContainers;
            OutgoingMultiplier = FMath::Min(OutgoingMultiplier, Tuning.BerserkMaxMultiplier);
        }
    }

    // (c) 시너지 오라 — ★이건 ASC 태그 카운트★ (칩 피처 태그가 아니다)
    const int32 SynergyStacks = SourceASC->GetGameplayTagCount(Tags.Buff_Upgrade_Synergy);
    if (SynergyStacks > 0)
    {
        const FDRChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(SourceASC->GetAvatarActor());
        OutgoingMultiplier *= (1.f + Tuning.SynergyDamageDealtPerStack * SynergyStacks);
    }

    Damage *= OutgoingMultiplier;
}
```

**적용 순서 확정** (섞이면 수치가 크게 달라진다):

| 순서 | 배율 | 근거 |
|---|---|---|
| 1 | `DamageDealt` 스탯 (⑥의 ×2.0) | 칩 정적 수치 |
| 2 | + 광폭화 컨테이너 보너스 → **상한 ×3.0** | 스펙이 "최대 300%" 라고 못박음 |
| 3 | × 시너지 (1 + 0.1×스택) | 별개 출처라 상한 밖에서 곱한다 |
| 4 | × Elite Roar (1.2) / Phase1 (0.7) — **기존 코드** | 적 전용이라 플레이어와 겹치지 않는다 |

> **투사체 주의**: 배율이 평가되는 시점은 **명중 시점**(ExecCalc 실행)이지 발사 시점이 아니다.
> 씨앗을 쏘고 나서 맞기 전에 컨테이너를 하나 잃으면 **더 센 피해로 명중한다.**
> 의도한 동작으로 본다(광폭화의 취지에 맞다). 발사 시점 고정이 필요하면 투사체 스폰 시
> `DamageEffectParams.BaseDamage` 에 배율을 미리 곱해 넣고 ExecCalc 에서는 건너뛰어야 하는데,
> "모든 피해 경로"를 잡는다는 목표와 충돌하므로 **하지 않는다.**

### 17.6.2 피격 시 컨테이너 1칸 즉시 소모

`ProcessNormalDamage()` 안에서 처리한다. `HandleIncomingDamage()` 본체가 아니라 여기인 이유:
- 오염 상태(`ProcessCorruptedDamage`)에는 컨테이너 개념이 없다
- 배율 계산(부품 1.5배 / DamageTaken / 시너지)이 끝난 뒤라 "피해량 무관" 의미가 분명해진다

```cpp
void UDRPlayerAttributeSet::ProcessNormalDamage(const FEffectProperties& Props, float Damage)
{
    float NewHealth;

    // ★Plan3(공용칩) §17.6.2★ 광폭화: 피해량과 무관하게 현재 컨테이너를 통째로 비운다.
    if (UDRAbilitySystemLibrary::HasUpgradeFeature(
            Props.TargetASC, FDRGameplayTags::Get().Upgrade_Feature_Combat_Berserk))
    {
        const int32 ContainerIndex = FMath::Max(0, GetCurrentContainerIndex());
        NewHealth = ContainerIndex * ContainerHealth;

        // 로비/튜토리얼에서는 죽지 않아야 한다 — 칸을 비우되 체력 1은 남긴다.
        if (NewHealth <= 0.f && ShouldPreventDeath())
        {
            NewHealth = 1.f;
        }
    }
    else
    {
        NewHealth = CalculateContainerDamage(GetHealth(), Damage);
    }

    SetHealth(FMath::Clamp(NewHealth, 0.f, GetMaxHealth()));

    if (NewHealth <= 0.f)
    {
        EnterCorruptedState(Props);
    }
    else
    {
        ApplyHitReactAndKnockback(Props);
    }
}
```

**컨테이너 경계 계산 검증** (`NumContainers = 4`, `ContainerHealth = 100`):

| 피격 전 체력 | `GetCurrentContainerIndex()` | 피격 후 체력 | 결과 |
|---|---|---|---|
| 400 (만피) | 3 | 300 | 1칸 소모 |
| 350 | 3 | 300 | 칸 바닥까지 |
| 300 | 2 (경계 보정) | 200 | 1칸 소모 |
| 150 | 1 | 100 | 1칸 소모 |
| 100 | 0 (경계 보정) | 0 | **오염 진입** |
| 50 | 0 | 0 | **오염 진입** |

> `GetCurrentContainerIndex()` 의 경계 보정(`IsNearlyEqual(H, Idx*CH)` → `Idx-1`)이 있어
> **칸 경계에 정확히 서 있을 때도 다음 칸이 아니라 현재 칸을 비운다.** 의도한 동작이다.

**⚠️ 데미지 텍스트 불일치**: `ShowFloatingText(Props, LocalIncomingDamage)` 는 **원래 피해량**을 띄운다.
실제로 깎인 체력과 숫자가 다르다(예: "37" 이 떴는데 100 이 깎임).
→ `ProcessNormalDamage` 가 실제 감소량을 돌려주게 바꾸고 그 값을 띄우거나,
광폭화일 때는 전용 큐/텍스트("컨테이너 파손")를 띄운다. **§17.13 O-11 로 남긴다.**

### 17.6.3 모든 체력 회복 차단 — 4개 경로 전부

**경로 1: 힐 GE**

```cpp
void UDRPlayerAttributeSet::HandleIncomingHealing(const FEffectProperties& Props)
{
    const float LocalIncomingHealing = GetIncomingHealing();
    SetIncomingHealing(0.f);

    if (LocalIncomingHealing <= 0.f) return;

    // ★Plan3(공용칩) §17.6.3★ 회복 차단 칩: 들어온 힐을 그대로 버린다.
    // ★IncomingHealing 을 0 으로 비운 ★뒤에★ 반환해야 다음 힐이 누적되지 않는다.
    if (UDRAbilitySystemLibrary::HasUpgradeFeature(
            Props.TargetASC, FDRGameplayTags::Get().Upgrade_Feature_Combat_NoHealing))
    {
        return;
    }

    // ... 기존 오염 정화 / 일반 회복 ...
}
```

**경로 2: 자동 재생 진입**

```cpp
void ADRPlayerState::StartHealthRegen()
{
    if (!HasAuthority() || bIsCorrupted) return;

    // ★Plan3(공용칩) §17.6.3★ 회복 차단 칩이면 재생 GE 자체를 얹지 않는다.
    if (CachedUpgradeRuntime.HasFeature(FDRGameplayTags::Get().Upgrade_Feature_Combat_NoHealing)) return;

    if (HealthRegenEffectHandle.IsValid()) return;
    // ... 기존 ...
}
```

추가로 `RebuildUpgradeRuntime()` 에서 **이미 붙어 있는 재생 GE 를 떼어낸다** — 로비에서 칩을 끼고 스테이지에 들어오면 이전 GE 가 살아 있을 수 있다:

```cpp
void ADRPlayerState::RebuildUpgradeRuntime()
{
    // ... 기존 ResolveLoadout / RefreshUpgradeEffects ...

    // 회복 차단 칩이 새로 켜졌으면 이미 돌고 있는 재생 GE 를 즉시 제거한다.
    if (HasAuthority()
        && CachedUpgradeRuntime.HasFeature(FDRGameplayTags::Get().Upgrade_Feature_Combat_NoHealing))
    {
        StopHealthRegen();
    }

    OnUpgradeRuntimeChanged.Broadcast();
}
```

**경로 3: 재생 틱 계산 (방어적 2중화)**

```cpp
// MMC_HealthRegen.cpp — PlayerAS 를 얻은 직후
if (PlayerAS->IsCorrupted()) return 0.f;

// ★Plan3(공용칩) §17.6.3★ GE 가 어떤 경로로든 남아 있어도 틱은 0 이 되게 한다.
if (UDRAbilitySystemLibrary::HasUpgradeFeature(TargetASC,
        FDRGameplayTags::Get().Upgrade_Feature_Combat_NoHealing))
{
    return 0.f;
}
```

**경로 4: 오염 정화 — ★결정 필요 (§17.13 O-9)★**

- **차단한다(권장)**: "모든 체력 회복이 불가" 를 문자 그대로. 오염 = 사실상 사망 선고가 되어 칩의 위험도가 완성된다.
- 차단하지 않는다: 오염 탈출만은 허용. 칩이 많이 순해진다.

차단하는 경우 `HandleIncomingHealing` 의 위 분기 하나로 **자동으로 같이 막힌다**(정화가 힐 경로를 타므로).
차단하지 않으려면 `IsCorrupted()` 일 때는 통과시키도록 조건을 좁혀야 한다:

```cpp
    if (!IsCorrupted() && HasUpgradeFeature(..., NoHealing)) return;   // 정화는 허용하는 버전
```

**차단하지 않는 것** (의도적):

| 경로 | 이유 |
|---|---|
| 리스폰 시 만피 | 회복이 아니라 **재시작**이다 |
| `RefreshUpgradeEffects()` 의 비율 유지 | 최대치 변화에 맞춘 **비율 보존**이지 회복이 아니다 |
| `ExitCorruptedState()` 의 `SetHealth(ContainerHealth)` | 정화 결과 처리 — 경로 4 의 결정에 따라감 |

### 17.6.4 상호작용 / 엣지 케이스

| 상황 | 동작 | 비고 |
|---|---|---|
| 부품 운반 중 피격 | 부품 1.5배는 **무의미해짐**(피해량 무시) + 부품은 그대로 떨어진다 | 기존 드롭 로직은 살린다 |
| 시너지 칩(⑦) 동시 장착 | 받는 피해 10% 감소도 **무의미**(칸 통째 소모) / 주는 피해 +10% 는 유효 | 조합 가치가 반쪽 |
| 오염 상태에서 피격 | `ProcessCorruptedDamage` 로 가므로 **일반 피해 계산** | 컨테이너가 없으므로 정상 |
| 탑승 피해 공유(`PropagateSharedDamage`) | 원래 피해량이 전파된다(칸 소모는 본인만) | 공유 대상까지 칸을 날리지는 않는다 |
| `ShouldPreventDeath()` (로비/튜토리얼) | 칸을 비우되 체력 1 유지 | 위 코드에 반영됨 |

### 17.6.5 ★밸런스 — 상한 300% 는 현재 구성에서 도달 불가★

`NumContainers = 4` 이므로 **잃을 수 있는 칸은 최대 3개**다.

| 남은 칸 | 잃은 칸 | 배율 (0.25/칸) |
|---|---|---|
| 4 (만피) | 0 | **200%** |
| 3 | 1 | 225% |
| 2 | 2 | 250% |
| 1 | 3 | **275%** ← 실질 최댓값 |
| — | (상한) | ~~300%~~ **도달 불가** |

선택지 3가지:

1. **칸당 보너스를 `1/(NumContainers-1)` 로 잡는다** — 4칸이면 `0.3334` → 마지막 칸에서 정확히 300%. ★권장★
   (`BerserkBonusPerLostContainer = 0.3334`)
2. **표기 상한을 275% 로 낮춘다** — 스펙 문구를 바꾼다. 수치 변경 없음.
3. **오염 상태를 "4번째 손실"로 친다** — 오염 중에도 싸우는 빌드가 생긴다. 다만 오염은 이미 최대 체력이 100 으로 줄어든 상태라 이중 페널티.

> 상한 자체는 코드에 남겨둔다 — 칩 ⑧(칸 용량 3배)이나 향후 "컨테이너 +1" 칩이 들어오면 실제로 걸리게 된다.

---

## 17.7 칩 ⑦ — 시너지 오라

### 17.7.1 아키텍처 선택

| 안 | 내용 | 평가 |
|---|---|---|
| **A. `ADRCharacter` 멤버 타이머** | 캐릭터에 타이머 + 핸들 맵 추가 | 파일 추가 없음. 다만 `ADRCharacter` 는 이미 1200줄이 넘고 오라는 칩이 없으면 전혀 안 도는 기능이다 |
| **B. 신규 `UDRSynergyAuraComponent`** ★권장★ | 액터 컴포넌트로 분리, 칩이 있을 때만 타이머 가동 | 캐릭터 비대화 없음. 켜고 끄기가 명확. 향후 다른 오라 칩이 생기면 같은 패턴 복제 |
| **C. 패시브 GameplayAbility** | 칩이 어빌리티를 부여 | **칩 → 어빌리티 부여**라는 축을 새로 만들어야 한다. 이번 범위를 크게 벗어난다 |

→ **안 B.** `UDREliteRoar::TickRoarAura()` 의 3단 구조(수집 → 이탈 제거 → 신규 적용)를 그대로 옮긴다.

### 17.7.2 `UDRSynergyAuraComponent` 설계

```cpp
// Public/AbilitySystem/DRSynergyAuraComponent.h  (신규)

/**
 * 칩 CM.Asc.Synergy 전용 근접 시너지 오라. ★서버 전용★ 으로 동작한다.
 *
 * 칩이 장착돼 있을 때만 타이머가 돈다 — 미장착 상태에서는 비용이 0이다.
 * 버프 수치는 이 컴포넌트가 들고 있지 않다. GE 는 Buff.Upgrade.Synergy 태그만 부여하고,
 * 실제 10% 는 ExecCalc_Damage / HandleIncomingDamage 가 ★태그 카운트★ 로 계산한다.
 * (그래야 여러 명이 겹쳐 걸었을 때 중첩이 자료구조만으로 성립한다 — §17.7.3)
 */
UCLASS(ClassGroup = (DaeRune), meta = (BlueprintSpawnableComponent))
class DAERUNE_API UDRSynergyAuraComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDRSynergyAuraComponent();

    /** ADRCharacter::RefreshUpgradeEffects() 에서 호출. 칩 유무에 따라 타이머를 켜고 끈다. */
    void RefreshFromUpgrade();

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void TickAura();

    /** 반경 내 살아 있는 다른 플레이어 중 최근접 1명. 없으면 nullptr. */
    AActor* FindNearestTeammate(const FVector& Origin, float Radius) const;

    void ApplyBuffTo(AActor* Target);
    void RemoveBuffFrom(const TWeakObjectPtr<AActor>& Key, FActiveGameplayEffectHandle Handle);
    void ClearAllBuffs();

    /** 대상 → 내가 그 대상에게 건 GE 핸들 */
    TMap<TWeakObjectPtr<AActor>, FActiveGameplayEffectHandle> AppliedBuffs;

    FTimerHandle AuraTickTimerHandle;

    /** 로드된 버프 GE 클래스 캐시 (Config 의 TSoftClassPtr 를 1회 동기 로드) */
    UPROPERTY(Transient)
    TSubclassOf<UGameplayEffect> CachedBuffEffectClass;
};
```

**`RefreshFromUpgrade()`**

```cpp
void UDRSynergyAuraComponent::RefreshFromUpgrade()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;   // ★서버 전용★ — GE 부여는 서버만

    const ADRCharacter* DRChar = Cast<ADRCharacter>(Owner);
    const ADRPlayerState* DRPS = DRChar ? DRChar->GetPlayerState<ADRPlayerState>() : nullptr;

    const bool bWantActive = DRPS && DRPS->HasUpgradeFeature(
        FDRGameplayTags::Get().Upgrade_Feature_Combat_Synergy);

    UWorld* World = GetWorld();
    if (!World) return;

    if (!bWantActive)
    {
        World->GetTimerManager().ClearTimer(AuraTickTimerHandle);
        ClearAllBuffs();   // ★칩을 빼면 내가 건 버프를 전부 회수한다★
        return;
    }

    if (World->GetTimerManager().IsTimerActive(AuraTickTimerHandle)) return;   // 이미 켜짐

    const float Interval = FMath::Max(0.05f,
        UDRAbilitySystemLibrary::GetChipTuning(Owner).SynergyTickInterval);

    World->GetTimerManager().SetTimer(
        AuraTickTimerHandle, this, &UDRSynergyAuraComponent::TickAura, Interval, /*bLoop*/ true);

    TickAura();   // 첫 틱을 기다리지 않고 즉시 1회
}
```

**`TickAura()`** — EliteRoar 3단 구조

```cpp
void UDRSynergyAuraComponent::TickAura()
{
    AActor* Owner = GetOwner();
    if (!Owner || !Owner->HasAuthority()) return;

    // 시전자가 죽으면 버프를 전부 회수한다 (시체가 계속 버프를 주지 않게)
    if (Owner->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Owner))
    {
        ClearAllBuffs();
        return;
    }

    const FDRChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(Owner);

    // --- Step 1: 대상 결정 ---
    AActor* Nearest = FindNearestTeammate(Owner->GetActorLocation(), Tuning.SynergyRadius);

    TSet<AActor*> DesiredTargets;
    if (Nearest)
    {
        // ★팀원이 있을 때만 자신도 버프를 받는다★ (혼자면 아무 효과 없음 — 요구사항 명시)
        DesiredTargets.Add(Owner);
        DesiredTargets.Add(Nearest);
    }

    // --- Step 2: 대상에서 빠진 것 회수 ---
    TArray<TWeakObjectPtr<AActor>> ToRemove;
    for (const auto& Pair : AppliedBuffs)
    {
        AActor* Buffed = Pair.Key.Get();
        if (!Buffed || !DesiredTargets.Contains(Buffed))
        {
            RemoveBuffFrom(Pair.Key, Pair.Value);
            ToRemove.Add(Pair.Key);
        }
    }
    for (const TWeakObjectPtr<AActor>& Key : ToRemove) { AppliedBuffs.Remove(Key); }

    // --- Step 3: 새로 들어온 대상에 부여 ---
    for (AActor* Target : DesiredTargets)
    {
        if (!AppliedBuffs.Contains(TWeakObjectPtr<AActor>(Target)))
        {
            ApplyBuffTo(Target);
        }
    }
}
```

**`FindNearestTeammate()`**

```cpp
AActor* UDRSynergyAuraComponent::FindNearestTeammate(const FVector& Origin, float Radius) const
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World || Radius <= 0.f) return nullptr;

    TArray<FOverlapResult> Overlaps;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Owner);

    // EliteRoar 와 같은 방식 — Z 차이를 감안해 오버랩 반경에 여유를 두고, 판정은 아래에서 다시 한다.
    World->OverlapMultiByChannel(
        Overlaps, Origin, FQuat::Identity, ECC_Pawn,
        FCollisionShape::MakeSphere(Radius + 200.f), Params);

    AActor* Best = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();

    for (const FOverlapResult& Result : Overlaps)
    {
        AActor* Candidate = Result.GetActor();
        if (!Candidate || Candidate == Owner) continue;

        // 팀원 판정 = 액터 태그 (IsNotFriend 와 같은 기준 — §17.2.6)
        if (!Candidate->ActorHasTag(FName("Player"))) continue;
        if (!Cast<ADRCharacter>(Candidate)) continue;
        if (Candidate->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Candidate)) continue;

        // ASC 가 없으면 버프를 걸 수 없다
        if (!UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate)) continue;

        const float DistSq = FVector::DistSquared(Candidate->GetActorLocation(), Origin);
        if (DistSq > Radius * Radius) continue;

        if (DistSq < BestDistSq)
        {
            BestDistSq = DistSq;
            Best = Candidate;
        }
    }
    return Best;
}
```

**`ApplyBuffTo()` / `ClearAllBuffs()`**

```cpp
void UDRSynergyAuraComponent::ApplyBuffTo(AActor* Target)
{
    UAbilitySystemComponent* SourceASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());
    UAbilitySystemComponent* TargetASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
    if (!SourceASC || !TargetASC) return;

    if (!CachedBuffEffectClass)
    {
        const FDRChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(GetOwner());
        CachedBuffEffectClass = Tuning.SynergyBuffEffect.LoadSynchronous();
        if (!CachedBuffEffectClass)
        {
            UE_LOG(LogDR, Warning,
                TEXT("[Upgrade] SynergyBuffEffect 미지정 — 시너지 칩이 아무 효과도 내지 않습니다."));
            return;
        }
    }

    FGameplayEffectContextHandle Context = SourceASC->MakeEffectContext();
    Context.AddSourceObject(GetOwner());

    const FGameplayEffectSpecHandle Spec =
        SourceASC->MakeOutgoingSpec(CachedBuffEffectClass, 1.f, Context);
    if (!Spec.IsValid()) return;

    const FActiveGameplayEffectHandle Handle =
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);

    if (Handle.IsValid())
    {
        AppliedBuffs.Add(TWeakObjectPtr<AActor>(Target), Handle);
    }
}

void UDRSynergyAuraComponent::ClearAllBuffs()
{
    for (const auto& Pair : AppliedBuffs)
    {
        RemoveBuffFrom(Pair.Key, Pair.Value);
    }
    AppliedBuffs.Reset();
}

void UDRSynergyAuraComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(AuraTickTimerHandle);
    }
    ClearAllBuffs();   // ★사망/디스커넥트/레벨 전환 시 남의 몸에 버프를 남기지 않는다★
    Super::EndPlay(EndPlayReason);
}
```

**`ADRCharacter` 연결** — 생성자에서 컴포넌트를 붙이고, 기존 훅에서 갱신한다.

```cpp
// DRCharacter.h
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Upgrade")
    TObjectPtr<UDRSynergyAuraComponent> SynergyAuraComponent;

// DRCharacter.cpp — 생성자
    SynergyAuraComponent = CreateDefaultSubobject<UDRSynergyAuraComponent>(TEXT("SynergyAuraComponent"));

// DRCharacter.cpp — RefreshUpgradeEffects() 끝 (서버/클라 공통 호출, 내부에서 권한 검사)
    if (SynergyAuraComponent)
    {
        SynergyAuraComponent->RefreshFromUpgrade();
    }
```

> `RefreshUpgradeEffects()` 는 `OnRep_EquippedChips` → `RebuildUpgradeRuntime()` 경로로 **모든 머신**에서 불린다.
> 컴포넌트가 내부에서 `HasAuthority()` 를 검사하므로 클라에서는 아무 일도 하지 않는다.

### 17.7.3 스택 판정 = `GetGameplayTagCount`

`GE_Upgrade_Synergy` 는 **Infinite + 모디파이어 없음 + `GrantedTags = Buff.Upgrade.Synergy`** 다.
GE 의 기본 `StackingType = None` 이므로 **적용할 때마다 별개의 Active GE 인스턴스**가 생기고,
ASC 의 태그 카운트는 인스턴스 수만큼 올라간다.

```
플레이어 A, B, C 가 전부 칩 장착 + 3m 안에 뭉침
  A 의 오라 → {A, 최근접(B)} 에 GE
  B 의 오라 → {B, 최근접(A)} 에 GE
  C 의 오라 → {C, 최근접(A or B)} 에 GE
  ⇒ A 의 태그 카운트: A자신(1) + B가 건 것(1) + C가 건 것(1) = 최대 3
```

→ **중첩이 자료구조만으로 성립한다.** 스택 수를 따로 관리할 필요가 없다.
같은 (소스, 대상) 쌍에 두 번 걸리는 것은 `AppliedBuffs` 맵이 막는다.

> ⚠️ **GE 에 모디파이어를 넣지 않는 이유**: `DamageTaken` / `DamageDealt` 는 어트리뷰트가 아니라
> `EDRUpgradeStat`(칩 런타임 값)이다. GE 로는 건드릴 수 없다.
> 어트리뷰트를 새로 만들어 GE 로 곱하는 방법도 있지만, 복제 대상 어트리뷰트가 2개 늘고
> 피해 계산 경로가 둘로 갈린다. **태그 카운트 방식이 이 코드베이스에 이미 있는 패턴**이다
> (`UDRVendingMachineBasicAttack::GetCurrentFireInterval()` 의 버프 스택 조회와 동형).

### 17.7.4 피해 적용 2지점

**주는 피해** — §17.6.1 의 (c) 블록이 이미 처리한다.

**받는 피해** — `HandleIncomingDamage()` 의 5번(DamageTaken 칩 배율) **바로 뒤**:

```cpp
    // 기존: EDRUpgradeStat::DamageTaken 배율
    ...

    // ★Plan3(공용칩) §17.7.4★ 시너지 오라: 받는 피해 감소 (ASC 태그 카운트 = 스택 수)
    if (Props.TargetASC)
    {
        const int32 SynergyStacks =
            Props.TargetASC->GetGameplayTagCount(FDRGameplayTags::Get().Buff_Upgrade_Synergy);
        if (SynergyStacks > 0)
        {
            const FDRChipTuning& Tuning =
                UDRAbilitySystemLibrary::GetChipTuning(Props.TargetAvatarActor);

            // 스택이 많아도 무적이 되지 않게 하한을 둔다
            const float Ratio = FMath::Max(
                Tuning.SynergyMinDamageTakenRatio,
                1.f - Tuning.SynergyDamageTakenPerStack * SynergyStacks);

            LocalIncomingDamage *= Ratio;
        }
    }
```

### 17.7.5 멀티플레이어 / 성능

| 항목 | 설계 |
|---|---|
| 권한 | **서버 전용.** GE 부여/해제는 서버에서만 일어나고, Active GE 는 소유 클라에 자동 복제된다 |
| 클라 예측 | 없음. 피해 계산은 어차피 서버 권한이므로 예측 불일치가 생기지 않는다 |
| 비용 | 칩 미장착이면 타이머가 아예 안 돈다. 장착 시 0.25초마다 반경 500uu(300+여유) 스피어 오버랩 1회 — 플레이어 최대 3명 기준 무시 가능 |
| 시전자 사망 | `TickAura()` 첫머리에서 `ClearAllBuffs()` |
| 시전자 퇴장/레벨 전환 | `EndPlay()` 에서 `ClearAllBuffs()` |
| 대상 사망 | 다음 틱에 `DesiredTargets` 에서 빠지며 회수. 사망한 폰이 파괴되면 `TWeakObjectPtr` 가 null → 같이 회수 |
| 대상이 먼저 파괴됨 | `RemoveBuffFrom()` 이 `Pair.Key.Get()` null 검사를 하고 조용히 넘어간다 |

---

## 17.8 칩 ⑧ — 피의 계약

### 17.8.1 최대 체력 +200% — **코드 0줄**

`ContainerHealth` Percent **+2.0** 하나면 끝난다. 기존 `RefreshUpgradeEffects()` 경로가 전부 처리한다.

```
UpgradedContainerHealth = (BaseContainerHealth + PerContainerBonus) × (1 + 2.0)
MaxHealthFlat = (Upgraded − Base) × NumContainers   ← 기존 줄이 자동 계산
```

| 조합 | 칸 용량 | MaxHealth |
|---|---|---|
| 칩 없음 | 100 | 400 |
| ⑧만 | 300 | **1200** |
| ①(+100) + ⑧ | (100+25)×3 = 375 | **1500** |

- 컨테이너 **개수**는 4 그대로 → `WBP_ContainerHealthBar` 의 4칸 고정 애니메이션이 그대로 유효하다. UI 작업 0.
- `ExitCorruptedState()` 의 `NumContainers × ContainerHealth` 도 자동 추종. ✔

### 17.8.2 물 소모 → 체력 소모

**★두 곳을 반드시 같이 고친다.★** 한쪽만 고치면 "쓸 수 있다고 했는데 실제로는 못 내는"(또는 그 반대) 불일치가 난다.

**(a) 사전 검사 — `UDRGameplayAbility::CheckCost()`**

```cpp
bool UDRGameplayAbility::CheckCost(...) const
{
    if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags)) return false;

    const float EffectiveWaterCost = GetEffectiveWaterCostFor(ActorInfo);
    if (EffectiveWaterCost <= 0.f) return true;

    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC) return false;

    const UDRAttributeSet* AttributeSet = Cast<UDRAttributeSet>(
        ASC->GetAttributeSet(UDRAttributeSet::StaticClass()));
    if (!AttributeSet) return false;

    const float CurrentWater  = AttributeSet->GetWater();
    const float CurrentHealth = AttributeSet->GetHealth();

    // ★Plan3(공용칩) §17.8.2★ 피의 계약: 물을 보지 않고 체력만 본다.
    //  ExecCalc_WaterCost 의 분기와 ★같은 수식★ 이어야 한다.
    if (UDRAbilitySystemLibrary::HasUpgradeFeature(
            ASC, FDRGameplayTags::Get().Upgrade_Feature_Combat_BloodCost))
    {
        const float Ratio = UDRAbilitySystemLibrary::GetChipTuning(
            ActorInfo->AvatarActor.Get()).BloodCostHealthRatio;
        const float RequiredHealth = FMath::FloorToFloat(EffectiveWaterCost * Ratio);

        // 자살 방지: 체력이 소모량을 ★초과★ 해야 쓸 수 있다
        return CurrentHealth > RequiredHealth;
    }

    // ... 기존 물 우선 / 부족분 체력 50% 로직 그대로 ...
}
```

**(b) 실제 차감 — `ExecCalc_WaterCost::Execute_Implementation()`**

```cpp
    float ActualWaterCost = 0.f;
    float HealthCost = 0.f;

    // ★Plan3(공용칩) §17.8.2★ 피의 계약: 물을 전혀 쓰지 않고 전액을 체력으로 낸다.
    const UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
    if (UDRAbilitySystemLibrary::HasUpgradeFeature(
            SourceASC, FDRGameplayTags::Get().Upgrade_Feature_Combat_BloodCost))
    {
        const AActor* Ctx = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
        const float Ratio = UDRAbilitySystemLibrary::GetChipTuning(Ctx).BloodCostHealthRatio;

        ActualWaterCost = 0.f;
        HealthCost = FMath::FloorToFloat(RequiredWater * Ratio);
    }
    else if (CurrentWater >= RequiredWater)
    {
        ActualWaterCost = RequiredWater;
    }
    else
    {
        ActualWaterCost = CurrentWater;
        HealthCost = FMath::FloorToFloat((RequiredWater - CurrentWater) * 0.5f);
    }

    // ... 기존 출력 모디파이어 2개 그대로 ...
```

> **`FloorToFloat` 를 양쪽에 똑같이 쓴다.** 한쪽만 내림하면 경계값(체력 = 소모량)에서
> 검사는 통과하는데 차감이 체력을 0 으로 만드는 사고가 난다.

### 17.8.3 체력 코스트가 컨테이너를 우회한다 — 기존 동작이며 그대로 둔다

`ExecCalc_WaterCost` 는 `Health` 어트리뷰트에 **Additive 직접 적용**이라 `IncomingDamage` 를 타지 않는다.
따라서 칩 ⑧으로 체력을 써도:

| | 발생하는가 |
|---|---|
| 컨테이너 오버플로우 규칙 | ✘ (체력 숫자만 줄어든다) |
| 전투 진입 (`NotifyEnterCombat`) | ✘ |
| 피격 리액션 / 넉백 | ✘ |
| 체력 0 도달 시 오염 진입 | ✘ — `EnterCorruptedState` 는 `ProcessNormalDamage` 안에서만 불린다 |

→ **의도적으로 기존 동작을 유지한다.** 기존 "물 부족 시 체력 50%" 규칙도 똑같이 동작하고 있었고,
스킬을 쓰다 스스로 오염 상태에 빠지는 것은 바람직하지 않다.
다만 `CheckCost` 의 `CurrentHealth > RequiredHealth` 가드가 있으므로 **체력 0 으로는 절대 떨어지지 않는다**(최소 1 남음).

### 17.8.4 부작용 — 물 경제가 무의미해진다

- 적 처치 물 보상(`GrantWaterToPlayers`), 칩 ③(최대 물 +50%), `WaterGain` 계열이 **전부 死 옵션**이 된다.
- 물이 줄지 않으므로 **물 고갈 기반 연출/UI 도 멈춘다.**
- → 이건 버그가 아니라 칩의 정체성이다. **칩 설명(`Description`)에 "물을 사용하지 않게 됩니다"를 명시**해
  플레이어가 ③번 칩과 같이 끼는 낭비를 하지 않게 한다.
- 조합 경고를 UI 에 띄우는 것은 과하다. §17.13 O-12 로만 남긴다.

---

## 17.9 칩 간 상호작용 매트릭스

| 조합 | 결과 | 평가 |
|---|---|---|
| ⑥ + ⑦ | 받는 피해 10% 감소가 **무의미**(칸 통째 소모), 주는 피해 +10% 만 유효 | 반쪽 시너지 |
| ⑥ + ⑧ | 체력 3배로 칸이 커지지만 **한 방에 한 칸**은 그대로 + 회복 불가 + 스킬이 체력을 먹음 → **확정 죽음 시계** | 하이 리스크 빌드. 의도적으로 허용 |
| ⑥ + ① | 칸 용량이 커져도 한 방에 한 칸 → **체력 칩이 무력화** | 같이 끼지 않는 것이 정답 |
| ⑦ + ⑧ | 받는 피해 10% 감소가 커진 체력통 위에서 유효 | 궁합 좋음 |
| ⑧ + ③ | 최대 물 +50% 가 **완전 무의미** | 설명으로 안내 |
| ⑧ + ⑤ | 회복 시작 단축이 체력 코스트 회복에 유효 | 궁합 좋음 |
| ⑥ + ⑤ | 회복 자체가 막혀 **⑤가 완전 무의미** | 같이 끼지 않는 것이 정답 |

> 슬롯이 6칸이고 기본 2개가 먹으므로 **유료 4칸**에 ⑥⑦⑧ 중 고르게 된다.
> 위 매트릭스가 "아무거나 다 끼면 됨"을 막아 준다 — §14 **O-6 가 이것으로 해소된다.**

---

## 17.10 변경 파일 체크리스트

### C++ — 신규

| 파일 | 내용 |
|---|---|
| `Public/AbilitySystem/DRSynergyAuraComponent.h` | 칩 ⑦ 오라 컴포넌트 |
| `Private/AbilitySystem/DRSynergyAuraComponent.cpp` | 〃 |

### C++ — 수정

| 파일 | 변경 | 칩 |
|---|---|---|
| `Public/Game/DRUpgradeTypes.h` | `EDRUpgradeStat::DamageDealt` 추가 (`Count` 직전) | ⑥ |
| `Public/DRGameplayTags.h` / `.cpp` | 피처 태그 4개 + `Buff.Upgrade.Synergy` | ⑥⑦⑧ |
| `Public/Game/DRProgressionConfig.h` | `FDRChipTuning` + `ChipTuning` 필드 | ⑥⑦⑧ |
| `Private/Game/DRChipCatalog.cpp` | `ValidateCatalog` 돌파 칩 규칙 완화 (§17.4.5) | ⑥⑦⑧ |
| `Private/Game/DRGameInstance.cpp` | `MakeChipViewModel` 의 `bHasDrawback` 보정 | ⑥⑦⑧ |
| `Public/AbilitySystem/DRAbilitySystemLibrary.h` / `.cpp` | `GetUpgradeRuntimeForASC` / `HasUpgradeFeature` / `GetPlayerAttributeSetFromASC` / `GetChipTuning` | 공통 |
| `Private/AbilitySystem/Abilities/DRGameplayAbility.cpp` | `GetUpgradeRuntimeFor` 위임(중복 제거) + `CheckCost` 혈액 분기 | 공통·⑧ |
| `Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp` | 소스측 배율 블록 (DamageDealt + 광폭화 + 시너지) | ⑥⑦ |
| `Private/AbilitySystem/ExecCalc/ExecCalc_WaterCost.cpp` | 혈액 코스트 분기 | ⑧ |
| `Private/AbilitySystem/DRPlayerAttributeSet.cpp` | `HandleIncomingDamage` 시너지 감소 / `ProcessNormalDamage` 칸 소모 / `HandleIncomingHealing` 회복 차단 | ⑥⑦ |
| `Private/AbilitySystem/ModMagCalc/MMC_HealthRegen.cpp` | 회복 차단 2중화 | ⑥ |
| `Private/Player/DRPlayerState.cpp` | `StartHealthRegen` 차단 + `RebuildUpgradeRuntime` 에서 기존 재생 GE 제거 | ⑥ |
| `Public/Character/DRCharacter.h` / `.cpp` | `SynergyAuraComponent` 생성 + `RefreshUpgradeEffects` 에서 갱신 | ⑦ |

### 콘텐츠 / 에셋

| 에셋 | 변경 |
|---|---|
| `GE_Upgrade_Synergy` **(신규)** | Infinite / 모디파이어 없음 / `GrantedTags = Buff.Upgrade.Synergy` / Stacking = None |
| `DA_ChipCatalog` | 돌파 칩 3종 추가 (§17.4.4) |
| `DA_ProgressionConfig` | `ChipTuning` 값 입력 + `SynergyBuffEffect` 에 `GE_Upgrade_Synergy` 지정 |
| 칩 아이콘 3종 | `Content/DaeRuneAssets/UI/Upgrade/Icon/` |

---

## 17.11 마일스톤

| 마일스톤 | 내용 | 산출물 | 예상 |
|---|---|---|---|
| **N6** | 공통 배관 | ✅ **코드 완료** — `DamageDealt` 스탯 / 태그 5개 / `FDRChipTuning` / 라이브러리 헬퍼 4개 / `ValidateCatalog` 완화 | 0.5일 |
| **N7** | 칩 ⑧ (가장 단순) | ✅ **코드 완료** — `CheckCost` + `ExecCalc_WaterCost` 2곳 | 0.3일 |
| **N8** | 칩 ⑥ | ✅ **코드 완료** — `ExecCalc_Damage` 소스측 + `ProcessNormalDamage` + 회복 차단 4경로 | 0.7일 |
| **N9** | 칩 ⑦ | ✅ **코드 완료** — `UDRSynergyAuraComponent` 신규 + 피해 2지점. ⬜ `GE_Upgrade_Synergy` 에셋 남음 | 1일 |
| **N10** | 에셋 + 밸런스 + 테스트 | ⬜ **미착수** — `GE_Upgrade_Synergy` / 카탈로그 3종 / `ChipTuning` 값 / §17.12 | 0.7일 |

> **2026-09-22 기준 — §17 의 C++ 전량 완료(빌드 통과).** 남은 것은 전부 에디터 작업이다:
> 1. `GE_Upgrade_Synergy` 생성 (Infinite / 모디파이어 없음 / `GrantedTags = Buff.Upgrade.Synergy` / **Stacking = None**)
> 2. `DA_ProgressionConfig` → `ChipTuning.SynergyBuffEffect` 에 그 GE 지정
> 3. `DA_ChipCatalog` 에 돌파 칩 3종 추가 (§17.4.4). **`Category = Ascension` + `Description` 필수** — 안 쓰면 검증에서 걸린다
>
> → **클릭 단위 절차는 §18 에 따로 정리해 뒀다. 실제 작업은 §18 을 보고 진행할 것.**
>
> ⚠️ 칩을 카탈로그에 넣기 전까지는 **어떤 경로도 실행되지 않는다** — 피처 태그를 주는 칩이 없으면
> 모든 분기가 false 라 기존 동작 그대로다(회귀 안전).

**합계 ≈ 3.2일.** 의존: N6 → (N7 ∥ N8 ∥ N9) → N10.
**최소 컷**: N6 → N7 → N8 → N10. ⑦은 신규 컴포넌트라 단독으로 뒤로 미룰 수 있다.

---

## 17.12 테스트 계획

### 17.12.1 칩 ⑥

| ID | 항목 | 기대 |
|---|---|---|
| B-1 | 만피에서 적 타격 | 피해 **×2.00** |
| B-2 | 칸 1·2·3 잃은 상태에서 타격 | ×2.25 / ×2.50 / ×2.75 (튜닝값 0.25 기준) |
| B-3 | 피격 1회 (약한 적) | 피해량과 무관하게 체력이 **정확히 칸 바닥**으로 떨어진다 |
| B-4 | 칸 경계(300)에서 피격 | 200 으로 (다음 칸이 아니라 현재 칸을 비운다) |
| B-5 | 마지막 칸에서 피격 | 체력 0 → **오염 진입** |
| B-6 | 힐 GE / 물웅덩이 등 회복 시도 | 체력이 **오르지 않는다** |
| B-7 | 전투 이탈 후 10초 대기 | 자동 재생이 **시작되지 않는다** (로그로 `StartHealthRegen` 미호출 확인) |
| B-8 | 재생 중에 로비에서 칩 장착 후 재입장 | 기존 재생 GE 가 **즉시 제거된다** |
| B-9 | 오염 상태에서 초과 힐 | O-9 결정대로 (차단 시 정화 불가) |
| B-10 | 튜토리얼/로비에서 피격 | 죽지 않는다 (체력 1 유지) |
| B-11 | 투사체 발사 후 칸을 잃고 명중 | **명중 시점** 배율로 계산된다 (§17.6.1 주의) |

### 17.12.2 칩 ⑦ (PIE 3인 필수)

| ID | 항목 | 기대 |
|---|---|---|
| S-1 | 혼자 있을 때 | 태그 카운트 0, 버프 없음 |
| S-2 | 팀원 1명이 3m 안에 진입 | 자신 + 그 팀원 **둘 다** 버프. 카운트 1 |
| S-3 | 팀원이 3m 밖으로 이탈 | **둘 다** 해제 (자신 것도) |
| S-4 | 팀원 2명이 범위 안 | **최근접 1명만** 버프 (3명 전원이 아니다) |
| S-5 | A·B 둘 다 칩 장착 후 근접 | 각자 카운트 **2** → 공수 20% |
| S-6 | A·B·C 전원 장착 후 밀집 | 카운트 최대 3 → 30%, 받는 피해 하한(20%) 미도달 확인 |
| S-7 | 시전자 사망 | 팀원의 버프가 **회수된다** |
| S-8 | 시전자 디스커넥트 / 레벨 전환 | 버프가 남지 않는다 (`EndPlay`) |
| S-9 | 로비에서 칩 해제 | 즉시 `ClearAllBuffs` + 타이머 정지 |
| S-10 | 칩 미장착 | 타이머가 **돌지 않는다** (프로파일러/로그 확인) |

### 17.12.3 칩 ⑧

| ID | 항목 | 기대 |
|---|---|---|
| L-1 | 장착 직후 | MaxHealth **1200** (①까지 끼면 1500) |
| L-2 | 스킬 사용 | 물이 **전혀 줄지 않는다** |
| L-3 | 스킬 사용 | 체력이 **물 소모량과 같은 양**만큼 준다 |
| L-4 | 체력이 소모량 이하 | 스킬이 **발동하지 않는다** (`CheckCost` 차단) |
| L-5 | 체력이 소모량+1 | 발동하고 체력 1 이 남는다 |
| L-6 | 체력 소모로 칸 경계 통과 | 오염 진입/피격 리액션이 **발생하지 않는다** (§17.8.3) |
| L-7 | 칩 해제 후 | 기존 물 소모로 정확히 복귀 |

### 17.12.4 회귀

| ID | 항목 | 기대 |
|---|---|---|
| RB-1 | 돌파 칩 미장착 | 피해·회복·코스트가 §17 도입 전과 **완전히 동일** |
| RB-2 | 적 → 플레이어 피해 | `GetUpgradeRuntimeForASC` 가 빈 캐시를 돌려줘 배율 1.0 |
| RB-3 | Elite Roar / Phase1 배율 | 기존 ×1.2 / ×0.7 이 그대로 동작 |
| RB-4 | 기존 "물 부족 시 체력 50%" | 칩 ⑧ 미장착 시 변함 없음 |
| RB-5 | `ValidateCatalog` | 기존 5종 + 신규 3종 모두 오류 0 |

---

## 17.13 열린 결정 사항

| ID | 질문 | 이 문서의 가정 | 다르면 |
|---|---|---|---|
| **O-9** | 칩 ⑥의 "모든 회복 불가"가 **오염 정화**까지 막는가? | **막는다** — 오염 = 사실상 사망 선고 | 막지 않으려면 `HandleIncomingHealing` 분기에 `!IsCorrupted()` 조건 추가 (§17.6.3) |
| **O-10** | 칩 ⑥의 300% 상한을 어떻게 도달시키나? | 칸당 0.25 유지 → **실질 275%** | ① 칸당 0.3334 (권장) ② 표기를 275% 로 ③ 오염을 4번째 손실로 (§17.6.5) |
| **O-11** | 칩 ⑥ 피격 시 데미지 텍스트에 무엇을 띄우나? | 현행 유지(원래 피해량) — **실제 감소량과 다르다** | 실제 감소량 표시 또는 "컨테이너 파손" 전용 연출 (§17.6.2) |
| **O-12** | ⑧ + ③(최대 물) 같은 **死 조합**을 UI 가 경고하나? | 경고하지 않는다. `Description` 문구로만 안내 | 경고하려면 칩 정의에 `ConflictsWith` 배열 + 뷰모델 확장이 필요 |
| **O-13** | 칩 ⑦의 버프 대상이 정말 **최근접 1명**인가? (전원이 아니라) | 원문대로 **1명** | 전원이면 `FindNearestTeammate` → `FindTeammatesInRadius` 로 바꾸고 Step 1 만 수정 |
| **O-14** | 돌파 칩 3종의 `RequiredSlotTier` (⑥5 / ⑦4 / ⑧5) | 튜닝값 | 자유 조정. 단 `MaxSlots`(6) 이하여야 한다 |

---

## 17.14 리스크 / 함정

| # | 함정 | 증상 | 방지 |
|---|---|---|---|
| 1 | `ValidateCatalog` 의 돌파 칩 규칙을 안 고침 | **3종 모두 검증 오류** → 콘텐츠 작업이 막힘 | §17.4.5 를 N6 에서 먼저 처리 |
| 2 | 회복 경로 4개 중 일부만 차단 | 칩 ⑥이 "반쯤" 동작 — 특히 이미 붙은 재생 GE 가 계속 틱 | §17.6.3 의 4경로 전부 + `RebuildUpgradeRuntime` 의 `StopHealthRegen` |
| 3 | `CheckCost` 와 `ExecCalc_WaterCost` 의 비율/내림이 어긋남 | "쓸 수 있다고 했는데 체력이 0" 또는 발동 불가 | 양쪽 다 `FloorToFloat(Cost × Ratio)` 로 **같은 식** |
| 4 | 칩 피처 태그를 ASC 에 Loose 로 얹음 | 어빌리티 `ActivationBlockedTags` 와 충돌해 **스킬이 안 나감** | §17.3.1 — 칩 태그는 ASC 에 넣지 않는다 |
| 5 | 오라 GE 를 회수하지 않음 | 시전자가 죽거나 나간 뒤에도 **남의 몸에 영구 버프** | `TickAura` 사망 검사 + `EndPlay` + 칩 해제 시 `ClearAllBuffs` |
| 6 | `GE_Upgrade_Synergy` 에 Stacking 을 켬 | 태그 카운트가 인스턴스 수와 어긋나 **중첩이 깨진다** | Stacking = **None** (기본값) 유지 |
| 7 | `ExecCalc_Damage` 에서 적 소스에 대해 null 체크 누락 | 적 공격 시 크래시 | `GetUpgradeRuntimeForASC` 가 **항상 유효 참조** 반환 |
| 8 | `EDRUpgradeStat::DamageDealt` 를 enum 중간에 삽입 | 기존 카탈로그 칩 효과가 다른 스탯으로 밀림 | `Count` 직전에만 추가 (D-4) |
| 9 | 광폭화 칸 소모를 `HandleIncomingDamage` 본체에 넣음 | 오염 상태에서도 칸을 비우려다 잘못된 체력 계산 | `ProcessNormalDamage` 안에서만 (§17.6.2) |
| 10 | `SynergyBuffEffect` 미지정 | 칩 ⑦이 조용히 아무 효과 없음 | `ApplyBuffTo` 의 경고 로그 + `ValidateCurrencyBudget` 에 지정 검사 추가 권장 |

---

# §18. 에디터 작업 매뉴얼 — 돌파 칩 3종 (STEP BY STEP)

> 작성일: 2026-09-22
> 대상: §17 의 C++ 가 빌드된 상태에서 **에디터에서만** 해야 하는 작업 전부.
> 언리얼을 처음 만지는 사람이 그대로 따라할 수 있도록 클릭 단위로 적는다.

## 18.0 시작 전 확인

**이미 끝나 있는 것** (다시 할 필요 없음)

- C++ 전량 (빌드 통과)
- 칩 아이콘 8종 — `Content/DaeRuneAssets/UI/Upgrade/Icon/Common/`
- `WBP_StageOverlay` HUD 표시 배선
- `DA_ChipCatalog` 의 기본 칩 5종 (`CM.Stat.*`, `CM.Util.HudDisplay`)

**이번에 만들 것 — 총 4개**

| # | 무엇 | 어디 |
|---|---|---|
| 1 | `GE_Upgrade_Synergy` (신규 에셋) | `Content/Blueprints/AbilitySystem/Player/Effects/` |
| 2 | `ChipTuning` 값 입력 | `DA_ProgressionConfig` |
| 3 | 돌파 칩 3종 추가 | `DA_ChipCatalog` |
| 4 | 검증 + 테스트 | — |

소요 시간: 40분~1시간.

---

## 18.1 STEP 0 — 에디터 완전 재시작

**라이브 코딩(Ctrl+Alt+F11)으로는 안 된다.** 반드시 에디터를 끄고 다시 켠다.

이번 C++ 변경에 아래가 포함돼 있어 재시작 없이는 에디터에 나타나지 않는다:

- 신규 클래스 `UDRSynergyAuraComponent`
- 신규 구조체/프로퍼티 `FDRChipTuning`, `ChipTuning`
- `EDRUpgradeStat` 의 새 항목 `주는 피해`
- 게임플레이 태그 5개

**확인 방법**: 재시작 후 `DA_ProgressionConfig` 를 열었을 때 `Chip Tuning` 칸이 보이면 정상이다.
안 보이면 재시작이 안 된 것이므로 STEP 1 로 넘어가지 말 것.

---

## 18.2 STEP 1 — `GE_Upgrade_Synergy` 만들기

칩 ⑦(시너지 오라)이 팀원에게 붙이는 게임플레이 이펙트다.

### 18.2.1 에셋 생성

1. 콘텐츠 브라우저에서 `Content/Blueprints/AbilitySystem/Player/Effects/` 폴더로 이동
2. 빈 공간에 **우클릭** → **블루프린트 클래스 (Blueprint Class)**
3. 창이 뜨면 아래 **모든 클래스 (All Classes)** 를 펼치고 검색창에 `GameplayEffect` 입력
4. **GameplayEffect** 선택 → **선택 (Select)**
5. 이름을 **`GE_Upgrade_Synergy`** 로 입력하고 엔터
6. 만들어진 에셋을 더블클릭해서 연다

> 기존 GE 를 복제하지 말 것. `GE_Roar_Buff` 등에는 모디파이어와 스태킹 설정이 남아 있어
> 그대로 쓰면 §18.2.3 의 함정에 걸린다.

### 18.2.2 설정

디테일 패널에서 아래 3가지만 만진다.

| 항목 | 설정값 | 위치 |
|---|---|---|
| **Duration Policy** | **`Infinite`** | Gameplay Effect 섹션 상단 |
| **Modifiers** | **비워둔다** (기본값) | Gameplay Effect 섹션 |
| **Components** | **Target Tags** 추가 (아래 참조) | Components 섹션 |

**Components 추가 절차**

1. 디테일 패널에서 **Components** 배열을 찾는다
2. 옆의 **`+`** 버튼 클릭
3. 드롭다운에서 **`Target Tags Gameplay Effect Component`** 선택
4. 추가된 항목을 펼친다
5. 안쪽의 **`Added`** (버전에 따라 `Add Tags` 로 표시) 배열의 **`+`** 클릭
6. 태그 선택창에서 **`Buff.Upgrade.Synergy`** 선택

7. **저장 (Ctrl+S)**

### 18.2.3 ⚠️ 반드시 확인할 것

| 항목 | 올바른 값 | 틀리면 |
|---|---|---|
| **Stacking Type** | **`None`** (기본값 — 건드리지 않는다) | 중첩이 깨진다. 3명이 뭉쳐도 스택이 1로 고정돼 시너지가 10% 에서 멈춘다 |
| **Duration Policy** | `Infinite` | `Instant` 면 태그가 붙자마자 사라져 효과가 0 이다 |
| **Modifiers** | 비어 있음 | 수치는 코드가 태그 개수로 계산한다. 여기 넣으면 이중 적용된다 |
| 태그 위치 | **Target Tags** 컴포넌트 | `Asset Tags` 에 넣으면 대상에게 부여되지 않는다 |

---

## 18.3 STEP 2 — `DA_ProgressionConfig` 설정

`Content/Blueprints/Progression/DA_ProgressionConfig` 를 더블클릭해서 연다.

디테일 패널에서 **`Upgrade` → `Chip Tuning`** 섹션을 펼친다.

### 18.3.1 입력값

| 필드 (에디터 표시명) | 값 | 필수 |
|---|---|---|
| **Synergy Buff Effect** | `GE_Upgrade_Synergy` | ★필수★ |
| Berserk Bonus Per Lost Container | `0.25` | 기본값 |
| Berserk Max Multiplier | `3.0` | 기본값 |
| Synergy Radius | `300` | 기본값 |
| Synergy Tick Interval | `0.25` | 기본값 |
| Synergy Damage Dealt Per Stack | `0.1` | 기본값 |
| Synergy Damage Taken Per Stack | `0.1` | 기본값 |
| Synergy Min Damage Taken Ratio | `0.2` | 기본값 |
| Blood Cost Health Ratio | `1.0` | 기본값 |

**`Synergy Buff Effect` 만 채우면 되고 나머지는 이미 기본값이 들어 있다.**
드롭다운을 눌러 `GE_Upgrade_Synergy` 를 고른다.

### 18.3.2 선택 사항 — 광폭화 최대 배율

컨테이너가 4칸이라 **최대 3칸까지만** 잃을 수 있다. `0.25` 로 두면 실제 도달 가능한 최대 배율은 **275%** 다.

| Berserk Bonus Per Lost Container | 만피 | 1칸 잃음 | 2칸 | 3칸 (최대) |
|---|---|---|---|---|
| `0.25` (기본) | 200% | 225% | 250% | **275%** |
| `0.3334` | 200% | 233% | 267% | **300%** |

기획 문구대로 "최대 300%" 를 찍고 싶으면 **`0.3334`** 로 바꾼다. 둘 다 정상 동작한다.

**저장 (Ctrl+S)**

---

## 18.4 STEP 3 — `DA_ChipCatalog` 에 돌파 칩 3종 추가

`Content/Blueprints/Progression/DA_ChipCatalog` 를 더블클릭해서 연다.

`Chips` 배열의 **`+`** 를 눌러 항목을 **3개** 추가한다 (기존 5개 뒤에 6·7·8번으로 붙는다).

### 18.4.1 공통 주의

- **`Common Chip` 체크를 빠뜨리지 말 것.** 안 켜면 정원로봇에서만 보인다.
- **`Category` 는 드롭다운에서 `돌파 칩` 을 고른다** (영어 `Ascension` 이 아니라 한글로 표시된다).
- **`Description` 을 비우면 검증에서 오류가 난다.** 이 3종은 단점이 코드로 구현돼 있어
  수치 목록에 안 나타나므로, 설명란에 적는 것이 유일한 안내 수단이다.
- `Owner Class` 는 `Common Chip` 이 켜져 있으면 무시된다. 기본값 `Gardener` 그대로 둔다.
- `Target Ability Tag` 는 **비워둔다.** 공용 칩에 스킬 태그를 넣으면 검증 경고가 난다.

### 18.4.2 칩 ⑥ — 광폭화

| 필드 | 값 |
|---|---|
| Chip Id | `CM.Asc.Berserk` |
| Owner Class | (기본값) |
| **Common Chip** | **☑ 체크** |
| **Category** | **`돌파 칩`** |
| Target Ability Tag | (비움) |
| Display Name | `무모한 과부하` |
| Effect Summary | `주는 피해 +100%, 잃은 칸마다 +25%` |
| **Description** | `체력 회복이 불가능해지고, 피격 시 피해량과 무관하게 체력 컨테이너 1칸을 통째로 잃습니다.` |
| Icon | `무모` |
| Required Slot Count | `1` |
| Required Slot Tier | `5` |

**Modifiers** — `+` 로 **1개** 추가

| 필드 | 값 |
|---|---|
| Stat | **`주는 피해`** |
| Target Ability Tag | (비움) |
| Op | **`곱연산(비율, 0.1 = +10%)`** |
| Value | **`1.0`** |
| Is Drawback | ☐ (체크 안 함) |
| Detail Override | (비움) |

**Granted Tags** — `+` 로 **2개** 추가

1. `Upgrade.Feature.Combat.Berserk`
2. `Upgrade.Feature.Combat.NoHealing`

> ⚠️ `NoHealing` 을 빠뜨리면 체력 회복이 그대로 돌아간다. 2개 모두 필요하다.

### 18.4.3 칩 ⑦ — 시너지 오라

| 필드 | 값 |
|---|---|
| Chip Id | `CM.Asc.Synergy` |
| **Common Chip** | **☑ 체크** |
| **Category** | **`돌파 칩`** |
| Display Name | `동조 링크` |
| Effect Summary | `근처 팀원과 함께 공격 +10%, 방어 +10%` |
| **Description** | `3m 안에 팀원이 없으면 효과가 사라집니다. 여러 명이 장착하면 중첩됩니다.` |
| Icon | `bff` |
| Required Slot Count | `1` |
| Required Slot Tier | `4` |

**Modifiers** — **비워둔다** (항목 0개)

**Granted Tags** — `+` 로 **1개**

1. `Upgrade.Feature.Combat.Synergy`

### 18.4.4 칩 ⑧ — 피의 계약

| 필드 | 값 |
|---|---|
| Chip Id | `CM.Asc.BloodCost` |
| **Common Chip** | **☑ 체크** |
| **Category** | **`돌파 칩`** |
| Display Name | `피의 계약` |
| Effect Summary | `최대 체력 +200%, 스킬이 물 대신 체력 소모` |
| **Description** | `스킬이 물을 전혀 쓰지 않는 대신, 물 소모량과 같은 양의 체력을 소모합니다.` |
| Icon | `스킬중독` |
| Required Slot Count | `1` |
| Required Slot Tier | `5` |

**Modifiers** — `+` 로 **1개** 추가

| 필드 | 값 |
|---|---|
| Stat | **`컨테이너 체력`** |
| Op | **`곱연산(비율, 0.1 = +10%)`** |
| Value | **`2.0`** |
| Is Drawback | ☐ |

> `최대 체력` 스탯이 아니라 **`컨테이너 체력`** 을 쓴다. `최대 체력` 은 합연산 전용이라
> 곱연산을 넣으면 검증에서 오류가 난다.

**Granted Tags** — `+` 로 **1개**

1. `Upgrade.Feature.Combat.BloodCost`

**저장 (Ctrl+S)**

---

## 18.5 STEP 4 — 검증

1. `DA_ProgressionConfig` 를 연다
2. 아무 숫자 필드나 값을 지웠다 다시 입력한다 (예: `Default Slot Cost` 에 같은 값 재입력)
3. **창 → 출력 로그 (Window → Output Log)** 를 연다
4. 필터 입력창에 **`ProgressionConfig`** 를 친다

**통과했을 때**

```
[ProgressionConfig] DA_ProgressionConfig: 검증 통과. 재화 예산 N / 필요 M
```

**오류가 나오면** 메시지에 칩 Id 와 원인이 그대로 적혀 있다. §18.7 표에서 찾는다.

---

## 18.6 STEP 5 — 테스트

### 18.6.1 준비

에디터에서 플레이(PIE) 시작 후 **`~`** 키로 콘솔을 열고:

```
DRUnlockUpgrade
DRUnlockSlot 4
```

로비의 업그레이드 장치와 상호작용해서 화면을 연다.

**기준값** (기본 장착 칩 ① 이 껴 있는 상태)

| | 값 |
|---|---|
| 컨테이너 개수 | 4칸 |
| 칸당 체력 | 125 |
| 최대 체력 | **500** |

### 18.6.2 칩 ⑧ — 피의 계약 (가장 쉬움, 먼저 할 것)

1. `피의 계약` 장착 → 스테이지 입장
2. 확인:

| 확인 | 기대 |
|---|---|
| 최대 체력 | **1500** (칸당 375) |
| 스킬 사용 시 물 | **전혀 줄지 않음** |
| 스킬 사용 시 체력 | 물 소모량과 **같은 양**만큼 감소 |
| 체력이 소모량 이하일 때 | 스킬이 **발동하지 않음** |

> 칩 ① 을 빼고 ⑧ 만 끼면 최대 체력은 1200 이 된다 (칸당 300).

### 18.6.3 칩 ⑥ — 광폭화

1. `무모한 과부하` 장착 → 스테이지 입장
2. 확인:

| 확인 | 기대 |
|---|---|
| 만피에서 적 타격 | 피해 **×2.00** |
| 적에게 1회 피격 | 체력이 정확히 **500 → 375** (피해량과 무관) |
| 2회 피격 | 375 → 250 |
| 3회 피격 | 250 → 125 |
| 4회 피격 | 125 → 0 → **오염 상태 진입** |
| 375 상태에서 타격 | 피해 ×2.25 |
| 125 상태에서 타격 | 피해 ×2.75 |
| 물웅덩이/힐 아이템 | 체력이 **오르지 않음** |
| 전투 이탈 후 대기 | 자동 회복이 **시작되지 않음** |

### 18.6.4 칩 ⑦ — 시너지 오라 (★PIE 2인 이상 필요★)

**혼자서는 아무 효과가 없는 것이 정상이다.**

1. 에디터 상단 플레이 버튼 옆 **⋮** → **플레이어 수 (Number of Players)** 를 **2** 로 설정
2. **넷 모드 (Net Mode)** 를 **리슨 서버로 플레이 (Play As Listen Server)** 로 설정
3. 두 플레이어 모두 `동조 링크` 를 장착하고 스테이지 입장
4. 확인:

| 상황 | 기대 |
|---|---|
| 서로 멀리 떨어짐 | 효과 없음 |
| 3m(300uu) 안으로 접근 | 양쪽 모두 공격 +10%, 방어 +10% |
| 둘 다 장착 상태로 붙어 있음 | 각자 **20%** (스택 2) |
| 다시 멀어짐 | **자기 효과까지** 해제됨 |
| 한 명이 사망 | 상대방의 버프가 사라짐 |

**확인 방법**: 로그로 보려면 출력 로그에서 `Upgrade` 로 필터하거나,
같은 적을 때렸을 때 데미지 숫자가 달라지는지로 비교한다.

### 18.6.5 회귀 확인 (중요)

**돌파 칩을 전부 뺀 상태**로 한 판 돌린다.

| 확인 | 기대 |
|---|---|
| 최대 체력 | 500 |
| 피해량 | 이전과 동일 |
| 스킬 물 소모 | 정상 |
| 체력 회복 | 정상 |

수치가 하나라도 다르면 §18.7 의 마지막 줄을 본다.

---

## 18.7 문제 해결

| 증상 | 원인 | 해결 |
|---|---|---|
| `DA_ProgressionConfig` 에 `Chip Tuning` 칸이 없다 | 에디터 재시작 안 함 | STEP 0 |
| `Granted Tags` 에서 `Upgrade.Feature.Combat.*` 태그가 검색되지 않는다 | 〃 | 〃 |
| `Stat` 드롭다운에 `주는 피해` 가 없다 | 〃 | 〃 |
| 칩 ⑦을 껴도 아무 일이 없다 (혼자) | **정상 동작** | 팀원이 3m 안에 있어야 한다 |
| 칩 ⑦을 껴도 아무 일이 없다 (2인) | `Synergy Buff Effect` 미지정 | STEP 2. 출력 로그에 경고가 1회 찍혀 있다 |
| 칩 ⑦ 스택이 항상 1 | GE 의 `Stacking Type` 이 `None` 이 아님 | §18.2.3 |
| 칩 ⑦ 효과가 즉시 사라짐 | GE 의 `Duration Policy` 가 `Instant` | §18.2.2 |
| 칩 ⑥이 회복을 막지 않는다 | `Granted Tags` 에 `NoHealing` 누락 | §18.4.2 (태그 2개) |
| 칩 ⑥ 피격 시 칸이 안 비워진다 | `Granted Tags` 에 `Berserk` 누락 | 〃 |
| 칩 ⑧ 최대 체력이 안 오른다 | `Stat` 을 `최대 체력` 으로 넣음 | `컨테이너 체력` + 곱연산 2.0 (§18.4.4) |
| 검증: `돌파 칩인데 ... Description` | Description 비어 있음 | 설명란 입력 |
| 검증: `MaxHealth 는 Flat 전용` | ⑧에서 `최대 체력` + 곱연산 조합 | 〃 |
| 검증: `공용 칩에 특정 스킬 태그` | `Target Ability Tag` 를 채움 | 비운다 |
| 검증: `GrantedTags 의 ... Upgrade.Feature 하위가 아닙니다` | 태그 오타/잘못 선택 | 태그 재선택 |
| 칩을 다른 로봇에서 못 본다 | `Common Chip` 체크 안 함 | 체크 |
| 칩이 다음 실행 때 사라진다 | `Required Slot Tier` 가 해금 슬롯 수보다 큼 | `DRUnlockSlot` 으로 슬롯을 더 열거나 Tier 를 낮춘다 |
| 돌파 칩을 다 뺐는데도 수치가 이전과 다르다 | 칩 문제가 아니다 | 칩 시스템 밖(다른 브랜치 변경 등)에서 원인을 찾는다 |

---

## 18.8 완료 체크리스트

- [ ] 에디터 재시작함
- [ ] `GE_Upgrade_Synergy` 생성 — Infinite / Modifiers 비움 / Target Tags 에 `Buff.Upgrade.Synergy` / Stacking `None`
- [ ] `DA_ProgressionConfig` → `Synergy Buff Effect` 지정
- [ ] `DA_ChipCatalog` 에 3종 추가 — 각각 `Common Chip` ☑, `Category = 돌파 칩`, `Description` 입력
- [ ] ⑥의 `Granted Tags` 2개 확인
- [ ] 검증 로그 오류 0
- [ ] ⑧ 테스트 — 최대 체력 1500, 물 안 줄고 체력 소모
- [ ] ⑥ 테스트 — ×2.00, 피격 시 500→375, 회복 불가
- [ ] ⑦ 테스트 (2인) — 근접 시 버프, 이탈 시 양쪽 해제
- [ ] 회귀 확인 — 돌파 칩 전부 해제 시 이전과 동일
