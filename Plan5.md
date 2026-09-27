# Plan5 — 로봇 전용 칩 (1부: 정원로봇 9종 · 2부: 자판기 기본 공격 3종)

> **1부(§1~§10) 상태: 구현 완료** — C++ · 에셋 · BP · 카탈로그 전부 반영됐다.
> 씨앗 폭탄·물대포는 게임에서 동작을 확인했고, **기본 공격(발톱)은 게임 내 검증이 남았다 → §10**
> **2부(문서 끝) 상태: 계획** — 자판기 기본 공격(동전) 칩 3종. 1부와 같은 배관을 쓴다.
> 2부가 구현·검증되면 1부 구조(스펙·결정·함정·레시피)에 흡수하고 계획 부분은 걷어낸다.
> 최종 갱신: 2026-09-27 / 브랜치 `feat/upgradeskill`
> 선행 문서: **Plan2.md**(재화·슬롯·칩 시스템) → **Plan3.md**(공용 칩 8종)
>
> 📌 **이 문서는 "계획서"가 아니라 ★완료된 기능의 참조 문서★ 다.**
> 계획 단계의 코드 해부·단계별 에디터 매뉴얼·테스트 체크리스트는 역할을 다해 걷어냈고,
> **다시 만들거나 고칠 때 필요한 것** — 최종 수치, 설계 결정과 그 근거, 밟으면 안 되는 함정,
> 다음 칩을 추가하는 레시피 — 만 남겼다.
>
> ⚠️ **코드 주석 표기 규칙** — 씨앗 폭탄 **`Plan5(씨앗칩)`** / 물대포 **`Plan5(물대포칩)`** / 기본 공격 **`Plan5(발톱칩)`**
> / 자판기 기본 공격 **`Plan5(동전칩)`**(2부).
> 코드 주석에 있던 이 문서의 절 번호(`§4.2` 등)는 **전부 제거했다.**
> 절 번호는 다시 붙이지 않는다 — 문서가 바뀔 때마다 조용히 틀린 참조가 되기 때문이다.
> 대신 **결정 번호(D-n)** 와 **함정 번호(T-n)** 로 가리킨다. 이 둘은 문서 구조가 바뀌어도 유지되며,
> 빠진 번호는 결번이다(재사용하지 않는다).
>
> ⚠️ 코드에 남아 있는 `Plan5 §5.2` 같은 **접두 없는** 참조는 **삭제된 "청소기/탑승 계획서"** 를
> 가리키는 별개의 것이다(`DRVacuumDash.cpp` 등). 이 문서와 무관하므로 건드리지 않았다.

---

## 1. 칩 9종 최종 스펙

공통: `OwnerClass = Gardener`, `bCommonChip = false`, `Category = Ascension`, `RequiredSlotCount = 1`.
모든 모디파이어는 `Op = Percent`(곱연산)이며 **각 모디파이어마다 `TargetAbilityTag` 를 지정**한다
(칩 레벨의 `TargetAbilityTag` 는 표기용이다 — 모디파이어에서 빠뜨리면 정원로봇 **모든 스킬**에 퍼진다).

### 1.1 씨앗 폭탄 3종 — `TargetAbilityTag = Abilities.GardenRobot.SeedCannon`

| # | `ChipId` | 이름 | Modifiers | GrantedTags | Tier |
|---|---|---|---|---|---|
| ① | `GR.Asc.SeedRefund` | 회수식 뇌관 | `SkillWaterCost` **+1.0** ★단점★ | `…SeedBomb.Refund` | 3 |
| ② | `GR.Asc.SeedSupport` | 관개 탄두 | `SkillHealing` **+1.0**<br>`SkillDamage` **−0.8** ★단점★ | `…SeedBomb.Slow`<br>`…SeedBomb.Haste` | 4 |
| ③ | `GR.Asc.SeedWide` | 광역 살포기 | `SkillAreaRadius` **+0.5**<br>`SkillWaterCost` **−0.3**<br>`SkillDamage` **−0.4** ★단점★ | (없음) | 4 |

- ① **적에게 직격**하면 사용한 물 전액 환급. 바닥·벽·아군에 맞으면 환급 없음. 대신 물 소모 ×2.
- ② 폭발에 닿은 아군 회복 ×2 + 이속 +30%(5초), 맞은 적 이속 −30%(5초). 대신 피해 −80%.
- ③ 내·외부 폭발 범위 각각 ×1.5, 물 소모 ×0.7. 대신 피해 −40%.
  (물 감소는 **장점**이라 `bIsDrawback = false`)

### 1.2 물대포 3종 — `TargetAbilityTag = Abilities.GardenRobot.WaterPump`

| # | `ChipId` | 이름 | Modifiers | GrantedTags | Tier |
|---|---|---|---|---|---|
| ① | `GR.Asc.PumpRamp` | 가압 노즐 | `SkillWaterGrant` **−1.0** ★단점★ | `…WaterPump.Ramp` | 3 |
| ② | `GR.Asc.PumpSupport` | 급수 노즐 | `SkillWaterGrant` **+1.0**<br>`SkillWaterCost` **−0.8**<br>`SkillDamage` **−1.0** ★단점★ | `…WaterPump.Slow`<br>`…WaterPump.Vulnerable` | 4 |
| ③ | `GR.Asc.PumpPierce` | 고압 노즐 | `SkillWaterCost` **+0.5** ★단점★ | `…WaterPump.Pierce` | 4 |

- ① **피해 틱(1초)에 적을 하나라도 맞히면** 스택 +1, 스택당 피해 **+5%**(최대 **+50%** = 10스택).
  2초간 아무도 못 맞히거나 물대포를 끄면 **전 스택 소멸**. 대신 아군 급수 0.
  ★관통으로 5명을 맞혀도 **그 틱의 스택은 1**★ / 아군만 맞힌 틱은 오르지 않는다.
- ② 맞은 적 이속 −30%, 맞은 적이 **받는 모든 피해 +50%**(1초, 적중마다 갱신).
  아군 급수 ×2, 물 소모 −80%. 대신 물대포 피해 0.
- ③ 물줄기가 **적을 관통**(지형지물은 관통 불가). **아군 급수는 가장 가까운 1명만**(D-18).
  대신 물 소모 ×1.5.

### 1.3 기본 공격(발톱) 3종 — `TargetAbilityTag = Abilities.GardenRobot.ClawSwipe`

| # | `ChipId` | 이름 | Modifiers | GrantedTags | Tier |
|---|---|---|---|---|---|
| ① | `GR.Asc.ClawHeavy` | 중량 삽날 | `SkillDamage` **+4.0**<br>`SkillWindup` **+2.0** ★단점★ | (없음) | 3 |
| ② | `GR.Asc.ClawDrain` | 수혈 삽날 | `SkillDamage` **−0.5** ★단점★ | `…Claw.Lifesteal`<br>`…Claw.AllyHeal` | 4 |
| ③ | `GR.Asc.ClawWide` | 광역 삽날 | `SkillAreaRadius` **+0.3**<br>`SkillDamage` **−0.2** ★단점★ | (없음) | 3 |

- ① 피해 ×5. 대신 선딜레이 ×3 — 몽타주를 1/3 속도로 틀다가 **타격 노티파이에서 원속으로 복구**하므로
  **후딜은 그대로**다(D-20).
- ② 적을 때리면 **적 체력이 실제로 깎인 양**만큼 자기 회복(받는 쪽 배율 반영, 오버킬 제외 — D-22).
  아군을 때리면 **내 체력을 비용으로** 아군 회복. 회복량 = 칩이 반영된 이번 공격 피해량을
  **아군이 실제로 받을 수 있는 양**(D-33)과 **내 체력 − 1**(D-26)로 자른 값. 대신 피해 −50%.
- ③ 판정 반경 ×1.3. 씨앗 폭탄용 `SkillAreaRadius` 축을 재사용하지만 모디파이어별 태그라 서로 간섭하지 않는다.
  대신 피해 −20%.
- 카탈로그 표기: `+4.0` 은 화면에 "+400%" 로 나오므로 `DetailOverride` 에 `피해 5배` / `선딜레이 3배` 를 쓴다.
  ②의 `Description` 에 "회복 차단 효과가 있으면 이 칩은 동작하지 않습니다" 를 명시한다(②+⑥ 경고는 이 문구뿐).

### 1.4 조합 결과 (곱연산이라 순서 무관)

**씨앗 폭탄** — 기준: 물 `W`, 피해 70/30, 회복 100/50, 범위 200/400

| 조합 | 물 | 피해(내/외) | 회복(내/외) | 범위(내/외) |
|---|---|---|---|---|
| ① | `W×2` | 70 / 30 | 100 / 50 | 200 / 400 |
| ② | `W` | **14 / 6** | **200 / 100** | 200 / 400 |
| ③ | `W×0.7` | **42 / 18** | 100 / 50 | **300 / 600** |
| ②+③ | `W×0.7` | **8.4 / 3.6** | 200 / 100 | 300 / 600 |
| ①+③ | **`W×1.4`** | 42 / 18 | 100 / 50 | 300 / 600 |

**물대포** — 기준: 물 `W`, 피해 `D`, 아군 급수 `G`, 스택 `n`(0~10)

| 조합 | 물 | 피해 | 아군 급수 | 관통 |
|---|---|---|---|---|
| ① | `W` | `D×(1+0.05n)` 최대 `D×1.5` | **0** | ✗ |
| ② | **`W×0.2`** | **0** | **`2G`** | ✗ |
| ③ | **`W×1.5`** | `D` (관통 전원) | `G` (1명) | ✓ |
| ①+③ | `W×1.5` | `D×(1+0.05n)` 관통 전원 | 0 | ✓ |
| ②+③ | **`W×0.3`** | 0 | `2G` (1명) | ✓ |
| ①+② | `W×0.2` | 0 | 0 | ✗ ⚠️ **둘 다 무력화되는 조합** |

**기본 공격** — 기준: 피해 `D`, 선딜 `T`, 판정 반경 `R`

| 조합 | 피해 | 선딜 | 반경 | 흡혈(적 1명당) |
|---|---|---|---|---|
| ① | **`5D`** | **`3T`** | `R` | — |
| ② | **`0.5D`** | `T` | `R` | `0.5D` |
| ③ | **`0.8D`** | `T` | **`1.3R`** | — |
| ①+② | **`2.5D`** | `3T` | `R` | **`2.5D`** ★느리지만 한 방에 크게 회복★ |
| ①+③ | **`4D`** | `3T` | `1.3R` | — |
| ②+③ | `0.4D` | `T` | `1.3R` | `0.4D` × **여러 명** (아군 수혈도 여러 명 — 비용도 인원수만큼, T-28) |
| ①+②+③ | **`2D`** | `3T` | `1.3R` | `2D` × 여러 명 |

**공용 칩·다른 칩과의 상호작용**

| | 효과 |
|---|---|
| ⑥ 광폭화 / ⑦ 시너지 | `ExecCalc_Damage` 소스측에서 **추가로 곱해진다**. 물대포 ②의 피해 0 에는 무효(0 × 무엇이든 0). 발톱 ②의 **흡혈**은 최종 피해 기준이라 같이 늘지만, **아군 수혈량**은 `GetUpgradedDamage()` 기준이라 이 배율을 받지 않는다 |
| ⑧ 피의 계약 | 물 대신 체력으로 지불. ★물대포 ②의 −80% 가 **체력 소모까지 80% 줄여 준다**★ (`CheckCost` 가 `EffectiveWaterCost × Ratio` 를 쓰기 때문). 발톱 ②와 겹치면(기본 공격에 물 소모가 있을 때) 체력 소모처가 둘이 되지만 둘 다 체력 0 방지 가드가 있다(`CheckCost` / D-26) |
| ⑥ 광폭화 + 씨앗 ② | 아군 힐은 정상, **본인은 회복 차단**(NoHealing — 받는 쪽 판정) |
| ⑥ 광폭화 + 발톱 ② | ★**서로를 죽이는 조합**★ — 본인이 ⑥이면 흡혈이 전부 버려진다. 아군이 ⑥이면 수혈이 발동하지 않는다(비용도 0 — D-33) |
| 물대포 ② → 발톱 ② | 취약(`Debuff.Vulnerable`) 걸린 적을 발톱으로 치면 피해 ×1.5 → **흡혈도 ×1.5** |

---

## 2. 시스템 구조

### 2.1 수치가 흐르는 경로

```
[DA_ChipCatalog]  칩 정의 (정적 수치 + 기능 스위치 태그)
      │
      ├─ Modifiers ────────────────► 숫자
      └─ GrantedTags ──────────────► 규칙 스위치 (Upgrade.Feature.*)
      │
      ▼  ADRPlayerController::ReportUpgradeLoadout → Server → ADRPlayerState::EquippedChips (복제)
      ▼  ★각 머신이★ 로컬에서 ResolveLoadout()   (서버 + 모든 클라이언트)
FDRUpgradeRuntime  (복제하지 않는다)
   ├ GlobalStats / SkillStats[태그]  →  FDRResolvedStat { Flat, Mult }
   │      최종값 = (Base + Flat) × Mult      ★Mult 는 곱연산 누산기, 기본값 1.0★
   └ GrantedTags
      │
      ├─► 어빌리티 안:  UDRGameplayAbility::GetUpgradedFloat(Stat, Base)
      │                 · 물 소모   = GetEffectiveWaterCostFor()   (CheckCost/ApplyCost)
      │                 · 피해      = GetUpgradedDamage() × GetRuntimeDamageMultiplier()
      │                 · 아군 급수 = GrantWaterToAlly()
      │                 · 판정 반경 = GetUpgradedAttackRadius()     ┐ UDRMeleeAttack
      │                 · 선딜      = GetWindupPlayRate()           ┘ (BP 핀에 연결)
      │
      ├─► 액터 안:      UDRAbilitySystemLibrary::ApplySkillUpgrade(SourceASC, SourceAbilityTags, Stat, Base)
      │                 (ADRSeedProjectile 이 BeginPlay 에서 1회 해석해 캐시)
      │
      ├─► 규칙 분기:    UDRAbilitySystemLibrary::HasUpgradeFeature(ASC, FeatureTag)
      │                 (UDRWaterPump 는 StartWaterPumpLoop 에서 1회 해석해 캐시,
      │                  UDRMeleeAttack 은 캐시 없이 매번 — 스윙당 조회가 몇 번뿐이다)
      │
      └─► 맞는 쪽:      UDREnemyAttributeSet::HandleIncomingDamage   (받는 쪽 배율이 끝난 최종 피해를 아는 곳)
                        · 흡혈 = GetLifestealRatioForDamage(Props) — 시전자 피처 태그 AND 기본 공격 피해

[DA_ProgressionConfig.ChipTuning]  숫자로 표현 못 하는 동적 상수
   └ .SeedBomb / .WaterPump / .Claw  →  UDRAbilitySystemLibrary::GetChipTuning(WorldContext)
```

### 2.2 스탯 축 (`EDRUpgradeStat`) — 전부 곱연산

| 구분 | 축 |
|---|---|
| 캐릭터 단위 | `ContainerHealth` `MaxWater` `MoveSpeed` `DamageTaken` `WaterGain` `MaxHealth` `HealthRegenDelay` `DamageDealt` |
| 스킬 단위 | `SkillDamage` `SkillWaterCost` `SkillCooldown` `SkillProjectileCount` **`SkillHealing`** **`SkillAreaRadius`** **`SkillWaterGrant`** **`SkillWindup`** |

★굵은 4개가 Plan5 에서 추가됐다.★ 스킬 단위 축은 **반드시 `DRIsSkillStat()` 에도 추가**해야 한다(T-12).
`SkillWindup` 은 **초가 아니라 기준 1.0 에 곱하는 배율**이다(D-21). 클수록 나쁘므로 `DRIsLowerBetter` 에 등록하고,
`DRIsTimeStat` 에는 **넣지 않는다**(T-25).

**`Mult` 는 곱연산 누산기다** — `Percent` 에서 이름을 바꾼 이유가 그것이다(D-7).
`Mult *= (1 + Value)` 로 누적되므로 감소 칩을 아무리 겹쳐도 부호가 뒤집히지 않는다
(합연산이던 시절 −80% + −40% = −120% → 배율 음수 → **적이 회복**하는 버그가 있었다).

### 2.3 세 종류의 태그 — ★절대 섞지 말 것★

