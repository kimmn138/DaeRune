# Plan2 — 재화 기반 로봇 업그레이드 시스템 (Currency · Slot · Chip)

> 작성일: 2026-07-01
> **1차 개정: 2026-07-27** (경험치·레벨 방식 폐기 → 재화 소비형 업그레이드)
> **2차 전면 개정: 2026-07-30** (랭크 구매 방식 폐기 → **슬롯 해금 + 칩 장착** 방식으로 대체)
> 대상 브랜치: `feat/PlayExpo`
> 작성 목적: 스팀 계정마다 **재화(Currency)** 를 누적하고, **로비의 업그레이드 장치**에서
> 로봇(정원로봇/자판기/청소기)의 **업그레이드 슬롯을 해금**한 뒤 **업그레이드 칩을 장착**하는
> 영구 성장 시스템의 상세 구현 계획.

---

## 0. 구현 현황 (Implementation Status)

> 마지막 갱신: 2026-07-30 — **M1~M4 코드 완료(UI 제외).** 콘텐츠 에셋 작성과 UI(M5)만 남음.

| 마일스톤 | 상태 | 비고 |
|---|---|---|
| **M1. 데이터/저장/슬롯·칩 규칙 토대** | ✅ 코드 완료 | 신규 타입/칩 카탈로그 에셋/Config/세이브 v3/GameInstance API |
| **M2. 업그레이드 → 능력치 반영** | ✅ 코드 완료 | PlayerState 운반 + `GE_Upgrade_Stats` + 컨테이너 체력/받는 피해/물 획득량 |
| **M3. 업그레이드 → 스킬 반영** | ✅ 코드 완료 | 피해량 / 물 소모량 / 쿨다운 / 투사체 수 |
| **M4. 재화 획득 파이프라인** | ✅ 코드 완료 | 스테이지 최초 클리어 + 업적/클리어 조건, 1회성 원장 |
| **M5. 로비 업그레이드 UI** | ⬜ 미착수 (UI 작업) | 드래그앤드롭 장착 화면. **코드 훅은 완료** (장치 액터 + PC 진입/종료 API) |
| **M6. 콘텐츠 에셋 + 튜닝** | ⬜ 미착수 | `DA_ChipCatalog` / `DA_ProgressionConfig` / `GE_Upgrade_Stats` / 쿨다운 GE 개조 |

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
9. **슬롯 구성**: 로봇별 **스탯 슬롯 3칸 + 돌파 슬롯 3칸**. 처음에는 전부 잠겨 있고 **순차 해금**한다.
10. **슬롯 해금 단계에 따라 사용 가능한 칩 종류가 늘어난다.** (칩마다 `RequiredSlotTier`)
11. **스탯 칩은 스탯 슬롯에만, 돌파 칩은 돌파 슬롯에만** 장착된다.
12. **같은 칩을 중복 장착할 수 없다.**
13. **슬롯을 비운 채 스테이지 입장 가능**하다. **스테이지 진행 중에는 칩 교체 불가.**
14. 로비에 **업그레이드 전용 장치**를 배치하고, 상호작용하면 업그레이드 화면으로 전환한다.
15. UI(§10): 좌측 = 로봇의 슬롯, 우측 = 사용 가능한 칩 목록(스탯/돌파 2개 카테고리), 드래그앤드롭 장착.

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
              ├─ UnlockedStatSlots / UnlockedAscensionSlots   (0..3)
              ├─ StatSlotChips[3] / AscensionSlotChips[3]     (칸별 점유 칩 Id)
              └─ SpentCurrency                                (슬롯 해금 총액 = 환불 상한)
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

