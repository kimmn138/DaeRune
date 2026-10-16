# Plan8 — 벨보이(BellBoy) 1인칭(FP) 적용 계획

> **작성일** 2026-10-15 · **상태** C++(STEP 1~3) 구현·빌드 완료 2026-10-16 · 남은 작업: 에디터(STEP 0, 4~9) · **작업 브랜치** `feat/bellboy_fp`
> **선행** 벨보이 TP 구현 완료(`3fe5b9f9`, PR #44) — 그때의 계획서는 git `3fe5b9f9^:Plan8.md` 에 있다. 이 문서 부록 B/C/E 에 그 결정·함정을 요약해 둔다.
> **범위** 벨보이 FP 메시·FP ABP·FP 몽타주 · 스킬 3종(가방/시선 집중/모자)의 FP 재생과 TP 동기화 · FP 피격/스턴/점프/걷기 · FP 손의 모자 소품 · 모자 발사 위치(FP 기준) · 부품 운반 소켓
> **비범위** 스킨(코스메틱 FP 부착물) · 벨보이 칩 · 다른 로봇의 FP 피격 몽타주 연결(구조만 열어 둔다 — E-19)
>
> 📌 **읽는 법** — STEP 0 → STEP 9 순서대로. `[C++]` / `[에디터]` 표시, 각 STEP 끝 **✅ 확인** 통과 후 다음으로.
> ⚠️ **코드 주석 표기** — FP 작업은 `Plan8(FP)`. 절 번호(§·STEP)는 코드에 쓰지 않고 D-n / T-n 으로 가리킨다. D/T/E 는 영구 ID(결번 재사용 금지).
> 🔎 수치는 2026-10-15 에 `.uasset` 을 직접 파싱해 확인한 값이다(§1).

---

## 0. 요약

| 항목 | 결론 |
|---|---|
| FP 메시 | `BellBoy/FP/Bellboy_FP_v0_1_0` — 팔·가방만(다리·모자·얼굴 없음). 머티리얼 슬롯 4(`BB_Red_001/003/Emission`, `BB_White`) |
| FP ABP | 새 `ABP_FP_BellBoy` — `ABP_FP_VendingMachine` 구조를 복사(Locomotion Idle/Walk · Jump Start/Loop/Land · Stun · `DefaultSlot`) (D-41) |
| FP 몽타주 | 5개 — 기본 공격 L/R · 도발 · 모자 던지기 · 피격. 전부 `DefaultSlot`(D-34) · **게임플레이 이벤트 노티파이 금지**, 대신 `FPSync` 동기 노티파이 1개(D-35 · T-30) |
| 재생 주체 | **소유 클라(리슨 호스트 자기 폰 포함)만** FP 몽타주를 튼다. 서버 판정은 지금처럼 TP 몽타주 노티파이(D-33) |
| 동기화 | FP 몽타주의 `FPSync` 프레임이 TP 이벤트(타격/도발 시작/릴리스)와 **같은 실시간**에 오도록 재생 속도를 계산. 기본 공격은 선딜·후딜 속도를 FP 도 따로(D-36) |
| 모자 소품 | FP 스켈레톤에 `Cap` 본이 없다 → FP 던지기 몽타주에 **소품 부착 노티파이 스테이트**(새 C++ `UDRAnimNotifyState_AttachProp`)로 시작~릴리스 동안 손에 `Bellboy_Cap` 을 붙인다(D-38, STEP 0 결과에 따라) |
| 모자 발사 위치 | GA 에 `FirstPersonReleaseSocketName` — FP 메시의 **루트 부모 고정 소켓**에서 발사(서버·클라 같은 위치). 비우면 지금처럼 TP `CapSocket`(D-39) |
| FP 피격 | `ADRCharacter` 에 범용 `FirstPersonHitReactMontage` + `Effects.HitReact` 태그 이벤트 → 소유 클라가 FP 피격 몽타주(D-40). 도발 슈퍼아머·스윙 중 차단은 그대로 존중 |
| 새 C++ | 1쌍 — `UDRAnimNotifyState_AttachProp` |
| 기존 C++ 수정 | 라이브러리 FP 헬퍼 4개 · 태그 1개(`Event.Montage.BellBoy.FPSync`) · GA 3개(FP 몽타주 칸·재생·정지) · `UDRProjectileSpell::SpawnProjectileAt` 분리 · `ADRCharacter` FP 피격 (≈ +200줄) |
| 새 에셋 | FP 몽타주 5 · `ABP_FP_BellBoy` · FP 스켈레톤 소켓 2(`TestPartHand`, `CapReleaseSocket`) + (조건부) `FPCapSocket` |
| 기존 에셋 수정 | `BP_BellBoy`(FirstPersonMesh 메시·ABP·트랜스폼, FP 피격 몽타주) · GA 3개 BP(FP 칸) · FP 시퀀스 루프 설정 |

### 0.1 타이밍 그림 (실측값 기준)

**기본 공격** — TP 0.8s(타격 노티파이 L **0.399s** / R **0.267s**), FP 0.767s(동기 노티파이 = STEP 5 에서 지정, 없으면 비례값 L 0.383 / R 0.256)
```
TP  |==== 선딜 (속도 = TP타격 / 선딜초) ====|● 타격(서버 판정)|== 후딜 (속도 = (0.8 − TP타격) / 후딜초) ==|
FP  |==== 선딜 (속도 = FP동기 / 선딜초) ====|● FPSync          |== 후딜 (속도 = (0.767 − FP동기) / 후딜초) ==|
                                      ↑ 같은 실시간(선딜초)에 도착 · 둘 다 같은 순간에 끝남
예) 첫 타(L): TP 0.399/0.4=1.00 → 후딜 0.401/0.4=1.00 · FP(비례) 0.383/0.4=0.96 → 0.384/0.4=0.96
    연속(R) : TP 0.267/0.2=1.34 → 0.533/0.1=5.33     · FP(비례) 0.256/0.2=1.28 → 0.511/0.1=5.11
```
**시선 집중** — TP `AM_BB_Taunt` 5.0s, 이벤트 0.986s · FP `FP_Taunt` 5.0s → FP 속도 = FP동기 / 0.986 (비례면 1.0)
**모자 던지기** — TP `AM_BB_ThrowCap` 0.8s, 릴리스 0.318s · FP `FP_ThrowCap` 0.833s → FP 속도 = FP동기 / 0.318 (비례면 1.04, FP 가 0.8s 에 끝남)
```
FP  |== 손에 모자 소품(ANS) ==|● FPSync = 릴리스 프레임 → 소품 사라짐 | 던진 손 회수 |
서버                           ● TP 릴리스 이벤트 → CapReleaseSocket 에서 모자 투사체 스폰
```

---

## 1. 조사 결과 — 이 계획의 근거

### 1.1 FP 에셋 (`Content/DaeRuneAssets/Characters/BellBoy/FP/`, 2026-10-15 저장분)

| 에셋 | 길이(30fps) | TP 대응 | 쓰는 곳 |
|---|---|---|---|
| `Bellboy_FP_v0_1_0` (+`_Skeleton`, `_PhysicsAsset`) | — | — | `FirstPersonMesh` |
| `FP_BasicAttack_Left` / `_Right` | 0.767 | `BB_BasicAttack_*` 0.8 | `AM_FP_BB_BasicAttack_L/R` |
| `FP_Taunt` | 5.0 | `BB_Taunt` 5.0 | `AM_FP_BB_Taunt` |
| `FP_ThrowCap` | 0.833 | `BB_ThrowCap` 0.8 | `AM_FP_BB_ThrowCap` |
| `FP_HitReact` | 0.5 | `BB_Idle_HitReact` 0.433 | `AM_FP_BB_HitReact` |
| `FP_Idle` | 1.5 | — | ABP Idle |
| `FP_Walk` | 0.5 | — | ABP Walk (블렌드스페이스 없음 — 단일 시퀀스) |
| `FP_Jump_Start` / `_Loop` / `_End` | 0.667 / 0.833 / 0.433 | (TP 는 `BB_Jump_*`) | ABP Jump / FallLoop / Land |
| `FP_Jump_Full` | 0.9 | — | **안 씀**(Start/Loop/End 로 충분 — D-41) |
| `FP_Stun` | 1.0 | `BB_Stun` 1.0 | ABP Stun |
| `BB_Red_001` · `BB_Red_003` · `BB_Red_Emission` · `BB_White` | — | — | FP 머티리얼(부모 `MI_Default_Opaque_DS` — 자판기·청소기 FP 와 같음, D-42) |

- 전부 노티파이 0개, 루프 설정 없음(임포트 기본값).
- **없는 것**: FP 부품 운반 자세 · FP 사망(불필요 — 사망 시 FP 메시를 숨긴다, `DRCharacterBase.cpp`) · 모자 본/모자 메시 → 부록 D.

### 1.2 FP 스켈레톤 본 (실측)
```
Armature_ProxyTrueRootJoint
├─ Body_001 ─ Body_002
├─ Arm_001~005_L/R ─ Hand_001~003_L/R · Hand_Thumb_001~003_L/R · Hand_X/Y/Z-rotate_L/R
├─ Arm_IK_L/R · Arm_Pole_L/R
├─ Bag
└─ MasterController
```
- **`Cap` · `CapButton` · 다리 본 없음**(TP 에는 `Body_002 → Cap`). 정확한 부모 관계는 STEP 0 에서 스켈레톤 트리로 확인.
- 소켓 0개 — `TestPartHand` 없음(T-34). 자판기·정원로봇 FP 스켈레톤에는 있다, 청소기 FP 에는 없다.
- 슬롯: 새 스켈레톤이라 `DefaultSlot` 만(T-31).

### 1.3 현재 상태 (TP 구현 완료 시점)

| 대상 | 현재 값(실측) | 이 계획에서 |
|---|---|---|
| `BP_BellBoy` › FirstPersonMesh | Skeletal Mesh **비어 있음**, Anim Class = **`ABP_FP_VendingMachine`**(자판기 BP 복제 잔재 — T-33), Loc (−15.9, 0, −28.8) Rot Yaw −90 | 메시·ABP 교체, 트랜스폼 조정 |
| `BP_BellBoy` › FollowCamera | Loc (34.9, 0, −29.9) | 필요 시 조정 |
| `BP_BellBoy` › Combat | Left = `BagSocket`, Right = `CapSocket`, HitReact = `AM_BB_Idle_HitReact`, Death = `AM_BB_Death` | 그대로 |
| `GA_BellBoy_BasicAttack` | `SwingMontages` = **[0] `AM_BB_BasicAttack_Left`, [1] `_Right`**(헤더 주석 "[0] 오른쪽" 과 반대 — T-32) · 반경 91 · 피해 30 · 넉백 400 · **`bDrawDebugHit = True`**(E-17) | FP 몽타주 배열 추가 |
| `GA_BellBoy_Taunt` | `AM_BB_Taunt`, 물 60 | FP 몽타주 추가 |
| `GA_BellBoy_HatThrow` | `AM_BB_ThrowCap`, `CombatSocket.RightHand`, 물 20 | FP 몽타주 + 발사 소켓 |
| TP 몽타주 | 전부 `UpperBody` 슬롯, `AN_MontageEvent`: 공격 L 0.399 / R 0.267, 도발 0.986, 던지기 0.318 | 그대로 |
| `ANS_BellBoy_HideCap` | **만들어지지 않음**(TP 계획 3-6) — TP 머리 모자가 던질 때 안 숨는다 | 범위 밖(E-18) |
| GA C++ | `DRBellBoyBasicAttack.cpp` 에 `(STEP 11) FP 에셋이 오면…` 주석 2곳 | 이 계획으로 대체 |

### 1.4 다른 로봇의 FP 구조 (본보기)

| 항목 | 실측 | 벨보이에 적용 |
|---|---|---|
| FP ABP | `ABP_FP_VendingMachine`: `Cast To DRCharacter`, 변수 `bShouldMove`·`bIsInAir`·`bIsStunned`·`Velocity`·`RightDirection`(+자판기 전용 `bHasAttackSpeedBuff`·`CannonScale`), 상태 Locomotion(Idle/Walk — `BS_Walk` 1D) · Jump · FallLoop · Land · Stun → `DefaultSlot` | 구조 그대로, 자판기 전용 변수 제거, Walk 는 시퀀스 |
| FP 몽타주 | `AM_Gardener_ShovelAttack` · `AM_VM_FP_CapsuleAttack` · `AM_FP_VC_Crush` — 전부 **`DefaultSlot`, 노티파이 없음** | 같게 + 동기 노티파이 1개 |
| FP 재생 | 정원로봇·자판기 = BP GA 에서 FP 메시에 `Play Montage`(1P) + 3P `PlayMontageAndWait`, 속도는 양쪽 핀(Plan5 T-26) · 청소기 = 서버 → `Client RPC` → `FirstPersonMesh->GetAnimInstance()->Montage_Play`(FP 메시가 안 보이면 생략) | 벨보이 GA 는 C++ 이라 라이브러리 헬퍼로(D-33) |
| FP 기준 발사 | 청소기 `GetCombatSocketLocation_Implementation` 이 `CombatSocket.LeftHand` 를 FP `Muzzle` 로 라우팅(서버에서도 FP 메시 소켓을 읽는다) | 캐릭터 클래스 없이 GA 칸으로(D-39) |
| FP 피격 | `Gardener_FP_HitReact` · `FP_VC_HitReact` 에셋은 있으나 **어디서도 안 쓴다** | 벨보이가 처음 — 범용 구조로(D-40) |
| FP 머티리얼 | 정원로봇만 `MI_Default_Opaque_DS_VM`(뷰모델 깊이 압축 — `M_ViewmodelBase`), 자판기·청소기는 기본 | 기본 유지, 벽 파고듦이 보이면 전환(D-42) |
| FP 메시 컴포넌트 | `ADRCharacter` 생성자: 카메라 부착 · `OnlyOwnerSee` · 그림자 끔 · `BoundsScale 4` / `FirstPersonPartMesh` 는 FP 메시 `TestPartHand` 에 부착 | 그대로 쓴다 |

### 1.5 피격 경로 (FP 피격을 끼울 자리)
- 서버 `UDRPlayerAttributeSet::ApplyHitReactAndKnockback` → `MulticastPlayHitReactFacial`(**매 피격, 차단 여부 무관**) → `TryActivateAbilitiesByTag(Effects.HitReact)` → `GA_HitReact`(BP) 가 TP `GetHitReactMontage` 재생 + `GE_HitReact`(Infinite, **Target Tags `Effects.HitReact`**) 적용, 몽타주 끝에 제거.
- 플레이어 ASC 는 PlayerState · `Mixed` 복제 → GE 가 소유 클라로 복제되어 **소유 클라에서도 `Effects.HitReact` 태그 이벤트가 온다**.
- 벨보이 GA 의 `BlockAbilitiesWithTag` 에 `Effects.HitReact` 가 있다(스윙·도발·던지기 중 피격 경직 없음) → 이때는 GA_HitReact 가 안 켜져 태그도 안 붙는다 → **태그 이벤트에 걸면 "TP 가 경직할 때만 FP 도 경직" 이 자동으로 맞는다**(D-40). 멀티캐스트에 걸면 차단 중에도 FP 만 움찔한다.

---

## 2. 전체 구조

```
[소유 클라 / 리슨 호스트 자기 폰]                              [서버]
 GA ActivateAbility ─┬─ TP PlayMontageAndWait(UpperBody) ───── TP 몽타주(복제) ─ AN_MontageEvent ─► 판정·스폰
                     └─ PlayFirstPersonMontage(FP, 속도) ──► FirstPersonMesh(ABP_FP_BellBoy · DefaultSlot)
 TP 노티파이(소유 클라에서도 TP 메시가 틱) ─► OnHitEvent ─► SetSwingPlayRate(TP후딜, FP후딜)
 취소(피격·서버 거절) ─► EndAbility(cancelled) ─► StopFirstPersonMontage
 GE_HitReact 복제 ─► Effects.HitReact 태그 이벤트 ─► ADRCharacter ─► FP 피격 몽타주
 AM_FP_BB_ThrowCap ─ ANS AttachProp(시작~릴리스) ─► 손에 Bellboy_Cap (OnlyOwnerSee)
                                                              릴리스 ─► SpawnProjectileAt(FP CapReleaseSocket)
```

## 3. 작업 순서 한눈에
- [ ] STEP 0 준비 · 에셋 점검(모자 유무 분기)
- [x] STEP 1 [C++] 라이브러리 FP 헬퍼 + 태그
- [x] STEP 2 [C++] GA 3종 FP 재생 · 모자 발사 위치
- [x] STEP 3 [C++] FP 피격 · 소품 노티파이 스테이트 → 빌드
- [ ] STEP 4 [에디터] FP 스켈레톤 소켓 · 시퀀스 설정
- [ ] STEP 5 [에디터] FP 몽타주 5개
- [ ] STEP 6 [에디터] `ABP_FP_BellBoy`
- [ ] STEP 7 [에디터] `BP_BellBoy` FP 연결 + 1차 테스트(이동·점프·스턴·피격)
- [ ] STEP 8 [에디터] GA 3종 FP 칸 + 2차 테스트(스킬)
- [ ] STEP 9 검증 체크리스트

---

## STEP 0. 준비

1. 에디터를 닫은 상태에서 브랜치 `feat/bellboy_fp` 확인, FP 에셋 21개가 커밋되어 있는지(현재 미추적 → 먼저 커밋 권장).
2. **`Bellboy_FP_v0_1_0` 을 열어 확인** — 결과에 따라 D-38 분기:
   - (a) 손에 모자 지오메트리가 **없다** → STEP 3-2 의 소품 노티파이 스테이트를 쓴다(기본 가정).
   - (b) 모자가 손 본에 스킨되어 **항상 보인다** → 소품 대신 "릴리스 이후 숨김"이 필요 → 아티스트에게 모자 본 분리 요청(부록 D), 임시로 (a) 와 같이 진행하고 해당 섹션 머티리얼만 숨김 검토.
3. 스켈레톤 트리에서 `Bag`·`MasterController`·`Body_001` 의 부모, 카메라 기준 정면(+X/+Y) 방향을 메모 — STEP 4·7 의 소켓·트랜스폼 기준.
4. `FP_ThrowCap` 을 재생해 **모자가 손을 떠나는 프레임**, `FP_BasicAttack_L/R` 에서 **가방이 화면 중앙을 지나는 프레임**, `FP_Taunt` 에서 **포즈가 잡히는 프레임**을 메모 — STEP 5 의 `FPSync` 위치.

✅ 확인: 분기 (a)/(b) 결정, 프레임 3종 메모.

---

## STEP 1. [C++] 라이브러리 FP 헬퍼 + 태그

### 1-1. 태그 — `Public/DRGameplayTags.h` · `Private/DRGameplayTags.cpp`
- `Event_Montage_BellBoy_FPSync` = `"Event.Montage.BellBoy.FPSync"` — 기존 `Event_Montage_BellBoy_HatThrow` 바로 아래(.cpp ≈909~922 블록). 주석: `Plan8(FP) FP 몽타주 동기 지점 — 아무 GA 도 기다리지 않는다 (D-35)`.

### 1-2. 헬퍼 — `Public/AbilitySystem/DRAbilitySystemLibrary.h` · `.cpp`
`FindMontageEventTime` 바로 아래 `// ========== ★Plan8(FP)★ 1인칭 몽타주 ==========` 블록에 4개(전부 `BlueprintCallable`/`Pure` — 나중에 BP GA 도 쓸 수 있게):

| 함수 | 동작 |
|---|---|
| `static UAnimMontage* PlayFirstPersonMontage(AActor* Avatar, UAnimMontage* Montage, float PlayRate = 1.f)` | `ADRCharacter` 이고 **`IsLocallyControlled()`** 이고 `FirstPersonMesh` 가 **보이고**(T-39) AnimInstance 가 있으면 `Montage_Play(Montage, PlayRate)` → 재생한 몽타주 반환, 아니면 `nullptr` |
| `static void SetFirstPersonMontagePlayRate(AActor* Avatar, UAnimMontage* Montage, float PlayRate)` | 같은 조건 + `Montage_IsPlaying(Montage)` 일 때만 `Montage_SetPlayRate` — FP 는 로컬 전용이라 ASC 경로를 쓰지 않는다(T-37) |
| `static void StopFirstPersonMontage(AActor* Avatar, UAnimMontage* Montage, float BlendOutTime = 0.1f)` | 재생 중이면 `Montage_Stop(BlendOutTime, Montage)` |
| `static float GetFirstPersonSyncTime(const UAnimMontage* FPMontage, const FGameplayTag& SyncTag, const UAnimMontage* TPMontage, float TPEventTime)` | `FindMontageEventTime(FPMontage, SyncTag)` 가 있으면 그 값, 없으면 **비례값** `TPEventTime × FP길이 / TP길이`(경고 없음 — 노티파이를 안 달아도 거의 맞는다, D-35) |

- `#include "Character/DRCharacter.h"` 는 .cpp 에 이미 있는지 확인(탑승칩 코드가 `ADRCharacter` 를 쓴다).

✅ 확인: 컴파일(헤더 변경 → 에디터 닫고 빌드, T-19).

---

## STEP 2. [C++] GA 3종 FP 재생

공통 규칙
- FP 칸은 **비워 두면 아무 일도 안 한다**(TP 동작 100% 동일) — 에셋 연결 전에도 빌드·플레이 가능.
- 재생은 `ActivateAbility` 에서 TP 몽타주 태스크를 만든 **직후**. 서버 전용 폰(리모트 클라의 서버 인스턴스)에서는 헬퍼가 알아서 건너뛴다(D-33).
- 이번에 튼 FP 몽타주를 멤버 `ActiveFirstPersonMontage`(TObjectPtr, 비복제)에 기억 → `EndAbility(bWasCancelled = true)` 에서 `StopFirstPersonMontage`(D-37). 정상 종료면 그대로 둬서 끝까지 블렌드아웃.
- `(STEP 11)` 주석 2곳은 지우고 `Plan8(FP)` 주석으로 바꾼다.

### 2-1. 기본 공격 — `DRBellBoyBasicAttack.h/.cpp`
헤더 추가
```cpp
// ========== ★Plan8(FP)★ ==========
/** FP 스윙 몽타주 — ★SwingMontages 와 같은 순서·같은 개수★ (T-32). DefaultSlot, 게임플레이 이벤트 노티파이 금지 (T-30) */
UPROPERTY(EditDefaultsOnly, Category = "BellBoy|FirstPerson")
TArray<TObjectPtr<UAnimMontage>> FirstPersonSwingMontages;

/** FP 몽타주에서 "TP 타격과 같은 순간"을 표시하는 노티파이 태그 (Event.Montage.BellBoy.FPSync) — 없으면 길이 비례 (D-35) */
UPROPERTY(EditDefaultsOnly, Category = "BellBoy|FirstPerson")
FGameplayTag FirstPersonSyncTag;
```
private: `TObjectPtr<UAnimMontage> ActiveFirstPersonMontage;` · `float PendingFirstPersonRecoveryRate = 1.f;`

`ActivateAbility` 변경
1. `NextSwingIndex` 를 올리기 **전에** `const int32 SwingIndex = NextSwingIndex;` 를 잡는다(FP 배열 인덱스용).
2. TP 속도 계산(②) 직후:
```cpp
// Plan8(FP) FP 는 자기 동기 프레임이 TP 타격과 같은 실시간(WindupTime)에 오도록 따로 속도를 준다 (D-36)
UAnimMontage* FPMontage = FirstPersonSwingMontages.IsValidIndex(SwingIndex) ? FirstPersonSwingMontages[SwingIndex].Get() : nullptr;
float FPWindupRate = WindupRate;
PendingFirstPersonRecoveryRate = PendingRecoveryRate;
if (FPMontage)
{
    const float FPHitTime = UDRAbilitySystemLibrary::GetFirstPersonSyncTime(FPMontage, FirstPersonSyncTag, Montage, HitTime);
    FPWindupRate = FPHitTime / FMath::Max(0.01f, WindupTime);
    PendingFirstPersonRecoveryRate = FMath::Max(0.01f, FPMontage->GetPlayLength() - FPHitTime) / FMath::Max(0.01f, RecoveryTime);
}
```
3. TP `MontageTask->ReadyForActivation()` 뒤: `ActiveFirstPersonMontage = UDRAbilitySystemLibrary::PlayFirstPersonMontage(GetAvatarActorFromActorInfo(), FPMontage, FPWindupRate);`

`SetSwingPlayRate(float NewRate)` → 끝에 FP 도: `UDRAbilitySystemLibrary::SetFirstPersonMontagePlayRate(GetAvatarActorFromActorInfo(), ActiveFirstPersonMontage, PendingFirstPersonRecoveryRate);` (서버 분기의 `return` 앞에도 — 리슨 호스트 자기 폰은 서버이면서 FP 를 본다. 즉 TP 처리 뒤 공통으로 호출되게 구조를 바꾼다.)

`EndAbility`: `if (bWasCancelled) StopFirstPersonMontage(...)`; `ActiveFirstPersonMontage = nullptr;`
- 연속 공격은 다음 스윙의 FP 몽타주가 같은 슬롯에서 이전 것을 자연히 밀어낸다 — 정상 종료 때 멈추지 않는 이유.

### 2-2. 시선 집중 — `DRBellBoyTaunt.h/.cpp`
- 칸: `FirstPersonTauntMontage`, `FirstPersonSyncTag`. 멤버 `ActiveFirstPersonMontage`.
- TP 태스크 뒤:
```cpp
// Plan8(FP) FP 포즈 프레임이 TP 판정 시작(0.986s)과 같은 순간에 (D-35)
const float TPEventTime = UDRAbilitySystemLibrary::FindMontageEventTime(TauntMontage, TauntStartEventTag);
float FPRate = 1.f;
if (FirstPersonTauntMontage && TPEventTime > 0.f)
{
    FPRate = UDRAbilitySystemLibrary::GetFirstPersonSyncTime(FirstPersonTauntMontage, FirstPersonSyncTag, TauntMontage, TPEventTime) / TPEventTime;
}
ActiveFirstPersonMontage = UDRAbilitySystemLibrary::PlayFirstPersonMontage(GetAvatarActorFromActorInfo(), FirstPersonTauntMontage, FPRate);
```
- `EndAbility` 오버라이드 추가(지금은 없음) — 취소면 FP 정지.
- (겸사) `[TauntDiag]` 임시 로그 6줄은 이 STEP 에서 지우지 않는다 — 별도 정리(E-17).

### 2-3. 모자 던지기 — `DRBellBoyHatThrow.h/.cpp` + `DRProjectileSpell.h/.cpp`
칸: `FirstPersonThrowMontage`, `FirstPersonSyncTag`, `FName FirstPersonReleaseSocketName`(기본 None). 멤버 `ActiveFirstPersonMontage`.
- FP 재생: 도발과 같은 식(TP 이벤트 = `ReleaseEventTag`, TP 속도 1).
- **발사 위치(D-39)** — `OnRelease`(서버):
```cpp
// Plan8(FP) FP 메시의 고정 소켓에서 — 서버·소유 클라가 같은 위치를 본다 (D-39 / T-36). 비우면 TP CapSocket
FVector SpawnLocation = ICombatInterface::Execute_GetCombatSocketLocation(Avatar, ThrowSocketTag);
if (!FirstPersonReleaseSocketName.IsNone())
{
    if (const ADRCharacter* DRChar = Cast<ADRCharacter>(Avatar);
        DRChar && DRChar->FirstPersonMesh && DRChar->FirstPersonMesh->DoesSocketExist(FirstPersonReleaseSocketName))
    {
        SpawnLocation = DRChar->FirstPersonMesh->GetSocketLocation(FirstPersonReleaseSocketName);
    }
}
SpawnProjectileAt(SpawnLocation, CalculateTargetLocation());
```
- `UDRProjectileSpell`: 기존 `SpawnProjectile` 본문을 `ADRProjectile* SpawnProjectileAt(const FVector& SpawnLocation, const FVector& TargetLocation, bool bOverridePitch = false, float PitchOverride = 0.f)`(protected, BlueprintCallable)로 옮기고, `SpawnProjectile` 은 소켓 위치를 구해 그것을 부르는 한 줄로. **다른 GA 동작 변화 없음**(같은 코드 경로).
- `EndAbility` 오버라이드 — 취소면 FP 정지.

✅ 확인: 빌드 성공. FP 칸이 비어 있는 상태로 PIE → TP 동작이 이전과 같다(회귀).

---

## STEP 3. [C++] FP 피격 · 소품 노티파이 스테이트

### 3-1. FP 피격 — `Public/Character/DRCharacter.h` · `Private/Character/DRCharacter.cpp`
- 헤더(FP 메시 블록 근처, `FirstPersonPartMesh` 아래):
```cpp
/** ★Plan8(FP)★ 소유 클라 FP 피격 몽타주. 비우면 FP 피격 없음(다른 로봇 기본값). Effects.HitReact 가 붙을 때 재생 (D-40) */
UPROPERTY(EditDefaultsOnly, Category = "Mesh|FirstPerson")
TObjectPtr<UAnimMontage> FirstPersonHitReactMontage;
```
  protected: `void FirstPersonHitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount);`
- .cpp `InitAbilityActorInfo` 의 Stun 바인딩(≈1149) 바로 뒤:
```cpp
// Plan8(FP) TP 가 실제로 경직할 때만 FP 도 — GA_HitReact 가 차단되면 태그가 안 붙는다 (D-40)
if (FirstPersonHitReactMontage)
{
    AbilitySystemComponent->RegisterGameplayTagEvent(FDRGameplayTags::Get().Effects_HitReact, EGameplayTagEventType::NewOrRemoved)
        .AddUObject(this, &ADRCharacter::FirstPersonHitReactTagChanged);
}
```
- 콜백: `if (NewCount > 0) UDRAbilitySystemLibrary::PlayFirstPersonMontage(this, FirstPersonHitReactMontage);` — 로컬 제어 검사는 헬퍼가 **호출 시점**에 한다(T-38: 바인딩 시점엔 클라 컨트롤러가 아직 없을 수 있다). 서버(리모트 폰)에서도 바인딩되지만 헬퍼가 건너뛴다.
- 파일은 UTF-8(BOM). 기존 깨진 주석(`// GAS �±� ...`)은 건드리지 않는다.

### 3-2. 소품 노티파이 스테이트 — 새 `Public/Animation/DRAnimNotifyState_AttachProp.h` · `Private/Animation/DRAnimNotifyState_AttachProp.cpp`
`UCLASS(meta = (DisplayName = "Attach Prop (DR)"))` `UDRAnimNotifyState_AttachProp : public UAnimNotifyState`

| 칸 | 기본 | 설명 |
|---|---|---|
| `UStaticMesh* PropMesh` | — | `Bellboy_Cap` |
| `FName SocketName` | `FPCapSocket` | 붙일 소켓/본 |
| `FTransform RelativeTransform` | Identity | 소켓 기준 보정 |

- `NotifyBegin(MeshComp, …)`: `NewObject<UStaticMeshComponent>(MeshComp->GetOwner())` → 메시 설정, `NoCollision`, 그림자 끔, **`SetOnlyOwnerSee(MeshComp->bOnlyOwnerSee)` · `SetOwnerNoSee(MeshComp->bOwnerNoSee)`**(FP 메시를 따라 주인만 보게), FP 메시면 `BoundsScale = 4`(기존 FP 컴포넌트와 같은 이유), `SetupAttachment(MeshComp, SocketName)` + `SetRelativeTransform` + `RegisterComponent`. 노티파이 인스턴스는 에셋당 1개라 여러 캐릭터가 공유 → `TMap<TWeakObjectPtr<USkeletalMeshComponent>, TWeakObjectPtr<UStaticMeshComponent>>` 로 메시별 보관.
- `NotifyEnd`: 맵에서 찾아 `DestroyComponent()`.
- 에디터 미리보기(페르소나)에서도 그대로 보인다(Owner = 프리뷰 액터).
- `DaeRune.Build.cs` 모듈 의존성 변경 불필요(Engine).
- 중간에 끊겨도(피격·서버 거절) NotifyEnd 가 오는지 STEP 9 에서 확인(T-35). 안 오면 GA `EndAbility` 에서 소품 정리 훅 추가.

✅ 확인: 에디터 닫고 빌드(새 UCLASS·헤더 — T-19). 경고 0.

---

## STEP 4. [에디터] FP 스켈레톤 · 시퀀스

### 4-1. 소켓 — `Bellboy_FP_v0_1_0_Skeleton`

| 소켓 | 부모 본 | 위치 | 쓰는 곳 |
|---|---|---|---|
| `TestPartHand` | 두 손 사이가 되는 본(예: `Hand_001_R`) 또는 루트 | 화면 하단 중앙, 부품이 보일 위치 | `FirstPersonPartMesh`(C++ 생성자가 이 이름에 붙인다 — T-34) |
| `CapReleaseSocket` | **`Armature_ProxyTrueRootJoint`**(고정) | 오른손 앞, 카메라보다 약간 아래·앞(캡슐 밖) | 모자 발사 위치(D-39). 손 본에 달면 서버(FP 애니 없음)와 클라 위치가 갈린다(T-36) |
| `FPCapSocket` (분기 a) | 모자를 쥐는 손 본(`Hand_001_R` 등) | 손바닥 | 소품 노티파이(3-2) |

- 미리보기: `FPCapSocket` 에 Add Preview Asset → `Bellboy_Cap` 으로 크기·각도 맞춤(`FP_ThrowCap` 재생하며).
- `Slot Manager`: 추가할 것 없음(`DefaultSlot` 만 사용 — D-34).

### 4-2. 시퀀스
- **Loop 켜기**: `FP_Idle`, `FP_Walk`, `FP_Jump_Loop`, `FP_Stun`. 나머지는 끈 채.
- (선택) `FP_Walk` Rate Scale 은 ABP 에서 속도 비례로 준다(6-3) — 시퀀스는 1.0 유지.

✅ 확인: 소켓 3개(분기 b 면 2개) 미리보기 위치가 자연스럽다.

---

## STEP 5. [에디터] FP 몽타주 5개

폴더 `BellBoy/FP/`, 원본 우클릭 → Create → Create AnimMontage. **슬롯 `DefaultGroup.DefaultSlot`**(T-31).

| 몽타주 | 원본 | 노티파이 | Blend In / Out |
|---|---|---|---|
| `AM_FP_BB_BasicAttack_L` | `FP_BasicAttack_Left` | `AN_MontageEvent` · Event Tag **`Event.Montage.BellBoy.FPSync`** — 가방이 화면 중앙을 지나는 프레임(비례 기준값 0.383) | 0.05 / 0.05 (T-4 와 같은 이유) |
| `AM_FP_BB_BasicAttack_R` | `FP_BasicAttack_Right` | `AN_MontageEvent` · Event Tag **`Event.Montage.BellBoy.FPSync`** — 가방이 화면 중앙을 지나는 프레임(비례 0.256) | 0.05 / 0.05 |
| `AM_FP_BB_Taunt` | `FP_Taunt` | `AN_MontageEvent` · Event Tag **`Event.Montage.BellBoy.FPSync`** — 포즈 잡히는 프레임(비례 0.986) | 0.15 / 0.25 (TP 와 같게) |
| `AM_FP_BB_ThrowCap` | `FP_ThrowCap` | `AN_MontageEvent` · Event Tag **`Event.Montage.BellBoy.FPSync`** — **모자가 손을 떠나는 프레임**(비례 0.331) + **`Attach Prop (DR)`** 노티파이 스테이트 0 → 그 FPSync 노티파이 프레임(PropMesh `Bellboy_Cap`, Socket `FPCapSocket`) | 0.1 / 0.15 |
| `AM_FP_BB_HitReact` | `FP_HitReact` | 없음 | 0.05 / 0.1 |

- ★**FP 몽타주 4개(공격 L/R · 도발 · 던지기) 모두 같은 노티파이(`AN_MontageEvent`)·같은 태그(`Event.Montage.BellBoy.FPSync`)** 를 하나씩 단다 — 몽타주마다 위치(프레임)만 다르다. 각 GA 는 자기 FP 몽타주에서 이 태그의 시각을 읽어 재생 속도를 정한다.
- ★**`Event.Montage.BellBoy.BasicAttack` / `.Taunt` / `.HatThrow` 태그를 FP 몽타주에 절대 넣지 않는다**(T-30).
- `FPSync` 를 안 달아도 동작한다(길이 비례 — D-35). 달면 그 프레임이 TP 판정 순간과 정확히 겹친다.
- 노티파이 시각은 Details › Link › Time 으로 정확히.

✅ 확인: 몽타주 에디터에서 `AM_FP_BB_ThrowCap` 재생 → 시작부터 손에 모자, 릴리스 프레임에서 사라진다.

---

## STEP 6. [에디터] `ABP_FP_BellBoy`

### 6-1. 만들기
- `Content/Blueprints/Character/PlayerCharacter/BellBoy/` → Animation Blueprint → 부모 `AnimInstance`, 스켈레톤 **`Bellboy_FP_v0_1_0_Skeleton`** → `ABP_FP_BellBoy`.
- 스켈레톤이 달라 자판기 ABP 를 복제해 쓸 수 없다 → **노드 복사·붙여넣기** 후 시퀀스 교체.

### 6-2. 이벤트 그래프 — `ABP_FP_VendingMachine` 에서 복사
- `Cast To DRCharacter` 유지(벨보이 BP 로 캐스트 금지 — 순환 의존). 변수: `DRCharacter`, `Character Movement`, `Velocity`, `bShouldMove`, `bIsInAir`, `bIsStunned`(+선택 `WalkPlayRate`).
- 자판기 전용 `bHasAttackSpeedBuff` · `CannonScale` · `RightDirection`(1D 블렌드스페이스용) 노드는 지운다.

### 6-3. 애님 그래프
```
State Machine "Main States" ──► Slot 'DefaultGroup.DefaultSlot' ──► Output Pose
```

| Main States | 내용 |
|---|---|
| `Locomotion`(서브 SM) | `Idle` = `FP_Idle`(Loop) · `Walk` = `FP_Walk`(Loop, Play Rate = `WalkPlayRate` 선택: 수평속도 / MaxWalkSpeed, 0.5~1.5 클램프) · 전이 `bShouldMove` / `NOT bShouldMove` |
| `Jump` | `FP_Jump_Start`(Loop ✗) |
| `FallLoop` | `FP_Jump_Loop`(Loop ✓) |
| `Land` | `FP_Jump_End`(Loop ✗) |
| `Stun` | `FP_Stun`(Loop ✓) |

| 전이 | 조건 (= 자판기 FP) |
|---|---|
| Locomotion → Jump | `bIsInAir` |
| Jump → FallLoop | 남은 시간 비율(Automatic Rule) |
| FallLoop → Land / Jump → Land | `NOT bIsInAir` |
| Land → Locomotion | Automatic Rule (또는 `bShouldMove` 로 빠른 탈출) |
| (별칭: 모든 상태) → Stun / Stun → Locomotion | `bIsStunned` / `NOT bIsStunned` |

- `FP_Jump_Full` 은 쓰지 않는다(D-41).
- 부품 운반 상태는 없다(FP 운반 자세 에셋 없음 — 부록 D). `FirstPersonPartMesh` 만 보인다.

✅ 확인: 컴파일 경고 0, 미리보기 Idle 정상.

---

## STEP 7. [에디터] `BP_BellBoy` FP 연결 + 1차 테스트

| 컴포넌트 · 칸 | 값 |
|---|---|
| FirstPersonMesh › Skeletal Mesh | `Bellboy_FP_v0_1_0` |
| FirstPersonMesh › Anim Class | **`ABP_FP_BellBoy`**(자판기 ABP 잔재 교체 — T-33) |
| FirstPersonMesh › Transform | 시작값 = 자판기 값 (−15.9, 0, −28.8) / Yaw −90 → PIE 에서 팔·가방이 화면 하단 좌우에 자연스럽게 오도록 조정. 시야 가림이 크면 X 를 뒤로 |
| FirstPersonPartMesh › Scale | 0.3 그대로(자판기와 같음), 위치는 `TestPartHand` 소켓으로 |
| Class Defaults › **First Person Hit React Montage** | `AM_FP_BB_HitReact` |
| (확인만) Player Character Class | CDO 값이 `EPlayerCharacterClass_MAX` 로 파싱됨 — 자판기 BP 도 같다(E-16). FP 와 무관, 이번엔 건드리지 않는다 |

1차 테스트 (PIE · Listen Server · 플레이어 2, 둘 다 벨보이로 한 번, 호스트/클라 각각)
- 내 화면에 팔·가방이 보인다 · 걷기 흔들림 · 점프 Start→Loop→End · 스턴(적 넉백 벽꿍 or 디버그) 시 FP 스턴 포즈
- 피격 시 FP 움찔 / **도발 중 피격 시 FP 도 안 움찔**(슈퍼아머 — D-40)
- 다른 플레이어 화면에는 FP 팔이 안 보인다(OnlyOwnerSee) · 내 화면에 TP 몸이 안 보인다
- 사망 시 FP 사라짐(기존 코드) · 부품을 들면 FP 화면에 부품이 `TestPartHand` 위치에
- 청소기 위 탑승 · 대기실 FreeRoam · 고정 카메라 구간에서 FP 표시 전환이 다른 로봇과 같다

✅ 확인: 위 전부. 아직 스킬 FP 는 없다(TP 만 재생).

---

## STEP 8. [에디터] GA 3종 FP 칸 + 2차 테스트

| GA BP | 칸 | 값 |
|---|---|---|
| `GA_BellBoy_BasicAttack` | First Person Swing Montages | **[0] `AM_FP_BB_BasicAttack_L`, [1] `AM_FP_BB_BasicAttack_R`** — TP 배열 순서(Left, Right)와 같게(T-32) |
| 〃 | First Person Sync Tag | `Event.Montage.BellBoy.FPSync` |
| 〃 | (겸사) Draw Debug Hit | **끄기**(현재 True — E-17) |
| `GA_BellBoy_Taunt` | First Person Taunt Montage / First Person Sync Tag | `AM_FP_BB_Taunt` / `Event.Montage.BellBoy.FPSync` |
| `GA_BellBoy_HatThrow` | First Person Throw Montage / First Person Sync Tag | `AM_FP_BB_ThrowCap` / `Event.Montage.BellBoy.FPSync` |
| 〃 | First Person Release Socket Name | `CapReleaseSocket` (비우면 TP `CapSocket` — D-39) |

2차 테스트
- LMB 한 번 = FP L 스윙, 누르고 있기 = L/R 교대·연속 속도, TP(다른 화면)와 박자가 같다
- 연속 중 피격 → FP 스윙이 0.1초 블렌드로 끊기고 FP 피격으로
- E = FP 도발 5초, 걸으면서도 유지, 1초 지점 포즈와 적 반응 시점이 맞다
- RMB = 손에 모자 → 릴리스 프레임에 소품 사라짐과 **같은 순간** 화면 오른쪽 아래에서 모자가 조준점으로 날아간다 · 다른 플레이어 화면에서도 모자가 벨보이 얼굴 앞에서 나간다
- 벽에 붙어 RMB → 모자가 즉시 바닥에 떨어지는지(T-36 — 과하면 소켓을 뒤로)

✅ 확인: 위 전부(호스트·클라 모두).

---

## STEP 9. 검증 체크리스트

### 9.1 동기화
- [ ] FP `FPSync` 를 일부러 지운 몽타주로도 동작(비례 폴백) · 경고 로그 없음
- [ ] `SkillWindup` 칩이 생겨 선딜이 바뀌어도 TP·FP 가 같이 늘어난다(같은 `WindupTime` 을 쓴다)
### 9.2 멀티
- [ ] 리슨 호스트 자기 벨보이: FP 재생 1회(중복 없음), 후딜 속도 전환 FP 반영
- [ ] 클라 벨보이: 서버가 활성화를 거절(물 부족 경합 등)하면 FP 몽타주가 멈춘다
- [ ] 클라 2명이 동시에 던질 때 소품이 서로의 화면에 안 보인다(OnlyOwnerSee)
### 9.3 끊김·정리
- [ ] 던지기 도중 스턴/사망 → 손의 소품이 남지 않는다(T-35)
- [ ] 도발 도중 사망 → FP 메시 숨김, 부활 후 FP 정상
### 9.4 회귀
- [ ] 정원로봇·자판기·청소기: 투사체 스폰 위치·FP 동작 변화 없음(`SpawnProjectile` 리팩터)
- [ ] 다른 로봇은 `FirstPersonHitReactMontage` 비어 있어 FP 피격 없음(기존과 동일)
- [ ] TP 판정 시점(서버) 변화 없음 — 디버그 구 타이밍 비교

---

## 부록 A. 튜닝 창구 (FP 분)

| 수치 | 바꾸는 곳 |
|---|---|
| FP 팔 위치·각도 | `BP_BellBoy` › FirstPersonMesh Transform |
| FP 동기 프레임 | 각 FP 몽타주의 `FPSync` 노티파이 위치(코드가 읽는다) |
| 모자 소품 위치·시간 | FP 스켈레톤 `FPCapSocket` · `AM_FP_BB_ThrowCap` 의 `Attach Prop (DR)` 구간 · RelativeTransform |
| 모자 발사 위치 | FP 스켈레톤 `CapReleaseSocket` / GA `First Person Release Socket Name`(비우면 TP) |
| FP 걷기 속도감 | `ABP_FP_BellBoy` › `WalkPlayRate` |
| FP 피격 | `BP_BellBoy` › First Person Hit React Montage(비우면 끔) · `AM_FP_BB_HitReact` 블렌드 |
| (TP 수치) | git `3fe5b9f9^:Plan8.md` 부록 A — 반경은 BP 실측 91 |

## 부록 B. 결정 (D-n)

### B-1. TP 결정 요약 (D-1~D-32, 원문 `3fe5b9f9^:Plan8.md`)
| # | 요약 |
|---|---|
| D-1 | 벨보이 전용 C++ 캐릭터 없음 — `BP_BellBoy` 는 `ADRCharacter` 직속 (FP 에서도 유지 — D-39) |
| D-2 | `EPlayerCharacterClass::BellBoy` 는 `Count` 바로 앞 |
| D-3 | (폐기 → 이 문서) FP 에셋 없이 진행 |
| D-4 | 선딜/후딜 = 타격 전·후 재생 속도를 따로 |
| D-5 | 타격 시각 = 몽타주 `AN_MontageEvent` 시각을 코드가 반사로 읽음 |
| D-6 | 연속 = 앞 스윙 종료 후 0.25초 안, 좌우 교대, 끊기면 첫 타 |
| D-7 | 다른 스킬 전환 후딜 = Block Abilities GE (선딜+0.4초) |
| D-8 | 판정 창 0.1초 · 4회 샘플 · 대상당 1회 |
| D-9 | 넉백 = 기존 파이프라인(45° 위) |
| D-10 | 후딜 속도 전환 = 서버 `CurrentMontageSetPlayRate` / 소유 클라 로컬 AnimInstance |
| D-11 | 도발 효과 = 이속 ×1.1 + 주는 피해 ×0.8 |
| D-12 | 도발 지속 = 걸린 순간부터 5초, 도발자 사망 시 해제 |
| D-13 | 도발 = GE(`Debuff.Taunted`+이속+SetByCaller), 도발자 = OriginalInstigatorASC |
| D-14 | 도발 피해 배율 = `UDRAttributeSet::PostGameplayEffectExecute` + 사이트 AS |
| D-15 | 새 도발이 기존 도발을 지움, 한 시전에 적당 1회 |
| D-16 | AI 연결 = `ApplyTauntAggro` 키 3개 + 0.2초 재기록 |
| D-17 | 타깃 키 없는 적(부품 도둑)은 도발 제외 |
| D-18 | 도발 중 슈퍼아머(`Effects.HitReact` Block) |
| D-19 | 도발 판정 0.5초·0.1초 간격·30m·시야 검사 없음 |
| D-20 | 모자 GE 2종(Single/Double), 부모 태그 `Buff.BellBoy.Hat` |
| D-21 | 모자 무효화 = `PostGameplayEffectExecute` 맨 앞, 도트도 1회 |
| D-22 | 모자 최대 1개, 이미 쓴 아군에 닿으면 바닥에 떨어짐 |
| D-23 | 2회는 던진 본인이 주웠을 때만 |
| D-24 | 벽·바닥 = Pawn 채널 Block, 사이트도 벽 |
| D-25 | 픽업은 0.25초마다 겹침 재검사 |
| D-26 | 모자 비주얼 = GameplayCue 액터, `HatSocket`/캡슐 꼭대기, OwnerNoSee |
| D-27 | 모자 GE 클래스 = `DA_GameBalanceConfig.BellBoy` |
| D-28 | 키 LMB/E/RMB |
| D-29 | 쿨다운 없음 |
| D-30 | 스탯 GE 정원로봇과 공유 |
| D-31 | 캐릭터 BP 는 `BP_VendingMachine` 복제 |
| D-32 | 기본 공격 대상 수집 = `GetLiveObjectsWithinRadius` |

### B-2. FP 결정 (D-33~)
| # | 결정 | 근거 |
|---|---|---|
| **D-33** | FP 몽타주는 **로컬 제어 폰에서만** 라이브러리 헬퍼(`PlayFirstPersonMontage` 등)로 재생. 서버 판정은 TP 몽타주 그대로 | FP 메시는 OnlyOwnerSee — 다른 기계에서 틀 이유가 없다. 벨보이 GA 는 C++ 이라 BP `Play Montage`(1P) 패턴을 못 쓰고, 청소기식 Client RPC 는 GA 가 이미 소유 클라에서도 돌아(LocalPredicted) 불필요. 헬퍼는 BP 에서도 쓸 수 있게 BlueprintCallable |
| **D-34** | FP 몽타주 슬롯 = `DefaultSlot` | FP 메시엔 다리가 없어 상·하체 분리가 필요 없고, 다른 로봇 FP 몽타주가 전부 `DefaultSlot`. FP 스켈레톤엔 `UpperBody` 슬롯도 없다 |
| **D-35** | FP 동기 = FP 몽타주의 `AN_MontageEvent`(태그 `Event.Montage.BellBoy.FPSync`) 시각을 읽어 **그 프레임이 TP 이벤트와 같은 실시간**에 오도록 속도 계산. 노티파이가 없으면 길이 비례 | FP 애니 길이가 TP 와 다르다(0.767 vs 0.8, 0.833 vs 0.8). D-5 와 같은 철학 — 노티파이를 옮기면 코드가 따라온다. 대기 GA 가 없는 태그라 이벤트가 나가도 무해 |
| **D-36** | 기본 공격 FP 는 선딜 속도(`FP동기 / 선딜초`)와 후딜 속도(`(FP길이 − FP동기) / 후딜초`)를 TP 와 **따로 계산**, `SetSwingPlayRate` 가 TP·FP 를 함께 전환 | D-4 를 FP 로 확장. 한 속도를 공유하면 FP 타격 프레임이 TP 판정과 어긋난다(Plan5 T-26 과 같은 "손과 몸이 따로" 문제) |
| **D-37** | 취소 종료(`bWasCancelled`)에서만 FP 몽타주를 0.1초 블렌드로 정지. 정상 종료는 그대로 | 연속 공격은 다음 FP 스윙이 같은 슬롯에서 밀어내므로 정상 종료에 멈추면 끊겨 보인다. 서버 거절·피격 끊김은 TP 가 끝났는데 FP 만 휘두르는 것을 막아야 한다 |
| **D-38** | FP 손의 모자 = 새 C++ `UDRAnimNotifyState_AttachProp` 로 FP 던지기 몽타주 시작~릴리스 동안 `Bellboy_Cap` 을 손 소켓에 부착(STEP 0 분기 a) | FP 스켈레톤에 `Cap` 본이 없다. GA 코드가 아니라 몽타주가 구간을 들고 있어 타이밍 조정이 에디터에서 끝나고, 다른 로봇 소품에도 재사용된다 |
| **D-39** | 모자 발사 위치 = GA 칸 `FirstPersonReleaseSocketName` → FP 메시의 **루트 부모 고정 소켓**(`CapReleaseSocket`). 비우면 TP `CapSocket`. 전용 캐릭터 클래스 없이 `UDRProjectileSpell::SpawnProjectileAt` 분리로 해결 | FP 화면에서 머리 위(카메라 뒤)에서 모자가 나오면 손 동작과 어긋난다. 청소기 선례(서버도 FP 소켓을 읽음)를 따르되, 고정 소켓이라 서버(FP 애니 없음)·클라 위치가 같다. D-1 유지 |
| **D-40** | FP 피격 = `ADRCharacter::FirstPersonHitReactMontage`(범용, 기본 비움) + `Effects.HitReact` 태그 이벤트 | `GE_HitReact` 태그는 GA_HitReact 가 실제로 켜질 때만 붙어 도발 슈퍼아머·스윙 차단과 자동으로 맞는다. `MulticastPlayHitReactFacial` 은 차단 중에도 매번 온다. 다른 로봇도 에셋만 넣으면 쓸 수 있다 |
| **D-41** | `ABP_FP_BellBoy` = 자판기 FP ABP 구조 복사, Walk 는 `FP_Walk` 단일 시퀀스, `FP_Jump_Full` 은 안 씀 | 검증된 구조. 벨보이 FP 걷기는 블렌드스페이스 재료가 1개뿐 |
| **D-42** | FP 머티리얼은 임포트 그대로(`MI_Default_Opaque_DS`). 벽 파고듦이 보이면 정원로봇처럼 뷰모델 부모로 | 자판기·청소기와 같다. 정원로봇의 `_VM` 부모 경로는 저장소 밖(엔진 플러그인 쪽)이라 의존을 늘리지 않는다 — 전환 시 경로 확인 |
| **D-43** | 이 문서에 TP D/T/E 요약을 보존하고 FP 는 D-33·T-30·E-16 부터 | 코드 주석이 `Plan8 D-n/T-n` 을 가리키는데 원문은 git 에만 있다 |

## 부록 C. 함정 (T-n)

### C-1. TP 함정 요약 (T-1~T-29)
| # | 요약 |
|---|---|
| T-1 | Layered Blend 에 `Bag` 브랜치 필수(루트 직속) |
| T-2 | 공격·도발 몽타주를 `DefaultSlot` 에 두면 못 걷는다 → `UpperBody` |
| T-3 | Debuff Chance 기본 20 → 0 |
| T-4 | 공격 몽타주 Blend Out 기본 0.25 → 0.05~0.1 |
| T-5 | `ComboResetTime` 너무 짧으면 서버·클라 갈림 |
| T-6 | 도발자를 `GetInstigator()` 로 읽으면 PlayerState |
| T-7 | 적 탐색은 `GetAliveEnemiesWithinRadius` |
| T-8 | 도발 키 3개·전투 시간 갱신·두더지 분기 누락 |
| T-9 | `DRAttributeSet.h` 혼합 인코딩 |
| T-10 | GE 태그는 Target Tags 에 |
| T-11 | GC 모자 메시 OwnerNoSee |
| T-12 | 비행 적은 이속 배율 무효 |
| T-13 | 넉백 → 벽꿍 스턴 연속 가능 |
| T-14 | 로봇 4종 → 슬롯 해금 예산 |
| T-15 | 모자 스폰이 캡슐 안 → 자기 무시 |
| T-16 | enum 추가 후 BP Select/Switch 핀 |
| T-17 | 픽업 바닥 트레이스는 오브젝트 타입 |
| T-18 | 모자는 도트 틱도 1회 |
| T-19 | 헤더·UCLASS 변경은 Live Coding 불가 → 에디터 닫고 빌드 |
| T-20 | 생성자에서 네이티브 태그 기본값 금지 → BP 에서 |
| T-21 | 투사체 Destroyed 임팩트 연출이 장착·픽업 때도 |
| T-22 | 노티파이 없으면 판정 창이 안 끝남 → 몽타주 종료 때 처리 |
| T-23 | HideCap ANS 가 끊기면 모자 계속 숨김 |
| T-24 | `UpperBody`·`DefaultSlot` 같은 그룹 → 피격이 공격을 끊음 |
| T-25 | 모자·도발도 누르고 있으면 반복 |
| T-26 | `DA_GameBalanceConfig` 없는 모드에선 모자 무동작 |
| T-27 | 모자를 스택 GE 1개로 하면 HUD 숫자 고착 |
| T-28 | 도발 갱신에서 `UnreachableSince` 초기화 필수, 주기 < 0.4초 |
| T-29 | 부여 태그 GE 는 `MakeQuery_MatchAnyOwningTags` 로 조회 |

### C-2. FP 함정 (T-30~)
| # | 함정 | 증상 / 대응 |
|---|---|---|
| **T-30** | FP 몽타주에 `Event.Montage.BellBoy.BasicAttack/Taunt/HatThrow` 노티파이를 넣음 | FP 메시 오너도 캐릭터라 이벤트가 두 번 간다. `OnlyTriggerOnce` 라 **먼저 온 FP 이벤트가 판정을 가져가** 리슨 호스트에선 서버 판정이 FP 타이밍으로 바뀐다 → FP 는 `FPSync` 태그만 |
| **T-31** | FP 몽타주 슬롯을 `UpperBody` 로 | FP ABP·스켈레톤에 그 슬롯이 없어 **FP 몽타주가 화면에 안 나온다**(에러 없음) → `DefaultSlot` |
| **T-32** | `FirstPersonSwingMontages` 순서를 헤더 주석("[0] 오른쪽")대로 | 실제 TP 배열은 **[0] Left, [1] Right** → FP 손이 TP 와 반대로 휘두른다 → TP BP 배열 순서를 그대로 따른다 |
| **T-33** | `BP_BellBoy` FirstPersonMesh 의 Anim Class 를 안 바꾸고 메시만 넣음 | 지금 `ABP_FP_VendingMachine`(다른 스켈레톤) → 메시 지정 시 ABP 가 풀리거나 T-포즈 → 메시·ABP 를 함께 |
| **T-34** | FP 스켈레톤에 `TestPartHand` 없음 | `FirstPersonPartMesh` 가 FP 메시 원점(카메라 아래)에 붙어 부품이 엉뚱한 곳 → STEP 4-1 |
| **T-35** | 소품 노티파이 스테이트가 몽타주 중단 때 `NotifyEnd` 를 못 받음 | 손에 모자가 남는다 → STEP 9.3 으로 확인, 남으면 GA `EndAbility` 에서 정리(소품 맵 접근 함수 추가) |
| **T-36** | `CapReleaseSocket` 을 손 본에 달거나 너무 앞에 둠 | 서버엔 FP 애니가 없어 손 위치가 클라와 다르다 / 벽에 붙으면 스폰 지점이 벽 안 → 즉시 바닥 픽업(T-15 와 같은 현상) → 루트 부모, 캡슐 바로 앞 |
| **T-37** | FP 속도 전환에 `ASC::CurrentMontageSetPlayRate` 를 씀 | 그 함수는 ASC 의 "현재 몽타주"(TP)만 바꾸고 서버 RPC 를 보낸다 → FP 는 `SetFirstPersonMontagePlayRate`(AnimInstance 직접) |
| **T-38** | FP 피격 바인딩 때 `IsLocallyControlled()` 로 거름 | 클라 `OnRep_PlayerState` 시점엔 컨트롤러가 아직 없어 false → 영영 바인딩 안 됨 → 바인딩은 무조건, 판정은 콜백(헬퍼)에서 |
| **T-39** | FP 메시가 숨겨진 상태(사망·고정 카메라·FreeRoam 토글)에서 FP 몽타주 재생 | 다시 보일 때 엉뚱한 포즈 / 의미 없는 재생 → 헬퍼가 `IsVisible()` 검사(청소기 선례) |

## 부록 D. 에셋 요청 (아티스트)

| 에셋 | 없어서 지금은 | 우선순위 |
|---|---|---|
| FP 부품 운반 자세(HoldingCleanserPart) | 운반 중에도 Idle 팔 + 부품만 표시 | 중간 |
| (STEP 0 분기 b 일 때) FP 모자 본 분리 | 소품 노티파이로 대체 | 중간 |
| FP 도발 중 걷기 변형(선택) | `FP_Taunt` 가 걷는 동안에도 그대로 | 낮음 |
| (TP) 표정·Jump 분리 등 — `3fe5b9f9^:Plan8.md` 부록 D 참고 | | |

## 부록 E. 열린 이슈

### E-1. TP 열린 이슈 요약 (E-1~E-15)
E-1 두더지 보스도 도발됨(의도) · E-2 모자 쓴 채 사망·부활 시 잔존 · E-3 (FP 로 해결 — 이 문서) · E-4 연속 DPS ≈100 · E-5 쿨다운 없음 · E-6 `[MoleDiag]` 로그 증가 · E-7 벨보이 칩 없음 · E-8 공속 칩 배율 미적용 · E-9 튜토리얼 미지원 · E-10 도발된 적 공속 그대로 · E-11 모자는 사이트를 벽 취급 · E-12 기본 공격이 사이트에도 맞음 · E-13 머리 모자 숨김은 연출용 · E-14 모자 도트 소모 · E-15 연속 판정 서버·클라 시계 따로

### E-2. FP 열린 이슈 (E-16~)
| # | 내용 | 현재 판단 |
|---|---|---|
| **E-16** | `BP_BellBoy`·`BP_VendingMachine` CDO 의 `PlayerCharacterClass` 가 `EPlayerCharacterClass_MAX` 로 저장돼 있다(파싱 결과) — 스폰은 DA 매핑이라 동작은 정상 | FP 와 무관. 에디터에서 값 확인 후 별도 정리 권장 |
| **E-17** | `GA_BellBoy_BasicAttack.bDrawDebugHit = True`, `DRBellBoyTaunt.cpp` 의 `[TauntDiag]` 임시 로그 | 디버그 잔재. 디버그 구는 STEP 8 에서 끄고, 로그는 별도 정리 |
| **E-18** | TP `ANS_BellBoy_HideCap` 이 만들어지지 않아 TP 머리 모자가 던질 때 안 숨는다 | TP 연출 — 범위 밖. 만들면 3-2 의 소품 노티파이와 같은 폴더에 |
| **E-19** | 정원로봇·청소기의 FP 피격 에셋(`Gardener_FP_HitReact`·`FP_VC_HitReact`)이 놀고 있다 | D-40 덕분에 각 BP 의 `First Person Hit React Montage` 만 채우면 된다(몽타주 생성 필요) |
| **E-20** | 모자 투사체가 FP 소켓에서 나가므로 다른 플레이어 화면에선 벨보이 얼굴 앞에서 나간다(TP 던지기 손 위치와 약간 다를 수 있음) | 청소기와 같은 절충. 어색하면 `CapReleaseSocket` 을 머리 쪽으로 올리거나 칸을 비워 TP 로 되돌린다 |
| **E-21** | FP 걷기 애니(0.5초 루프)가 이동 속도와 안 맞을 수 있다(도발 이속 버프 등 아님 — 벨보이 자신) | `WalkPlayRate` 로 보정 |