| | **칩 피처 태그**<br>`Upgrade.Feature.*` | **GAS 태그**<br>`Buff.*` / `Debuff.*` | **SetByCaller 태그**<br>`Data.*` |
|---|---|---|---|
| 어디에 사나 | `FDRUpgradeRuntime::GrantedTags` — **ASC 에 없다** | **ASC 에 있다** (GE 의 Granted Tags) | GE 스펙 안 — 크기를 넘기는 키일 뿐, 어디에도 부여되지 않는다 |
| 언제 변하나 | 로드아웃이 바뀔 때만 | 런타임에 수시로 | GE 를 적용할 때마다 |
| 쓰는 법 | `HasUpgradeFeature(ASC, Tag)` | `HasMatchingGameplayTag()` / GE 핸들의 `GetCurrentStackCount()` | `SetSetByCallerMagnitude(Tag, 값)` |
| Plan5 에서 추가 | `…SeedBomb.{Refund,Slow,Haste}`<br>`…WaterPump.{Ramp,Slow,Vulnerable,Pierce}`<br>`…Claw.{Lifesteal,AllyHeal}` | `Buff.WaterPump.Pressure`<br>`Debuff.Vulnerable`<br>(에셋 전용) `Debuff.SeedBomb.Slow` `Buff.SeedBomb.Haste` `Debuff.WaterPump.Slow` | `Data.MoveSpeed.Mult`<br>`Data.Duration`<br>`Data.HealthCost` |

- 피처 태그를 **ASC 에 Loose 로 얹지 말 것** — 어빌리티의 `ActivationBlockedTags` 와 의도치 않게 상호작용한다.
- C++ 이 읽는 GAS 태그만 **네이티브**로 등록한다. VFX/에셋에서만 쓰는 태그는 에디터에서 만들면 된다.
- 발톱칩은 새 GAS 태그가 없다 — 지속 상태 없이 즉발 효과뿐이다.

### 2.4 공용 배관 (한 번만 깔면 되는 것)

| | 무엇 | 어디 |
|---|---|---|
| **A** | 스탯 축 `SkillHealing` / `SkillAreaRadius` | `DRUpgradeTypes.h` |
| **B** | `Percent` → **곱연산 `Mult`** 전환 | `FDRResolvedStat` / `AddModifier` / `ApplySkill` |
| **C** | 액터가 스킬 업그레이드를 읽는 길 | `ApplySkillUpgrade()` / `GetUpgradeKeyTagFromAbilityTags()` / `ApplyTimedSpeedEffect()` |
| **D** | 씨앗 폭탄 피해 경로 복구 + 음수 피해 0 클램프 | `DRSeedProjectile.cpp` / `ExecCalc_Damage.cpp` |
| **E** | 스탯 축 `SkillWaterGrant` | `DRUpgradeTypes.h` |
| **F** | 런타임 피해 배율 훅 `GetRuntimeDamageMultiplier()` | `DRDamageGameplayAbility` |
| **G** | 적이 **받는** 피해 배율 관문 | `UDREnemyAttributeSet::HandleIncomingDamage` |
| **H** | 스탯 축 `SkillWindup`(배율) | `DRUpgradeTypes.h` — `DRIsSkillStat` + `DRIsLowerBetter` |
| **I** | 흡혈 판정·지급 + 회복 공용 헬퍼 | `GetLifestealRatioForDamage()` → G 관문에서 지급 / `ApplyHealToActor()` / `GetHealableAmount()` |
| **J** | 체력을 **비용으로** 깎는 창구 | `GE_Claw_SelfCost` + `UDRMeleeAttack::TryHealAllyWithOwnHealth()` |

---

## 3. 설계 결정

| # | 결정 | 근거 (한 줄) |
|---|---|---|
| **D-1 / D-19** | 9종 전부 정원로봇 전용 + `Category = Ascension` | 전부 명확한 단점을 동반. `ValidateCatalog()` 가 "단점 없는 돌파 칩"을 잡아 준다 |
| **D-2** | 정적 수치는 **`DA_ChipCatalog`**, 동적 상수는 **`DA_ProgressionConfig.ChipTuning`**. C++ 에 리터럴을 남기지 않는다 | 밸런스 패치에 재컴파일 금지 |
| **D-3** | 씨앗 폭탄 "직격" 판정은 `Cast<ADREnemy>` 성공 + 미사망 | `IsNotFriend()` 는 `Player`/`Enemy` 태그가 **둘 다 없는** 중립 액터에도 true 를 준다 |
| **D-4 / D-17** | `EDRUpgradeStat` 신규 값은 **반드시 `Count` 바로 앞** | 중간에 끼우면 카탈로그에 직렬화된 기존 칩 효과가 다른 스탯으로 밀린다(T-30) |
| **D-5** | 발사체는 `BeginPlay`(서버)에서 **1회 해석해 캐시** | 비행 중 로드아웃이 바뀔 일이 없다 |
| **D-6** | 이속 증감의 배율·지속시간은 **GE 가 아니라 Config** 에 (SetByCaller) | 밸런스 창구를 한 곳으로 |
| **D-7** | `Percent` → **`Mult`** 로 **이름까지** 바꿨다 | 기본값이 0 이 아니라 **1** 이라, 이름을 두면 `1.f + Stat.Percent` 관용구가 조용히 2배가 된다. **컴파일 에러로 모든 사용처를 강제 노출** |
| **D-9** | 물대포 램프 스택은 **스택형 GE** 로 들고, 스택 수는 **`GetCurrentStackCount(RampStackHandle)`** 로 읽는다 | 2초 갱신/전체 소멸·사망 시 정리를 GAS 가 공짜로 해 준다. ★`GetGameplayTagCount` 가 아니다★ → §6 **T-3** |
| **D-10** | "받는 피해 +50%"는 `ExecCalc_Damage` 가 **아니라** `HandleIncomingDamage` 에서 곱한다 | 디버프 DoT 은 `IncomingDamage` 에 SetByCaller 로 직접 꽂혀 ExecCalc 를 안 탄다 → "모든 피해"를 못 덮는다 |
| **D-11** | 취약은 **중첩하지 않는다** (태그 보유 여부만 본다) | 카운트로 곱하면 정원로봇 N명이 같은 적을 쏠 때 배율이 N제곱으로 튄다 |
| **D-12** | 취약 배율은 GE 의 SetByCaller 가 아니라 **피해 시점에 Config 를 읽는다** | 값이 단 하나뿐인 전역 상수라 읽는 쪽 한 줄이 더 싸고 명확하다 |
| **D-13** | 관통이 "무엇을 뚫는가"는 **`ICombatInterface` 구현 여부**로 판정 | `Cast<ADREnemy>` 는 홀로그램 두더지를 놓치고, `IsValidDamageTarget` 은 죽은 적을 false 로 줘서 **시체가 빔을 막는다** |
| **D-14** | 물대포 칩 해석은 **서버 + 소유 클라 양쪽**에서 (`StartWaterPumpLoop` 최상단) | 관통이 **빔 길이 계산**을 바꾸는데 그 계산은 클라의 1P 빔도 한다. 서버만 하면 "판정은 뚫는데 이펙트는 첫 적에서 잘린다" |
| **D-15** | 한 틱이 여러 대상을 때리면 BP 이벤트 `OnDamageTickReached` 를 **대상 수만큼** 호출 | BP 그래프를 한 노드도 안 고치고 관통을 얻는다. 관통이 없으면 목록이 1개라 호출 횟수도 기존과 같다 |
| **D-16** | 디버프(감속·취약)를 **피해보다 먼저** 건다 | 같은 틱에 들어오는 아군의 피해가 곧바로 취약 배율을 받도록 |
| **D-18** | **관통은 적에게만.** 아군은 관통 여부와 무관하게 **가장 가까운 1명**만 급수 | 파티 전원 급수는 칩 ②(급수 ×2)와 겹쳤을 때 과하고, "뚫고 지나간다"는 연출도 적에게만 자연스럽다 |
| **D-20** | 선딜레이 = 몽타주 `PlayRate` 를 낮추고 **타격 노티파이에서 원속 복구** | `PlayRate` 만 낮추면 후딜까지 3배("전체가 느림"), 몽타주 앞 `Delay` 는 가만히 섰다가 갑자기 휘둘러 부자연스럽다 |
| **D-21** | `SkillWindup` 은 초가 아니라 **기준 1.0 에 곱하는 배율** | 선딜의 실제 길이는 몽타주 에셋 안에 있어 어빌리티가 모른다. 몽타주가 바뀌어도 칩은 그대로 동작 |
| **D-22** | 흡혈량 = **적 체력이 실제로 깎인 양** (엘리트·Phase1 ×0.7, 취약 ×1.5 반영 / 오버킬 제외) | 화면에 뜬 피해 숫자와 일치해야 한다. 빈사 잡몹으로 폭발적 회복을 하는 것을 막는다 |
| **D-23** | 흡혈은 **`UDREnemyAttributeSet::HandleIncomingDamage`** 에서 지급하고, 판정은 **라이브러리 함수 하나**로 뺀다 | 최종 피해를 아는 유일한 지점이다(D-10 과 같은 이유). 칩 지식이 어트리뷰트셋으로 새지 않게 |
| **D-24** | 흡혈 조건 = 피처 태그 **AND** 피해 컨텍스트의 `SourceAbilityTags` 에 `ClawSwipe` | 피처 태그만 보면 씨앗 폭탄·물대포 피해까지 흡혈된다. 컨텍스트에 이미 실려 있어 추가 배선이 없다 |
| **D-25** | 수혈 비용은 **`Health` 를 직접 깎는 GE**(`IncomingDamage` 아님). 코드는 `Data.HealthCost` 에 **양수(비용 액수)** 를 넣고 부호는 GE(`Coefficient −1`)가 정한다 | 피해가 아니라 **비용**이다 — 컨테이너 오버플로·히트리액트·전투 진입·부품 낙하를 발동시키지 않는다(피의 계약과 같은 방식) |
| **D-26** | ★자살 방지★ 수혈량 ≤ (내 체력 − `AllyHealMinSelfHealth`) ÷ 비용 비율. 0 이하면 아무 일도 없다 | 체력 직접 차감은 `Die()` 를 타지 않아 **체력 0 인 채 생존**이 된다(T-21) |
| **D-27** | 아군은 **`!IsNotFriend`(둘 다 Player)** 로 양성 판정 | "유효 피해 대상이 아님"으로 뒤집으면 **클렌저 사이트**(Player/Enemy 태그 없음, `ICombatInterface` 미구현)가 아군으로 잡힌다 |
| **D-28** | 판정 반경을 BP 리터럴에서 **`UDRMeleeAttack::AttackRadius`** 로 옮기고 BP 는 `GetUpgradedAttackRadius()` 를 부른다 | 리터럴엔 칩이 잡을 손잡이가 없다. 물대포 `AllyWaterGrantAmount` 와 같은 처리 |
| **D-33** | 수혈 비용은 **실제로 회복된 양** 기준 — GE 를 걸기 **전에** `GetHealableAmount()` 로 자른다 | 만피 아군을 때려 체력만 잃지 않게. NoHealing → 0 / 오염 → 요청량 전액(정화) / 그 외 → `min(요청량, MaxHealth − Health)` |
| **D-34** | 씨앗 발사체의 `ApplyHealToAlly` 를 **`ApplyHealToActor`** 로 통합 | 같은 일을 하는 코드가 둘이면 한쪽만 고치는 사고가 난다. `SourceASC=nullptr` + `SourceObject=this` 로 동작이 동일하다 |
| **D-35** | `UDRMeleeAttack` 추가분은 전부 **BP 가 명시적으로 읽어야 효과가 나는** 형태(`EditDefaultsOnly` 프로퍼티 + 호출형 함수) | **적 GA 6종도 상속**한다. 생성자·`ActivateAbility`·가상 오버라이드를 건드리지 않았으므로 적 GA 는 동작이 바뀔 수 없다 |

> 보류: `EDRUpgradeOp::Override`("값을 N 으로 **고정**")는 소비자가 없어 범위에서 뺐다.
> 나중에 필요해지면 ★`MaxHealth`/`MaxWater`/`MoveSpeed` 는 `GetGlobalStat()` 을 직접 꺼내 쓰므로
> Override 를 조용히 무시한다★ — `ValidateCatalog()` 로 막는 것이 필수다.

---

## 4. 코드 지도

