# Plan5 — 로봇 전용 칩 18종 (정원로봇 9 · 자판기 동전 3 · 잭팟 3 · 공속 버프 3) + 스킬당 1칩 규칙

> **상태**
> - 정원로봇 9종(씨앗 폭탄 · 물대포 · 발톱): **구현 완료**. 씨앗 폭탄·물대포는 게임 검증 완료, **발톱은 게임 검증이 남았다 → §10**
> - 자판기 기본 공격(동전) 3종 · 잭팟 3종: **구현 완료**(C++ 빌드 통과 · 에셋 입력 확인 2026-09-28), **게임 검증 남음 → §10**
> - 자판기 공속 버프 3종(§1.6): **구현 완료**(C++ 빌드 통과 · 에셋 입력 확인 2026-10-07), **게임 검증 남음 → §10.3**
> - 장착 규칙 "같은 스킬 칩은 1개만"(§1.7): **구현 완료**, 게임 검증 남음 → §10
> - 로봇 청소기 칩 12종(탑승 §12 · 공기탄 §13 · 물 점프 §14 · 돌진 §15): **C++ 구현 완료**(빌드 통과 2026-10-08) — **남은 것 = 에디터 작업(§16 안내서) → 게임 검증**. 칩 없이도 동작이 바뀌는 선행 변경 5건 포함(공유 피해에서 공격 배율 제거 · 오염 최대 체력 100 · 공기탄 단계 피해 10/30/50/70 · 물 점프 물 30 실제 소모 · 돌진 지형 충돌 자해 50)
>
> 최종 갱신: 2026-10-08 / 브랜치 `feat/upgradeskill`
> 선행 문서: **Plan2.md**(재화·슬롯·칩 시스템) → **Plan3.md**(공용 칩 8종)
>
> 📌 **이 문서는 "계획서"가 아니라 ★완료된 기능의 참조 문서★ 다.**
> 계획 단계의 코드 해부·단계별 에디터 매뉴얼·테스트 체크리스트는 역할을 다해 걷어냈고,
> **다시 만들거나 고칠 때 필요한 것** — 최종 수치, 설계 결정과 그 근거, 밟으면 안 되는 함정,
> 다음 칩을 추가하는 레시피 — 만 남겼다.
>
> ⚠️ **코드 주석 표기 규칙** — 씨앗 폭탄 **`Plan5(씨앗칩)`** / 물대포 **`Plan5(물대포칩)`** / 기본 공격 **`Plan5(발톱칩)`**
> / 자판기 기본 공격 **`Plan5(동전칩)`** / 잭팟 **`Plan5(잭팟칩)`** / 공속 버프 **`Plan5(공속칩)`**
> / 장착 규칙 **`Plan5(스킬당1칩)`** / 청소기 탑승 **`Plan5(탑승칩)`** / 청소기 기본 공격 **`Plan5(공기탄칩)`** / 청소기 물 점프 **`Plan5(물점프칩)`** / 청소기 돌진 **`Plan5(돌진칩)`**.
> 코드 주석에 있던 이 문서의 절 번호(`§4.2` 등)는 **전부 제거했다.**
> 절 번호는 다시 붙이지 않는다 — 문서가 바뀔 때마다 조용히 틀린 참조가 되기 때문이다.
> 대신 **결정 번호(D-n)** 와 **함정 번호(T-n)** 로 가리킨다. 이 둘은 문서 구조가 바뀌어도 유지되며,
> 빠진 번호는 결번이다(재사용하지 않는다).
>
> ⚠️ 코드에 남아 있는 `Plan5 §5.2` 같은 **접두 없는** 참조는 **삭제된 "청소기/탑승 계획서"** 를
> 가리키는 별개의 것이다(`DRVacuumDash.cpp` 등). 이 문서와 무관하므로 건드리지 않았다.

---

## 1. 칩 18종 최종 스펙

공통: 로봇 전용(`bCommonChip = false`), `Category = Ascension`, `RequiredSlotCount = 1`. ★칩 레벨 `TargetAbilityTag` 가 같은 칩은 동시에 1개만 장착된다(§1.7)★.
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
공격 속도 버프(스킬 2)는 발사 간격 ÷ 공속 배율 M(§1.6 — 칩 없으면 1 + 0.2 × 스택, 최대 10스택).

- ① 동전·캡슐 피해 ×1.2, 발사 간격 0.6 → **1.0초**(버프는 그 위에 곱해진다 — D-38).
  ★`Flat +0.4` 는 "1.0초 고정"이 아니라 "기준 +0.4초"다★(T-38). 단독 DPS 는 **×0.72** 다(§9).
- ② 동전·캡슐이 **적에게** 맞을 때마다 시전자 체력 **+2**(고정량 — 피해·취약·엘리트 무관). 벽·바닥·클렌저 사이트·
  아군·죽은 적은 제외(D-41). **오염 중이거나 회복 차단(⑥)·만피면 없다**(D-42 / D-43).
- ③ 동전·캡슐의 외형과 판정 구가 함께 **×1.3**(반경 31.6uu). 벽·바닥에도 먼저 닿는다(T-43). 임팩트 이펙트는 그대로.
- **잭팟 캡슐도 기본 공격이다** — 세 칩 모두 캡슐에 적용되고, 캡슐 피해도 `SkillDamage` 를 탄다(D-39).

### 1.5 자판기 잭팟 3종 — `TargetAbilityTag = Abilities.VendingMachine.Jackpot`(그룹 키 전용), `Modifiers` 없음

| # | `ChipId` | 이름 | GrantedTags | 효과 | 단점 | Tier | 아이콘 |
|---|---|---|---|---|---|---|---|
| ① | `VM.Asc.JackpotGold` | 황금 레버 | `…Jackpot.GoldRush` | 확률 브론즈 **75** / 실버 **0** / 골드 **25** | 실버가 나오지 않는다 | 3 | 공격-4 |
| ② | `VM.Asc.JackpotDouble` | 쌍둥이 캡슐 | `…Jackpot.Double` | 나온 캡슐이 **같은 색으로 0.15초 뒤 1발 더** | 골드 0% — 기본 골드의 ½ 씩 브론즈·실버로 → **55 / 45 / 0** | 4 | 지원-4 |
| ③ | `VM.Asc.JackpotPity` | 적립 카드 | `…Jackpot.Pity` | 브론즈·실버가 나오면 다음 잭팟에서 **그 색 −2%p · 골드 +2%p**(누적), 골드가 나오면 초기화 | 기본 **45 / 50 / 5**, 골드 피해 **150 → 120** (③ 전용) | 3 | 편의-4 |

기준(칩 없음, GA BP 값): 동전 **10발** 뒤 캡슐, 확률 **50 / 40 / 10**, 피해 **10 / 50 / 150**. 잭팟은 기본 0.6초 × 11발 = 6.6초마다.

| | 잭팟 1회 평균 피해 | 골드 주기 |
|---|---|---|
| (없음) | 40 | 평균 10회 |
| ① | 45 (+12.5%) | 평균 4회 — 분산이 크다 |
| ② | 56 (28 × 2) (+40%) | 없음 |
| ③ | 약 43 (+8%, 손 계산 근사) | 평균 약 7.3회, 20회 안 99.7% |

- 확률은 매 잭팟 "기본(BP) → 칩 규칙 → 정규화"로 계산한다(D-55). ②는 고정표가 아니라 규칙이다.
- ③의 %는 **%p** 다: 브론즈 → 43/50/7, 이어 실버 → 43/48/9 … 한 색이 0 에 닿으면 멈추고 골드는 실제로 뺀 만큼만 오른다(D-57).
- 캡슐 피해 = 등급 피해(③이면 골드 120) → `SkillDamage`(동전칩) → 런타임 배율. ②의 두 번째 캡슐도 같다(D-60).
- 잭팟 칩은 동전칩과 **다른 그룹**이라 동전칩 1개 + 잭팟칩 1개를 함께 낄 수 있다(D-53).

### 1.6 자판기 공속 버프(스킬 2) 3종 — `TargetAbilityTag = Abilities.VendingMachine.AttackSpeedBuff`

| # | `ChipId` | 이름 | Modifiers | GrantedTags | 효과 | Tier | 아이콘 |
|---|---|---|---|---|---|---|---|
| ① | `VM.Asc.SpeedOverdrive` | 과부하 모터 | `SkillWaterCost` **+1.6** ★단점★ | `…AttackSpeedBuff.Overdrive` | 1회 **+30%**. 대신 물 50 → **130**, 지속 20 → **5초** | 3 | 공격-5 |
| ② | `VM.Asc.SpeedShare` | 공유 회로 | (없음) | `…AttackSpeedBuff.Share` | 쓰는 순간 **본인 + 20m 안 살아 있는 아군**에게 버프. 대신 1회 **+10%**, 최대 **5중첩**(20초) | 4 | 지원-5 |
| ③ | `VM.Asc.SpeedDecay` | 잔류 가속 | (없음) | `…AttackSpeedBuff.Decay` | 20초가 끝나면 중첩이 **1초에 1개씩** 빠진다. 줄어드는 중에 다시 쓰면 감소가 멈추고 다시 20초. 대신 1회 **+16%** | 3 | 편의-5 |

기준(칩 없음): `GA_VendingMachine_AttackSpeedBuff`(E) — 물 **50**, 쿨다운 없음, 1회 **+20%**, 최대 **10중첩**, **20초**(쓸 때마다 리셋).
②·③은 단점이 규칙이라 모디파이어 없이 `Description` 으로 알린다(검사가 강제).

**공속 배율 M** = 1 + 자기 버프 스택 × (① 0.3 / ③ 0.16 / 그 외 0.2) + 공유 버프 스택 × 0.1 (D-62 · D-64).
세 로봇 모두 쿨다운이 없어서 "공속"은 로봇마다 다른 손잡이다.

| 대상 | 바뀌는 것 | ② 5중첩(M = 1.5) |
|---|---|---|
| 자판기 | 동전 발사 간격 ÷ M (잭팟 주기도 같이) | 0.6 → **0.4초** |
| 정원로봇 발톱 | 몽타주 PlayRate × M — 선딜·후딜 모두 | 스윙 1/1.5 |
| 정원로봇 씨앗 대포 | 몽타주 PlayRate × M (1P·3P) | 발사 동작 1/1.5 |
| 청소기 에어샷 | 차지 간격 ÷ M(충전 시작 때 고정) / 강화탄 몽타주 × M | 풀차지 1.5 → **1.0초** |

- ③ 타임라인(10중첩, 마지막 사용 = 0초): 20초에 9 → 21초에 8 → … → 29초에 0. 배율은 ×2.6 → 2.44 → … → 1.16.
- ② 아군 판정은 **쓰는 순간 1회**(오라가 아니다). 공유 버프를 받은 자판기는 자기 버프와 **더한다**. 여러 자판기의 공유 버프는 한 더미(최대 5).
- HUD: 자판기 스킬 아이콘 숫자 = 자기 + 공유 스택(③의 1초 감소도 보인다).
  남은 시간 그래프 = 실제 GE 남은 시간(기본·② 20초, ① 5초 / ③은 유지 20초 동안 줄고 감소 구간에는 비어 있다 — D-76).

### 1.7 장착 규칙 — 같은 스킬 칩은 동시에 1개만

- 묶음 키 = **칩 레벨 `TargetAbilityTag`**(정확히 일치 — D-47). 공용 칩 · 태그 없는 칩은 제한 없음.
- 그룹 6개 × 3칩: 씨앗 폭탄 / 물대포 / 발톱 / 동전(`…VendingMachine.BasicAttack`) / 잭팟(`…VendingMachine.Jackpot`) / 공속 버프(`…VendingMachine.AttackSpeedBuff`).
- 같은 그룹 칩이 있으면 빈 칸 장착은 **거부**(`EDRUpgradeResult::SkillConflict`), **그 칩이 있는 칸에 드롭하면 교체**(D-48). 칸 이동은 검사하지 않는다.
- 겹친 칩도 목록에서 **드래그는 된다**(드롭 교체를 위해 — 사용자 확인 2026-09-28).
- 서버 정화 · 세이브 정화(앞 칸 칩만 남김 — D-50) · 칩 카드 사유(`ConflictingChipName`) · 미리보기(충돌 칩을 뺀 수치)가 모두 `UDRChipCatalog::FindSkillConflict()` 하나를 쓴다(D-49).

### 1.8 칩별 수치 (같은 스킬 조합은 §1.7 규칙으로 불가능 — 단일 칩 기준)

**씨앗 폭탄** — 기준: 물 `W`, 피해 70/30, 회복 100/50, 범위 200/400

| 조합 | 물 | 피해(내/외) | 회복(내/외) | 범위(내/외) |
|---|---|---|---|---|
| ① | `W×2` | 70 / 30 | 100 / 50 | 200 / 400 |
| ② | `W` | **14 / 6** | **200 / 100** | 200 / 400 |
| ③ | `W×0.7` | **42 / 18** | 100 / 50 | **300 / 600** |

**물대포** — 기준: 물 `W`, 피해 `D`, 아군 급수 `G`, 스택 `n`(0~10)

| 조합 | 물 | 피해 | 아군 급수 | 관통 |
|---|---|---|---|---|
| ① | `W` | `D×(1+0.05n)` 최대 `D×1.5` | **0** | ✗ |
| ② | **`W×0.2`** | **0** | **`2G`** | ✗ |
| ③ | **`W×1.5`** | `D` (관통 전원) | `G` (1명) | ✓ |

**기본 공격** — 기준: 피해 `D`, 선딜 `T`, 판정 반경 `R`

| 조합 | 피해 | 선딜 | 반경 | 흡혈(적 1명당) |
|---|---|---|---|---|
| ① | **`5D`** | **`3T`** | `R` | — |
| ② | **`0.5D`** | `T` | `R` | `0.5D` |
| ③ | **`0.8D`** | `T` | **`1.3R`** | — |

**동전** — 기준: 한 발 20, 간격 0.6초, 버프 0스택 (캡슐 10/50/150 도 같은 피해 배율)

| 조합 | 한 발 | 간격 | 크기 | 적중 회복 | 동전 DPS |
|---|---|---|---|---|---|
| ① | **24** | **1.0** | ×1 | — | 24.0 (**×0.72**) |
| ② | **14** | 0.6 | ×1 | 2/발 (3.3/초) | 23.3 (×0.70) |
| ③ | **14** | 0.6 | **×1.3** | — | 23.3 (×0.70) |

잭팟·② 회복은 "발사 수" 기준이라 ①을 끼면 초당 캡슐·초당 회복도 40% 준다. ① + 버프 10스택 = 0.333초.
동전칩 + 잭팟칩(다른 그룹): 고액 동전이면 캡슐 피해 ×1.2 · 잭팟 11초마다 / 거스름 동전이면 캡슐 적중도 +2(쌍둥이 캡슐과 겹치면 잭팟 1회 최대 +4) / 대형 동전이면 두 캡슐 모두 ×1.3.

**공속 버프** — 자판기 본인, 동전 간격 0.6초 기준

| 조합 | 1회 | 최대 배율 | 최대 발사 간격 | 최대 스택 유지 비용 |
|---|---|---|---|---|
| (없음) | +20% | ×3.0 (10) | 0.2초 | 물 50 / 20초 |
| ① | +30% | ×4.0 (10) | **0.15초** | 물 130 / **5초**(초당 약 26) |
| ② | +10% | ×1.5 (5) | 0.4초 | 물 50 / 20초 — 대신 아군 전원 |
| ③ | +16% | ×2.6 (10) | 약 0.23초 | 물 50 / 20초 — 끊겨도 9초에 걸쳐 줄어든다 |

동전 ① 고액(1.0초) + 공속 ① 10중첩 = 0.25초 / + 공속 ③ 10중첩 ≈ 0.38초.

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
| 오염 | 동전 ② 적중 회복 · 발톱 ② 흡혈 둘 다 차단 — 시전자가 오염 중이면 회복 0(D-42, 발톱은 2026-10-08 §9-15) |
| ⑥ 광폭화 · ⑦ 시너지 → 잭팟 캡슐 | 캡슐 피해에도 추가로 곱해진다 |
| ⑧ 피의 계약 + 공속 ① | 물 130 대신 ★체력 130★ 을 낸다(비율 1.0) |
| 발톱 ① 중량 삽날 + 공유 버프 | 선딜 배율과 곱해진다 — `PlayRate = Base × M ÷ WindupScale` |

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
      │                              ÷ GetAttackSpeedMultiplier()       │ (C++ 안에서 끝 — BP 배선 없음)
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

잭팟 (UDRVendingMachineBasicAttack::ExecuteShot, 서버)
   GetJackpotOdds() ─► RollJackpotTier() ─► SpawnJackpotCapsule() ─► AdvanceJackpotPity() ─► (②) 0.15초 뒤 FireJackpotDoubleShot()
   기본표 = BP BronzeCapsuleChance/SilverCapsuleChance, 칩 규칙 = ChipTuning.Jackpot, 매번 "[Jackpot]" 로그 1줄

공속 버프 (UDRVendingMachineAttackSpeedBuff::ApplyAttackSpeedBuff, 서버 — BP 는 이 노드 하나만 부른다)
   ② → GetAlivePlayersWithinRadius(20m) → 본인 + 아군에게 SharedEffect
   ③ → DecayEffect(지속 1초) → ModifyActiveEffectStartTime(+19초)      그 외 → GE_AttackSpeedBuff(① 5초 / 20초)
   지속은 ApplyBuffEffect 가 Spec.SetDuration(값, 고정)으로 넣는다 — GE 에셋의 Duration 값은 무시된다
공속 배율 M = UDRAbilitySystemLibrary::GetAttackSpeedMultiplier(ASC)     (서버·클라 어디서나)
   └─► 자판기 발사 간격 · 발톱 GetWindupPlayRate/RestoreMontagePlayRate · 씨앗 GetAttackSpeedPlayRate · 청소기 StartCharging/강화탄
HUD: 태그 AnyCountChange → OnVendingMachineAttackSpeedBuff(자기 + 공유 스택) / 위젯 타이머 → GetAttackSpeedBuffFillRatio()

[DA_ProgressionConfig.ChipTuning]  숫자로 표현 못 하는 동적 상수
   └ .SeedBomb / .WaterPump / .Claw / .Coin / .Jackpot / .AttackSpeedBuff  →  UDRAbilitySystemLibrary::GetChipTuning(WorldContext)
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
| Plan5 에서 추가 | `…SeedBomb.{Refund,Slow,Haste}`<br>`…WaterPump.{Ramp,Slow,Vulnerable,Pierce}`<br>`…Claw.{Lifesteal,AllyHeal}`<br>`…Coin.HitHeal`<br>`…Jackpot.{GoldRush,Double,Pity}`<br>`…AttackSpeedBuff.{Overdrive,Share,Decay}` | `Buff.WaterPump.Pressure`<br>`Debuff.Vulnerable`<br>`Buff.AttackSpeed.Shared`<br>(에셋 전용) `Debuff.SeedBomb.Slow` `Buff.SeedBomb.Haste` `Debuff.WaterPump.Slow` | `Data.MoveSpeed.Mult`<br>`Data.Duration`<br>`Data.HealthCost` |

- 피처 태그를 **ASC 에 Loose 로 얹지 말 것** — 어빌리티의 `ActivationBlockedTags` 와 의도치 않게 상호작용한다.
- C++ 이 읽는 GAS 태그만 **네이티브**로 등록한다. VFX/에셋에서만 쓰는 태그는 에디터에서 만들면 된다.
- 발톱칩·동전칩·잭팟칩은 새 GAS 태그가 없다 — 지속 상태 없이 즉발 효과뿐이다.
  공속칩은 `Buff.AttackSpeed.Shared` 하나(② 공유 버프 — 자기 버프와 스택을 섞지 않으려고, D-63).
- `Abilities.VendingMachine.Jackpot` 은 ★칩 그룹 키 전용★ 이다 — 어떤 GA 에도 붙이지 않는다(T-51).

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
| **N** | 스킬당 1칩 판정 | `UDRChipCatalog::FindSkillConflict()` / `FDRUpgradeChipDefinition::GetExclusiveSkillTag()` / `EDRUpgradeResult::SkillConflict` |
| **O** | 잭팟 확률표 | `FDRJackpotOdds`(+`Normalized`) / `FDRJackpotChipTuning` / `UDRVendingMachineBasicAttack::GetJackpotOdds()` |
| **P** | 공속 배율 · 스택 읽기 | `GetAttackSpeedMultiplier()` / `GetEffectStackCountWithTag()`(라이브러리) + `UDRGameplayAbility::GetAttackSpeedMultiplier()`(BP Pure) |
| **Q** | 살아 있는 아군 탐색 | `GetAlivePlayersWithinRadius()` — 로그 없음, 적·사이트 제외 |
| **R** | 시한 버프의 지속을 코드가 고정 | `UDRVendingMachineAttackSpeedBuff::ApplyBuffEffect()` — `Spec.SetDuration(값, true)` + SetByCaller 둘 다 |

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
| **D-38** | 칩과 공격 속도 버프는 **곱**: `간격 = ApplySkill(SkillFireInterval, Base) ÷ 공속 배율 M`(D-62) | 버프 공식은 그대로 두고 "기준 간격"만 바꾼다. 순서 무관 |
| **D-39** | **잭팟 캡슐도 기본 공격** — 세 칩 모두 적용. 캡슐 피해 = `GetUpgradedFloat(SkillDamage, 캡슐 피해) × GetRuntimeDamageMultiplier()` | 사용자 확정. 상수 덮어쓰기는 T-1 과 같은 누락(②③의 −30% 가 캡슐에서 샜다). `UDRVacuumAirShot` 선례와 같은 보정 |
| **D-40** | 투사체 크기 = **새 축 `SkillProjectileScale`(기준 1.0 배율)** → 스폰 트랜스폼 Scale3D, 공용 `SpawnProjectile` 에서 적용 | 액터 스케일은 외형과 판정 구를 함께 키운다. `SkillAreaRadius`(효과 반경)와 뜻이 달라 재사용 기각. 적 GA 는 칩 캐시가 비어 항상 1.0. 스폰 스케일은 `SerializeNewActor` 가 클라로 보낸다(엔진 확인, Iris 미사용) |
| **D-41** | 적중 회복은 **맞는 쪽** `HandleIncomingDamage` 에서 정산, 판정은 `GetHitHealAmountForDamage` 하나(피처 AND 컨텍스트에 `VendingMachine.BasicAttack`) | "적에게 피해가 들어갔다"가 확정되는 유일한 지점 — 벽·사이트·아군·죽은 적·두더지는 구조적으로 안 온다. `ADRProjectile` 은 적 투사체와 공유라 칩 지식을 넣지 않는다 |
| **D-42** | **오염 중엔 적중 회복 없음** — 판정 함수가 ★시전자★ 의 `IsCorrupted()` 를 본다 | 사용자 확정. `HandleCorruptionPurification` 은 회복량을 보지 않아(정화 + 체력 = 컨테이너 1칸) 막지 않으면 "적 1회 적중 = 정화" |
| **D-43** | 회복량은 **고정 2**(`ChipTuning.Coin.HitHealAmount`). 걸기 전에 `GetHealableAmount` 로 잘라 만피·회복 차단이면 GE 를 걸지 않는다 | 원문이 "체력 2". `SkillHealing` 은 "아군에게 주는 회복" 축이라 뜻이 다르다. 적중이 잦다(초당 1.7~5회) |
| **D-44** | 발사 간격 하한 `MinFireInterval = 0.05초` | `SetTimer` 는 Rate ≤ 0 이면 타이머를 지운다(T-35) |
| **D-45** | 동전칩은 **BP 재배선 · 새 GE 없음** | 효과가 전부 C++ 경로(`StartAutoFire` · `ExecuteShot` · `SpawnProjectile`)와 피해 관문 안에서 끝난다 |
| **D-46** | (미적용 · 권장) `ValidateCatalog` 에 "칩 레벨 대상 스킬이 있는 전용 칩의 스킬 단위 모디파이어는 `TargetAbilityTag` 필수" | `GR.Asc.SeedSupport` 누락 같은 실수를 검사로 막는다(§9) |

| **D-47** | 스킬당 1칩 묶음 키 = **칩 레벨 `TargetAbilityTag`**(정확히 일치). 모디파이어 태그로 추론하지 않는다 | 카드의 "적용 대상 스킬" 표기와 같은 값. `GrantedTags` 만 있는 칩에서 추론이 틀린다. 태그 누락은 D-51 로 막는다 |
| **D-48** | 충돌 시 **거부**(`SkillConflict`) + **충돌 칩 칸에 드롭하면 교체**. 자동 교체 없음 | 사용자 확정. 드롭 교체는 기존 규칙이라 새 UI 불필요 |
| **D-49** | 판정은 `FindSkillConflict()` **하나** — 장착 판정 · 서버 정화 · 세이브 정화 · 뷰모델 · 미리보기 | 한 곳이라도 따로 판정하면 "다음 실행/멀티에서 사라지는" 늦은 증상 |
| **D-50** | 옛 세이브에 같은 그룹 칩이 2개 이상이면 **앞 칸 칩만** 남긴다(`SanitizeClassState` 3차 패스) | 칩은 사는 게 아니라 환급 불필요 |
| **D-51** | `ValidateCatalog`: 전용 칩에 스킬 단위 모디파이어나 `Upgrade.Feature.Skill.*` 가 있으면 칩 태그 필수 + 모디파이어 태그 = 칩 태그 | 칩 태그가 비면 규칙을 빠져나간다. SeedSupport 같은 모디파이어 태그 누락도 잡는다(D-46 대체) |
| **D-52** | `SkillConflict` 는 `EDRUpgradeResult` **맨 끝** | BP Switch/바이트 값이 밀리지 않게 |
| **D-53** | 잭팟 그룹 키 = 새 태그 `Abilities.VendingMachine.Jackpot` — 칩 레벨에만, GA 에는 안 붙인다 | 사용자 확정(동전칩과 별도 그룹). GA 에 붙이면 업그레이드 키가 흔들린다 |
| **D-54** | 잭팟 칩은 `Modifiers` 없이 기능 스위치 + `ChipTuning.Jackpot` 만 | 확률·등급 피해는 스탯 축으로 표현 불가 |
| **D-55** | 확률 = 기본(BP) → 칩 규칙 → 정규화, 매 잭팟 계산. ①표 교체 / ②골드 ½씩 재분배(규칙) / ③자기 기본표 + 누적 보정 | ②를 고정표로 박으면 BP 기본값 변경 시 어긋난다 |
| **D-56** | ② 추가 발사: 같은 등급(재굴림 없음), 0.15초 뒤, 조준 재계산, 버튼을 떼도 나감, 스택·③ 보정 불변. 소리 2번, 몽타주 이벤트는 첫 발만 | 요구 원문 + 몽타주 끊김 방지 |
| **D-57** | ③ 보정 = %p 절대값, 색마다 누적, 0 바닥, 골드는 실제로 뺀 만큼만, 골드면 초기화 | 합이 항상 1 |
| **D-58** | ③ 상태는 GA 인스턴스(서버, 비복제) — 스테이지 동안 유지(부활 포함), 맵 이동 시 초기화 | 굴림이 서버 전용 |
| **D-59** | ③ 단점(45/50/5 · 골드 120)은 `ChipTuning.Jackpot` — BP 기본값은 그대로 | 사용자 확정: ③ 전용 |
| **D-60** | 캡슐 피해 경로 유지: 등급 피해 → `SkillDamage` → 런타임 배율, 크기 `GetUpgradedProjectileScale()` | 동전칩과 함께 낄 때 캡슐이 칩을 빠져나가지 않게 |
| **D-61** | `DA_ProgressionConfig` 검사에 잭팟 확률표(음수 · 합 ≈ 1) · `PityStep` · `DoubleShotDelay` 검사 | 정규화가 조용히 숫자를 바꾸는 것을 알린다 |


| **D-62** | 공속 배율 M 은 `UDRAbilitySystemLibrary::GetAttackSpeedMultiplier(ASC)` **하나**에서 계산(어빌리티는 `UDRGameplayAbility::GetAttackSpeedMultiplier()`). 예전 리터럴 0.2 는 `BaseSpeedPerStack` 으로 이관 | 자판기·발톱·씨앗·청소기가 같은 값을 써야 한다 |
| **D-63** | ② 공유 버프 = **별도 GE** `GE_AttackSpeedBuff_Share` + GAS 태그 `Buff.AttackSpeed.Shared`(로봇 공용 이름), Stack Limit 5 | 원본 GE 로 걸면 자기 버프와 스택이 섞여 스택당 값(20% / 10%)을 구분할 수 없다(T-61) |
| **D-64 / D-74** | 스택당 값은 **읽는 쪽**이 자기 칩으로 정한다 — 자기 버프 ① 0.3 / ③ 0.16 / 그 외 0.2, 공유 버프 0.1, 합성은 덧셈 | 자기 버프는 자기만 건다. 공유 버프를 거는 길은 ②뿐이라 값이 하나다 |
| **D-65** | 버프 적용은 C++ `ApplyAttackSpeedBuff()`(서버) — BP 는 이 노드 하나. 지속은 스펙에 `SetDuration(값, 고정)` 으로 넣어 **GE 에셋의 Duration 을 고치지 않는다** | GE 고정 20초로는 ①의 5초를 못 낸다. 칩 지식이 BP 로 새지 않게. "GE 를 SetByCaller 로 바꾸고 BP 를 안 바꾸면 0초" 위험도 사라진다(엔진 확인: 고정된 지속은 적용 시 재계산이 덮지 않는다) |
| **D-66** | ② 아군 = `GetAlivePlayersWithinRadius()`(Synergy 오라와 같은 필터), **쓰는 순간 1회** | 요구가 "사용 시"다. `GetLiveObjectsWithinRadius` 는 적·사이트·진단 로그가 섞인다(T-65) |
| **D-67** | 발톱: `GetWindupPlayRate()` · `RestoreMontagePlayRate()` **둘 다** × M | 선·후딜 모두 짧게(사용자 확정). BP 무변경. 적 GA 는 이 함수를 부르지 않아 불변(D-35) |
| **D-68** | 씨앗 대포: `UDRSeedCannon::GetAttackSpeedPlayRate()`(= `BaseMontagePlayRate` × M) → BP 1P·3P PlayRate 핀 | 몽타주 하나에 선·후딜이 다 들어 있다 |
| **D-69** | 청소기: 차지 간격 ÷ M 을 `StartCharging` 에서 **한 번** 잡는다 + 강화탄 몽타주 × M. C++ 만 | 충전 중 버프가 끝나도 단계가 뒤로 튀지 않게(T-64). 발사 뒤 잠금이 없어 실효는 차지 단축 |
| **D-70** | 아이콘 숫자 · `NotifySkillActivated` 음높이 = **자기 + 공유 스택 합** | ②를 낀 자판기는 자기 GE 대신 공유 GE 를 받는다 — 안 그러면 숫자가 사라진다 |
| **D-71** | 튜닝 = `ChipTuning.AttackSpeedBuff`(§5.7). ①의 물 +160% 만 카탈로그 모디파이어 | D-2. 물은 모디파이어여야 단점이 카드·미리보기에 빨갛게 뜬다 |
| **D-72** | ③ = **감쇠 전용 GE** `GE_AttackSpeedBuff_Decay`(원본 복제, 태그 **그대로**, `Stack Expiration Policy = Remove Single Stack and Refresh Duration`) | 만료 정책은 에셋 단위다(T-68). 태그가 같아 다른 코드는 고칠 것이 없고, ①~③은 동시 장착 불가라 두 GE 가 같이 걸릴 일도 없다 |
| **D-73** | "20초 유지 → 1초에 1개" = 지속 `DecayInterval`(1초)로 걸고 **매 적용 직후** `ModifyActiveEffectStartTime(+BuffDuration − Interval)`. 이후는 내장 정책이 1초마다 1개를 뺀다. 다시 쓰면 Refresh 로 리셋 → 다시 밀기 → 감소 정지 | 엔진 실측: 내장 정책은 Spec 지속 전체로 다시 잰다(20초 GE 면 20초마다 1개). 서버 타이머를 들고 있지 않아 사망·리스폰 정리가 필요 없고, 시작 시각은 GAS 가 복제한다 |
| **D-75** | 아이콘 숫자는 태그 **`AnyCountChange`** 이벤트로(`HandleAttackSpeedBuffTagChanged`). 스택 읽기는 `GetEffectStackCountWithTag()` 하나. 예전의 정확 일치 방송(적용 · 제거) 2곳은 지웠다 | 스택만 줄 때는 적용·제거 델리게이트가 안 온다(T-72). 태그 이벤트는 서버·클라 모두 온다 |
| **D-76** | 남은 시간 그래프는 위젯이 **세지 않고** 매 틱 `GetAttackSpeedBuffFillRatio()`(0~1)를 읽는다 — 실제 GE 남은 시간 ÷ 지속(① 5초), ③ = 유지 구간 ÷ `BuffDuration` · 감소 구간 0(사용자 확정 "A 방식"), 자기·공유 중 큰 쪽 | 위젯의 20초 하드코딩으로는 ①을 못 그린다. 시간을 이벤트로 밀어 주면 원격 클라 10중첩 재사용(스택 불변)을 놓친다 — 읽어 오면 복제된 시작 시각을 보므로 빠짐이 없다 |

> 기각(공속칩): 증가량·지속·중첩을 새 스탯 축으로 — 중첩 한도는 GE 에셋이 권위라 축으로 표현이 안 된다 / 공속을 쿨다운 축으로 — 세 로봇 모두 쿨다운이 없다.

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
| `Public/DRGameplayTags.h` · `.cpp` | 칩 피처 태그 전부(§2.3) + GAS 태그 3개(`Buff.WaterPump.Pressure` · `Debuff.Vulnerable` · `Buff.AttackSpeed.Shared`) + SetByCaller `Data.MoveSpeed.Mult` / `Data.Duration` / `Data.HealthCost` |
| `AbilitySystem/DRAbilitySystemLibrary` | `GetUpgradeRuntimeForASC` / `HasUpgradeFeature` / `GetChipTuning` / `GetUpgradeKeyTagFromAbilityTags` / `ApplySkillUpgrade` / `ApplyTimedSpeedEffect` / `ApplyTimedTagEffect` / **`ApplyHealToActor`**(오버로드 2개 — `UClass*` 를 넘기면 C2668 모호 에러, `TSubclassOf` 로 감쌀 것) / **`GetHealableAmount`** / **`GetLifestealRatioForDamage`** / **`GetHitHealAmountForDamage`** / **`GetEffectStackCountWithTag`** / **`GetAttackSpeedMultiplier`** / **`GetAlivePlayersWithinRadius`** |
| `Abilities/DRGameplayAbility` | `GetUpgradedFloat` / `GetEffectiveWaterCostFor` / `ApplyCooldown` / `GetAttackSpeedMultiplier()`(BP Pure) |
| `Abilities/DRDamageGameplayAbility` | `GetUpgradedDamage()` = `ApplySkill(SkillDamage) × ` **`GetRuntimeDamageMultiplier()`**(가상 훅, 기본 1.0) |
| **`Abilities/DRWaterPump`** | 칩 해석 캐시 `Resolved` / 관통 트레이스 `TraceBeamBlocker` / 대상 수집 `CollectTargetsInBeam` + 칩 규칙 필터 `NarrowTargetsToChipRules` / `ApplyChipDebuffs` / `HasEnemyTarget` + `AdvanceRampStack` + `RampStackHandle` / `ClearRampStacks` / `GrantWaterToAlly` / `GetRuntimeDamageMultiplier` 오버라이드 |
| **`Abilities/DRMeleeAttack`** | `AttackRadius` / `BaseMontagePlayRate` / `GetUpgradedAttackRadius` / `GetWindupPlayRate` / `RestoreMontagePlayRate`(1P·3P 메시 모두, 둘 다 × 공속 배율) / `TryHealAllyWithOwnHealth` — 피해량 → D-33 으로 자르기 → D-26 으로 자르기 → **비용 먼저, 회복 나중**("깎였으면 회복도 갔다"가 항상 성립) |
| **`Actor/DRSeedProjectile`** | `FDRSeedBombResolved` 캐시 / 직격 판정 `bDirectHitOnEnemy` / `GrantWaterRefund` / 감속·가속 부여 / 아군 회복은 `ApplyHealToActor` 위임(D-34) |
| `Abilities/DRSeedCannon` | `SpentWaterCost` 주입(환급 근거) / `BaseMontagePlayRate`(1.0) · `GetAttackSpeedPlayRate()`(BP PlayRate 핀용) |
| `ExecCalc/ExecCalc_Damage` | 주는 피해 관문 — `DamageDealt`·광폭화·시너지, **음수 피해 0 클램프** |
| **`DREnemyAttributeSet`** | 받는 피해 관문 — 엘리트 배율 + **`Debuff.Vulnerable` ×1.5** + **흡혈 지급**(★`SetHealth` 전★ — T-24) + **적중 회복 지급**(흡혈 바로 아래, 더미 분기 위 — T-40) |
| `DRChipCatalog.cpp` | `ValidateCatalog()` — 스킬 태그/공용 칩/`MaxHealth` Flat 전용/**`Percent ≥ −1.0`**/D-51 칩 태그 검사 · **`FindSkillConflict()`** · `SanitizeLoadout` 스킬 중복 제거 |
| `DRGameInstance.cpp` | `CanEquipChip`(→ `SkillConflict`) · `GetSkillConflictChip` · `CanEquipChipAtSlot`(충돌 칩 칸이면 교체 허용) · `SanitizeClassState` 3차 패스 · `MakeChipViewModel`(`ConflictingChipId/Name`) · `BuildPreviewRuntime`(충돌 칩 제외) |
| `DRProgressionConfig.h/.cpp` | `FDRCoinChipTuning` · `FDRJackpotOdds`(`Normalized`) · `FDRJackpotChipTuning` · `FDRAttackSpeedBuffChipTuning` / 검사: 잭팟 확률표(D-61) · 공속 ②③ GE(비었는지 · Target Tags · Stack Limit · ③ 만료 정책 — 그 칩이 카탈로그에 있을 때만) |
| `UI/Widget/DRUpgradeChipWidget.h` | `IsDraggable()` = 해금 && 미장착 — `SkillConflict` 칩도 드래그된다(드롭 교체용) |
| `UI/DRUpgradeUILibrary.cpp` | 스탯 짧은 이름(5자 제한) — `SkillWindup` = "선딜" / `SkillFireInterval` = "발사 간격" / `SkillProjectileScale` = "탄 크기" |
| **`Abilities/DRVendingMachineBasicAttack`** | `GetCurrentFireInterval()` = 칩 간격 ÷ 공속 배율, `MinFireInterval` 바닥 / 잭팟: `GetJackpotOdds` · `RollJackpotTier` · `GetJackpotCapsuleBaseDamage` · `SpawnJackpotCapsule`(스케일 + `SkillDamage` + null 체크 + 사운드 — T-36) · `AdvanceJackpotPity` · `FireJackpotDoubleShot` / `[Jackpot]` 로그 |
| **`Abilities/DRProjectileSpell`** | `GetUpgradedProjectileScale()` + `SpawnProjectile()` 스폰 트랜스폼 Scale3D (적 GA 도 지나가지만 항상 1.0) |
| **`Abilities/DRVendingMachineAttackSpeedBuff`** | `ApplyAttackSpeedBuff()`(서버 — ② / ③ / 그 외 분기, ②③ GE 가 비면 경고 로그 + 기본 버프) · `ApplyBuffEffect()`(지속 고정) · `BuffDuration`(20) · `FindBuffDuration(ASC)` · `NotifySkillActivated()`(자기 + 공유 스택) |
| **`Abilities/DRVacuumAirShot`** | `EffectiveChargeInterval`(`StartCharging` 에서 고정, 하한 0.05초) · 강화탄 몽타주 PlayRate = 공속 배율 |
| **`UI/WidgetController/OverlayWidgetController`** | `HandleAttackSpeedBuffTagChanged`(아이콘 숫자 + ③ 감소 구간 판정 `bAttackSpeedBuffDecaying` — ★방송 전에 갱신★) · `GetAttackSpeedBuffFillRatio()`(그래프) |

**BP 쪽 계약** (깨지면 조용히 오동작한다)

| GA | 계약 | 깨지면 |
|---|---|---|
| `GA_WaterPump` | `OnDamageTickReached` 는 **매 호출마다** `Make Damage Effect Params From Class Defaults` 를 부른다(결과 캐시 금지) | 램프 증가가 반영되지 않는다 |
| `GA_WaterPump` | 같은 이벤트의 아군 분기는 `Apply Gameplay Effect To Target` 이 아니라 **`Grant Water To Ally`** | 칩이 반영된 급수량(SetByCaller)이 안 들어간다 |
| `GA_ClawSwipe` | `Play Montage and Wait` **1P·3P 둘 다** `Play Rate` ← `Get Windup Play Rate` | 선딜이 안 늘어난다 / 한쪽만이면 손과 몸이 따로 논다(T-26) |
| `GA_ClawSwipe` | 타격 이벤트(`Event.Montage.ClawSwipe`) 맨 앞에 `Restore Montage Play Rate` — **서버 전용 분기 밖**(모든 머신에서 불려야 한다) | 후딜까지 3배 |
| `GA_ClawSwipe` | `Get Live Objects Within Radius` 와 디버그 구의 `Radius` ← `Get Upgraded Attack Radius` | 범위 칩 무반응 / 그려진 구와 실제 판정이 어긋난다 |
| `GA_ClawSwipe` | `Branch(Is Valid Damage Target)` 의 **False** → `Try Heal Ally With Own Health`(같은 `Array Element`). 칩 유무는 C++ 이 판단하므로 BP 는 **항상** 부른다 | 수혈 무반응 |
| `GA_VendingMachineBasicAttack` | ★계약 없음★ — 동전칩·잭팟칩은 BP 를 건드리지 않았다(D-45). 단 `AbilityTags` 는 `Abilities.VendingMachine.BasicAttack` **하나**로 유지(`…Jackpot` 을 넣지 말 것) | 업그레이드 키(첫 `Abilities.*` 태그)가 바뀌면 칩 전체가 무반응 |
| `GA_VendingMachine_AttackSpeedBuff` | `Commit Ability Cost → Apply Attack Speed Buff → Notify Skill Activated → Delay(0.5) → End Ability`. 옛 `Make Outgoing Spec` · `Apply GE Spec to Self` 는 **없어야** 한다 | 칩 ①②③ 전부 무동작(T-60) |
| `GA_SeedCannon` | `Play Montage`(1P) `Play Rate` · `Play Montage and Wait`(3P) `Rate` ← `Get Attack Speed Play Rate` | 씨앗 대포만 안 빨라진다 / 한쪽만이면 손·몸이 따로 논다 |
| `WBP_SkillIcon_VendingMachine` | 스택 이벤트: `FillRatio` ← `Get Attack Speed Buff Fill Ratio`(남은 시간 리셋 없음) / `UpdateBuffUI`: `FillRatio` ← 같은 함수, `<= 0` 이면 타이머 정지. 변수 `Buff Duration` · `Buff Remaining Time` 은 안 쓴다 | ①의 5초를 못 그리거나, ③ 감소마다 그래프가 리셋된다(D-76) |
| `WBP_UpgradeSlot` | ★계약 없음★ — 이 위젯의 `EDRUpgradeResult` Switch 는 **슬롯 해금 버튼** 결과용이라 `SkillConflict` 가 나오지 않는다. `CanEquipChipAtSlot` 은 Switch 없이 쓰인다 | — |

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
아이콘은 `Content/DaeRuneAssets/UI/Upgrade/Icon/Vending/` — 동전 `-3` · 잭팟 `-4` · 공속 `-5` (각각 공격 / 지원 / 편의 순).

### 5.6 `ChipTuning.Jackpot` (전부 C++ 기본값 — 에셋에 입력하지 않아도 된다)

| 필드 | 값 | 비고 |
|---|---|---|
| `GoldRushOdds` | 0.75 / 0 / 0.25 | ① 확률표 |
| `DoubleGoldToBronzeShare` | 0.5 | ② 기본 골드를 브론즈로 옮기는 비율(나머지는 실버) |
| `DoubleShotDelay` | 0.15 | ② 두 번째 캡슐 지연. ★최단 잭팟 간격(0.55초)보다 짧게★(T-56) |
| `PityBaseOdds` | 0.45 / 0.5 / 0.05 | ③ 시작 확률표 |
| `PityStep` | 0.02 | ③ 비골드 1회당 %p |
| `PityGoldDamage` | 120 | ③ 골드 피해. ★BP `GoldCapsuleDamage`(150)는 건드리지 않는다★(T-58) |

칩이 없을 때의 잭팟 값(50/40/10, 10/50/150, `MaxJackpotStacks` 10)은 `GA_VendingMachineBasicAttack` BP 에 있다.
카탈로그 `SkillDisplayNames` 에 `Abilities.VendingMachine.Jackpot → 잭팟` 이 있다.

### 5.7 `ChipTuning.AttackSpeedBuff`

| 필드 | 값 | 비고 |
|---|---|---|
| `BaseSpeedPerStack` | 0.2 | 칩 없을 때 1회 +20% (예전 C++ 리터럴) |
| `OverdriveSpeedPerStack` / `OverdriveDuration` | 0.3 / 5 | ① |
| `SharedEffect` | `GE_AttackSpeedBuff_Share` | ② ★비우면 경고 로그 + 기본 버프★ |
| `SharedSpeedPerStack` / `SharedRadius` / `SharedDuration` | 0.1 / 2000 / 20 | ② (2000uu = 20m) |
| `DecayEffect` | `GE_AttackSpeedBuff_Decay` | ③ ★비우면 경고 로그 + 기본 버프★ |
| `DecaySpeedPerStack` / `DecayInterval` | 0.16 / 1 | ③ |

칩 없을 때의 지속(20초) · ③의 유지 시간은 GA `BuffDuration`, 최대 중첩은 GE 의 Stack Limit 이 권위다.
카탈로그 `SkillDisplayNames` 에 `Abilities.VendingMachine.AttackSpeedBuff → 공속 버프` 가 있다.

### 5.8 GE · GA 에셋 요구 사양

모두 `Content/Blueprints/AbilitySystem/Player/GardenRobot/Abilities/` 아래
(`Skill1` = 기본 공격, `Skill2` = 물대포, `Skill3` = 씨앗 폭탄). `GE_Heal` 만 `Player/Effects/`.
공속 버프 GE · GA 는 `Player/VendingMachine/Abilities/Skill2/`.

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
| `GE_AttackSpeedBuff` (원본) | HasDuration(값은 코드가 덮는다) / 모디파이어 없음 / Asset · Target Tags `Buff.VendingMachine.AttackSpeed` / Aggregate by Target, Limit 10, `Clear Entire Stack`. ★감쇠 정책을 넣지 말 것(T-68)★ |
| `GE_AttackSpeedBuff_Share` | 원본 복제 → **Limit 5**, Asset · Target Tags **`Buff.AttackSpeed.Shared`** |
| `GE_AttackSpeedBuff_Decay` | 원본 복제 → **`Remove Single Stack and Refresh Duration`**. 태그 · Limit 10 · `Refresh on Successful Application` 그대로 |
| `GA_VendingMachine_AttackSpeedBuff` | `WaterCost` 50 / `AttackSpeedBuffEffect` = `GE_AttackSpeedBuff` / `BuffDuration` 20(C++ 기본) |
| `GA_SeedCannon` | `BaseMontagePlayRate` 1.0(C++ 기본 = BP 원래 핀 값) |

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
| **T-28** | 한 스윙에 아군 여러 명이면 | 체력이 **인원수만큼** 나간다(의도된 동작) |
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
| **T-46** | 스킬당 1칩 판정을 `CanEquipChip` 에만 넣기 | 옛 세이브·조작된 보고는 2개가 그대로 적용된다 — `SanitizeLoadout` · `SanitizeClassState` 도(D-49) |
| **T-47** | `CanEquipChipAtSlot` 이 `SkillConflict` 를 그대로 반환 | 충돌 칩 칸에 드롭해도 교체가 안 된다 — "목적 칸 칩을 빼면 더는 안 부딪히는가"로 판정 |
| **T-48** | `SkillConflict` 를 enum 중간에 삽입 | BP Switch 핀·바이트 값이 밀린다(D-52) |
| **T-49** | 스킬 칩의 칩 레벨 `TargetAbilityTag` 를 비움 | 제한을 조용히 빠져나간다 — D-51 검사가 잡는다 |
| **T-50** | 미리보기가 충돌 칩을 안 빼고 계산 | 불가능한 "두 칩 합친 수치"를 보여 준다 |
| **T-51** | `Abilities.VendingMachine.Jackpot` 을 GA `AbilityTags` 에 넣기 | 업그레이드 키가 흔들려 동전칩 전체가 무반응이 될 수 있다 |
| **T-52** | 잭팟 칩에 스킬 모디파이어를 `Jackpot` 태그로 달기 | 그 태그를 가진 GA 가 없어 아무 데도 안 걸린다(검사도 통과) — `ChipTuning.Jackpot` 으로 |
| **T-53** | 클라에서 `GetJackpotOdds()` 로 UI 표시 | ③ 누적 상태는 서버 전용 — 보정이 빠진 값이 보인다(§9-24) |
| **T-54** | ② 타이머 콜백에서 죽음·권한 확인 누락 | 죽은 뒤에 캡슐이 나가거나 스폰이 두 번 |
| **T-55** | ② 두 번째 발에서 재굴림 · 스택/③ 보정 변경 | "같은 색 1발 더"가 아니게 된다 — 상태는 첫 발에서만 |
| **T-56** | `DoubleShotDelay` ≥ 다음 잭팟까지의 시간 | 두 번째 발이 다음 잭팟 예약에 덮인다. 최단 0.55초 > 0.15초 |
| **T-57** | 골드 0% 인데 부동소수 오차로 골드 | `RollJackpotTier` 가 골드 0 이면 마지막 칸을 실버/브론즈로 돌린다 |
| **T-58** | ③ 골드 120 을 BP `GoldCapsuleDamage` 로 바꾸기 | 칩이 없을 때까지 골드가 약해진다 — `PityGoldDamage` 로만 |
| **T-59** | 칩의 `Granted Tags` 누락 | 기능 스위치 칩(거스름 동전·잭팟 3종)이 **에러 없이** 무동작한다. 단점 모디파이어가 있으면 검사도 못 잡는다 — 실제로 거스름 동전에서 났다 |
| **T-60** | `GA_VendingMachine_AttackSpeedBuff` 에 옛 `Make Outgoing Spec → Apply GE Spec to Self` 를 남기기 | 기본 20초 버프만 걸려 **칩 ①②③ 전부 무동작**, 에러도 없다 |
| **T-61** | 공유 버프를 원본 GE 로 걸기 | 자기 버프와 스택이 섞인다(D-63) |
| **T-62** | 공유 GE 의 Stack Limit 을 5 로 안 두기 | "최대 5중첩"은 에셋이 권위다 — 코드가 자르지 않는다 |
| **T-63** | `RestoreMontagePlayRate` 에 공속 배율 누락 | 후딜만 원속이 된다 |
| **T-64** | 청소기 차지 간격을 매번 다시 계산 | 충전 중 버프가 끝나면 단계가 뒤로 튄다 |
| **T-65** | 아군 탐색에 `GetLiveObjectsWithinRadius` | 적·사이트가 섞이고 `[MoleDiag]` 로그가 쏟아진다 |
| **T-66** | 클라 예측 몽타주(발톱·씨앗)의 재생 속도 | 클라가 자기 ASC 의 공유 GE 를 알아야 한다(소유 클라에는 복제된다) — **멀티로 확인**(§10.3) |
| **T-67** | 공속 칩 그룹 키를 `BasicAttack` 으로 | 동전칩과 함께 못 낀다 |
| **T-68** | 감쇠 정책을 원본 `GE_AttackSpeedBuff` 에 넣기 | 칩 없는 자판기도 20초마다 1개씩 빠진다 |
| **T-69** | 시작 시각 밀기를 첫 적용에만 | 다시 쓰는 순간 지속이 1초로 리셋돼 **1초 뒤** 감소가 시작된다 — 매 적용 직후 민다 |
| **T-70** | ③ 감소 간격·유지 시간을 GE 의 Duration 으로 바꾸려 하기 | 코드가 지속을 고정해 넣어 에셋 값은 무시된다 — `DecayInterval` / GA `BuffDuration` 에서 바꾼다 |
| **T-71** | `Buff.VendingMachine.AttackSpeed` 를 `DA_StatusEffectInfo` 에 넣기 | ③ GE 의 Spec 지속이 1초라 상태효과 위젯이 "1초"로 표시한다 |
| **T-72** | 공속 HUD 를 적용·제거 델리게이트로만 갱신 | 스택 감소와 (원격 클라) 10중첩 재사용을 놓친다 — 숫자는 태그 `AnyCountChange`, 그래프는 읽어 오기(D-75 · D-76) |
| **T-73** | ③ 감소 구간을 남은 시간만으로 판정 | 유지 구간의 마지막 1초와 구분이 안 된다 → 자기 버프 스택 흐름으로 판정하고, ★스택 방송 **전에** 갱신★ 한다(위젯이 방송을 받자마자 비율을 읽는다) |

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
| **8. 공격 속도에 반응하게 하기** (1~3줄) | "벨보이 기본 공격도 공속 버프를 받게" | 그 공격의 손잡이에 `GetAttackSpeedMultiplier()`(C++ / BP Pure)를 곱한다 — 몽타주면 PlayRate(1P·3P, 선·후딜 모두), 타이머면 간격 ÷. 몽타주 중간에 PlayRate 를 되돌리는 곳이 있으면 거기에도 곱한다(T-63). 버프가 없으면 1.0 이라 회귀가 없다 |
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
| 스킬당 1칩 판정을 호출부마다 따로 쓰기 | `FindSkillConflict()` 하나만 쓴다(D-49) |
| 칩 그룹 키 태그(`…Jackpot`)를 GA 에 붙이기 | 업그레이드 키가 흔들린다(T-51) |
| 칩마다 지속이 다른 버프 GE 를 BP 에서 직접 Apply | 칩 지식이 BP 로 샌다 — 지속은 C++ 이 스펙에 고정해 넣는다(D-65 · T-60) |
| HUD 위젯이 버프 시간을 직접 세기 | 칩마다 지속이 달라지면 틀린다 — 남은 시간은 GE 에서 읽는다(D-76) |

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
| **10** | ~~`[MoleDiag]` 임시 로그 6개(A~F)가 남아 있다~~ | **해결**(2026-10-08, 사용자 요청) — 6개 전부와 `DRS2Mole.cpp` BeginPlay 의 `[S2Mole]` 진단 로그까지 지웠다(벨보이 `[임시 진단]` 로그는 두더지와 무관해 남겼다) |
| **11** | 튜토리얼 더미를 때려도 흡혈된다(흡혈 블록이 더미 분기보다 위에 있다) | 연습용이므로 허용 |
| **12** | 발톱 ③으로 범위가 +30% 여도 **스윙 이펙트는 그대로**다 | 그대로 둔다. 바꾸려면 나이아가라 `RadiusScale` User 파라미터("하지 말 것" 참고 — 에셋을 둘로 나누지 말 것) |
| **13** | 기본 공격은 **클렌저 사이트에도 적중 처리**된다(`IsValidDamageTarget` 이 true) | Plan5 이전부터의 동작이라 건드리지 않았다. 수혈은 D-27 로 사이트를 거른다 |
| **14** | ★동전 ①은 단독 DPS **×0.72**★ — 한 발 +20% 보다 연사 −40% 가 크다(초당 캡슐·초당 ② 회복도 준다) | 사용자 확정값. "한 방은 세고 DPS 는 비슷한" 칩을 원하면 피해 **+67% 이상**이 필요 — 플레이 후 카탈로그 `Value` 만 조정 |
| **15** | ~~발톱 흡혈은 오염 중에도 회복된다 → 적 1회 타격으로 즉시 정화~~ | **해결**(2026-10-08, 사용자 결정) — `GetLifestealRatioForDamage` 에 시전자 오염 검사를 넣어 동전 ②(D-42)와 같이 막았다. 오염 해제는 아군 회복으로만 |
| **16** | 동전 ③으로 커져도 임팩트 이펙트(`NS_Basic_3_*`)는 그대로 | 12번과 같은 판단 |
| **17** | ~~공격 속도 버프의 스택당 0.2 가 C++ 리터럴(`GetCurrentFireInterval`)~~ | **해결**(2026-10-07) — `ChipTuning.AttackSpeedBuff.BaseSpeedPerStack` 으로 옮겼다(D-62) |
| **18** | ~~잭팟 캡슐 분기에 `Projectile` null 체크가 없다~~ | **해결** — `SpawnJackpotCapsule()` 에서 확인한다 |
| **19** | 주석이 실제 값과 다르다("5회 후"는 고쳤다): `BaseFireInterval = 0.3f`(BP 0.6) / ~~`DRVendingMachineAttackSpeedBuff.h` "+10%"~~(2026-10-07 고침) | `BaseFireInterval` 주석만 남았다 |
| **20** | ~~D-46 검증 규칙 미적용~~ | **D-51 로 적용됨** |
| **21** | ★잭팟 기대값 차이가 크다★ — ② +40% / ① +12.5% / ③ 약 +8% | 사용자 확정 수치. 플레이 후 `ChipTuning.Jackpot` 만 조정(재컴파일 없음) |
| **22** | ③ 기대값(약 43)은 손 계산 근사 | `[Jackpot]` 로그 100회 이상으로 확인 |
| **23** | `[Jackpot]` 로그가 잭팟마다 1줄 남는다(`LogDR`, Log) | 검증용. 수 초에 한 줄이라 그대로 둬도 되고, 지운다면 `ExecuteShot()` 의 한 줄 |
| **24** | ③의 "현재 골드 확률"을 HUD 에 안 보여 준다 | 보여 주려면 `PityBronzeSteps/PitySilverSteps` 소유자 전용 복제 + BP 표시(T-53) |
| **25** | 칩 카드에 `SkillConflict` 사유 문구가 없다 — 겹친 칩은 흐리게만 보인다 | 선택. `WBP_UpgradeChip` 에서 `EquipBlockReason == SkillConflict` 이면 `ConflictingChipName` 으로 "'○○'과 같은 스킬 — 그 칸에 놓으면 교체" |
| **26** | 고액 동전 발사 간격 줄의 `Detail Override` 가 비어 있다 | 자동 문구 "스킬 발사 간격 +0.4초"가 나온다(틀리진 않음). 원하면 `발사 간격 1.0초 (기존 0.6초)` |
| **27** | 공속 ① 최대치 유지 비용(초당 물 약 26) | 사용자 확정 수치. 플레이 후 카탈로그 `Value` / `OverdriveDuration` 만 조정 |
| **28** | ~~공속 칩 ③ 보류~~ | **확정**(2026-10-07) — D-72 ~ D-75 |
| **29** | 여러 자판기의 공유 버프는 한 더미(최대 5) | 따로 쌓으려면 공유 GE 를 Aggregate by Source 로 |
| **30** | ② 아군 판정은 쓰는 순간 1회 | 오라(범위 안에 있는 동안만)를 원하면 Synergy 오라처럼 컴포넌트로 — 범위가 커진다 |
| **31** | 청소기 "후딜"은 거의 없다(발사 즉시 다시 쓸 수 있다) | 실효는 차지 단축. 강화탄 몽타주만 빨라진다 |
| **32** | ~~공속 GA 헤더 주석 "+10%"~~ | **해결**(2026-10-07) |
| **33** | ③ 이름 "잔류 가속" · Tier 3 · `편의-5` 는 가안을 그대로 입력했다 | 바꾸려면 카탈로그만 |
| **34** | ③ 첫 감소 = 20초가 되는 순간(29초에 0) | 21초를 원하면 밀기 값을 `BuffDuration` 으로(−Interval 없이) |
| **35** | `DA_StatusEffectInfo` 에 `Buff.AttackSpeed.Shared` 가 없다 — 공유 버프를 받은 정원로봇·청소기 화면에 버프 아이콘이 안 뜬다(효과는 정상) | 선택. 한 줄(태그 · "공속 증가" · `SkillIcon_AttackSpeedBuff`). ★`Buff.VendingMachine.AttackSpeed` 는 넣지 말 것(T-71)★ |

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

### 10.2 동전칩 · 잭팟칩 · 스킬당 1칩

C++ 빌드 통과, 에셋 입력은 2026-09-28 에셋을 직접 읽어 확인했다(동전 3 · 잭팟 3 · 태그 · 아이콘 · `SkillDisplayNames` 5줄 · `ChipTuning.Coin.HealEffect`).
준비: 자판기를 고른 뒤 콘솔 `DRUnlockUpgrade` → `DRUnlockSlot 6`. 발사 간격은 10초간 발사 수(0.6초 ≈ 17발, 1.0초 = 10발),
잭팟 확률은 출력 로그 **`[Jackpot]`** 줄(결과 · 그때의 확률표 · 적립 횟수)로 본다.

- [ ] **회귀(칩 없음)** — 동전 20 / 0.6초 / 잭팟 50·40·10, 캡슐 10·50·150, 1발 / 크기 그대로 / 적 원거리·잠자리·청소기 에어샷 그대로 / 발톱 흡혈 그대로
- [ ] **동전 ①** 동전 24 / 1.0초 / 캡슐 12·60·180 / 버프 10스택 0.333초
- [ ] **동전 ②** 적 적중마다 +2(캡슐 포함) / 동전 14 / 벽·바닥·사이트·만피 → 없음 / ★오염 중 → 회복 없음★ / +⑥ → 없음
- [ ] **동전 ③** 동전·캡슐 ×1.3 · 판정 확대 / 동전 14 / 벽에 붙어 쏘기·바닥 사격(T-43)
- [ ] **잭팟 ①** 확률 75/0/25, 실버 0회
- [ ] **잭팟 ②** 확률 55/45/0, 골드 0회 / 같은 색 0.15초 뒤 1발 더 / 버튼을 떼도 나감 / 죽으면 안 나감 / 소리 2 · 몽타주 1
- [ ] **잭팟 ③** 45/50/5 → 브론즈 뒤 43/50/7 → 실버 뒤 43/48/9 … 골드면 45/50/5 / 골드 피해 120 / 부활해도 적립 유지
- [ ] **스킬당 1칩** 같은 그룹 칩 빈 칸 장착 거부 / 그 칩 칸에 드롭하면 교체 / 동전 1 + 잭팟 1 은 함께 / 미리보기는 "교체" 기준 수치 / 옛 세이브(같은 그룹 2개)는 앞 칸만 남고 경고 로그
- [ ] **멀티** 클라 동전 ③ → 호스트 화면에서도 크다 / 클라 동전 ② → 클라 체력바 +2 / 클라 잭팟 ② → 호스트 화면에서도 두 발 / 조작된 보고 → 서버 정화 로그
- [ ] **검사** `DA_ProgressionConfig` 를 편집해 검사 → 칩·확률표 오류 0건

전부 통과하면 §10.2 를 지우고 머리말 상태를 "완료"로 바꾼다.

### 10.3 공속 버프칩

에셋 입력은 2026-10-07 에셋을 직접 읽어 확인했다(GE 2 · GA 노드 교체 · 씨앗 대포 핀 · 아이콘 위젯 · `ChipTuning.AttackSpeedBuff` · 카탈로그 3종 + `SkillDisplayNames`).
`GA_VendingMachine_AttackSpeedBuff` 의 노드 **순서**(Apply → Notify)는 바이트코드 도구가 이 에셋을 못 읽어 확인하지 못했다 — 에디터에서 한 번 본다.
로그: `[AttackSpeedBuff] Share: … N명(본인 포함)` / `… 미지정` 경고.

- [ ] **회귀(칩 없음)** 20초 · 10중첩 · +20% · 물 50 / 그래프 20초 / 발톱 · 씨앗 · 청소기 속도 그대로 / 적 근접 공격 그대로
- [ ] **①** 10중첩 0.15초 · 물 130 · 5초 뒤 전 스택 소멸 · 그래프 5초 · 카드에 물 +160% 빨강
- [ ] **② 본인** +10% · 5에서 멈춤 · 아이콘 숫자 = 공유 스택
- [ ] **② 아군** 20m 안 정원로봇 발톱 · 씨앗 동작 / 청소기 풀차지 1.0초 / 자판기 발사 간격 / 20m 밖 · 죽은 아군 제외 / 자판기 2대 모두 ② → 합쳐 최대 5
- [ ] **③** +16% / 20초 뒤 1초에 1개 → 29초에 0 / 감소 중 다시 쓰면 감소 정지 · 20초 유지(T-69) / 아이콘 10 → 9 → … / 그래프는 20초 동안 줄고 감소 중엔 빈다 / 감소 중 사망 · 페이즈 전환에 잔여 · 에러 없음
- [ ] **스킬당 1칩** ①②③ 중 하나만, 동전칩 · 잭팟칩과는 함께
- [ ] **멀티** 클라 ② → 호스트 · 다른 클라가 버프 / 클라 정원로봇 **자기 화면** 스윙도 빨라진다(T-66) / 클라 ③ 아이콘 · 발사 간격이 1초씩 줄어든다 / 원격 클라가 10중첩에서 다시 쓰면 그래프가 다시 찬다
- [ ] **검사** `DA_ProgressionConfig` 편집 → 오류 0건(공속 GE 검사 포함)

전부 통과하면 §10.3 을 지우고 머리말 상태를 "완료"로 바꾼다.

---

## 11. 남은 작업

- **청소기 칩 12종(§12~§15)**: C++ 구현 완료(빌드 통과 2026-10-08). **남은 것 = 에디터 작업 → §16 안내서**(이미 있는 에셋 4곳 수정 · 새 GE 4개 · `DA_ProgressionConfig` · 카탈로그 12칩 · `SkillDisplayNames` 4줄) → 각 절의 검증(§12.7 · §13.7 · §14.7 · §15.7) → §1~§9 흡수(§1.7 그룹 6 → 10개, 칩 18 → 30종). 아이콘은 사용자가 따로 넣는다.
- 청소기 칩을 빼면 **필수 에디터 작업은 없다**(2026-10-07 에셋 확인 — 정원로봇 · 동전 · 잭팟 · 공속 전부).
  선택 사항만 남았다 — §9-35(공유 버프 상태 아이콘), §9-25(칩 카드 사유 문구), §9-26(`Detail Override`).
- 그다음은 §10 플레이 검증과, 결과에 따른 밸런스 조정(§9-14 · §9-21 · §9-27)이다.

---

## 12. 로봇 청소기 탑승(패시브) 칩 3종 (C++ 구현 완료 — 에디터 · 검증 남음)

> 2026-10-07 요청 · 같은 날 질의응답과 두 번의 정정으로 확정. 코드 주석 접두 **`Plan5(탑승칩)`** · 결정 **D-77 ~ D-92** · 함정 **T-74 ~ T-94** (앞 번호에 이어서 매긴 영구 ID).
> **C++ 구현 완료**(2026-10-08 빌드 통과). 에디터 작업은 **§16 안내서**, 그다음 §12.7 검증. 계획과 달라진 점은 §12.10. 검증이 끝나면 §1~§9 에 흡수한다.
> ★선행 변경 2건 — **D-82**(공유 피해에서 '주는 피해' 배율 제거) · **D-87**(오염 최대 체력 100 고정) — 은 칩이 없어도 지금 동작을 바꾼다(§12.8-42). 공유 범위는 지금처럼 1칸(D-92 — 2차 정정).★
>
> 사용자 확정
> - ① 탑의 **맨 아래(직접 달리는) 청소기의 칩만** 유효. 그 위 인원 1명당 **탑승자 전원** 공격력 +8%(최대 24%), **그 청소기** 이속 −5%.
>   "여러 명 탑승" = 청소기 위에 청소기가 타고 … 맨 위에 다른 캐릭터 — 새 기능이 아니라 **지금의 탑승 그대로**다(맨 아래 위로 최대 3명).
> - ② 칩② 청소기 **위의 전원**이 보호된다. 보호받는 사람이 **최종적으로 받아야 할 피해**(받는 쪽 배율 · 데미지 디버프 반영)를 **80% 줄여서** 칩② 청소기가 대신 받는다.
>   칩② 청소기 자신의 공유분은 그대로다. 예: 2층 V② ← R 에서 누가 20 맞든 V 24(20 + 4), R 0. 대신 최대 체력 −100.
> - ③ **바로 위 1명**만 본다. **피격 직후 체력 30% 이하면 매번**(30% 이하로 탔어도 다음 피격에), 피해로 오염에 들어가도 발동.
>   즉시 탈출 + 탑승자·청소기 체력 +100, 그 사람은 20초 동안 **이 청소기에** 못 탄다. 체력 **비용**(피의 계약 등)으로는 발동하지 않는다.
> - (2차 정정) 탑승 공유는 **지금처럼 바로 위 · 아래 1칸**이다(D-92). 3층 A② ← B ← C 에서 C 가 맞으면 A = 4 + 4 = **8**, B 가 맞으면 **28**, A 가 맞으면 **24**.
> - (2차 정정) **디버프로 받는 피해도 넘긴다** — 보호받는 사람에게도 디버프는 그대로 걸리고, 그 DoT 틱이 20% 로 넘어간다(D-84).
> - (2차 정정) 광폭화 탑승자는 칸이 아니라 **피해 수치**로 계산한다(§12.8-47).
> - 공유 · 이전 피해는 **받는 피해** 기준이다 — 탑승자가 **주는 피해**(공격력 · 광폭화 · 시너지 · 칩①)는 상관없다. 탑승자에게 걸린 데미지 디버프(받는 피해 증가)는 포함 (D-82).
> - 오염 상태의 최대 체력은 칩과 무관하게 **항상 100** (D-87).

### 12.0 지금의 탑승 (코드·에셋 실측)

| 항목 | 지금 | 어디 |
|---|---|---|
| 탑승 | **F**(상호작용) → `ServerRequestInteract` → `CanBeMountedBy`(위에 라이더 없음 · 후보 미탑승 · 둘 다 생존 · 후보 부품 없음 · 후보 스턴 아님 · 순환 없음) + 거리 ≤ 800 → `MountRider` | `DRPlayerController.cpp` · `DRRobotVacuumCharacter.cpp` |
| 감지·프롬프트 | 반경 300 스피어 오버랩 → 카메라 레이 250uu(Pawn 오브젝트) → `!bDead && RiderOnTop == nullptr` 면 프롬프트(로컬, 서버가 최종 검증) | `FindMountByLineTrace` |
| 하차 | 점프키 → `ServerRequestDismount` → `DismountRider(true)`(마운트 위 60 · 앞 80 으로 옮기고 위 300 · 앞 200 발사). 사망 · EndPlay 는 `DismountRider(false)` | 〃 |
| 상태 | `RiderOnTop`(청소기 쪽) ↔ `MountedOn`(탑승자 쪽) 둘 다 복제. 탑승자는 `MOVE_None` · 이동 입력 차단 · `State.Riding`(Loose + Replicated). 시점·스킬은 쓴다(`State.Riding` 을 막는 GA 없음 — 에셋 검색) | `DRCharacter` |
| 탑(여러 층) | **별도 기능 없음** — `CanBeMountedBy` 가 "위에 사람을 태운 후보"를 막지 않아 청소기 위에 청소기가 탄다. 4인 → 맨 아래 위로 최대 3명 | |
| 피해 공유 | `UDRPlayerAttributeSet::HandleIncomingDamage` 맨 끝 → `PropagateSharedDamage(L)` → **바로 위 · 아래 1칸에만** 같은 L. 받은 쪽은 `Damage.MountShared` 라 다시 퍼뜨리지 않는다 → **3층 A ← B ← C 에서 C 가 맞으면 A 는 0**. 옛 청소기 계획서(git `f1c21fba:Plan3.md`)의 결정 3 "직접 연결 1홉 — 3층 전체 공유가 원하는 동작이면 확장"을 따른 것. 사용자 확정으로 **그대로 둔다**(D-92) | `DRCharacter.cpp` |
| 공유 GE | 캐릭터 BP 의 `MountSharedDamageEffectClass`(4종 모두 `GE_Damage` — 에셋 확인), 물리, 소스 = 원 피격자 ASC. L = 받는 쪽 배율(부품 ×1.5 · `DamageTaken` · 시너지 · 로비 클램프)이 끝난 값 — 받는 피해 기준이라 의도와 같다 | 〃 |
| ★공유 피해 증폭(기존 버그)★ | `GE_Damage` 는 `ExecCalc_Damage` 를 탄다 → 소스(원 피격자)의 DamageDealt · 광폭화 · 시너지가 **공유분에 곱해진다** — 광폭화 탑승자가 20 맞으면 청소기 45~55. 공유는 받는 피해라 주는 피해 배율과 무관해야 한다(사용자 확정) | `ExecCalc_Damage.cpp` → D-82 |
| 사망 | 탑승자 사망 → 그 청소기에서 하차 / 청소기 사망 → 위 탑승자 하차 + 자기도 아래 청소기에서 하차 (둘 다 `bDead` 세팅 **전**) | `MulticastHandleDeath` 2곳 |
| 체력 | 컨테이너 4칸 × 100 = 400(`DA_GameBalanceConfig` 가 덮지 않음). 일반 체력 0 → 오염(최대 100) → 오염 체력 0 → 사망 | `DRPlayerAttributeSet.cpp` |
| ★오염 × 최대 체력 칩(기존 버그)★ | 오염 진입·해제·정화가 `SetMaxHealth`(= **BaseValue**)를 쓴다 → `GE_Upgrade_Stats`(Infinite, MaxHealth **AddBase**)의 칩 가산이 그 위에 다시 붙는다. +100 칩: 오염 200 · 정화 후 600 / ⑧ 피의 계약(칸 ×3): 오염 900 · 정화 후 2000 | → D-87 (오염 최대 **항상 100**) |
| 이동 속도 | 청소기 550(`GE_DRPrimaryAttributes_RobotVacuum`). MoveSpeed 어트리뷰트 → `OnMoveSpeedChanged` → `MaxWalkSpeed`. 돌진 = `GE_VacuumDashSpeedBuff`(Infinite · **MultiplyAdditive** · by Target 5) | |
| 돌진 자해 | 지속 돌진 충돌 → `GE_VacuumDashSelfDamage`(IncomingDamage +50) → 일반 피해처럼 탑승자에게도 공유된다 | `DRVacuumDash.cpp` |
| 청소기 칩 | **없음** — 카탈로그 26종 = 공용 8 · 정원로봇 9 · 자판기 9. 아이콘 폴더도 `Common` · `Gardener` · `Vending` 뿐 | `DA_ChipCatalog` |

**이 설계가 기대는 엔진·코드 사실**

| 항목 | 사실 |
|---|---|
| 탑 구조 | 청소기마다 `RiderOnTop` 1명 · 캐릭터마다 `MountedOn` 1개 → 탑은 항상 **한 줄**(가지가 없다). 맨 아래까지 내려간 뒤 `RiderOnTop` 사슬로 올라가면 전원이 나온다 |
| MoveSpeed 합성 | `((Base + AddBase) × MultiplyAdditive ÷ DivideAdditive × MultiplyCompound) + AddFinal` (UE 5.5 `GameplayEffectTypes.h`) — Compound 는 돌진 버프까지 포함한 값에 곱해진다 |
| `SetMaxHealth` | `ATTRIBUTE_ACCESSORS` 세터 = `SetNumericAttributeBase` → **BaseValue**. 현재값 = Base + 활성 GE 모디파이어 |
| 어트리뷰트 갱신 순서 | `FActiveGameplayEffectsContainer::InternalUpdateNumericalAttribute` 는 값을 넣은 **뒤에**(Pre/PostAttributeChange 포함) 값 변경 델리게이트를 방송한다 — `PostAttributeChange` 안에서 Base 를 다시 바꾸면 안쪽 방송(새 값) 뒤에 바깥 방송(옛 값)이 나간다 |
| 플레이어 ASC | PlayerState 소유 · **Mixed** 복제 → 활성 GE(스펙 · 컨텍스트 포함)는 소유 클라에 온다. ★리스폰해도 ASC 의 GE 는 남는다★ |
| 컨텍스트 복제 | `FDRGameplayEffectContext::NetSerialize` 는 Instigator · EffectCauser 를 보내고 받는 쪽이 `AddInstigator` 로 시전자 ASC 를 복원한다. **`SourceAbilityTags` 는 보내지 않는다**(서버 전용) |
| 피해 관문 순서 | `UDRAttributeSet::PostGameplayEffectExecute`: 사망 대상 무시 → **모자(Plan8) 1회 무효** → 도발 ×0.8 → `HandleIncomingDamage` |
| `GE_Damage` | `ExecCalc_Damage` 하나(에셋 확인) |
| GE 필드 | UE 5.5 `UGameplayEffect` 의 `DurationPolicy` · `Modifiers` · `StackingType` 는 공개 UPROPERTY — 검사 코드가 CDO 에서 바로 읽는다 |
| `CorruptMaxHealth` | `UDRPlayerAttributeSet` 의 `UPROPERTY()` 100 (에디터 노출 없음) — 오염 최대 체력의 유일한 값 |

### 12.1 칩 3종 스펙

공통: `OwnerClass = RobotVacuum`, `Category = Ascension`, `RequiredSlotCount = 1`,
칩 레벨 `TargetAbilityTag = Abilities.RobotVacuum.Ride`(**그룹 키 전용** — D-77). → ①·②·③ 중 하나만 낄 수 있다(청소기의 첫 칩 그룹).

| # | `ChipId` | 이름(가칭) | Modifiers | GrantedTags | 효과 | 단점 | Tier | 아이콘(제안) |
|---|---|---|---|---|---|---|---|---|
| ① | `RV.Asc.RidePower` | 합승 부스터 | (없음) | `…Ride.Power` | 맨 아래에서 달릴 때, 위 인원 1명당 **탑승자 전원** 주는 피해 **+8%**(최대 **24%**) | 1명당 **이 청소기** 이속 **−5%** | 3 | 공격-6 |
| ② | `RV.Asc.RideGuard` | 대신 맞는 범퍼 | `MaxHealth` **Flat −100** ★단점★ | `…Ride.Guard` | 이 청소기 **위의 전원**이 피해를 받지 않고, 그 사람들이 받아야 할 최종 피해를 **80% 줄여** 이 청소기가 받는다(자기 공유분에 더해 — 3층에서 누가 20 맞든 이 청소기 28) | 최대 체력 400 → **300**(칸 75 × 4) | 4 | 지원-6 |
| ③ | `RV.Asc.RideEject` | 비상 탈출 좌석 | (없음) | `…Ride.Eject` | **바로 위** 탑승자가 피격 후 체력 **30% 이하** → 즉시 탈출 + 탑승자·청소기 체력 **+100** | 탈출한 사람은 **20초** 동안 **이 청소기에** 못 탄다 | 3 | 편의-6 |

`…` = `Upgrade.Feature.Skill`. ①·③은 단점이 규칙이라 모디파이어 없이 **`Description` 필수**(검사가 강제 — D-1 규칙).
`ChipId` 접두 `RV` = RobotVacuum(정원로봇 `GR` · 자판기 `VM` 과 같은 방식). 이름 · Tier · 아이콘은 가칭(§12.8-36).

**① 수치** (기본 이속 550, N = 맨 아래 청소기 위 인원)

| N | 탑승자 주는 피해 | 청소기 이속 |
|---|---|---|
| 0 | ×1.0 | 550 |
| 1 | ×1.08 | 522.5 |
| 2 | ×1.16 | 495 |
| 3 | ×1.24 (상한) | 467.5 |

- 위층 청소기의 칩①은 업혀 있는 동안 **쉰다**(공격력도 감속도 없음). 내려서 맨 아래가 되는 순간 켜진다.
- 맨 아래 청소기가 칩①이 아니면 탑 전체에 효과가 없다. 청소기 **자신**은 오르지 않는다(탑승자만).
- 돌진 속도에도 −5%/명이 곱해진다(Compound — D-80).
- 탑승자의 DoT(화상 등)는 오르지 않는다 — ⑥ 광폭화 · ⑦ 시너지와 같다(ExecCalc 를 안 탄다).
- **공유 · 이전 피해에는 붙지 않는다**(D-82 — 공유는 받는 피해 기준).

**② 수치** (피해 20, 받는 쪽 배율 없음, 공유는 지금처럼 1칸 — D-92)

| 상황 | 칩 없음 (D-82 뒤) | 칩② |
|---|---|---|
| 2층 V② ← R, R 이 맞음 | R 20 / V 20 | R **0** / V **24** (20 + 4) |
| 〃 V 가 맞음 | V 20 / R 20 | V **24** (20 + 4) / R **0** |
| 3층 A② ← B ← C, C 가 맞음 | C 20 / B 20 / A 0 | A **8** (C 몫 4 + B 몫 4) · B · C 0 |
| 〃 B 가 맞음 | A 20 / B 20 / C 20 | A **28** (20 + 4 + 4) · B · C 0 |
| 〃 A 가 맞음 | A 20 / B 20 / C 0 | A **24** (20 + 4) · B · C 0 |
| 3층 A ← B② ← C, C 가 맞음 | C 20 / B 20 / A 0 | B **24** (20 + 4) · A · C 0 |
| 〃 B 가 맞음 | A 20 / B 20 / C 20 | A 20 / B **24** / C 0 |
| 칩② 두 대 A② ← B② ← C, B 가 맞음 | A 20 / B 20 / C 20 | A **24.8** (20 + 4 + 0.8) · B · C 0 (§12.8-46) |
| 화상 DoT 틱 5 (V② ← R, 화상은 R 에게) | R 5 / V 5 | R **0** / V **6** (5 + 1) |
| 돌진 충돌 자해 50 (V② ← R) | V 50 / R 50 | V **60** (50 + 10) / R 0 |
| 광폭화 탑승자 R 이 맞음 (V② ← R) | R 칸 1개 / V 20 | R **0** / V **24** — 칸이 아니라 피해 수치(사용자 확정) |
| 청소기 최대 체력 | 400 (100 × 4) | **300 (75 × 4)** · 오염 최대 **100**(D-87) |

- 보호받는 사람은 **어디서 온 피해든**(직접 피격 · 공유 · 디버프 DoT · 자해) 0 이고, 그 최종 피해(받는 쪽 배율 · 데미지 디버프 반영)의 20% 가 아래의 **가장 가까운** 칩② 청소기로 간다.
- 공유는 지금처럼 **바로 위 · 아래 1칸**(D-92) — 그래서 3층에서 맨 위가 맞으면 맨 아래 칩② 청소기는 이전분(4 + 4)만 받는다.
- **80% 감소는 이전분에만** 걸린다 — 칩② 청소기 자신의 공유분은 그대로다(T-93).
- 탑승자가 **주는** 피해(공격력 · 광폭화 · 시너지 · 칩①)는 공유 · 이전 어디에도 붙지 않는다(D-82).
- 보호받는 사람에게도 **디버프는 걸린다** — 그 DoT 틱이 같은 관문을 지나 20% 로 넘어간다(D-84). 스턴 같은 피해 없는 디버프도 탑승자가 받는다(§12.8-40).
- 칩② 청소기 쪽 데미지 숫자는 **여러 번** 뜬다(공유분 + 이전분마다) (§12.8-37).

**③ 수치**

| 항목 | 값 |
|---|---|
| 발동 | **바로 위 탑승자**가 피해를 받은 직후 체력 ≤ 최대의 30%(최대 400 이면 120) **또는** 이번 피해로 오염 진입 |
| 효과 | 즉시 하차(기존 점프 하차) → 탑승자 +100 · 청소기 +100 (`GE_Heal`) |
| 금지 | 그 탑승자는 **20초** 동안 **이 청소기**에 못 탄다 — 서버가 거부하고 F 프롬프트도 안 뜬다. 다른 청소기는 탈 수 있다 |
| 발동 안 함 | 체력 비용(⑧ 피의 계약 · 발톱 수혈), 칩② 보호 중(맞지 않으므로), 사망(사망 하차가 먼저) |

- 오염 탑승자는 회복 100 이 곧 **정화**(칸 1개)다 → "피해로 0 → 오염 진입"이 사실상 구조가 된다.
- 바로 아래 청소기가 맞으면 1칸 공유로 탑승자 체력도 깎이므로 그때도 발동한다.
- 20초 뒤 다시 타면 다시 발동할 수 있다(금지 중에는 못 타므로 그 사이엔 없다).

### 12.2 상호작용

| 조합 | 결과 |
|---|---|
| 공유 · 이전 피해 + 넘겨주는 사람의 광폭화(⑥) · 시너지(⑦) · `DamageDealt` · 칩① | **붙지 않는다**(D-82) — 공유는 받는 피해다. 광폭화 탑승자 20 피격 → 청소기 20(지금 45~55), 칩②면 탑승자 0 · 청소기 24 |
| ① + 광폭화(⑥) · 시너지(⑦) · `DamageDealt` 탑승자의 공격 | ExecCalc 소스측에서 전부 곱해진다. 광폭화 상한(3.0)은 DamageDealt + 광폭화에만 걸리고, 탑승 배율은 시너지처럼 그 밖(D-78) |
| ① + 다른 로봇 칩(동전 · 발톱 · 공속 …) | 별개 축이라 그대로 곱 — 예: 동전 ① 고액(×1.2) 자판기가 3명 탑 위 → 동전 ×1.2 × 1.24 |
| ① 탑에 칩② 청소기도 (A① ← B② ← C) | 둘 다 동작 — B · C 공격력 +16%. C 가 20 맞으면 A 0 · B 24(20 + C 몫 4) · C 0 |
| ② + 광폭화(⑥) **청소기** | 탑 피격 1회가 청소기에 **여러 번**(자기 공유분 + 이전분마다) 들어와 이전분이 4 여도 칸을 그만큼 비운다 — 3층 가운데가 맞으면 3칸 ★최악의 조합★ (§12.8-37) |
| ② + 광폭화(⑥) 탑승자 | 탑승자는 맞지 않아 칸을 잃지 않는다 |
| ② + 시너지(⑦) | 탑승자의 받는 피해 감소가 적용된 최종 피해의 20% → 청소기의 받는 피해 감소가 한 번 더 |
| ② + 모자(Plan8) 쓴 탑승자 | 모자가 먼저 막는다(관문 순서) — 모자 1개 소모, 그 피격은 **아무도** 피해 없음(공유도 없다) |
| ② + 도발(Plan8)된 적의 공격 | ×0.8 이 먼저 적용된 양이 공유 · 이전된다 |
| ② + 돌진 충돌 자해 | 청소기 50 + 탑승자 몫 50 × 0.2 = **60** |
| ② + 오염 청소기 | 오염 체력(최대 100 — D-87)으로 받는다 → 빨리 죽는다. 죽으면 탑승자 하차(기존) 뒤 탑승자가 직접 맞는다 |
| 칩② 청소기 두 대 (A② ← B② ← C) | C 몫이 B 를 거쳐 A 로 가며 0.2 가 두 번(0.04) — B 가 20 맞으면 A 24.8, C 가 맞으면 A 4.8 (§12.8-46) |
| ② 청소기 위에 ③ 청소기 (A② ← B③ ← C) | C 는 A 의 보호로 맞지 않아 B 의 ③ 이 발동하지 않는다 |
| ③ + 회복 차단(⑥) | 탈출 · 금지는 되고 그 대상의 회복만 빠진다 |
| ③ + 오염 탑승자 / 오염 청소기 | 회복 100 = 정화(칸 1개) |
| ③ + 피의 계약(⑧) · 발톱 ② 수혈 | 체력 **비용**으로는 발동 안 함(D-88) |
| ①②③ 같은 청소기 | 불가 — 같은 그룹(§1.7) |

### 12.3 설계 결정

| # | 결정 | 근거 |
|---|---|---|
| **D-77** | 그룹 키 = 새 태그 **`Abilities.RobotVacuum.Ride`** — 칩 레벨 `TargetAbilityTag` · `SkillDisplayNames` 에만, 어떤 GA 에도 안 붙인다. 기능 스위치 `Upgrade.Feature.Skill.Ride.{Power,Guard,Eject}` | 탑승은 GA 가 없는 캐릭터 패시브라 잭팟(D-53)과 같은 처리. `Skill` 하위라 D-51 검사가 칩 태그를 강제한다 |
| **D-78** | ① 탑승자 공격력은 **상태 없이** `ExecCalc_Damage` "주는 피해" 블록에서 계산 — `GetRideDamageMultiplier(SourceAvatar)`. 시너지(c)처럼 광폭화 상한 밖에서 곱한다 | 탑 구조(`MountedOn`/`RiderOnTop`)는 이미 복제되는 액터 상태다. 탑승자에게 GE 를 걸면 탑승 · 하차 · 사망 · 이탈마다 걸고 지울 곳이 생긴다(광폭화가 컨테이너 수를 매번 읽는 것과 같은 방식). 대신 HUD 표시가 없다(§12.8-38) |
| **D-79** | ① 은 **맨 아래 청소기의 칩만** — 위층 칩①은 업혀 있는 동안 쉰다 | 사용자 확정. 단점(이속)이 항상 의미를 갖고, 칩① 여러 개의 합산/최대 문제가 없다 |
| **D-80** | ① 감속 = 무한 GE **`GE_RideLoad`**(MoveSpeed **Multiply Compound** ← SetByCaller `Data.MoveSpeed.Mult` = 1 − 0.05 × N)를 맨 아래 청소기에. `RefreshRideLoad()`(서버)가 **태그로 지우고** 조건이 맞으면 다시 건다(멱등) | `MaxWalkSpeed` 는 MoveSpeed 어트리뷰트 → `OnMoveSpeedChanged` 경로라 GE 여야 서버 · 소유 클라 CMC 가 같은 속도를 쓴다(씨앗 가속·감속과 같은 경로). 태그로 지우는 이유는 T-84. Compound = 돌진 버프까지 포함한 최종 속도에 −5%/명 |
| **D-81** | 갱신 지점 = `MountRider` · `DismountRider` 끝(서버): 오르내린 탑승자(청소기면) + **이 청소기부터 맨 아래까지**. 사망 · EndPlay · 하차 입력 · ③ 탈출은 전부 `DismountRider` 를 지나 따로 걸 곳이 없다. 로드아웃 변경 · 빙의는 `RefreshUpgradeEffects` 의 새 가상 훅 `OnUpgradeEffectsRefreshed()` | 3층 탑에서 위층끼리 타고 내려도 맨 아래 인원이 바뀐다(T-85). 빙의 때 한 번 돌면 리스폰 뒤 남은 감속도 지워진다 |
| **D-82** | ★선행 변경★ 공유 · 이전 피해(`Damage.MountShared`)는 **받는 피해**다 — `ExecCalc_Damage` 의 "주는 피해" 배율((a) DamageDealt · (b) 광폭화 · (c) 시너지 · (d) 탑승)을 **전부 건너뛴다**. 넘기는 양 = 넘겨주는 사람이 최종적으로 받아야 할 피해 L(받는 쪽 배율 · 데미지 디버프 반영) | 사용자 확정(2026-10-07 정정): "받는 피해와 관련 — 탑승자의 공격력 증가는 상관없다", "탑승자에게 가해지는 데미지 디버프는 포함". 지금은 소스가 원 피격자라 그 사람의 공격 배율이 공유분을 키운다(광폭화 탑승자 20 → 청소기 45~55). 받는 쪽 디버프는 L 에 이미 들어 있다 |
| **D-83** | ② = **보호받는 사람의 관문에서 넘긴다**. `UDRPlayerAttributeSet::HandleIncomingDamage` 에서 `FindRideGuard()` 가 아래의 가장 가까운 칩② 청소기를 찾으면: 그 사람이 받아야 할 최종 피해 L(받는 쪽 배율 적용 후)의 **20%** 를 `TransferRideDamageToGuard()` 로 그 청소기에 넘기고(공유와 같은 `ApplyMountSharedDamageTo` — `Damage.MountShared`), 피격 자체의 1칸 공유(`PropagateSharedDamage(L)`)는 그대로 보내고, 자기 처리는 건너뛴다 | 사용자 확정("위 전원" · "둘 다 이전" · "최종 피해의 80% 감소"). 관문 한 곳만 고치면 직접 피격 · 공유 · DoT · 자해 · 여러 층이 전부 같은 규칙으로 풀린다(§12.1 표). `MountShared` 라 칩② 청소기가 다시 공유하지 않는다(T-74) |
| **D-84** | 보호받는 탑승자에게 그 피격의 **피격 큐 · 히트리액트 · 넉백 · 표정 · 데미지 숫자가 없다**. **디버프는 그대로 건다** — 디버프로 받는 피해(화상 DoT 등)는 매 틱 같은 관문을 지나 20% 로 넘어간다. 전투 진입(`NotifyEnterCombat`)은 남긴다. 모자(Plan8) · 도발 ×0.8 은 관문 앞이라 먼저 적용된다 | 사용자 확정(2026-10-07 2차 정정): "디버프로 받는 데미지도 넘겨야 한다". 디버프를 막으면 그 피해가 통째로 사라지고, 칩② 청소기에 걸면 80% 감소를 건너뛴다 — 탑승자에게 그대로 걸어야 DoT 틱이 일반 피해와 같은 규칙(이전 20% + 1칸 공유)을 탄다(T-94). 스턴처럼 피해 없는 디버프도 탑승자가 받는다(§12.8-40) |
| **D-85** | 이전 비율 = **`ChipTuning.Ride.GuardTransferRatio` 0.2**(80% 감소). **이전분에만** 곱한다 — 칩② 청소기 자신이 받는 공유분은 100% | 사용자 확정 — "최종적으로 탑승자에게 들어가야 할 피해의 80% 가 감소된 만큼". 숫자는 D-2 대로 Config 에 둔다. 칩② 청소기 둘이 겹치면 위쪽 칩② 청소기를 거치며 0.2 가 한 번 더 곱해진다(§12.8-46) |
| **D-86** | ② 최대 체력 −100 = 카탈로그 **`MaxHealth Flat −100`**(단점, 모디파이어 태그 비움) — 기존 분배로 칸 75 × 4 | Plan3 의 "총 HP 를 칸 수로 나눠 칸 용량에" 규칙 그대로. ★D-87 없이는 오염 최대 체력이 0 이 된다★ |
| **D-87** | ★선행 변경★ **오염 상태의 최대 체력은 칩과 무관하게 항상 100**(`CorruptMaxHealth`). 오염 진입 · 해제 · 정화의 `SetMaxHealth` 3곳을 새 헬퍼 `SetMaxHealthCurrentValue(목표)`(Base = 목표 − (현재 − Base))로 바꾸고, 오염 중에 `RefreshUpgradeEffects` 가 칩 GE 를 다시 붙이거나 떼도 끝에서 100 으로 다시 맞춘다(체력은 그대로) | 사용자 확정. `SetMaxHealth` 는 BaseValue 를 바꾸고 `GE_Upgrade_Stats`(AddBase)의 칩 가산이 그 위에 다시 붙는다 — 칩②면 오염 최대 **0**(다음 피격 즉사). 기존 칩도 틀려 있다(+100 칩 오염 200 · 정화 후 600 — Plan3 T-3 기대값 500, ⑧ 오염 900 · 정화 후 2000). 정화 · 해제 뒤는 지금 의도대로 칸 수 × 칸 용량 |
| **D-88** | ③ = **바로 위 1명**. 발동 = 피격 처리 **맨 끝**에서 그 탑승자 체력 ≤ 30% **또는** 이번 피격으로 오염 진입. 이미 30% 이하로 탔어도 다음 피격마다 발동. 체력 **비용**으로는 발동 안 함 | 사용자 확정. 1명만 보면 "이 청소기 20초 금지"와 정확히 맞물리고, 탑의 다른 청소기로 갈아타 회복을 반복하는 악용이 없다. 오염 진입은 체력을 오염 최대치로 다시 채워 비율로는 못 잡는다(T-79). 맨 끝이어야 그 피격의 공유분이 먼저 나간다(T-78). 비용 발동을 허용하면 자해로 회복을 뽑는다 |
| **D-89** | ③ 실행 = `EmergencyEjectRider()`: `DismountRider(true)`(기존 점프 하차) → 탑승자 · 청소기에 `ApplyHealToActor`(`GE_Heal`, 소스 = 청소기) → 탑승자에게 금지 GE | 먼저 내려야 회복 · 금지가 탑 상태와 무관하다. 회복 대상은 방금까지 내 위에 있던 아군과 나 자신이라 팀 판정이 필요 없다(T-33 의 "호출부 책임" 충족) |
| **D-90** | 20초 금지 = 탑승자에게 거는 시한 GE **`GE_RideBan`**(`Debuff.Ride.Banned`, 지속 SetByCaller, **Stacking None**), 소스 = 칩③ 청소기 ASC. 판정 `IsRideBannedFrom(탑승자, 청소기)` 하나를 서버(`CanBeMountedBy`)와 소유 클라(`FindMountByLineTrace` 프롬프트)가 같이 쓴다 | 진실이 하나다 — 만료 · 복제는 GAS 가 하고, `DA_StatusEffectInfo` 한 줄이면 20초 아이콘이 생긴다. 청소기 쪽 배열로 들면 복제 · 만료 · 사망 정리를 따로 해야 한다. ASC(PlayerState)로 비교해 청소기가 리스폰해도 금지가 이어진다 |
| **D-91** | 칩 판정은 라이브러리 함수로 — 어트리뷰트셋은 `FindRideGuard` · `TransferRideDamageToGuard` · `TryRideEmergencyEject` 셋, ExecCalc 는 `GetRideDamageMultiplier` 하나만 부른다. 청소기 자기 GE(감속) · 탈출 실행은 청소기 메서드, 공유 · 이전 GE 적용은 `ADRCharacter::ApplyMountSharedDamageTo` 하나 | D-23 과 같은 원칙 — 칩 지식이 어트리뷰트셋 · ExecCalc 로 새지 않게. 공유와 이전이 같은 함수를 지나 GE · 태그가 어긋날 수 없다 |
| **D-92** | 탑승 공유 범위 = **지금처럼 바로 위 · 아래 1칸**(옛 청소기 계획서 결정 3 그대로). 탑 전체로 넓히지 않는다 | 사용자 확정(2026-10-07 2차 정정 — 탑 전체 공유를 검토한 뒤 기각). ② 의 수치(3층 A② ← B ← C: C 피격 8 · B 피격 28 · A 피격 24)는 이 범위를 전제로 한다. 넓히는 방법은 T-92 |

> 기각: ① 공격력을 탑승자에게 거는 GE(스택 = 인원)로 — HUD 아이콘은 생기지만 걸고 지울 곳이 탑승 · 하차 · 사망 · 이탈 · 로드아웃마다 생긴다(D-78).
> 기각: ② 를 청소기 쪽에서 "먼저 키워 받고 + 탑승자 공유 생략"으로 — 숫자가 한 번에 뜨지만 직접 피격 · 공유 · 여러 층마다 분기가 갈라진다(§12.8-37).
> 기각: 탑 전체 공유를 "받은 방향 반대쪽으로 한 칸씩" 이어 전달(옛 계획서의 방향 플래그안) — 단계마다 받는 쪽 배율이 겹치고 칩② 이전과 얽힌다(D-92 · T-92).
> 기각: 오염 최대 체력을 `PostAttributeChange` 에서 매번 100 으로 고정 — 바깥 갱신이 끝난 뒤 옛 값으로 델리게이트가 한 번 더 방송돼(§12.0 엔진 사실) 호스트 HUD 가 틀린 값을 본다.
> 기각: ③ 금지를 청소기의 복제 배열로 — 만료 · 정리 · 복제를 직접 관리해야 한다(D-90).

### 12.4 C++ 변경 (적용 순서)

① · ③ 은 **선행 변경**(칩 없이도 동작이 바뀐다), ② · ④~⑫ 는 칩 3종을 위한 것이다(② 는 동작 불변).

**① ★선행★ `ExecCalc_Damage.cpp`** — 공유 · 이전 피해는 "주는 피해" 배율 제외 (D-82)

```cpp
	// ★Plan5(탑승칩)★ 탑승 공유 · 이전 피해(Damage.MountShared)는 ★받는 피해★ 다 — 넘겨주는 사람의
	// "주는 피해" 배율((a) DamageDealt · (b) 광폭화 · (c) 시너지 · (d) 탑승)을 전부 건너뛴다 (D-82 · T-77).
	// 넘어오는 양은 그 사람이 받은 최종 피해(받는 쪽 배율 · 데미지 디버프 반영)라 여기서 더 곱할 것이 없다.
	// (SourceAbilityTags 는 서버 전용 필드지만 ExecCalc 는 서버에서만 돈다)
	const bool bMountShared = UDRAbilitySystemLibrary::GetSourceAbilityTags(Spec.GetContext())
		.HasTagExact(FDRGameplayTags::Get().Damage_MountShared);

	if (SourceASC && !bMountShared)   // ← 기존 if (SourceASC)
	{
		... (a) DamageDealt / (b) 광폭화 / (c) 시너지 그대로 ...
		... (d) 는 ⑦ 에서 추가 ...
	}
```

- 포효 ×1.2 · Phase1 ×0.7 은 적 소스 전용이라 공유(소스 = 플레이어)에 원래 안 붙는다 — 손대지 않는다.

**② `DRCharacter.h / .cpp`** — 공유 GE 적용부 추출 (D-91 · **동작 불변**)

`PropagateSharedDamage` 의 대상(바로 위 · 아래 1칸 — D-92)과 재전파 방지는 그대로 두고, GE 를 거는 부분만 `ApplyMountSharedDamageTo` 로 뽑는다. ② 이전분도 이 함수를 지난다.
(탑 전체 공유 예시 코드는 2차 정정으로 기각돼 지웠다 — 넓히는 방법은 T-92.)

```cpp
// .h public — PropagateSharedDamage 바로 아래
	bool ApplyMountSharedDamageTo(ADRCharacter* Target, float Damage);

// .cpp PropagateSharedDamage — 1칸 대상 수집 · 재전파 방지는 그대로, 루프 본문만
	for (ADRCharacter* Linked : LinkedCharacters)
	{
		ApplyMountSharedDamageTo(Linked, Damage);
	}

bool ADRCharacter::ApplyMountSharedDamageTo(ADRCharacter* Target, float Damage)
{
	if (!HasAuthority() || !IsValid(Target) || Target->bDead) return false;
	if (Damage <= 0.f) return true;
	if (!MountSharedDamageEffectClass) { /* 경고 로그 */ return false; }

	UAbilitySystemComponent* SelfASC = GetAbilitySystemComponent();
	UAbilitySystemComponent* TargetASC = Target->GetAbilitySystemComponent();
	if (!SelfASC || !TargetASC) return false;

	// Source = 넘겨주는 사람 ASC. 주는 피해 배율은 ExecCalc 가 MountShared 를 보고 건너뛴다 (D-82)
	FDamageEffectParams Params;
	Params.WorldContextObject = this;
	Params.DamageGameplayEffectClass = MountSharedDamageEffectClass;
	Params.SourceAbilitySystemComponent = SelfASC;
	Params.TargetAbilitySystemComponent = TargetASC;
	Params.BaseDamage = Damage;
	Params.DamageType = FDRGameplayTags::Get().Damage_Physical;
	Params.SourceAbilityTags.AddTag(FDRGameplayTags::Get().Damage_MountShared);   // ★필수★ — 빠지면 무한 반복 (T-74)
	UDRAbilitySystemLibrary::ApplyDamageEffect(Params);
	return true;
}
```

**③ ★선행★ `DRPlayerAttributeSet.h / .cpp` + `DRCharacter.cpp`** — 오염 최대 체력 항상 100 (D-87)

```cpp
// DRPlayerAttributeSet.h public
	// ★Plan5(탑승칩)★ 오염 중이면 최대 체력을 CorruptMaxHealth(100)로 다시 맞추고 체력을 KeepHealth(≤ 100)로 되돌린다 (D-87).
	// ADRCharacter::RefreshUpgradeEffects 가 칩 GE 를 다시 붙이거나 뗀 뒤 부른다(서버)
	void ReapplyCorruptMaxHealth(float KeepHealth);

// DRPlayerAttributeSet.h private
	// ★Plan5(탑승칩)★ MaxHealth 의 ★현재값★을 Desired 로 맞춘다 (D-87)
	void SetMaxHealthCurrentValue(float Desired);

// DRPlayerAttributeSet.cpp
void UDRPlayerAttributeSet::SetMaxHealthCurrentValue(float Desired)
{
	// SetMaxHealth 는 BaseValue 를 바꾼다 — GE_Upgrade_Stats(Infinite, AddBase)의 칩 가산이 그 위에 다시 붙으므로
	// 그 몫(현재 − Base)을 빼고 넣는다. ★MaxHealth 에 곱 모디파이어가 없다는 전제★ (T-88)
	const UAbilitySystemComponent* ASC = GetOwningAbilitySystemComponent();
	const float ModifierOffset = ASC ? GetMaxHealth() - ASC->GetNumericAttributeBase(GetMaxHealthAttribute()) : 0.f;
	SetMaxHealth(Desired - ModifierOffset);
}

void UDRPlayerAttributeSet::ReapplyCorruptMaxHealth(float KeepHealth)
{
	if (!bCorrupted) return;
	SetMaxHealthCurrentValue(GetCorruptMaxHealth());
	SetHealth(FMath::Clamp(KeepHealth, 0.f, GetMaxHealth()));
}
```

- `EnterCorruptedState` → `SetMaxHealthCurrentValue(GetCorruptMaxHealth())`
- `ExitCorruptedState` · `HandleCorruptionPurification` → `SetMaxHealthCurrentValue(NormalMaxHealth)` (★세 곳 모두 — T-87★). 뒤의 `SetHealth` 는 그대로.
- 칩 없는 플레이어는 오프셋 0 → 지금과 완전히 같다.

`DRCharacter.cpp` `RefreshUpgradeEffects()` — `if (!HasAuthority()) return;` 바로 다음(`#include "Misc/ScopeExit.h"`):

```cpp
	// ★Plan5(탑승칩)★ 오염 중이면 아래 ★모든 return 경로의 끝★ 에서 최대 체력을 100 으로 다시 맞춘다 (D-87).
	// 칩 GE 를 떼고 붙이는 사이 비율 유지 로직이 체력을 바꾸므로 오염 체력은 시작 값으로 되돌린다 (T-88)
	UDRPlayerAttributeSet* CorruptAS = Cast<UDRPlayerAttributeSet>(AttributeSets);
	const bool bCorruptedAtStart = CorruptAS && CorruptAS->IsCorrupted();
	const float CorruptHealthAtStart = bCorruptedAtStart ? CorruptAS->GetHealth() : 0.f;
	ON_SCOPE_EXIT
	{
		if (bCorruptedAtStart)
		{
			CorruptAS->ReapplyCorruptMaxHealth(CorruptHealthAtStart);
		}
	};
```

**④ `DRGameplayTags.h / .cpp`** — 태그 6개(네이티브)

| 멤버 | 태그 | 종류 |
|---|---|---|
| `Abilities_RobotVacuum_Ride` | `Abilities.RobotVacuum.Ride` | ★칩 그룹 키 전용★ — GA 에 넣지 말 것(T-90) |
| `Upgrade_Feature_Skill_Ride_Power` | `Upgrade.Feature.Skill.Ride.Power` | ① 기능 스위치 |
| `Upgrade_Feature_Skill_Ride_Guard` | `Upgrade.Feature.Skill.Ride.Guard` | ② 기능 스위치 |
| `Upgrade_Feature_Skill_Ride_Eject` | `Upgrade.Feature.Skill.Ride.Eject` | ③ 기능 스위치 |
| `Debuff_RobotVacuum_RideLoad` | `Debuff.RobotVacuum.RideLoad` | ★GAS 태그★ — `GE_RideLoad` 가 부여, C++ 이 이 태그로 지운다 |
| `Debuff_Ride_Banned` | `Debuff.Ride.Banned` | ★GAS 태그★ — `GE_RideBan` 이 탑승자에게 부여(로봇 공용 이름), C++ 이 조회 |

**⑤ `DRProgressionConfig.h`** — `FDRAttackSpeedBuffChipTuning` 다음

```cpp
/**
 * ★Plan5(탑승칩)★ 로봇 청소기 탑승(패시브) 칩 3종의 동적 상수.
 * ②의 최대 체력 −100 은 여기가 아니라 DA_ChipCatalog 의 MaxHealth 모디파이어다.
 */
USTRUCT(BlueprintType)
struct FDRRideChipTuning
{
	GENERATED_BODY()

	// ========== ① RV.Asc.RidePower ==========

	// 맨 아래 청소기 위 인원 1명당 탑승자 전원의 주는 피해 증가 (0.08 = +8%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ride|Power", meta = (ClampMin = "0.0"))
	float PowerDamagePerRider = 0.08f;

	// 위 증가의 상한 (0.24 = +24%). 맨 아래 위로 최대 3명이라 기본값이면 딱 닿는다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ride|Power", meta = (ClampMin = "0.0"))
	float PowerMaxDamageBonus = 0.24f;

	// 인원 1명당 그 청소기의 이동 속도 감소 (0.05 = −5%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ride|Power", meta = (ClampMin = "0.0", ClampMax = "0.3"))
	float PowerSpeedPenaltyPerRider = 0.05f;

	/**
	 * 감속 GE. 요구 사양 (Plan5 D-80):
	 *   - Duration = Infinite / Stacking = None
	 *   - Modifier: MoveSpeed / ★Multiply (Compound)★ / SetByCaller(Data.MoveSpeed.Mult)
	 *   - Target Tags(Granted) = Debuff.RobotVacuum.RideLoad  ★코드가 이 태그로 지운다 — 없으면 감속이 쌓인다 (T-84)★
	 * ★비우면 경고 로그와 함께 감속 없이 공격력만 오른다★
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ride|Power")
	TSoftClassPtr<UGameplayEffect> PowerLoadEffect;

	// ========== ② RV.Asc.RideGuard ==========

	// 보호받는 사람이 최종적으로 받아야 할 피해 중 칩② 청소기로 넘어가는 비율 (0.2 = 80% 감소).
	// ★이전분에만★ 곱한다 — 칩② 청소기 자신이 받는 공유분은 그대로다 (Plan5 D-85 · T-93)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ride|Guard", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float GuardTransferRatio = 0.2f;

	// ========== ③ RV.Asc.RideEject ==========

	// 바로 위 탑승자의 체력 비율이 이 값 이하가 되면 탈출 (0.3 = 30%)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ride|Eject", meta = (ClampMin = "0.01", ClampMax = "1.0"))
	float EjectHealthRatio = 0.3f;

	// 탈출 때 탑승자 · 청소기가 각각 회복하는 체력(고정량)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ride|Eject", meta = (ClampMin = "0.0"))
	float EjectHealAmount = 100.f;

	// 회복 GE — GE_Heal 그대로. ★Claw · Coin 의 HealEffect 와 별개 필드 — 비우면 회복 없이 탈출 · 금지만★
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ride|Eject")
	TSoftClassPtr<UGameplayEffect> EjectHealEffect;

	// 탈출한 탑승자가 그 청소기에 다시 탈 수 없는 시간(초)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ride|Eject", meta = (ClampMin = "0.1"))
	float EjectBanDuration = 20.f;

	/**
	 * 탑승 금지 GE. 요구 사양 (Plan5 D-90):
	 *   - Has Duration / Duration = SetByCaller(Data.Duration) / 모디파이어 없음
	 *   - Target Tags(Granted) = Debuff.Ride.Banned  (HUD 아이콘을 원하면 Asset Tags 에도)
	 *   - ★Stacking = None★ (청소기마다 따로 — T-81)
	 * ★비우면 경고 로그와 함께 금지 없이 탈출 · 회복만★
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ride|Eject")
	TSoftClassPtr<UGameplayEffect> EjectBanEffect;
};
```

`FDRChipTuning` 끝에:

```cpp
	// ========== 로봇 청소기 탑승 칩 (Plan5(탑승칩)) ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ride")
	FDRRideChipTuning Ride;
```

**⑥ `DRAbilitySystemLibrary.h / .cpp`** — 칩 판정 5개(D-91). cpp 에 `#include "Character/DRRobotVacuumCharacter.h"`

```cpp
	// ========== 로봇 청소기 탑승 칩 (Plan5(탑승칩)) ==========

	/**
	 * ★Plan5(탑승칩)★ ① 이 캐릭터가 주는 피해의 탑승 배율. 칩이 없거나 탑승 중이 아니면 정확히 1.0.
	 *   탑의 ★맨 아래★ 청소기가 살아 있고 Ride.Power 면 1 + min(PowerDamagePerRider × 그 위 인원, PowerMaxDamageBonus)
	 * 위층 청소기의 칩①은 보지 않는다 (D-79). ExecCalc_Damage 에서만 부른다(서버, 공유 · 이전 피해 제외 — D-82).
	 */
	static float GetRideDamageMultiplier(const AActor* SourceAvatar);

	/** ★Plan5(탑승칩)★ ② 이 캐릭터 ★아래★ 에서 가장 가까운, 살아 있는 Ride.Guard 청소기. 있으면 "보호받는 사람"이다 (D-83) */
	static class ADRRobotVacuumCharacter* FindRideGuard(const class ADRCharacter* Rider);

	/**
	 * ★Plan5(탑승칩)★ ② 보호받는 Rider 가 최종적으로 받아야 할 피해 Damage 의 GuardTransferRatio(0.2)만큼을 Guard 에게 넘긴다(서버).
	 * 공유와 같은 ApplyMountSharedDamageTo(Damage.MountShared) 로 건다 (D-83).
	 * 못 넘기면(청소기 사망 · GE 미지정) false — 호출부는 탑승자가 그대로 받게 한다 (T-89)
	 */
	static bool TransferRideDamageToGuard(class ADRCharacter* Rider, class ADRRobotVacuumCharacter* Guard, float Damage);

	/**
	 * ★Plan5(탑승칩)★ ③ 피격 처리 ★맨 끝★ 에서 부른다(서버). 이 캐릭터가 Ride.Eject 청소기의 ★바로 위★ 탑승자이고
	 * 체력 ≤ EjectHealthRatio 이거나 이번 피격으로 오염에 들어갔으면 그 청소기가 비상 탈출시킨다 (D-88).
	 */
	static void TryRideEmergencyEject(class ADRCharacter* Rider, bool bEnteredCorruptionThisHit);

	/**
	 * ★Plan5(탑승칩)★ ③ Rider 가 Mount 에 탑승 금지 중인가 — 서버(CanBeMountedBy)와 소유 클라(프롬프트)가 같이 쓴다.
	 * 금지 GE 중 시전자 ASC 가 Mount 의 ASC 인 것이 있으면 true (D-90). SourceAbilityTags 는 클라에 없어 쓰지 않는다 (T-83).
	 */
	static bool IsRideBannedFrom(const AActor* Rider, const class ADRRobotVacuumCharacter* Mount);
```

```cpp
float UDRAbilitySystemLibrary::GetRideDamageMultiplier(const AActor* SourceAvatar)
{
	const ADRCharacter* Rider = Cast<ADRCharacter>(SourceAvatar);
	if (!Rider || !IsValid(Rider->MountedOn)) return 1.f;   // 빠른 탈출 — 거의 모든 피해가 여기서 끝난다

	const ADRRobotVacuumCharacter* Bottom = Rider->MountedOn->GetStackBottom();   // 맨 아래만 (D-79)
	if (!IsValid(Bottom) || Bottom->bDead) return 1.f;
	if (!HasUpgradeFeature(Bottom->GetAbilitySystemComponent(), FDRGameplayTags::Get().Upgrade_Feature_Skill_Ride_Power)) return 1.f;

	const FDRRideChipTuning& Tuning = GetChipTuning(SourceAvatar).Ride;
	const float Bonus = FMath::Min(Tuning.PowerDamagePerRider * Bottom->GetRiderCountAbove(), Tuning.PowerMaxDamageBonus);
	return 1.f + FMath::Max(0.f, Bonus);
}

ADRRobotVacuumCharacter* UDRAbilitySystemLibrary::FindRideGuard(const ADRCharacter* Rider)
{
	if (!Rider) return nullptr;
	const FGameplayTag& GuardTag = FDRGameplayTags::Get().Upgrade_Feature_Skill_Ride_Guard;

	// 아래로 내려가며 가장 가까운 칩② 청소기 ("위 전원" 보호 — D-83). 상한 8 은 순환 방어용(CanBeMountedBy 가 이미 막는다)
	int32 Steps = 0;
	for (ADRRobotVacuumCharacter* Below = Rider->MountedOn; IsValid(Below) && Steps++ < 8; Below = Below->MountedOn)
	{
		if (!Below->bDead && HasUpgradeFeature(Below->GetAbilitySystemComponent(), GuardTag))   // 죽은 청소기 건너뛰기 (T-89)
		{
			return Below;
		}
	}
	return nullptr;
}

bool UDRAbilitySystemLibrary::TransferRideDamageToGuard(ADRCharacter* Rider, ADRRobotVacuumCharacter* Guard, float Damage)
{
	if (!IsValid(Rider) || !IsValid(Guard) || Guard->bDead) return false;

	// 80% 감소는 ★이전분에만★ — 피격 자체의 1칸 공유는 호출부가 PropagateSharedDamage(L) 로 그대로 보낸다 (D-85 · T-93)
	const float Ratio = FMath::Clamp(GetChipTuning(Rider).Ride.GuardTransferRatio, 0.f, 1.f);
	return Rider->ApplyMountSharedDamageTo(Guard, Damage * Ratio);
}

void UDRAbilitySystemLibrary::TryRideEmergencyEject(ADRCharacter* Rider, bool bEnteredCorruptionThisHit)
{
	if (!IsValid(Rider) || !Rider->HasAuthority() || Rider->bDead) return;   // 사망 하차가 이미 일어났다

	ADRRobotVacuumCharacter* Mount = Rider->MountedOn;   // ★바로 위 1명만★ (D-88)
	if (!IsValid(Mount) || Mount->bDead) return;
	if (!HasUpgradeFeature(Mount->GetAbilitySystemComponent(), FDRGameplayTags::Get().Upgrade_Feature_Skill_Ride_Eject)) return;

	const UDRPlayerAttributeSet* RiderAS = GetPlayerAttributeSetFromASC(Rider->GetAbilitySystemComponent());
	if (!RiderAS) return;

	// 오염 진입은 체력이 오염 최대치로 다시 차서 비율로는 못 잡는다 → 따로 받는다 (T-79)
	const float Ratio = GetChipTuning(Rider).Ride.EjectHealthRatio;
	const bool bLow = bEnteredCorruptionThisHit
		|| (RiderAS->GetMaxHealth() > 0.f && RiderAS->GetHealth() <= RiderAS->GetMaxHealth() * Ratio);
	if (bLow)
	{
		Mount->EmergencyEjectRider();
	}
}

bool UDRAbilitySystemLibrary::IsRideBannedFrom(const AActor* Rider, const ADRRobotVacuumCharacter* Mount)
{
	const UAbilitySystemComponent* RiderASC = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Rider);
	const UAbilitySystemComponent* MountASC = Mount ? Mount->GetAbilitySystemComponent() : nullptr;
	if (!RiderASC || !MountASC) return false;

	const FGameplayTag& BanTag = FDRGameplayTags::Get().Debuff_Ride_Banned;
	if (!RiderASC->HasMatchingGameplayTag(BanTag)) return false;   // 빠른 탈출

	// ★OwningTags 쿼리★ — 금지 태그는 GE 의 부여 태그다 (T-82, Plan8 T-29 와 같은 이유)
	for (const FActiveGameplayEffectHandle& Handle :
		RiderASC->GetActiveEffects(FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(FGameplayTagContainer(BanTag))))
	{
		const FActiveGameplayEffect* Active = RiderASC->GetActiveGameplayEffect(Handle);
		// 시전자 ASC(복제됨)로 "어느 청소기"인지 가린다 (T-83)
		if (Active && Active->Spec.GetContext().GetOriginalInstigatorAbilitySystemComponent() == MountASC)
		{
			return true;
		}
	}
	return false;
}
```

**⑦ `ExecCalc_Damage.cpp`** — ① 에서 만든 `if (SourceASC && !bMountShared)` 블록 안, 시너지(c) 다음 · `if (!FMath::IsNearlyEqual(OutgoingMultiplier, 1.f))` 앞에

```cpp
		// (d) ★Plan5(탑승칩)★ ① 탑승 공격력 — 탑의 맨 아래 청소기가 칩①이면 위 인원 × 8% (상한 24%).
		//     시너지처럼 광폭화 상한 밖에서 곱한다 (D-78). 칩 · 탑승이 없으면 정확히 1.0.
		//     블록 자체가 공유 · 이전 피해를 건너뛰므로 여기서 따로 거를 것이 없다 (D-82)
		OutgoingMultiplier *= UDRAbilitySystemLibrary::GetRideDamageMultiplier(SourceAvatar);
```

**⑧ `DRCharacter.h / .cpp`** — 파생 클래스 훅(D-81)

```cpp
// .h protected
	// ★Plan5(탑승칩)★ RefreshUpgradeEffects 가 모든 머신에서 부른다. 청소기가 ① 감속 GE 를 다시 맞춘다 (D-81)
	virtual void OnUpgradeEffectsRefreshed() {}

// .cpp RefreshUpgradeEffects() — "1-2) 시너지 오라" 블록 바로 다음 (★아래 조기 return 들보다 앞★)
	// ===== 1-3) ★Plan5(탑승칩)★ 파생 클래스 훅 =====
	// ①·③은 모디파이어가 없어 Runtime.IsEmpty() 가 true 일 수 있다 — 시너지와 같은 이유로 조기 return 앞에 둔다
	OnUpgradeEffectsRefreshed();
```

- 같은 함수의 서버 구간 맨 앞에는 ③ 의 오염 재고정(`ON_SCOPE_EXIT`)이 들어간다.

**⑨ `DRRobotVacuumCharacter.h / .cpp`**

```cpp
// .h public — "탑승 (마운트 측)" 아래
	// ========== 탑승 칩 (Plan5(탑승칩)) ==========

	// 내 위로 쌓인 인원(RiderOnTop 사슬 길이). 맨 아래 기준 최대 3
	int32 GetRiderCountAbove() const;

	// MountedOn 을 끝까지 내려간 청소기 (내가 맨 아래면 나)
	const ADRRobotVacuumCharacter* GetStackBottom() const;

	// 서버: ① 감속 GE 를 ★태그로★ 지우고, 맨 아래 && 칩① && 인원 > 0 이면 다시 건다 (D-80). 몇 번 불러도 안전
	void RefreshRideLoad();

	// 서버: ③ 바로 위 탑승자를 비상 탈출시킨다 — 하차 → 둘 다 회복 → 이 청소기 탑승 금지 (D-89)
	void EmergencyEjectRider();

// .h protected
	virtual void OnUpgradeEffectsRefreshed() override;   // → RefreshRideLoad()

// .h private
	// 서버: 탑승 · 하차 직후 — Rider(청소기면) + 나부터 맨 아래까지 RefreshRideLoad (D-81 · T-85)
	void RefreshRideLoadsAfterLinkChange(ADRCharacter* Rider);
```

```cpp
void ADRRobotVacuumCharacter::RefreshRideLoad()
{
	if (!HasAuthority()) return;
	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;

	const FDRGameplayTags& Tags = FDRGameplayTags::Get();

	// ★핸들이 아니라 태그로 지운다★ — ASC 는 PlayerState 소유라 리스폰 · 클래스 변경 뒤에도 GE 가 남는다 (T-84)
	ASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(Tags.Debuff_RobotVacuum_RideLoad));

	if (bDead || MountedOn) return;   // ① 은 맨 아래(직접 달리는) 청소기만 (D-79)
	if (!UDRAbilitySystemLibrary::HasUpgradeFeature(ASC, Tags.Upgrade_Feature_Skill_Ride_Power)) return;

	const int32 Riders = GetRiderCountAbove();
	if (Riders <= 0) return;

	const FDRRideChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(this).Ride;
	UClass* EffectClass = Tuning.PowerLoadEffect.LoadSynchronous();
	if (!EffectClass)
	{
		UE_LOG(LogDR, Warning, TEXT("[RideLoad] ChipTuning.Ride.PowerLoadEffect 미지정 — 칩①이 감속 없이 동작"));
		return;
	}

	const float SpeedMult = FMath::Clamp(1.f - Tuning.PowerSpeedPenaltyPerRider * Riders, 0.05f, 1.f);
	FGameplayEffectContextHandle Context = ASC->MakeEffectContext();
	Context.AddSourceObject(this);
	const FGameplayEffectSpecHandle Spec = ASC->MakeOutgoingSpec(EffectClass, 1.f, Context);
	if (!Spec.IsValid()) return;
	Spec.Data->SetSetByCallerMagnitude(Tags.Data_MoveSpeed_Mult, SpeedMult);
	ASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	UE_LOG(LogDR, Log, TEXT("[RideLoad] %s 위 %d명 → 이속 ×%.2f"), *GetName(), Riders, SpeedMult);
}

void ADRRobotVacuumCharacter::RefreshRideLoadsAfterLinkChange(ADRCharacter* Rider)
{
	// 탑승한 청소기는 맨 아래가 아니게 됐고, 내린 청소기는 맨 아래가 됐다
	if (ADRRobotVacuumCharacter* RiderVacuum = Cast<ADRRobotVacuumCharacter>(Rider))
	{
		RiderVacuum->RefreshRideLoad();
	}
	// 인원이 바뀐 것은 나부터 맨 아래까지 전부다 (T-85)
	int32 Steps = 0;
	for (ADRRobotVacuumCharacter* V = this; IsValid(V) && Steps++ < 8; V = V->MountedOn)
	{
		V->RefreshRideLoad();
	}
}

void ADRRobotVacuumCharacter::EmergencyEjectRider()
{
	if (!HasAuthority() || bDead || !IsValid(RiderOnTop)) return;

	ADRCharacter* Rider = RiderOnTop;
	UAbilitySystemComponent* SelfASC = GetAbilitySystemComponent();
	const FDRRideChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(this).Ride;

	// ① 먼저 내린다 — 이후 회복 · 금지는 탑 상태와 무관해진다 (D-89). 감속 갱신도 DismountRider 가 한다
	DismountRider(/*bLaunchOff*/ true);

	// ② 둘 다 회복. 대상은 방금까지 내 위에 있던 아군과 나 자신이라 팀 판정이 필요 없다 (T-33)
	UDRAbilitySystemLibrary::ApplyHealToActor(Rider, SelfASC, Tuning.EjectHealEffect, Tuning.EjectHealAmount, this);
	UDRAbilitySystemLibrary::ApplyHealToActor(this, SelfASC, Tuning.EjectHealEffect, Tuning.EjectHealAmount, this);

	// ③ 이 청소기 탑승 금지 — 소스 = 내 ASC 로 "어느 청소기"인지 구분한다 (D-90)
	const FActiveGameplayEffectHandle Ban =
		UDRAbilitySystemLibrary::ApplyTimedTagEffect(Rider, SelfASC, Tuning.EjectBanEffect, Tuning.EjectBanDuration);

	UE_LOG(LogDR, Log, TEXT("[RideEject] %s → %s 비상 탈출 · 회복 %.0f · 금지 %.0f초%s"),
		*GetName(), *GetNameSafe(Rider), Tuning.EjectHealAmount, Tuning.EjectBanDuration,
		Ban.IsValid() ? TEXT("") : TEXT(" ★EjectBanEffect 미지정 — 금지 없음★"));
}

void ADRRobotVacuumCharacter::OnUpgradeEffectsRefreshed()
{
	RefreshRideLoad();   // 서버에서만 실제로 동작. 빙의 때 리스폰 잔류 감속도 지운다
}
```

- `GetRiderCountAbove()` / `GetStackBottom()`: `RiderOnTop` / `MountedOn` 사슬을 상한 8 로 따라간다(순환 방어 — `CanBeMountedBy` 가 이미 막는다).
- `MountRider()` 끝(`OnRep_RiderOnTop()` 앞): `RefreshRideLoadsAfterLinkChange(Rider);`
- `DismountRider()` 의 **두 return 경로 모두**: 무효 라이더 분기는 `RefreshRideLoadsAfterLinkChange(nullptr);`, 정상 분기는 `MountedOn = nullptr` 뒤 `RefreshRideLoadsAfterLinkChange(Rider);`
- `CanBeMountedBy()` 의 스턴 검사 다음:

```cpp
	// ★Plan5(탑승칩)★ ③ 비상 탈출 뒤 이 청소기 탑승 금지 (D-90) — 클라 프롬프트와 같은 함수
	if (UDRAbilitySystemLibrary::IsRideBannedFrom(Candidate, this)) return false;
```

**⑩ `DRPlayerAttributeSet.cpp` `HandleIncomingDamage`** — ② 관문 · ③ 훅. `#include "Character/DRRobotVacuumCharacter.h"`

```cpp
	float LocalIncomingDamage = GetIncomingDamage();
	SetIncomingDamage(0.f);
	if (LocalIncomingDamage <= 0.f) return;

	ADRCharacter* TargetDRCharacter = Cast<ADRCharacter>(Props.TargetAvatarActor);

	// ★Plan5(탑승칩)★ ② 아래에 칩② 청소기가 있으면 이 피해는 그 청소기가 줄여서 대신 받는다 (D-83 · D-84).
	// ★맨 위에서 먼저 구한다★ — 바로 아래 피격 큐를 건너뛰어야 한다 (T-76)
	ADRRobotVacuumCharacter* RideGuard = UDRAbilitySystemLibrary::FindRideGuard(TargetDRCharacter);

	if (Props.TargetASC && !RideGuard) { ... 피격 큐 그대로 ... }

	NotifyEnterCombat(Props);                       // 그대로 (D-84)
	... 적 AIC UpdateCombatTime 그대로 ...
	... 부품 ×1.5 — 안쪽 `if (ADRCharacter* TargetDRCharacter = …)` 선언을 바깥 변수로 바꾼다 (T-91) ...
	... DamageTaken / 시너지 그대로 ...      ← 여기까지가 "최종적으로 받아야 할 피해" L

	// ★Plan5(탑승칩)★ ② 보호받는 사람 — L 의 20% 를 칩② 청소기로 넘기고, 피격 자체의 1칸 공유는 L 그대로 보낸다(T-93).
	// 이 사람은 맞지 않았다. ★로비 클램프 · 오염 분기 전★ (T-75). 못 넘기면 그대로 받는다 (T-89)
	if (RideGuard && UDRAbilitySystemLibrary::TransferRideDamageToGuard(TargetDRCharacter, RideGuard, LocalIncomingDamage))
	{
		// ★디버프는 그대로 건다★ — 화상 DoT 같은 디버프 피해는 매 틱 이 관문을 다시 지나 20% 로 넘어간다 (D-84 · T-94)
		if (UDRAbilitySystemLibrary::IsSuccessfulDebuff(Props.EffectContextHandle))
		{
			Debuff(Props);
		}
		TargetDRCharacter->PropagateSharedDamage(LocalIncomingDamage, Props.EffectContextHandle);   // 공유로 들어온 피해면 안에서 멈춘다
		return;
	}

	... ShouldPreventDeath 클램프 그대로 ...

	const bool bWasCorrupted = bCorrupted;   // ★Plan5(탑승칩)★ ③ 이번 피격의 오염 진입 감지 (T-79)
	... Process / FloatingText / Debuff / PropagateSharedDamage 그대로 ...

	// ★Plan5(탑승칩)★ ③ 비상 탈출 — ★반드시 맨 끝★. 하차가 공유보다 먼저면 이번 공유분이 탑에 다 안 나간다 (T-78)
	UDRAbilitySystemLibrary::TryRideEmergencyEject(TargetDRCharacter, !bWasCorrupted && bCorrupted);
```

- 맨 끝의 기존 `if (ADRCharacter* MountLinkedCharacter = Cast<ADRCharacter>(Props.TargetAvatarActor))` 는 `TargetDRCharacter` 로 바꿔도 같다.

**⑪ `DRPlayerController.cpp` `FindMountByLineTrace()`** — 프롬프트 조건에 한 줄(D-90)

```cpp
			HitVacuum->RiderOnTop == nullptr &&
			// ★Plan5(탑승칩)★ ③ 이 청소기에서 비상 탈출한 뒤 20초는 프롬프트도 안 띄운다 (서버도 같은 함수로 거부)
			!UDRAbilitySystemLibrary::IsRideBannedFrom(DRCharacter, HitVacuum))
```

**⑫ `DRProgressionConfig.cpp`** — 공속칩 검사 다음, 칩이 카탈로그에 있을 때만(코드가 에셋보다 먼저 들어가는 기간 대비)

| 조건 | 검사 | 틀리면 |
|---|---|---|
| `IsFeatureGated(Ride.Power)` | `PowerLoadEffect` 비었나 / CDO `GetGrantedTags()` 에 `Debuff.RobotVacuum.RideLoad` / `DurationPolicy == Infinite` / `Modifiers` 에 MoveSpeed · **MultiplyCompound** 가 있나 | 감속 없음 / ★감속이 갱신마다 쌓임(T-84)★ / 감속이 사라짐 / 돌진 중 비율이 달라짐(T-86) |
| `IsFeatureGated(Ride.Eject)` | `EjectHealEffect` 비었나 / `EjectBanEffect` 비었나 · `Debuff.Ride.Banned` 부여 · **`StackingType != AggregateByTarget`** / `EjectBanDuration > 0` | 회복 없음 / 금지 없음 · 두 번째 청소기 금지가 첫 GE 에 합쳐짐(T-81) |
| (검사 없음) | `GuardTransferRatio` 는 meta 로 0~1 | 0 이면 보호받는 사람의 피해가 사라진다 · 1 이면 줄이지 않는다 — 둘 다 의도면 허용 |

**바뀔 C++ 파일**: `ExecCalc_Damage` · `DRCharacter`(.h/.cpp — 공유 GE 함수 추출 · 훅 · 오염 재고정) · `DRPlayerAttributeSet`(.h/.cpp) · `DRGameplayTags`(.h/.cpp) ·
`DRProgressionConfig`(.h 구조체 / .cpp 검사) · `DRAbilitySystemLibrary`(.h/.cpp) · `DRRobotVacuumCharacter`(.h/.cpp) · `DRPlayerController.cpp`.
**새 스탯 축 없음**(② 는 기존 `MaxHealth`) → `DRUpgradeTypes.h` · UI 짧은 이름 불변.

### 12.5 에디터 작업 (C++ 빌드 뒤)

1. **`GE_RideLoad`** — `Content/Blueprints/AbilitySystem/Player/VacuumCleaner/Effects/` 에 새 GameplayEffect BP
   - Duration Policy **Infinite** / Stacking **None**
   - Modifiers +1: Attribute `DRAttributeSet.MoveSpeed` · Modifier Op **Multiply (Compound)** · Magnitude **Set by Caller** · Data Tag `Data.MoveSpeed.Mult`
   - Components → **Target Tags**(Grant Tags to Target Actor) `Debuff.RobotVacuum.RideLoad` ★필수 — 코드가 이 태그로 지운다★ (HUD 아이콘을 원하면 Asset Tags 에도 — 선택)
2. **`GE_RideBan`** — 같은 폴더
   - Duration Policy **Has Duration** · Duration Magnitude **Set by Caller** `Data.Duration` / 모디파이어 없음
   - Target Tags `Debuff.Ride.Banned` (+ 선택: Asset Tags 같은 태그 — 20초 아이콘)
   - **Stacking None** (T-81)
3. **`DA_ProgressionConfig` → Chip Tuning → Ride**: `PowerLoadEffect = GE_RideLoad` / `EjectHealEffect = GE_Heal`(`Player/Effects/`) / `EjectBanEffect = GE_RideBan`. 숫자는 C++ 기본값 그대로(0.08 · 0.24 · 0.05 · **0.2** · 0.3 · 100 · 20).
4. **`DA_ChipCatalog`** → Chips +3 (§12.1 표). 공통: `OwnerClass RobotVacuum` · `bCommonChip` 끔 · `Category Ascension` · `TargetAbilityTag Abilities.RobotVacuum.Ride` · `RequiredSlotCount 1`
   - ① `RV.Asc.RidePower` · 합승 부스터 · 요약 "탑승 1명당 탑승자 공격력 +8%" · Tier 3 · Modifiers 없음 · GrantedTags `Upgrade.Feature.Skill.Ride.Power`
     · Description "이 청소기가 탑의 맨 아래에서 달릴 때, 위에 탄 사람 1명당 탑승자 전원의 공격력이 8% 오릅니다(최대 24%). 대신 1명당 이 청소기의 이동 속도가 5% 줄어듭니다. 다른 청소기 위에 업혀 있는 동안에는 동작하지 않습니다."
   - ② `RV.Asc.RideGuard` · 대신 맞는 범퍼 · 요약 "탑승자 피해를 줄여서 대신 받음" · Tier 4 · GrantedTags `…Ride.Guard`
     · Modifiers 1줄: Stat `MaxHealth` · **TargetAbilityTag 비움**(캐릭터 단위 — 넣으면 검사 오류) · Op `Flat` · Value **−100** · `bIsDrawback` ✓
     · Description "이 청소기보다 위에 탄 사람은 피해를 받지 않고, 그 사람이 받았어야 할 피해를 80% 줄여 이 청소기가 대신 받습니다(원래 함께 받던 피해에 더해집니다). 대신 최대 체력이 100 줄어듭니다."
   - ③ `RV.Asc.RideEject` · 비상 탈출 좌석 · 요약 "위험한 탑승자 탈출 + 체력 100" · Tier 3 · Modifiers 없음 · GrantedTags `…Ride.Eject`
     · Description "바로 위에 탄 사람이 피해를 받아 체력이 30% 이하가 되면 즉시 튕겨 내리고, 그 사람과 이 청소기 모두 체력을 100 회복합니다. 대신 탈출한 사람은 20초 동안 이 청소기에 다시 탈 수 없습니다."
   - `SkillDisplayNames` 에 `Abilities.RobotVacuum.Ride → 탑승`
5. (선택) `DA_StatusEffectInfo` — `Debuff.Ride.Banned`("탑승 금지", 남은 시간) · `Debuff.RobotVacuum.RideLoad`("탑승 하중"). ★그 GE 의 **Asset Tags** 에도 태그가 있어야 위젯에 온다(HUD 경로는 에셋 태그를 본다)★
6. (선택) 아이콘 `Content/DaeRuneAssets/UI/Upgrade/Icon/Vacuum/공격-6 · 지원-6 · 편의-6` (자판기가 -3~-5)
7. 검사: `DA_ProgressionConfig` 를 한 번 편집 → 출력 로그 `[ProgressionConfig] … 검증 통과` / 카탈로그 정합성 오류 0건

> C++ 구현 뒤 필요하면 공속칩 때처럼 "처음 보는 사람용" 단계별 안내서(§13)로 풀어 쓴다.

### 12.6 함정

| # | 함정 | 대응 |
|---|---|---|
| **T-74** | 공유 · 이전 피해를 `ApplyMountSharedDamageTo` 를 거치지 않고 따로 만든 GE 로 걸면서 `Damage.MountShared` 를 빠뜨린다 | 받은 쪽이 다시 공유 → 탑 안에서 핑퐁 → **무한 반복**(스택 오버플로) |
| **T-75** | ② 분기를 로비 클램프 · 오염 분기 **뒤**에 둔다 | 탑승자 체력이 넘길 양을 깎거나(로비 · 튜토리얼), 탑승자가 이미 맞았다. 받는 쪽 배율 직후 · 클램프 전 |
| **T-76** | 보호 여부를 ② 분기에서 처음 구한다 | 피격 큐가 함수 맨 위라 이미 나갔다 — 보호 여부를 맨 위에서 먼저 구한다 |
| **T-77** | ExecCalc 의 `MountShared` 제외(D-82)를 빠뜨리거나 일부 배율만 뺀다 | 공유 · 이전분이 넘겨주는 사람의 공격 배율로 커진다 — 광폭화 탑승자 20 피격 → 청소기 45~55, 칩① 탑승자 → ×1.24. 공유는 받는 피해라 (a)~(d) ★전부★ 빠져야 한다(사용자 확정) |
| **T-78** | ③ 판정을 `PropagateSharedDamage` **앞**에 둔다 | 하차가 먼저 일어나 그 피격의 공유분이 탑에 다 안 나간다 |
| **T-79** | ③ 을 체력 비율로만 판정 | 피해로 체력 0 → 오염 진입(체력이 오염 최대치로 다시 참)을 놓친다 — 이번 피격의 오염 진입을 따로 넘긴다 |
| **T-80** | ③ 을 Health 어트리뷰트 변경 델리게이트에 건다 | `SetHealth` 도중(공유 전)에 하차하고, 체력 비용에도 발동하고, 오염 진입 재충전에 덮인다 |
| **T-81** | 금지 GE 를 Aggregate by Target 으로 | 두 번째 청소기의 금지가 첫 GE 에 합쳐져 시전자가 첫 청소기로 남는다(두 번째에 탈 수 있다) — Stacking None |
| **T-82** | 금지 조회에 `GetActiveEffectsWithAllTags` | 에셋 태그만 비교해 못 찾는다(Plan8 T-29) — `MakeQuery_MatchAnyOwningTags` |
| **T-83** | 금지 판정에 컨텍스트 `SourceAbilityTags` 를 쓴다 | 복제되지 않아 클라 프롬프트가 틀린다 — 시전자 ASC(복제됨)로 비교 |
| **T-84** | 감속 GE 를 핸들로만 지운다 / GE 에 `Debuff.RobotVacuum.RideLoad` 태그가 없다 | 전자: 리스폰 · 클래스 변경 뒤 핸들을 잃어 감속이 남는다(ASC = PlayerState). 후자: ★태그로 못 지워 감속이 갱신마다 쌓인다★ — 검사가 잡는다 |
| **T-85** | 감속을 `RiderOnTop` 이 바뀐 청소기만 갱신 | 3층 탑에서 위층끼리 타고 내리면 맨 아래 인원이 그대로 — 마운트부터 맨 아래까지 + 탑승자 자신 |
| **T-86** | 감속 GE 의 ModOp 를 Multiply (Additive) 로 | 돌진 버프(MultiplyAdditive)와 더해져 돌진 중 −5% 가 다르게 먹는다 — Compound |
| **T-87** | 오염 최대 체력 보정(D-87)을 진입에만 | 해제 · 정화에서 다시 어긋난다 — 세 곳 모두 + `RefreshUpgradeEffects` 재고정 |
| **T-88** | D-87 은 MaxHealth 에 **곱 모디파이어가 없다**는 전제(`Data.Upgrade.MaxHealth.Mult` 는 항상 1.0) / 오염 중 `RefreshUpgradeEffects` 의 비율 유지 | 곱을 쓰기 시작하면 다시 볼 것. 오염 중 칩 GE 를 뗀 순간 최대 체력이 100 이 아니라 비율 유지 로직이 체력을 바꾼다(⑧이면 Base 가 음수로 보이기도 한다) → 시작 체력을 저장했다가 `ON_SCOPE_EXIT` 에서 되돌린다 |
| **T-89** | 이전 실패(청소기 사망 직후 · GE 미지정)에도 탑승자 처리를 건너뛴다 | 피해가 **사라진다**(무적) — `TransferRideDamageToGuard` 가 false 면 그대로 받는다. `FindRideGuard` 도 죽은 청소기를 건너뛴다 |
| **T-90** | `Abilities.RobotVacuum.Ride` 를 청소기 GA 의 `AbilityTags` 에 넣는다 | 업그레이드 키(첫 `Abilities.*` 태그)가 흔들린다(T-51 과 같다) |
| **T-91** | `HandleIncomingDamage` 맨 위에 `TargetDRCharacter` 를 선언하고 부품 블록의 같은 이름 선언을 남긴다 | 그림자 변수(C4456) — UE 빌드는 오류. 안쪽을 `if (TargetDRCharacter && TargetDRCharacter->IsCarryingPart())` 로 |
| **T-92** | 공유 범위를 탑 전체로 넓힌다 | 사용자 확정은 1칸(D-92) — 넓히면 ② 수치가 모두 바뀐다(3층 C 피격 A 8 → 28). 넓힐 때는 받은 쪽이 한 칸씩 이어 전달하지 말고 피격자가 탑 전원을 모아 각자에게 1번씩 건다(배율이 겹치지 않게) |
| **T-93** | 80% 감소를 피격 자체의 공유에까지 건다 | 칩② 청소기 자신의 몫까지 줄어 지금보다 덜 맞는다(2층 24 → 8) — 감소는 이전분에만, 공유는 `PropagateSharedDamage(L)` 그대로 |
| **T-94** | 보호받는 사람 분기에서 `Debuff(Props)` 를 빠뜨린다 | 디버프가 안 걸려 화상 DoT 같은 "디버프 피해"가 통째로 사라진다(사용자 확정 위반) — ② 분기에서도 `IsSuccessfulDebuff` 면 `Debuff(Props)` |

### 12.7 검증

준비: PIE **Listen Server · 플레이어 4**(청소기 2~3명 — 3층 탑용). 콘솔 `DRUnlockUpgrade` → `DRUnlockSlot 6`(§10.2 와 같다).
수치는 `showdebug abilitysystem`(MaxHealth · MoveSpeed · 활성 GE), 서버 로그 `[RideLoad]` · `[RideEject]`.

- [ ] **회귀 — 공유 범위(칩 없음)** 지금과 같다 — 3층 A ← B ← C 에서 C 가 맞으면 B 만 20, A 0 (D-92)
- [ ] **선행 변경 — 공유 배율(칩 없음)** 광폭화(⑥) 탑승자 20 피격 → 청소기 **20**(지금 45~55) / 시너지 탑승자도 받은 만큼만 / 받는 쪽 디버프(부품 ×1.5 등)가 붙은 피해는 그 값 그대로 공유
- [ ] **선행 변경 — 오염** 칩 없음: 오염 100 → 정화 후 400 / +100 칩: 오염 **100**(예전 200) → 정화 후 **500**(예전 600) / ⑧: 오염 **100**(예전 900) → 정화 후 **1200**(예전 2000) / 칩②: 오염 **100**(수정 전 0) → 정화 후 **300**
- [ ] **①** 맨 아래 칩① 청소기 위 1 · 2 · 3명 → 탑승자 데미지 숫자 ×1.08 · 1.16 · 1.24 / 청소기 MoveSpeed 522.5 · 495 · 467.5 / 하차 · 사망 · 위층끼리 하차에 즉시 갱신 / 돌진 중에도 감속 / 위층 청소기의 칩①은 효과 없음 → 그 청소기가 내려 맨 아래가 되면 켜짐 / 청소기 자신의 공격은 그대로 / 리스폰 뒤 감속 잔류 없음 / ★칩① 탑승자가 맞아도 공유분은 ×1.24 가 아니다★
- [ ] **②** 2층: 탑승자 20 피격 → 탑승자 체력 · 숫자 · 히트리액트 · 넉백 · 디버프 없음, 청소기 **20 + 4**(숫자 2개) / 청소기 20 피격 → 청소기 24, 탑승자 0 / 3층 A② ← B ← C: C 피격 **A 8** · B 피격 **A 28** · A 피격 **A 24**, B · C 0 / 화상 걸린 보호 탑승자: 체력 불변 · 틱마다 청소기에 공유분 + 20% / 광폭화 탑승자 → 청소기 24(칸 아님) / 가운데 B② · 칩② 두 대 · 4층이 §12.1 표대로 / 돌진 자해 50 → 청소기 60 / 최대 체력 300 · 칸 75 / 모자 쓴 탑승자 → 모자만 소모, 아무도 피해 없음 / 칩② 청소기 사망 → 탑승자 하차 후 정상 피격 / `GuardTransferRatio` 를 0.5 로 바꾸면 2층 24 → 30
- [ ] **③** 30% 초과 → 이하로 맞으면 즉시 튕겨 내림 · 둘 다 +100 / 30% 이하로 탄 뒤 맞아도 발동 / 아래층 피격의 공유로 30% 이하가 돼도 발동 / 피해로 오염 진입 → 탈출 + 정화 / 20초 동안 그 청소기 F 프롬프트 없음 · 서버 거부, 다른 청소기는 탈 수 있음 / 20초 뒤 다시 탈 수 있음 / ⑧ 체력 비용 · 발톱 수혈로는 발동 안 함 / 회복 차단(⑥) 대상은 탈출 · 금지만 / 3층 탑의 맨 위 사람(바로 위가 아님)은 감시 안 함
- [ ] **스킬당 1칩** ①②③ 중 하나만, 공용 칩과는 함께 / 미리보기 라벨 "탑승" / ② 카드에 "최대 체력 −100" 빨강
- [ ] **멀티** 클라 청소기 ① → 호스트 화면에서도 느려지고 끊김 · 되돌림 없음 / 클라 탑승자의 ③ 금지 → 클라 화면 프롬프트 사라짐 · 20초 뒤 다시 / 클라 ② 탑승자 체력바 불변 / 클라 청소기 ② 의 오염 최대 체력 100(클라 HUD 도)
- [ ] **검사** `DA_ProgressionConfig` 편집 → 오류 0건 / `GE_RideLoad` 의 태그를 빼면 검사 오류(T-84) / `GE_RideBan` 을 by Target 으로 바꾸면 검사 오류(T-81)

### 12.8 열린 결정 · 알려진 제약 (§9 번호에 이어서)

| # | 내용 | 현재 판단 |
|---|---|---|
| **36** | 이름 · Tier · 아이콘 · `ChipId`(`RV.Asc.*`)는 가칭 | 사용자가 정하면 카탈로그만(`ChipId` 는 첫 입력 전에 확정 — 세이브 키) |
| **37** | ② 청소기에 피해가 **여러 번**(자기 공유분 + 보호받는 사람마다 이전분 — 2층이면 20 · 4) 들어온다 — 숫자 여러 개, 광폭화면 그만큼 칸, 컨테이너 넘침 규칙도 그만큼 | 그대로 둔다. 한 번으로 합치려면 청소기 관문에서 "먼저 키워 받고 + 위 사람 공유 생략" 분기가 필요하다(기각안 — 범위가 커진다) |
| **38** | ① 탑승자 공격력이 HUD 에 안 보인다(상태 없는 계산) | 원하면 탑승자에게 표시 전용 GE(스택 = 인원) — 걸고 지울 곳이 늘어난다(D-78) |
| **39** | ① 위층 청소기의 칩①이 "쉬는 중"임을 알려 주지 않는다 | `Description` 문구로만 안내 |
| **40** | ② 보호받는 사람에게도 디버프는 그대로 걸린다 — 화상 등 DoT 피해는 20% 로 넘어간다(**사용자 확정**). 스턴 · 둔화처럼 피해 없는 디버프도 탑승자가 받는다(설계 판단) | 피해 없는 디버프까지 막으려면 ② 분기에서 DoT 디버프만 골라 건다 |
| **41** | ③ 탈출 연출은 기존 점프 하차(위 300 · 앞 200) | 더 세게 튕기려면 튜닝 필드 추가 |
| **42** | ★선행 변경 2건이 칩 없이도 체감을 바꾼다★ — D-82: 광폭화 · 시너지 탑승자의 공유분이 받은 만큼으로(45~55 → 20) / D-87: +100 칩 · ⑧ 의 오염 · 정화 후 최대 체력(⑧ 오염 900 → 100, 정화 후 2000 → 1200). 공유 범위는 그대로 1칸(D-92) | 사용자 확정. 패치 노트 · 플레이 확인 |
| **43** | (관찰 · 범위 밖) Stage2 부활(`Revive`)은 오염을 풀지 않는다 — 오염 중 사망 → 부활해도 최대 체력은 오염값 | D-87 뒤에는 칩과 무관하게 100. 지금은 ⑧ 이면 900 이었다 |
| **44** | ③ 금지 중 프롬프트를 "N초 후 탑승 가능"으로 바꾸지 않는다(그냥 안 뜬다) | 원하면 `GetActiveEffectsTimeRemaining` 으로 위젯 작업 |
| **45** | ~~칩① 배율을 공유분에서 뺄지 — 설계 판단~~ | **해결**(2026-10-07) — 사용자 확정: 공유 · 이전은 받는 피해라 주는 피해 배율은 칩①을 포함해 전부 빠진다(D-82) |
| **46** | 칩② 청소기 두 대가 겹치면 위쪽 칩② 청소기를 거치며 이전분에 0.2 가 두 번 곱해진다(0.04) — B 가 맞으면 A 24.8 | 그대로 둔다 — 겹칠수록 보호가 강해진다. 한 번만 줄이려면 이전분(MountShared)을 받은 칩② 청소기는 다시 줄이지 않게 분기한다 |
| **47** | ~~광폭화 탑승자의 "최종 피해"를 칸으로 볼지 수치로 볼지~~ | **해결**(2026-10-07) — 사용자 확정: 칸이 아니라 피해 수치 L |

### 12.9 작업 순서

1. 선행 변경 ① · ③(ExecCalc 공유 배율 제외 · 오염 최대 체력 100) + ②(동작 불변 추출) → 빌드 → §12.7 회귀 · "선행 변경" 항목(칩 없이 확인 — 오염은 +100 칩 · ⑧ 로)
2. 칩 C++ ④~⑫ → 빌드 (태그 · USTRUCT 추가라 **에디터를 닫고** — T-45)
3. 에디터 §12.5
4. §12.7 검증 → §1 · §3 · §5 · §6 · §9 에 흡수(§1.7 그룹 6 → 7개, 머리말 칩 수 18 → 21), 머리말 상태 갱신

### 12.10 구현 결과 — 계획과 달라진 점

2026-10-08 C++ 구현 · 빌드 통과. 계획(§12.4 ①~⑫)대로 넣었고, 다른 점은 아래뿐이다. 에디터 작업 · 게임 검증은 아직이다.

- ② 공유 GE 함수 추출은 **1칸 공유 그대로**(동작 불변). §12.4 ② 에 남아 있던 "탑 전체" 예시 코드는 지웠다.
- `RefreshRideLoad` 의 지역 변수 이름은 `DRTags` — `Tags` 는 `AActor::Tags` 를 가려 C4458 빌드 오류가 난다.
- `HandleIncomingDamage` 의 `TargetDRCharacter` 는 `Props.TargetAvatarActor` 에서 구한다(부품 블록이 쓰던 `Props.TargetCharacter` 와 같은 액터). 맨 끝 공유 호출도 이 변수를 쓴다.
- ③ 검사(⑫)는 표대로 — 회복 GE 는 "비었나"만, 금지 GE 는 태그 · Stacking 까지 본다.

---

## 13. 로봇 청소기 기본 공격(공기탄) 칩 3종 (C++ 구현 완료 — 에디터 · 검증 남음)

> 2026-10-07 요청 · 같은 날 질의응답으로 확정. 코드 주석 접두 **`Plan5(공기탄칩)`** · 결정 **D-93 ~ D-101** · 함정 **T-95 ~ T-107** (앞 번호에 이어서 매긴 영구 ID).
> **C++ 구현 완료**(2026-10-08 빌드 통과). 에디터 작업은 **§16 안내서**, 그다음 §13.7 검증. 계획과 달라진 점은 §13.10. 검증이 끝나면 §1~§9 에 흡수한다.
> ★선행 변경 1건 — **D-101**(공기탄 단계 피해 10 / 15 / 22 / 35 → **10 / 30 / 50 / 70**) — 은 칩이 없어도 공기탄을 바꾼다(§13.8-49).★
>
> 사용자 확정
> - ① 기본 공격 피해 **+50%**. 대신 **아군에게도 같은 피해**를 입힌다. 탑승으로 직접 붙어 있는 사람(내 위 탑승자 · 내가 탄 청소기)은 지금처럼 통과한다.
> - ② 기본 공격에 맞은 적: 이속 **−10%** · 받는 피해 **+5%**(2초, 적중마다 갱신), **최대 3중첩**(−30% / +15%), **적 한 마리당 합쳐서 3**(청소기 여러 대가 같이 쌓는다).
>   **보스에게는 감속 · 피해 증가 둘 다 없다**. 대신 기본 공격 피해 **−20%**.
> - ③ 차지 단계 간격 **0.5 → 0.35초**. 대신 단계당 피해 증가 **+20 → +12**.
>   단계 피해 기본값은 지금(10 / 15 / 22 / 35)이 아니라 **10 / 30 / 50 / 70(+20/단계)으로 바꾼다** → 칩③이면 **10 / 22 / 34 / 46**.

### 13.0 지금의 공기탄 (코드·에셋 실측)

| 항목 | 지금 | 어디 |
|---|---|---|
| 스킬 | `GA_VacuumAirShot`(LMB) — `UDRVacuumAirShot : UDRProjectileSpell`. `AbilityTags = Abilities.RobotVacuum.AirShot`(업그레이드 키), 물 소모 · 쿨다운 없음 | GA BP CDO |
| 차지 | `StartCharging` 이 시각을 잡고 `GetChargeStage = floor(홀드 ÷ EffectiveChargeInterval)`(0~3). `EffectiveChargeInterval = ChargeInterval(0.5) ÷ 공속 배율 M`(Plan5 D-69), 하한 0.05초. UI 타이머도 같은 값 | `DRVacuumAirShot.cpp` |
| 단계 피해 | `Stages[]` = **10 / 15 / 22 / 35**, 탄속 1500 / 1800 / 2200 / 2600 — ★BP 가 `Stages` · `ChargeInterval` 을 덮지 않는다(에셋 확인) → C++ 생성자 값이 곧 게임 값★ | `UDRVacuumAirShot()` |
| 피해 주입 | `FireShot`(서버): `BaseDamage = GetUpgradedFloat(SkillDamage, Stages[단계].Damage)` — `SkillDamage` 칩은 이미 읽힌다. 3단계 = 강화탄(넉백 100% · 1000, 시전자 반동, 몽타주) | 〃 |
| 발사체 | `BP_VacuumAirProjectile`(`ADRVacuumAirProjectile`) — 2초 뒤 페이드. `bEnhanced`(복제)는 `SpawnProjectile` ★뒤에★ 넣는다(`FinishSpawning` 은 `SpawnProjectile` 안) | `DRVacuumAirProjectile.cpp` |
| 아군 처리 | ① 탑승 연결(내 위 탑승자 · 내가 탄 청소기)은 통과 ② 강화탄 + 아군 = 피해 없이 넉백만 ③ 그 외 아군은 부모 `ADRProjectile::OnSphereOverlap` 이 `IsNotFriend` 로 걸러 **그냥 통과** | 〃 · `DRProjectile.cpp` |
| 적 피해 관문 | `UDREnemyAttributeSet::HandleIncomingDamage`: 엘리트 배율 → **취약 ×1.5** → 흡혈 → 적중 회복 → 더미 → `SetHealth` | `DREnemyAttributeSet.cpp` |
| 보스 | `ADREnemy::bIsBoss`(원래 "처치 시 전체 물 지급" 플래그) — `BP_ElteBear`(Stage1 Phase3 엘리트 보스) **True**, `ADRS2MoleBoss`(Stage2 두더지 보스) 생성자 **True**. 일반 엘리트(`Buff.Elite`)는 False | 에셋 · `DRS2MoleBoss.cpp` |
| 적 감속 선례 | 씨앗 · 물대포 칩 감속 = `ApplyTimedSpeedEffect` + `FDRTimedSpeedEffect`(GE · 배율 · 지속, SetByCaller `Data.MoveSpeed.Mult` · `Data.Duration`), GE 는 **Multiply(Compound) · by Source · Limit 1** | Plan5 §5.2 · §5.3 |
| 청소기 칩 | 탑승칩 3종(§12, 계획) 뿐 — 기본 공격 그룹은 새로 생긴다 | |

**이 설계가 기대는 엔진·코드 사실**

| 항목 | 사실 |
|---|---|
| ★스택 모디파이어 크기★ | UE 5.5 `GameplayEffectUtilities::ComputeStackedModifierMagnitude` = (값 − 바이어스) × 스택 + 바이어스. 바이어스 `{AddBase 0, MultiplyAdditive 1, DivideAdditive 1, Override 0, **MultiplyCompound 0**, AddFinal 0}` → 0.9 를 3스택 하면 **Additive 0.7 / Compound 2.7(가속!)** |
| 스택 수 읽기 | Aggregate by Target 이면 GE 인스턴스 하나 — `GetEffectStackCountWithTag`(Plan5 D-75)로 읽는다. 태그 카운트는 1(T-3) |
| 발사체 스폰 순서 | `SpawnActorDeferred` → `FinishSpawning`(초기 오버랩 판정) → 그 뒤에 `FireShot` 이 `bEnhanced` 등을 넣는다 — 스폰 순간 겹친 대상은 플래그를 모른다 |
| 디버프 DoT | `IncomingDamage` 에 직접 꽂혀 `ExecCalc` 를 안 탄다 — "받는 모든 피해" 배율은 적 관문에서 곱해야 DoT 까지 덮는다(Plan5 D-10) |
| 아군 판정 | `IsNotFriend(A, B)` 가 false = 둘 다 `Player` 태그(또는 둘 다 `Enemy`) — 클렌저 사이트는 "적"으로 나온다(D-3) |

### 13.1 칩 3종 스펙

공통: `OwnerClass = RobotVacuum`, `Category = Ascension`, `RequiredSlotCount = 1`,
칩 레벨 `TargetAbilityTag = Abilities.RobotVacuum.AirShot`(실제 GA 태그 — D-93). → ①·②·③ 중 하나만, **탑승칩(§12)과는 함께** 낄 수 있다.

| # | `ChipId` | 이름(가칭) | Modifiers | GrantedTags | 효과 | 단점 | Tier | 아이콘(제안) |
|---|---|---|---|---|---|---|---|---|
| ① | `RV.Asc.ShotPower` | 고압 공기탄 | `SkillDamage` **+0.5** | `…AirShot.FriendlyFire` | 기본 공격 피해 **×1.5** | **아군에게도 같은 피해**(탑승 연결은 통과) | 3 | 공격-7 |
| ② | `RV.Asc.ShotDrag` | 흡착 공기탄 | `SkillDamage` **−0.2** ★단점★ | `…AirShot.Drag` | 맞은 적 1스택: 이속 **−10%** · 받는 피해 **+5%**, 2초(적중마다 갱신), 최대 **3**(적 한 마리당 합산). **보스 제외** | 기본 공격 피해 ×0.8 | 4 | 지원-7 |
| ③ | `RV.Asc.ShotQuick` | 속사 압축기 | `SkillWindup` **−0.3**<br>`SkillChargeGrowth` **−0.4** ★단점★ | (없음) | 차지 단계 간격 **0.5 → 0.35초**(×0.7) | 단계당 피해 증가 **+20 → +12**(×0.6) | 3 | 편의-7 |

`…` = `Upgrade.Feature.Skill`. ①은 단점이 규칙이라 `Description` 필수(검사가 강제 — D-1).
③ 카드 표기: `DetailOverride` — `SkillWindup` 줄 "차지 간격 0.5 → 0.35초", `SkillChargeGrowth` 줄 "단계당 피해 +20 → +12".
이름 · Tier · 아이콘 · `ChipId` 는 가칭(§13.8-48).

**단계 피해** (탄 1발, 공용 칩 · 버프 없음)

| | 0단계 | 1단계 | 2단계 | 3단계(강화탄) | 풀차지 시간 |
|---|---|---|---|---|---|
| 지금(선행 변경 전) | 10 | 15 | 22 | 35 | 1.5초 |
| **칩 없음(D-101 뒤)** | 10 | 30 | 50 | 70 | 1.5초 |
| ① ×1.5 | 15 | 45 | 75 | 105 | 1.5초 |
| ② ×0.8 | 8 | 24 | 40 | 56 | 1.5초 |
| ③ +12/단계 · 0.35초 | 10 | 22 | 34 | 46 | **1.05초** |

**단계까지 홀드 시간 기준 초당 피해** (뗀 뒤 다시 누르는 시간 제외 — 상대 비교용)

| | 1단계 | 2단계 | 3단계 |
|---|---|---|---|
| 칩 없음 | 30 / 0.5 = **60** | 50 / 1.0 = **50** | 70 / 1.5 = **46.7** |
| ① | 90 | 75 | 70 |
| ② | 48 (+ 아군 전원 받는 피해 최대 +15%) | 40 | 37.3 |
| ③ | 22 / 0.35 = **62.9** | 34 / 0.7 = **48.6** | 46 / 1.05 = **43.8** |

- ③ 은 초당 피해가 칩 없음과 거의 같거나 조금 낮다(+5% / −3% / −6%) — 대신 **강화탄(넉백 · 로켓 점프)까지 30% 빨리** 간다(§13.8-50).
- ② 3스택 적에게 ② 청소기 자신의 풀차지 = 56 × 1.15 = **64.4**, 아군의 모든 피해 +15%.

**② 중첩 타임라인** (적 1마리, 같은 청소기 또는 여러 청소기)

| 시각 | 0초 적중 | 0.5초 적중 | 1.0초 적중 | 2.0초 적중 | (마지막 적중 + 2초) |
|---|---|---|---|---|---|
| 스택 | 1 | 2 | 3 | 3(갱신) | 0 (한꺼번에) |
| 이속 | ×0.9 | ×0.8 | ×0.7 | ×0.7 | ×1.0 |
| 이 적중의 받는 피해 | ×1.0 | ×1.05 | ×1.10 | ×1.15 | — |

- 받는 피해 배율은 **그 적중 직전 스택**으로 계산하고, 스택은 그 뒤에 쌓는다(D-97).

### 13.2 상호작용

| 조합 | 결과 |
|---|---|
| ① + 공용 칩(광폭화 · 시너지 · `DamageDealt`) · 탑승칩① | 적에게 주는 피해처럼 **아군 피해에도** 곱해진다(ExecCalc 소스측 — "같은 피해"). 광폭화 청소기의 아군 피해 최대 ×3 |
| ① + 강화탄 | 아군에게 **피해 + 넉백**(지금의 "넉백만" 분기 대신 적과 같은 경로) |
| ① 아군 피격 → 그 아군의 받는 쪽 효과 | 아군의 `DamageTaken` · 시너지 · 모자(Plan8 — 1회 무효) · 오염 · 탑승 공유 · 탑승칩②③이 평소처럼 동작한다 |
| ① + 로비 · 튜토리얼 | 아군도 체력 1 아래로 안 내려간다(`ShouldPreventDeath`) |
| ① + 클렌저 사이트 | 지금과 같다(사이트는 "적"으로 판정 — 기존 동작) |
| ② + 물대포 ② 취약(×1.5) | 둘 다 곱한다 — 3스택이면 ×1.5 × 1.15 = **×1.725** |
| ② + 씨앗 · 물대포 감속(Compound) | 따로 곱해진다 — 물대포 ②(×0.7) + ② 3스택(×0.7) = ×0.49 |
| ② + 비행 적(잠자리 등) | 감속은 안 걸리고(§9-1) **받는 피해 증가만** 걸린다 |
| ② + 보스(`bIsBoss`) | 스택 자체를 안 건다 — 감속 · 피해 증가 둘 다 없음. 일반 엘리트(`Buff.Elite`)는 걸린다 |
| ② + 디버프 DoT · 다른 사람의 피해 | 받는 피해 +5%/스택은 **누가 · 무엇으로 때리든** 붙는다(적 관문 — D-98) |
| ② + 튜토리얼 더미 | 걸린다(연습용) |
| ③ + 자판기 공속 ② 공유 버프 | 간격 = 0.5 × 0.7 ÷ M — 5중첩(M 1.5)이면 **0.233초** |
| ③ + 발톱처럼 `SkillWindup` 을 읽는 다른 스킬 | 없음 — 모디파이어 태그가 `AirShot` 이라 공기탄에만 걸린다(T-12) |
| ①②③ + 탑승칩 ①②③ | 다른 그룹이라 하나씩 함께 낄 수 있다 |

### 13.3 설계 결정

| # | 결정 | 근거 |
|---|---|---|
| **D-93** | 그룹 키 = 실제 GA 태그 **`Abilities.RobotVacuum.AirShot`**(동전칩 `BasicAttack` 과 같은 방식). 기능 스위치 `Upgrade.Feature.Skill.AirShot.{FriendlyFire,Drag}` | GA 의 `AbilityTags` 가 이미 이 태그 하나라 업그레이드 키와 그룹 키가 일치한다. 탑승칩(`…Ride`)과 다른 그룹이라 함께 낄 수 있다 |
| **D-94** | ① 피해 +50% = 카탈로그 `SkillDamage +0.5`(레시피 유형 1 — `FireShot` 이 이미 읽는다). 아군 피해 = 기능 스위치를 **스폰 때 1회** 해석해 발사체 `bHitsAllies`(복제)에 싣고, `ADRVacuumAirProjectile::OnSphereOverlap` 의 아군 분기에서 적과 같은 피해를 건다. 피해 적용부는 `ADRProjectile::ApplyImpactDamage(Target)`(팀 판정 없는 범용 함수)로 뽑아 부모 · 자식이 같이 쓴다 | 발사체는 비행 중 로드아웃이 바뀔 일이 없다(D-5). `ADRProjectile` 은 적 투사체와 공유라 칩 지식을 넣지 않는다(§8) — 청소기 전용 자식 클래스에만. 탑승 연결 통과는 그대로(사용자 확정 — 총구 근처 오폭 방지) |
| **D-95** | ① 아군 피해 = **적에게 주는 것과 같은 피해**(ExecCalc 소스측 배율 포함), 받는 쪽은 아군 자신의 효과(DamageTaken · 오염 · 모자 · 탑승 공유 …). 강화탄이면 피해 + 넉백(일반 피해 경로의 넉백) | "동일한 피해"(사용자 원문). 적 전용 배율(엘리트 · 취약 · 흡혈)은 적 어트리뷰트셋에만 있어 아군에겐 원래 없다 |
| **D-96** | ② 표식 = 스택형 GE **`GE_AirShot_Drag`**: Has Duration(SetByCaller `Data.Duration` 2초) · **Aggregate by Target · Limit 3 · Refresh on Successful Application · Clear Entire Stack · Deny Overflow 끔** · MoveSpeed **Multiply (Additive)** ← SetByCaller `Data.MoveSpeed.Mult`(0.9) · 부여 태그 `Debuff.AirShot.Drag`. 적용은 기존 `ApplyTimedSpeedEffect` + `FDRTimedSpeedEffect` 재사용 | 사용자 확정(적 한 마리당 합산 3, 적중마다 갱신). Additive 라야 스택 감속이 0.9 / 0.8 / 0.7 이 된다(Compound 는 바이어스 0 → T-95). Deny Overflow 를 켜면 3스택에서 갱신이 거부돼 2초 뒤 통째로 풀린다(T-96). 마지막 적중 2초 뒤 한꺼번에 사라진다 |
| **D-97** | ② 적용 지점 = **적 피해 관문**(`UDREnemyAttributeSet::HandleIncomingDamage`, 적중 회복 블록 다음) — 판정은 라이브러리 `TryApplyAirShotDrag(Props)` 하나(피처 **AND** 컨텍스트 `SourceAbilityTags` 에 `AirShot` **AND** `ADREnemy` · `!bIsBoss`). 이번 적중의 받는 피해는 **이전 스택**으로 계산하고 스택은 그 뒤에 쌓는다 | "적에게 피해가 들어갔다"가 확정되는 유일한 지점(D-41 과 같은 이유) — 벽 · 사이트 · 아군 · 죽은 적 · 두더지 홀로그램은 구조적으로 안 온다. `AirShot` 태그 검사가 없으면 청소기의 돌진 충돌 피해까지 쌓인다(T-99). 보스는 `bIsBoss`(사용자 확정 "보스들") |
| **D-98** | ② 받는 피해 증가 = 적 관문에서 **취약 바로 다음**에 `× (1 + 0.05 × 스택)` — **누가 · 무엇으로 때리든**(DoT 포함). 스택은 `GetEffectStackCountWithTag`, `DragMaxStacks`(3)로 자른다. 취약과는 곱 | 원문 "적이 받는 피해량 +5%". DoT 까지 덮으려면 ExecCalc 가 아니라 여기(D-10). 스택 수는 태그 카운트가 아니다(T-98) |
| **D-99** | ③ 차지 간격 = 기존 축 **`SkillWindup`**(배율) −0.3 → `EffectiveChargeInterval = ChargeInterval × WindupScale ÷ M`(하한 0.05초) | 차지가 곧 공기탄의 선딜이다 — 새 축 없이 0.5 × 0.7 = 0.35. 배율이라 기본 간격을 바꿔도 칩이 비례해 따라간다. 공속 버프(D-69)와 곱 |
| **D-100** | ③ 단계 피해 증가 = 새 축 **`SkillChargeGrowth`**(스킬 단위 배율, 기준 1.0, 높을수록 좋음, 초 아님) −0.4 → `단계 피해 = 0단계 + (그 단계 − 0단계) × Growth`. 0단계는 그대로 | 정적 수치라 카탈로그 모디파이어로(D-2) — 단점이 카드에 빨갛게 뜬다. 배율이라 기본 +20 이 바뀌어도 "증가분의 60%"가 유지된다. 전체 피해에 곱하면 0단계까지 줄어든다(T-104) |
| **D-101** | ★선행 변경★ 공기탄 단계 피해 기본값 **10 / 15 / 22 / 35 → 10 / 30 / 50 / 70**(+20/단계, 탄속은 그대로). `UDRVacuumAirShot` 생성자 값을 바꾼다 | 사용자 확정. BP 가 `Stages` 를 덮지 않아(에셋 확인) C++ 기본값이 곧 게임 값이다 — 한 곳만 바꾼다(T-102). 칩 없이도 공기탄이 강해진다(§13.8-49) |

> 기각: ③ 간격을 새 초 단위 축(`SkillChargeInterval`)으로 — 축을 하나 더 늘릴 이유가 없다(0.5 × 0.7 = 0.35 가 정확히 맞는다).
> 기각: ③ 단계 증가를 `ChipTuning` 절대값(+12)으로 — 정적 수치는 카탈로그가 원칙(D-2)이고, 기본 단계값을 바꾸면 절대값은 어긋난다.
> 기각: ② 를 발사체/GA 에서 적용 — 벽 · 사이트 · 아군 · 죽은 적을 따로 걸러야 한다(D-97).
> 기각: ② 감속을 Compound + 코드가 스택마다 다시 걸기 — 스택 · 지속 · 정리를 GAS 대신 코드가 들고 있어야 한다(Additive 한 줄로 끝난다).

### 13.4 C++ 변경 (적용 순서)

**① ★선행★ `DRVacuumAirShot.cpp` 생성자** — 단계 피해 10 / 30 / 50 / 70 (D-101)

```cpp
	// 기본 4단계 ([0] 즉발 ~ [3] 강화) — ★BP 가 이 배열을 덮지 않는다(에셋 확인) → 이 값이 곧 게임 값★
	// ★Plan5(공기탄칩)★ 단계당 +20 (사용자 확정 2026-10-07, 예전 10/15/22/35). 칩③은 증가분을 ×0.6 (D-100 · D-101)
	Stages.SetNum(4);
	Stages[0].Damage = 10.f;  Stages[0].ProjectileSpeed = 1500.f;
	Stages[1].Damage = 30.f;  Stages[1].ProjectileSpeed = 1800.f;
	Stages[2].Damage = 50.f;  Stages[2].ProjectileSpeed = 2200.f;
	Stages[3].Damage = 70.f;  Stages[3].ProjectileSpeed = 2600.f;
```

**② `DRUpgradeTypes.h` · `DRUpgradeUILibrary.cpp`** — 새 축 `SkillChargeGrowth` (D-100)

```cpp
	// ===== Plan5(공기탄칩) 추가 =====
	// ★신규 값은 반드시 Count 바로 앞에 붙인다.★ (T-30)

	/**
	 * 스킬 단위. 차지형 스킬의 ★단계당 증가분 배율★ — 기준 1.0 에 Apply 해서 쓴다.
	 *     단계 피해 = 0단계 + (그 단계 − 0단계) × ApplySkill(SkillChargeGrowth, 1.0)
	 * 0단계(즉발)는 바뀌지 않는다. 배율이라 DRIsTimeStat · DRIsLowerBetter 에 ★넣지 않는다★ (Plan5 D-100 · T-103)
	 */
	SkillChargeGrowth	UMETA(DisplayName = "스킬 차지 증가량"),

	Count UMETA(Hidden)
```

- `DRIsSkillStat()` 에 `|| Stat == EDRUpgradeStat::SkillChargeGrowth	// Plan5(공기탄칩)` (★빠뜨리면 전역 블록에 쌓여 청소기 모든 스킬에 퍼진다 — T-12★).
- `DRUpgradeUILibrary.cpp` 짧은 이름: `case EDRUpgradeStat::SkillChargeGrowth: return LOCTEXT("StatShort_SkillChargeGrowth", "차지 증가");`(5자 제한).

**③ `DRGameplayTags.h / .cpp`** — 태그 3개(네이티브)

| 멤버 | 태그 | 종류 |
|---|---|---|
| `Upgrade_Feature_Skill_AirShot_FriendlyFire` | `Upgrade.Feature.Skill.AirShot.FriendlyFire` | ① 기능 스위치 |
| `Upgrade_Feature_Skill_AirShot_Drag` | `Upgrade.Feature.Skill.AirShot.Drag` | ② 기능 스위치 |
| `Debuff_AirShot_Drag` | `Debuff.AirShot.Drag` | ★GAS 태그★ — `GE_AirShot_Drag` 가 적에게 부여, C++ 이 스택을 읽는다 |

**④ `DRProgressionConfig.h`** — `FDRRideChipTuning` 다음

```cpp
/**
 * ★Plan5(공기탄칩)★ 청소기 기본 공격(공기탄) 칩의 동적 상수.
 * 피해 +50% · −20% · 차지 간격 · 단계 증가분은 여기가 아니라 DA_ChipCatalog 모디파이어다.
 */
USTRUCT(BlueprintType)
struct FDRAirShotChipTuning
{
	GENERATED_BODY()

	/**
	 * ② 흡착 — 맞은 적 1스택. Magnitude 0.1 = 스택당 이속 −10%, Duration 2 = 마지막 적중 뒤 유지 시간.
	 * GE 요구 사양 (Plan5 D-96):
	 *   - Has Duration / Duration = SetByCaller(Data.Duration)
	 *   - Modifier: MoveSpeed / ★Multiply (Additive)★ / SetByCaller(Data.MoveSpeed.Mult)  ★Compound 금지 — 스택하면 가속 (T-95)★
	 *   - Target Tags(Granted) = Debuff.AirShot.Drag
	 *   - Stacking = Aggregate by Target · Limit = DragMaxStacks(3) · Refresh on Successful Application
	 *     · Expiration = Clear Entire Stack · ★Deny Overflow Application 끔★ (T-96)
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AirShot|Drag")
	FDRTimedSpeedEffect Drag;   // 기본값을 Magnitude 0.1 · Duration 2.0 으로 둔다 (생성자 · 기본 멤버 초기화)

	// 스택당 받는 피해 증가 (0.05 = +5%). 누가 · 무엇으로 때리든 (D-98)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AirShot|Drag", meta = (ClampMin = "0.0"))
	float DragDamageTakenPerStack = 0.05f;

	// 받는 피해 계산에 쓰는 스택 상한. ★GE 의 Stack Limit 과 같아야 한다★ — 검사가 비교한다
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AirShot|Drag", meta = (ClampMin = "1"))
	int32 DragMaxStacks = 3;
};
```

- `FDRTimedSpeedEffect` 기본값은 0.3 / 5.0 이다 → `Drag` 는 **0.1 / 2.0** 으로 초기화한다(`FDRAirShotChipTuning() { Drag.Magnitude = 0.1f; Drag.Duration = 2.f; }`). 물대포 `EnemySlow` 처럼 "구조체 기본값이 5.0" 함정(§5.3)을 반복하지 않게.
- `FDRChipTuning` 끝에 `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "AirShot") FDRAirShotChipTuning AirShot;`

**⑤ `DRAbilitySystemLibrary.h / .cpp`** — 판정 2개(D-97 · D-98)

```cpp
	// ========== 청소기 기본 공격 칩 (Plan5(공기탄칩)) ==========

	/** ★Plan5(공기탄칩)★ ② 이 적이 받는 피해 배율 = 1 + DragDamageTakenPerStack × min(흡착 스택, DragMaxStacks). 표식이 없으면 정확히 1.0 */
	static float GetAirShotDragDamageTakenMultiplier(const UAbilitySystemComponent* TargetASC);

	/**
	 * ★Plan5(공기탄칩)★ ② 적 피해 관문에서 부른다(서버). 세 조건을 ★모두★ 만족하면 흡착 1스택(지속 갱신):
	 *   ① 소스가 Upgrade.Feature.Skill.AirShot.Drag  ② 컨텍스트 SourceAbilityTags 에 Abilities.RobotVacuum.AirShot
	 *   ③ 대상이 ADREnemy · 보스(bIsBoss) 아님
	 * ②를 빠뜨리면 그 청소기의 돌진 충돌 피해까지 쌓인다 (T-99)
	 */
	static void TryApplyAirShotDrag(const FEffectProperties& Props);
```

```cpp
float UDRAbilitySystemLibrary::GetAirShotDragDamageTakenMultiplier(const UAbilitySystemComponent* TargetASC)
{
	if (!TargetASC || !TargetASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Debuff_AirShot_Drag)) return 1.f;   // 빠른 탈출

	const FDRAirShotChipTuning& Tuning = GetChipTuning(TargetASC->GetAvatarActor()).AirShot;
	// ★스택 수는 태그 카운트가 아니다★ — Aggregate by Target 이라 인스턴스 하나의 스택 (T-98)
	const int32 Stacks = FMath::Min(GetEffectStackCountWithTag(TargetASC, FDRGameplayTags::Get().Debuff_AirShot_Drag),
		FMath::Max(1, Tuning.DragMaxStacks));
	return 1.f + FMath::Max(0.f, Tuning.DragDamageTakenPerStack) * Stacks;
}

void UDRAbilitySystemLibrary::TryApplyAirShotDrag(const FEffectProperties& Props)
{
	const FDRGameplayTags& Tags = FDRGameplayTags::Get();
	if (!HasUpgradeFeature(Props.SourceASC, Tags.Upgrade_Feature_Skill_AirShot_Drag)) return;

	const FDRGameplayEffectContext* DRContext = static_cast<const FDRGameplayEffectContext*>(Props.EffectContextHandle.Get());
	if (!DRContext || !DRContext->GetSourceAbilityTags().HasTagExact(Tags.Abilities_RobotVacuum_AirShot)) return;

	ADREnemy* Enemy = Cast<ADREnemy>(Props.TargetAvatarActor);
	if (!IsValid(Enemy) || Enemy->bIsBoss) return;   // 보스는 감속 · 피해 증가 둘 다 없다 (사용자 확정)

	const FDRAirShotChipTuning& Tuning = GetChipTuning(Enemy).AirShot;
	ApplyTimedSpeedEffect(Enemy, Props.SourceASC, Tuning.Drag, 1.f - Tuning.Drag.Magnitude);   // 0.9 — 스택은 GE 가 쌓는다
}
```

**⑥ `DREnemyAttributeSet.cpp` `HandleIncomingDamage`** — 두 줄 (D-97 · D-98)

```cpp
		// (취약 블록 바로 다음)
		// ★Plan5(공기탄칩)★ ② 흡착 — 스택당 받는 피해 +5%(최대 3). 취약과 같은 자리 · 같은 이유(D-10) — 모든 피해에.
		// ★이번 적중이 쌓을 스택은 아직 없다★ — 직전 스택으로 계산하고, 스택은 아래에서 쌓는다 (D-97)
		LocalIncomingDamage *= UDRAbilitySystemLibrary::GetAirShotDragDamageTakenMultiplier(Props.TargetASC);

		...(흡혈 · 적중 회복 그대로)...

		// (적중 회복 블록 다음, 더미 분기 · SetHealth 위)
		// ★Plan5(공기탄칩)★ ② 공기탄이 적에게 들어갔다 — 흡착 1스택(보스 제외). 판정은 라이브러리가 한다 (D-97)
		UDRAbilitySystemLibrary::TryApplyAirShotDrag(Props);
```

**⑦ `DRVacuumAirShot.cpp`** — ③ 간격 · 단계 증가 · ① 아군 플래그 (D-94 · D-99 · D-100)

```cpp
// StartCharging()
	// ★Plan5(공기탄칩)★ ③ 칩 선딜 배율(SkillWindup — 0.7 이면 0.5 → 0.35초)을 곱한 뒤 공속 배율로 나눈다 (D-99).
	//   칩 · 버프가 없으면 둘 다 1.0 이라 ChargeInterval 그대로. GetChargeStage · UI 타이머가 이 값 하나를 쓴다 (T-105)
	const float WindupScale = FMath::Max(0.01f, GetUpgradedFloat(EDRUpgradeStat::SkillWindup, 1.f));
	EffectiveChargeInterval = FMath::Max(0.05f, ChargeInterval * WindupScale / FMath::Max(1.f, GetAttackSpeedMultiplier()));

// FireShot() — 피해 주입부
	// ★Plan5(공기탄칩)★ ③ 단계 피해 = 0단계 + (그 단계 − 0단계) × 차지 증가 배율 — 0단계는 그대로 (D-100 · T-104)
	const float Growth = FMath::Max(0.f, GetUpgradedFloat(EDRUpgradeStat::SkillChargeGrowth, 1.f));
	const float StageDamage = Stages[0].Damage + (StageData.Damage - Stages[0].Damage) * Growth;
	Projectile->DamageEffectParams.BaseDamage = GetUpgradedFloat(EDRUpgradeStat::SkillDamage, StageDamage);

	// ★Plan5(공기탄칩)★ ① 아군에게도 피해 — 스폰 때 1회 해석해 발사체에 싣는다 (D-94).
	//   ★강화탄 분기(if (!bMaxStage) return;) 보다 위★ — 0~2단계 탄도 아군을 맞혀야 한다
	if (ADRVacuumAirProjectile* AirProjectile = Cast<ADRVacuumAirProjectile>(Projectile))
	{
		AirProjectile->bHitsAllies = UDRAbilitySystemLibrary::HasUpgradeFeature(
			GetAbilitySystemComponentFromActorInfo(), FDRGameplayTags::Get().Upgrade_Feature_Skill_AirShot_FriendlyFire);
	}
```

**⑧ `DRProjectile.h / .cpp`** — 피해 적용부 추출(동작 불변)

```cpp
// .h protected
	// 서버: Target 에게 이 발사체의 피해(넉백 확률 · 사망 임펄스 포함)를 건다.
	// ★팀 판정은 하지 않는다 — 호출부 책임★. 칩 지식은 없다(적 투사체와 공유 클래스 — Plan5 §8) (Plan5(공기탄칩) D-94)
	void ApplyImpactDamage(AActor* Target);
```

- `OnSphereOverlap` 의 `if (HasAuthority()) { … ApplyDamageEffect … Destroy(); }` 안쪽 피해 부분을 그대로 옮기고, 원래 자리는 `ApplyImpactDamage(OtherActor); Destroy();` — 적 투사체 · 다른 플레이어 투사체 동작 불변.

**⑨ `DRVacuumAirProjectile.h / .cpp`** — ① 아군 분기 (D-94 · D-95)

```cpp
// .h public
	// ★Plan5(공기탄칩)★ ① 아군에게도 피해 — FireShot 이 스폰 직후 넣는다(서버). 클라 적중 연출용으로 복제 (bEnhanced 와 같다)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AirShot")
	bool bHitsAllies = false;

// .cpp GetLifetimeReplicatedProps: DOREPLIFETIME(ADRVacuumAirProjectile, bHitsAllies);

// .cpp OnSphereOverlap — 탑승 연결 통과 검사 ★다음★, 강화탄 아군 분기 ★앞★
	// ★Plan5(공기탄칩)★ ① 아군도 적과 같은 피해 — 강화탄이면 넉백도 일반 피해 경로로 (D-94 · D-95).
	//   탑승 연결(내 위 탑승자 · 내가 탄 청소기)은 바로 위에서 이미 통과했다 (사용자 확정)
	if (bHitsAllies && !UDRAbilitySystemLibrary::IsNotFriend(SourceAvatarActor, OtherActor))
	{
		if (!bHit) OnHit();
		if (HasAuthority())
		{
			ApplyImpactDamage(OtherActor);
			Destroy();
		}
		else
		{
			bHit = true;
		}
		return;
	}
```

**⑩ `DRProgressionConfig.cpp`** — 검사(칩이 카탈로그에 있을 때만)

| 조건 | 검사 | 틀리면 |
|---|---|---|
| `IsFeatureGated(AirShot.Drag)` | `Drag.Effect` 비었나 / 부여 태그 `Debuff.AirShot.Drag` / `StackingType == AggregateByTarget` / `GetStackLimitCount() == DragMaxStacks` / `GetStackExpirationPolicy() == ClearEntireStack` / `!bDenyOverflowApplication` / MoveSpeed 모디파이어가 ★**MultiplyAdditive**★ / `Drag.Magnitude` 0~1 · `Drag.Duration > 0` | 감속 · 피해 증가 없음 / 받는 피해 증가 안 읽힘 / 청소기마다 따로 쌓임 / 표시 스택과 계산 스택 불일치 / 1개씩 빠짐 / ★3스택에서 갱신 실패 → 2초 뒤 통째로 풀림(T-96)★ / ★Compound 면 3스택 가속(T-95)★ |

**바뀔 C++ 파일**: `DRVacuumAirShot`(.cpp) · `DRUpgradeTypes.h` · `DRUpgradeUILibrary.cpp` · `DRGameplayTags`(.h/.cpp) · `DRProgressionConfig`(.h/.cpp) · `DRAbilitySystemLibrary`(.h/.cpp) ·
`DREnemyAttributeSet.cpp` · `DRProjectile`(.h/.cpp — 추출만) · `DRVacuumAirProjectile`(.h/.cpp). **BP 재배선 없음.**

### 13.5 에디터 작업 (C++ 빌드 뒤)

1. **`GE_AirShot_Drag`** — `Content/Blueprints/AbilitySystem/Player/VacuumCleaner/Abilities/Skill1/` 에 새 GameplayEffect BP
   - Duration Policy **Has Duration** · Duration Magnitude **Set by Caller** `Data.Duration`
   - Modifiers +1: Attribute `DRAttributeSet.MoveSpeed` · Modifier Op ★**Multiply (Additive)**★ · Magnitude **Set by Caller** `Data.MoveSpeed.Mult` (★Compound 금지 — T-95★)
   - Components → **Target Tags** `Debuff.AirShot.Drag`
   - Stacking **Aggregate by Target** · Stack Limit Count **3** · Stack Duration Refresh Policy **Refresh on Successful Application** · Stack Expiration Policy **Clear Entire Stack** · **Deny Overflow Application 끔**
2. **`DA_ProgressionConfig` → Chip Tuning → Air Shot**: `Drag.Effect = GE_AirShot_Drag`. 숫자는 C++ 기본값(0.1 · 2.0 · 0.05 · 3).
3. **`DA_ChipCatalog`** → Chips +3 (§13.1 표). 공통: `OwnerClass RobotVacuum` · `bCommonChip` 끔 · `Category Ascension` · `TargetAbilityTag Abilities.RobotVacuum.AirShot` · `RequiredSlotCount 1`. ★모든 스킬 단위 모디파이어의 `TargetAbilityTag` 도 `Abilities.RobotVacuum.AirShot`★(D-51)
   - ① `RV.Asc.ShotPower` · 고압 공기탄 · 요약 "기본 공격 피해 +50%" · Tier 3 · Modifiers `SkillDamage` Percent +0.5 · GrantedTags `Upgrade.Feature.Skill.AirShot.FriendlyFire`
     · Description "기본 공격의 피해가 50% 늘어납니다. 대신 아군에게도 같은 피해를 줍니다(내가 태운 사람 · 나를 태운 청소기는 맞지 않습니다)."
   - ② `RV.Asc.ShotDrag` · 흡착 공기탄 · 요약 "맞은 적 감속 · 받는 피해 증가" · Tier 4 · Modifiers `SkillDamage` Percent −0.2 `bIsDrawback` ✓ · GrantedTags `Upgrade.Feature.Skill.AirShot.Drag`
     · Description "기본 공격에 맞은 적은 2초 동안 이동 속도가 10% 줄고 받는 피해가 5% 늘어납니다. 맞힐 때마다 시간이 갱신되고 최대 3번까지 쌓입니다(이동 속도 −30%, 받는 피해 +15%). 보스에게는 걸리지 않습니다. 대신 기본 공격의 피해가 20% 줄어듭니다."
   - ③ `RV.Asc.ShotQuick` · 속사 압축기 · 요약 "차지가 30% 빨라짐" · Tier 3
     · Modifiers: `SkillWindup` Percent **−0.3**(`DetailOverride` "차지 간격 0.5 → 0.35초") / `SkillChargeGrowth` Percent **−0.4** `bIsDrawback` ✓(`DetailOverride` "단계당 피해 +20 → +12")
     · Description "기본 공격의 차지 단계가 0.35초마다 오릅니다(기존 0.5초). 대신 단계마다 늘어나는 피해가 20에서 12로 줄어듭니다."
   - `SkillDisplayNames` 에 `Abilities.RobotVacuum.AirShot → 기본 공격`
4. (선택) 아이콘 `Content/DaeRuneAssets/UI/Upgrade/Icon/Vacuum/공격-7 · 지원-7 · 편의-7`
5. (선택) 흡착 표식 연출 — 적 머리 위 스택 표시가 필요하면 `GameplayCue`(부여 태그 기반) 하나. 지금 HUD 의 상태 효과 위젯은 플레이어 전용이다
6. 검사: `DA_ProgressionConfig` 편집 → `[ProgressionConfig] … 검증 통과` / 카탈로그 오류 0건(새 축 `SkillChargeGrowth` 가 스킬 단위로 잡히는지 포함)

### 13.6 함정

| # | 함정 | 대응 |
|---|---|---|
| **T-95** | ★스택형 감속 GE 에 Multiply (Compound)★ | UE 5.5 Compound 바이어스가 0 이라 0.9 × 스택 → 3스택 **×2.7(가속)**. **Multiply (Additive)**(바이어스 1 → 0.9 / 0.8 / 0.7). 검사가 잡는다. ※씨앗 · 물대포 감속은 Limit 1 이라 Compound 가 맞다(T-8) — 규칙을 그대로 옮기지 말 것 |
| **T-96** | `Deny Overflow Application` 을 켠다 | 3스택에서 다음 적중이 거부돼 지속이 갱신되지 않는다 → 계속 맞혀도 2초 뒤 통째로 풀린다(T-4 와 같다) |
| **T-97** | Stack Expiration Policy 가 `Clear Entire Stack` 이 아니다 | 1개씩 빠지거나(Remove Single) 남은 시간이 꼬인다 — 마지막 적중 2초 뒤 한꺼번에 사라져야 한다 |
| **T-98** | 받는 피해 증가를 태그 카운트로 계산 | Aggregate by Target 이라 태그 카운트는 항상 1 → 항상 +5%. `GetEffectStackCountWithTag`(T-3) |
| **T-99** | ② 판정에서 `SourceAbilityTags` 의 `AirShot` 검사를 빠뜨린다 | 소스가 같은 청소기면 **돌진 충돌 피해**에도 스택이 쌓인다 |
| **T-100** | 보스를 `Buff.Elite` · 캐릭터 클래스로 판정 | 일반 엘리트까지 빠지거나 두더지 보스가 걸린다 — `ADREnemy::bIsBoss`(엘리트 곰 BP · 두더지 보스 C++) |
| **T-101** | `bHitsAllies` · `bEnhanced` 는 `SpawnProjectile`(=`FinishSpawning`) **뒤**에 들어간다 | 스폰 순간 총구와 겹쳐 있던 아군은 첫 오버랩에서 플래그를 모른다(지금 `bEnhanced` 도 같다). 문제가 되면 `SpawnProjectile` 에 지연 초기화 훅을 넣는다 |
| **T-102** | 단계 피해를 BP 와 C++ 양쪽에서 바꾼다 / 나중에 BP 에서 `Stages` 를 건드린다 | 지금은 BP 가 덮지 않아 C++ 이 권위다. BP 에서 한 번이라도 바꾸면 그때부터 C++ 기본값은 무시된다 — 한 곳만(지금은 C++) |
| **T-103** | `SkillChargeGrowth` 를 `Count` 앞이 아닌 곳에 넣거나 `DRIsSkillStat` 에서 빠뜨린다 | 기존 칩 효과가 밀리거나(T-30) 전역 블록에 쌓여 청소기 모든 스킬로 퍼진다(T-12). 배율이라 `DRIsTimeStat` · `DRIsLowerBetter` 에는 넣지 않는다 |
| **T-104** | 증가 배율을 단계 피해 전체에 곱한다 | 0단계까지 줄어든다(10 → 6) — `0단계 + (단계 − 0단계) × Growth` |
| **T-105** | `SkillWindup` 을 UI 타이머에만(또는 `GetChargeStage` 에만) 적용 | 게이지와 실제 단계가 어긋난다 — `EffectiveChargeInterval` 하나를 둘 다 쓴다 |
| **T-106** | 아군 피해 분기를 `ADRProjectile` 에 넣는다 | 적 투사체와 공유 클래스라 적 탄이 적을 맞히는 사고가 난다(§8) — 범용 `ApplyImpactDamage` 만 부모에, 분기는 청소기 자식에 |
| **T-107** | ①+강화탄을 기존 "아군 = 넉백만" 분기로 보낸다 | 강화탄은 아군에게 피해를 못 준다 — ① 분기를 그 분기 ★앞★ 에 둔다 |

### 13.7 검증

준비: PIE Listen Server · 플레이어 2~3(청소기 1 + 아군). 콘솔 `DRUnlockUpgrade` → `DRUnlockSlot 6`. 수치는 데미지 숫자 · `showdebug abilitysystem`(적 MoveSpeed · 활성 GE 스택).

- [ ] **선행 변경(칩 없음)** 0 · 0.5 · 1.0 · 1.5초 홀드 → 피해 10 / 30 / 50 / 70, 탄속 · 넉백 · 반동 그대로 / 아군은 지금처럼 통과(강화탄은 넉백만)
- [ ] **①** 적 15 / 45 / 75 / 105 / 아군에게 같은 피해(아군 체력 · 컨테이너 · 오염 · 탑승 공유가 평소처럼) / 강화탄 = 아군 피해 + 넉백 / 내 위 탑승자 · 내가 탄 청소기는 통과 / 로비에서 아군 체력 1 아래로 안 감 / 클라 청소기의 탄이 호스트 아군을 맞히면 클라 화면에서도 그 자리에서 터진다
- [ ] **②** 피해 8 / 24 / 40 / 56 / 1 · 2 · 3번 맞히면 적 이속 ×0.9 · 0.8 · 0.7(★1.8 · 2.7 이면 T-95★) / 4번째부터 ×0.7 유지 · 지속 갱신(★3스택에서 2초 뒤 풀리면 T-96★) / 마지막 적중 2초 뒤 한꺼번에 해제 / 받는 피해: 같은 적을 정원로봇이 때려도 +5 · 10 · 15% / 엘리트 곰 · 두더지 보스는 감속도 피해 증가도 없음, 일반 엘리트는 걸림 / 청소기 2대가 같이 쌓아도 최대 3 / 돌진 충돌 · 씨앗 폭탄으로는 안 쌓임 / 물대포 ② 취약과 함께면 ×1.725 / 비행 적은 감속 없이 피해 증가만
- [ ] **③** 단계가 0.35 · 0.70 · 1.05초에 오른다(게이지와 실제 일치) / 피해 10 / 22 / 34 / 46 / 공유 공속 버프 5중첩이면 0.233초 간격 / 발톱 · 돌진 등 다른 스킬은 그대로
- [ ] **스킬당 1칩** ①②③ 중 하나만, 탑승칩과는 함께 / 미리보기 라벨 "기본 공격" / ③ 카드에 "차지 간격 0.5 → 0.35초"(초록) · "단계당 피해 +20 → +12"(빨강)
- [ ] **멀티** 클라 청소기 ② → 호스트 화면의 적이 느려지고(복제) 다른 플레이어 피해도 늘어남 / 클라 ③ → 클라 게이지 · 서버 단계 일치
- [ ] **검사** `DA_ProgressionConfig` 편집 → 오류 0건 / `GE_AirShot_Drag` 를 Compound 로 바꾸면 오류(T-95) · Deny Overflow 를 켜면 오류(T-96)

### 13.8 열린 결정 · 알려진 제약 (§9 번호에 이어서)

| # | 내용 | 현재 판단 |
|---|---|---|
| **48** | 이름 · Tier · 아이콘 · `ChipId`(`RV.Asc.Shot*`)는 가칭 | 사용자가 정하면 카탈로그만(`ChipId` 는 첫 입력 전에 확정 — 세이브 키) |
| **49** | ★선행 변경(D-101)으로 칩 없는 공기탄이 크게 강해진다★ — 풀차지 35 → 70, 1단계 15 → 30 | 사용자 확정. 플레이 확인 후 `Stages` 만 조정 |
| **50** | ③ 은 초당 피해가 칩 없음과 비슷하거나 조금 낮다(1단계 +5% · 2단계 −3% · 3단계 −6%) — 이득은 "강화탄(넉백 · 로켓 점프)까지 30% 빨리" | 사용자 확정 수치. 약하면 카탈로그 `SkillChargeGrowth` 값만 조정 |
| **51** | ① 아군 피해로 아군이 오염 · 사망할 수 있다. 아군을 맞혔다는 별도 HUD 표시는 없다 | 원문 "동일한 피해" 그대로. 로비 · 튜토리얼은 체력 1 클램프 |
| **52** | ① 아군 피해에도 시전자의 공격 배율(광폭화 · 시너지 · 탑승칩①)이 붙는다 | "같은 피해" — 적에게 주는 것과 같은 계산 |
| **53** | ② 비행 적에게는 감속이 안 걸린다(§9-1) — 받는 피해 증가는 걸린다 | §9-1 과 같은 판단(범위 밖) |
| **54** | ② 스택 표시(연출)가 없다 — 적 머리 위 아이콘 · 나이아가라 없음 | 원하면 `GameplayCue` 하나(§13.5-5) |
| **55** | ② 받는 피해 증가는 그 적중 **직전** 스택으로 계산 — 첫 적중은 +0% | 설계 판단(D-97). 첫 적중부터 +5% 를 원하면 적용 순서만 바꾼다 |

### 13.9 작업 순서

1. 선행 변경 ①(단계 피해 10 / 30 / 50 / 70) → 빌드 → §13.7 "선행 변경"
2. 칩 C++ ②~⑩ → 빌드 (`UENUM` · 태그 · `USTRUCT` 추가라 **에디터를 닫고** — T-45)
3. 에디터 §13.5
4. §13.7 검증 → §1 · §3 · §5 · §6 · §9 에 흡수(§1.7 그룹 +1, 스탯 축 표에 `SkillChargeGrowth`), 머리말 상태 갱신

### 13.10 구현 결과 — 계획과 달라진 점

2026-10-08 C++ 구현 · 빌드 통과. 다른 점:

- ★`UDRAbilitySystemLibrary::GetEffectStackCountWithTag` 를 **OwningTags 쿼리**(에셋 태그 ∪ 부여 태그)로 바꿨다★.
  UE 5.5 `GetActiveEffectsWithAllTags` 는 GE 의 **에셋 태그만** 비교한다(엔진 `FGameplayEffectQuery::Matches` 의 `EffectTagQuery` — `Spec.GetAllAssetTags` 만 본다).
  그래서 부여(Target) 태그만 단 `GE_AirShot_Drag` 는 스택이 늘 0 으로 읽혀 받는 피해 증가가 안 붙는다. 공속 GE 3개는 두 태그를 다 달고 있어(에셋 확인) 결과가 같다.
  → 흡수할 때 §6 함정에 올릴 것("스택 · 존재 조회에 `GetActiveEffectsWithAllTags` 금지 — `MakeQuery_MatchAnyOwningTags`", T-82 와 같은 원인).
- `ADREnemy::bIsBoss` 가 protected 라 공개 읽기 함수 `IsBoss()` 를 추가했다(값 변경 없음).
- 단계 피해 10 / 30 / 50 / 70(D-101)은 C++ 생성자에 들어갔다 — 에셋 작업 없음.

---

## 14. 로봇 청소기 물 점프(스킬 1 · Q) 칩 3종 (C++ 구현 완료 — 에디터 · 검증 남음)

> 2026-10-07 요청 · 같은 날 질의응답으로 확정. 코드 주석 접두 **`Plan5(물점프칩)`** · 결정 **D-102 ~ D-110** · 함정 **T-108 ~ T-118** (앞 번호에 이어서 매긴 영구 ID).
> **C++ 구현 완료**(2026-10-08 빌드 통과). 에디터 작업은 **§16 안내서**, 그다음 §14.7 검증. 계획과 달라진 점은 §14.10. 검증이 끝나면 §1~§9 에 흡수한다.
> ★선행 변경 1건 — **D-103**(물 점프가 물 30 을 ★실제로★ 소모 — 지금은 30 이 있는지 확인만 하고 빼지 않는다) — 은 칩이 없어도 물 점프를 바꾼다(§14.8-57).★
>
> 사용자 확정
> - ① 강판정(반경 2m 안) 피해 **20 → 40**, 약판정(2~4m) **5 → 15**. 대신 상승 높이 **−30%**.
> - ② 쓰는 순간부터 **0.3초 무적** — **피격을 통째로 무효**(피해 · 경직 · 넉백 · 디버프 · DoT 틱 · 탑승 공유 — 벨보이 모자와 같은 방식, 모자보다 먼저). 대신 물 점프 피해 **0**.
> - ③ 물 소모 **30 → 20**, 공중 사용 **1 → 2회**. 대신 상승 높이 **−25%**.
> - "상승 높이"(①)와 "상승력"(③)은 둘 다 **실제 최고 높이** 기준이다 — 높이는 속도의 제곱에 비례하므로 점프 임펄스에는 √배율을 곱한다(D-105).
> - 물 점프가 지금 물을 빼지 않는 것은 **버그로 보고 고친다**(D-103).

### 14.0 지금의 물 점프 (코드·에셋 실측)

| 항목 | 지금 | 어디 |
|---|---|---|
| 스킬 | `GA_VacuumJetJump`(Q) — `UDRVacuumJetJump : UDRDamageGameplayAbility`. `AbilityTags = Abilities.RobotVacuum.JetJump`(업그레이드 키). **즉발**(ActivateAbility 안에서 End). `BlockAbilitiesWithTag` = HitReact · Carrying / `ActivationBlockedTags` = Carrying | GA BP CDO |
| ★물★ | `WaterCost` **30**(C++ 생성자, BP 미변경). ★`CostGameplayEffectClass` 가 비어 있다★ → `CheckCost` 는 물 30(부족하면 체력 대납 가능 여부)을 확인하지만 `ApplyCost` 는 `GetCostGameplayEffect()` 가 null 이라 **아무것도 빼지 않는다**. `GE_Cost_VacuumJetJump`(ExecCalc_WaterCost)는 엉뚱하게 **`GA_VacuumDash` 의 Cost GE** 로 들어가 있고, 돌진은 `WaterCost` 0 이라 무동작 | 에셋 · `DRGameplayAbility.cpp` |
| 쿨다운 | `GE_Cooldown_VacuumJetJump` **0.3초**(`Cooldown.RobotVacuum.JetJump` — Q 홀드 연타 방지, 고정값) | 에셋 |
| 점프 | 서버에서 `LaunchCharacter((0, 0, JumpImpulseZ), XY 유지, Z 덮어쓰기)` — `JumpImpulseZ` **900**(BP 미변경). `BP_DRVacuumCleaner` 의 이동 컴포넌트는 덮은 값이 없다 → 중력 −980 · GravityScale 1 → **최고 높이 900² ÷ (2 × 980) ≈ 413uu**, 정점까지 0.92초. (스페이스 점프 = `JumpZVelocity` 기본 420 → 약 90uu) | `DRVacuumJetJump.cpp` · `BP_DRVacuumCleaner` |
| 공중 사용 | `ADRRobotVacuumCharacter::bAirJumpUsed`(서버 전용 · 복제 안 함). 떨어지는 중에 쓰면 true, `Landed()` 에서 false. `CanActivateAbility` 가 `IsFalling() && bAirJumpUsed` 면 거부 → **지상 Q → 공중 Q 2회 / 스페이스 → 공중 Q 1회 / 낭떠러지 낙하 중 1회**. 클라는 값이 늘 false 라 예측은 통과하고 서버가 거부한다 | `DRRobotVacuumCharacter` · `DRVacuumJetJump` |
| 피해 | 시전 지점 **2D** 반경(Z 무시): 200 안 `InnerDamage` **20**(강판정), 200~400 `OuterDamage` **5**(약판정). 적만(`IsNotFriend`). 넉백 · 디버프 0. ★**`SkillDamage` 를 읽지 않는다**★ — `MakeDamageEffectParamsFromClassDefaults` 의 `BaseDamage`(= `GetUpgradedDamage()` = GA `Damage` 0 기준)를 Inner/OuterDamage 로 그냥 덮는다 | 〃 · GA CDO |
| 연출 | `GameplayCue.Skill.VacuumJetJump`(서버 Execute) / 애님 `bIsJetJumping` · `JetJumpCounter`(복제) | 〃 |
| 피해 관문 | `UDRAttributeSet::PostGameplayEffectExecute` IncomingDamage 분기: 죽음 검사 → ★**모자**(1회 무효 — 피해 0 + return)★ → 도발 ×0.8 → `HandleIncomingDamage`(플레이어: 받는 쪽 배율 → 로비 클램프 → 디버프 → 탑승 공유 → 체력 · 컨테이너 → 히트리액트 · 넉백) | `DRAttributeSet.cpp` · `DRPlayerAttributeSet.cpp` |
| 무적 선례 | **플레이어 무적은 없다.** 모자(Plan8)가 위 관문에서 "피해 0 + return" 으로 피격을 통째로 지운다. 적 쪽은 튜토리얼 더미(체력 1 클램프) · 두더지 보스 잠수(숨김 + 콜리전 끔 + 디버프 정리) | `DRAbilitySystemLibrary::TryConsumeBellBoyHat` |
| 물값 지불 | `ExecCalc_WaterCost` 는 Water · Health 어트리뷰트를 **직접** 깎는다(IncomingDamage 아님) → 무적 · 모자에 막히지 않는다 | `ExecCalc_WaterCost.cpp` |
| 청소기 칩 | 탑승(§12) · 기본 공격(§13) 계획뿐 — 물 점프 그룹은 새로 생긴다 | |

### 14.1 칩 3종 스펙

공통: `OwnerClass = RobotVacuum`, `Category = Ascension`, `RequiredSlotCount = 1`,
칩 레벨 · **모든 모디파이어**의 `TargetAbilityTag = Abilities.RobotVacuum.JetJump`(실제 GA 태그 — D-102). → ①·②·③ 중 하나만, **탑승칩(§12) · 공기탄칩(§13)과는 함께** 낄 수 있다.

| # | `ChipId` | 이름(가칭) | Modifiers | GrantedTags | 효과 | 단점 | Tier | 아이콘(제안) |
|---|---|---|---|---|---|---|---|---|
| ① | `RV.Asc.JumpBlast` | 폭압 분사구 | `SkillDamage` **+1.0**<br>`SkillOuterDamage` **+0.5**<br>`SkillLaunchHeight` **−0.3** ★단점★ | (없음) | 강판정 **20 → 40**, 약판정 **5 → 15** | 상승 높이 **×0.70**(413 → 289uu) | 3 | 공격-8 |
| ② | `RV.Asc.JumpGuard` | 완충 분사구 | `SkillDamage` **−1.0** ★단점★ | `…JetJump.Guard` | 쓰는 순간부터 **0.3초 무적**(피격 통째로 무효) | 물 점프 피해 **0** | 4 | 지원-8 |
| ③ | `RV.Asc.JumpMulti` | 다단 분사구 | `SkillWaterCost` **Flat −10**<br>`SkillAirUseCount` **Flat +1**<br>`SkillLaunchHeight` **−0.25** ★단점★ | (없음) | 물 **30 → 20**, 공중 사용 **1 → 2회** | 상승 높이 **×0.75**(413 → 310uu) | 3 | 편의-8 |

`…` = `Upgrade.Feature.Skill`. 모디파이어 Op 는 `Flat` 표기가 없으면 `Percent`(곱연산).
카드 표기(`DetailOverride`): ① `SkillDamage` "강판정 피해 20 → 40" · `SkillOuterDamage` "약판정 피해 5 → 15" / ② `SkillDamage` "피해 0" /
③ `SkillWaterCost` "물 소모 30 → 20" · `SkillAirUseCount` "공중 사용 1 → 2회". `SkillLaunchHeight` 줄은 기본 표기("상승 높이 −30% / −25%") 그대로.
②는 단점이 모디파이어라 검사상 `Description` 필수는 아니지만, 무적 범위를 알려야 하므로 쓴다. 이름 · Tier · 아이콘 · `ChipId` 는 가칭(§14.8-56).

**1회 수치** (공용 칩 · 버프 없음 / 높이는 중력 −980 기준)

| | 강판정(≤2m) | 약판정(2~4m) | 점프 임펄스 | 1회 최고 높이 | 정점까지 | 물 | 공중 사용 |
|---|---|---|---|---|---|---|---|
| 지금(선행 변경 전) | 20 | 5 | 900 | 413uu | 0.92초 | **0**(30 확인만) | 1 |
| **칩 없음(D-103 뒤)** | 20 | 5 | 900 | 413uu | 0.92초 | **30** | 1 |
| ① | **40** | **15** | 753 (×√0.7) | **289uu** | 0.77초 | 30 | 1 |
| ② | **0**(판정 생략) | **0** | 900 | 413uu | 0.92초 | 30 | 1 |
| ③ | 20 | 5 | 779 (×√0.75) | **310uu** | 0.80초 | **20** | **2** |

- ① 계산: 강판정 20 × 2 = 40 / 약판정 5 × 2(`SkillDamage`) × 1.5(`SkillOuterDamage`) = 15.
- `SkillLaunchHeight` −0.3 은 "높이 ×0.7" 이고 임펄스는 ×√0.7 = ×0.837 이다 — 임펄스에 0.7 을 그대로 곱하면 높이가 ×0.49 가 된다(T-109).

**한 번 떴을 때 최대** (지상 Q 후 정점마다 공중 Q — 높이 단순 합, 수평 이동 · 천장 무시)

| | Q 횟수 | 최대 높이 | 물 | 체공(대략) | 무적 |
|---|---|---|---|---|---|
| 칩 없음 | 2 | 826uu | 60 | 3.1초 | — |
| ① | 2 | 579uu | 60 | 2.6초 | — |
| ② | 2 | 826uu | 60 | 3.1초 | Q 마다 0.3초 — 지상 Q 직후 0.3초(쿨다운) 만에 공중 Q 를 쓰면 **0.6초 연속** |
| ③ | **3** | **930uu** | 60 (20 × 3) | 3.8초 | — |

- ③은 1회 높이는 낮지만 같은 물 60 으로 더 높이(+13%) · 더 오래 뜬다. 스페이스 점프로 시작하면 90uu + 공중 Q 2회.

### 14.2 상호작용

| 조합 | 결과 |
|---|---|
| ① + 공용 칩(⑥ 광폭화 · ⑦ 시너지 · `DamageDealt`) · 탑승칩① | ExecCalc 소스측에서 40 / 15 에 추가로 곱해진다 |
| ② + 같은 배율들 | 0 이라 무효 — 판정 자체를 건너뛴다(D-109) |
| ② + 벨보이 모자 | **무적이 먼저** — 0.3초 동안 맞아도 모자가 닳지 않는다(D-108 · T-111) |
| ② + 디버프 DoT(화상 · 출혈) | 이미 걸린 DoT 의 틱도 0.3초 동안 0. 새 디버프는 안 걸린다(피격이 통째로 무효) |
| ② + 탑승 공유 | **내가 맞으면** 통째로 무효 → 내 위 탑승자 · 내가 탄 청소기로 공유도 안 간다. **탑승자가 맞으면** 탑승자는 맞고 나에게 오는 공유분만 0 |
| ② + 탑승칩② 대신 맞는 범퍼(§12) | 나에게 넘어오는 20% 도 0.3초 동안 0 |
| ② + 공기탄칩① 아군 오폭(§13) | 0.3초 동안 0 |
| ② + 오염 | 오염 중 체력 피해도 0 |
| ② + 물 부족 체력 대납 · ⑧ 피의 계약 | 물값으로 내는 체력은 **막지 않는다** — `ExecCalc_WaterCost` 가 Health 를 직접 깎고, `CommitAbility` 가 무적보다 먼저다 |
| ② + 피해 GE 밖의 효과(적 GA 가 직접 거는 `LaunchCharacter` · 스턴 GE 등) | 막지 못한다(§14.8-58) |
| ③ + 쿨다운 0.3초 | 공중 Q 2회도 0.3초 간격 |
| ③ + ⑧ 피의 계약 | 체력 20 × 비율 |
| 선행 변경 + ⑧ 피의 계약 | 이제 점프마다 체력 30 × 비율(지금은 0) |
| ①②③ + 탑승칩 · 공기탄칩 | 다른 그룹이라 하나씩 함께 낄 수 있다 |

### 14.3 설계 결정

| # | 결정 | 근거 |
|---|---|---|
| **D-102** | 그룹 키 = 실제 GA 태그 **`Abilities.RobotVacuum.JetJump`**. 기능 스위치는 ② 하나 — `Upgrade.Feature.Skill.JetJump.Guard`. ①③ 은 모디파이어만 | GA 의 `AbilityTags` 가 이 태그 하나라 업그레이드 키 = 그룹 키(D-93 과 같은 방식). ①③ 은 전부 정적 수치라 카탈로그로 끝난다(D-2) |
| **D-103** | ★선행 변경★ 물 소모 버그 수정 = `GA_VacuumJetJump` 의 **Cost Gameplay Effect Class = `GE_Cost_VacuumJetJump`**(에셋만, C++ 변경 없음). 돌진의 Cost GE 는 건드리지 않는다 | 사용자 확정. `ApplyCost` 가 이미 SetByCaller `Cost.Water = GetEffectiveWaterCostFor()` 로 채우므로 GE 만 붙이면 `SkillWaterCost`(칩③)까지 그대로 흐른다. 돌진은 `WaterCost` 0 이라 그 GE 를 들고 있어도 무해(§14.8-63) |
| **D-104** | ① 피해 = 기존 축 `SkillDamage` **+1.0**(양쪽 ×2) + 새 축 **`SkillOuterDamage`**(약판정에만 곱하는 배율) **+0.5** → 40 / 15. 물 점프가 이때 처음으로 `SkillDamage` 를 읽는다: 강판정 = `Inner × SkillDamage`, 약판정 = `Outer × SkillDamage × SkillOuterDamage` | 두 판정의 배율이 달라(×2 / ×3) 축 하나로는 안 된다. `SkillDamage` 를 양쪽에 걸어 두면 ②의 "피해 0" 이 모디파이어 한 줄로 양쪽을 덮는다. 절대값(`ChipTuning`)은 카드에 단점 · 수치가 안 뜬다(D-2) |
| **D-105** | 상승 = 새 축 **`SkillLaunchHeight`**(스킬 단위 배율, 기준 1.0, ★**실제 최고 높이**★ 기준) → 임펄스 = `JumpImpulseZ × √배율`. ① **−0.3** · ③ **−0.25** | 사용자 확정(둘 다 실제 높이). 높이 = v² ÷ 2g 라 임펄스에 배율을 그대로 곱하면 높이는 제곱으로 준다(T-109). 카드의 "−30%" 가 플레이어가 보는 높이와 일치한다 |
| **D-106** | ③ 물 = `SkillWaterCost` **Flat −10**(30 → 20) | Percent −1/3 은 30 × 0.6666667 = 20.000001 이라 **물이 정확히 20 일 때 시전 실패**(T-110). Flat 은 정수로 떨어진다. 낮을수록 좋은 축이라 카드에 초록으로 뜬다 |
| **D-107** | ③ 공중 횟수 = 새 축 **`SkillAirUseCount`**(Flat, 정수) **+1** + GA `MaxAirJumps`(기본 1) + 캐릭터 `bAirJumpUsed`(bool) → **`AirJumpsUsed`(uint8 횟수)**. `CanActivateAbility` 는 `GetUpgradeRuntimeFor(ActorInfo)` 로 읽는다 | 정적 수치라 카탈로그(D-2). "지상 Q → 공중 Q × 한도" · "스페이스 → 공중 Q × 한도" 규칙이 그대로 일반화된다. `CanActivateAbility` 는 CDO 에서도 불릴 수 있어 현재 ActorInfo 판(`GetUpgradedInt`)을 쓰면 칩이 안 읽힌다(T-114) |
| **D-108** | ② 무적 = GE **`GE_JetJump_Guard`**(Has Duration · SetByCaller `Data.Duration` 0.3초 · 모디파이어 없음 · Target Tags **`State.Invulnerable`**)를 `ActivateAbility`(서버) **`CommitAbility` 뒤 · 발사 앞**에 `ApplyTimedTagEffect` 로 건다. 판정은 **기본 관문 `UDRAttributeSet::PostGameplayEffectExecute` IncomingDamage 분기 맨 앞 — ★모자보다 먼저★** — 에서 `State.Invulnerable` 이면 피해 0 + return | 사용자 확정(피격 통째로 무효). 모자와 같은 자리 · 같은 이유 — 근접 · 원거리 · DoT · 탑승 공유가 전부 지나는 유일한 관문이고, return 하면 히트리액트 · 넉백 · 디버프 · 전투 진입이 모두 없다. 모자 뒤에 두면 무적 중에 모자가 닳는다(T-111). 태그는 범용(`State.*`) — 적 · 다른 스킬도 GE 만 걸면 같은 무적을 쓴다 |
| **D-109** | ② 피해 0 = `SkillDamage` **−1.0**(×0, 카드에 빨간 "피해 0"). 최종 강판정 · 약판정이 **둘 다 0 이하면 AoE 판정 자체를 건너뛴다**(한쪽만 0 이면 그 대상만 건너뜀). 큐(물 분사 연출)는 그대로 | 단점이 카드에 보이고(D-2), 0 피해 GE 를 적에게 걸지 않아 ExecCalc · 큐 · 적 관문을 헛돌지 않는다(적 관문은 0 피해를 이미 무시하지만 명시적으로 끊는다) |
| **D-110** | 새 축 3개(`SkillOuterDamage` · `SkillLaunchHeight` · `SkillAirUseCount`)는 모두 **스킬 단위 · 높을수록 좋음 · 초 아님** → `DRIsSkillStat` 에만 넣는다. 위치는 **구현 시점의 `Count` 바로 앞**(§13 `SkillChargeGrowth` 보다 먼저 구현하면 그 앞) | T-30 · T-12. 배율 · 횟수라 `DRIsTimeStat` · `DRIsLowerBetter` 에 넣으면 카드 색 · 단위가 틀린다 |

> 기각: ① 을 `ChipTuning` 절대값(강 40 / 약 15)으로 — 카드에 수치 · 단점이 안 뜨고, 기본 피해를 바꾸면 어긋난다(D-2).
> 기각: ③ 공중 횟수를 기능 스위치 + `ChipTuning` 으로 — 정적 정수라 카탈로그가 원칙이다.
> 기각: ② 무적을 플레이어 `HandleIncomingDamage` 안에서 — 배율 · 로비 클램프가 이미 지나가고, 적 · 사이트에서 같은 태그를 못 쓴다(T-112).
> 기각: ② 무적을 콜리전 끄기(두더지 잠수 방식)로 — DoT · 탑승 공유 피해는 콜리전과 무관하게 들어온다.

### 14.4 C++ 변경 (적용 순서)

**① ★선행★ — C++ 없음.** 에셋 1곳(§14.5-1). `ApplyCost` · `ExecCalc_WaterCost` 는 그대로 쓴다(D-103).

**② `DRUpgradeTypes.h` · `DRUpgradeUILibrary.cpp`** — 새 축 3개 (D-104 · D-105 · D-107 · D-110)

```cpp
	// ===== Plan5(물점프칩) 추가 =====
	// ★신규 값은 반드시 Count 바로 앞에 붙인다.★ (T-30) — Plan5(공기탄칩) SkillChargeGrowth 보다 먼저 구현하면 그 앞이 된다

	/**
	 * 스킬 단위. 판정이 두 겹(내부 강판정 / 외부 약판정)인 스킬의 ★외부(약판정) 피해에만★ 곱하는 배율.
	 * SkillDamage 는 양쪽에 걸리고, 이 축은 그 위에 외부에만 한 번 더 곱해진다 —
	 *     외부 피해 = ApplySkill(SkillOuterDamage, ApplySkill(SkillDamage, OuterDamage))
	 * ⚠️ 지금은 물 점프만 읽는다. 씨앗 폭탄 등에 쓰려면 그 스킬도 읽게 해야 한다 (Plan5 D-104 · §9-60)
	 */
	SkillOuterDamage		UMETA(DisplayName = "스킬 외곽 피해"),

	/**
	 * 스킬 단위. 점프 · 발사형 이동기의 ★실제 최고 높이 배율★ — 기준 1.0 에 Apply 해서 쓴다.
	 * ★높이는 속도의 제곱에 비례★ 하므로 임펄스에는 √배율을 곱한다 —
	 *     임펄스 = BaseImpulse × Sqrt(ApplySkill(SkillLaunchHeight, 1.0))   // 0.7 → ×0.837 → 높이 ×0.70
	 * 배율이라 DRIsTimeStat 에 ★넣지 않는다★ (Plan5 D-105 · T-109)
	 */
	SkillLaunchHeight		UMETA(DisplayName = "스킬 상승 높이"),

	/**
	 * 스킬 단위. 공중(떨어지는 중)에서 그 스킬을 쓸 수 있는 ★횟수★. 착지하면 다시 채워진다.
	 * Flat 으로 쓴다(+1 = 한 번 더). 정수 — GA 가 반올림해 읽는다. (Plan5 D-107)
	 */
	SkillAirUseCount		UMETA(DisplayName = "스킬 공중 사용 횟수"),

	Count UMETA(Hidden)
```

- `DRIsSkillStat()` 에 세 줄 추가(`// Plan5(물점프칩)`) — ★빠뜨리면 전역 블록에 쌓여 청소기 모든 스킬에 퍼진다(T-12 · T-117)★. `DRIsLowerBetter` · `DRIsTimeStat` 에는 넣지 않는다.
- `DRUpgradeUILibrary.cpp` 짧은 이름(5자 제한): `SkillOuterDamage` → `"외곽 피해"` · `SkillLaunchHeight` → `"상승 높이"` · `SkillAirUseCount` → `"공중 횟수"`.

**③ `DRGameplayTags.h / .cpp`** — 태그 2개(네이티브)

| 멤버 | 태그 | 종류 |
|---|---|---|
| `Upgrade_Feature_Skill_JetJump_Guard` | `Upgrade.Feature.Skill.JetJump.Guard` | ② 기능 스위치 |
| `State_Invulnerable` | `State.Invulnerable` | ★GAS 태그★ — `GE_JetJump_Guard` 가 부여, 기본 피해 관문이 읽는다. 범용(적 · 다른 스킬도 GE 만 걸면 같은 무적) |

**④ `DRProgressionConfig.h`** — `FDRAirShotChipTuning` 다음(§13 보다 먼저 구현하면 `FDRAttackSpeedBuffChipTuning` / `FDRRideChipTuning` 다음 — 구현 순서대로)

```cpp
/**
 * ★Plan5(물점프칩)★ 청소기 물 점프 칩의 동적 상수.
 * 피해 · 상승 높이 · 물 · 공중 횟수는 여기가 아니라 DA_ChipCatalog 모디파이어다.
 */
USTRUCT(BlueprintType)
struct FDRJetJumpChipTuning
{
	GENERATED_BODY()

	/**
	 * ② 무적 표식. 요구 사양 (Plan5 D-108):
	 *   - Has Duration / Duration = ★SetByCaller(Data.Duration)★ (Scalable Float 면 GuardDuration 이 무시된다 — T-113)
	 *   - Modifier 없음 / Target Tags(Granted) = State.Invulnerable
	 *   - Stacking = Aggregate by Target · Limit 1 · Refresh on Successful Application
	 * ★비우면 칩 ②가 무적 없이 피해 0 만 남는다★ — 검사가 잡는다
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JetJump|Guard")
	TSoftClassPtr<UGameplayEffect> GuardEffect;

	// 무적 시간(초). 물 점프를 쓰는 순간부터
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JetJump|Guard", meta = (ClampMin = "0.01"))
	float GuardDuration = 0.3f;
};
```

- `FDRChipTuning` 끝에 `UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "JetJump") FDRJetJumpChipTuning JetJump;`

**⑤ `DRAbilitySystemLibrary.h / .cpp`** — 무적 판정 1개 (D-108)

```cpp
	// ========== 무적 (Plan5(물점프칩)) ==========

	/** ★Plan5(물점프칩)★ 대상이 무적(State.Invulnerable)인가. 기본 피해 관문이 ★모든 피해마다★ 부른다 — 태그 조회 한 번 */
	static bool IsInvulnerable(const UAbilitySystemComponent* ASC);
```

```cpp
bool UDRAbilitySystemLibrary::IsInvulnerable(const UAbilitySystemComponent* ASC)
{
	return ASC && ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().State_Invulnerable);
}
```

**⑥ `DRAttributeSet.cpp` `PostGameplayEffectExecute`** — IncomingDamage 분기 맨 앞, 모자 **위** (D-108)

```cpp
	else if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		// ★Plan5(물점프칩)★ 무적(State.Invulnerable) — 어떤 피해든 통째로 무효 (D-108).
		// 모자와 같은 자리 · 같은 이유: 근접 · 원거리 · DoT · 탑승 공유가 전부 지나는 유일한 관문이고,
		// 여기서 return 하면 히트리액트 · 넉백 · 디버프 · 탑승 공유 · 전투 진입이 모두 없다.
		// ★모자보다 먼저★ — 무적 중에 들어온 피해에 모자가 닳으면 안 된다 (T-111)
		if (GetIncomingDamage() > 0.f && UDRAbilitySystemLibrary::IsInvulnerable(Props.TargetASC))
		{
			SetIncomingDamage(0.f);
			return;
		}

		// ★Plan8(모자)★ 모자 보호막 — 어떤 피해든 1회 무효 (Plan8 D-21)
		...(그대로)...
```

**⑦ `DRRobotVacuumCharacter.h / .cpp`** — 공중 사용 bool → 횟수 (D-107)

```cpp
// .h  "========== 더블 점프 ==========" 아래 — bAirJumpUsed 를 대체
	// 공중 Q 사용 횟수 (서버 전용 판정 값 — 복제 불필요). Landed()에서 0 으로
	// ★Plan5(물점프칩)★ bool(bAirJumpUsed) → 횟수. 한도는 GA 의 MaxAirJumps(칩③ SkillAirUseCount +1) (D-107)
	uint8 AirJumpsUsed = 0;

// .cpp Landed()
		AirJumpsUsed = 0;   // ★Plan5(물점프칩)★ (예전 bAirJumpUsed = false)
```

- `bAirJumpUsed` 는 **4곳**(헤더 · `Landed` · `CanActivateAbility` · `ActivateAbility`)뿐이다(UPROPERTY 가 아니라 BP 참조 없음) — grep 으로 전부 바꾼다(T-115).

**⑧ `DRVacuumJetJump.h / .cpp`** — ① 피해 · ①③ 높이 · ② 무적 · ③ 공중 횟수 (D-104 · D-105 · D-107 · D-108 · D-109)

```cpp
// .h — 클래스 주석의 "공중 사용은 bAirJumpUsed로 1회 제한" 을 "AirJumpsUsed 로 MaxAirJumps(칩 반영)회 제한" 으로
// .h protected
	// ★Plan5(물점프칩)★ 공중 사용 가능 횟수 기본값 (칩③ SkillAirUseCount Flat +1 → 2). 착지하면 다시 채워진다
	UPROPERTY(EditDefaultsOnly, Category = "JetJump", meta = (ClampMin = "0"))
	int32 MaxAirJumps = 1;

	// ★Plan5(물점프칩)★ 칩이 반영된 공중 사용 가능 횟수.
	// ★CanActivateAbility 는 CDO 에서 불릴 수 있어 ActorInfo 를 직접 받는다★ — GetUpgradedInt 는 현재 ActorInfo 판이다 (T-114)
	int32 GetMaxAirJumpsFor(const FGameplayAbilityActorInfo* ActorInfo) const;
```

```cpp
int32 UDRVacuumJetJump::GetMaxAirJumpsFor(const FGameplayAbilityActorInfo* ActorInfo) const
{
	const float Upgraded = GetUpgradeRuntimeFor(ActorInfo)
		.ApplySkill(GetUpgradeKeyTag(), EDRUpgradeStat::SkillAirUseCount, static_cast<float>(MaxAirJumps));
	return FMath::Max(0, FMath::RoundToInt(Upgraded));
}

// CanActivateAbility()
	// 공중 사용 횟수 제한 — AirJumpsUsed 는 서버 전용 값이므로 실제 차단은 서버 판정에서 이뤄짐 (Plan3 §7.1)
	// ★Plan5(물점프칩)★ 1회 고정 → GetMaxAirJumpsFor (칩③ +1) (D-107)
	if (const ADRRobotVacuumCharacter* Vacuum = Cast<ADRRobotVacuumCharacter>(ActorInfo->AvatarActor.Get()))
	{
		if (Vacuum->GetCharacterMovement()->IsFalling() && Vacuum->AirJumpsUsed >= GetMaxAirJumpsFor(ActorInfo))
		{
			return false;
		}
	}
```

```cpp
// ActivateAbility() — CommitAbility 성공 뒤, 서버 블록
	if (Avatar && Avatar->HasAuthority())
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();

		// ★Plan5(물점프칩)★ ② 0.3초 무적 — 쓰는 순간부터. 물값은 위 CommitAbility 에서 이미 냈다 (D-108).
		//   여기서는 표식(State.Invulnerable)만 건다. 막는 건 기본 피해 관문(UDRAttributeSet)이다
		if (UDRAbilitySystemLibrary::HasUpgradeFeature(ASC, FDRGameplayTags::Get().Upgrade_Feature_Skill_JetJump_Guard))
		{
			const FDRJetJumpChipTuning& Tuning = UDRAbilitySystemLibrary::GetChipTuning(Avatar).JetJump;
			UDRAbilitySystemLibrary::ApplyTimedTagEffect(Avatar, ASC, Tuning.GuardEffect, Tuning.GuardDuration);
		}

		ADRRobotVacuumCharacter* Vacuum = Cast<ADRRobotVacuumCharacter>(Avatar);
		if (Vacuum)
		{
			// 공중 사용만 센다 → "지상 Q → 공중 Q × 한도" / "스페이스 점프 → 공중 Q × 한도"
			if (Vacuum->GetCharacterMovement()->IsFalling())
			{
				++Vacuum->AirJumpsUsed;   // ★Plan5(물점프칩)★ bool → 횟수 (D-107)
			}
			// 애님 플래그 (Landed()에서 해제)
			Vacuum->SetJetJumping(true);

			// ★Plan5(물점프칩)★ ①③ 상승 높이 배율 — ★높이는 속도의 제곱에 비례★ 하므로 임펄스에는 √배율 (D-105 · T-109).
			//   0.7 → 임펄스 ×0.837 → 최고 높이 ×0.70. 칩이 없으면 1.0 이라 JumpImpulseZ 그대로
			const float HeightScale = FMath::Max(0.f, GetUpgradedFloat(EDRUpgradeStat::SkillLaunchHeight, 1.f));
			Vacuum->LaunchCharacter(FVector(0.f, 0.f, JumpImpulseZ * FMath::Sqrt(HeightScale)), false, true);
		}

		// ===== AoE 데미지: Z 무관 2D 반경 (Plan3 §7.1) =====
		// ★Plan5(물점프칩)★ 강판정 = Inner × SkillDamage / 약판정 = Outer × SkillDamage × SkillOuterDamage (D-104).
		//   ★GetUpgradedDamage() 를 쓰지 않는다★ — 그 기준인 GA Damage(스케일러블)는 이 스킬에서 0 이다 (T-118)
		const float FinalInner = GetUpgradedFloat(EDRUpgradeStat::SkillDamage, InnerDamage);
		const float FinalOuter = GetUpgradedFloat(EDRUpgradeStat::SkillOuterDamage,
			GetUpgradedFloat(EDRUpgradeStat::SkillDamage, OuterDamage));

		// ★Plan5(물점프칩)★ ② 피해 0 이면 판정 자체를 건너뛴다 — 0 피해 GE 를 적에게 걸지 않는다 (D-109)
		const FVector Origin = Avatar->GetActorLocation();
		if (FinalInner > 0.f || FinalOuter > 0.f)
		{
			TArray<AActor*> Candidates;
			UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(this, Candidates, { Avatar }, OuterRadius * 1.5f, Origin);

			for (AActor* Target : Candidates)
			{
				if (!IsValid(Target)) continue;
				if (!UDRAbilitySystemLibrary::IsNotFriend(Avatar, Target)) continue;   // 적만

				const float Dist2D = FVector::DistXY(Origin, Target->GetActorLocation());
				if (Dist2D > OuterRadius) continue;

				const float HitDamage = (Dist2D <= InnerRadius) ? FinalInner : FinalOuter;   // SeedProjectile 내/외 패턴
				if (HitDamage <= 0.f) continue;

				FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(Target);
				Params.BaseDamage = HitDamage;
				UDRAbilitySystemLibrary::ApplyDamageEffect(Params);
			}
		}

		// 물 분사 VFX/SFX — ②(피해 0)여도 그대로 (기존 코드)
		...
	}
```

**⑨ `DRProgressionConfig.cpp`** — 검사(칩이 카탈로그에 있을 때만)

| 조건 | 검사 | 틀리면 |
|---|---|---|
| `IsFeatureGated(JetJump.Guard)` | `GuardEffect` 비었나 / 부여 태그 `State.Invulnerable` / `DurationPolicy == HasDuration` / Duration 이 **SetByCaller** / `GuardDuration > 0` | 무적 없음(피해 0 만 남음) / 무적이 안 읽힘 / 영구 무적 · 즉시 사라짐 / 튜닝 값 무시(T-113) |

**바뀔 C++ 파일**: `DRUpgradeTypes.h` · `DRUpgradeUILibrary.cpp` · `DRGameplayTags`(.h/.cpp) · `DRProgressionConfig`(.h/.cpp) · `DRAbilitySystemLibrary`(.h/.cpp) ·
`DRAttributeSet.cpp` · `DRRobotVacuumCharacter`(.h/.cpp) · `DRVacuumJetJump`(.h/.cpp). **BP 재배선 없음**(에셋은 GA Cost GE 1칸 + 새 GE 1개 + 데이터).

### 14.5 에디터 작업

1. ★**선행 (C++ 빌드 전에도 가능)**★ `GA_VacuumJetJump`(`Content/Blueprints/AbilitySystem/Player/VacuumCleaner/Abilities/Skill3/`) → Class Defaults → Costs → **Cost Gameplay Effect Class = `GE_Cost_VacuumJetJump`**. 돌진(`GA_VacuumDash`)의 Cost GE 는 그대로 둔다(D-103).
2. **`GE_JetJump_Guard`** — 같은 Skill3 폴더에 새 GameplayEffect BP
   - Duration Policy **Has Duration** · Duration Magnitude **Set by Caller** `Data.Duration` (★Scalable Float 금지 — T-113★)
   - Modifiers 없음 / Components → **Target Tags** `State.Invulnerable`
   - Stacking **Aggregate by Target** · Stack Limit Count **1** · **Refresh on Successful Application**
   - (선택) GameplayCue — 0.3초 무적 연출(물막 · 반짝임). §14.8-59
3. **`DA_ProgressionConfig` → Chip Tuning → Jet Jump**: `GuardEffect = GE_JetJump_Guard` · `GuardDuration 0.3`(C++ 기본값).
4. **`DA_ChipCatalog`** → Chips +3 (§14.1 표). 공통: `OwnerClass RobotVacuum` · `bCommonChip` 끔 · `Category Ascension` · `TargetAbilityTag Abilities.RobotVacuum.JetJump` · `RequiredSlotCount 1`.
   ★**모든 모디파이어**(Flat 포함)의 `TargetAbilityTag` 도 `Abilities.RobotVacuum.JetJump`★(D-51 — 빠지면 청소기 모든 스킬에 퍼진다)
   - ① `RV.Asc.JumpBlast` · 폭압 분사구 · 요약 "물 점프 피해 강화" · Tier 3
     · Modifiers: `SkillDamage` Percent **+1.0**(`DetailOverride` "강판정 피해 20 → 40") / `SkillOuterDamage` Percent **+0.5**(`DetailOverride` "약판정 피해 5 → 15") / `SkillLaunchHeight` Percent **−0.3** `bIsDrawback` ✓
     · Description "물 점프의 피해가 커집니다 — 가까운 적(2m 안) 20 → 40, 먼 적(2~4m) 5 → 15. 대신 점프 높이가 30% 낮아집니다."
   - ② `RV.Asc.JumpGuard` · 완충 분사구 · 요약 "점프 순간 0.3초 무적" · Tier 4 · GrantedTags `Upgrade.Feature.Skill.JetJump.Guard`
     · Modifiers: `SkillDamage` Percent **−1.0** `bIsDrawback` ✓(`DetailOverride` "피해 0")
     · Description "물 점프를 쓰는 순간부터 0.3초 동안 어떤 공격도 받지 않습니다(피해 · 경직 · 넉백 · 디버프 모두). 대신 물 점프가 적에게 피해를 주지 않습니다."
   - ③ `RV.Asc.JumpMulti` · 다단 분사구 · 요약 "공중 2회 · 물 20" · Tier 3
     · Modifiers: `SkillWaterCost` **Flat −10**(`DetailOverride` "물 소모 30 → 20") / `SkillAirUseCount` **Flat +1**(`DetailOverride` "공중 사용 1 → 2회") / `SkillLaunchHeight` Percent **−0.25** `bIsDrawback` ✓
     · Description "물 점프의 물 소모가 30에서 20으로 줄고, 공중에서 2번까지 쓸 수 있습니다. 대신 점프 높이가 25% 낮아집니다."
   - `SkillDisplayNames` 에 `Abilities.RobotVacuum.JetJump → 물 점프`
5. (선택) 아이콘 `Content/DaeRuneAssets/UI/Upgrade/Icon/Vacuum/공격-8 · 지원-8 · 편의-8` (탑승 -6 · 공기탄 -7)
6. (선택) 무적 상태 아이콘은 넣지 않는다 — 0.3초라 `DA_StatusEffectInfo` 위젯이 깜빡이기만 한다. 연출이 필요하면 2번의 GameplayCue
7. 검사: `DA_ProgressionConfig` 편집 → `[ProgressionConfig] … 검증 통과` / 카탈로그 오류 0건(새 축 3개가 스킬 단위로 잡히는지 포함)

### 14.6 함정

| # | 함정 | 대응 |
|---|---|---|
| **T-108** | 선행 변경(Cost GE 연결) 없이 칩③만 넣는다 | 물이 원래 안 빠지므로 "30 → 20" 은 시전 가능 기준만 낮춘다 — 칩이 사실상 단점만 남는다. §14.5-1 을 먼저 |
| **T-109** | 높이 배율을 임펄스에 그대로 곱한다 | 높이 = v² ÷ 2g 라 ×0.7 이 높이 ×0.49(① 202uu), ×0.75 가 ×0.56(③ 232uu)이 된다 — 임펄스엔 `√배율` |
| **T-110** | ③ 물을 `SkillWaterCost` Percent −0.3333 으로 | 30 × 0.6666667 = 20.000001 → 물이 정확히 20 일 때 `CheckCost` 실패. **Flat −10** |
| **T-111** | 무적 검사를 모자 **뒤**에 둔다 | 무적 0.3초 동안 맞은 피해에 모자가 닳는다(D-108) |
| **T-112** | 무적 검사를 `UDRPlayerAttributeSet::HandleIncomingDamage` 안에 둔다 | 위치에 따라 디버프 · 탑승 공유 · 전투 진입이 먼저 지나가고, 적 · 사이트에서는 같은 태그가 안 먹는다 — 기본 관문 맨 앞 |
| **T-113** | `GE_JetJump_Guard` Duration 을 Scalable Float 로 둔다 | `ApplyTimedTagEffect` 가 넣는 `Data.Duration` 이 무시되고 에셋 값으로 돈다(0 이면 즉시 사라짐). 검사가 잡는다 |
| **T-114** | `CanActivateAbility` 에서 `GetUpgradedInt`(현재 ActorInfo 판)로 공중 횟수를 읽는다 | CDO 경로에서 업그레이드 캐시가 비어 칩이 안 읽힌다 → 공중 1회로 막힌다. `GetUpgradeRuntimeFor(ActorInfo)`(CheckCost 와 같은 이유) |
| **T-115** | `bAirJumpUsed` 를 일부만 바꾼다 | `Landed` 리셋을 빠뜨리면 착지 후에도 공중 Q 불가, `ActivateAbility` 를 빠뜨리면 무한 공중 점프. 4곳 전부(§14.4-⑦) |
| **T-116** | `SkillOuterDamage` 를 강판정에도 곱한다 | ① 강판정이 60 이 된다 — 약판정에만 |
| **T-117** | 새 축 3개를 `Count` 앞이 아닌 곳에 넣거나 `DRIsSkillStat` 에서 빠뜨린다 | 기존 칩 효과가 밀리거나(T-30) 전역 블록에 쌓여 청소기 모든 스킬로 퍼진다(T-12 — 예: `SkillDamage` 처럼 같은 칩의 스킬 단위 값과 섞인다). `ValidateCatalog` 가 오류를 내준다 |
| **T-118** | 피해를 `GetUpgradedDamage()` 로 읽는다 | 그 기준은 GA `Damage`(스케일러블) = 0 이라 칩 유무와 무관하게 0 이 된다 — `GetUpgradedFloat(SkillDamage, InnerDamage / OuterDamage)` |

### 14.7 검증

준비: PIE Listen Server · 플레이어 2(청소기 1 + 아군). 콘솔 `DRUnlockUpgrade` → `DRUnlockSlot 6`. 수치는 HUD 물 · 데미지 숫자 · `showdebug abilitysystem`(태그 · 활성 GE). 높이는 같은 자리 벽 · 지붕 높이와 비교.

- [ ] **선행 변경(칩 없음)** Q 1회 → 물 −30 / 물 20 에서 Q → 물 0 + 체력 5(부족분 10 × 0.5) / 물 0 · 체력 낮으면 시전 불가(기존 규칙) / 피해 20 · 5, 높이 · 공중 1회 그대로 / 돌진 동작 변화 없음
- [ ] **①** 2m 안 적 40, 2~4m 적 15 / 높이 약 ×0.7(정점 0.77초) / 물 30 / 광폭화면 40 · 15 에 추가 배율
- [ ] **②** 피해 숫자 없음 · 적 히트리액트 · 어그로 없음 · 물 분사 연출은 나옴 / Q 직후 0.3초 안에 맞으면 체력 · 컨테이너 그대로, 경직 · 넉백 · 새 디버프 없음 / 화상 중이면 0.3초 동안 틱 0 / 모자가 있으면 모자 유지 / 0.3초 뒤엔 정상 피격 / 지상 Q → 0.3초 뒤 공중 Q → 0.6초 연속 무적 / 내 위 탑승자가 맞으면 탑승자만 맞음 · 내가 맞으면 탑승자에게도 공유 없음 / 물이 모자라 체력으로 낼 때 그 체력은 빠짐 / `showdebug` 에 `State.Invulnerable` 0.3초
- [ ] **③** 물 −20(물 정확히 20 에서도 시전) / 지상 Q → 공중 Q → 공중 Q(3회) · 4번째 거부 / 스페이스 → 공중 Q 2회 / 착지하면 다시 2회 / 높이 약 ×0.75
- [ ] **스킬당 1칩** ①②③ 중 하나만, 탑승칩 · 공기탄칩과는 함께 / 미리보기 라벨 "물 점프" / 카드: ① "강판정 피해 20 → 40" · "약판정 피해 5 → 15"(초록) · "상승 높이 −30%"(빨강) / ② "피해 0"(빨강) / ③ "물 소모 30 → 20" · "공중 사용 1 → 2회"(초록) · "상승 높이 −25%"(빨강)
- [ ] **멀티** 클라 청소기 ② → 호스트 쪽 적이 그 청소기를 0.3초 안에 때려도 0 / 클라 청소기 ③ → 공중 3번째 Q 가 서버에서 거부되고 클라 화면이 튀지 않음(예측 취소 — §14.8-61)
- [ ] **검사** `DA_ProgressionConfig` 편집 → 오류 0건 / `GuardEffect` 를 비우거나 Duration 을 Scalable Float 로 바꾸면 오류

### 14.8 열린 결정 · 알려진 제약 (§9 번호에 이어서)

| # | 내용 | 현재 판단 |
|---|---|---|
| **56** | 이름 · Tier · 아이콘 · `ChipId`(`RV.Asc.Jump*`)는 가칭 | 사용자가 정하면 카탈로그만(`ChipId` 는 첫 입력 전에 확정 — 세이브 키) |
| **57** | ★선행 변경(D-103)으로 칩 없는 물 점프가 물 30 을 실제로 쓴다★ — 이동기 사용 빈도 · 물 경제가 바뀐다 | 사용자 확정. 플레이 확인 후 `WaterCost`(C++ 생성자 — BP 미변경)만 조정 |
| **58** | ② 무적은 **피해 GE(IncomingDamage)를 지나는 것만** 막는다 — 적 GA 가 직접 거는 `LaunchCharacter` · 스턴 GE 등은 못 막는다 | 그런 공격이 있는지는 이번 조사 범위 밖. 필요하면 그 GA 에 `State.Invulnerable` 검사 한 줄 |
| **59** | ② 무적 연출이 없다 — 막았다는 표시(모자의 `HatBlock` 큐 같은 것) 없음 | 선택: `GE_JetJump_Guard` 에 GameplayCue(§14.5-2), 막을 때 큐가 필요하면 관문에 한 줄 |
| **60** | `SkillOuterDamage` 는 지금 물 점프만 읽는다 | 씨앗 폭탄 등 다른 2단 판정 스킬에 쓰려면 그 스킬도 읽게 한다 |
| **61** | 공중 횟수는 서버 전용 카운터 — 클라는 늘 허용으로 예측해, 한도를 넘긴 Q 가 클라에서 잠깐 발동했다 취소될 수 있다(지금의 3번째 Q 와 같은 현상) | 기존 구조 유지. 거슬리면 `AirJumpsUsed` 를 OwnerOnly 복제 |
| **62** | 높이 수치는 중력 −980 · GravityScale 1 · 공기 저항 없음 가정 | 중력을 바꿔도 비율(×0.70 · ×0.75)은 그대로다 |
| **63** | `GA_VacuumDash` 가 `GE_Cost_VacuumJetJump` 를 Cost GE 로 들고 있다 | 돌진 `WaterCost` 0 이라 무해 — 건드리지 않는다. 정리하려면 비운다(선택) |
| **64** | ② 지상 Q → 공중 Q 로 한 번 뜰 때 최대 0.6초 연속 무적 · 물 60 | 의도대로(사용 1회당 0.3초). 쿨다운(0.3초 고정 GE)이 무적보다 짧아지면 무적이 끊김 없이 이어질 수 있다 — 지금 그런 칩은 없다 |

### 14.9 작업 순서

1. 선행 변경(§14.5-1, 에셋만) → PIE → §14.7 "선행 변경"
2. 칩 C++ ②~⑨ → 빌드 (`UENUM` · 태그 · `USTRUCT` 추가라 **에디터를 닫고** — T-45)
3. 에디터 §14.5-2~7
4. §14.7 검증 → §1 · §3 · §5 · §6 · §9 에 흡수(§1.7 그룹 +1, 스탯 축 표에 새 축 3개), 머리말 상태 갱신

### 14.10 구현 결과 — 계획과 달라진 점

2026-10-08 C++ 구현 · 빌드 통과. 다른 점:

- 칩② 인데 `ChipTuning.JetJump.GuardEffect` 가 비어 있으면 물 점프를 쓸 때마다 경고 로그 `[JetJump] … 미지정` 을 남긴다(계획에는 Config 검사만 있었다).
- `AirJumpsUsed` 는 255 에서 더 오르지 않는다(uint8 넘침 방지).
- 선행 변경(D-103 — Cost GE 연결)은 에셋 작업이라 아직이다 → §16.1-1.

---

## 15. 로봇 청소기 돌진(스킬 2 · RMB) 칩 3종 (C++ 구현 완료 — 에디터 · 검증 남음)

> 2026-10-07 요청 · 같은 날 질의응답으로 확정. 코드 주석 접두 **`Plan5(돌진칩)`** · 결정 **D-111 ~ D-119** · 함정 **T-119 ~ T-130** (앞 번호에 이어서 매긴 영구 ID).
> **C++ 구현 완료**(2026-10-08 빌드 통과). 에디터 작업은 **§16 안내서**, 그다음 §15.7 검증. 계획과 달라진 점은 §15.10. 검증이 끝나면 §1~§9 에 흡수한다.
> ★선행 변경 1건 — **D-112**(돌진 자해가 지금은 **한 번도 안 들어간다** → 고치면서 조건을 "지속 돌진 중 지형 충돌"로) — 은 칩이 없어도 돌진을 바꾼다(§15.8-66).★
>
> 사용자 확정
> - ① 게이지 1칸당 **적에게** 주는 충돌 피해 **10 → 18**(아군 충돌 피해 칸당 5 는 그대로). 대신 지형지물 충돌 시 받는 피해 **50 → 100**.
> - ② 돌진 중 **아군과 부딪히지 않고 통과**한다. 대신 **적에게** 주는 충돌 피해 **−15%**.
> - ③ 게이지 1칸 충전 시간 **0.5 → 0.35초**. 대신 칸당 이속 버프 **+20% → +14%**.
> - 돌진 자해(지금 0 — 버그)는 고치고, 조건은 **5칸 지속 돌진 중 벽 · 지형지물에 부딪힐 때만**이다(적 · 아군과 부딪힐 때는 자해 없음).

### 15.0 지금의 돌진 (코드·에셋 실측)

| 항목 | 지금 | 어디 |
|---|---|---|
| 스킬 | `GA_VacuumDash`(RMB) — `UDRVacuumDash : UDRDamageGameplayAbility`. `AbilityTags = Abilities.RobotVacuum.Dash`(업그레이드 키). BP 그래프는 `StartChargingGauge`(누름) · `ReleaseGauge`(뗌) · `OnBrakePressed`(S 브레이크 이벤트) 세 노드를 부르기만 한다. 게이지 · 물 · 버프 · 충돌 처리는 전부 **서버** | GA BP · `DRVacuumDash.cpp` |
| 수치 | BP 가 덮은 값 없음 → C++ 기본값 그대로: `MaxGauge` **5** · `ChargeInterval` **0.5초** · `DecayInterval` **1초** · `WaterPerGauge` **20** · `EnemyDamagePerGauge` **10** · `PlayerDamagePerGauge` **5** | GA CDO |
| 충전 | 누르는 동안 `ChargeInterval` 마다 1칸(물 20, 부족분 ×0.5 는 체력 대납, 그마저 부족하면 충전 일시 정지). 5칸이면 대기. HUD 는 칸 수만 받는다(시간 정보 없음) | `TickCharge` · `NotifyVacuumDashGaugeChanged` |
| 돌진 | 떼면 `GE_VacuumDashSpeedBuff` 를 칸 수만큼 쌓고 충돌 판정을 켠다. **1~4칸** = 1초마다 1칸 감쇠, 0 이면 끝 / **5칸** = 지속 돌진(자동 전진 · 감쇠 없음) — 충돌 · S 브레이크로만 끝 | `ReleaseGauge` · `TickDecay` |
| 이속 버프 | `GE_VacuumDashSpeedBuff`: Infinite · MoveSpeed **MultiplyAdditive 1.2**(ScalableFloat) · Aggregate by Target · Limit 5 · 에셋 태그 `Buff.RobotVacuum.DashSpeed`(공기탄 반동 예외 판정이 읽는다). 기본 이속 550 → 칸당 +110(5칸 1100) | 에셋 |
| 충돌 감지 | 캡슐 `OnComponentHit`(서버 · 돌진 중에만 · 1회). **Pawn**(적 · 다른 플레이어)이면 무조건, Pawn 이 아니면 **수직면**(노말 Z < 0.7)이고 속도 350 이상일 때만. 내 위 탑승자 · 내가 탄 청소기는 제외 | `ADRRobotVacuumCharacter::OnCapsuleHit` |
| 충돌 피해 | 맞은 액터에 ASC 가 있으면 `G × (적 10 / 아군 5)` — `IsNotFriend` 로 적 · 아군 구분, `ApplyDamageEffect` 직접(아군 필터 없음). 그리고 **돌진이 끝난다**(Crush 몽타주). ★`SkillDamage` 를 읽지 않는다★(BaseDamage 를 위 값으로 덮는다). 클렌저 사이트는 Pawn 이 아니고 ASC 가 있어 "적"으로 계산된다(D-3) | `HandleDashImpact` |
| ★자해★ | 코드: 지속 돌진(5칸) 중 **무엇이든** 부딪히면 `SelfDamageEffect` 적용. 그런데 ★BP 의 `SelfDamageEffect` 칸에 `GE_VacuumDashSpeedBuff`(이속 버프)가 들어가 있다★ — `GE_VacuumDashSelfDamage`(IncomingDamage AddBase **50**)는 어디서도 참조하지 않는다. → **지금 자해는 0**(이속 버프 1스택이 더해졌다가 바로 `FinishDash` 가 전부 지운다) | GA CDO · 에셋 참조 |
| 기타 BP 값 | `CostGameplayEffectClass = GE_Cost_VacuumJetJump`(돌진 `WaterCost` 0 이라 무동작 — §14.8-63) | GA CDO |
| 아군 충돌 | 청소기 캡슐은 기본 `Pawn` 프로필 — 적과 플레이어가 **같은 Pawn 오브젝트 채널**이라 채널로는 아군만 골라 통과시킬 수 없다. 탑승 · 착석 중인 캐릭터는 이미 `ECC_Pawn` 응답을 Ignore 로 바꾼다 | `BP_DRVacuumCleaner` · `DefaultEngine.ini` · `ADRCharacter::OnRep_MountedOn` |
| 청소기 칩 | 탑승(§12) · 공기탄(§13) · 물 점프(§14) 계획뿐 — 돌진 그룹은 새로 생긴다 | |

**이 설계가 기대는 엔진·코드 사실**

| 항목 | 사실 |
|---|---|
| 이동 무시 목록 | `UPrimitiveComponent::IgnoreActorWhenMoving(Actor, bShouldIgnore)` 는 그 컴포넌트가 **스스로 움직일 때**의 스윕 · 바닥 검사 · 겹침 해소에서 그 액터를 뺀다. ★복제되지 않는다★ — 이동을 시뮬레이션하는 머신(서버 · 각 소유 클라)마다 따로 넣어야 하고, 한쪽만 넣으면 상대가 움직일 때 다시 막힌다 |
| 스택 GE 크기 | MultiplyAdditive 바이어스 1 → 값 v 를 n 스택 = 1 + (v − 1) × n (1.14 × 5 = ×1.7). Compound 면 바이어스 0 이라 v × n(T-95) |
| SetByCaller + 스택 | 같은 GE 를 다시 걸면 기존 인스턴스의 스택만 오르고 크기는 첫 스펙 값을 쓴다 — 한 번 돌진 안에서 칸당 값이 같으니 문제없다. 값을 안 넣고 걸면 에러 로그 + 크기 0 |
| 자해 경로 | `IncomingDamage` 에 직접 → 기본 관문(무적 · 모자 · 받는 쪽 배율 · 탑승 공유)을 모두 탄다. ExecCalc(공격 배율)는 안 탄다 |

### 15.1 칩 3종 스펙

공통: `OwnerClass = RobotVacuum`, `Category = Ascension`, `RequiredSlotCount = 1`,
칩 레벨 · **모든 모디파이어**의 `TargetAbilityTag = Abilities.RobotVacuum.Dash`(실제 GA 태그 — D-111). → ①·②·③ 중 하나만, **탑승 · 공기탄 · 물 점프 칩과는 함께** 낄 수 있다.

| # | `ChipId` | 이름(가칭) | Modifiers | GrantedTags | 효과 | 단점 | Tier | 아이콘(제안) |
|---|---|---|---|---|---|---|---|---|
| ① | `RV.Asc.DashRam` | 충각 범퍼 | `SkillDamage` **+0.8**<br>`SkillSelfDamage` **+1.0** ★단점★ | (없음) | 적 충돌 피해 칸당 **10 → 18** | 지속 돌진 지형 충돌 자해 **50 → 100** | 3 | 공격-9 |
| ② | `RV.Asc.DashPass` | 유령 주행 | `SkillDamage` **−0.15** ★단점★ | `…Dash.PassAllies` | 돌진 중 **아군을 통과**(충돌 · 피해 · 정지 없음) | 적 충돌 피해 칸당 **10 → 8.5** | 4 | 지원-9 |
| ③ | `RV.Asc.DashQuick` | 급속 충전기 | `SkillWindup` **−0.3**<br>`SkillSpeedBonus` **−0.3** ★단점★ | (없음) | 칸당 충전 **0.5 → 0.35초** | 칸당 이속 **+20% → +14%** | 3 | 편의-9 |

`…` = `Upgrade.Feature.Skill`. 모디파이어 Op 는 전부 `Percent`(곱연산).
카드 표기(`DetailOverride`): ① `SkillDamage` "칸당 충돌 피해 10 → 18" · `SkillSelfDamage` "벽 충돌 자해 50 → 100" / ② `SkillDamage` "칸당 충돌 피해 10 → 8.5" /
③ `SkillWindup` "칸당 충전 0.5 → 0.35초" · `SkillSpeedBonus` "칸당 이속 +20% → +14%".
②는 단점이 모디파이어라 검사상 `Description` 필수는 아니지만, 통과 대상을 알려야 하므로 쓴다. 이름 · Tier · 아이콘 · `ChipId` 는 가칭(§15.8-65).

**적 충돌 피해** (공용 칩 · 버프 없음, G = 해소 순간 칸 수)

| | 1칸 | 2칸 | 3칸 | 4칸 | 5칸(지속) | 아군 충돌 |
|---|---|---|---|---|---|---|
| 칩 없음 | 10 | 20 | 30 | 40 | 50 | 칸당 5 · 돌진 끝 |
| ① | **18** | **36** | **54** | **72** | **90** | 칸당 5 · 돌진 끝 |
| ② | **8.5** | **17** | **25.5** | **34** | **42.5** | **통과**(0 · 계속 달린다) |
| ③ | 10 | 20 | 30 | 40 | 50 | 칸당 5 · 돌진 끝 |

**자해** (5칸 지속 돌진 중 지형 충돌만 — 적 · 아군 충돌은 0)

| | 지금(버그) | 칩 없음(D-112 뒤) | ① | ② | ③ |
|---|---|---|---|---|---|
| 자해 | 0 | **50** | **100** | 50 | 50 |

**충전 · 이속** (기본 이속 550, 가속 무시)

| | 1칸 | 2칸 | 3칸 | 4칸 | 5칸 | 충전 중 초당 물 |
|---|---|---|---|---|---|---|
| 칩 없음 — 도달 시간 | 0.5초 | 1.0 | 1.5 | 2.0 | **2.5** | 40 |
| ③ — 도달 시간 | 0.35초 | 0.70 | 1.05 | 1.40 | **1.75** | 57 |
| 칩 없음 — 이속 | 660 | 770 | 880 | 990 | **1100** | |
| ③ — 이속 | 627 | 704 | 781 | 858 | **935** | |

- 일반 돌진(1~4칸)은 1초에 1칸씩 줄며 달린다 → 4칸 이동 거리 ≈ 550 × (4 + 0.2 × 10) = **3300uu**, ③ 은 550 × (4 + 0.14 × 10) = **2970uu**(−10%).
- 물은 칸당 20 그대로(5칸 = 100) — ③ 은 같은 물을 더 빨리 쓴다.

### 15.2 상호작용

| 조합 | 결과 |
|---|---|
| ①② + 공용 칩(⑥ 광폭화 · ⑦ 시너지 · `DamageDealt`) · 탑승칩① | ExecCalc 소스측에서 18 · 8.5 에 추가로 곱해진다(아군 충돌 칸당 5 에도 지금처럼 곱해진다) |
| ①② + 공기탄칩② 흡착 · 물대포② 취약 | 적 관문에서 받는 피해 배율이 곱해진다. 돌진 피해로는 흡착 스택이 안 쌓인다(T-99) |
| ①② + 클렌저 사이트 | 사이트는 "적" 판정이라 18 · 8.5 로 계산된다(기존 동작). 사이트는 Pawn 이 아니라 **지형**이기도 해서 지속 돌진으로 부딪히면 자해도 받는다(§15.8-67) |
| 자해 + 탑승 | 자해는 일반 피해라 탑승 공유를 탄다 — 내 위 탑승자도 50(①이면 100) / 탑승칩② 대신 맞는 범퍼면 청소기 50 + 10 = **60**(①이면 100 + 20 = **120**, §12.2) |
| 자해 + 벨보이 모자 | 모자가 1회 막는다(자해도 "어떤 피해든") |
| 자해 + 물 점프칩② 무적(§14) | 0.3초 무적 안이면 0 |
| 자해 + 받는 피해 칩(`DamageTaken`) · 시너지 · 오염 · 로비 | 받는 쪽 배율이 그대로 걸린다 / 로비 · 튜토리얼은 체력 1 클램프 |
| ② + 탑승 | 내 위 탑승자 · 내가 탄 청소기는 원래 충돌 판정에서 빠진다 — 그대로 |
| ② + 적 · 지형 | 그대로 부딪혀 피해(−15%) · 종료 · 자해(지속 · 지형) |
| ② + 다른 플레이어의 돌진 · 이동 | 서로 통과(양쪽 다 무시 — D-115). 돌진이 끝나면 다시 막힌다 |
| ③ + 자판기 공속 버프(M) | 돌진 충전에는 원래 공속이 안 걸린다 — 그대로(0.35초 고정) |
| ③ 이속 + 다른 이속 효과(씨앗② 가속 · 탑승칩① 하중 · 공용 이동 속도 칩) | Compound · 기본값 쪽과 곱해진다 — 탑승자 1명(하중 −5%)을 태운 청소기의 ③ 5칸 = 550 × 1.7 × 0.95 ≈ **888** |
| ③ + 공기탄 반동 예외(`HasDashBuff`) | 에셋 태그 `Buff.RobotVacuum.DashSpeed` 를 그대로 두므로 변화 없음 |
| ①②③ + 탑승 · 공기탄 · 물 점프 칩 | 다른 그룹이라 하나씩 함께 낄 수 있다 |

### 15.3 설계 결정

| # | 결정 | 근거 |
|---|---|---|
| **D-111** | 그룹 키 = 실제 GA 태그 **`Abilities.RobotVacuum.Dash`**. 기능 스위치는 ② 하나 — `Upgrade.Feature.Skill.Dash.PassAllies`. ①③ 은 모디파이어만. ★동적 상수가 없어 `ChipTuning` · Config 검사는 추가하지 않는다★ | D-93 · D-102 와 같은 방식. 수치가 전부 정적이라 카탈로그(D-2)로 끝난다 |
| **D-112** | ★선행 변경★ 자해 수정 = ① BP `SelfDamageEffect = GE_VacuumDashSelfDamage`(에셋) ② 조건을 **지속 돌진 + Pawn 이 아닌 충돌(지형)** 로(C++) ③ 자해량을 GE 고정값 50 에서 GA `SustainedImpactSelfDamage`(50) → SetByCaller **`Data.SelfDamage`** 로 옮긴다 | 사용자 확정(지속 돌진 + 지형만). "지형" 기준은 `OnCapsuleHit` 의 분류(Pawn 여부)와 같다 — ASC 유무로 하면 사이트가 빠진다(T-128). 칩①이 배율을 곱하려면 값이 코드에 있어야 한다 |
| **D-113** | ①② 적 충돌 피해 = `G × ApplySkill(SkillDamage, EnemyDamagePerGauge)` — **적에게만**. 아군 충돌(`PlayerDamagePerGauge` 5)은 `SkillDamage` 를 안 탄다. ① **+0.8**(10 → 18) · ② **−0.15**(10 → 8.5) | 사용자 확정("적에게 주는 충돌 피해"). 돌진이 이때 처음으로 `SkillDamage` 를 읽는다. ②의 8.5 는 소수 그대로 둔다(반올림하면 −15% 가 −10%(9)나 −20%(8)이 된다) |
| **D-114** | ① 자해 = 새 축 **`SkillSelfDamage`**(스킬 단위 배율, **낮을수록 좋음**) **+1.0** → `ApplySkill(SkillSelfDamage, 50)` = 100 | 정적 수치라 카탈로그(D-2) — 단점이 카드에 빨갛게 뜬다. 다른 자해 스킬이 생겨도 같은 축을 쓴다 |
| **D-115** | ② 아군 통과 = 캐릭터 복제 플래그 **`bDashPassThroughAllies`**(서버가 `ReleaseGauge` 에서 켜고 `FinishDash` · `EndAbility` 에서 끈다) → **모든 머신**의 `OnRep` 에서 내 캡슐 ↔ 아군 캡슐을 **서로** `IgnoreActorWhenMoving`. 이 머신에서 넣은 아군만 기억했다가 그대로 되돌린다. 서버 `OnCapsuleHit` 는 통과 중 아군 충돌을 무시한다(안전망) | 적 · 아군이 같은 Pawn 채널이라 채널 변경은 적까지 통과시킨다(T-126). 이동 무시는 복제가 안 돼 머신마다(T-123), 한쪽만이면 상대가 움직일 때 막혀서 양쪽 다(T-124). 안전망은 돌진 중 새로 생긴 아군 폰 대비 — 피해 · 종료는 절대 없다 |
| **D-116** | ② 통과 대상 = **다른 플레이어 캐릭터 전부**(`ADRCharacter` 중 `!IsNotFriend`). 적 · 사이트 · 지형은 그대로 부딪힌다 | 원문 "아군과 충돌하지 않고 통과". 사이트는 아군이 아니다(D-3) |
| **D-117** | ③ 충전 = 기존 축 **`SkillWindup`** **−0.3** → 타이머 간격 = `ChargeInterval × WindupScale`(하한 0.05초). 감쇠(`DecayInterval`)에는 곱하지 않는다 | §13 D-99 와 같은 해석 — 충전이 곧 이 스킬의 선딜이다. 0.5 × 0.7 = 0.35 가 정확히 맞는다. 감쇠까지 줄이면 돌진 시간이 짧아진다(T-129) |
| **D-118** | ③ 이속 = 새 축 **`SkillSpeedBonus`**(스킬 단위, 칸당 이속 증가분의 배율, 높을수록 좋음) **−0.3** → 칸당 `ApplySkill(SkillSpeedBonus, SpeedBonusPerGauge 0.2)` = 0.14. `GE_VacuumDashSpeedBuff` 크기를 ScalableFloat 1.2 → **SetByCaller `Data.MoveSpeed.Mult`**(= 1 + 칸당 값)로 바꾸고 **MultiplyAdditive · by Target 5 는 유지** | 값이 GE 에 박혀 있으면 칩이 못 바꾼다. 기존 태그 `Data.MoveSpeed.Mult`(최종 배율 의미)를 그대로 쓴다. Additive 라야 5칸이 1 + 0.14 × 5 = ×1.7(Compound 면 ×5.7 — T-122) |
| **D-119** | 새 축 2개 — `SkillSelfDamage`(스킬 · **낮을수록 좋음** · 초 아님) · `SkillSpeedBonus`(스킬 · 높을수록 좋음 · 초 아님). `DRIsSkillStat` 에 둘 다, `DRIsLowerBetter` 에 `SkillSelfDamage` 만. 위치는 **구현 시점의 `Count` 바로 앞** | T-30 · T-12. `SkillSelfDamage` 를 `DRIsLowerBetter` 에서 빠뜨리면 "자해 +100%" 가 초록으로 뜬다(T-130) |

> 기각: ② 를 콜리전 채널(Pawn → Overlap/Ignore)로 — 적도 Pawn 이라 같이 통과해 충돌 피해 자체가 사라진다.
> 기각: ② 를 "아군 전용 오브젝트 채널" 신설로 — 프로젝트 전체 콜리전 프로필 · 모든 캐릭터 BP 를 바꿔야 한다. 칩 하나에 비해 너무 크다.
> 기각: ③ 이속을 기능 스위치 + 별도 GE(1.14)로 — 정적 수치는 카탈로그가 원칙이고 GE 가 둘이 되면 공기탄 반동 예외 · HUD 가 둘 다 알아야 한다.
> 기각: ① 자해를 GE 를 두 번 적용해 100 으로 — 받는 쪽 배율 · 모자 · 무적이 두 번 판정된다(모자가 한 번만 막는 등).

### 15.4 C++ 변경 (적용 순서)

**① `DRGameplayTags.h / .cpp`** — 태그 2개(네이티브)

| 멤버 | 태그 | 종류 |
|---|---|---|
| `Upgrade_Feature_Skill_Dash_PassAllies` | `Upgrade.Feature.Skill.Dash.PassAllies` | ② 기능 스위치 |
| `Data_SelfDamage` | `Data.SelfDamage` | SetByCaller — `GE_VacuumDashSelfDamage` 의 자해량(D-112) |

**② `DRUpgradeTypes.h` · `DRUpgradeUILibrary.cpp`** — 새 축 2개 (D-114 · D-118 · D-119)

```cpp
	// ===== Plan5(돌진칩) 추가 =====
	// ★신규 값은 반드시 Count 바로 앞에 붙인다.★ (T-30) — §13 · §14 의 축보다 먼저 구현하면 그 앞이 된다

	/**
	 * 스킬 단위. 그 스킬이 ★시전자 자신에게 주는 피해(자해)★ 의 배율 — 낮을수록 좋다.
	 *     자해 = ApplySkill(SkillSelfDamage, 기본 자해량)   // +1.0 → 50 → 100
	 * DRIsLowerBetter 에 ★넣는다★ (안 넣으면 "+100%" 가 초록으로 뜬다 — T-130) (Plan5 D-114)
	 */
	SkillSelfDamage			UMETA(DisplayName = "스킬 자해 피해"),

	/**
	 * 스킬 단위. 그 스킬이 시전자에게 주는 ★이동 속도 증가분(칸 · 스택당)★ 의 배율.
	 *     칸당 증가 = ApplySkill(SkillSpeedBonus, 0.2)   // −0.3 → 0.14 → GE 배율 1.14
	 * 배율 자체가 아니라 "증가분"에 곱한다 — 1.2 에 곱하면 1.2 × 0.7 = 0.84(감속)가 된다 (Plan5 D-118)
	 */
	SkillSpeedBonus			UMETA(DisplayName = "스킬 이속 버프"),

	Count UMETA(Hidden)
```

- `DRIsSkillStat()` 에 두 줄(`// Plan5(돌진칩)`), `DRIsLowerBetter()` 에 `SkillSelfDamage` 한 줄. `DRIsTimeStat` 에는 넣지 않는다.
- `DRUpgradeUILibrary.cpp` 짧은 이름(5자 제한): `SkillSelfDamage` → `"자해 피해"` · `SkillSpeedBonus` → `"이속 버프"`.

**③ `DRVacuumDash.h`** — 값 2개를 GE 에서 C++ 로, 클래스 주석 갱신

```cpp
// 클래스 주석: "5칸(최대): 지속 돌진 — 자동 전진 + 감쇠 없음, 충돌(50 자해) 또는 S 브레이크(무피해)로 종료"
//   → "… 지형 충돌(자해 50 — 적 · 아군 충돌은 자해 없음) 또는 적 · 아군 충돌 · S 브레이크(무피해)로 종료"
// SpeedBuffEffect 주석: "MoveSpeed ×(1+0.2×스택)" → "MultiplyAdditive · SetByCaller(Data.MoveSpeed.Mult) = 1 + 칸당 증가 · by Target 5"
// SelfDamageEffect 주석: "(GE_VacuumDashSelfDamage — 50)" → "(GE_VacuumDashSelfDamage — IncomingDamage · SetByCaller(Data.SelfDamage))"

	// ★Plan5(돌진칩)★ 칸당 이속 증가분 (0.2 = +20%). 예전엔 GE 에 1.2 로 박혀 있었다 — 칩③ SkillSpeedBonus 가 곱해진다 (D-118)
	UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "0.0"))
	float SpeedBonusPerGauge = 0.2f;

	// ★Plan5(돌진칩)★ 지속 돌진 지형 충돌 자해량. 예전엔 GE 에 50 으로 박혀 있었다 — 칩① SkillSelfDamage 가 곱해진다 (D-112 · D-114)
	UPROPERTY(EditDefaultsOnly, Category = "Dash", meta = (ClampMin = "0.0"))
	float SustainedImpactSelfDamage = 50.f;
```

**④ `DRVacuumDash.cpp`** — ③ 충전 · ③ 이속 · ② 통과 · 선행 · ①② 피해 (D-112 · D-113 · D-115 · D-117 · D-118)

```cpp
// StartChargingGauge() — 타이머 간격
	// ★Plan5(돌진칩)★ ③ 칸당 충전 시간 × 선딜 배율(SkillWindup — 0.7 이면 0.5 → 0.35초) (D-117).
	//   감쇠(DecayInterval)에는 곱하지 않는다 — 돌진 시간이 짧아진다 (T-129)
	const float WindupScale = FMath::Max(0.01f, GetUpgradedFloat(EDRUpgradeStat::SkillWindup, 1.f));
	const float EffectiveChargeInterval = FMath::Max(0.05f, ChargeInterval * WindupScale);
	GetWorld()->GetTimerManager().SetTimer(ChargeTimerHandle, this, &UDRVacuumDash::TickCharge, EffectiveChargeInterval, true);
```

```cpp
// ApplyBuffStacks() — 스펙마다 SetByCaller
	// ★Plan5(돌진칩)★ ③ 칸당 이속 = SpeedBonusPerGauge × SkillSpeedBonus (0.2 → 0.14). GE 는 SetByCaller · MultiplyAdditive · by Target 5 (D-118).
	//   ★모든 스펙에 넣는다★ — 빠지면 크기 0 → (0 − 1) × 스택 = 이속 0 이하 (T-121)
	const float BonusPerGauge = FMath::Max(0.f, GetUpgradedFloat(EDRUpgradeStat::SkillSpeedBonus, SpeedBonusPerGauge));
	for (int32 i = 0; i < Stacks; ++i)
	{
		const FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(SpeedBuffEffect, GetAbilityLevel(), ContextHandle);
		if (SpecHandle.IsValid())
		{
			SpecHandle.Data->SetSetByCallerMagnitude(FDRGameplayTags::Get().Data_MoveSpeed_Mult, 1.f + BonusPerGauge);
			SpeedBuffHandle = ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		}
	}
```

```cpp
// ReleaseGauge() — SetDashCollisionEnabled(true) 바로 다음
	// ★Plan5(돌진칩)★ ② 아군 통과 — 돌진하는 동안만. 실제 무시 목록은 캐릭터가 모든 머신에서 맞춘다 (D-115)
	if (UDRAbilitySystemLibrary::HasUpgradeFeature(
			GetAbilitySystemComponentFromActorInfo(), FDRGameplayTags::Get().Upgrade_Feature_Skill_Dash_PassAllies))
	{
		Vacuum->SetDashPassThroughAllies(true);
	}
```

```cpp
// HandleDashImpact() — 피해 · 자해 블록 교체
	// ★Plan5(돌진칩)★ "지형" = Pawn 이 아닌 충돌 — OnCapsuleHit 의 분류와 같다 (사이트도 여기 든다 — D-112 · T-128)
	const bool bHitPawn = Cast<APawn>(HitActor) != nullptr;

	// 대상 데미지: 적 = G × 10(칩 반영), 타 플레이어 = G × 5. ASC 없는 지형은 대상 데미지 없음.
	if (IsValid(HitActor) && UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor))
	{
		const bool bEnemy = UDRAbilitySystemLibrary::IsNotFriend(Avatar, HitActor);
		// ★Plan5(돌진칩)★ ①② 적 충돌 피해만 SkillDamage 를 탄다 — 아군 충돌(칸당 5)은 그대로 (D-113 · 사용자 확정)
		const float DamagePerGauge = bEnemy
			? GetUpgradedFloat(EDRUpgradeStat::SkillDamage, EnemyDamagePerGauge)
			: PlayerDamagePerGauge;
		FDamageEffectParams Params = MakeDamageEffectParamsFromClassDefaults(HitActor);
		Params.BaseDamage = G * DamagePerGauge;
		// ApplyDamageEffect 직접 호출 — 아군 필터에 걸리지 않음 (Plan3 §8.2)
		UDRAbilitySystemLibrary::ApplyDamageEffect(Params);
	}

	// ★Plan5(돌진칩)★ 자해 = 지속 돌진 + 지형 충돌만 (사용자 확정 — 적 · 아군 충돌은 자해 없음). 기본 50, 칩① ×2 (D-112 · D-114)
	//   자해량은 SetByCaller(Data.SelfDamage) — GE 에 값을 박지 않는다 (T-120)
	if (bWasSustained && !bHitPawn && SelfDamageEffect)
	{
		UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
		FGameplayEffectSpecHandle SpecHandle = UDRAbilitySystemLibrary::CreateEffectSpec(ASC, SelfDamageEffect, Avatar);
		UDRAbilitySystemLibrary::ApplyEffectSpecWithSetByCaller(ASC, SpecHandle, FDRGameplayTags::Get().Data_SelfDamage,
			FMath::Max(0.f, GetUpgradedFloat(EDRUpgradeStat::SkillSelfDamage, SustainedImpactSelfDamage)));
	}

	FinishDash(true, HitActor);
```

```cpp
// FinishDash() · EndAbility() — SetDashCollisionEnabled(false) 바로 다음 (두 곳 모두 — T-125)
		Vacuum->SetDashPassThroughAllies(false);   // ★Plan5(돌진칩)★ ② (D-115)
```

**⑤ `DRRobotVacuumCharacter.h / .cpp`** — ② 아군 통과 (D-115 · D-116)

```cpp
// .h public — "========== 돌진 ==========" 블록
	// ★Plan5(돌진칩)★ ② 돌진 중 아군 통과. GA 가 서버에서 켜고 끈다 → 모든 머신의 OnRep 이 이동 무시 목록을 맞춘다 (D-115)
	UPROPERTY(ReplicatedUsing = OnRep_DashPassThroughAllies, BlueprintReadOnly, Category = "Dash")
	bool bDashPassThroughAllies = false;

	void SetDashPassThroughAllies(bool bEnabled);   // 서버 전용

	UFUNCTION()
	void OnRep_DashPassThroughAllies();

// .h private
	// ★Plan5(돌진칩)★ 이 머신에서 "서로 이동 무시"로 넣은 아군 — 끌 때 정확히 이것만 되돌린다 (이동 무시는 복제되지 않는다)
	TArray<TWeakObjectPtr<ADRCharacter>> PassThroughAllies;

	void ApplyAllyPassThrough(bool bEnable);
```

```cpp
// .cpp — #include "EngineUtils.h"(TActorIterator) · "AbilitySystem/DRAbilitySystemLibrary.h"
// GetLifetimeReplicatedProps: DOREPLIFETIME(ADRRobotVacuumCharacter, bDashPassThroughAllies);

void ADRRobotVacuumCharacter::SetDashPassThroughAllies(bool bEnabled)
{
	if (!HasAuthority() || bDashPassThroughAllies == bEnabled) return;
	bDashPassThroughAllies = bEnabled;
	OnRep_DashPassThroughAllies();   // 리슨 서버 parity (OnRep_SustainedDash 와 같은 관례)
}

void ADRRobotVacuumCharacter::OnRep_DashPassThroughAllies()
{
	ApplyAllyPassThrough(bDashPassThroughAllies);
}

void ADRRobotVacuumCharacter::ApplyAllyPassThrough(bool bEnable)
{
	UCapsuleComponent* MyCapsule = GetCapsuleComponent();
	if (!MyCapsule) return;

	// 먼저 이 머신에서 넣었던 것을 전부 되돌린다 (켜기 · 끄기 공통 — 중복 등록 방지)
	for (const TWeakObjectPtr<ADRCharacter>& Weak : PassThroughAllies)
	{
		if (ADRCharacter* Ally = Weak.Get())
		{
			MyCapsule->IgnoreActorWhenMoving(Ally, false);
			if (UCapsuleComponent* AllyCapsule = Ally->GetCapsuleComponent())
			{
				AllyCapsule->IgnoreActorWhenMoving(this, false);
			}
		}
	}
	PassThroughAllies.Reset();

	if (!bEnable) return;

	// ★양쪽 다★ — 내가 움직일 때도, 아군이 움직일 때도 서로 막지 않는다 (T-124)
	for (TActorIterator<ADRCharacter> It(GetWorld()); It; ++It)
	{
		ADRCharacter* Ally = *It;
		if (Ally == this || UDRAbilitySystemLibrary::IsNotFriend(this, Ally)) continue;   // 플레이어끼리만 (D-116)

		MyCapsule->IgnoreActorWhenMoving(Ally, true);
		if (UCapsuleComponent* AllyCapsule = Ally->GetCapsuleComponent())
		{
			AllyCapsule->IgnoreActorWhenMoving(this, true);
		}
		PassThroughAllies.Add(Ally);
	}
}

// EndPlay() — Super 앞에
	ApplyAllyPassThrough(false);   // ★Plan5(돌진칩)★ 돌진 중 파괴 · 레벨 전환 — 아군 캡슐에 내 항목이 남지 않게 (T-125)
```

```cpp
// OnCapsuleHit() — "탑 상태에서 서로 충돌 판정 금지" 다음
	// ★Plan5(돌진칩)★ ② 아군 통과 중이면 아군 충돌은 무시 — 무시 목록에 없던 아군(돌진 중 부활 · 입장)의 안전망.
	//   피해 · 돌진 종료가 절대 없다 (물리적으로는 막힐 수 있다 — §15.8-69) (D-115)
	if (bDashPassThroughAllies && !UDRAbilitySystemLibrary::IsNotFriend(this, OtherActor)) return;
```

**바뀔 C++ 파일**: `DRGameplayTags`(.h/.cpp) · `DRUpgradeTypes.h` · `DRUpgradeUILibrary.cpp` · `DRVacuumDash`(.h/.cpp) · `DRRobotVacuumCharacter`(.h/.cpp).
**`DRProgressionConfig` · `DRAbilitySystemLibrary` 는 바뀌지 않는다**(동적 상수 · 새 판정 없음 — D-111). BP 그래프 재배선 없음.

### 15.5 에디터 작업 (★C++ 빌드 뒤★ — 순서가 거꾸로면 T-120 · T-121)

1. ★**선행**★ `GA_VacuumDash`(`…/VacuumCleaner/Abilities/Skill2/`) → Class Defaults → Dash → **Self Damage Effect = `GE_VacuumDashSelfDamage`**(지금은 `GE_VacuumDashSpeedBuff` — 버그). `Cost Gameplay Effect Class` 는 그대로(§14.8-63).
2. **`GE_VacuumDashSelfDamage`** → Modifiers[0](IncomingDamage · Add) → Magnitude Calculation Type **Set by Caller** · Data Tag **`Data.SelfDamage`**(예전 Scalable Float 50 → 값은 GA `SustainedImpactSelfDamage` 로 이사)
3. **`GE_VacuumDashSpeedBuff`** → Modifiers[0](MoveSpeed) → Magnitude **Set by Caller** · Data Tag **`Data.MoveSpeed.Mult`**. ★Modifier Op 는 **Multiply (Additive)** 그대로★(T-122) · Stacking(by Target · 5) · Infinite · 에셋 태그 `Buff.RobotVacuum.DashSpeed` 모두 그대로
4. **`DA_ChipCatalog`** → Chips +3 (§15.1 표). 공통: `OwnerClass RobotVacuum` · `bCommonChip` 끔 · `Category Ascension` · `TargetAbilityTag Abilities.RobotVacuum.Dash` · `RequiredSlotCount 1`.
   ★**모든 모디파이어**의 `TargetAbilityTag` 도 `Abilities.RobotVacuum.Dash`★(D-51 — 특히 ③의 `SkillWindup` 이 빠지면 공기탄 · 물 점프에도 걸린다)
   - ① `RV.Asc.DashRam` · 충각 범퍼 · 요약 "돌진 충돌 피해 +80%" · Tier 3
     · Modifiers: `SkillDamage` Percent **+0.8**(`DetailOverride` "칸당 충돌 피해 10 → 18") / `SkillSelfDamage` Percent **+1.0** `bIsDrawback` ✓(`DetailOverride` "벽 충돌 자해 50 → 100")
     · Description "돌진으로 적에게 부딪힐 때 게이지 1칸당 피해가 10에서 18로 늘어납니다. 대신 최대 충전(지속 돌진) 중 벽이나 지형지물에 부딪히면 받는 피해가 50에서 100으로 늘어납니다."
   - ② `RV.Asc.DashPass` · 유령 주행 · 요약 "돌진 중 아군 통과" · Tier 4 · GrantedTags `Upgrade.Feature.Skill.Dash.PassAllies`
     · Modifiers: `SkillDamage` Percent **−0.15** `bIsDrawback` ✓(`DetailOverride` "칸당 충돌 피해 10 → 8.5")
     · Description "돌진하는 동안 다른 플레이어와 부딪히지 않고 그대로 지나갑니다(아군에게 피해를 주거나 멈추지 않습니다). 대신 적에게 부딪힐 때의 피해가 15% 줄어듭니다."
   - ③ `RV.Asc.DashQuick` · 급속 충전기 · 요약 "돌진 충전 30% 빨라짐" · Tier 3
     · Modifiers: `SkillWindup` Percent **−0.3**(`DetailOverride` "칸당 충전 0.5 → 0.35초") / `SkillSpeedBonus` Percent **−0.3** `bIsDrawback` ✓(`DetailOverride` "칸당 이속 +20% → +14%")
     · Description "돌진 게이지가 0.35초마다 1칸씩 찹니다(기존 0.5초). 대신 1칸당 이동 속도 증가가 20%에서 14%로 줄어듭니다."
   - `SkillDisplayNames` 에 `Abilities.RobotVacuum.Dash → 돌진`
5. (선택) 아이콘 `Content/DaeRuneAssets/UI/Upgrade/Icon/Vacuum/공격-9 · 지원-9 · 편의-9` (탑승 -6 · 공기탄 -7 · 물 점프 -8)
6. 검사: 카탈로그 오류 0건(새 축 2개가 스킬 단위로 잡히는지 포함). Config 검사 항목은 없다(D-111)

### 15.6 함정

| # | 함정 | 대응 |
|---|---|---|
| **T-119** | 선행 변경(§15.5-1) 없이 칩①만 넣는다 | 자해가 원래 0 이라 "50 → 100" 이 아무 일도 안 한다 — 칩①이 단점 없는 칩이 된다 |
| **T-120** | `GE_VacuumDashSelfDamage` 를 SetByCaller 로 바꿨는데 C++ 가 아직 옛 코드(`CreateAndApplyEffectSpec`) / 반대로 C++ 만 바꾸고 GE 는 Scalable Float 50 그대로 | 앞은 자해 0 + 에러 로그, 뒤는 칩① 이 무시된다(늘 50). **C++ 빌드 → GE 변경** 순서 |
| **T-121** | `GE_VacuumDashSpeedBuff` 를 SetByCaller 로 바꾼 뒤 값을 안 넣고 거는 경로가 남는다 | 크기 0 → MultiplyAdditive (0 − 1) × 스택 → **이속 0 이하**. 지금의 `SelfDamageEffect` 오배선(이속 GE)이 바로 그런 경로다 — 선행 ①로 같이 없어진다. 이 GE 를 거는 곳은 `ApplyBuffStacks` 하나여야 한다 |
| **T-122** | 이속 GE 를 SetByCaller 로 바꾸면서 Op 를 Multiply (Compound)로 | 스택 바이어스 0 → 1.14 × 5 = **×5.7**(T-95 와 같은 함정). Additive 유지 |
| **T-123** | 이동 무시를 서버에서만 넣는다 | 소유 클라 CMC 는 여전히 아군에 막혀 위치 보정이 튀고, 아군 클라에서는 아군이 청소기에 막힌다. 모든 머신의 `OnRep` 에서 |
| **T-124** | 한쪽(청소기 → 아군)만 무시 | 아군이 움직일 때 청소기에 막히거나, 겹친 채로 밀어낸다. 양쪽 다 |
| **T-125** | 해제를 `FinishDash` 에만 둔다 | 사망 · 스턴 · 강제 취소(`EndAbility`) 경로에서 **영구 통과**가 남는다. `FinishDash` · `EndAbility` · 캐릭터 `EndPlay` 세 곳 |
| **T-126** | 통과를 콜리전 채널로(캡슐 Pawn 응답 Overlap/Ignore) | 적도 같은 Pawn 채널이라 같이 통과 → 충돌 피해 자체가 사라진다 |
| **T-127** | `SkillDamage` 를 아군 충돌 피해에도 곱한다 | 사용자 확정 위반 — 칸당 5 가 ① 9 · ② 4.25 가 된다 |
| **T-128** | "지형"을 "ASC 없음"으로 판정 | 사이트(ASC 있음 · Pawn 아님)가 빠지고, 판정 기준이 `OnCapsuleHit` 와 달라진다 — Pawn 여부로 |
| **T-129** | `SkillWindup` 을 `DecayInterval` 에도 곱한다 | 감쇠가 빨라져 일반 돌진 시간 · 거리가 30% 준다 — 칩 의도 아님. 충전에만 |
| **T-130** | 새 축 2개를 `Count` 앞이 아닌 곳에 / `DRIsSkillStat` 누락 / `SkillSelfDamage` 를 `DRIsLowerBetter` 에서 누락 | 기존 칩 효과가 밀리거나(T-30) 전역 블록에 쌓이거나(T-12), 카드에 "자해 +100%" 가 초록(장점)으로 뜬다 |

### 15.7 검증

준비: PIE Listen Server · 플레이어 2~3(청소기 1 + 아군 1~2, 클라 청소기도 1회). 콘솔 `DRUnlockUpgrade` → `DRUnlockSlot 6`. 수치는 데미지 숫자 · HUD 체력 · `showdebug abilitysystem`(MoveSpeed · 활성 GE 스택).

- [ ] **선행 변경(칩 없음)** 5칸 지속 돌진으로 벽 → 자해 50(숫자 · 체력) / 5칸으로 적 · 아군 → 자해 없음 / 1~4칸으로 벽 → 자해 없음 / 칸당 이속 660 · 770 … 1100 그대로(GE SetByCaller 전환 확인) / 탑승자가 있으면 자해가 공유된다 / 모자가 있으면 모자가 막는다
- [ ] **①** 적 1칸 18 · 3칸 54 · 5칸 90 / 아군 충돌은 칸당 5 그대로 / 5칸 벽 충돌 자해 100 / 카드 "벽 충돌 자해 50 → 100" 빨강
- [ ] **②** 아군을 향해 1~5칸 돌진 → 피해 · 정지 · Crush 없이 지나감(호스트 화면 · 아군 클라 화면 · 내 클라 화면 모두) / 아군이 달려오는 청소기 쪽으로 걸어도 막히지 않음 / 적에게는 1칸 8.5 · 5칸 42.5 로 부딪혀 끝남 / 돌진이 끝나면 다시 막힘 / 돌진 중 사망 · S 브레이크 · 자연 감쇠 뒤에도 통과가 남지 않음 / 내 위 탑승자 · 내가 탄 청소기는 지금처럼 문제없음
- [ ] **③** 0.35 · 0.70 · 1.05 · 1.40 · 1.75초에 1~5칸(HUD 칸 수) / 물 칸당 20 그대로 / 칸당 이속 627 · 704 … 935 / 감쇠는 여전히 1초에 1칸 / 공기탄 강화탄 반동 예외(돌진 중) 그대로
- [ ] **스킬당 1칩** ①②③ 중 하나만, 탑승 · 공기탄 · 물 점프 칩과는 함께 / 미리보기 라벨 "돌진" / ③ 의 `SkillWindup` 이 공기탄 차지 간격을 바꾸지 않음
- [ ] **멀티** 클라 청소기 ② → 호스트 플레이어를 통과(보정 튐 없음 — 돌진 시작 순간 붙어 있으면 1회 튈 수 있다, §15.8-68) / 클라 청소기 ③ → 이속이 클라 화면에서도 935

### 15.8 열린 결정 · 알려진 제약 (§9 번호에 이어서)

| # | 내용 | 현재 판단 |
|---|---|---|
| **65** | 이름 · Tier · 아이콘 · `ChipId`(`RV.Asc.Dash*`)는 가칭 | 사용자가 정하면 카탈로그만(`ChipId` 는 첫 입력 전에 확정 — 세이브 키) |
| **66** | ★선행 변경(D-112)으로 칩 없는 5칸 지속 돌진도 벽에 부딪히면 자해 50(지금은 0)★. 대신 원래 코드의 "적 · 아군 충돌 자해"는 없어진다. §12(탑승칩)의 "돌진 자해 50" 예시는 이 수정을 전제로 한다 | 사용자 확정. 플레이 확인 후 GA `SustainedImpactSelfDamage` 만 조정 |
| **67** | 클렌저 사이트는 Pawn 이 아니라 **지형**으로 분류된다 — 지속 돌진으로 부딪히면 자해를 받고, 사이트는 "적" 판정이라 충돌 피해 계산 대상이기도 하다(기존 동작) | `OnCapsuleHit` 분류를 그대로 따른다. 사이트를 지형에서 빼고 싶으면 `HandleDashImpact` 에서 `ADRCleanserSite` 예외 한 줄 |
| **68** | ② 돌진 시작 순간 아군과 붙어 있으면 소유 클라가 플래그를 받기 전(복제 지연) 한 번 막혔다가 보정될 수 있다 | 허용. 거슬리면 소유 클라에서 `ReleaseGauge` 때 로컬로 먼저 켠다(업그레이드 캐시는 소유 클라에도 있다) |
| **69** | ② 돌진 중 새로 생긴 아군 폰(부활 · 중도 입장)은 통과 목록에 없다 — 물리적으로는 막히지만 피해 · 종료는 없다(서버 안전망) | 드문 경우라 허용 |
| **70** | ② 아군과 겹친 채로 돌진이 끝나면 충돌이 돌아오면서 엔진이 밀어낸다(겹침 해소) | 허용 — 짧은 밀림 |
| **71** | ② 적 피해 8.5/칸은 소수다(1칸 8.5 · 5칸 42.5) | 의도대로(D-113). 화면 숫자는 기존 표시 방식 그대로 |
| **72** | ③ 은 같은 물 100 을 1.75초에 쓴다 — 충전 중 초당 물 40 → 57. 물이 모자라면 충전이 멈추는 규칙은 그대로 | 사용자 확정 수치 |
| **73** | ③ 지속 돌진 최고 속도 1100 → 935(−15%), 4칸 일반 돌진 거리 −10% | 사용자 확정 수치. 약하면 `SkillSpeedBonus` 값만 조정 |
| **74** | ① 자해 100 은 받는 쪽 배율을 탄다 — `DamageTaken` 칩 · 오염이면 더 크다. 탑승자에게도 공유된다 | 일반 피해와 같은 규칙(D-112) |

### 15.9 작업 순서

1. C++ ①~⑤ → 빌드 (`UENUM` · 태그 · 복제 프로퍼티 추가라 **에디터를 닫고** — T-45)
2. 에디터 §15.5-1~3(선행 · GE 2개) → PIE → §15.7 "선행 변경"
3. 에디터 §15.5-4~6(카탈로그)
4. §15.7 검증 → §1 · §3 · §5 · §6 · §9 에 흡수(§1.7 그룹 +1, 스탯 축 표에 새 축 2개), 머리말 상태 갱신

### 15.10 구현 결과 — 계획과 달라진 점

2026-10-08 C++ 구현 · 빌드 통과. 계획대로이고 다른 점:

- `ApplyAllyPassThrough` 는 켤 때도 먼저 이 머신의 이전 목록을 되돌린 뒤 다시 넣는다(중복 등록 방지).
- ★지금 상태(에디터 작업 전)로 플레이해도 깨지지 않는다★ — 코드가 SetByCaller 값을 넣어도 GE 가 아직 고정값(이속 1.2 · 자해 50)이라 그 값이 쓰인다.
  다만 자해는 `SelfDamageEffect` 오배선(§16.1-2)을 고치기 전까지 여전히 0 이고, 그 사이 칩①③ 의 수치는 반영되지 않는다.
  ★순서★: 이 C++ 가 들어간 뒤에 GE 두 개를 SetByCaller 로 바꾼다(반대면 이속 0 · 자해 0 — T-120 · T-121).

---

## 16. 청소기 칩 12종 — 에디터 작업 안내서 (처음 보는 사람용)

> C++ 는 전부 들어갔다(2026-10-08 빌드 통과). 이 절은 **에디터에서 손으로 할 일만** 위에서부터 순서대로 적었다.
> "왜 그렇게 하는가"는 §12~§15 에 있다. 여기서는 **어디를 눌러 무엇을 넣는가**만 본다.
> 아이콘은 사용자가 따로 넣는다(이 안내서에서 뺐다).
> ★순서를 지킨다★ — 16.1 → 16.2 → 16.3 → 16.4 → 16.5. 특히 16.1 은 꼭 먼저 한다.

### 16.0 시작 전에 알아 둘 것

- 에디터를 연다. "모듈을 다시 빌드할까요?"가 뜨면 **예**.
- 저장: 에셋 하나는 `Ctrl + S`, 전부는 `Ctrl + Shift + S`.
- **GE 만들기** = 콘텐츠 브라우저에서 폴더를 연다 → 빈 곳 **우클릭** → `Blueprint Class` → 창 아래 검색칸에 `GameplayEffect` → 고르고 `Select` → 이름 입력.
- **에셋 열기** = 더블클릭. 블루프린트(GA)는 툴바의 **Class Defaults** 를 눌러야 오른쪽 디테일 패널에 칸이 보인다.
- 디테일 패널 맨 위의 **검색칸**에 칸 이름을 치면 바로 찾아진다.
- **Set by Caller** = "숫자를 에셋에 적지 않고 코드가 넣는다"는 뜻. 고르면 바로 아래 `Data Tag` 칸이 생긴다 → 그 칸을 눌러 태그를 고른다.
- **태그 고르기** = 태그 칸을 누르면 목록 창이 뜬다 → 위 검색칸에 태그 이름 일부(예: `RideLoad`)를 치고 → 체크.
- **Target Tags 추가**(GE 에 "이 태그를 대상에게 붙여라") = 디테일의 **Components** 옆 `+` → `Grant Tags to Target Actor` → 그 항목을 펼쳐 **Add Tags → Added** 의 `+` → 태그 선택.

### 16.1 이미 있는 에셋 4곳 고치기 ★먼저★

칩이 없어도 게임이 바뀌는 수정이다(선행 변경 — 지금 잘못 연결돼 있는 것들).

| # | 에셋 (폴더) | 할 일 | 결과 |
|---|---|---|---|
| 1 | `GA_VacuumJetJump`<br>`Content/Blueprints/AbilitySystem/Player/VacuumCleaner/Abilities/Skill3/` | 열기 → **Class Defaults** → 검색 `Cost` → **Cost Gameplay Effect Class** = `GE_Cost_VacuumJetJump` | 물 점프가 물 30 을 실제로 쓴다(지금은 안 빠진다) |
| 2 | `GA_VacuumDash`<br>`…/VacuumCleaner/Abilities/Skill2/` | 열기 → **Class Defaults** → 검색 `Self Damage` → **Self Damage Effect** = `GE_VacuumDashSelfDamage`<br>(지금 `GE_VacuumDashSpeedBuff` 가 잘못 들어가 있다) | 5칸 돌진으로 벽에 부딪히면 자해가 들어간다 |
| 3 | `GE_VacuumDashSelfDamage`<br>`…/Abilities/Skill2/` | 열기 → **Modifiers** → `[0]` 펼치기 → **Modifier Magnitude** → **Magnitude Calculation Type** = `Set by Caller` → **Data Tag** = `Data.SelfDamage` | 자해량(50, 돌진칩①이면 100)을 코드가 넣는다 |
| 4 | `GE_VacuumDashSpeedBuff`<br>`…/Abilities/Skill2/` | 열기 → **Modifiers** → `[0]` → ★**Modifier Op 는 `Multiply (Additive)` 그대로**★ → Magnitude Calculation Type = `Set by Caller` → **Data Tag** = `Data.MoveSpeed.Mult` | 칸당 이속(+20%, 돌진칩③이면 +14%)을 코드가 넣는다 |

- ⚠️ 4번에서 Op 를 `Multiply (Compound)` 로 바꾸면 5칸 돌진이 5배 넘게 빨라진다. 건드리지 않는다.
- ⚠️ 3 · 4 의 Data Tag 를 다른 태그로 고르면 자해 0 · 이속 0(멈춤)이 된다. 철자를 한 번 더 본다.
- 확인(PIE, 칩 없이): 물 점프 → 물 30 이 줄어든다 / 돌진 5칸 → 속도가 예전과 같다 / 5칸 돌진으로 벽에 박으면 체력 −50, 적에게 박으면 자해 없음.

### 16.2 새 GE 4개 만들기

**① `GE_RideLoad`** — 폴더 `Content/Blueprints/AbilitySystem/Player/VacuumCleaner/Effects/` · 탑승칩① 감속

| 칸 | 값 |
|---|---|
| Duration Policy | `Infinite` |
| Modifiers | `+` 1개 → **Attribute** `DRAttributeSet.MoveSpeed` · **Modifier Op** `Multiply (Compound)` · **Magnitude Calculation Type** `Set by Caller` · **Data Tag** `Data.MoveSpeed.Mult` |
| Components | `Grant Tags to Target Actor` → `Debuff.RobotVacuum.RideLoad` ★빠지면 감속이 계속 쌓인다★ |
| Stacking Type | `None`(기본값 그대로) |

**② `GE_RideBan`** — 같은 폴더 · 탑승칩③ 20초 탑승 금지

| 칸 | 값 |
|---|---|
| Duration Policy | `Has Duration` |
| Duration Magnitude | **Magnitude Calculation Type** `Set by Caller` · **Data Tag** `Data.Duration` |
| Modifiers | 없음 |
| Components | `Grant Tags to Target Actor` → `Debuff.Ride.Banned` |
| Stacking Type | `None` ★`Aggregate by Target` 으로 바꾸지 말 것★ |

**③ `GE_AirShot_Drag`** — 폴더 `…/VacuumCleaner/Abilities/Skill1/` · 공기탄칩② 흡착(감속 · 받는 피해 증가)

| 칸 | 값 |
|---|---|
| Duration Policy | `Has Duration` |
| Duration Magnitude | `Set by Caller` · `Data.Duration` |
| Modifiers | `+` 1개 → `DRAttributeSet.MoveSpeed` · ★`Multiply (Additive)`★ · `Set by Caller` · `Data.MoveSpeed.Mult` |
| Components | `Grant Tags to Target Actor` → `Debuff.AirShot.Drag` |
| Stacking Type | `Aggregate by Target` |
| Stack Limit Count | `3` |
| Stack Duration Refresh Policy | `Refresh on Successful Application` |
| Stack Expiration Policy | `Clear Entire Stack` |
| Deny Overflow Application | **체크 해제** |

- ⚠️ 여기는 ①과 반대로 **Additive** 다. Compound 로 하면 3번 맞힌 적이 오히려 빨라진다.

**④ `GE_JetJump_Guard`** — 폴더 `…/VacuumCleaner/Abilities/Skill3/` · 물점프칩② 0.3초 무적

| 칸 | 값 |
|---|---|
| Duration Policy | `Has Duration` |
| Duration Magnitude | `Set by Caller` · `Data.Duration` |
| Modifiers | 없음 |
| Components | `Grant Tags to Target Actor` → `State.Invulnerable` |
| Stacking Type | `Aggregate by Target` |
| Stack Limit Count | `1` |
| Stack Duration Refresh Policy | `Refresh on Successful Application` |

- (선택) 무적 연출이 필요하면 이 GE 에 GameplayCue 를 하나 붙인다.

### 16.3 `DA_ProgressionConfig` 에 GE 연결

`Content/Blueprints/Progression/DA_ProgressionConfig` 열기 → **Chip Tuning** 펼치기 → 아래 칸을 채운다.

| 위치 | 넣을 것 |
|---|---|
| Ride → **Power Load Effect** | `GE_RideLoad` |
| Ride → **Eject Heal Effect** | `GE_Heal` (`Content/Blueprints/AbilitySystem/Player/Effects/`) |
| Ride → **Eject Ban Effect** | `GE_RideBan` |
| Air Shot → Drag → **Effect** | `GE_AirShot_Drag` |
| Jet Jump → **Guard Effect** | `GE_JetJump_Guard` |

- 숫자 칸은 **건드리지 않는다** — 코드 기본값이 맞다. 다만 Air Shot → Drag 의 **Magnitude 0.1 · Duration 2** 인지만 본다(다르면 0.1 / 2 로).
- 저장. (검사는 칩을 다 넣은 뒤 16.5 에서 한 번에 한다.)

### 16.4 `DA_ChipCatalog` 에 칩 12개 넣기

`Content/Blueprints/Progression/DA_ChipCatalog` 열기 → **Chips** 배열 옆 `+` 를 12번 → 맨 아래 새 칸부터 하나씩 채운다.

**12개 모두 같은 칸**

| 칸 | 값 |
|---|---|
| Owner Class | `RobotVacuum` |
| B Common Chip | 체크 해제 |
| Category | `돌파 칩` |
| Required Slot Count | `1` |

**모디파이어 넣는 법** — 칩의 **Modifiers** `+` → 한 줄마다 아래 6칸:
- **Stat**: 표의 이름(한글 표시 그대로 고른다)
- **Target Ability Tag**: ★그 칩의 Target Ability Tag 와 같은 태그★. 단 탑승칩②의 `최대 체력` 만 **비워 둔다**(캐릭터 전체 스탯)
- **Op**: 표에 "곱" = `곱연산(비율, 0.1 → ×1.1)`, "합" = `합연산(절대값)`
- **Value**: 표의 숫자(음수는 앞에 `-`)
- **B Is Drawback**: 표에 ✓ 면 체크(카드에 빨갛게 뜬다)
- **Detail Override**: 표에 문구가 있으면 그대로 입력(카드에 이 문구가 대신 뜬다). 없으면 비워 둔다

Tier 는 **Required Slot Tier** 칸이다. GrantedTags 는 **Granted Tags** 칸(태그 고르기와 같은 방법).

#### ① 탑승 3종 — Target Ability Tag = `Abilities.RobotVacuum.Ride`

| Chip Id | Display Name | Effect Summary | Required Slot Tier | Modifiers | Granted Tags |
|---|---|---|---|---|---|
| `RV.Asc.RidePower` | 합승 부스터 | 탑승 1명당 탑승자 공격력 +8% | 3 | (없음) | `Upgrade.Feature.Skill.Ride.Power` |
| `RV.Asc.RideGuard` | 대신 맞는 범퍼 | 탑승자 피해를 줄여서 대신 받음 | 4 | `최대 체력` · Tag **비움** · 합 · `-100` · ✓ | `Upgrade.Feature.Skill.Ride.Guard` |
| `RV.Asc.RideEject` | 비상 탈출 좌석 | 위험한 탑승자 탈출 + 체력 100 | 3 | (없음) | `Upgrade.Feature.Skill.Ride.Eject` |

Description(그대로 복사):
- RidePower — 이 청소기가 탑의 맨 아래에서 달릴 때, 위에 탄 사람 1명당 탑승자 전원의 공격력이 8% 오릅니다(최대 24%). 대신 1명당 이 청소기의 이동 속도가 5% 줄어듭니다. 다른 청소기 위에 업혀 있는 동안에는 동작하지 않습니다.
- RideGuard — 이 청소기보다 위에 탄 사람은 피해를 받지 않고, 그 사람이 받았어야 할 피해를 80% 줄여 이 청소기가 대신 받습니다(원래 함께 받던 피해에 더해집니다). 대신 최대 체력이 100 줄어듭니다.
- RideEject — 바로 위에 탄 사람이 피해를 받아 체력이 30% 이하가 되면 즉시 튕겨 내리고, 그 사람과 이 청소기 모두 체력을 100 회복합니다. 대신 탈출한 사람은 20초 동안 이 청소기에 다시 탈 수 없습니다.

#### ② 기본 공격(공기탄) 3종 — Target Ability Tag = `Abilities.RobotVacuum.AirShot`

| Chip Id | Display Name | Effect Summary | Required Slot Tier | Modifiers | Granted Tags |
|---|---|---|---|---|---|
| `RV.Asc.ShotPower` | 고압 공기탄 | 기본 공격 피해 +50% | 3 | `스킬 피해량` · 곱 · `0.5` | `Upgrade.Feature.Skill.AirShot.FriendlyFire` |
| `RV.Asc.ShotDrag` | 흡착 공기탄 | 맞은 적 감속 · 받는 피해 증가 | 4 | `스킬 피해량` · 곱 · `-0.2` · ✓ | `Upgrade.Feature.Skill.AirShot.Drag` |
| `RV.Asc.ShotQuick` | 속사 압축기 | 차지가 30% 빨라짐 | 3 | `스킬 선딜레이` · 곱 · `-0.3` · Detail `차지 간격 0.5 → 0.35초`<br>`스킬 차지 증가량` · 곱 · `-0.4` · ✓ · Detail `단계당 피해 +20 → +12` | (없음) |

Description:
- ShotPower — 기본 공격의 피해가 50% 늘어납니다. 대신 아군에게도 같은 피해를 줍니다(내가 태운 사람 · 나를 태운 청소기는 맞지 않습니다).
- ShotDrag — 기본 공격에 맞은 적은 2초 동안 이동 속도가 10% 줄고 받는 피해가 5% 늘어납니다. 맞힐 때마다 시간이 갱신되고 최대 3번까지 쌓입니다(이동 속도 −30%, 받는 피해 +15%). 보스에게는 걸리지 않습니다. 대신 기본 공격의 피해가 20% 줄어듭니다.
- ShotQuick — 기본 공격의 차지 단계가 0.35초마다 오릅니다(기존 0.5초). 대신 단계마다 늘어나는 피해가 20에서 12로 줄어듭니다.

#### ③ 물 점프 3종 — Target Ability Tag = `Abilities.RobotVacuum.JetJump`

| Chip Id | Display Name | Effect Summary | Required Slot Tier | Modifiers | Granted Tags |
|---|---|---|---|---|---|
| `RV.Asc.JumpBlast` | 폭압 분사구 | 물 점프 피해 강화 | 3 | `스킬 피해량` · 곱 · `1.0` · Detail `강판정 피해 20 → 40`<br>`스킬 외곽 피해` · 곱 · `0.5` · Detail `약판정 피해 5 → 15`<br>`스킬 상승 높이` · 곱 · `-0.3` · ✓ | (없음) |
| `RV.Asc.JumpGuard` | 완충 분사구 | 점프 순간 0.3초 무적 | 4 | `스킬 피해량` · 곱 · `-1.0` · ✓ · Detail `피해 0` | `Upgrade.Feature.Skill.JetJump.Guard` |
| `RV.Asc.JumpMulti` | 다단 분사구 | 공중 2회 · 물 20 | 3 | `스킬 물 소모량` · ★합★ · `-10` · Detail `물 소모 30 → 20`<br>`스킬 공중 사용 횟수` · ★합★ · `1` · Detail `공중 사용 1 → 2회`<br>`스킬 상승 높이` · 곱 · `-0.25` · ✓ | (없음) |

Description:
- JumpBlast — 물 점프의 피해가 커집니다 — 가까운 적(2m 안) 20 → 40, 먼 적(2~4m) 5 → 15. 대신 점프 높이가 30% 낮아집니다.
- JumpGuard — 물 점프를 쓰는 순간부터 0.3초 동안 어떤 공격도 받지 않습니다(피해 · 경직 · 넉백 · 디버프 모두). 대신 물 점프가 적에게 피해를 주지 않습니다.
- JumpMulti — 물 점프의 물 소모가 30에서 20으로 줄고, 공중에서 2번까지 쓸 수 있습니다. 대신 점프 높이가 25% 낮아집니다.

- ⚠️ JumpMulti 의 물 소모는 **합**(-10)이다. 곱(-0.3333)으로 넣으면 물이 정확히 20 일 때 점프가 안 된다.

#### ④ 돌진 3종 — Target Ability Tag = `Abilities.RobotVacuum.Dash`

| Chip Id | Display Name | Effect Summary | Required Slot Tier | Modifiers | Granted Tags |
|---|---|---|---|---|---|
| `RV.Asc.DashRam` | 충각 범퍼 | 돌진 충돌 피해 +80% | 3 | `스킬 피해량` · 곱 · `0.8` · Detail `칸당 충돌 피해 10 → 18`<br>`스킬 자해 피해` · 곱 · `1.0` · ✓ · Detail `벽 충돌 자해 50 → 100` | (없음) |
| `RV.Asc.DashPass` | 유령 주행 | 돌진 중 아군 통과 | 4 | `스킬 피해량` · 곱 · `-0.15` · ✓ · Detail `칸당 충돌 피해 10 → 8.5` | `Upgrade.Feature.Skill.Dash.PassAllies` |
| `RV.Asc.DashQuick` | 급속 충전기 | 돌진 충전 30% 빨라짐 | 3 | `스킬 선딜레이` · 곱 · `-0.3` · Detail `칸당 충전 0.5 → 0.35초`<br>`스킬 이속 버프` · 곱 · `-0.3` · ✓ · Detail `칸당 이속 +20% → +14%` | (없음) |

Description:
- DashRam — 돌진으로 적에게 부딪힐 때 게이지 1칸당 피해가 10에서 18로 늘어납니다. 대신 최대 충전(지속 돌진) 중 벽이나 지형지물에 부딪히면 받는 피해가 50에서 100으로 늘어납니다.
- DashPass — 돌진하는 동안 다른 플레이어와 부딪히지 않고 그대로 지나갑니다(아군에게 피해를 주거나 멈추지 않습니다). 대신 적에게 부딪힐 때의 피해가 15% 줄어듭니다.
- DashQuick — 돌진 게이지가 0.35초마다 1칸씩 찹니다(기존 0.5초). 대신 1칸당 이동 속도 증가가 20%에서 14%로 줄어듭니다.

#### ⑤ 미리보기 라벨 4줄 — 같은 `DA_ChipCatalog` 의 **Skill Display Names**

`+` 4번 → 왼쪽 칸(태그) · 오른쪽 칸(이름):

| 태그 | 이름 |
|---|---|
| `Abilities.RobotVacuum.Ride` | 탑승 |
| `Abilities.RobotVacuum.AirShot` | 기본 공격 |
| `Abilities.RobotVacuum.JetJump` | 물 점프 |
| `Abilities.RobotVacuum.Dash` | 돌진 |

- 저장.
- ⚠️ **Chip Id 는 세이브 파일의 열쇠**다. 한 번 넣고 플레이한 뒤에는 바꾸지 않는다(바꾸면 장착해 둔 칩이 사라진다). 이름 · 설명 · Tier 는 언제 바꿔도 된다.

### 16.5 검사 — 저장만으로는 안 돈다, 한 번 "편집"해야 돈다

1. 메뉴 **창(Window) → 출력 로그(Output Log)** 를 연다.
2. `DA_ProgressionConfig` 를 열고 아무 숫자 칸 하나를 바꿨다가 원래대로 되돌린다(검사는 편집할 때 돈다) → 저장.
3. 출력 로그에서 확인:
   - ✅ `[ProgressionConfig] DA_ProgressionConfig: 검증 통과 …` → 끝.
   - ❌ `[ProgressionConfig] …` 줄이 여러 개 → **그 문장에 틀린 칸 이름이 적혀 있다**. 아래 표로 찾아 고친 뒤 2번을 다시.

| 로그에 이런 말이 있으면 | 고칠 곳 |
|---|---|
| `Ride.PowerLoadEffect … Target Tags 에 Debuff.RobotVacuum.RideLoad 가 없습니다` | 16.2 ① Components |
| `… Multiply (Compound) 모디파이어가 없습니다` | 16.2 ① Modifier Op |
| `Ride.EjectBanEffect … Aggregate by Target` | 16.2 ② Stacking Type → `None` |
| `AirShot.Drag.Effect … Multiply (Additive)` | 16.2 ③ Modifier Op |
| `… Deny Overflow Application 이 켜져` | 16.2 ③ 체크 해제 |
| `… Stack Limit(…)이 DragMaxStacks(3)와 달라` | 16.2 ③ Stack Limit Count `3` |
| `JetJump.GuardEffect … Set by Caller(Data.Duration)` | 16.2 ④ Duration |
| `ChipTuning.… 가 비어 있어` | 16.3 해당 칸 |
| 칩 Id 와 함께 `TargetAbilityTag` · `Description` 이야기 | 16.4 그 칩의 태그 칸 / 설명 칸 |

### 16.6 빠른 확인 (PIE)

1. **Play** 옆 ⋮ → Net Mode `Play As Listen Server`, 플레이어 2명(탑승 확인은 3~4명).
2. 게임 안에서 `~` 키(콘솔) → `DRUnlockUpgrade` → `DRUnlockSlot 6`.
3. 청소기로 업그레이드 화면을 열어 칩을 낀다. 같은 스킬 칩은 하나만 낄 수 있다(정상).
4. 한 칩씩 확인한다 — 무엇을 보면 되는지는 각 절의 검증 목록:
   탑승 **§12.7** · 공기탄 **§13.7** · 물 점프 **§14.7** · 돌진 **§15.7**.
   서버 로그에서 `[RideLoad]` · `[RideEject]` 줄이 보이면 탑승칩①③이 동작한 것이다.

### 16.7 이상하면

| 증상 | 대부분의 원인 |
|---|---|
| 칩이 업그레이드 목록에 안 보인다 | Owner Class 가 `RobotVacuum` 이 아니다 / `B Common Chip` 이 켜져 있다 |
| 돌진하면 청소기가 안 움직인다(이속 0) | 16.1-4 의 Data Tag 가 `Data.MoveSpeed.Mult` 가 아니다 |
| 5칸 돌진이 엄청 빠르다 | 16.1-4 의 Modifier Op 를 Compound 로 바꿨다 |
| 벽에 박아도 자해가 없다 | 16.1-2(Self Damage Effect) 또는 16.1-3(Data Tag `Data.SelfDamage`) |
| 물 점프해도 물이 안 준다 | 16.1-1 |
| 흡착 3번 맞힌 적이 오히려 빠르다 | 16.2 ③ Op 가 Compound 다 |
| 탑승칩①이면 청소기가 점점 느려진다 | 16.2 ① 에 `Debuff.RobotVacuum.RideLoad` 태그가 없다 |
| 칩 하나가 청소기의 다른 스킬까지 바꾼다 | 그 칩 모디파이어의 Target Ability Tag 가 비어 있다(탑승칩② `최대 체력` 만 비워야 한다) |
| 카드 숫자가 이상하다(예: "+100%") | Detail Override 문구를 안 넣었다 — 틀린 건 아니다, 표의 문구를 넣으면 깔끔해진다 |
