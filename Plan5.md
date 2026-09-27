# Plan5 — 로봇 전용 칩 12종 (정원로봇 9 · 자판기 기본 공격 3)

> **상태**
> - 정원로봇 9종(씨앗 폭탄 · 물대포 · 발톱): **구현 완료**. 씨앗 폭탄·물대포는 게임 검증 완료, **발톱은 게임 검증이 남았다 → §10**
> - 자판기 기본 공격(동전) 3종: **C++ 완료(빌드 통과) · 에셋 입력 완료 — 단 ★칩 ② 의 `GrantedTags` 누락★ → §11**, 게임 검증 남음 → §10
>
> - 같은 스킬 칩은 동시에 1개만: **C++ 완료(빌드 통과) · BP 사유 문구 1곳(선택) → §12.4**
>
> 최종 갱신: 2026-09-27 / 브랜치 `feat/upgradeskill`
> 선행 문서: **Plan2.md**(재화·슬롯·칩 시스템) → **Plan3.md**(공용 칩 8종)
>
> 📌 **이 문서는 "계획서"가 아니라 ★완료된 기능의 참조 문서★ 다.**
> 계획 단계의 코드 해부·단계별 에디터 매뉴얼·테스트 체크리스트는 역할을 다해 걷어냈고,
> **다시 만들거나 고칠 때 필요한 것** — 최종 수치, 설계 결정과 그 근거, 밟으면 안 되는 함정,
> 다음 칩을 추가하는 레시피 — 만 남겼다.
>
> ⚠️ **코드 주석 표기 규칙** — 씨앗 폭탄 **`Plan5(씨앗칩)`** / 물대포 **`Plan5(물대포칩)`** / 기본 공격 **`Plan5(발톱칩)`**
> / 자판기 기본 공격 **`Plan5(동전칩)`**.
> 코드 주석에 있던 이 문서의 절 번호(`§4.2` 등)는 **전부 제거했다.**
> 절 번호는 다시 붙이지 않는다 — 문서가 바뀔 때마다 조용히 틀린 참조가 되기 때문이다.
> 대신 **결정 번호(D-n)** 와 **함정 번호(T-n)** 로 가리킨다. 이 둘은 문서 구조가 바뀌어도 유지되며,
> 빠진 번호는 결번이다(재사용하지 않는다).
>
> ⚠️ 코드에 남아 있는 `Plan5 §5.2` 같은 **접두 없는** 참조는 **삭제된 "청소기/탑승 계획서"** 를
> 가리키는 별개의 것이다(`DRVacuumDash.cpp` 등). 이 문서와 무관하므로 건드리지 않았다.

---

## 1. 칩 12종 최종 스펙

공통: 로봇 전용(`bCommonChip = false`), `Category = Ascension`, `RequiredSlotCount = 1`.
모디파이어는 동전 ①의 발사 간격(`Flat`)을 빼면 전부 `Op = Percent`(곱연산)이며 **각 모디파이어마다 `TargetAbilityTag` 를 지정**한다
(칩 레벨의 `TargetAbilityTag` 는 표기용이다 — 모디파이어에서 빠뜨리면 그 로봇의 **모든 스킬**에 퍼진다.
실제로 `GR.Asc.SeedSupport` 가 이렇게 빠져 정원로봇 전 스킬 피해가 −80% 였다 — 커밋 `0612381c` 에서 수정).

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

### 1.4 자판기 기본 공격(동전) 3종 — `OwnerClass = VendingMachine`, `TargetAbilityTag = Abilities.VendingMachine.BasicAttack`

| # | `ChipId` | 이름 | Modifiers | GrantedTags | Tier | 아이콘 |
|---|---|---|---|---|---|---|
| ① | `VM.Asc.CoinHeavy` | 고액 동전 | `SkillDamage` **+0.2**<br>`SkillFireInterval` **Flat +0.4** ★단점★ | (없음) | 3 | 공격-3 |
| ② | `VM.Asc.CoinDrain` | 거스름 동전 | `SkillDamage` **−0.3** ★단점★ | `…Coin.HitHeal` | 4 | 지원-3 |
| ③ | `VM.Asc.CoinWide` | 대형 동전 | `SkillProjectileScale` **+0.3**<br>`SkillDamage` **−0.3** ★단점★ | (없음) | 3 | 편의-3 |

기준값(에셋 실측): 동전 피해 **20**, 발사 간격 **0.6초**(C++ 기본 0.3 을 `GA_VendingMachineBasicAttack` BP 가 덮어씀),
잭팟 = 동전 **10발** 뒤 캡슐 1발(브론즈 10 / 실버 50 / 골드 150, 확률 50/40/10%), 동전 판정 구 반경 24.3uu.
공격 속도 버프(스킬 2)는 발사 간격 ÷ (1 + 0.2 × 스택), 최대 10스택.

- ① 동전·캡슐 피해 ×1.2, 발사 간격 0.6 → **1.0초**(버프는 그 위에 곱해진다 — D-38).
  ★`Flat +0.4` 는 "1.0초 고정"이 아니라 "기준 +0.4초"다★(T-38). 단독 DPS 는 **×0.72** 다(§9).
- ② 동전·캡슐이 **적에게** 맞을 때마다 시전자 체력 **+2**(고정량 — 피해·취약·엘리트 무관). 벽·바닥·클렌저 사이트·
  아군·죽은 적은 제외(D-41). **오염 중이거나 회복 차단(⑥)·만피면 없다**(D-42 / D-43).
- ③ 동전·캡슐의 외형과 판정 구가 함께 **×1.3**(반경 31.6uu). 벽·바닥에도 먼저 닿는다(T-43). 임팩트 이펙트는 그대로.
- **잭팟 캡슐도 기본 공격이다** — 세 칩 모두 캡슐에 적용되고, 캡슐 피해도 `SkillDamage` 를 탄다(D-39).

### 1.5 조합 결과 (곱연산이라 순서 무관)

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

**동전** — 기준: 한 발 20, 간격 0.6초, 버프 0스택 (캡슐 10/50/150 도 같은 피해 배율)

| 조합 | 한 발 | 간격 | 크기 | 적중 회복 | 동전 DPS |
|---|---|---|---|---|---|
| ① | **24** | **1.0** | ×1 | — | 24.0 (**×0.72**) |
| ② | **14** | 0.6 | ×1 | 2/발 (3.3/초) | 23.3 (×0.70) |
| ③ | **14** | 0.6 | **×1.3** | — | 23.3 (×0.70) |
| ①+② | 16.8 | 1.0 | ×1 | 2/발 (2.0/초) | 16.8 |
| ②+③ | **9.8** | 0.6 | ×1.3 | 3.3/초 | 16.3 — ★−60% 가 아니라 ×0.49★ |
| ①+②+③ | 11.76 | 1.0 | ×1.3 | 2.0/초 | 11.8 |

