# WaterPump 애니메이션 걷기 블렌드 안 되는 문제 수정 계획

## 문제 정의

- 방안 A(ABP State Machine)로 `AM_InHoseBlast`가 모든 클라이언트에서 보이게 됨 ✅
- **하지만**: 물대포를 쏘면서 이동할 때 **다리가 고정된 채로 미끄러지듯 움직임**
- 원인: State Machine에서 `WaterPump_Loop` 상태가 **풀바디(Full Body)**로 `AM_InHoseBlast`를 재생하므로, 로코모션(걷기/달리기) 애니메이션이 완전히 덮어써짐

---

## 원인 분석

### 현재 ABP 구조 (추정)

```
AnimGraph:
    [State Machine]
        ├─ Idle/Locomotion 상태: BS_Walk (Blend Space) → 속도에 따라 Idle/Walk 블렌드
        ├─ WaterPump_Loop 상태: DEF_InHoseBlast (풀바디 루프) ← ★ 문제
        └─ ... 기타 상태들
    → Output Pose
```

`WaterPump_Loop` 상태에 진입하면 `DEF_InHoseBlast`가 **전신**을 제어한다.
상체(호스 들기 포즈)와 하체(다리 걷기)가 분리되어 있지 않으므로, 이동 중에도 다리가 InHoseBlast 포즈 그대로 고정된다.

### 해결 원리

**Layered Blend per Bone**: 특정 본(Bone)을 기준으로 상체와 하체의 애니메이션 소스를 분리한다.

```
Base Layer (전신):  Locomotion (BS_Walk) → 다리가 걷기 애니메이션 재생
Upper Layer (상체): DEF_InHoseBlast      → 상체만 호스 들기 포즈

→ Layered Blend per Bone (Spine 본 기준)
→ 결과: 상체는 물대포, 하체는 걷기
```

---

## 해결 방안

### ABP_GardenRobot AnimGraph 재구성

현재 State Machine 방식에서, **Layered Blend per Bone** 노드를 추가하여 상체/하체를 분리한다.

#### 목표 AnimGraph 구조

```
[기존 State Machine / Locomotion 출력]  ──→  Base Pose (전신 로코모션)
                                                │
                                                ▼
                                        ┌─────────────────────┐
                                        │ Layered Blend       │
                                        │   per Bone          │
                                        │                     │
                                        │ Base: Locomotion    │  ← 하체 (다리 걷기)
                                        │ Blend: WaterPump    │  ← 상체 (호스 포즈)
                                        │ Bone: "spine_01"    │  ← 블렌드 시작 본
                                        │ Alpha: 0.0 or 1.0   │  ← bIsInWaterPump로 제어
                                        └─────────────────────┘
                                                │
                                                ▼
                                          Output Pose
```

---

## 상세 구현 단계

### 단계 1: GardenRobot 스켈레톤의 Spine 본 이름 확인

ABP에서 Layered Blend per Bone을 설정하려면 **블렌드 시작 본(Branch Filter Bone)**의 정확한 이름이 필요하다.

1. `Content/DaeRuneAssets/Characters/GardenRobot/TP/Player1-Rig_Ani-3_1_Skeleton.uasset` 열기
2. 본 계층 구조에서 **척추(Spine) 본** 이름 확인
   - 일반적인 이름: `spine_01`, `spine_02`, `Spine`, `Spine1` 등
   - GardenRobot이 커스텀 리그라면 다를 수 있음
3. 상체와 하체를 나누는 적절한 본 선택
   - 보통 `spine_01` 또는 `spine_02`가 적당 (골반 위, 상체 시작점)
   - 이 본과 그 하위 모든 자식 본(팔, 머리 등)이 상체 레이어로 블렌드됨
   - 이 본의 부모 본(골반, 다리 등)은 Base Layer(로코모션)를 유지

### 단계 2: ABP_GardenRobot AnimGraph 수정

#### 2-1. State Machine에서 WaterPump_Loop 상태 제거 (또는 유지하되 변경)

기존에 State Machine 안에 만든 `WaterPump_Loop` 상태를 **State Machine 밖으로** 빼야 한다. Layered Blend per Bone은 State Machine 외부에서 두 개의 포즈를 합성하는 노드이기 때문이다.

**방법 A: State Machine을 Base 포즈로만 사용 (권장)**

```
AnimGraph:

[State Machine (Locomotion 전용)]
    ├─ Idle/Locomotion: BS_Walk
    ├─ Jump_Start / Jump_Loop / Jump_Land
    └─ (WaterPump 상태 제거)
    → Locomotion Pose

[WaterPump Animation]
    Play DEF_InHoseBlast (루프)
    → WaterPump Pose

[Layered Blend per Bone]
    Base Pose: Locomotion Pose
    Blend Poses 0: WaterPump Pose
    Branch Filter: "spine_01" (또는 해당 본 이름)
    Blend Weight 0: bIsInWaterPump ? 1.0 : 0.0
    → Output Pose
```

