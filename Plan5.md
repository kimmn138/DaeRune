# Plan5 — 로봇 전용 칩 30종 (정원로봇 9 · 자판기 9 · 청소기 12) + 스킬당 1칩 규칙

> **상태**
> - 정원로봇 9종(씨앗 폭탄 · 물대포 · 발톱): **구현 완료**. 씨앗 폭탄·물대포는 게임 검증 완료, **발톱은 게임 검증이 남았다 → §10**
> - 자판기 기본 공격(동전) 3종 · 잭팟 3종: **구현 완료**(C++ 빌드 통과 · 에셋 입력 확인 2026-09-28), **게임 검증 남음 → §10**
> - 자판기 공속 버프 3종(§1.6): **구현 완료**(C++ 빌드 통과 · 에셋 입력 확인 2026-10-07), **게임 검증 남음 → §10.3**
> - 장착 규칙 "같은 스킬 칩은 1개만"(§1.7): **구현 완료**, 게임 검증 남음 → §10
> - 로봇 청소기 칩 12종(탑승 · 공기탄 · 물 점프 · 돌진 — §1.9~§1.13): **구현 완료**(C++ 빌드 통과 · 에셋 입력 확인 2026-10-08, 커밋 `89120e20`), **게임 검증 남음 → §10.4**. 칩 없이도 바뀐 것 5건(§1.13)
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

## 1. 칩 30종 최종 스펙

공통: 로봇 전용(`bCommonChip = false`), `Category = Ascension`, `RequiredSlotCount = 1`. ★칩 레벨 `TargetAbilityTag` 가 같은 칩은 동시에 1개만 장착된다(§1.7)★.
모디파이어는 `Flat` 표시가 있는 것(동전 ① 발사 간격 · 탑승 ② 최대 체력 · 물 점프 ③ 물 · 공중 횟수)을 빼면 전부 `Op = Percent`(곱연산)이며 **각 모디파이어마다 `TargetAbilityTag` 를 지정**한다
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
- 그룹 10개 × 3칩: 씨앗 폭탄 / 물대포 / 발톱 / 동전(`…VendingMachine.BasicAttack`) / 잭팟(`…VendingMachine.Jackpot`) / 공속 버프(`…VendingMachine.AttackSpeedBuff`) / 청소기 탑승(`…RobotVacuum.Ride` — 그룹 키 전용) / 공기탄(`…RobotVacuum.AirShot`) / 물 점프(`…RobotVacuum.JetJump`) / 돌진(`…RobotVacuum.Dash`).
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

### 1.9 청소기 탑승(패시브) 3종 — `OwnerClass = RobotVacuum`, `TargetAbilityTag = Abilities.RobotVacuum.Ride`(그룹 키 전용 — GA 에 없다)

| # | `ChipId` | 이름 | Modifiers | GrantedTags | Tier | 아이콘 |
|---|---|---|---|---|---|---|
| ① | `RV.Asc.RidePower` | 합승 부스터 | (없음) | `…Ride.Power` | 3 | 인간타워 |
| ② | `RV.Asc.RideGuard` | 대신 맞는 범퍼 | `MaxHealth` **Flat −100** ★단점★(태그 비움) | `…Ride.Guard` | 4 | 완충 |
| ③ | `RV.Asc.RideEject` | 비상 탈출 좌석 | (없음) | `…Ride.Eject` | 3 | 긴급하차 |

"탑" = 청소기 위에 청소기가 타는 기존 탑승(맨 아래 위로 최대 3명 — 새 기능 아님). 탑승 공유 = 받은 피해를 **바로 위 · 아래 1칸**에 그대로(D-92).

- ① 탑의 **맨 아래**(직접 달리는) 청소기가 ①이면 위 인원 N명당 **탑승자 전원**의 주는 피해 +8%(최대 +24%), 그 청소기 이속 −5%/명.
  위층 청소기의 ①은 업혀 있는 동안 쉰다(D-79). 청소기 자신 · 탑승자의 DoT · 공유분에는 안 붙는다.
- ② 이 청소기 **위의 전원**이 보호된다 — 그 사람이 최종적으로 받아야 할 피해 L(받는 쪽 배율 · 데미지 디버프 반영)을 받지 않고,
  **L × 0.2** 가 아래의 가장 가까운 ② 청소기로 넘어간다(피격 자체의 1칸 공유 L 은 그대로). 디버프는 보호받는 사람에게도 걸리고 그 DoT 틱이 20% 로 넘어간다(D-84).
  대신 최대 체력 400 → **300**(칸 75 × 4). 오염 최대는 칩과 무관하게 100(D-87).
- ③ **바로 위 1명**이 피격 직후 체력 ≤ 30% 이거나 그 피격으로 오염에 들어가면 → 점프 하차 + 탑승자 · 청소기 각각 **+100** →
  그 사람은 **20초** 동안 **이 청소기에** 못 탄다(서버 거부 + F 프롬프트 없음, 다른 청소기는 탈 수 있다). 체력 **비용**(⑧ · 발톱 수혈)으로는 발동 안 함.

| ① N | 탑승자 주는 피해 | 청소기 이속(550) |
|---|---|---|
| 0 / 1 / 2 / 3 | ×1.0 / 1.08 / 1.16 / **1.24** | 550 / 522.5 / 495 / 467.5 |

| ② 상황 (피해 20) | 칩 없음 | ② |
|---|---|---|
| 2층 V② ← R, R 또는 V 가 맞음 | 둘 다 20 | R **0** / V **24** (20 + 4) |
| 3층 A② ← B ← C — C / B / A 가 맞음 | A 0 · 20 · 20 | A **8** / **28** / **24**, B · C 0 |
| ② 두 대 A② ← B② ← C, B 가 맞음 | 셋 다 20 | A **24.8** (0.2 가 두 번 — §9-46) |
| 화상 틱 5 (V② ← R) / 지형 충돌 자해 50 (V② ← R) | 5 · 5 / 50 · 50 | V **6** / V **60**, R 0 |

### 1.10 청소기 기본 공격(공기탄) 3종 — `TargetAbilityTag = Abilities.RobotVacuum.AirShot`

| # | `ChipId` | 이름 | Modifiers | GrantedTags | Tier | 아이콘 |
|---|---|---|---|---|---|---|
| ① | `RV.Asc.ShotPower` | 고압 공기탄 | `SkillDamage` **+0.5** | `…AirShot.FriendlyFire` | 3 | 고압노즐 |
| ② | `RV.Asc.ShotDrag` | 흡착 공기탄 | `SkillDamage` **−0.2** ★단점★ | `…AirShot.Drag` | 4 | 감속벨브 |
| ③ | `RV.Asc.ShotQuick` | 속사 압축기 | `SkillWindup` **−0.3**<br>`SkillChargeGrowth` **−0.4** ★단점★ | (없음) | 3 | 저압밸브 |

기준(C++ — BP 가 안 덮는다): 차지 간격 0.5초, 단계 피해 **10 / 30 / 50 / 70**(+20/단계 — 예전 10/15/22/35, D-101), 3단계 = 강화탄(넉백 · 반동).

- ① 피해 ×1.5. 대신 **아군에게도 같은 피해**(공격 배율 포함, 강화탄은 피해 + 넉백). 탑승 연결(내 위 탑승자 · 내가 탄 청소기)은 통과.
- ② 맞은 적 1스택: 이속 −10% · **받는 모든 피해** +5%(누가 · 무엇으로든), 2초 · 적중마다 갱신, **적 한 마리당 합쳐 최대 3**(−30% / +15%), 마지막 적중 2초 뒤 한꺼번에 해제.
  **보스(`bIsBoss` — 엘리트 곰 · 두더지 보스)는 둘 다 없음**, 일반 엘리트는 걸림. 받는 피해는 그 적중 **직전** 스택으로 계산(첫 적중 +0%). 대신 피해 ×0.8.
- ③ 차지 간격 0.5 → **0.35초**(공속 버프와 곱). 대신 단계당 증가 +20 → **+12**(0단계 10 은 그대로).

| 단계 피해 | 0 | 1 | 2 | 3 | 풀차지 | 홀드 기준 초당 피해(1 / 2 / 3단계) |
|---|---|---|---|---|---|---|
| 칩 없음 | 10 | 30 | 50 | 70 | 1.5초 | 60 / 50 / 46.7 |
| ① | 15 | 45 | 75 | 105 | 1.5초 | 90 / 75 / 70 |
| ② | 8 | 24 | 40 | 56 | 1.5초 | 48 / 40 / 37.3 (+ 3스택 적 ×1.15) |
| ③ | 10 | 22 | 34 | 46 | **1.05초** | 62.9 / 48.6 / 43.8 — 이득은 강화탄까지 30% 빨리 |

### 1.11 청소기 물 점프(스킬 1 · Q) 3종 — `TargetAbilityTag = Abilities.RobotVacuum.JetJump`

| # | `ChipId` | 이름 | Modifiers | GrantedTags | Tier | 아이콘 |
|---|---|---|---|---|---|---|
| ① | `RV.Asc.JumpBlast` | 폭압 분사구 | `SkillDamage` **+1.0**<br>`SkillOuterDamage` **+0.5**<br>`SkillLaunchHeight` **−0.3** ★단점★ | (없음) | 3 | 고압분사 |
| ② | `RV.Asc.JumpGuard` | 완충 분사구 | `SkillDamage` **−1.0** ★단점★ | `…JetJump.Guard` | 4 | 완충_버스트 |
| ③ | `RV.Asc.JumpMulti` | 다단 분사구 | `SkillWaterCost` **Flat −10**<br>`SkillAirUseCount` **Flat +1**<br>`SkillLaunchHeight` **−0.25** ★단점★ | (없음) | 3 | 저압버스트 |

기준(C++ — BP 가 안 덮는다): 물 **30**(실제로 빠진다 — 예전엔 확인만 했다, D-103) · 점프 임펄스 900(중력 −980 → 최고 높이 ≈ 413uu) ·
2D 반경 200 안 강판정 **20** / 200~400 약판정 **5** · 공중 사용 1회(지상 Q → 공중 Q) · 쿨다운 GE 0.3초.

- ① 강판정 20 → **40**, 약판정 5 → **15**. 대신 최고 높이 ×0.70(임펄스 ×√0.7).
- ② 쓰는 순간부터 **0.3초 무적** — 피격 통째로 무효(피해 · 경직 · 넉백 · 디버프 · DoT 틱 · 탑승 공유, **모자보다 먼저**라 모자가 안 닳는다). 대신 물 점프 피해 0(판정 생략).
  물값으로 내는 체력은 막지 않는다. 적 GA 가 피해 없이 직접 거는 넉백 · 스턴은 못 막는다(§9-58).
- ③ 물 30 → **20**, 공중 사용 1 → **2회**. 대신 최고 높이 ×0.75.
- "상승 높이" · "상승력"은 둘 다 **실제 최고 높이** 기준(사용자 확정) — 높이 ∝ 임펄스² 이라 임펄스에는 √배율.

