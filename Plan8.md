# Plan8 — 4번째 플레이어 캐릭터 "벨보이(BellBoy)" 제작 계획

> **작성일** 2026-09-29 · **상태** 계획(미착수 — 이 문서를 쓰면서 코드·에셋은 한 줄도 바꾸지 않았다) · **작업 브랜치(예정)** `feat/bellboy`
> **선행 문서** Plan5.md(GA·칩 규약, "선딜 = 몽타주 재생 속도" D-20), Plan7.md(계획서 양식)
> **범위** 벨보이 캐릭터 1종 + 스킬 3종(기본 공격 · 시선 집중 · 모자 던지기) — C++ · BP · ABP · GA · GE · GameplayCue · 몽타주 · AI 연결 · UI · 검증
> **비범위** 벨보이 전용 칩 · 1인칭(FP) 에셋 적용(→ STEP 11, 에셋이 오면) · 튜토리얼 · 메인 메뉴/대기실 연출 포즈
>
> 📌 **읽는 법** — **STEP 0 → STEP 11 을 위에서부터 순서대로** 하면 된다. 각 STEP 은 `[C++]`(코드) / `[에디터]`(언리얼 에디터)로 표시했고
> 끝에 **✅ 확인** 이 있다. 확인이 통과하지 않으면 다음 STEP 으로 넘어가지 말 것 — 뒤 단계가 앞 단계에 기대고 있다.
>
> ⚠️ **코드 주석 표기 규칙** (Plan5 와 같음) — 공통 `Plan8(벨보이)` / 기본 공격 `Plan8(가방)` / 스킬1 `Plan8(시선집중)` / 스킬2 `Plan8(모자)`.
> 이 문서의 **STEP·절 번호는 코드 주석에 쓰지 않는다.** 대신 결정 **D-n**(부록 B), 함정 **T-n**(부록 C)으로 가리킨다.
> D/T 번호는 영구 ID 다 — 문서가 바뀌어도 유지하고, 빠진 번호는 결번으로 두며 재사용하지 않는다.
>
> 🔎 이 문서의 수치(애니 길이·본 계층·기존 GA 값·BT 구성 등)는 2026-09-29 에 에셋 파일을 직접 파싱해 확인한 값이다(§2).

---

## 0. 요약

| 항목 | 결론 |
|---|---|
| 캐릭터 | `BP_BellBoy` ← `ADRCharacter` 직속 **(새 C++ 캐릭터 클래스 없음 — D-1)**, `EPlayerCharacterClass::BellBoy` 추가 |
| 기본 공격 (LMB) | 여행 가방 휘두르기, 피해 **30**, 약한 넉백(기존 넉백 코드). 좌·우 몽타주 교대. 첫 타 선딜 0.4 / 후딜 0.4, 연속 **0.2 / 0.1**. 판정 창 0.1초. 기본 공격 뒤 다른 스킬은 타격 **0.4초** 뒤부터 |
| 스킬1 시선 집중 (E) | 물 **60**. 1초 뒤 30m 안의 적을 0.5초 동안 도발 → 걸린 적은 **5초** 동안 벨보이만 노리고 **이속 +10%**, **주는 피해 −20%**. 도발 모션 5초(걸으면서, 다른 스킬 불가) |
| 스킬2 모자 던지기 (RMB) | 물 **20**. 빙그르르 도는 모자. 적 → 피해 30 후 소멸 / 아군 → 머리에 모자(**어떤 피해든 1회 무효**, 최대 1개) / 벽·바닥 → 바닥에 **10초** 픽업(적·아군 누구든 주움, **던진 본인은 2회**) |
| 새 C++ | 스킬 클래스 5개 — GA 3(`UDRBellBoyBasicAttack` · `UDRBellBoyTaunt` · `UDRBellBoyHatThrow`) + 액터 2(`ADRBellBoyHatProjectile` · `ADRBellBoyHatPickup`) |
| 기존 C++ 수정 | enum 1줄 · 태그 15개 · `GameBalanceConfig`(구조체 1) · 라이브러리 헬퍼 12개 · 피해 관문 2곳(`DRAttributeSet.cpp` · `DRCleanserSiteAttributeSet.cpp`) · 블랙보드 키 상수 1개 |
| 새 에셋 | 몽타주 6 · AO · 블렌드스페이스 · ABP · 캐릭터 BP · GA 3 · GE 6 · GameplayCue 4(+선택 1) · 투사체/픽업 BP · 노티파이 스테이트 1 · 스킬 아이콘 위젯 |
| 기존 에셋 수정 | 벨보이 스켈레톤(소켓·슬롯) · `DA_PlayerCharacterClassInfo` · `DA_GameBalanceConfig` · `DA_AbilityInfo` · `DA_StatusEffectInfo` · 대기실/업그레이드 위젯 3개 · (선택) 두더지 서비스 `BTS_FindNearestPlayer_Mole` 분기 1개 |
| 1인칭 | **FP 에셋 없이 진행**(사용자 확정 — D-3). 판정·연출은 3인칭 몽타주로 한다. 내 화면에는 팔이 안 보인다 → STEP 11 |

### 0.1 타이밍 그림

**기본 공격** — 몽타주 0.8초, 타격 노티파이 0.4초(에셋 실측)
```
첫 타    |====== 선딜 0.4s (재생 ×1.0) ======|● 타격|====== 후딜 0.4s (×1.0) ======|   → 0.8초
연속     |=== 선딜 0.2s (×2.0) ===|● 타격|후딜 0.1s (×4.0)|                                → 0.3초
                                   └ 판정 창 0.1s — 0 / 0.025 / 0.05 / 0.075 초에 가방 주변을 4번 훑는다
다른 스킬 |← 스윙 시작 ─── 선딜 ───● ──── 이후 0.4s 동안 E·RMB 차단 ────┤ 여기부터 사용 가능
```
- **"연속"** = 앞 스윙이 끝나고 **0.25초 안에** 다음 스윙이 시작된 경우(LMB 를 누르고 있으면 항상 연속). 연속 중에도 좌·우가 번갈아 나온다.
- 연속 공격은 초당 약 3.3회 × 30 = **초당 약 100 피해**(정원로봇 발톱은 약 68) — 부록 E-4.

**시선 집중**
```
t=0.0  E 입력 · 물 60 · AM_BB_Taunt 재생(상체 슬롯 — 다리는 계속 걷는다)
t=1.0  ● 노티파이 → 판정 시작: 30m 구를 0.1초마다 훑어 "새로 들어온" 적을 도발 (0.0/0.1/…/0.5 → 6회)
t=1.5  판정 끝
t=5.0  도발 모션 끝 → 기본 공격·모자 다시 가능
t≈6.0~6.5  적마다 "걸린 순간 + 5초"에 도발이 풀린다 (사용자 확정 — D-12)
```

**모자 던지기** — 부딪힌 대상에 따른 분기
```
던짐 → 비행 (Yaw 720°/s 회전, 약한 중력 0.35)
  ├─ 던진 본인 · 시체 · 트리거 볼륨 · 다른 투사체/픽업 ……… 무시하고 계속 난다
  ├─ 적 (캐릭터 · 홀로그램 두더지) ………………………………………… 피해 30 → 소멸
  ├─ 아군 (플레이어)
  │     ├─ 모자 없음 ……………………………………………………… 머리에 모자(1회) → 소멸
  │     └─ 이미 모자 ……………………………………………………… 부딪혀 그 자리 바닥에 픽업 (사용자 확정 — D-22)
  └─ 벽·바닥 (폰을 막는 표면, 클렌저 사이트 포함) ……………… 그 자리 바닥에 픽업
픽업 (10초) — 닿은 캐릭터 누구든: 모자가 없으면 장착(던진 본인이면 2회), 이미 있으면 픽업이 그대로 남는다
```

---

## 1. 확정 사양

### 1.1 기본 공격 — 여행 가방 휘두르기 (LMB)

| 항목 | 값 | 비고 |
|---|---|---|
| 피해 | 30 | `Damage.Physical` · `GE_Damage` |
| 넉백 | 약하게 — `KnockbackForceMagnitude` **400** 권장 | 기존 파이프라인 그대로(D-9). 45° 위로 살짝 튄다 |
| 몽타주 | `BB_BasicAttack_Right` → `BB_BasicAttack_Left` → … 교대 | 첫 타는 항상 오른쪽 |
| 첫 타 | 선딜 0.4 / 후딜 0.4 | 몽타주 원속(×1.0) |
| 연속 | 선딜 0.2 / 후딜 0.1 | 타격 전 ×2.0, 타격 후 ×4.0 |
| 연속 판정 | 앞 스윙 종료 후 0.25초 안에 시작 | `ComboResetTime` (D-6) |
| 다른 스킬로 전환 | 타격 후 0.4초 동안 E·RMB 차단 | `GE_BellBoy_BasicAttackRecovery` (D-7) |
| 타격 시점 | 몽타주에 단 `AN_MontageEvent` 노티파이 시각 | 코드가 몽타주에서 직접 읽는다(D-5) |
| 판정 | 타격부터 0.1초, 0.025초 간격 4회, 가방 소켓 중심 구(반경 **90** 권장) | 한 스윙에 한 대상 1회(D-8) |
| 물 소모 / 쿨다운 | 없음 / 없음 | 다른 로봇 기본 공격과 같음 |

### 1.2 스킬1 — 시선 집중 (E)

| 항목 | 값 | 비고 |
|---|---|---|
| 물 소모 | 60 | |
| 모션 | `BB_Taunt` 5.0초, **상체 슬롯**(이동 가능) | 모션 동안 기본 공격·모자 던지기 불가 + 피격 경직 없음(D-18) |
| 선딜 | 1.0초 → 노티파이에서 판정 시작 | |
| 판정 | 벨보이 중심 **30m**(3000uu), **0.5초** 동안 0.1초 간격 6회 | 벽 너머도 도발(D-19 — 옵션 있음) |
| 도발 지속 | **걸린 순간부터 5초** | 사용자 확정(D-12) |
| 도발 효과 | 벨보이를 우선 공격 대상으로 · **이속 ×1.1** · **주는 피해 ×0.8**(맞는 대상 무관 — 아군·벨보이·클렌저 사이트 모두) | 사용자 확정(D-11) |
| 해제 | 5초 경과 / 벨보이 사망 | |
| 대상 | 살아 있고, AI 블랙보드에 타깃 키가 있는 적 | 부품 도둑(도주형) 제외(D-17) |
| 쿨다운 | 없음 | 요구 없음 — 물이 유일한 제한(D-29) |

### 1.3 스킬2 — 모자 던지기 (RMB)

| 항목 | 값 | 비고 |
|---|---|---|
| 물 소모 | 20 | |
| 모션 | `BB_ThrowCap` 0.8초, 상체 슬롯 | 던지는 순간부터 머리의 모자 본을 숨긴다(`ANS_BellBoy_HideCap`) |
| 비행 | 조준점(크로스헤어) 방향, 속도 1400, 중력 0.35, Yaw 720°/s 회전 | BP 에서 조정 |
| 적 적중 | 피해 30 → 소멸 | 넉백·디버프 없음 |
| 아군 적중 | 모자 없음 → 머리에 모자(1회) / 이미 있음 → 부딪혀 그 자리 바닥에 픽업 | 최대 1개(D-22) |
| 벽·바닥 | 그 자리 바닥에 픽업 | 폰을 막는 표면만(D-24) |
| 픽업 | 10초 유지, 적·아군 누구든 닿으면 장착. **던진 벨보이 본인이면 2회** | 모자 쓴 대상은 못 줍고 픽업이 남는다 |
| 모자 효과 | **어떤 피해든**(1이든 200이든, 도트 틱 포함) 1회 무효 → 모자 사라짐 | 적이 써도 똑같다 |
| 쿨다운 | 없음 | |

### 1.4 공통

| 항목 | 값 |
|---|---|
| 키 | LMB 기본 공격 / **E** 시선 집중 / **RMB** 모자 던지기 — GA 의 `StartupInputTag` 한 칸이라 바꾸기 쉽다(D-28) |
| 스탯 | 정원로봇과 같은 `GE_DRPrimaryAttributes` / `GE_VitalAttributes`(D-30) |
| 입력 | 누르고 있으면 반복 — 다른 로봇과 같음(`UDRAbilitySystemComponent::AbilityInputTagHeld`) |
| 칩 | 공용 칩만 적용. 코드는 `SkillDamage` · `SkillAreaRadius` · `SkillWindup` · `SkillWaterCost` 칩이 들어와도 그대로 동작하게 만든다 |

---

## 2. 조사 결과 — 이 계획의 근거

### 2.1 벨보이 에셋 (`Content/DaeRuneAssets/Characters/BellBoy/`)

| 에셋 | 길이 | 쓰는 곳 |
|---|---|---|
| `Bellboy_v0_1_0` (+ `_Skeleton`, `_PhysicsAsset`) | — | 3P 메시 |
| `BB_BasicAttack_Right` / `BB_BasicAttack_Left` | 0.8s | 기본 공격 몽타주 |
| `BB_Taunt` | 5.0s | 시선 집중 몽타주 |
| `BB_ThrowCap` | 0.8s | 모자 던지기 몽타주 |
| `BB_Idle_1` / `_2` / `_3` | 1.5 / 1.5 / 3.17s | ABP Idle(랜덤) |
| `BB_Walk_Forward` / `_Backward` / `_Left` / `_Right` | 1.0s | `BS_BB_Walk` |
| `BB_Jump` | 0.93s | ABP Jump — 시작/루프/착지 분리 없음 |
| `BB_Stun` | 1.0s | ABP Stun |
| `BB_Idle_HitReact` | 0.43s | 피격 몽타주 |
| `BB_Death` | 2.0s | 사망 몽타주 |
| `BB_AimOffset_Up` / `_Forward` / `_Down` | 1프레임 | `AO_BB` |
| `Bellboy_Face_Default` / `BB_VC_Display_Face` | — | 얼굴 텍스처 / 얼굴 머티리얼(부모 = glTF 기본 MI — 다른 로봇 얼굴과 같은 구조) |

**없는 것** — FP 메시·FP 애니 · 몽타주 · 블렌드스페이스 · AO 에셋 · 스켈레톤 소켓 · **노티파이(애니 18개 모두 0개)** · 모자만 떼어 낸 스태틱 메시 · 표정(깜빡임·피격·사망) · Sitting(열차 좌석) · 부품 운반 자세 · MainMenu 포즈 → 부록 D.

### 2.2 본 계층 (스켈레톤 실측)
```
Armature_ProxyTrueRootJoint (루트)
├─ Body_001                  ← 상체 시작
│  ├─ Body_002               ← 머리 쪽
│  │  └─ Cap ─┬─ CapButton   ← 벨보이가 쓰고 있는 모자
│  │          └─ MasterController
│  ├─ Arm_001_L … Hand_*_L
│  └─ Arm_001_R … Hand_*_R
├─ Bag                       ★루트 직속★ — 상체 브랜치(Body_001) 밖에 있다 (T-1)
├─ Leg_F_L / Leg_B_L / Leg_F_R / Leg_B_R
└─ Arm_IK_* / Leg_*_IK_* / Arm_Pole_*
```

### 2.3 기존 로봇에서 가져오는 구조

| 벨보이 | 본보기 | 근거(실측) |
|---|---|---|
| 기본 공격 GA | `GA_ClawSwipe`(BP) / `UDRMeleeAttack` | 3P `PlayMontageAndWait`(UpperBody) + `AN_MontageEvent` → `WaitGameplayEvent` → `GetLiveObjectsWithinRadius` **1회** → `MakeDamageEffectParamsFromClassDefaults` → `ApplyDamageEffect`. CDO: 피해 70 · 반경 60 · 넉백 0 |
| 선딜 조절 | Plan5 D-20 | 재생 속도를 낮추고 타격 노티파이에서 되돌린다 → 벨보이는 "타격 앞·뒤 속도를 따로"로 확장 |
| 넉백 | `UDRDamageGameplayAbility::MakeDamageEffectParamsFromClassDefaults` | 대상이 있으면 45° 위 방향 × `KnockbackForceMagnitude` → 적 어트리뷰트셋이 `LaunchCharacter` |
| 투사체 GA | `UDRProjectileSpell::SpawnProjectile` / `CalculateTargetLocation` | 자판기·청소기 |
| 투사체 | `ADRProjectile` | 오버랩 판정, 서버가 `Destroy` |
| 바닥에 떨구기 | `ADREnemy::DropPart` | 아래 방향 트레이스 + 오프셋 |
| 몽타주 슬롯 | 공격·스킬 = `UpperBody`, 피격·사망 = `DefaultSlot` | `AM_Gardener_*`, `AM_HitReact` |
| 블렌드스페이스 | `BS_Walk` — 축 `MoveRight`·`MoveForward` −1~1, 샘플 4개 | 정원로봇·자판기 동일 |
| AO | `AO_Gardener` — 축 `AimPitch` −60~60, 샘플 3개 | |
| ABP | `ABP_Gardener` — 스테이트머신 → 캐시 → UpperBody 슬롯 → Layered Blend → AO → DefaultSlot | |
| 캐릭터 BP | `BP_VendingMachine` — 커스텀 변수 없음, `PlayerCharacterClass` 명시 | 정원로봇 BP 는 물대포 전용 변수(`InWaterLoop`)가 있다 |
| 스킬 부여 | `DA_PlayerCharacterClassInfo` **만** | `ADRCharacter::PossessedBy → GivePlayerStartupAbilities`. 캐릭터 BP 의 `Startup Abilities` 는 아무도 안 읽는다(`AddCharacterAbilities()` 호출 0건) |
| 물 비용 | GA `WaterCost` + `GE_Cost_*`(Instant, Execution `ExecCalc_WaterCost`) | `GE_Cost_SeedCannon` |
| 스킬 아이콘 | `WBP_SkillIcon_GardenRobot` = `WBP_SkillSlot` 3개(각각 `AbilityTag` · `InputTag`) + `DA_AbilityInfo` | |
| 상태 아이콘 | `Buff.*`/`Debuff.*` 태그를 주는 GE → `DA_StatusEffectInfo` 한 줄 | `OverlayWidgetController` |
| 적 이속 변경 | `UDRAttributeSet::PostAttributeChange` 가 `MaxWalkSpeed` 갱신 | 씨앗 폭탄 감속과 같은 경로 |

### 2.4 AI 가 타깃을 고르는 방식 — 도발이 끼어들 자리
- 타깃은 **BP 서비스**가 정한다. 키: `FirstAttacker`(Object) · `HasFirstAttacker`(Bool) · `TargetToFollow`(Object) · `DistanceToTarget`(Float).
- 서비스 ↔ BT (에셋 실측):

| 서비스 | 쓰는 BT |
|---|---|
| `BTS_FindNearestPlayer_Dog` | Armadillo · Armadillo_Phase1 · Dog · Phase3_Common · Phase3_Elite |
| `BTS_CheckValidTarget` | Armadillo · Armadillo_Phase1 · Dog · Phase3_Common · Phase3_Elite |
| `BTS_FindNearestPlayer_DragonFly` | DragonFly · DragonFly_Part |
| `BTS_CheckValidTarget_DragonFly` | DragonFly · DragonFly_Part |
| `BTS_FindNearestPlayer_Mole` | EliteMole |
| (`BTS_FindNearestPlayer` 기본형) | 어느 BT 도 안 쓴다 |

- BT 구조(사용자 확인): **`TargetToFollow` 가 있으면 그 플레이어를 쫓아가 공격, 없으면 `TargetCleanserSite` 를 쫓아가 공격.**
- 서비스 동작 (2026-09-29 컴파일된 BP 바이트코드를 직접 풀어 확인):
  - `BTS_FindNearestPlayer_Dog` — 매 틱 먼저 `HasFirstAttacker` 를 본다. **true 면 아무것도 안 한다.** false 면 인지된 플레이어 중 15m(출혈이면 30m) 안의 가장 가까운 대상을 `TargetToFollow` 에 쓰고, 아무도 없고 전투가 3초 넘게 없으면 `TargetToFollow` 를 비운다(→ 사이트로 복귀).
  - `BTS_CheckValidTarget` — `TargetToFollow` 가 ① 죽었거나 ② 1.5초 넘게 도달 불가거나 ③ 시야 거리(`NormalSightRange`)보다 멀면서 **전투가 3초 넘게 없으면** `TargetToFollow`·`FirstAttacker` 를 비우고 `HasFirstAttacker = false`.
  - 잠자리 서비스 2개도 `HasFirstAttacker` 를 먼저 따른다(사용자 확인).
  - `BTS_FindNearestPlayer_Mole`(움직이지 않는 두더지 보스) 은 `HasFirstAttacker` 를 보지 않고 매번 가장 가까운 플레이어를 `TargetToFollow` 에 쓴다(에셋에 해당 키 참조 없음) — 기본 공격 대상이 이 키다.
- → **도발 = C++ 이 키 3개(`TargetToFollow`·`FirstAttacker`·`HasFirstAttacker`)를 벨보이로 쓰고, 도발 동안 짧은 주기로 다시 쓰며 전투 시간을 갱신한다**(D-16). 서비스는 `HasFirstAttacker` 때문에 손을 떼고, `CheckValidTarget` 의 ③은 전투 시간 갱신으로 막힌다. BT 서비스 수정은 두더지 서비스 1곳(선택)뿐.
- 부품 도둑 BT(`BB_EnemyBlackboard_PartThief`)에는 타깃 키가 없다(D-17).

