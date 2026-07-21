# Plan5 — 로봇 청소기 1인칭(FP) 애니메이션 시스템 + FP 기준 발사 + 가로형 공기탄

> **범위**: ① `FP_VC_*` 애니 시퀀스 14종의 배치 결정(1인칭 ABP vs GA 몽타주) 및 `ABP_FP_VacuumCleaner` 상세 설계, ② 공기탄(기본 공격)을 1인칭 모델 기준으로 발사하도록 변경, ③ 공기탄 투사체를 가로로 길게(수평 와이드) 변경.
> **선행 문서**: Plan3 §13 (3인칭 애니메이션 설계 — 분류 기준과 플래그 인프라를 그대로 계승), Plan3 §6 (공기탄), §8 (돌진).
> **작성일**: 2026-07-18. 코드/에셋 실측 기반 (아래 §1).

---

## 0. 결정사항 요약 ★

| 질문 | 결정 |
|---|---|
| 어떤 애니를 FP ABP에 넣나 | `Idle`, `Walk`, `Jump_Start/Loop/Land`, `Stun`, `Dash_Charge`, `Dash_Start→Dash_Loop` — **루프/가변 지속시간/이동 결합** 애니 전부. 기존 복제 플래그(`bIsDashCharging`, `bSustainedDash`, `bIsStunned`)와 로컬 CMC 값(`Speed`, `IsFalling`)을 매 프레임 폴링 (TP §13.1 분류 기준 동일 적용) |
| 어떤 애니를 GA(몽타주)로 재생하나 | `EnhancedAttack`(강화탄 발사), `Dash_Stop`(S 브레이크), `Crush`(돌진 충돌) — **1회성 + 어느 상태에서든 끼어듦 + 서버가 발생 시점 결정**. FP 전용 몽타주 3종(`AM_FP_VC_*`)을 신규 생성해 소유 클라이언트에서만 로컬 재생 |
| 더블 점프(제트 점프, Q) | **FP 전용 애니 없음 → `Jump_Start/Loop/Land` 3종 재사용**. 공중 Q 발동 순간 SM이 `Jump_Loop → Jump_Start`로 재진입해 점프 시동 애니를 다시 재생. 트리거는 신규 복제 카운터 `JetJumpCounter`의 변화 감지(§4.6) — 기존 `bIsJetJumping`은 착지까지 true 유지라 "지상 Q→공중 Q" 2연속에서 엣지가 안 생겨 부적합 |
| 미사용/보류 | `FP_VC_Jump`(통짜 — Start/Loop/Land 분할본 사용으로 불필요), `FP_VC_HitReact`(§10 보류 — 사용자 요구 목록에 없음, 확장 경로만 명시) |
| FP 기준 발사 | `ADRRobotVacuumCharacter`가 `GetCombatSocketLocation`을 오버라이드 → `CombatSocket.Weapon` 요청 시 **FP 메시의 `Muzzle` 소켓 위치** 반환. `SpawnProjectile()`·반동 계산이 자동으로 FP 기준이 됨 (C++ 1곳 수정 + FP 스켈레톤 소켓 1개 추가) |
| 가로로 긴 투사체 | `ADRVacuumAirProjectile`에 **`UBoxComponent`(WideCollision) 추가** (C++) — 구체 콜리전은 비균등 스케일이 불가능하므로 박스로 판정, 비주얼은 BP에서 메시/나이아가라 Y 스케일 |

---

## 1. 설계 전제 (코드·에셋 실측 근거)

### 1.1 FP 에셋 실측 (`Content/DaeRuneAssets/Characters/VacuumCleaner_v2/FP/`)

| 에셋 | 비고 |
|---|---|
| `VccumCleaner_FP_v0_1_0` (+ `_Skeleton`, `_PhysicsAsset`) | **FP 전용 스켈레탈 메시/스켈레톤**. TP(`VecuumCleaner_v0_1_1_Skeleton`)와 **별도 스켈레톤** → TP용 몽타주(`AM_VC_*`)는 FP 메시에서 재생 불가. FP 전용 몽타주를 새로 만들어야 함 |
| `FP_VC_Idle`, `FP_VC_Walk` | 로코모션. Walk는 **단일 시퀀스** (Gardener FP의 방향별 3종+BS와 달리 블렌드스페이스 불필요) |
| `FP_VC_Jump_Start / Jump_Loop / Jump_Land` | 점프 3분할 |
| `FP_VC_Jump` | 통짜 점프 — **미사용** (분할본이 있으므로) |
| `FP_VC_Stun` | 스턴 루프 |
| `FP_VC_Dash_Charge` | 돌진 게이지 충전 |
| `FP_VC_Dash_Start / Dash_Loop` | 지속 돌진 진입/유지 |
| `FP_VC_Dash_Stop` | S 브레이크 급정거 (1회성) |
| `FP_VC_Crush` | 돌진 충돌 (1회성) |
| `FP_VC_EnhancedAttack` | 강화탄(3단계) 발사 (1회성) |
| `FP_VC_HitReact` | 피격 — §10 보류 |
| `VC_Black/Green_001/GreenLight/White_001` | FP 텍스처 (ABP 소관 아님) |

- **FP 스켈레톤에 소켓 없음** (uasset 실측 — `Sockets` 배열 비어 있음). Gardener/VM FP 스켈레톤에는 소켓 존재. → **`Muzzle` 소켓 추가 필요** (§6.2).

### 1.2 기존 FP 파이프라인 관례 (Gardener/VendingMachine 실측)

1. **FP 메시**: `ADRCharacter` 생성자가 `FirstPersonMesh`를 `FollowCamera`에 부착, `SetOnlyOwnerSee(true)`, 그림자/콜리전 off (`DRCharacter.cpp:69-74`). **서버에도 컴포넌트가 존재**하며 카메라(=컨트롤 회전)를 따라간다.
2. **FP ABP**: `ABP_FP_Gardener` / `ABP_FP_VendingMachine`이 선례. VM FP ABP 실측: `Locomotion` 스테이트머신(Idle/Walk/Jump_Start/FallLoop/Jump_Land), 변수 `Speed`·`bIsInAir`·`ForwardDirection`·`RightDirection`·`bHasAttackSpeedBuff` 폴링. **FP 애니는 상태머신 + 복제 플래그 폴링**이 관례.
3. **FP 몽타주 재생 위치**: `GA_VendingMachineBasicAttack` BP 실측 — `IsLocallyControlled` 분기 → `PlayMontage`(K2Node_PlayMontage, 대상 = `DRCharacter.FirstPersonMesh`, 몽타주 = `AM_VM_FP_CapsuleAttack`). **GAS 몽타주 복제(AbilityTask_PlayMontageAndWait)는 TP 메인 메시 전용**이고, FP 메시는 오너에게만 보이므로 **복제 없이 로컬 재생**이 정답 — 이 관례를 그대로 따른다.
4. **FP 소켓 발사 선례**: `DRWaterPump.cpp:274-277` (1P 빔 = `FirstPersonMesh->GetSocketLocation(Muzzle)`), `DRSeedCannon`(FP 소켓 위치를 받아 서버 스폰). FP 소켓을 발사 기준으로 쓰는 것은 이미 검증된 패턴.
5. **애님 플래그 인프라 완비** (TP §13.6에서 이미 구축): `bIsDashCharging`·`bIsJetJumping`(Replicated, `DRRobotVacuumCharacter.h:101-106`), `bSustainedDash`(RepNotify, `:78`), `bIsStunned`(베이스). **소유 클라이언트에도 복제되므로 FP ABP가 추가 코드 없이 그대로 읽으면 된다.**
6. **사망 시 FP 메시는 자동 숨김** (`DRCharacterBase.cpp:191-193`) → FP Death 애니 불필요 (에셋도 없음).
7. **분류 기준(TP §13.1)을 FP에도 동일 적용**:
   - **몽타주** = 1회성 + 어떤 상태에서든 끼어듦 + 서버가 시점 결정.
   - **ABP 스테이트** = 루프/가변 지속시간/이동 결합. 플래그 폴링으로 전이.
   - 일반 공기탄(0~2단계)은 애니 없음, 일반 돌진(1~4칸)은 Walk 재생속도 가속으로 표현 — TP 결정 그대로.