| | 강 / 약 | 1회 높이 | 물 | 공중 | 한 번 뜰 때 최대(정점마다 Q) |
|---|---|---|---|---|---|
| 칩 없음 | 20 / 5 | 413uu | 30 | 1 | 2회 826uu · 물 60 · 약 3.1초 |
| ① | **40 / 15** | 289uu | 30 | 1 | 579uu · 2.6초 |
| ② | 0 | 413uu | 30 | 1 | 826uu · Q 마다 0.3초 무적(연속이면 0.6초) |
| ③ | 20 / 5 | 310uu | **20** | **2** | 3회 **930uu** · 물 60 · 3.8초 |

### 1.12 청소기 돌진(스킬 2 · RMB) 3종 — `TargetAbilityTag = Abilities.RobotVacuum.Dash`

| # | `ChipId` | 이름 | Modifiers | GrantedTags | Tier | 아이콘 |
|---|---|---|---|---|---|---|
| ① | `RV.Asc.DashRam` | 충각 범퍼 | `SkillDamage` **+0.8**<br>`SkillSelfDamage` **+1.0** ★단점★ | (없음) | 3 | 과충전_코일 |
| ② | `RV.Asc.DashPass` | 유령 주행 | `SkillDamage` **−0.15** ★단점★ | `…Dash.PassAllies` | 4 | 무해코팅 |
| ③ | `RV.Asc.DashQuick` | 급속 충전기 | `SkillWindup` **−0.3**<br>`SkillSpeedBonus` **−0.3** ★단점★ | (없음) | 3 | 저항_브레이크 |

기준(C++ — BP 가 안 덮는다): 칸당 충전 0.5초 · 물 20, 최대 5칸. 칸당 이속 +20%(550 → 5칸 1100), 1~4칸은 1초에 1칸 감쇠, 5칸 = 지속 돌진.
충돌 피해 = 칸 수 × (적 10 / 다른 플레이어 5), 부딪히면 끝. **자해 = 5칸 지속 돌진 중 지형(Pawn 아닌 것) 충돌만 50**(예전엔 BP 오배선으로 0, D-112).

- ① **적** 충돌 피해 칸당 10 → **18**(아군 5 는 그대로). 대신 지형 충돌 자해 50 → **100**.
- ② 돌진 중 **다른 플레이어를 통과**(충돌 · 피해 · 정지 없음, 끝나면 다시 막힌다). 대신 적 충돌 피해 칸당 **8.5**.
- ③ 칸당 충전 0.5 → **0.35초**(5칸 2.5 → 1.75초, 충전 중 초당 물 40 → 57). 대신 칸당 이속 +20% → **+14%**(5칸 1100 → 935).

| 적 충돌 피해 1 / 2 / 3 / 4 / 5칸 | 칩 없음 10 · 20 · 30 · 40 · 50 / ① 18 · 36 · 54 · 72 · 90 / ② 8.5 · 17 · 25.5 · 34 · 42.5 |
|---|---|
| 이속 1 / … / 5칸 | 칩 없음 660 · 770 · 880 · 990 · 1100 / ③ 627 · 704 · 781 · 858 · 935 |
| 4칸 일반 돌진 거리(가속 무시) | 칩 없음 3300uu / ③ 2970uu |

### 1.13 청소기 칩 — 칩 없이도 바뀐 것 · 상호작용

★선행 변경 5건(2026-10-08 — 칩을 안 껴도 적용된다)★

| # | 바뀐 것 | 결정 |
|---|---|---|
| 1 | 탑승 공유 피해에 넘겨주는 사람의 **주는 피해** 배율(광폭화 · 시너지 · `DamageDealt` · 탑승①)이 안 붙는다 — 광폭화 탑승자 20 피격 → 청소기 45~55 였던 것이 **20** | D-82 |
| 2 | 오염 최대 체력은 칩과 무관하게 **항상 100**(+100 칩 오염 200 → 100, ⑧ 900 → 100 / 정화 뒤 ⑧ 2000 → 1200) | D-87 |
| 3 | 공기탄 단계 피해 10 / 15 / 22 / 35 → **10 / 30 / 50 / 70** | D-101 |
| 4 | 물 점프가 물 30 을 **실제로 소모**(물이 모자라면 기존 규칙대로 체력 대납) | D-103 |
| 5 | 5칸 돌진 **지형 충돌 자해 50**(예전 0). 적 · 아군 충돌 자해는 없음 | D-112 |

| 조합 | 결과 |
|---|---|
| 공용 ⑥ 광폭화 · ⑦ 시너지 · `DamageDealt` · 탑승① | 공기탄 · 물 점프 · 돌진 피해에 ExecCalc 소스측에서 추가로 곱. 공기탄① 아군 피해에도 붙는다 / 탑승 공유 · 이전분에는 **안 붙는다** |
| 탑승② + 모자 쓴 탑승자 / 도발된 적 | 모자가 먼저 막는다(아무도 피해 없음) / ×0.8 이 먼저 적용된 양이 공유 · 이전 |
| 탑승② + 광폭화 **청소기** | 피격 1회가 여러 번(공유분 + 이전분마다) 들어와 칸을 그만큼 비운다 — 3층 가운데 피격이면 3칸 ★최악의 조합★ (§9-37) |
| 탑승③ + 회복 차단(⑥) / 오염 탑승자 | 탈출 · 금지만 / 회복 100 = 정화 |
| 물 점프② 무적 + 모자 · 탑승 공유 · 오염 | 모자 유지 · 내가 맞으면 공유도 안 감(탑승자가 맞으면 탑승자만 맞음) · 오염 피해도 0 |
| 공기탄② + 물대포② 취약 / 씨앗 · 물대포 감속 | ×1.5 × 1.15 = **×1.725** / Compound 와 곱(×0.7 × ×0.7 = ×0.49) |
| 공기탄② + 비행 적 · 튜토리얼 더미 | 감속 없이 피해 증가만(§9-1) / 걸린다 |
| 공기탄③ · 돌진③ 의 `SkillWindup` | 모디파이어 태그가 각 GA 라 서로 · 발톱에 안 퍼진다. 공속 버프(M)는 공기탄에만 곱(돌진 충전엔 원래 없음) |
| 돌진 자해 + 탑승 · 탑승② · 모자 · 물 점프② | 공유된다(②면 청소기 60 / ①이면 120) · 모자가 1회 막음 · 무적 중이면 0 |
| 돌진① ② + 클렌저 사이트 | 사이트는 "적" 판정이라 충돌 피해(18 · 8.5)를 받고, Pawn 이 아니라 **지형**이기도 해서 5칸이면 자해도 받는다(§9-67) |
| 네 그룹 | 서로 다른 그룹이라 그룹마다 1개씩 동시에 낄 수 있다 |

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

청소기 칩 (판정 · 정산은 전부 서버)
   주는 쪽  ExecCalc_Damage (d) GetRideDamageMultiplier(SourceAvatar)   ★Damage.MountShared 면 (a)~(d) 전부 건너뜀★
   받는 쪽  UDRAttributeSet 기본 관문  [사망 무시 → ★무적 State.Invulnerable★ → 모자 → 도발 ×0.8] → HandleIncomingDamage
              플레이어: 맨 위 FindRideGuard → … 받는 쪽 배율 → (보호받는 사람) TransferRideDamageToGuard(L×0.2) + Debuff + PropagateSharedDamage(L) → return
                        … → Process · Debuff · PropagateSharedDamage → ★맨 끝★ TryRideEmergencyEject
              적:       엘리트 → 취약 → ×GetAirShotDragDamageTakenMultiplier → 흡혈 → 적중 회복 → TryApplyAirShotDrag → 더미 / SetHealth
   탑승 감속  MountRider · DismountRider 끝 / RefreshUpgradeEffects → OnUpgradeEffectsRefreshed → RefreshRideLoad (맨 아래 청소기만, 태그로 지우고 다시)
   물 점프   ActivateAbility(서버): 무적 GE → AirJumpsUsed++ → Launch(900 × √SkillLaunchHeight) → AoE(Inner × SkillDamage / Outer × SkillDamage × SkillOuterDamage)
   돌진      StartChargingGauge(간격 × SkillWindup) → ApplyBuffStacks(SetByCaller 1 + 0.2 × SkillSpeedBonus) → ReleaseGauge: SetDashPassThroughAllies(복제)
             → 모든 머신 OnRep → ApplyAllyPassThrough(양방향 IgnoreActorWhenMoving) / HandleDashImpact: 적만 × SkillDamage, 지속 + 지형이면 자해(Data.SelfDamage)

[DA_ProgressionConfig.ChipTuning]  숫자로 표현 못 하는 동적 상수
   └ .SeedBomb / .WaterPump / .Claw / .Coin / .Jackpot / .AttackSpeedBuff / .Ride / .AirShot / .JetJump  →  UDRAbilitySystemLibrary::GetChipTuning(WorldContext)