잭팟·② 회복은 "발사 수" 기준이라 ①을 끼면 초당 캡슐·초당 회복도 40% 준다. ① + 버프 10스택 = 0.333초.

**공용 칩·다른 칩과의 상호작용**

| | 효과 |
|---|---|
| ⑥ 광폭화 / ⑦ 시너지 | `ExecCalc_Damage` 소스측에서 **추가로 곱해진다**. 물대포 ②의 피해 0 에는 무효(0 × 무엇이든 0). 발톱 ②의 **흡혈**은 최종 피해 기준이라 같이 늘지만, **아군 수혈량**은 `GetUpgradedDamage()` 기준이라 이 배율을 받지 않는다 |
| ⑧ 피의 계약 | 물 대신 체력으로 지불. ★물대포 ②의 −80% 가 **체력 소모까지 80% 줄여 준다**★ (`CheckCost` 가 `EffectiveWaterCost × Ratio` 를 쓰기 때문). 발톱 ②와 겹치면(기본 공격에 물 소모가 있을 때) 체력 소모처가 둘이 되지만 둘 다 체력 0 방지 가드가 있다(`CheckCost` / D-26) |
| ⑥ 광폭화 + 씨앗 ② | 아군 힐은 정상, **본인은 회복 차단**(NoHealing — 받는 쪽 판정) |
| ⑥ 광폭화 + 발톱 ② | ★**서로를 죽이는 조합**★ — 본인이 ⑥이면 흡혈이 전부 버려진다. 아군이 ⑥이면 수혈이 발동하지 않는다(비용도 0 — D-33) |
| 물대포 ② → 발톱 ② | 취약(`Debuff.Vulnerable`) 걸린 적을 발톱으로 치면 피해 ×1.5 → **흡혈도 ×1.5** |
| ⑥ 광폭화 + 동전 ② | 동전 ② 완전 무력화 — 회복 차단이라 `GetHealableAmount` 가 0 → GE 를 걸지도 않는다 |
| 물대포 ② → 동전 | 취약 적에게 동전·캡슐 피해 ×1.5. 동전 ② 회복은 고정 2 그대로 |
| 오염 | 동전 ② 차단(D-42). ★발톱 ② 흡혈은 오염 중에도 회복 → 즉시 정화된다★(§9) |

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
      │                 · 발사 간격 = GetCurrentFireInterval()      ┐ UDRVendingMachineBasicAttack
      │                              ÷ (1 + 0.2 × 공격 속도 버프 스택) │ (C++ 안에서 끝 — BP 배선 없음)
      │                 · 캡슐 피해 = GetUpgradedFloat(SkillDamage, 캡슐 피해) ┘
      │                 · 투사체 크기 = GetUpgradedProjectileScale() → 스폰 트랜스폼 Scale3D
      │                               (UDRProjectileSpell::SpawnProjectile 공용 + 잭팟 캡슐 분기)
      │
      ├─► 액터 안:      UDRAbilitySystemLibrary::ApplySkillUpgrade(SourceASC, SourceAbilityTags, Stat, Base)
      │                 (ADRSeedProjectile 이 BeginPlay 에서 1회 해석해 캐시)
      │
      ├─► 규칙 분기:    UDRAbilitySystemLibrary::HasUpgradeFeature(ASC, FeatureTag)
      │                 (UDRWaterPump 는 StartWaterPumpLoop 에서 1회 해석해 캐시,
      │                  UDRMeleeAttack 은 캐시 없이 매번 — 스윙당 조회가 몇 번뿐이다)
      │
      └─► 맞는 쪽:      UDREnemyAttributeSet::HandleIncomingDamage   (받는 쪽 배율이 끝난 최종 피해를 아는 곳)
                        · 흡혈 = GetLifestealRatioForDamage(Props) — 시전자 피처 태그 AND 발톱 피해
                        · 적중 회복 = GetHitHealAmountForDamage(Props) — 피처 태그 AND 동전 피해 AND 시전자 비오염

[DA_ProgressionConfig.ChipTuning]  숫자로 표현 못 하는 동적 상수
   └ .SeedBomb / .WaterPump / .Claw / .Coin  →  UDRAbilitySystemLibrary::GetChipTuning(WorldContext)