### 2.5 엔진(UE 5.5) 확인
`UBlockAbilityTagsGameplayEffectComponent`("Block Abilities with Tags") · `UAbilityTask_Repeat`(**활성화 즉시 1회 실행** 후 간격마다 반복) · `UAbilitySystemComponent::CurrentMontageSetPlayRate`(서버면 시뮬레이티드 프록시로 복제) · GameplayCue 액터의 Owner = 대상 액터(`GameplayCueManager.cpp`) · `EGameplayModOp::MultiplyCompound` · `RemoveActiveEffectsWithGrantedTags` — 전부 있다.

---

## 3. 전체 구조

```
[입력 LMB / E / RMB] → UDRAbilitySystemComponent (InputTag → GA)
                        │
       ┌────────────────┼───────────────────────────┬───────────────────────────────┐
       ▼                                            ▼                               ▼
GA_BellBoy_BasicAttack                     GA_BellBoy_Taunt                 GA_BellBoy_HatThrow
(UDRBellBoyBasicAttack)                    (UDRBellBoyTaunt)                (UDRBellBoyHatThrow)
 AM_BB_BasicAttack_R/L 교대                  AM_BB_Taunt (5s)                  AM_BB_ThrowCap
 ● 타격 → 0.1s 동안 4회 판정                  ● 1.0s → 0.5s 동안 6회 스캔          ● 릴리스 → 투사체 스폰
 → GE_Damage (30 + 넉백)                     → 적에게 GE_BellBoy_Taunted          → BP_BellBoyHatProjectile
 → GE_BellBoy_BasicAttackRecovery               Debuff.Taunted · 이속 ×1.1            ├ 적:  GE_Damage 30
   (E·RMB 차단)                                  피해 배율 0.8 (SetByCaller)          ├ 아군: GE_BellBoy_Hat_Single
                                             → ApplyTauntAggro (즉시 + 0.2s 주기)     └ 벽/바닥/모자 쓴 아군:
                                                                                         BP_BellBoyHatPickup (10s)
                                                                                         └ 주우면 Single / 던진 본인 Double

피해 관문  UDRAttributeSet::PostGameplayEffectExecute   (플레이어 · 적 공용)
  ① TryConsumeBellBoyHat           → 막았으면 피해 0, 여기서 끝                     (D-21)
  ② GetTauntDamageMultiplier(피해를 준 쪽) → 도발된 적이면 ×0.8                    (D-14)
  ③ HandleIncomingDamage (기존 그대로)
  + UDRCleanserSiteAttributeSet::HandleIncomingDamage 에도 ② 한 줄                 (D-14)

AI   C++ 이 TargetToFollow · FirstAttacker · HasFirstAttacker = 벨보이 + 전투 시간 갱신 (도발 동안 0.2s 주기) (D-16)
     기존 서비스는 HasFirstAttacker 때문에 손을 뗀다 — BT 수정 없음 (두더지 서비스만 선택적 분기 1개)
연출 GC_BellBoy_Hat_Single/Double (머리 위 모자) · GC_BellBoy_HatBlock (막는 순간) · GC_BellBoy_Taunted ("!")
```

---

## 4. 작업 순서 한눈에

- [ ] **STEP 0** 준비
- [ ] **STEP 1** [C++] 기반 — enum · 태그 · 설정 구조체 · 블랙보드 키 · 라이브러리 헬퍼 · 피해 관문
- [ ] **STEP 2** [C++] 스킬 클래스 5개 → **빌드**
- [ ] **STEP 3** [에디터] 애니메이션 에셋 — 소켓 · 슬롯 · 시퀀스 설정 · AO · 블렌드스페이스 · 노티파이 스테이트 · 몽타주 6개
- [ ] **STEP 4** [에디터] `ABP_BellBoy`
- [ ] **STEP 5** [에디터] `BP_BellBoy` + 클래스 등록 + 대기실 위젯 → **1차 테스트(선택·이동·애니)**
- [ ] **STEP 6** [에디터] 기본 공격 GE·GA → 테스트
- [ ] **STEP 7** [에디터] 시선 집중 GE·GC·GA + (선택) 두더지 서비스 분기 → 테스트
- [ ] **STEP 8** [에디터] 모자 GE·GC·설정·픽업·투사체·GA → 테스트
- [ ] **STEP 9** [에디터] UI — 스킬 아이콘 · 상태 아이콘 · (선택) 캐릭터 설명
- [ ] **STEP 10** 검증(싱글 · 멀티 · 회귀)
- [ ] **STEP 11** (나중에) FP 에셋이 오면

---

## STEP 0. 준비

1. 지금 브랜치 `feat/upgradeskill` 에 **미커밋 칩 작업**(잭팟칩 C++ · `DA_ChipCatalog` · Plan5.md)이 있다 → 먼저 커밋하거나 정리한다. 벨보이 작업이 칩 작업과 섞이지 않게 한다.
2. `main`(또는 칩 작업이 머지된 브랜치)에서 `feat/bellboy` 를 딴다.
3. **언리얼 에디터를 닫는다.** STEP 1~2 는 헤더·`UENUM`·`USTRUCT` 를 바꾸므로 Live Coding 으로는 반영되지 않는다(T-19).
4. 벨보이 에셋 폴더는 **옮기지 않는다**(리디렉터가 생긴다). 새 몽타주·AO·블렌드스페이스는 `BellBoy/` 폴더 안에 그대로 만든다.

✅ 확인: `git status` 가 깨끗하고, `feat/bellboy` 위에 있다.

---

## STEP 1. [C++] 기반 공사

> - 새 선언은 전부 UTF-8 헤더에만 넣는다. **`DRAttributeSet.h` 는 CP949/UTF-8 혼합 인코딩(2줄)** 이라 이번 계획은 그 헤더를 건드리지 않게 짰다(T-9). 고쳐야 할 일이 생기면 바이트 단위로 편집한다.
> - 새 파일은 UTF-8(BOM) + CRLF 로 만든다(주변 파일과 같게).
> - 원하면 STEP 1 끝에 한 번 빌드해도 된다(이 단계만으로는 게임 동작이 바뀌지 않는다 — 아래 ✅ 참고).

### 1-1. 캐릭터 enum — `Public/AbilitySystem/Data/CharacterClassInfo.h`

```cpp
UENUM(BlueprintType)
enum class EPlayerCharacterClass : uint8
{
	Gardener,
	VendingMachine,
	RobotVacuum,

	// ★Plan8(벨보이)★ 반드시 Count 바로 앞에 — 앞에 끼우면 저장된 값(세이브·칩 소속·에셋)이 밀린다 (Plan8 D-2)
	BellBoy,

	Count UMETA(Hidden) // 클래스 개수 계산용 - 항상 마지막에 유지
};
```
- C++ 에서 `Count` 로 도는 곳은 **자동 대응**한다 — 세이브 초기화(`DRGameInstance.cpp` 의 `ClassUpgrades`/`Cosmetics` FindOrAdd), 대기실 클래스 순환(`DRPlayerController::ServerRequestChangeClass`), 슬롯 해금 총비용(`DRProgressionConfig::GetTotalSlotUnlockCost`). 따로 고칠 곳 없음.
- BP 의 `Select`/`Switch` 노드는 **자동 대응되지 않는다** → STEP 5-4(T-16).

### 1-2. 게임플레이 태그 — `Public/DRGameplayTags.h` · `Private/DRGameplayTags.cpp`

| 멤버 | 태그 | 누가 쓰나 |
|---|---|---|
| `Abilities_BellBoy_BasicAttack` | `Abilities.BellBoy.BasicAttack` | GA AbilityTags · 스킬 아이콘 · 칩 키 |
| `Abilities_BellBoy_Taunt` | `Abilities.BellBoy.Taunt` | 〃 |
| `Abilities_BellBoy_HatThrow` | `Abilities.BellBoy.HatThrow` | 〃 |
| `Event_Montage_BellBoy_BasicAttack` | `Event.Montage.BellBoy.BasicAttack` | 몽타주 노티파이 → 가방 타격 |
| `Event_Montage_BellBoy_Taunt` | `Event.Montage.BellBoy.Taunt` | → 도발 판정 시작 |
| `Event_Montage_BellBoy_HatThrow` | `Event.Montage.BellBoy.HatThrow` | → 모자 릴리스 |
| `Debuff_Taunted` | `Debuff.Taunted` | 도발된 적(GE 가 부여, C++ 이 읽음) |
| `Data_Taunt_DamageMult` | `Data.Taunt.DamageMult` | SetByCaller — 도발된 적이 주는 피해 배율 |
| `Buff_BellBoy_Hat` | `Buff.BellBoy.Hat` | ★부모★ — "모자를 쓰고 있나" 조회 전용 |
| `Buff_BellBoy_Hat_Single` | `Buff.BellBoy.Hat.Single` | 1회 모자 (HUD 상태 아이콘 키) |
| `Buff_BellBoy_Hat_Double` | `Buff.BellBoy.Hat.Double` | 2회 모자 |
| `GameplayCue_BellBoy_HatBlock` | `GameplayCue.BellBoy.HatBlock` | 모자가 막는 순간(C++ 이 실행) |
| `GameplayCue_BellBoy_Hat_Single` | `GameplayCue.BellBoy.Hat.Single` | 머리 위 모자(GE 가 켠다) |
| `GameplayCue_BellBoy_Hat_Double` | `GameplayCue.BellBoy.Hat.Double` | 〃 |
| `GameplayCue_BellBoy_Taunted` | `GameplayCue.BellBoy.Taunted` | 도발된 적 머리 위 "!" |

- 이벤트 태그를 네이티브로 두는 이유: 기존 `Event.Montage.ClawSwipe` 는 `Config/DefaultGameplayTags.ini` 에 있지만, 벨보이 GA 는 C++ 이 주도하므로 에디터에서 태그를 따로 만들 일이 없게 한다.

**`.h`** — 파일 끝(두더지 보스 블록 뒤, `private:` 앞)에:
```cpp
	// ========== 벨보이 (Plan8) ==========
	FGameplayTag Abilities_BellBoy_BasicAttack;
	FGameplayTag Abilities_BellBoy_Taunt;
	FGameplayTag Abilities_BellBoy_HatThrow;

	// 몽타주 노티파이(AN_MontageEvent)가 보내는 이벤트
	FGameplayTag Event_Montage_BellBoy_BasicAttack;	// 가방이 닿는 순간
	FGameplayTag Event_Montage_BellBoy_Taunt;		// 도발 판정 시작
	FGameplayTag Event_Montage_BellBoy_HatThrow;	// 모자가 손을 떠나는 순간

	// ★GAS 태그★ — GE_BellBoy_Taunted 가 적에게 부여. C++ 이 읽는다 (Plan8 D-13)
	FGameplayTag Debuff_Taunted;
	// SetByCaller — 도발된 적이 "주는" 피해 배율(0.8). 도발 GE 스펙이 들고 다닌다 (Plan8 D-13)
	FGameplayTag Data_Taunt_DamageMult;

	// ★GAS 태그★ — 모자 보호막 (Plan8 D-20). 부모 태그는 "모자를 쓰고 있나" 조회 전용
	FGameplayTag Buff_BellBoy_Hat;
	FGameplayTag Buff_BellBoy_Hat_Single;
	FGameplayTag Buff_BellBoy_Hat_Double;

	FGameplayTag GameplayCue_BellBoy_HatBlock;
	FGameplayTag GameplayCue_BellBoy_Hat_Single;
	FGameplayTag GameplayCue_BellBoy_Hat_Double;
	FGameplayTag GameplayCue_BellBoy_Taunted;
```

**`.cpp`** — `InitializeNativeGameplayTags()` 끝(`State_MoleBoss_Burrowed` 다음)에 같은 양식으로 15개:
```cpp
	/*
	 * 벨보이 (Plan8)
	 */

	GameplayTags.Abilities_BellBoy_BasicAttack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.BellBoy.BasicAttack"),
		FString("BellBoy suitcase swing (LMB)")
	);

	// … 나머지 14개도 같은 모양 — FName = 위 표의 태그, FString = 한 줄 설명.
	//   Buff_BellBoy_Hat(부모)도 ★명시적으로 등록한다★ — 멤버 변수로 부모 태그를 들고 있어야 조회에 쓸 수 있다.
```

### 1-3. 모자 설정 — `Public/AbilitySystem/Data/GameBalanceConfig.h`

```cpp
class UGameplayEffect;   // ← 파일 위쪽 전방 선언

/**
 * ★Plan8(모자)★ 벨보이 모자 보호막 설정.
 * 피해 관문(1회 무효화) · 모자 투사체(아군 장착) · 픽업(주운 대상 장착)이 전부 여기를 본다 (Plan8 D-27).
 * ★서버에서만 읽는다★ — GameMode 의 DA 라 클라이언트에는 없다. 세 곳 모두 서버 경로라 문제없다.
 */
USTRUCT(BlueprintType)
struct FBellBoyConfig
{
	GENERATED_BODY()

	// 1회 모자 — Infinite / Target Tags = Buff.BellBoy.Hat.Single / Cue = GameplayCue.BellBoy.Hat.Single
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BellBoy|Hat")
	TSubclassOf<UGameplayEffect> HatSingleEffect;

	// 2회 모자(던진 벨보이 본인이 주웠을 때). 한 번 막으면 Single 로 바뀐다 (Plan8 D-20)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "BellBoy|Hat")
	TSubclassOf<UGameplayEffect> HatDoubleEffect;
};
```
`UGameBalanceConfig` 클래스 안(Phase3 설정 위)에:
```cpp
	// ========== 캐릭터 설정 ==========

	// 벨보이 (Plan8)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character")
	FBellBoyConfig BellBoy;
```
- `DA_GameBalanceConfig` 는 **Lobby · Stage · Stage2** 게임모드 BP 에 연결돼 있다(튜토리얼 제외 — 벨보이는 튜토리얼 대상이 아니다, T-26).

### 1-4. 블랙보드 키 상수 — `Public/Character/DREnemy.h` 의 `namespace DRBlackboardKeys`

```cpp
	// 타겟팅
	inline const FName FirstAttacker = TEXT("FirstAttacker");
	inline const FName HasFirstAttacker = TEXT("HasFirstAttacker");
	inline const FName TargetToFollow = TEXT("TargetToFollow");
	// ★Plan8(시선집중)★ 타깃 서비스들이 쓰는 거리 키 (Plan8 D-16)
	inline const FName DistanceToTarget = TEXT("DistanceToTarget");
```

### 1-5. 라이브러리 헬퍼 — `Public/AbilitySystem/DRAbilitySystemLibrary.h` · `Private/.../DRAbilitySystemLibrary.cpp`

**헤더** — 전방 선언 `class AAIController; class UAnimMontage;` 추가, 클래스 끝에:
```cpp
	// ========== ★Plan8(모자)★ 모자 보호막 ==========

	/** 대상이 모자를 쓰고 있나 (Single/Double 무관 — 부모 태그 Buff.BellBoy.Hat) */
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|BellBoy")
	static bool HasBellBoyHat(AActor* Target);

	/**
	 * 대상에게 모자를 씌운다. ★서버 전용★
	 * 조건: 살아 있는 캐릭터(ADRCharacterBase) + ASC + ★아직 모자가 없다★ (최대 1개 — Plan8 D-22)
	 * @param Charges 1 = 일반 / 2 이상 = Double (던진 벨보이 본인이 주웠을 때 — Plan8 D-23)
	 * @return 씌웠으면 true. false 면 호출부가 모자를 남긴다(투사체는 바닥에 떨어지고, 픽업은 그대로).
	 */
	static bool TryEquipBellBoyHat(AActor* Target, UAbilitySystemComponent* SourceASC, int32 Charges);

	/**
	 * 들어온 피해 1회를 모자로 막는다. ★서버 전용★ — UDRAttributeSet::PostGameplayEffectExecute 에서만 부른다.
	 * Double → (Single 을 먼저 걸고) Double 제거 / Single → 제거. 막았으면 HatBlock 큐를 실행한다.
	 * @return 막았으면 true — 호출부는 IncomingDamage 를 0 으로 만들고 피해 처리를 건너뛴다 (Plan8 D-21)
	 */
	static bool TryConsumeBellBoyHat(UAbilitySystemComponent* TargetASC);

	/**
	 * 모자 비주얼(GameplayCue)이 붙을 자리. 메시에 HatSocket 이 있으면 그 소켓, 없으면 캡슐 꼭대기.
	 * 적 BP·스켈레톤을 하나하나 고치지 않아도 되게 폴백을 둔다 (Plan8 D-26).
	 */
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|BellBoy")
	static USceneComponent* GetHatAttachPoint(AActor* Target, FName& OutSocketName, FVector& OutRelativeOffset);

	// ========== ★Plan8(시선집중)★ 도발 ==========

	/** Origin 반경 안의 살아 있는 적(ADREnemy). 캡슐·히트박스 중복 제거, 로그 없음 (Plan8 T-7) */
	static void GetAliveEnemiesWithinRadius(const UObject* WorldContextObject, const FVector& Origin,
		float Radius, TArray<AActor*>& OutEnemies);

	/** 이 적을 도발할 수 있나 — 살아 있고 AI 블랙보드에 FirstAttacker 또는 TargetToFollow 키가 있다 (Plan8 D-17) */
	static bool CanBeTaunted(AActor* Enemy);

	/**
	 * 이 적에게 걸린 도발 GE 의 정보. 도발 중이 아니면 false.
	 * ★도발자 = 컨텍스트 OriginalInstigatorASC 의 아바타★ — GetInstigator() 는 플레이어면 PlayerState 다 (Plan8 T-6).
	 * 도발자가 죽었어도 GE 가 남아 있으면 true (살아 있는지는 호출부가 본다).
	 */
	static bool GetTauntInfo(AActor* Enemy, AActor*& OutTaunter, float& OutDamageMultiplier,
		FActiveGameplayEffectHandle* OutHandle = nullptr);

	/** BP·BT 용 — 이 적을 도발 중인 ★살아 있는★ 벨보이. 아니면 nullptr */
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|BellBoy")
	static AActor* GetTauntTarget(AActor* Enemy);

	/** 피해를 주는 쪽(Source)이 도발된 적이면 그 배율(0.8), 아니면 1.0 — 피해 관문 전용 (Plan8 D-14) */
	static float GetTauntDamageMultiplier(AActor* SourceAvatar);

	/**
	 * ★도발 어그로 1회 적용★ (Plan8 D-16) — 서버 전용
	 *  · 도발 중      → 블랙보드 TargetToFollow / FirstAttacker = 도발자, HasFirstAttacker = true, DistanceToTarget,
	 *                   포커스, 전투 시간(UpdateCombatTime) 을 갱신하고 true.
	 *                   HasFirstAttacker 가 true 라 기존 FindNearestPlayer 서비스는 타깃을 건드리지 않고,
	 *                   전투 시간이 갱신돼 CheckValidTarget 의 "멀고 3초간 전투 없음" 조건에도 안 걸린다.
	 *  · 도발자 사망  → 도발 GE 를 지우고 false (타깃 정리는 CheckValidTarget 이 한다 — Plan8 D-12)
	 *  · 도발 아님    → 아무것도 안 하고 false
	 * 블랙보드에 없는 키는 건드리지 않는다 — BB 마다 키 구성이 다르다.
	 */
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|BellBoy")
	static bool ApplyTauntAggro(AAIController* OwnerController);

	/**
	 * ★도발 유지 타이머★ (Plan8 D-16) — 서버 전용. 도발을 건 직후 한 번 부른다.
	 * Interval(0.2초)마다 ApplyTauntAggro 를 다시 부르고, false 가 나오면(도발 GE 만료·도발자 사망) 스스로 멈춘다.
	 * 적이 도발 GE 를 들고 있는 동안만 돌기 때문에 어빌리티가 끝나도(도발 5초가 모션보다 길다) 계속 유지된다.
	 */
	static void StartTauntAggroRefresh(AAIController* OwnerController, float Interval = 0.2f);

	// ========== ★Plan8(가방)★ 몽타주 ==========

	/**
	 * 몽타주에서 EventTag 를 보내는 노티파이(AN_MontageEvent 처럼 "EventTag" 변수를 가진 노티파이)의 시각(초). 없으면 -1.
	 * ★노티파이를 옮겨도 코드 값을 따로 고칠 필요가 없게 한다 (Plan8 D-5)★
	 */
	static float FindMontageEventTime(const UAnimMontage* Montage, const FGameplayTag& EventTag);
```

**구현** — include 추가: `AIController.h`, `BehaviorTree/BlackboardComponent.h`, `Components/CapsuleComponent.h`, `Animation/AnimMontage.h`, `Character/DRCharacterBase.h`, `Character/DREnemy.h`, `AI/DRAIController.h`
(`GameBalanceConfig.h` · `GameFramework/Character.h` · `CombatInterface.h` · `Engine/OverlapResult.h` · `DRLogChannels.h` 는 이미 들어 있다)