| 파일 | 이 기능에서 하는 일 |
|---|---|
| `Public/Game/DRUpgradeTypes.h` | 스탯 축 열거형, `DRIsSkillStat()` / `DRIsLowerBetter()`, `FDRResolvedStat{Flat,Mult}`, `AddModifier`/`ApplySkill`(곱연산) |
| `Public/Game/DRProgressionConfig.h` | `FDRTimedSpeedEffect` / `FDRSeedBombChipTuning` / `FDRWaterPumpChipTuning` / `FDRClawChipTuning` / `FDRChipTuning` |
| `Public/DRGameplayTags.h` · `.cpp` | 피처 태그 9개 + GAS 태그 2개 + SetByCaller `Data.MoveSpeed.Mult` / `Data.Duration` / `Data.HealthCost` |
| `AbilitySystem/DRAbilitySystemLibrary` | `GetUpgradeRuntimeForASC` / `HasUpgradeFeature` / `GetChipTuning` / `GetUpgradeKeyTagFromAbilityTags` / `ApplySkillUpgrade` / `ApplyTimedSpeedEffect` / `ApplyTimedTagEffect` / **`ApplyHealToActor`**(오버로드 2개 — `UClass*` 를 넘기면 C2668 모호 에러, `TSubclassOf` 로 감쌀 것) / **`GetHealableAmount`** / **`GetLifestealRatioForDamage`** |
| `Abilities/DRGameplayAbility` | `GetUpgradedFloat` / `GetEffectiveWaterCostFor` / `ApplyCooldown` |
| `Abilities/DRDamageGameplayAbility` | `GetUpgradedDamage()` = `ApplySkill(SkillDamage) × ` **`GetRuntimeDamageMultiplier()`**(가상 훅, 기본 1.0) |
| **`Abilities/DRWaterPump`** | 칩 해석 캐시 `Resolved` / 관통 트레이스 `TraceBeamBlocker` / 대상 수집 `CollectTargetsInBeam` + 칩 규칙 필터 `NarrowTargetsToChipRules` / `ApplyChipDebuffs` / `HasEnemyTarget` + `AdvanceRampStack` + `RampStackHandle` / `ClearRampStacks` / `GrantWaterToAlly` / `GetRuntimeDamageMultiplier` 오버라이드 |
| **`Abilities/DRMeleeAttack`** | `AttackRadius` / `BaseMontagePlayRate` / `GetUpgradedAttackRadius` / `GetWindupPlayRate` / `RestoreMontagePlayRate`(1P·3P 메시 모두) / `TryHealAllyWithOwnHealth` — 피해량 → D-33 으로 자르기 → D-26 으로 자르기 → **비용 먼저, 회복 나중**("깎였으면 회복도 갔다"가 항상 성립) |
| **`Actor/DRSeedProjectile`** | `FDRSeedBombResolved` 캐시 / 직격 판정 `bDirectHitOnEnemy` / `GrantWaterRefund` / 감속·가속 부여 / 아군 회복은 `ApplyHealToActor` 위임(D-34) |
| `Abilities/DRSeedCannon` | `SpentWaterCost` 주입(환급 근거) |
| `ExecCalc/ExecCalc_Damage` | 주는 피해 관문 — `DamageDealt`·광폭화·시너지, **음수 피해 0 클램프** |
| **`DREnemyAttributeSet`** | 받는 피해 관문 — 엘리트 배율 + **`Debuff.Vulnerable` ×1.5** + **흡혈 지급**(★`SetHealth` 전★ — T-24) |
| `DRChipCatalog.cpp` | `ValidateCatalog()` — 스킬 태그/공용 칩/`MaxHealth` Flat 전용/**`Percent ≥ −1.0`** 검사 |
| `UI/DRUpgradeUILibrary.cpp` | 스탯 짧은 이름(5자 제한) — `SkillWindup` = "선딜" |

**BP 쪽 계약** (깨지면 조용히 오동작한다)

| GA | 계약 | 깨지면 |
|---|---|---|
| `GA_WaterPump` | `OnDamageTickReached` 는 **매 호출마다** `Make Damage Effect Params From Class Defaults` 를 부른다(결과 캐시 금지) | 램프 증가가 반영되지 않는다 |
| `GA_WaterPump` | 같은 이벤트의 아군 분기는 `Apply Gameplay Effect To Target` 이 아니라 **`Grant Water To Ally`** | 칩이 반영된 급수량(SetByCaller)이 안 들어간다 |
| `GA_ClawSwipe` | `Play Montage and Wait` **1P·3P 둘 다** `Play Rate` ← `Get Windup Play Rate` | 선딜이 안 늘어난다 / 한쪽만이면 손과 몸이 따로 논다(T-26) |
| `GA_ClawSwipe` | 타격 이벤트(`Event.Montage.ClawSwipe`) 맨 앞에 `Restore Montage Play Rate` — **서버 전용 분기 밖**(모든 머신에서 불려야 한다) | 후딜까지 3배 |
| `GA_ClawSwipe` | `Get Live Objects Within Radius` 와 디버그 구의 `Radius` ← `Get Upgraded Attack Radius` | 범위 칩 무반응 / 그려진 구와 실제 판정이 어긋난다 |
| `GA_ClawSwipe` | `Branch(Is Valid Damage Target)` 의 **False** → `Try Heal Ally With Own Health`(같은 `Array Element`). 칩 유무는 C++ 이 판단하므로 BP 는 **항상** 부른다 | 수혈 무반응 |

---

## 5. 에셋 지도 · 밸런스 튜닝 창구

### 5.1 밸런스를 만지는 곳은 두 군데뿐이다

| | 무엇을 담나 | 예 |
|---|---|---|
| **`DA_ChipCatalog`** | **정적 수치** — 칩이 거는 ±N, ±N% | 피해 −80%, 범위 +50%, 물 −0.8, 선딜 +2.0 |
| **`DA_ProgressionConfig` → `ChipTuning`** | **동적 상수** — 조건·시간·GE 지정 | 환급 비율, 감속 30%/지속, 스택당 5%, 취약 50%/1초, 흡혈 비율 |

★둘 다 재컴파일 없이 바뀐다.★ C++ 에 칩 수치 리터럴은 하나도 없다.

### 5.2 `ChipTuning.SeedBomb`

| 필드 | 값 | 비고 |
|---|---|---|
| `DirectHitRefundRatio` | 1.0 | 직격 시 환급 비율 |
| `RefundEffect` | `GE_WaterGrant` | 적 처치 물 지급용을 **재사용** |
| `EnemySlow` { Effect / Magnitude / Duration } | `GE_SeedBomb_Slow` / 0.3 / 5.0 | |
| `AllyHaste` { … } | `GE_SeedBomb_Haste` / 0.3 / 5.0 | |

### 5.3 `ChipTuning.WaterPump`

| 필드 | 값 | 비고 |
|---|---|---|
| `RampStackEffect` | `GE_WaterPump_Ramp` | |
| `RampDamageBonusPerStack` | 0.05 | 스택당 +5% |
| `RampMaxDamageBonus` | 0.50 | **이 값이 최종 권위** — GE 의 Stack Limit 이 더 커도 코드가 자른다 |
| `RampResetGrace` | 2.0 | ★피해 틱 간격(1초)보다 커야 한다★ |
| `EnemySlow` { … } | `GE_WaterPump_Slow` / 0.3 / **1.2** | ★구조체 기본값이 5.0 이라 반드시 낮춰야 한다★ |
| `VulnerableEffect` | `GE_WaterPump_Vulnerable` | |
| `VulnerableDamageTakenBonus` | 0.50 | ×1.5 |
| `VulnerableDuration` | 1.0 | 피해 틱 간격과 같아 경계에서 한 프레임 끊길 수 있다 → 신경 쓰이면 1.2 |

### 5.4 `ChipTuning.Claw`

| 필드 | 값 | 비고 |
|---|---|---|
| `LifestealRatio` | 1.0 | 적 체력이 실제로 깎인 양 대비 |
| `AllyHealHealthCostRatio` | 1.0 | 실제로 회복시킨 양 대비 체력 소모. 0.5 면 "반값 수혈" |
| `HealEffect` | `GE_Heal` | 흡혈·수혈 **공용**. ★비우면 둘 다 에러 없이 무동작★ |
| `SelfCostEffect` | `GE_Claw_SelfCost` | ★비우면 수혈이 **공짜**★ |
| `AllyHealMinSelfHealth` | 1.0 | ★0 으로 내리지 말 것★ — 체력 0 인 채 생존(D-26) |

### 5.5 GE · GA 에셋 요구 사양

모두 `Content/Blueprints/AbilitySystem/Player/GardenRobot/Abilities/` 아래
(`Skill1` = 기본 공격, `Skill2` = 물대포, `Skill3` = 씨앗 폭탄). `GE_Heal` 만 `Player/Effects/`.

| 에셋 | 핵심 설정 |
|---|---|
| `GE_SeedBomb_Slow` / `_Haste` | HasDuration + SetByCaller(`Data.Duration`) / `MoveSpeed` **`Multiply (Compound)`** + SetByCaller(`Data.MoveSpeed.Mult`) / **Aggregate by Source, Limit 1, Refresh** |
| `GE_WaterPump_Slow` | 위와 동일. Granted Tag 만 `Debuff.WaterPump.Slow` |
| `GE_WaterPump_Ramp` | HasDuration + SetByCaller(`Data.Duration`) / **모디파이어 없음** / Granted Tag `Buff.WaterPump.Pressure` / **Aggregate by Target, Limit 10, Refresh, `Clear Entire Stack`, `Deny Overflow Application` 해제** |
| `GE_WaterPump_Vulnerable` | HasDuration + SetByCaller / **모디파이어 없음** / Granted Tag `Debuff.Vulnerable` / **Aggregate by Source, Limit 1, Refresh** |
| `GE_WaterPump_GrantWater` | Instant / `Water` **`Add Base`** / Magnitude **SetByCaller(`Water.SetByCaller.Grant`)** |
| `GE_Claw_SelfCost` | Instant / `Health` **`Add Base`** / SetByCaller(`Data.HealthCost`) / ★**`Coefficient −1.0`**★ (T-34) |
| `GE_Heal` (재사용) | Instant / `UExecCalc_Heal` + SetByCaller(`Heal`) → `IncomingHealing`. **건드리지 않는다** |
| `GA_WaterPump` | `AllyWaterGrantAmount`(기본 급수량) / `AllyWaterGrantEffect` / `MaxPierceTargets`(8) |
| `GA_ClawSwipe` | `AttackRadius` = 원래 BP 리터럴 값 / `BaseMontagePlayRate` = 원래 PlayRate(1.0) (T-20). ★적 GA 6종에도 같은 필드가 보이지만 읽는 곳이 없다 — 건드리지 말 것★ |

> 태그는 **`Target Tags` 컴포넌트**에 넣는다 — `Asset Tags` 에 넣으면 대상에게 안 붙는다.
> 카탈로그 `SkillDisplayNames` 에 `Abilities.GardenRobot.WaterPump → 물대포`, `…SeedCannon → 씨앗 폭탄`,
> `…ClawSwipe → 기본 공격` 이 있어야 미리보기 라벨이 한글로 나온다.

---

## 6. ★함정★ — 다시 밟지 말 것

실제로 버그를 냈거나, 조건을 한 칸만 틀려도 조용히 오동작하는 것들이다.

| # | 함정 | 왜 위험한가 / 어떻게 피하나 |
|---|---|---|
| **T-1** | ★실제 버그였음★ 씨앗 폭탄이 `BaseDamage` 를 발사체 상수로 **덮어써서** `SkillDamage` 칩이 통째로 무시됐다 | 단점만 사라지고 장점만 남는 칩이 된다. 수치를 액터가 들고 있는 스킬은 **반드시 `ApplySkillUpgrade` 를 태울 것** |
| **T-2** | ★실제 버그였음★ `Percent` 합연산이라 감소 칩 2개가 −120% → 배율 **음수** → **적이 회복** | 곱연산(`Mult`)으로 전환해 구조적으로 차단. `ValidateCatalog` 가 `Percent < −1.0` 을 막고, `ExecCalc_Damage` 가 0 클램프로 이중 방어 |
| **T-3** | ★실제 버그였음★ 스택형 GE 의 스택 수를 `GetGameplayTagCount()` 로 읽으면 **항상 1** | GAS 는 Stacking 이 켜진 GE 를 **액티브 GE 하나로 합치고 StackCount 만** 올린다. 부여 태그는 그 인스턴스에 1회만 더해진다.<br>→ `GetCurrentStackCount(Handle)` 로 읽는다.<br>★칩 ⑦ 시너지의 "태그 카운트 = 스택 수"는 그 GE 가 **`Stacking = None`** 일 때만 성립한다 — **선례의 결론만 옮기지 말 것** |
| **T-4** | `GE_WaterPump_Ramp` 의 **`Deny Overflow Application`** 체크 | 상한 도달 후 적용이 거부되고, 거부되면 **지속시간 갱신도 안 돼** 만렙에서 스택이 통째로 날아간다 |
| **T-5** | `Stack Expiration Policy` 가 `Clear Entire Stack` 이 아니면 | 스택이 **하나씩** 빠져 "2초 뒤 초기화"가 안 된다 |
| **T-6** | 관통 구현을 `LineTraceMultiByChannel` 로 시도 | Multi 도 **첫 Block 에서 멈춘다.** 적 커스텀 히트박스가 `ECC_Visibility` 를 **Block** 으로 잡고 있어서(`DREnemy::SetupHitboxComponents`) 첫 적까지만 나온다 → **무시 목록을 늘려 가며 반복 트레이스** |
| **T-7** | 관통 시 `CalculateWaterBeamEndPoint` 의 **카메라 광선**을 안 고치면 | 조준점이 첫 적에서 확정되어 소켓 트레이스도 짧아진다 → 관통이 "첫 적까지"만 된다. **트레이스 2곳 모두** 고쳐야 한다 |
| **T-8** | 이속 GE 의 ModOp 이 `Multiply (Additive)` | 4인이 같은 적을 감속하면 배율이 0 이 되어 **적이 완전히 멈춘다**. `Multiply (Compound)` 를 쓸 것 |
| **T-9** | 이속/취약 GE 의 Stacking 을 기본값(None)으로 두면 | 연속 적중마다 곱으로 쌓여 적이 멈춘다. `Aggregate by Source` + Limit 1 + Refresh |
| **T-10** | 시한 GE 의 Duration 이 SetByCaller 인데 값을 안 넣으면 | **지속시간 0 으로 즉시 소멸.** `ApplyTimedSpeedEffect` / `ApplyTimedTagEffect` 는 항상 채우므로 **그 두 함수 밖에서 직접 적용하지 말 것** |
| **T-11** | 디버프 지속시간 ≤ 피해 틱 간격 | 틱 경계에서 한 프레임 끊긴다. 물대포 피해 틱은 `TickInterval × DamageApplicationInterval` = 1초 |
| **T-12 / T-29** | `DRIsSkillStat()` 에 신규 축을 빠뜨리면 | 스킬 블록이 아니라 **전역 블록**에 누적되어 **그 캐릭터의 모든 스킬**에 효과가 퍼진다(예: `SkillWindup` 누락 → 정원로봇 모든 스킬의 선딜 3배). `ValidateCatalog` 가 오류를 내주긴 한다 |
| **T-13** | `FDRResolvedStat` 을 `Memzero` / `{0}` 으로 초기화 | `Mult` 기본값이 **1.0** 이라 0 이 되면 모든 수치가 0. `TArray::SetNum()` 은 기본 생성자를 부르므로 안전 |
| **T-14** | `1.f + Stat.Mult` 로 쓰기 | `Mult` 는 **이미 배율**이다. `Percent` 시절 관용구를 옮기면 2배가 된다 |
| **T-15** | 같은 액터의 히트박스가 여러 개면 오버랩이 **중복**으로 잡힌다 | 관통 시 한 적을 여러 번 때린다 → `AddUnique`. (단일 대상 시절엔 최솟값만 골라 드러나지 않던 문제) |
| **T-16** | `OnDamageTickReached` 루프 중 대상이 죽어 무효화 | 매 반복 `IsValid(Target)` 재확인 |
| **T-17** | "적을 맞혔는가"를 **피해 뒤에** 판정 | 마지막 일격으로 적이 죽으면 `IsValidDamageTarget` 이 false 가 되어 **킬 타이밍에 스택이 끊긴다** → `HasEnemyTarget()` 을 피해 **전에** 호출 |
| **T-18** | 물대포 칩 해석을 **서버에서만** 하면 | 클라 1P 빔이 첫 적에서 잘린다 (D-14) |
| **T-19** | `Resolved` 를 Multicast 함수 안에서 쓰기 (씨앗 발사체) | 발사체 `Resolved` 는 **서버에만** 채워진다. 클라는 기본값 — 디버그 드로우를 되살릴 때 주의 |
| **T-20** | `AttackRadius`/`BaseMontagePlayRate` 에 BP 원래 값을 안 옮기면 | 칩이 없어도 사거리·스윙 속도가 **조용히** 바뀐다. BP 리터럴을 손잡이로 옮길 때는 원래 값부터 기록 |
| **T-21** | ★체력 직접 차감은 사망 판정을 타지 않는다★ | `Die()` 는 `IncomingDamage` 경로에만 있다. `Health` 를 직접 깎으면 클램프만 되어 **체력 0 인 채 생존**. 비용으로 깎는 곳은 반드시 가드(D-26 / 피의 계약 `CheckCost`) |
| **T-22** | 아군 판정을 "유효 피해 대상이 아니다"로 뒤집으면 | **클렌저 사이트에 체력을 붓는다** → `!IsNotFriend` 양성 판정(D-27) |
| **T-23** | 흡혈 판정에서 스킬 태그 검사를 빠뜨리면 | **씨앗 폭탄·물대포 피해까지 흡혈**된다(D-24) |
| **T-24** | 흡혈 지급을 `SetHealth()` **뒤에** 두면 | `GetHealth()` 가 이미 깎인 값이라 오버킬 계산이 틀어진다 |
| **T-25** | `SkillWindup` 을 `DRIsTimeStat` 에 넣으면 | UI 가 **"+200초"** 로 표시한다 — 배율이지 초가 아니다(D-21) |
| **T-26** | 1P/3P 몽타주 중 **한쪽만** `PlayRate` 를 바꾸면 | 손과 몸이 따로 논다. 재생도 복구(`RestoreMontagePlayRate`)도 두 메시 모두 |
| **T-27** | 선딜 ×3 이 쿨다운보다 길어지면 | 입력이 씹히거나 스윙이 끊긴다 — `CooldownDuration` 과 몽타주 길이를 같이 볼 것 |
| **T-28** | 한 스윙에 아군 여러 명이면 | 체력이 **인원수만큼** 나간다(의도된 동작). ③과 겹치면 급격하다 |
| **T-30** | `EDRUpgradeStat` 을 중간에 끼우면 | 카탈로그에 직렬화된 **기존 칩 효과가 밀린다** → 반드시 `Count` 바로 앞(D-4 / D-17) |
| **T-31** | 흡혈 블록은 **적이 맞는 모든 피해**(근접·원거리·DoT·적끼리)가 지나는 지점에 있다 | 칩이 없으면 첫 줄에서 0 이지만, 이 근처를 고친 뒤 회귀 확인은 근접만이 아니라 **적 피해 전반**으로 |
| **T-32** | 공유 베이스(`UDRMeleeAttack`)에 프로퍼티를 추가할 때 파생 BP 에 **같은 이름의 변수**가 있으면 | 그 BP 컴파일이 깨진다. 추가 전에 파생 GA 전체를 이름으로 검색(이번엔 7개 GA 충돌 0건) |
| **T-33** | ★`ApplyHealToActor()` 는 대상을 가리지 않는다★ (팀·사이트 판정 없음) | 거르는 건 **호출부 책임**. 현재 두 호출부(씨앗 발사체·기본 공격)는 사이트 제외 + `!IsNotFriend` 를 앞에 둔다 — 새 호출부도 똑같이 |
| **T-34** | `GE_Claw_SelfCost` 의 `Coefficient` 가 −1 이 아니면 | 코드는 `Data.HealthCost` 에 **양수**를 넣으므로, 아군을 때릴 때마다 **내 체력이 오히려 회복**된다 |

---

## 7. 다음 칩을 추가하는 레시피

| 유형 | 예 | 해야 할 일 |
|---|---|---|
| **1. 순수 수치** (C++ 0줄) | "물대포 쿨다운 −20%" | 카탈로그에 `{Stat, TargetAbilityTag, Op, Value, bIsDrawback}` 한 줄. **조건**: 그 스탯을 이미 `Apply()`/`ApplySkill()` 로 읽고 있어야 한다 |
| **2. 새 수치 손잡이** (2~3줄) | "제트점프 높이 +30%" | ① `EDRUpgradeStat` 에 축 추가(**`Count` 바로 앞**) ② `DRIsSkillStat()` 등록 ③ 그 숫자를 쓰는 곳을 감싼다 — 어빌리티면 `GetUpgradedFloat`, 액터면 `ApplySkillUpgrade`, BP 리터럴이면 유형 7 ④ (선택) 5자 짧은 이름 |
| **3. 규칙 변경** (10~30줄) | "○○ 적중 시 감속" | ① `Upgrade.Feature.*` 태그 1개 ② `FDRChipTuning` 아래 구조체 ③ 적용 지점에서 `HasUpgradeFeature()` 분기 ④ 카탈로그의 `GrantedTags` 에 태그 ⑤ Ascension 이면 `Description` 에 단점 명시(검증이 강제) |
| **4. 런타임 상태로 변하는 수치** (5~15줄) | "연속 명중 시 강해짐" | ① 상태를 **스택형 GE** 로 ② `GetRuntimeDamageMultiplier()` 오버라이드 ③ 스택을 쌓는/지우는 지점 결정.<br>★`Stacking` 을 켰으면 `GetCurrentStackCount(Handle)`, `None` 이면 `GetGameplayTagCount(Tag)`★ (T-3) |
| **5. 대상에게 붙는 표식** (4줄 + GE 1개) | "표식 찍힌 적은 받는 피해 증가" | ① `Debuff.<이름>` 네이티브 태그 + 태그만 부여하는 시한 GE ② `ApplyTimedTagEffect()` 로 부여 ③ **읽는 곳은 `HandleIncomingDamage`** — `ExecCalc_Damage` 가 아니다(D-10) |
| **6. 맞는 쪽에서 정산하는 효과** (15~25줄) | "준 피해만큼 흡혈" | ① 판정 함수를 라이브러리에 — 피처 태그 **AND** 컨텍스트 `SourceAbilityTags`(D-24) ② `UDREnemyAttributeSet::HandleIncomingDamage` 에서 **`SetHealth` 전에** 지급(T-24) ③ 회복은 `ApplyHealToActor()` 재사용 — 대상 거르기는 호출부 몫(T-33) |
| **7. BP 로직뿐인 어빌리티에 손잡이 달기** (C++ 함수 + BP 재배선) | "기본 공격 범위·속도" | ① BP 리터럴을 C++ `EditDefaultsOnly` 프로퍼티로 옮기고 **원래 값을 그대로** 적는다(T-20) ② `BlueprintPure` 조회 함수를 BP 핀에 연결 ③ 켜짐/꺼짐 판단은 C++ 안에서 — BP 는 **항상** 부른다 ④ 공유 베이스면 자동 실행 경로를 건드리지 않는다(D-35 / T-32) |

---

## 8. 🚫 하지 말 것

| 금지 | 이유 |
|---|---|
| 어빌리티/액터에 `if (칩ID == "GR.Asc.Xxx")` 분기 | `ChipId` 는 세이브 키다. 코드가 알면 리네임이 불가능해지고 밸런스 담당자가 손댈 수 없다 |
| 칩 수치를 C++ 상수로 박기 | 재컴파일 없이 못 바꾼다. 정적=카탈로그 / 동적=`ChipTuning` |
| 칩 피처 태그를 ASC 에 Loose 로 얹기 | 어빌리티의 `ActivationBlockedTags` 와 의도치 않게 상호작용 |
| `FDRUpgradeRuntime` 복제 | 각 머신이 `EquippedChips` 로 각자 만든다. 복제하면 동기화 지점이 둘이 된다 |
| 받는 피해 배율·흡혈을 `ExecCalc_Damage` 에 넣기 | 디버프 DoT 이 그 경로를 안 타고(D-10), 받는 쪽 배율이 끝난 최종 피해도 모른다(D-22) |
| 체력을 **비용으로** 깎는 데 `IncomingDamage` 쓰기 | 컨테이너·히트리액트·전투 진입·부품 낙하가 전부 발동한다(D-25) |
| 아군을 "유효 피해 대상이 아님"으로 판정 | 클렌저 사이트가 아군으로 잡힌다(D-27) |
| 공유 베이스(`UDRMeleeAttack`)의 생성자·`ActivateAbility`·가상 함수에 칩 로직 넣기 | 적 GA 6종의 공격이 통째로 바뀐다(D-35) |
| 채널링 상태를 복제 안 되는 멤버에 담기 | 만료·정리를 직접 관리해야 하고 UI 가 못 본다 |
| 폭발/빔 연출을 "넓은 버전 / 좁은 버전" 에셋 2개로 나누기 | 범위는 **연속값**이다(칩이 늘면 곱해진다). 나이아가라 `RadiusScale` User 파라미터로 분기할 것 |
| `IsValidDamageTarget` 으로 "빔을 막는가" 판정 | 죽은 적을 false 로 줘서 **시체가 빔을 막는다** |

---

## 9. 남은 이슈 / 알려진 제약

| # | 내용 | 현재 판단 |
|---|---|---|
| **1** | **비행 적(`ADRFlyingEnemy`)에게 감속이 안 걸린다.** `MOVE_Flying` + `MaxFlySpeed` 를 쓰는데 `PreAttributeChange` 는 `MaxWalkSpeed` 만 건드리고, `OnMoveSpeedChanged` 오버라이드가 어디에서도 바인딩되지 않는다 | **범위 밖.** 고치려면 적 쪽에도 어트리뷰트 변경 델리게이트를 바인딩해야 하는데, `MaxFlySpeed` 와 `MoveSpeed` 의 기준값이 달라 회귀 위험이 있다 |
| **2** | **물대포 ②(피해 0)는 적 어그로를 끌지 않는다.** `HandleIncomingDamage` 가 `> 0` 가드 안에서 `FirstAttacker`/`SetFocus`/전투 진입을 처리하기 때문 | **의도된 동작으로 둔다**(지원 전담 빌드). 바꾸려면 피해를 0 대신 −0.95 로 낮춰 1 이라도 남기는 방법이 가장 싸다(카탈로그 값) |
| **3** | 물 부족 시 체력 대납 비율 `0.5` 가 `DRGameplayAbility.cpp` 와 `ExecCalc_WaterCost.cpp` **두 곳에 중복** | 한쪽만 고치면 "쓸 수 있다고 했는데 체력이 0" 사고. Config 이관 후보 |
| **4** | 씨앗 폭탄 직격 환급은 체력으로 대납한 경우에도 **전액을 물로** 돌려준다 | 소폭 이득. 허용 |
| **5** | 씨앗 폭탄 시전자 본인은 회복·가속 대상이 아니다 (`AddIgnoredActor(SourceActor)`) | 기존 동작 유지 |
| **6** | `GA_SeedCannon.Damage`(FScalableFloat)는 **사문(死文)** — 실제 피해는 발사체의 Inner/OuterDamage | 정리하려면 발사체 상수를 GA 로 옮기는 별도 리팩터 |
| **7** | 취약(`Debuff.Vulnerable`)은 **적에게만** 동작한다. `UDRPlayerAttributeSet` 에는 넣지 않았다 | 아군에게 거는 스킬이 생기면 같은 4줄을 추가 |
| **8** | 취약에 **시각 연출이 없다** | GameplayCue 1개 + 나이아가라면 붙는다 |
| **9** | 물대포 램프 스택 **HUD 표시 없음** | `DA_StatusEffectInfo` 에 `Buff.WaterPump.Pressure` 항목(이름+아이콘) 한 줄이면 **코드 변경 없이** 붙는다 — `ClientEffectApplied` 가 StackCount 를 이미 흘려보낸다 |
| **10** | `[MoleDiag]` 임시 로그 6개(A~F)가 남아 있다 — `DRAbilitySystemLibrary.cpp`(A·B·D·F), `DRDamageGameplayAbility.cpp`(E), `DRS2Mole.cpp`(C). ★A·B 는 `GetLiveObjectsWithinRadius` 안이라 **기본 공격 스윙마다** 찍힌다★ | 물대포 경로의 것은 제거했다. 남은 것은 두더지 이슈와 공유라 별도 판단 — 지운다면 6개를 한 번에 |
| **11** | 튜토리얼 더미를 때려도 흡혈된다(흡혈 블록이 더미 분기보다 위에 있다) | 연습용이므로 허용 |
| **12** | 발톱 ③으로 범위가 +30% 여도 **스윙 이펙트는 그대로**다 | 그대로 둔다. 바꾸려면 나이아가라 `RadiusScale` User 파라미터("하지 말 것" 참고 — 에셋을 둘로 나누지 말 것) |
| **13** | 기본 공격은 **클렌저 사이트에도 적중 처리**된다(`IsValidDamageTarget` 이 true) | Plan5 이전부터의 동작이라 건드리지 않았다. 수혈은 D-27 로 사이트를 거른다 |

---

## 10. 발톱칩 게임 내 검증 — ★미확인★

에디터 작업(`GE_Claw_SelfCost` · `GA_ClawSwipe` 재배선 · `ChipTuning.Claw` · 카탈로그 3종)은 에셋에 반영된 것까지 확인했다.
아래는 **플레이로만** 확인할 수 있는 핵심 항목이다. **전부 통과하면 이 절을 지우고 머리말의 상태를 갱신한다.**

- [ ] **회귀(칩 없음)** — 기본 공격 피해·판정 범위·스윙 속도가 이전과 같다(T-20) / 아군을 때려도 아무 일 없다 /
      원거리·DoT·엘리트·Phase1 감쇠·처치 물 지급 등 **적 피해 전반**이 그대로다(T-31)
- [ ] **①** 피해 정확히 5배 / 선딜만 느리고 **후딜은 원속** / 1P·3P 속도 일치 / 연타해도 꼬이지 않는다(T-26, T-27)
- [ ] **② 적** 준 피해만큼 회복 / 빈사 적은 남은 체력만큼만(D-22) / 취약 걸린 적은 흡혈 ×1.5
- [ ] **② 적** ★씨앗 폭탄·물대포로는 흡혈되지 않는다★(D-24)
- [ ] **② 아군** 아군 +, 나 − 같은 양(★반대로 움직이면 T-34★) / ★만피 아군이면 체력이 안 나간다★(D-33) /
      내 체력 1 이면 무동작(D-26) / 오염된 아군은 정화된다 / 클렌저 사이트를 때려도 체력 불변(D-27)
- [ ] **②+⑥** 흡혈이 전혀 없다
- [ ] **③** 기존 사거리 밖 적이 맞는다 / 피해 80% / 씨앗 폭탄 폭발 범위는 그대로
- [ ] **멀티** 클라가 ②로 호스트를 수혈 → 호스트 +, 클라 − / 클라의 ① 스윙이 다른 화면에서도 느리게 보인다

---
---

# 2부 — 자판기 기본 공격(동전) 칩 3종 · 구현 계획

> **상태: 계획 (미착수)** — 2026-09-27 작성 / 브랜치 `feat/upgradeskill`
> 코드 주석 접두 **`Plan5(동전칩)`** · 결정 **D-36 ~ D-46** · 함정 **T-35 ~ T-45**
> (1부 번호에 ★이어서★ 매긴 영구 ID다. 2부가 1부에 흡수돼도 번호는 바뀌지 않는다.)
>
> 1부의 배관(§2)을 그대로 쓴다. 새로 까는 것은 **스탯 축 2개 · 피처 태그 1개 · 튜닝 구조체 1개 · 판정 함수 1개** 뿐이고,
> **BP 재배선 · 새 GE · 새 C++ 파일은 없다**(D-45).
>
> 2부 안에서는 절을 **V0 ~ V10** 으로 부른다(1부의 §1~§10 과 섞이지 않게).
> ★코드 주석에는 V 번호도 쓰지 않는다★ — 1부 규칙대로 D-n / T-n 만 쓴다.

---

## V0. 한눈에 보기

### V0.1 요구사항 (원문 → 해석)

| # | 원문 (2026-09-27) | 해석 |
|---|---|---|
| ① | 데미지 20% 증가, 발사 주기 ~~0.5초~~ → **1.0초**로 변경 | 동전 한 발 피해 ×1.2. 발사 간격 0.6초 → **1.0초**(느려짐 = ★단점★) |
| ② | 일반 공격이 적에게 적중할 때마다 체력 2 회복, 데미지 30% 감소 | 적에게 맞은 ★1회마다★ 체력 **+2**(고정량). 피해 ×0.7 |
| ③ | 투사체 크기 30% 증가(판정 범위도 증가), 데미지 30% 감소 | 투사체 외형 **×1.3** + 충돌 구 반경 **×1.3**. 피해 ×0.7 |

사용자 확정 사항 (같은 날 질의응답):

- ①의 발사 주기는 처음 요청의 0.5초가 아니라 **1.0초**다 — 현재값이 0.6초라 0.5초면 단점이 사라지기 때문이다(V0.2).
- **잭팟 캡슐도 "일반 공격"** 이다 — 세 칩을 캡슐에도 똑같이 적용한다(D-39).
- **오염 중에는 ② 적중 회복을 막는다**(D-42).

### V0.2 기존 발사 주기 — 조사 결과

**실제 발사 간격은 0.6초다.**

- `UDRVendingMachineBasicAttack::BaseFireInterval` 의 C++ 기본값은 **0.3초**(`DRVendingMachineBasicAttack.h`)지만,
  **`GA_VendingMachineBasicAttack` BP 가 0.6초로 덮어쓴다**(에셋 CDO 직렬화 값을 직접 읽어 확인).
- 공격 속도 버프(스킬 2, `GE_AttackSpeedBuff`)가 걸리면 `0.6 ÷ (1 + 0.2 × 스택)`. Stack Limit 이 10 이라
  최단 **0.2초**다. (헤더 주석의 "+10%" 는 낡았다 — 코드는 스택당 0.2 다 → V10-④)
- 따라서 ①을 끼면: 버프 없음 **1.0초**, 버프 10스택 **0.333초**.

### V0.3 현재 기본 공격 실측값 (에셋 CDO 에서 읽은 값)

| 항목 | 값 | 비고 |
|---|---|---|
| 발사 간격 `BaseFireInterval` | **0.6초** | C++ 0.3 → BP 가 덮어씀 |
| 동전 피해 `Damage` | **20** (`Damage.Physical`) | `FScalableFloat` 상수(커브 없음) |
| 잭팟 주기 `MaxJackpotStacks` | 동전 **10**발 뒤 11번째가 캡슐 | C++ 5 → BP 10 |
| 캡슐 피해 / 확률 | 브론즈 10 (50%) / 실버 **50** (40%) / 골드 150 (10%) | 실버만 BP 값, 나머지는 C++ 기본값 |
| 동전 투사체 `BP_Coin` | Sphere 반경 **24.3uu** / 메시 스케일 0.2 / 속도 2000 / **중력 1.0**(포물선) | 부모 `ADRProjectile` |
| 캡슐 투사체 `BP_*Capsule` | Sphere 24.9 / 21.7 / 22.8uu, 속도 2000, 중력 1.0 | 브론즈·실버 메시는 구 뒤로 −120 / −61uu 오프셋 |
| 넉백 / 디버프 / 물 소모 | 전부 0 | |
| 실행 정책 | LocalPredicted(기본값) — 단 **발사(`ExecuteShot`)는 서버 전용** | |

평균 DPS(버프 없음): 동전만 **33.3** / 캡슐 포함 **36.4**(한 발 평균 21.8 = (20 × 10 + 캡슐 기대값 40) ÷ 11).

### V0.4 무엇을 새로 만들고 무엇을 재사용하나

| 구분 | 항목 | 어디 |
|---|---|---|
| **신규** | 스탯 축 `SkillFireInterval`(초) · `SkillProjectileScale`(배율) | `DRUpgradeTypes.h` |
| **신규** | 피처 태그 `Upgrade.Feature.Skill.Coin.HitHeal` | `DRGameplayTags` |
| **신규** | `FDRCoinChipTuning`(`HitHealAmount` / `HealEffect`) + `FDRChipTuning::Coin` | `DRProgressionConfig.h` |
| **신규** | `GetHitHealAmountForDamage()` — 발톱 흡혈 판정의 형제 | `DRAbilitySystemLibrary` |
| **신규** | `UDRProjectileSpell::GetUpgradedProjectileScale()` | `DRProjectileSpell` |
| **신규** | `UDRVendingMachineBasicAttack::MinFireInterval` | `DRVendingMachineBasicAttack.h` |
| 수정 | `GetCurrentFireInterval()` · 캡슐 스폰 분기 · `SpawnProjectile()` · `HandleIncomingDamage()` · 카드 짧은 이름 2개 | |
| 재사용 | `SkillDamage` · `ApplyHealToActor` · `GetHealableAmount` · `GE_Heal` · `HasUpgradeFeature` · `GetChipTuning` | 1부 배관 |
| **없음** | BP 재배선 · 새 GE · 새 GAS 태그 · 새 C++ 파일 | D-45 |

### V0.5 수치가 흐르는 경로

```
[DA_ChipCatalog]  VM.Asc.CoinHeavy / CoinDrain / CoinWide
                  (모디파이어마다 TargetAbilityTag = Abilities.VendingMachine.BasicAttack)
   │
   ├─ SkillDamage (+0.2 / −0.3 / −0.3)
   │     ├─► GetUpgradedDamage() ──────────────────────────► 동전 BaseDamage             (기존 배관)
   │     └─► GetUpgradedFloat(SkillDamage, 캡슐 피해) ───────► 캡슐 BaseDamage             ★신규 D-39★
   │
   ├─ SkillFireInterval (Flat +0.4초) ─► GetCurrentFireInterval() ÷ (1 + 0.2 × 버프) ─► SetTimer   ★D-37/38/44★
   │
   ├─ SkillProjectileScale (+30%) ────► GetUpgradedProjectileScale() ─► SpawnTransform.Scale3D (동전·캡슐)  ★D-40★
   │
   └─ GrantedTags: Upgrade.Feature.Skill.Coin.HitHeal
            │
            ▼  (적이 피해를 받는 순간 — 서버)
   UDREnemyAttributeSet::HandleIncomingDamage
      └─► GetHitHealAmountForDamage(Props) ─► GetHealableAmount ─► ApplyHealToActor(시전자, 2)   ★D-41/42/43★

[DA_ProgressionConfig.ChipTuning.Coin]   HitHealAmount = 2 / HealEffect = GE_Heal
```

---

## V1. 칩 3종 최종 스펙

공통: `OwnerClass = VendingMachine`, `bCommonChip = false`, `Category = Ascension`, `RequiredSlotCount = 1`,
칩 레벨 `TargetAbilityTag = Abilities.VendingMachine.BasicAttack`.
★**모디파이어마다** `TargetAbilityTag` 를 같은 태그로 지정한다★ — 빠뜨리면 자판기의 모든 스킬에 퍼진다
(1부의 `GR.Asc.SeedSupport` 가 실제로 이렇게 빠져 있다 → V10-①).

| # | `ChipId` | 이름(가칭) | Modifiers | GrantedTags | Tier |
|---|---|---|---|---|---|
| ① | `VM.Asc.CoinHeavy` | 고액 동전 | `SkillDamage` **+0.2**<br>`SkillFireInterval` **Flat +0.4** ★단점★ | (없음) | 3 |
| ② | `VM.Asc.CoinDrain` | 거스름 동전 | `SkillDamage` **−0.3** ★단점★ | `…Coin.HitHeal` | 4 |
| ③ | `VM.Asc.CoinWide` | 대형 동전 | `SkillProjectileScale` **+0.3**<br>`SkillDamage` **−0.3** ★단점★ | (없음) | 3 |

- `ChipId` 는 1부 발톱 세트(`GR.Asc.Claw{Heavy,Drain,Wide}`)와 1:1 로 맞췄다(D-36). **세이브 키라 이 이름으로 확정**한다.
  표시명·설명은 가칭이다 — `DisplayName` 은 언제 바꿔도 세이브에 영향이 없다(VO-2).
- ① 동전·캡슐 피해 ×1.2. 발사 간격 0.6 → **1.0초**. 공격 속도 버프는 그 위에 그대로 걸린다(D-38).
  ★`Flat +0.4` 는 "1.0초 고정"이 아니라 "기준 +0.4초"다★ — BP 의 0.6 을 바꾸면 결과도 바뀐다(T-38).
- ② 동전·캡슐이 **적에게** 맞을 때마다 시전자 체력 **+2**. 피해량·취약·엘리트와 무관한 고정량이다(D-43).
  벽·바닥·클렌저 사이트·아군·죽은 적에게는 없다(D-41). **오염 중이거나 회복 차단(⑥)이면 없다**(D-42).
- ③ 동전·캡슐의 외형과 충돌 구가 함께 **×1.3**(동전 반경 24.3 → 31.6uu). 판정이 넓어지는 만큼
  **벽·바닥에도 먼저 닿는다**(T-43). 임팩트 이펙트 크기는 그대로다(V9-1).
- 카탈로그 표기: ①의 발사 간격 모디파이어는 `DetailOverride = 발사 간격 1.0초 (기존 0.6초)`.
  ②의 `Description` 에 "오염 상태이거나 회복 차단 효과가 있으면 회복되지 않습니다" 를 명시한다.

### V1.1 조합 결과 (곱연산이라 순서 무관)

기준: 동전 피해 `D = 20`, 발사 간격 `T = 0.6초`, 크기 `S = 1.0`(반경 24.3uu), 적중 회복 `H = 2`. 공격 속도 버프 0스택.

| 조합 | 한 발 피해 | 발사 간격 | 크기(반경) | 적중 회복 | 동전 DPS |
|---|---|---|---|---|---|
| (없음) | 20 | 0.6 | 1.0 (24.3) | — | 33.3 |
| ① | **24** | **1.0** | 1.0 | — | **24.0 (×0.72)** ⚠️ VO-1 |
| ② | **14** | 0.6 | 1.0 | 2/발 (**3.3/초**) | 23.3 (×0.70) |
| ③ | **14** | 0.6 | **1.3 (31.6)** | — | 23.3 (×0.70) |
| ①+② | 16.8 | 1.0 | 1.0 | 2/발 (**2.0/초**) | 16.8 (×0.50) |
| ①+③ | 16.8 | 1.0 | 1.3 | — | 16.8 (×0.50) |
| ②+③ | **9.8** | 0.6 | 1.3 | 2/발 (3.3/초) | 16.3 (×0.49) — ★−60% 가 아니라 ×0.49★ |
| ①+②+③ | 11.76 | 1.0 | 1.3 | 2/발 (2.0/초) | 11.8 (×0.35) |

- 잭팟 캡슐은 **한 발 피해 배율을 그대로** 받는다: 브론즈 / 실버 / 골드 10 / 50 / 150 → ① 12 / 60 / 180, ②③ 각각 7 / 35 / 105.
- 공격 속도 버프 n스택이면 모든 행의 발사 간격 ÷ (1 + 0.2n) — ① + 10스택 = **0.333초**.
- 잭팟은 "발사 수" 기준(동전 10발마다)이라 ①을 끼면 **초당 캡슐 수도 40% 줄어든다.**
- 초당 회복은 발사 간격에 반비례한다 — ①+②는 한 발 회복량은 같지만 초당 회복이 3.3 → 2.0 으로 준다.

### V1.2 공용 칩·다른 시스템과의 상호작용

| | 효과 |
|---|---|
| 공격 속도 버프 (스킬 2) | 발사 간격 ÷ (1 + 0.2 × 스택, 최대 10). ①과 ★곱으로★ 합성(D-38) — ①의 단점을 버프로 메우는 운용이 된다 |
| ⑥ 광폭화 / ⑦ 시너지 | `ExecCalc_Damage` 소스측에서 동전·캡슐 피해에 ★추가로★ 곱해진다. ② 회복량(고정 2)은 영향 없음 |
| ⑥ 광폭화 + ② | ★② 완전 무력화★ — 회복 차단(`NoHealing`)이라 `GetHealableAmount` 가 0 → GE 자체를 걸지 않는다 |
| ⑧ 피의 계약 | 기본 공격은 물 소모가 0 이라 무관 |
| 오염 상태 | ② 차단(D-42). ①③ 은 무관 |
| 물대포 ②(취약) → 동전 | 취약 적에게 동전·캡슐 피해 ×1.5. ② 회복은 고정 2 그대로 |
| 발톱 흡혈(1부 ②) | 무관 — 흡혈은 `ClawSwipe` 태그, 적중 회복은 `VendingMachine.BasicAttack` 태그로 판정해 서로 안 섞인다 |

---

## V2. 현재 코드 기준 사실 — 어디를 건드려야 하는가

줄 번호는 2026-09-27 기준이다(작업하면서 밀린다 — 함수 이름으로 찾을 것).

### V2.1 발사 주기의 실체 — 서버 타이머

```
GA_VendingMachineBasicAttack (BP)
  K2_ActivateAbility ─► StartAutoFire()            (LocalPredicted 라 서버·소유 클라 둘 다 부른다)
  WaitInputRelease   ─► StopAutoFire() ─► K2_EndAbility

UDRVendingMachineBasicAttack  (DRVendingMachineBasicAttack.cpp)
  StartAutoFire()            :14   마지막 발사 후 간격이 지났으면 즉시 ExecuteShot() + SetTimer(간격)
  FireShotAndScheduleNext()  :52   발사 → ★매번★ GetCurrentFireInterval() 재계산 → SetTimer (버프 즉시 반영)
  ExecuteShot()              :66   ★HasAuthority 가 아니면 return★ → 실제 발사 수는 서버 타이머가 정한다
  GetCurrentFireInterval()   :167  return BaseFireInterval / (1 + 0.2 × 버프 스택)
```

- 발사 간격 칩은 **`GetCurrentFireInterval()` 한 곳**만 고치면 된다 — 첫 발 판정(`StartAutoFire`)과
  이후 스케줄링(`FireShotAndScheduleNext`)이 모두 이 함수를 부른다.
- 클라 쪽 타이머도 돌지만 `ExecuteShot` 이 첫 줄에서 빠지므로 **발사 수는 서버가 정한다**(T-44).
- ⚠️ `SetTimer` 는 Rate ≤ 0 이면 ★타이머를 지운다★ — 계산이 0 이하가 되면 연사가 조용히 멈춘다(T-35).

### V2.2 동전과 잭팟 캡슐은 스폰 경로가 ★둘★ 이다

| | 동전 (10발) | 잭팟 캡슐 (11번째) |
|---|---|---|
| 스폰 | `SpawnProjectile(TargetLocation, FireSocketTag)` — 부모 `UDRProjectileSpell` 공용 함수 | `ExecuteShot()` 안에서 ★직접★ `SpawnActorDeferred` (:114) |
| 피해 | `MakeDamageEffectParamsFromClassDefaults()` → `BaseDamage = GetUpgradedDamage()` ✅ `SkillDamage` 반영 | 같은 함수로 만든 뒤 **`BaseDamage = SelectedDamage`(상수)로 덮어쓴다** (:121) ❌ `SkillDamage` 무시 |
| 크기 | (손잡이 없음) | (손잡이 없음) |

- 캡슐의 상수 덮어쓰기는 **1부 T-1 과 같은 누락**이다. 청소기 `UDRVacuumAirShot::FireShot`(:119)은 같은 상황에서
  `GetUpgradedFloat(SkillDamage, StageData.Damage)` 로 보정하고 있다 → **같은 방식으로 고친다**(D-39).
- 두 경로가 따로라 ★크기와 피해를 두 곳에 다 넣어야 한다★(T-36).
- 캡슐도 피해 컨텍스트에 `SourceAbilityTags = Abilities.VendingMachine.BasicAttack` 이 실린다
  (`MakeDamageEffectParamsFromClassDefaults` 가 `GetAssetTags()` 를 넣고 `ApplyDamageEffect` 가 컨텍스트로 옮긴다)
  → ② 판정은 캡슐에도 코드 추가 없이 걸린다.

### V2.3 투사체 판정 — `ADRProjectile`

- 루트 = `USphereComponent Sphere`(QueryOnly, WorldStatic · WorldDynamic · Pawn 에 Overlap).
- `OnSphereOverlap`: 자기 자신·아군(`!IsNotFriend`)은 무시 → 그 외 **첫 겹침**에서 이펙트 + (서버) 대상에 ASC 가 있으면
  `ApplyDamageEffect` → `Destroy()`. ★관통이 없다 — 한 발은 최대 한 번 적중한다.★ 벽·바닥도 "겹침"이라 거기서 사라진다.
- 스폰은 `SpawnActorDeferred(…, SpawnTransform, …)` → `FinishSpawning(SpawnTransform)` 이다.
  이 트랜스폼에 Scale3D 를 넣으면 루트 Sphere 와 자식 StaticMesh 가 ★같이★ 커진다
  (코인·캡슐 BP 의 루트 스케일이 1.0 이라 엔진의 스케일 합성 방식과 무관하게 결과가 같다)
  → "투사체 크기 + 판정 범위"가 한 줄로 해결된다(D-40).
- 스폰 직후의 겹침도 판정된다 — `AActor::DispatchBeginPlay` 가 `BeginPlay()`(오버랩 바인딩) ★다음에★
  `UpdateInitialOverlaps()` 를 부른다(UE 5.5 엔진 소스 확인). 그래서 총구 바로 앞의 적은 맞고, 총구가 벽에 붙어 있으면 즉시 사라진다.
- 복제: `bReplicates` + `SetReplicateMovement(true)`. 스폰 스케일은 새 액터 스폰 정보에 실려 클라로 간다 —
  `UPackageMapClient::SerializeNewActor` 가 `bSerializeScale` 이면 스케일을 보내고 클라가 `SetActorRelativeScale3D` 로 적용한다
  (UE 5.5 엔진 소스 확인, 이 프로젝트는 Iris 미사용). 눈으로는 멀티 테스트 M-2 로 확인한다.
- `SpawnProjectile` 을 쓰는 다른 GA: C++ `UDRVacuumAirShot`, 적 BP `GA_RangedAttack` · `GA_DragonFly_BasicAttack` ·
  `GA_DragonFly_Skill`, Player 폴더의 구형 `GA_FireBolt`(`BP_DRCharacter` 가 참조). 적 GA 는 칩 캐시가 비어 있고
  플레이어 GA 는 자기 태그를 대상으로 한 칩이 없어서 스케일이 항상 1.0 이다(D-40).

### V2.4 "적에게 적중" = `UDREnemyAttributeSet::HandleIncomingDamage`

- 이 어트리뷰트셋을 가진 것은 **`ADREnemy` 계열뿐**이다(`DREnemy.cpp:65`). 클렌저 사이트는 전용 AS, 벽·바닥은 ASC 가 없다.
- 죽은 적은 `UDRAttributeSet::PostGameplayEffectExecute` 가 먼저 거르고, 0 피해는 `LocalIncomingDamage > 0` 가드가 거른다.
- 홀로그램 두더지(`IDRProximityHitOnly`)는 `ApplyDamageEffect` 에서 가로채져 GE 가 안 걸린다 → 여기 오지 않는다.
- 가드 안의 순서(:18~): 히트 큐 → 전투 진입 → 엘리트 ×0.7 → 취약 ×1.5 → **발톱 흡혈(:81~92, `SetHealth` 전)** →
  튜토리얼 더미 분기(조기 return) → `SetHealth` → AI 어그로 → 사망 / 넉백.
- 흡혈 판정은 `GetLifestealRatioForDamage(Props)`(`DRAbilitySystemLibrary.cpp:419`) — 피처 태그 AND 컨텍스트 스킬 태그.
  ② 는 ★이 함수의 형제★ 로 만들고 흡혈 블록 바로 아래에서 지급한다(D-41).

### V2.5 회복 경로와 오염 — 왜 D-42 가 필요한가

`ApplyHealToActor` → `GE_Heal`(Instant · `UExecCalc_Heal` · 게임플레이 큐 없음) → `IncomingHealing` →
`UDRPlayerAttributeSet::HandleIncomingHealing`(:376):

| 시전자 상태 | 결과 |
|---|---|
| 회복 차단(⑥ `NoHealing`) | 힐을 통째로 버린다 |
| **오염** | `HandleCorruptionPurification`(:420) — ★회복량을 보지 않는다★: 오염 해제 + `MaxHealth` 복구 + `Health = ContainerHealth`(100) |
| 그 외 | `Health = min(Health + 양, MaxHealth)` |

→ ② 를 그대로 두면 **"오염 상태에서 적 1회 적중 = 즉시 정화 + 체력 100"** 이 된다.
오염 해제는 원래 **아군의 회복**으로 하는 팀 메커니즘이므로 ② 쪽에서 막는다(D-42).
(발톱 흡혈에도 같은 구멍이 있다 → V9-3)

### V2.6 BP 는 건드리지 않는다

`GA_VendingMachineBasicAttack` 그래프가 부르는 C++ 은 `StartAutoFire` · `StopAutoFire` · `K2_EndAbility`,
이벤트는 `OnCapsuleShotFired`(캡슐 몽타주) · `OnNormalShotFired` · `OnJackpotStacksChanged` 뿐이다.
`GetCurrentFireInterval` · `SpawnProjectile` 은 BP 가 부르지 않는다 → **칩 반영은 전부 C++ 안에서 끝난다**(D-45).

---

## V3. 설계 결정 (D-36 ~ D-46)

| # | 결정 | 근거 |
|---|---|---|
| **D-36** | 3종 모두 자판기 전용 `Ascension`. `ChipId = VM.Asc.Coin{Heavy,Drain,Wide}`, Tier 3 / 4 / 3 | 1부 발톱 세트와 1:1 대응이라 UI·밸런스 비교가 쉽다. 셋 다 `bIsDrawback` 모디파이어를 가져 D-1/D-19 규칙(단점 필수)을 지킨다. Plan2 명명 규칙 `{로봇}.{Stat\|Asc}.{이름}` |
| **D-37** | 발사 간격 = **새 축 `SkillFireInterval`(초)**. ①은 `Flat +0.4`. `SkillCooldown` 재사용·배율 축은 기각 | ⓐ `SkillCooldown` 은 `ApplyCooldown` 이 쿨다운 GE 에 쓰는 축이다 — 기본 공격에 쿨다운 GE 가 붙는 순간 ★같은 칩이 이중으로★ 걸리고, 카드에도 "쿨다운" 으로 뜬다. ⓑ 발사 간격은 GA 프로퍼티라 어빌리티가 기준값을 초 단위로 정확히 안다 — 기준을 몰라 배율로 간 선딜(D-21)과 반대 상황이라, 요구 원문("1.0초")을 그대로 쓰는 초 단위가 맞다 |
| **D-38** | 칩과 공격 속도 버프는 **곱으로 합성**: `간격 = ApplySkill(SkillFireInterval, Base) ÷ (1 + 0.2 × 스택)` | 칩은 "기준 간격"을 바꾸고 버프 공식은 그대로 둔다(스킬 2 칩이 생길 자리). 곱이라 순서 무관 |
| **D-39** | **잭팟 캡슐도 기본 공격이다** — 세 칩을 캡슐에도 적용. 캡슐 피해는 `GetUpgradedFloat(SkillDamage, 캡슐 피해) × GetRuntimeDamageMultiplier()` | 사용자 확정. 상수 덮어쓰기는 T-1 과 같은 누락이라 안 고치면 ②③의 −30% 가 캡슐에서 샌다. `UDRVacuumAirShot` 선례와 같은 방식이고, 런타임 배율까지 곱해 `GetUpgradedDamage()` 와 순서를 맞춘다 |
| **D-40** | 투사체 크기 = **새 축 `SkillProjectileScale`(기준 1.0 배율)** → 스폰 트랜스폼 **Scale3D**. 공용 `UDRProjectileSpell::SpawnProjectile` 에서 읽고, 캡슐 분기도 같은 함수를 부른다 | ⓐ 액터 스케일은 외형과 충돌 구를 ★함께★ 키운다 — "판정 범위도 증가"가 한 줄. ⓑ `SkillAreaRadius` 재사용 기각 — 투사체 몸통 ≠ 효과 반경이라 공용 함수에서 그 축을 읽으면 다른 로봇의 범위 칩과 뜻이 섞인다. ⓒ 공용이라 청소기 에어샷 등도 나중에 카탈로그만으로 크기 칩을 얻는다(1부 레시피 유형 1). 적 GA 는 칩 캐시가 비어 항상 1.0 → 동작 불변 |
| **D-41** | 적중 회복은 **맞는 쪽** `UDREnemyAttributeSet::HandleIncomingDamage` 에서 정산. 판정은 `GetHitHealAmountForDamage(Props)` 하나 — 피처 태그 **AND** 컨텍스트 `SourceAbilityTags` 에 `VendingMachine.BasicAttack` | "적에게 피해가 실제로 들어갔다"가 확정되는 유일한 지점이다 — 벽·바닥·사이트·아군·죽은 적·두더지는 구조적으로 여기 오지 않는다. 투사체(`ADRProjectile`)는 적 투사체와 공유라 칩 지식을 넣지 않는다. 스킬 태그 검사는 D-24 / T-23 과 같은 이유 |
| **D-42** | **오염 중에는 적중 회복 없음** — 판정 함수가 ★시전자★ 의 `IsCorrupted()` 를 본다 | 사용자 확정. 정화는 회복량을 보지 않아(V2.5) 막지 않으면 "적 1회 적중 = 정화"가 된다. 정화 규칙 자체(`HandleCorruptionPurification`)는 건드리지 않는다 — 아군 힐 정화는 그대로 |
| **D-43** | 회복량은 **고정 2**(`ChipTuning.Coin.HitHealAmount`). 피해 배율·`SkillHealing` 과 무관. 걸기 전에 `GetHealableAmount` 로 잘라 만피·회복 차단이면 GE 를 걸지 않는다 | 원문이 "체력 2". `SkillHealing` 은 "아군에게 주는 회복" 축이라 뜻이 다르다. 적중이 잦아(기본 초당 1.7회, 버프 10스택이면 5회) 쓸모없는 GE 적용을 줄인다 |
| **D-44** | 발사 간격 하한 **`MinFireInterval = 0.05초`**(EditDefaultsOnly) | `SetTimer` 는 Rate ≤ 0 이면 타이머를 지운다(T-35). `UDRGameplayAbility::MinCooldownDuration` 과 같은 역할 |
| **D-45** | **BP 재배선 없음 · 새 GE 없음** | 효과가 전부 BP 가 이미 부르는 C++ 경로(`StartAutoFire` · `ExecuteShot` · `SpawnProjectile`)와 피해 관문 안에서 끝난다. `GA_ClawSwipe` BP 계약이 4줄이던 발톱칩(1부 §4)과 다른 점 |
| **D-46** | (별건 권장) `ValidateCatalog` 에 "칩 레벨 대상 스킬이 있는 전용 칩의 ★스킬 단위★ 모디파이어는 `TargetAbilityTag` 필수" 규칙 추가 | 조사 중 `GR.Asc.SeedSupport` 에서 실제 누락을 발견했다(V10-①). 동전칩 3종도 모디파이어마다 태그가 필요해 같은 실수를 막는다 |

> 기각한 대안 — **`EDRUpgradeOp::Override`("1.0초로 고정")**: 1부 §3 끝의 보류 메모대로 리졸버·UI·검증을 전부 건드려야 하고,
> 공격 속도 버프와의 합성 규칙이 모호해진다. `Flat +0.4` 로 충분하다(대신 T-38 을 지킨다).

---

## V4. 코드 변경 상세 (적용 순서)

★헤더(리플렉션) 변경이 섞여 있다 — Live Coding 금지. 에디터를 닫고 VS 에서 빌드한다(T-45).★
대상 파일은 모두 UTF-8(일부 BOM 포함)이다 — 기존 BOM 을 그대로 둔다.

### V4.1 `Public/Game/DRUpgradeTypes.h` — 스탯 축 2개 + 판정 함수 3곳

**(1) 열거형** — `SkillWindup` 다음, **`Count` 바로 앞**에 붙인다(D-4 / T-30):

```cpp
	SkillWindup			UMETA(DisplayName = "스킬 선딜레이"),

	// ===== Plan5(동전칩) 추가 =====
	// ★신규 값은 반드시 Count 바로 앞에 붙인다.★ 기존 값 사이에 끼우면
	// DA_ChipCatalog 에 직렬화된 기존 칩의 효과가 다른 스탯으로 밀린다.

	/**
	 * 스킬 단위. 연사형 스킬의 ★발사 간격(초)★. 짧을수록 좋다.
	 *
	 *     간격 = ApplySkill(SkillFireInterval, BaseFireInterval) ÷ 공격 속도 버프 배율
	 *
	 * ★SkillWindup 과 달리 단위가 "초"다.★ 발사 간격은 몽타주가 아니라 GA 프로퍼티라서
	 * 어빌리티가 기준값을 초 단위로 정확히 안다. 그래서 DRIsTimeStat 에 ★넣는다★.
	 *
	 * ⚠️ SkillCooldown 을 재사용하지 않는다 — 그 축은 ApplyCooldown 이 쿨다운 GE 에 쓴다.
	 *    기본 공격에 쿨다운 GE 가 붙는 순간 같은 칩이 이중으로 걸린다. (Plan5(동전칩) D-37)
	 */
	SkillFireInterval	UMETA(DisplayName = "스킬 발사 간격"),

	/**
	 * 스킬 단위. 그 스킬이 만드는 ★투사체의 크기 배율★ — 외형과 충돌 구(판정)가 함께 커진다.
	 *
	 * ★단위가 "배율"이다.★ 기준값 1.0 에 Apply 해서 스폰 트랜스폼의 Scale3D 로 쓴다 —
	 *     Scale = ApplySkill(SkillProjectileScale, 1.0f);   // 칩이 없으면 정확히 1.0
	 * 투사체의 실제 반경은 발사체 BP(Sphere)에 있어 어빌리티가 모르기 때문이다(SkillWindup 과 같은 이유).
	 *
	 * ⚠️ SkillAreaRadius(효과 반경)와 다른 축이다 — 투사체 몸통 ≠ 폭발·판정 반경. (Plan5(동전칩) D-40)
	 */
	SkillProjectileScale	UMETA(DisplayName = "스킬 투사체 크기"),

	Count UMETA(Hidden) // 배열 크기 계산용 - 항상 마지막