```

### 2.2 스탯 축 (`EDRUpgradeStat`) — 전부 곱연산

| 구분 | 축 |
|---|---|
| 캐릭터 단위 | `ContainerHealth` `MaxWater` `MoveSpeed` `DamageTaken` `WaterGain` `MaxHealth` `HealthRegenDelay` `DamageDealt` |
| 스킬 단위 | `SkillDamage` `SkillWaterCost` `SkillCooldown` `SkillProjectileCount` **`SkillHealing`** **`SkillAreaRadius`** **`SkillWaterGrant`** **`SkillWindup`** **`SkillFireInterval`** **`SkillProjectileScale`** |

★굵은 6개가 Plan5 에서 추가됐다.★ 스킬 단위 축은 **반드시 `DRIsSkillStat()` 에도 추가**해야 한다(T-12).

| 축 | 단위 | `DRIsLowerBetter` | `DRIsTimeStat` | 이유 |
|---|---|---|---|---|
| `SkillWindup` | 기준 1.0 배율 | ✓ | ✗ (T-25) | 선딜 길이는 몽타주 안에 있어 어빌리티가 초를 모른다(D-21) |
| `SkillFireInterval` | **초** | ✓ | **✓** | 발사 간격은 GA 프로퍼티라 초를 안다(D-37). 빠뜨리면 미리보기 색·단위가 틀린다(T-37) |
| `SkillProjectileScale` | 기준 1.0 배율 | ✗ | ✗ | 투사체 반경은 발사체 BP 에 있다(D-40) |

**`Mult` 는 곱연산 누산기다** — `Percent` 에서 이름을 바꾼 이유가 그것이다(D-7).
`Mult *= (1 + Value)` 로 누적되므로 감소 칩을 아무리 겹쳐도 부호가 뒤집히지 않는다
(합연산이던 시절 −80% + −40% = −120% → 배율 음수 → **적이 회복**하는 버그가 있었다).

### 2.3 세 종류의 태그 — ★절대 섞지 말 것★

| | **칩 피처 태그**<br>`Upgrade.Feature.*` | **GAS 태그**<br>`Buff.*` / `Debuff.*` | **SetByCaller 태그**<br>`Data.*` |
|---|---|---|---|
| 어디에 사나 | `FDRUpgradeRuntime::GrantedTags` — **ASC 에 없다** | **ASC 에 있다** (GE 의 Granted Tags) | GE 스펙 안 — 크기를 넘기는 키일 뿐, 어디에도 부여되지 않는다 |
| 언제 변하나 | 로드아웃이 바뀔 때만 | 런타임에 수시로 | GE 를 적용할 때마다 |
| 쓰는 법 | `HasUpgradeFeature(ASC, Tag)` | `HasMatchingGameplayTag()` / GE 핸들의 `GetCurrentStackCount()` | `SetSetByCallerMagnitude(Tag, 값)` |
| Plan5 에서 추가 | `…SeedBomb.{Refund,Slow,Haste}`<br>`…WaterPump.{Ramp,Slow,Vulnerable,Pierce}`<br>`…Claw.{Lifesteal,AllyHeal}`<br>`…Coin.HitHeal` | `Buff.WaterPump.Pressure`<br>`Debuff.Vulnerable`<br>(에셋 전용) `Debuff.SeedBomb.Slow` `Buff.SeedBomb.Haste` `Debuff.WaterPump.Slow` | `Data.MoveSpeed.Mult`<br>`Data.Duration`<br>`Data.HealthCost` |

- 피처 태그를 **ASC 에 Loose 로 얹지 말 것** — 어빌리티의 `ActivationBlockedTags` 와 의도치 않게 상호작용한다.
- C++ 이 읽는 GAS 태그만 **네이티브**로 등록한다. VFX/에셋에서만 쓰는 태그는 에디터에서 만들면 된다.
- 발톱칩·동전칩은 새 GAS 태그가 없다 — 지속 상태 없이 즉발 효과뿐이다.

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
| **K** | 스탯 축 `SkillFireInterval`(초) · `SkillProjectileScale`(배율) | `DRUpgradeTypes.h` — `DRIsSkillStat` 둘 다, 발사 간격만 `DRIsLowerBetter` + `DRIsTimeStat` |
| **L** | 투사체 크기 = 스폰 스케일 | `UDRProjectileSpell::GetUpgradedProjectileScale()` — `SpawnProjectile` 에서 공용 적용(칩 없으면 1.0) |
| **M** | 적중 회복(고정량) 판정·지급 | `GetHitHealAmountForDamage()` → G 관문에서 흡혈 블록 바로 아래 지급 |

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

| **D-36** | 동전 3종 = 자판기 전용 `Ascension`, `ChipId = VM.Asc.Coin{Heavy,Drain,Wide}`, Tier 3/4/3 | 발톱 세트와 1:1 대응. 셋 다 `bIsDrawback` 모디파이어를 가져 D-1 규칙 유지. ①은 원래 "0.5초"였으나 현재값이 0.6초라 단점이 사라져 **1.0초로 확정**(사용자) |
| **D-37** | 발사 간격 = **새 축 `SkillFireInterval`(초)**, ①은 `Flat +0.4` | `SkillCooldown` 은 `ApplyCooldown` 이 쿨다운 GE 에 쓰는 축 — 기본 공격에 쿨다운 GE 가 붙으면 이중 적용된다. 발사 간격은 GA 프로퍼티라 초를 안다(D-21 과 반대 상황) |
| **D-38** | 칩과 공격 속도 버프는 **곱**: `간격 = ApplySkill(SkillFireInterval, Base) ÷ (1 + 0.2 × 스택)` | 버프 공식은 그대로 두고 "기준 간격"만 바꾼다. 순서 무관 |
| **D-39** | **잭팟 캡슐도 기본 공격** — 세 칩 모두 적용. 캡슐 피해 = `GetUpgradedFloat(SkillDamage, 캡슐 피해) × GetRuntimeDamageMultiplier()` | 사용자 확정. 상수 덮어쓰기는 T-1 과 같은 누락(②③의 −30% 가 캡슐에서 샜다). `UDRVacuumAirShot` 선례와 같은 보정 |
| **D-40** | 투사체 크기 = **새 축 `SkillProjectileScale`(기준 1.0 배율)** → 스폰 트랜스폼 Scale3D, 공용 `SpawnProjectile` 에서 적용 | 액터 스케일은 외형과 판정 구를 함께 키운다. `SkillAreaRadius`(효과 반경)와 뜻이 달라 재사용 기각. 적 GA 는 칩 캐시가 비어 항상 1.0. 스폰 스케일은 `SerializeNewActor` 가 클라로 보낸다(엔진 확인, Iris 미사용) |
| **D-41** | 적중 회복은 **맞는 쪽** `HandleIncomingDamage` 에서 정산, 판정은 `GetHitHealAmountForDamage` 하나(피처 AND 컨텍스트에 `VendingMachine.BasicAttack`) | "적에게 피해가 들어갔다"가 확정되는 유일한 지점 — 벽·사이트·아군·죽은 적·두더지는 구조적으로 안 온다. `ADRProjectile` 은 적 투사체와 공유라 칩 지식을 넣지 않는다 |
| **D-42** | **오염 중엔 적중 회복 없음** — 판정 함수가 ★시전자★ 의 `IsCorrupted()` 를 본다 | 사용자 확정. `HandleCorruptionPurification` 은 회복량을 보지 않아(정화 + 체력 = 컨테이너 1칸) 막지 않으면 "적 1회 적중 = 정화" |
| **D-43** | 회복량은 **고정 2**(`ChipTuning.Coin.HitHealAmount`). 걸기 전에 `GetHealableAmount` 로 잘라 만피·회복 차단이면 GE 를 걸지 않는다 | 원문이 "체력 2". `SkillHealing` 은 "아군에게 주는 회복" 축이라 뜻이 다르다. 적중이 잦다(초당 1.7~5회) |
| **D-44** | 발사 간격 하한 `MinFireInterval = 0.05초` | `SetTimer` 는 Rate ≤ 0 이면 타이머를 지운다(T-35) |
| **D-45** | 동전칩은 **BP 재배선 · 새 GE 없음** | 효과가 전부 C++ 경로(`StartAutoFire` · `ExecuteShot` · `SpawnProjectile`)와 피해 관문 안에서 끝난다 |
| **D-46** | (미적용 · 권장) `ValidateCatalog` 에 "칩 레벨 대상 스킬이 있는 전용 칩의 스킬 단위 모디파이어는 `TargetAbilityTag` 필수" | `GR.Asc.SeedSupport` 누락 같은 실수를 검사로 막는다(§9) |

> 보류: `EDRUpgradeOp::Override`("값을 N 으로 **고정**")는 소비자가 없어 범위에서 뺐다.
> (동전 ①의 "1.0초로 변경"도 Override 대신 `Flat +0.4` 로 풀었다 — T-38)
> 나중에 필요해지면 ★`MaxHealth`/`MaxWater`/`MoveSpeed` 는 `GetGlobalStat()` 을 직접 꺼내 쓰므로
> Override 를 조용히 무시한다★ — `ValidateCatalog()` 로 막는 것이 필수다.

---

## 4. 코드 지도

| 파일 | 이 기능에서 하는 일 |
|---|---|
| `Public/Game/DRUpgradeTypes.h` | 스탯 축 열거형, `DRIsSkillStat()` / `DRIsLowerBetter()`, `FDRResolvedStat{Flat,Mult}`, `AddModifier`/`ApplySkill`(곱연산) |
| `Public/Game/DRProgressionConfig.h` | `FDRTimedSpeedEffect` / `FDRSeedBombChipTuning` / `FDRWaterPumpChipTuning` / `FDRClawChipTuning` / `FDRChipTuning` |
| `Public/DRGameplayTags.h` · `.cpp` | 피처 태그 9개 + GAS 태그 2개 + SetByCaller `Data.MoveSpeed.Mult` / `Data.Duration` / `Data.HealthCost` |
| `AbilitySystem/DRAbilitySystemLibrary` | `GetUpgradeRuntimeForASC` / `HasUpgradeFeature` / `GetChipTuning` / `GetUpgradeKeyTagFromAbilityTags` / `ApplySkillUpgrade` / `ApplyTimedSpeedEffect` / `ApplyTimedTagEffect` / **`ApplyHealToActor`**(오버로드 2개 — `UClass*` 를 넘기면 C2668 모호 에러, `TSubclassOf` 로 감쌀 것) / **`GetHealableAmount`** / **`GetLifestealRatioForDamage`** / **`GetHitHealAmountForDamage`** |
| `Abilities/DRGameplayAbility` | `GetUpgradedFloat` / `GetEffectiveWaterCostFor` / `ApplyCooldown` |
| `Abilities/DRDamageGameplayAbility` | `GetUpgradedDamage()` = `ApplySkill(SkillDamage) × ` **`GetRuntimeDamageMultiplier()`**(가상 훅, 기본 1.0) |
| **`Abilities/DRWaterPump`** | 칩 해석 캐시 `Resolved` / 관통 트레이스 `TraceBeamBlocker` / 대상 수집 `CollectTargetsInBeam` + 칩 규칙 필터 `NarrowTargetsToChipRules` / `ApplyChipDebuffs` / `HasEnemyTarget` + `AdvanceRampStack` + `RampStackHandle` / `ClearRampStacks` / `GrantWaterToAlly` / `GetRuntimeDamageMultiplier` 오버라이드 |
| **`Abilities/DRMeleeAttack`** | `AttackRadius` / `BaseMontagePlayRate` / `GetUpgradedAttackRadius` / `GetWindupPlayRate` / `RestoreMontagePlayRate`(1P·3P 메시 모두) / `TryHealAllyWithOwnHealth` — 피해량 → D-33 으로 자르기 → D-26 으로 자르기 → **비용 먼저, 회복 나중**("깎였으면 회복도 갔다"가 항상 성립) |
| **`Actor/DRSeedProjectile`** | `FDRSeedBombResolved` 캐시 / 직격 판정 `bDirectHitOnEnemy` / `GrantWaterRefund` / 감속·가속 부여 / 아군 회복은 `ApplyHealToActor` 위임(D-34) |
| `Abilities/DRSeedCannon` | `SpentWaterCost` 주입(환급 근거) |
| `ExecCalc/ExecCalc_Damage` | 주는 피해 관문 — `DamageDealt`·광폭화·시너지, **음수 피해 0 클램프** |
| **`DREnemyAttributeSet`** | 받는 피해 관문 — 엘리트 배율 + **`Debuff.Vulnerable` ×1.5** + **흡혈 지급**(★`SetHealth` 전★ — T-24) + **적중 회복 지급**(흡혈 바로 아래, 더미 분기 위 — T-40) |
| `DRChipCatalog.cpp` | `ValidateCatalog()` — 스킬 태그/공용 칩/`MaxHealth` Flat 전용/**`Percent ≥ −1.0`** 검사 |
| `UI/DRUpgradeUILibrary.cpp` | 스탯 짧은 이름(5자 제한) — `SkillWindup` = "선딜" / `SkillFireInterval` = "발사 간격" / `SkillProjectileScale` = "탄 크기" |
| **`Abilities/DRVendingMachineBasicAttack`** | `GetCurrentFireInterval()` = 칩 간격 ÷ 버프, `MinFireInterval` 바닥 / 잭팟 캡슐 분기의 **스케일 + `SkillDamage`** 두 줄(T-36) |
| **`Abilities/DRProjectileSpell`** | `GetUpgradedProjectileScale()` + `SpawnProjectile()` 스폰 트랜스폼 Scale3D (적 GA 도 지나가지만 항상 1.0) |

**BP 쪽 계약** (깨지면 조용히 오동작한다)

| GA | 계약 | 깨지면 |
|---|---|---|
| `GA_WaterPump` | `OnDamageTickReached` 는 **매 호출마다** `Make Damage Effect Params From Class Defaults` 를 부른다(결과 캐시 금지) | 램프 증가가 반영되지 않는다 |
| `GA_WaterPump` | 같은 이벤트의 아군 분기는 `Apply Gameplay Effect To Target` 이 아니라 **`Grant Water To Ally`** | 칩이 반영된 급수량(SetByCaller)이 안 들어간다 |
| `GA_ClawSwipe` | `Play Montage and Wait` **1P·3P 둘 다** `Play Rate` ← `Get Windup Play Rate` | 선딜이 안 늘어난다 / 한쪽만이면 손과 몸이 따로 논다(T-26) |
| `GA_ClawSwipe` | 타격 이벤트(`Event.Montage.ClawSwipe`) 맨 앞에 `Restore Montage Play Rate` — **서버 전용 분기 밖**(모든 머신에서 불려야 한다) | 후딜까지 3배 |
| `GA_ClawSwipe` | `Get Live Objects Within Radius` 와 디버그 구의 `Radius` ← `Get Upgraded Attack Radius` | 범위 칩 무반응 / 그려진 구와 실제 판정이 어긋난다 |
| `GA_ClawSwipe` | `Branch(Is Valid Damage Target)` 의 **False** → `Try Heal Ally With Own Health`(같은 `Array Element`). 칩 유무는 C++ 이 판단하므로 BP 는 **항상** 부른다 | 수혈 무반응 |
| `GA_VendingMachineBasicAttack` | ★계약 없음★ — 동전칩은 BP 를 건드리지 않았다(D-45). 단 `AbilityTags` 는 `Abilities.VendingMachine.BasicAttack` 하나로 유지 | 업그레이드 키(첫 `Abilities.*` 태그)가 바뀌면 칩 전체가 무반응 |

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

### 5.5 `ChipTuning.Coin`

| 필드 | 값 | 비고 |
|---|---|---|
| `HitHealAmount` | 2.0 | 적중 1회 회복량(고정). 구조체 기본값 |
| `HealEffect` | `GE_Heal` | ★`Claw.HealEffect` 와 별개 필드 — 비우면 동전 ② 무동작★(T-39) |

동전칩 카탈로그의 `SkillDisplayNames` 에 `Abilities.VendingMachine.BasicAttack → 기본 공격` 이 있다(정원로봇 3종 이름도 함께 채워져 있다).
아이콘은 `Content/DaeRuneAssets/UI/Upgrade/Icon/Vending/` (`공격-3` · `지원-3` · `편의-3`, 4·5 번은 예비).

### 5.6 GE · GA 에셋 요구 사양

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
| `GA_VendingMachineBasicAttack` | `BaseFireInterval = 0.6` (C++ 기본 0.3 을 덮어쓴 값 — ①의 `Flat +0.4` 가 이 값 기준, T-38) / `MinFireInterval = 0.05`(C++ 기본) / `MaxJackpotStacks = 10` |
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
| **T-35** | 발사 간격 계산이 0 이하 | `SetTimer` 는 Rate ≤ 0 이면 ★타이머를 지운다★ → 버튼을 누르고 있어도 연사가 조용히 멈춘다. `MinFireInterval` 로 바닥(D-44) |
| **T-36** | 잭팟 캡슐 분기에 크기·피해 중 하나만 넣기 | 캡슐은 `SpawnProjectile` 을 안 탄다. 두 줄 중 하나라도 빠지면 ★캡슐만★ 칩을 무시한다 |
| **T-37** | `SkillFireInterval` 을 `DRIsLowerBetter` / `DRIsTimeStat` 에서 빠뜨리기 | 전자: ①의 단점(+0.4초)이 미리보기에 ★초록★. 후자: 상세 문구가 "+0.4" 로만 나와 단위를 모른다 |
| **T-38** | 동전 ①의 `Flat +0.4` 를 "1.0초 고정"으로 착각 | 기준 +0.4초다. BP 의 `BaseFireInterval`(0.6)을 바꾸면 결과도 바뀐다 → 카탈로그 `Value` · `Description` 을 같이 고친다 |
| **T-39** | `ChipTuning.Coin.HealEffect` 미지정 / ★칩 ②의 `GrantedTags` 누락★ | 둘 다 동전 ② 가 **에러 없이** 무동작한다. 태그 누락은 `ValidateCatalog` 도 못 잡는다(단점 모디파이어가 있어서) |
| **T-40** | 적중 회복 블록 위치 | `LocalIncomingDamage > 0` 가드 ★안★, 튜토리얼 더미 조기 return ★위★(= 흡혈 블록 바로 아래) |
| **T-41** | 오염 판정을 타깃에서 하기 | 오염은 플레이어에게만 있다 — 판정은 ★시전자★(`Props.SourceASC`)의 `UDRPlayerAttributeSet` |
| **T-42** | 스폰 후 `SetActorScale3D` 로 키우기 | 크기는 스폰 ★전★ 트랜스폼에 넣는다(`SpawnActorDeferred` · `FinishSpawning` 같은 트랜스폼). 사후 변경은 초기 오버랩·복제 스폰 정보와 어긋날 수 있다 |
| **T-43** | 큰 투사체의 부작용 | 동전은 중력 1.0 포물선이라 바닥·벽 모서리에 먼저 닿는다 / 초기 오버랩도 판정되므로(`DispatchBeginPlay` → `UpdateInitialOverlaps`) 총구가 벽에 31.6uu 이내면 쏘자마자 사라진다 / 브론즈·실버 캡슐 메시 오프셋(−120 / −61uu)도 1.3배로 벌어진다 |
| **T-44** | 클라 로그로 발사 간격 디버깅 | 발사는 ★서버 타이머★ 가 정한다(`ExecuteShot` 권한 체크). 서버(호스트) 로그를 볼 것 |
| **T-45** | 헤더 변경을 Live Coding 으로 반영 | `UENUM` 값 · `USTRUCT` 필드 추가는 리플렉션 변경 — 에디터를 닫고 빌드한다 |

---

## 7. 다음 칩을 추가하는 레시피

| 유형 | 예 | 해야 할 일 |
|---|---|---|
| **1. 순수 수치** (C++ 0줄) | "물대포 쿨다운 −20%" / "에어샷 투사체 크기 +30%"(`SkillProjectileScale` 은 `SpawnProjectile` 공용이라 이미 읽힌다) | 카탈로그에 `{Stat, TargetAbilityTag, Op, Value, bIsDrawback}` 한 줄. **조건**: 그 스탯을 이미 `Apply()`/`ApplySkill()` 로 읽고 있어야 한다 |
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
| 스폰 경로가 따로 있는 투사체(잭팟 캡슐 등)에 상수 피해를 그대로 넣기 | `SkillDamage` 칩이 통째로 빠진다(T-1 / D-39) — `GetUpgradedFloat(SkillDamage, 값)` 을 태울 것 |
| 칩 효과를 `ADRProjectile` 에 넣기 | 적 투사체와 공유 클래스다. 판정은 맞는 쪽 관문에서(D-41) |

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
| **14** | ★동전 ①은 단독 DPS **×0.72**★ — 한 발 +20% 보다 연사 −40% 가 크다(초당 캡슐·초당 ② 회복도 준다) | 사용자 확정값. "한 방은 세고 DPS 는 비슷한" 칩을 원하면 피해 **+67% 이상**이 필요 — 플레이 후 카탈로그 `Value` 만 조정 |
| **15** | 발톱 흡혈은 오염 중에도 회복된다 → 적 1회 타격으로 **즉시 정화**(동전 ②는 D-42 로 막음) | 발톱에도 막을지 사용자 결정. 막는다면 `GetLifestealRatioForDamage` 에 오염 검사 3줄 |
| **16** | 동전 ③으로 커져도 임팩트 이펙트(`NS_Basic_3_*`)는 그대로 | 12번과 같은 판단 |
| **17** | 공격 속도 버프의 스택당 0.2 가 C++ 리터럴(`GetCurrentFireInterval`) | 스킬 2 칩을 만들 때 `ChipTuning` 또는 GE 로 이관 |
| **18** | 잭팟 캡슐 분기에 `Projectile` null 체크가 없다 | 기존 코드. `SpawnActorDeferred` 실패 시 크래시 — 가드 추가 권장 |
| **19** | 주석이 실제 값과 다르다: `DRVendingMachineBasicAttack.h` "일반 공격 5회 후"(BP 10) · `BaseFireInterval = 0.3f`(BP 0.6) / `DRVendingMachineAttackSpeedBuff.h` "+10%"(코드는 스택당 +20%) | 주석만 고치면 된다 |
| **20** | ~~D-46 검증 규칙 미적용~~ → **§12 에서 D-51 로 적용됨** | `ValidateCatalog` 모디파이어 루프에 조건 1개(`!bCommonChip && 칩 태그 있음 && DRIsSkillStat && 모디파이어 태그 없음` → 오류) |

---

## 10. 게임 내 검증 — ★미확인★

### 10.1 발톱칩

에디터 작업(`GE_Claw_SelfCost` · `GA_ClawSwipe` 재배선 · `ChipTuning.Claw` · 카탈로그 3종)은 에셋에 반영된 것까지 확인했다.
아래는 **플레이로만** 확인할 수 있는 핵심 항목이다. **전부 통과하면 그 절을 지우고 머리말의 상태를 갱신한다.**

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

### 10.2 동전칩

C++ 는 빌드 통과(2026-09-27), 에셋(카탈로그 3종 · `ChipTuning.Coin` · 아이콘 · 스킬 이름표)은 입력된 것까지 확인했다 — **단 §11 부터**.
준비: 자판기를 고른 뒤 콘솔 `DRUnlockUpgrade` → `DRUnlockSlot 6`. 발사 간격은 10초간 발사 수로 잰다(0.6초 ≈ 17발, 1.0초 = 10발).

- [ ] **회귀(칩 없음)** — 동전 20 / 0.6초 / 캡슐 10·50·150 / 크기 그대로 / 적 원거리·잠자리·청소기 에어샷 투사체 그대로 / 발톱 흡혈 그대로
- [ ] **①** 동전 24 / 1.0초 / 캡슐 12·60·180 / 공격 속도 버프가 그 위에 그대로 걸린다(10스택 0.333초)
- [ ] **②** 적 적중마다 +2(캡슐 포함) / 동전 14 / 벽·바닥·클렌저 사이트·만피 → 없음 / ★오염 중 → 오염 유지·회복 없음★ / ②+⑥ → 없음
- [ ] **③** 동전·캡슐 ×1.3 · 판정 확대 / 동전 14 / 벽에 붙어 쏘기·바닥 쪽 사격이 과하게 일찍 터지지 않는가(T-43)
- [ ] **조합** ②+③ 한 발 9.8(×0.49) / ①+②+③ 11.76
- [ ] **멀티** 클라 자판기 ③ → ★호스트 화면에서도 크다★(스폰 스케일 복제) / 클라 ② → 클라 체력바 +2 / 클라 ① → 모든 화면 1.0초
- [ ] **UI** 카드 "동전 피해 +20%" · "적중 시 체력 +2" · "동전 크기 +30%" / 미리보기 "기본 공격 · 스킬 발사 간격 +0.4"(빨강)

---

## 11. 동전칩 남은 에디터 작업

2026-09-27 에셋을 다시 읽어 확인한 상태다. **나머지는 전부 들어가 있다**(칩 3종 수치·태그·Tier·아이콘, `ChipTuning.Coin.HealEffect = GE_Heal`, `SkillDisplayNames` 4줄).

| # | 할 일 | 방법 | 안 하면 |
|---|---|---|---|
| **1** | ★`VM.Asc.CoinDrain` 의 `Granted Tags` 에 `Upgrade.Feature.Skill.Coin.HitHeal` 추가★ | `DA_ChipCatalog` → 거스름 동전 칸 → `Granted Tags` 클릭 → 검색 `Coin` → 체크 → 저장 | **동전 ②의 회복이 전혀 없다**(피해 −30% 만 걸린다). 검사에도 안 걸린다(T-39) |
| 2 | (선택) `VM.Asc.CoinHeavy` 발사 간격 줄의 `Detail Override` = `발사 간격 1.0초 (기존 0.6초)` | 같은 파일 | 상세 문구가 자동 문구 "스킬 발사 간격 +0.4초" 로 나온다(틀리진 않음) |
| 3 | 저장 후 검사 로그 확인 | `DA_ProgressionConfig` 에서 아무 값이나 바꿨다 되돌리기 → 출력 로그에 `ProgressionConfig` 검색 → `VM.Asc.Coin` 이 들어간 줄 0개 | — |

그다음 §10.2 를 플레이로 확인한다. 전부 통과하면 §10.2 · §11 을 지우고 머리말 상태를 "완료"로 바꾼다.

---

## 12. 같은 스킬 칩은 동시에 1개만 — C++ 완료 · BP 남음

> 2026-09-27 요청: "같은 스킬과 관련된 칩은 동시에 1개만 장착 가능". 결정 **D-47 ~ D-52**, 함정 **T-46 ~ T-50**.
> 코드 주석 접두는 **`Plan5(스킬당1칩)`**. 구현·검증이 끝나면 이 절을 §1~§9 에 흡수한다.
>
> **진행 (2026-09-27)** — 충돌 시 **거부** 방식 확정(사용자). §12.3 ①~⑤ 적용, `DaeRuneEditor` 빌드 통과(변경 파일 경고 0).
> ⑥은 **불필요해 생략**: 기본 장착 칩은 이미 "공용 칩이어야 한다" 검사가 있고, 공용 칩은 묶음 키가 비어 있어 겹칠 수 없다.
> 현재 카탈로그를 에셋으로 읽어 새 검사 규칙(D-51)을 미리 돌려 봤다 → **오류 0건**, 묶음 4그룹 × 3칩(씨앗·물대포·발톱·동전).
> 남은 것: §12.4 `WBP_UpgradeChip` 사유 문구(선택) → §12.6 검증.

### 12.1 규칙

- **"같은 스킬" = 칩 레벨 `TargetAbilityTag` 가 정확히 같은 칩**(D-47).
  현재 해당 그룹 4개 — 씨앗 폭탄 `GR.Asc.Seed*` / 물대포 `GR.Asc.Pump*` / 발톱 `GR.Asc.Claw*` / 동전 `VM.Asc.Coin*` → **그룹마다 1개**.
- 칩 레벨 태그가 **비어 있는** 칩(공용 `CM.*` 전부, 로봇 전체 대상 칩)은 제한이 없다.
- 이미 같은 그룹 칩이 끼워져 있으면:
  - 빈 칸에 장착(클릭 · 빈 칸 드롭) → **거부**, 사유 `SkillConflict`(D-48)
  - **그 칩이 있는 칸에 드롭 → 교체 허용**(기존 드롭 교체 규칙 그대로 — D-48)
  - 칸끼리 이동(`MoveChip`) → 장착 집합이 안 바뀌므로 검사하지 않는다
- 로봇당 최대: 스킬 칩 3개(정원로봇 3스킬) + 공용 칩. 슬롯 6칸 구성은 그대로.

**영향** — §1.5 조합 표의 같은 스킬 조합(씨앗 ②+③, 물대포 ①+③, 발톱 ①+②, 동전 ②+③ …)은 **전부 불가능해진다.**
구현 후 §1.5 는 "공용 칩·타 스킬 칩과의 상호작용"만 남기고, 같은 스킬 조합을 전제로 한 설명(T-28 의 "③과 겹치면 급격하다", 발톱 ①+② 흡혈 등)도 정리한다.

### 12.2 설계 결정

| # | 결정 | 근거 |
|---|---|---|
| **D-47** | 묶음 키 = **칩 레벨 `TargetAbilityTag`**(정확히 일치). 모디파이어 태그로 추론하지 않는다 | 칩 카드의 "적용 대상 스킬" 표기와 같은 값이라 플레이어가 보는 것과 규칙이 일치한다. 추론은 ②처럼 `GrantedTags` 만 있는 칩에서 틀린다. 대신 칩 태그 누락을 검증으로 막는다(D-51) |
| **D-48** | 충돌 시 **거부**(`EDRUpgradeResult::SkillConflict`) + **충돌 칩 칸에 드롭하면 교체 허용**. 자동 교체는 하지 않는다 | 클릭 한 번에 다른 칩이 조용히 빠지면 놀란다. 드롭 교체는 이미 있는 규칙(점유 칸 교체)이라 새 UI 가 필요 없다 |
| **D-49** | 판정은 **카탈로그 함수 하나** `UDRChipCatalog::FindSkillConflict()` 로 모은다 — 클라 장착 판정 · 서버 정화 · 세이브 정화 · 뷰모델이 전부 이것만 부른다 | `IsUsableBy` 처럼 한 곳이라도 빠지면 "장착은 되는데 다음 실행/멀티에서 사라지는" 늦은 증상이 난다(Plan3 교훈) |
| **D-50** | **기존 세이브**에 같은 그룹 칩이 2개 이상이면 로드 시 **가장 앞 칸의 칩만 남기고** 나머지를 뺀다(`SanitizeClassState` 3차 패스) | 칩은 사는 게 아니라 슬롯을 사는 구조라 환급이 필요 없다. 앞 칸 = 먼저 끼운 칩일 가능성이 높다 |
| **D-51** | `ValidateCatalog` 보강: 전용 칩인데 **스킬 단위 모디파이어가 있거나 스킬 피처 태그(`Upgrade.Feature.Skill.*`)가 있으면 칩 레벨 태그 필수** + 스킬 단위 모디파이어 태그는 칩 태그와 **같아야** 한다(§9-20 / D-46 흡수) | 칩 태그가 비면 이 규칙을 조용히 빠져나간다. 모디파이어 태그 누락(SeedSupport 사고)도 같이 잡는다 |
| **D-52** | 새 결과값 `SkillConflict` 는 `EDRUpgradeResult` **맨 끝**에 붙인다 | BP 의 Switch/바이트 값이 앞 값들과 어긋나지 않게(D-4 와 같은 이유) |

### 12.3 C++ 변경

**① `Public/Game/DRUpgradeTypes.h`**

```cpp
	// EDRUpgradeResult — PreviousSlotLocked 다음, 맨 끝에 (D-52)
	PreviousSlotLocked,		// 아직 해금되지 않은 칸 (순차 해금 — 앞 칸을 먼저 해금해야 한다)
	SkillConflict			// ★Plan5(스킬당1칩)★ 같은 스킬 칩이 이미 장착돼 있다 (그 칩 칸에 드롭하면 교체)