```cpp
// ========================= ★Plan8(모자)★ 모자 보호막 =========================

bool UDRAbilitySystemLibrary::HasBellBoyHat(AActor* Target)
{
	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target);
	return ASC && ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Buff_BellBoy_Hat);   // 부모 태그 — 자식 둘 다 잡힌다
}

bool UDRAbilitySystemLibrary::TryEquipBellBoyHat(AActor* Target, UAbilitySystemComponent* SourceASC, int32 Charges)
{
	ADRCharacterBase* Character = Cast<ADRCharacterBase>(Target);
	if (!IsValid(Character) || !Character->HasAuthority()) return false;
	if (ICombatInterface::Execute_IsDead(Character)) return false;

	UAbilitySystemComponent* TargetASC = Character->GetAbilitySystemComponent();
	if (!TargetASC) return false;

	// 최대 1개 (D-22) — 이미 쓰고 있으면 호출부가 모자를 남긴다
	if (TargetASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Buff_BellBoy_Hat)) return false;

	const UGameBalanceConfig* Config = GetGameBalanceConfig(Character);
	const TSubclassOf<UGameplayEffect> EffectClass = !Config ? nullptr
		: (Charges >= 2 ? Config->BellBoy.HatDoubleEffect : Config->BellBoy.HatSingleEffect);
	if (!EffectClass)
	{
		UE_LOG(LogDR, Warning, TEXT("[BellBoy] DA_GameBalanceConfig.BellBoy 의 모자 GE 가 비어 있다 — 모자 무동작"));
		return false;
	}

	// 소스 ASC 가 없으면(던진 벨보이가 나감 등) 대상 자신을 소스로 — ApplyTimedTagEffect 와 같은 처리
	UAbilitySystemComponent* Source = SourceASC ? SourceASC : TargetASC;
	FGameplayEffectContextHandle Context = Source->MakeEffectContext();
	Context.AddSourceObject(Source->GetAvatarActor());

	const FGameplayEffectSpecHandle Spec = Source->MakeOutgoingSpec(EffectClass, 1.f, Context);
	return Spec.IsValid() && TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get()).IsValid();
}

bool UDRAbilitySystemLibrary::TryConsumeBellBoyHat(UAbilitySystemComponent* TargetASC)
{
	if (!TargetASC || !TargetASC->IsOwnerActorAuthoritative()) return false;

	const FDRGameplayTags& Tags = FDRGameplayTags::Get();
	// ★빠른 탈출★ — 모든 피해가 이 함수를 지난다. 모자가 없으면 태그 조회 한 번으로 끝난다.
	if (!TargetASC->HasMatchingGameplayTag(Tags.Buff_BellBoy_Hat)) return false;

	FGameplayCueParameters Cue;
	Cue.Location = TargetASC->GetAvatarActor() ? TargetASC->GetAvatarActor()->GetActorLocation() : FVector::ZeroVector;

	// ① Double → Single ★Single 을 먼저 걸고 Double 을 지운다★ (D-20)
	//    순서를 거꾸로 하면 한 순간 "모자 없음"이 되어 그 사이 들어온 피해를 못 막는다.
	const TArray<FActiveGameplayEffectHandle> Doubles =
		TargetASC->GetActiveEffectsWithAllTags(FGameplayTagContainer(Tags.Buff_BellBoy_Hat_Double));
	if (Doubles.Num() > 0)
	{
		const UGameBalanceConfig* Config = GetGameBalanceConfig(TargetASC->GetAvatarActor());
		if (Config && Config->BellBoy.HatSingleEffect)
		{
			const FGameplayEffectSpecHandle Single = TargetASC->MakeOutgoingSpec(
				Config->BellBoy.HatSingleEffect, 1.f, TargetASC->MakeEffectContext());
			if (Single.IsValid())
			{
				TargetASC->ApplyGameplayEffectSpecToSelf(*Single.Data.Get());
			}
		}
		TargetASC->RemoveActiveGameplayEffect(Doubles[0]);
		TargetASC->ExecuteGameplayCue(Tags.GameplayCue_BellBoy_HatBlock, Cue);
		return true;
	}

	// ② Single → 제거
	const TArray<FActiveGameplayEffectHandle> Singles =
		TargetASC->GetActiveEffectsWithAllTags(FGameplayTagContainer(Tags.Buff_BellBoy_Hat_Single));
	if (Singles.Num() > 0)
	{
		TargetASC->RemoveActiveGameplayEffect(Singles[0]);
		TargetASC->ExecuteGameplayCue(Tags.GameplayCue_BellBoy_HatBlock, Cue);
		return true;
	}
	return false;
}

USceneComponent* UDRAbilitySystemLibrary::GetHatAttachPoint(AActor* Target, FName& OutSocketName, FVector& OutRelativeOffset)
{
	OutSocketName = NAME_None;
	OutRelativeOffset = FVector::ZeroVector;

	ACharacter* Character = Cast<ACharacter>(Target);
	if (!Character) return Target ? Target->GetRootComponent() : nullptr;

	static const FName HatSocketName(TEXT("HatSocket"));
	if (USkeletalMeshComponent* Mesh = Character->GetMesh())
	{
		if (Mesh->DoesSocketExist(HatSocketName))
		{
			OutSocketName = HatSocketName;
			return Mesh;
		}
	}

	// 폴백: 캡슐 꼭대기 + 10uu (머리를 따라 흔들리지는 않지만 모든 적에게 동작한다)
	UCapsuleComponent* Capsule = Character->GetCapsuleComponent();
	OutRelativeOffset = FVector(0.f, 0.f, Capsule->GetScaledCapsuleHalfHeight() + 10.f);
	return Capsule;
}

// ========================= ★Plan8(시선집중)★ 도발 =========================

void UDRAbilitySystemLibrary::GetAliveEnemiesWithinRadius(const UObject* WorldContextObject, const FVector& Origin,
	float Radius, TArray<AActor*>& OutEnemies)
{
	OutEnemies.Reset();
	const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);
	if (!World) return;

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(Overlaps, Origin, FQuat::Identity,
		FCollisionObjectQueryParams(ECC_Pawn), FCollisionShape::MakeSphere(Radius));

	for (const FOverlapResult& Overlap : Overlaps)
	{
		ADREnemy* Enemy = Cast<ADREnemy>(Overlap.GetActor());
		if (!IsValid(Enemy) || ICombatInterface::Execute_IsDead(Enemy)) continue;
		OutEnemies.AddUnique(Enemy);   // 커스텀 히트박스도 Pawn 타입이라 한 적이 여러 번 잡힌다
	}
}

bool UDRAbilitySystemLibrary::CanBeTaunted(AActor* Enemy)
{
	const APawn* Pawn = Cast<APawn>(Enemy);
	const AAIController* AIC = Pawn ? Cast<AAIController>(Pawn->GetController()) : nullptr;
	const UBlackboardComponent* BB = AIC ? AIC->GetBlackboardComponent() : nullptr;
	if (!BB) return false;

	return BB->GetKeyID(DRBlackboardKeys::FirstAttacker) != FBlackboard::InvalidKey
		|| BB->GetKeyID(DRBlackboardKeys::TargetToFollow) != FBlackboard::InvalidKey;
}

bool UDRAbilitySystemLibrary::GetTauntInfo(AActor* Enemy, AActor*& OutTaunter, float& OutDamageMultiplier,
	FActiveGameplayEffectHandle* OutHandle)
{
	OutTaunter = nullptr;
	OutDamageMultiplier = 1.f;

	const UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy);
	const FDRGameplayTags& Tags = FDRGameplayTags::Get();
	if (!ASC || !ASC->HasMatchingGameplayTag(Tags.Debuff_Taunted)) return false;   // 빠른 탈출

	const TArray<FActiveGameplayEffectHandle> Handles =
		ASC->GetActiveEffectsWithAllTags(FGameplayTagContainer(Tags.Debuff_Taunted));
	const FActiveGameplayEffect* Active = Handles.Num() > 0 ? ASC->GetActiveGameplayEffect(Handles[0]) : nullptr;
	if (!Active) return false;   // 새 도발이 이전 것을 지우므로 항상 1개다 (D-15)

	if (const UAbilitySystemComponent* SourceASC = Active->Spec.GetContext().GetOriginalInstigatorAbilitySystemComponent())
	{
		OutTaunter = SourceASC->GetAvatarActor();                  // ★Instigator(PlayerState)가 아니다 (T-6)★
	}
	OutDamageMultiplier = Active->Spec.GetSetByCallerMagnitude(Tags.Data_Taunt_DamageMult, false, 1.f);
	if (OutHandle) *OutHandle = Handles[0];
	return true;
}

AActor* UDRAbilitySystemLibrary::GetTauntTarget(AActor* Enemy)
{
	AActor* Taunter = nullptr;
	float Multiplier = 1.f;
	if (!GetTauntInfo(Enemy, Taunter, Multiplier) || !IsValid(Taunter)) return nullptr;
	if (Taunter->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Taunter)) return nullptr;
	return Taunter;
}

float UDRAbilitySystemLibrary::GetTauntDamageMultiplier(AActor* SourceAvatar)
{
	AActor* Taunter = nullptr;
	float Multiplier = 1.f;
	if (!IsValid(SourceAvatar) || !GetTauntInfo(SourceAvatar, Taunter, Multiplier)) return 1.f;
	return FMath::Max(0.f, Multiplier);   // 음수로 피해가 회복으로 뒤집히지 않게
}

bool UDRAbilitySystemLibrary::ApplyTauntAggro(AAIController* OwnerController)
{
	if (!OwnerController || !OwnerController->HasAuthority()) return false;

	APawn* Pawn = OwnerController->GetPawn();
	UBlackboardComponent* BB = OwnerController->GetBlackboardComponent();
	auto HasKey = [BB](const FName& Key) { return BB && BB->GetKeyID(Key) != FBlackboard::InvalidKey; };

	AActor* Taunter = nullptr;
	float Multiplier = 1.f;
	FActiveGameplayEffectHandle Handle;
	const bool bTaunted = Pawn && GetTauntInfo(Pawn, Taunter, Multiplier, &Handle);

	const bool bTaunterAlive = bTaunted && IsValid(Taunter)
		&& !(Taunter->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Taunter));

	if (!bTaunterAlive)
	{
		// 도발자가 죽었거나 사라졌다 → 도발 GE 를 지운다 (D-12).
		// 블랙보드는 건드리지 않는다 — 죽은 타깃은 BTS_CheckValidTarget 이 알아서 비운다.
		if (bTaunted)
		{
			if (UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Pawn))
			{
				ASC->RemoveActiveGameplayEffect(Handle);
			}
		}
		return false;
	}

	// ★키 3개를 같이 쓴다★ — TargetToFollow 만 쓰면 HasFirstAttacker 가 false 로 남아
	// BTS_FindNearestPlayer_* 가 다음 틱에 가장 가까운 플레이어로 덮어쓴다 (바이트코드 확인 — §2.4).
	// 값이 같으면 블랙보드는 옵저버를 깨우지 않으므로 주기적으로 다시 써도 MoveTo 가 재경로를 잡지 않는다.
	if (HasKey(DRBlackboardKeys::FirstAttacker))    BB->SetValueAsObject(DRBlackboardKeys::FirstAttacker, Taunter);
	if (HasKey(DRBlackboardKeys::HasFirstAttacker)) BB->SetValueAsBool(DRBlackboardKeys::HasFirstAttacker, true);
	if (HasKey(DRBlackboardKeys::TargetToFollow))   BB->SetValueAsObject(DRBlackboardKeys::TargetToFollow, Taunter);
	if (HasKey(DRBlackboardKeys::DistanceToTarget)) BB->SetValueAsFloat(DRBlackboardKeys::DistanceToTarget, Pawn->GetDistanceTo(Taunter));

	OwnerController->SetFocus(Taunter);

	// ★전투 시간 갱신★ — BTS_CheckValidTarget 은 "타깃이 시야 거리보다 멀고 3초간 전투 없음"이면 타깃을 비운다.
	// 30m 도발은 이 조건에 쉽게 걸리므로, 도발 동안 계속 갱신해 막는다.
	if (ADRAIController* DRAIC = Cast<ADRAIController>(OwnerController))
	{
		DRAIC->UpdateCombatTime();
	}
	// 어그로 연출(ABP) — 첫 피격 어그로(DREnemyAttributeSet)와 같은 플래그
	if (ADREnemy* Enemy = Cast<ADREnemy>(Pawn))
	{
		Enemy->bIsAggroed = true;
	}
	return true;
}

void UDRAbilitySystemLibrary::StartTauntAggroRefresh(AAIController* OwnerController, float Interval)
{
	if (!OwnerController || !OwnerController->HasAuthority()) return;
	UWorld* World = OwnerController->GetWorld();
	if (!World) return;

	// 타이머 핸들을 람다가 들고 있어야 스스로 멈출 수 있다.
	// 같은 적에게 다시 도발이 걸려 타이머가 두 개 돌아도 무해하다(같은 값을 쓰고, 둘 다 GE 만료 때 멈춘다).
	TSharedRef<FTimerHandle> Handle = MakeShared<FTimerHandle>();
	TWeakObjectPtr<AAIController> WeakController(OwnerController);
	World->GetTimerManager().SetTimer(*Handle, FTimerDelegate::CreateLambda([WeakController, Handle, World]()
	{
		if (!WeakController.IsValid() || !ApplyTauntAggro(WeakController.Get()))
		{
			World->GetTimerManager().ClearTimer(*Handle);   // 도발 만료·도발자 사망·적 소멸
		}
	}), FMath::Max(0.05f, Interval), true);
}

// ========================= ★Plan8(가방)★ 몽타주 =========================

float UDRAbilitySystemLibrary::FindMontageEventTime(const UAnimMontage* Montage, const FGameplayTag& EventTag)
{
	if (!Montage || !EventTag.IsValid()) return -1.f;

	static const FName EventTagPropertyName(TEXT("EventTag"));
	for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
	{
		// AN_MontageEvent 는 BP 노티파이다 — C++ 타입을 모르므로 "EventTag" 라는 FGameplayTag 변수를 반사로 찾는다
		const UAnimNotify* Notify = NotifyEvent.Notify;
		if (!Notify) continue;

		const FStructProperty* Property =
			CastField<FStructProperty>(Notify->GetClass()->FindPropertyByName(EventTagPropertyName));
		if (!Property || Property->Struct != FGameplayTag::StaticStruct()) continue;

		const FGameplayTag* Tag = Property->ContainerPtrToValuePtr<FGameplayTag>(Notify);
		if (Tag && Tag->MatchesTagExact(EventTag))
		{
			return NotifyEvent.GetTriggerTime();
		}
	}
	return -1.f;
}
```

### 1-6. 피해 관문 — `Private/AbilitySystem/DRAttributeSet.cpp` (`PostGameplayEffectExecute`)

기존:
```cpp
	else if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		HandleIncomingDamage(Props);
	}
```
→ 바꾼 뒤:
```cpp
	else if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		// ★Plan8(모자)★ 모자 보호막 — 어떤 피해든 1회 무효 (Plan8 D-21)
		// 근접·원거리·DoT·탑승 공유 피해가 전부 지나는 유일한 관문이라 여기서 가로챈다.
		// 막으면 HandleIncomingDamage 를 아예 타지 않는다 → 히트리액트·넉백·디버프·어그로·부품 낙하 모두 없음.
		// (죽은 대상은 이 함수 맨 위에서 이미 돌아갔다 — 시체는 모자를 쓰지 않는다)
		if (GetIncomingDamage() > 0.f && UDRAbilitySystemLibrary::TryConsumeBellBoyHat(Props.TargetASC))
		{
			SetIncomingDamage(0.f);
			return;
		}

		// ★Plan8(시선집중)★ 도발된 적이 "주는" 피해 −20% — 맞는 대상 무관 (Plan8 D-11 / D-14)
		// ExecCalc_Damage 가 아니라 여기인 이유: 디버프 DoT 은 ExecCalc 를 타지 않는다 (Plan5 D-10 과 같은 이유).
		const float TauntMultiplier = UDRAbilitySystemLibrary::GetTauntDamageMultiplier(Props.SourceAvatarActor);
		if (TauntMultiplier != 1.f)
		{
			SetIncomingDamage(GetIncomingDamage() * TauntMultiplier);
		}

		HandleIncomingDamage(Props);
	}
```
- 탑승 공유 피해(`ADRCharacter::PropagateSharedDamage`)는 소스가 원 피격자(플레이어)라 도발 배율이 두 번 붙지 않는다(코드 확인).

### 1-7. 클렌저 사이트 — `Private/AbilitySystem/DRCleanserSiteAttributeSet.cpp` (`HandleIncomingDamage`)

include 추가 `AbilitySystem/DRAbilitySystemLibrary.h`(이 파일에는 아직 없다), 그리고 `if (LocalIncomingDamage > 0.0f)` 바로 안쪽 첫 줄에:
```cpp
		// ★Plan8(시선집중)★ 도발된 적이 사이트를 때려도 −20% (Plan8 D-14 — 맞는 대상 무관)
		// 사이트 AS 는 UDRAttributeSet 을 상속하지 않아 공용 관문을 타지 않는다 → 여기 따로 한 줄.
		if (const UAbilitySystemComponent* SourceASC =
				Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent())
		{
			LocalIncomingDamage *= UDRAbilitySystemLibrary::GetTauntDamageMultiplier(SourceASC->GetAvatarActor());
		}
```

✅ 확인(빌드는 STEP 2 끝에 한 번): 이 단계만으로는 게임 동작이 바뀌지 않는다 — 모자 GE·도발 GE 가 아직 없으므로 두 관문 모두 첫 줄의 태그 검사에서 빠진다.

---

## STEP 2. [C++] 스킬 클래스 5개

> 공통 규칙
> - 전부 **InstancedPerActor**(생성자에서 지정). 넷 실행 정책은 기본(Local Predicted).
> - **태그 기본값을 C++ 생성자에서 넣지 않는다** — CDO 가 만들어질 때 네이티브 태그가 아직 초기화 전일 수 있다(T-20). 태그는 전부 BP 에서 지정(STEP 6~8 값 표).
> - 몽타주 태스크 → 이벤트 태스크 순서가 아니라 **이벤트 대기를 먼저** 걸고 몽타주를 튼다(노티파이를 놓치지 않게).

### 2-1. 기본 공격 — `Public/AbilitySystem/Abilities/DRBellBoyBasicAttack.h`

```cpp
// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRMeleeAttack.h"
#include "DRBellBoyBasicAttack.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * ★Plan8(가방)★ 벨보이 기본 공격 — 여행 가방 휘두르기 (LMB).
 *
 *  · 좌/우 몽타주 교대. 첫 타 선0.4/후0.4, 연속 선0.2/후0.1 — 몽타주 재생 속도를
 *    타격 노티파이 앞·뒤로 따로 준다 (Plan8 D-4, Plan5 D-20 의 확장)
 *  · 타격 시각은 몽타주 노티파이에서 읽는다 (Plan8 D-5) — 노티파이를 옮기면 자동으로 따라온다
 *  · 타격부터 HitWindowDuration 동안 서버가 가방 주변을 여러 번 훑는다 (Plan8 D-8)
 *  · 스윙 시작 때 SkillSwitchLockEffect 를 (선딜 + 0.4초) 걸어 다른 스킬을 막는다 (Plan8 D-7)
 *
 * 부모(UDRMeleeAttack)에서 쓰는 것: AttackRadius · GetUpgradedAttackRadius() (범위 칩).
 * 부모의 GetWindupPlayRate / RestoreMontagePlayRate 는 쓰지 않는다 — 재생 속도는 이 클래스가 관리한다.
 */
UCLASS()
class DAERUNE_API UDRBellBoyBasicAttack : public UDRMeleeAttack
{
	GENERATED_BODY()

public:
	UDRBellBoyBasicAttack();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled) override;

protected:
	// ========== 에셋 ==========

	/** 3P 스윙 몽타주. [0] 오른쪽 → [1] 왼쪽 → … 연속 공격마다 번갈아 쓴다. 전부 UpperBody 슬롯 (Plan8 T-2) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|BasicAttack")
	TArray<TObjectPtr<UAnimMontage>> SwingMontages;

	/** 타격 이벤트 — 몽타주의 AN_MontageEvent 가 보내는 태그 (Event.Montage.BellBoy.BasicAttack) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|BasicAttack")
	FGameplayTag HitEventTag;

	/** 노티파이를 못 찾았을 때만 쓰는 타격 시각(몽타주 초). 평소엔 노티파이 시각을 읽는다 (Plan8 D-5) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|BasicAttack", meta = (ClampMin = "0.01"))
	float FallbackHitTime = 0.4f;

	// ========== 타이밍 (실제 초) ==========

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.01"))
	float FirstWindupTime = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.01"))
	float FirstRecoveryTime = 0.4f;

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.01"))
	float ComboWindupTime = 0.2f;

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.01"))
	float ComboRecoveryTime = 0.1f;

	/** 앞 스윙이 끝난 뒤 이 시간 안에 다음 스윙이 시작되면 "연속" (Plan8 D-6). 너무 짧으면 서버·클라가 갈린다 (Plan8 T-5) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.0"))
	float ComboResetTime = 0.25f;

	/** 다른 스킬로 넘어갈 때의 후딜(타격 기준 초). 스윙 시작 때 (선딜 + 이 값) 동안 다른 스킬을 막는다 (Plan8 D-7) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing", meta = (ClampMin = "0.0"))
	float SkillSwitchRecoveryTime = 0.4f;

	/** 위 차단용 GE — Has Duration(SetByCaller Data.Duration) + Block Abilities with Tags(시선 집중·모자) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Timing")
	TSubclassOf<UGameplayEffect> SkillSwitchLockEffect;

	// ========== 판정 ==========

	/** 판정 창 길이(실제 초). 0 이면 타격 순간 1회만 — 정원로봇 발톱과 같다 (Plan8 D-8) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hit", meta = (ClampMin = "0.0"))
	float HitWindowDuration = 0.1f;

	/** 판정 창 안에서 훑는 횟수. 4 면 0 / 0.025 / 0.05 / 0.075 초 */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hit", meta = (ClampMin = "1"))
	int32 HitWindowSamples = 4;

	/** 판정 중심 — CombatSocket.LeftHand → BP_BellBoy 의 LeftHandSocketName(BagSocket) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hit")
	FGameplayTag HitSocketTag;

	/** 켜면 판정 구를 0.5초 그린다 (반경 조정용) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hit")
	bool bDrawDebugHit = false;

private:
	UFUNCTION()
	void OnHitEvent(FGameplayEventData Payload);

	UFUNCTION()
	void OnHitSample(int32 SampleIndex);

	UFUNCTION()
	void OnHitWindowFinished(int32 ActionNumber);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageAborted();

	// 서버 = ASC::CurrentMontageSetPlayRate(프록시 복제) / 소유 클라 = 로컬 AnimInstance (Plan8 D-10)
	void SetSwingPlayRate(float NewRate);

	// 몽타주와 판정 창이 ★둘 다★ 끝나야 어빌리티를 끝낸다
	void TryFinishSwing();

	// ── 스윙 사이에 유지 (InstancedPerActor — 서버·소유 클라가 각자 들고 있다) ──
	double LastSwingEndTime = -1000.0;
	int32 NextSwingIndex = 0;

	// ── 이번 스윙 ──
	float PendingRecoveryRate = 1.f;
	bool bHitEventReceived = false;
	bool bMontageDone = false;
	bool bHitWindowDone = false;
	TSet<TWeakObjectPtr<AActor>> HitActorsThisSwing;
};
```