```

**(2) `DRIsSkillStat`** — 빠뜨리면 전역 블록에 쌓여 그 로봇의 모든 스킬에 퍼진다(T-12 / T-29):

```cpp
		|| Stat == EDRUpgradeStat::SkillWindup			// Plan5(발톱칩)
		|| Stat == EDRUpgradeStat::SkillFireInterval	// Plan5(동전칩)
		|| Stat == EDRUpgradeStat::SkillProjectileScale;	// 〃
```

**(3) `DRIsLowerBetter`** — 빠뜨리면 ①의 단점(+0.4초)이 미리보기에서 ★초록(좋아짐)★ 으로 뜬다(T-37):

```cpp
		|| Stat == EDRUpgradeStat::SkillWindup		// Plan5(발톱칩) 선딜은 짧을수록 좋다
		|| Stat == EDRUpgradeStat::SkillFireInterval;	// Plan5(동전칩) 발사 간격도 짧을수록 좋다
```

**(4) `DRIsTimeStat`** — 빠뜨리면 상세 문구가 "+0.4" 로만 나와 초인지 비율인지 모른다(T-37):

```cpp
	return Stat == EDRUpgradeStat::SkillCooldown
		|| Stat == EDRUpgradeStat::HealthRegenDelay
		|| Stat == EDRUpgradeStat::SkillFireInterval;	// Plan5(동전칩) 단위가 초다 (배율인 SkillWindup 은 제외 — T-25)