```

### 2.2 스탯 축 (`EDRUpgradeStat`) — 전부 곱연산

| 구분 | 축 |
|---|---|
| 캐릭터 단위 | `ContainerHealth` `MaxWater` `MoveSpeed` `DamageTaken` `WaterGain` `MaxHealth` `HealthRegenDelay` `DamageDealt` |
| 스킬 단위 | `SkillDamage` `SkillWaterCost` `SkillCooldown` `SkillProjectileCount` **`SkillHealing`** **`SkillAreaRadius`** **`SkillWaterGrant`** **`SkillWindup`** **`SkillFireInterval`** **`SkillProjectileScale`** **`SkillChargeGrowth`** **`SkillOuterDamage`** **`SkillLaunchHeight`** **`SkillAirUseCount`** **`SkillSelfDamage`** **`SkillSpeedBonus`** |

★굵은 12개가 Plan5 에서 추가됐다(이 순서로 `Count` 앞에 붙였다).★ 스킬 단위 축은 **반드시 `DRIsSkillStat()` 에도 추가**해야 한다(T-12).

| 축 | 단위 | `DRIsLowerBetter` | `DRIsTimeStat` | 이유 |
|---|---|---|---|---|
| `SkillWindup` | 기준 1.0 배율 | ✓ | ✗ (T-25) | 선딜 길이는 몽타주 안에 있어 어빌리티가 초를 모른다(D-21). 공기탄 · 돌진의 **충전 간격**에도 쓴다(D-99 · D-117) |
| `SkillFireInterval` | **초** | ✓ | **✓** | 발사 간격은 GA 프로퍼티라 초를 안다(D-37). 빠뜨리면 미리보기 색·단위가 틀린다(T-37) |
| `SkillProjectileScale` | 기준 1.0 배율 | ✗ | ✗ | 투사체 반경은 발사체 BP 에 있다(D-40) |
| `SkillChargeGrowth` | 단계 **증가분** 배율 | ✗ | ✗ | 0단계는 그대로(D-100 · T-104) |
| `SkillOuterDamage` | 외부(약)판정 피해 배율 | ✗ | ✗ | `SkillDamage` 위에 외부에만 한 번 더(D-104). 지금은 물 점프만 읽는다 |
| `SkillLaunchHeight` | **실제 최고 높이** 배율 | ✗ | ✗ | 임펄스엔 √배율(D-105 · T-109) |
| `SkillAirUseCount` | 횟수(`Flat`) | ✗ | ✗ | GA 가 반올림해 읽는다(D-107) |
| `SkillSelfDamage` | 자해 배율 | **✓** | ✗ | 빠뜨리면 "+100%" 가 초록(T-130) |
| `SkillSpeedBonus` | 칸당 이속 **증가분** 배율 | ✗ | ✗ | 1.2 가 아니라 0.2 에 곱한다(D-118) |

**`Mult` 는 곱연산 누산기다** — `Percent` 에서 이름을 바꾼 이유가 그것이다(D-7).
`Mult *= (1 + Value)` 로 누적되므로 감소 칩을 아무리 겹쳐도 부호가 뒤집히지 않는다
(합연산이던 시절 −80% + −40% = −120% → 배율 음수 → **적이 회복**하는 버그가 있었다).

### 2.3 세 종류의 태그 — ★절대 섞지 말 것★

| | **칩 피처 태그**<br>`Upgrade.Feature.*` | **GAS 태그**<br>`Buff.*` / `Debuff.*` | **SetByCaller 태그**<br>`Data.*` |
|---|---|---|---|
| 어디에 사나 | `FDRUpgradeRuntime::GrantedTags` — **ASC 에 없다** | **ASC 에 있다** (GE 의 Granted Tags) | GE 스펙 안 — 크기를 넘기는 키일 뿐, 어디에도 부여되지 않는다 |
| 언제 변하나 | 로드아웃이 바뀔 때만 | 런타임에 수시로 | GE 를 적용할 때마다 |
| 쓰는 법 | `HasUpgradeFeature(ASC, Tag)` | `HasMatchingGameplayTag()` / GE 핸들의 `GetCurrentStackCount()` | `SetSetByCallerMagnitude(Tag, 값)` |
| Plan5 에서 추가 | `…SeedBomb.{Refund,Slow,Haste}`<br>`…WaterPump.{Ramp,Slow,Vulnerable,Pierce}`<br>`…Claw.{Lifesteal,AllyHeal}`<br>`…Coin.HitHeal`<br>`…Jackpot.{GoldRush,Double,Pity}`<br>`…AttackSpeedBuff.{Overdrive,Share,Decay}`<br>`…Ride.{Power,Guard,Eject}`<br>`…AirShot.{FriendlyFire,Drag}`<br>`…JetJump.Guard`<br>`…Dash.PassAllies` | `Buff.WaterPump.Pressure`<br>`Debuff.Vulnerable`<br>`Buff.AttackSpeed.Shared`<br>`Debuff.RobotVacuum.RideLoad`<br>`Debuff.Ride.Banned`<br>`Debuff.AirShot.Drag`<br>`State.Invulnerable`(범용)<br>(에셋 전용) `Debuff.SeedBomb.Slow` `Buff.SeedBomb.Haste` `Debuff.WaterPump.Slow` | `Data.MoveSpeed.Mult`<br>`Data.Duration`<br>`Data.HealthCost`<br>`Data.SelfDamage` |

- 피처 태그를 **ASC 에 Loose 로 얹지 말 것** — 어빌리티의 `ActivationBlockedTags` 와 의도치 않게 상호작용한다.
- C++ 이 읽는 GAS 태그만 **네이티브**로 등록한다. VFX/에셋에서만 쓰는 태그는 에디터에서 만들면 된다.
- 발톱칩·동전칩·잭팟칩은 새 GAS 태그가 없다 — 지속 상태 없이 즉발 효과뿐이다.
  공속칩은 `Buff.AttackSpeed.Shared` 하나(② 공유 버프 — 자기 버프와 스택을 섞지 않으려고, D-63).
- `Abilities.VendingMachine.Jackpot` · `Abilities.RobotVacuum.Ride` 는 ★칩 그룹 키 전용★ 이다 — 어떤 GA 에도 붙이지 않는다(T-51 · T-90).
- ★GE 를 태그로 찾을 때는 `MakeQuery_MatchAnyOwningTags`★ — `GetActiveEffectsWithAllTags` 는 에셋 태그만 본다(T-82 / T-131).

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
| **S** | 탑승 공유 GE 적용 한 곳 + 공유는 받는 피해 | `ADRCharacter::ApplyMountSharedDamageTo()` / ExecCalc 가 `Damage.MountShared` 면 (a)~(d) 건너뜀 |
| **T** | 오염 최대 체력 = 현재값 기준 고정 | `UDRPlayerAttributeSet::SetMaxHealthCurrentValue()` · `ReapplyCorruptMaxHealth()` + `RefreshUpgradeEffects` 의 `ON_SCOPE_EXIT` |
| **U** | 파생 캐릭터의 업그레이드 훅 | `ADRCharacter::OnUpgradeEffectsRefreshed()` — 모든 머신, 조기 return 앞 |
| **V** | 무적 | `State.Invulnerable` + `IsInvulnerable()` — 기본 관문 맨 앞(모자보다 먼저) |
| **W** | 발사체 피해 적용 추출 | `ADRProjectile::ApplyImpactDamage(Target)` — 팀 판정 없음(호출부 책임) |
| **X** | GE 스택 조회 = OwningTags | `GetEffectStackCountWithTag()` — 에셋 ∪ 부여 태그(2026-10-08 전환, 공속 GE 결과 불변) |

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
| **D-77** | 탑승 그룹 키 = 새 태그 `Abilities.RobotVacuum.Ride`(칩 레벨 · `SkillDisplayNames` 전용, GA 에 없음). 스위치 `Upgrade.Feature.Skill.Ride.{Power,Guard,Eject}` | 탑승은 GA 가 없는 패시브 — 잭팟(D-53)과 같은 처리. `Skill` 하위라 D-51 검사가 칩 태그를 강제한다 |
| **D-78** | 탑승① 공격력은 **상태 없이** ExecCalc (d) `GetRideDamageMultiplier(SourceAvatar)` — 시너지처럼 광폭화 상한 밖 | 탑 구조(`MountedOn`/`RiderOnTop`)는 이미 복제된다. 탑승자에게 GE 를 걸면 탑승 · 하차 · 사망마다 걸고 지울 곳이 생긴다(대신 HUD 표시 없음 — §9-38) |
| **D-79** | 탑승①은 **맨 아래 청소기의 칩만** — 위층 ①은 쉰다 | 사용자 확정. 단점(감속)이 항상 의미를 갖고, 여러 ①의 합산 문제가 없다 |
| **D-80** | 탑승① 감속 = 무한 `GE_RideLoad`(MoveSpeed **Compound** ← `Data.MoveSpeed.Mult` = 1 − 0.05N). `RefreshRideLoad()` 가 **태그로** 지우고 조건이 맞으면 다시 건다(멱등) | GE 라야 서버 · 소유 클라 CMC 가 같은 속도를 쓴다. Compound = 돌진 버프까지 포함한 최종 속도에 −5%/명. 태그로 지우는 이유는 T-84 |
| **D-81** | 감속 갱신 = `MountRider` · `DismountRider` 끝(오르내린 청소기 + 나부터 맨 아래까지) + `RefreshUpgradeEffects` 의 가상 훅 `OnUpgradeEffectsRefreshed()` | 사망 · EndPlay · 하차 · ③ 탈출이 전부 `DismountRider` 를 지난다. 위층끼리 타고 내려도 맨 아래 인원이 바뀐다(T-85) |
| **D-82** | ★선행★ 탑승 공유 · 이전 피해(`Damage.MountShared`)는 **받는 피해** — ExecCalc 의 주는 피해 배율 (a)~(d)를 전부 건너뛴다 | 사용자 확정. 예전엔 소스가 원 피격자라 그 사람의 광폭화가 공유분을 키웠다(20 → 45~55) |
| **D-83** | 탑승② = **보호받는 사람의 관문**에서 넘긴다 — `FindRideGuard` → 받는 쪽 배율 뒤 `TransferRideDamageToGuard`(L × 0.2, `ApplyMountSharedDamageTo`) + 1칸 공유 L 그대로 + 자기 처리 생략 | 관문 한 곳으로 직접 피격 · 공유 · DoT · 자해 · 여러 층이 같은 규칙으로 풀린다. `MountShared` 라 재공유 없음(T-74) |
| **D-84** | 보호받는 사람: 그 피격의 큐 · 히트리액트 · 넉백 · 표정 · 숫자 없음, **디버프는 건다**(DoT 틱이 다시 관문을 지나 20% 로 넘어감), 전투 진입은 남김. 모자 · 도발은 관문 앞이라 먼저 | 사용자 확정(2차 정정 — "디버프로 받는 피해도 넘긴다"). 스턴 같은 피해 없는 디버프도 탑승자가 받는다(§9-40) |
| **D-85** | 이전 비율 = `ChipTuning.Ride.GuardTransferRatio` 0.2 — **이전분에만** | ② 청소기 자신의 공유분은 100%(T-93). ② 두 대가 겹치면 0.2 가 두 번(§9-46) |
| **D-86** | 탑승② 최대 체력 −100 = 카탈로그 `MaxHealth Flat −100`(모디파이어 태그 비움) → 칸 75 × 4 | Plan3 분배 규칙 그대로. ★D-87 없이는 오염 최대가 0★ |
| **D-87** | ★선행★ 오염 최대 체력은 칩과 무관하게 **항상 100** — `SetMaxHealthCurrentValue`(Base = 목표 − (현재 − Base))로 진입 · 해제 · 정화 3곳 + 오염 중 `RefreshUpgradeEffects` 끝(`ON_SCOPE_EXIT`)에서 다시 100 · 체력 복원 | 사용자 확정. `SetMaxHealth` 는 BaseValue 라 칩 AddBase 가 다시 붙었다(+100 칩 오염 200, ⑧ 900, 칩② 0) |
| **D-88** | 탑승③ = **바로 위 1명**, 피격 처리 **맨 끝**, 체력 ≤ 30% **또는** 이번 피격 오염 진입, 매번. 체력 비용으로는 안 함 | 사용자 확정. 오염 진입은 비율로 못 잡고(T-79), 맨 끝이라야 공유가 먼저 나간다(T-78). 비용 발동은 자해로 회복을 뽑는다 |
| **D-89** | 탑승③ 실행 `EmergencyEjectRider()`: 점프 하차 → 둘 다 `ApplyHealToActor` → 탑승자에게 금지 GE | 먼저 내려야 회복 · 금지가 탑 상태와 무관하다 |
| **D-90** | 금지 = 탑승자에게 `GE_RideBan`(`Debuff.Ride.Banned`, **Stacking None**, 소스 = ③ 청소기 ASC). `IsRideBannedFrom()` 하나를 서버 `CanBeMountedBy` 와 소유 클라 프롬프트가 같이 쓴다 | 만료 · 복제는 GAS 몫. ASC(PlayerState)로 비교해 청소기가 리스폰해도 이어진다 |
| **D-91** | 칩 판정은 라이브러리(`GetRideDamageMultiplier` · `FindRideGuard` · `TransferRideDamageToGuard` · `TryRideEmergencyEject` · `IsRideBannedFrom`), 공유 · 이전 GE 적용은 `ApplyMountSharedDamageTo` 하나 | D-23 원칙. 공유와 이전이 같은 함수라 GE · 태그가 어긋날 수 없다 |
| **D-92** | 탑승 공유 범위 = **바로 위 · 아래 1칸**(기존 그대로) | 사용자 확정(탑 전체 공유를 검토한 뒤 기각). ② 수치가 이 범위 전제. 넓히는 법은 T-92 |
| **D-93** | 공기탄 그룹 키 = GA 태그 `Abilities.RobotVacuum.AirShot`. 스위치 `…AirShot.{FriendlyFire,Drag}` | GA 의 `AbilityTags` 와 일치 — 동전칩과 같은 방식 |
| **D-94** | 공기탄① 피해 = `SkillDamage +0.5`. 아군 피해 = `FireShot` 이 스폰 때 `bHitsAllies`(복제)를 싣고 `ADRVacuumAirProjectile` 아군 분기가 `ADRProjectile::ApplyImpactDamage`(추출한 범용 함수)를 부른다 | 비행 중 로드아웃 불변(D-5). 칩 지식은 청소기 자식 클래스에만(T-106). 탑승 연결 통과는 유지(사용자 확정) |
| **D-95** | 공기탄① 아군 피해 = 적에게 주는 것과 **같은 계산**(소스측 배율 포함), 강화탄이면 피해 + 넉백 | "동일한 피해". 적 전용 배율(엘리트 · 취약 · 흡혈)은 적 어트리뷰트셋에만 있어 아군엔 원래 없다 |
| **D-96** | 공기탄② 표식 = `GE_AirShot_Drag`(SetByCaller 2초 · by Target · Limit 3 · Refresh · Clear Entire Stack · Deny Overflow 끔 · MoveSpeed **Additive** 0.9 · `Debuff.AirShot.Drag`), `ApplyTimedSpeedEffect` 재사용 | 적 한 마리당 합산 3 · 적중마다 갱신(사용자 확정). Additive 라야 0.9 / 0.8 / 0.7(T-95), Deny Overflow 면 3스택에서 풀린다(T-96) |
| **D-97** | 공기탄② 적용 = 적 관문(적중 회복 블록 다음) `TryApplyAirShotDrag` — 피처 **AND** `SourceAbilityTags` 에 `AirShot` **AND** `ADREnemy` · 비보스(`IsBoss()`). 이번 적중의 배율은 **직전** 스택 | 피해가 확정되는 유일한 지점(D-41). `AirShot` 검사 없으면 돌진 충돌도 쌓는다(T-99) |
| **D-98** | 공기탄② 받는 피해 = 적 관문 **취약 다음** × (1 + 0.05 × 스택) — 누가 · 무엇으로든(DoT 포함). 스택 = `GetEffectStackCountWithTag`, `DragMaxStacks` 로 자름 | D-10. 태그 카운트는 항상 1(T-98) |
| **D-99** | 공기탄③ 간격 = `SkillWindup −0.3` → `EffectiveChargeInterval = ChargeInterval × WindupScale ÷ M` | 차지가 곧 선딜. 0.5 × 0.7 = 0.35. 공속(D-69)과 곱 |
| **D-100** | 공기탄③ 단계 증가 = 새 축 `SkillChargeGrowth −0.4` → 단계 피해 = 0단계 + (그 단계 − 0단계) × Growth | 정적 수치라 카탈로그(D-2). 배율이라 기본값이 바뀌어도 "증가분의 60%" 유지 |
| **D-101** | ★선행★ 공기탄 단계 피해 10 / 15 / 22 / 35 → **10 / 30 / 50 / 70** — C++ 생성자 | 사용자 확정. BP 가 `Stages` 를 덮지 않아 C++ 값이 곧 게임 값(T-102) |
| **D-102** | 물 점프 그룹 키 = `Abilities.RobotVacuum.JetJump`. 스위치는 ② 하나 `…JetJump.Guard` | ①③ 은 모디파이어만 |
| **D-103** | ★선행★ `GA_VacuumJetJump` 의 Cost GE = `GE_Cost_VacuumJetJump`(예전엔 비어 있어 물이 안 빠졌다) | 사용자 확정. `ApplyCost` 가 `Cost.Water` 를 이미 넣는다 — 칩③ 물 감소도 그대로 흐른다 |
| **D-104** | 물 점프① 강판정 = Inner × `SkillDamage`, 약판정 = Outer × `SkillDamage` × 새 축 `SkillOuterDamage`(+0.5). 물 점프가 이때부터 `SkillDamage` 를 읽는다 | 두 판정의 배율이 달라(×2 / ×3) 축 하나로는 안 된다. ②의 "피해 0" 이 한 줄로 양쪽을 덮는다 |
| **D-105** | 상승 = 새 축 `SkillLaunchHeight`(**실제 최고 높이** 배율) → 임펄스 × √배율. ① −0.3 · ③ −0.25 | 사용자 확정(둘 다 높이). 높이 = v² ÷ 2g(T-109) |
| **D-106** | 물 점프③ 물 = `SkillWaterCost` **Flat −10** | Percent −1/3 은 20.000001 → 물 20 에서 시전 실패(T-110) |
| **D-107** | 물 점프③ 공중 횟수 = 새 축 `SkillAirUseCount` **Flat +1** + GA `MaxAirJumps`(1) + 캐릭터 `AirJumpsUsed`(uint8 — 예전 `bAirJumpUsed`). `CanActivateAbility` 는 `GetUpgradeRuntimeFor(ActorInfo)` | 정적 정수라 카탈로그. CDO 에서 불릴 수 있다(T-114) |
| **D-108** | 물 점프② 무적 = `GE_JetJump_Guard`(SetByCaller 0.3초 · `State.Invulnerable`)를 `CommitAbility` 뒤 · 발사 앞에 → **기본 관문 맨 앞(모자보다 먼저)** 에서 피해 0 + return | 사용자 확정(피격 통째로 무효). 모자와 같은 자리 · 같은 이유. 태그는 범용 — 다른 스킬도 GE 만 걸면 된다 |
| **D-109** | 물 점프② 피해 0 = `SkillDamage −1.0`. 강 · 약 둘 다 0 이면 AoE 판정 자체를 건너뛴다(연출은 그대로) | 단점이 카드에 보이고, 0 피해 GE 를 적에게 걸지 않는다 |
| **D-110** | 청소기 칩의 새 축 6개(§2.2)는 전부 스킬 단위, `Count` 바로 앞, `DRIsSkillStat` 필수 | T-30 · T-12 |
| **D-111** | 돌진 그룹 키 = `Abilities.RobotVacuum.Dash`. 스위치는 ② 하나 `…Dash.PassAllies`. 동적 상수가 없어 `ChipTuning` · Config 검사가 없다 | D-2 — 수치가 전부 정적 |
| **D-112** | ★선행★ 돌진 자해 수정: `SelfDamageEffect` = `GE_VacuumDashSelfDamage`(예전엔 이속 GE — 자해 0) · 조건 = **지속 돌진 + Pawn 이 아닌 충돌(지형)** · 양 = GA `SustainedImpactSelfDamage`(50) → `Data.SelfDamage` | 사용자 확정(지형만). 기준은 `OnCapsuleHit` 의 Pawn 분류와 같다(T-128) |
| **D-113** | 돌진①② 는 **적** 충돌 피해만 `SkillDamage`(×1.8 / ×0.85) — 아군 충돌 칸당 5 는 그대로 | 사용자 확정("적에게 주는"). 8.5 소수 그대로 |
| **D-114** | 돌진① 자해 = 새 축 `SkillSelfDamage +1.0`(낮을수록 좋음) | 정적 수치 — 카드에 빨갛게 |
| **D-115** | 돌진② 아군 통과 = 복제 플래그 `bDashPassThroughAllies` → **모든 머신** `OnRep` 에서 내 캡슐 ↔ 아군 캡슐 `IgnoreActorWhenMoving` **양방향**(이 머신에서 넣은 목록만 되돌림). 서버 `OnCapsuleHit` 는 통과 중 아군 충돌 무시(안전망). 끄기 = `FinishDash` · `EndAbility` · `EndPlay` | 적 · 플레이어가 같은 Pawn 채널이라 채널로는 안 된다(T-126). 이동 무시는 복제되지 않는다(T-123) |
| **D-116** | 통과 대상 = 다른 플레이어 캐릭터 전부(`!IsNotFriend`). 적 · 사이트 · 지형은 부딪힌다 | 원문 "아군" |
| **D-117** | 돌진③ 충전 = `SkillWindup −0.3` → 타이머 = `ChargeInterval × WindupScale`(하한 0.05). 감쇠에는 안 곱한다 | D-99 와 같은 해석. 감쇠까지 줄이면 돌진이 짧아진다(T-129) |
| **D-118** | 돌진③ 이속 = 새 축 `SkillSpeedBonus −0.3` → 칸당 `ApplySkill(SkillSpeedBonus, SpeedBonusPerGauge 0.2)`. `GE_VacuumDashSpeedBuff` 크기 = SetByCaller `Data.MoveSpeed.Mult`(1 + 칸당), Additive · by Target 5 유지 | GE 에 1.2 가 박혀 있으면 칩이 못 바꾼다. Compound 면 ×5.7(T-122) |
| **D-119** | 새 축 중 `SkillSelfDamage` 만 `DRIsLowerBetter`. 나머지는 높을수록 좋음 · 초 아님 | T-130 |

> 기각(공속칩): 증가량·지속·중첩을 새 스탯 축으로 — 중첩 한도는 GE 에셋이 권위라 축으로 표현이 안 된다 / 공속을 쿨다운 축으로 — 세 로봇 모두 쿨다운이 없다.

> 기각(청소기 칩): 탑승① 공격력을 탑승자 GE 로 — 걸고 지울 곳이 늘어난다(D-78) / 탑 전체 공유 — 사용자 기각(D-92) / 오염 최대를 `PostAttributeChange` 에서 매번 100 — 바깥 갱신 뒤 옛 값이 한 번 더 방송된다 /
> 금지를 청소기 복제 배열로 — 만료 · 정리를 직접(D-90) / 흡착을 Compound + 코드 재적용 — Additive 한 줄로 끝난다 / 물 점프① 을 `ChipTuning` 절대값으로 — 카드에 안 뜬다 /
> 무적을 콜리전 끄기로 — DoT · 공유는 못 막는다 / 아군 통과를 콜리전 채널 · 새 오브젝트 채널로 — 적까지 통과 / 범위가 너무 크다 / 자해 ×2 를 GE 두 번 적용으로 — 모자 · 무적이 두 번 판정된다.

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
| `Public/DRGameplayTags.h` · `.cpp` | 칩 피처 태그 전부(§2.3) + GAS 태그 3개(`Buff.WaterPump.Pressure` · `Debuff.Vulnerable` · `Buff.AttackSpeed.Shared`) + SetByCaller `Data.MoveSpeed.Mult` / `Data.Duration` / `Data.HealthCost` + 청소기 칩 태그(`Abilities.RobotVacuum.Ride` · `…Ride/AirShot/JetJump/Dash` 스위치 · `Debuff.RobotVacuum.RideLoad` · `Debuff.Ride.Banned` · `Debuff.AirShot.Drag` · `State.Invulnerable` · `Data.SelfDamage`) |
| `AbilitySystem/DRAbilitySystemLibrary` | `GetUpgradeRuntimeForASC` / `HasUpgradeFeature` / `GetChipTuning` / `GetUpgradeKeyTagFromAbilityTags` / `ApplySkillUpgrade` / `ApplyTimedSpeedEffect` / `ApplyTimedTagEffect` / **`ApplyHealToActor`**(오버로드 2개 — `UClass*` 를 넘기면 C2668 모호 에러, `TSubclassOf` 로 감쌀 것) / **`GetHealableAmount`** / **`GetLifestealRatioForDamage`** / **`GetHitHealAmountForDamage`** / **`GetEffectStackCountWithTag`** / **`GetAttackSpeedMultiplier`** / **`GetAlivePlayersWithinRadius`** / 청소기: **`GetRideDamageMultiplier`** · **`FindRideGuard`** · **`TransferRideDamageToGuard`** · **`TryRideEmergencyEject`** · **`IsRideBannedFrom`** · **`GetAirShotDragDamageTakenMultiplier`** · **`TryApplyAirShotDrag`** · **`IsInvulnerable`** / `GetEffectStackCountWithTag` = OwningTags 쿼리(T-131) |
| `Abilities/DRGameplayAbility` | `GetUpgradedFloat` / `GetEffectiveWaterCostFor` / `ApplyCooldown` / `GetAttackSpeedMultiplier()`(BP Pure) |
| `Abilities/DRDamageGameplayAbility` | `GetUpgradedDamage()` = `ApplySkill(SkillDamage) × ` **`GetRuntimeDamageMultiplier()`**(가상 훅, 기본 1.0) |
| **`Abilities/DRWaterPump`** | 칩 해석 캐시 `Resolved` / 관통 트레이스 `TraceBeamBlocker` / 대상 수집 `CollectTargetsInBeam` + 칩 규칙 필터 `NarrowTargetsToChipRules` / `ApplyChipDebuffs` / `HasEnemyTarget` + `AdvanceRampStack` + `RampStackHandle` / `ClearRampStacks` / `GrantWaterToAlly` / `GetRuntimeDamageMultiplier` 오버라이드 |
| **`Abilities/DRMeleeAttack`** | `AttackRadius` / `BaseMontagePlayRate` / `GetUpgradedAttackRadius` / `GetWindupPlayRate` / `RestoreMontagePlayRate`(1P·3P 메시 모두, 둘 다 × 공속 배율) / `TryHealAllyWithOwnHealth` — 피해량 → D-33 으로 자르기 → D-26 으로 자르기 → **비용 먼저, 회복 나중**("깎였으면 회복도 갔다"가 항상 성립) |
| **`Actor/DRSeedProjectile`** | `FDRSeedBombResolved` 캐시 / 직격 판정 `bDirectHitOnEnemy` / `GrantWaterRefund` / 감속·가속 부여 / 아군 회복은 `ApplyHealToActor` 위임(D-34) |
| `Abilities/DRSeedCannon` | `SpentWaterCost` 주입(환급 근거) / `BaseMontagePlayRate`(1.0) · `GetAttackSpeedPlayRate()`(BP PlayRate 핀용) |
| `ExecCalc/ExecCalc_Damage` | 주는 피해 관문 — `DamageDealt`·광폭화·시너지, **음수 피해 0 클램프** / **(d) 탑승 배율** · ★`Damage.MountShared` 면 (a)~(d) 전부 건너뜀★(D-82) |
| **`DREnemyAttributeSet`** | 받는 피해 관문 — 엘리트 배율 + **`Debuff.Vulnerable` ×1.5** + **흡혈 지급**(★`SetHealth` 전★ — T-24) + **적중 회복 지급**(흡혈 바로 아래, 더미 분기 위 — T-40) + **흡착 배율**(취약 다음) + **`TryApplyAirShotDrag`**(적중 회복 다음, 더미 분기 위) |
| `DRChipCatalog.cpp` | `ValidateCatalog()` — 스킬 태그/공용 칩/`MaxHealth` Flat 전용/**`Percent ≥ −1.0`**/D-51 칩 태그 검사 · **`FindSkillConflict()`** · `SanitizeLoadout` 스킬 중복 제거 |
| `DRGameInstance.cpp` | `CanEquipChip`(→ `SkillConflict`) · `GetSkillConflictChip` · `CanEquipChipAtSlot`(충돌 칩 칸이면 교체 허용) · `SanitizeClassState` 3차 패스 · `MakeChipViewModel`(`ConflictingChipId/Name`) · `BuildPreviewRuntime`(충돌 칩 제외) |
| `DRProgressionConfig.h/.cpp` | `FDRCoinChipTuning` · `FDRJackpotOdds`(`Normalized`) · `FDRJackpotChipTuning` · `FDRAttackSpeedBuffChipTuning` / 검사: 잭팟 확률표(D-61) · 공속 ②③ GE(비었는지 · Target Tags · Stack Limit · ③ 만료 정책 — 그 칩이 카탈로그에 있을 때만) / `FDRRideChipTuning` · `FDRAirShotChipTuning`(생성자에서 Drag 0.1 · 2.0) · `FDRJetJumpChipTuning` / 검사: 탑승 ①③ · 흡착 · 무적 GE(태그 · 스택 · Op · Duration) |
| `UI/Widget/DRUpgradeChipWidget.h` | `IsDraggable()` = 해금 && 미장착 — `SkillConflict` 칩도 드래그된다(드롭 교체용) |
| `UI/DRUpgradeUILibrary.cpp` | 스탯 짧은 이름(5자 제한) — `SkillWindup` = "선딜" / `SkillFireInterval` = "발사 간격" / `SkillProjectileScale` = "탄 크기" / "차지 증가" · "외곽 피해" · "상승 높이" · "공중 횟수" · "자해 피해" · "이속 버프" |
| **`Abilities/DRVendingMachineBasicAttack`** | `GetCurrentFireInterval()` = 칩 간격 ÷ 공속 배율, `MinFireInterval` 바닥 / 잭팟: `GetJackpotOdds` · `RollJackpotTier` · `GetJackpotCapsuleBaseDamage` · `SpawnJackpotCapsule`(스케일 + `SkillDamage` + null 체크 + 사운드 — T-36) · `AdvanceJackpotPity` · `FireJackpotDoubleShot` / `[Jackpot]` 로그 |
| **`Abilities/DRProjectileSpell`** | `GetUpgradedProjectileScale()` + `SpawnProjectile()` 스폰 트랜스폼 Scale3D (적 GA 도 지나가지만 항상 1.0) |
| **`Abilities/DRVendingMachineAttackSpeedBuff`** | `ApplyAttackSpeedBuff()`(서버 — ② / ③ / 그 외 분기, ②③ GE 가 비면 경고 로그 + 기본 버프) · `ApplyBuffEffect()`(지속 고정) · `BuffDuration`(20) · `FindBuffDuration(ASC)` · `NotifySkillActivated()`(자기 + 공유 스택) |
| **`Abilities/DRVacuumAirShot`** | `EffectiveChargeInterval`(`StartCharging` 에서 고정, 하한 0.05초) · 강화탄 몽타주 PlayRate = 공속 배율 / `Stages` 10/30/50/70 · `SkillWindup` 을 간격에 · `SkillChargeGrowth` 를 단계 증가분에 · `bHitsAllies` 주입(강화탄 분기 위) |
| **`UI/WidgetController/OverlayWidgetController`** | `HandleAttackSpeedBuffTagChanged`(아이콘 숫자 + ③ 감소 구간 판정 `bAttackSpeedBuffDecaying` — ★방송 전에 갱신★) · `GetAttackSpeedBuffFillRatio()`(그래프) |
| **`Character/DRCharacter`** | `ApplyMountSharedDamageTo()`(공유 · 탑승② 이전 공용 GE 적용 — `Damage.MountShared` 필수) · `PropagateSharedDamage`(1칸) · 가상 훅 `OnUpgradeEffectsRefreshed()` · `RefreshUpgradeEffects` 오염 재고정(`ON_SCOPE_EXIT`) |
| **`Character/DRRobotVacuumCharacter`** | 탑승: `GetRiderCountAbove` · `GetStackBottom` · `RefreshRideLoad`(+ `RefreshRideLoadsAfterLinkChange` — Mount/Dismount 끝) · `EmergencyEjectRider` · `CanBeMountedBy` 금지 검사 / 물 점프: `AirJumpsUsed`(uint8, `Landed` 에서 0) / 돌진: `bDashPassThroughAllies`(복제) · `ApplyAllyPassThrough` · `OnCapsuleHit` 안전망 · `EndPlay` 해제 |
| **`DRPlayerAttributeSet`** | 탑승② 관문(맨 위 `FindRideGuard` → 받는 쪽 배율 뒤 이전 · `Debuff` · 공유 → return) · ③ `TryRideEmergencyEject`(★맨 끝★) · `SetMaxHealthCurrentValue` / `ReapplyCorruptMaxHealth`(D-87) |
| **`DRAttributeSet`**(기본 관문) | `PostGameplayEffectExecute` IncomingDamage: **무적(`IsInvulnerable`)** → 모자 → 도발 → `HandleIncomingDamage` |
| **`Abilities/DRVacuumJetJump`** | `MaxAirJumps` · `GetMaxAirJumpsFor(ActorInfo)` · 무적 GE(없으면 `[JetJump]` 경고) · 임펄스 × √`SkillLaunchHeight` · Inner × `SkillDamage` / Outer × `SkillDamage` × `SkillOuterDamage`, 둘 다 0 이면 판정 생략 |
| **`Abilities/DRVacuumDash`** | `SpeedBonusPerGauge`(0.2) · `SustainedImpactSelfDamage`(50) · 충전 × `SkillWindup` · 이속 SetByCaller(매 스펙) · 적 충돌만 × `SkillDamage` · 지속 + 지형 자해(`Data.SelfDamage`) · 통과 켜기(`ReleaseGauge`) · 끄기(`FinishDash` · `EndAbility`) |
| **`Actor/DRVacuumAirProjectile`** | `bHitsAllies`(복제) — 아군 분기는 탑승 연결 통과 **다음**, 강화탄 아군(넉백만) 분기 **앞** |
| **`Actor/DRProjectile`** | `ApplyImpactDamage(Target)` — 피해 적용부 추출(팀 판정 없음, 동작 불변) |
| `Character/DREnemy` | `IsBoss()` — protected `bIsBoss` 읽기 전용 |
| `Player/DRPlayerController` | `FindMountByLineTrace` — 탑승 금지 중이면 프롬프트 없음(`IsRideBannedFrom`) |

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
| `GA_VacuumJetJump` | **Cost Gameplay Effect Class = `GE_Cost_VacuumJetJump`** | 물이 안 빠진다(예전 상태 — D-103) |
| `GA_VacuumDash` | **Self Damage Effect = `GE_VacuumDashSelfDamage`**(이속 GE 가 들어가 있었다) | 자해 0 / 이속 GE 가 값 없이 걸려 이속 0(T-121) |
| `GA_VacuumAirShot` | `Stages` · `ChargeInterval` 을 BP 에서 덮지 않는다 | 덮는 순간 C++ 기본값(10/30/50/70)이 무시된다(T-102) |
| 플레이어 캐릭터 BP 4종 | `MountSharedDamageEffectClass` = `GE_Damage` | 탑승 공유 · ② 이전이 무동작(경고 로그) |

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

### 5.9 `ChipTuning.Ride`

| 필드 | 값 | 비고 |
|---|---|---|
| `PowerDamagePerRider` / `PowerMaxDamageBonus` / `PowerSpeedPenaltyPerRider` | 0.08 / 0.24 / 0.05 | ① |
| `PowerLoadEffect` | `GE_RideLoad` | ① ★비우면 감속 없이 공격력만★ |
| `GuardTransferRatio` | 0.2 | ② 이전분에만(0 = 피해가 사라짐, 1 = 안 줄임) |
| `EjectHealthRatio` / `EjectHealAmount` / `EjectBanDuration` | 0.3 / 100 / 20 | ③ |
| `EjectHealEffect` | `GE_Heal` | ③ ★`Claw` · `Coin` 의 `HealEffect` 와 별개 필드★ |
| `EjectBanEffect` | `GE_RideBan` | ③ |

### 5.10 `ChipTuning.AirShot` · `ChipTuning.JetJump` (돌진칩은 `ChipTuning` 없음 — D-111)

| 필드 | 값 | 비고 |
|---|---|---|
| `AirShot.Drag` { Effect / Magnitude / Duration } | `GE_AirShot_Drag` / 0.1 / 2.0 | 0.1 · 2.0 은 구조체 생성자 기본값(에셋엔 Effect 만 저장) |
| `AirShot.DragDamageTakenPerStack` / `DragMaxStacks` | 0.05 / 3 | ★`DragMaxStacks` = GE 의 Stack Limit — 검사가 비교★ |
| `JetJump.GuardEffect` / `GuardDuration` | `GE_JetJump_Guard` / 0.3 | |

칩 없을 때의 기본 수치는 전부 **GA 의 C++ 기본값**이다(BP 가 덮지 않는다 — 에셋 확인):
공기탄 `Stages` 10/30/50/70 · `ChargeInterval` 0.5 / 물 점프 `JumpImpulseZ` 900 · `Inner/OuterRadius` 200/400 · `Inner/OuterDamage` 20/5 · `WaterCost` 30 · `MaxAirJumps` 1 /
돌진 `ChargeInterval` 0.5 · `DecayInterval` 1 · `WaterPerGauge` 20 · `MaxGauge` 5 · `Enemy/PlayerDamagePerGauge` 10/5 · `SpeedBonusPerGauge` 0.2 · `SustainedImpactSelfDamage` 50.
카탈로그 `SkillDisplayNames` 에 `Abilities.RobotVacuum.{Ride → 탑승, AirShot → 기본 공격, JetJump → 물 점프, Dash → 돌진}`. 아이콘은 `Content/DaeRuneAssets/UI/Upgrade/Icon/Vacuum/`(이름은 §1.9~§1.12 표).

### 5.11 청소기 GE · GA 에셋 요구 사양

모두 `Content/Blueprints/AbilitySystem/Player/VacuumCleaner/` 아래(`Effects/` · `Abilities/Skill1` 공기탄 · `Skill2` 돌진 · `Skill3` 물 점프).

| 에셋 | 핵심 설정 |
|---|---|
| `GE_RideLoad` (`Effects/`) | Infinite / MoveSpeed **`Multiply (Compound)`** ← SetByCaller `Data.MoveSpeed.Mult` / Target Tags **`Debuff.RobotVacuum.RideLoad`**(★코드가 이 태그로 지운다★) / Stacking None |
| `GE_RideBan` (`Effects/`) | HasDuration · SetByCaller `Data.Duration` / 모디파이어 없음 / Target Tags `Debuff.Ride.Banned` / ★Stacking None★ |
| `GE_AirShot_Drag` (`Skill1/`) | HasDuration · SetByCaller `Data.Duration` / MoveSpeed ★**`Multiply (Additive)`**★ ← `Data.MoveSpeed.Mult` / Target Tags `Debuff.AirShot.Drag` / Aggregate by Target · Limit 3 · Refresh on Successful Application · Clear Entire Stack · Deny Overflow 끔 |
| `GE_JetJump_Guard` (`Skill3/`) | HasDuration · SetByCaller `Data.Duration` / 모디파이어 없음 / Target Tags `State.Invulnerable` / Aggregate by Target · Limit 1 · Refresh |
| `GE_VacuumDashSpeedBuff` (`Skill2/`) | Infinite / MoveSpeed **`Multiply (Additive)`** ← ★SetByCaller `Data.MoveSpeed.Mult`★(예전 1.2 고정) / Aggregate by Target · Limit 5 / Asset Tags `Buff.RobotVacuum.DashSpeed`(공기탄 반동 예외가 읽는다) |
| `GE_VacuumDashSelfDamage` (`Skill2/`) | Instant / IncomingDamage `Add (Base)` ← ★SetByCaller `Data.SelfDamage`★(예전 50 고정) |
| `GA_VacuumJetJump` · `GA_VacuumDash` | §4 BP 쪽 계약 참조. `GA_VacuumDash` 의 `CostGameplayEffectClass = GE_Cost_VacuumJetJump` 는 돌진 `WaterCost` 0 이라 무해 — 그대로(§9-63) |

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
| **T-74** | 탑승 공유 · 이전 피해를 `ApplyMountSharedDamageTo` 밖의 GE 로 걸며 `Damage.MountShared` 를 빠뜨림 | 받은 쪽이 다시 공유 → 탑 안 핑퐁 → **무한 반복** |
| **T-75** | 탑승② 분기를 로비 클램프 · 오염 분기 **뒤**에 | 넘길 양이 깎이거나 탑승자가 이미 맞았다 — 받는 쪽 배율 직후 · 클램프 전 |
| **T-76** | 보호 여부를 ② 분기에서 처음 구함 | 피격 큐가 함수 맨 위라 이미 나갔다 — 맨 위에서 먼저 |
| **T-77** | ExecCalc 의 `MountShared` 제외를 빼거나 일부 배율만 뺌 | 공유분이 넘겨주는 사람의 공격 배율로 커진다 — (a)~(d) ★전부★ |
| **T-78** | 탑승③ 판정을 `PropagateSharedDamage` **앞**에 | 하차가 먼저라 그 피격의 공유분이 덜 나간다 — 맨 끝 |
| **T-79** | 탑승③ 을 체력 비율로만 | 피해로 오염 진입(체력이 오염 최대로 다시 참)을 놓친다 — 진입 여부를 따로 넘긴다 |
| **T-80** | 탑승③ 을 Health 변경 델리게이트에 | 공유 전에 하차 · 체력 비용에도 발동 · 오염 재충전에 덮인다 |
| **T-81** | 금지 GE 를 Aggregate by Target | 두 번째 청소기의 금지가 첫 GE 에 합쳐져 그 청소기에 탈 수 있다 — Stacking None(검사가 잡는다) |
| **T-82 / T-131** | GE 존재 · 스택 조회에 `GetActiveEffectsWithAllTags`(= `MakeQuery_MatchAllEffectTags`) | ★GE 의 **에셋 태그만** 본다(UE 5.5 `FGameplayEffectQuery::Matches`)★ — 부여 태그만 단 GE(금지 · 흡착)를 못 찾는다. `MakeQuery_MatchAnyOwningTags` 를 쓴다(`GetEffectStackCountWithTag` 는 2026-10-08 이걸로 바꿨다) |
| **T-83** | 금지 판정에 컨텍스트 `SourceAbilityTags` | 복제되지 않아 클라 프롬프트가 틀린다 — 시전자 ASC 로 |
| **T-84** | 탑승 감속 GE 를 핸들로만 지움 / GE 에 `Debuff.RobotVacuum.RideLoad` 가 없음 | 리스폰 뒤 감속이 남는다(ASC = PlayerState) / ★태그로 못 지워 갱신마다 쌓인다★ — 검사가 잡는다 |
| **T-85** | 감속을 `RiderOnTop` 이 바뀐 청소기만 갱신 | 위층끼리 타고 내리면 맨 아래 인원이 그대로 — 나부터 맨 아래까지 + 오르내린 청소기 |
| **T-86** | 탑승 감속 GE 의 Op 를 Additive 로 | 돌진 버프(Additive)와 더해져 −5% 가 다르게 먹는다 — Compound |
| **T-87** | 오염 최대 보정(D-87)을 진입에만 | 해제 · 정화에서 다시 어긋난다 — 세 곳 + `RefreshUpgradeEffects` 재고정 |
| **T-88** | D-87 의 전제: MaxHealth 에 **곱 모디파이어가 없다** / 오염 중 `RefreshUpgradeEffects` 의 비율 유지 | 곱을 쓰기 시작하면 다시 볼 것. 칩 GE 를 떼는 순간 체력이 바뀌므로 시작 체력을 저장했다가 `ON_SCOPE_EXIT` 에서 되돌린다 |
| **T-89** | 이전 실패(청소기 사망 직후 · GE 미지정)에도 탑승자 처리를 건너뜀 | 피해가 **사라진다** — `TransferRideDamageToGuard` 가 false 면 그대로 받는다, `FindRideGuard` 는 죽은 청소기를 건너뛴다 |
| **T-90** | `Abilities.RobotVacuum.Ride` 를 GA `AbilityTags` 에 | 업그레이드 키가 흔들린다(T-51 과 같다) |
| **T-91** | `HandleIncomingDamage` 에 `TargetDRCharacter` 를 두 번 선언 | 그림자 변수(C4456) — 바깥 변수 하나로 |
| **T-92** | 공유 범위를 탑 전체로 넓힘 | 탑승② 수치가 전부 바뀐다(3층 C 피격 A 8 → 28). 넓힐 땐 한 칸씩 이어 전달하지 말고 피격자가 전원에게 직접 1번씩(배율이 겹치지 않게) |
| **T-93** | 80% 감소를 피격 자체의 공유에까지 | ② 청소기 자신의 몫까지 준다(2층 24 → 8) |
| **T-94** | 보호 분기에서 `Debuff(Props)` 누락 | 화상 DoT 같은 디버프 피해가 통째로 사라진다 |
| **T-95** | ★스택형 감속 GE 에 Multiply (Compound)★ | UE 5.5 Compound 바이어스 0 → 0.9 × 3스택 = **×2.7(가속)**. Additive(0.9/0.8/0.7). 검사가 잡는다. ※Limit 1 감속(씨앗 · 물대포)은 Compound 가 맞다(T-8) |
| **T-96** | 흡착 GE 에 Deny Overflow 켬 | 3스택에서 갱신이 거부돼 계속 맞혀도 2초 뒤 통째로 풀린다(T-4 와 같다) |
| **T-97** | 흡착 GE 의 Expiration 이 Clear Entire Stack 이 아님 | 1개씩 빠진다 |
| **T-98** | 흡착 받는 피해를 태그 카운트로 계산 | Aggregate by Target 이라 항상 1 → 항상 +5%(T-3) |
| **T-99** | 흡착 판정에서 `SourceAbilityTags` 의 `AirShot` 검사 누락 | 같은 청소기의 돌진 충돌 피해로도 쌓인다 |
| **T-100** | 보스를 `Buff.Elite` · 캐릭터 클래스로 판정 | 일반 엘리트가 빠지거나 두더지 보스가 걸린다 — `ADREnemy::IsBoss()` |
| **T-101** | `bHitsAllies` · `bEnhanced` 는 `SpawnProjectile`(= `FinishSpawning`) **뒤**에 들어간다 | 스폰 순간 총구와 겹쳐 있던 대상은 첫 오버랩에서 플래그를 모른다. 문제가 되면 지연 초기화 훅 |
| **T-102** | 공기탄 단계 피해를 BP 에서도 바꿈 | BP 가 한 번 덮으면 C++ 기본값은 무시 — 한 곳만(지금은 C++) |
| **T-103 / T-117** | 새 축을 `Count` 앞이 아닌 곳에 넣거나 `DRIsSkillStat` 에서 빠뜨림 | 기존 칩 효과가 밀리거나(T-30) 청소기 모든 스킬로 퍼진다(T-12). `ValidateCatalog` 가 잡는다 |
| **T-104** | 단계 증가 배율을 단계 피해 전체에 곱함 | 0단계까지 줄어든다(10 → 6) |
| **T-105** | `SkillWindup` 을 UI 타이머나 `GetChargeStage` 한쪽에만 | 게이지와 실제 단계가 어긋난다 — `EffectiveChargeInterval` 하나 |
| **T-106** | 아군 피해 분기를 `ADRProjectile` 에 | 적 투사체와 공유 클래스라 적 탄이 적을 맞힌다 — 범용 `ApplyImpactDamage` 만 부모에 |
| **T-107** | 공기탄①+강화탄을 "아군 = 넉백만" 분기로 | 강화탄이 아군에게 피해를 못 준다 — ① 분기를 그 앞에 |
| **T-108** | 물 점프 Cost GE 연결(D-103) 없이 ③ | 물이 원래 안 빠져 "30 → 20" 이 시전 기준만 바꾼다 |
| **T-109** | 높이 배율을 임펄스에 그대로 곱함 | 높이 ∝ 임펄스² 이라 ×0.7 이 높이 ×0.49 가 된다 — √배율 |
| **T-110** | 물 점프③ 물을 Percent −0.3333 으로 | 20.000001 → 물이 정확히 20 일 때 시전 실패 — Flat −10 |
| **T-111** | 무적 검사를 모자 **뒤**에 | 무적 중 맞은 피해에 모자가 닳는다 |
| **T-112** | 무적 검사를 `UDRPlayerAttributeSet::HandleIncomingDamage` 안에 | 디버프 · 공유 · 전투 진입이 먼저 지나가고, 적 · 사이트에는 같은 태그가 안 먹는다 — 기본 관문 맨 앞 |
| **T-113** | `GE_JetJump_Guard` Duration 을 Scalable Float 로 | `GuardDuration` 이 무시된다(0 이면 즉시 사라짐) — 검사가 잡는다 |
| **T-114** | `CanActivateAbility` 에서 `GetUpgradedInt`(현재 ActorInfo 판) | CDO 경로에서 칩이 안 읽혀 공중 1회로 막힌다 — `GetUpgradeRuntimeFor(ActorInfo)`(CheckCost 와 같은 이유) |
| **T-115** | `bAirJumpUsed` → `AirJumpsUsed` 를 일부만 바꿈 | `Landed` 리셋 누락 → 착지 후에도 공중 불가 / 증가 누락 → 무한 공중 점프 |
| **T-116** | `SkillOuterDamage` 를 강판정에도 곱함 | 강판정 60 |
| **T-118** | 물 점프 피해를 `GetUpgradedDamage()` 로 읽음 | 그 기준인 GA `Damage`(스케일러블)가 이 스킬에선 0 — `GetUpgradedFloat(SkillDamage, Inner/OuterDamage)` |
| **T-119** | 돌진 자해 GE 연결(D-112) 없이 ① | 자해가 원래 0 이라 ①이 단점 없는 칩이 된다 |
| **T-120** | 자해 GE 를 SetByCaller 로 바꾼 순서 | GE 만 먼저면 자해 0 + 에러, C++ 만이면 칩 무시(늘 50) — **C++ → GE** |
| **T-121** | `GE_VacuumDashSpeedBuff` 를 SetByCaller 로 바꾼 뒤 값 없이 거는 경로 | 크기 0 → (0 − 1) × 스택 = **이속 0 이하(멈춤)**. 예전 `SelfDamageEffect` 오배선이 그런 경로였다 — 거는 곳은 `ApplyBuffStacks` 하나 |
| **T-122** | 돌진 이속 GE 의 Op 를 Compound 로 | 1.14 × 5 = **×5.7**(T-95 와 같은 함정) |
| **T-123** | 아군 통과의 이동 무시를 서버에서만 | 소유 클라 CMC 는 여전히 막혀 보정이 튀고, 아군 클라에서는 아군이 막힌다 — 모든 머신 `OnRep` |
| **T-124** | 한쪽(청소기 → 아군)만 무시 | 아군이 움직일 때 막히거나 겹친 채 밀어낸다 — 양방향 |
| **T-125** | 통과 해제를 `FinishDash` 에만 | 사망 · 스턴 · 강제 취소 경로에서 **영구 통과** — `FinishDash` · `EndAbility` · `EndPlay` |
| **T-126** | 통과를 콜리전 채널로(캡슐 Pawn 응답) | 적도 같은 Pawn 채널이라 같이 통과 → 충돌 피해가 사라진다 |
| **T-127** | `SkillDamage` 를 아군 충돌 피해에도 | 사용자 확정 위반(5 → 9 / 4.25) |
| **T-128** | "지형"을 "ASC 없음"으로 판정 | 사이트(ASC 있음 · Pawn 아님)가 빠지고 `OnCapsuleHit` 기준과 달라진다 — Pawn 여부 |
| **T-129** | `SkillWindup` 을 돌진 감쇠에도 곱함 | 일반 돌진 시간 · 거리가 30% 준다 |
| **T-130** | `SkillSelfDamage` 를 `DRIsLowerBetter` 에서 빠뜨림 | 카드에 "자해 +100%" 가 초록(장점)으로 뜬다 |

---

## 7. 다음 칩을 추가하는 레시피

| 유형 | 예 | 해야 할 일 |
|---|---|---|
| **1. 순수 수치** (C++ 0줄) | "물대포 쿨다운 −20%" / "에어샷 투사체 크기 +30%"(`SkillProjectileScale` 은 `SpawnProjectile` 공용이라 이미 읽힌다) | 카탈로그에 `{Stat, TargetAbilityTag, Op, Value, bIsDrawback}` 한 줄. **조건**: 그 스탯을 이미 `Apply()`/`ApplySkill()` 로 읽고 있어야 한다 |
| **2. 새 수치 손잡이** (2~3줄) | "물대포 사거리 +30%" (선례: 청소기 칩의 `SkillLaunchHeight` · `SkillSpeedBonus`) | ① `EDRUpgradeStat` 에 축 추가(**`Count` 바로 앞**) ② `DRIsSkillStat()` 등록 ③ 그 숫자를 쓰는 곳을 감싼다 — 어빌리티면 `GetUpgradedFloat`, 액터면 `ApplySkillUpgrade`, BP 리터럴이면 유형 7 ④ (선택) 5자 짧은 이름 |
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
| 탑승 공유 · 이전 피해를 `ApplyMountSharedDamageTo` 밖에서 걸기 | `Damage.MountShared` 가 빠지면 탑 안에서 무한 반복(T-74) |
| 스택형 감속 · 가속 GE 에 `Multiply (Compound)` | 스택하면 오히려 빨라진다(T-95 · T-122) |
| 아군만 통과시키려고 콜리전 채널을 바꾸기 | 적과 플레이어가 같은 Pawn 채널이라 적까지 통과한다(T-126) |
| GE 를 태그로 찾는 데 `GetActiveEffectsWithAllTags` | 에셋 태그만 본다(T-131) |

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
| **36 / 48 / 56 / 65** | ~~청소기 칩 12종의 이름 · Tier · 아이콘 · `ChipId` 가칭~~ | **확정**(2026-10-08, 사용자) — 가칭 그대로 입력, 아이콘은 사용자가 따로 만들어 넣었다(§1.9~§1.12) |
| **37** | 탑승② 청소기에 피해가 **여러 번** 들어온다(공유분 + 보호받는 사람마다 이전분) — 숫자 여러 개, 광폭화면 칸도 그만큼 | 그대로(사용자 확인 2026-10-08). 한 번으로 합치려면 청소기 관문에 별도 분기가 필요하다 |
| **38** | 탑승① 탑승자 공격력이 HUD 에 안 보인다(상태 없는 계산) | 원하면 탑승자에게 표시 전용 GE(스택 = 인원) |
| **39** | 위층 청소기의 탑승①이 "쉬는 중"임을 알려 주지 않는다 | `Description` 문구로만 안내 |
| **40** | 탑승② 보호받는 사람도 스턴 · 둔화 같은 **피해 없는** 디버프는 받는다 | 그대로(사용자 확인 2026-10-08) |
| **41** | 탑승③ 탈출 연출은 기존 점프 하차(위 300 · 앞 200) | 더 세게면 튜닝 필드 추가 |
| **42** | ★선행 변경(D-82 · D-87)이 칩 없이도 체감을 바꾼다★ — 광폭화 · 시너지 탑승자의 공유분이 받은 만큼(45~55 → 20) / 오염 최대 항상 100(⑧ 900 → 100, 정화 뒤 2000 → 1200) | 사용자 확정. 플레이로 확인(§10.4) |
| **43** | (범위 밖) Stage2 부활(`Revive`)은 오염을 풀지 않는다 | D-87 뒤에는 칩과 무관하게 100 |
| **44** | 탑승③ 금지 중 "N초 후 탑승 가능" 표시가 없다(프롬프트가 안 뜰 뿐) | 원하면 `GetActiveEffectsTimeRemaining` 으로 위젯 |
| **45** | ~~탑승① 배율을 공유분에서 뺄지~~ | **해결** — 공유 · 이전은 받는 피해라 주는 피해 배율이 전부 빠진다(D-82) |
| **46** | 탑승② 청소기 두 대가 겹치면 이전분에 0.2 가 두 번(0.04) — B 피격 → A 24.8 | 그대로(사용자 확인 2026-10-08) |
| **47** | ~~광폭화 탑승자의 "최종 피해"를 칸으로 볼지 수치로 볼지~~ | **해결** — 피해 수치 |
| **49** | ★선행 변경(D-101)으로 칩 없는 공기탄이 강해졌다★ — 풀차지 35 → 70, 1단계 15 → 30 | 사용자 확정. 플레이 후 C++ `Stages` 만 조정 |
| **50** | 공기탄③ 은 초당 피해가 칩 없음과 비슷하거나 조금 낮다(+5% / −3% / −6%) — 이득은 "강화탄(넉백 · 로켓 점프)까지 30% 빨리" | 사용자 확정 수치. 약하면 카탈로그 `SkillChargeGrowth` 만 |
| **51** | 공기탄① 아군 피해로 아군이 오염 · 사망할 수 있다. 아군을 맞혔다는 별도 HUD 표시는 없다 | 원문 "동일한 피해". 로비 · 튜토리얼은 체력 1 클램프 |
| **52** | 공기탄① 아군 피해에도 시전자의 공격 배율(광폭화 · 시너지 · 탑승①)이 붙는다 | 그대로(사용자 확인 2026-10-08) |
| **53** | 공기탄② 비행 적에게는 감속이 안 걸린다(§9-1) — 받는 피해 증가는 걸린다 | §9-1 과 같은 판단 |
| **54** | 흡착 스택 표시(연출)가 없다 | 원하면 `GE_AirShot_Drag` 에 GameplayCue 하나 |
| **55** | 흡착 받는 피해는 그 적중 **직전** 스택으로 계산 — 첫 적중 +0% | 그대로(사용자 확인 2026-10-08). 바꾸려면 적 관문의 적용 순서만 |
| **57** | ★선행 변경(D-103)으로 칩 없는 물 점프가 물 30 을 실제로 쓴다★ | 사용자 확정. 플레이 후 C++ `WaterCost` 만 |
| **58** | 물 점프② 무적은 **피해 GE 를 지나는 것만** 막는다 — 적 GA 가 직접 거는 `LaunchCharacter` · 스턴 GE 는 못 막는다(그런 공격이 있는지는 조사하지 않았다) | 그대로(사용자 확인 2026-10-08). 필요하면 그 GA 에 `State.Invulnerable` 검사 한 줄 |
| **59** | 물 점프② 무적 연출이 없다 | 선택: `GE_JetJump_Guard` 에 GameplayCue |
| **60** | `SkillOuterDamage` 는 지금 물 점프만 읽는다 | 씨앗 폭탄 등에 쓰려면 그 스킬도 읽게 한다 |
| **61** | 물 점프 공중 횟수는 서버 전용 카운터 — 한도를 넘긴 Q 가 클라에서 잠깐 발동했다 취소될 수 있다(예전 두 번째 공중 Q 와 같은 현상) | 기존 구조 유지. 거슬리면 `AirJumpsUsed` 를 OwnerOnly 복제 |
| **62** | 물 점프 높이 수치는 중력 −980 · GravityScale 1 가정 | 중력을 바꿔도 비율(×0.70 · ×0.75)은 그대로 |
| **63** | `GA_VacuumDash` 가 `GE_Cost_VacuumJetJump` 를 Cost GE 로 들고 있다 | 돌진 `WaterCost` 0 이라 무해 — 건드리지 않는다 |
| **64** | 물 점프② 지상 Q → 0.3초 뒤 공중 Q 로 한 번 뜰 때 최대 0.6초 연속 무적 · 물 60 | 의도(사용 1회당 0.3초). 쿨다운이 무적보다 짧아지면 끊김 없이 이어질 수 있다 — 지금 그런 칩은 없다 |
| **66** | ★선행 변경(D-112)으로 칩 없는 5칸 돌진도 지형 충돌 자해 50(예전 0)★, 원래 코드의 적 · 아군 충돌 자해는 없어졌다 | 사용자 확정. 플레이 후 GA `SustainedImpactSelfDamage` 만 |
| **67** | 클렌저 사이트는 Pawn 이 아니라 **지형**으로 분류 — 5칸 돌진으로 박으면 자해, 사이트는 "적"이라 충돌 피해도 받는다(기존 동작) | 그대로(사용자 확인 2026-10-08). 빼려면 `HandleDashImpact` 에 `ADRCleanserSite` 예외 한 줄 |
| **68** | 돌진② 시작 순간 아군과 붙어 있으면 소유 클라가 플래그를 받기 전 한 번 막혔다가 보정될 수 있다 | 허용. 거슬리면 소유 클라에서 `ReleaseGauge` 때 로컬로 먼저 켠다 |
| **69** | 돌진 중 새로 생긴 아군 폰(부활 · 중도 입장)은 통과 목록에 없다 — 물리적으로는 막히지만 피해 · 종료는 없다(서버 안전망) | 허용 |
| **70** | 아군과 겹친 채 돌진이 끝나면 엔진이 밀어낸다(겹침 해소) | 허용 |
| **71** | 돌진② 적 피해 8.5/칸은 소수다 | 의도(D-113) |
| **72** | 돌진③ 은 같은 물 100 을 1.75초에 쓴다 — 충전 중 초당 물 40 → 57 | 사용자 확정 수치 |
| **73** | 돌진③ 지속 돌진 최고 속도 1100 → 935(−15%), 4칸 일반 돌진 거리 −10% | 사용자 확정 수치. 약하면 `SkillSpeedBonus` 만 |
| **74** | 돌진① 자해 100 은 받는 쪽 배율(`DamageTaken` · 오염)과 탑승 공유를 탄다 | 일반 피해와 같은 규칙(D-112) |

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

### 10.4 청소기 칩 12종

C++ 빌드 통과, 에셋 입력은 2026-10-08 에셋을 직접 읽어 확인했다(커밋 `89120e20` — 선행 4곳 · 새 GE 4개 · `ChipTuning.Ride/AirShot/JetJump` · 카탈로그 12칩 + `SkillDisplayNames` 4줄 · 아이콘 12).
준비: PIE Listen Server · 플레이어 3~4(탑승 3층용 청소기 2~3), 콘솔 `DRUnlockUpgrade` → `DRUnlockSlot 6`. 수치는 데미지 숫자 · `showdebug abilitysystem`(MoveSpeed · MaxHealth · 활성 GE 스택), 서버 로그 `[RideLoad]` · `[RideEject]`.

- [ ] **선행 변경(칩 없음)** 광폭화 탑승자 20 피격 → 청소기 20 / 오염 100 → 정화 400(+100 칩 오염 100 → 500, ⑧ 100 → 1200) / 공기탄 10 · 30 · 50 · 70 / 물 점프 물 −30(물 20 이면 물 0 + 체력 5) / 5칸 돌진 지형 충돌 자해 50, 적 · 아군 · 1~4칸 충돌은 없음 / 돌진 이속 660 … 1100 그대로 / 3층 A ← B ← C 에서 C 피격 → A 0(공유 1칸)
- [ ] **탑승①** 위 1 · 2 · 3명 → 탑승자 피해 ×1.08 · 1.16 · 1.24, 맨 아래 청소기 MoveSpeed 522.5 · 495 · 467.5 / 하차 · 사망 · 위층끼리 하차에 즉시 갱신 / 위층 ①은 효과 없음 / 리스폰 뒤 감속 잔류 없음 / 공유분은 ×1.24 가 아니다
- [ ] **탑승②** 2층 탑승자 20 피격 → 탑승자 체력 · 숫자 · 히트리액트 없음, 청소기 20 + 4 / 3층 A② ← B ← C: C · B · A 피격 → A 8 · 28 · 24 / 화상 탑승자 → 틱마다 청소기에 공유분 + 20% / 최대 체력 300 · 오염 100 / 모자 쓴 탑승자 → 모자만 소모 / ② 청소기 사망 → 하차 후 정상 피격
- [ ] **탑승③** 30% 이하로 맞으면 튕겨 내림 + 둘 다 +100 / 30% 이하로 탄 뒤 맞아도 / 아래층 공유로 30% 이하여도 / 오염 진입 → 탈출 + 정화 / 20초 그 청소기만 F 프롬프트 없음 · 서버 거부, 다른 청소기는 OK / ⑧ 비용 · 발톱 수혈로는 발동 안 함
- [ ] **공기탄** ① 15 · 45 · 75 · 105, 아군에게 같은 피해(강화탄 = 피해 + 넉백), 내 위 탑승자 · 내가 탄 청소기는 통과 / ② 8 · 24 · 40 · 56, 적 이속 ×0.9 · 0.8 · 0.7(★1.8 · 2.7 이면 T-95★) · 4번째부터 유지 · 마지막 적중 2초 뒤 해제 · 다른 플레이어 피해도 +5 · 10 · 15% · 보스 없음 · 일반 엘리트 걸림 · 돌진으로는 안 쌓임 / ③ 0.35 · 0.70 · 1.05초에 단계(게이지와 일치), 10 · 22 · 34 · 46
- [ ] **물 점프** ① 2m 안 40 · 2~4m 15, 높이 약 ×0.7 / ② 피해 숫자 · 히트리액트 없음(물 분사 연출은 있음), Q 직후 0.3초 안 피격 무효 · 모자 유지 · 지상 Q → 0.3초 뒤 공중 Q 면 0.6초 / ③ 물 −20(물 정확히 20 에서도), 지상 Q → 공중 Q × 2(4번째 거부), 착지하면 다시 2회, 높이 약 ×0.75
- [ ] **돌진** ① 적 18/칸 · 아군 5/칸 · 5칸 지형 자해 100 / ② 아군을 피해 · 정지 없이 통과(호스트 · 아군 · 내 화면 모두), 아군이 다가와도 안 막힘, 끝나면 다시 막힘, 사망 · 브레이크 · 자연 감쇠 뒤 잔류 없음, 적 8.5/칸 / ③ 0.35초마다 1칸, 칸당 이속 627 … 935, 감쇠는 1초에 1칸 그대로
- [ ] **스킬당 1칩** 그룹마다 1개, 네 그룹 동시 가능 / 미리보기 라벨 "탑승 · 기본 공격 · 물 점프 · 돌진" / 카드 문구(Detail Override)
- [ ] **멀티** 클라 청소기 탑승① 감속이 호스트 화면에서도 / 클라 탑승자 ③ 금지 → 클라 프롬프트 사라짐 · 20초 뒤 다시 / 클라 공기탄② 감속이 호스트 화면에서도 / 클라 돌진② 통과 시 보정 튐 없음(시작 순간 붙어 있으면 1회 — §9-68) / 클라 물 점프③ 공중 3번째 Q 서버 거부
- [ ] **검사** `DA_ProgressionConfig` 편집 → 오류 0건(청소기 GE 검사 포함)

전부 통과하면 §10.4 를 지우고 머리말 상태를 "완료"로 바꾼다.

---

## 11. 남은 작업

- **필수 에디터 작업은 없다**(2026-10-08 에셋 확인 — 청소기 칩 12종 포함 전부).
  선택 사항만 남았다 — §9-35(공유 버프 상태 아이콘), §9-25(칩 카드 사유 문구), §9-26(`Detail Override`), 청소기 연출 · 표시(§9-38 · 44 · 54 · 59).
- 그다음은 §10 플레이 검증과, 결과에 따른 밸런스 조정(§9-14 · 21 · 27 · 49 · 50 · 73)이다.