**`Private/AbilitySystem/Abilities/DRBellBoyBasicAttack.cpp`**
```cpp
// Copyright DaeRune

#include "AbilitySystem/Abilities/DRBellBoyBasicAttack.h"

#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_Repeat.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "DRGameplayTags.h"
#include "DrawDebugHelpers.h"
#include "Interaction/CombatInterface.h"
#include "DaeRune/DRLogChannels.h"

UDRBellBoyBasicAttack::UDRBellBoyBasicAttack()
{
	// 콤보 상태(LastSwingEndTime / NextSwingIndex)를 인스턴스에 둔다
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UDRBellBoyBasicAttack::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// ── ① 첫 타인가, 연속인가 (D-6) ──
	const bool bCombo = (GetWorld()->GetTimeSeconds() - LastSwingEndTime) <= ComboResetTime;
	if (!bCombo)
	{
		NextSwingIndex = 0;   // 첫 타는 항상 오른쪽부터
	}

	UAnimMontage* Montage = SwingMontages.IsValidIndex(NextSwingIndex) ? SwingMontages[NextSwingIndex].Get() : nullptr;
	if (!Montage)
	{
		UE_LOG(LogDR, Warning, TEXT("[BellBoy] %s: SwingMontages[%d] 가 비어 있다"), *GetName(), NextSwingIndex);
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	NextSwingIndex = (NextSwingIndex + 1) % SwingMontages.Num();

	// ── ② 선딜·후딜 → 재생 속도 (D-4) ──
	float HitTime = UDRAbilitySystemLibrary::FindMontageEventTime(Montage, HitEventTag);
	if (HitTime <= 0.f)
	{
		UE_LOG(LogDR, Warning, TEXT("[BellBoy] %s 에서 %s 노티파이를 못 찾음 — FallbackHitTime %.2f 사용"),
			*GetNameSafe(Montage), *HitEventTag.ToString(), FallbackHitTime);
		HitTime = FallbackHitTime;
	}

	// 칩(SkillWindup) — 칩이 없으면 정확히 1.0. 나중에 벨보이 칩이 생겨도 그대로 동작한다.
	const float WindupScale = FMath::Max(0.01f, GetUpgradedFloat(EDRUpgradeStat::SkillWindup, 1.f));
	const float WindupTime = (bCombo ? ComboWindupTime : FirstWindupTime) * WindupScale;
	const float RecoveryTime = bCombo ? ComboRecoveryTime : FirstRecoveryTime;

	const float WindupRate = HitTime / FMath::Max(0.01f, WindupTime);                                  // 0.4/0.4=1.0 · 0.4/0.2=2.0
	PendingRecoveryRate = FMath::Max(0.01f, Montage->GetPlayLength() - HitTime) / FMath::Max(0.01f, RecoveryTime); // 1.0 · 4.0

	// ── ③ 다른 스킬 차단 (선딜 + 0.4초) — 활성화 순간이라 예측 키가 살아 있어 클라 UI 도 즉시 막힌다 (D-7) ──
	if (SkillSwitchLockEffect)
	{
		FGameplayEffectSpecHandle LockSpec = MakeOutgoingGameplayEffectSpec(SkillSwitchLockEffect, GetAbilityLevel());
		if (LockSpec.IsValid())
		{
			LockSpec.Data->SetSetByCallerMagnitude(FDRGameplayTags::Get().Data_Duration, WindupTime + SkillSwitchRecoveryTime);
			ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, LockSpec);
		}
	}

	// ── ④ 이번 스윙 상태 ──
	bHitEventReceived = false;
	bMontageDone = false;
	bHitWindowDone = false;
	HitActorsThisSwing.Reset();

	// ── ⑤ 타격 이벤트를 먼저 기다리고, 몽타주를 튼다 ──
	UAbilityTask_WaitGameplayEvent* HitTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, HitEventTag, nullptr, /*OnlyTriggerOnce*/ true);
	HitTask->EventReceived.AddDynamic(this, &UDRBellBoyBasicAttack::OnHitEvent);
	HitTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, Montage, WindupRate);
	MontageTask->OnCompleted.AddDynamic(this, &UDRBellBoyBasicAttack::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UDRBellBoyBasicAttack::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UDRBellBoyBasicAttack::OnMontageAborted);
	MontageTask->OnCancelled.AddDynamic(this, &UDRBellBoyBasicAttack::OnMontageAborted);
	MontageTask->ReadyForActivation();

	// (STEP 11) FP 에셋이 오면 여기서 FirstPersonMesh 에 같은 속도로 1P 몽타주를 튼다
}

void UDRBellBoyBasicAttack::OnHitEvent(FGameplayEventData Payload)
{
	bHitEventReceived = true;

	// ① 후딜 속도로 — 서버·소유 클라 모두 (D-4 / D-10)
	SetSwingPlayRate(PendingRecoveryRate);

	// ② 판정은 서버만. 클라는 판정 창이 없는 것으로 친다
	if (!HasAuthority(&CurrentActivationInfo))
	{
		bHitWindowDone = true;
		TryFinishSwing();
		return;
	}

	if (HitWindowDuration <= 0.f || HitWindowSamples <= 1)
	{
		OnHitSample(0);   // 정원로봇 발톱과 같은 1회 판정
		bHitWindowDone = true;
		TryFinishSwing();
		return;
	}

	// 0 / 0.025 / 0.05 / 0.075 초 — AbilityTask_Repeat 는 활성화 즉시 1회를 실행한다 (엔진 확인)
	UAbilityTask_Repeat* RepeatTask = UAbilityTask_Repeat::RepeatAction(
		this, HitWindowDuration / HitWindowSamples, HitWindowSamples);
	RepeatTask->OnPerformAction.AddDynamic(this, &UDRBellBoyBasicAttack::OnHitSample);
	RepeatTask->OnFinished.AddDynamic(this, &UDRBellBoyBasicAttack::OnHitWindowFinished);
	RepeatTask->ReadyForActivation();
}

void UDRBellBoyBasicAttack::OnHitSample(int32 SampleIndex)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	const FVector Center = ICombatInterface::Execute_GetCombatSocketLocation(Avatar, HitSocketTag);
	const float Radius = GetUpgradedAttackRadius();   // 범위 칩(SkillAreaRadius) 반영

	if (bDrawDebugHit)
	{
		DrawDebugSphere(GetWorld(), Center, Radius, 12, FColor::Orange, false, 0.5f);
	}

	// 대상 수집은 정원로봇 발톱과 같은 함수 — 홀로그램 두더지·클렌저 사이트 처리까지 같게 (D-32)
	TArray<AActor*> Targets;
	UDRAbilitySystemLibrary::GetLiveObjectsWithinRadius(Avatar, Targets, TArray<AActor*>{ Avatar }, Radius, Center);

	for (AActor* Target : Targets)
	{
		if (!IsValid(Target) || HitActorsThisSwing.Contains(Target)) continue;   // 한 스윙에 한 대상 1회
		if (!UDRAbilitySystemLibrary::IsValidDamageTarget(Avatar, Target)) continue;

		HitActorsThisSwing.Add(Target);
		// 피해 30 + 넉백(KnockbackForceMagnitude, 45° 위) — 기존 파이프라인 그대로 (D-9)
		UDRAbilitySystemLibrary::ApplyDamageEffect(MakeDamageEffectParamsFromClassDefaults(Target));
	}
}

void UDRBellBoyBasicAttack::OnHitWindowFinished(int32 ActionNumber)
{
	bHitWindowDone = true;
	TryFinishSwing();
}

void UDRBellBoyBasicAttack::OnMontageFinished()
{
	bMontageDone = true;

	// 노티파이가 없어서 타격 이벤트를 못 받았으면 판정 창도 끝난 것으로 — 안 그러면 영원히 안 끝난다 (T-22)
	if (!bHitEventReceived)
	{
		bHitWindowDone = true;
	}
	TryFinishSwing();
}

void UDRBellBoyBasicAttack::OnMontageAborted()
{
	// 피격·스턴으로 끊김 → 콤보도 끊는다 (다음 스윙은 첫 타)
	LastSwingEndTime = -1000.0;
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UDRBellBoyBasicAttack::TryFinishSwing()
{
	// 연속 공격은 후딜(0.1초)과 판정 창(0.1초)이 거의 같이 끝난다 — 둘 다 기다려야 마지막 샘플이 잘리지 않는다
	if (bMontageDone && bHitWindowDone)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UDRBellBoyBasicAttack::SetSwingPlayRate(float NewRate)
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	if (ASC->IsOwnerActorAuthoritative())
	{
		// 서버: 로컬 재생 속도 + 시뮬레이티드 프록시용 복제 데이터 갱신 (엔진 CurrentMontageSetPlayRate)
		ASC->CurrentMontageSetPlayRate(NewRate);
		return;
	}

	// 소유 클라: 로컬만. (CurrentMontageSetPlayRate 를 쓰면 서버로 RPC 가 한 번 더 간다)
	if (const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo())
	{
		if (UAnimInstance* AnimInstance = Info->GetAnimInstance())
		{
			if (UAnimMontage* ActiveMontage = AnimInstance->GetCurrentActiveMontage())
			{
				AnimInstance->Montage_SetPlayRate(ActiveMontage, NewRate);
			}
		}
	}
	// (STEP 11) FP 메시 몽타주도 같은 속도로
}

void UDRBellBoyBasicAttack::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
	if (!bWasCancelled)
	{
		LastSwingEndTime = GetWorld()->GetTimeSeconds();   // 연속 판정 기준 (D-6)
	}
	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
```

### 2-2. 시선 집중 — `Public/AbilitySystem/Abilities/DRBellBoyTaunt.h`

```cpp
// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "DRBellBoyTaunt.generated.h"

class UAnimMontage;
class UGameplayEffect;

/**
 * ★Plan8(시선집중)★ 벨보이 스킬1 — 시선 집중 (E).
 *
 *  · 5초 도발 모션(상체 슬롯 — 걸을 수 있다). 모션 동안 다른 스킬 불가 + 피격 경직 없음(BP 의 Block 태그, Plan8 D-18)
 *  · 1초 노티파이 → 0.5초 동안 0.1초마다 30m 구를 훑어 새 적을 도발 (Plan8 D-19)
 *  · 도발 = 적에게 GE_BellBoy_Taunted — 걸린 순간부터 5초, 이속 ×1.1, 주는 피해 ×0.8 (Plan8 D-11 / D-12 / D-13)
 *  · ★어빌리티가 끝나도 도발은 남는다★ — 적마다 자기 5초가 끝날 때 풀린다 (사용자 확정)
 */
UCLASS()
class DAERUNE_API UDRBellBoyTaunt : public UDRGameplayAbility
{
	GENERATED_BODY()

public:
	UDRBellBoyTaunt();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 도발 모션 (BB_Taunt 5초, UpperBody 슬롯 — 걸으면서 도발) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt")
	TObjectPtr<UAnimMontage> TauntMontage;

	/** 판정 시작 이벤트 (Event.Montage.BellBoy.Taunt — 몽타주 1.0초) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt")
	FGameplayTag TauntStartEventTag;

	/** 도발 반경(uu). 3000 = 30m */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.0"))
	float TauntRadius = 3000.f;

	/** 판정 유지 시간 / 간격 — 0.5초 동안 0.1초마다 (0.0 ~ 0.5 → 6회) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.0"))
	float DetectWindow = 0.5f;

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.02"))
	float DetectInterval = 0.1f;

	/** 적 1마리의 도발 시간 — ★걸린 순간부터★ (Plan8 D-12) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.1"))
	float TauntDuration = 5.f;

	/** 도발된 적의 이동 속도 배율 (1.1 = +10%) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.0"))
	float TauntedMoveSpeedMultiplier = 1.1f;

	/** 도발된 적이 "주는" 피해 배율 (0.8 = −20%, 맞는 대상 무관 — Plan8 D-11) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt", meta = (ClampMin = "0.0"))
	float TauntedDamageMultiplier = 0.8f;

	/** 적에게 거는 GE — Has Duration(Data.Duration) / MoveSpeed Multiply(Compound)(Data.MoveSpeed.Mult) / Target Tags Debuff.Taunted */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt")
	TSubclassOf<UGameplayEffect> TauntedEffect;

	/** 켜면 벽 너머의 적은 도발하지 않는다 (기본 꺼짐 — 요구 원문 "30m 이내의 모든 적", Plan8 D-19) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt")
	bool bRequireLineOfSight = false;

	/** 판정 시작 때 벨보이 위치에 실행할 연출 큐 (선택 — 외침·충격파). RawMagnitude = 반경 */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Taunt")
	FGameplayTag TauntStartCueTag;

private:
	UFUNCTION()
	void OnTauntStart(FGameplayEventData Payload);

	UFUNCTION()
	void OnScan(int32 ActionNumber);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageAborted();

	void ApplyTauntTo(AActor* Enemy);
	bool HasLineOfSight(const AActor* From, const AActor* To) const;

	// 이번 시전에 이미 도발한 적 — 한 시전에 적당 1회 (Plan8 D-15)
	TSet<TWeakObjectPtr<AActor>> TauntedThisCast;
};
```

**`DRBellBoyTaunt.cpp`** — 핵심 본문 (include: `AbilitySystemBlueprintLibrary.h`, `AIController.h`, 태스크 3종, `DRAbilitySystemLibrary.h`, `DRGameplayTags.h`)
```cpp
UDRBellBoyTaunt::UDRBellBoyTaunt()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UDRBellBoyTaunt::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))   // 물 60 (WaterCost + GE_Cost)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}
	TauntedThisCast.Reset();

	UAbilityTask_WaitGameplayEvent* StartTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, TauntStartEventTag, nullptr, /*OnlyTriggerOnce*/ true);
	StartTask->EventReceived.AddDynamic(this, &UDRBellBoyTaunt::OnTauntStart);
	StartTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, TauntMontage, 1.f);
	MontageTask->OnCompleted.AddDynamic(this, &UDRBellBoyTaunt::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UDRBellBoyTaunt::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UDRBellBoyTaunt::OnMontageAborted);
	MontageTask->OnCancelled.AddDynamic(this, &UDRBellBoyTaunt::OnMontageAborted);
	MontageTask->ReadyForActivation();
}

void UDRBellBoyTaunt::OnTauntStart(FGameplayEventData Payload)
{
	if (!HasAuthority(&CurrentActivationInfo)) return;   // 판정·GE 는 서버만

	if (TauntStartCueTag.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			FGameplayCueParameters Params;
			Params.Location = GetAvatarActorFromActorInfo()->GetActorLocation();
			Params.RawMagnitude = TauntRadius;
			ASC->ExecuteGameplayCue(TauntStartCueTag, Params);
		}
	}

	// 0.0 / 0.1 / … / 0.5 초 → 6회 (Repeat 는 활성화 즉시 1회를 실행한다)
	const int32 ScanCount = FMath::Max(1, FMath::FloorToInt(DetectWindow / DetectInterval + KINDA_SMALL_NUMBER) + 1);
	UAbilityTask_Repeat* ScanTask = UAbilityTask_Repeat::RepeatAction(this, DetectInterval, ScanCount);
	ScanTask->OnPerformAction.AddDynamic(this, &UDRBellBoyTaunt::OnScan);
	ScanTask->ReadyForActivation();
}

void UDRBellBoyTaunt::OnScan(int32 ActionNumber)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	if (!Avatar) return;

	TArray<AActor*> Enemies;
	UDRAbilitySystemLibrary::GetAliveEnemiesWithinRadius(Avatar, Avatar->GetActorLocation(), TauntRadius, Enemies);

	for (AActor* Enemy : Enemies)
	{
		if (TauntedThisCast.Contains(Enemy)) continue;                     // 이번 시전에 이미 (D-15)
		if (!UDRAbilitySystemLibrary::CanBeTaunted(Enemy)) continue;       // 도주형 제외 (D-17)
		if (bRequireLineOfSight && !HasLineOfSight(Avatar, Enemy)) continue;

		ApplyTauntTo(Enemy);
		TauntedThisCast.Add(Enemy);
	}
}

void UDRBellBoyTaunt::ApplyTauntTo(AActor* Enemy)
{
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Enemy);
	if (!TargetASC || !TauntedEffect) return;

	const FDRGameplayTags& Tags = FDRGameplayTags::Get();

	// 마지막으로 건 벨보이가 이긴다 — 이전 도발(다른 벨보이 것 포함)을 먼저 지운다 (D-15)
	TargetASC->RemoveActiveEffectsWithGrantedTags(FGameplayTagContainer(Tags.Debuff_Taunted));

	// 스펙 컨텍스트 = 이 어빌리티 → OriginalInstigatorASC = 벨보이 (도발자 식별 — D-13 / T-6)
	FGameplayEffectSpecHandle Spec = MakeOutgoingGameplayEffectSpec(TauntedEffect, GetAbilityLevel());
	if (!Spec.IsValid()) return;

	Spec.Data->SetSetByCallerMagnitude(Tags.Data_Duration, TauntDuration);                       // 걸린 순간부터 5초
	Spec.Data->SetSetByCallerMagnitude(Tags.Data_MoveSpeed_Mult, TauntedMoveSpeedMultiplier);   // ×1.1
	Spec.Data->SetSetByCallerMagnitude(Tags.Data_Taunt_DamageMult, TauntedDamageMultiplier);    // ×0.8
	TargetASC->ApplyGameplayEffectSpecToSelf(*Spec.Data.Get());

	// 지금 바로 타깃을 벨보이로 바꾸고, 도발이 끝날 때까지 0.2초마다 다시 쓴다 (D-16)
	if (const APawn* Pawn = Cast<APawn>(Enemy))
	{
		AAIController* AIC = Cast<AAIController>(Pawn->GetController());
		UDRAbilitySystemLibrary::ApplyTauntAggro(AIC);
		UDRAbilitySystemLibrary::StartTauntAggroRefresh(AIC);
	}
}

bool UDRBellBoyTaunt::HasLineOfSight(const AActor* From, const AActor* To) const
{
	// ★월드 지형만★ 본다 — Visibility 채널로 쏘면 사이에 선 다른 적의 히트박스가 시야를 막는다 (T-17 과 같은 이유)
	FCollisionQueryParams Params(SCENE_QUERY_STAT(BellBoyTauntLOS), false, From);
	Params.AddIgnoredActor(To);
	return !GetWorld()->LineTraceTestByObjectType(From->GetActorLocation(), To->GetActorLocation(),
		FCollisionObjectQueryParams(ECC_WorldStatic), Params);
}

void UDRBellBoyTaunt::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);   // 도발은 적마다 남는다
}

void UDRBellBoyTaunt::OnMontageAborted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
```

### 2-3. 모자 던지기 — `Public/AbilitySystem/Abilities/DRBellBoyHatThrow.h`

```cpp
// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRProjectileSpell.h"
#include "DRBellBoyHatThrow.generated.h"

class UAnimMontage;

/**
 * ★Plan8(모자)★ 벨보이 스킬2 — 모자 던지기 (RMB).
 * 릴리스 노티파이에서 BP_BellBoyHatProjectile 을 조준점으로 스폰한다. 이후 판정은 전부 투사체·픽업이 한다.
 * 피해(30)·소스 ASC 는 부모 SpawnProjectile 이 MakeDamageEffectParamsFromClassDefaults() 로 넣어 준다.
 */
UCLASS()
class DAERUNE_API UDRBellBoyHatThrow : public UDRProjectileSpell
{
	GENERATED_BODY()

public:
	UDRBellBoyHatThrow();

	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData) override;

protected:
	/** 던지기 모션 (BB_ThrowCap 0.8초, UpperBody) — 릴리스 노티파이 + ANS_BellBoy_HideCap */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|HatThrow")
	TObjectPtr<UAnimMontage> ThrowMontage;

	/** 모자가 손을 떠나는 순간 (Event.Montage.BellBoy.HatThrow) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|HatThrow")
	FGameplayTag ReleaseEventTag;

	/** 모자가 나가는 위치 — CombatSocket.RightHand → BP_BellBoy 의 RightHandSocketName(CapSocket) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|HatThrow")
	FGameplayTag ThrowSocketTag;

private:
	UFUNCTION()
	void OnRelease(FGameplayEventData Payload);

	UFUNCTION()
	void OnMontageFinished();

	UFUNCTION()
	void OnMontageAborted();
};
```