```

```cpp
	// FDRUpgradeChipDefinition — IsUsableBy 아래
	/**
	 * ★Plan5(스킬당1칩)★ 동시 장착 1개 제한의 묶음 키. 비어 있으면 제한 없음(공용·로봇 전체 칩).
	 * 칩 레벨 TargetAbilityTag 그대로다 — 카드의 "적용 대상 스킬" 표기와 같은 값 (D-47).
	 */
	FGameplayTag GetExclusiveSkillTag() const
	{
		return bCommonChip ? FGameplayTag() : TargetAbilityTag;
	}
```

**② `DRChipCatalog.h / .cpp`**

```cpp
	/**
	 * ★Plan5(스킬당1칩)★ ChipId 를 EquippedChips 에 더하면 같은 스킬로 부딪히는 칩. 없으면 NAME_None.
	 * IgnoreChipId 는 "곧 빠질 칩"(드롭 교체 대상)으로 취급해 건너뛴다.
	 * 장착 판정 · 서버 정화 · 세이브 정화 · 뷰모델이 전부 이 함수만 쓴다 (D-49).
	 */
	FName FindSkillConflict(const TArray<FName>& EquippedChips, FName ChipId, FName IgnoreChipId = NAME_None) const;
```

```cpp
FName UDRChipCatalog::FindSkillConflict(const TArray<FName>& EquippedChips, FName ChipId, FName IgnoreChipId) const
{
	const FDRUpgradeChipDefinition* Chip = FindChip(ChipId);
	if (!Chip) return NAME_None;

	const FGameplayTag Key = Chip->GetExclusiveSkillTag();
	if (!Key.IsValid()) return NAME_None;   // 공용·전체 칩은 제한 없음

	for (const FName& OtherId : EquippedChips)
	{
		if (OtherId.IsNone() || OtherId == ChipId || OtherId == IgnoreChipId) continue;
		if (const FDRUpgradeChipDefinition* Other = FindChip(OtherId))
		{
			if (Other->GetExclusiveSkillTag().MatchesTagExact(Key)) return OtherId;
		}
	}
	return NAME_None;
}
```

- `SanitizeLoadout()`(서버 정화) — 중복 Id 검사 다음에 한 줄: `if (!FindSkillConflict(Sanitized, ChipId).IsNone()) { bModified = true; continue; }`
  → 앞에 온 칩을 남긴다. `ADRPlayerController` 의 서버 RPC 가 이미 이 함수를 부르므로 **서버 쪽 배선은 끝**이다.
- `ResolveLoadout()` 은 건드리지 않는다 — 들어오는 목록은 이미 서버가 정화했다.
- `ValidateCatalog()` — D-51 규칙: 스킬 단위 모디파이어가 있거나 `Upgrade.Feature.Skill.*` 를 주는 전용 칩은 칩 태그 필수 / 모디파이어 태그 = 칩 태그. (§9-20 의 D-46 검사도 이것으로 대체됐다)

**③ `DRGameInstance.h / .cpp`**

| 함수 | 변경 |
|---|---|
| `CanEquipChip()` ✅ | `AlreadyEquipped` 검사 다음: 장착 목록(`State.SlotChips`)에 `FindSkillConflict` → 있으면 `SkillConflict`. 슬롯 수 검사보다 **앞** |
| 신규 `GetSkillConflictChip(Class, ChipId)` (BlueprintPure) | 부딪히는 칩 Id — UI 문구("○○ 와 같은 스킬")와 교체 대상 강조용 |
| `CanEquipChipAtSlot()` | 기반 결과가 `SkillConflict` 여도, **목적 칸의 점유 칩 == 충돌 칩**이면 통과시켜 기존 "점유 칸 교체" 판정으로 넘긴다. 다른 칸이면 `SkillConflict` 그대로 |
| `SwapOrEquipChip()` · `EquipChip()` | 변경 없음 — 위 두 판정을 그대로 탄다 |
| `MoveChip()` | 변경 없음(집합 불변) |
| `SanitizeClassState()` | **3차 패스**(2차 패스 뒤): 칸 순서대로 훑으며 이미 본 스킬 키와 겹치는 칩의 칸을 전부 비운다 → `bModified` (D-50). 재화 변화 없음 |
| `MakeChipViewModel()` | `EquipBlockReason = SkillConflict` 가 자동으로 들어간다 + 신규 필드 `ConflictingChipId` / `ConflictingChipName` 채우기 |
| `BuildPreviewRuntime()` | 추가 칩이 충돌하면 **충돌 칩을 빼고** 미리보기 → "교체하면 이렇게 바뀐다"가 정확히 나온다 |

**④ `Public/Game/DRProgressionTypes.h`** — `FDRChipViewModel` 에 `FName ConflictingChipId` · `FText ConflictingChipName`(BlueprintReadOnly).

**⑤ `DRUpgradeUILibrary.cpp`** — `GetUpgradeResultText` 에 `case SkillConflict: "같은 스킬의 칩은 하나만 장착할 수 있습니다."`

**⑥ ~~`DRProgressionConfig.cpp` 기본 장착 칩 검사~~** — 생략(위 진행 메모 참고).

### 12.4 BP (에디터)

| 에셋 | 할 일 | 안 하면 |
|---|---|---|
| `WBP_UpgradeSlot` | ~~Switch 핀 연결~~ → **할 일 없음**(2026-09-27 에셋 확인). 이 위젯의 `EDRUpgradeResult` Switch(이벤트 그래프 X −304, Y 1888)는 **슬롯 해금 버튼**(`CanUnlockSlot`) 결과를 분기하는 노드라 `SkillConflict` 가 나올 수 없다. `CanEquipChipAtSlot`(OnDragEnter, X −544, Y 2464)는 Switch 없이 쓰이므로 `SkillConflict` 는 "성공이 아님"으로 자동 처리된다 | — |
| `WBP_UpgradeChip` | `EquipBlockReason == SkillConflict` 이면 사유 문구("'<ConflictingChipName>' 과 같은 스킬 — 그 칸에 끌어다 놓으면 교체") | 칩이 흐리게만 보이고 이유를 모른다 |
| 공통 | 드래그는 C++ `IsDraggable()`(해금 && 미장착)이라 **충돌 칩도 끌 수 있다** — 교체 흐름에 추가 작업 없음 | — |

### 12.5 함정

| # | 함정 | 대응 |
|---|---|---|
| **T-46** | 판정을 `CanEquipChip` 에만 넣고 서버/세이브 정화를 빠뜨림 | 클라 UI 만 막히고, 옛 세이브·조작된 보고는 2개가 그대로 적용된다 — `SanitizeLoadout` · `SanitizeClassState` 둘 다(D-49) |
| **T-47** | `CanEquipChipAtSlot` 에서 `SkillConflict` 를 그대로 반환 | 충돌 칩 칸에 드롭해도 교체가 안 된다 — 목적 칸 점유 칩이 충돌 칩이면 통과시킬 것 |
| **T-48** | `SkillConflict` 를 enum 중간에 삽입 | BP Switch 핀·바이트 값이 밀린다(D-52) |
| **T-49** | 칩 레벨 `TargetAbilityTag` 를 비운 스킬 칩 | 제한을 조용히 빠져나간다 — D-51 검증으로 잡는다 |
| **T-50** | 미리보기가 충돌 칩을 안 빼고 계산 | 두 칩 효과가 합쳐진, 실제로는 불가능한 수치를 보여 준다 |

### 12.6 검증

- [ ] 같은 그룹 칩 A 장착 상태에서 B 클릭/빈 칸 드롭 → 거부 + 사유 문구 / A 칸에 B 드롭 → **교체**(A 해제, B 장착)
- [ ] 다른 스킬 칩 · 공용 칩은 자유롭게 함께 장착
- [ ] 미리보기: A 장착 중 B 를 올리면 "A 제거 + B 추가" 기준 수치
- [ ] 옛 세이브(발톱 ①+③ 동시 장착 등)로 실행 → 앞 칸 칩만 남고 경고 로그 1회, 재화 불변
- [ ] 멀티: 클라가 세이브를 조작해 2개 보고 → 서버 정화 로그 + 효과는 1개만
- [ ] 카탈로그 검사: 동전·정원로봇 12종 오류 0건(D-51 추가 후)

### 12.7 작업 순서

1. C++ ①~⑥ → 에디터 닫고 빌드
2. 에디터: `WBP_UpgradeChip` 사유 문구(선택) — `WBP_UpgradeSlot` 은 할 일 없음
3. §12.6 검증 → §1.5 · T-28 · §9 정리하고 이 절을 흡수