### 1.3 현재 공기탄 발사 경로 (변경 대상)

- `UDRVacuumAirShot::FireShot()` (`DRVacuumAirShot.cpp:82-156`): 서버에서 `SpawnProjectile(TargetLocation, FireSocketTag)` 호출.
- `UDRProjectileSpell::SpawnProjectile()` (`DRProjectileSpell.cpp:17-48`): `ICombatInterface::Execute_GetCombatSocketLocation(Avatar, SocketTag)`로 스폰 위치 결정 → 현재는 `ADRCharacterBase::GetCombatSocketLocation_Implementation`(`DRCharacterBase.cpp:308-329`)이 **TP Weapon 소켓**을 반환.
- 조준점은 `CalculateTargetLocation()`(카메라 라인트레이스 10000uu)이므로 **방향은 이미 크로스헤어 수렴** — 문제는 스폰 위치만 TP 기준이라는 것.
- 반동 계산(`FireShot` 내 `DRVacuumAirShot.cpp:136`)도 같은 `GetCombatSocketLocation`을 사용 → 오버라이드 한 곳으로 발사·반동이 함께 FP 기준이 됨.

### 1.4 현재 투사체 구조 (변경 대상)

- `ADRProjectile`: 루트가 `USphereComponent Sphere` (`DRProjectile.h:50`), 오버랩 판정 `OnSphereOverlap`.
- `ADRVacuumAirProjectile`: 자체 페이드(`StartFade()`가 `Sphere->SetCollisionEnabled(NoCollision)` — `DRVacuumAirProjectile.cpp:47`), 강화탄 아군 넉백 분기.
- **구체 콜리전은 비균등 스케일 불가** (스피어는 스케일 최소축만 반영) → "가로로 길게"는 스케일로 해결 불가. 별도 박스 콜리전 필요 (§7).

---

## 2. 애니메이션 분류 총괄표 ★ (최종 답)

| FP 애니 시퀀스 | 분류 | 사용 위치 | 트리거 / 데이터 소스 | 재생 주체 |
|---|---|---|---|---|
| `FP_VC_Idle` | **ABP** | Locomotion SM `Idle` | `Speed < 3` | ABP_FP_VacuumCleaner |
| `FP_VC_Walk` | **ABP** | SM `Walk` (단일 시퀀스, PlayRate 가변) | `Speed ≥ 3`, `WalkPlayRate = Clamp(Speed/250, 0.6, 2.5)` | ABP |
| `FP_VC_Jump_Start` | **ABP** | SM `Jump_Start` | `bIsInAir` 진입 / **더블 점프 재진입: `bJetJumpPulse` (§4.6)** | ABP |
| `FP_VC_Jump_Loop` | **ABP** | SM `Jump_Loop` (루프) | `Jump_Start` 종료 후 자동 전이 (일반/더블 점프 공용) | ABP |
| `FP_VC_Jump_Land` | **ABP** | SM `Jump_Land` | `!bIsInAir` (일반/더블 점프 공용 — 더블 점프 착지도 동일 경로) | ABP |
| `FP_VC_Jump` (통짜) | **미사용** | — | — | — |
| `FP_VC_Stun` | **ABP** | SM `Stunned` (루프) | `bIsStunned` (기존 복제 플래그) | ABP |
| `FP_VC_Dash_Charge` | **ABP** | SM `Dash_Charge` | `bIsDashCharging` (기존 복제 플래그, §13.6) | ABP |
| `FP_VC_Dash_Start` → `FP_VC_Dash_Loop` | **ABP** | SM `Dash_Start`→`Dash_Loop` | `bSustainedDash` (기존 복제 플래그) | ABP |
| `FP_VC_Dash_Stop` | **몽타주** `AM_FP_VC_Dash_Stop` | FP ABP `DefaultSlot` | S 브레이크로 지속 돌진 종료 → `FinishDash(false)` → **신규 Client RPC** | **GA_VacuumDash → C++ Client RPC** (§5.2) |
| `FP_VC_Crush` | **몽타주** `AM_FP_VC_Crush` | FP ABP `DefaultSlot` | 돌진 충돌(일반/지속 공통) → `FinishDash(true)` → **신규 Client RPC** | **GA_VacuumDash → C++ Client RPC** (§5.2) |
| `FP_VC_EnhancedAttack` | **몽타주** `AM_FP_VC_EnhancedAttack` | FP ABP `DefaultSlot` | 3단계 강화탄 발사 시에만 (0~2단계는 애니 없음 — TP와 동일) | **GA_VacuumAirShot BP** (VM 패턴, §5.1) |
| `FP_VC_HitReact` | **보류** | — | — | §10 참고 |

**분류 근거 요약**:
- 몽타주 3종은 TP에서 이미 같은 분류(`AM_VC_Dash_Stop`/`AM_VC_Dash_Crush`/`AM_VC_EnhancedAttack` — Plan3 §13.1)로 확정된 것의 FP 미러. 어떤 로코모션/대시 상태에서든 끼어들어야 하고(충돌은 Walk 중에도 발생), 발생 시점을 서버가 결정한다.
- 나머지는 전부 루프이거나 이동/낙하와 결합 → SM 상태. 몽타주로 만들면 루프 중단 처리만 복잡해진다.
- **제트 점프(더블 점프)**: FP 전용 애니가 없으므로 **`FP_VC_Jump_Start/Loop/Land`를 그대로 재사용**하고, 공중 발동 순간 `Jump_Loop → Jump_Start` 재진입으로 시동 애니를 다시 재생한다 (TP의 `VC_DoubleJump_*` 전용 3상태와 달리 FP는 상태 추가 없이 재진입만). 상세 설계·트리거 인프라는 §4.6. 물 분사 체공 피드백은 기존 GameplayCue(`GameplayCue.Skill.VacuumJetJump`)가 병행 담당.