**`DRBellBoyHatThrow.cpp`** — 핵심 본문 (include: `Abilities/Tasks/AbilityTask_PlayMontageAndWait.h`, `Abilities/Tasks/AbilityTask_WaitGameplayEvent.h`)
```cpp
UDRBellBoyHatThrow::UDRBellBoyHatThrow()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	NumProjectiles = 1;   // 부모 기본값은 5 — 모자는 한 개다
}

void UDRBellBoyHatThrow::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))   // 물 20
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	UAbilityTask_WaitGameplayEvent* ReleaseTask = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
		this, ReleaseEventTag, nullptr, /*OnlyTriggerOnce*/ true);
	ReleaseTask->EventReceived.AddDynamic(this, &UDRBellBoyHatThrow::OnRelease);
	ReleaseTask->ReadyForActivation();

	UAbilityTask_PlayMontageAndWait* MontageTask =
		UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(this, NAME_None, ThrowMontage, 1.f);
	MontageTask->OnCompleted.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageFinished);
	MontageTask->OnBlendOut.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageFinished);
	MontageTask->OnInterrupted.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageAborted);
	MontageTask->OnCancelled.AddDynamic(this, &UDRBellBoyHatThrow::OnMontageAborted);
	MontageTask->ReadyForActivation();
}

void UDRBellBoyHatThrow::OnRelease(FGameplayEventData Payload)
{
	// 스폰은 서버만 (SpawnProjectile 안에서도 확인한다). 조준점 = 크로스헤어(카메라 트레이스)
	if (!HasAuthority(&CurrentActivationInfo)) return;
	SpawnProjectile(CalculateTargetLocation(), ThrowSocketTag);
}

void UDRBellBoyHatThrow::OnMontageFinished()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UDRBellBoyHatThrow::OnMontageAborted()
{
	// 릴리스 전에 끊기면 모자는 안 나간다 — 물은 이미 썼다(다른 스킬과 같은 규칙)
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}
```

### 2-4. 모자 투사체 — `Public/Actor/DRBellBoyHatProjectile.h`

```cpp
// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DRProjectile.h"
#include "DRBellBoyHatProjectile.generated.h"

class ADRBellBoyHatPickup;
class URotatingMovementComponent;
class UStaticMeshComponent;

/**
 * ★Plan8(모자)★ 날아가는 모자.
 * 부모(ADRProjectile)의 이동·수명·임팩트 연출을 그대로 쓰고 "무엇에 닿았나"만 다시 판정한다:
 *   자신·시체·트리거·다른 투사체 → 무시 / 적 → 피해 후 소멸 / 아군 → 모자(없으면) 또는 그 자리 픽업(있으면)
 *   / 폰을 막는 표면(벽·바닥·클렌저 사이트) → 그 자리 바닥에 픽업
 */
UCLASS()
class DAERUNE_API ADRBellBoyHatProjectile : public ADRProjectile
{
	GENERATED_BODY()

public:
	ADRBellBoyHatProjectile();

protected:
	virtual void BeginPlay() override;

	virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult) override;

	/** 날아가는 모자 외형 (충돌 없음). 회전은 SpinComponent 가 이 메시만 돌린다 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BellBoy|Hat")
	TObjectPtr<UStaticMeshComponent> HatMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BellBoy|Hat")
	TObjectPtr<URotatingMovementComponent> SpinComponent;

	/** 벽·바닥·모자 쓴 아군에 닿았을 때 생기는 픽업 */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hat")
	TSubclassOf<ADRBellBoyHatPickup> PickupClass;

	/** 픽업을 바닥에서 얼마나 띄울지(uu) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hat")
	float PickupGroundOffset = 5.f;

private:
	AActor* GetThrower() const;
	void HandleEnemyHit(AActor* Target);
	void DropAsPickup(const FVector& FromLocation);

	// 한 번만 처리 (캡슐·히트박스가 같은 프레임에 여러 번 겹친다)
	bool bResolved = false;
};
```

**`Private/Actor/DRBellBoyHatProjectile.cpp`** — 핵심 본문 (include: `AbilitySystemBlueprintLibrary.h`, `AbilitySystem/DRAbilitySystemLibrary.h`, `Actor/DRBellBoyHatPickup.h`, `Character/DRCharacterBase.h`, `Components/SphereComponent.h`, `GameFramework/ProjectileMovementComponent.h`, `GameFramework/RotatingMovementComponent.h`, `Interaction/CombatInterface.h`, `Interaction/DRProximityHitOnly.h`)
```cpp
ADRBellBoyHatProjectile::ADRBellBoyHatProjectile()
{
	HatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HatMesh"));
	HatMesh->SetupAttachment(GetRootComponent());
	HatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SpinComponent = CreateDefaultSubobject<URotatingMovementComponent>(TEXT("Spin"));
	SpinComponent->RotationRate = FRotator(0.f, 720.f, 0.f);   // Yaw 로 빙그르르
	SpinComponent->bAutoRegisterUpdatedComponent = false;     // ★루트(구)가 아니라 모자 메시만 돈다 → BeginPlay★

	// 부모 기본값(550, 중력 0)을 모자에 맞게 — BP 에서 다시 조정 가능
	ProjectileMovement->InitialSpeed = 1400.f;
	ProjectileMovement->MaxSpeed = 1400.f;
	ProjectileMovement->ProjectileGravityScale = 0.35f;       // 아무도 안 맞으면 바닥으로 떨어진다
}

void ADRBellBoyHatProjectile::BeginPlay()
{
	Super::BeginPlay();
	SpinComponent->SetUpdatedComponent(HatMesh);   // 연출이라 모든 머신에서. 루트를 돌리면 비행 방향이 틀어진다
}

AActor* ADRBellBoyHatProjectile::GetThrower() const
{
	// 스폰할 때 Instigator 는 비어 있다(플레이어 ASC 소유자가 PlayerState) — 소스 ASC 의 아바타가 던진 벨보이다
	return DamageEffectParams.SourceAbilitySystemComponent
		? DamageEffectParams.SourceAbilitySystemComponent->GetAvatarActor() : nullptr;
}

void ADRBellBoyHatProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	// 판정은 서버만. 클라는 서버의 Destroy 복제를 기다린다 (부모 Destroyed 가 클라 임팩트 연출 — T-21)
	if (!HasAuthority() || bResolved) return;

	AActor* Thrower = GetThrower();
	if (!IsValid(OtherActor) || OtherActor == this || OtherActor == Thrower) return;   // 스폰 위치가 캡슐 안 (T-15)
	if (OtherActor->IsA<ADRProjectile>() || OtherActor->IsA<ADRBellBoyHatPickup>()) return;

	// ① 캐릭터 — 아군 / 적
	if (ADRCharacterBase* Character = Cast<ADRCharacterBase>(OtherActor))
	{
		if (ICombatInterface::Execute_IsDead(Character)) return;   // 시체는 통과

		if (!UDRAbilitySystemLibrary::IsNotFriend(Thrower, Character))   // 아군 = 둘 다 "Player" 태그
		{
			bResolved = true;
			if (UDRAbilitySystemLibrary::TryEquipBellBoyHat(Character, DamageEffectParams.SourceAbilitySystemComponent, 1))
			{
				Destroy();                              // 머리에 씌웠다
			}
			else
			{
				DropAsPickup(GetActorLocation());       // ★이미 모자 → 부딪혀 그 자리 바닥에 (D-22, 사용자 확정)★
			}
			return;
		}

		HandleEnemyHit(Character);                  // 적 → 피해 30 후 소멸
		return;
	}

	// ② 캐릭터가 아닌 전투 대상(홀로그램 두더지) — 적과 같게 (근접 규칙은 ApplyDamageEffect 가 처리)
	if (Cast<IDRProximityHitOnly>(OtherActor))
	{
		HandleEnemyHit(OtherActor);
		return;
	}

	// ③ 벽·바닥 = 폰을 막는 표면만 (D-24) — 트리거 볼륨(Overlap 전용)은 무시. 클렌저 사이트도 여기로 온다
	if (OtherComp && OtherComp->GetCollisionResponseToChannel(ECC_Pawn) == ECR_Block)
	{
		bResolved = true;
		DropAsPickup(GetActorLocation());
	}
}

void ADRBellBoyHatProjectile::HandleEnemyHit(AActor* Target)
{
	bResolved = true;
	if (!bHit) OnHit();   // 부모의 임팩트 이펙트·사운드

	if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Target))
	{
		DamageEffectParams.TargetAbilitySystemComponent = TargetASC;
		DamageEffectParams.DeathImpulse = GetActorForwardVector() * DamageEffectParams.DeathImpulseMagnitude;
		UDRAbilitySystemLibrary::ApplyDamageEffect(DamageEffectParams);   // BaseDamage = GA 의 Damage(30)
	}
	Destroy();
}

void ADRBellBoyHatProjectile::DropAsPickup(const FVector& FromLocation)
{
	if (PickupClass)
	{
		// 바닥 찾기 — ★오브젝트 타입(WorldStatic/Dynamic)으로★ 쏜다.
		// Visibility 채널로 쏘면 적 히트박스(Visibility Block)에 맞아 픽업이 공중에 뜬다 (T-17)
		FVector SpawnLocation = FromLocation;
		FCollisionObjectQueryParams ObjectParams;
		ObjectParams.AddObjectTypesToQuery(ECC_WorldStatic);
		ObjectParams.AddObjectTypesToQuery(ECC_WorldDynamic);
		FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(BellBoyHatDrop), false, this);

		FHitResult GroundHit;
		if (GetWorld()->LineTraceSingleByObjectType(GroundHit, FromLocation + FVector(0.f, 0.f, 50.f),
				FromLocation - FVector(0.f, 0.f, 5000.f), ObjectParams, QueryParams))
		{
			SpawnLocation = GroundHit.ImpactPoint + FVector(0.f, 0.f, PickupGroundOffset);
		}

		const FTransform SpawnTransform(FRotator(0.f, GetActorRotation().Yaw, 0.f), SpawnLocation);
		if (ADRBellBoyHatPickup* Pickup = GetWorld()->SpawnActorDeferred<ADRBellBoyHatPickup>(
				PickupClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn))
		{
			Pickup->InitThrower(GetThrower(), DamageEffectParams.SourceAbilitySystemComponent);   // 던진 본인이면 2회 (D-23)
			Pickup->FinishSpawning(SpawnTransform);
		}
	}
	Destroy();
}
```

### 2-5. 바닥의 모자 — `Public/Actor/DRBellBoyHatPickup.h`

```cpp
// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRBellBoyHatPickup.generated.h"

class UAbilitySystemComponent;
class USphereComponent;
class UStaticMeshComponent;

/**
 * ★Plan8(모자)★ 바닥에 떨어진 모자. LifeTime(10초) 뒤 사라진다.
 * 닿은 캐릭터는 적이든 아군이든 모자를 쓴다(모자가 없을 때만). 던진 벨보이 본인이면 2회 (Plan8 D-23).
 * 판정은 서버만, 액터는 복제된다(모든 화면에 보인다).
 */
UCLASS()
class DAERUNE_API ADRBellBoyHatPickup : public AActor
{
	GENERATED_BODY()

public:
	ADRBellBoyHatPickup();

	/** 스폰 직후(FinishSpawning 전) 투사체가 부른다. 서버 전용 값이라 복제하지 않는다 */
	void InitThrower(AActor* InThrower, UAbilitySystemComponent* InThrowerASC);

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	// 이미 겹쳐 있던 대상 / 모자를 잃고 그 자리에 서 있는 대상을 위해 주기적으로 다시 본다 (Plan8 D-25)
	void RecheckOverlaps();
	void TryGiveTo(AActor* Candidate);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BellBoy|Hat")
	TObjectPtr<USphereComponent> PickupSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "BellBoy|Hat")
	TObjectPtr<UStaticMeshComponent> HatMesh;

	/** 바닥에 남아 있는 시간(초) */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hat", meta = (ClampMin = "0.1"))
	float LifeTime = 10.f;

	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hat", meta = (ClampMin = "0.05"))
	float RecheckInterval = 0.25f;

	/** 던진 벨보이 본인이 주웠을 때 막는 횟수 */
	UPROPERTY(EditDefaultsOnly, Category = "BellBoy|Hat", meta = (ClampMin = "1"))
	int32 ThrowerCharges = 2;

private:
	TWeakObjectPtr<AActor> Thrower;
	TWeakObjectPtr<UAbilitySystemComponent> ThrowerASC;
	FTimerHandle RecheckTimerHandle;
	bool bConsumed = false;
};
```

**`Private/Actor/DRBellBoyHatPickup.cpp`** — 핵심 본문 (include: `Components/SphereComponent.h`, `Components/StaticMeshComponent.h`, `AbilitySystem/DRAbilitySystemLibrary.h`, `Character/DRCharacterBase.h`, `Interaction/CombatInterface.h`, `TimerManager.h`)
```cpp
ADRBellBoyHatPickup::ADRBellBoyHatPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;   // 서버가 스폰, 모든 클라가 본다 (움직이지 않으므로 이동 복제는 끈 채로)

	PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
	SetRootComponent(PickupSphere);
	PickupSphere->InitSphereRadius(60.f);
	PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	PickupSphere->SetCollisionObjectType(ECC_WorldDynamic);
	PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);   // 캐릭터 캡슐만
	PickupSphere->SetGenerateOverlapEvents(true);

	HatMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HatMesh"));
	HatMesh->SetupAttachment(PickupSphere);
	HatMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void ADRBellBoyHatPickup::InitThrower(AActor* InThrower, UAbilitySystemComponent* InThrowerASC)
{
	Thrower = InThrower;
	ThrowerASC = InThrowerASC;
}

void ADRBellBoyHatPickup::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority()) return;

	SetLifeSpan(LifeTime);   // 10초 뒤 서버가 Destroy → 복제
	PickupSphere->OnComponentBeginOverlap.AddDynamic(this, &ADRBellBoyHatPickup::OnPickupOverlap);

	// ★즉시 1회 + 주기 재확인★ — BeginOverlap 만으로는 "이미 올라서 있던 대상"이 모자를 잃은 뒤 못 줍는다 (D-25)
	GetWorldTimerManager().SetTimer(RecheckTimerHandle, this, &ADRBellBoyHatPickup::RecheckOverlaps,
		RecheckInterval, true, 0.f);
}

void ADRBellBoyHatPickup::OnPickupOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
	TryGiveTo(OtherActor);
}

void ADRBellBoyHatPickup::RecheckOverlaps()
{
	TArray<AActor*> Overlapping;
	PickupSphere->GetOverlappingActors(Overlapping, ADRCharacterBase::StaticClass());
	for (AActor* Actor : Overlapping)
	{
		TryGiveTo(Actor);
		if (bConsumed) return;
	}
}

void ADRBellBoyHatPickup::TryGiveTo(AActor* Candidate)
{
	if (bConsumed || !HasAuthority()) return;

	ADRCharacterBase* Character = Cast<ADRCharacterBase>(Candidate);
	if (!IsValid(Character) || ICombatInterface::Execute_IsDead(Character)) return;

	// 적이든 아군이든 (사용자 확정). 던진 벨보이 본인이면 2회 (D-23)
	const int32 Charges = (Thrower.IsValid() && Character == Thrower.Get()) ? ThrowerCharges : 1;
	if (UDRAbilitySystemLibrary::TryEquipBellBoyHat(Character, ThrowerASC.Get(), Charges))
	{
		bConsumed = true;
		Destroy();
	}
	// 실패(이미 모자를 쓰고 있음) → 픽업은 그대로 남는다 (D-22)
}
```

### 2-6. 빌드

- 에디터가 닫힌 상태에서 Visual Studio 로 `DaeRune`(Development Editor) 빌드, 또는:
```
"D:\UE_5.5\Engine\Build\BatchFiles\Build.bat" DaeRuneEditor Win64 Development -Project="C:\Users\OWNER\Documents\Unreal Projects\DaeRune\DaeRune.uproject" -WaitMutex
```
- 모듈 의존성 추가 없음 — `GameplayAbilities` · `GameplayTags` · `GameplayTasks` · `AIModule` · `Niagara` 가 이미 `DaeRune.Build.cs` 에 있다.

✅ 확인
- 빌드 성공(경고 0 목표).
- 에디터를 열면 BP 부모 클래스 목록에 `DRBellBoyBasicAttack` · `DRBellBoyTaunt` · `DRBellBoyHatThrow` · `DRBellBoyHatProjectile` · `DRBellBoyHatPickup` 이 보인다.
- 게임플레이 태그 목록에 `Abilities.BellBoy.*` · `Event.Montage.BellBoy.*` · `Buff.BellBoy.Hat.*` · `Debuff.Taunted` 등이 보인다.
- 기존 로봇으로 한 판 — 아무것도 달라지지 않았다(회귀 없음).

---

## STEP 3. [에디터] 애니메이션 에셋

### 3-1. 스켈레톤 소켓 — `Bellboy_v0_1_0_Skeleton` 을 열고 본에서 우클릭 → Add Socket

| 소켓 | 부모 본 | 위치 | 쓰는 곳 |
|---|---|---|---|
| `BagSocket` | `Bag` | 가방 몸통 중앙 | `LeftHandSocketName` → `CombatSocket.LeftHand` → 기본 공격 판정 중심 |
| `CapSocket` | `Cap` | 모자 중앙 | `RightHandSocketName` → `CombatSocket.RightHand` → 모자 스폰 위치 |
| `HatSocket` | `Body_002` | 벨보이 자기 모자 꼭대기보다 살짝 위 | 모자 보호막 비주얼 부착점(`GetHatAttachPoint`) |
| `TestPartHand` | `Hand_003_R`(또는 두 손 사이) | 부품을 드는 위치 | C++ 이 `ThirdPersonPartMesh` 를 이 이름으로 붙인다(`DRCharacter.cpp` 생성자) — 없으면 부품이 엉뚱한 곳에 붙는다 |
| (선택) `MountSocket` | 루트 | 발바닥 | `MountAlignSocketName` — 청소기 위에 탈 때 높이. 정원로봇·자판기 스켈레톤과 같은 이름 |

- 소켓 미리보기: 소켓 우클릭 → Add Preview Asset → 모자 메시(STEP 8-1)를 붙여 `HatSocket` 높이를 맞춘다.
- (선택) 정원로봇·자판기·청소기 스켈레톤에도 `HatSocket` 을 머리 위에 추가하면 모자가 머리를 따라 움직인다. 안 해도 캡슐 꼭대기에 붙는다(D-26).

### 3-2. 애님 슬롯 — 같은 스켈레톤에서 Window → **Anim Slot Manager**
- `DefaultGroup` 에 **`UpperBody`** 슬롯을 추가한다(`DefaultSlot` 은 이미 있음). 슬롯 이름은 스켈레톤마다 따로라 벨보이에는 아직 없다.
- **같은 그룹(DefaultGroup)** 에 둔다 — 정원로봇과 같은 구조다. 같은 그룹이라 피격 몽타주(DefaultSlot)가 공격 몽타주를 끊는다(T-24).

### 3-3. 시퀀스 설정
- **루프 켜기**: `BB_Idle_1/2/3`, `BB_Walk_*`, `BB_Stun` (Asset Details → Loop). `BB_Jump`, 공격·도발·던지기·피격·사망은 끈 채로.
- **AimOffset 3장 가산 설정**: `BB_AimOffset_Up` / `_Forward` / `_Down` → Asset Details → Additive Settings
  - Additive Anim Type = **Mesh Space**
  - Base Pose Type = **Selected animation frame**, Base Pose Animation = `BB_AimOffset_Forward`, Frame = 0
  - (`Gardener_AimOffset_Front` 등의 설정을 열어 같은지 대조)

### 3-4. `AO_BB` — Aim Offset 1D
- `BellBoy/` 폴더에서 우클릭 → Animation → Blend Spaces → **Aim Offset 1D** → 스켈레톤 `Bellboy_v0_1_0_Skeleton` → 이름 `AO_BB`.
- 축: 이름 `AimPitch`, Min **−60**, Max **60**(= `AO_Gardener`).
- 샘플: −60 = `BB_AimOffset_Down`, 0 = `BB_AimOffset_Forward`, 60 = `BB_AimOffset_Up`.

### 3-5. `BS_BB_Walk` — Blend Space (2D)
- 축(= 정원로봇 `BS_Walk`): 가로 `MoveRight` −1~1, 세로 `MoveForward` −1~1.
- 샘플: (0, 1) `BB_Walk_Forward` · (0, −1) `BB_Walk_Backward` · (1, 0) `BB_Walk_Right` · (−1, 0) `BB_Walk_Left`. Idle 은 스테이트머신의 별도 상태라 넣지 않는다(정원로봇과 같음).

### 3-6. `ANS_BellBoy_HideCap` — Anim Notify State (BP)
- `Content/Blueprints/AnimNotifies/` → Blueprint Class → **AnimNotifyState** → `ANS_BellBoy_HideCap`.
- 변수 `BoneName`(Name, 기본값 `Cap`).
- 함수 오버라이드
  - **Received_NotifyBegin**: `Mesh Comp` → **Hide Bone by Name**(Bone Name = `BoneName`, Phys Body Option = No Physics Body) → Return true
  - **Received_NotifyEnd**: `Mesh Comp` → **Unhide Bone by Name**(`BoneName`) → Return true
- `Cap` 을 숨기면 자식(`CapButton` 등)도 같이 숨는다. 몽타주가 복제되므로 **모든 화면에서** 같이 숨는다.