```

`SkillProjectileScale` 은 배율이고 클수록 좋으므로 `DRIsLowerBetter` · `DRIsTimeStat` 어디에도 넣지 않는다.

### V4.2 `DRGameplayTags.h / .cpp` — 피처 태그 1개

`.h` — 발톱칩 피처 태그(`Upgrade_Feature_Skill_Claw_AllyHeal`) 바로 아래:

```cpp
	// 자판기 기본 공격(동전) 칩 피처 스위치 (Plan5(동전칩)). ASC 에 부여되지 않는다.
	FGameplayTag Upgrade_Feature_Skill_Coin_HitHeal;	// ② 적에게 적중할 때마다 고정량 자기 회복
```

`.cpp` — `Data.HealthCost` 등록 블록 다음:

```cpp
	// ========== 자판기 기본 공격(동전) 칩 기능 스위치 (Plan5(동전칩)) ==========

	GameplayTags.Upgrade_Feature_Skill_Coin_HitHeal = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Coin.HitHeal"),
		FString("자판기 기본 공격이 적에게 적중할 때마다 시전자 체력 고정량 회복")
	);
```

- 이름의 `Coin` 은 발톱칩의 `Claw` 와 같은 자리(스킬 별칭)다. ★ASC 에 Loose 로 얹지 않는다★(1부 §2.3).
- 새 **GAS 태그는 없다** — ②는 지속 상태 없이 즉발 회복뿐이다.

### V4.3 `Public/Game/DRProgressionConfig.h` — `FDRCoinChipTuning`

`FDRClawChipTuning` 바로 다음에 추가:

```cpp
/**
 * ★Plan5(동전칩)★ 자판기 기본 공격(동전) 칩의 동적 상수.
 *
 * 카탈로그 Modifiers 로 표현할 수 없는 값만 여기 둔다 — "적중 1회에 얼마를", "어떤 GE 로".
 * 정적 수치(피해 +20% / −30%, 발사 간격 +0.4초, 크기 +30%)는 전부 DA_ChipCatalog 에 있다.
 */