---

## 3. 신규 생성 에셋 목록

| 에셋 | 타입 | 사양 |
|---|---|---|
| `ABP_FP_VacuumCleaner` | Animation Blueprint | 스켈레톤 `VccumCleaner_FP_v0_1_0_Skeleton`. 배치: `Content/Blueprints/Character/PlayerCharacter/VacuumCleaner/` (ABP_FP_Gardener/ABP_FP_VendingMachine 폴더 관례). §4 전체 구현 |
| `AM_FP_VC_EnhancedAttack` | 몽타주 (FP 스켈레톤) | 소스 `FP_VC_EnhancedAttack`. 슬롯 `DefaultGroup.DefaultSlot`, BlendIn 0.1 / BlendOut 0.2 |
| `AM_FP_VC_Dash_Stop` | 몽타주 (FP 스켈레톤) | 소스 `FP_VC_Dash_Stop`. 슬롯 DefaultSlot, BlendIn **0.05** (급정거 스냅감 — TP `AM_VC_Dash_Stop`과 동일 수치) |
| `AM_FP_VC_Crush` | 몽타주 (FP 스켈레톤) | 소스 `FP_VC_Crush`. 슬롯 DefaultSlot, BlendIn **0.05** |
| FP 스켈레톤 `Muzzle` 소켓 | 스켈레톤 소켓 | §6.2 |

- 몽타주 배치 폴더: `Content/DaeRuneAssets/Characters/VacuumCleaner_v2/FP/` (TP 몽타주가 `TP/`에 있는 관례의 미러) 또는 ABP와 같은 폴더 — 프로젝트 취향대로 하되 `AM_FP_VC_` 접두어 고정.
- 슬롯은 **DefaultSlot 하나만** 사용 (TP §13.2 결정과 동일 이유 — 청소기는 상/하체 분리 개념이 없고, 몽타주끼리 나중 것이 이전 것을 끊는 것이 의도된 동작).
- 게임플레이 판정용 AnimNotify 금지 (TP §13.0-3과 동일). FP 몽타주의 노티파이는 총구 플래시 같은 코스메틱만 허용 — 단, FP는 오너 전용이므로 사실상 자유.

---

## 4. ABP_FP_VacuumCleaner 상세 설계

### 4.1 Event Graph — 변수 수집

`Event Blueprint Update Animation`에서 매 프레임 갱신. `TryGetPawnOwner` → `Cast to DRRobotVacuumCharacter` 캐시(`OwnerVacuum`) 후:

| ABP 변수 | 타입 | 계산식 (소스) | 용도 |
|---|---|---|---|
| `OwnerVacuum` | `ADRRobotVacuumCharacter*` | Initialize에서 1회 캐스팅 캐시 | 아래 전부의 소스 |
| `Speed` | float | `VSizeXY(OwnerVacuum.GetVelocity())` | Idle↔Walk 전이, WalkPlayRate |
| `bIsInAir` | bool | `OwnerVacuum.CharacterMovement.IsFalling()` | 점프 SM |
| `bIsStunned` | bool | `OwnerVacuum.bIsStunned` (베이스 복제 플래그) | Stunned 상태 |
| `bIsDashCharging` | bool | `OwnerVacuum.bIsDashCharging` (복제) | Dash_Charge 전이 |
| `bSustainedDash` | bool | `OwnerVacuum.bSustainedDash` (복제) | Dash_Start/Loop 전이 |
| `WalkPlayRate` | float | `FClamp(Speed / 250.0, 0.6, 2.5)` | Walk 재생속도. **일반 돌진(1~4칸) 이속버프가 자동 반영**되어 별도 대시 애니 없이 가속 표현 (TP §13.4와 동일 설계). 250 = `BaseWalkSpeed` 수동 일치 — 변경 시 함께 수정 |
| `bJetJumpPulse` | bool | `OwnerVacuum.JetJumpCounter != PrevJetJumpCounter` (§4.6 신규 복제 카운터의 변화 감지) | **더블 점프 `Jump_Loop → Jump_Start` 재진입 전이** — 계산 직후 `PrevJetJumpCounter` 갱신으로 1프레임 펄스화 |
| `PrevJetJumpCounter` | int (ABP 로컬) | 매 프레임 말미에 `OwnerVacuum.JetJumpCounter` 저장 | 위 펄스의 이전값 기억 (엣지 감지) |

주의:
- **FP ABP는 소유 클라이언트에서만 의미가 있다** (OnlyOwnerSee). 그래도 서버/타 클라에서 돌 수 있으므로 `OwnerVacuum` null 가드 필수 (VM FP ABP와 동일).
- 전이 조건식은 전부 위 ABP 변수만 사용 (Result 핀 직결, 함수 호출 없음 → 스레드세이프 경고 0 — TP §13.4.3 규칙 동일).
- `bIsDashCharging`/`bSustainedDash`는 서버가 세팅한 복제값이라 소유 클라 기준 **~1 RTT 지연**이 있다. 게이지 충전은 0.5초 단위(§8.2)라 체감 미미 — 지연이 거슬리면 §10의 로컬 예측 옵션 적용.

### 4.2 AnimGraph — 최종 파이프라인

```
[Locomotion State Machine]
  → [Slot 'DefaultGroup.DefaultSlot']   ← FP 몽타주 3종이 여기서 SM 출력을 덮음
  → Output Pose
```

- TP처럼 AimOffset/Boarding 레이어가 없으므로 **SM → Slot → Output** 두 노드가 전부다. FP 메시는 카메라에 붙어 있어 화면 조준 = 카메라 회전이고, 에임오프셋이 필요 없다.
- **Slot 노드는 필수** — 없으면 `PlayMontage`(FirstPersonMesh) 호출이 조용히 무시된다 (가장 흔한 실수).
- Save/Use Cached Pose 불필요 (SM 출력 소비처가 한 곳).

### 4.3 Locomotion State Machine 상태표

| 상태 | 애니 | 설정 |
|---|---|---|
| `Idle` | `FP_VC_Idle` | Loop |
| `Walk` | `FP_VC_Walk` | Loop, `PlayRate = WalkPlayRate` |
| `Jump_Start` | `FP_VC_Jump_Start` | Loop off |
| `Jump_Loop` | `FP_VC_Jump_Loop` | Loop |
| `Jump_Land` | `FP_VC_Jump_Land` | Loop off |
| `Stunned` | `FP_VC_Stun` | Loop |
| `Dash_Charge` | `FP_VC_Dash_Charge` | Loop off, 마지막 프레임 유지, **Reset on Becoming Relevant** (재진입 시 처음부터). `PlayRate = 애니 길이 ÷ 2.5s` (2.5 = MaxGauge 5 × ChargeInterval 0.5 — 게이지 만충과 애니 클라이맥스 동기화, TP §13.4.2와 동일 공식. 에디터에서 실측 길이 확인 후 계산해 상수 입력, §8.2 수치 변경 시 재계산) |
| `Dash_Start` | `FP_VC_Dash_Start` | Loop off |
| `Dash_Loop` | `FP_VC_Dash_Loop` | Loop |