### 3-7. 몽타주 6개 — 원본 애니 우클릭 → Create → **Create AnimMontage**

| 몽타주 | 원본 | 슬롯 | 노티파이 | Blend In / Out |
|---|---|---|---|---|
| `AM_BB_BasicAttack_R` | `BB_BasicAttack_Right` | `DefaultGroup.UpperBody` | **0.40s** `AN_MontageEvent` — Event Tag `Event.Montage.BellBoy.BasicAttack` (+ 선택: `AnimNotify_PlaySound` 휘두르는 소리) | 0.05 / 0.05 |
| `AM_BB_BasicAttack_L` | `BB_BasicAttack_Left` | `UpperBody` | **0.40s** 같은 노티파이 | 0.05 / 0.05 |
| `AM_BB_Taunt` | `BB_Taunt` | `UpperBody` | **1.00s** `AN_MontageEvent` — `Event.Montage.BellBoy.Taunt` | 0.15 / 0.25 |
| `AM_BB_ThrowCap` | `BB_ThrowCap` | `UpperBody` | **릴리스 프레임**(모자가 손을 떠나는 순간) `AN_MontageEvent` — `Event.Montage.BellBoy.HatThrow` + **`ANS_BellBoy_HideCap`**(릴리스 → 끝) | 0.1 / 0.15 |
| `AM_BB_HitReact` | `BB_Idle_HitReact` | `DefaultGroup.DefaultSlot` | — | 0.05 / 0.1 |
| `AM_BB_Death` | `BB_Death` | `DefaultSlot` | — | `AM_Gardener_Death` 설정을 그대로(자동 블렌드아웃 여부 포함 — 쓰러진 채 유지되는지) |

노티파이 다는 법 (기본 공격 예)
1. 몽타주를 열고 슬롯 트랙의 슬롯 이름을 `DefaultGroup.UpperBody` 로 바꾼다.
2. Notifies 트랙에서 0.40초 지점 우클릭 → Add Notify → **AN_MontageEvent**.
3. 노티파이를 선택하고 Details 의 **Event Tag** = `Event.Montage.BellBoy.BasicAttack`. 시각은 Details 의 Link → Time 으로 정확히 맞춘다.
4. Asset Details → Blend Option 에서 Blend In/Out 시간.
- ★**노티파이 위치 = 판정 시점**이다. 0.40초는 시작값이고, 가방이 실제로 닿는 프레임으로 옮겨도 된다 — **코드가 노티파이 시각을 직접 읽으므로 GA 값은 안 고친다**(D-5). 그래도 "선딜 0.4초"는 유지된다(재생 속도가 자동으로 맞춰진다).
- ★기본 공격 몽타주의 Blend Out 은 짧게(0.05~0.1) — 기본값 0.25 는 연속 공격의 0.1초 후딜을 다 잡아먹는다(T-4).
- `AN_MontageEvent` 는 `SendGameplayEventToActor(Owner, EventTag)` 를 부른다 — 정원로봇과 같은 노티파이를 그대로 쓴다.

✅ 확인
- 몽타주 에디터에서 재생해 보면 노티파이 위치가 가방이 닿는 순간 / 도발 포즈가 잡히는 순간 / 모자가 손을 떠나는 순간과 맞는다.
- `AM_BB_ThrowCap` 을 재생하면 릴리스 이후 머리의 모자가 사라지고, 끝나면 다시 보인다.
- `AO_BB` · `BS_BB_Walk` 미리보기에서 포즈가 자연스럽다.

---

## STEP 4. [에디터] `ABP_BellBoy`

### 4-1. 만들기
- `Content/Blueprints/Character/PlayerCharacter/BellBoy/` 폴더를 만들고 → 우클릭 → Animation → **Animation Blueprint** → 부모 `AnimInstance`, 스켈레톤 `Bellboy_v0_1_0_Skeleton` → `ABP_BellBoy`.

### 4-2. 이벤트 그래프 — 정원로봇 것을 복사
1. `ABP_Gardener` 의 EventGraph 를 전부 선택 → 복사 → `ABP_BellBoy` EventGraph 에 붙여넣기.
2. 컴파일 → 빨간 변수 노드에서 우클릭 → **Create variable** 로 변수를 만든다.
3. `Cast To BP_GardenRobot` 을 **`Cast To DRCharacter`** 로 바꾼다 — BP_BellBoy 는 STEP 5 에서 만들기 때문에(순환 의존 방지), 필요한 값은 전부 C++ 쪽에 있다.
4. 물대포 전용 노드(`bWaterPumpActive`)는 지운다.

| 변수 | 갱신 | 쓰는 곳 |
|---|---|---|
| `bShouldMove` | 수평 속도 > 3 && 가속 중 | Idle ↔ Walk |
| `MoveForward` / `MoveRight` | 로컬 속도 방향(−1~1) — 정원로봇 그래프 그대로 | `BS_BB_Walk` |
| `IsFalling` | CharacterMovement → Is Falling | Jump |
| `AimPitch` | 정원로봇 그래프 그대로(Base Aim Rotation 기준 — 다른 화면에서도 맞다) | `AO_BB` |
| `bIsStunned` | `DRCharacter.bIsStunned` | Stun |
| `bIsDead` / `IsMounted` / `bIsCarryingPart` | 정원로봇 그래프 그대로 | (지금은 연결할 자세 에셋 없음 — 부록 D) |

### 4-3. 애님 그래프
```
State Machine "Locomotion" ──► Save Cached Pose [Locomotion]

[Use Cached Pose: Locomotion] ──────────────────────────────► Layered blend per bone  (Base Pose)
[Use Cached Pose: Locomotion] ──► Slot 'DefaultGroup.UpperBody' ──► Layered blend per bone  (Blend Poses 0)
      Layer Setup[0] → Branch Filters:  { Bone Name = Body_001, Blend Depth = 0 }
                                        { Bone Name = Bag,      Blend Depth = 0 }   ★Bag 필수 (T-1)★
      Mesh Space Rotation Blend ✓
  ──► AimOffset Player (AO_BB, AimPitch = AimPitch)
  ──► Slot 'DefaultGroup.DefaultSlot'        ← 피격·사망(전신)
  ──► Output Pose
```
- 순서는 `ABP_Gardener` 와 같게 둔다(AO 위치가 다르면 정원로봇 쪽을 따른다).
- ★`Bag` 브랜치를 빠뜨리면 상체는 휘두르는데 가방은 걷기 포즈에 남는다 — `Bag` 본이 루트 직속이라서다(T-1).

**스테이트머신 "Locomotion"**

| 상태 | 내용 |
|---|---|
| `Idle` | **Random Player** — `BB_Idle_1/2/3`, Shuffle ✓ |
| `Walk` | **Blend Space Player** — `BS_BB_Walk`, X = `MoveRight`, Y = `MoveForward` |
| `Jump` | Sequence Player — `BB_Jump`, Loop ✗(마지막 프레임 유지) |
| `Stun` | Sequence Player — `BB_Stun`, Loop ✓ |

| 전이 | 조건 |
|---|---|
| Idle → Walk / Walk → Idle | `bShouldMove` / `NOT bShouldMove` |
| (Idle·Walk 별칭) → Jump | `IsFalling` |
| Jump → Idle | `NOT IsFalling` (블렌드 0.15) |
| (모든 상태 별칭) → Stun | `bIsStunned` |
| Stun → Idle | `NOT bIsStunned` |

- 정원로봇은 `Jump_Start/Loop/Land` 3개지만 벨보이는 `BB_Jump` 하나라 상태를 하나로 합쳤다(부록 D).

✅ 확인: 컴파일 경고 0, 미리보기에서 Idle 이 랜덤으로 바뀐다.

---

## STEP 5. [에디터] `BP_BellBoy` + 클래스 등록 + 1차 테스트

### 5-1. 캐릭터 BP — `BP_VendingMachine` 을 복제
- `BP_VendingMachine` 우클릭 → Duplicate → `BP_BellBoy` → `Content/Blueprints/Character/PlayerCharacter/BellBoy/` 로 이동.
- 자판기 BP 를 쓰는 이유: 커스텀 변수·전용 그래프가 없고 `PlayerCharacterClass` 를 명시하는 구조라 가장 깨끗하다(D-31). 오버헤드 닉네임 위젯·사망 카메라 타임라인·부품 메시·디버프 컴포넌트가 같이 따라온다.

| 항목 | 값 |
|---|---|
| Class Defaults → **Player Character Class** | **`BellBoy`** |
| Mesh(CharacterMesh0) → Skeletal Mesh | `Bellboy_v0_1_0` |
| Mesh → Anim Class | `ABP_BellBoy` |
| Mesh → Transform | Location Z = −(캡슐 반높이), Rotation Yaw = −90 (뷰포트의 파란 화살표 방향이 벨보이 정면이 되게) |
| Capsule | 반경·반높이를 벨보이 크기에 맞춤 |
| **FirstPersonMesh** | Skeletal Mesh **비움**, Anim Class **비움** (FP 없음 — D-3) |
| CameraBoom | Relative Location Z 를 벨보이 눈높이에 맞춤(PIE 에서 확인) |
| Facial Expression → Default Texture | `Bellboy_Face_Default` |
| Facial Expression → Blink / Heat / Death / IncreasedAttackSpeed Texture | **비움**(없음 — 비우면 현재 얼굴이 유지된다, 코드 확인) |
| Facial Expression → Face Material Index | 메시 머티리얼 슬롯 중 `BB_VC_Display_Face` 의 번호(메시 에디터에서 확인) |
| Facial Expression → Face Texture Parameter Name | 자판기 BP 값 그대로(모든 로봇 얼굴이 같은 glTF 부모라 같은 이름) |
| Combat → Left Hand Socket Name | `BagSocket` |
| Combat → Right Hand Socket Name | `CapSocket` |
| Combat → Hit React Montages | [`AM_BB_HitReact`] |
| Combat › Death → Death Montages | [`AM_BB_Death`] |
| Abilities → Startup Abilities / Startup Passive Abilities | **비움** — 플레이어는 DA 로만 부여된다(§2.3) |
| Upgrade Stat Effect Class / Mount Shared Damage Effect Class | 복제로 들어온 값 그대로(`GE_Upgrade_Stats` / `GE_Damage`) |
| (선택) Mount Align Socket Name | `MountSocket` (자판기 BP 에서 복제되어 이미 이 이름일 수 있다 — 벨보이 스켈레톤에 소켓만 만들면 된다) |

### 5-2. 클래스 등록 — `DA_PlayerCharacterClassInfo`
- **Character Class Information** → `+` → 키 `BellBoy`:

| 칸 | 값 |
|---|---|
| Primary Attributes | `GE_DRPrimaryAttributes` (정원로봇과 같음 — D-30) |
| Vital Attributes | `GE_VitalAttributes` |
| Startup Abilities | STEP 6~8 에서 하나씩 추가(`GA_BellBoy_BasicAttack` · `GA_BellBoy_Taunt` · `GA_BellBoy_HatThrow`) |
| Death Abilities | 다른 로봇과 같게(비어 있으면 비움) |
| Skill Icon Widget Class | STEP 9 에서 `WBP_SkillIcon_BellBoy` |
| Character Info Widget Class | (선택) STEP 9 |
| Crosshair Texture / Install Ready Texture / Size | 임시로 `Aim` / `GardenPartAim` / 32×32 (전용 텍스처는 부록 D) |

- **Character BP Classes** → `+` → `BellBoy` = `BP_BellBoy`.

### 5-3. (선택) 다른 로봇 스켈레톤에 `HatSocket`
- 모자를 받은 아군의 모자가 머리를 따라 흔들리게 하려면 정원로봇·자판기·청소기 TP 스켈레톤의 머리 본에 `HatSocket` 을 단다. 안 하면 캡슐 꼭대기(고정 높이)에 붙는다.

### 5-4. enum 이 바뀐 BP 위젯 3곳 (T-16)

| 위젯 | 노드 | 할 일 |
|---|---|---|
| `UI/Slot/WBP_PlayerSlot` | `Select`(Index = `SelectedClass`) → `Text_ClassName` | 새로 생긴 `BellBoy` 핀에 **"벨보이"** 를 다른 항목과 같은 형식으로 |
| `UI/Overlay/WBP_WaitingRoomOverlay` | `Switch on EPlayerCharacterClass`(`OnPlayerClassChanged`) | 새 `BellBoy` 실행 핀을 다른 클래스 핀과 같은 흐름에 연결(에셋이 클래스별이면 임시로 정원로봇 것) |
| `UI/Upgrade/WBP_UpgradeScreen` | `Select`(`GetViewedClass`) | `BellBoy` 옵션을 다른 클래스처럼 채움 |

- `WBP_UpgradeChip` · `WBP_UpgradeSlot` 도 enum 을 참조한다 — 열어서 **컴파일 경고가 없는지**만 본다.
- 컴파일 후 새 핀이 안 보이면 노드 우클릭 → Refresh Node.

### 5-5. 1차 테스트 (PIE · Listen Server · 플레이어 2)
- 대기실에서 클래스 넘기기로 **벨보이**가 나온다 · 슬롯 이름이 "벨보이" · 대기실 3인칭 모습과 Idle 이 정상.
- 스테이지 진입 → 걷기(4방향) · 점프 · 위아래 조준(다른 플레이어 화면에서 상체가 따라 움직임) · 스턴 · 피격 · 사망 모습.
- 내 화면은 1인칭인데 팔이 안 보인다 — 정상(D-3).
- 부품을 들면 부품이 손 근처에 붙는다(`TestPartHand`).

✅ 확인: 위 항목 전부. 아직 스킬은 없다(LMB/E/RMB 무반응이 정상).

---

## STEP 6. [에디터] 기본 공격

폴더: `Content/Blueprints/AbilitySystem/Player/BellBoy/Abilities/Skill1/`

### 6-1. `GE_BellBoy_BasicAttackRecovery` (Blueprint Class → GameplayEffect)

| 칸 | 값 |
|---|---|
| Duration Policy | Has Duration |
| Duration Magnitude | **Set by Caller** — Data Tag `Data.Duration` |
| Components | + **Block Abilities with Tags** → `Abilities.BellBoy.Taunt`, `Abilities.BellBoy.HatThrow` |
| Stacking | None (스윙마다 새로 걸리고, 겹쳐도 가장 늦게 끝나는 것까지 막힌다) |
| Modifiers / Cues | 없음 |

### 6-2. `GA_BellBoy_BasicAttack` (Blueprint Class → 부모 **DRBellBoyBasicAttack**)

| 칸 | 값 | 비고 |
|---|---|---|
| Ability Tags(Asset Tags) | `Abilities.BellBoy.BasicAttack` | 첫 `Abilities.*` 태그 = 칩 키 |
| Activation Blocked Tags | `State.Carrying`, `Effects.HitReact` | 다른 로봇과 같음 |
| Block Abilities with Tag | `Abilities.BellBoy.Taunt`, `Abilities.BellBoy.HatThrow` | 휘두르는 동안 |
| Startup Input Tag | `InputTag.LMB` | |
| Water Cost / Cost GE | 0 / 없음 | |
| Damage Effect Class | `GE_Damage` | |
| Damage Type | `Damage.Physical` | |
| Damage | **30** | |
| Debuff Chance | **0** | C++ 기본 20 (T-3) |
| Death Impulse Magnitude | 0~300 (취향) | 발톱은 0 |
| Knockback Force Magnitude | **400** | 약한 넉백 (D-9) |
| Attack Radius | **90** | 디버그 구로 조정 |
| Swing Montages | [`AM_BB_BasicAttack_R`, `AM_BB_BasicAttack_L`] | 순서 = 오른쪽 먼저 |
| Hit Event Tag | `Event.Montage.BellBoy.BasicAttack` | |
| Fallback Hit Time | 0.4 | 노티파이가 있으면 안 쓰인다 |
| First Windup / First Recovery | 0.4 / 0.4 | |
| Combo Windup / Combo Recovery | 0.2 / 0.1 | |
| Combo Reset Time | 0.25 | |
| Skill Switch Recovery Time | 0.4 | |
| Skill Switch Lock Effect | `GE_BellBoy_BasicAttackRecovery` | |
| Hit Window Duration / Samples | 0.1 / 4 | 0 이면 정원로봇식 1회 |
| Hit Socket Tag | `CombatSocket.LeftHand` | → `BagSocket` |
| Draw Debug Hit | 처음엔 ✓, 검증 후 ✗ | |

- `DA_PlayerCharacterClassInfo` → BellBoy → Startup Abilities 에 `GA_BellBoy_BasicAttack` 추가.

### 6-3. 테스트
- [ ] LMB 한 번 → 0.4초 뒤 타격, 0.8초에 끝(오른쪽 스윙).
- [ ] LMB 누르고 있기 → 0.8초 뒤부터 0.3초 간격, 오른쪽·왼쪽 교대. 손을 떼고 0.25초 넘게 쉬면 다시 느린 첫 타.
- [ ] 적 피해 30 · 살짝 뒤로 튐 · 한 스윙에 같은 적 1번만 · 여러 적 동시 타격.
- [ ] 디버그 구가 가방을 따라 4번 찍힌다(주황) — 반경 조정.
- [ ] 연타 중 E/RMB → 마지막 타격 0.4초 뒤에야 발동, 그동안 스킬 아이콘이 '차단' 표시.
- [ ] 벽 앞의 적을 치면 벽꿍 스턴이 날 수 있다(T-13 — 의도 확인).
- [ ] 출력 로그에 `[BellBoy] … 노티파이를 못 찾음` 이 없어야 한다.

---

## STEP 7. [에디터] 시선 집중

폴더: `Content/Blueprints/AbilitySystem/Player/BellBoy/Abilities/Skill2/`

### 7-1. `GE_Cost_BellBoy_Taunt`
- `GardenRobot/Abilities/Skill3/GE_Cost_SeedCannon` 을 복제 → 이름 변경 → 이 폴더로(Instant + Execution `ExecCalc_WaterCost` — 고칠 것 없음).

### 7-2. `GE_BellBoy_Taunted`

| 칸 | 값 |
|---|---|
| Duration Policy | Has Duration |
| Duration Magnitude | Set by Caller — `Data.Duration` |
| Modifiers[0] | Attribute `DRAttributeSet.MoveSpeed` · Modifier Op **Multiply (Compound)** · Set by Caller `Data.MoveSpeed.Mult` |
| Components | + **Grant Tags to Target Actor**(Target Tags) = `Debuff.Taunted` (T-10) · + **Gameplay Cues** = `GameplayCue.BellBoy.Taunted`(7-3 을 만들 때) |
| Stacking | None (C++ 이 새로 걸기 전에 이전 것을 지운다 — D-15) |

- ★Modifier Op 을 `Multiply (Additive)` 로 두지 말 것 — 여러 배율이 더해진다(Plan5 T-8).

### 7-3. (선택) `GC_BellBoy_Taunted` — 도발된 적 머리 위 "!"
- `Content/Blueprints/AbilitySystem/GameplayCueNotifies/` → Blueprint Class → **GameplayCueNotify_Actor** → Gameplay Cue Tag `GameplayCue.BellBoy.Taunted`, Auto Destroy On Remove ✓.
- 컴포넌트: 빨간 "!" (Niagara 또는 Text Render / 빌보드).
- **While Active**: `Get Hat Attach Point(My Target)` → `Attach Actor To Component`(반환 컴포넌트, 소켓, Snap to Target) → `Set Actor Relative Location`(오프셋 + Z 60).
- On Remove: 숨김(자동 파괴).

### 7-4. `GA_BellBoy_Taunt` (부모 **DRBellBoyTaunt**)

| 칸 | 값 | 비고 |
|---|---|---|
| Ability Tags | `Abilities.BellBoy.Taunt` | |
| Activation Blocked Tags | `State.Carrying`, `Effects.HitReact` | |
| Block Abilities with Tag | `Abilities.BellBoy.BasicAttack`, `Abilities.BellBoy.HatThrow`, **`Effects.HitReact`** | 마지막 것 = 슈퍼아머(D-18) |
| Startup Input Tag | `InputTag.E` | |
| Water Cost | **60** | |
| Cost Gameplay Effect Class | `GE_Cost_BellBoy_Taunt` | |
| Taunt Montage | `AM_BB_Taunt` | |
| Taunt Start Event Tag | `Event.Montage.BellBoy.Taunt` | |
| Taunt Radius | 3000 | 30m |
| Detect Window / Detect Interval | 0.5 / 0.1 | 6회 |
| Taunt Duration | **5** | 걸린 순간부터 |
| Taunted Move Speed Multiplier | 1.1 | |
| Taunted Damage Multiplier | 0.8 | |
| Taunted Effect | `GE_BellBoy_Taunted` | |
| Require Line Of Sight | ✗ | |
| Taunt Start Cue Tag | (선택) | 외침·충격파 연출 |

- `DA_PlayerCharacterClassInfo` → BellBoy → Startup Abilities 에 추가.
- `GA_HitReact` 의 AbilityTags 는 `Effects.HitReact` 다(에셋 확인) — 그래서 Block 목록에 이 태그를 넣으면 도발 중 피격 경직이 막힌다.

### 7-5. AI 연결 — BT 는 거의 그대로 (C++ 가 처리)