USTRUCT(BlueprintType)
struct FDRCoinChipTuning
{
	GENERATED_BODY()

	// ========== ② VM.Asc.CoinDrain ==========

	/**
	 * 기본 공격(동전·잭팟 캡슐)이 적에게 ★한 번 적중할 때마다★ 회복하는 체력(고정량).
	 *
	 * ★준 피해와 무관하다★ — 발톱 흡혈(Claw.LifestealRatio)처럼 "비율"이 아니라 "액수"다.
	 * 취약·엘리트·광폭화가 피해를 바꿔도 회복량은 그대로다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coin|Drain", meta = (ClampMin = "0.0"))
	float HitHealAmount = 2.0f;

	/**
	 * 회복에 쓰는 GE. Instant + ExecCalc_Heal + SetByCaller(Heal) — 기존 GE_Heal 을 그대로 지정한다.
	 * ★비우면 에러 없이 무동작★ 이다. Claw.HealEffect 와는 별개 필드라 ★따로 한 번 더★ 지정해야 한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coin|Drain")
	TSoftClassPtr<UGameplayEffect> HealEffect;
};
```

`FDRChipTuning` 의 `Claw` 필드 다음:

```cpp
	// ========== 자판기 기본 공격 칩 (Plan5(동전칩)) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Coin")
	FDRCoinChipTuning Coin;
```

### V4.4 `DRAbilitySystemLibrary.h / .cpp` — `GetHitHealAmountForDamage()`

`.h` — `GetLifestealRatioForDamage` 선언 바로 아래:

```cpp
	/**
	 * ★Plan5(동전칩)★ 이 피해에 붙는 "적중 회복" 고정량. 대상이 아니면 0.
	 *
	 * 세 조건을 ★모두★ 만족해야 한다:
	 *   ① 소스가 Upgrade.Feature.Skill.Coin.HitHeal 피처를 갖고 있다
	 *   ② 그 피해가 ★자판기 기본 공격에서★ 나왔다 (컨텍스트의 SourceAbilityTags — 동전·캡슐 둘 다)
	 *   ③ 소스(시전자)가 ★오염 상태가 아니다★ — 오염 정화는 회복량을 보지 않으므로,
	 *      막지 않으면 "적 1회 적중 = 즉시 정화 + 컨테이너 1칸" 이 된다
	 *
	 * GetLifestealRatioForDamage 의 형제다 — UDREnemyAttributeSet 는 이 둘만 부른다.
	 * 반환값은 "요청량"이다. 실제로 받을 수 있는 양(만피·회복 차단)은 호출부가 GetHealableAmount 로 자른다.
	 */
	static float GetHitHealAmountForDamage(const FEffectProperties& Props);
```

`.cpp` — `GetLifestealRatioForDamage` 정의 바로 아래:

```cpp
float UDRAbilitySystemLibrary::GetHitHealAmountForDamage(const FEffectProperties& Props)
{
	const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();

	// ① 소스가 적중 회복 칩을 끼고 있는가
	if (!HasUpgradeFeature(Props.SourceASC, GameplayTags.Upgrade_Feature_Skill_Coin_HitHeal))
	{
		return 0.f;
	}

	// ② 이 피해가 ★자판기 기본 공격에서★ 나왔는가 (흡혈의 ClawSwipe 검사와 같은 이유 —
	//    빠뜨리면 자판기의 다른 피해원까지 회복된다). 동전·잭팟 캡슐은 둘 다 이 GA 의 태그를 싣는다.
	const FDRGameplayEffectContext* DRContext =
		static_cast<const FDRGameplayEffectContext*>(Props.EffectContextHandle.Get());
	if (!DRContext) return 0.f;

	if (!DRContext->GetSourceAbilityTags().HasTagExact(GameplayTags.Abilities_VendingMachine_BasicAttack))
	{
		return 0.f;
	}

	// ③ ★시전자가 오염 중이면 막는다★ — HandleCorruptionPurification 은 회복량과 무관하게
	//    정화 + 컨테이너 1칸을 준다. 오염 해제는 아군 회복으로만 하는 팀 메커니즘이다.
	//    (오염은 플레이어에게만 있다 — 판정 대상은 적이 아니라 ★시전자★ 다)
	if (const UDRPlayerAttributeSet* SourcePlayerAS = GetPlayerAttributeSetFromASC(Props.SourceASC))
	{
		if (SourcePlayerAS->IsCorrupted()) return 0.f;
	}

	const AActor* TuningContext = Props.SourceASC ? Props.SourceASC->GetAvatarActor() : nullptr;
	return FMath::Max(0.f, GetChipTuning(TuningContext).Coin.HitHealAmount);
}
```

- 필요한 include(`DRAbilityTypes.h` · `DRPlayerAttributeSet.h` · `Game/DRProgressionConfig.h`)는 이미 있다.

### V4.5 `Private/AbilitySystem/DREnemyAttributeSet.cpp` — 적중 회복 지급

발톱 흡혈 블록(`{ const float LifestealRatio = … }`) **바로 아래**, `const float NewHealth = …` **위**에 넣는다(T-40):

```cpp
		// ★Plan5(동전칩)★ 적중 회복 — 자판기 기본 공격이 적에게 ★맞을 때마다★ 고정량 회복.
		//
		// 흡혈과 같은 자리인 이유: "적에게 피해가 실제로 들어갔다"가 확정되는 지점이 여기다
		// (죽은 적은 PostGameplayEffectExecute 가, 0 피해는 위의 > 0 가드가 이미 걸렀다).
		// 피해량과 무관한 고정량이라 SetHealth 전후는 상관없지만, 튜토리얼 더미의 조기 return 보다는
		// 반드시 위여야 한다(흡혈과 똑같이 더미에서도 연습이 되게).
		//
		// 칩이 없거나 / 기본 공격이 아니거나 / 시전자가 오염 중이면 GetHitHealAmountForDamage 가 0 이다.
		// 만피·회복 차단이면 GetHealableAmount 가 0 이라 GE 자체를 걸지 않는다(적중이 잦아 헛 적용을 줄인다).
		{
			const float HitHeal = UDRAbilitySystemLibrary::GetHitHealAmountForDamage(Props);
			if (HitHeal > 0.f)
			{
				const float Healable = UDRAbilitySystemLibrary::GetHealableAmount(Props.SourceAvatarActor, HitHeal);
				if (Healable > 0.f)
				{
					UDRAbilitySystemLibrary::ApplyHealToActor(
						Props.SourceAvatarActor,
						Props.SourceASC,
						UDRAbilitySystemLibrary::GetChipTuning(Props.SourceAvatarActor).Coin.HealEffect,
						Healable);
				}
			}
		}
```

- 대상 거르기(1부 T-33): `ApplyHealToActor` 는 대상을 가리지 않지만 여기의 대상은 ★피해를 준 시전자 본인★ 뿐이라
  팀·사이트 판정이 필요 없다(흡혈과 같다).
- 이 블록도 ★적이 맞는 모든 피해★ 가 지나는 곳에 있다 — 회귀 확인은 적 피해 전반으로 한다(1부 T-31).

### V4.6 `DRProjectileSpell.h / .cpp` — 투사체 크기

`.h` — `GetEffectiveNumProjectiles()` 아래(public):

```cpp
	/**
	 * ★Plan5(동전칩)★ 칩이 반영된 투사체 크기 배율. 칩이 없으면 정확히 1.0.
	 *
	 * SpawnProjectile() 이 스폰 트랜스폼의 Scale3D 로 쓴다 — 외형(메시)과 충돌 구(Sphere)가 함께 커진다.
	 * 스폰 경로가 따로 있는 파생 GA(자판기 잭팟 캡슐)는 이 함수를 직접 불러 같은 값을 넣는다.
	 */
	UFUNCTION(BlueprintPure, Category = "Projectile|Upgrade")
	float GetUpgradedProjectileScale() const;
```

`.cpp` — `SpawnProjectile()` 의 트랜스폼 구성부(`SetRotation` 다음 줄):

```cpp
	FTransform SpawnTransform;
	SpawnTransform.SetLocation(SocketLocation);
	SpawnTransform.SetRotation(Rotation.Quaternion());
	// ★Plan5(동전칩)★ 투사체 크기 칩(SkillProjectileScale). 칩이 없으면 1.0 이라 기존과 완전히 같다 —
	// 이 함수를 쓰는 적 GA(원거리·잠자리)는 칩 캐시가 비어 있어 항상 1.0 이다.
	// SpawnActorDeferred 와 FinishSpawning 에 ★같은 트랜스폼★ 이 넘어가야 한다(아래 코드 그대로).
	SpawnTransform.SetScale3D(FVector(GetUpgradedProjectileScale()));
```

`.cpp` — 파일 끝:

```cpp
float UDRProjectileSpell::GetUpgradedProjectileScale() const
{
	// ★기준값 1.0 에 곱한다★ — 칩이 없으면 정확히 1.0.
	// 0 에 가까워지면 충돌 구가 사라져 아무것도 못 맞히므로 하한을 둔다.
	return FMath::Max(0.1f, GetUpgradedFloat(EDRUpgradeStat::SkillProjectileScale, 1.f));
}
```

### V4.7 `DRVendingMachineBasicAttack.h / .cpp`

**`.h`** — `BaseFireInterval` 아래에 하한 프로퍼티:

```cpp
	/** 기본 발사 간격 (초) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|BasicAttack")
	float BaseFireInterval = 0.3f;

	/**
	 * ★Plan5(동전칩)★ 발사 간격의 하한(초).
	 *
	 * 발사 간격 칩(SkillFireInterval)과 공격 속도 버프가 겹쳐 0 이하가 되면 SetTimer 가 타이머를
	 * ★지워서★ 연사가 조용히 멈춘다 — 그걸 막는 바닥값이다(UDRGameplayAbility::MinCooldownDuration 과 같은 역할).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "VendingMachine|BasicAttack", meta = (ClampMin = "0.01"))
	float MinFireInterval = 0.05f;
```

- `BaseFireInterval` 의 C++ 기본값(0.3)은 바꾸지 않는다 — BP 가 0.6 으로 덮어쓰고 있어 값에는 영향이 없다.
  주석만 실제 값을 알리도록 고칠지는 V10-④ 에서 처리.

**`.cpp` ① — `GetCurrentFireInterval()` 전체 교체:**

```cpp
float UDRVendingMachineBasicAttack::GetCurrentFireInterval() const
{
	// ★Plan5(동전칩)★ 발사 간격 칩(SkillFireInterval, 초 단위)부터 반영한다.
	//   칩이 없으면 ApplySkill 이 항등이라 BaseFireInterval(BP 0.6초)이 그대로 나온다.
	//   VM.Asc.CoinHeavy: 0.6 + 0.4 = 1.0초
	const float ChipInterval = GetUpgradedFloat(EDRUpgradeStat::SkillFireInterval, BaseFireInterval);

	// 공격 속도 버프(스킬 2)는 기존 공식 그대로 ★그 위에서★ 나눈다 — 칩과 버프는 곱으로 합성된다(D-38).
	int32 BuffStacks = 0;
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		FGameplayTagContainer BuffTagFilter;
		BuffTagFilter.AddTag(FDRGameplayTags::Get().Buff_VendingMachine_AttackSpeed);

		TArray<FActiveGameplayEffectHandle> ActiveEffects =
			ASC->GetActiveEffectsWithAllTags(BuffTagFilter);

		for (const FActiveGameplayEffectHandle& EffectHandle : ActiveEffects)
		{
			const FActiveGameplayEffect* ActiveGE = ASC->GetActiveGameplayEffect(EffectHandle);
			if (ActiveGE)
			{
				BuffStacks = ActiveGE->Spec.GetStackCount();
				break;
			}
		}
	}

	const float SpeedMultiplier = 1.0f + (BuffStacks * 0.2f);

	// ★하한 필수★ — SetTimer 는 Rate <= 0 이면 타이머를 지워서 연사가 조용히 멈춘다(D-44).
	return FMath::Max(MinFireInterval, ChipInterval / SpeedMultiplier);
}
```

- 기존 코드의 `if (!ASC) return BaseFireInterval;` 조기 반환은 없앴다 — 그 경로로 빠지면 ★칩이 무시된다★.
  ASC 가 없으면 버프 스택 0 으로 계산을 이어 간다.
- 버프 스택 조회 부분은 기존 코드를 그대로 옮긴 것이다(동작 불변).

**`.cpp` ② — `ExecuteShot()` 의 잭팟 캡슐 분기**(크기 1줄 + 피해 1줄):

```cpp
			FTransform SpawnTransform;
			SpawnTransform.SetLocation(SocketLocation);
			SpawnTransform.SetRotation(Rotation.Quaternion());
			// ★Plan5(동전칩)★ 캡슐도 기본 공격 투사체다 — 동전과 같은 크기 칩을 받는다(D-39).
			//   이 분기는 SpawnProjectile() 을 타지 않으므로 여기서 따로 넣는다.
			SpawnTransform.SetScale3D(FVector(GetUpgradedProjectileScale()));

			ADRProjectile* Projectile = GetWorld()->SpawnActorDeferred<ADRProjectile>(
				SelectedClass, SpawnTransform,
				GetOwningActorFromActorInfo(),
				Cast<APawn>(GetOwningActorFromActorInfo()),
				ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

			Projectile->DamageEffectParams = MakeDamageEffectParamsFromClassDefaults();
			// ★Plan5(동전칩)★ 캡슐 피해도 SkillDamage 칩을 탄다(D-39). 상수를 그대로 넣으면
			//   ①의 +20% 도, ②③의 −30% 도 캡슐에서만 빠진다(T-1 과 같은 누락 — UDRVacuumAirShot 과 같은 보정).
			//   GetUpgradedDamage() 와 같은 순서로 런타임 배율까지 곱한다(자판기는 지금 1.0).
			Projectile->DamageEffectParams.BaseDamage =
				GetUpgradedFloat(EDRUpgradeStat::SkillDamage, SelectedDamage) * GetRuntimeDamageMultiplier();

			Projectile->FinishSpawning(SpawnTransform);
```

- 동전(일반) 분기의 `SpawnProjectile(TargetLocation, FireSocketTag)` 는 **수정 없음** — V4.6 에서 크기가,
  기존 배관에서 피해가 이미 반영된다.
- `GetUpgradedProjectileScale()` 은 부모 `UDRProjectileSpell` 의 public 함수라 include 추가가 없다.

### V4.8 `Private/UI/DRUpgradeUILibrary.cpp` — 카드 짧은 이름

`GetStatShortName()` 의 `SkillWindup` case 아래(두 축 모두 정식 명칭이 5자를 넘는다):

```cpp
	case EDRUpgradeStat::SkillFireInterval:
		return LOCTEXT("StatShort_SkillFireInterval", "발사 간격");		// 정식: 스킬 발사 간격

	case EDRUpgradeStat::SkillProjectileScale:
		return LOCTEXT("StatShort_SkillProjectileScale", "탄 크기");		// 정식: 스킬 투사체 크기
```

- 동전칩 3종은 `EffectSummary` 가 "이름 +수치" 꼴이라 카드 라벨은 그쪽이 쓰인다(`MakeShortLabel` 1순위).
  짧은 이름은 `EffectSummary` 를 비웠을 때의 폴백이자 앞으로의 다른 칩용이다.
- 미리보기 라벨(`MakeStatPreviewLine`)은 정식 명칭을 쓴다 → "기본 공격 · 스킬 발사 간격 +0.4"(빨강).

### V4.9 (별건 권장) `Private/Game/DRChipCatalog.cpp` — 모디파이어 대상 태그 검사 (D-46)

`ValidateCatalog()` 의 모디파이어 루프 안, 기존 "캐릭터 단위 스탯에 TargetAbilityTag" 검사 다음:

```cpp
			// ★Plan5(동전칩)★ 칩 레벨에 대상 스킬이 지정된 전용 칩인데 ★스킬 단위★ 모디파이어의 태그가 비어 있으면
			// 그 모디파이어는 "그 로봇의 모든 스킬"에 걸린다 — 칩 레벨 태그는 표기용일 뿐이다.
			// (T-12 와 같은 증상을 데이터 쪽에서 내는 실수. GR.Asc.SeedSupport 가 실제로 이랬다)
			if (!Chip.bCommonChip && Chip.TargetAbilityTag.IsValid()
				&& DRIsSkillStat(Modifier.Stat) && !Modifier.TargetAbilityTag.IsValid())
			{
				OutErrors.Add(FString::Printf(
					TEXT("%s: 스킬 단위 모디파이어(%s)의 TargetAbilityTag 가 비어 있어 이 로봇의 모든 스킬에 적용됩니다. %s 를 지정하세요."),
					*IdString, *UEnum::GetValueAsString(Modifier.Stat), *Chip.TargetAbilityTag.ToString()));
			}
```

- 넣는 순간 `GR.Asc.SeedSupport` 가 오류로 뜬다 → V10-① 데이터 수정과 **같은 커밋**으로 처리한다.
- 칩 레벨 태그가 ★비어 있는★ 전용 칩(= 그 로봇 전체 대상 칩)은 검사하지 않으므로 정당한 "모든 스킬" 칩은 막지 않는다.

---

## V5. 에디터 작업 순서 (STEP)

### STEP 0. 선행 확인
- [ ] `GA_VendingMachineBasicAttack` 의 `AbilityTags` 가 `Abilities.VendingMachine.BasicAttack` **하나**인가
      (업그레이드 키 = "Abilities" 접두 태그 중 첫 번째 — `GetUpgradeKeyTagFromAbilityTags`. 2026-09-27 현재 하나다)
- [ ] `BaseFireInterval = 0.6` 인가 (바뀌었으면 ①의 `Flat` 값을 다시 계산 — T-38)
- [ ] `DA_ProgressionConfig` → `ChipTuning.Claw.HealEffect = GE_Heal` 인가 (같은 GE 를 ② 에도 쓴다)

### STEP 1. C++ 빌드
V4.1 ~ V4.8(+ 선택 V4.9) 적용 → **에디터를 닫고** VS 에서 `DaeRune` Development Editor 빌드 → 에디터 실행(T-45).

### STEP 2. `DA_ProgressionConfig` → `ChipTuning` → `Coin`

| 필드 | 값 | 비고 |
|---|---|---|
| `HitHealAmount` | 2.0 | 구조체 기본값과 같다 — 확인만 |
| `HealEffect` | `GE_Heal` (`/Game/Blueprints/AbilitySystem/Player/Effects/`) | ★비우면 ② 무동작★(T-39) |

### STEP 3. `DA_ChipCatalog` — 칩 3종 추가

공통 필드: `OwnerClass = Vending Machine` / `bCommonChip = false` / `Category = 돌파 칩` /
`TargetAbilityTag = Abilities.VendingMachine.BasicAttack` / `RequiredSlotCount = 1`.

**① `VM.Asc.CoinHeavy`**

| 필드 | 값 |
|---|---|
| `DisplayName` | 고액 동전 |
| `EffectSummary` | 동전 피해 +20% |
| `Description` | 동전과 잭팟 캡슐의 피해가 20% 늘어납니다. 대신 발사 간격이 0.6초에서 1.0초로 길어집니다. |
| `RequiredSlotTier` | 3 |
| `Modifiers[0]` | `SkillDamage` / `Abilities.VendingMachine.BasicAttack` / `Percent` / **0.2** / `bIsDrawback = false` |
| `Modifiers[1]` | `SkillFireInterval` / 〃 / **`Flat`** / **0.4** / `bIsDrawback = true` / `DetailOverride = 발사 간격 1.0초 (기존 0.6초)` |
| `GrantedTags` | (없음) |

**② `VM.Asc.CoinDrain`**

| 필드 | 값 |
|---|---|
| `DisplayName` | 거스름 동전 |
| `EffectSummary` | 적중 시 체력 +2 |
| `Description` | 기본 공격이 적에게 맞을 때마다 체력을 2 회복합니다. 대신 기본 공격 피해가 30% 감소합니다. 오염 상태이거나 회복 차단 효과가 있으면 회복되지 않습니다. |
| `RequiredSlotTier` | 4 |
| `Modifiers[0]` | `SkillDamage` / `Abilities.VendingMachine.BasicAttack` / `Percent` / **−0.3** / `bIsDrawback = true` |
| `GrantedTags` | `Upgrade.Feature.Skill.Coin.HitHeal` |

**③ `VM.Asc.CoinWide`**

| 필드 | 값 |
|---|---|
| `DisplayName` | 대형 동전 |
| `EffectSummary` | 동전 크기 +30% |
| `Description` | 동전과 잭팟 캡슐이 30% 커지고, 맞는 판정도 그만큼 넓어집니다. 대신 기본 공격 피해가 30% 감소합니다. |
| `RequiredSlotTier` | 3 |
| `Modifiers[0]` | `SkillProjectileScale` / `Abilities.VendingMachine.BasicAttack` / `Percent` / **0.3** / `bIsDrawback = false` |
| `Modifiers[1]` | `SkillDamage` / 〃 / `Percent` / **−0.3** / `bIsDrawback = true` |
| `GrantedTags` | (없음) |

**`SkillDisplayNames`** 에 `Abilities.VendingMachine.BasicAttack → 기본 공격` 한 줄 추가
(없으면 미리보기 라벨이 태그 마지막 조각인 "BasicAttack" 으로 나온다).

### STEP 4. 아이콘 (VO-4)
`Content/DaeRuneAssets/UI/Upgrade/Icon/VendingMachine/` 에 정원로봇과 같은 규칙으로 3장 — ① 공격, ② 지원, ③ 편의.
텍스처 설정은 기존과 같다(Texture Group = UI, sRGB = on, Compression = UserInterface2D, Mip Gen = NoMipmaps).
준비 전에는 비워 두면 `DefaultChipIcon` 으로 폴백된다.

### STEP 5. 검증
`DA_ChipCatalog` → `Validate Catalog`(MaxSlots 6) → **동전칩 3종 오류 0건**.
V4.9 를 넣었다면 `GR.Asc.SeedSupport` 1건이 뜬다 → STEP 6 에서 해소.

### STEP 6. (별건) 기존 데이터 이상 수정 — V10 표대로

---

## V6. 함정 (T-35 ~ T-45)

| # | 함정 | 왜 위험한가 / 어떻게 피하나 |
|---|---|---|
| **T-35** | 발사 간격 계산이 0 이하 | `SetTimer` 는 Rate ≤ 0 이면 ★타이머를 지운다★ → 버튼을 누르고 있어도 연사가 조용히 멈춘다. `MinFireInterval` 로 바닥(D-44) |
| **T-36** | 캡슐 분기에 크기·피해 중 하나만 넣기 | 캡슐은 `SpawnProjectile` 을 안 탄다(V2.2). 두 줄 중 하나라도 빠지면 ★캡슐만★ 칩을 무시한다 — 특히 피해 줄이 빠지면 ②③의 −30% 가 캡슐에서 샌다 |
| **T-37** | `SkillFireInterval` 을 `DRIsLowerBetter` / `DRIsTimeStat` 에서 빠뜨리기 | 전자: ①의 단점(+0.4초)이 미리보기에 ★초록★ 으로 뜬다. 후자: 상세 문구가 "+0.4" 로만 나와 단위를 모른다 |
| **T-38** | ①의 `Flat +0.4` 를 "1.0초 고정"으로 착각 | 기준 +0.4초다. BP 의 `BaseFireInterval` 을 바꾸면 ①의 결과도 바뀐다 → 카탈로그 `Value` · `DetailOverride` · `Description` 세 곳을 같이 고친다 |
| **T-39** | `ChipTuning.Coin.HealEffect` 미지정 | ② 가 에러 없이 무동작한다. `Claw.HealEffect` 와 별개 필드라 둘 다 채워야 한다 |
| **T-40** | 적중 회복 블록의 위치 | `LocalIncomingDamage > 0` 가드 ★안★, 튜토리얼 더미 조기 return ★위★(= 흡혈 블록 바로 아래). 가드 밖이면 0 피해에도 회복되고, 더미 분기 아래면 튜토리얼에서만 안 된다 |
| **T-41** | 오염 판정을 ★타깃★ 에서 하기 | 오염은 플레이어에게만 있다 — 판정은 ★시전자★(`Props.SourceASC`)의 `UDRPlayerAttributeSet` 이다. 적 쪽을 보면 항상 "오염 아님"이라 D-42 가 통째로 무력화된다 |
| **T-42** | 스폰 후 `SetActorScale3D` 로 키우기 | 크기는 스폰 ★전★ 트랜스폼에 넣는다(`SpawnActorDeferred` · `FinishSpawning` 에 같은 트랜스폼). 사후 변경은 초기 오버랩·복제 스폰 정보와 어긋날 수 있다 |
| **T-43** | 큰 투사체의 부작용 | ⓐ 동전은 중력 1.0 포물선이라 ★바닥·벽 모서리에 먼저 닿아★ 사라진다(사거리 체감 감소). ⓑ 스폰 직후의 겹침도 판정되므로(V2.3) 총구(왼손 소켓)에서 반경 31.6uu 안에 벽이 있으면 ★쏘자마자 사라진다★ — 기존 24.3uu 보다 잦아진다. ⓒ 브론즈·실버 캡슐 메시는 구 뒤로 −120 / −61uu 오프셋이 있어 1.3배 시 −156 / −79uu 로 벌어진다(판정 구와 외형이 떨어져 보일 수 있다). 벽에 붙어 쏘기·바닥 쪽 사격을 반드시 테스트 |
| **T-44** | 클라 로그로 발사 간격 디버깅 | 발사는 ★서버 타이머★ 가 정한다(`ExecuteShot` 권한 체크). 클라에서도 타이머가 돌지만 아무것도 쏘지 않는다 — 서버(호스트) 로그를 볼 것 |
| **T-45** | 헤더 변경을 Live Coding 으로 반영 | `UENUM` 값 · `USTRUCT` 필드 · `UFUNCTION` 추가는 리플렉션 변경이다. 에디터를 닫고 빌드하지 않으면 카탈로그 드롭다운에 새 축이 안 보이거나 에셋 저장이 꼬일 수 있다 |

1부 함정 중 2부에서 다시 밟기 쉬운 것: **T-12/T-29**(`DRIsSkillStat` 누락), **T-30**(열거형 중간 삽입),
**T-23**(스킬 태그 검사 누락), **T-31**(적 피해 전반 회귀), **T-33**(`ApplyHealToActor` 는 대상을 가리지 않는다).

---

## V7. 테스트 계획

도구: 로비 업그레이드 화면에서 칩 장착 → 스테이지 입장. 발사 간격은 호스트 화면에서 60초간 발사 수를 세거나
`GetCurrentFireInterval()` 반환값을 임시 로그로 찍는다(★서버 로그★ — T-44). 테스트 ID 는 함정 번호(T-n)와 겹치지 않게
R / C / X / M / U 로 쓴다.

### V7.1 회귀 — 칩 없음 (가장 먼저)

| ID | 항목 | 기대 |
|---|---|---|
| R-1 | 동전 | 피해 20, 발사 간격 0.6초(60초에 100발), 버프 n스택이면 0.6 ÷ (1 + 0.2n) |
| R-2 | 잭팟 | 동전 10발 뒤 캡슐 1발, 피해 10 / 50 / 150 — ★수정 전과 동일★(`SkillDamage` 항등) |
| R-3 | 크기 | 동전·캡슐의 외형과 판정이 수정 전과 같다 |
| R-4 | 다른 투사체 | 적 원거리(`GA_RangedAttack`) · 잠자리 기본/스킬 · 청소기 에어샷의 크기·피해 그대로 (`SpawnProjectile` 공용 수정) |
| R-5 | 발톱 흡혈 | 정원로봇 수혈 삽날의 흡혈이 그대로 동작한다(바로 아래에 블록을 추가하므로) |
| R-6 | 적 피해 전반 | 근접·원거리·DoT·엘리트·Phase1 감쇠·처치 물 지급 그대로 (1부 T-31) |

### V7.2 칩별

| ID | 칩 | 기대 |
|---|---|---|
| C-1 | ① | 동전 24 / 캡슐 12 · 60 · 180 / 발사 간격 **1.0초**(60초에 60발) / 잭팟은 여전히 동전 10발마다 |
| C-2 | ① + 버프 10스택 | 발사 간격 **0.333초** |
| C-3 | ② | 적 적중마다 체력 +2 (동전·캡슐 모두) / 피해 동전 14, 캡슐 7 · 35 · 105 |
| C-4 | ② 비대상 | 벽·바닥·클렌저 사이트 적중 → 회복 없음 / 만피면 GE 적용 없음 / 튜토리얼 더미 → 회복됨(흡혈과 같다) |
| C-5 | ② + 오염 | 오염 상태에서 적을 맞혀도 ★오염 유지 · 체력 불변★ / 아군 회복으로 정화된 뒤에는 다시 +2 |
| C-6 | ② + ⑥ 광폭화 | 회복이 전혀 없다 |
| C-7 | ③ | 동전·캡슐 외형 ×1.3, 적 가장자리를 스치게 쏴서 판정 확대 확인 / 피해 14 / 임팩트 이펙트 크기는 그대로 |
| C-8 | ③ 부작용 | 벽 가까이·바닥 쪽 사격에서 수정 전보다 일찍 터진다(T-43 ⓐⓑ) — 체감이 과하면 ③의 크기 배율(+0.3)을 낮출지 검토 |

### V7.3 조합

| ID | 조합 | 기대 |
|---|---|---|
| X-1 | ①+② | 한 발 16.8 / 1.0초 / 적중마다 +2 |
| X-2 | ②+③ | 한 발 **9.8** — ★×0.49★ (−60% 가 아니다 — 곱연산) |
| X-3 | ①+②+③ | 한 발 11.76 / 1.0초 / ×1.3 / 적중마다 +2 |
| X-4 | 물대포 ②(취약)가 걸린 적에게 동전 | 피해 ×1.5, ② 회복은 2 그대로 |

### V7.4 멀티플레이어 (PIE 리슨 서버 + 클라 2)

| ID | 항목 | 기대 |
|---|---|---|
| M-1 | 클라 자판기 ① | 모든 화면에서 1.0초 간격(서버 타이머) |
| M-2 | 클라 자판기 ③ | ★호스트·다른 클라 화면에서도★ 동전·캡슐이 1.3배 — 스폰 스케일 복제 확인. 안 되면 VO-3 |
| M-3 | 클라 자판기 ② | 서버에서 회복 → 클라 체력바에 +2 반영 |
| M-4 | 호스트 자판기 ①②③ | 클라와 같은 동작 |
| M-5 | 서버 정화 로그 | 클라가 동전칩 3종을 끼고 입장해도 `SanitizeLoadout` "정화" 로그가 없다 |

### V7.5 UI

| ID | 항목 | 기대 |
|---|---|---|
| U-1 | 칩 카드 | "동전 피해 / +20%", "적중 시 체력 / +2", "동전 크기 / +30%" |
| U-2 | 상세 문구 | ① "스킬 피해량 +20%" + 빨간 "발사 간격 1.0초 (기존 0.6초)" / ③ "스킬 투사체 크기 +30%" |
| U-3 | 미리보기 | "기본 공격 · 스킬 발사 간격 +0.4"(빨강), "기본 공격 · 스킬 투사체 크기 +30%"(정상), "기본 공격 · 스킬 피해량 -30%"(빨강) |
| U-4 | 목록 | 자판기 화면에만 3종이 보이고 정원로봇·청소기에는 안 보인다 |
| U-5 | 검증 | `Validate Catalog` 동전칩 3종 오류 0건 |

---

## V8. 변경 파일 체크리스트 · 마일스톤

### C++ — 수정만 (신규 파일 없음)

| 파일 | 변경 |
|---|---|
| `Public/Game/DRUpgradeTypes.h` | 축 2개 · `DRIsSkillStat` 2 · `DRIsLowerBetter` 1 · `DRIsTimeStat` 1 |
| `Public/DRGameplayTags.h` · `Private/DRGameplayTags.cpp` | 피처 태그 1 |
| `Public/Game/DRProgressionConfig.h` | `FDRCoinChipTuning` + `FDRChipTuning::Coin` |
| `Public/AbilitySystem/DRAbilitySystemLibrary.h` · `.cpp` | `GetHitHealAmountForDamage` |
| `Private/AbilitySystem/DREnemyAttributeSet.cpp` | 적중 회복 블록 |
| `Public/AbilitySystem/Abilities/DRProjectileSpell.h` · `.cpp` | `GetUpgradedProjectileScale` + `SpawnProjectile` 스케일 1줄 |
| `Public/AbilitySystem/Abilities/DRVendingMachineBasicAttack.h` · `.cpp` | `MinFireInterval` · `GetCurrentFireInterval` 교체 · 캡슐 분기 2줄 |
| `Private/UI/DRUpgradeUILibrary.cpp` | 짧은 이름 2 |
| (선택) `Private/Game/DRChipCatalog.cpp` | 검증 규칙 1 (D-46) |

새 이름(`GetUpgradedProjectileScale` · `MinFireInterval` · 두 축 이름 · `CoinHeavy/Drain/Wide`)은 콘텐츠 에셋 전체를
검색해 **충돌 0건**을 확인했다(1부 T-32).

### 콘텐츠

| 에셋 | 변경 |
|---|---|
| `DA_ProgressionConfig` | `ChipTuning.Coin.HealEffect = GE_Heal` |
| `DA_ChipCatalog` | 칩 3 + `SkillDisplayNames` 1 |
| 아이콘 3장 (선택) | `UI/Upgrade/Icon/VendingMachine/` |

### 마일스톤

| 단계 | 내용 | 완료 조건 |
|---|---|---|
| **1단계** | C++ 전체(V4.1 ~ V4.8) | 빌드 통과 + V7.1 회귀 R-1 ~ R-6 |
| **2단계** | 에셋(STEP 2 ~ 5) | Validate 0건 + V7.2 · V7.3 |
| **3단계** | 멀티 · UI | V7.4 · V7.5 |
| **4단계** | (별건) V4.9 + V10 데이터 수정 | 별도 커밋 — 1부 동작이 바뀌므로 동전칩과 섞지 않는다 |
| **5단계** | 문서 압축 | 2부를 1부 구조(§1 스펙 · §3 결정 · §5 튜닝 · §6 함정)에 흡수하고 계획 부분을 걷어낸다 |

---

## V9. 열린 결정 · 남은 이슈

| # | 내용 | 현재 판단 |
|---|---|---|
| **VO-1** | ★①은 단독으로 동전 DPS 가 ×0.72★ 다 — 한 발 +20% 보다 연사 −40% 가 크다(초당 캡슐·초당 ② 회복도 같이 준다) | 사용자 확정값(1.0초)대로 구현한다. "한 방은 세고 DPS 는 비슷한" 칩을 원하면 피해 증가가 **+67% 이상**(0.6 → 1.0초 보상)이어야 한다 → 플레이 후 카탈로그 `Value` 만 조정 |
| **VO-2** | 표시명 '고액 동전' / '거스름 동전' / '대형 동전' 은 가칭 | `ChipId` 만 확정(세이브 키). 이름·설명은 언제든 변경 가능 |
| **VO-3** | 스폰 스케일이 클라에 안 보이면(M-2 실패) | 엔진이 스폰 스케일을 보내므로(V2.3) 가능성은 낮다. 실패하면 `ADRProjectile` 에 복제 `ProjectileScale` + `OnRep` 에서 `SetActorScale3D`. 판정은 서버라 게임플레이는 이미 맞고 외형만의 문제다 |
| **VO-4** | 아이콘 3장 | 미정. 비워 두면 기본 아이콘 폴백 |
| 1 | 임팩트 이펙트(`NS_Basic_3_*`)는 커지지 않는다 | 그대로 둔다(1부 §9-12 와 같은 판단). 필요하면 나이아가라 `RadiusScale` User 파라미터 |
| 2 | 동전은 관통이 없어 ② 는 "발사 1회 = 회복 최대 1회" | 관통 칩이 생기면 적 수만큼 회복된다 — 그때 상한을 검토 |
| 3 | 발톱 흡혈(1부 ②)도 오염 중 적을 치면 ★정화된다★(같은 구멍 — D-42 는 동전만 막는다) | 1부에도 같은 규칙을 적용할지 사용자 결정. 적용한다면 `GetLifestealRatioForDamage` 에 같은 ③ 조건 3줄 |
| 4 | 공격 속도 버프의 스택당 0.2 는 C++ 리터럴(`GetCurrentFireInterval`) | 스킬 2 칩을 만들 때 `ChipTuning` 또는 GE 로 이관(1부 D-2 원칙) |
| 5 | 캡슐 분기에 `Projectile` null 체크가 없다(`SpawnActorDeferred` 실패 시 크래시) | 기존 코드. 이번에 고치는 줄 바로 옆이라 `if (Projectile)` 가드 추가를 권장 |

---

## V10. 조사 중 발견한 기존 데이터 이상 (2부 범위 밖 · 별건)

에셋 CDO 를 직접 읽다가 발견했다. 전부 **1부 동작에 영향**을 주므로 동전칩과 별도 커밋으로 고친다(V8 의 4단계).

| # | 발견 | 영향 | 수정 |
|---|---|---|---|
| ① | `GR.Asc.SeedSupport` 의 `Modifiers[1]`(`SkillDamage −0.8`) ★`TargetAbilityTag` 가 비어 있다★ | ★정원로봇 모든 스킬(발톱·물대포 포함) 피해 −80%★ — 설명문은 "씨앗 폭탄의 피해가 80% 감소" | 그 모디파이어에 `Abilities.GardenRobot.SeedCannon` 지정 (+ V4.9 검증 규칙) |
| ② | `SkillDisplayNames` 에 `WaterPump → 물대포` ★한 줄뿐★ | 1부 §5.5 가 요구하는 `SeedCannon → 씨앗 폭탄` · `ClawSwipe → 기본 공격` 이 없어 미리보기 라벨이 영문 태그 조각으로 나온다 | 두 줄 추가 (STEP 3 의 자판기 줄과 함께) |
| ③ | 적 감속 지속시간이 문서와 에셋에서 ★서로 뒤바뀌어★ 있다.<br>**문서**(1부 §1.1 · §5.2 · §5.3, `GR.Asc.SeedSupport` 설명문): 씨앗 폭탄 **5.0초** / 물대포 **1.2초**<br>**에셋**(`DA_ProgressionConfig` = 실제 적용값): `SeedBomb.EnemySlow.Duration = 1.2` / `WaterPump.EnemySlow.Duration` 은 입력되지 않아 구조체 기본값 **5.0** | 코드는 각자 맞는 구조체를 읽고(`DRSeedProjectile` → `SeedBomb`, `DRWaterPump` → `WaterPump`) 두 GE 모두 `Data.Duration` 을 SetByCaller 로 받으므로 ★에셋 값이 그대로 게임에 나간다★ → 지금 게임에서는 씨앗 폭탄 감속 1.2초, 물대포 감속 5초. 1부 §5.3 의 "1.2 로 낮출 것" 이 물대포가 아니라 씨앗 폭탄 칸에 들어간 것으로 추정(커밋 전 작업분 — HEAD 에는 이 섹션이 없다) | `SeedBomb.EnemySlow.Duration` → 5.0(= 기본값으로 초기화), `WaterPump.EnemySlow.Duration` → 1.2 |
| ④ | 주석이 실제 값과 다르다: `DRVendingMachineBasicAttack.h` "일반 공격 5회 후"(BP 10) · `BaseFireInterval = 0.3f`(BP 0.6) / `DRVendingMachineAttackSpeedBuff.h` "공격속도 +10%"(코드는 스택당 0.2 = +20%) | 코드를 읽는 사람이 잘못된 기준으로 계산한다(이번 조사에서 발사 주기를 다시 확인해야 했던 원인) | 주석만 실제 값으로 |
