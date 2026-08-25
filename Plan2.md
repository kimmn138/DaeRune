# Plan2 — 재화 기반 로봇 업그레이드 시스템 (Currency · Slot · Chip)

> 작성일: 2026-07-01
> **1차 개정: 2026-07-27** (경험치·레벨 방식 폐기 → 재화 소비형 업그레이드)
> **2차 전면 개정: 2026-07-30** (랭크 구매 방식 폐기 → **슬롯 해금 + 칩 장착** 방식으로 대체)
> **3차 개정: 2026-08-15** (스탯 슬롯 / 돌파 슬롯 분리 폐기 → **구분 없는 통합 슬롯 6칸**)
> 대상 브랜치: `feat/PlayExpo`
> 작성 목적: 스팀 계정마다 **재화(Currency)** 를 누적하고, **로비의 업그레이드 장치**에서
> 로봇(정원로봇/자판기/청소기)의 **업그레이드 슬롯을 해금**한 뒤 **업그레이드 칩을 장착**하는
> 영구 성장 시스템의 상세 구현 계획.

---

## 0. 구현 현황 (Implementation Status)

> 마지막 갱신: 2026-08-16 — **M1~M4 + M1' 코드 완료(UI 제외).**
> 통합 슬롯 6칸 전환이 끝나 슬롯/칩 API 에서 카테고리 인자가 전부 사라졌다.
> **남은 작업은 전부 에디터 작업이다.** 실행 순서는 **§23 마스터 실행 순서**를 따른다
> (§22 는 UI 제작 순서 그 자체로는 유효하지만, 전체 일정에서는 §23 이 우선한다 — §23.0 참조).

| 마일스톤 | 상태 | 비고 |
|---|---|---|
| **M1. 데이터/저장/슬롯·칩 규칙 토대** | ✅ 코드 완료 | 신규 타입/칩 카탈로그 에셋/Config/세이브 v4/GameInstance API |
| **M1'. 통합 슬롯 전환 (3차 개정)** | ✅ 코드 완료 | 카테고리 인자 제거 · 세이브 v4 · `SlotChips[6]` 단일 배열 (§17). 빌드 통과 |
| **M2. 업그레이드 → 능력치 반영** | ✅ 코드 완료 | PlayerState 운반 + `GE_Upgrade_Stats` + 컨테이너 체력/받는 피해/물 획득량 |
| **M3. 업그레이드 → 스킬 반영** | ✅ 코드 완료 | 피해량 / 물 소모량 / 쿨다운 / 투사체 수 |
| **M4. 재화 획득 파이프라인** | ✅ 코드 완료 | 스테이지 최초 클리어 + 업적/클리어 조건, 1회성 원장 |
| **M5a. UI 지원 코드 보강** | ✅ 코드 완료 | 슬롯 뷰모델 / 칸 단위 장착 API / DragDropOperation / 위젯 C++ 베이스 (§21). 빌드 통과, 구현 결과는 §21.10 |
| **M5b. 로비 업그레이드 UI (위젯)** | ⬜ 미착수 (UI 작업) | 시안 기준 화면 제작. **진입 훅은 완료** (장치 액터 + PC 진입/종료 API). 스펙 §10·§18~§20, 제작 순서 §22 |
| **M6. 콘텐츠 에셋 + 튜닝** | ⬜ 미착수 | `DA_ChipCatalog` / `DA_ProgressionConfig` / `GE_Upgrade_Stats` / 쿨다운 GE 개조 |
| **업적 / 클리어 조건 판정** | ⬜ 미착수 (설계 선행) | `GrantStageAchievement()` 호출부 **0곳**. 조건이 확정돼야 착수 가능 (§23 Phase G) |

> ⚠️ **"코드 완료"는 "컴파일된다"까지의 의미다.** 2026-08-16 기준 콘텐츠 에셋이 **하나도 없다**
> (`DA_ChipCatalog` / `DA_ProgressionConfig` / `GE_Upgrade_Stats` / `BP_DRUpgradeStation` 전부 부재,
> `Content/Blueprints/UI/Upgrade/` 는 빈 폴더). 따라서 `UDRGameInstance::ProgressionConfig` 가 null 이고
> **M2~M4 의 모든 경로가 아직 한 번도 실행된 적이 없다** — 전부 `InvalidConfig` 로 early-return 중이다.
> → 그래서 §23 의 1순위는 UI가 아니라 **배관 연결(Phase A) + 백엔드 검증(Phase B)** 이다.
>
> M2~M4는 **장착된 칩 목록(`TArray<FName>`)** 만 소비하므로 통합 슬롯 전환의 영향을 받지 않았다.
> M1' 의 변경 범위는 §17에 한정된다.

### 3차 개정에서 폐기된 2차 설계

| 폐기 항목 | 이유 |
|---|---|
| **슬롯 카테고리 분리**(스탯 슬롯 3 + 돌파 슬롯 3) | 슬롯은 **구분 없는 6칸**이다. 어떤 칩이든 어느 칸에나 들어간다. |
| **카테고리별 장착 제한**(스탯 칩 → 스탯 슬롯 전용 등) | 제한이 사라졌다. 남는 제약은 빈 칸 수 / 해금 단계 / 중복 금지뿐. |
| **카테고리별 해금 수·비용 배열**(`UnlockedStatSlots` / `StatSlotCosts` 등) | 단일 `UnlockedSlots`(0..6) + 단일 `SlotCosts[6]` 로 통합. |
| `EDRUpgradeResult::WrongSlotType` | 잘못된 슬롯 종류라는 개념 자체가 없어졌다. |
| `EDRChipCategory` 의 **장착 규칙 역할** | enum 자체는 남기되 **UI 분류·표기 전용**으로 격하한다 (§4.4). |

### 2차 개정에서 폐기된 1차 설계 (코드에서 제거 완료)

| 폐기 항목 | 이유 |
|---|---|
| 업그레이드 **랭크**(`MaxRank`/`CostPerRank`/`FDRUpgradeEntry.Rank`) | 칩은 랭크가 없다. 장착/미장착 2상태뿐. |
| 업그레이드 **개별 구매 비용** | 재화는 **슬롯 해금에만** 사용한다. 칩 장착/교체/제거는 무료. |
| **선행/배타 조건**(`Prerequisites` / `MutuallyExclusive`) | 해금 조건이 "슬롯 해금 단계" 하나로 단일화됐다. |
| **처치 수 재화**(`CurrencyPerKill` / `KillMilestones`) | 반복 획득 금지 요구사항과 충돌. 재화 총량이 유한해야 한다. |
| **페이즈 수 비례 재화**(`BaseStageCurrency` / `CurrencyPerClearedPhase`) | 게임오버·반복 플레이로 무한 파밍이 가능해 총량 상한이 성립하지 않는다. |
| 업그레이드 **1랭크 환불** | 랭크가 없으므로 무의미. 슬롯 환불만 남긴다(§6.5). |

---

## 1. 기능 요약 (Requirements)

1. **스테이지를 클리어**하면 클리어 방식에 따라 재화를 획득한다.
2. **업그레이드 시스템은 스테이지1 최초 클리어 시 해금**된다. 해금 전에는 장치 상호작용이 막힌다.
3. **업그레이드 진행도와 장착 상태는 플레이어 개인**에게 적용된다 (계정 = 그 PC의 로컬 세이브).
4. **로비에서만** 슬롯을 해금하거나 칩을 교체할 수 있다. **캐릭터 선택 이후**(= 대기실에서 클래스를 고른 뒤 자유 이동 상태)에 가능하다.
5. **재화는 업그레이드 슬롯 해금에만** 사용된다.
6. **재화 획득 경로 3종** — 전부 **계정당 1회성**이다.
   - 스테이지 최초 클리어 (스테이지당 1회)
   - 스테이지 업적 달성 (업적당 1회)
   - 특정 클리어 조건 달성 (조건당 1회)
7. **재화 총량에 상한이 있다.** 동일 스테이지·업적을 반복해도 재화가 다시 나오지 않는다.
8. **최종적으로 모든 로봇의 모든 슬롯을 해금할 수 있는 총량**을 제공한다.
   → 재화 부족으로 특정 로봇의 업그레이드를 영구 포기해야 하는 상황이 없어야 한다. (§5.4 예산 검증)
9. **슬롯 구성**: 로봇별 **구분 없는 통합 슬롯 6칸**. 처음에는 전부 잠겨 있고 **순차 해금**한다.
10. **슬롯 해금 단계에 따라 사용 가능한 칩 종류가 늘어난다.** (칩마다 `RequiredSlotTier`, 1..6)
11. **슬롯에 종류 구분이 없다.** 스탯 칩이든 돌파 칩이든 **아무 칸에나** 장착할 수 있다.
    → 유일한 장착 제약은 ① 빈 칸이 `RequiredSlotCount` 이상 남았는가, ② 칩이 해금됐는가, ③ 중복이 아닌가.
12. **같은 칩을 중복 장착할 수 없다.**
13. **슬롯을 비운 채 스테이지 입장 가능**하다. **스테이지 진행 중에는 칩 교체 불가.**
14. 로비에 **업그레이드 전용 장치**를 배치하고, 상호작용하면 업그레이드 화면으로 전환한다.
15. UI(§10): 좌측 = 로봇의 통합 슬롯 6칸, 우측 = **분류 없는 단일 칩 목록**(스탯/돌파를 나누지 않는다 — 나누면 장착 제한이 있는 것처럼 보인다), 드래그앤드롭 장착.

---

## 2. 현재 코드베이스 분석 (이 기능과 직결되는 사실들)

### 2.1 기본 스탯은 GE로 초기화된다 — 업그레이드를 얹기 좋은 구조
- `ADRCharacter::InitAbilityActorInfo()` 끝에서 서버 권한일 때 `InitializeDefaultAttributes()` 를 호출한다.
- 그 함수는 `UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes()` 를 타고
  **활성 GE 제거 → BaseValue 0 리셋 → 클래스별 `PrimaryAttributes`/`VitalAttributes` GE 재적용** 순으로 동작한다.
- 대상 어트리뷰트는 `MaxHealth` / `MaxWater` / `MoveSpeed` / `Health` / `Water`.
- → **업그레이드 스탯은 이 초기화가 끝난 "다음"에 Infinite GE 한 장으로 얹는다.** 기존 초기화 로직은 무변경.

### 2.2 ⚠️ 플레이어의 `MaxHealth` 는 직접 올리면 안 된다 (1차 설계의 오류 — 수정됨)
- 플레이어 체력은 **컨테이너 체력 시스템**이다. `UDRPlayerAttributeSet` 는
  `NumContainers`(4) / `ContainerHealth`(100) 를 들고 `GetCurrentContainerIndex()` 를
  `FloorToInt(Health / ContainerHealth)` 로 계산한다.
- `ExitCorruptedState()` 는 정상 최대 체력을 **`NumContainers * ContainerHealth` 로 재설정**한다.
- 따라서 `MaxHealth` 만 GE로 올리면
  1) 컨테이너 인덱스가 `NumContainers-1` 에서 클램프돼 초과분이 UI에 안 보이고,
  2) 오염 해제 시 업그레이드분이 **소멸**한다.
- → 체력 업그레이드는 **`EDRUpgradeStat::ContainerHealth`(컨테이너 1칸 용량)** 로 정의하고,
  `MaxHealth` 에는 `NumContainers × (업그레이드 후 − 기본)` 만큼을 Flat 으로 얹어 불변식을 유지한다. (§8.4)
- `ADRCharacter::NumContainers` / `ContainerHealth` 는 **복제되지 않는다.** 서버는 `GameBalanceConfig` 에서,
  클라는 BP CDO 기본값에서 읽는다. 업그레이드 배율은 양쪽 모두 **복제된 칩 목록**에서 로컬 계산하므로 일치한다.
  (BP 기본값과 `GameBalanceConfig` 값이 어긋나 있으면 그건 기존 버그다 — 콘텐츠 점검 항목.)

### 2.3 스킬 수치는 전부 어빌리티 CDO의 고정값이다 — 런타임 보정 지점이 필요하다
| 대상 | 현재 위치 | 현재 방식 |
|---|---|---|
| 물 소모량 | `UDRGameplayAbility::WaterCost` | CDO 고정 float. `CheckCost`/`ApplyCost` 가 그대로 사용 |
| 피해량 | `UDRDamageGameplayAbility::Damage` | `FScalableFloat`. `Damage.GetValueAtLevel(GetAbilityLevel())` 를 **3곳**에서 호출 |
| 딜레이(쿨다운) | 어빌리티 BP의 `CooldownGameplayEffect` | GE Duration 고정값. 코드에 보정 훅 없음 |
| 투사체 수 | `UDRProjectileSpell::NumProjectiles` | CDO 고정 int32. `UDRFireBolt::SpawnProjectiles()` 가 사용 |

- 어빌리티 레벨은 부여 시 1로 하드코딩된다(`FGameplayAbilitySpec(AbilityClass, 1)`).
  레벨 방식을 폐기했으므로 **그대로 둔다.** 어빌리티가 업그레이드 값을 직접 조회한다. (§8.5)
- `UDRFireBolt::MaxNumProjectiles` 는 **아무도 읽지 않는 죽은 프로퍼티**다(실사용은 부모의 `NumProjectiles`).
  이번 작업에서 건드리지 않되, 혼동 방지를 위해 향후 제거 대상으로 남긴다.
- `UDRSeedCannon::SpawnSeedProjectile()` 은 호출 1회 = 투사체 1발이라 개수 개념이 없다.
  투사체 수 칩의 대상이 아니다(대상으로 만들려면 BP 호출부를 루프로 바꿔야 한다).

### 2.4 스킬을 식별할 키가 이미 있다
- `FDRGameplayTags` 에 스킬별 태그가 있다:
  `Abilities.GardenRobot.{SeedCannon, WaterPump, ClawSwipe}`,
  `Abilities.VendingMachine.{BasicAttack, AttackSpeedBuff}`,
  `Abilities.RobotVacuum.{AirShot, JetJump, Dash}`
- 어빌리티는 `GetAssetTags()` 로 자기 태그를 얻는다. `"Abilities"` 접두 태그를 고르는 규칙이
  `UDRAbilitySystemComponent::GetAbilityTagFromSpec()` 에 이미 있다.
- → **칩 모디파이어의 대상 키 = 이 스킬 태그.** 신규 식별자를 만들 필요가 없다.

### 2.5 저장 시스템
- `UDRSaveGame` 은 로컬 단일 슬롯(`DaeRunePlayerProgress`, UserIndex 0).
- `UDRGameInstance::Init()` 이 `LoadProgress()` 를 호출한다.
- 스팀: `OnlineSubsystemSteam` 활성. 한 PC = 한 스팀 계정이 보통이므로
  **"로컬 세이브 = 그 PC의 계정 진행도"** 가 성립한다. (강화안은 §7.5)

### 2.6 클래스 선택 / 로비 구조
- 선택 클래스: `ADRPlayerState::SelectedPlayerClass` (복제 + RepNotify).
- `EPlayerCharacterClass { Gardener, VendingMachine, RobotVacuum, Count UMETA(Hidden) }`
  - **배열/맵 크기 하드코딩 금지.** `(int32)EPlayerCharacterClass::Count` 로 산정한다.
- 로비 흐름: `ELobbyState::WaitingRoom`(고정 카메라 · 클래스 선택) → `Transitioning` → `FreeRoam`(자유 이동).
- `FreeRoam` 에는 이미 `ADRStageSelectActor`(스테이지 포털)가 배치돼 있다.
  → **업그레이드 장치는 같은 자리에 놓이는 상호작용 액터**로 만든다. 클래스 선택이 끝난 뒤라는
  요구사항이 자연히 충족된다. (§8.8)

### 2.7 스테이지 종료 흐름 (재화 지급 지점)
- 서버 권한 `ADRStageGameMode::TriggerGameOver()` / `TriggerGameClear()`
  → `NotifyAllPlayersGameEnd(bIsGameClear)`
  → `WipeoutDelayTime`(5초) 타이머 후 `ReturnToLobby()`(`ServerTravel`).
- 결과 UI는 `PC->Client_ShowGameOverUI()` / `Client_ShowGameClearUI()`.
- 도달 페이즈는 `ADRStageGameState::GetCurrentPhaseIndex()`(0-base, 복제).

### 2.8 적 처치 판정 지점이 이미 있다
- 적 사망은 `UDREnemyAttributeSet::PostGameplayEffectExecute()` 의 `bFatal` 분기에서 확정되고,
  그 시점에 `Props.SourceController`(마지막 타격자)가 채워져 있다.
  → **처치 크레딧을 붙일 최적 지점.** 신규 델리게이트가 필요 없다. (§8.7)
- 처치 수는 **재화로 환산되지 않는다.** 업적 판정 입력과 결과창 표시에만 쓴다.

### 2.9 핵심 아키텍처 난점
- **권한 분리**: "클리어했는가 / 업적을 달성했는가"는 **서버만** 안다.
  하지만 "내 재화와 장착 상태"는 **각 플레이어 본인 PC의 로컬 세이브**에 있다.
  → 진행도는 **클라이언트가 권위적으로 관리**하고, 서버는 **성과 원본만 통지**한다.
- 반대 방향으로, 스폰 시 칩 효과를 적용하려면 서버가 각 플레이어의 **장착 목록**을 알아야 한다
  → 클라이언트가 자기 장착 목록을 서버로 올려 PlayerState에 싣는다 (Server RPC).

---

## 3. 설계 개요 (High-Level Design)

```
[클라이언트 로컬 세이브 (계정별, 권위적 진행도)]
        UDRSaveGame  (SaveVersion 3)
        ├─ int32 Currency / LifetimeCurrency            // 계정 공용 지갑
        ├─ bool  bUpgradeSystemUnlocked                 // 스테이지1 최초 클리어 시 true
        ├─ TArray<FName> ClaimedRewards                 // 1회성 재화 원장 (중복 지급 차단)
        └─ TMap<EPlayerCharacterClass, FDRClassUpgradeState>
              ├─ UnlockedSlots        (0..6, 카테고리 구분 없음)
              ├─ SlotChips[6]         (칸별 점유 칩 Id — 어떤 칩이든 어느 칸에나)
              └─ SpentCurrency        (슬롯 해금 총액 = 환불 상한)
                │  (로비 장치에서 해금/장착 — UDRGameInstance API가 즉시 저장)
                │
                ▼  ServerReportUpgradeLoadout(Class, TArray<FName>) → ServerRPC
[서버: ADRPlayerState]
        ├─ EquippedChips : TArray<FName>        (Replicated → 전 머신 수신)
        └─ CachedUpgradeRuntime : FDRUpgradeRuntime   (각 머신이 로컬 Resolve, 복제 안 함)
                │  (캐릭터 스폰 시 / 목록 변경 시)
                ├─→ 기본 스탯: GE_Upgrade_Stats(SetByCaller) 적용 + 컨테이너 체력 반영
                └─→ 스킬 수치: 어빌리티가 사용 시점에 CachedUpgradeRuntime 조회
                ▼
[플레이 → 스테이지 클리어 (서버)]
        ADRStageGameMode::NotifyAllPlayersGameEnd()
        └─ FDRStageRewardReport{StageId, PlayedClass, bGameClear, ClearedPhaseCount,
                                KillCount, AchievementIds}
           → 각 PC에 Client_GrantStageReward(Report) RPC
                │
                ▼
[클라이언트: 1회성 원장 확인 → 지갑 가산 → 저장 → 결과창 연출]
        UDRGameInstance::ApplyStageReward(Report) → FDRStageRewardResult
```

설계 원칙:
- **진행도(재화/장착)의 단일 진실 원천(SSOT)은 각 클라이언트의 로컬 세이브.** 서버는 "성과 보고"만 한다.
- **PvE 협동**이라 클라이언트 신고 장착 목록을 신뢰한다(치트해도 자기 캐릭터 강화뿐).
  단 서버는 **구조적 검증(`SanitizeLoadout`)** 으로 존재하지 않는 칩/타 클래스 칩/슬롯 총량 초과를 걸러낸다. (§7.6)
- **업그레이드 수치는 단일 해석 경로(`FDRUpgradeRuntime`)로만 조회한다.** 계산식이 흩어지지 않게 한다.
- **재화 원장은 단일 배열(`ClaimedRewards`)** 로 통일한다. "1회성"이 자료구조 차원에서 보장된다.

---

## 4. 데이터 모델

### 4.1 세이브 (`UDRSaveGame`, SaveVersion = 4)
```cpp
bool  bHasCompletedTutorial;                                  // 기존 유지
int32 Currency;                                               // 현재 보유 재화 (계정 공용)
int32 LifetimeCurrency;                                       // 통계용 누적 획득량
bool  bUpgradeSystemUnlocked;                                 // 스테이지1 최초 클리어로 해금
TArray<FName> ClaimedRewards;                                 // 지급 완료 보상 Id (1회성 원장)
TMap<EPlayerCharacterClass, FDRClassUpgradeState> ClassUpgrades;   // v4: 통합 슬롯 6칸 (§4.2)
int32 SaveVersion;   // static constexpr CurrentSaveVersion = 4
```
- **마이그레이션**: `SaveVersion != 4` 이면 진행도 필드를 초기화하고 v4로 스탬프한다.
  v1(경험치/레벨), v2(랭크 구매), v3(슬롯 카테고리 분리)의 제거된 프로퍼티는
  UE 세이브 역직렬화 규칙상 자동으로 버려진다.
  미출시 단계라 구 진행도 → 신 진행도 환산은 하지 않는다. (`UDRGameInstance::EnsureProgressInitialized()`)
- **누락 키 lazy 초기화**: `(int32)EPlayerCharacterClass::Count` 만큼 순회하며 `FindOrAdd`.

### 4.2 슬롯/칩 보유 상태 (`FDRClassUpgradeState`)
```cpp
int32 UnlockedSlots = 0;      // 0..MaxSlots(6). 카테고리 구분 없는 통합 해금 수
TArray<FName> SlotChips;      // 길이 = MaxSlots. 칸별 점유 칩 Id (없으면 NAME_None)
int32 SpentCurrency = 0;      // 슬롯 해금에 지불한 총액 (환불 상한)
```
- **왜 칸 배열인가**: UI가 `01..06` 슬롯을 개별 칸으로 보여주고 드래그앤드롭 목표가 칸이다.
  여러 칸을 점유하는 칩(`RequiredSlotCount` ≥ 2)은 **같은 ChipId를 여러 칸에 기록**한다.
  점유 칸 수 = 그 Id가 배열에 등장하는 횟수 → 자료구조만으로 정합성이 유지된다.
- **`SpentCurrency` 의 역할**: 환불 상한. 슬롯 비용을 나중에 올려도 지불액보다 많이 돌려주지 않는다.
- **카테고리 인자를 받는 헬퍼는 전부 삭제했다.** 배열이 하나뿐이라 분기할 것이 없으므로
  래퍼 접근자도 두지 않고 **`State.SlotChips` / `State.UnlockedSlots` 를 직접 읽는다.**
  남은 헬퍼는 계산이 필요한 셋뿐이다 — `CountFreeSlots()` / `CountOccupiedSlots(ChipId)` / `GetEquippedChips(Out)`.
- **해금 칸은 항상 앞에서부터**다: `SlotChips[0..UnlockedSlots-1]` 이 사용 가능 칸,
  `[UnlockedSlots..MaxSlots-1]` 은 잠긴 칸. 순차 해금이라 이 불변식이 성립한다.

### 4.3 칩 효과 (`FDRUpgradeModifier`)
```cpp
EDRUpgradeStat Stat;              // ContainerHealth / MaxWater / MoveSpeed / DamageTaken / WaterGain
                                  // SkillDamage / SkillWaterCost / SkillCooldown / SkillProjectileCount
FGameplayTag   TargetAbilityTag;  // Skill* 스탯일 때 대상 스킬. 비어 있으면 "모든 스킬"
EDRUpgradeOp   Op;                // Flat(절대값) / Percent(0.1 = +10%)
float          Value;             // 음수면 감소
bool           bIsDrawback;       // 단점 효과 → UI 빨간색/경고. 돌파 칩은 상세 토글과 무관하게 항상 표시
FText          DetailOverride;    // 비면 "스탯명 ±수치" 자동 생성
```
**합산 규칙(순서 의존성 제거)**: `최종값 = (Base + ΣFlat) × (1 + ΣPercent)`
- 두 누산기(`FDRResolvedStat{Flat, Percent}`)로만 모으므로 장착 순서가 결과에 영향을 주지 않는다.
- 스킬 스탯은 **"모든 스킬" 성분(전역) + "해당 태그" 성분**을 합산한다. (`FDRUpgradeRuntime::ApplySkill`)

### 4.4 칩 정의 (`FDRUpgradeChipDefinition`, `UDRChipCatalog` DataAsset)
```cpp
FName    ChipId;                     // 세이브/복제 키. 한 번 정하면 변경 금지
EPlayerCharacterClass OwnerClass;    // 어느 로봇의 칩인지
EDRChipCategory Category;            // Stat / Ascension — ★UI 분류·표기 전용. 장착 제한 아님
FGameplayTag TargetAbilityTag;       // "적용 대상 스킬" 표기용 (없으면 캐릭터 전체)
FText DisplayName;                   // 칩 이름
FText EffectSummary;                 // 이해하기 쉬운 효과 요약 (기본 표시)
FText Description;                   // 부가 설명(선택)
UTexture2D* Icon;
int32 RequiredSlotCount = 1;         // 점유 슬롯 수 (1..MaxSlots)
int32 RequiredSlotTier  = 1;         // 슬롯을 N개 이상 해금해야 사용 가능 (1..MaxSlots)
TArray<FDRUpgradeModifier> Modifiers;// 장점 + 단점(bIsDrawback) 모두 여기에
```
- **`RequiredSlotTier` 가 유일한 칩 해금 조건**이다. "슬롯 해금 단계에 따라 칩 종류가 증가"를 그대로 구현.
  3차 개정으로 비교 대상이 **카테고리별 해금 수 → 통합 `UnlockedSlots`(0..6)** 로 바뀌었다.
  기존 카탈로그 값(1..3 스케일)은 6칸 스케일로 재조정해야 한다 (§6.2).
- **`Category` 는 더 이상 어느 칸에 꽂히는지를 결정하지 않는다.** 남는 역할은 두 가지뿐이다:
  ① `ValidateCatalog()` 의 "단점 없는 돌파 칩" 경고, ② 툴팁/표기상의 분류.
  (우측 목록의 필터 탭도 `Category` 를 썼지만 **단일 목록으로 통합하면서 폐기**했다 — §18.4)
  → 장착 판정 코드에서 `Category` 를 읽는 곳은 **하나도 없어야 한다.**
- 돌파 칩은 관례적으로 `Category = Ascension` + 단점 모디파이어를 1개 이상 갖는다.

### 4.5 재화/슬롯 규칙 (`UDRProgressionConfig` DataAsset)
```cpp
UDRChipCatalog* ChipCatalog;          // 칩 정의 카탈로그

// 슬롯 (카테고리 구분 없음)
int32 MaxSlots          = 6;
TArray<int32> SlotCosts;              // index = 해금할 슬롯 번호-1. 비면 DefaultSlotCost * 번호
int32 DefaultSlotCost   = 500;
bool  bAllowSlotRefund  = true;       // 마지막 해금 슬롯 환불 허용 (요구사항 8의 안전망)
float RefundRatio       = 1.0f;       // 1.0 = 전액 환불

// 재화 획득 (전부 1회성)
TArray<FDRStageRewardDef>   StageRewards;   // {StageId, DisplayName, FirstClearCurrency, bUnlocksUpgradeSystem}
TArray<FDRAchievementDef>   Achievements;   // {AchievementId, Kind, StageId, 이름/설명, Currency}
```
- 재화 환산이 필요한 곳은 **클라이언트**(서버 없는 메인메뉴/로비에서도 필요) → `UDRGameInstance` 가 보유한다.
- 슬롯 비용은 **순차 해금** 비용이다(1번 칸부터 6번 칸까지). 예: `{400, 700, 1100, 1500, 1900, 2400}` (= 로봇당 8000).
  - 카테고리 분리 시절의 두 배열(`{500,900,1500}` + `{800,1400,2200}`)을 하나로 합치고 단조 증가로 재배치한 값이다.
  - 로봇 수(3)를 곱한 총 필요액이 재화 총량을 넘지 않는지는 §5.4가 검증한다.

### 4.6 런타임 캐시 (`FDRUpgradeRuntime`)
```cpp
TArray<FDRResolvedStat> GlobalStats;                 // index = (int32)EDRUpgradeStat
TMap<FGameplayTag, FDRSkillStatBlock> SkillStats;    // 스킬 태그별

float Apply(EDRUpgradeStat, float Base) const;                              // 캐릭터 단위
float ApplySkill(const FGameplayTag&, EDRUpgradeStat, float Base) const;    // 스킬 단위
```
- `UDRChipCatalog::ResolveLoadout(EquippedChips, Class, OutRuntime)` 이 칩 Id 배열을 이 캐시로 펼친다.
- **복제하지 않는다.** 서버/각 클라가 복제된 `EquippedChips` 로부터 각자 구축한다(대역폭 절약 + 예측 일관성).
- 비어 있으면 **항등 함수**다 → 칩을 하나도 안 낀 상태는 개편 전과 수치가 완전히 동일하다.

---

## 5. 재화 획득 규칙

> 전부 `UDRProgressionConfig` 로 튜닝. 아래는 초기 권장값.

### 5.1 스테이지 최초 클리어
- 지급 조건: `bGameClear == true` **이고** 원장에 `StageClear.<StageId>` 가 없을 때.
- 원장 키는 `StageId` 에서 결정적으로 생성한다(`FDRStageRewardDef` 에 별도 키 필드 없음).
- **게임오버는 재화를 주지 않는다.** (요구사항 1·7 — 클리어 방식에 따른 획득 + 반복 획득 금지)
- 권장값: `Stage1.FirstClearCurrency = 1200`.
- `bUnlocksUpgradeSystem = true` 인 스테이지를 최초 클리어하면 **업그레이드 시스템이 해금**된다(요구사항 2).
  해금은 재화 지급과 별개로 원장에 남으므로, 이미 해금된 뒤에는 다시 처리하지 않는다.

### 5.2 업적 / 클리어 조건
- 판정은 **서버**가 한다(그 스테이지의 규칙을 서버만 안다).
- 지급/중복 방지는 **클라이언트 세이브**(`ClaimedRewards`)가 한다.
- `EDRRewardKind { StageFirstClear, Achievement, ClearCondition }` — 결과창 라벨과 UI 분류에만 쓴다.
- 예시:
  | AchievementId | Kind | 조건 | 보상 |
  |---|---|---|---|
  | `Stage1.NoDeath` | Achievement | 아무도 쓰러지지 않고 클리어 | 400 |
  | `Stage1.PerfectSite` | ClearCondition | 사이트 체력 100% 유지 클리어 | 500 |
  | `Stage1.FastClear` | ClearCondition | 10분 이내 클리어 | 400 |
  | `Stage1.NoPartDrop` | Achievement | 부품을 한 번도 떨어뜨리지 않고 클리어 | 300 |
- 판정 코드는 페이즈/게임모드에 흩지 말고
  **`ADRStageGameMode::GrantStageAchievement(FName Id, ADRPlayerController* PC = nullptr)`** 하나로 모은다
  (PC가 null이면 전원). 각 페이즈/BP는 이 함수만 호출한다. (§8.7)

### 5.3 처치 수
- **재화로 환산하지 않는다.** `ADRPlayerState::StageKillCount`(복제)는
  ① 업적 판정 입력, ② 인게임/결과창 표시 용도다.

### 5.4 총량 상한과 예산 검증 (요구사항 7·8의 구현)
```
총 획득 가능 재화 = Σ StageRewards.FirstClearCurrency + Σ Achievements.Currency
총 필요 재화     = (Σ SlotCosts, 6칸 전부) × 로봇 수(EPlayerCharacterClass::Count)
```
- `UDRProgressionConfig::ValidateCurrencyBudget(OutErrors)` 가 **총 획득 ≥ 총 필요** 를 검사한다.
  콘텐츠 작업 후 반드시 1회 실행한다. 실패하면 특정 로봇을 영구 포기해야 하는 상태라는 뜻이다.
- 슬롯 환불(`bAllowSlotRefund`)은 예산이 아직 다 모이지 않은 중간 단계에서
  "다른 로봇을 먼저 키우고 싶다"를 가능하게 하는 **되돌리기 장치**다. 재화 총량을 늘리지 않는다
  (환불 비율 1.0 이하 · 상한은 실제 지불액).

### 5.5 어떤 로봇에 귀속되나?
- **재화는 계정 공용이라 귀속 개념이 없다.** 어느 로봇으로 벌든 같은 지갑에 들어간다.
- `FDRStageRewardReport::PlayedClass` 는 결과창 표기/통계용으로만 보낸다.

---

## 6. 슬롯 · 칩 시스템 상세

### 6.1 유형별 매핑 (요구사항 → 구현 수단)
| 칩 효과 | `EDRUpgradeStat` | 적용 방식 | 비고 |
|---|---|---|---|
| 컨테이너 체력 | `ContainerHealth` | `SetContainerInfo()` 갱신 + `GE_Upgrade_Stats` 의 MaxHealth Flat | §2.2 |
| 최대 물 | `MaxWater` | `GE_Upgrade_Stats` (SetByCaller Add/Multiply) | |
| 이동 속도 | `MoveSpeed` | 〃 | |
| 받는 피해 | `DamageTaken` | `UDRPlayerAttributeSet::HandleIncomingDamage` 에서 배율 조회 | 주로 단점용 |
| 물 획득량 | `WaterGain` | `ADREnemy::GrantWaterToPlayers` 의 지급량 배율 | |
| 스킬 피해량 | `SkillDamage` | `UDRDamageGameplayAbility::GetUpgradedDamage()` | 3개 호출부 통합 |
| 스킬 물 소모량 | `SkillWaterCost` | `UDRGameplayAbility::GetEffectiveWaterCost()` | 0 하한 |
| 스킬 딜레이(쿨다운) | `SkillCooldown` | `ApplyCooldown()` 오버라이드 + SetByCaller Duration | 0.05초 하한 |
| 스킬 투사체 수 | `SkillProjectileCount` | `UDRProjectileSpell::GetEffectiveNumProjectiles()` | 1 하한 |

### 6.2 카탈로그 예시 (정원로봇 — `DA_ChipCatalog` 작성 가이드)

> **해금 단계 = `RequiredSlotTier`** — 통합 슬롯을 N개 이상 해금해야 목록에 열린다(1..6).
> 카테고리 분리 시절의 1..3 스케일을 6칸 스케일로 재조정한 값이다(1 → 1, 2 → 3, 3 → 5).
>
> **`RequiredSlotCount` 는 전부 `1` 이다**(§6.4 콘텐츠 결정). 아래 표에 칸 수 열이 없는 이유다.

**스탯 칩 (Category = Stat — 표기 분류일 뿐, 6칸 어디에나 장착 가능)**
| ChipId | 효과 | 해금 단계 |
|---|---|---|
| `GR.Stat.Container` | ContainerHealth Flat +20 | 1 |
| `GR.Stat.Water` | MaxWater Flat +25 | 1 |
| `GR.Stat.Speed` | MoveSpeed Percent +6% | 1 |
| `GR.Stat.SeedDamage` | SkillDamage Percent +12% (`Abilities.GardenRobot.SeedCannon`) | 3 |
| `GR.Stat.SeedCost` | SkillWaterCost Percent −15% (SeedCannon) | 3 |
| `GR.Stat.PumpCooldown` | SkillCooldown Percent −20% (`Abilities.GardenRobot.WaterPump`) | 3 |
| `GR.Stat.AllDamage` | SkillDamage Percent +15% (태그 비움 = 전체) | 5 |
| `GR.Stat.Siphon` | WaterGain Percent +30% | 5 |

**돌파 칩 (Category = Ascension — 장점 + 단점 동반. 역시 6칸 어디에나 장착 가능)**
| ChipId | 장점 | 단점(`bIsDrawback`) | 해금 단계 |
|---|---|---|---|
| `GR.Asc.Overdrive` | 모든 스킬 피해 +35% | 모든 스킬 물 소모 +50% | 1 |
| `GR.Asc.GlassCannon` | 모든 스킬 피해 +60% | 받는 피해 +40% | 3 |
| `GR.Asc.Turbine` | 이동 속도 +25% | 컨테이너 체력 −20% | 1 |
| `GR.Asc.Barrage` | SeedCannon 투사체 +2 | SeedCannon 피해 −30% | 3 |
| `GR.Asc.Bulwark` | 받는 피해 −25% | 이동 속도 −15% | 5 |
| `GR.Asc.Flood` | 물 획득량 +60% | 모든 스킬 쿨다운 +25% | 5 |
- 자판기/청소기도 같은 패턴으로 각자의 스킬 태그(`Abilities.VendingMachine.*`, `Abilities.RobotVacuum.*`)에 맞춰 작성한다.
- **밸런스 주의(3차 개정의 핵심 부작용)**: 슬롯 구분이 없어졌으므로 **6칸을 전부 돌파 칩으로 채우는 빌드**가
  가능해졌다. 예전에는 돌파 칩이 최대 3칸으로 강제 제한됐다.
  → 돌파 칩의 단점 강도를 "3칸까지" 기준이 아니라 **"6칸 전부" 기준**으로 다시 잡아야 한다.
  극단 빌드를 막고 싶으면 §12.13의 선택지(카탈로그 태그 기반 상한)를 검토한다.
- ⚠️ **칸 수를 비용으로 쓸 수 없다.** 모든 칩이 1칸이므로, 예전에 2~3칸으로 값을 매기던
  강한 칩들(`GR.Stat.AllDamage`, `GR.Asc.Flood` 등)은 **지금 1칸에 그 성능을 준다.**
  남은 비용 수단은 **`RequiredSlotTier`(해금 단계) 하나뿐**이다 —
  강한 칩일수록 5단계로 미루고, 그래도 세면 **효과 수치 자체를 내린다.**

### 6.3 슬롯 해금 규칙 (`UDRGameInstance::CanUnlockSlot`)
검사 순서와 실패 사유(`EDRUpgradeResult`) — **카테고리 인자를 받지 않는다**:
1. `ProgressionConfig`/`ChipCatalog` 미지정 → `InvalidConfig`
2. 업그레이드 시스템 미해금 → `SystemLocked`
3. 6칸을 이미 전부 해금 → `AllSlotsUnlocked`
4. 지갑 부족 → `NotEnoughCurrency`
5. 통과 → `Success`

> UI는 이 결과 코드를 그대로 받아 버튼 비활성 사유로 쓴다. 별도 판정 로직을 UI에 복제하지 말 것.

### 6.4 칩 장착 규칙 (`UDRGameInstance::CanEquipChip`)
1. Config/카탈로그 미지정 → `InvalidConfig`
2. 시스템 미해금 → `SystemLocked`
3. 카탈로그에 없는 ChipId → `UnknownChip`
4. `Chip.OwnerClass != CharacterClass` → `WrongClass`
5. `Chip.RequiredSlotTier > UnlockedSlots` → `ChipLocked` (통합 해금 수와 비교)
6. 이미 장착 중 → `AlreadyEquipped` (동일 칩 중복 장착 금지)
7. 남은 빈 슬롯 < `RequiredSlotCount` → `NotEnoughSlots`
   → 이때 UI는 `OutRequiredSlots` / `OutFreeSlots` 로 **필요한 슬롯 수**를 표시한다(요구사항 UI).
8. 통과 → `Success`

> **카테고리 검사는 없다.** 스탯 칩이든 돌파 칩이든 위 목록을 통과하면 어느 칸에나 들어간다.
> `WrongSlotType` 결과 코드는 enum에서 제거한다.

**칸 배정 규칙** (`EquipChip(Class, ChipId, PreferredSlotIndex)`):
- `PreferredSlotIndex` 가 유효(해금됨 + 비어 있음)하면 그 칸부터 채운다 = 드래그앤드롭한 칸.
  **칸 번호는 이제 0..5 전역 인덱스**다(카테고리별 0..2가 아니다).
- 남은 점유 칸은 앞에서부터 빈 칸을 순서대로 채운다.
- **연속 칸을 요구하지 않는다.** 빈 칸 총량만 충족하면 장착 가능 — 칸이 흩어져 장착이 막히는
  불필요한 실패를 없애기 위한 의도적 결정이다.
- 해제는 `UnequipChip(Class, ChipId)`(그 칩이 점유한 모든 칸 비움) 또는
  `UnequipSlot(Class, SlotIndex)`(그 칸을 점유한 칩 전체 해제).
- **장착/해제/교체는 전부 무료**다. 재화가 오가지 않는다.

> ### ★콘텐츠 결정 — 당분간 모든 칩은 1칸짜리다★
>
> `RequiredSlotCount` 는 **1 로 고정**해서 카탈로그를 만든다. 2칸 이상 먹는 칩은 만들지 않는다.
>
> **백엔드는 건드리지 않는다.** 위 7번 판정, 칸 배정, 세이브 정합성 검사(§6.6),
> `FDRSlotViewModel::GroupSize`/`GroupOrder`(§21.1)는 **이미 구현·검증된 상태로 그대로 둔다.**
> 나중에 다중 칸 칩을 도입하면 카탈로그에 숫자만 올리면 된다.
>
> **UI 는 이 전제 위에서 만든다** — 다음 표시는 **만들지 않는다**:
>
> | 안 만드는 것 | 원래 용도 | 대신 |
> |---|---|---|
> | `Img_GroupRing` · `Txt_GroupOrder` | 두 칸이 한 칩임을 묶어 표시 | 필요 없음 |
> | `Txt_SlotNeed` (드래그 비주얼) | "필요 슬롯 2" | 필요 없음 |
> | `Txt_SlotInfo` (툴팁) | "필요 슬롯 2칸 · 남은 칸 1" | `Txt_BlockReason` 이 실패 시에만 알림 |
> | 다중 칸 하이라이트 미리보기 | 함께 먹힐 칸들을 미리 칠함 | 드롭 대상 칸 하나만 |
>
> `NotEnoughSlots` 는 **6칸이 전부 찼을 때만** 발생한다.
> `SLOT n` 뱃지는 남는다 — 텍스처에 이미 그려져 있고, 값은 항상 `1` 이다.

### 6.5 슬롯 환불 규칙 (`RefundSlot`)
- **마지막으로 해금한 슬롯**(= `UnlockedSlots - 1` 번 칸)만 환불한다(순차 해금의 역순). 카테고리 인자 없음.
- 그 칸이 칩에 점유돼 있으면 → `SlotOccupied` (먼저 칩을 빼야 한다).
  - 다중 칸 칩이 그 칸을 물고 있는 경우도 동일하게 점유로 본다.
- `bAllowSlotRefund == false` 면 → `RefundDisabled`.
- 환불액 = `floor( min(그 슬롯 비용, SpentCurrency) × RefundRatio )`.
- 환불로 해금 단계가 내려가면 **그 단계에서만 쓸 수 있던 칩이 자동으로 해제**된다
  (`SanitizeClassState()` 가 처리 — 잠긴 칩이 장착된 상태로 남지 않는다).

### 6.6 카탈로그 개편 시 안전장치
- 칩을 삭제/개명하거나 소속 클래스를 바꾸면 세이브의 장착 항목이 "고아"가 된다.
- `EnsureProgressInitialized()` 가 로드 직후 `SanitizeClassState()` 를 돌려
  ① 배열 길이를 `MaxSlots`(6)에 맞추고, ② 고아/타 클래스/잠긴 칩을 비우고,
  ③ 점유 칸 수가 `RequiredSlotCount` 와 어긋난 칩을 정리한다.
  ④ `UnlockedSlots` 를 `0..MaxSlots` 로 클램프하고, 줄어들었으면 **차액을 지갑으로 환급**한다(재화 증발 방지).
  ⑤ 잠긴 칸(`index >= UnlockedSlots`)에 남아 있는 칩을 비운다.
  > **카테고리 불일치 검사는 삭제한다.** 어떤 칩이든 어느 칸에나 유효하므로 정화 대상이 아니다.
- 카탈로그 자체의 정합성(중복 Id, `RequiredSlotCount`/`RequiredSlotTier` 가 `1..MaxSlots` 범위 밖,
  단점 없는 돌파 칩 등)은 `UDRChipCatalog::ValidateCatalog()` 로 점검한다. 콘텐츠 작업 후 반드시 1회 실행.

---

## 7. 멀티플레이어 / 저장 위치 설계

### 7.1 장착 목록의 서버 반영 (스폰 전에 필요)
1. 클라이언트가 로컬 세이브에서 **선택 클래스의 장착 목록**을 읽는다.
2. `ADRPlayerController::ServerReportUpgradeLoadout(EPlayerCharacterClass ForClass, const TArray<FName>& Chips)`
   (Server, Reliable)로 서버에 전달한다.
3. 서버는 **`ForClass != PS->GetSelectedPlayerClass()` 면 폐기**한다(경합/스푸핑 방어).
   통과하면 `SanitizeLoadout()` 으로 정화한 뒤 `ADRPlayerState::SetEquippedChips()` 로 싣는다.
4. 복제된 배열을 **각 머신(서버 + 모든 클라)** 이 `ResolveLoadout()` 으로 로컬 캐시에 펼친다.

**보고 트리거 (경합 없는 이벤트 기반 — 타이머 폴링 없음)**
| 시점 | 담당 | 덮는 경우 |
|---|---|---|
| `ADRPlayerController::OnRep_PlayerState()` | 클라 | PlayerState 가 복제로 막 도착한 순간(최초 보고) |
| `ADRPlayerController::OnLevelEntered()` | 클라 + 리슨 호스트 | 호스트처럼 OnRep 이 없는 경우 / 레벨 이동 |
| `ADRPlayerState::OnRep_SelectedPlayerClass()` | 클라 | 로비에서 로봇을 바꿨을 때 |
| `ADRPlayerController::CloseUpgradeScreen()` | 클라 + 호스트 | 슬롯/칩을 만지고 화면을 닫았을 때 |

> **주의**: `ADRPlayerState::GetOwner()`(= 컨트롤러)는 클라이언트에서 늦게 복제될 수 있어
> PlayerState 쪽 보고 헬퍼는 Owner 를 믿지 않고 **로컬 컨트롤러 중 `PlayerState == this` 인 것을 찾아** 보고한다.

**PlayerState 추가**
```cpp
UPROPERTY(ReplicatedUsing = OnRep_EquippedChips)
TArray<FName> EquippedChips;

FDRUpgradeRuntime CachedUpgradeRuntime;   // 복제 안 함 — 각 머신이 로컬 구축

const FDRUpgradeRuntime& GetUpgradeRuntime() const;
void SetEquippedChips(const TArray<FName>& InChips);   // 서버 전용, 내부에서 Rebuild
UFUNCTION() void OnRep_EquippedChips();                // 클라: Rebuild
void RebuildUpgradeRuntime();                          // 공용 (+ 폰에 갱신 통지)
```
- **모든 머신이 캐시를 갖는 이유**: 어빌리티는 클라이언트에서 예측 실행되므로
  클라가 물 소모량/투사체 수/쿨다운을 서버와 동일하게 알아야 예측 불일치가 없다.
- 클래스가 실제로 바뀌면 서버는 `EquippedChips` 를 **비운다**(타 클래스 칩이 남지 않게).
  이후 클라이언트의 `OnRep_SelectedPlayerClass` 보고로 새 목록이 채워진다.
- `CopyProperties()`(Seamless Travel)에 `EquippedChips` 복사 → 로비↔스테이지 보존.
  `StageKillCount` 는 **복사하지 않는다**(스테이지 한정 값).

### 7.2 스폰 시 적용 지점
- `ADRCharacter::InitAbilityActorInfo()` 가 `PlayerCharacterClass = PS->GetSelectedPlayerClass()` 를 수행하고
  마지막에 서버 권한으로 `InitializeDefaultAttributes()` 를 호출한다.
- **그 직후** `RefreshUpgradeEffects()` 를 호출한다:
  - (모든 머신) 컨테이너 체력 재계산 → `PlayerAS->SetContainerInfo()`
  - (서버) `GE_Upgrade_Stats` 재적용 (`MaxHealth/MaxWater/MoveSpeed`)
- `RebuildUpgradeRuntime()` 도 폰이 있으면 `RefreshUpgradeEffects()` 를 호출한다
  → **보고가 스폰보다 늦게 도착해도 자동 치유**된다(순서 의존성 제거).
- **스킬 업그레이드는 스폰 시 적용할 것이 없다.** 어빌리티가 사용 시점에 캐시를 조회한다.

### 7.3 재화 지급 (스테이지 종료)
- 서버가 플레이어별 `FDRStageRewardReport` 를 만들어 `Client_GrantStageReward` 로 보낸다.
- 클라가 `ApplyStageReward()` 로 원장 확인·지갑 반영·저장하고 결과창에 내역을 표시한다.
- **주의**: 곧이어 `ReturnToLobby()` 의 `ServerTravel` 이 발생한다. Reliable RPC는 트래블 전에 큐잉되고
  현재 `WipeoutDelayTime`(5초) 지연이 있어 전송이 보장된다. 그래도 RPC는 결과 UI 표시 **전에** 보낸다.

### 7.4 리슨 서버 호스트
- 호스트도 자기 `ADRPlayerController` 를 가지므로 `Client_*` RPC가 로컬 실행된다 → 동일 경로로 저장된다.
- 호스트는 서버이기도 하므로 **중복 지급이 없는지** 검증한다(§14.2).
  1회성 원장이 있어 중복 호출도 두 번째부터는 0 지급이 된다(구조적 보호).

### 7.5 스팀 계정 키잉 (선택적 강화)
- 1차: 머신당 단일 로컬 슬롯으로 충분.
- 강화: `SlotName = FString::Printf(TEXT("Progress_%s"), *UniqueNetIdString)`.
  Steam 미연결(에디터/오프라인) 폴백은 기존 고정 슬롯명.

### 7.6 신뢰/치트 고려
- 클라가 장착 목록을 신고 → 위조 가능. **PvE 협동이라 영향은 자기 캐릭터 강화에 한정**된다.
- 서버 방어선(`UDRChipCatalog::SanitizeLoadout`):
  - 카탈로그에 없는 Id / 다른 클래스 소속 항목 제거
  - 중복 Id 제거 (동일 칩 중복 장착 금지)
  - **`RequiredSlotCount` 총합이 `MaxSlots`(6) 를 넘으면 초과분 제거**
    → 서버는 클라의 "해금 단계"를 모르지만 **구조적 상한(총 슬롯 6칸)** 은 알기 때문에 이 선까지 막는다.
    카테고리 분리가 사라져 **검사가 단일 합산 하나로 단순해졌다**(예전엔 카테고리별로 각각 3칸 검사).
  - 정화가 발생하면 `LogDR` 경고
- 서버가 검증할 수 **없는** 것: "그만한 재화를 실제로 벌었는가" / "그 슬롯을 실제로 해금했는가".
  재화 원장이 클라에만 있기 때문. 진짜 서버 권위가 필요하면 온라인 백엔드로 이전(§15).

---

## 8. C++ 구현 상세 (파일별)

### 8.1 ✅ `DRUpgradeTypes.h` (전면 개정)
- `EDRUpgradeStat`(ContainerHealth/MaxWater/MoveSpeed/DamageTaken/WaterGain/Skill*),
  `EDRUpgradeOp`, `EDRChipCategory{Stat, Ascension, Count}` ← **UI 분류 전용으로 격하**,
  `EDRUpgradeResult`(`WrongSlotType` 제거),
  `EDRRewardKind{StageFirstClear, Achievement, ClearCondition}`
- `FDRUpgradeModifier`(랭크 제거 · `Value` 단일값 · `GetDetailText()`),
  `FDRUpgradeChipDefinition`, `FDRResolvedStat`, `FDRSkillStatBlock`, `FDRUpgradeRuntime`
- `DRIsSkillStat()` 헬퍼로 스킬 단위 스탯 여부를 판정.

### 8.2 ✅ `DRProgressionTypes.h` (전면 개정)
- `FDRClassUpgradeState`(§4.2 — 통합 슬롯 해금 수 + 단일 칸 배열(6) + SpentCurrency)
- `FDRStageRewardReport`, `FDRRewardLineItem`, `FDRStageRewardResult`
- `FDRChipViewModel` — UI가 칩 1개를 그리는 데 필요한 모든 것을 한 구조체로 제공
  (이름 / 대상 스킬 / 효과 요약 / 필요 슬롯 수 / 장착 여부 / 해금 여부 / 상세 라인 / 단점 라인).
  → **UI가 카탈로그와 세이브를 직접 뒤지지 않게** 만드는 것이 목적. (`FDRUpgradeEntry` 는 삭제)

### 8.3 ✅ `DRChipCatalog.h/.cpp` (`DRUpgradeTree` 대체) / `DRProgressionConfig` / `DRSaveGame` / `DRGameInstance`
`UDRGameInstance` 공개 API:
```cpp
// 지갑
int32 GetCurrency() / GetLifetimeCurrency() const;
void  AddCurrency(int32);                          // 치트/디버그
FOnCurrencyChangedSignature OnCurrencyChanged;

// 시스템 해금
bool IsUpgradeSystemUnlocked() const;
void UnlockUpgradeSystem();                        // 스테이지1 최초 클리어 / 치트
FOnUpgradeSystemUnlockedSignature OnUpgradeSystemUnlocked;

// 슬롯 (카테고리 인자 전부 제거)
int32 GetMaxSlotCount() const;                                  // = ProgressionConfig->MaxSlots (6)
int32 GetUnlockedSlotCount(Class) const;
int32 GetNextSlotUnlockCost(Class) const;                       // -1 = 더 없음
EDRUpgradeResult CanUnlockSlot(Class) const;
EDRUpgradeResult UnlockSlot(Class);
EDRUpgradeResult RefundSlot(Class, int32& OutRefunded);

// 칩
TArray<FName> GetSlotAssignments(Class) const;                  // 6칸 점유 칩 (UI 좌측)
bool IsChipUnlocked(Class, FName ChipId) const;
bool IsChipEquipped(Class, FName ChipId) const;
int32 GetFreeSlotCount(Class) const;
EDRUpgradeResult CanEquipChip(Class, FName, int32& OutRequired, int32& OutFree) const;
EDRUpgradeResult EquipChip(Class, FName, int32 PreferredSlotIndex = -1);   // SlotIndex 는 0..5
EDRUpgradeResult UnequipChip(Class, FName);
EDRUpgradeResult UnequipSlot(Class, int32 SlotIndex);
// UI 우측 목록. Category 는 장착 제한이 아니라 ★필터★ 다 — 전체 목록이 필요하면 필터를 끈다
void GetChipViewModels(Class, TArray<FDRChipViewModel>& Out) const;                    // 전체
void GetChipViewModelsByCategory(Class, EDRChipCategory, TArray<FDRChipViewModel>& Out) const;
FOnUpgradesChangedSignature OnUpgradesChanged;

// 서버 보고 / 미리보기
TArray<FName> GetEquippedChips(Class) const;
void BuildUpgradeRuntime(Class, FDRUpgradeRuntime& Out) const;                     // 현재 상태
void BuildPreviewRuntime(Class, FName AddChip, FName RemoveChip, FDRUpgradeRuntime& Out) const;  // 미리보기

// 보상
FDRStageRewardResult ApplyStageReward(const FDRStageRewardReport&);
bool IsRewardClaimed(FName RewardId) const;
```

### 8.4 ✅ M2 — 기본 스탯 반영
**`DRPlayerState.h/.cpp`** — §7.1의 프로퍼티/함수 + `StageKillCount`(복제) + `AddStageKill()`.
`GetLifetimeReplicatedProps` 등록, `CopyProperties` 에 `EquippedChips` 복사.
`StageKillCount` 는 복사하지 않으므로 Seamless Travel 때 자동으로 0에서 다시 시작한다(별도 리셋 API 없음).

**`DRPlayerController.h/.cpp`**
```cpp
UFUNCTION(Server, Reliable) void ServerReportUpgradeLoadout(EPlayerCharacterClass ForClass, const TArray<FName>& Chips);
UFUNCTION(Client, Reliable) void Client_GrantStageReward(const FDRStageRewardReport& Report);
void ReportUpgradeLoadout();                       // 로컬 → 서버 보고 (PS/GI 확인 후)
void OpenUpgradeScreen(); void CloseUpgradeScreen();  bool IsUpgradeScreenOpen() const;
UFUNCTION(BlueprintImplementableEvent) void OnUpgradeScreenOpened();   // BP가 위젯 생성
UFUNCTION(BlueprintImplementableEvent) void OnUpgradeScreenClosed();   // BP가 위젯 제거
```
- 업그레이드 화면은 **로컬 UI**다. 열 때 입력 모드를 UI로 바꾸고 마우스를 띄운다(설정창과 동일 패턴).
- 닫을 때 `ReportUpgradeLoadout()` 을 호출해 변경된 장착을 서버에 반영한다.
- **`OpenUpgradeScreen()` 은 `IsInLobby()` 가 아니면 즉시 반환**한다.
  화면을 닫으면 재보고 → 서버가 스탯 GE 를 즉시 재적용하므로, 이 가드가 없으면
  "스테이지 진행 중 칩 교체 불가" 규칙이 실제로 깨진다(요구사항 13).
  변조 클라이언트까지 막지는 못하지만, §7.6대로 PvE 협동에서 영향은 자기 캐릭터 강화에 한정된다.

**`DRCharacter.h/.cpp`**
```cpp
void RefreshUpgradeEffects();     // 컨테이너 체력(모든 머신) + GE_Upgrade_Stats(서버)
UPROPERTY(EditDefaultsOnly, Category = "Upgrade") TSubclassOf<UGameplayEffect> UpgradeStatEffectClass;
UPROPERTY(EditDefaultsOnly, Category = "Upgrade") bool bPreserveVitalRatioOnUpgradeApply = true;
FActiveGameplayEffectHandle UpgradeStatEffectHandle;
```
- `InitAbilityActorInfo()` 의 `InitializeDefaultAttributes()` 직후에 호출한다.
- **반드시 그 뒤**여야 한다 — `InitializePlayerDefaultAttributes()` 가 활성 GE를 제거하고 BaseValue를 0으로 리셋하기 때문.
- 재적용 시 이전 핸들을 먼저 제거한다(ASC가 PlayerState에 있어 리스폰/클래스 변경 후에도 살아 있다).
- 칩이 하나도 없으면 **GE 자체를 얹지 않는다** → 개편 전과 완전히 동일한 수치(회귀 안전).
- **체력/물은 "만피로 채우기"가 아니라 적용 전후 비율을 유지**한다.
  스폰 시점엔 만피라 결과가 같고, 칩 목록이 늦게 도착해 재적용될 때 **공짜 회복이 생기지 않는다.**
  컨테이너 시스템에서는 비율 유지 = "남은 컨테이너 수 유지"라 의미도 정확하다.

**신규 GE 에셋** `GE_Upgrade_Stats` (Infinite):
| Modifier | Op | Magnitude (SetByCaller) |
|---|---|---|
| MaxHealth | Add | `Data.Upgrade.MaxHealth.Flat` |
| MaxHealth | Multiply | `Data.Upgrade.MaxHealth.Mult` (= 1 + Percent) |
| MaxWater | Add / Multiply | `Data.Upgrade.MaxWater.Flat` / `.Mult` |
| MoveSpeed | Add / Multiply | `Data.Upgrade.MoveSpeed.Flat` / `.Mult` |
> GAS 애그리게이터는 `((Base + ΣAdd) × ΠMultiply)` 순으로 평가하므로 §4.3의 합산 규칙과 일치한다.
> MaxHealth의 Flat 은 컨테이너 체력 증가분(`NumContainers × Δ`)이고, Mult 는 컨테이너 체력 Percent 성분이다.

**`DRPlayerAttributeSet.cpp`** — `HandleIncomingDamage()` 에서 `DamageTaken` 배율 적용
(부품 소지 1.5배 배율 **다음**, 컨테이너/오염 분기 **앞**).
**`DREnemy.cpp`** — `GrantWaterToPlayers()` 의 지급량에 대상 플레이어의 `WaterGain` 배율 적용.

### 8.5 ✅ M3 — 스킬 반영
**`DRGameplayAbility.h/.cpp`** — 모든 조회의 단일 진입점
```cpp
FGameplayTag GetUpgradeKeyTag() const;                       // GetAssetTags() 중 "Abilities" 접두 태그
const FDRUpgradeRuntime& GetUpgradeRuntime() const;          // 소유 PS 캐시 (없으면 static 빈 캐시)
const FDRUpgradeRuntime& GetUpgradeRuntimeFor(const FGameplayAbilityActorInfo*) const;  // ★
float GetUpgradedFloat(EDRUpgradeStat, float Base) const;
int32 GetUpgradedInt(EDRUpgradeStat, int32 Base, int32 MinValue = 1) const;
float GetEffectiveWaterCost() const;                         // 0 하한
float GetEffectiveWaterCostFor(const FGameplayAbilityActorInfo*) const;                // ★
UPROPERTY(EditDefaultsOnly, Category = "Cooldown") float CooldownDuration = 0.f;
virtual void ApplyCooldown(...) const override;              // SetByCaller(Data.Cooldown)
```
- ★ **ActorInfo 를 직접 받는 버전이 필요한 이유**: `CheckCost` / `ApplyCost` / `ApplyCooldown` 은
  CDO 에서 호출될 수 있어 `GetCurrentActorInfo()` 가 비어 있을 수 있다. 그때 빈 캐시로 폴백하면
  **CheckCost 는 기본 소모량, ApplyCost 는 업그레이드 소모량**으로 갈려 판정이 어긋난다.
  이 세 경로는 반드시 전달받은 `ActorInfo` 를 쓴다.
- `CheckCost()` / `ApplyCost()` 가 `WaterCost` 대신 `GetEffectiveWaterCostFor(ActorInfo)` 를 쓴다.
- **쿨다운**: `CooldownDuration > 0` 일 때만 `Data.Cooldown` 을 주입한다.
  0이면 기존 GE의 고정 Duration이 그대로 쓰인다 → **기존 어빌리티 전부 무변경 동작**(하위 호환).
  쿨다운 GE의 Duration이 SetByCaller인데 `CooldownDuration` 이 0이면 조용히 쿨다운 0이 되므로 **경고 로그**를 남긴다.

**`DRDamageGameplayAbility.h/.cpp`**
```cpp
UFUNCTION(BlueprintPure) float GetUpgradedDamage() const
{ return GetUpgradedFloat(EDRUpgradeStat::SkillDamage, Damage.GetValueAtLevel(GetAbilityLevel())); }
```
- `CauseDamage()` / `MakeDamageEffectParamsFromClassDefaults()` / `GetDamageAtLevel()` 세 곳 모두 이 함수로 교체.
  **반드시 한 함수로 모아** 누락을 막는다.

**`DRProjectileSpell.h/.cpp`**
```cpp
UFUNCTION(BlueprintPure) int32 GetEffectiveNumProjectiles() const
{ return GetUpgradedInt(EDRUpgradeStat::SkillProjectileCount, NumProjectiles, 1); }
```
- `UDRFireBolt::SpawnProjectiles()` 의 `NumProjectiles` → `GetEffectiveNumProjectiles()`.
- **`UDRVacuumAirShot::Stages[].Damage`** 는 CDO 배열에 수치가 박혀 있어 `Damage`(FScalableFloat) 경로를
  타지 않는다 → 투사체에 주입하는 지점에서 `GetUpgradedFloat(SkillDamage, StageData.Damage)` 로 직접 보정한다.
  (안 하면 청소기 에어샷을 대상으로 한 피해량 칩이 조용히 무효가 된다.)

### 8.6 ✅ M4 — 처치 카운트
**`DREnemyAttributeSet.cpp`** — `bFatal` 분기에서 처치 크레딧 부여
```cpp
if (bFatal)
{
    if (ADRPlayerState* KillerPS = Props.SourceController ? Props.SourceController->GetPlayerState<ADRPlayerState>() : nullptr)
    {
        KillerPS->AddStageKill();   // 서버 전용
    }
    // 기존 Die() 처리 ...
}
```

### 8.7 ✅ M4 — 업적 판정 + 보상 파이프라인
**`DRStageGameMode.h/.cpp`**
```cpp
UPROPERTY(EditDefaultsOnly, Category = "Stage|Config") FName StageId = TEXT("Stage1");

UFUNCTION(BlueprintCallable, Category = "Stage|Achievement")
void GrantStageAchievement(FName AchievementId, ADRPlayerController* PC = nullptr);   // PC==null → 전원

private:
TMap<TWeakObjectPtr<ADRPlayerController>, TArray<FName>> PendingAchievements;   // 종료 시 일괄 전송
TArray<FName> GlobalPendingAchievements;                                        // 전원 대상
```
- `NotifyAllPlayersGameEnd(bIsGameClear)` 의 기존 단일 순회를 확장해 플레이어별 리포트를 만들고
  **결과 UI 표시보다 먼저** `Client_GrantStageReward(Report)` 를 보낸다.
- 업적은 **게임 클리어 시에만** 전송한다(클리어 조건 = 클리어 전제).
- ⚠️ **판정 로직은 아직 없다.** `GrantStageAchievement()` 를 호출하는 곳이 한 군데도 없으므로
  현재는 "스테이지 최초 클리어" 재화만 지급된다. 업적/조건이 확정되면 각 판정 지점에서 이 함수만 호출하면 된다.
  판정 자료는 이미 다 있다 — `PS->GetStageKillCount()`, 사이트 체력, 사망 이벤트(`OnDeathDelegate`),
  페이즈 전환 시각 등. (§16 잔여 항목)

### 8.8 ✅ M5 코드 훅 — 로비 업그레이드 장치
**신규 `Actor/DRUpgradeStation.h/.cpp`** (`ADRStageSelectActor` 패턴 차용, 단 **로컬 플레이어 기준**)
- 구성: `StaticMeshComponent`(장치) + `BoxComponent`(상호작용 범위) + `WidgetComponent`(프롬프트).
- 오버랩은 **각 머신 로컬**로 판정한다(로컬 폰 소유 컨트롤러만). 서버 권한 검사를 하지 않는다
  — 업그레이드 화면은 순수 로컬 UI라 RPC가 필요 없기 때문이다.
- 로컬 플레이어 진입 시 프롬프트 표시 + `PC->OnInteractPressed` 바인딩.
- 상호작용 시:
  - `GI->IsUpgradeSystemUnlocked() == false` → `OnInteractBlocked()` (BP: "스테이지1을 클리어하세요" 표기)
  - 해금됨 → `PC->OpenUpgradeScreen()`
- `EndPlay` / 오버랩 종료에서 델리게이트를 해제한다(댕글링 방지).

### 8.9 ✅ 게임플레이 태그 추가 (`DRGameplayTags.h/.cpp`)
```
Data.Upgrade.MaxHealth.Flat / .Mult
Data.Upgrade.MaxWater.Flat  / .Mult
Data.Upgrade.MoveSpeed.Flat / .Mult
Data.Cooldown
```

---

## 9. 데이터 흐름 (시퀀스 정리)

### 9.1 로비: 슬롯 해금 + 칩 장착
```
Client 장치 상호작용 → (해금 확인) → PC->OpenUpgradeScreen() → BP 위젯 생성
Client 슬롯 해금 클릭 → GI->CanUnlockSlot(Class) → GI->UnlockSlot(Class)
    지갑 차감 → SpentCurrency 가산 → UnlockedSlots+1 (0..6) → SaveProgress()
    OnCurrencyChanged / OnUpgradesChanged 발화 → UI 갱신 (해금된 칩 목록도 늘어남)
Client 칩 드래그앤드롭 → GI->CanEquipChip(...) 로 하이라이트/경고 표시
                      → GI->EquipChip(Class, ChipId, DropSlotIndex)
    빈 칸 배정 → SaveProgress() → OnUpgradesChanged → UI 갱신
Client 화면 닫기 → PC->CloseUpgradeScreen() → ReportUpgradeLoadout()
Server → SanitizeLoadout → PS->SetEquippedChips()  (복제)
모든 머신 → OnRep/Set 에서 RebuildUpgradeRuntime() → 폰에 RefreshUpgradeEffects()
```

### 9.2 스테이지 스폰: 칩 효과 적용
```
Server GameMode 스폰 → ADRCharacter::InitAbilityActorInfo()
   PlayerCharacterClass = PS->SelectedPlayerClass
   InitializeDefaultAttributes()      // 기본 GE (기존 로직 무변경)
   RefreshUpgradeEffects()            // 컨테이너 체력 + GE_Upgrade_Stats  ← 신규
플레이 중 어빌리티 사용 (서버 + 예측 클라 동일 경로)
   GetEffectiveWaterCost() / GetUpgradedDamage() / GetEffectiveNumProjectiles() / ApplyCooldown()
   → PS->CachedUpgradeRuntime 조회
피격 시 → HandleIncomingDamage 에서 DamageTaken 배율
적 사망 시 → GrantWaterToPlayers 에서 대상별 WaterGain 배율
```

### 9.3 스테이지 종료: 재화 지급
```
Server 적 사망 시마다 → DREnemyAttributeSet(bFatal) → KillerPS->AddStageKill()
Server 업적 달성 시 → GameMode::GrantStageAchievement(Id, PC)  → PendingAchievements
Server TriggerGameClear/Over → NotifyAllPlayersGameEnd()
   각 PC: Client_GrantStageReward(Report)   → 그 다음 결과 UI
Client → GI->ApplyStageReward(Report)
   원장 확인 → 최초 클리어 + 업적/조건 재화 합산 → 지갑 가산 → (필요 시 시스템 해금) → SaveProgress()
Client → 결과창에 FDRStageRewardResult.Lines 표시
Server → ReturnToLobby (ServerTravel)
로비 진입 → 갱신된 지갑으로 슬롯 해금 가능
```

---

## 10. UI 작업 (M5 — 아직 미구현, 코드 훅은 완료)

### 10.1 로비 업그레이드 화면 (확정 시안 기준)

> **확정 시안**: `최종모습.jpg`(화면 전체) + `최종칩.jpg`(칩 카드 3상태).
> 픽셀 스펙·컬러·에셋 매핑은 **§18**, 위젯 트리는 **§19**, 드래그앤드롭은 **§20**,
> 필요한 코드 보강은 **§21**, 에디터 제작 순서는 **§22** 에 있다. 여기서는 화면의 규칙만 정의한다.

- 진입: 로비 `ADRUpgradeStation` 상호작용 → `PC->OpenUpgradeScreen()` → `OnUpgradeScreenOpened()` BP 이벤트.
- **화면은 3영역 + 전면 오버레이 1장**이다.
  - 상단 바: 타이틀 `UPGRADE SYSTEM`, 우측 상단 **재화 아이콘 + 보유량**.
  - 좌측 패널 `CHIP SLOTS`: **구분 없는 통합 6칸**을 **3열 × 2행** 그리드로 표시.
  - 우측 패널 `CHIPS`: **분류 없는 단일 칩 목록**(**4열 그리드**) + 우측 세로 스크롤바. **탭 없음.**
  - 하단 우측: 키 안내 `ESC BACK` / `M EXIT` (설정 화면의 `WBP_KeyHintBar` 재사용, `A`/`R` 은 숨김).
  - 최상단: 스캔라인 화면 효과 1장(§18.6).

**좌측 — 슬롯 6칸**
- 데이터 원본은 `GI->GetSlotViewModels(Class, Out)` **하나**다(§21.2). UI가 카탈로그/세이브를 직접 뒤지지 않는다.
- 칸 상태 3종 = 배경 텍스처 3종 (`T_UI_Slot_Locked` / `T_UI_Slot_Empty` / `T_UI_Slot_Filled`).
  - **Locked**: 자물쇠 + **그 칸의 해금 비용**(`재화 아이콘 + 숫자`)을 표시한다.
    시안처럼 **모든 잠긴 칸에 비용을 그린다.** 단 실제로 누를 수 있는 칸은
    `bIsNextUnlockable == true` 인 **맨 앞 잠긴 칸 하나뿐**이고, 나머지는 흐리게(0.45) + 클릭 불가.
    (순차 해금 규칙을 유지하면서 "다음에 얼마가 드는지"를 미리 보여주기 위한 절충.)
  - **Empty**: 중앙 `+`(텍스처에 포함). 드롭 타겟.
  - **Occupied**: 칩 아이콘 + 짧은 라벨(`ATK +5`) + 단점 경고 뱃지. 클릭/드래그로 해제·이동.
- **한 칸 = 한 칩**이다(§6.4 콘텐츠 결정). 여러 칸을 묶어 보이게 하는 표현은 만들지 않는다.
- 칸 라벨은 `01`~`06` 단일 번호. **`STAT` / `ASCENSION` 구역 헤더는 넣지 않는다** — 구역을 그리면
  장착 제한이 있는 것처럼 오해를 준다(3차 개정의 핵심 규칙).

**우측 — 칩 목록**
- ★**탭이 없다. 이 로봇의 칩 전체가 한 목록에 나온다.**★
  - `STATS` / `ASCENSION` 로 나누던 필터 탭은 **폐기**했다. 슬롯에 종류 구분이 없는데
    목록만 종류로 갈라 놓으면 **장착 제한이 있는 것처럼 오해**를 준다(3차 개정의 핵심 규칙).
    좌측 6칸 어디에나 드롭된다는 사실은 §20.3 의 하이라이트 동작으로 전달한다.
  - 패널 상단 노치의 `CHIPS` 는 **버튼이 아니라 라벨**이다(§18.3).
- 목록은 `GI->GetChipViewModels(Class, Out)`(§21.2) — **분류 인자가 없는 쪽**을 쓴다.
  `GetChipViewModelsByCategory()` 는 남아 있지만 **UI 가 호출하지 않는다.**
- 정렬은 `UDRChipCatalog::GetChipsForClass()` 가 이미 해 주는
  **`RequiredSlotTier` 오름차순 → `ChipId`** 가 기본이고, 화면이 그 앞에
  **`bUnlocked` 내림차순 → `bEquipped` 오름차순**을 덧붙인다(STEP 9-9).
  → 결과: **지금 쓸 수 있는 칩이 맨 위**, 장착 완료가 그다음, 미해금이 맨 아래.
  탭이 사라져 목록이 길어진 만큼 이 정렬이 유일한 정리 수단이다.
- 카드에 표시할 항목:
  | 항목 | 뷰모델 필드 | 시안 위치 |
  |---|---|---|
  | 아이콘 | `Icon` (미해금이면 `T_UI_Chip_Lock`) | 카드 상단 아이콘 박스 |
  | 칩 이름 | `DisplayName` | 아이콘 아래 1행 |
  | 효과 요약 | `EffectSummary` | 이름 아래 큰 글씨 (`+3` 등) |
  | 필요한 슬롯 수 | `RequiredSlotCount` | 하단 `SLOT n` 뱃지 (`T_UI_Chip_SlotBadge`) |
  | 적용 대상 스킬 | `TargetSkillName` | 툴팁 1행 (태그 없으면 "전체") |
  | 장착 여부 | `bEquipped` | 카드 우상단 `EQUIPPED` 리본 + 탈채도 |
  | 해금 여부 | `bUnlocked` (+ `RequiredSlotTier`) | 자물쇠 + `해금 조건: 슬롯 n칸` 문구 |
  | 장착 불가 사유 | `EquipBlockReason` | 툴팁 마지막 줄 (빨강) |
  | 상세 수치 | `DetailLines` | 툴팁 (세부 정보 토글 ON일 때만) |
  | 돌파 장점/단점 | `BenefitLines` / `DrawbackLines` | 툴팁에 **토글과 무관하게 항상** |
- 카드 상태 4종(§18.4): `Normal` / `Hover` / `Disabled(장착됨·장착불가)` / `Locked(미해금)`.

**공통 규칙**
- **세부 정보 토글**: 기본 OFF(요약만). ON이면 툴팁에 `DetailLines` 노출. 토글 상태는 위젯 로컬(저장 불필요).
- **모든 판정은 `UDRGameInstance` 결과 코드를 그대로 쓴다.** UI에 판정 로직을 복제하지 않는다.
  실패 사유 → 문구 변환은 `UDRUpgradeUILibrary::GetUpgradeResultText()` 한 곳(§21.5).
- **현지화**: 모든 문구 LOCTEXT. 칩 이름/요약/설명은 `FDRUpgradeChipDefinition` 의 `FText` 필드(에셋에서 번역).
  (Plan4의 현지화 규칙을 따른다. 위젯은 `UDRUserWidget` 을 상속해 `OnLanguageChanged` 로 재빌드한다.)
- 미리보기(선택): `GI->GetStatPreview(Class, AddChip, RemoveChip, Out)`(§21.4)로 "현재 → 장착 후" 델타 표시.

### 10.2 인게임 HUD (선택)
- 이번 스테이지 처치 수는 `PS->GetStageKillCount()`(복제) 바인딩.

### 10.3 스테이지 종료 결과 화면
- 기존 `Client_ShowGameOverUI` / `Client_ShowGameClearUI` 와 연동.
- `FDRStageRewardResult::Lines` 를 그대로 롤업 연출:
  ```
  스테이지 1 최초 클리어      +1200
  업적: 무사고 클리어          +400
  조건: 사이트 무손상          +500
  ─────────────────────────
  획득 재화                   +2100
  보유 재화        1,250 → 3,350
  ```
- 이미 받은 항목은 리포트에 담기더라도 `Lines` 에 들어가지 않는다(중복 표기 방지).

---

## 11. 콘텐츠/에셋 작업 (코드 외 필수 작업)

1. **`DA_ChipCatalog`** 생성 — §6.2 기준으로 3로봇 분량 작성.
   - 작성 후 `ValidateCatalog()` 1회 실행.
   - `ChipId` 명명 규칙: `{로봇약어}.{Stat|Asc}.{이름}` (예: `GR.Stat.Container`, `VM.Asc.Overclock`)
   - `RequiredSlotTier` 는 **1..6 스케일**로 입력한다(3차 개정). 카테고리별 1..3이 아니다.
2. **`DA_ProgressionConfig`** 생성 — `ChipCatalog` 연결, `MaxSlots = 6` + `SlotCosts`(6개)/스테이지 보상/업적 입력.
   - **`ValidateCurrencyBudget()` 1회 실행 필수** (§5.4). 실패 시 슬롯 비용을 낮추거나 업적을 추가한다.
3. **GameInstance BP** 에 `ProgressionConfig` 지정. ← 빼먹으면 재화가 0으로만 지급된다.
4. **`GE_Upgrade_Stats`** 생성 (§8.4 표). SetByCaller 태그는 이미 네이티브 등록돼 있다.
5. **캐릭터 BP** 에 `UpgradeStatEffectClass = GE_Upgrade_Stats` 지정 (3로봇 전부).
6. **쿨다운 칩을 쓸 스킬만** 쿨다운 GE의 Duration을 SetByCaller(`Data.Cooldown`)로 바꾸고,
   원래 고정값을 어빌리티 BP의 `CooldownDuration` 프로퍼티로 옮긴다.
   > ⚠️ GE만 바꾸고 `CooldownDuration` 을 안 채우면 쿨다운이 0이 된다(경고 로그로 검출됨).
   > 반대로 둘 다 손대지 않으면 쿨다운 칩만 무효가 되고 나머지는 정상 동작한다.
7. **로비 레벨**에 `BP_DRUpgradeStation` 배치 (`ADRUpgradeStation` 상속) + 프롬프트 위젯 지정.
8. **칩 아이콘 텍스처** — 칩 카드 안의 아이콘 박스(53×50)에 들어가는 스탯별 픽토그램
   (검=피해, 방패=방어, 화살표=속도 등). 카드 프레임 자체는 `T_UI_Upg_Chip_Base` 가 담당하므로
   **아이콘만** 만들면 된다. 미지정 시 `DA_UpgradeUIStyle::DefaultChipIcon` 으로 폴백한다.
9. **업그레이드 화면 UI 에셋 일습** — §18.1 텍스처 13장 + §22 의 위젯/데이터 에셋. (M5b)

---

## 12. 엣지 케이스 & 결정 사항

1. **해금 전 상호작용**: 장치가 `SystemLocked` 상태를 BP 이벤트로 알린다. 화면 자체가 열리지 않는다.
2. **게임오버**: 재화 0. 업적도 전송하지 않는다(클리어 조건이 전제).
3. **중도 접속/이탈**: Join-in-progress 차단됨. 종료 시점에 존재하는 PC에게만 지급.
4. **세이브 손상/마이그레이션**: `SaveVersion` 불일치 시 안전 초기화. 로드 실패 시 새 세이브 생성.
   3차 개정으로 `FDRClassUpgradeState` 의 형태가 바뀌므로 **`CurrentSaveVersion` 을 4로 올린다**(§17).
5. **카탈로그 개편**: §6.6 정화/환급 로직이 처리. `ChipId` 변경은 사실상 삭제 + 신규 추가다.
6. **클래스 enum 추가 시**: 세이브 맵 키는 `EPlayerCharacterClass::Count` 기반으로 자동 확장.
   단 **총 필요 재화가 늘어나므로 `ValidateCurrencyBudget()` 를 다시 통과시켜야 한다.**
7. **스테이지 도중 클래스 변경 금지 전제**: 허용하려면 재보고 + GE 재적용 경로가 필요하다
   (이미 `RefreshUpgradeEffects()` 가 멱등이라 대응 자체는 가능).
8. **단점으로 스탯이 0 이하가 되는 경우**: Percent 합이 −100% 이하면 스탯이 0/음수가 된다.
   `MoveSpeed`/`MaxHealth` 는 `PreAttributeChange` 클램프가 있고,
   **쿨다운 0.05초 하한 / 투사체 1 하한 / 물 소모 0 하한 / 컨테이너 체력 1 하한**을 조회 지점에서 직접 건다.
9. **`DamageTaken` 이 음수 배율이 되는 경우**: 0 하한을 걸어 회복으로 뒤집히지 않게 한다.
10. **처치 크레딧 없는 사망**(환경 피해 등 `SourceController == nullptr`): 아무에게도 크레딧이 안 간다(의도).
11. **튜토리얼/메인메뉴**: 보상 흐름에서 제외(별도 게임모드라 자연히 제외됨).
12. **슬롯 환불 후 잠긴 칩**: 자동 해제된다(§6.5). 플레이어에게 알릴 필요가 있으면 UI에서 토스트.
13. **돌파 칩 6개 몰빵 빌드**(3차 개정으로 새로 생긴 경우): 설계상 **허용한다.**
    단점을 6중으로 짊어지는 하이리스크 빌드이므로 그 자체로 자정 작용이 있다.
    밸런스 패스에서 문제가 되면 그때 `UDRProgressionConfig::MaxAscensionChips`(0 = 무제한) 같은
    **상한 하나만** 추가한다 — 슬롯 카테고리를 되살리지 않는다.
14. **`RequiredSlotCount >= 2` 칩**: 백엔드는 지원하지만 **콘텐츠로 만들지 않는다**(§6.4 결정).
    UI도 다중 칸 표시를 만들지 않는다. "6칸 전부를 먹는 초월 칩"은 §15 확장 항목으로 남긴다.

---

## 13. 구현 단계 (마일스톤)

- **✅ M1. 데이터/저장/규칙 토대** — `DRUpgradeTypes` / `DRProgressionTypes` / `DRChipCatalog` /
  `DRProgressionConfig` / `DRSaveGame` v3 / `DRGameInstance` API.
- **✅ M2. 기본 스탯 반영** — PlayerState 운반 + 보고 RPC + `RefreshUpgradeEffects()` + DamageTaken/WaterGain.
- **✅ M3. 스킬 반영** — 조회 헬퍼 + 피해량/물 소모량/쿨다운/투사체 수.
- **✅ M4. 재화 파이프라인** — 처치 카운트 + 업적 API + 리포트 전송 + `ApplyStageReward`.
- **⬜ M5. UI** — 업그레이드 화면(드래그앤드롭), 결과창 재화 연출.
- **⬜ M6. 콘텐츠 + 튜닝** — §11 에셋 작성, 예산 검증, 밸런스 패스.

---

## 14. 테스트 계획

### 14.1 단일/로컬
- **해금 게이트**: 세이브 초기 상태에서 장치 상호작용 → 화면이 열리지 않고 잠김 안내가 뜨는지.
  스테이지1 클리어 후 → 열리는지.
- **세이브 라운드트립**: 재화 획득 → 슬롯 해금 → 칩 장착 → 게임 재시작 → 지갑/해금 수/장착 칸 유지.
- **1회성 원장**: 같은 스테이지를 두 번 클리어 → 두 번째는 재화 0. 같은 업적 재달성 → 0.
- **슬롯/칩 규칙**: `EDRUpgradeResult` 각 실패 사유가 의도대로 반환되는지 개별 확인.
  - **스탯 칩을 아무 칸에나 / 돌파 칩을 아무 칸에나 → 전부 `Success`** (3차 개정 핵심 검증)
  - 6칸을 전부 돌파 칩으로 채우기 → 성공하고, 단점이 6중으로 합산되는지
  - 6칸을 전부 채운 뒤 한 장 더 → `NotEnoughSlots` (필요 1 / 남은 0)
  - 같은 칩 두 번 → `AlreadyEquipped`
  - 해금 단계 미달 칩 → `ChipLocked` (통합 `UnlockedSlots` 와 비교되는지)
  - 잠긴 칸(index ≥ UnlockedSlots)에 드롭 → 실패
- **환불**: 점유 칸 환불 시도 → `SlotOccupied`. 정상 환불 후 지갑 = 이전 + `floor(비용 × RefundRatio)`.
  **반복 해금/환불로 재화가 증식하지 않는지** (RefundRatio 0.5로 두고 확인).
- **카탈로그 개편 내성**: 장착 중인 칩을 카탈로그에서 삭제 → 재시작 → 정화 로그 + 재화 증발 없음.
- **예산 검증**: `ValidateCurrencyBudget()` 가 실제 데이터에서 통과하는지.

### 14.2 멀티플레이어 (PIE 2+ 인스턴스, 리슨 서버)
- 각 클라이언트가 자기 로컬 세이브에만 기록(타 클라 세이브 미오염).
- **호스트 플레이어도 정확히 1회만** 지급/저장.
- 서로 다른 칩을 장착한 두 플레이어가 각자 수치대로 스폰/전투.
- **예측 일관성**: 클라에서 스킬 사용 시 물 소모량/쿨다운/투사체 수가 서버 판정과 어긋나 롤백이 없는지.
- **보고 경합**: 로비에서 클래스를 여러 번 바꾼 뒤 스테이지 진입 → 최종 선택 클래스의 칩만 적용되는지.
- 위조 목록(없는 Id, `RequiredSlotCount` 합 6칸 초과, 타 클래스 칩)을 신고했을 때
  서버가 정화하고 경고 로그를 남기는지.
- 처치 크레딧이 막타 친 플레이어에게만 1회 가는지.

### 14.3 회귀
- 칩을 하나도 장착하지 않은 상태에서 모든 수치가 **개편 전과 완전히 동일**한지
  (`FDRUpgradeRuntime` 이 비면 항등이고 `GE_Upgrade_Stats` 를 아예 안 얹는다).
- 컨테이너 UI: 컨테이너 체력 칩 장착 시 칸 수는 그대로(4)이고 칸당 용량만 커지는지.
  **오염 진입 → 해제 후에도 업그레이드분이 유지되는지** (§2.2 회귀 항목).
- 기존 튜토리얼 완료 플래그가 v3 → v4 마이그레이션 후에도 유지되는지.
- v3 세이브(카테고리 분리)로 실행 시 진행도가 안전 초기화되고 **크래시/고아 데이터가 없는지**.
- `GE_Upgrade_Stats` 가 적/클렌저 사이트 등 비플레이어 ASC에 영향을 주지 않는지.
- 쿨다운 GE를 개조하지 않은 스킬의 쿨다운이 그대로인지.

---

## 15. 향후 확장 (Out of Scope, 메모)

- 로봇별 전용 재화(듀얼 커런시)로 성장 분리.
- 서버 권위 진행도(온라인 백엔드/스팀 클라우드)로 치트 방지.
- 칩 프리셋 저장/전환(빌드 슬롯).
- 칩 획득 개념 추가(현재는 슬롯 해금 단계로만 열림 — 드롭/제작으로 확장 가능).
- 업적 목록 전용 화면(달성/미달성 + 남은 재화 표시).
- **6칸 전부를 점유하는 "초월 칩"**(`RequiredSlotCount = 6`) — 통합 슬롯이라 구조적으로 가능해진 콘텐츠.
- 특정 분류의 장착 상한(`MaxAscensionChips` 등) — 밸런스가 무너질 때만 꺼내는 카드 (§12.13).

---

## 16. 변경 파일 요약 체크리스트

**신규**
- [x] `Public/Game/DRUpgradeTypes.h` — 칩/슬롯 타입 전면 재작성
- [x] `Public/Game/DRChipCatalog.h` + `Private/Game/DRChipCatalog.cpp` — 칩 카탈로그 (구 `DRUpgradeTree` 대체)
- [x] `Public/Actor/DRUpgradeStation.h` + `Private/Actor/DRUpgradeStation.cpp` — 로비 업그레이드 장치
- [ ] 콘텐츠: `DA_ChipCatalog`, `DA_ProgressionConfig`, `GE_Upgrade_Stats`, `BP_DRUpgradeStation`, 칩 아이콘

**수정**
- [x] `DRProgressionTypes.h` — 슬롯 상태 / 보상 리포트 / 칩 뷰모델
- [x] `DRProgressionConfig.h/.cpp` — 슬롯 비용 + 1회성 보상 + 예산 검증
- [x] `DRSaveGame.h` — 재화/해금/원장/슬롯, SaveVersion 3
- [x] `DRGameInstance.h/.cpp` — 지갑/슬롯/칩 API, 보상 반영, 마이그레이션, 정화
- [x] `DRPlayerState.h/.cpp` — `EquippedChips`(복제) + 런타임 캐시 + `StageKillCount`
- [x] `DRPlayerController.h/.cpp` — 보고 RPC, 보상 RPC, 업그레이드 화면 열기/닫기
- [x] `DRCharacter.h/.cpp` — `RefreshUpgradeEffects()` (컨테이너 체력 + 스탯 GE)
- [x] `DRPlayerAttributeSet.cpp` — `DamageTaken` 배율
- [x] `DREnemy.cpp` — `WaterGain` 배율
- [x] `DRGameplayAbility.h/.cpp` — 조회 헬퍼, 물 소모량, 쿨다운 SetByCaller
- [x] `DRDamageGameplayAbility.h/.cpp` — `GetUpgradedDamage()` 로 3개 호출부 통합
- [x] `DRProjectileSpell.h/.cpp` + `DRFireBolt.cpp` — 투사체 수 보정
- [x] `DREnemyAttributeSet.cpp` — 처치 크레딧
- [x] `DRStageGameMode.h/.cpp` — `StageId`, 업적 API, 보상 리포트 전송
- [x] `DRGameplayTags.h/.cpp` — `Data.Upgrade.*`, `Data.Cooldown`
- [ ] UI 위젯들 — 업그레이드 화면, 결과창 재화 연출 (M5)

**M5a — UI 지원 코드 (§21)**
- [ ] `DRUpgradeTypes.h` — `EDRSlotState` 추가, `EDRUpgradeResult::PreviousSlotLocked` 추가
- [ ] `DRProgressionTypes.h` — `FDRSlotViewModel`, `FDRStatPreviewLine`, `UDRChipViewModelObject`
- [ ] `DRGameInstance.h/.cpp` — `GetSlotViewModels` / `GetSlotUnlockCost` / `GetChipViewModelsByCategory` /
      `CanEquipChipAtSlot` / `SwapOrEquipChip` / `MoveChip` / `PreviewSlotAssignment` / `GetStatPreview` /
      `ComputeAssignment()` 추출 / `UpgradeUIStyle` 프로퍼티
- [ ] 신규 `Public/UI/DRUpgradeUILibrary.h/.cpp` — 결과 코드/스탯명/재화 포맷/짧은 라벨
- [ ] 신규 `Public/UI/DRUpgradeUIStyle.h` — 컬러/아이콘 SSOT DataAsset
- [ ] 신규 `Public/UI/Widget/DRChipDragDropOperation.h`
- [ ] 신규 `Public/UI/Widget/DRUpgradeScreenWidget.h/.cpp`
- [ ] 신규 `Public/UI/Widget/DRUpgradeSlotWidget.h/.cpp`
- [ ] 신규 `Public/UI/Widget/DRUpgradeChipWidget.h/.cpp`
- [ ] `DRPlayerController.h` — `UpgradeScreenWidgetClass` / `UpgradeScreenWidget` 프로퍼티

**M5b — UI 콘텐츠 (§22)**
- [ ] 텍스처 13장 임포트 (`Content/DaeRuneAssets/UI/Upgrade/`, `T_UI_Upg_*`)
- [ ] `DA_UpgradeUIStyle`
- [ ] (선택) `M_UI_Desat` / `M_UI_SoftLight` + 인스턴스
- [ ] `WBP_UpgradeScreen` / `WBP_UpgradeSlot` / `WBP_UpgradeChip`
- [ ] `WBP_ChipDragVisual` / `WBP_UpgradeTooltip` / `WBP_UpgradeToast`
- [ ] `BP_DRPlayerController` 의 `OnUpgradeScreenOpened/Closed` 그래프
- [ ] `BP_DRUpgradeStation` 로비 배치

- [x] `DRVacuumAirShot.cpp` — 단계별 피해량에 `SkillDamage` 적용

**3차 개정(통합 슬롯) 재수정** — 상세는 §17. **전부 완료 · 빌드 통과**
- [x] `DRUpgradeTypes.h` — `EDRUpgradeResult::WrongSlotType` 제거, `EDRChipCategory` 주석을 UI 전용으로
- [x] `DRProgressionTypes.h` — `FDRClassUpgradeState` 통합(§4.2), 카테고리 헬퍼 삭제
- [x] `DRProgressionConfig.h/.cpp` — `MaxSlots`/`SlotCosts` 단일화, 예산 공식 수정
- [x] `DRSaveGame.h` — `CurrentSaveVersion = 4`
- [x] `DRGameInstance.h/.cpp` — 슬롯/칩 API에서 카테고리 인자 제거, 정화 로직 단순화
- [x] `DRChipCatalog.h/.cpp` — `SanitizeLoadout` 단일 합산 검사, `ValidateCatalog` 범위 검사 1..6
- [x] `DRPlayerController.cpp` — 카테고리 참조 1곳 정리
- [ ] 콘텐츠: `DA_ChipCatalog` 의 `RequiredSlotTier` 1..6 재조정, `DA_ProgressionConfig` 슬롯 비용 재입력
      → **아직 두 에셋이 존재하지 않는다.** M6 에서 처음 만들 때 새 필드로 바로 넣으면 되므로 재입력 작업은 없다.

**잔여 확인 항목**
- [ ] **업적/클리어 조건 판정 로직** — `GrantStageAchievement()` 호출부. 조건이 확정돼야 작성 가능
      (§5.2 표는 제안일 뿐이다). 이게 없으면 재화는 "스테이지 최초 클리어"분만 들어온다.
- [ ] `UDRFireBolt::MaxNumProjectiles` — 아무도 읽지 않는 죽은 프로퍼티. 제거 권장
- [ ] `UDRSeedCannon` — 투사체 수 칩 대상으로 만들려면 BP 호출부를 루프로 바꿔야 함
- [ ] 자판기(`Abilities.VendingMachine.*`) 스킬들이 `UDRDamageGameplayAbility` 경로를 타는지 확인
      (타지 않는 커스텀 피해 계산이 있으면 에어샷과 같은 보정이 필요)

---

## 17. M1' — 통합 슬롯 전환 (3차 개정) ✅ 완료

> **상태: 2026-08-15 완료.** 아래 1~7 순서대로 적용했고 `DaeRuneEditor Win64 Development` 빌드가 통과했다.
> **M2~M4(스탯/스킬/재화)는 손대지 않았다** — 그쪽은 `TArray<FName> EquippedChips` 만 소비하기 때문이다.
>
> **8(콘텐츠)만 남았다.** 대상 에셋이 아직 없어 M1' 시점에는 "해당 사항 없음"이었지만,
> 에셋을 새로 만들 때 `RequiredSlotTier` 를 **1..6 스케일**로 넣어야 한다는 지시는 그대로 유효하다.
> → **§23 Phase A-2 (`DA_ChipCatalog` 작성)에 흡수됐다.** 그쪽을 따르면 된다.

### 17.1 작업 순서
1. **`DRProgressionTypes.h` `FDRClassUpgradeState`** — 여기서 시작한다. 이 구조체가 모든 호출부의 근원이다.
   - `UnlockedStatSlots` + `UnlockedAscensionSlots` → `int32 UnlockedSlots`
   - `StatSlotChips` + `AscensionSlotChips` → `TArray<FName> SlotChips`
   - `GetSlotChips(EDRChipCategory)` / `GetUnlockedSlotCount(EDRChipCategory)` 오버로드
     → **인자 없는 단일 접근자**로 교체. 카테고리 분기 헬퍼는 전부 삭제.
2. **`DRProgressionConfig.h/.cpp`** — `MaxStatSlots`/`MaxAscensionSlots` → `MaxSlots = 6`,
   `StatSlotCosts`/`AscensionSlotCosts` → `SlotCosts`, `GetMaxSlotCount(Category)` → `GetMaxSlotCount()`.
   `ValidateCurrencyBudget()` 의 필요액 공식을 §5.4로 교체.
3. **`DRUpgradeTypes.h`** — `EDRUpgradeResult::WrongSlotType` 삭제(`DRUpgradeTypes.h:84`),
   `EDRChipCategory` 에 "UI 분류 전용" 주석 명시.
4. **`DRChipCatalog.h/.cpp`**
   - `SanitizeLoadout(..., int32 MaxStatSlots, int32 MaxAscensionSlots)` → `(..., int32 MaxSlots)`.
     내부의 카테고리별 두 누산기를 **단일 합산**으로 교체.
   - `ValidateCatalog(int32 MaxStatSlots, int32 MaxAscensionSlots, ...)` → `(int32 MaxSlots, ...)`.
     `RequiredSlotCount` / `RequiredSlotTier` 범위 검사를 `1..MaxSlots` 로.
5. **`DRGameInstance.h/.cpp`** — §8.3의 새 시그니처로 슬롯/칩 API에서 `EDRChipCategory` 인자 제거.
   - `IsChipUnlocked()` 의 `GetUnlockedSlotCount(Class, Chip->Category)` → `GetUnlockedSlotCount(Class)`
     (`DRGameInstance.cpp:497` — **칩의 Category 를 읽는 마지막 장착 판정 지점**이다. 여기가 사라지면
     "Category 는 UI 전용" 원칙이 코드로 보장된다.)
   - `EnsureProgressInitialized` / `SanitizeClassState` 의 카테고리 순회(`for Category in Count`) 제거.
   - `GetChipViewModels` 는 전체 반환 + 카테고리 필터 버전 분리 (§8.3).
6. **`DRSaveGame.h:69`** — `CurrentSaveVersion = 3` → `4`.
   기존 마이그레이션 로직이 그대로 안전 초기화한다(미출시 단계라 환산 없음, §4.1).
7. **`DRPlayerController.cpp:1386`** — `SanitizeLoadout` 호출 인자를 `GI->GetMaxSlotCount()` 하나로.
8. **콘텐츠** — `DA_ChipCatalog` 의 `RequiredSlotTier` 를 1..6 스케일로 재입력(§6.2),
   `DA_ProgressionConfig` 의 `SlotCosts` 6개 입력 후 `ValidateCurrencyBudget()` 재실행.

### 17.2 완료 판정 (실측 결과)

| 판정 항목 | 결과 |
|---|---|
| `EDRChipCategory` 참조가 UI 표시 경로 + `ValidateCatalog` 돌파 칩 경고에만 남는가 | ✅ 8곳 — enum 정의 / `FDRUpgradeChipDefinition::Category` / `FDRChipViewModel::Category` / `GetChipViewModelsByCategory` 3곳 / `ValidateCatalog` 경고 1곳. **장착 판정 코드에는 0곳** |
| `grep -r "Ascension" Source/` 에 슬롯 관련 식별자가 없는가 | ✅ 2곳 (enum 값 `Ascension`, 돌파 칩 단점 경고) — 전부 칩 분류 이름 |
| 컴파일 | ✅ 통과 (신규 경고 0건, 기존 GAS deprecation 경고만 잔존) |
| BP 에셋 참조 파손 | ✅ 없음 — 변경된 13개 함수명을 참조하는 `.uasset` 이 하나도 없다(M5 UI 미착수) |
| 6칸 임의 조합(스탯 6 / 돌파 6 / 혼합) 시나리오 (§14.1) | ⬜ **PIE 수동 검증 대기** — `DA_ChipCatalog` / `DA_ProgressionConfig` 가 있어야 실행 가능(M6) |

### 17.3 계획과 달라진 점 (구현 중 내린 결정)

1. **`FDRClassUpgradeState` 에 래퍼 접근자를 두지 않았다.**
   §17.1-1 은 `GetSlotChips()` / `GetUnlockedSlotCount()` 를 인자 없는 접근자로 바꾸라고 했지만,
   배열이 하나뿐이라 분기가 사라진 시점에서 public 필드를 그대로 감싸는 함수는 순수한 잡음이다.
   → `State.SlotChips` / `State.UnlockedSlots` 직접 접근. 계산이 필요한 헬퍼 3개만 남겼다.
2. **`UDRChipCatalog::GetChipsForClassAndCategory()` 를 만들지 않았다.**
   `GetChipViewModelsByCategory()` 가 전체 뷰모델을 만든 뒤 `Category` 로 걸러내므로 필요가 없었다
   (칩 수가 로봇당 14개 안팎이라 비용이 무의미). 카탈로그에는 `GetChipsForClass(Class, Out)` 하나만 남는다.
3. **`UDRGameInstance::MakeChipViewModel()` 을 private 헬퍼로 추출했다.**
   `GetChipViewModels` / `GetChipViewModelsByCategory` 가 뷰모델 생성 로직을 복제하지 않게 하기 위함.
4. **`UnequipChip()` 에서 `UnknownChip` 검사를 제거했다.**
   기존 코드는 `Chip->Category` 로 배열을 고르기 위해 카탈로그를 조회했고, 그 부산물로 에러 코드를 냈다.
   통합 배열에서는 조회 자체가 불필요하다. 결과적으로 **카탈로그에서 삭제된 고아 칩도 UI에서 뺄 수 있게** 됐고,
   미장착 칩에는 `NotEquipped` 가 돌아간다(의미상 더 정확). 실사용 경로는 항상 유효한 Id 를 넘기므로 영향 없음.
5. **`EDRSlotState` / `PreviousSlotLocked` 는 넣지 않았다.** 그건 M5a(§21) 범위다 — M1' 을 순수 리팩터링으로 유지했다.

### 17.4 실행 시 주의

- 기존 세이브(`Saved/SaveGames/DaeRunePlayerProgress.sav`, v3)는 다음 실행 때
  `EnsureProgressInitialized()` 가 v4 불일치를 감지해 **진행도만 안전 초기화**한다.
  `bHasCompletedTutorial` 은 마이그레이션 대상이 아니므로 **튜토리얼 완료 플래그는 유지된다.**
  재화/해금/장착은 아직 아무것도 벌 수 없는 상태(`ProgressionConfig` 미지정)라 실질 손실이 없다.

---

## 18. 업그레이드 UI — 에셋 / 레이아웃 / 컬러 스펙 (M5b)

> 근거: `최종모습.jpg`(1170×670) · `최종칩.jpg`(1170×643) 픽셀 계측값을
> 프로젝트 디자인 해상도 **1920×1080** (`DefaultEngine.ini` → `DesignScreenSize`, `UIScaleRule=ShortestSide`) 로
> 환산한 값이다. 환산 계수 = 1080 / 670 ≈ **1.612**, 가로 여백 차(34px)는 좌우 17px씩 분배했다.
> 아래 수치는 **시작값**이다. 디자이너가 에디터에서 미세 조정하는 것을 전제로 한다.

### 18.1 텍스처 임포트 표

경로: `Content/DaeRuneAssets/UI/Upgrade/`

> ★**아래 "에셋명"은 프로젝트에 실제로 임포트된 이름이다**★ — 문서가 처음 제안했던
> `T_UI_Upg_*` 규칙은 **쓰이지 않았다.** 이 표가 유일한 정답이고, 문서 다른 곳에 남아 있는
> `T_UI_Upg_*` 표기를 보면 이 표로 환산해서 읽는다.

| 원본 파일 | 크기 | **실제 에셋명** | 용도 | Draw As / 특기 |
|---|---|---|---|---|
| `왼쪽.png` | 818×924 | **`Upgrade_LeftPanel`** | 좌측 슬롯 패널 배경 (탭 노치 포함) | `Image` (통짜 스트레치) |
| `오른쪽.png` | 962×923 | **`Upgrade_RightPanel`** | 우측 칩 패널 배경 **(1개로 통합)** | `Image` |
| `미창착슬롯.png` | 439×653 | **`UnInsertedSlot`** | 빈 슬롯 (해금됨·장착 안 됨) | `Image` |
| `장착슬롯.png` | 439×653 | **`InsertedSlot`** | 장착된 슬롯 | `Image` |
| `잠긴슬롯.png` | 439×653 | **`LockedSlot`** | 잠긴 슬롯 | `Image` |
| `idle 칩.png` | 117×222 | **`Upgrade_ChipBase`** | 칩 카드 **최하단** 배경 | `Image` |
| `idle 칩 그룹 맨위(오버레이).png` | 117×222 | **`Upgrade_ChipOverlay`** | 칩 카드 **최상단** 오버레이 | `Image` · §18.4 주의 |
| `Locked 자물쇠.png` | 192×192 | **`Upgrade_LockIcon`** | 미해금 칩 아이콘 | `Image` |
| `슬롯개수.png` | 186×50 | **`Upgrade_RequireSlot`** | `SLOT n` 뱃지 배경(+IC 글리프) | `Box` · Margin L=0.28 |
| `재화 아이콘.png` | 126×102 | **`Upgrade_CoinIcon`** | 재화 아이콘 | `Image` |
| `스크롤바.png` | 26×142 | **`Upgrade_ScrollBar`** | 스크롤 썸 | `Box` · Margin (0.45, 0.10) |
| `스크롤바 배경.png` | 19×693 | **`Upgrade_ScrollBar_Background`** | 스크롤 트랙 | `Box` · Margin (0.45, 0.02) |
| `맨위화면효과(블랜드모드소프트라이트).png` | 1907×1080 | **`Upgrade_UpperBlend`** | 전면 스캔라인 효과 | §18.6 |

**공통 임포트 설정** — 전부 동일하게 적용한다:
- `Texture Group = UI` / `Compression Settings = UserInterface2D (RGBA)`
- `sRGB = ✓` / `Mip Gen Settings = NoMipmaps` / `Address X,Y = Clamp`
- `Never Stream = ✓` (화면 열릴 때 밉 로딩 팝 방지)
- 알파 있는 PNG이므로 `Alpha Coverage Thresholds` 는 건드리지 않는다.

### 18.2 컬러 팔레트 (시안에서 추출)

| 이름 | Hex | 용도 |
|---|---|---|
| `TextPrimary` | `#EEFCFF` | 타이틀 / 탭 활성 / 키 안내 |
| `TextSecondary` | `#BCDAE5` | 재화 숫자 / 보조 라벨 |
| `TextDim` | `#7E93A6` | 탭 비활성 / 비활성 문구 |
| `AccentCyan` | `#A6E4FB` | 자물쇠 · 하이라이트 |
| `AccentCyanGlow` | `#CAFAFF` | 드롭 가능 칸 글로우 / 선택 링 |
| `ChipName` | `#9FD8F5` | 칩 이름 |
| `ChipValue` | `#A6D8F3` | 칩 효과 수치(큰 글씨) |
| `ChipBadgeText` | `#C0D1EB` | `SLOT n` 뱃지 글자 |
| `ChipBodyDim` | `#3C3F44` | 비활성 칩 카드 바탕(탈채도 결과) |
| `TextDisabled` | `#9AA0AE` | 비활성 칩 글자 |
| `WarnRed` | `#FF6B6B` | 단점(`bIsDrawback`) / 실패 사유 |
| `OkGreen` | `#7BE0A8` | 장점 라인 / 성공 토스트 |
| `PanelTint` | `#FFFFFF` | 패널 텍스처 기본 틴트(변경 금지) |

> 프로젝트에 컬러 SSOT가 없으므로 위 값을 **`DA_UpgradeUIStyle`**(신규 `UDataAsset`, §21.6) 에 넣고
> 위젯은 전부 여기서 읽는다. 하드코딩하면 색 보정 때 위젯을 전부 열어야 한다.

### 18.3 화면 레이아웃 (1920×1080 캔버스 기준)

```
┌──────────────────────────────────────────────────────────────────────┐  Canvas 1920×1080
│ (146,40) UPGRADE SYSTEM  [44pt]                  [재화 아이콘][120]  │  우측 정렬 x=1726
│                                                                      │
│  ┌ SB_PanelLeft (144,140) 717×810┐  ┌ SB_PanelRight (882,140) 844×810┐│
│  │ [CHIP SLOTS]                 │   │ [CHIPS]                      ▌││  ▌= 스크롤바
│  │   ┌────┬────┬────┐           │   │  ┌──┐ ┌──┐ ┌──┐ ┌──┐         ▌││
│  │   │ 01 │ 02 │ 03 │           │   │  │  │ │  │ │  │ │  │         ▌││
│  │   ├────┼────┼────┤           │   │  └──┘ └──┘ └──┘ └──┘         ▌││
│  │   │ 04 │ 05 │ 06 │           │   │  ┌──┐ ┌──┐ ┌──┐ ┌──┐         ▌││
│  │   └────┴────┴────┘           │   │  └──┘ └──┘ └──┘ └──┘          ││
│  └──────────────────────────────┘   └───────────────────────────────┘│
│                                              (1726,1010) M EXIT · ESC │
└──────────────────────────────────────────────────────────────────────┘
```

| 요소 | 앵커 | 위치(px) | 크기(px) | 비고 |
|---|---|---|---|---|
| `Img_ScreenFX` | Full (0,0,1,1) | Offset 0 | 화면 전체 | ZOrder **최상단**, HitTest **None** |
| `Txt_Title` | TopLeft | (**149, 44**) | Auto | `BrunoAceSC` **54pt**, `TextPrimary`, Letter Spacing +60 |
| `HB_Currency` | TopRight | (**-210, 66**) | Auto | Align (1,0). 아이콘 32×26 + 여백 12 + 숫자 **38pt** |
| `SB_PanelLeft` | TopLeft | (**99, 65**) | **849 × 959** | 패널 텍스처 100% 채움 |
| `SB_PanelRight` | TopLeft | (**823, 67**) | **998 × 958** | 〃 |
| `WBP_KeyHintBar` | BottomRight | (**-65, -60**) | Auto | Align (1,1). 별도 WBP (§19.5) |

> ★★**패널 크기는 시안 원본(1170×670) 재계측으로 확정한 값이다**★★
>
> 초안의 `717×810 / 844×810` 은 **약 18 % 작았다.** 시안에서 패널은 화면 세로의
> 대부분(65 → 1024)을 차지하는데, 810 은 그보다 한참 작다. 이것이 STEP 9 결과물이
> 시안과 가장 크게 달라 보인 원인이다 — 패널이 작으니 **안쪽 슬롯도 덩달아 작아 보인다.**
>
> **산출 과정**
>
> | 단계 | 값 |
> |---|---|
> | 시안 좌 패널 프레임 실측 | 가로 436 · 세로(본체) 473 → 비율 0.9218 |
> | 좌 텍스처 프레임 실측 | 가로 677(x 70~747) · 세로 736(y 116~852) → 비율 0.9198 |
> | → 시안이 텍스처 비율을 **그대로 지킨다**(오차 0.2 %) | ✅ |
> | 시안 → 화면 배율 | 1170×670 는 16:9 가 아니다. **세로 기준 ×1.612** 로 맞추고 가로 34px 을 좌우 17 씩 나눈다 |
> | 텍스처 → 화면 배율 | k = **1.0379** (좌 924 → 959) |
>
> **양쪽 캔버스를 같은 배율 k 로 키운다** — 두 텍스처의 프레임 높이가 781 로 동일해서
> 이렇게 해야 위아래가 딱 맞는다.
> 좌 `818×924 × 1.0379` = **849 × 959** / 우 `962×923 × 1.0379` = **998 × 958**
>
> **가로 배치 검산**: 좌 프레임 왼끝 `99 + 70k = 172` → 좌 프레임 오른끝 `99 + 747k = 875`
> → 간격 **21** → 우 프레임 왼끝 `823 + 70k = 896` → 우 프레임 오른끝 `823 + 891k = 1748`
> → 우측 여백 `1920 − 1748 = 172` ✅ **좌우 여백이 172 로 대칭**이다.
>
> **세로 검산**: 본체 윗선이 좌 `65 + 116k = 185`, 우 `67 + 114k = 185` ✅ 두 패널이 같은 높이에서 시작한다.
>
> ⚠️ **시안의 우 패널은 텍스처보다 6.8 % 넓게 그려져 있다.** 시안을 그대로 베끼면 우 패널만
> 가로로 늘어난다. **텍스처 비율을 따르기로 한 결정**(직전 개정)에 맞춰 998 을 쓴다.

**패널 내부 정규화 좌표** (텍스처 원본 실측 → 패널 크기에 비례 적용)

| 구간 | 좌 패널 (818×924 → 849×959) | 우 패널 (962×923 → 998×958) |
|---|---|---|
| 프레임 X | 8.56 % → 91.32 % | 7.28 % → 92.62 % |
| 탭 플레이트 Y | 7.68 % → 12.55 % | 7.69 % → 12.35 % |
| 탭 플레이트 X 끝 | 36.06 % | 25.68 % |
| 본체 Y | 12.55 % → 92.21 % | 12.35 % → 92.31 % |
| → 탭 플레이트 px | **(73, 74) ~ (306, 120)** | **(73, 74) ~ (256, 118)** |
| → 콘텐츠 px | **(73, 120) 702 × 764** | **(73, 118) 852 × 766** |
| → `Padding (L,T,R,B)` | **(73, 120, 74, 75)** | **(73, 118, 73, 74)** |

> **검산**: 좌 `73 + 702 + 74 = 849` ✓ `120 + 764 + 75 = 959` ✓
> / 우 `73 + 852 + 73 = 998` ✓ `118 + 766 + 74 = 958` ✓

- **좌 패널 탭**: `CHIP SLOTS` 라벨을 **(73,74)~(306,120)** 영역 **중앙**에 배치. `BrunoAceSC` **20pt**, `TextPrimary`.
- **우 패널 탭**: `CHIPS` 라벨을 **(73,74)~(256,118)** 영역 **중앙**에 배치. 〃

> ⚠️ ★**라벨은 좌측 정렬이 아니라 노치 안 중앙 정렬이다**★ — 시안에서 `CHIP SLOTS` 의
> 글자 중심(151.5)과 노치 중심(151.5)이 정확히 일치한다. 초안의 `Left + Pad(62,66,0,0)` 은
> 글자를 노치 왼쪽 끝에 붙여 버린다. **`SizeBox` 로 노치 크기를 잡고 그 안에서 Center** 로 둔다.
  - ★**탭 버튼이 아니라 그냥 `TextBlock` 이다**★ — 우측 패널은 **분류 없는 단일 목록**이라
    누를 것이 없다. 좌 패널의 `CHIP SLOTS` 와 완전히 같은 취급이다.
  - 노치(플레이트)는 두 텍스처 모두 **왼쪽 위에 1개씩** 그려져 있고, 이 라벨이 그 안에 들어간다.

**슬롯 그리드 (좌 패널 콘텐츠 안, 중앙 정렬)**

| 항목 | 값 |
|---|---|
| 배치 | `UniformGridPanel` 3열 × 2행 |
| 슬롯 카드 | **136 × 211** ← 시안 실측 `135.67 × 211.25` (원본 439×653) |
| 가로 간격 | **27** |
| 세로 간격 | **34** |
| 그리드 총 크기 | 3×136 + 2×27 = **462** × (2×211 + 34 = **456**) |
| `Slot Padding` | `FMargin(13.5, 17, 13.5, 17)` → 인접 칸 사이 27 / 34 |
| 콘텐츠 내 정렬 | Horizontal/Vertical **Center** (**702 × 764** 안에서 중앙) |
| 남는 여백 | 좌우 (702−462)/2 = **120** · 상하 (764−456)/2 = **154** |

> 그리드는 콘텐츠 영역보다 작다. **`Fill` 이 아니라 `Center`** 로 두어야 칸이 늘어나지 않는다.
>
> **슬롯 크기는 안 바꾼다.** 패널을 시안 크기로 키우고 나면 `136 × 211` 이 시안 비율과 맞는다:
> 시안에서 슬롯 폭 ÷ 콘텐츠 폭 = 81/436 = **18.6 %**, 이 스펙은 136/702 = **19.4 %** — 오차 4 %.
> 그리드 전체로 봐도 시안 277/436 = **63.5 %** vs 462/702 = **65.8 %** 로 거의 같다.
> → **슬롯이 작아 보였던 것은 슬롯 잘못이 아니라 패널이 작아서였다.**

> 슬롯 크기·간격은 **시안 디자인 파일의 크기 표시(135.67 × 211.25)에서 직접 가져온 값**이다.
> 초안의 142×206 / 간격 21·19 는 눈대중 환산이었다.

**칩 그리드 (우 패널 콘텐츠 안, ScrollBox)**

| 항목 | 값 |
|---|---|
| 배치 | `WrapBox` (Explicit Wrap Width) 또는 `UniformGridPanel` 4열 |
| 칩 카드 | **120 × 228** (원본 117×222 비율 0.527 유지) |
| 가로 간격 | 36 |
| 세로 간격 | 40 |
| 4열 총 폭 | 4×120 + 3×36 = **588** |
| ScrollBox 영역 | (73,118) **852 × 766** — 이 중 우측 24px 을 스크롤바에 양보 → 카드 영역 **828** |
| 좌우 패딩 | (828 − 588) / 2 = **120** |
| 스크롤바 | 폭 **18**, 패널 로컬 x = 73+852−18 = **907**, 항상 표시(`Always`) |

> **칩 카드 크기는 시안과 일치한다.** 시안 실측 카드 폭 75 · 간격 22 (시안 px)
> → 화면 배율 1.612 를 곱하면 **120 / 35.5** — 기존 스펙 `120 / 36` 그대로다. ✅
>
> ⚠️ ★**이제 5열이 들어가 버린다**★ — 카드 영역이 828 로 넓어져서
> 5×120 + 4×36 = **744 ≤ 828**. `WrapBox` 를 그냥 두면 **시안과 달리 5열로 배치된다.**
> 반드시 둘 중 하나로 막는다:
> - `WrapBox` → **`Explicit Wrap Size` ✓ + `Wrap Size` = 590** (4열 588 은 통과, 5열은 차단)
> - 또는 `UniformGridPanel` 로 바꾸고 BP 에서 `Row = i / 4`, `Column = i % 4` 로 넣는다 (확실함)

### 18.4 칩 카드 내부 스펙 (`최종칩.jpg` 계측)

칩 원본 카드는 313×563(글로우 제외)이고, 내부 요소는 카드 크기 대비 아래 비율로 배치돼 있다.
**120 × 228 카드 기준 px** 을 함께 적는다.

> ⚠️ **폰트 크기는 시안 계측값을 0.38배 축소한 값이어야 한다**(313 → 120px 카드).
> 아래 표의 pt 값은 그 축소를 반영한 최종값이며, 산출 근거와 넘침 대책은 **§22 STEP 5-5** 에 있다.
> 텍스트를 실제로 어디에 어떻게 놓는지(Overlay 정렬/Padding)는 **§22 STEP 5-3** 을 따른다.

| 요소 | X 비율 | Y 비율 | 120×228 기준 px | 내용 |
|---|---|---|---|---|
| 상단 노치 마커 `▲▼▲` | 중앙 | 7 % → 11 % | y 16 → 25 | 텍스처에 포함(별도 위젯 X) |
| 아이콘 박스 | 23 % → 73 % | 19 % → 42 % | (28,43) 60×53 | **텍스처에 그려져 있다 — 위젯 없음** |
| 칩 아이콘 | 박스 안 **83 %** | 〃 | 50×44, 중앙 y=47 | `Icon` / 미해금 시 `T_UI_Chip_Lock` |
| 이름 텍스트 | 8 % → 92 % | 46 % → 57 % | y 105 → 131 | **20pt**, `ChipName`, Center |
| 효과 수치 | 8 % → 92 % | 57 % → 74 % | y 131 → 170 | **30pt**, `ChipValue`, Center |
| `SLOT n` 뱃지 | 25 % → 75 % | 76 % → 82 % | (30,174) 61×13 | `T_UI_Upg_SlotBadge` + 10pt |
| 핀 스트립 | 8 % → 92 % | 92 % → 100 % | y 210 → 228 | 텍스처에 포함 |

**미해금 칩**은 이름/수치 자리에 `해금 조건: 슬롯 {n}칸` 을 **2행 워드랩**으로 대체하고
`SLOT n` 뱃지를 숨긴다(시안 3번 칩과 동일).

**카드 상태 4종**

| 상태 | 조건 | 표현 |
|---|---|---|
| `Normal` | `bUnlocked && !bEquipped && bCanEquipNow` | 기본. `Img_Overlay` Opacity **0.10** (표면 광택) |
| `Hover` | 마우스 오버 | Scale 1.06 (0.12s Ease), 외곽 글로우 `AccentCyanGlow`, Overlay 0.0 |
| `Disabled` | `bEquipped` 또는 `!bCanEquipNow` | 탈채도 회색. Overlay **0.70**, 텍스트 → `TextDisabled` |
| `Locked` | `!bUnlocked` | 아이콘 → 자물쇠, 이름/수치 → 해금 조건 문구, Overlay **0.35**, 드래그 금지 |

> ⚠️ **오버레이 텍스처에 대한 계측 결과와 결정.**
> `idle 칩 그룹 맨위(오버레이).png` 는 균일 회색 `RGB(182,182,182)` · 알파 **≈0.70** 의 칩 실루엣이다.
> 그런데 시안의 회색 칩(`최종칩.jpg` 가운데)은 **바탕의 명도(≈64)가 그대로 보존**돼 있다 —
> 이는 알파 합성이 아니라 포토샵 **Color/Saturation 블렌드(탈채도)** 로 만들어진 결과다.
> 알파 0.70 으로 그대로 덮으면 아이콘·글자 대비가 무너진다.
> → **기본 구현은 위 표의 Opacity 값 스텝**으로 간다(간단·저비용, 눈으로는 충분히 "회색 칩").
> → **시안과 픽셀 단위로 같게** 하려면 §22 STEP 3 의 `M_UI_Desat`(Desaturation 노드) 경로를 쓴다.
> 둘 중 어느 쪽을 쓸지는 아티스트가 실물을 보고 정한다. 위젯 구조는 **양쪽 다 그대로 지원**한다
> (Overlay 이미지 1장 + 각 이미지의 브러시를 머티리얼로 교체 가능하게 만들어 둔다).

### 18.5 슬롯 카드 내부 스펙 (`장착슬롯.png` / `잠긴슬롯.png` 계측)

원본 439×653 기준 내부 박스 위치 → **136 × 211 카드 기준 px**(시안 실측 `135.67 × 211.25`).

| 요소 | 136×211 px | 사용 상태 | 만드는 주체 |
|---|---|---|---|
| 아이콘 박스(청록 코너 삼각형) | (44, 55) **51 × 44** | `Filled` | **텍스처** |
| └ 그 안의 칩 아이콘 | **44 × 44**, 박스 중앙 | `Filled` | 위젯 `Img_ChipIcon` |
| 짧은 라벨 (`speed` / `+3`) | y **105 → 153** | `Filled` | 위젯 — **16pt / 22pt** (§22 STEP 6-5) |
| `SLOT n` 뱃지 | (43, 160) **50 × 13** | `Filled` | 위젯 — 배경 `T_UI_Upg_SlotBadge` + 9pt |
| 비용 박스(오목한 사각형) | (30, 160) **77 × 26** | `Locked` | **텍스처** |
| └ 그 안의 재화 아이콘 + 숫자 | 아이콘 17×13 · 숫자 **18pt** | `Locked` | 위젯 `HB_Cost` |
| 자물쇠 | 중앙 (54, 92) 26 × 33 | `Locked` | **텍스처** |
| `+` | 중앙 | `Empty` | **텍스처** |

> **칸 번호(`01`~`06`)는 없다.** 초안에 있었지만 확정 시안의 어느 슬롯에도 들어가지 않았다.
>
> 핵심은 **아이콘 박스와 비용 박스가 텍스처에 이미 그려져 있다**는 것이다.
> 위젯은 그 안을 채우기만 한다 — 박스 자체를 위젯으로 다시 그리지 않는다.

**슬롯 카드 상태 표현**

| 상태 | 배경 | 추가 표현 |
|---|---|---|
| `Locked` (다음 해금 대상) | `T_UI_Upg_Slot_Locked` | Opacity 1.0, 비용 표시, Hover 시 커서 Hand |
| `Locked` (그 뒤 칸) | 〃 | Opacity **0.45**, 클릭 불가, 툴팁 "앞 슬롯을 먼저 해금하세요" |
| `Empty` | `T_UI_Upg_Slot_Empty` | 드래그 중 드롭 가능하면 프레임 발광(§22 STEP 6-6) |
| `Occupied` | `T_UI_Upg_Slot_Filled` | 아이콘 + 이름/수치 + `SLOT n` 뱃지 |

### 18.6 전면 화면 효과 (`맨위화면효과`)

계측: 그레이스케일 스캔라인, **알파 최대 48/255(≈19 %)**, 화면 중앙에서 가장자리로 갈수록 알파 0(비네트).
원본 의도는 포토샵 **Soft Light** 다. UMG 에는 Soft Light 블렌드가 없다 — 두 가지 경로가 있다.

**경로 A — 권장(간단)**: `Img_ScreenFX` 를 그냥 `Image` 로 깔고 `ColorAndOpacity.A = 0.85`.
원본 알파가 이미 낮아서 스캔라인/비네트 느낌은 그대로 나온다. 비용 0, 위젯 1개.

**경로 B — 시안 정확 재현**: 화면 루트를 `RetainerBox` 로 감싸고 `Effect Material` 에 `M_UI_SoftLight` 지정.
`RetainerBox` 는 자식을 렌더타깃에 그린 뒤 머티리얼에 넘겨주므로 **바탕색을 읽을 수 있는 유일한 UMG 수단**이다.
Pegtop 근사식(1-pass, 분기 없음):

```
Result = (1 - 2*Blend) * Base*Base  +  2*Blend * Base
Final  = lerp(Base, Result, ScanTex.A * Strength)
```
- `Base` = RetainerBox 가 넣어주는 텍스처 파라미터(기본 이름 `Texture`)
- `Blend` = `T_UI_Upg_ScreenFX` 의 RGB, `ScanTex.A` = 그 알파
- `Strength` 스칼라 기본 1.0
- 주의: 풀스크린 RT 1장을 매 프레임 갱신한다. `RetainerBox` 의 `Phase Count = 2` 로 갱신 빈도를 낮출 수 있다.
  또 RetainerBox 안에서는 **위젯 단위 블러/리타이너 중첩이 깨질 수 있으므로** 화면 열림 애니메이션은
  RetainerBox **바깥**에 둔다.

> 1차 구현은 **경로 A** 로 진행한다. 아트 확인 후 부족하면 경로 B 로 교체 — 위젯 트리는 그대로다
> (`RetainerBox` 를 `CanvasPanel` 과 콘텐츠 사이에 한 층 끼우면 끝).

---

## 19. 위젯 구성 요소와 역할

> 명명 규칙: `WBP_` 접두 + `Upgrade` 도메인. C++ 베이스는 `UDRUpgrade*Widget`(§21.7).
> 위치: 위젯 `Content/Blueprints/UI/Upgrade/`, 머티리얼 `Content/DaeRuneAssets/UI/Upgrade/Material/`.
>
> **이 절은 "각 요소가 무엇을 하고 어떤 데이터를 보여주는가"만 다룬다.**
> 트리 구조 · 크기 · 정렬 · Padding · 폰트 같은 **제작 수치는 §22 STEP 5~9 가 유일한 출처**다.
> (같은 트리를 두 곳에 두었더니 한쪽만 고쳐져 서로 어긋났다 — 그래서 트리를 한 곳으로 합쳤다.)

### 19.1 위젯 목록

| 위젯 | C++ 베이스 | 역할 | 제작 |
|---|---|---|---|
| `WBP_UpgradeScreen` | `UDRUpgradeScreenWidget` | 화면 루트. GI 델리게이트 구독 · 전체 리프레시 · 닫기 | STEP 9 |
| `WBP_UpgradeSlot` | `UDRUpgradeSlotWidget` | 슬롯 1칸. 드롭 타깃 + 해금 버튼 + 장착 칩 드래그 소스 | STEP 6 |
| `WBP_UpgradeChip` | `UDRUpgradeChipWidget` | 칩 카드 1장. 드래그 소스 + 툴팁 | STEP 5 |
| `WBP_ChipDragVisual` | `UUserWidget` | 드래그 중 커서를 따라다니는 반투명 칩 | STEP 8-1 |
| `WBP_UpgradeTooltip` | `UDRUserWidget` | 칩 상세 툴팁 | STEP 8-2 |
| `WBP_UpgradeToast` | `UDRUserWidget` | 실패 사유 토스트(2.5초 자동 소멸) | STEP 8-3 |

> 제작 순서가 `Chip → Slot → 보조 → Screen` 인 이유: **화면이 나머지를 전부 참조**하기 때문이다.
> 부품을 먼저 만들어야 화면에서 바로 끌어다 놓을 수 있다.
>
> **`WBP_UpgradeTabButton` 은 만들지 않는다.** 우측 패널을 단일 목록으로 통합해 탭이 사라졌다(§18.4).
> 패널 상단의 `CHIPS` 는 화면 위젯 안의 `TextBlock` 하나면 된다. STEP 7 은 그래서 비어 있다.

### 19.2 `WBP_UpgradeScreen` 구성 요소

| 요소 | 역할 | 데이터 출처 |
|---|---|---|
| `Img_Backdrop` | 로비 3D 뷰를 어둡게 눌러 UI 대비 확보 | 고정 (`#0B1017` A=0.55) |
| `Txt_Title` | 화면 제목 `UPGRADE SYSTEM` | 고정 문구(LOCTEXT) |
| `HB_Currency` | 보유 재화 | `GI->GetCurrency()` + `OnCurrencyChanged` |
| `Txt_SlotsTabLabel` | 좌측 패널 제목 `CHIP SLOTS` | 고정 문구 |
| `Grid_Slots` | **통합 슬롯 6칸** (`Slot_0` … `Slot_5`) | `GI->GetSlotViewModels()` |
| `Txt_ChipsTabLabel` | 우측 패널 제목 `CHIPS` | 고정 문구 |
| `Scroll_Chips` → `Wrap_Chips` | **이 로봇의 칩 전체**를 담는 단일 목록 (탭 없음) | `GI->GetChipViewModels()` |
| `Txt_EmptyList` | 표시할 칩이 없을 때의 안내 | 목록이 비었을 때만 |
| `WBP_KeyHintBar` | `ESC BACK` / `M EXIT` — **설정 화면 위젯 재사용** (`A`/`R` 숨김) | 고정 문구 |
| `VB_Toasts` | `WBP_UpgradeToast` 가 추가되는 자리 | 실패 시 런타임 추가 |
| `Img_ScreenFX` | 전면 스캔라인 효과 | 고정 텍스처 |

**이 위젯에서만 지켜야 할 것**
- 루트 위젯 **`Is Focusable ✓`** — ESC 키를 위젯에서 받으려면 필요하다.
- `Img_ScreenFX` / `Img_Backdrop` 은 반드시 **`Hit Test Invisible`**.
  `Visible` 이면 화면 전체를 덮은 이미지가 드래그·클릭을 전부 먹는다 — **가장 흔한 실수.**
- `Grid_Slots` 의 6칸은 **디자이너에서 미리 배치**한다(런타임 생성 X).
  `MaxSlots = 6` 고정이고, 고정 배치해야 하이라이트·애니메이션에서 이름으로 참조할 수 있다.
  단 `Event Construct` 에서 `GI->GetMaxSlotCount()` 와 개수가 다르면 **경고 로그**를 남긴다
  (Config 의 `MaxSlots` 를 바꿨는데 위젯을 안 고친 상황을 조용히 넘기지 않기 위함).
- `Wrap_Chips` 의 칩은 **런타임 생성**한다. 칩 개수가 로봇마다 다르기 때문이다.

### 19.3 `WBP_UpgradeSlot` 구성 요소

뷰모델: **`FDRSlotViewModel`** (§21.1)

| 요소 | 역할 | 표시 조건 | 데이터 출처 |
|---|---|---|---|
| `Img_SlotBg` | 칸 배경. **상태별 3텍스처 스왑** + 드롭 가능 시 **발광** | 항상 | `State` → `Slot_Empty/Filled/Locked` |
| `Panel_Filled` | 장착 상태 묶음 | `State == Occupied` | — |
| └ `Img_ChipIcon` | 장착된 칩 아이콘 (텍스처의 아이콘 박스 안) | 〃 | `ChipIcon` (폴백은 C++ `ResolveChipIcon()`) |
| └ `Txt_StatName` | 효과 이름 | 〃 | **`ShortLabelTop`** |
| └ `Txt_StatValue` | 효과 수치 | 〃 | **`ShortLabelBottom`** |
| └ `Panel_SlotBadge` | `SLOT n` — **이 칩이 먹는 칸 수** | 〃 | `GroupSize` |
| `Panel_Locked` | 잠긴 상태 묶음 | `State == Locked` | — |
| └ `HB_Cost` | 해금 비용 (재화 아이콘 + 숫자) | `UnlockCost >= 0` | `UnlockCost` |
| └ `Btn_Unlock` | **해금 클릭 전담** | 〃 (`bIsNextUnlockable` 일 때만 Enabled) | `UnlockBlockReason` |

> **`+` 기호 · 자물쇠 · 아이콘 박스 · 비용 박스는 전부 텍스처에 그려져 있다.**
> 위젯은 그 안을 **채우기만** 한다 — 박스 자체를 다시 그리지 않는다.
> 칸 번호(`01`~`06`)는 **시안에 없어서 만들지 않는다.**

**이 위젯에서만 지켜야 할 것**
- 슬롯 카드는 **드롭 타깃이자 드래그 소스**다(`Occupied` 일 때 칩을 끌어낼 수 있어야 한다).
- 드래그는 **루트의 `OnMouseButtonDown` → `DetectDragIfPressed`** 로 잡는다.
  그래서 `Btn_Unlock` 은 **`Panel_Locked` 안**에 둬서 잠긴 칸에만 존재하게 한다 —
  슬롯 전체를 덮는 버튼이 있으면 마우스 다운을 삼켜 드래그가 시작되지 않는다.
- **칩 카드(`WBP_UpgradeChip`)를 슬롯 안에 넣지 않는다.** 칩 카드에는 자체 배경·노치·핀 스트립이
  있어 슬롯 프레임과 이중으로 겹친다. 슬롯은 자기 텍스처 위에 아이콘/텍스트를 직접 얹는다.
  대신 `ShortLabelTop`/`Bottom` 이 **칩 카드와 같은 함수**로 생성되므로 표기는 항상 일치한다.
- 드롭 가능 표시는 **슬롯 텍스처 자신을 밝히는 방식**이다(§22 STEP 6-6).
  둥근 사각형 테두리로는 노치가 있는 프레임 모양을 따라갈 수 없다.
- 잠긴 칸 중 **바로 다음 칸만** 또렷하고(`bIsNextUnlockable`), 그 뒤 칸은 흐리게 + 클릭 불가.

### 19.4 `WBP_UpgradeChip` 구성 요소

뷰모델: **`FDRChipViewModel`** (§8.2)

| 요소 | 역할 | 표시 조건 | 데이터 출처 |
|---|---|---|---|
| `Img_ChipBase` | 칩 카드 배경 | 항상 | 고정 텍스처 |
| `Img_ChipIcon` | 칩 아이콘 / **미해금이면 자물쇠** | 항상 | `Icon` · 미해금 시 `Style.LockIcon` |
| `VB_Text` | 이름 + 수치 묶음 | `bUnlocked` | — |
| └ `Txt_ChipName` | 효과 이름 | 〃 | **`ShortLabelTop`** (★`DisplayName` 아님 — §22 STEP 5-5) |
| └ `Txt_ChipValue` | 효과 수치 | 〃 | **`ShortLabelBottom`** |
| `Txt_UnlockReq` | `해금 조건: 슬롯 N칸` | **`!bUnlocked`** | `RequiredSlotTier` |
| `Panel_SlotBadge` | `SLOT n` — **이 칩이 먹는 칸 수** | `bUnlocked` | `RequiredSlotCount` |
| `Img_Overlay` | 표면 광택 + **비활성 회색 처리** | 항상(알파만 변함) | `bEquipped \|\| !bCanEquipNow` → 0.70 |
| (Hover 확대) | "집을 수 있다" | Hover **이며 `IsDraggable()`** | `Anim_Hover` |
| (툴팁) | 이름 · 대상 스킬 · 요약 · 장단점 · 차단 사유 | 마우스 오버 | `WBP_UpgradeTooltip` |

> **장착됨 / 장착 불가가 같은 회색으로 보인다.** 전용 리본 위젯을 두지 않기로 했기 때문이다.
> 구분이 필요해지면 툴팁의 `Txt_BlockReason`(`AlreadyEquipped` vs `NotEnoughSlots`)으로 알린다.

**이 위젯에서만 지켜야 할 것**
- **카드에는 `DisplayName` / `EffectSummary` 가 들어가지 않는다.** 120px 카드에 한글 문장이
  들어가지 않기 때문이다. 그 둘은 **툴팁**에서 보여준다.
- `Img_Overlay` 는 **레이어 최상단**이다. 단 `Hit Test Invisible` 이어야 드래그가 막히지 않는다.
- Hover 스케일은 **루트의 `Render Transform → Scale`** 에 건다. 자식에 걸면 레이아웃이 밀린다.
- **미해금·장착 완료 칩은 Hover 연출을 재생하지 않는다.** 집을 수 없는데 집을 수 있는 것처럼
  반응하면 거짓 정보다. 판정은 C++ 이 주는 `IsDraggable()` 하나로 한다.

### 19.5 보조 위젯 구성 요소

**`WBP_ChipDragVisual`** — 드래그 중 커서를 따라다닌다

| 요소 | 역할 |
|---|---|
| (칩 카드 축약본) | 무엇을 들고 있는지 보여준다 |

> **반드시 `Hit Test Invisible`.** 아니면 커서 아래 붙어 다니는 이 위젯이
> 슬롯 대신 드롭을 가로챈다 — **드래그앤드롭 버그 1순위.**

**`WBP_UpgradeTooltip`** — 칩의 진짜 설명이 나오는 곳

| 요소 | 데이터 출처 | 표시 조건 |
|---|---|---|
| `Txt_Name` | `DisplayName` | 항상 |
| `Txt_TargetSkill` | `TargetSkillName` (없으면 "전체") | 항상 |
| `Txt_Summary` | `EffectSummary` | 항상 |
| `VB_Benefit` | `BenefitLines` (`OkGreen`) | **항상** — 세부 토글과 무관 |
| `VB_Drawback` | `DrawbackLines` (`WarnRed`) | **항상** — 세부 토글과 무관 |
| `VB_Detail` | `DetailLines` | **세부 정보 토글 ON 일 때만** |
| `Txt_BlockReason` | `GetUpgradeResultText(EquipBlockReason)` | `EquipBlockReason != Success` |

> 돌파 칩의 **장단점은 숨기면 안 되는 정보**다(요구사항 §10.1). 그래서 `VB_Benefit`/`VB_Drawback`
> 만 토글에서 제외한다. 토글은 `VB_Detail`(정확한 수치)에만 걸린다.

**`WBP_UpgradeToast`** — `Txt_Message` 한 줄 + `Anim_Toast`(2.5초 후 자동 소멸).
문구는 전부 `UDRUpgradeUILibrary::GetUpgradeResultText()` 가 만든다(§21.5).

### 19.6 스크롤 / 성능 메모

- 칩 개수는 로봇당 14개 안팎(§6.2)이라 **`ScrollBox` + 전량 생성으로 충분**하다.
  탭이 없어져 목록이 한 번에 14장 전부 뜨지만, 장착/해금 때 `ClearChildren()` 후
  전량 재생성해도 프레임 하락이 체감되지 않는다.
- 30개를 넘어가면 `ListView`(+ `UDRUpgradeChipWidget` 에 `IUserObjectListEntry` 구현)로 바꾼다.
  그때는 뷰모델을 `UObject` 래퍼(`UDRChipViewModelObject`)로 감싸야 하므로 **§21.1 에 래퍼도 함께 정의해 둔다.**
- 스크롤바 스타일은 `Scroll_Chips → Style → WidgetStyle` 에서
  `Normal/Hovered/Dragged Thumb Image = T_UI_Upg_ScrollThumb`,
  `Vertical/Horizontal Background Image = T_UI_Upg_ScrollTrack` 로 지정한다(둘 다 `Box` 브러시).
- `Scroll_Chips` 의 `Allow Overscroll = ✗`, `Animate Wheel Scrolling = ✓`, `Wheel Scroll Multiplier = 1.5`.

---

## 20. 드래그앤드롭 상세 설계

### 20.1 상호작용 매트릭스

| 출발 | 도착 | 동작 | 호출 API |
|---|---|---|---|
| 칩 목록 | 해금된 빈 칸 | 장착 | `EquipChip(Class, ChipId, DropIndex)` |
| 칩 목록 | 해금된 점유 칸 | **교체** (기존 칩 해제 후 장착) | `SwapOrEquipChip(Class, ChipId, DropIndex)` |
| 칩 목록 | 잠긴 칸 | 실패 — 토스트 "잠긴 슬롯입니다" | (사전 차단, 호출 안 함) |
| 칩 목록 | 패널 밖 | 취소(원위치) | — |
| 슬롯 | 다른 빈 칸 | **이동** | `MoveChip(Class, FromIndex, ToIndex)` |
| 슬롯 | 다른 점유 칸 | **자리 교환** | `MoveChip()` 내부에서 스왑 처리 |
| 슬롯 | 우측 칩 목록 영역 | **해제** | `UnequipChip(Class, ChipId)` |
| 슬롯 | 자기 자신 | 아무 것도 안 함 | — |
| (클릭) 칩 목록 더블클릭 | — | 앞에서부터 자동 장착 | `EquipChip(Class, ChipId, -1)` |
| (클릭) 슬롯 우클릭 | — | 그 칸의 칩 해제 | `UnequipSlot(Class, SlotIndex)` |
| (클릭) 잠긴 칸 클릭 | — | 해금 확인 → 해금 | `CanUnlockSlot` → `UnlockSlot` |

> **키보드/게임패드 대체 경로가 반드시 필요하다.** 드래그앤드롭만 있으면 패드로 조작할 수 없다.
> 위 표의 **더블클릭 장착 / 우클릭 해제 / 클릭 해금**이 그 대체 경로다. 드래그는 "빠른 길"일 뿐,
> 모든 조작이 클릭만으로도 가능해야 한다. (§20.7)

### 20.2 이벤트 배선 (BP 그래프)

**드래그 소스 — `WBP_UpgradeChip`**
```
OnMouseButtonDown(Root)
  └─ if (!VM.bUnlocked) → return Unhandled            // 미해금 칩은 못 든다
     if (VM.bEquipped)  → return Unhandled            // 이미 장착 → 좌측에서 빼야 한다
     DetectDragIfPressed(PointerEvent, Key = LeftMouseButton) → Handled

OnDragDetected(Geometry, PointerEvent, out Operation)
  ├─ Op = NewObject<UDRChipDragDropOperation>()
  ├─ Op.ChipId = VM.ChipId ; Op.OwnerClass = ViewedClass
  ├─ Op.SourceSlotIndex = -1                          // 목록에서 시작
  ├─ Op.RequiredSlotCount = VM.RequiredSlotCount
  ├─ Op.bHasDrawback = VM.bHasDrawback
  ├─ Op.DefaultDragVisual = CreateWidget(WBP_ChipDragVisual)
  │      → InitFromDrag(VM.Icon, VM.ShortLabelTop, VM.ShortLabelBottom)
  ├─ Op.Pivot = MouseDown                              // 잡은 지점을 유지 (CenterCenter 는 튄다)
  └─ Screen->BeginDragHighlight(Op)                    // §20.3
```

**드래그 소스 — `WBP_UpgradeSlot` (장착 칩 끌어내기)**
```
OnMouseButtonDown(Root)
  └─ if (State != Occupied) → Unhandled
     DetectDragIfPressed(LeftMouseButton)

OnDragDetected
  ├─ Op.SourceSlotIndex = VM.SlotIndex                 // 위와 다른 유일한 값
  └─ Op.DefaultDragVisual = CreateWidget(WBP_ChipDragVisual)
         → InitFromDrag(VM.ChipIcon, VM.ShortLabelTop, VM.ShortLabelBottom)
                        ★★★ 슬롯 뷰모델은 Icon 이 아니라 ChipIcon 이다
     Screen->BeginDragHighlight(Op)
```

> ★**`InitFromDrag` 가 뷰모델 구조체를 받지 않는 이유**★ — 드래그 소스가 두 곳인데
> 슬롯은 `FDRSlotViewModel` 만 갖고 있어 **`FDRChipViewModel` 을 넘길 수가 없다.**
> 드래그 비주얼이 실제로 쓰는 값은 아이콘·2행 라벨 **3개뿐**이고 두 뷰모델 모두 그것을 갖고 있으므로,
> **원시 타입 3개로 받는다.** 상세는 §22 STEP 8-1.

**드롭 타깃 — `WBP_UpgradeSlot`**
```
OnDragEnter  → if (CanAcceptDrop(Op)) PlayDropPulse()      // 슬롯 프레임 발광 (STEP 6-5)
               else                    커서 = SlashedCircle
OnDragLeave  → StopDropPulse()
OnDragOver   → return true  (호버 유지)
OnDrop       → Screen->HandleDropOnSlot(Op, SlotIndex) → true
```

**드롭 타깃 — `WBP_UpgradeScreen` 의 우측 패널(`SB_PanelRight`)**
```
OnDrop → if (Op.SourceSlotIndex >= 0)  GI->UnequipChip(Class, Op.ChipId)  // 해제
         else                          아무 것도 안 함(원위치)
```

**드래그 종료 공통 — `WBP_UpgradeScreen`**
```
OnDragCancelled / (모든 OnDrop 끝)
  └─ EndDragHighlight()    // 6칸 하이라이트 전부 원복. 반드시 호출 — 안 하면 링이 남는다
```

> `UDragDropOperation::OnDrop`/`OnDragCancelled` 델리게이트를 쓰지 말고
> **화면 위젯이 `EndDragHighlight()` 를 책임지게** 한다. 델리게이트는 위젯 파괴 순서에 따라 안 불릴 수 있다.

### 20.3 하이라이트 규칙 (통합 슬롯을 눈으로 알리는 장치)

```
BeginDragHighlight(Op):
    for SlotIndex in 0..MaxSlots-1:
        Result = GI->CanEquipChipAtSlot(Class, Op.ChipId, SlotIndex, Req, Free)
        switch (Result):
          Success        → 청록 글로우 ON  (드롭 가능)
          AlreadyEquipped→ (자기 자신이 점유한 칸) 회색 링, 드롭 시 무시
          NotEnoughSlots → 주황 링 + 칸 위에 "필요 {Req} / 남은 {Free}"
          그 외          → Opacity 0.4 로 눌러서 "여긴 안 된다"를 명시
```
- **카테고리로 거르지 않는다.** 스탯 칩이든 돌파 칩이든 **해금된 빈 칸은 전부 청록**이 된다.
  이 동작 하나가 "슬롯에 종류 구분이 없다"(요구사항 11)를 UI에서 증명한다.
- **칩은 전부 1칸짜리다**(§6.4 콘텐츠 결정). 그래서 하이라이트는 **드롭 대상 칸 하나만** 칠하면 된다 —
  "이 칩을 놓으면 어느 칸들이 함께 먹히는가"를 예측할 필요가 없다.
  `NotEnoughSlots` 는 **빈 칸이 0일 때**만 나온다.

### 20.4 드롭 처리 (`WBP_UpgradeScreen::HandleDropOnSlot`)

```
HandleDropOnSlot(Op, TargetIndex):
    Class = GetViewedClass()

    // 0) 이 화면이 보고 있는 로봇의 칩이 아니면 무시 (DRChipDragDropOperation.h:30)
    if (Op.OwnerClass != Class):
        EndDragHighlight(); return false

    // 1) 슬롯 → 슬롯
    if (Op.SourceSlotIndex >= 0):
        if (Op.SourceSlotIndex == TargetIndex):                     // 제자리
            EndDragHighlight(); return true
        R = GI->MoveChip(Class, Op.SourceSlotIndex, TargetIndex)

    // 2) 목록 → 슬롯
    else:
        R = GI->SwapOrEquipChip(Class, Op.ChipId, TargetIndex)

    if (R == Success):  PlaySound(ChipEquip)
    else:               PlaySound(Denied);     ShowToast(GetUpgradeResultText(R))
    EndDragHighlight()
    return true
```

- **성공 시 갱신을 직접 부르지 않는다.** `MoveChip`/`SwapOrEquipChip` 이 성공하면 GameInstance 가
  `OnUpgradesChanged` 를 쏘고, C++ 이 `OnRefreshSlots`/`OnRefreshChips` 를 발화시킨다(§21.7).
  여기서 `RefreshAll()` 을 또 부르면 목록이 두 번 재생성돼 스크롤이 튄다.
- 반대로 **실패했을 때는 델리게이트가 안 온다**(변경이 없으므로). 그래서 `EndDragHighlight()` 는
  성공/실패 양쪽 경로에서 **직접** 불러야 한다 — 위 코드가 마지막에 한 번만 부르는 이유다.
- 모든 상태 변경은 `UDRGameInstance` 안에서 **`SaveProgress()` 까지 끝난다.** UI는 저장을 신경 쓰지 않는다.

### 20.5 실패 사유 → 사용자 문구

`UDRUpgradeUILibrary::GetUpgradeResultText()`(§21.5) 한 곳에서만 변환한다.

| `EDRUpgradeResult` | 문구 (LOCTEXT) |
|---|---|
| `SystemLocked` | 스테이지 1을 클리어하면 열립니다. |
| `UnknownChip` | 알 수 없는 칩입니다. |
| `WrongClass` | 이 로봇의 칩이 아닙니다. |
| `ChipLocked` | 슬롯을 {0}칸 이상 해금해야 사용할 수 있습니다. |
| `AlreadyEquipped` | 이미 장착한 칩입니다. |
| `NotEquipped` | 장착되어 있지 않습니다. |
| `NotEnoughSlots` | 빈 슬롯이 부족합니다. (필요 {0} / 남은 {1}) |
| `NotEnoughCurrency` | 재화가 부족합니다. ({0} 필요) |
| `AllSlotsUnlocked` | 모든 슬롯을 해금했습니다. |
| `NoSlotToRefund` | 환불할 슬롯이 없습니다. |
| `RefundDisabled` | 슬롯 환불이 비활성화되어 있습니다. |
| `SlotOccupied` | 칩을 먼저 해제해야 합니다. |
| `InvalidConfig` | 업그레이드 설정이 없습니다. (개발자에게 문의) |

### 20.6 애니메이션 / 사운드

| 이벤트 | 연출 | 사운드 |
|---|---|---|
| 화면 열림 | 패널 2장이 좌·우에서 24px 슬라이드 인 + Fade 0→1 (0.18s) | `UI_Upgrade_Open` |
| 화면 닫힘 | 역재생 (0.14s) | `UI_Upgrade_Close` |
| 칩 Hover | Scale 1.0 → 1.06 (0.12s, EaseOut) | `UI_Hover` |
| 드래그 시작 | 원본 칩 Opacity 1.0 → 0.35 | `UI_Chip_Pick` |
| 드롭 가능 칸 | 링 Opacity 0.4 ↔ 1.0 루프 (0.8s) | — |
| 장착 성공 | 슬롯 카드 Scale 1.15 → 1.0 (0.2s) + 청록 플래시 | `UI_Chip_Equip` |
| 장착 실패 | 슬롯 Shake ±6px (0.15s) | `UI_Denied` |
| 슬롯 해금 | 잠긴 텍스처 → 빈 텍스처 크로스페이드 + 파티클성 플래시 (0.35s) | `UI_Slot_Unlock` |
| 재화 변동 | 숫자 롤링(0.4s) + 아이콘 Punch | `UI_Currency` |

- 사운드는 기존 `UDRSoundDataAsset`(GameInstance 보유)에 키를 추가해 참조한다. 위젯에 하드 레퍼런스 금지.

### 20.7 접근성 / 입력 대체

- **패드/키보드 전용 경로**: 좌우 패널을 `Tab`/`Shift+Tab` 으로 오가고, 방향키로 칸/칩 이동,
  `Enter` = 장착·해금, `Delete`/`Backspace` = 해제. UMG `Navigation` 규칙을 각 위젯 `Widget Navigation` 에서
  Explicit 으로 지정한다(Wrap 기본값은 그리드에서 엉뚱한 칸으로 튄다).
- `ESC` = 닫기(`PC->CloseUpgradeScreen()`), `M` = 닫기(장치 밖으로).
  `WBP_UpgradeScreen::NativeOnKeyDown` 에서 처리하고 **`Handled` 를 반환**한다.
- 드래그 중 `ESC` → `CancelDragDrop()` 후 하이라이트 원복.

---

## 21. M5a — UI 지원 코드 보강 (신규/변경 C++)

> 전제: **§17 의 M1'(통합 슬롯 전환)이 먼저 끝나 있어야 한다.** 아래 시그니처는 전부 카테고리 인자가 없는
> 통합 슬롯 API 기준이다. M1' 전에 UI를 만들면 API가 두 번 바뀐다.

### 21.1 신규 타입 (`DRUpgradeTypes.h` / `DRProgressionTypes.h`)

```cpp
// DRUpgradeTypes.h
/** 슬롯 1칸의 상태. UI 텍스처 스왑의 기준. */
UENUM(BlueprintType)
enum class EDRSlotState : uint8
{
    Locked      UMETA(DisplayName = "잠김"),
    Empty       UMETA(DisplayName = "빈 칸"),
    Occupied    UMETA(DisplayName = "장착됨")
};
```

```cpp
// DRProgressionTypes.h
/**
 * 슬롯 1칸을 그리는 데 필요한 모든 정보. (FDRChipViewModel 의 슬롯 버전)
 * UI 가 GetSlotAssignments() + 카탈로그를 직접 조합하지 않게 하기 위한 뷰모델.
 */
USTRUCT(BlueprintType)
struct FDRSlotViewModel
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Slot") int32 SlotIndex = 0;          // 0..MaxSlots-1
    UPROPERTY(BlueprintReadOnly, Category = "Slot") EDRSlotState State = EDRSlotState::Locked;

    // ===== Occupied 일 때 =====
    UPROPERTY(BlueprintReadOnly, Category = "Slot") FName ChipId;
    UPROPERTY(BlueprintReadOnly, Category = "Slot") FText ChipName;
    UPROPERTY(BlueprintReadOnly, Category = "Slot") FText ShortLabelTop;      // "ATK"
    UPROPERTY(BlueprintReadOnly, Category = "Slot") FText ShortLabelBottom;   // "+5"
    UPROPERTY(BlueprintReadOnly, Category = "Slot") TObjectPtr<UTexture2D> ChipIcon;
    UPROPERTY(BlueprintReadOnly, Category = "Slot") bool bHasDrawback = false;
    // 다중 칸 칩 묶음 표시용 (GroupSize == RequiredSlotCount)
    UPROPERTY(BlueprintReadOnly, Category = "Slot") int32 GroupSize = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Slot") int32 GroupOrder = 0;      // 0-base

    // ===== Locked 일 때 =====
    UPROPERTY(BlueprintReadOnly, Category = "Slot") int32 UnlockCost = -1;     // -1 = 비용 미정
    UPROPERTY(BlueprintReadOnly, Category = "Slot") bool bIsNextUnlockable = false;  // 순차 해금상 지금 살 수 있는 칸
    UPROPERTY(BlueprintReadOnly, Category = "Slot") EDRUpgradeResult UnlockBlockReason = EDRUpgradeResult::Success;
};
```

```cpp
// ListView 로 전환할 때만 필요 (§19.6). 미리 정의해 두면 교체가 1줄이다.
UCLASS(BlueprintType)
class DAERUNE_API UDRChipViewModelObject : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadOnly, Category = "Chip") FDRChipViewModel Data;
};
```

### 21.2 `UDRGameInstance` — 조회 API 추가

```cpp
// 좌측 6칸을 그리는 단일 진입점. 길이 = GetMaxSlotCount().
UFUNCTION(BlueprintCallable, Category = "Progression|Slot")
void GetSlotViewModels(EPlayerCharacterClass CharacterClass, TArray<FDRSlotViewModel>& OutSlots) const;

// 특정 칸의 해금 비용 (SlotIndex 0..MaxSlots-1). 범위 밖이면 -1.
// 시안이 "모든 잠긴 칸에 비용 표시" 라서 GetNextSlotUnlockCost() 만으로는 부족하다.
UFUNCTION(BlueprintPure, Category = "Progression|Slot")
int32 GetSlotUnlockCost(int32 SlotIndex) const;

// 카테고리 필터 버전 (기존 GetChipViewModels 는 전체 반환으로 시그니처 변경 — §17.1 참조)
UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
void GetChipViewModelsByCategory(EPlayerCharacterClass CharacterClass, EDRChipCategory Category,
    TArray<FDRChipViewModel>& OutViewModels) const;
```

`GetSlotViewModels()` 구현 요지:
```cpp
const int32 Max = GetMaxSlotCount();
const FDRClassUpgradeState* State = FindClassState(CharacterClass);
const int32 Unlocked = State ? State->UnlockedSlots : 0;

// 같은 ChipId 가 몇 칸을 먹는지 미리 센다 (GroupSize / GroupOrder 계산용)
TMap<FName, int32> Counts, Seen;
for (const FName& Id : State->SlotChips) if (!Id.IsNone()) Counts.FindOrAdd(Id)++;

OutSlots.SetNum(Max);
for (int32 i = 0; i < Max; ++i)
{
    FDRSlotViewModel& VM = OutSlots[i];
    VM.SlotIndex = i;

    if (i >= Unlocked)                       // 잠긴 칸
    {
        VM.State = EDRSlotState::Locked;
        VM.UnlockCost = GetSlotUnlockCost(i);
        VM.bIsNextUnlockable = (i == Unlocked);
        VM.UnlockBlockReason = VM.bIsNextUnlockable ? CanUnlockSlot(CharacterClass)
                                                    : EDRUpgradeResult::AllSlotsUnlocked; // "앞 칸 먼저"
        continue;
    }

    const FName Id = State->SlotChips.IsValidIndex(i) ? State->SlotChips[i] : NAME_None;
    if (Id.IsNone()) { VM.State = EDRSlotState::Empty; continue; }

    VM.State = EDRSlotState::Occupied;
    VM.ChipId = Id;
    VM.GroupSize  = Counts.FindRef(Id);
    VM.GroupOrder = Seen.FindOrAdd(Id)++;
    if (const FDRUpgradeChipDefinition* Chip = FindChip(Id))
    {
        VM.ChipName = Chip->DisplayName;
        VM.ChipIcon = Chip->Icon;
        VM.bHasDrawback = Chip->HasDrawback();
        UDRUpgradeUILibrary::MakeShortLabel(*Chip, VM.ShortLabelTop, VM.ShortLabelBottom); // "ATK" / "+5"
    }
}
```

> `bIsNextUnlockable == false` 인 잠긴 칸의 `UnlockBlockReason` 에 쓸 전용 코드가 없다.
> **`EDRUpgradeResult` 에 `PreviousSlotLocked` 를 1개 추가**하는 편이 정직하다(§21.8 체크리스트).

### 21.3 `UDRGameInstance` — 칸 단위 장착 API 추가

기존 `CanEquipChip()` 은 "어딘가에 넣을 수 있는가"만 본다. 드래그앤드롭은 **특정 칸**에 대한 판정이 필요하다.

```cpp
// 이 칩을 "이 칸에" 놓을 수 있는가. 잠긴 칸/자기 자신 점유 칸까지 구분해 돌려준다.
UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
EDRUpgradeResult CanEquipChipAtSlot(EPlayerCharacterClass CharacterClass, FName ChipId, int32 SlotIndex,
    int32& OutRequiredSlots, int32& OutFreeSlots) const;

// 점유 칸에 드롭했을 때: 기존 칩을 해제하고 새 칩을 넣는다.
// 넣기에 실패하면 해제를 되돌린다(원자적).
UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
EDRUpgradeResult SwapOrEquipChip(EPlayerCharacterClass CharacterClass, FName ChipId, int32 SlotIndex);

// 슬롯 → 슬롯 이동/교환. 다중 칸 칩은 통째로 재배치한다.
UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
EDRUpgradeResult MoveChip(EPlayerCharacterClass CharacterClass, int32 FromSlotIndex, int32 ToSlotIndex);

// 이 칩을 이 칸에 놓으면 실제로 어느 칸들이 채워지는지 (하이라이트 미리보기용).
// EquipChip() 의 배정 규칙과 **같은 내부 함수**를 쓴다 — UI 가 자체 예측하면 실제와 어긋난다.
UFUNCTION(BlueprintCallable, Category = "Progression|Chip")
void PreviewSlotAssignment(EPlayerCharacterClass CharacterClass, FName ChipId, int32 PreferredSlotIndex,
    TArray<int32>& OutSlotIndices) const;
```

구현 주의:
- `EquipChip()` 안에 있는 "선호 칸 → 앞에서부터 빈 칸" 배정 로직을
  **`private: bool ComputeAssignment(State, Chip, PreferredIndex, TArray<int32>& Out) const`** 로 뽑아내고
  `EquipChip()` / `PreviewSlotAssignment()` 가 **둘 다 그것만** 호출하게 한다. 로직 복제 금지.
- `SwapOrEquipChip()` 은 실패 시 되돌려야 하므로 `FDRClassUpgradeState` 사본을 떠서 작업하고
  성공했을 때만 커밋 + `SaveProgress()` 한다.
- `MoveChip()` 은 같은 칩을 대상으로 하는 재배치이므로 **`AlreadyEquipped` 검사를 건너뛰어야 한다.**
  (그냥 `UnequipChip` → `EquipChip` 순으로 부르면 자기 자신 때문에 실패하지 않지만,
  중간 상태에서 `SaveProgress()` 가 두 번 나가고 `OnUpgradesChanged` 가 두 번 발화한다.
  → **사본 작업 + 1회 커밋**으로 구현한다.)
- 세 함수 모두 성공 시에만 `SaveProgress()` + `OnUpgradesChanged.Broadcast(Class)` 를 **정확히 1회** 발화한다.

### 21.4 스탯 미리보기 (BP 노출)

`BuildUpgradeRuntime` / `BuildPreviewRuntime` 은 `UFUNCTION` 이 아니라 BP에서 못 쓴다. 래퍼를 추가한다.

```cpp
USTRUCT(BlueprintType)
struct FDRStatPreviewLine
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) EDRUpgradeStat Stat = EDRUpgradeStat::ContainerHealth;
    UPROPERTY(BlueprintReadOnly) FGameplayTag SkillTag;      // 비면 캐릭터 전역
    UPROPERTY(BlueprintReadOnly) FText Label;                // "컨테이너 체력" / "씨앗 대포 피해량"
    UPROPERTY(BlueprintReadOnly) float CurrentFlat = 0.f;
    UPROPERTY(BlueprintReadOnly) float CurrentPercent = 0.f;
    UPROPERTY(BlueprintReadOnly) float PreviewFlat = 0.f;
    UPROPERTY(BlueprintReadOnly) float PreviewPercent = 0.f;
    UPROPERTY(BlueprintReadOnly) FText DeltaText;            // "+12%" / "-15%"
    UPROPERTY(BlueprintReadOnly) bool bIsWorse = false;      // 빨간색 표기용
};

// AddChipId / RemoveChipId 는 NAME_None 이면 무시. 변화가 있는 항목만 담는다.
UFUNCTION(BlueprintCallable, Category = "Progression|Upgrade")
void GetStatPreview(EPlayerCharacterClass CharacterClass, FName AddChipId, FName RemoveChipId,
    TArray<FDRStatPreviewLine>& OutLines) const;
```

> **절대값(예: 최대 체력 400 → 480)은 이번 범위에서 하지 않는다.** 절대값을 내려면
> 클래스별 `PrimaryAttributes` GE 의 기본값을 UI가 읽어야 하는데, 그건 GE 커브/SetByCaller까지
> 해석해야 해서 비용이 크다. **증분(델타) 표기**로 충분하고, 절대값은 §15 확장 항목으로 남긴다.

### 21.5 `UDRUpgradeUILibrary` (신규 `UBlueprintFunctionLibrary`)

`Public/UI/DRUpgradeUILibrary.h`

```cpp
UCLASS()
class DAERUNE_API UDRUpgradeUILibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // 실패 사유 → 사용자 문구 (§20.5). Arg0/Arg1 은 필요 슬롯 수·비용 등 포맷 인자.
    UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
    static FText GetUpgradeResultText(EDRUpgradeResult Result, int32 Arg0 = 0, int32 Arg1 = 0);

    // 정식 표시명 ("컨테이너 체력"). 툴팁·상세 수치 문구용.
    UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
    static FText GetStatDisplayName(EDRUpgradeStat Stat);

    // 카드용 짧은 이름 ("컨테이너"). 한글 5자 이내 — 120px 카드 폭 제약(§22 STEP 5-5).
    // enum DisplayName 을 줄이지 않고 따로 둔 이유: 정식 명칭은 툴팁에서 그대로 필요하다.
    UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
    static FText GetStatShortName(EDRUpgradeStat Stat);

    // "1,250" 처럼 천 단위 구분 (현재 컬처 적용)
    UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
    static FText FormatCurrency(int32 Amount);

    // 칩 정의 → 슬롯 카드용 2행 짧은 라벨 ("ATK" / "+5").
    // 규칙: 첫 번째 비-단점 모디파이어를 대표로 삼고, Flat 은 정수, Percent 는 % 로.
    // 카탈로그의 EffectSummary 가 "ATK +5" 형태면 공백 기준으로 쪼개 그대로 쓴다.
    UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
    static void MakeShortLabel(const FDRUpgradeChipDefinition& Chip, FText& OutTop, FText& OutBottom);

    UFUNCTION(BlueprintPure, Category = "Upgrade|UI")
    static FText MakeModifierText(const FDRUpgradeModifier& Modifier);   // FDRUpgradeModifier::GetDetailText() 위임
};
```

> 모든 문구는 `#define LOCTEXT_NAMESPACE "DRUpgradeUI"` 안에서 `LOCTEXT()` 로 쓴다(Plan4 규칙).

### 21.6 `UDRUpgradeUIStyle` (신규 DataAsset)

`Public/UI/DRUpgradeUIStyle.h` — §18.2 팔레트 + 공용 브러시의 SSOT.

```cpp
UCLASS(BlueprintType)
class DAERUNE_API UDRUpgradeUIStyle : public UDataAsset
{
    GENERATED_BODY()
public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Color") FLinearColor TextPrimary;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Color") FLinearColor TextSecondary;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Color") FLinearColor TextDim;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Color") FLinearColor TextDisabled;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Color") FLinearColor AccentCyan;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Color") FLinearColor AccentCyanGlow;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Color") FLinearColor WarnRed;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Color") FLinearColor OkGreen;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chip")  float OverlayOpacityNormal   = 0.10f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chip")  float OverlayOpacityDisabled = 0.70f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chip")  float OverlayOpacityLocked   = 0.35f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chip")  TObjectPtr<UTexture2D> DefaultChipIcon;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chip")  TObjectPtr<UTexture2D> LockIcon;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Chip")  TObjectPtr<UTexture2D> CurrencyIcon;
};
```
- `UDRGameInstance` 에 `UPROPERTY(EditDefaultsOnly) TObjectPtr<UDRUpgradeUIStyle> UpgradeUIStyle;` 를 추가하고
  BP_GameInstance 에서 `DA_UpgradeUIStyle` 을 지정한다. 위젯은 GI에서 읽는다.

### 21.7 위젯 C++ 베이스

`Public/UI/Widget/DRUpgradeScreenWidget.h` / `DRUpgradeSlotWidget.h` / `DRUpgradeChipWidget.h`
— 전부 `UDRUserWidget` 상속(언어 변경 훅을 그대로 받기 위해).

```cpp
UCLASS(Abstract)
class DAERUNE_API UDRUpgradeScreenWidget : public UDRUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure, Category="Upgrade") EPlayerCharacterClass GetViewedClass() const;
    UFUNCTION(BlueprintPure, Category="Upgrade") UDRGameInstance* GetProgression() const;
    UFUNCTION(BlueprintCallable, Category="Upgrade") void RequestClose();   // → PC->CloseUpgradeScreen()

protected:
    virtual void NativeConstruct() override;    // GI 델리게이트 구독 + 최초 Refresh
    virtual void NativeDestruct() override;     // ★ 반드시 구독 해제 (GI 는 맵 전환에도 살아남는다)
    virtual FReply NativeOnKeyDown(const FGeometry&, const FKeyEvent&) override;  // ESC / M

    UFUNCTION(BlueprintImplementableEvent, Category="Upgrade") void OnRefreshSlots();
    UFUNCTION(BlueprintImplementableEvent, Category="Upgrade") void OnRefreshChips();
    UFUNCTION(BlueprintImplementableEvent, Category="Upgrade") void OnCurrencyUpdated(int32 NewCurrency);

private:
    UFUNCTION() void HandleUpgradesChanged(EPlayerCharacterClass CharacterClass);
    UFUNCTION() void HandleCurrencyChanged(int32 NewCurrency);
};
```

```cpp
UCLASS(Abstract)
class DAERUNE_API UDRUpgradeSlotWidget : public UDRUserWidget
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Upgrade")
    void SetSlotViewModel(const FDRSlotViewModel& InViewModel);

    UPROPERTY(BlueprintReadOnly, Category="Upgrade") FDRSlotViewModel ViewModel;

protected:
    UFUNCTION(BlueprintImplementableEvent, Category="Upgrade") void OnViewModelUpdated();
};
// UDRUpgradeChipWidget 도 FDRChipViewModel 로 동일한 형태
```

> **`NativeDestruct` 에서의 구독 해제가 특히 중요하다.** `UDRGameInstance` 는 레벨 전환에도 파괴되지 않으므로
> 해제를 빼먹으면 죽은 위젯이 델리게이트에 남아 다음 진입 때 크래시하거나 유령 갱신이 발생한다.

### 21.8 `ADRPlayerController` 보강

```cpp
// 이미 있음: OpenUpgradeScreen / CloseUpgradeScreen / OnUpgradeScreenOpened / OnUpgradeScreenClosed
// 추가:
UPROPERTY(EditDefaultsOnly, Category = "UI") TSubclassOf<UDRUpgradeScreenWidget> UpgradeScreenWidgetClass;
UPROPERTY(Transient)                          TObjectPtr<UDRUpgradeScreenWidget> UpgradeScreenWidget;
```
- 위젯 생성/제거는 **기존 BP 이벤트(`OnUpgradeScreenOpened/Closed`) 를 그대로 쓴다.**
  C++ 프로퍼티는 "BP가 만든 위젯을 여기에 보관"하는 용도(치트 콘솔/디버그에서 접근하기 위함).
- `SetInputMode(FInputModeUIOnly())` 는 이미 `OpenUpgradeScreen()` 에 있다. **BP에서 또 설정하지 말 것.**
- ⚠️ 현재 `OpenUpgradeScreen()` 은 `FInputModeUIOnly` 를 쓴다. 이 모드에서는 위젯이 포커스를 못 잡으면
  키 입력이 어디로도 안 간다 → BP `OnUpgradeScreenOpened` 에서 위젯 생성 직후
  **`SetUserFocus(PlayerController)`** 를 호출한다(§22 STEP 11).

### 21.9 §17 에 추가되는 작업 (M1' 확장)

| 파일 | 추가 변경 |
|---|---|
| `DRUpgradeTypes.h` | `EDRSlotState` 추가 · `EDRUpgradeResult::PreviousSlotLocked` 추가 · `WrongSlotType` 제거 |
| `DRProgressionTypes.h` | `FDRSlotViewModel` / `FDRStatPreviewLine` 추가 |
| `DRGameInstance.h/.cpp` | §21.2·§21.3·§21.4 API 추가, `ComputeAssignment()` 추출, `UpgradeUIStyle` 프로퍼티 |
| `DRChipCatalog.h/.cpp` | `SanitizeLoadout`/`ValidateCatalog` 단일 `MaxSlots` 시그니처 (§17.1-4) |
| 신규 | `UI/DRUpgradeUILibrary.h/.cpp`, `UI/DRUpgradeUIStyle.h`, `UI/Widget/DRChipDragDropOperation.h`, `UI/Widget/DRUpgrade{Screen,Slot,Chip}Widget.h/.cpp` |

### 21.10 구현 결과 ✅ (2026-08-16 완료)

> `DaeRune Win64 Development` 빌드 통과. **신규 파일 경고 0건** (기존 GAS deprecation 경고만 잔존).
> `DRChipCatalog` 시그니처는 M1' 에서 이미 정리돼 있어 이번 범위에서 손대지 않았다.

**작성한 파일**

| 파일 | 내용 |
|---|---|
| `Public/Game/DRUpgradeTypes.h` | `EDRSlotState` · `EDRUpgradeResult::PreviousSlotLocked` · `DRIsLowerBetter()` |
| `Public/Game/DRProgressionTypes.h` | `FDRSlotViewModel` · `FDRStatPreviewLine` · `UDRChipViewModelObject` |
| `Public/UI/DRUpgradeUIStyle.h` | 색 8종 + 오버레이 3종 + 아이콘 3종 (기본값 포함) |
| `Public/UI/DRUpgradeUILibrary.h` `Private/UI/DRUpgradeUILibrary.cpp` | 실패 문구 14종 · 스탯명 · 재화/수치/델타 포맷 · 짧은 라벨 |
| `Public/UI/Widget/DRChipDragDropOperation.h` | 드래그 페이로드 + `IsFromChipList()` |
| `Public/UI/Widget/DRUpgradeScreenWidget.h/.cpp` | 클래스 판정 · 델리게이트 구독/해제 · ESC/M · `RefreshAll()` |
| `Public/UI/Widget/DRUpgradeSlotWidget.h/.cpp` | 슬롯 뷰모델 + `IsDraggable()` |
| `Public/UI/Widget/DRUpgradeChipWidget.h/.cpp` | 칩 뷰모델 + `IsDraggable()` |
| `Game/DRGameInstance.h/.cpp` | §21.2·21.3·21.4 API + `ComputeAssignment()` 추출 + `UpgradeUIStyle` |
| `Player/DRPlayerController.h` | `UpgradeScreenWidgetClass` / `UpgradeScreenWidget` |

**계획과 달라진 점 (구현 중 내린 결정)**

1. **`PreviousSlotLocked` 를 enum 중간이 아니라 맨 끝에 추가했다.**
   `EDRUpgradeResult` 는 BP 에셋에 값으로 직렬화된다. 중간 삽입은 기존 값들의 번호를 밀어
   이미 저장된 BP 기본값의 의미를 바꿔 버린다. **결과 코드는 항상 끝에만 추가한다.**
2. **`ComputeAssignment()` 가 칩 정의가 아니라 `RequiredSlots`(int32) 를 받는다.**
   §21.3 은 `Chip` 을 넘기라고 했지만, `MoveChip` 은 **카탈로그에서 삭제된 고아 칩**도 옮길 수 있어야 한다
   (§17.3-4 에서 `UnequipChip` 의 `UnknownChip` 검사를 뺀 것과 같은 이유).
   → 칸 수 판정을 `GetChipSlotCount()` 로 분리했다. 정의가 있으면 `RequiredSlotCount`,
   없으면 **현재 점유 칸 수**로 폴백한다(이동해도 칸 수가 변하지 않게).
3. **`CanEquipChipAtSlot()` 은 점유 칸에도 `Success` 를 돌려준다.**
   §20.1 이 "점유 칸 드롭 = 교체"로 정의했으므로, 점유 칸은 **유효한 드롭 타깃**이다.
   내부적으로 "그 칩을 뺀 상태"를 가정해 배정이 되는지까지 확인하고 답한다
   → 드롭 하이라이트가 실제 결과와 어긋나지 않는다.
4. **`GetSlotUnlockCost(SlotIndex)` 는 0-base, `UDRProgressionConfig::GetSlotUnlockCost(SlotNumber)` 는 1-base다.**
   UI 는 칸 인덱스(0..5)로 생각하고 Config 는 "몇 번째 슬롯"(1..6)으로 생각한다.
   GameInstance 쪽에서 `SlotIndex + 1` 로 변환한다 — **off-by-one 이 나기 쉬운 유일한 지점**이다.
5-b. **`GetStatShortName()` 을 추가했다** (2026-08-16 UI 조정 중). 칩/슬롯 카드는 폭이 120~142px 라
   한글 5자가 한계인데 `EDRUpgradeStat` 정식 명칭에는 7자짜리가 있다.
   enum 의 DisplayName 을 줄이면 툴팁·상세 수치 문구까지 함께 짧아지므로, **카드 전용 짧은 이름만
   따로** 만들었다. `MakeShortLabel()` 이 이 함수를 쓴다.

5. **`DRIsLowerBetter()` 를 새로 만들었다.** §21.4 는 `bIsWorse` 필드만 정의하고 판정 기준을 정하지 않았다.
   받는 피해 / 물 소모 / 쿨다운은 **낮을수록 좋으므로** 부호 해석을 뒤집어야 한다.
   (이게 없으면 "받는 피해 +40%" 가 초록색으로 표시된다.)
6. **`MakeDeltaText()` / `FormatValue()` 를 라이브러리에 추가했다.** §21.5 목록에는 없지만
   `GetStatPreview()` 가 필요로 하고, 위젯도 같은 포맷을 써야 하므로 한곳에 뒀다.
7. **`MoveChip()` 의 무의미한 이동은 `Success`(무동작)로 처리한다.**
   같은 칸에 놓거나(제자리 드롭) 다중 칸 칩의 다른 칸에 놓는 경우다.
   에러로 처리하면 정상 조작에 실패 토스트가 뜬다. 저장/브로드캐스트도 발생하지 않는다.
8. **`HandleCurrencyChanged` 가 슬롯도 갱신한다.** 재화가 변하면 잠긴 칸의 해금 버튼 활성 상태가
   바뀌므로(`CanUnlockSlot` → `NotEnoughCurrency` ↔ `Success`) 재화만 갱신하면 UI가 어긋난다.
9. **위젯에 `IsDraggable()` 을 넣었다.** §20.2 는 이 조건을 BP 의사코드로만 적어 뒀는데,
   "미해금/장착됨은 못 든다" 는 규칙이 위젯마다 복제되지 않도록 C++ 로 올렸다.
10. **`GetUpgradeUIStyle()` 의 본체를 .cpp 로 내렸다.** 헤더에서 `TObjectPtr<전방선언 타입>` 을
    raw 포인터로 변환하는 인라인 함수는 컴파일러/버전에 따라 완전한 타입을 요구할 수 있다.

**M5a 완료 판정**

| 항목 | 결과 |
|---|---|
| UHT 리플렉션 생성 | ✅ 20개 파일 |
| 컴파일 + 링크 | ✅ 73/73, `DaeRune.exe` 생성 |
| 신규 파일 경고 | ✅ 0건 |
| 기존 API 회귀 | ✅ `EquipChip` 이 `ComputeAssignment` 를 쓰도록 바뀐 것 외 동작 변화 없음 |
| BP 에셋 참조 파손 | ✅ 없음 (UI 위젯이 아직 없어 참조하는 `.uasset` 이 없다) |

> ⚠️ **에디터 DLL 은 아직 갱신되지 않았다.** 검증은 게임 타깃(`DaeRune`)으로 했다
> — 빌드 시점에 에디터가 실행 중이라 Live Coding 이 에디터 타깃 빌드를 막았기 때문이다.
> 에디터에서 새 클래스를 쓰려면 **에디터를 재시작하거나 `Ctrl+Alt+F11`(Live Coding 컴파일)** 을 한 번 눌러야 한다.

---

## 22. 언리얼 에디터 제작 순서 (STEP BY STEP)

> 이 순서대로 하면 앞 단계의 산출물이 다음 단계의 입력이 된다. **순서를 바꾸면 참조가 끊긴다.**
> 각 STEP 끝에 ✅ 확인 항목이 있다. 통과 못 하면 다음으로 넘어가지 않는다.
> 소요 예상: STEP 0~4 = 코드 작업 반나절, STEP 5~10 = UMG 작업 2~3일, STEP 11~16 = 반나절.

### STEP 0. 선행 조건 확인

1. ~~**§17 의 M1'(통합 슬롯 전환)이 끝나 있는가?**~~ → **✅ 2026-08-15 완료.**
   확인 방법: `UDRGameInstance::GetMaxSlotCount()` 가 **인자 없이** 호출되면 완료다.
2. `DA_ProgressionConfig` 에 `MaxSlots = 6`, `SlotCosts` 6개가 들어 있는가? (없으면 STEP 13 을 먼저 부분 진행)
3. Visual Studio 로 `Development Editor` 빌드가 깨끗하게 통과하는가?
4. 작업 브랜치 확인: `feat/PlayExpo` 에서 `feat/UpgradeUI` 를 파고 시작한다.
   `.uasset` 은 병합이 불가능하므로 **UMG 작업 중에는 같은 위젯을 두 사람이 만지지 않는다.**

### STEP 1. 폴더 생성 & 텍스처 임포트

**1-1. 콘텐츠 폴더 생성** (Content Browser 우클릭 → New Folder)
```
Content/DaeRuneAssets/UI/Upgrade/
Content/DaeRuneAssets/UI/Upgrade/Material/
Content/Blueprints/UI/Upgrade/
```

**1-2. 텍스처 13장 드래그 임포트** → `Content/DaeRuneAssets/UI/Upgrade/`
- 파일명이 한글이므로 **임포트 후 §18.1 표의 `T_UI_Upg_*` 이름으로 즉시 리네임**한다.
  (한글 에셋명은 쿠킹/패키징에서 문제를 일으킬 수 있다.)

**1-3. 13장 전체 선택 → 우클릭 → Asset Actions → Bulk Edit via Property Matrix** 로 한 번에 설정
| 속성 | 값 |
|---|---|
| `Texture Group` | `UI` |
| `Compression Settings` | `UserInterface2D (RGBA)` |
| `sRGB` | ✓ |
| `Mip Gen Settings` | `NoMipmaps` |
| `Address X` / `Address Y` | `Clamp` |
| `Never Stream` | ✓ |
| `Filter` | `Default` (Bilinear) |

**1-4. 9-slice(Box) 대상 3장만 개별 설정** — 텍스처 자체가 아니라 **브러시** 설정이므로,
여기서는 기록만 해 두고 위젯에서 지정한다.
| 에셋 | Draw As | Margin (L,T,R,B) |
|---|---|---|
| `T_UI_Upg_SlotBadge` | `Box` | 0.28, 0.20, 0.06, 0.20 |
| `T_UI_Upg_ScrollThumb` | `Box` | 0.45, 0.10, 0.45, 0.10 |
| `T_UI_Upg_ScrollTrack` | `Box` | 0.45, 0.02, 0.45, 0.02 |

✅ **확인**: 13장 전부 `UserInterface2D`, 밉맵 없음. 에디터에서 확대해도 알파 경계가 뭉개지지 않는다.

### STEP 2. 폰트 확인 & 스타일 데이터 에셋

**2-1. 폰트** — 프로젝트에 이미 있다. 새로 만들지 않는다.
| 용도 | 에셋 |
|---|---|
| 영문 타이틀·탭·수치 | `Content/DaeRuneAssets/UI/Font/BrunoAceSC-Regular_Font` |
| 한글 본문 | `Content/DaeRuneAssets/UI/Font/Pretendard-SemiBold__1__Font` |
- 시안의 `UPGRADE SYSTEM` / `SPEED +3` 같은 영문 표기는 `BrunoAceSC`,
  `해금 조건:` 같은 한글은 `Pretendard`. **하나의 TextBlock 에 두 언어가 섞이면 Pretendard 로 통일**한다
  (BrunoAceSC 에 한글 글리프가 없어 두부(□)가 나온다 — 현지화 시 가장 자주 터지는 지점).

**2-2. `DA_UpgradeUIStyle` 생성**
- Content Browser 우클릭 → Miscellaneous → Data Asset → `DRUpgradeUIStyle` 선택
- 경로/이름: `Content/DaeRuneAssets/UI/Upgrade/DA_UpgradeUIStyle`
- §18.2 팔레트 값을 전부 입력. 색은 **Hex Linear 가 아니라 Hex sRGB** 칸에 넣는다.
- `DefaultChipIcon` / `LockIcon`(`T_UI_Upg_Chip_Lock`) / `CurrencyIcon`(`T_UI_Upg_Currency`) 지정.

**2-3. `BP_DRGameInstance` 에 연결** — `Upgrade UI Style` 슬롯에 `DA_UpgradeUIStyle` 지정.

✅ **확인**: `DA_UpgradeUIStyle` 을 열었을 때 색 12종 + 아이콘 3종이 채워져 있다.

### STEP 3. 머티리얼 (선택 — 시안 정확 재현이 필요할 때만)

> **1차 구현에서는 건너뛰어도 된다.** §18.4 / §18.6 의 "경로 A"로 먼저 만들고,
> 아트 확인 후 부족할 때 이 STEP 으로 돌아온다.

**3-1. `M_UI_Desat`** (칩 탈채도)
- Material Domain = `User Interface`, Blend Mode = `Translucent`
- 노드:
  ```
  TextureSampleParameter2D "Tex"
    ├─ RGB ─→ Desaturation(Fraction = ScalarParameter "Desat" 기본 0)
    │           └─→ Multiply(VectorParameter "Tint" 기본 (1,1,1,1)) ─→ Final Color
    └─ A   ─→ Multiply(ScalarParameter "Alpha" 기본 1) ─────────────→ Opacity
  ```
- 인스턴스: `MI_ChipBase_Normal`(Desat 0) / `MI_ChipBase_Gray`(Desat 1, Tint 0.92)
- 칩 아이콘처럼 **텍스처가 칩마다 다른** 곳은 위젯에서 `CreateDynamicMaterialInstance` → `SetTextureParameterValue("Tex", VM.Icon)`.

**3-2. `M_UI_SoftLight`** (전면 화면 효과, §18.6 경로 B)
- Material Domain = `User Interface`, Blend Mode = `Translucent`
- `TextureSampleParameter2D "Texture"` ← **이름을 반드시 `Texture` 로.**
  `RetainerBox` 가 이 이름의 파라미터에 렌더타깃을 꽂는다.
- `TextureSample T_UI_Upg_ScreenFX` → Blend
- 수식(Pegtop):
  ```
  Result = Lerp( Base*Base, Base, 2*Blend )      // == (1-2B)*Base² + 2B*Base
  Final  = Lerp( Base, Result, ScanA * Strength )
  ```
- `ScalarParameter "Strength"` 기본 1.0
- Opacity = 1.0 (RetainerBox 는 불투명 합성)

✅ **확인**: `MI_ChipBase_Gray` 를 이미지 브러시에 물렸을 때 시안 가운데 칩과 같은 회색이 나온다.

### STEP 4. C++ 클래스 추가 & 컴파일

에디터를 닫고 Visual Studio 에서 §21 의 파일을 만든다. **순서대로** 만들어야 컴파일이 한 번에 통과한다.

**4-1.** `Public/Game/DRUpgradeTypes.h` — `EDRSlotState` 추가, `EDRUpgradeResult` 에
`PreviousSlotLocked` 추가 / `WrongSlotType` 제거.
**4-2.** `Public/Game/DRProgressionTypes.h` — `FDRSlotViewModel`, `FDRStatPreviewLine`, `UDRChipViewModelObject`.
**4-3.** `Public/UI/DRUpgradeUILibrary.h/.cpp` — §21.5.
**4-4.** `Public/UI/DRUpgradeUIStyle.h` — §21.6.
**4-5.** `Public/UI/Widget/DRChipDragDropOperation.h`
```cpp
#pragma once
#include "CoreMinimal.h"
#include "Blueprint/DragDropOperation.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRChipDragDropOperation.generated.h"

/** 업그레이드 화면에서 칩을 끌 때의 페이로드. (Plan2.md 20.2) */
UCLASS(BlueprintType)
class DAERUNE_API UDRChipDragDropOperation : public UDragDropOperation
{
    GENERATED_BODY()
public:
    UPROPERTY(BlueprintReadWrite, Category = "Chip") FName ChipId;
    UPROPERTY(BlueprintReadWrite, Category = "Chip") EPlayerCharacterClass OwnerClass = EPlayerCharacterClass::Gardener;
    /** -1 = 우측 칩 목록에서 시작 / 0.. = 좌측 슬롯에서 끌어냄 */
    UPROPERTY(BlueprintReadWrite, Category = "Chip") int32 SourceSlotIndex = -1;
    UPROPERTY(BlueprintReadWrite, Category = "Chip") int32 RequiredSlotCount = 1;
    UPROPERTY(BlueprintReadWrite, Category = "Chip") bool bHasDrawback = false;
};
```
**4-6.** `Public/UI/Widget/DRUpgradeScreenWidget.h/.cpp`, `DRUpgradeSlotWidget.h/.cpp`, `DRUpgradeChipWidget.h/.cpp` — §21.7.
**4-7.** `Public/Game/DRGameInstance.h/.cpp` — §21.2 / §21.3 / §21.4 API 추가 + `ComputeAssignment()` 추출.
**4-8.** `Public/Player/DRPlayerController.h` — §21.8 프로퍼티 2개.

**4-9. 빌드** → 에디터 실행. `Hot Reload` 말고 **에디터를 끄고 풀 빌드**한다
(USTRUCT/UENUM 추가는 핫리로드에서 자주 깨진다).

✅ **확인**: BP 그래프에서 `Get Slot View Models`, `Can Equip Chip At Slot`, `Swap Or Equip Chip`,
`Move Chip`, `Preview Slot Assignment`, `Get Upgrade Result Text` 노드가 검색된다.

### STEP 4.5. UMG 공통 규칙 (STEP 5~10 전체에 적용 — 먼저 읽을 것)

> STEP 5 부터는 이 규칙을 전제로 쓴다. 여기를 건너뛰면 "표대로 넣었는데 화면이 시안과 다르다"가 된다.

#### 4.5-1. 위젯 이름 접두어

트리 그림과 표에 나오는 접두어는 **위젯 종류를 뜻한다.** 새 위젯을 만들 때도 이 규칙을 따른다.

| 접두 | 위젯 종류 | 예 |
|---|---|---|
| `Img_` | `Image` | `Img_ChipBase` |
| `Txt_` | `TextBlock` | `Txt_ChipName` |
| `Btn_` | `Button` | `Btn_Unlock` |
| **`HB_`** | **`HorizontalBox`** — 자식을 가로로 나열 | `HB_Currency` |
| **`VB_`** | **`VerticalBox`** — 자식을 세로로 나열 | `VB_Label` |
| `SB_` | `SizeBox` — **크기를 강제하는 껍데기** | `SB_Root` |
| `Overlay_` | `Overlay` — 자식을 겹쳐 쌓는 좌표계 | `Overlay_Root` |
| `Panel_` | `Overlay` 중에서도 **통째로 켜고 끄는 묶음** | `Panel_Filled` |
| `Canvas_` | `CanvasPanel` — 앵커/좌표로 자유 배치 | `Canvas_Root` |
| `Border_` | `Border` — 배경 + 안쪽 여백 | `Border_Bg` |
| `Grid_` | `UniformGridPanel` | `Grid_Slots` |
| `Wrap_` | `WrapBox` | `Wrap_Chips` |
| `Scroll_` | `ScrollBox` | `Scroll_Chips` |
| `Anim_` | 위젯 애니메이션(Animations 탭) | `Anim_Hover` |
| `WBP_` | Widget Blueprint 에셋 자체 | `WBP_UpgradeChip` |

#### 4.5-2. ★Overlay 안에서 위치를 잡는 법★ (가장 자주 틀리는 부분)

`Overlay` 는 자식을 **전부 같은 자리에 겹쳐 쌓는다.** 자식마다 위치를 따로 주지 않으면
**전부 좌상단에 몰리거나 칸 전체로 늘어난다.** 위치는 자식의 **Slot 설정 3개**로 정한다:

| Slot 설정 | 의미 |
|---|---|
| `Horizontal Alignment` | `Fill`(가로 꽉 채움) / `Left` / `Center` / `Right` |
| `Vertical Alignment` | `Fill`(세로 꽉 채움) / `Top` / `Center` / `Bottom` |
| `Padding (L, T, R, B)` | 정렬 기준점으로부터의 **여백** |

동작이 두 가지로 갈린다 — 이걸 모르면 Padding 값이 전혀 다르게 해석된다:

- **Alignment = `Fill`** → Padding 은 **네 변에서 안쪽으로 깎는 여백**이다.
  `Padding(10,105,10,50)` = 좌10·상105·우10·하50 을 깎은 나머지 영역.
  즉 120×228 카드에서 → x 10..110, y 105..178 영역이 된다.
- **Alignment = `Top`/`Left` 등** → 그 변에서 떨어뜨리는 **오프셋**이고,
  크기는 자식의 **Desired Size**(내용물 크기)로 정해진다.

> **이 문서의 규칙**: 위치는 **정렬 + 한쪽 방향 Padding** 으로만 준다.
> 예) 아이콘을 y=47 에 가로 중앙 배치 → `HAlign = Center` + `VAlign = Top` + `Padding(0, 47, 0, 0)`
> `Padding(30,47,37,131)` 처럼 네 값을 계산해 넣지 않는다 — 카드 크기를 조금만 바꿔도
> 네 값이 전부 어긋난다. 실제로 초안의 칩 아이콘 Padding `(30,47,31,131)` 은
> 우측 값이 틀려 있었다(120 − 30 − 53 = **37** 이 맞다). 정렬 방식으로 바꾸면 이 산수 자체가 사라진다.

**여러 텍스트를 세로로 쌓을 때**는 `VerticalBox` 를 쓰고, **박스 자체의 시작 y 를 Padding 으로** 준다.
박스 안의 각 줄은 위에서부터 차례로 쌓이므로, 줄 간격은 각 줄의 `Padding.Bottom` 으로 조절한다.
→ 텍스트가 카드 맨 위에 몰렸다면 **`VB_` 박스에 상단 Padding 을 주지 않은 것**이다.

#### 4.5-2-b. ★크기는 위젯 종류마다 정하는 곳이 다르다★

정렬이 `Fill` 이 아니면 위젯은 **자기 Desired Size** 로 그려진다. 그 Desired Size 를 정하는 방법이
위젯 종류마다 다르다 — **`Width/Height Override` 는 `SizeBox` 에만 있는 속성**이다.

| 위젯 종류 | 크기를 정하는 곳 | 비고 |
|---|---|---|
| `Image` | **`Appearance → Brush → Image Size (X, Y)`** | 이게 곧 위젯 크기다. `SizeBox` 불필요 |
| `TextBlock` | 없음 — **글자 내용에 맞춰 자동** | 폭 제한은 `Wrap Text At` 으로 |
| `Button` | 없음 — **자식 + `Style→Normal Padding`** 에 맞춰 자동 | |
| `HorizontalBox` / `VerticalBox` | **없음 — 자식들의 합에 맞춰 자동** | 크기를 강제하려면 `SizeBox` 로 감싼다 |
| `Overlay` | 없음 — **가장 큰 자식**에 맞춰 자동 | 〃 |
| `SizeBox` | `Width Override` / `Height Override` | 한쪽만 체크해도 된다 |

> ⚠️ **이 문서의 초안에는 `HB_SlotBadge (SizeBox 67×18)` 처럼 적힌 곳이 있었다. 잘못된 표기다.**
> `HorizontalBox` 에는 크기 속성이 없다. 아래 둘 중 하나여야 한다:
> - 안에 크기가 확정된 `Image` 가 있다 → **그 이미지의 `Brush Image Size` 가 곧 묶음 크기**다 (권장)
> - 내용과 무관하게 크기를 못 박아야 한다 → **`SizeBox` 로 감싸고 그 SizeBox 에 이름을 준다**(`SB_*`)
>
> 이 문서의 트리에서는 **크기가 필요한 곳에 `Image` 가 이미 있으므로 `SizeBox` 를 거의 쓰지 않는다.**
> 예외는 각 위젯의 **최상위 루트**(카드 전체 크기를 못 박아야 한다)와 탭 버튼의 높이뿐이다.

#### 4.5-3. 폰트 크기를 정하는 법

UMG 의 폰트 `Size` 는 대략 **글자 한 줄 높이 ≈ Size × 1.3 px** 다(한글 폰트 기준).
그리고 **한글 한 글자의 가로 폭 ≈ Size px** 다(영문 소문자는 그 절반쯤).

그래서 크기는 감이 아니라 **들어갈 영역에서 역산**한다:

```
세로: 폰트 Size ≤ (영역 높이 ÷ 줄 수) ÷ 1.3
가로: 폰트 Size ≤ 영역 너비 ÷ 한 줄에 넣을 한글 글자 수
```

> ⚠️ **시안(`최종칩.jpg`)의 글자 크기를 그대로 쓰면 안 된다.** 시안의 칩 카드는 313px 폭이고
> 실제 배치되는 카드는 **120px** 다(§18.3). 시안에서 재 온 pt 값은 **0.38배** 해야 맞는다.
> §18.4 / §18.5 의 초기 표에는 이 축소가 반영되지 않은 값이 있었다 —
> **STEP 5-5 / 6-5 의 폰트 표가 최종값**이고, 그쪽이 §18 보다 우선한다.

#### 4.5-4. 애니메이션을 만드는 이유 (공통 원칙)

이 화면의 애니메이션은 장식이 아니라 **말로 못 하는 상태를 눈으로 알리는 장치**다.
아래 3가지 질문에 답이 없는 애니메이션은 만들지 않는다.

| 질문 | 예 (`Anim_DropPulse`) |
|---|---|
| **무엇을 알리는가** | "이 칸에 지금 놓을 수 있다" |
| **없으면 어떻게 되는가** | 정지된 테두리는 배경 장식과 구분이 안 돼 어디 놓을지 모른다 |
| **누가 켜고 끄는가** | 화면 위젯의 `BeginDragHighlight` 가 켜고 `EndDragHighlight` 가 끈다 |

- 길이는 **0.1~0.2초**. 이보다 길면 조작이 굼떠 보인다(`Anim_Toast` 만 예외).
- `Loop` 는 **드래그 중 하이라이트에만** 쓴다. 그 외 반복 애니메이션은 시선을 계속 뺏는다.
- 켠 애니메이션은 **반드시 끄는 지점을 같이 만든다.** 끄는 쪽을 안 만들면 링/글로우가 화면에 남는다.

---

### STEP 5. `WBP_UpgradeChip` 제작 (가장 먼저 — 나머지가 이걸 참조한다)

**5-0. 이 카드가 최종적으로 어떻게 생겼는가** (120 × 228, 왼쪽 숫자 = y 좌표)

```
      0 ┌──────────────────┐  ← Img_ChipBase (카드 배경 텍스처)
     16 │      ▲ ▼ ▲       │     노치 마커 — 텍스처에 그려져 있다. 위젯 없음
     43 │   ┌──────────┐   │  ← 아이콘 박스(청록 코너)도 텍스처
     47 │   │  [아이콘] │   │  ← Img_ChipIcon  50×44  (중앙, y=47)
     96 │   └──────────┘   │
    105 │    이동 속도      │  ← Txt_ChipName   20pt  (VB_Text 1행)
    131 │       +6%        │  ← Txt_ChipValue  30pt  (VB_Text 2행)
    174 │   [▪ SLOT 1 ]    │  ← Panel_SlotBadge 61×13 (중앙, y=174)
    210 │  ║║║║║║║║║║║║    │     핀 스트립 — 텍스처에 포함. 위젯 없음
    228 └──────────────────┘
```

> **텍스처에 이미 그려져 있는 것**(노치 마커·아이콘 박스 테두리·핀 스트립)은 **위젯으로 만들지 않는다.**
> 위젯으로 만들어야 하는 건 **런타임에 값이 바뀌는 것**(아이콘·이름·수치·뱃지)뿐이다.

**5-1. 전체 하이어라키** (이 트리를 그대로 만들면 된다)

```
WBP_UpgradeChip                          부모 클래스: DRUpgradeChipWidget
└── SB_Root                  SizeBox        120 × 228
    └── Overlay_Root         Overlay        ← 모든 자식의 좌표계
        ├── Img_ChipBase     Image          Fill / Fill
        ├── Img_ChipIcon     Image          Center / Top    Pad(0, 47,0,0)
        ├── VB_Text          VerticalBox    Fill   / Top    Pad(10,105,10,0)
        │   ├── Txt_ChipName   TextBlock      Fill   Auto   Pad(0,0,0,-4)
        │   └── Txt_ChipValue  TextBlock      Fill   Auto
        ├── Txt_UnlockReq    TextBlock      Fill   / Top    Pad(8,110,8,0)
        ├── Panel_SlotBadge  Overlay        Center / Top    Pad(0,174,0,0)
        │   ├── Img_BadgeBg    Image          Fill / Fill
        │   └── Txt_SlotCount  TextBlock      Left / Center  Pad(20,0,0,0)
        └── Img_Overlay      Image          Fill / Fill
```

> **Hover 글로우 · 아이콘 박스 글로우 · 장착 리본은 만들지 않는다.**
> 초안에 있던 `Img_IconBoxGlow` / `Img_HoverGlow` / `Img_EquippedRibbon` 3개를 뺐다.
> - Hover 표현은 **카드 확대(`Anim_Hover`)** 만으로 충분하다.
> - 장착 상태는 **`Img_Overlay` 의 회색 처리**(Opacity 0.70)로 이미 구분된다.
>   다만 "장착됨"과 "빈 칸 부족으로 못 낌"이 **같은 회색으로 보인다** — 구분이 필요해지면
>   그때 리본을 되살리거나 툴팁 문구로 처리한다.

**Overlay 자식은 아래에 적힌 것이 위에 그려진다.** 위 순서 그대로 넣는다.

> 각 요소가 **무엇을 보여주는지**(데이터 출처·표시 조건)는 **§19.4** 표에 있다.

**5-2. 생성**
- `Content/Blueprints/UI/Upgrade/` 우클릭 → User Interface → Widget Blueprint
- 부모 클래스 선택 창에서 **`DRUpgradeChipWidget`** 선택 (기본 `UserWidget` 아님 — 뷰모델 함수가 사라진다)
- 이름 `WBP_UpgradeChip`
- 우상단 `Fill Screen` → **`Custom`** → `120 × 228` (디자이너 미리보기 크기일 뿐, 실행에는 영향 없음)
- 기본으로 들어 있는 `CanvasPanel` 을 **삭제**한다
  (Canvas 는 자식마다 앵커·오프셋을 따로 잡아야 해서 이런 고정 크기 카드에는 과하다)

**5-3. 크기 · 위치 · 브러시** (§4.5-2-b 대로 — `Image` 는 `Brush Image Size` 가 곧 크기다)

| 위젯 | 크기를 정하는 값 | 브러시 / 비고 |
|---|---|---|
| `SB_Root` | `Width Override` **120** · `Height Override` **228** | `Render Transform → Pivot (0.5, 0.5)` ★ |
| `Overlay_Root` | 없음(부모 SizeBox 가 결정) | |
| `Img_ChipBase` | 정렬 `Fill` — 카드 전체 | `T_UI_Upg_Chip_Base`, Draw As `Image` |
| `Img_ChipIcon` | **Brush Image Size (50, 44)** | 브러시 텍스처는 비워 둠(런타임 지정) |
| `VB_Text` | 폭: `Fill` + 좌우 Pad 10 → **100** / 높이: 자식 합 | — |
| `Txt_UnlockReq` | 폭: `Fill` + 좌우 Pad 8 → **104** | `Wrap Text At` **104** |
| `Panel_SlotBadge` | **자식 `Img_BadgeBg` 크기를 따라간다 → 61×13** | Overlay 는 크기 속성이 없다 |
| `Img_BadgeBg` | **Brush Image Size (61, 13)** | `T_UI_Upg_SlotBadge`, Draw As **`Box`**, Margin (0.28, 0.20, 0.06, 0.20) |
| `Img_Overlay` | 정렬 `Fill` | `T_UI_Upg_Chip_Overlay` |

★ `Pivot (0.5, 0.5)` 를 안 주면 Hover 확대가 좌상단 고정으로 커져서 카드가 오른쪽 아래로 밀린 것처럼 보인다.

> **`Panel_SlotBadge` 가 `HorizontalBox` 가 아니라 `Overlay` 인 이유**:
> 뱃지는 배경 이미지 **위에** 글자를 얹는 구조다. `HorizontalBox` 는 자식을 **옆으로 나열**하므로
> 배경과 글자가 나란히 놓여 버린다. 겹쳐 쌓아야 하니 `Overlay` 가 맞다.
> 그리고 Overlay 는 **가장 큰 자식(=`Img_BadgeBg` 61×13)** 크기를 그대로 물려받으므로
> 별도 `SizeBox` 가 필요 없다.

> **★위치 미세 조정은 픽셀 수치가 아니라 "텍스처에 그려진 자리"에 맞춘다.**
> 카드 배경 텍스처에는 **아이콘 박스**(청록 삼각형이 있는 사각형)와 **뱃지 자리**가 이미 그려져 있다.
> 위 Padding 값은 시작점일 뿐이고, 최종 확인은 눈으로 한다:
> - `Img_ChipIcon` 의 흰 사각형이 아이콘 박스 안에서 **상하좌우 여백이 같은가**
>   → 좌우가 다르면 `HAlign` 이 `Center` 가 아닌 것이다
> - `Panel_SlotBadge` 의 좌우 끝이 아이콘 박스의 좌우 끝과 **같은 세로선에 있는가**

**5-4. 텍스트 · 색 설정**

| 위젯 | 폰트 | 색 | 그 외 |
|---|---|---|---|
| `Txt_ChipName` | `Pretendard` **20** | `ChipName` | Justification `Center`, `Auto Wrap ✗`, `Is Variable ✓` |
| `Txt_ChipValue` | `BrunoAceSC` **30** | `ChipValue` | `Center`, `Is Variable ✓` |
| `Txt_UnlockReq` | `Pretendard` **14** | `AccentCyan` | `Center`, **`Auto Wrap ✓`**, 기본 `Collapsed`, `Is Variable ✓` |
| `Txt_SlotCount` | `BrunoAceSC` **10** | `ChipBadgeText` | `Is Variable ✓` |

> **`Txt_ChipName` 의 `Padding.Bottom` 은 `-4`** 로 준다(UMG 는 음수 Padding 을 허용한다).
> 20pt 글자의 실제 높이는 15px 인데 줄 상자는 26px 이라 아래쪽에 빈 공간이 남는다.
> 그대로 두면 이름과 수치 사이가 시안보다 두 배로 벌어진다.
>
> **`Txt_ChipName` 은 `Auto Wrap ✗` 다.** 켜면 긴 이름이 2줄이 되면서 수치를 아래로 밀어
> `Panel_SlotBadge` 를 침범한다. **넘치면 잘리는 쪽**을 택했다 — 레이아웃이 깨지는 것보다 낫다.
> 대신 카탈로그의 스탯 표시명을 **한글 5자 이내**로 유지해야 한다(5-5 참조).

**`Is Variable ✓` 가 필요한 위젯** (그래프에서 이름으로 접근하는 것들):
`Img_ChipIcon` · `Img_Overlay` · `Txt_ChipName` · `Txt_ChipValue` · `Txt_UnlockReq` ·
`Txt_SlotCount` · `VB_Text` · `Panel_SlotBadge`

**`Visibility` 초기값**

| 값 | 대상 | 이유 |
|---|---|---|
| `Hit Test Invisible` | `Img_ChipBase` · `Img_ChipIcon` · `Img_Overlay` | 마우스 입력을 먹지 않게 |
| **`Collapsed`** | `Txt_UnlockReq` | 해금된 칩에서는 `VB_Text` 와 **같은 자리(y105)** 를 쓰므로, 켜 두면 글자가 겹쳐 보인다 |
| `Visible` | 그 외 | |

> ⚠️ **`Hit Test Invisible` 을 빼먹으면 드래그가 안 된다.** 카드 위에 덮인 이미지가
> 마우스 입력을 먼저 먹어서 루트의 `OnMouseButtonDown` 이 아예 호출되지 않는다.
> 특히 **최상단의 `Img_Overlay`** 는 카드 전체를 덮으므로 반드시 확인한다.

> **이름만 봐서는 역할이 안 드러나는 2개**
> - `Txt_UnlockReq` — **미해금 칩** 전용 문구. `VB_Text` 와 **같은 자리**를 쓰고
>   둘 중 하나만 켜진다(그래서 기본값이 `Collapsed` 다).
> - `Img_Overlay` — 칩 표면 광택 + 비활성 회색 처리용 텍스처. **항상 최상단**이며 알파만 바뀐다.

**5-5. 폰트 크기 근거** (§4.5-3 의 역산 규칙 적용)

| 텍스트 | 영역 | 줄 수 | 세로 한계 | 가로 한계 | **채택** |
|---|---|---|---|---|---|
| `Txt_ChipName` | 폭 100 × 높이 26 | **1** | 26÷1.3 = **20** | 100÷20 = **한글 5자** | **20pt** |
| `Txt_ChipValue` | 폭 100 × 높이 39 | 1 | 39÷1.3 = **30** | "+100%" 6자 → 숫자는 폭 0.55배라 여유 | **30pt** |
| `Txt_UnlockReq` | 폭 104 × 높이 64 | 3~4 | 64÷3÷1.3 = **16** | "해금 조건: 슬롯 3칸" | **14pt** |
| `Txt_SlotCount` | 뱃지 안 41×13 | 1 | 13÷1.3 = **10** | "SLOT 1" 6자(영문) | **10pt** |

> **초안의 13pt / 24pt 는 너무 작았다.** 이름을 **2줄로 가정**하고 `34 ÷ 2 ÷ 1.3 = 13` 으로
> 역산한 것이 원인이다. 실제 시안(`speed`)은 1줄이라 훨씬 클 수 있다.
> 시안을 120px 카드로 환산하면 이름 ≈ 24pt / 수치 ≈ 30pt 이고, 한글 폭을 감안해 이름만 20pt 로 낮췄다.
>
> ⚠️ **이름은 한글 5자까지만 들어간다.** 카드에는 `DisplayName`("강화 컨테이너")이 아니라
> **`ShortLabelTop`/`ShortLabelBottom`**(짧은 스탯명 / "+20")을 쓴다(5-7 그래프).
>
> 짧은 스탯명은 **`UDRUpgradeUILibrary::GetStatShortName()`** 이 만든다 ✅(구현 완료).
> `EDRUpgradeStat` 의 DisplayName 자체는 **줄이지 않았다** — 그건 툴팁과 상세 수치 문구
> (`FDRUpgradeModifier::GetDetailText()`)에서 정식 명칭 그대로 필요하기 때문이다.
>
> | 정식 명칭 (툴팁) | 자수 | 짧은 이름 (카드) |
> |---|---|---|
> | `컨테이너 체력` | 7 | **`컨테이너`** |
> | `스킬 물 소모량` | 7 | **`물 소모`** |
> | `스킬 투사체 수` | 7 | **`투사체`** |
> | `스킬 피해량` | 6 | **`스킬 피해`** |
> | `스킬 딜레이` | 6 | **`쿨다운`** |
> | `최대 물` / `이동 속도` / `받는 피해` / `물 획득량` | ≤5 | 그대로 |
>
> 카탈로그에서 `EffectSummary` 를 `"ATK +5"` 형태로 직접 써 두면 그쪽이 우선한다(§21.5 규칙 1).
> 그래도 부족하면 §18.3 의 칩 그리드를 **4열 120×228 → 3열 150×285** 로 바꾼다(폭 130 확보).

**5-6. 애니메이션 2개 — 무엇을 알리는가**

| 애니메이션 | 알리는 것 | 없으면 | 켜고 끄는 곳 |
|---|---|---|---|
| `Anim_Hover` | "이 칩은 **집을 수 있다**" | 어떤 칩이 드래그 가능한지 커서만으로는 알 수 없다 | `OnMouseEnter` → Play / `OnMouseLeave` → Play(Reverse) |
| `Anim_PickUp` | "이 카드는 **지금 커서에 붙어 있다**" | 드래그 비주얼이 따로 뜨므로 **칩이 두 개로 보인다** | `OnDragDetected` → Play / 드래그 종료 → Play(Reverse) |

- `Anim_Hover` (0 → **0.12s**) — `SB_Root` → `Render Transform → Scale` : `1.0` → `1.04`
  - 글로우 이미지를 쓰지 않으므로 **트랙은 이 하나뿐**이다. 확대만으로 충분히 읽힌다.
  - ⚠️ **`IsDraggable()`(C++ 제공)이 false 면 재생하지 않는다.** 미해금·장착 완료 칩이
    집을 수 있는 것처럼 반응하면 거짓 정보다.
- `Anim_PickUp` (0 → **0.1s**) — `SB_Root` → `Render Opacity` : `1.0` → `0.35`

**5-7. Graph — 뷰모델 적용**

```
Event On View Model Updated  (부모 C++ 의 BlueprintImplementableEvent)
├─ Style = GetGameInstance → Cast<BP_DRGameInstance> → GetUpgradeUIStyle()
├─ Branch (ViewModel.bUnlocked)
│   ├ False — 미해금:
│   │   Img_ChipIcon.SetBrushFromTexture(Style.LockIcon)
│   │   VB_Text.Collapsed
│   │   Txt_UnlockReq.Visible
│   │     + SetText(Format("해금 조건: 슬롯 {0}칸", VM.RequiredSlotTier))
│   │   Panel_SlotBadge.Collapsed
│   │   Img_Overlay.SetColorAndOpacity(A = Style.OverlayOpacityLocked)   // 0.35
│   └ True — 해금됨:
│       Img_ChipIcon.SetBrushFromTexture( VM.Icon )   // 폴백은 C++ ResolveChipIcon() 이 이미 했다
│       VB_Text.Visible
│       Txt_ChipName.SetText(VM.ShortLabelTop)       // ★DisplayName 아님 — 5-5 참조
│       Txt_ChipValue.SetText(VM.ShortLabelBottom)
│       Txt_UnlockReq.Collapsed
│       Panel_SlotBadge.Visible
│         + Txt_SlotCount.SetText(Format("SLOT {0}", VM.RequiredSlotCount))
│       Img_Overlay.SetColorAndOpacity(
│           A = (VM.bEquipped || !VM.bCanEquipNow) ? Style.OverlayOpacityDisabled   // 0.70
│                                                  : Style.OverlayOpacityNormal )   // 0.10
└─ ToolTipWidgetDelegate 바인딩  (아래 주의)
```

> **여기서는 두 분기가 서로의 위젯을 다 끄고 켜도 안전하다** — 칩 카드는 갱신 때마다
> `Wrap_Chips.ClearChildren()` 후 **새로 만들어지므로**(STEP 9-9) 이전 상태가 남을 수가 없다.
>
> ⚠️ **슬롯은 다르다.** 슬롯 6칸은 디자이너에 **고정 배치되어 계속 재사용**되므로,
> 같은 위젯이 `Occupied → Empty` 를 오간다. 그래서 슬롯 그래프는 **맨 앞에서 전부 끄고 시작**해야
> 한다(STEP 6-8). 이 차이를 놓치면 **칩을 빼도 이름·수치가 그대로 남는다.**

#### `ViewModel` 은 어디서 오는가 — 데이터 흐름 전체

`ViewModel` 은 **BP 에서 만드는 변수가 아니다.** C++ 부모 클래스 `UDRUpgradeChipWidget` 의
public 멤버이고, 화면 위젯이 넣어 준다.

```
① DA_ChipCatalog                                      ← 디자이너가 채운 칩 정의(FDRUpgradeChipDefinition)
     │
② UDRGameInstance::GetChipViewModels()                ← 화면이 목록을 요청 (분류 인자 없음)
     │  └ MakeChipViewModel() 이 칩 1개 → FDRChipViewModel 1개로 변환
     │       bUnlocked = IsChipUnlocked(Class, ChipId)
     │                 = GetUnlockedSlotCount(Class) >= Chip.RequiredSlotTier   ★여기서 결정된다
     │
③ WBP_UpgradeScreen 의 Event On Refresh Chips (STEP 9-9)
     │  CreateWidget(WBP_UpgradeChip) → SetChipViewModel(VM) 호출
     │
④ C++ UDRUpgradeChipWidget::SetChipViewModel(InViewModel)
     │  { ViewModel = InViewModel;   ← ★멤버 변수에 저장★
     │    OnViewModelUpdated(); }    ← ★그 직후 BP 이벤트 호출★
     │
⑤ WBP_UpgradeChip 의 Event On View Model Updated      ← 우리가 지금 짜는 그래프
        ViewModel 을 읽어 위젯에 반영한다
```

**즉 `Event On View Model Updated` 가 불린 시점에는 `ViewModel` 이 이미 채워져 있다.**
이벤트에 인자로 넘어오지 않으므로 **변수를 직접 읽어야 한다.**

**BP 에서 `ViewModel` 을 꺼내는 법**
1. 좌측 `My Blueprint` 패널 → `Variables` → **`Upgrade`** 카테고리에 `View Model` 이 보인다
   (부모 C++ 클래스에서 상속된 변수라 회색으로 표시된다)
2. 그래프로 드래그 → `Get View Model`
3. 구조체이므로 **`Break FDRChipViewModel`** 노드를 붙이거나,
   핀에 우클릭 → **`Split Struct Pin`** 으로 필요한 필드만 꺼낸다

> `Get View Model` 을 매번 만들지 말고 **`Break` 노드 하나만 두고 여러 곳에 연결**한다.
> 구조체 복사가 여러 번 일어나지 않고 그래프도 읽기 쉽다.

#### 상태를 결정하는 bool 3개의 차이

셋 다 `FDRChipViewModel` 에 있고 **의미가 전부 다르다.** 헷갈리면 카드 상태가 뒤집힌다.

| 필드 | 뜻 | C++ 계산 근거 |
|---|---|---|
| **`bUnlocked`** | 이 칩을 **쓸 수 있게 열렸나** | `GetUnlockedSlotCount(Class) >= RequiredSlotTier` |
| **`bEquipped`** | **지금 끼워져 있나** | `SlotChips` 배열에 이 `ChipId` 가 있나 |
| **`bCanEquipNow`** | **지금 당장 낄 수 있나** | `CanEquipChip() == Success` (빈 칸 수까지 본다) |

`bUnlocked` 는 **재화나 빈 칸과 무관**하다. 오직 "슬롯을 몇 칸 해금했는가" 하나로 정해진다.
슬롯을 3칸 해금했고 칩의 `RequiredSlotTier` 가 3이면 → `bUnlocked = true`.
그 상태에서 빈 칸이 없으면 `bCanEquipNow` 만 false 가 된다.

**카드가 그려지는 4가지 조합**

| bUnlocked | bEquipped | bCanEquipNow | 카드 모습 | Overlay A |
|:---:|:---:|:---:|---|---|
| ✗ | — | — | 자물쇠 + `해금 조건: 슬롯 N칸` | 0.35 |
| ✓ | ✓ | — | 정상 카드지만 **회색** (이미 장착됨) | 0.70 |
| ✓ | ✗ | ✗ | 정상 카드지만 **회색** (빈 칸 부족 등) | 0.70 |
| ✓ | ✗ | ✓ | **정상** (집어서 끌 수 있다) | 0.10 |

> 2행과 3행이 **똑같이 회색**이다(§19.4 참고). 이유가 궁금하면 툴팁의 `Txt_BlockReason` 이
> `AlreadyEquipped` / `NotEnoughSlots` 로 구분해 알려 준다.

#### `Style` 을 얻는 법과 null 대비

```
Get Game Instance  →  Cast To BP_DRGameInstance  →  Get Upgrade UI Style
```
- `BP_DRGameInstance` 는 `UDRGameInstance` 를 상속한 프로젝트의 GameInstance BP 다.
- `Get Upgrade UI Style` 은 `UDRGameInstance` 의 `BlueprintPure` 함수다(§21.6).

> ⚠️ **`DA_UpgradeUIStyle` 을 GameInstance BP 에 지정하지 않으면 여기서 `None` 이 나온다**
> (Phase A-3 미완료 상태). 그러면 `Style.LockIcon` 등이 전부 null 이라 아이콘이 사라진다.
> `Is Valid` 로 한 번 걸러서, null 이면 색/아이콘 설정을 건너뛰고 **텍스트만 반영**하도록 짜면
> 에셋이 아직 없어도 그래프를 테스트할 수 있다.

#### 툴팁 바인딩

**`Set Tool Tip Widget` 을 쓰지 않는다.** 그러면 칩 20장마다 툴팁 위젯이 1장씩 **미리** 생성돼
낭비다. 대신 디테일 패널의 **`Behavior → Tool Tip Widget Delegate`** 에 함수를 바인딩한다
→ 실제로 마우스를 올렸을 때 **1장만** 만들어진다.

> ★**`GetChipTooltip()` 은 C++ 에 없다. 이 BP 에 직접 만드는 함수다.**★
> 이름도 자유다 — 델리게이트에 바인딩만 되면 된다. 다만 **시그니처는 고정**이다:
> **입력 없음 / 출력 `Widget` 타입 1개.** 이게 안 맞으면 디테일 패널의
> `Tool Tip Widget Delegate` 드롭다운에 아예 나타나지 않는다.
> (`My Blueprint → Functions → +` → 이름 `GetChipTooltip` → `Details → Outputs → +` → 타입 `Widget`)

```
함수 GetChipTooltip()  →  입력 없음 / 반환: Widget          ← ★직접 만드는 BP 함수★
├─ W = CreateWidget(Class = WBP_UpgradeTooltip, Owning Player = GetOwningPlayer())
├─ S = FindOwnerScreen()
├─ W.InitFromChip( ViewModel, IsValid(S) ? S->bShowDetails : false )
└─ return W
```

**여기 쓰인 것들의 출처**

| 이름 | 정체 | 어디서 오나 |
|---|---|---|
| `ViewModel` | **C++ 부모 변수** | `UDRUpgradeChipWidget::ViewModel` — 그대로 읽으면 된다 |
| `InitFromChip(VM, bShowDetails)` | **BP 함수** | `WBP_UpgradeTooltip` 에 만든다 (STEP 8-2) |
| `FindOwnerScreen()` | **C++ 함수** (`BlueprintPure`) | 아래 참조. 인자 없이 부르면 된다 |
| `bShowDetails` | **BP 변수** | `WBP_UpgradeScreen` 의 로컬 bool (기본 `false`) |

**`OwnerScreen` 은 ★변수가 아니라 C++ 함수★ 다** — `Find Owner Screen` 노드를 쓴다.

```
Find Owner Screen  (BlueprintPure, DefaultToSelf)   ← UDRUpgradeScreenWidget::FindOwnerScreen
   → 반환: WBP_UpgradeScreen
```

- `WBP_UpgradeChip` · `WBP_UpgradeSlot` 어디서든 **인자 없이** 끌어다 쓰면 된다
  (`Self` 가 자동으로 들어간다).
- **BP 변수를 만들지 않는다. 화면이 주입하지도 않는다.**

> ★**왜 주입 방식을 버렸는가**★ — `W.OwnerScreen = Self` 한 줄을 빠뜨리면 변수가 조용히 `None` 이
> 되고, 그 사실은 **드래그를 실제로 해 봐야** `Accessed None trying to read property OwnerScreen`
> 으로 드러난다. 칩(런타임 생성)과 슬롯(디자이너 배치)의 주입 시점이 서로 달라서 더 잘 틀린다.
> C++ 이 찾아 주면 잊을 수가 없다.
>
> **탐색 방식**(`DRUpgradeScreenWidget.cpp`):
> 1. 자신의 Outer 체인 — 디자이너에 배치된 **슬롯**은 화면의 `WidgetTree` 안이라 바로 잡힌다
> 2. 부모 위젯의 Outer 체인 — 런타임 생성된 **칩**은 `CreateWidget` 의 Owner 가 PlayerController 라
>    Outer 로는 못 찾는다. 부모 패널(`Wrap_Chips`)이 화면 소속이므로 한 단계만 올라가면 된다
>
> 드래그 중에도 **드래그 소스 위젯은 부모에 그대로 붙어 있다**(떠다니는 것은 `DragVisual` 이다).
> 그래서 `OnDragDetected` / `OnDrop` 시점에 항상 찾아진다.

> ⚠️ **`bShowDetails` 를 켜고 끄는 UI 는 아직 레이아웃에 없다.** 요구사항(§18.4 공통 규칙)에는
> "세부 정보 토글, 기본 OFF" 가 있지만 **시안에도 STEP 9 하이어라키에도 토글 위젯이 없다.**
> 지금 상태로는 항상 `false` 라 `VB_Detail` 이 영영 안 보인다. 둘 중 하나를 골라야 한다:
> - **(A) 토글을 뺀다** — 툴팁에 `DetailLines` 를 항상 표시. 시안과 일치하고 위젯이 안 늘어난다.
> - **(B) 토글을 넣는다** — 우측 패널 상단에 `CheckBox` 1개를 추가하고 `bShowDetails` 에 연결.
>
> 어느 쪽이든 **장점/단점(`VB_Benefit`/`VB_Drawback`)은 토글과 무관하게 항상 보인다**(§10.1).

**5-8. 디자이너 미리보기용 더미 (`Event Pre Construct`)**

`Pre Construct` 에서 `Txt_ChipName`/`Txt_ChipValue` 에 더미 문자열을 넣어 둔다.
런타임에는 `On View Model Updated` 가 즉시 덮어쓰므로 안전하고, **에디터에서 글자 넘침을
바로 볼 수 있다.** 가장 긴 경우로 넣는 게 요령이다 — 이름 `"컨테이너"`(5자), 수치 `"+100%"`.

✅ **확인**
- [ ] 흰 아이콘 사각형이 텍스처의 아이콘 박스 안에서 **상하좌우 여백이 같다**
      (좌우가 다르면 `Img_ChipIcon` 의 `HAlign` 이 `Center` 가 아니다)
- [ ] 이름과 수치가 **거의 붙어 있다** (벌어져 있으면 `Txt_ChipName` 의 `Padding.Bottom` 이 −4 가 아니다)
- [ ] 한글 5자 이름(`"컨테이너"`)이 **한 줄로** 들어간다
- [ ] `Panel_SlotBadge` 의 좌우 끝이 아이콘 박스의 좌우 끝과 **같은 세로선**에 있다
- [ ] `Img_Overlay` A 를 0.70 으로 바꾸면 카드가 회색으로 죽는다(Disabled 상태 확인)
- [ ] `Txt_UnlockReq` 만 켜고 `VB_Text` 를 끄면 Locked 상태가 시안 3번 칩처럼 보인다
      (**둘 다 켜면 같은 자리라 겹친다 — 정상이다.** 비교용으로만 함께 켠다)

### STEP 6. `WBP_UpgradeSlot` 제작

> ⚠️ **2026-08-17 재개정.** 슬롯 텍스처 원본 4장과 확정 시안을 직접 계측해 다시 썼다.
> **직전 개정의 "칩 WBP 를 축소해 넣는다"는 폐기**한다 — 장착 슬롯은 **빈 슬롯 텍스처 위에
> 아이콘과 텍스트를 직접 얹는다**(칩 카드를 중첩하지 않는다).
> 슬롯 크기도 시안의 실측값 **135.67 × 211.25 → `136 × 211`** 로 바로잡았다(기존 142×206).

**6-0. 슬롯 3상태와 텍스처가 이미 그려 주는 것**

슬롯은 텍스처 3장이 거의 모든 것을 그려 준다. **위젯은 런타임에 값이 바뀌는 것만** 얹는다.

| 상태 | 텍스처 | 텍스처가 이미 그려 주는 것 | 위젯으로 얹을 것 |
|---|---|---|---|
| `Empty` | `미창착슬롯.png` | 프레임 + 가운데 **`+`** | **없음** |
| `Occupied` | `장착슬롯.png` | 프레임 + **아이콘 박스**(청록 코너 삼각형) | 아이콘 · 이름 · 수치 · `SLOT n` 뱃지 |
| `Locked` | `잠긴슬롯.png` | 프레임 + **자물쇠** + **비용 박스**(오목한 사각형) | 재화 아이콘 · 비용 숫자 |

```
   Empty                Occupied              Locked          ← y 좌표 (136 × 211 기준)
 ┌────────────┐      ┌────────────┐      ┌────────────┐  0
 │            │      │  ┌──────┐  │      │            │  55  ← 아이콘 박스(텍스처)
 │            │      │  │[아이콘]│  │      │  [자물쇠]  │      (자물쇠도 텍스처)
 │     +      │      │  └──────┘  │      │            │  99
 │  (텍스처)   │      │   speed    │      │            │  105 Txt_StatName  16pt
 │            │      │    +3      │      │            │  126 Txt_StatValue  22pt
 │            │      │ [▪ SLOT 1] │      │ [▪ 100  ]  │  160 뱃지 / 비용 박스
 │            │      │            │      │            │
 └────────────┘      └────────────┘      └────────────┘  211
```

> **`Txt_SlotNumber`(01~06)는 만들지 않는다.** 시안의 어느 슬롯에도 칸 번호가 없다.
> 초안(§18.5)의 항목이었지만 실제 디자인에는 들어가지 않았다.
>
> **장착 슬롯의 `SLOT n` 은 칸 번호가 아니라 "이 칩이 먹는 칸 수"** 다(칩 카드의 뱃지와 같은 의미).
> 데이터는 `FDRSlotViewModel::GroupSize` 에서 온다.
> 현재 칩은 전부 1칸짜리라(§6.4) **항상 `SLOT 1` 로 찍힌다** — 시안과 같다.
> 상수로 박지 말고 `GroupSize` 를 그대로 읽어 둔다. 나중에 다중 칸 칩이 생겨도 고칠 게 없다.

**6-1. 전체 하이어라키**

```
WBP_UpgradeSlot                          부모 클래스: DRUpgradeSlotWidget
└── SB_Root                    SizeBox        136 × 211
    └── Overlay_Root           Overlay
        ├── Img_SlotBg         Image          Fill / Fill    ← 3텍스처 스왑 + 발광까지 담당
        │
        ├── Panel_Filled       Overlay        Fill / Fill    ← Occupied 일 때만 Visible
        │   ├── Img_ChipIcon     Image          Center / Top  Pad(0, 55,0,0)
        │   ├── VB_Label         VerticalBox    Fill   / Top  Pad(8,105,8,0)
        │   │   ├── Txt_StatName   TextBlock      Fill  Auto  Pad(0,0,0,-2)
        │   │   └── Txt_StatValue  TextBlock      Fill  Auto
        │   └── Panel_SlotBadge  Overlay        Center / Top  Pad(0,160,0,0)
        │       ├── Img_BadgeBg    Image          Fill / Fill
        │       └── Txt_SlotCount  TextBlock      Left / Center  Pad(18,0,0,0)
        │
        └── Panel_Locked       Overlay        Fill / Fill    ← Locked 일 때만 Visible
            ├── HB_Cost          HorizontalBox  Center / Top  Pad(0,164,0,0)
            │   ├── Img_CostIcon   Image          VAlign Center  Pad(0,0,5,0)
            │   └── Txt_Cost       TextBlock      VAlign Center
            └── Btn_Unlock       Button         Fill / Fill    ← 해금 클릭 전담
```

> **`Btn_Unlock` 이 `Panel_Locked` 안에 있는 이유** ★중요★:
> 버튼은 `Hit Test Invisible` 이 될 수 없어서, 슬롯 전체를 덮는 위치에 두면
> **마우스 다운을 삼켜 드래그가 시작되지 않는다.** 클릭이 필요한 건 잠긴 칸뿐이므로
> `Panel_Locked` 안에 넣어 **잠겼을 때만 존재**하게 한다.
> `Empty` / `Occupied` 에서는 버튼이 아예 `Collapsed` 라 `SB_Root` 가 마우스 이벤트를 온전히 받는다.
>
> **칩 카드(`WBP_UpgradeChip`)를 슬롯 안에 넣지 않는다.** 칩 카드에는 자체 배경·노치·핀 스트립이
> 있어서 슬롯 프레임과 이중으로 겹친다. 슬롯은 **자기 텍스처의 아이콘 박스 자리에 직접** 그린다.
>
> 각 요소가 **무엇을 보여주는지**(데이터 출처·표시 조건)는 **§19.3** 표에 있다.

**6-2. 생성**
- 부모 = **`DRUpgradeSlotWidget`**, 이름 `WBP_UpgradeSlot`
- 디자이너 크기 **`Custom 136 × 211`**, `CanvasPanel` **삭제**

> ⚠️ **가장 먼저 확인할 것 — 슬롯이 정사각형으로 보이면 크기 설정이 안 된 것이다.**
> 슬롯 텍스처는 **439 × 653**(세로로 긴 비율 0.672)이다. 정사각형으로 보인다면
> `SB_Root` 의 `Width Override 136` / `Height Override 211` 이 비어 있거나
> 디자이너 미리보기가 `Custom` 이 아닌 것이다. **텍스처가 가로로 늘어나 시안과 완전히 달라 보인다.**

**6-3. 크기 · 위치 · 브러시** (전부 시안 실측 → 136×211 환산값)

| 위젯 | 크기를 정하는 값 | 브러시 / 비고 |
|---|---|---|
| `SB_Root` | `Width Override` **136** · `Height Override` **211** | `Render Transform → Pivot (0.5, 0.5)` |
| `Img_SlotBg` | 정렬 `Fill` | Draw As `Image`. 텍스처는 런타임에 3종 스왑 |
| `Img_ChipIcon` | **Brush Image Size (44, 44)** | 텍스처의 아이콘 박스(44,55)51×44 안에 중앙 정렬 |
| `VB_Label` | 폭: `Fill` + 좌우 Pad 8 → **120** / 높이: 자식 합 | — |
| `Panel_SlotBadge` | **자식 `Img_BadgeBg` 크기를 따라간다 → 50×13** | Overlay 는 크기 속성이 없다 |
| `Img_BadgeBg` | **Brush Image Size (50, 13)** | `T_UI_Upg_SlotBadge`, Draw As **`Box`**, Margin (0.28, 0.20, 0.06, 0.20) |
| `HB_Cost` | **자동**(아이콘 17 + 여백 5 + 숫자) | 크기 속성 없음 |
| `Img_CostIcon` | **Brush Image Size (17, 13)** | `T_UI_Upg_Currency` |
| `Btn_Unlock` | 정렬 `Fill` | Style 의 **Normal · Hovered · Pressed · Disabled 4개 전부** `Draw As = None` |

> ⚠️ ★**버튼 브러시 4개를 전부 꺼야 한다**★ — `Btn_Unlock` 은 `Fill` 로 슬롯 전체를 덮으므로
> 브러시가 하나라도 살아 있으면 **그 상태일 때 칸 전체가 UMG 기본 사각형으로 덮인다.**
>
> | 안 끄면 생기는 증상 | 범인 |
> |---|---|
> | 해금 가능한 칸이 **평상시에 흰색** | Normal |
> | 마우스를 올리면 **칸 전체가 흰색** | **Hovered** |
> | 클릭하는 순간 번쩍임 | Pressed |
> | 뒤쪽 잠긴 칸에 회색이 깔림 | **Disabled** (`SetIsEnabled(false)` 상태) |
>
> **호버 피드백은 버튼 브러시가 아니라 슬롯 프레임으로 준다** — 6-6 의 발광을 재활용한다:
> ```
> Btn_Unlock.OnHovered   → Img_SlotBg.SetColorAndOpacity( (1.4, 1.7, 1.9, 1) )
> Btn_Unlock.OnUnhovered → Img_SlotBg.SetColorAndOpacity( (1, 1, 1, 1) )
> ```
> 드롭 발광 `(1.8, 2.4, 2.6, 1)` 보다 약하게 잡아 **"놓을 수 있는 칸"과 "누를 수 있는 칸"을 구분**한다.

**아이콘을 44×44 정사각형으로 두는 이유**: 텍스처의 아이콘 박스는 **51 × 44**(가로가 조금 길다)인데,
칩 아이콘 원본은 정사각 캔버스(`Locked 자물쇠.png` 가 192×192)다.
박스를 꽉 채우면 **가로로 늘어난다.** 박스 높이에 맞춘 정사각형이 왜곡이 없다.

**6-4. 텍스트 · 색 설정**

| 위젯 | 폰트 | 색 | 그 외 |
|---|---|---|---|
| `Txt_StatName` | `Pretendard` **16** | `ChipName` | `Center`, `Auto Wrap ✗`, `Padding.Bottom` **−2** |
| `Txt_StatValue` | `BrunoAceSC` **22** | `ChipValue` | `Center` |
| `Txt_SlotCount` | `BrunoAceSC` **9** | `ChipBadgeText` | "SLOT 1" |
| `Txt_Cost` | `BrunoAceSC` **18** | `TextSecondary` | 비용 숫자 |

**`Is Variable ✓`**: `Img_SlotBg` · `Img_ChipIcon` · `Txt_StatName` ·
`Txt_StatValue` · `Txt_SlotCount` · `Txt_Cost` ·
`Panel_Filled` · `Panel_Locked` · `Panel_SlotBadge` · `HB_Cost` · `Btn_Unlock`

**`Visibility` 초기값**

| 값 | 대상 | 이유 |
|---|---|---|
| **`Visible`** | `SB_Root` | ★`Hit Test Invisible` 이면 **드롭을 못 받는다** |
| `Hit Test Invisible` | `Img_SlotBg` · `Img_ChipIcon` · `Img_BadgeBg` · `Img_CostIcon` | 입력을 먹지 않게 |
| `Collapsed` | `Panel_Filled` · `Panel_Locked` | 상태에 따라 켜진다 |

**6-5. 폰트 크기 근거** (시안 실측 → 136×211 환산)

| 텍스트 | 시안 글자 높이 | 환산 em | **채택** | 가로 확인 |
|---|---|---|---|---|
| `Txt_StatName` | 11.4 (ascender 포함) | 11.4 ÷ 0.73 = **15.6** | **16pt** | 폭 120 ÷ 16 = 한글 **7자** |
| `Txt_StatValue` | 15.4 (숫자 cap) | 15.4 ÷ 0.70 = **22.0** | **22pt** | `"+100%"` 도 여유 |
| `Txt_SlotCount` | 5.4 | 5.4 ÷ 0.70 = **7.7** | **9pt** | 뱃지 50 × 13 |
| `Txt_Cost` | 13.9 (숫자 cap) | 13.9 ÷ 0.70 = **19.9** | **18pt** | 비용 박스 77 × 26 |

> **칩 카드보다 이름이 한 글자 더 들어간다.** 슬롯 텍스트 폭이 120 으로 칩 카드(100)보다 넓고
> 폰트가 16pt 라 **한글 7자**까지 한 줄에 들어간다 — `GetStatShortName()` 의 5자 제한 안에서 여유롭다.
>
> **세로 검산**: `VB_Label` 시작 105 + 이름 줄상자 21 − 2 + 수치 줄상자 29 = **153**.
> `Panel_SlotBadge` 가 160 에서 시작하므로 **7px 여유**가 남는다. ✅

**6-6. ★드롭 가능 슬롯 발광 처리★** (시안 1번 칸)

시안에서 드래그 중 놓을 수 있는 칸은 **프레임 전체가 밝게 빛난다.** 초안의
`Img_DropHighlight`(둥근 사각형 테두리)로는 이걸 만들 수 없다 — 슬롯 텍스처는 단순한 둥근 사각형이
아니라 **노치와 장식이 있는 복잡한 외곽선**이라 사각 테두리가 art 를 따라가지 못한다.

**슬롯 텍스처 자신을 밝히는 방식**으로 간다. 두 경로가 있다.

**경로 A — 무료 (1차 구현 권장)**
- 추가 위젯·머티리얼 **0개**. `Img_SlotBg` 의 `Color and Opacity` 만 애니메이션한다.
- 평상시 `(1, 1, 1, 1)` ↔ 발광 시 **`(1.8, 2.4, 2.6, 1)`**
- UMG 의 틴트는 곱셈이고 **1.0 을 넘는 값도 들어간다** → 청록 선화가 흰색에 가깝게 타오른다.
- 한계: 진짜 빛번짐(bloom)은 아니고 **선이 밝아지는** 수준이다.

**경로 B — 시안 정확 재현 (아트 확인 후 필요하면)**
- `Img_SlotBg` 바로 위에 **`Img_SlotGlow`** 를 한 장 더 깐다 (같은 텍스처, `Fill/Fill`, `Hit Test Invisible`).
- 신규 머티리얼 **`M_UI_SlotGlow`**:
  - `Material Domain = User Interface`, **`Blend Mode = Additive`**
  - `TextureSampleParameter2D "Tex"` → RGB × `VectorParameter "GlowTint"`(기본 `AccentCyanGlow`) → Final Color
  - Tex 의 A × `ScalarParameter "GlowStrength"` → Opacity
- 위젯에서 `CreateDynamicMaterialInstance` → 상태가 바뀔 때마다 `SetTextureParameterValue("Tex", 현재 슬롯 텍스처)`
- `Img_SlotGlow` 의 `Render Opacity` 를 0 ↔ 1 로 애니메이션 → **가산 합성이라 진짜로 빛나 보인다.**

> 두 경로 모두 **위젯 트리는 그대로**다(경로 B 는 이미지 1장 추가). 나중에 갈아타기 쉽다.
> §18.6 의 화면 효과와 같은 전략이다 — 싼 것부터 하고, 아트가 부족하다고 하면 올린다.

**6-7. 애니메이션 3개 — 무엇을 알리는가**

| 애니메이션 | 알리는 것 | 없으면 | 켜고 끄는 곳 |
|---|---|---|---|
| `Anim_DropPulse` | "**이 칸에 놓을 수 있다**" | 정지된 밝기는 원래 그런 칸인지 구분이 안 된다 | 화면의 `BeginDragHighlight` 가 켜고 `EndDragHighlight` 가 끈다 |
| `Anim_EquipPop` | "**방금 이 칸이 바뀌었다**" | 6칸이 통째로 다시 그려져 어디가 변했는지 놓친다 | 장착 성공 직후 해당 칸만 |
| `Anim_Deny` | "**이 칸이 거부했다**" | 토스트만으로는 어느 칸이 거부했는지 모른다 | 해금 실패 / 드롭 거부 시 |

- `Anim_DropPulse` — **6-5 의 발광을 켜고 끄는 애니메이션**이다. 0.6초 왕복, **`Loop` ✓**
  - 경로 A: `Img_SlotBg` → `Color and Opacity` : `(1,1,1,1)` ↔ `(1.8,2.4,2.6,1)`
  - 경로 B: `Img_SlotGlow` → `Render Opacity` : `0.35` ↔ `1.0`
  - **이 화면에서 `Loop` 를 쓰는 유일한 애니메이션**이다. 드래그 중에만 돌고 끝나면 멈춘다.
  - ⚠️ **정지 시 원래 값으로 되돌리는 것까지** `StopDropPulse()` 에 넣는다.
    애니메이션만 멈추면 **마지막 프레임의 밝은 색이 그대로 남는다.**
- `Anim_EquipPop` — `SB_Root` → `Render Transform Scale` `1.15` → `1.0`, **0.2s**, `Ease Out`
- `Anim_Deny` — `SB_Root` → `Render Transform Translation X` : `0 → +6 → -6 → +4 → 0`, **0.15s**

> 이 3개는 **화면 위젯이 호출해야** 하므로, `WBP_UpgradeSlot` 에 커스텀 이벤트 4개를 만들어 노출한다:
> `PlayDropPulse()` / `StopDropPulse()` / `PlayEquipPop()` / `PlayDeny()`
> 화면이 슬롯 내부 애니메이션 이름을 직접 알게 하면 슬롯 위젯을 고칠 때마다 화면도 고쳐야 한다.

**6-8. Graph — 뷰모델 적용**

뷰모델은 **`FDRSlotViewModel`** 이고, 부모 C++ 클래스 `UDRUpgradeSlotWidget` 의 `ViewModel` 변수에
화면 위젯이 `SetSlotViewModel()` 로 넣어 준다(칩 위젯과 같은 구조 — §22 STEP 5-7 의 데이터 흐름 참조).

> ★**상태는 3개, 패널은 2개다**★ — 이게 헷갈리기 쉬운 지점이다.
>
> | `EDRSlotState` | 배경 텍스처 | `Panel_Filled` | `Panel_Locked` | 위에 얹는 위젯 |
> |---|---|---|---|---|
> | `Occupied` (장착됨) | **`InsertedSlot`** | **Visible** | Collapsed | 아이콘 · 이름 · 수치 · 뱃지 |
> | `Empty` (장착 안 됨) | **`UnInsertedSlot`** | Collapsed | Collapsed | **없음** |
> | `Locked` (해금 안 됨) | **`LockedSlot`** | Collapsed | **Visible** | 재화 아이콘 · 비용 · 해금 버튼 |
>
> **`Empty` 에 전용 패널이 없는 이유**: 빈 칸의 중앙 `+` 기호는 **텍스처에 이미 그려져 있다.**
> 얹을 위젯이 하나도 없으므로 패널을 만들 필요가 없다 — **텍스처만 바꾸고 두 패널을 다 끄면 끝**이다.
> "패널이 2개니까 상태도 2개"가 아니다. 상태를 가르는 것은 **`Img_SlotBg` 의 텍스처**다.
>
> ⚠️ **`Empty` 분기에서 텍스처를 안 넣으면 그 칸만 흰 사각형으로 나온다.**
> `SetBrushFromTexture(None)` 은 에러를 내지 않고 **브러시 기본값인 흰색**을 그린다.

> ★★**분기 안에서 "끄기"를 하지 않는다 — 맨 앞에서 전부 끄고 시작한다**★★
>
> 슬롯은 **한 위젯이 6칸에 재사용**되고, 칩을 뺐다 꽂았다 하며 **같은 위젯이 상태를 오간다.**
> 각 분기가 "내가 켤 것 + 남이 켜 둔 것 끄기"를 모두 책임지면 **3분기 × 2패널 = 6곳**을
> 빠짐없이 맞춰야 하고, 하나라도 빠지면 **이전 상태가 화면에 남는다**
> (예: 칩을 해제했는데 이름·수치·뱃지가 그대로 남는 증상).
>
> 맨 앞에서 초기화하면 **각 분기는 자기가 켤 것만** 신경 쓰면 된다.

```
Event On View Model Updated
│
├─ ── ★공통 초기화 — 어떤 상태든 무조건 먼저 실행★ ──
│   Panel_Filled.SetVisibility(Collapsed)
│   Panel_Locked.SetVisibility(Collapsed)
│   SetRenderOpacity(1.0)
│
└─ Switch on ViewModel.State          ← EDRSlotState (Locked / Empty / Occupied) 3분기 전부 연결
    ├ Locked:
    │   Img_SlotBg.SetBrushFromTexture(LockedSlot)              ★
    │   Panel_Locked.SetVisibility(Visible)
    │   Txt_Cost.SetText(FormatCurrency(VM.UnlockCost))
    │   HB_Cost.SetVisibility(VM.UnlockCost >= 0 ? Visible : Collapsed)
    │   // 지금 살 수 있는 칸만 또렷하게. 그 뒤 칸은 흐리게
    │   if (!VM.bIsNextUnlockable) SetRenderOpacity(0.45)
    │   Btn_Unlock.SetIsEnabled(VM.bIsNextUnlockable)
    │
    ├ Empty:                                                    ← ★빠뜨리기 쉬운 분기★
    │   Img_SlotBg.SetBrushFromTexture(UnInsertedSlot)          ★
    │   (그 외에 켤 것이 없다 — 공통 초기화가 이미 다 껐다)
    │
    └ Occupied:
        Img_SlotBg.SetBrushFromTexture(InsertedSlot)            ★
        Panel_Filled.SetVisibility(Visible)
        ── 텍스처의 아이콘 박스 자리에 직접 그린다 ──
        Img_ChipIcon.SetBrushFromTexture(VM.ChipIcon)   // 폴백은 C++ ResolveChipIcon() 이 이미 했다
        Txt_StatName.SetText(VM.ShortLabelTop)         // "speed" / "이동 속도"
        Txt_StatValue.SetText(VM.ShortLabelBottom)     // "+3"
        Txt_SlotCount.SetText(Format("SLOT {0}", VM.GroupSize))
```

> **필요한 값이 전부 `FDRSlotViewModel` 에 이미 있다.** C++ 추가가 없다 —
> `ChipIcon` / `ShortLabelTop` / `ShortLabelBottom` / `GroupSize` /
> `UnlockCost` / `bIsNextUnlockable` 이 §21.1 에서 정의된 그대로 쓰인다.
>
> `GroupOrder` 는 **UI가 쓰지 않는다.** 칩이 전부 1칸짜리라 묶음 안 순번이라는 개념이 없다(§6.4).
> 구조체 필드는 그대로 두되(백엔드가 이미 채운다) 위젯에서는 읽지 않는다.
>
> `ShortLabelTop`/`Bottom` 은 칩 카드와 **같은 함수**(`MakeShortLabel` → `GetStatShortName`)로
> 만들어지므로, 같은 칩이 목록과 슬롯에서 **다른 글자로 보이는 일이 없다.**

> **`SetRenderOpacity` 를 쓰는 이유**: 자식마다 색 알파를 건드리면 상태가 바뀔 때마다
> 전부 되돌려야 한다. `Render Opacity` 는 **위젯 트리 전체에 한 번에** 걸리고 원복도 한 줄이다.

**6-9. Graph — 클릭 처리 (드래그 없이도 모든 조작이 되어야 한다)**

```
Btn_Unlock.OnClicked                      // 잠긴 칸에서만 존재한다
├─ R = GI->CanUnlockSlot(Class)
├─ if (R == Success):
│      GI->UnlockSlot(Class)              // 성공 → OnUpgradesChanged 가 화면 전체를 갱신
│      PlaySound(UI_Slot_Unlock)
└─ else:
       PlayDeny()
       FindOwnerScreen()->ShowToast( GetUpgradeResultText(R, VM.UnlockCost) )

Event On Mouse Button Down (SB_Root)      // 잠기지 않은 칸에서만 도달한다
└─ if (Key == RightMouseButton && State == Occupied):
       GI->UnequipSlot(Class, VM.SlotIndex)
       return Handled
   else:
       (좌클릭은 STEP 10 의 드래그 감지로 넘긴다)
```

> **왜 우클릭 해제와 클릭 해금이 필요한가**: 드래그앤드롭만 있으면 **게임패드로 조작할 수 없다.**
> §20.1 의 "더블클릭 장착 / 우클릭 해제 / 클릭 해금"이 그 대체 경로다.
> 드래그는 빠른 길일 뿐, **모든 조작이 클릭만으로도 가능해야 한다.**

✅ **확인** — 텍스처에 이미 그려진 자리와 위젯이 맞는지가 전부다

- [ ] 슬롯이 **세로로 긴 비율**로 보인다 (정사각형이면 `SB_Root` 크기 미설정 — 6-2 참조)
- [ ] `Img_ChipIcon` 이 텍스처의 **아이콘 박스(청록 코너 삼각형이 있는 사각형) 안**에 들어간다
      → 박스 밖으로 나가면 `Pad(0,55,0,0)` 을, 박스를 꽉 채우면 `Brush Image Size` 를 조정
- [ ] `Txt_StatName` / `Txt_StatValue` 가 아이콘 박스 **아래 빈 공간 가운데**에 온다
- [ ] `Panel_SlotBadge` 가 `Txt_StatValue` 와 프레임 하단 **사이**에 여유 있게 놓인다
- [ ] `Panel_Locked` 를 켜면 텍스처의 **오목한 비용 박스 안에** 재화 아이콘 + 숫자가 들어간다
      (박스는 `잠긴슬롯.png` 에 이미 그려져 있다 — 시안 3번 칸은 비어 있고 4·5·6번은 채워져 있다)
- [ ] `Img_SlotBg` 의 Tint 를 `(1.8,2.4,2.6,1)` 로 바꾸면 **프레임이 밝게 타오른다**(6-6 경로 A)
- [ ] `Render Opacity` 를 0.45 로 낮추면 "그 뒤 잠긴 칸" 모습이 된다

### STEP 7. ~~`WBP_UpgradeTabButton`~~ — **삭제됨**

우측 패널을 **분류 없는 단일 목록**으로 통합하기로 해서 탭 버튼 위젯이 필요 없어졌다(§18.4).

- `STATS` / `ASCENSION` 필터 탭 → **폐기**
- 패널 상단 노치의 `CHIPS` 는 화면 위젯 안의 **`TextBlock` 하나**(`Txt_ChipsTabLabel`)로 끝난다.
  좌측 패널의 `CHIP SLOTS` 와 같은 취급이다 — 누를 것이 없다.
- 칩 목록은 `GI->GetChipViewModels()`(분류 인자 없는 쪽)로 한 번에 받는다.

> **STEP 번호는 당기지 않는다.** 8·9·10 을 재번호하면 문서 곳곳의 `STEP 9-5` 같은 상호 참조가 전부 어긋난다.
>
> **C++ 은 손대지 않는다.** `GetChipViewModelsByCategory()` 와 `EDRChipCategory` 는 남긴다 —
> 카탈로그 검증(돌파 칩은 단점 필수, `DRChipCatalog.cpp`)이 `Category` 를 읽고 있고,
> 나중에 목록이 길어져 필터가 다시 필요해지면 그대로 꺼내 쓰면 된다. **UI 가 호출하지 않을 뿐이다.**

### STEP 8. `WBP_ChipDragVisual` / `WBP_UpgradeTooltip` / `WBP_UpgradeToast`

**8-1. `WBP_ChipDragVisual`** — 드래그하는 동안 커서를 따라다니는 반투명 칩

```
WBP_ChipDragVisual                       부모 클래스: UserWidget (★DRUpgradeChipWidget 아님)
└── SB_Root                  SizeBox      120 × 228
    └── Overlay_Root         Overlay
        ├── Img_ChipBase     Image        Fill / Fill
        ├── Img_ChipIcon     Image        Center / Top     Pad(0,47,0,0)
        ├── VB_Text          VerticalBox  Fill   / Top     Pad(10,105,10,0)
        │   ├── Txt_ChipName   TextBlock    Fill  Auto     Pad(0,0,0,2)
        │   └── Txt_ChipValue  TextBlock    Fill  Auto
        └── Img_Overlay      Image        Fill / Fill
```

- 만드는 법: `WBP_UpgradeChip` 을 **복제(Ctrl+W)** → 이름 변경 → **부모를 `UserWidget` 으로 되돌린다**
  (뷰모델/드래그 로직이 필요 없고, 오히려 있으면 이벤트가 중복 발화한다)
- **지울 것**: `Panel_SlotBadge` · `Txt_UnlockReq` · 모든 애니메이션
  (드래그 중에는 뱃지도 해금 문구도 볼 일이 없다)
- **추가할 것**: 없다. 아이콘 + 이름 + 수치면 "무엇을 들고 있는지"는 충분히 전달된다.

| 대상 | 설정 |
|---|---|
| `SB_Root` | 120 × 228 · `Render Opacity` **0.85** · `Render Transform Scale` **(0.9, 0.9)** |
| `SB_Root` `Visibility` | **`Hit Test Invisible`** ← ★★★ |

- 초기화 함수 하나만 만든다. ★**뷰모델 구조체를 받지 않는다**★:

```
함수 InitFromDrag( Icon : Texture2D,  TopLabel : Text,  BottomLabel : Text )
├─ Img_ChipIcon.SetBrushFromTexture( Icon )
├─ Txt_ChipName.SetText( TopLabel )
└─ Txt_ChipValue.SetText( BottomLabel )
```

> ★**왜 `FDRChipViewModel` 을 통째로 받지 않는가**★ — **드래그 소스가 두 곳**이기 때문이다.
>
> | 드래그 시작 위치 | 그 위젯이 가진 뷰모델 | 아이콘 필드 |
> |---|---|---|
> | `WBP_UpgradeChip` (우측 목록) | **`FDRChipViewModel`** | `Icon` |
> | `WBP_UpgradeSlot` (좌측 장착 칸) | **`FDRSlotViewModel`** | `ChipIcon` |
>
> **슬롯은 칩 뷰모델을 갖고 있지 않다.** `InitFromDrag(FDRChipViewModel)` 로 두면
> 슬롯에서 칩을 끌어낼 때 넘길 값이 없어서, `GetChipViewModels()` 로 목록을 통째로 받아
> `ChipId` 로 찾는 낭비를 매 드래그마다 해야 한다.
>
> 다행히 **드래그 비주얼이 실제로 쓰는 값은 3개뿐**이고, 두 뷰모델 모두 그 3개를 갖고 있다.
> 그래서 **원시 타입 3개로 받으면 양쪽이 그대로 넘길 수 있다** — C++ 추가도 필요 없다.
>
> | | 칩에서 | 슬롯에서 |
> |---|---|---|
> | `Icon` | `ViewModel.Icon` | `ViewModel.ChipIcon` |
> | `TopLabel` | `ViewModel.ShortLabelTop` | `ViewModel.ShortLabelTop` |
> | `BottomLabel` | `ViewModel.ShortLabelBottom` | `ViewModel.ShortLabelBottom` |
>
> 라벨 이름이 양쪽에서 같은 것은 우연이 아니다 — **같은 함수**(`MakeShortLabel`)가 만든다(§21.5).
> 그래서 같은 칩이면 목록에서 끌든 슬롯에서 끌든 **드래그 비주얼이 똑같이 보인다.**
>
> ★**아이콘 폴백은 BP 에서 하지 않는다**★ — C++ 의 `UDRGameInstance::ResolveChipIcon()` 이
> 뷰모델을 만들 때 이미 처리한다. 카탈로그에 `Icon` 이 없으면 `Style.DefaultChipIcon` 이
> 들어간 채로 넘어오므로, **`ViewModel.Icon` / `ViewModel.ChipIcon` 을 그대로 쓰면 된다.**
>
> 폴백을 위젯마다 따로 하면 **어떤 위젯은 기본 아이콘이 뜨고 어떤 위젯은 흰 사각형**이 되는
> 불일치가 생긴다(칩 카드는 폴백하는데 드래그 비주얼은 안 해서 흰색으로 나오던 문제).
> 카드·슬롯·드래그 비주얼·툴팁이 전부 같은 그림을 받게 하려면 한 곳에서 끝내야 한다.

> ⚠️ **복제 직후 반드시 정리해야 할 것이 있다.** `WBP_UpgradeChip` 을 복제한 뒤
> 부모를 `UserWidget` 으로 되돌리면, 원본에 있던
> **`Event On View Model Updated`(C++ `BlueprintImplementableEvent`)와 `ViewModel` 변수 참조가
> 전부 무효 노드로 남아 컴파일 에러**가 난다. 반드시:
> 1. `Event On View Model Updated` 이벤트 노드와 거기 붙은 그래프를 **통째로 삭제**
> 2. 위의 **`InitFromDrag(Icon, TopLabel, BottomLabel)`** 를 새로 만들고 인자를 직접 연결
> 3. `IsDraggable()` 같은 부모 함수 호출도 삭제 — 드래그 비주얼은 판정을 하지 않는다
>
> **부모를 되돌리는 이유**를 다시 확인해 두자: `UDRUpgradeChipWidget` 을 그대로 두면
> 이 위젯도 드래그 소스가 되어 **드래그 중에 또 드래그가 걸린다.**

> ⚠️ **`Hit Test Invisible` 을 빼먹으면 "드롭이 안 된다".** 드래그 비주얼은 커서 바로 아래에
> 붙어 다니므로, 입력을 받는 상태면 **자기 자신이 슬롯 대신 드롭을 가로챈다.**
> 슬롯의 `OnDragEnter` 조차 호출되지 않아서 원인을 찾기 어렵다. **드래그앤드롭 버그 1순위.**

**8-2. `WBP_UpgradeTooltip`** — 칩/슬롯 공용 상세 툴팁

```
WBP_UpgradeTooltip                       부모 클래스: DRUserWidget
└── SB_Root                    SizeBox      Min Desired Width 340 (Override 는 ✗)
    └── Border_Bg              Border       Padding 14
        └── VB_Content         VerticalBox
            ├── Txt_Name         TextBlock    Fill  Auto   Pad(0,0,0,4)
            ├── Txt_TargetSkill  TextBlock    Fill  Auto   Pad(0,0,0,6)
            ├── Txt_Summary      TextBlock    Fill  Auto   Pad(0,0,0,8)
            ├── Img_Separator    Image        Fill  Auto   Pad(0,0,0,8)
            ├── VB_Benefit       VerticalBox  Fill  Auto   Pad(0,0,0,4)
            ├── VB_Drawback      VerticalBox  Fill  Auto   Pad(0,0,0,4)
            ├── VB_Detail        VerticalBox  Fill  Auto   Pad(0,0,0,8)
            └── Txt_BlockReason  TextBlock    Fill  Auto
```

- 부모 `DRUserWidget`, 디자이너 크기 `Custom 340 × 260`(실행 시엔 내용에 따라 늘어난다)
- `SB_Root` — **`Width Override` 가 아니라 `Min Desired Width` 340**.
  Override 를 쓰면 내용이 길어져도 340 에 갇혀 글자가 잘린다. Min 은 "최소 340, 필요하면 더"다.
- `Border_Bg` — Draw As `Rounded Box`, Radius 6, Tint `#101823` A=0.94, Outline 1px `#3B5570`,
  `Padding 14` (Border 의 Padding 이 곧 내부 여백이다 — 자식에 또 주지 않는다)
- `Img_Separator` — **Brush Image Size (300, 1)**, Tint `#3B5570`. HAlign `Fill` 이라 폭은 늘어난다
- `VB_Benefit` / `VB_Drawback` / `VB_Detail` 은 **디자이너에서 비워 둔다.** 런타임에
  `TextBlock` 을 만들어 `AddChildToVerticalBox` 로 채운다(줄 수가 칩마다 다르다)
- 줄 간격은 각 줄의 **`Padding.Bottom`** 으로만 준다

| 줄 | 폰트 | 색 | 표시 조건 |
|---|---|---|---|
| `Txt_Name` | Pretendard **18** | `TextPrimary` | 항상 (`DisplayName`) |
| `Txt_TargetSkill` | Pretendard **13** | `TextSecondary` | 항상 ("적용 대상: 씨앗 대포" / "전체") |
| `Txt_Summary` | Pretendard **14** | `TextSecondary` | 항상 (`EffectSummary`) |
| `VB_Benefit` | Pretendard **13** | `OkGreen` | **항상** |
| `VB_Drawback` | Pretendard **13** | `WarnRed` | **항상** |
| `VB_Detail` | Pretendard **13** | `TextSecondary` | `bShowDetails` 일 때만 |
| `Txt_BlockReason` | Pretendard **13** | `WarnRed` | `EquipBlockReason != Success` 일 때만 |

> **`Txt_SlotInfo`("필요 슬롯 n칸 · 남은 칸 m")는 만들지 않는다.** 칩이 전부 1칸짜리라
> 필요 칸 수는 항상 1이고, 남은 칸 수는 왼쪽 6칸을 보면 바로 보인다.
> 칸이 모자랄 때는 `Txt_BlockReason` 이 `NotEnoughSlots` 문구로 이미 알려 준다.

- 함수 하나: `InitFromChip(FDRChipViewModel VM, bool bShowDetails)`
- **카드에 못 담은 `DisplayName` / `EffectSummary` 가 여기서 나온다**(STEP 5-5 참조).
  툴팁이 칩의 진짜 설명 자리다.
- `VB_Benefit` / `VB_Drawback` 은 **`bShowDetails` 와 무관하게 항상 채운다** — 돌파 칩의
  장단점은 숨기면 안 되는 정보라는 요구사항이다(§10.1). `VB_Detail` 만 토글에 반응한다.

**8-3. `WBP_UpgradeToast`** — 실패 사유를 알리는 짧은 알림

```
WBP_UpgradeToast                         부모 클래스: DRUserWidget
└── SB_Root            SizeBox    Min Desired Width 280 (Override 는 ✗)
    └── Border_Bg      Border     Padding 12
        └── Txt_Message  TextBlock  Fill / Center
```

- `Border_Bg` — Draw As `Rounded Box`, Radius 6, Tint `#101823` A=0.92
- `Txt_Message` — `Pretendard` **15**, `WarnRed`, `Auto Wrap ✓`, `Justification Center`, `Is Variable ✓`
- 함수 `Init(FText Message)` → 텍스트 설정 후 `PlayAnimation(Anim_Toast)`
- `SB_Root` `Visibility` = **`Hit Test Invisible`** — 토스트는 화면 아래쪽에 뜨는데,
  입력을 받으면 그 아래 있는 것을 클릭할 수 없게 된다

`Anim_Toast` (총 **2.5초** — 이 화면에서 가장 긴 애니메이션이고, 유일하게 0.2초를 넘긴다)

| 구간 | 시간 | 내용 |
|---|---|---|
| Fade In | 0 → 0.1s | Root Opacity 0 → 1, Translation Y +12 → 0 |
| 유지 | 0.1 → 2.1s | (키프레임 없음) |
| Fade Out | 2.1 → 2.5s | Opacity 1 → 0 |

- **왜 2.5초인가**: 실패 사유는 읽어야 하는 정보다. 0.2초짜리 연출로는 못 읽는다.
  반대로 계속 떠 있으면 조작을 가린다.
- 소멸: `Anim_Toast` 의 **`Animation Finished` 이벤트**에서 `RemoveFromParent()`.
  타이머보다 애니메이션 종료 이벤트가 안전하다(재생 속도를 바꿔도 어긋나지 않는다).

✅ **확인**
- [ ] 드래그 비주얼이 커서를 따라오고, **그 아래 슬롯의 `OnDragEnter` 가 발화한다**
- [ ] 툴팁이 마우스를 올렸을 때만 뜨고, 돌파 칩은 장단점이 항상 보인다
- [ ] 토스트가 2.5초 뒤 스스로 사라지고 `RemoveFromParent` 가 호출된다

### STEP 9. `WBP_UpgradeScreen` 제작

**9-1. 전체 하이어라키**

```
WBP_UpgradeScreen                            부모 클래스: DRUpgradeScreenWidget
└── Canvas_Root                  CanvasPanel
    ├── Img_Backdrop             Image          Anchor Full,        Offset 0
    ├── Txt_Title                TextBlock      Anchor TopLeft,     Pos(146, 36)   SizeToContent
    ├── HB_Currency              HorizontalBox  Anchor TopRight,    Pos(-194, 44)  Align(1,0)
    │   ├── Img_CurrencyIcon       Image          VAlign Center  Pad(0,0,12,0)
    │   └── Txt_Currency           TextBlock      VAlign Center
    │
    ├── SB_PanelLeft             SizeBox        Anchor TopLeft, Pos(99,65), Size 849×959
    │   └── Overlay_Left         Overlay
    │       ├── Img_PanelLeftBg    Image          Fill / Fill
    │       ├── SB_SlotsTab        SizeBox        Left / Top   Pad(73,74,0,0)  233×46
    │       │   └── Txt_SlotsTabLabel  TextBlock  Center / Center   ← ★노치 안 중앙★
    │       └── Grid_Slots         UniformGridPanel  Center / Center  Pad(73,120,74,75)
    │           ├── Slot_0 … Slot_2   WBP_UpgradeSlot   Row 0 / Col 0,1,2
    │           └── Slot_3 … Slot_5   WBP_UpgradeSlot   Row 1 / Col 0,1,2
    │
    ├── SB_PanelRight            SizeBox        Anchor TopLeft, Pos(823,67), Size 998×958
    │   └── Overlay_Right        Overlay
    │       ├── Img_PanelRightBg   Image          Fill / Fill
    │       ├── SB_ChipsTab        SizeBox        Left / Top   Pad(73,74,0,0)  183×44
    │       │   └── Txt_ChipsTabLabel  TextBlock  Center / Center   ← 〃
    │       ├── Scroll_Chips       ScrollBox      Fill / Fill  Pad(73,118,73,74)
    │       │   └── Wrap_Chips       WrapBox   Wrap Size 590 ★ ← 자식은 런타임 생성
    │       └── Txt_EmptyList      TextBlock      Center / Center
    │
    ├── WBP_KeyHintBar           (기존 WBP)     Anchor BottomRight, Pos(-65,-60) Align(1,1)
    │                                            ← 설정 화면이 쓰는 위젯을 그대로 끌어다 놓는다
    │                                              (ESC BACK / M EXIT — A·R 은 SetApplyVisible(false) 등으로 숨긴다)
    │
    ├── VB_Toasts                VerticalBox    Anchor BottomCenter, Pos(0,-140) Align(0.5,1)
    │                                            ← WBP_UpgradeToast 가 여기 추가된다
    └── Img_ScreenFX             Image          Anchor Full, Offset 0
```

> 각 요소가 **무엇을 보여주는지**(데이터 출처)는 §19.2 표에 있다. 이 트리는 **어떻게 만드는지**만 다룬다.

**9-2. 생성 / 캔버스 설정**
- 부모 = **`DRUpgradeScreenWidget`**, 이름 `WBP_UpgradeScreen`
- 디자이너 `Fill Screen`, `Desired Screen Size 1920 × 1080`
- 루트 `CanvasPanel` 은 **지우지 않는다** — 전체 화면 위젯은 앵커 배치가 맞다
  (카드 위젯과 반대다. 카드는 고정 크기라 Canvas 가 불필요했다)
- 루트 위젯 **`Is Focusable ✓`** ← ESC 키를 위젯에서 받으려면 필요하다

**9-3. CanvasPanel 슬롯 좌표 입력 요령**

| 위젯 | Anchor | Position | Size | Alignment |
|---|---|---|---|---|
| `Img_Backdrop` | Full (0,0)-(1,1) | Offset 0,0,0,0 | — | — |
| `Txt_Title` | TopLeft | (**149, 44**) | Size To Content ✓ | (0,0) |
| `HB_Currency` | TopRight | (**-210, 66**) | Size To Content ✓ | **(1, 0)** |
| `SB_PanelLeft` | TopLeft | (**99, 65**) | **849 × 959** | (0,0) |
| `SB_PanelRight` | TopLeft | (**823, 67**) | **998 × 958** | (0,0) |
| `WBP_KeyHintBar` | BottomRight | (**-65, -60**) | Size To Content ✓ | **(1, 1)** |
| `VB_Toasts` | BottomCenter | (0, -140) | Size To Content ✓ | **(0.5, 1)** |
| `Img_ScreenFX` | Full (0,0)-(1,1) | Offset 0,0,0,0 | — | — |

> ⚠️ **앵커를 먼저 잡고 Position 을 입력한다.** 순서를 바꾸면 Position 이 새 앵커 기준으로
> 자동 재계산돼 값이 틀어진다.
>
> ⚠️ **`Alignment` 를 빼먹으면 우측/하단 정렬이 어긋난다.** `Alignment` 는 "위젯의 어느 점을
> Position 에 맞출 것인가"다. 우측 정렬 요소는 `(1,0)` 이어야 위젯의 **오른쪽 끝**이
> Position 에 붙는다. 기본값 `(0,0)` 이면 왼쪽 끝이 붙어서 화면 밖으로 나간다.
>
> ⚠️ **`Img_Backdrop` / `Img_ScreenFX` 는 반드시 `Visibility = Hit Test Invisible`.**
> `Visible` 로 두면 화면 전체를 덮은 이미지가 **모든 클릭과 드래그를 먹는다.**
> `Img_ScreenFX` 는 트리 맨 끝(=최상단)이라 특히 치명적이다.

**9-4. 패널 내부 배치** (`Overlay_Left` / `Overlay_Right` 자식들)

패널 텍스처에는 테두리 장식이 있어서, 콘텐츠는 그 안쪽에만 놓아야 한다.
그 안쪽 여백을 **Overlay 자식의 Padding** 으로 준다(§18.3 의 콘텐츠 좌표가 곧 Padding 값이다).

| 위젯 | HAlign | VAlign | Padding (L,T,R,B) | 결과 영역 |
|---|---|---|---|---|
| `Img_PanelLeftBg` | Fill | Fill | 0 | 849×959 전체 |
| `SB_SlotsTab` (→ 라벨 Center) | Left | Top | **73, 74, 0, 0** | **233 × 46** |
| `Grid_Slots` | **Center** | **Center** | **73, 120, 74, 75** | 462 × 456 (영역 702×764 중앙) |
| `Img_PanelRightBg` | Fill | Fill | 0 | 998×958 전체 |
| `SB_ChipsTab` (→ 라벨 Center) | Left | Top | **73, 74, 0, 0** | **183 × 44** |
| `Scroll_Chips` | Fill | Fill | **73, 118, 73, 74** | 852 × 766 |
| `Txt_EmptyList` | Center | Center | 0 | 자동 |

> 초안에 있던 `SizeBox_LeftContent` / `SizeBox_RightContent` 는 **없앴다.**
> 여백만 주는 SizeBox 는 트리를 한 겹 깊게 만들 뿐, `Grid_Slots` 자체의 Padding 으로 똑같이 된다.

**9-5. `Grid_Slots`** — `UniformGridPanel`
- `Slot Padding` = `FMargin(13.5, 17, 13.5, 17)` — 인접 칸 사이 간격이 좌우 **27** / 상하 **34** 가 된다.
- `Min Desired Slot Width 136` / `Min Desired Slot Height 211`
  (자식 `WBP_UpgradeSlot` 의 `SB_Root` 가 이미 136×211 이라 안전장치 역할만 한다)
- 자식 6개: `WBP_UpgradeSlot` 을 드래그해서 넣고 Row/Column 을 (0,0)(0,1)(0,2)(1,0)(1,1)(1,2) 로 지정.
- 각 위젯 이름을 `Slot_0` … `Slot_5` 로. **`Is Variable ✓`**.
- 6칸은 **디자이너에서 미리 배치한다**(런타임 생성 X). `MaxSlots = 6` 고정이고,
  고정 배치해야 애니메이션·하이라이트에서 이름으로 참조할 수 있다.

**9-6. `Scroll_Chips`** — `ScrollBox`
| 속성 | 값 |
|---|---|
| `Orientation` | `Vertical` |
| `Scroll Bar Visibility` | `Visible` |
| `Always Show Scrollbar` | ✓ |
| `Scrollbar Thickness` | `18` |
| `Scrollbar Padding` | `(0,0,0,0)` |
| `Allow Overscroll` | ✗ |
| `Animate Wheel Scrolling` | ✓ |
| `Wheel Scroll Multiplier` | `1.5` |
| `Consume Mouse Wheel` | `When Scrolling Possible` |
| `Style → Widget Style` | Thumb 3종 = `T_UI_Upg_ScrollThumb` (Box, Margin 0.45/0.10) |
| 〃 | `Vertical Background Image` = `T_UI_Upg_ScrollTrack` (Box, Margin 0.45/0.02) |
| 〃 | `Normal/Hovered/Dragged Thumb Tint` = 흰색, Hover 시 살짝 밝게 |

**9-7. `Wrap_Chips`** — `WrapBox`
- `Explicit Wrap Width ✓` + `Wrap Width = 708`
- `Inner Slot Padding = (36, 40)` ← 이게 **칩 사이 간격**이다. 자식에 Padding 을 따로 주지 않는다.
  (`InnerSlotPadding` 은 각 슬롯의 우/하단에 적용되므로 4열 총 폭 = 4×(120+36) − 36 = **624 ≤ 708**,
   5번째는 780 > 708 이라 줄바꿈 → **정확히 4열**이 된다.)
- `Horizontal Alignment = Center`
- 디자이너에는 **자식을 넣지 않는다.** 런타임 생성.

**9-8. 애니메이션 — 무엇을 알리는가**

| 애니메이션 | 알리는 것 | 없으면 |
|---|---|---|
| `Anim_Open` | "**게임 화면에서 업그레이드 화면으로 전환됐다**" | 전체 화면 UI가 한 프레임에 튀어나와 눈이 따라가지 못한다 |

- `Anim_Open` (0 → **0.18s**)

| 트랙 | 0s | 0.18s |
|---|---|---|
| `Img_Backdrop` → Render Opacity | 0 | 1 (브러시 자체 A=0.55) |
| `SB_PanelLeft` → Translation X | **−24** | 0 |
| `SB_PanelRight` → Translation X | **+24** | 0 |
| `SB_PanelLeft` / `SB_PanelRight` → Render Opacity | 0 | 1 |

- 두 패널이 **바깥에서 안쪽으로** 모이는 방향이다. 좌우가 서로 반대 부호인 이유가 이것이다.
- `Anim_Close` 는 **만들지 않는다.** `Play Animation` 에 `Reverse` 를 넘겨 `Anim_Open` 을 되감는다.
  (닫기 연출을 따로 만들면 열기와 어긋날 때 두 곳을 고쳐야 한다)
- ⚠️ 닫기는 **연출이 끝날 때까지 기다리지 않는다.** `RequestClose()` 는 즉시 컨트롤러를 호출하고
  위젯이 제거된다. 되감기 연출을 보여주려면 BP 에서 `Play(Reverse)` → `Animation Finished` →
  `RequestClose()` 순으로 묶어야 한다(선택 사항).

**9-9. Graph — 초기화 & 리프레시**

먼저 **부모 C++ 이 무엇을 주는지** 확인한다. BP 가 만들 것과 이미 있는 것을 헷갈리면 안 된다.

| 이름 | 종류 | 누가 구현하나 | 설명 |
|---|---|---|---|
| `RefreshAll()` | `BlueprintCallable` **함수** | **C++ 이 구현** | 아래 3개를 순서대로 호출한다 |
| `Event On Refresh Slots` | `BlueprintImplementableEvent` | **BP 가 구현** | 좌측 6칸 다시 그리기 |
| `Event On Refresh Chips` | 〃 | **BP 가 구현** | 우측 목록 다시 그리기 |
| `Event On Currency Updated(int32)` | 〃 | **BP 가 구현** | 재화 숫자 갱신 |
| `GetViewedClass()` / `GetProgression()` / `GetUIStyle()` / `RequestClose()` | `BlueprintPure`·`Callable` | **C++ 이 구현** | 조회·닫기 |

> ⚠️ ★**`RefreshSlots()` / `RefreshChips()` / `RefreshCurrency()` 라는 함수는 없다**★ —
> 초안에 그렇게 적혀 있었지만 **존재하지 않는 이름**이다. 실제로는
> **`RefreshAll()` 하나**와, 그것이 호출하는 **BP 이벤트 3개**뿐이다
> (`DRUpgradeScreenWidget.h:52,61,65,68`).
>
> ⚠️ ★**`Event Construct` 에서 갱신을 부르지 않는다**★ —
> `NativeConstruct()` 가 **`Super` → 델리게이트 구독 → `RefreshAll()`** 순으로 이미 처리한다
> (`DRUpgradeScreenWidget.cpp:56~72`). BP `Event Construct` 는 그 `Super` 안에서 발화하므로
> **C++ 의 첫 `RefreshAll()` 보다 먼저** 실행된다. 여기서 또 부르면 **첫 프레임에 목록을 두 번 만든다.**

```
Event Construct        ← C++ NativeConstruct 의 Super 단계에서 발화한다
├─ PlayAnimation(Anim_Open)
└─ SetUserFocus(GetOwningPlayer())     ← FInputModeUIOnly 에서 키 입력을 받으려면 필수
                                          ※ 갱신 호출 없음 — 곧이어 C++ 이 RefreshAll() 을 부른다

Event On Refresh Slots   (C++ → BP)
└─ GetProgression()->GetSlotViewModels( GetViewedClass(), Slots )
   ForEachWithIndex → Slot_{i}.SetSlotViewModel(Slots[i])

Event On Refresh Chips
├─ Wrap_Chips.ClearChildren()
├─ GetProgression()->GetChipViewModels( GetViewedClass(), VMs )   // 분류 인자 없음 — 칩 전체
├─ Sort: bUnlocked ↓, bEquipped ↑             // 뒤의 Tier↑ / ChipId↑ 는 C++ 이 이미 정렬해 줬다
├─ ForEach VM:
│    W = CreateWidget(WBP_UpgradeChip, GetOwningPlayer())
│    W.SetChipViewModel(VM)
│    Wrap_Chips.AddChild(W)          // 간격은 InnerSlotPadding 이 처리 — 슬롯 Padding 손대지 않는다
└─ Txt_EmptyList.SetVisibility(VMs.Num()==0 ? Visible : Collapsed)

Event On Currency Updated (int32 New)
└─ Txt_Currency.SetText(FormatCurrency(New))   (+ 숫자 롤링 연출)

On Key Down (Geometry, KeyEvent) → 반환 Event Reply       ← ★Override 로 추가★
├─ Key = KeyEvent.GetKey()
├─ if (Key == Escape) → RequestClose()      ; return Handled   // C++ BlueprintCallable
├─ if (Key == M)      → (메인메뉴로 이동)    ; return Handled
└─ return Unhandled
```

> ★**키 처리는 전부 BP 가 한다**★ — `WBP_SettingsScreen` 과 같은 방식이다.
> 그 화면의 부모도 `DRUserWidget`(순수 베이스)이고, ESC/M 처리는 BP `OnKeyDown` 안에만 있다.
> `UDRUpgradeScreenWidget` 에도 **`NativeOnKeyDown` 오버라이드를 두지 않는다.**
>
> C++ 이 키를 먼저 가로채면 **BP 의 `OnKeyDown` 이 영원히 안 불린다**(BP 이벤트는
> `Super::NativeOnKeyDown` 안에서 발화하기 때문). 한쪽이 온전히 소유하는 편이 안전하다.
>
> **`A`(APPLY) 와 `R`(RESET)은 쓰지 않는다.** 키 힌트에서도 뺀다.
> 설정 화면은 변경을 대기(pending)시켰다가 A 에서 커밋하지만,
> 업그레이드는 **칩을 꽂는 즉시 확정**되는 설계라 "적용"이라는 단계가 없다.
> **정렬이 유일한 정리 수단이다.** 탭이 없어져 스탯 칩과 돌파 칩이 한 목록에 섞이므로,
> `bUnlocked ↓ → bEquipped ↑` 로 **지금 쓸 수 있는 칩을 맨 위**에 모으는 것이 중요해졌다.
> 뒤쪽 `RequiredSlotTier ↑ → ChipId ↑` 는 `UDRChipCatalog::GetChipsForClass()` 가 이미 적용해 준다 —
> BP 에서 다시 넣으면 중복이다. **안정 정렬(Stable Sort)** 을 써야 그 순서가 보존된다.
> **누가 언제 이 3개를 부르는가** — 전부 C++ 이 부른다. BP 는 **구현만** 한다.
>
> | 시점 | 경로 | 발화하는 이벤트 |
> |---|---|---|
> | 화면이 열릴 때 | `NativeConstruct()` → `RefreshAll()` | 3개 전부 |
> | 칩 장착/해제/이동 | `OnUpgradesChanged` → `HandleUpgradesChanged()` | Slots · Chips |
> | 재화 증감·슬롯 해금 | `OnCurrencyChanged` → `HandleCurrencyChanged()` | Currency · **Slots** |
>
> 재화가 바뀔 때 슬롯까지 다시 그리는 이유: **해금 가능해진 칸이 생기거나 사라지기 때문**이다
> (`bIsNextUnlockable` 이 바뀐다). C++ 이 이미 챙기므로 BP 에서 또 부르지 않는다.
>
> **BP 에서 `EquipChip()` 같은 걸 호출한 뒤 수동으로 갱신하지 않는다** — GameInstance 가
> `OnUpgradesChanged` 를 쏘고 그 경로로 자동 갱신된다. 손으로 또 부르면 이중 갱신이라
> 목록이 두 번 재생성되고 스크롤 위치가 튄다.
>
> 화면 전체를 강제로 다시 그려야 할 때만 **`RefreshAll()`** 을 쓴다(BP 에서 호출 가능).

✅ **확인**: 로비에서 화면을 열면 6칸 + 칩 목록이 시안 배치로 뜨고, 목록이 해금 → 미장착 순으로 정렬돼 있다.

### STEP 10. 드래그앤드롭 배선

**10-0. 먼저 — 여기서 쓰는 이름이 각각 어디서 오는가**

이 STEP 이 헷갈리는 이유는 **출처가 4군데**이기 때문이다. 표부터 보고 시작한다.

**(a) C++ 이 이미 주는 것 — 그냥 쓰면 된다**

| 이름 | 종류 | 정의 위치 | 시그니처 / 값 |
|---|---|---|---|
| `UDRChipDragDropOperation` | 클래스 | `DRChipDragDropOperation.h:22` | `UDragDropOperation` 파생. BP 에서 `Construct Object` 로 생성 |
| └ `ChipId` | 변수 `FName` | 〃`:28` | 끌고 있는 칩 |
| └ `OwnerClass` | 변수 `EPlayerCharacterClass` | 〃`:32` | 이 칩의 로봇 |
| └ `SourceSlotIndex` | 변수 `int32` | 〃`:36` | **−1 = 목록에서 시작 / 0.. = 그 칸에서 끌어냄** |
| └ `RequiredSlotCount` | 변수 `int32` | 〃`:40` | 항상 1 (§6.4) |
| └ `bHasDrawback` | 변수 `bool` | 〃`:43` | 단점 보유 |
| └ `IsFromChipList()` | `BlueprintPure` | 〃`:47` | `SourceSlotIndex < 0` |
| `ViewModel` | 변수 `FDRChipViewModel` | `DRUpgradeChipWidget.h:25` | **칩 위젯**의 부모 변수 |
| `ViewModel` | 변수 `FDRSlotViewModel` | `DRUpgradeSlotWidget.h:26` | **슬롯 위젯**의 부모 변수 |
| └ `ViewModel.SlotIndex` | `int32` | `DRProgressionTypes.h:269` | ★**칸 번호는 여기서 나온다**★ (별도 변수 만들지 말 것) |
| └ `ViewModel.State` | `EDRSlotState` | 〃`:272` | `Locked` / `Empty` / `Occupied` |
| └ `ViewModel.ChipId` | `FName` | 〃`:277` | 이 칸이 물고 있는 칩 |
| `IsDraggable()` | `BlueprintPure` | 칩 `.h:33` · 슬롯 `.h:30` | 칩 = `bUnlocked && !bEquipped` / 슬롯 = `State == Occupied` |
| `GetViewedClass()` | `BlueprintPure` | `DRUpgradeScreenWidget.h:35` | ★**`Class` 는 이 함수다**★ |
| `GetProgression()` | `BlueprintPure` | 〃`:39` | ★**`GI` 는 이 함수다**★ (`UDRGameInstance*`) |

**(b) `GetProgression()` 이 돌려주는 `UDRGameInstance` 의 함수들**

| 함수 | 정의 | 시그니처 |
|---|---|---|
| `CanEquipChipAtSlot` | `DRGameInstance.h:188` | `(Class, ChipId, SlotIndex, out Req, out Free) → EDRUpgradeResult` |
| `SwapOrEquipChip` | 〃`:194` | `(Class, ChipId, SlotIndex) → EDRUpgradeResult` |
| `MoveChip` | 〃`:199` | `(Class, FromIndex, ToIndex) → EDRUpgradeResult` |
| `UnequipChip` | 〃`:175` | `(Class, ChipId) → EDRUpgradeResult` |
| `UnequipSlot` | 〃`:179` | `(Class, SlotIndex) → EDRUpgradeResult` |
| `PreviewSlotAssignment` | 〃`:204` | `(Class, ChipId, PreferredIndex, out int32[]) → void` |

**(c) 내가 이 STEP 에서 새로 만드는 BP 함수 — 아직 아무 데도 없다**

| 만들 곳 | 이름 | 입력 | 출력 |
|---|---|---|---|
| `WBP_UpgradeScreen` | `BeginDragHighlight` | `Op : DRChipDragDropOperation` | — |
| `WBP_UpgradeScreen` | `EndDragHighlight` | — | — |
| `WBP_UpgradeScreen` | `HandleDropOnSlot` | `Op : DRChipDragDropOperation`, `TargetIndex : int32` | `bool` |
| `WBP_UpgradeScreen` | `ShowToast` | `Message : FText` | — |
| `WBP_UpgradeSlot` | `SetDragHint` | `Result : EDRUpgradeResult`, `bPreview : bool`, `Req : int32`, `Free : int32` | — |
| `WBP_UpgradeSlot` | `ClearDragHint` | — | — |
| `WBP_UpgradeSlot` | `PlayDropPulse` / `StopDropPulse` | — | — |

> - `PlayDropPulse` / `StopDropPulse` 는 **STEP 6-7 에서 이미 만들라고 한 것**이다. 없으면 지금 만든다.
> - `ShowToast` 는 **STEP 5-7(해금 실패 문구)에서 이미 호출**하고 있다. 거기까지 만들었다면
>   이미 있을 것이고, 없으면 여기서 만든다 — `VB_Toasts` 에 `WBP_UpgradeToast` 를 추가하는
>   한 줄짜리 함수다(STEP 8-3).

**(d) 이미 만들어 둔 BP 변수**

| 이름 | 있는 곳 | 타입 | 만든 STEP |
|---|---|---|---|
| — | 없음 | 화면 참조는 **`Find Owner Screen`** C++ 함수로 얻는다 (§22 STEP 5-7) | — |

---

**10-1. `WBP_UpgradeChip`** — 드래그 **소스** (§20.2)

`Graph → Override → On Mouse Button Down` / `On Drag Detected` 로 추가한다.

```
On Mouse Button Down (Geometry, PointerEvent) → 반환 Event Reply
├─ if ( !IsDraggable() ) → return Unhandled     // C++ 부모 함수 (칩 .h:33)
└─ return DetectDragIfPressed( PointerEvent, Key = Left Mouse Button )

On Drag Detected (Geometry, PointerEvent, out Operation)
├─ Op = Construct Object( Class = DRChipDragDropOperation, Outer = Self )
├─ Op.ChipId            = ViewModel.ChipId            ← C++ 부모 변수 (칩 .h:25)
├─ Op.OwnerClass        = FindOwnerScreen()->GetViewedClass()
├─ Op.SourceSlotIndex   = -1                          ← ★목록에서 시작★
├─ Op.RequiredSlotCount = ViewModel.RequiredSlotCount
├─ Op.bHasDrawback      = ViewModel.bHasDrawback
├─ DV = CreateWidget( WBP_ChipDragVisual, GetOwningPlayer() )
├─ DV->InitFromDrag( ViewModel.Icon,                  ← ★칩 뷰모델은 Icon★ (STEP 8-1)
│                    ViewModel.ShortLabelTop,
│                    ViewModel.ShortLabelBottom )
├─ Op.DefaultDragVisual = DV
├─ Op.Pivot             = Mouse Down                  ← Center Center 는 커서에서 튄다
├─ FindOwnerScreen()->BeginDragHighlight( Op )              ← 10-4 에서 만드는 BP 함수
└─ Operation = Op
```

**10-2. `WBP_UpgradeSlot`** — 드래그 **소스 겸 드롭 타깃**

슬롯은 **양쪽 다** 한다. 장착된 칩을 끌어낼 수도 있고, 칩을 받을 수도 있다.

```
── 소스 쪽 ──
On Mouse Button Down
├─ if ( !IsDraggable() ) → return Unhandled           // 슬롯 .h:30 (Occupied 일 때만 true)
└─ return DetectDragIfPressed( PointerEvent, Left Mouse Button )

On Drag Detected
├─ Op = Construct Object( DRChipDragDropOperation, Self )
├─ Op.ChipId          = ViewModel.ChipId              ← 슬롯 뷰모델 (DRProgressionTypes.h:277)
├─ Op.OwnerClass      = FindOwnerScreen()->GetViewedClass()
├─ Op.SourceSlotIndex = ViewModel.SlotIndex           ← ★이 칸에서 시작★ (〃:269)
├─ Op.RequiredSlotCount = ViewModel.GroupSize        // 이 칸의 칩이 먹고 있는 칸 수
├─ Op.bHasDrawback      = ViewModel.bHasDrawback
├─ DV = CreateWidget( WBP_ChipDragVisual, GetOwningPlayer() )
├─ DV->InitFromDrag( ViewModel.ChipIcon,              ← ★슬롯 뷰모델은 ChipIcon★ (〃:290)
│                    ViewModel.ShortLabelTop,                              (〃:284)
│                    ViewModel.ShortLabelBottom )                          (〃:287)
├─ Op.DefaultDragVisual = DV
├─ Op.Pivot             = Mouse Down
├─ FindOwnerScreen()->BeginDragHighlight( Op )
└─ Operation = Op

── 타깃 쪽 ──
On Drag Over  (Geometry, PointerEvent, Operation) → 반환 bool
└─ return true                                        ★false 면 OnDrop 이 오지 않는다★

On Drag Enter (Geometry, PointerEvent, Operation)
├─ Op = Cast<DRChipDragDropOperation>(Operation) ; if (Cast 실패) return
├─ GI = FindOwnerScreen()->GetProgression()
├─ R  = GI->CanEquipChipAtSlot( FindOwnerScreen()->GetViewedClass(),
│                               Op.ChipId, ViewModel.SlotIndex, Req, Free )
└─ if ( R == Success ) PlayDropPulse()                // STEP 6-7 의 BP 함수

On Drag Leave (PointerEvent, Operation)
└─ StopDropPulse()                                    ★밝기 원복까지 포함★

On Drop (Geometry, PointerEvent, Operation) → 반환 bool
├─ Op = Cast<DRChipDragDropOperation>(Operation) ; if 실패 return false
└─ return FindOwnerScreen()->HandleDropOnSlot( Op, ViewModel.SlotIndex )
```

**10-3. `WBP_UpgradeScreen`** — 해제 드롭 + 취소 정리

```
Scroll_Chips 의 On Drop        (우측 목록에 떨어뜨리면 = 해제)
├─ Op = Cast<DRChipDragDropOperation>(Operation) ; if 실패 return false
├─ if ( !Op.IsFromChipList() )                        ← C++ Pure (Op .h:47)
│      R = GetProgression()->UnequipChip( GetViewedClass(), Op.ChipId )
│      if (R != Success) ShowToast( GetUpgradeResultText(R) )
├─ EndDragHighlight()
└─ return true

루트의 On Drag Cancelled (PointerEvent, Operation)
└─ EndDragHighlight()      ★이걸 빼먹으면 발광이 화면에 남는다★
```

**10-4. 새로 만드는 BP 함수 3개의 내용**

```
함수 BeginDragHighlight( Op : DRChipDragDropOperation )
├─ Class = GetViewedClass() ; GI = GetProgression()
├─ GI->PreviewSlotAssignment( Class, Op.ChipId, -1, PreviewIndices )
└─ ForEach i in 0..5 :
       R = GI->CanEquipChipAtSlot( Class, Op.ChipId, i, Req, Free )
       Slot_{i}->SetDragHint( R, PreviewIndices.Contains(i), Req, Free )

함수 EndDragHighlight()
└─ ForEach i in 0..5 : Slot_{i}->ClearDragHint()

함수 HandleDropOnSlot( Op, TargetIndex : int32 ) → bool     // 본문은 §20.4
├─ Class = GetViewedClass() ; GI = GetProgression()
├─ if ( Op.OwnerClass != Class )               → EndDragHighlight(); return false   // 다른 로봇 칩
├─ if ( Op.SourceSlotIndex == TargetIndex )    → EndDragHighlight(); return true    // 제자리 드롭
├─ if ( Op.IsFromChipList() )  R = GI->SwapOrEquipChip( Class, Op.ChipId, TargetIndex )
│  else                        R = GI->MoveChip( Class, Op.SourceSlotIndex, TargetIndex )
├─ if ( R == Success ) PlaySound(ChipEquip)
│  else                PlaySound(Denied) ; ShowToast( GetUpgradeResultText(R) )
├─ EndDragHighlight()
└─ return true
```

> **`Slot_{i}` 는 무엇인가**: `WBP_UpgradeScreen` 의 디자이너에 **미리 배치한 6개 위젯**
> (`Slot_0` … `Slot_5`, STEP 9-1). 런타임 생성이 아니므로 이름으로 바로 참조된다.
> 반복문으로 돌리려면 `Event Construct` 에서 배열 변수 하나에 6개를 담아 두면 편하다.
>
> **갱신을 직접 부르지 않는다**: `SwapOrEquipChip` / `MoveChip` / `UnequipChip` 이 성공하면
> GameInstance 가 `OnUpgradesChanged` 를 쏘고 → C++ 이 `OnRefreshSlots` / `OnRefreshChips` 를
> 발화시킨다(STEP 9-9). 여기서 또 부르면 이중 갱신이다.

**`SetDragHint` / `ClearDragHint` 의 내용** (`WBP_UpgradeSlot`)

```
함수 SetDragHint( Result : EDRUpgradeResult, bPreview : bool, Req : int32, Free : int32 )
└─ Switch on Result:
     Success        → PlayDropPulse()                       // 청록 발광
     AlreadyEquipped→ /* 자기 자신이 낀 칸 — 아무 것도 안 한다 */
     NotEnoughSlots → SetRenderOpacity(0.4)
     그 외          → SetRenderOpacity(0.4)

함수 ClearDragHint()
├─ StopDropPulse()              ★발광 정지 + Img_SlotBg 틴트를 (1,1,1,1) 로 원복★
└─ SetRenderOpacity( ViewModel.State == Locked && !ViewModel.bIsNextUnlockable ? 0.45 : 1.0 )
```

> ⚠️ **`ClearDragHint` 가 원래 상태로 되돌리는 것까지 책임진다.** 애니메이션만 멈추면
> **마지막 프레임의 밝은 색이 슬롯에 그대로 남는다.** `Render Opacity` 도 마찬가지라,
> 그냥 1.0 으로 되돌리면 **"뒤쪽 잠긴 칸"의 0.45 흐림이 풀려 버린다** — 위처럼 뷰모델을 보고 복원한다.

**10-5. 자주 터지는 함정 체크**
| 증상 | 원인 |
|---|---|
| 드래그가 아예 시작되지 않는다 | 칩의 `OnMouseButtonDown` 이 `Unhandled` 반환 / 위에 `Visible` 인 이미지가 깔림 |
| 드롭이 안 먹는다 | 드래그 비주얼이 `Hit Test Invisible` 이 아님 (**1순위**) |
| 드롭이 안 먹는다 2 | `Img_ScreenFX` / `Img_Backdrop` 이 `Visible` |
| `OnDragEnter` 는 오는데 `OnDrop` 이 안 온다 | `OnDragOver` 에서 `false` 반환 |
| 링이 남는다 | `OnDragCancelled` 미구현 |
| 슬롯 클릭이 드래그를 막는다 | 버튼이 슬롯 전체를 덮고 있음 → `Btn_Unlock` 을 `Panel_Locked` 안으로 |
| 장착된 칩을 슬롯에서 못 끌어낸다 | `Panel_Filled` 안의 이미지/텍스트가 `Hit Test Invisible` 이 아님 |
| **해제했는데 칸에 이름·수치가 남는다** | `Empty` 분기가 `Panel_Filled` 를 안 끔 → **맨 앞에서 전부 끄기**(STEP 6-8) |
| 잠긴 칸이 흐려지지 않는다 / 계속 흐리다 | `SetRenderOpacity` 를 분기마다 원복하지 않음 → 공통 초기화에서 `1.0` 으로 |

✅ **확인**: §20.1 매트릭스 10줄을 하나씩 손으로 해 본다. 전부 의도대로 동작한다.

### STEP 11. PlayerController BP 연결

`BP_DRPlayerController`(또는 클래스별 파생 BP) 를 연다.

```
Event On Upgrade Screen Opened      (C++ BlueprintImplementableEvent)
├─ if (IsValid(UpgradeScreenWidget)) → return           // 중복 생성 방지
├─ UpgradeScreenWidget = CreateWidget(WBP_UpgradeScreen, Self)
├─ UpgradeScreenWidget->AddToViewport(ZOrder = 50)
└─ UpgradeScreenWidget->SetUserFocus(Self)              // ★ FInputModeUIOnly 대응

Event On Upgrade Screen Closed
├─ if (!IsValid(UpgradeScreenWidget)) → return
├─ UpgradeScreenWidget->PlayAnimation(Anim_Close)  (선택: 0.14s 뒤 제거)
├─ UpgradeScreenWidget->RemoveFromParent()
└─ UpgradeScreenWidget = null
```
- **`SetInputMode` / `SetShowMouseCursor` 를 BP 에서 다시 부르지 않는다.**
  `OpenUpgradeScreen()` / `CloseUpgradeScreen()` 이 C++ 에서 이미 처리한다(§8.4). 중복 호출하면
  설정창과 겹쳤을 때 커서가 사라진다.
- `ZOrder 50` — 로딩 화면(`UDRLoadingScreenSubsystem`)보다 낮고 HUD 보다 높게.

> ★**설정 메뉴와 완전히 같은 구조다**★ — 헷갈리면 `WBP_SettingsScreen` / `BP_DRPlayerController` 를 보면 된다.
>
> | 단계 | 설정 | 업그레이드 |
> |---|---|---|
> | 키 입력 | `WBP_SettingsScreen` 의 **BP `OnKeyDown`** | `WBP_UpgradeScreen` 의 **BP `OnKeyDown`** |
> | 닫기 요청 | `PC->CloseSettingsMenu()` | `PC->CloseUpgradeScreen()` |
> | 위젯 제거 | `Event On Settings Menu Closed` (BP) | `Event On Upgrade Screen Closed` (BP) |
> | C++ 이 하는 일 | 플래그 · 입력 모드 · BP 훅 호출 **뿐** | 〃 (+ 닫을 때 `ReportUpgradeLoadout()`) |
>
> **C++ 에는 `NativeOnKeyDown` 오버라이드가 없다.** 키는 전적으로 BP 소관이다 —
> C++ 이 먼저 가로채면 BP 의 `OnKeyDown` 이 발화하지 않는다(BP 이벤트는 `Super` 안에서 불린다).
>
> ⚠️ **`Event On Upgrade Screen Closed` 에서 `RemoveFromParent()` 를 빠뜨리면**
> ESC 를 눌러도 **창이 안 닫힌다.** 이때도 `CloseUpgradeScreen()` 은 정상 실행되므로
> **칩 효과는 적용되는데 화면만 남는** 모습이 된다 — 원인을 짚기 어려운 증상이다.

✅ **확인**: 로비에서 장치에 상호작용 → 화면이 뜨고 마우스가 나온다. ESC 로 **화면이 사라진다.**

### STEP 12. 로비 배치 & 장치

1. `BP_DRUpgradeStation` 이 없으면 생성: `ADRUpgradeStation` 상속 → `Content/Blueprints/Actor/`.
   - `StaticMesh` 지정, `BoxComponent` 범위 조정(반경 200 정도), `WidgetComponent` 에 프롬프트 위젯 지정.
   - `OnInteractBlocked` 이벤트에서 "스테이지 1을 클리어하면 열립니다" 표시.
2. `LobbyMap` 을 열고 `ADRStageSelectActor`(스테이지 포털) **옆에** 배치한다.
   `FreeRoam` 구역이어야 한다(요구사항 4: 클래스 선택 이후).
3. 레벨 저장.

✅ **확인**: 세이브를 지운 상태로 로비 진입 → 장치가 잠김 안내를 띄운다.
치트로 `UnlockUpgradeSystem()` 호출 후 → 화면이 열린다.

### STEP 13. 데이터 에셋 작성 (M6 과 겹침)

1. **`DA_ChipCatalog`** — §6.2 표대로 3로봇 분량 입력.
   - `RequiredSlotTier` 는 **1..6 스케일**(3차 개정).
   - `Icon` 에 칩 아이콘 텍스처 지정. 없으면 비워도 되고, UI가 `DefaultChipIcon` 으로 폴백한다.
   - `EffectSummary` 는 슬롯 카드의 짧은 라벨로도 쓰이므로 **`"ATK +5"` 처럼 `"이름 공백 값"` 형식**으로 쓴다
     (`MakeShortLabel()` 이 공백 기준으로 2행으로 쪼갠다).
   - 입력 후 `ValidateCatalog(6, Errors)` 1회 실행.
2. **`DA_ProgressionConfig`** — `MaxSlots = 6`, `SlotCosts = {400, 700, 1100, 1500, 1900, 2400}`,
   스테이지 보상/업적 입력 → **`ValidateCurrencyBudget()` 1회 실행 필수**.
3. `BP_DRGameInstance` 에 `ProgressionConfig` + `UpgradeUIStyle` 지정 확인.

✅ **확인**: 두 Validate 함수가 오류 0으로 통과한다.

### STEP 14. 입력 (M / ESC)

- `ESC`: `WBP_UpgradeScreen::NativeOnKeyDown` (C++) 에서 처리 → `RequestClose()` → `Handled`.
- `M`: 같은 곳에서 처리. 로비의 `M` 키가 다른 곳에 바인딩돼 있으면 화면이 열려 있는 동안
  `FInputModeUIOnly` 라 게임 입력이 안 가므로 충돌하지 않는다.
- **드래그 중 ESC**: `NativeOnKeyDown` 진입 전에 Slate 가 드래그를 처리하므로,
  `UWidgetBlueprintLibrary::CancelDragDrop()` 을 먼저 부르고 `EndDragHighlight()` 한 뒤 닫는다.

✅ **확인**: 드래그 도중 ESC → 드래그만 취소되고 화면은 남는다. 다시 ESC → 화면이 닫힌다.

### STEP 15. 현지화

1. 위젯의 **모든 고정 문구**를 디자이너에서 직접 입력한다(자동으로 로컬라이즈 대상이 된다).
2. C++ 문구(`GetUpgradeResultText`, `GetStatDisplayName`)는 `LOCTEXT_NAMESPACE "DRUpgradeUI"` 로 감싼다.
3. 칩 이름/요약/설명은 `DA_ChipCatalog` 의 `FText` 필드 → 에셋 로컬라이제이션으로 번역.
4. `Window → Localization Dashboard` 에서 **Gather** 실행 → `ko` / `en` 번역 입력 → **Compile**.
5. 한글 폰트 확인: 영문 전용 `BrunoAceSC` 를 쓴 TextBlock 에 한글이 들어가지 않는지 전수 점검.
   (Plan4 의 언어 SSOT / `OnLanguageChanged` 훅을 그대로 따른다.)

✅ **확인**: 게임 내 언어를 영어로 바꿔도 두부(□)가 없고, 칩 이름이 바뀐다.

### STEP 16. 테스트 (§14 에 추가되는 UI 항목)

**단일 플레이**
- [ ] 세이브 초기 상태: 6칸 전부 잠김, 1번 칸만 밝고 나머지는 0.45 흐림.
- [ ] 재화 부족 상태에서 1번 칸 클릭 → `NotEnoughCurrency` 토스트, 흔들림 연출.
- [ ] 해금 → 재화 숫자가 줄고, 칸이 `Empty` 로 바뀌고, **우측 칩 목록의 해금 칩이 늘어난다.**
- [ ] 스탯 칩을 6칸 아무 데나 드롭 → 전부 성공(3차 개정 핵심).
- [ ] 돌파 칩을 6칸 아무 데나 드롭 → 전부 성공.
- [ ] 6칸을 전부 채운 뒤 칩을 하나 더 드롭 → `NotEnoughSlots` 토스트, 배치는 그대로.
- [ ] 슬롯 → 슬롯 드래그로 위치 교환.
- [ ] 슬롯 → 우측 목록 드래그로 해제.
- [ ] 미해금 칩은 드래그가 시작되지 않는다.
- [ ] 화면을 닫았다 다시 열어도 배치가 그대로다(세이브 라운드트립).
- [ ] 게임 재시작 후에도 그대로다.
- [ ] 스탯 칩과 돌파 칩이 **한 목록에 섞여** 나오고, 해금된 미장착 칩이 맨 위에 모인다.
- [ ] 장착/해제로 목록이 재생성돼도 스크롤 위치가 튀지 않는다.

**멀티플레이 (PIE 2인, 리슨 서버)**
- [ ] 클라이언트에서 화면을 열고 칩을 바꾼 뒤 닫음 → 스테이지 진입 시 그 클라이언트만 수치가 바뀐다.
- [ ] 호스트도 동일하게 동작한다.
- [ ] 스테이지 진행 중 `M` 을 눌러도 화면이 열리지 않는다(요구사항 13, `IsInLobby()` 가드).

**회귀**
- [ ] 화면을 여러 번 열고 닫아도 메모리가 늘지 않는다(`NativeDestruct` 구독 해제 확인).
- [ ] 로비 → 스테이지 → 로비 이동 후에도 화면이 정상 동작한다(GI 델리게이트 중복 구독 없음).
- [ ] `Img_ScreenFX` 가 클릭/드래그를 가로채지 않는다.

---

### 22.17 작업 순서 요약 (한 줄 체크리스트)

```
[ ] STEP 0  M1' 완료 확인 · 브랜치 분기
[ ] STEP 1  폴더 3개 + 텍스처 13장 임포트/리네임/일괄 설정
[ ] STEP 2  폰트 확인 + DA_UpgradeUIStyle 생성 + GI 연결
[ ] STEP 3  (선택) M_UI_Desat / M_UI_SoftLight
[ ] STEP 4  C++ 8묶음 추가 → 풀 빌드
[ ] STEP 5  WBP_UpgradeChip
[ ] STEP 6  WBP_UpgradeSlot
[–] STEP 7  (삭제됨 — 우측 패널 통합으로 탭 버튼 불필요)
[ ] STEP 8  WBP_ChipDragVisual / Tooltip / Toast
[ ] STEP 9  WBP_UpgradeScreen (레이아웃 + 리프레시 그래프)
[ ] STEP 10 드래그앤드롭 배선 + 함정 체크
[ ] STEP 11 BP_DRPlayerController 연결
[ ] STEP 12 BP_DRUpgradeStation 로비 배치
[ ] STEP 13 DA_ChipCatalog / DA_ProgressionConfig + Validate 2종
[ ] STEP 14 M / ESC 입력
[ ] STEP 15 현지화 Gather → 번역 → Compile
[ ] STEP 16 테스트 체크리스트 전수
```

---

## 23. 마스터 실행 순서 (남은 작업 전체)

> 작성: 2026-08-16. **이 절이 남은 작업의 최상위 순서다.**
> §22 는 "UI 를 만드는 순서"로는 정확하지만 전체 일정의 순서는 아니다 — §22 는 이 절의 **Phase D** 를 펼친 상세 매뉴얼이다.
> 각 Phase 의 ✅ 완료 기준을 통과하지 못하면 다음 Phase 로 넘어가지 않는다.

### 23.0 왜 §22 순서를 그대로 쓰지 않는가

§22 는 데이터 에셋 작성을 **STEP 13(거의 마지막)** 에 둔다. 그런데 현재 상태는:

- 콘텐츠 에셋이 **0개** → `UDRGameInstance::ProgressionConfig` 가 **null**
- → `CanUnlockSlot` / `CanEquipChip` / `ApplyStageReward` 가 전부 `InvalidConfig` 로 즉시 반환
- → **M1~M4 백엔드 코드는 컴파일만 됐을 뿐 한 줄도 실행된 적이 없다**

이 상태에서 UI 를 먼저 만들면 최초 통합 시점에 UI 버그와 백엔드 버그가 **동시에** 터진다.
"드래그가 안 먹는다"의 원인이 위젯 배선인지, `EquipChip` 이 `InvalidConfig` 를 뱉는 건지,
`GE_Upgrade_Stats` 가 없어 수치가 안 변하는 건지 가르는 비용이 순서를 바꿔 아끼는 시간보다 훨씬 크다.

→ **STEP 13(데이터 에셋)을 맨 앞으로 당기고, UI 없이 백엔드를 먼저 증명한다.**
그 외 §22 의 내부 순서(STEP 5 → 6 → 7 → 9 → 10 …)는 그대로 유효하다.

| Phase | 내용 | §22 대응 | 예상 |
|---|---|---|---|
| **A** | 배관 연결 (에셋 4개 + 지정 2곳) | STEP 13 을 앞당김 | 반나절 |
| **B** | UI 없이 백엔드 검증 | (신규) | 반나절 |
| **C** | M5a — UI 지원 C++ | STEP 4 | 반나절 |
| **D** | UI 제작 | STEP 1~3, 5~12, 14 | 2~3일 |
| **E** | 카탈로그 전량 + 예산 + 밸런스 | STEP 13 나머지 | 1~2일 |
| **F** | 재화 E2E + 결과창 | — | 1일 |
| **G** | 업적 판정 (설계 선행) | — | 미정 |
| **H** | 현지화 · 잔여 정리 · 전수 테스트 | STEP 15~16 | 반나절 |

---

### Phase A. 배관 연결 — 에셋 4개 + 지정 2곳 [반나절]

> **목적**: 죽어 있는 백엔드에 피를 돌린다. **칩은 딱 3개만 만든다.**
> 카탈로그 전량 작성(Phase E)을 여기서 하면, 배관이 잘못됐을 때 헛수고가 3배가 된다.

#### A-1. `DA_ProgressionConfig` 생성

경로: `Content/Blueprints/Progression/DA_ProgressionConfig`
생성: Content Browser 우클릭 → Miscellaneous → **Data Asset** → `DRProgressionConfig` 선택

| 필드 | 입력값 | 비고 |
|---|---|---|
| `ChipCatalog` | (A-2 후에 연결) | 먼저 비워 두고 A-2 끝나면 돌아와 지정 |
| `MaxSlots` | `6` | |
| `SlotCosts` | `400, 700, 1100, 1500, 1900, 2400` | index 0 = 1번 칸. 단조 증가 권장 |
| `DefaultSlotCost` | `500` | `SlotCosts` 가 비었을 때만 쓰이는 폴백 |
| `bAllowSlotRefund` | `true` | |
| `RefundRatio` | `1.0` | 검증 단계에서는 전액 환불이 편하다 |
| `StageRewards[0].StageId` | `Stage1` | ⚠️ **`ADRStageGameMode::StageId` 와 문자열이 정확히 같아야 한다** (기본값 `Stage1`) |
| `StageRewards[0].DisplayName` | `스테이지 1` | |
| `StageRewards[0].FirstClearCurrency` | `1200` | |
| `StageRewards[0].bUnlocksUpgradeSystem` | **`true`** | 이게 false 면 시스템이 영원히 잠긴다 |
| `Achievements` | **비워 둔다** | Phase G 까지 미정 |

> ⚠️ **여기서 `ValidateCurrencyBudget()` 는 반드시 실패한다. 정상이다.**
> 총 획득 1200 < 총 필요 8000 × 3로봇 = 24000. 에셋을 저장할 때마다
> `PostEditChangeProperty` 가 예산 부족 경고를 로그에 찍지만 **Phase E 에서 해결할 문제**다.
> 지금 단계에서는 재화를 치트로 넣어 검증하므로 아무 지장이 없다.

#### A-2. `DA_ChipCatalog` 생성 — 칩 3개만

경로: `Content/Blueprints/Progression/DA_ChipCatalog`
생성: Data Asset → `DRChipCatalog` 선택 → `Chips` 배열에 3개 추가

| 필드 | 칩 1 | 칩 2 | 칩 3 |
|---|---|---|---|
| `ChipId` | `GR.Stat.Container` | `GR.Stat.Water` | `GR.Stat.Speed` |
| `OwnerClass` | `Gardener` | `Gardener` | `Gardener` |
| `Category` | `Stat` | `Stat` | `Stat` |
| `TargetAbilityTag` | (비움) | (비움) | (비움) |
| `DisplayName` | `강화 컨테이너` | `대용량 물탱크` | `경량 구동계` |
| `EffectSummary` | `컨테이너 용량 증가` | `최대 물 증가` | `이동 속도 증가` |
| `RequiredSlotCount` | `1` | `1` | `1` |
| `RequiredSlotTier` | `1` | `1` | `1` |
| `Modifiers[0].Stat` | `ContainerHealth` | `MaxWater` | `MoveSpeed` |
| `Modifiers[0].Op` | `Flat` | `Flat` | **`Percent`** |
| `Modifiers[0].Value` | `20` | `25` | `0.5` ← 검증용 과장값 |
| `Modifiers[0].bIsDrawback` | `false` | `false` | `false` |

> **`MoveSpeed` 를 일부러 +50% 로 넣는다.** 검증 단계에서는 눈으로 즉시 판별되는 값이 좋다.
> Phase E 에서 `0.06`(+6%) 으로 되돌린다. `Percent` 는 **0.5 = +50%** 이지 50 이 아니다 — 50 을 넣으면 +5000% 다.
>
> `ChipId` 는 세이브 키다. **한 번 정하면 바꾸지 않는다** — 바꾸면 기존 장착 정보가 고아가 된다.

작성 후 **A-1 로 돌아가 `ChipCatalog` 에 이 에셋을 지정**한다.

#### A-3. `BP_DRGameInstance` 에 `ProgressionConfig` 지정 ← **가장 흔한 함정**

`BP_DRGameInstance` 열기 → Class Defaults → `Progression Config` 슬롯에 `DA_ProgressionConfig` 지정.

> 이걸 빼먹으면 **모든 API 가 조용히 `InvalidConfig` 를 반환**하고, 화면상으로는
> "재화가 0이고 아무것도 안 열린다"로만 보인다. 증상이 원인을 전혀 가리키지 않는 지점이라
> Phase B 에서 이상하면 **제일 먼저 여기를 확인**한다.

#### A-4. `GE_Upgrade_Stats` 생성

경로: `Content/Blueprints/AbilitySystem/GameplayEffects/GE_Upgrade_Stats`
Blueprint Class → `GameplayEffect` 상속

- **Duration Policy = `Infinite`**
- `Modifiers` 6개:

| # | Attribute | Modifier Op | Magnitude Calculation | SetByCaller Tag |
|---|---|---|---|---|
| 0 | `DRPlayerAttributeSet.MaxHealth` | `Add` | `Set By Caller` | `Data.Upgrade.MaxHealth.Flat` |
| 1 | `DRPlayerAttributeSet.MaxHealth` | `Multiply` | `Set By Caller` | `Data.Upgrade.MaxHealth.Mult` |
| 2 | `DRPlayerAttributeSet.MaxWater` | `Add` | `Set By Caller` | `Data.Upgrade.MaxWater.Flat` |
| 3 | `DRPlayerAttributeSet.MaxWater` | `Multiply` | `Set By Caller` | `Data.Upgrade.MaxWater.Mult` |
| 4 | `DRPlayerAttributeSet.MoveSpeed` | `Add` | `Set By Caller` | `Data.Upgrade.MoveSpeed.Flat` |
| 5 | `DRPlayerAttributeSet.MoveSpeed` | `Multiply` | `Set By Caller` | `Data.Upgrade.MoveSpeed.Mult` |

> **태그 6개는 이미 네이티브 등록돼 있다**(`DRGameplayTags.cpp:310~338`). 태그 피커에서 바로 골라진다.
>
> ⚠️ **SetByCaller 의 치명적 실패 모드**: GE 가 참조하는 태그를 코드가 설정하지 않으면
> GAS 는 경고를 찍고 **0 을 반환**한다. `Multiply` 자리에 0 이 들어가면 그 어트리뷰트가 **0 이 된다**
> (최대 체력 0 = 즉사). 위 6개 태그는 `ADRCharacter::RefreshUpgradeEffects()`
> (`DRCharacter.cpp:985~994`) 가 **전부 설정**하므로 표대로만 만들면 안전하다.
> 반대로 **표에 없는 모디파이어를 추가하면 안 된다** — 코드가 그 태그를 안 채워 0 이 된다.
>
> `MaxHealth.Mult` 는 현재 코드가 **항상 1.0** 을 넣는다(컨테이너 Percent 성분을 Flat 으로 접어 넣기 때문).
> 지금은 항등원이지만 만들어 두면 나중에 코드에서 켤 때 에셋을 다시 안 건드려도 된다.
>
> GAS 애그리게이터 평가 순서는 `(Base + ΣAdd) × ΠMultiply` 라 §4.3 합산 규칙과 일치한다.

#### A-5. 캐릭터 BP 3종에 `UpgradeStatEffectClass` 지정

정원로봇 / 자판기 / 청소기 BP 각각 → Class Defaults → Category **`Upgrade`** →
`Upgrade Stat Effect Class` = `GE_Upgrade_Stats`

- 같은 카테고리의 `bPreserveVitalRatioOnUpgradeApply` 는 **기본값 `true` 유지**.
  (칩 목록이 늦게 도착해 GE 가 재적용될 때 공짜 회복이 생기지 않게 하는 값 — §8.4)
- 미지정 시 로그에 다음이 찍힌다:
  `[Upgrade] <이름>: UpgradeStatEffectClass 미지정 — 스탯 칩(체력/물/이속)이 반영되지 않습니다.`

#### ✅ Phase A 완료 기준
- [ ] `DA_ProgressionConfig` 의 `ChipCatalog` 가 `DA_ChipCatalog` 를 가리킨다
- [ ] `BP_DRGameInstance` 의 `ProgressionConfig` 가 지정돼 있다
- [ ] `GE_Upgrade_Stats` 에 모디파이어가 정확히 6개, 전부 SetByCaller
- [ ] 캐릭터 BP 3종에 `UpgradeStatEffectClass` 지정
- [ ] PIE 실행 시 `UpgradeStatEffectClass 미지정` 경고가 **안 나온다**
- [ ] 예산 경고는 나와도 무시한다(Phase E 에서 해결)

---

### Phase B. UI 없이 백엔드 검증 [반나절]

> **목적**: UI 를 만들기 전에 "칩을 끼면 실제로 세진다"를 증명한다.
> 여기서 잡는 버그 1개가 Phase D 에서는 3배 시간이 든다.

#### B-0. 임시 디버그 위젯 만들기

프로젝트에 `CheatManager` 도 `Exec` 함수도 없다(확인함). 콘솔로는 호출할 수 없으므로
**버리는 위젯**을 하나 만든다.

- `Content/Blueprints/UI/Upgrade/WBP_UpgradeDebug` (Phase D 완료 후 **삭제**)
- 버튼 6개 → 각각 `Get Game Instance` → `Cast To BP_DRGameInstance` → 아래 함수 호출
- 반환된 `EDRUpgradeResult` 는 전부 `Print String` 으로 출력

| 버튼 | 호출 | 인자 |
|---|---|---|
| 재화 +10000 | `AddCurrency` | `10000` |
| 시스템 해금 | `UnlockUpgradeSystem` | — |
| 슬롯 해금 | `UnlockSlot` | `Gardener` |
| 칩 장착 | `EquipChip` | `Gardener`, `GR.Stat.Speed`, `-1` |
| 칩 해제 | `UnequipChip` | `Gardener`, `GR.Stat.Speed` |
| 상태 출력 | `GetCurrency` / `GetUnlockedSlotCount` / `GetSlotAssignments` | `Gardener` |

- 띄우기: `BP_DRPlayerController` 의 `BeginPlay` 에 임시로 `Create Widget` + `Add to Viewport`
  (Phase D 에서 진짜 화면으로 대체하며 제거)
- **레벨 블루프린트에는 넣지 말 것** — `.umap` 은 병합이 불가능한 바이너리라 팀 작업에서 충돌한다.
  새 위젯 에셋은 충돌이 없다.

#### B-1. 슬롯/칩 CRUD 규칙 검증 (로비에서)

순서대로 눌러 가며 `EDRUpgradeResult` 를 확인한다:

| 시나리오 | 기대 결과 |
|---|---|
| 시스템 해금 전에 슬롯 해금 시도 | `SystemLocked` |
| 재화 0 에서 슬롯 해금 | `NotEnoughCurrency` |
| 정상 해금 | `Success`, 재화 −400, `GetUnlockedSlotCount` = 1 |
| 해금 단계 미달 칩 장착(`RequiredSlotTier` 를 임시로 3 으로 올려 테스트) | `ChipLocked` |
| 같은 칩 두 번 장착 | `AlreadyEquipped` |
| 빈 칸 0 에서 장착 | `NotEnoughSlots` (+ `OutRequiredSlots` / `OutFreeSlots` 확인) |
| 6칸 초과 해금 시도 | `AllSlotsUnlocked` |
| 점유된 칸 환불 | `SlotOccupied` |
| 칩 뺀 뒤 환불 | `Success`, 재화가 정확히 복구 |

> **환불 증식 검사**: `RefundRatio` 를 잠시 `0.5` 로 두고 해금↔환불을 5회 반복해
> **재화가 늘어나지 않는지** 확인한다. 끝나면 `1.0` 으로 되돌린다.

#### B-2. 스탯 반영 검증 (M2 가 처음 실행되는 순간)

1. 로비에서 `GR.Stat.Speed`(+50%) 장착 → 스테이지 진입
2. 콘솔에 `showdebug abilitysystem` → `MoveSpeed` 가 기본값 × 1.5 인지 확인
3. 눈으로도 확연히 빨라야 한다(그래서 과장값을 넣었다)
4. `GR.Stat.Container`(+20) 로 교체 → 재진입 →
   **컨테이너 칸 수는 4 그대로, 칸당 용량만 증가**, `MaxHealth = NumContainers × ContainerHealth` 성립
5. `GR.Stat.Water`(+25) → `MaxWater` 증가 확인

> **회귀 검사(중요)**: 칩을 **전부 뺀 상태**에서 모든 수치가 개편 전과 동일해야 한다.
> 칩이 0개면 `GE_Upgrade_Stats` 를 아예 얹지 않도록 되어 있다(§8.4).
>
> **오염 왕복 검사**: 오염 진입 → 해제 후에도 컨테이너 업그레이드분이 살아 있는지.
> `ExitCorruptedState()` 가 최대 체력을 재설정하기 때문에 여기가 §2.2 의 핵심 회귀 지점이다.

#### B-3. 스킬 반영 검증 (M3)

임시로 칩 하나의 모디파이어를 바꿔 가며 확인한다:
- `SkillWaterCost` Percent `-0.5` → 물 소모가 절반인지
- `SkillProjectileCount` Flat `+2` → `UDRFireBolt` 투사체가 실제로 늘어나는지
- `SkillDamage` Percent `+1.0` → 피해 2배인지
- **`SkillCooldown` 은 지금 무효인 게 정상이다** — 쿨다운 GE 를 SetByCaller 로 개조하지 않았기 때문(Phase E-3)

#### B-4. 세이브 라운드트립

- 장착 후 **에디터 완전 재시작** → 지갑/해금 수/장착 칸이 유지되는지
- 세이브 파일 위치: `Saved/SaveGames/DaeRunePlayerProgress.sav`
- 파일 삭제 후 실행 → 초기 상태로 안전 생성되는지
- **카탈로그 개편 내성**: `DA_ChipCatalog` 에서 장착 중인 칩을 삭제 → 재시작 →
  정화 로그가 뜨고 **재화가 증발하지 않는지**

#### B-5. 멀티플레이어 (PIE 2인 · 리슨 서버)

- 두 플레이어가 **서로 다른 칩**을 끼고 진입 → 각자 수치대로 동작
- 로그에 `SanitizeLoadout` 경고가 **없어야** 정상
- 로비에서 클래스를 여러 번 바꾼 뒤 진입 → **최종 선택 클래스의 칩만** 적용되는지
- 호스트(리슨 서버)도 클라와 동일하게 저장되는지

#### B-6. 재화 파이프라인 검증 (스테이지를 돌지 않고)

디버그 위젯에 버튼을 하나 더 만들어 `FDRStageRewardReport` 를 직접 구성해 호출한다:

```
Make FDRStageRewardReport
  StageId          = Stage1
  PlayedClass      = Gardener
  bGameClear       = true
  ClearedPhaseCount= 2
  KillCount        = 30
  AchievementIds   = (비움)
→ GI->ApplyStageReward(Report) → FDRStageRewardResult
```

| 확인 | 기대 |
|---|---|
| `TotalGained` | 1200 |
| `CurrencyBefore` / `CurrencyAfter` | 차이가 정확히 1200 |
| `bUpgradeSystemNewlyUnlocked` | 첫 호출에서 `true` |
| `Lines` | 1줄 ("스테이지 1 최초 클리어 +1200") |
| **두 번째 호출** | `TotalGained` = **0**, `Lines` **비어 있음** ← 1회성 원장의 핵심 |

#### ✅ Phase B 완료 기준
- [ ] B-1 의 실패 코드 9종이 전부 의도대로 나온다
- [ ] 칩 장착 → 스테이지에서 수치가 실제로 변한다 (이속/체력/물)
- [ ] 칩 0개일 때 개편 전과 수치가 동일하다
- [ ] 에디터 재시작 후 진행도가 유지된다
- [ ] 2인 PIE 에서 각자 다른 수치로 동작한다
- [ ] 같은 보상을 두 번 지급해도 두 번째는 0 이다

> **이 시점에서 시스템은 UI 만 없을 뿐 완전히 동작한다.** 이제부터의 작업은 전부 표현 계층이다.

---

### Phase C. M5a — UI 지원 C++ ✅ 완료 (2026-08-16)

> 작성한 파일과 구현 중 내린 결정은 **§21.10** 에 기록했다. 아래는 원래 작업 지시다.

§21 의 신규/변경 C++ 를 작성하고 **§22 STEP 4** 순서대로 컴파일한다.

1. `DRUpgradeTypes.h` — `EDRSlotState` 추가, `EDRUpgradeResult` 에 `PreviousSlotLocked` 추가
   > `WrongSlotType` 제거는 **M1' 에서 이미 완료**됐다. §22 STEP 4-1 의 해당 문구는 무시한다.
2. `DRProgressionTypes.h` — `FDRSlotViewModel`, `FDRStatPreviewLine`, `UDRChipViewModelObject`
3. `DRUpgradeUILibrary.h/.cpp` (§21.5)
4. `DRUpgradeUIStyle.h` (§21.6)
5. `DRChipDragDropOperation.h` (§20.2)
6. 나머지 §21 항목 → **풀 빌드**

#### ✅ Phase C 완료 기준
- [x] 빌드 통과, 신규 경고 0 — **게임 타깃(`DaeRune`)으로 검증.**
      에디터 타깃은 Live Coding 때문에 막혀 있었다 → 에디터 재시작 또는 `Ctrl+Alt+F11` 필요
- [ ] 디버그 위젯(B-0)이 여전히 동작한다 (API 시그니처 회귀 없음) — Phase A/B 진행 시 확인

---

### Phase D. UI 제작 [2~3일]

**§22 를 그대로 따른다.** 단 아래만 조정한다:

| §22 STEP | 조정 |
|---|---|
| STEP 0 | 1번(M1' 확인)은 완료. 2번(`DA_ProgressionConfig`)은 **Phase A 에서 이미 끝났다** |
| STEP 4 | **Phase C 에서 이미 끝났다** — 건너뛴다 |
| STEP 13 | **Phase A(최소본) + Phase E(전량)** 으로 분리됐다 — 여기서는 하지 않는다 |
| STEP 15 | Phase H 로 미룬다 (칩 문구가 Phase E 에서 확정되므로) |

수행 순서: STEP 1 → 2 → (3 선택) → 5 → 6 → 7 → 8 → 9 → 10 → 11 → 12 → 14

- **STEP 12(로비 배치)를 끝내면** 디버그 위젯 없이 정상 동선으로 진입할 수 있다:
  로비 `FreeRoam` → `BP_DRUpgradeStation` 접근 → 상호작용 → 업그레이드 화면
- 해금 전 상호작용 시 `OnInteractBlocked()` 가 불려 "스테이지1을 클리어하세요" 안내가 뜨는지 확인
- **STEP 12 통과 후 `WBP_UpgradeDebug` 를 삭제**하고 `BP_DRPlayerController` 의 임시 `Create Widget` 도 제거한다

#### ✅ Phase D 완료 기준
- [ ] 장치 상호작용 → 화면 진입 → 슬롯 해금 → 드래그앤드롭 장착 → 닫기 가 끊김 없이 된다
- [ ] 화면을 닫으면 `ReportUpgradeLoadout()` 이 서버로 나가고 스탯이 갱신된다
- [ ] 실패 사유(`EDRUpgradeResult`)가 토스트/툴팁으로 표시된다
- [ ] 임시 디버그 위젯이 프로젝트에서 삭제됐다

---

### Phase E. 카탈로그 전량 + 예산 + 밸런스 [1~2일]

#### E-1. 칩 전량 작성
- §6.2 를 기준으로 **3로봇 분량**을 작성한다 (로봇당 스탯 8 + 돌파 6 안팎)
- `ChipId` 규칙: `{로봇약어}.{Stat|Asc}.{이름}` (`GR.` / `VM.` / `RV.`)
- `RequiredSlotTier` 는 **1..6 스케일** (1 → 1, 중급 → 3, 상급 → 5)
- **Phase A 의 과장값을 되돌린다**: `GR.Stat.Speed` 의 `Value` 를 `0.5` → `0.06`
- 자판기/청소기 칩의 `TargetAbilityTag` 는 `Abilities.VendingMachine.*` / `Abilities.RobotVacuum.*` 사용

#### E-2. 검증 2종 실행 (필수)
- `UDRChipCatalog::ValidateCatalog()` — 중복 Id / 범위 초과 / 단점 없는 돌파 칩
- `UDRProgressionConfig::ValidateCurrencyBudget()` — **총 획득 ≥ 총 필요**
  - 현재 필요액: 8000 × 3로봇 = **24000**
  - 획득액이 모자라면 ① 업적을 추가하거나 ② `SlotCosts` 를 낮춘다
  - **여기를 통과 못 하면 "특정 로봇을 영구 포기해야 하는" 상태**라는 뜻이다 (요구사항 8 위반)
  - 업적이 Phase G 로 미뤄져 있으면, 일단 `SlotCosts` 를 낮춰 통과시키고 Phase G 에서 되돌린다

#### E-3. 쿨다운 칩을 쓸 스킬만 GE 개조
- 대상 스킬의 쿨다운 GE Duration → `Set By Caller (Data.Cooldown)`
- 원래 고정값을 그 어빌리티 BP 의 **`CooldownDuration`** 프로퍼티로 옮긴다
- ⚠️ GE 만 바꾸고 `CooldownDuration` 을 안 채우면 **쿨다운이 0** 이 된다(경고 로그로 검출됨)
- 둘 다 안 건드리면 쿨다운 칩만 무효가 되고 나머지는 정상 — **선택적 작업**이다

#### E-4. 밸런스 패스 — 3차 개정의 유일한 실질 부작용
> 슬롯 구분이 사라져 **6칸 전부를 돌파 칩으로 채우는 빌드**가 가능해졌다(예전엔 3칸 상한).
> 돌파 칩의 단점 강도를 **"6칸 전부" 기준**으로 다시 잡는다.
> 극단 빌드를 막아야 하면 §12.13 의 `MaxAscensionChips` 상한을 검토한다 — **슬롯 카테고리는 되살리지 않는다.**

#### ✅ Phase E 완료 기준
- [ ] `ValidateCatalog()` 오류 0
- [ ] `ValidateCurrencyBudget()` **통과**
- [ ] 3로봇 전부 칩이 있고, 각자의 스킬 태그가 올바르다
- [ ] 6칸 돌파 몰빵 빌드를 실제로 만들어 보고 게임이 파탄나지 않는다

---

### Phase F. 재화 E2E + 결과창 [1일]

1. 스테이지1 을 **실제로 클리어** → `Client_GrantStageReward` → 결과창에 획득 내역 표시
2. `FDRStageRewardResult::Lines` 를 §10.3 롤업 형태로 연출
3. 로비 복귀 → 늘어난 지갑으로 슬롯 해금
4. **다시 클리어 → 재화 0** 확인 (1회성 원장)
5. **게임오버 → 재화 0, 업적 미전송** 확인
6. 리슨 서버 호스트도 **정확히 1회만** 지급되는지

> RPC 는 결과 UI 표시 **전에** 보내고, `WipeoutDelayTime`(5초) 안에 `ServerTravel` 전 전송이 보장된다(§7.3).

#### ✅ Phase F 완료 기준
- [ ] 클리어 → 재화 → 로비 → 해금 → 강해짐 이 한 바퀴 돈다
- [ ] 재클리어 시 0 지급, 결과창에 중복 표기 없음

---

### Phase G. 업적 / 클리어 조건 판정 [설계 확정 후]

> **유일하게 게임 디자인 결정이 선행돼야 하는 항목**이라 마지막에 둔다.
> 여기를 안 해도 시스템은 완전히 동작한다 — "스테이지 최초 클리어" 재화만 들어올 뿐이다.

1. 조건 확정 (§5.2 표는 **제안일 뿐이다**)
2. `DA_ProgressionConfig::Achievements` 에 정의 추가 (`AchievementId`, `Kind`, `DisplayName`, `Currency`)
3. 판정 지점에서 **`ADRStageGameMode::GrantStageAchievement(Id, PC)`** 만 호출한다 (PC null = 전원)
   - 판정 자료는 이미 전부 있다: `PS->GetStageKillCount()`, 사이트 체력, `OnDeathDelegate`, 페이즈 전환 시각
   - **판정 코드를 페이즈/게임모드 여기저기 흩지 않는다** — 이 함수 하나로 모은다
4. 업적은 **게임 클리어 시에만** 전송된다(클리어 조건이 전제)
5. `ValidateCurrencyBudget()` **재실행** — 업적 재화가 늘었으니 E-2 에서 낮춘 `SlotCosts` 를 되돌릴 수 있다

---

### Phase H. 마감 [반나절]

1. **현지화** (§22 STEP 15) — Gather → 번역 → Compile. 칩 이름/요약은 에셋의 `FText` 필드
2. **잔여 정리**
   - `UDRFireBolt::MaxNumProjectiles` 제거 (아무도 읽지 않는 죽은 프로퍼티)
   - 자판기 스킬이 `UDRDamageGameplayAbility` 경로를 타는지 확인 —
     안 타는 커스텀 피해 계산이 있으면 `UDRVacuumAirShot` 과 같은 보정 필요
   - `UDRSeedCannon` 을 투사체 수 칩 대상으로 만들지 결정 (BP 호출부를 루프로 바꿔야 함)
3. **전수 테스트** — §14 전체 + §22 STEP 16 UI 항목

---

### 23.9 시연 일정이 촉박하다면 (최소 컷)

**Phase A → B → C → D + 칩 6~8개**까지가 "동작하는 데모"의 최소 구성이다.

- Phase E 는 **칩 개수만 줄여** 축약 (로봇 1대만 완성하고 나머지는 칩 2~3개씩)
- Phase F 는 **치트 재화**로 대체 (B-0 디버그 버튼을 남겨 둔다)
- Phase G 는 **통째로 생략** — 시스템 동작에 영향 없음
- Phase H 의 현지화는 한국어만 채우고 영어는 후순위

> 단 **Phase B 는 절대 건너뛰지 않는다.** 여기를 생략하면 시연 당일에
> "칩을 꼈는데 안 세진다"를 UI 문제로 오인해 시간을 태운다.

### 23.10 한 줄 체크리스트

```
[ ] A-1  DA_ProgressionConfig 생성 (MaxSlots 6 · SlotCosts 6개 · Stage1 보상)
[ ] A-2  DA_ChipCatalog 생성 (정원로봇 칩 3개, 이속은 과장값 0.5)
[ ] A-3  BP_DRGameInstance 에 ProgressionConfig 지정   ← 최대 함정
[ ] A-4  GE_Upgrade_Stats 생성 (Infinite · SetByCaller 6개)
[ ] A-5  캐릭터 BP 3종에 UpgradeStatEffectClass 지정
[ ] B-0  임시 디버그 위젯 (버리는 용도)
[ ] B-1  슬롯/칩 실패 코드 9종 검증
[ ] B-2  스탯 반영 + 칩 0개 회귀 + 오염 왕복
[ ] B-3  스킬 반영 (쿨다운은 아직 무효가 정상)
[ ] B-4  세이브 라운드트립 + 카탈로그 개편 내성
[ ] B-5  PIE 2인 리슨 서버
[ ] B-6  ApplyStageReward 1회성 원장 (두 번째 = 0)
[x] C    §21 C++ 작성 → 풀 빌드          ✅ 2026-08-16 (§21.10)
[ ] D    §22 STEP 1~3,5~12,14 (STEP 0·4·13·15 는 조정)
[ ] D-끝 디버그 위젯 삭제
[ ] E-1  칩 전량 + 과장값 원복
[ ] E-2  ValidateCatalog / ValidateCurrencyBudget 통과
[ ] E-3  (선택) 쿨다운 GE SetByCaller 개조
[ ] E-4  돌파 6칸 몰빵 기준 밸런스 재조정
[ ] F    실제 클리어 → 재화 → 로비 해금 E2E
[ ] G    업적 판정 (설계 확정 후) + 예산 재검증
[ ] H    현지화 · 죽은 코드 제거 · 전수 테스트
```