- 도발은 C++ `ApplyTauntAggro` + `StartTauntAggroRefresh`(STEP 1-5, 2-2)가 처리한다. **일반 적의 BT·서비스는 고치지 않는다.**
  - BT 는 `TargetToFollow` 가 있으면 그 플레이어를, 없으면 `TargetCleanserSite` 를 쫓는다 → 사이트를 치던 적도 도발 순간 벨보이로 돌아선다.
  - `BTS_FindNearestPlayer_Dog`/`_DragonFly` 는 `HasFirstAttacker = true` 면 아무것도 안 한다 → 타깃을 덮어쓰지 않는다.
  - `BTS_CheckValidTarget`(계열)의 "멀고 3초간 전투 없음" 해제는 0.2초 주기 전투 시간 갱신으로 막힌다. 죽음·1.5초 도달 불가 해제는 그대로 동작한다(D-16 / T-8).
- **두더지 보스(`BTS_FindNearestPlayer_Mole`)** — 움직이지 않고, 튀어나오기(스킬1)는 기존처럼 **무작위 플레이어**, 기본 공격은 `TargetToFollow`(가장 가까운 플레이어)에게 한다.
  도발로 `TargetToFollow` 를 벨보이로 바꾸면 기본 공격이 벨보이에게 간다. 단 이 서비스는 `HasFirstAttacker` 를 보지 않고 매 틱 가장 가까운 플레이어로 덮어쓰므로(§2.4), **서비스에 분기 1개를 넣는다(권장)**:
```
Event Receive Tick AI (Owner Controller, Controlled Pawn, Delta Seconds)
   │
   ▼
Get Taunt Target (Controlled Pawn) ─► Is Valid ── True ──► (아무것도 연결하지 않는다 — 원래 로직 건너뜀)
                                               └─ False ─► [원래 첫 노드]
```
  - `Get Taunt Target` 은 C++ BlueprintPure 노드(카테고리 `DRAbilitySystemLibrary|BellBoy`). 도발 중이면 벨보이, 아니면 None.
  - 안 넣으면: C++ 의 0.2초 재기록과 서비스의 덮어쓰기가 번갈아 일어나, 기본 공격이 가끔 가까운 다른 플레이어에게 간다(T-8).
- 두더지 보스는 도발돼도 움직이지 않으므로 이속 +10% 는 의미가 없다. 주는 피해 −20% 는 적용된다.

### 7-6. (결번 — 페이즈3 BT 폴백은 필요 없어졌다: BT 가 `TargetToFollow` 를 사이트보다 먼저 본다)

### 7-7. 테스트
- [ ] 물 60 소모 · 도발 모션 5초 · **모션 중 걸을 수 있다** · 모션 중 LMB/RMB 불가(아이콘 차단) · **맞아도 모션이 안 끊긴다**.
- [ ] 1초 뒤 30m 안 적 전원이 벨보이 쪽으로(더 가까운 아군이 있어도) · 0.5초 판정 동안 새로 들어온 적도 걸린다 · 1.5초 이후 들어온 적은 안 걸린다.
- [ ] 도발된 적 이속 +10%(지상 적 — 잠자리는 T-12) · 도발된 적이 **누구를 때려도** 피해 ×0.8 · 도발 안 된 적은 그대로.
- [ ] 적마다 걸린 뒤 5초에 풀린다(모션이 끝난 뒤에도 잠깐 유지).
- [ ] 벨보이가 죽으면 적들이 곧바로(≤ 서비스 주기) 다른 대상을 찾는다.
- [ ] 부품 도둑은 도발되지 않는다 / 아르마딜로·잠자리·개 모두 끌려온다.
- [ ] 페이즈3 에서 클렌저 사이트를 치던 적도 벨보이에게 끌려오고, 도발이 끝나면 멀리 있는 적은 3초쯤 뒤 사이트로 돌아간다.
- [ ] 도발 중 다른 플레이어가 그 적을 때려도 타깃이 바뀌지 않는다.
- [ ] 두더지 보스: 튀어나오는 위치는 여전히 무작위 · 기본 공격은 도발 5초 동안 벨보이에게만.

---

## STEP 8. [에디터] 모자 던지기

폴더: `Content/Blueprints/AbilitySystem/Player/BellBoy/Abilities/Skill3/` (GameplayCue 는 `GameplayCueNotifies/`)

### 8-1. 모자 메시
- **정식**: 아티스트에게 모자 단독 스태틱 메시 `SM_BellBoy_Cap` 요청(부록 D).
- **임시**: `/Engine/BasicShapes/Cylinder` + 스케일 (0.30, 0.30, 0.12) + 머티리얼 `BB_Red_001`. (또는 모델링 모드로 `Bellboy_v0_1_0` 의 모자 삼각형만 떼어 스태틱 메시로 만든다 — 고급)

### 8-2. `GE_BellBoy_Hat_Single` / `GE_BellBoy_Hat_Double`

| 칸 | Single | Double |
|---|---|---|
| Duration Policy | **Infinite** | Infinite |
| Components → Grant Tags to Target Actor | `Buff.BellBoy.Hat.Single` | `Buff.BellBoy.Hat.Double` |
| Components → Gameplay Cues | `GameplayCue.BellBoy.Hat.Single` | `GameplayCue.BellBoy.Hat.Double` |
| Stacking | None | None |
| Modifiers | 없음 | 없음 |

- ★태그는 **Target Tags(Grant Tags to Target Actor)** 컴포넌트에 — Asset Tags 에 넣으면 대상에 안 붙는다(T-10).

### 8-3. `GC_BellBoy_Hat_Single` / `GC_BellBoy_Hat_Double` — 머리 위 모자
- Blueprint Class → **GameplayCueNotify_Actor** → `GC_BellBoy_Hat_Single`

| Class Defaults | 값 |
|---|---|
| Gameplay Cue Tag | `GameplayCue.BellBoy.Hat.Single` |
| Auto Destroy On Remove / Auto Destroy Delay | ✓ / 0.5 |
| Unique Instance Per Instigator / Per Source Object | ✗ / ✗ |
| Allow Multiple While Active Events | ✗ |

- 컴포넌트: Static Mesh `HatMesh`(모자 메시, Collision **NoCollision**, **Owner No See ✓**(T-11), Cast Shadow ✓)
- 이벤트 그래프
```
Event While Active (My Target, Parameters)          ← OnActive 가 아니라 While Active (늦게 들어온 플레이어도 보이게)
  → Set Owner (My Target)                            ← OwnerNoSee 안전장치(엔진도 대상을 Owner 로 두지만 재활용 대비)
  → Get Hat Attach Point (Target = My Target)        → Return Value(부모 컴포넌트) · Out Socket Name · Out Relative Offset
  → Attach Actor To Component (Parent, Socket Name, Location = Snap to Target, Rotation = Snap to Target, Scale = Keep World)
  → Set Actor Relative Location (Out Relative Offset)
  → HatMesh → Set Visibility (true)                 (선택: 0.15초 "뿅" 스케일 타임라인)
Event On Remove (My Target, Parameters)
  → Spawn System at Location (팝 이펙트, HatMesh 위치) + Play Sound at Location
  → HatMesh → Set Visibility (false)
```
- `GC_BellBoy_Hat_Double`: `GC_BellBoy_Hat_Single` 우클릭 → **Create Child Blueprint Class** → Gameplay Cue Tag = `GameplayCue.BellBoy.Hat.Double` → `HatMesh` 머티리얼을 금색 등으로(2회짜리 구분).
- Double → Single 로 바뀔 때는 서로 다른 큐라 "금색 모자가 팝 → 빨간 모자가 나타남"으로 보인다(D-20).

### 8-4. `GC_BellBoy_HatBlock` — 막는 순간
- Blueprint Class → **GameplayCueNotify_Burst** → Tag `GameplayCue.BellBoy.HatBlock`.
- Burst Effects: 스파크 Niagara + "팅" 소리. 위치는 C++ 이 넘기는 `Location`(대상 중심) — 머리 높이로 올리려면 배치 오프셋 Z.

### 8-5. `DA_GameBalanceConfig` (`Content/Blueprints/Game/Data/`)
- Character → **Bell Boy** → Hat Single Effect = `GE_BellBoy_Hat_Single`, Hat Double Effect = `GE_BellBoy_Hat_Double`.
- ★비우면 모자가 **에러 없이** 무동작한다(경고 로그만 — `[BellBoy] DA_GameBalanceConfig.BellBoy 의 모자 GE 가 비어 있다`).

### 8-6. `BP_BellBoyHatPickup` (부모 **DRBellBoyHatPickup**)

| 칸 | 값 |
|---|---|
| HatMesh | 모자 메시 — 바닥에 놓인 모양이 되게 회전·스케일 |
| PickupSphere Radius | 60 |
| Life Time / Recheck Interval / Thrower Charges | 10 / 0.25 / 2 |
| (선택) Rotating Movement 컴포넌트 | Yaw 90 — 바닥에서 천천히 도는 연출 |
| (선택) 사라지기 3초 전 깜빡임 | BeginPlay → Delay(LifeTime − 3) → Timeline 으로 HatMesh 가시성 토글 |

### 8-7. `BP_BellBoyHatProjectile` (부모 **DRBellBoyHatProjectile**)

| 칸 | 값 |
|---|---|
| Sphere Radius | 25 (모자 크기) |
| HatMesh | 모자 메시(챙이 수평이 되게) |
| Projectile Movement — Initial / Max Speed | 1400 / 1400 |
| Projectile Movement — Gravity Scale | 0.35 |
| Projectile Movement — Rotation Follows Velocity | ✗ |
| Spin — Rotation Rate | (0, 720, 0) |
| Pickup Class | `BP_BellBoyHatPickup` |
| Life Span | 6 (허공으로 날아가면 조용히 사라짐) |
| Impact Effect / Impact Sound | 가벼운 "퐁" — 클라에서는 장착·픽업 전환 때도 재생될 수 있다(T-21) |
| Looping Sound | (선택) 휙휙 |

### 8-8. `GE_Cost_BellBoy_HatThrow`
- `GE_Cost_SeedCannon` 복제(고칠 것 없음).

### 8-9. `GA_BellBoy_HatThrow` (부모 **DRBellBoyHatThrow**)

| 칸 | 값 |
|---|---|
| Ability Tags | `Abilities.BellBoy.HatThrow` |
| Activation Blocked Tags | `State.Carrying`, `Effects.HitReact` |
| Block Abilities with Tag | `Abilities.BellBoy.BasicAttack`, `Abilities.BellBoy.Taunt` |
| Startup Input Tag | `InputTag.RMB` |
| Water Cost / Cost GE | **20** / `GE_Cost_BellBoy_HatThrow` |
| Projectile Class | `BP_BellBoyHatProjectile` |
| Damage Effect Class / Damage Type / Damage | `GE_Damage` / `Damage.Physical` / **30** |
| Debuff Chance / Knockback Chance | **0** / 0 |
| Throw Montage | `AM_BB_ThrowCap` |
| Release Event Tag | `Event.Montage.BellBoy.HatThrow` |
| Throw Socket Tag | `CombatSocket.RightHand` (→ `CapSocket`) |

- `DA_PlayerCharacterClassInfo` → BellBoy → Startup Abilities 에 추가.

### 8-10. 테스트
- [ ] 물 20 · 크로스헤어 방향으로 빙글빙글 날아간다 · 던지는 순간부터 벨보이 머리 모자가 사라졌다가 모션 끝에 돌아온다.
- [ ] 적 적중 → 30 피해 후 모자 소멸.
- [ ] 아군 적중 → 아군 머리에 모자(다른 화면에서 보임) · 아군 본인 화면에는 모자가 안 보이고 HUD 에 "모자 보호" 아이콘(STEP 9 후).
- [ ] 모자를 쓴 아군에게 또 던지면 → 부딪혀 **그 아군 발밑에** 픽업이 생긴다.
- [ ] 벽 → 벽 아래 바닥에 픽업 / 바닥 → 떨어진 자리에 픽업 / 10초 뒤 사라진다.
- [ ] 적이 픽업을 밟으면 적 머리에 모자 → 그 적을 때리면 첫 타 피해 0 + 팝 연출, 둘째 타부터 정상.
- [ ] 벨보이가 자기 모자를 주우면 금색 모자(2회) → 두 번 막는다(첫 번째 뒤 빨간 모자로 바뀜).
- [ ] 200 짜리 피해도 0 · 화상 틱도 모자를 소모(T-18).
- [ ] 모자를 쓴 채 픽업 위에 서 있다가 한 대 맞아 모자를 잃으면 → 0.25초 안에 그 픽업을 자동으로 줍는다.
- [ ] 트리거 볼륨(문 센서 등)을 지나도 모자가 떨어지지 않는다.

---

## STEP 9. [에디터] UI

### 9-1. `DA_AbilityInfo` — 3줄 추가
- 기존 항목 하나를 복사해 `AbilityType` · `BackgroundMaterial` 을 같게 두고:

| Ability Tag | Icon(임시) | Ability |
|---|---|---|
| `Abilities.BellBoy.BasicAttack` | `UI/SkillIcon/GardenRobot/Icon_Skill1-2` | `GA_BellBoy_BasicAttack` |
| `Abilities.BellBoy.Taunt` | `…/Icon_Skill2-2` | `GA_BellBoy_Taunt` |
| `Abilities.BellBoy.HatThrow` | `…/Icon_Skill3-2` | `GA_BellBoy_HatThrow` |

### 9-2. `WBP_SkillIcon_BellBoy`
- `UI/SkillIcon/WBP_SkillIcon_GardenRobot` 복제 → 디자이너에서 각 `WBP_SkillSlot` 인스턴스의 값만 바꾼다:

| 슬롯 | Ability Tag | Input Tag |
|---|---|---|
| `WBP_SkillSlot_LMB` | `Abilities.BellBoy.BasicAttack` | `InputTag.LMB` |
| `WBP_SkillSlot_RMB` | `Abilities.BellBoy.HatThrow` | `InputTag.RMB` |
| `WBP_SkillSlot_E` | `Abilities.BellBoy.Taunt` | `InputTag.E` |

- `DA_PlayerCharacterClassInfo` → BellBoy → Skill Icon Widget Class = `WBP_SkillIcon_BellBoy`.

### 9-3. `DA_StatusEffectInfo` — 2줄 추가

| Effect Tag | Effect Name | Icon |
|---|---|---|
| `Buff.BellBoy.Hat.Single` | 모자 보호 | 모자 아이콘(임시 아무거나) |
| `Buff.BellBoy.Hat.Double` | 모자 보호 ×2 | 금색 모자 아이콘 |

- Infinite GE 라 시간 표시 없이 떠 있다가, 모자가 소모되면 사라진다(Double → Single 은 아이콘이 바뀐다).

### 9-4. (선택) 캐릭터 설명(Tab) — `WBP_BellBoyInfo`
- `UI/CharacterInfo/WBP_VendingInfo` 복제 → 텍스트를 벨보이 스킬 설명으로 → `DA_PlayerCharacterClassInfo` 의 Character Info Widget Class.

### 9-5. 현지화
- 새 위젯 텍스트("벨보이", 스킬 설명, "모자 보호")는 **Localizable** 로 둔다(한/영 현지화 파이프라인 — Localization Dashboard 작업은 별도).

✅ 확인: 벨보이로 들어가면 하단 스킬 아이콘 3개(LMB/RMB/E) · 스킬을 쓰면 아이콘이 눌림/차단 표시 · 모자를 받으면 상태 아이콘.

---

## STEP 10. 검증 체크리스트

### 10.1 싱글(PIE Standalone 또는 Listen Server 1인)
- [ ] STEP 5-5 / 6-3 / 7-7 / 8-10 의 항목 전부
- [ ] 물이 부족하면 체력으로 대납되는 기존 규칙이 E/RMB 에도 적용된다(다른 로봇과 같음)
- [ ] 부품을 들고 있으면 세 스킬 모두 안 나간다(`State.Carrying`)
- [ ] 스턴 중에는 입력이 막힌다(다른 로봇과 같음)

### 10.2 멀티(PIE Listen Server + 클라 2)
- [ ] **클라 벨보이**의 연속 공격이 **호스트 화면에서도 같은 속도**(0.3초 간격)로 보인다 — 프록시 재생 속도 복제(D-10)
- [ ] 클라 벨보이의 타격 판정이 클라 화면의 타격 모션과 맞는다(연속 경계에서 가끔 어긋나면 T-5 — `ComboResetTime` 을 늘린다)
- [ ] 클라가 던진 모자가 모든 화면에서 같은 궤적 · 픽업이 모든 화면에 보인다 · 모자를 받은 사람의 머리 모자가 **본인 외** 화면에 보인다
- [ ] 늦게 들어온(재접속) 플레이어도 이미 모자를 쓴 사람의 모자가 보인다(While Active)
- [ ] 도발 "!" 마커 · 모자 팝 연출이 모든 화면에
- [ ] 벨보이 2명: 같은 적을 둘이 도발하면 **나중에 건 벨보이**를 쫓는다(D-15) · A 가 던진 모자를 B 가 주우면 1회(D-23)

### 10.3 회귀
- [ ] 정원로봇·자판기·청소기의 공격·피해·스킬이 그대로
- [ ] 도발이 없을 때 적 AI(추적·사이트 공격·잠자리 비행·부품 도둑 도주)가 그대로 — 도발 GE 가 없으면 C++ 은 블랙보드를 건드리지 않는다
- [ ] 적이 주는 피해·플레이어 컨테이너·오염·탑승 공유 피해 그대로(피해 관문 변경)
- [ ] 업그레이드 화면: 벨보이 탭에 공용 칩만 보인다 · 다른 로봇 탭 그대로
- [ ] `DA_ProgressionConfig` 를 열어 검사 경고 확인(로봇 4개 → 슬롯 해금 총비용 증가, T-14)

✅ 전부 통과하면 이 문서를 Plan5 처럼 "완료된 기능의 참조 문서"로 압축한다(최종 수치 · D · T · 남은 이슈만 남기고 단계별 매뉴얼은 걷어낸다).

---

## STEP 11. (나중에) FP 에셋이 오면

1. **에셋**: FP 메시(팔 + 가방) · FP 애니(기본 공격 L/R · 도발 · 모자 던지기 · Idle/Walk/Jump/Stun/HitReact) → FP 몽타주(`DefaultSlot`) · `ABP_FP_BellBoy`(정원로봇 `ABP_FP_Gardener` 참고).
2. **BP_BellBoy**: `FirstPersonMesh` = FP 메시 + `ABP_FP_BellBoy`. `FirstPersonPartMesh` 용 `TestPartHand` 소켓을 FP 스켈레톤에도.
3. **C++ (작은 추가)**
   - `UDRBellBoyBasicAttack` 에 `FirstPersonSwingMontages`(`SwingMontages` 와 같은 순서) → 소유 클라에서 `FirstPersonMesh` 의 AnimInstance 로 같은 `WindupRate` 로 재생, `SetSwingPlayRate` 에서 FP 몽타주도 같은 속도로(정원로봇 `RestoreMontagePlayRate` 가 1P·3P 둘 다 바꾸는 것과 같은 이유 — Plan5 T-26).
   - 도발·모자 GA 에도 FP 몽타주 1개씩(소유 클라 재생만, 판정 없음).
   - 모자 스폰 위치를 1인칭 기준으로 바꾸고 싶으면 청소기처럼 `GetCombatSocketLocation_Implementation` 을 FP 소켓으로 라우팅(`ADRRobotVacuumCharacter` 선례 — 이때는 벨보이 전용 C++ 캐릭터가 필요해진다).
4. **FP 에서도 모자 숨김**: `ANS_BellBoy_HideCap` 을 FP 던지기 몽타주에도.

---

## 부록 A. 수치 튜닝 창구 — "어디서 바꾸나"

| 수치 | 현재 | 바꾸는 곳 |
|---|---|---|
| 기본 공격 피해 / 반경 / 넉백 | 30 / 90 / 400 | `GA_BellBoy_BasicAttack` — Damage / Attack Radius / Knockback Force Magnitude |
| 첫 타·연속 선딜/후딜 | 0.4·0.4 / 0.2·0.1 | 〃 First/Combo Windup·Recovery Time |
| 연속 판정 유예 | 0.25 | 〃 Combo Reset Time |
| 스킬 전환 후딜 | 0.4 | 〃 Skill Switch Recovery Time |
| 판정 창 / 횟수 | 0.1 / 4 | 〃 Hit Window Duration / Samples |
| 타격 시점 | 0.40s | **몽타주 노티파이 위치**(코드가 읽는다) |
| 도발 물 / 반경 / 판정 창·간격 | 60 / 3000 / 0.5·0.1 | `GA_BellBoy_Taunt` |
| 도발 지속 / 이속 / 피해 배율 | 5 / 1.1 / 0.8 | 〃 Taunt Duration / Taunted Move Speed Multiplier / Taunted Damage Multiplier |
| 도발 선딜 | 1.0s | `AM_BB_Taunt` 노티파이 위치 |
| 모자 물 / 피해 | 20 / 30 | `GA_BellBoy_HatThrow` |
| 모자 속도 / 중력 / 회전 | 1400 / 0.35 / 720 | `BP_BellBoyHatProjectile` |
| 픽업 유지 / 본인 횟수 | 10 / 2 | `BP_BellBoyHatPickup` |
| 모자 GE | Single / Double | `DA_GameBalanceConfig` → Bell Boy |
| 키 | LMB / E / RMB | 각 GA 의 Startup Input Tag (+ 스킬 아이콘 위젯 슬롯) |

---

## 부록 B. 결정 (D-n)