**방법 B: State Machine 내 상태는 유지하되 풀바디가 아닌 참조용으로만 사용**

더 복잡하고 이점이 적으므로 방법 A를 권장.

#### 2-2. Layered Blend per Bone 노드 설정

ABP_GardenRobot의 AnimGraph에서:

1. **노드 추가**: 우클릭 → "Layered blend per bone" 검색 → 추가
2. **Base Pose 연결**: State Machine(Locomotion)의 출력을 `Base Pose` 핀에 연결
3. **Blend Poses 추가**: "Add Blend Pin" 버튼으로 Blend Pose 슬롯 1개 추가
4. **Blend Pose 0 연결**: `DEF_InHoseBlast` 애니메이션 시퀀스 (루프 재생) 노드를 연결
5. **Blend Weights 0 연결**: `bIsInWaterPump` 변수를 Float로 변환(Bool to Float 또는 Select) 후 연결
   - `true` → 1.0 (상체 WaterPump 포즈)
   - `false` → 0.0 (상체도 Locomotion)
6. **노드 디테일 패널에서 Branch Filter 설정**:
   - `Layer Setup` 배열에서 `Branch Filters` 추가
   - `Bone Name`: 확인한 Spine 본 이름 (예: `spine_01`)
   - `Blend Depth`: 0 (이 본부터 모든 자식 본에 블렌드 적용)
   - `Mesh Space Rotation Blend`: 체크 (월드 공간 회전 블렌드로 더 자연스러운 결과)

#### 2-3. 블렌드 알파 부드럽게 전환 (선택사항)

급격한 전환을 피하려면 `FInterp To` 또는 `Blend Weights`에 보간을 적용:

```
Event Blueprint Update Animation:
    bIsInWaterPump (bool) → Select (True: 1.0, False: 0.0) → FInterp To (Speed: 10.0)
    → Set WaterPumpBlendAlpha (float 변수)
```

그리고 Layered Blend per Bone의 `Blend Weights 0`에 `WaterPumpBlendAlpha` 연결.

이렇게 하면 물대포 시작/종료 시 약 0.1~0.2초에 걸쳐 부드럽게 상체 포즈가 전환된다.

### 단계 3: DEF_InHoseBlast 애니메이션 확인

`Content/DaeRuneAssets/Characters/GardenRobot/TP/DEF_InHoseBlast.uasset`:

1. **루프 설정 확인**: 애니메이션 에셋을 열어 `Loop` 체크 여부 확인. 체크되어 있지 않으면 활성화
2. **루트 모션 비활성화**: Root Motion이 켜져 있으면 이동과 충돌할 수 있으므로 비활성화 확인
3. **Additive 여부**: 이 애니메이션이 Additive가 아닌 일반(Normal) 애니메이션인지 확인. Layered Blend per Bone은 둘 다 지원하지만, 일반 애니메이션이면 `Blend Mode`를 `Blend` 그대로 사용

### 단계 4: GA_WaterPump 블루프린트 정리 확인

이전 수정에서 이미 처리했어야 할 것들:
- `Play Montage(GetMesh, AM_InHoseBlast)` 노드가 제거되었는지 확인
- `Montage Stop(AM_InHoseBlast)` 노드가 제거되었는지 확인
- ABP의 `bIsInWaterPump` 구동은 `bWaterPumpActive` 리플리케이트 변수로 이루어짐

---

## 최종 AnimGraph 구조 다이어그램

```
┌─────────────────────────────────────────────────────────────┐
│                    ABP_GardenRobot AnimGraph                │
│                                                             │
│  ┌──────────────────────┐                                   │
│  │   State Machine      │                                   │
│  │   (Locomotion)       │                                   │
│  │                      │                                   │
│  │  ┌────────────────┐  │                                   │
│  │  │ Idle/Walk      │  │    ┌──────────────────────┐       │
│  │  │ (BS_Walk)      │  │    │  DEF_InHoseBlast     │       │
│  │  └────────────────┘  │    │  (Loop = true)       │       │
│  │  ┌────────────────┐  │    └──────────┬───────────┘       │
│  │  │ Jump_Start     │  │               │                   │
│  │  └────────────────┘  │               │ WaterPump Pose    │
│  │  ┌────────────────┐  │               │                   │
│  │  │ Jump_Loop      │  │               │                   │
│  │  └────────────────┘  │               │                   │
│  │  ┌────────────────┐  │               │                   │
│  │  │ Jump_Land      │  │               │                   │
│  │  └────────────────┘  │               │                   │
│  └──────────┬───────────┘               │                   │
│             │                           │                   │
│             │ Base Pose                 │ Blend Pose 0      │
│             │                           │                   │
│             ▼                           ▼                   │
│  ┌──────────────────────────────────────────────┐           │
│  │         Layered Blend per Bone               │           │
│  │                                              │           │
│  │  Branch Filter: "spine_01"                   │           │
│  │  Blend Weight: WaterPumpBlendAlpha           │           │
│  │  Mesh Space Rotation Blend: true             │           │
│  │                                              │           │
│  │  결과:                                       │           │
│  │    spine_01 이하 (상체): WaterPump Pose      │           │
│  │    spine_01 이상 (하체): Locomotion Pose     │           │
│  └──────────────────┬───────────────────────────┘           │
│                     │                                       │
│                     ▼                                       │
│              [Output Pose]                                  │
│                                                             │
└─────────────────────────────────────────────────────────────┘
```