### 4.4 전이표 (Priority Order: 숫자가 작을수록 먼저 평가)

| 전이 | 조건 | Blend | Priority | 비고 |
|---|---|---|---|---|
| Entry → Idle | (기본) | — | — | |
| Idle ↔ Walk | `Speed ≥ 3` / `Speed < 3` | 0.15 | 5 | |
| (Idle/Walk **Alias**) → Jump_Start | `bIsInAir` | 0.1 | 2 | |
| Jump_Start → Jump_Loop | Automatic Rule (남은 시간) | 0.1 | — | |
| **Jump_Loop → Jump_Start** | `bJetJumpPulse` | 0.05 | 1 | **더블 점프 재시동** (§4.6). 착지 판정(`NOT bIsInAir`)보다 먼저 평가되도록 Priority 1 |
| Jump_Loop → Jump_Land | `NOT bIsInAir` | 0.1 | 2 | |
| Jump_Land → Idle | Automatic Rule | 0.15 | — | Land 중 재점프 대비: Jump_Land → Jump_Start (`bIsInAir`, Priority 1) 추가 |
| (Idle/Walk Alias) → Dash_Charge | `bIsDashCharging AND NOT bIsInAir` | 0.2 | 3 | 충전 중 이동 가능(§8.2)이므로 Walk에서도 진입 |
| Dash_Charge → Dash_Start | `bSustainedDash` | 0.05 | 1 | 5칸 만충 해소 → 지속 돌진 |
| Dash_Charge → Idle | `NOT bIsDashCharging AND NOT bSustainedDash` | 0.2 | 2 | 1~4칸 해소(일반 돌진) 또는 취소 → Walk 가속 표현으로 복귀 |
| Dash_Start → Dash_Loop | Automatic Rule | 0.1 | — | |
| Dash_Loop → Idle | `NOT bSustainedDash` | 0.2 | — | 실제 종료 연출은 같은 순간 도착하는 `AM_FP_VC_Dash_Stop/Crush` 몽타주가 Slot에서 덮고, SM 복귀는 몽타주 블렌드아웃 뒤에 자연스럽게 드러남 (TP §13.4.2와 동일 트릭) |
| (Any 또는 Idle/Walk/Jump계 Alias) → Stunned | `bIsStunned` | 0.1 | 1 | 대시 계열 상태에서도 스턴 진입이 필요하면 Alias 범위에 포함. 단, 스턴 시 GA가 강제 취소되어 `bSustainedDash`/`bIsDashCharging`이 서버에서 false로 정리되므로(`EndAbility` — `DRVacuumDash.cpp:290-304`) Idle 경유로도 1프레임 내 도달함 — 우선 Idle/Walk/Jump Alias만으로 시작하고 PIE에서 어색하면 확장 |
| Stunned → Idle | `NOT bIsStunned` | 0.2 | — | |

컴파일 후 **경고 0** 확인 (Result 핀에 조건 미연결 채 저장 금지 — TP §13.4.3-1의 실수 사례 재발 방지).

### 4.5 캐릭터 연결

`BP_DRVacuumCleaner`의 `FirstPersonMesh` 컴포넌트:
1. Skeletal Mesh = `VccumCleaner_FP_v0_1_0` 확인 (미지정이면 지정).
2. Anim Class = `ABP_FP_VacuumCleaner` 지정.
3. 트랜스폼(카메라 기준 오프셋)은 Gardener/VM FP 메시 배치를 참고해 뷰포트에서 조정.

### 4.6 더블 점프(제트 점프) — `Jump_Start/Loop/Land` 3종 재사용 상세 설계 ★

**요구사항**: 더블 점프 전용 FP 애니가 없으므로 일반 점프와 **같은 3종을 그대로 사용**한다. 목표 동작: 공중에서 Q(제트 점프) 발동 순간 `Jump_Start`가 처음부터 다시 재생 → 자동으로 `Jump_Loop` → 착지 시 `Jump_Land`. 즉 **상태 추가 없이 `Jump_Loop → Jump_Start` 재진입 전이 하나로 해결**한다.

#### 4.6.1 왜 기존 `bIsJetJumping` 플래그로는 안 되는가 (트리거 인프라가 필요한 이유)

- `bIsJetJumping`은 GA가 발동 시 `SetJetJumping(true)`로 켜고 **착지(`Landed()`)에서야 꺼지는 레벨(level) 플래그**다 (`DRRobotVacuumCharacter.h:104-106`, `DRVacuumJetJump.cpp:65`). "발동 순간"을 표현하는 엣지(edge) 신호가 아니다.
- 상승 엣지 감지(이번 프레임 true ∧ 이전 프레임 false)로 우회하려 해도 **"지상 Q → 공중 Q" 2연속 규칙(§7.1)에서 깨진다**: 첫 지상 Q가 이미 `bIsJetJumping = true`로 만들고 착지 전까지 유지되므로, 두 번째 공중 Q의 `SetJetJumping(true)`는 값 변화가 없어 **엣지가 발생하지 않는다** → 두 번째 점프에서 `Jump_Start` 재시동 불가.
- 속도 기반 감지(Z 속도 급증)는 넉백·강화탄 로켓 점프·점프대 등과 오탐이 겹쳐 부적합.
- **결론**: 발동 횟수를 세는 **복제 카운터** `JetJumpCounter`를 신설한다. 값이 "변할 때마다" = 제트 점프 1회 발동. 레벨 플래그의 한계와 무관하게 매 발동을 엣지로 만들 수 있고, uint8 랩어라운드도 "변화 감지"에는 무해하다.

#### 4.6.2 C++ 변경 — `JetJumpCounter` (신규 복제 프로퍼티)

`DRRobotVacuumCharacter.h` — 기존 애님 전용 복제 플래그 블록(§13.6 — `bIsDashCharging`/`bIsJetJumping` 옆)에 추가:

```cpp
// 제트 점프 발동 횟수 카운터 (서버 전용 증가, 랩어라운드 무해).
// FP ABP가 값 "변화"를 엣지로 감지해 Jump_Start 재진입 — bIsJetJumping은 착지까지 true 유지라
// "지상 Q → 공중 Q" 2연속에서 엣지가 안 생기므로 이 카운터가 필요 (Plan5 §4.6)
UPROPERTY(Replicated, BlueprintReadOnly, Category = "Animation")
uint8 JetJumpCounter = 0;
```

`DRRobotVacuumCharacter.cpp`:

```cpp
void ADRRobotVacuumCharacter::SetJetJumping(bool bNew)
{
	// (기존 구현 유지) ...
	if (bNew && HasAuthority())
	{
		++JetJumpCounter;   // 발동 1회당 1 증가 — bIsJetJumping이 이미 true여도 변화 발생
	}
	// ...
}
```