| # | 결정 | 근거 |
|---|---|---|
| **D-1** | 벨보이 전용 C++ 캐릭터 클래스를 만들지 않는다 — `BP_BellBoy` 는 `ADRCharacter` 직속 | 정원로봇·자판기와 같다. 벨보이 고유 상태(콤보·도발·모자)는 전부 GA·GE·액터가 가진다. FP 소켓 라우팅이 필요해지면 그때 만든다(STEP 11) |
| **D-2** | `EPlayerCharacterClass::BellBoy` 를 `Count` 바로 앞에 | 중간에 끼우면 저장된 enum 값이 밀린다(Plan7 `MoleBoss` 와 같은 규칙) |
| **D-3** | FP 에셋 없이 진행 — FP 메시 비움, 판정·연출은 3P 몽타주 | 사용자 확정(2026-09-29). 에셋이 오면 STEP 11 |
| **D-4** | 선딜/후딜 = 몽타주 재생 속도를 **타격 전·후로 따로** 준다 (`선딜속도 = 타격시각 / 선딜초`, `후딜속도 = (길이 − 타격시각) / 후딜초`) | Plan5 D-20 의 확장. 몽타주 앞 `Delay` 는 가만히 섰다가 휘둘러 부자연스럽고, 한 속도로는 "선 0.2 / 후 0.1" 을 동시에 못 맞춘다 |
| **D-5** | 타격 시각은 몽타주의 `AN_MontageEvent` 노티파이 시각을 **코드가 반사로 읽는다**(없으면 `FallbackHitTime` + 경고) | "노티파이 위치 = 판정 기준"(요구). 노티파이를 옮길 때 GA 값을 같이 고치는 일이 없어진다 |
| **D-6** | "연속" = 앞 스윙 종료 후 `ComboResetTime`(0.25s) 안에 시작. 연속은 좌·우 교대, 끊기면 첫 타(오른쪽) | 누르고 있기·연타 모두 자연스럽게 연속이 된다. 끊김(피격·스턴)도 첫 타로 |
| **D-7** | "다른 스킬로 넘어가면 후딜이 길어진다" = 스윙 시작 때 **Block Abilities with Tags GE** 를 (선딜 + 0.4초) 건다 | 엔진 GE 컴포넌트라 `BlockedAbilityTags` 를 통해 스킬 아이콘 차단 UI 가 자동으로 뜬다. 활성화 순간에 걸어 예측 키가 살아 있다 |
| **D-8** | 판정 창 0.1초 = 서버가 0.025초 간격 4회 구 오버랩, 한 스윙에 대상당 1회. `HitWindowDuration = 0` 이면 발톱처럼 1회 | 가방은 호를 그리며 지나가고 연속 공격은 ×4 속도라 한 번 찍으면 호 가장자리 적을 놓친다. 요구의 "0.1초 열어 둠"을 그대로 구현하고 발톱식으로 되돌릴 손잡이도 둔다 |
| **D-9** | 넉백은 기존 파이프라인 그대로(`KnockbackForceMagnitude`, 45° 위) — 약하게 400 | 요구 "현재 있는 넉백 코드 그대로". 발톱은 0 이다 |
| **D-10** | 후딜 속도 전환은 서버 `ASC::CurrentMontageSetPlayRate`(프록시 복제), 소유 클라는 로컬 AnimInstance | 발톱의 `RestoreMontagePlayRate` 는 AnimInstance 를 직접 바꿔 시뮬레이티드 프록시에 복제되지 않는다. 클라에서 ASC 함수를 쓰면 서버 RPC 가 중복된다 |
| **D-11** | 도발 효과 = 도발된 적의 **이속 ×1.1** + 그 적이 **주는 피해 ×0.8(맞는 대상 무관)** | 사용자 확정(원문 괄호 "도발 안 된 적의 피해·이속은 그대로") |
| **D-12** | 도발 지속 = **걸린 순간부터 5초** — 어빌리티가 끝나도 유지. 벨보이가 죽으면 AI 관문이 발견하는 즉시 해제 | 사용자 확정. 죽은 도발자를 쫓는 적이 남지 않게 |
| **D-13** | 도발 상태 = GE(`Debuff.Taunted` + 이속 모디파이어 + SetByCaller 피해 배율). 도발자 = 컨텍스트 `OriginalInstigatorASC` 의 아바타 | 지속·만료를 GAS 가 관리하고, 배율을 스펙이 들고 다녀 나중에 칩이 시전 시점에 바꿀 수 있다 |
| **D-14** | 도발 피해 배율은 `UDRAttributeSet::PostGameplayEffectExecute`(플레이어·적 공용 관문) + 클렌저 사이트 AS 에서 곱한다 | Plan5 D-10 과 같은 이유 — 디버프 DoT 은 `ExecCalc_Damage` 를 안 탄다. 사이트 AS 는 공용 관문을 상속하지 않는다 |
| **D-15** | 새 도발은 기존 도발 GE 를 지우고 건다(마지막 벨보이가 이김). 한 시전에 적당 1회 | 이속 모디파이어가 두 번 곱해지지 않고, 도발자가 늘 하나로 정해진다 |
| **D-16** | AI 연결 = C++ `ApplyTauntAggro` 가 `TargetToFollow`·`FirstAttacker` = 벨보이, `HasFirstAttacker` = true 를 쓰고 전투 시간을 갱신한다. 도발 GE 가 있는 동안 `StartTauntAggroRefresh` 가 0.2초마다 다시 부른다. BT 서비스는 두더지 1곳만(선택) | 사용자 제안(2026-09-29). BT 는 `TargetToFollow` 를 사이트보다 먼저 보고, FindNearest 서비스는 `HasFirstAttacker` 면 손을 떼며, `CheckValidTarget` 의 거리 해제는 전투 시간 갱신으로 막힌다(바이트코드 확인). 값이 같으면 블랙보드가 옵저버를 깨우지 않아 주기 재기록 비용이 없다 |
| **D-17** | 블랙보드에 `FirstAttacker`/`TargetToFollow` 키가 없는 적(부품 도둑)은 도발하지 않는다 | 도주형이 이속 버프만 받아 더 빨리 도망가는 것을 막는다 |
| **D-18** | 도발 중 슈퍼아머 — 시선 집중 GA 가 `Effects.HitReact` 를 Block | 같은 슬롯 그룹이라 피격 몽타주가 5초 도발 모션을 끊고, 그러면 어빌리티가 즉시 끝난다(T-24) |
| **D-19** | 판정 = 0.5초 동안 0.1초 간격 6회, 30m 구, 시야 검사 없음(옵션 `bRequireLineOfSight`) | 요구 "판정 범위 유지 0.5초 / 30m 이내의 모든 적". 창 안에 들어온 적도 걸린다 |
| **D-20** | 모자 = GE 2종(`Hat.Single` / `Hat.Double`), 부모 태그 `Buff.BellBoy.Hat` 로 조회. Double 소모 = Single 을 먼저 걸고 Double 제거 | HUD 상태 아이콘이 태그 단위라 스택 감소를 방송하지 않는다(T-27). 먼저 걸어야 빈틈이 없다 |
| **D-21** | 모자 무효화 = `UDRAttributeSet::PostGameplayEffectExecute` 맨 앞(도발 배율보다 먼저). 도트 틱도 1회 | 모든 피해가 지나는 유일한 관문이라 히트리액트·넉백·디버프·어그로·부품 낙하까지 한 번에 막는다. 요구 원문 "어떤 피해든" |
| **D-22** | 최대 1개. 날아가는 모자가 이미 모자를 쓴 아군에 닿으면 **부딪혀 그 자리 바닥에 떨어진다**. 떨어진 모자는 모자를 쓴 대상이 밟아도 남는다 | 사용자 확정(2026-09-29) |
| **D-23** | 2배(2회)는 그 모자를 **던진 벨보이 본인**이 주웠을 때만 | 요구 "벨보이 자신이 직접 주우면". 벨보이 2명일 때 남의 모자는 1회 |
| **D-24** | 벽·바닥 판정 = 겹친 컴포넌트가 **Pawn 채널을 Block** 하는가. 클렌저 사이트도 벽 취급 | 트리거 볼륨(Overlap 전용)에서 모자가 떨어지는 사고를 막는다. 사이트는 캐릭터가 아니다 |
| **D-25** | 픽업은 0.25초마다 겹친 대상을 다시 본다(스폰 즉시 1회 포함) | BeginOverlap 만 쓰면 이미 올라서 있던 대상(모자를 잃은 아군·스폰 위치의 대상)이 못 줍는다 |
| **D-26** | 모자 비주얼 = GameplayCue(While Active) 액터, 부착점 = `HatSocket`(있으면) / 캡슐 꼭대기(없으면), 메시 OwnerNoSee | 모든 적 BP·스켈레톤을 고치지 않아도 되고, 에셋 하나로 복제·늦은 입장까지 처리된다 |
| **D-27** | 모자 GE 클래스는 `DA_GameBalanceConfig.BellBoy` | 어트리뷰트셋(Double→Single)·투사체·픽업이 같은 곳을 본다. 전부 서버 경로라 GameMode DA 로 충분 |
| **D-28** | 키 LMB 기본 공격 / E 시선 집중 / RMB 모자 던지기 | 조준형 투사체는 우클릭, 유틸 스킬은 E. `StartupInputTag` 한 칸이라 바꾸기 쉽다 |
| **D-29** | 쿨다운 없음 | 요구 없음 — 다른 로봇처럼 물이 유일한 제한 |
| **D-30** | 스탯 GE 는 정원로봇과 공유(`GE_DRPrimaryAttributes`/`GE_VitalAttributes`) | 요구 없음. 탱커로 조정하려면 청소기처럼 전용 GE 를 만든다 |
| **D-31** | 캐릭터 BP 는 `BP_VendingMachine` 을 복제해 만든다 | 커스텀 변수가 없고 `PlayerCharacterClass` 를 명시하는 가장 깨끗한 플레이어 BP |
| **D-32** | 기본 공격의 대상 수집은 발톱과 같은 `GetLiveObjectsWithinRadius` | 홀로그램 두더지·클렌저 사이트 처리까지 정원로봇과 같게(동작 차이를 만들지 않는다) |

---

## 부록 C. 함정 (T-n) — 밟으면 조용히 오동작한다

| # | 함정 | 증상 / 대응 |
|---|---|---|
| **T-1** | Layered Blend 브랜치 필터에 `Body_001` 만 넣음 | `Bag` 본이 루트 직속이라 **가방이 공격 몽타주를 안 따라간다** → `Bag` 을 두 번째 브랜치로 |
| **T-2** | 도발·공격 몽타주를 `DefaultSlot` 에 둠 | 전신 슬롯이라 5초 동안 **걷지 못한다** → `UpperBody` |
| **T-3** | GA 의 `Debuff Chance` 를 C++ 기본값(20)으로 둠 | 20% 확률로 물리 디버프가 붙는다 → 0 |
| **T-4** | 기본 공격 몽타주 Blend Out 기본값(0.25) | 연속 공격의 0.1초 후딜을 블렌드가 다 먹어 끊겨 보인다 → 0.05~0.1 |
| **T-5** | `ComboResetTime` 을 너무 짧게 | 서버는 "첫 타", 클라는 "연속"으로 갈려 판정이 0.2초 어긋난다 → 0.25 이상 |
| **T-6** | 도발자를 `GetInstigator()` 로 읽음 | 플레이어면 **PlayerState** 가 나온다 → `GetOriginalInstigatorAbilitySystemComponent()->GetAvatarActor()` |
| **T-7** | 적 탐색에 `GetLiveObjectsWithinRadius` 사용 | 클렌저 사이트까지 섞이고 `[MoleDiag]` 로그가 쏟아진다 → `GetAliveEnemiesWithinRadius`. (기본 공격은 발톱과 같게 쓰므로 스윙마다 로그 4번 — 부록 E-6) |
| **T-8** | 도발 때 `TargetToFollow` 만 쓰거나, 전투 시간 갱신을 빼거나, 두더지 서비스 분기를 안 넣음 | 앞의 것: `HasFirstAttacker` 가 false 라 FindNearest 서비스가 다음 틱에 가까운 플레이어로 덮어쓴다 / 가운데: 30m 떨어진 벨보이는 `CheckValidTarget` 이 3초 뒤 지운다 / 뒤의 것: 두더지 보스 기본 공격이 가끔 다른 플레이어에게 간다 (STEP 7-5) |
| **T-9** | `DRAttributeSet.h` 를 일반 편집기로 수정 | CP949/UTF-8 혼합 인코딩(2줄)이 깨진다 → 이번 계획은 `.cpp` 만 수정. 불가피하면 바이트 단위 편집 후 diff 확인 |
| **T-10** | GE 태그를 **Asset Tags** 에 넣음 | 대상에게 안 붙는다 → **Grant Tags to Target Actor(Target Tags)** 컴포넌트 |
| **T-11** | GC 모자 메시의 **Owner No See** 를 안 켬 | 1인칭 카메라 바로 위에 모자가 떠서 시야를 가린다 |
| **T-12** | 비행 적(잠자리)에게 도발 이속 기대 | `MaxFlySpeed` 는 이속 어트리뷰트를 안 따른다(Plan5 §9-1) — 타깃 고정은 동작, 이속만 무효 |
| **T-13** | 벨보이 넉백이 벽꿍 스턴(5초)을 부른다 | `MinSpeedForStun` 50 이라 벽 앞 연타가 스턴 연속이 될 수 있다 → 플레이로 확인, 과하면 넉백 값을 낮춘다 |
| **T-14** | 로봇이 4개가 되면 슬롯 해금 총비용(× 클래스 수)이 늘어난다 | `DA_ProgressionConfig` 의 재화 예산 검사가 경고할 수 있다 → 보상·비용 재검토 |
| **T-15** | 모자 스폰 위치(`CapSocket`)가 벨보이 캡슐 안 | 던진 사람을 무시하지 않으면 **쏘자마자 자기에게 씌운다** → 코드에서 무시(이미 반영) / 벽에 붙어 던지면 즉시 떨어진다 |
| **T-16** | enum 에 값을 추가하고 BP `Select`/`Switch` 핀을 안 채움 | 대기실 이름이 빈칸, 클래스별 연출이 안 나온다 → STEP 5-4 |
| **T-17** | 픽업 바닥 트레이스를 Visibility 채널로 | 적 히트박스가 Visibility 를 Block 해 **픽업이 적 머리 위에 뜬다** → 오브젝트 타입(WorldStatic/Dynamic) 트레이스 |
| **T-18** | 모자는 도트 틱도 1회로 친다 | 화상 중인 아군에게 준 모자는 다음 틱에 사라진다(요구 원문대로) → 원하면 부록 E-14 |
| **T-19** | 헤더·`UENUM`·`USTRUCT` 변경을 Live Coding 으로 | 반영되지 않는다 → 에디터 닫고 빌드(Plan5 T-45) |
| **T-20** | C++ 생성자에서 `FDRGameplayTags::Get()` 으로 태그 기본값 지정 | CDO 생성 시점엔 네이티브 태그가 아직 비어 있을 수 있다 → 태그는 BP 에서 지정 |
| **T-21** | 부모 `ADRProjectile::Destroyed` 는 클라에서 `bHit` 가 아니면 임팩트 연출을 튼다 | 모자가 장착·픽업으로 사라질 때도 클라에서 임팩트가 재생될 수 있다 → Impact Effect 를 무난한 "퐁" 으로 |
| **T-22** | 기본 공격 몽타주에 노티파이가 없거나 태그가 틀림 | 타격 이벤트가 안 와서 판정 창이 안 끝나면 어빌리티가 영원히 안 끝난다 → 몽타주가 끝날 때 판정 창도 끝난 것으로(코드 반영) + 경고 로그 |
| **T-23** | `ANS_BellBoy_HideCap` 이 도중에 끊김 | NotifyEnd 가 안 불리면 **머리 모자가 계속 숨겨진다** → 피격으로 던지기를 끊어 보고 확인. 문제가 되면 몽타주 BlendOut 이벤트에서 Unhide |
| **T-24** | `UpperBody` 와 `DefaultSlot` 이 같은 슬롯 그룹 | 피격 몽타주가 공격·도발 몽타주를 끊는다(정원로봇과 같은 구조) → 도발만 슈퍼아머(D-18) |
| **T-25** | 모자·도발도 **누르고 있으면 반복** | 입력 규칙이 다른 로봇과 같다(Held → 재발동). RMB 를 누르고 있으면 0.8초마다 물 20 씩 → 의도 확인 |
| **T-26** | `DA_GameBalanceConfig` 가 없는 게임모드(튜토리얼) | 모자 GE 를 못 찾아 모자가 무동작(경고 로그). 벨보이는 튜토리얼 대상이 아니라 현재는 무해 |
| **T-27** | 모자를 스택형 GE 1개로 만들면 | 스택이 2→1 로 줄어도 `OverlayWidgetController` 가 방송하지 않아 HUD 숫자가 "2" 로 남는다 → GE 2종(D-20) |

---

## 부록 D. 에셋 요청 목록 (아티스트)

| 에셋 | 없어서 지금은 | 우선순위 |
|---|---|---|
| FP 메시(팔+가방) · FP 애니 전부 | 1인칭 화면에 팔이 안 보인다(D-3) | 높음 |
| 모자 단독 스태틱 메시 `SM_BellBoy_Cap` | 원통으로 대체 | 높음 |
| 스킬 아이콘 3종 · 조준선(일반/설치 가능) | 정원로봇 것으로 대체 | 중간 |
| 모자 상태 아이콘(1회/2회) | 임시 아이콘 | 중간 |
| 사운드 — 가방 휘두르기·타격·도발 외침·모자 던지기·줍기·막기 팝 | 무음 | 중간 |
| VFX — 도발 "!" · 도발 충격파 · 모자 막기 팝 · 모자 사라짐 | 임시 Niagara | 중간 |
| 표정 텍스처 — 깜빡임 · 피격(Heat) · 사망 | 기본 얼굴 유지 | 낮음 |
| Jump Start / Loop / Land 분리 | `BB_Jump` 하나로 | 낮음 |
| Sitting(열차 좌석) + 좌석용 AO | 좌석에서 Idle 자세 | 낮음 |
| 부품 운반 자세(HoldingCleanserPart) | 운반 중에도 일반 자세 | 낮음 |
| MainMenu 포즈 | 메인 메뉴 연출 없음 | 낮음 |

---

## 부록 E. 열린 이슈 / 알려진 제약

| # | 내용 | 현재 판단 |
|---|---|---|
| **E-1** | 두더지 보스도 도발된다 — 튀어나오기(스킬1)는 여전히 무작위 플레이어, 기본 공격만 벨보이에게 간다 | 사용자 확정(의도한 동작). 보스 면역을 원하면 `CanBeTaunted` 에 한 줄 |
| **E-2** | 모자를 쓴 채 사망·부활하면 모자가 남을 수 있다(플레이어 ASC 는 PlayerState 에 있다) | 무해. 원하면 사망 처리에서 `Buff.BellBoy.Hat` GE 제거 |
| **E-3** | 1인칭 화면에서 기본 공격 피드백이 약하다(팔이 안 보임) | FP 에셋 전까지 사운드·카메라 셰이크로 보완 가능 |
| **E-4** | 연속 공격 단일 대상 DPS ≈ 100 (발톱 ≈ 68) | 사용자 지정 수치. 플레이 후 피해·연속 타이밍으로 조정 |
| **E-5** | 쿨다운이 없어 물 60 마다 도발을 이어 쓸 수 있다 · 모자는 물 20 | 필요하면 `CooldownDuration` + 쿨다운 GE (기존 GA 공용 경로) |
| **E-6** | 기본 공격이 `GetLiveObjectsWithinRadius` 를 스윙당 4번 불러 `[MoleDiag]` 로그가 늘어난다 | Plan5 §9-10 의 임시 로그 정리 때 함께 사라진다 |
| **E-7** | 벨보이 전용 칩 없음 | 만들 때 Plan5 의 레시피를 따른다. 기본 공격은 `SkillDamage`·`SkillAreaRadius`·`SkillWindup` 을 이미 읽는다. 도발 배율은 SetByCaller 라 시전 시점에 바꾸면 된다 |
| **E-8** | 자판기 공속 버프 칩(Plan5 §12)이 구현되면 벨보이 기본 공격도 배율 대상이 되어야 한다 | 그때 `WindupTime`/`RecoveryTime` 을 배율 M 으로 나누는 한 줄 |
| **E-9** | 튜토리얼 미지원(`DA_TutorialPlayerCharacterClassInfo` 에 없음) | 범위 밖 |
| **E-10** | 도발된 적의 **공격 속도**는 그대로 | 요구 없음 |
| **E-11** | 모자는 클렌저 사이트를 벽으로 취급한다(사이트에 씌우거나 피해를 주지 않음) | D-24 |
| **E-12** | 기본 공격은 클렌저 사이트에도 맞는다(발톱과 같음 — Plan5 §9-13) | D-32 로 발톱과 동작을 맞췄다 |
| **E-13** | 벨보이가 모자를 던지는 동안 머리 모자를 숨기는 것은 연출용 — 모자가 "하나뿐"이라는 규칙은 없다(물만 있으면 계속 던진다) | 요구 없음 |
| **E-14** | 모자 무효화가 도트 틱에도 소모된다(T-18) | 요구 원문대로. 바꾸려면 `TryConsumeBellBoyHat` 호출 전에 "주기(Periodic) 피해면 건너뜀" 조건 |
| **E-15** | 연속 판정이 서버·클라 시계로 따로 계산된다(T-5) | 유예 0.25초로 충분하다고 가정. 멀티에서 어긋나면 클라 판정을 이벤트 페이로드로 서버에 보내는 방식으로 |