### 4.1 세이브 (`UDRSaveGame`, SaveVersion = 3)
```cpp
bool  bHasCompletedTutorial;                                  // 기존 유지
int32 Currency;                                               // 현재 보유 재화 (계정 공용)
int32 LifetimeCurrency;                                       // 통계용 누적 획득량
bool  bUpgradeSystemUnlocked;                                 // 스테이지1 최초 클리어로 해금
TArray<FName> ClaimedRewards;                                 // 지급 완료 보상 Id (1회성 원장)
TMap<EPlayerCharacterClass, FDRClassUpgradeState> ClassUpgrades;
int32 SaveVersion;   // static constexpr CurrentSaveVersion = 3
```
- **마이그레이션**: `SaveVersion != 3` 이면 진행도 필드를 초기화하고 v3로 스탬프한다.
  v1(경험치/레벨), v2(랭크 구매)의 제거된 프로퍼티는 UE 세이브 역직렬화 규칙상 자동으로 버려진다.
  미출시 단계라 구 진행도 → 신 진행도 환산은 하지 않는다. (`UDRGameInstance::EnsureProgressInitialized()`)
- **누락 키 lazy 초기화**: `(int32)EPlayerCharacterClass::Count` 만큼 순회하며 `FindOrAdd`.

### 4.2 슬롯/칩 보유 상태 (`FDRClassUpgradeState`)
```cpp
int32 UnlockedStatSlots      = 0;   // 0..MaxStatSlots
int32 UnlockedAscensionSlots = 0;   // 0..MaxAscensionSlots
TArray<FName> StatSlotChips;        // 길이 = MaxStatSlots. 칸별 점유 칩 Id (없으면 NAME_None)
TArray<FName> AscensionSlotChips;   // 길이 = MaxAscensionSlots
int32 SpentCurrency = 0;            // 슬롯 해금에 지불한 총액 (환불 상한)
```
- **왜 칸 배열인가**: UI가 `01..06` 슬롯을 개별 칸으로 보여주고 드래그앤드롭 목표가 칸이다.
  여러 칸을 점유하는 칩(`RequiredSlotCount` ≥ 2)은 **같은 ChipId를 여러 칸에 기록**한다.
  점유 칸 수 = 그 Id가 배열에 등장하는 횟수 → 자료구조만으로 정합성이 유지된다.
- **`SpentCurrency` 의 역할**: 환불 상한. 슬롯 비용을 나중에 올려도 지불액보다 많이 돌려주지 않는다.
- 카테고리별 접근은 `GetSlotChips(EDRChipCategory)` / `GetUnlockedSlotCount(EDRChipCategory)` 헬퍼로만 한다
  (`if (Category == Stat)` 분기를 호출부에 복제하지 않는다).

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
EDRChipCategory Category;            // Stat(스탯 칩) / Ascension(돌파 칩) → 장착 가능 슬롯 종류
FGameplayTag TargetAbilityTag;       // "적용 대상 스킬" 표기용 (없으면 캐릭터 전체)
FText DisplayName;                   // 칩 이름
FText EffectSummary;                 // 이해하기 쉬운 효과 요약 (기본 표시)
FText Description;                   // 부가 설명(선택)
UTexture2D* Icon;
int32 RequiredSlotCount = 1;         // 점유 슬롯 수
int32 RequiredSlotTier  = 1;         // 이 종류 슬롯을 N개 이상 해금해야 사용 가능
TArray<FDRUpgradeModifier> Modifiers;// 장점 + 단점(bIsDrawback) 모두 여기에
```
- **`RequiredSlotTier` 가 유일한 칩 해금 조건**이다. "슬롯 해금 단계에 따라 칩 종류가 증가"를 그대로 구현.
- 돌파 칩은 관례적으로 `Category = Ascension` + 단점 모디파이어를 1개 이상 갖는다.
  `ValidateCatalog()` 가 단점 없는 돌파 칩에 경고를 낸다.

### 4.5 재화/슬롯 규칙 (`UDRProgressionConfig` DataAsset)
```cpp
UDRChipCatalog* ChipCatalog;          // 칩 정의 카탈로그