- `GetLifetimeReplicatedProps()`에 `DOREPLIFETIME(ADRRobotVacuumCharacter, JetJumpCounter);` 추가 (기존 `bIsJetJumping`과 같은 블록).
- RepNotify 불필요 — ABP가 매 프레임 폴링하는 §13.6 관례 그대로.
- 증가 위치를 `SetJetJumping(true)` 내부에 두는 이유: GA(`DRVacuumJetJump.cpp:65`)가 발동마다 이 함수를 호출하는 유일한 경로라서, GA 쪽 코드는 **한 줄도 안 바뀐다**. `Landed()`의 `SetJetJumping(false)`는 증가시키지 않음.

#### 4.6.3 ABP Event Graph — 펄스 생성 (매 프레임, §4.1 변수 수집 말미)

```
bJetJumpPulse     = (OwnerVacuum.JetJumpCounter != PrevJetJumpCounter)
PrevJetJumpCounter = OwnerVacuum.JetJumpCounter        ← 반드시 펄스 계산 "다음"에 갱신
```

- 갱신 순서가 뒤집히면 펄스가 영원히 false — 노드 실행 핀 순서에 주의.
- `Event Initialize Animation`에서 `PrevJetJumpCounter = OwnerVacuum.JetJumpCounter`로 초기화 (리스폰/폰 재빙의 직후 잔여 카운터로 인한 가짜 펄스 1회 방지).
- Event Graph는 AnimGraph 상태머신 갱신보다 같은 프레임 먼저 실행되므로 1프레임 펄스로 전이 조건에 충분하다.
- `bIsInAir` 게이트는 걸지 않는다: 지상 Q의 펄스가 Idle/Walk 상태에서 발생해도 해당 상태에는 펄스 전이가 없어 무해하고, 지상 진입은 기존 `(Idle/Walk Alias) → Jump_Start (bIsInAir)` 전이가 같은 프레임에 처리한다.

#### 4.6.4 상태머신 동작 — 시나리오별 애니 흐름 (모두 기존 3종만 사용)

| 시나리오 | 상태 흐름 | 트리거 |
|---|---|---|
| 스페이스 일반 점프 | Idle/Walk → **Jump_Start** → Jump_Loop → Jump_Land | `bIsInAir` (기존) |
| 스페이스 점프 → 공중 Q (더블 점프) | … Jump_Loop → **Jump_Start (재진입, 처음부터 재생)** → Jump_Loop → Jump_Land | `bJetJumpPulse` (신규 전이) |
| 지상 Q (제트 점프 1회차) | Idle/Walk → **Jump_Start** → Jump_Loop → … | `bIsInAir` — 펄스도 같은 프레임 발생하지만 Idle/Walk엔 펄스 전이가 없어 기존 경로 사용 |
| 지상 Q → 공중 Q (2회차) | … Jump_Loop → **Jump_Start (재진입)** → Jump_Loop → Jump_Land | `bJetJumpPulse` — **카운터 방식이라 두 번째도 확실히 발화** (§4.6.1의 핵심) |
| 더블 점프 착지 | Jump_Loop → **Jump_Land** → Idle | `NOT bIsInAir` (기존, 일반 점프와 완전 동일 — `Landed()`가 `bAirJumpUsed`/`bIsJetJumping` 리셋) |
| Jump_Land 블렌드 중 재점프/Q | Jump_Land → Jump_Start | `bIsInAir` (기존 Priority 1 전이가 커버) |

재진입 시 `Jump_Start`는 `Jump_Loop`에서 다시 관련(relevant) 상태가 되므로 시퀀스가 자동으로 0초부터 재생된다. 확실히 하려면 `Jump_Start` 시퀀스 플레이어에 **Reset on Becoming Relevant 체크** (짧은 크로스페이드 중 재진입하는 극단 케이스 방어).

#### 4.6.5 한계·엣지 케이스 (수용 및 근거)

- **`Jump_Start` 재생 중 Q**: SM은 자기 자신으로의 전이가 불가능해 시동 애니를 0초로 되감을 수 없다. 실제로는 `bAirJumpUsed`로 공중 1회 제한이 있어 "시동 직후 Q"는 지상 Q→즉시 공중 Q 한 케이스뿐이고, 이때 시동 애니가 이미 재생 중이므로 시각적으로 자연스럽다 — **수용**.
- **복제 지연**: 카운터는 서버 증가 → 소유 클라 ~1 RTT 후 도착. `LaunchCharacter`(서버) 결과인 위치/속도 변화와 같은 복제 묶음으로 도착하므로 상승 시작과 애니 재시동이 사실상 동기. TP `bIsJetJumping`도 같은 지연을 이미 수용 중.
- **스턴/대시 상태 중 Q**: `Dash_Loop` 등에는 펄스 전이를 두지 않는다 — 대시 연출 우선 (펄스는 그 프레임에 소멸, 잔류 부작용 없음).
- **관전/타 클라 ABP 인스턴스**: FP 메시는 OnlyOwnerSee라 시각 영향 없음. 카운터는 전 클라 복제되지만 펄스 소비처가 FP ABP뿐이므로 무해.

---

## 5. FP 몽타주 3종 트리거 배선

### 5.1 `AM_FP_VC_EnhancedAttack` — GA_VacuumAirShot BP (VM 패턴, 코드 변경 없음)

`GA_VendingMachineBasicAttack`의 검증된 그래프를 그대로 이식:

```
[InputReleased 이벤트]
 ├─ Branch: IsLocallyControlled(AvatarActor) AND (GetChargeStage() == 3)   ← 3 = Stages.Num()-1
 │    └─ True → PlayMontage (대상: DRCharacter.FirstPersonMesh, 몽타주: AM_FP_VC_EnhancedAttack)
 └─ ReleaseAndFire()      ← 몽타주 재생 "다음에" 호출
```

- **순서 중요**: `ReleaseAndFire()`가 내부에서 `EndAbility`를 호출하므로(`DRVacuumAirShot.cpp:79`), FP `PlayMontage`를 **먼저** 실행한다. `PlayMontage`(K2 노드)는 AnimInstance 위에서 재생되므로 GA가 끝나도 계속 재생된다 (TP 쪽은 C++에서 `bStopWhenAbilityEnds=false`로 같은 문제를 이미 해결 — `DRVacuumAirShot.cpp:117`).
- `GetChargeStage()`는 클라 인스턴스에서도 로컬 시계로 동일하게 계산되므로(`DRVacuumAirShot.cpp:57-65`) 서버 왕복 없이 판정 가능.
- 0~2단계는 아무것도 재생하지 않음 (TP 결정 그대로).
- TP `AM_VC_EnhancedAttack`은 기존 C++ 경로(GAS 복제 → 타 클라 시뮬 프록시 재생)가 그대로 담당 — **소유 클라에서는 TP 메시가 안 보이므로 이중 재생이어도 시각 충돌 없음**.