---

## EventGraph 수정

```
Event Blueprint Update Animation
│
├─ Try Get Pawn Owner
│   └─ Cast To ADRCharacter (또는 BP_GardenRobot)
│       ├─ Get bWaterPumpActive → Set bIsInWaterPump
│       │
│       └─ bIsInWaterPump → Select(True: 1.0, False: 0.0)
│           └─ FInterp To(Current: WaterPumpBlendAlpha, Target: 위 결과, Speed: 10.0, DeltaTime)
│               └─ Set WaterPumpBlendAlpha
│
├─ (기존 Speed, IsFalling 등 로코모션 변수 업데이트 로직 유지)
│
└─ (기존 bIsStunned, bIsBurned 등 디버프 변수 업데이트 로직 유지)
```

**ABP 변수 추가**:
| 변수 | 타입 | 용도 |
|------|------|------|
| `bIsInWaterPump` | Bool | `bWaterPumpActive` 복사 (State Machine 전환 조건에도 사용 가능) |
| `WaterPumpBlendAlpha` | Float | Layered Blend per Bone의 Blend Weight (0.0~1.0 보간) |

---

## 수정 후 예상 결과

```
[물대포 쏘면서 정지]
    상체: DEF_InHoseBlast (호스 들기 포즈) ✅
    하체: BS_Walk의 Idle 포즈 (속도 0) ✅

[물대포 쏘면서 이동]
    상체: DEF_InHoseBlast (호스 들기 포즈) ✅
    하체: BS_Walk의 Walk 애니메이션 (속도에 따라 블렌드) ✅  ← 이 부분이 해결됨

[물대포 중단]
    WaterPumpBlendAlpha가 1.0 → 0.0으로 보간 (약 0.1초)
    상체: Locomotion으로 자연스럽게 복귀 ✅
    하체: 변화 없음 (계속 Locomotion) ✅
```

---

## 테스트 체크리스트

- [ ] 물대포 쏘면서 정지 → 상체 호스 포즈 + 하체 Idle
- [ ] 물대포 쏘면서 전진 → 상체 호스 포즈 + 하체 걷기
- [ ] 물대포 쏘면서 좌우/후진 → 상체 호스 포즈 + 하체 방향별 걷기
- [ ] 물대포 시작 시 → 상체가 부드럽게 호스 포즈로 전환
- [ ] 물대포 중단 시 → 상체가 부드럽게 Idle로 복귀
- [ ] 서버에서 보이는 모습과 클라이언트에서 보이는 모습 일치
- [ ] 물대포 중 점프 → 하체가 점프 애니메이션, 상체는 호스 유지 (또는 원하는 동작)
- [ ] 히트리액트/스턴 등 다른 상태와 충돌 없는지 확인

---

## 주의사항

- **Spine 본 이름**: 반드시 GardenRobot 스켈레톤에서 실제 본 이름을 확인해야 함. 잘못된 이름이면 블렌드가 적용되지 않음
- **AimOffset 연동**: GardenRobot에 `AO_Garden.uasset` (Aim Offset)이 있음. WaterPump 상체 포즈와 Aim Offset이 충돌할 수 있으므로, WaterPump 활성 시 Aim Offset 적용을 고려해야 할 수 있음
- **Mesh Space Rotation Blend**: 이 옵션을 켜면 상체 회전이 월드 기준으로 블렌드되어 이동 방향에 관계없이 상체가 안정적으로 보임. 끄면 로컬 본 공간에서 블렌드되어 부자연스러울 수 있음
- **기존 몽타주 슬롯**: 히트리액트(AM_HitReact)나 공격 몽타주가 FullBody 슬롯을 사용한다면, Layered Blend per Bone 이후에 Slot 노드를 배치하여 몽타주가 최종 포즈를 덮어쓸 수 있도록 해야 함