// 슬롯
int32 MaxStatSlots      = 3;
int32 MaxAscensionSlots = 3;
TArray<int32> StatSlotCosts;          // index = 해금할 슬롯 번호-1. 비면 DefaultSlotCost * 번호
TArray<int32> AscensionSlotCosts;
int32 DefaultSlotCost   = 500;
bool  bAllowSlotRefund  = true;       // 마지막 해금 슬롯 환불 허용 (요구사항 8의 안전망)
float RefundRatio       = 1.0f;       // 1.0 = 전액 환불

// 재화 획득 (전부 1회성)
TArray<FDRStageRewardDef>   StageRewards;   // {StageId, DisplayName, FirstClearCurrency, bUnlocksUpgradeSystem}
TArray<FDRAchievementDef>   Achievements;   // {AchievementId, Kind, StageId, 이름/설명, Currency}
```
- 재화 환산이 필요한 곳은 **클라이언트**(서버 없는 메인메뉴/로비에서도 필요) → `UDRGameInstance` 가 보유한다.
- 슬롯 비용은 카테고리별 **순차 해금** 비용이다. 예: Stat `{500, 900, 1500}`, Ascension `{800, 1400, 2200}`.

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
총 필요 재화     = (Σ StatSlotCosts + Σ AscensionSlotCosts) × 로봇 수(EPlayerCharacterClass::Count)
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

**스탯 칩 (Category = Stat, 스탯 슬롯 전용)**
| ChipId | 효과 | 필요 슬롯 수 | 해금 단계 |
|---|---|---|---|
| `GR.Stat.Container` | ContainerHealth Flat +20 | 1 | 1 |
| `GR.Stat.Water` | MaxWater Flat +25 | 1 | 1 |
| `GR.Stat.Speed` | MoveSpeed Percent +6% | 1 | 1 |
| `GR.Stat.SeedDamage` | SkillDamage Percent +12% (`Abilities.GardenRobot.SeedCannon`) | 1 | 2 |
| `GR.Stat.SeedCost` | SkillWaterCost Percent −15% (SeedCannon) | 1 | 2 |
| `GR.Stat.PumpCooldown` | SkillCooldown Percent −20% (`Abilities.GardenRobot.WaterPump`) | 1 | 2 |
| `GR.Stat.AllDamage` | SkillDamage Percent +15% (태그 비움 = 전체) | **2** | 3 |
| `GR.Stat.Siphon` | WaterGain Percent +30% | **2** | 3 |

**돌파 칩 (Category = Ascension, 돌파 슬롯 전용 — 장점 + 단점 동반)**
| ChipId | 장점 | 단점(`bIsDrawback`) | 필요 슬롯 수 | 해금 단계 |
|---|---|---|---|---|
| `GR.Asc.Overdrive` | 모든 스킬 피해 +35% | 모든 스킬 물 소모 +50% | 1 | 1 |
| `GR.Asc.GlassCannon` | 모든 스킬 피해 +60% | 받는 피해 +40% | 2 | 2 |
| `GR.Asc.Turbine` | 이동 속도 +25% | 컨테이너 체력 −20% | 1 | 1 |
| `GR.Asc.Barrage` | SeedCannon 투사체 +2 | SeedCannon 피해 −30% | 2 | 2 |
| `GR.Asc.Bulwark` | 받는 피해 −25% | 이동 속도 −15% | 2 | 3 |
| `GR.Asc.Flood` | 물 획득량 +60% | 모든 스킬 쿨다운 +25% | 3 | 3 |
- 자판기/청소기도 같은 패턴으로 각자의 스킬 태그(`Abilities.VendingMachine.*`, `Abilities.RobotVacuum.*`)에 맞춰 작성한다.
- `RequiredSlotCount` 를 슬롯 총량(3)과 맞물려 설계하면 "무엇을 포기할지" 선택이 생긴다.

### 6.3 슬롯 해금 규칙 (`UDRGameInstance::CanUnlockSlot`)
검사 순서와 실패 사유(`EDRUpgradeResult`):
1. `ProgressionConfig`/`ChipCatalog` 미지정 → `InvalidConfig`
2. 업그레이드 시스템 미해금 → `SystemLocked`
3. 해당 카테고리 슬롯을 이미 전부 해금 → `AllSlotsUnlocked`
4. 지갑 부족 → `NotEnoughCurrency`
5. 통과 → `Success`

> UI는 이 결과 코드를 그대로 받아 버튼 비활성 사유로 쓴다. 별도 판정 로직을 UI에 복제하지 말 것.

### 6.4 칩 장착 규칙 (`UDRGameInstance::CanEquipChip`)
1. Config/카탈로그 미지정 → `InvalidConfig`
2. 시스템 미해금 → `SystemLocked`
3. 카탈로그에 없는 ChipId → `UnknownChip`
4. `Chip.OwnerClass != CharacterClass` → `WrongClass`
5. `Chip.RequiredSlotTier > UnlockedSlots[Category]` → `ChipLocked`
6. 이미 장착 중 → `AlreadyEquipped` (동일 칩 중복 장착 금지)
7. 남은 빈 슬롯 < `RequiredSlotCount` → `NotEnoughSlots`
   → 이때 UI는 `OutRequiredSlots` / `OutFreeSlots` 로 **필요한 슬롯 수**를 표시한다(요구사항 UI).
8. 통과 → `Success`

**칸 배정 규칙** (`EquipChip(Class, ChipId, PreferredSlotIndex)`):
- `PreferredSlotIndex` 가 유효(해금됨 + 비어 있음)하면 그 칸부터 채운다 = 드래그앤드롭한 칸.
- 남은 점유 칸은 앞에서부터 빈 칸을 순서대로 채운다.
- **연속 칸을 요구하지 않는다.** 빈 칸 총량만 충족하면 장착 가능 — 칸이 흩어져 장착이 막히는
  불필요한 실패를 없애기 위한 의도적 결정이다.
- 해제는 `UnequipChip(Class, ChipId)`(그 칩이 점유한 모든 칸 비움) 또는
  `UnequipSlot(Class, Category, SlotIndex)`(그 칸을 점유한 칩 전체 해제).
- **장착/해제/교체는 전부 무료**다. 재화가 오가지 않는다.

### 6.5 슬롯 환불 규칙 (`RefundSlot`)
- 카테고리별 **마지막으로 해금한 슬롯**만 환불한다(순차 해금의 역순).
- 그 칸이 칩에 점유돼 있으면 → `SlotOccupied` (먼저 칩을 빼야 한다).
- `bAllowSlotRefund == false` 면 → `RefundDisabled`.
- 환불액 = `floor( min(그 슬롯 비용, SpentCurrency) × RefundRatio )`.
- 환불로 해금 단계가 내려가면 **그 단계에서만 쓸 수 있던 칩이 자동으로 해제**된다
  (`SanitizeClassState()` 가 처리 — 잠긴 칩이 장착된 상태로 남지 않는다).

### 6.6 카탈로그 개편 시 안전장치
- 칩을 삭제/개명하거나 소속 클래스를 바꾸면 세이브의 장착 항목이 "고아"가 된다.
- `EnsureProgressInitialized()` 가 로드 직후 `SanitizeClassState()` 를 돌려
  ① 배열 길이를 현재 Max 슬롯 수에 맞추고, ② 고아/타 클래스/카테고리 불일치/잠긴 칩을 비우고,
  ③ 점유 칸 수가 `RequiredSlotCount` 와 어긋난 칩을 정리한다.
  ④ `UnlockedSlots` 를 `0..Max` 로 클램프하고, 줄어들었으면 **차액을 지갑으로 환급**한다(재화 증발 방지).
- 카탈로그 자체의 정합성(중복 Id, `RequiredSlotCount` 범위 초과, 단점 없는 돌파 칩 등)은
  `UDRChipCatalog::ValidateCatalog()` 로 점검한다. 콘텐츠 작업 후 반드시 1회 실행.

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
  - **카테고리별 `RequiredSlotCount` 합이 `MaxSlots` 를 넘으면 초과분 제거**
    → 서버는 클라의 "해금 단계"를 모르지만 **구조적 상한(Max 슬롯 수)** 은 알기 때문에 이 선까지 막는다.
  - 정화가 발생하면 `LogDR` 경고
- 서버가 검증할 수 **없는** 것: "그만한 재화를 실제로 벌었는가" / "그 슬롯을 실제로 해금했는가".
  재화 원장이 클라에만 있기 때문. 진짜 서버 권위가 필요하면 온라인 백엔드로 이전(§15).

---

## 8. C++ 구현 상세 (파일별)

### 8.1 ✅ `DRUpgradeTypes.h` (전면 개정)
- `EDRUpgradeStat`(ContainerHealth/MaxWater/MoveSpeed/DamageTaken/WaterGain/Skill*),
  `EDRUpgradeOp`, `EDRChipCategory{Stat, Ascension, Count}`, `EDRUpgradeResult`,
  `EDRRewardKind{StageFirstClear, Achievement, ClearCondition}`
- `FDRUpgradeModifier`(랭크 제거 · `Value` 단일값 · `GetDetailText()`),
  `FDRUpgradeChipDefinition`, `FDRResolvedStat`, `FDRSkillStatBlock`, `FDRUpgradeRuntime`
- `DRIsSkillStat()` 헬퍼로 스킬 단위 스탯 여부를 판정.

### 8.2 ✅ `DRProgressionTypes.h` (전면 개정)
- `FDRClassUpgradeState`(§4.2 — 슬롯 해금 수 + 칸 배열 + SpentCurrency, 카테고리 헬퍼 포함)
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

// 슬롯
int32 GetMaxSlotCount(EDRChipCategory) const;
int32 GetUnlockedSlotCount(Class, EDRChipCategory) const;
int32 GetNextSlotUnlockCost(Class, EDRChipCategory) const;      // -1 = 더 없음
EDRUpgradeResult CanUnlockSlot(Class, EDRChipCategory) const;
EDRUpgradeResult UnlockSlot(Class, EDRChipCategory);
EDRUpgradeResult RefundSlot(Class, EDRChipCategory, int32& OutRefunded);

// 칩
TArray<FName> GetSlotAssignments(Class, EDRChipCategory) const; // 칸별 점유 칩 (UI 좌측)
bool IsChipUnlocked(Class, FName ChipId) const;
bool IsChipEquipped(Class, FName ChipId) const;
int32 GetFreeSlotCount(Class, EDRChipCategory) const;
EDRUpgradeResult CanEquipChip(Class, FName, int32& OutRequired, int32& OutFree) const;
EDRUpgradeResult EquipChip(Class, FName, int32 PreferredSlotIndex = -1);
EDRUpgradeResult UnequipChip(Class, FName);
EDRUpgradeResult UnequipSlot(Class, EDRChipCategory, int32 SlotIndex);
void GetChipViewModels(Class, EDRChipCategory, TArray<FDRChipViewModel>& Out) const;  // UI 우측
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
Client 슬롯 해금 클릭 → GI->CanUnlockSlot(Class, Cat) → GI->UnlockSlot(Class, Cat)
    지갑 차감 → SpentCurrency 가산 → UnlockedSlots+1 → SaveProgress()
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

### 10.1 로비 업그레이드 화면 (첨부 시안 기준)
- 진입: 로비 `ADRUpgradeStation` 상호작용 → `PC->OpenUpgradeScreen()` → `OnUpgradeScreenOpened()` BP 이벤트.
- **상단**: 보유 재화(`GI->GetCurrency()`, `OnCurrencyChanged` 바인딩), 대상 로봇 표기(= 선택한 클래스), 닫기 버튼.
- **좌측 "Chip Slots"**: `GI->GetSlotAssignments(Class, Cat)` 으로 6칸 렌더.
  - 칸 상태 3종: **잠김**(해금 비용 + 해금 버튼) / **빈 칸** / **점유**(칩 카드 + 해제 버튼)
  - 다중 칸 칩은 같은 ChipId가 연달아/흩어져 나타나므로 **같은 Id 칸을 시각적으로 묶어** 표시한다.
  - 칸 라벨: `01~03 STAT`, `04~06 ASCENSION` (시안과 동일).
- **우측 "Chips"**: 탭 2개(STATS / ASCENSION) → `GI->GetChipViewModels(Class, Cat, Out)`.
  각 카드에 표시할 항목(요구사항):
  | 항목 | 뷰모델 필드 |
  |---|---|
  | 칩 이름 | `DisplayName` |
  | 적용 대상 스킬 | `TargetSkillName` (태그 없으면 "전체") |
  | 효과 요약 | `EffectSummary` |
  | 필요한 슬롯 수 | `RequiredSlotCount` |
  | 장착 여부 | `bEquipped` |
  | 해금 여부 | `bUnlocked` (+ `RequiredSlotTier`) |
  | 상세 수치 | `DetailLines` (세부 정보 토글 ON일 때만) |
  | 돌파 장점/단점 | `BenefitLines` / `DrawbackLines` (**토글과 무관하게 항상 표시**) |
- **드래그앤드롭**:
  - 드래그 시작 → `GI->CanEquipChip(Class, ChipId, Req, Free)` 로 **장착 가능한 슬롯 칸을 강조**.
  - 장착 불가 칸(카테고리 불일치 / 잠긴 칸 / 이미 점유)은 **경고 표시와 함께 비활성**.
  - `NotEnoughSlots` 면 **필요 슬롯 수**(`Req`)와 남은 수(`Free`)를 함께 표시.
  - 드롭 → `GI->EquipChip(Class, ChipId, DropSlotIndex)`. 실패 코드는 그대로 토스트/툴팁에 사용.
- **세부 정보 토글**: 기본 OFF(요약만). ON이면 `DetailLines` 노출. 토글 상태는 위젯 로컬(저장 불필요).
- **현지화**: 모든 문구 LOCTEXT. 칩 이름/요약/설명은 `FDRUpgradeChipDefinition` 의 `FText` 필드(에셋에서 번역).
  (Plan4의 현지화 규칙을 따른다)
- 미리보기(선택): `GI->BuildPreviewRuntime(Class, AddChip, RemoveChip, Out)` 로 "현재 vs 장착 후" 비교.

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
2. **`DA_ProgressionConfig`** 생성 — `ChipCatalog` 연결, 슬롯 비용/스테이지 보상/업적 입력.
   - **`ValidateCurrencyBudget()` 1회 실행 필수** (§5.4). 실패 시 슬롯 비용을 낮추거나 업적을 추가한다.
3. **GameInstance BP** 에 `ProgressionConfig` 지정. ← 빼먹으면 재화가 0으로만 지급된다.
4. **`GE_Upgrade_Stats`** 생성 (§8.4 표). SetByCaller 태그는 이미 네이티브 등록돼 있다.
5. **캐릭터 BP** 에 `UpgradeStatEffectClass = GE_Upgrade_Stats` 지정 (3로봇 전부).
6. **쿨다운 칩을 쓸 스킬만** 쿨다운 GE의 Duration을 SetByCaller(`Data.Cooldown`)로 바꾸고,
   원래 고정값을 어빌리티 BP의 `CooldownDuration` 프로퍼티로 옮긴다.
   > ⚠️ GE만 바꾸고 `CooldownDuration` 을 안 채우면 쿨다운이 0이 된다(경고 로그로 검출됨).
   > 반대로 둘 다 손대지 않으면 쿨다운 칩만 무효가 되고 나머지는 정상 동작한다.
7. **로비 레벨**에 `BP_DRUpgradeStation` 배치 (`ADRUpgradeStation` 상속) + 프롬프트 위젯 지정.
8. 칩 아이콘 텍스처 (시안의 M.2 SSD 모양 카드).

---

## 12. 엣지 케이스 & 결정 사항

1. **해금 전 상호작용**: 장치가 `SystemLocked` 상태를 BP 이벤트로 알린다. 화면 자체가 열리지 않는다.
2. **게임오버**: 재화 0. 업적도 전송하지 않는다(클리어 조건이 전제).
3. **중도 접속/이탈**: Join-in-progress 차단됨. 종료 시점에 존재하는 PC에게만 지급.
4. **세이브 손상/마이그레이션**: `SaveVersion` 불일치 시 안전 초기화. 로드 실패 시 새 세이브 생성.
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
  - 스탯 칩을 돌파 슬롯에 → `WrongSlotType`(칸 지정) / 카테고리 탭 분리로 UI 차단
  - 2칸 칩 + 남은 1칸 → `NotEnoughSlots` 와 함께 필요 수 2 / 남은 수 1 표시
  - 같은 칩 두 번 → `AlreadyEquipped`
  - 해금 단계 미달 칩 → `ChipLocked`
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
- 위조 목록(없는 Id, 6칸 초과, 타 클래스 칩)을 신고했을 때 서버가 정화하고 경고 로그를 남기는지.
- 처치 크레딧이 막타 친 플레이어에게만 1회 가는지.

### 14.3 회귀
- 칩을 하나도 장착하지 않은 상태에서 모든 수치가 **개편 전과 완전히 동일**한지
  (`FDRUpgradeRuntime` 이 비면 항등이고 `GE_Upgrade_Stats` 를 아예 안 얹는다).
- 컨테이너 UI: 컨테이너 체력 칩 장착 시 칸 수는 그대로(4)이고 칸당 용량만 커지는지.
  **오염 진입 → 해제 후에도 업그레이드분이 유지되는지** (§2.2 회귀 항목).
- 기존 튜토리얼 완료 플래그가 v3 마이그레이션 후에도 유지되는지.
- `GE_Upgrade_Stats` 가 적/클렌저 사이트 등 비플레이어 ASC에 영향을 주지 않는지.
- 쿨다운 GE를 개조하지 않은 스킬의 쿨다운이 그대로인지.

---

## 15. 향후 확장 (Out of Scope, 메모)

- 로봇별 전용 재화(듀얼 커런시)로 성장 분리.
- 서버 권위 진행도(온라인 백엔드/스팀 클라우드)로 치트 방지.
- 칩 프리셋 저장/전환(빌드 슬롯).
- 칩 획득 개념 추가(현재는 슬롯 해금 단계로만 열림 — 드롭/제작으로 확장 가능).
- 업적 목록 전용 화면(달성/미달성 + 남은 재화 표시).

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

- [x] `DRVacuumAirShot.cpp` — 단계별 피해량에 `SkillDamage` 적용

**잔여 확인 항목**
- [ ] **업적/클리어 조건 판정 로직** — `GrantStageAchievement()` 호출부. 조건이 확정돼야 작성 가능
      (§5.2 표는 제안일 뿐이다). 이게 없으면 재화는 "스테이지 최초 클리어"분만 들어온다.
- [ ] `UDRFireBolt::MaxNumProjectiles` — 아무도 읽지 않는 죽은 프로퍼티. 제거 권장
- [ ] `UDRSeedCannon` — 투사체 수 칩 대상으로 만들려면 BP 호출부를 루프로 바꿔야 함
- [ ] 자판기(`Abilities.VendingMachine.*`) 스킬들이 `UDRDamageGameplayAbility` 경로를 타는지 확인
      (타지 않는 커스텀 피해 계산이 있으면 에어샷과 같은 보정이 필요)