### 5.2 `AM_FP_VC_Dash_Stop` / `AM_FP_VC_Crush` — C++ Client RPC (신규, 유일한 신규 통지 경로)

**문제**: 트리거 지점이 서버 전용 C++ `UDRVacuumDash::FinishDash()`(`DRVacuumDash.cpp:224-261`)다. TP 몽타주는 GAS 복제로 전 클라에 퍼지지만 FP 몽타주는 **소유 클라이언트의 FP 메시에서 로컬 재생**해야 하므로, 서버→소유 클라 통지가 필요하다. 기존 선례: `NotifyVacuumDashGaugeChanged → ClientVacuumDashGaugeChanged`(ASC Client RPC, `DRAbilitySystemComponent.cpp:252-260`)와 `MulticastPlayDashImpactSound`(캐릭터 RPC).

**구현 — `ADRRobotVacuumCharacter`에 Client RPC 추가** (헤더):

```cpp
// ========== FP 애니메이션 (Plan5 §5.2) ==========

// 돌진 종료 FP 몽타주 — S 브레이크 급정거 (BP_DRVacuumCleaner에서 AM_FP_VC_Dash_Stop 지정)
UPROPERTY(EditDefaultsOnly, Category = "Dash|Animation")
TObjectPtr<UAnimMontage> FPDashStopMontage;

// 돌진 충돌 FP 몽타주 — 일반/지속 돌진 충돌 공통 (AM_FP_VC_Crush 지정)
UPROPERTY(EditDefaultsOnly, Category = "Dash|Animation")
TObjectPtr<UAnimMontage> FPDashCrushMontage;

// 서버(GA FinishDash) → 소유 클라: FP 메시에 돌진 종료 몽타주 재생.
// Reliable — 1회성 저빈도 연출이지만 유실 시 FP만 뻣뻣하게 남아 눈에 띔
UFUNCTION(Client, Reliable)
void ClientPlayDashEndFPMontage(bool bFromImpact);
```

구현(cpp):

```cpp
void ADRRobotVacuumCharacter::ClientPlayDashEndFPMontage_Implementation(bool bFromImpact)
{
	// 소유 클라(리슨 호스트 포함)에서만 도착. FP 메시가 안 보이는 상태(사망 등)면 생략
	UAnimMontage* Montage = bFromImpact ? FPDashCrushMontage.Get() : FPDashStopMontage.Get();
	if (!Montage || !FirstPersonMesh || !FirstPersonMesh->IsVisible()) return;

	if (UAnimInstance* AnimInst = FirstPersonMesh->GetAnimInstance())
	{
		AnimInst->Montage_Play(Montage);
	}
}
```

**호출 지점 — `UDRVacuumDash::FinishDash()`**, TP 몽타주 재생 블록(`DRVacuumDash.cpp:246-258`) 바로 옆에 TP와 **동일한 선택 규칙**으로 추가:

```cpp
// FP 미러 (Plan5 §5.2): 충돌 = Crush(일반/지속 공통), S 브레이크(지속) = Stop, 자연 감쇠 = 없음
if (Vacuum && (bFromImpact || bWasSustained))
{
	Vacuum->ClientPlayDashEndFPMontage(bFromImpact);
}
```

- 선택 규칙이 TP(`FinishMontage` 삼항식, `DRVacuumDash.cpp:248-249`)와 정확히 일치해야 한다: **충돌 → Crush, 지속 돌진 브레이크 → Stop, 일반 돌진 자연 감쇠(게이지 0) → 재생 없음.**
- ASC가 아닌 캐릭터에 RPC를 두는 이유: 몽타주 에셋 참조가 캐릭터 BP 소관이고(`DashStopMontage` 등 TP 참조도 GA에 있지만 FP는 메시가 캐릭터 소유), `MulticastPlayDashImpactSound` 선례와 위치가 같다.

### 5.3 등록 체크리스트 (BP 프로퍼티)

| 프로퍼티 | 위치 | 값 |
|---|---|---|
| `FPDashStopMontage` | `BP_DRVacuumCleaner` | `AM_FP_VC_Dash_Stop` |
| `FPDashCrushMontage` | `BP_DRVacuumCleaner` | `AM_FP_VC_Crush` |
| (BP 그래프) FP 강화탄 몽타주 | `GA_VacuumAirShot` | `AM_FP_VC_EnhancedAttack` (§5.1 노드의 인라인 지정) |

---

## 6. 기본 공격(공기탄) — 1인칭 모델 기준 발사

### 6.1 설계

발사는 서버 권한(`FireShot` → `SpawnProjectile`)을 유지한 채, **스폰 위치의 소스만 TP Weapon 소켓 → FP 메시 `Muzzle` 소켓으로 교체**한다. `FirstPersonMesh`는 서버에도 존재하고 `FollowCamera`(컨트롤 회전은 서버가 정확히 앎)에 부착되어 있으므로 서버가 직접 읽을 수 있다 — `GA_SeedCannon`이 서버 BP 인스턴스에서 FP 소켓 위치를 읽어 스폰하는 기존 패턴과 동일한 원리.

**구현 — `ADRRobotVacuumCharacter`에 `GetCombatSocketLocation` 오버라이드** (헤더):

```cpp
// 공기탄 발사 기준 소켓 — FP 메시의 총구 (Plan5 §6)
UPROPERTY(EditDefaultsOnly, Category = "Combat")
FName FPMuzzleSocketName = FName("Muzzle");

// CombatSocket.Weapon 요청을 FP 메시 Muzzle로 라우팅 — 발사 위치/반동 기준을 1인칭 모델에 맞춤
virtual FVector GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag) override;
```

구현(cpp):

```cpp
FVector ADRRobotVacuumCharacter::GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag)
{
	if (MontageTag.MatchesTagExact(FDRGameplayTags::Get().CombatSocket_Weapon)
		&& FirstPersonMesh && FirstPersonMesh->DoesSocketExist(FPMuzzleSocketName))
	{
		return FirstPersonMesh->GetSocketLocation(FPMuzzleSocketName);
	}
	return Super::GetCombatSocketLocation_Implementation(MontageTag);
}
```

**이 한 곳으로 함께 해결되는 것**:
- `SpawnProjectile()`의 스폰 위치 (`DRProjectileSpell.cpp:22-24`) → FP 총구.
- 스폰 회전 = `(TargetLocation - SocketLocation).Rotation()` → FP 총구에서 크로스헤어 조준점으로 수렴 (조준점은 기존 카메라 트레이스 유지).
- 강화탄 반동 방향 계산 (`DRVacuumAirShot.cpp:136`) → FP 총구 기준. 발밑 사격 로켓 점프 동작 불변.

**전제 조건**: `GA_VacuumAirShot`의 `FireSocketTag`가 `CombatSocket.Weapon`으로 지정되어 있어야 매칭된다 (BP 확인 — 주석상 기본 의도가 Weapon).

**정합성 노트**:
- 서버의 FP 메시는 렌더되지 않아 애님 포즈가 틱하지 않을 수 있다(`VisibilityBasedAnimTickOption` 기본값). 이 경우 소켓은 레퍼런스 포즈 기준 위치 = **카메라에 대한 고정 오프셋**이 되어 오히려 결정적(deterministic)이다. 클라 화면의 총구와 수 cm 이내 오차 — FPS 표준 허용 범위. WaterPump 1P/3P 빔도 같은 이원화를 이미 허용 중.
- 타 클라이언트 관점: 투사체가 눈높이(카메라)에서 나온다 — FPS 표준. 3인칭 총구 이펙트가 필요하면 TP `Muzzle` 소켓(이미 존재, Plan3 §13.4.3-(0))에 나이아가라만 별도 스폰 (선택, §10).
- 다른 클래스 영향 없음: 오버라이드는 청소기 클래스에만 존재하고, 청소기에서 `CombatSocket.Weapon`을 쓰는 소비자는 공기탄뿐.

### 6.2 FP 스켈레톤 `Muzzle` 소켓 추가 (에디터 작업)

1. `VccumCleaner_FP_v0_1_0_Skeleton` 스켈레톤 에디터 열기 (실측: 현재 소켓 0개).
2. 흡입구/노즐 끝 본에 소켓 `Muzzle` 추가 (본 이름은 에디터에서 확인 — 노즐이 없으면 루트/바디 본에 추가 후 프리뷰에서 총구 위치로 오프셋).
3. 프리뷰 메시에서 위치 확인: **노즐 전방 약간 앞(+X 10~20uu)** — 투사체가 FP 메시와 겹쳐 보이는 첫 프레임 방지.
4. `FP_VC_EnhancedAttack` 재생 상태에서 소켓이 화면 밖으로 튀지 않는지 확인.

---

## 7. 공기탄 투사체 — 가로로 길게 (수평 와이드)

### 7.1 왜 C++ 변경이 필요한가

- 판정 루트가 `USphereComponent`(C++ 생성, `DRProjectile.h:50`)다. 스피어 콜리전은 비균등 스케일을 지원하지 않는다(최소축 반영) → BP 스케일 조정만으로는 **판정을 가로로 늘릴 수 없다**.
- 따라서 판정용 **박스 콜리전을 C++로 추가**하고, 비주얼(메시/나이아가라)만 BP에서 Y 스케일로 늘린다.

### 7.2 구현 — `ADRVacuumAirProjectile`에 WideCollision 추가

헤더:

```cpp
class UBoxComponent;

// 가로형 판정 박스 (Plan5 §7) — 루트 Sphere는 비균등 스케일 불가라 별도 박스로 판정.
// 기본 NoCollision — BP_VacuumAirProjectile에서 프로파일/Extent 설정 (Sphere 세팅 미러)
UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirShot")
TObjectPtr<UBoxComponent> WideCollision;
```

생성자(신규 — 현재 `ADRVacuumAirProjectile`은 생성자가 없으므로 추가):

```cpp
ADRVacuumAirProjectile::ADRVacuumAirProjectile()
{
	WideCollision = CreateDefaultSubobject<UBoxComponent>(TEXT("WideCollision"));
	WideCollision->SetupAttachment(GetRootComponent());   // 루트 = Sphere
	WideCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);   // 실제 세팅은 BP
	WideCollision->OnComponentBeginOverlap.AddDynamic(this, &ADRVacuumAirProjectile::OnSphereOverlap);
}
```

- `OnSphereOverlap`은 컴포넌트 오버랩 시그니처의 `UFUNCTION`이라 그대로 바인딩 가능. 기존 파이프라인(강화탄 아군 넉백 분기 → 부모 데미지)이 어느 콜리전에서 발화하든 동일하게 동작하며, `bHit`/`Destroy`가 중복 발화를 막는다.
- **페이드 시 박스도 꺼야 한다** — `StartFade()` 수정 (`DRVacuumAirProjectile.cpp:47` 옆):

```cpp
	Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (WideCollision) WideCollision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
```

### 7.3 BP_VacuumAirProjectile 세팅

| 항목 | 값 | 이유 |
|---|---|---|
| `WideCollision` 콜리전 | 기존 `Sphere`와 **동일 프로파일/오브젝트 타입/오버랩 응답** 복사, `Generate Overlap Events` on | 판정 주체 이관 |
| `WideCollision` Box Extent | **X 16 / Y 100 / Z 16** (시작값 — X = 진행 방향 두께, Y = 좌우 폭, Z = 상하 두께) | "가로로 길게": 진행 방향과 수직인 좌우로 넓은 에어 블레이드 형태 |
| 기존 `Sphere` | 반경 축소(예: 8) 또는 콜리전 응답 전부 Ignore | 이중 판정 방지. 루트 유지(트랜스폼 기준)를 위해 컴포넌트 삭제는 하지 않음 |
| 비주얼 (메시/나이아가라) | 상대 스케일 Y를 박스 폭에 맞춤 (예: Y 4~6배) | 판정과 보이는 폭 일치 |
| (선택) 강화탄 확대 | `bEnhanced` 복제값으로 BeginPlay/OnRep에서 Extent·스케일 1.5배 등 | 3단계 탄 차별화 |

**기하 정합성 근거**: 스폰 회전은 `(Target - Socket).Rotation()`으로 Roll이 항상 0 → 박스 로컬 Y축이 언제나 월드 수평이다. `bRotationFollowsVelocity`를 켜도 중력 없는 직선 탄이라 Roll이 생기지 않는다. 즉 **박스는 자동으로 "가로" 방향 유지** — 추가 보정 불필요.

**속도 주의**: 판정이 오버랩 방식 + 최고 탄속 2600uu/s는 기존과 동일하므로 터널링 리스크 변화 없음. 박스 X(진행 방향)를 지나치게 얇게(<10) 만들지만 않으면 된다.

---

## 8. 구현 순서 (Phase)

| 단계 | 작업 | 산출물 | 검증 |
|---|---|---|---|
| **A. C++ 일괄** | §5.2 Client RPC + 몽타주 프로퍼티, §6.1 소켓 오버라이드, §7.2 WideCollision, **§4.6.2 `JetJumpCounter`(SetJetJumping 증가 + DOREPLIFETIME)**. 빌드 | `DRRobotVacuumCharacter.h/.cpp`, `DRVacuumDash.cpp`, `DRVacuumAirProjectile.h/.cpp` | 컴파일 + 기존 TP 동작 회귀 없음 (공기탄이 일시적으로 캐릭터 중심 근처에서 나가는 것은 B 전까지 허용 — `DoesSocketExist` 폴백이 Super로 안전) |
| **B. 에셋 기초** | FP 스켈레톤 `Muzzle` 소켓(§6.2), FP 몽타주 3종(§3), FP 메시/AnimClass 연결(§4.5) | 소켓 1, `AM_FP_VC_*` 3, BP 세팅 | 발사 위치가 FP 총구로 이동했는지 PIE 확인 |
| **C. FP ABP** | `ABP_FP_VacuumCleaner` — 로코모션/점프/스턴부터 (§4.3 위 6개 상태) + **더블 점프 펄스/재진입 전이(§4.6.3~4)**, 이후 대시 3상태 | ABP 컴파일 경고 0 | PIE: 걷기/점프/더블 점프/스턴 FP 반응 |
| **D. 몽타주 배선** | §5.1 GA BP 노드, §5.3 프로퍼티 지정 | GA_VacuumAirShot 그래프, BP_DRVacuumCleaner | 강화탄/브레이크/충돌 FP 몽타주 재생 |
| **E. 투사체 튜닝** | §7.3 BP 세팅 + 폭/비주얼 조정 | BP_VacuumAirProjectile | 아래 §9 |

---

## 9. PIE 검증 체크리스트 (멀티플레이 2인, 리슨 서버)

**FP ABP** (청소기 조종 클라 기준):
1. 정지=Idle, 이동=Walk, 이속버프(일반 돌진 1~4칸) 시 Walk 재생 가속.
2. 점프: Start→Loop→Land 순서.
2-1. **더블 점프 4시나리오 (§4.6.4 표 그대로)**: ⓐ 스페이스 점프 → 공중 Q: 체공 중 Jump_Start가 처음부터 재시동 후 Loop 복귀. ⓑ 지상 Q: 일반 점프와 동일한 Start→Loop. ⓒ **지상 Q → 공중 Q: 두 번째 Q에서도 Jump_Start 재시동** (카운터 엣지 검증 핵심 — 이게 실패하면 §4.6.1의 레벨 플래그 함정 재발). ⓓ 더블 점프 착지: Jump_Land 1회 재생 후 Idle, 이후 다시 점프하면 정상 시동 (Landed 리셋 확인). 각 시나리오에서 GameplayCue 물 분사 VFX 병행 확인.
2-2. 리스폰/폰 재빙의 직후 첫 프레임에 Jump_Start가 가짜로 재생되지 않는지 (§4.6.3 Initialize 초기화 검증).
3. RMB 홀드=Dash_Charge 진입(≤0.5s 지연 허용), 만충 해소=Dash_Start→Dash_Loop, 1~4칸 해소=Idle/Walk 복귀.
4. 스턴(적 스킬 또는 치트)=FP_VC_Stun 루프, 해제 시 복귀. 스턴이 대시 충전 중 걸려도 FP가 Idle/Stun으로 정리되는지.
5. 사망 시 FP 메시 숨김(기존 동작) — FP 애니 잔상 없음.

**FP 몽타주**:
6. 3단계 강화탄 발사 → FP EnhancedAttack 재생, 0~2단계는 재생 없음.
7. 지속 돌진 중 S=FP Dash_Stop, 벽/적 충돌=FP Crush (일반 돌진 충돌도 Crush). 자연 감쇠 종료(게이지 소진)는 아무것도 재생 안 됨.
8. 위 6~7이 **타 클라이언트 화면의 TP 몽타주와 동시에** 일어나는지 (TP 경로 회귀 확인).
9. 리슨 서버 호스트가 청소기일 때도 6~7 동작 (Client RPC는 호스트 로컬 실행).

**FP 발사 + 와이드 탄**:
10. 크로스헤어 정조준: 탄이 FP 총구에서 나가 크로스헤어 지점에 명중 (근·중·원거리 3곳).
11. 상하 시야 한계(±30°)에서도 총구-크로스헤어 수렴 유지.
12. 발밑 강화탄 로켓 점프 반동 방향 정상 (반동 기준점 변경 회귀 체크).
13. 나란히 선 적 2기가 한 발에 동시 피격 (가로 판정 폭 확인). 세로로 위아래 배치 시엔 한 기만 피격 (Z 얇음 확인).
14. 강화탄 아군 넉백/탑승자 통과 예외 정상 (박스 판정으로 이관 후 회귀).
15. 페이드 시작 후 탄에 스치면 무판정 (박스 콜리전 off 확인).

---

## 10. 명시적 비범위 / 선택 확장

| 항목 | 상태 | 확장 경로 (필요해지면) |
|---|---|---|
| `FP_VC_HitReact` | **보류** (요구 목록 외) | `GA_HitReact`(공용 BP)에 §5.1과 같은 `IsLocallyControlled → PlayMontage(FirstPersonMesh)` 분기 + 클래스별 몽타주 선택 추가. 화면 셰이크/포스트프로세스로 대체 중이면 불필요 |
| `FP_VC_Jump` (통짜) | **미사용** | — |
| 제트 점프 FP 애니 | ~~보류~~ → **§4.6에 포함됨** (Jump_Start/Loop/Land 재사용 + `JetJumpCounter` 펄스 재진입) | 전용 더블점프 애니가 제작되면 §4.6 전이는 유지한 채 상태의 시퀀스만 교체 |
| Dash_Charge 로컬 예측 | 복제 플래그 폴링(~1 RTT 지연) 수용 | `GA_VacuumDash` BP ActivateAbility에서 `IsLocallyControlled` 시 캐릭터 로컬 bool(비복제) set → ABP가 `bIsDashCharging OR 로컬 bool` 사용. 해제 누락 방지를 위해 EndAbility 경로에서도 정리 필요 |
| 3인칭 총구 플래시 | 미적용 | TP `Muzzle` 소켓에 GameplayCue/멀티캐스트 나이아가라 (타 클라 전용, OwnerNoSee) |
| TP ABP 수정 | **없음** — 본 계획은 TP 경로를 일절 건드리지 않음 | — |

## 11. 변경 파일 요약

| 파일 | 변경 |
|---|---|
| `Source/DaeRune/Public/Character/DRRobotVacuumCharacter.h` | `FPMuzzleSocketName`, `GetCombatSocketLocation_Implementation` 오버라이드 선언, `FPDashStopMontage`/`FPDashCrushMontage`, `ClientPlayDashEndFPMontage`, **`JetJumpCounter` (§4.6.2)** |
| `Source/DaeRune/Private/Character/DRRobotVacuumCharacter.cpp` | 위 구현 + `SetJetJumping()` 카운터 증가 1줄 + `GetLifetimeReplicatedProps()`에 `JetJumpCounter` 등록 |
| `Source/DaeRune/Private/AbilitySystem/Abilities/DRVacuumDash.cpp` | `FinishDash()`에 `ClientPlayDashEndFPMontage(bFromImpact)` 호출 1블록 |
| `Source/DaeRune/Public/Actor/DRVacuumAirProjectile.h` | 생성자 선언, `WideCollision` |
| `Source/DaeRune/Private/Actor/DRVacuumAirProjectile.cpp` | 생성자(박스 생성+바인딩), `StartFade()`에 박스 off 1줄 |
| 에디터 | `ABP_FP_VacuumCleaner`, `AM_FP_VC_*` 3종, FP 스켈레톤 `Muzzle` 소켓, `BP_DRVacuumCleaner`/`GA_VacuumAirShot`/`BP_VacuumAirProjectile` 세팅 |
