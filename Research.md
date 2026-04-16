# 튜토리얼 맵 설계 (Research)

## 1. 튜토리얼 맵 전체 구조

### 1.1 개요

TutorialMap은 **3개의 구간**으로 나뉘며, 각 구간 사이는 **문(Gate)**으로 막혀 있다.
각 구간에는 **발판(Pressure Plate)**이 있고, 해당 구간의 클리어 조건을 달성한 뒤 발판을 밟으면 문이 열린다.
마지막 구간에서는 문 대신 **튜토리얼 클리어**를 트리거한다.

```
┌─────────────────────────────────────────────────────────────────────────────────┐
│                            TutorialMap 전체 레이아웃                             │
│                                                                                 │
│  ┌──────────────┐    ┌─┐    ┌──────────────────┐    ┌─┐    ┌──────────────┐    │
│  │   구간 1      │    │문│    │     구간 2        │    │문│    │   구간 3      │    │
│  │  (점프 파쿠르) │    │1 │    │ (전투 훈련)       │    │2 │    │ (부품 수집)    │    │
│  │              │    │  │    │                  │    │  │    │              │    │
│  │ [장애물들]    │    │  │    │ [적 캐릭터들]      │    │  │    │ [클렌저사이트] │    │
│  │              │    │  │    │                  │    │  │    │ [부품 적 2마리]│    │
│  │       [발판1] │→→→│  │    │          [발판2] │→→→│  │    │       [발판3] │→→→ 클리어!
│  └──────────────┘    └─┘    └──────────────────┘    └─┘    └──────────────┘    │
│                                                                                 │
│  시작점(PlayerStart)                                                             │
└─────────────────────────────────────────────────────────────────────────────────┘
```

### 1.2 공통 시스템: 문(Gate) + 발판(Pressure Plate)

```
발판(Pressure Plate)
  ├── BoxCollision → 플레이어가 위에 올라섰는지 감지
  ├── 조건 체크: bSectionCleared == true인가?
  │     ├── true  → 문 열기 애니메이션/이동 트리거
  │     └── false → 아무 반응 없음 (또는 "조건 미달성" 피드백)
  └── 마지막 구간 발판은 문 열기 대신 TriggerTutorialComplete() 호출

문(Gate)
  ├── StaticMesh (문 모델)
  ├── 닫힌 상태: 물리적으로 길을 막음 (BlockAll 콜리전)
  ├── 열린 상태: 위로 슬라이드 또는 양쪽으로 열림 (Timeline/Interp)
  └── 열린 후에는 다시 닫히지 않음 (일방향)
```

---

## 2. 구간 1: 점프 파쿠르 (Platforming Section)

### 2.1 목적
플레이어에게 **기본 이동(WASD)과 점프(Space)** 조작을 익히게 한다.

### 2.2 구성

```
[PlayerStart] → [평지] → [장애물1] → [장애물2] → [장애물3] → ... → [발판1] → [문1]
```

- 다양한 높이/간격의 장애물(박스, 벽, 틈)을 배치
- 점프로 넘어가야 진행 가능
- 떨어지면 시작점 또는 체크포인트로 복귀 (킬존 또는 리스폰 볼륨)

### 2.3 클리어 조건
- **조건 없음** — 장애물을 넘어서 발판에 도달하면 자동 클리어
- 발판을 밟으면 문1이 열림

### 2.4 UI 안내 (선택)
- "WASD로 이동, Space로 점프" 텍스트 표시
- 구간 시작 시 표시, 발판 도달 시 사라짐

---

## 3. 구간 2: 전투 훈련 (Combat Training Section)

### 3.1 목적
플레이어에게 **3가지 전투 스킬**을 단계적으로 학습시킨다.

### 3.2 초기 배치

```
[문1 통과] → [전투 구역]

전투 구역 중앙:
  - 적 캐릭터 1마리 (움직이지 않음, 공격 안 함, 죽지 않음 = 샌드백)
  - 정면을 바라보고 서 있음
```

**샌드백 적 특성**:
- 이동 비활성화 (`DisableMovement()`)
- AI Controller 비활성화 또는 비할당 (공격 안 함)
- **무적** (체력이 깎이지 않거나, 깎여도 즉시 회복)
- 데미지 히트 리액션은 재생 (피격 모션, 넉백 없음)

### 3.3 목표 1: 기본 공격 (ClawSwipe) 5회 적중

```
[목표 1 시작]
  ├── UI 표시: "기본 공격으로 적을 공격하세요 (좌클릭)"
  ├── 카운터: 0 / 5
  ├── 플레이어가 LMB (좌클릭)으로 기본 공격 → 적에게 적중 시 카운터 +1
  ├── 카운터가 5에 도달하면 → 목표 1 완료
  └── UI 갱신: "기본 공격 완료!"
```

**관련 어빌리티 정보**:
| 항목 | 값 |
|---|---|
| 어빌리티 클래스 | `UDRMeleeAttack` |
| 블루프린트 | `GA_ClawSwipe` |
| 입력 태그 | `InputTag.LMB` (좌클릭) |
| 어빌리티 태그 | `Abilities.GardenRobot.ClawSwipe` |
| GameplayCue | `GameplayCue.Skill.ClawSwipe` |
| 동작 | 근접 공격, 태그 기반 애니메이션 몽타주, 데미지 타입/디버프 설정 가능 |

**적중 감지 방식**:
- 기본 공격이 적에게 적중 시 `ExecCalc_Damage`가 실행됨
- 샌드백 적의 데미지 처리 시 카운터를 증가시키는 로직 필요
- 방법 A: 샌드백 적 전용 AttributeSet에서 `PostGameplayEffectExecute`에서 카운트
- 방법 B: 튜토리얼 매니저 액터가 데미지 이벤트를 감지하여 카운트

### 3.4 목표 2: 물대포 (WaterPump) 3초간 발사

```
[목표 2 시작] (목표 1 완료 후 자동 전환)
  ├── UI 표시: "물대포를 발사하세요 (우클릭 유지)"
  ├── 타이머: 0.0 / 3.0초
  ├── 플레이어가 RMB (우클릭) 유지 → WaterPump 어빌리티 활성화 중 타이머 증가
  ├── 누적 3.0초 도달 시 → 목표 2 완료
  └── UI 갱신: "물대포 훈련 완료!"
```

**관련 어빌리티 정보**:
| 항목 | 값 |
|---|---|
| 어빌리티 클래스 | `UDRWaterPump` |
| 블루프린트 | `GA_WaterPump` |
| 입력 태그 | `InputTag.RMB` (우클릭 유지) |
| 어빌리티 태그 | `Abilities.GardenRobot.WaterPump` |
| GameplayCue | `GameplayCue.Skill.WaterPump` |
| 동작 | 연속 빔 발사, RMB 홀드 중 지속, 카메라 정렬 타겟팅 |
| 빔 범위 | 1000.0 유닛 |
| 빔 폭/높이 | 25.0 / 25.0 (박스 오버랩) |
| 데미지 틱 | 0.1초 간격, 10틱(1초) 누적 시 데미지 적용 |

**3초 측정 방식**:
- WaterPump 어빌리티가 활성화(InputTag.RMB Held) 상태인 동안 타이머 누적
- 중간에 놓았다가 다시 눌러도 누적 (총 3초)
- 또는 연속 3초 유지 필요 (설계 선택)

### 3.5 목표 3: SeedCannon으로 5마리 동시 적중

```
[목표 3 시작] (목표 2 완료 후)
  ├── 기존 샌드백 적 1마리 유지
  ├── 동서남북 4방향에 적 1마리씩 추가 스폰 (총 5마리)
  │     ├── 북쪽: SpawnPoint_N
  │     ├── 남쪽: SpawnPoint_S
  │     ├── 동쪽: SpawnPoint_E
  │     └── 서쪽: SpawnPoint_W
  │   (모든 추가 적도 샌드백 = 움직이지 않음, 공격 안 함, 무적)
  │
  ├── UI 표시: "시드캐논으로 모든 적을 한 번에 맞추세요 (Q)"
  ├── 조건: SeedCannon 1발의 AoE 폭발로 5마리 모두 적중
  │     ├── 적중 실패 시: 다시 시도 가능 (무한 재시도)
  │     └── 5마리 모두 적중 시: 목표 3 완료 → 구간 2 클리어
  └── UI 갱신: "전투 훈련 완료!"
```

**관련 어빌리티 정보**:
| 항목 | 값 |
|---|---|
| 어빌리티 클래스 | `UDRSeedCannon` |
| 투사체 클래스 | `ADRSeedProjectile` |
| 블루프린트 | `GA_SeedCannon` |
| 입력 태그 | `InputTag.Q` (Q 키) |
| 어빌리티 태그 | `Abilities.GardenRobot.SeedCannon` |
| 동작 | 포물선 투사체 발사 → 착탄 시 AoE 폭발 |
| 발사 속도 | 500.0 유닛/초 |
| 발사 각도 | 35도 위쪽 (포물선 궤도) |
| **내부 반경** | **200.0 유닛** (풀 데미지) |
| **외부 반경** | **400.0 유닛** (감소 데미지) |
| 내부 데미지 | 70.0 |
| 외부 데미지 | 30.0 |
| 추가 효과 | 아군에게는 힐링 (내부 100, 외부 50) |

**적 배치 간격 고려**:
- SeedCannon 외부 반경 = 400 유닛
- 5마리 모두 한 번에 맞으려면, 가장 먼 적까지의 거리가 400 유닛 이내여야 함
- 중앙 적 기준 동서남북 적까지 거리: **약 300~350 유닛** 추천
- 이렇게 하면 중앙에 정확히 맞추면 5마리 모두 외부 반경 안에 들어옴
- 너무 쉽지 않도록 내부 반경(200) 바로 밖에 배치하면 정확한 조준 필요

```
적 배치 예시 (위에서 본 시점):

            [적_N] (0, +300)
              │
              │
[적_W] ─── [적_중앙] ─── [적_E]
(-300, 0)    (0, 0)     (+300, 0)
              │
              │
            [적_S] (0, -300)

→ 중앙에서 모든 적까지 거리: 300 유닛
→ SeedCannon 외부 반경(400) 안에 모두 포함
→ 플레이어가 중앙 부근에 SeedCannon을 착탄시키면 5마리 동시 적중
```

### 3.6 구간 2 전체 흐름

```
[문1 통과]
    ↓
[초기 배치: 샌드백 적 1마리 (중앙)]
    ↓
[목표 1] 기본 공격(LMB) 5회 적중
    ↓ 완료
[목표 2] 물대포(RMB) 3초간 발사
    ↓ 완료
[목표 3 시작: 동서남북에 적 4마리 추가 스폰 → 총 5마리]
    ↓
[목표 3] SeedCannon(Q)으로 5마리 동시 적중
    ↓ 완료
[구간 2 클리어 → bSectionCleared = true]
    ↓
[발판2 밟기 → 문2 열림]
```

---

## 4. 구간 3: 부품 수집 (Part Collection Section)

### 4.1 목적
플레이어에게 **부품 수집 → 클렌저사이트 설치** 시스템을 학습시킨다.

### 4.2 초기 배치

```
[문2 통과] → [구간 3 영역]

구간 3 레이아웃:
┌─────────────────────────────────────────┐
│                                         │
│            [부품적_1]                    │
│               ↑ (도주 경로)               │
│                                         │
│   [시작 타일]  →  [클렌저사이트]           │
│                    (가운데)              │
│                                         │
│               ↓ (도주 경로)               │
│            [부품적_2]                    │
│                                         │
│                          [발판3]         │
└─────────────────────────────────────────┘
```

### 4.3 시작 타일 (Trigger Volume)

```
시작 타일
  ├── BoxCollision → 플레이어가 밟으면 감지
  ├── 밟기 전: 적 2마리는 정지 상태 (DisableMovement)
  ├── 밟은 후:
  │     ├── 적 2마리의 AI 활성화 → 도망 시작
  │     ├── UI 표시: "부품을 들고 있는 적을 잡아 부품을 수집하세요"
  │     └── 시작 타일 비활성화 (1회성)
  └── 시각적 피드백: 발광 타일 또는 마커
```

### 4.4 부품 적 (Part-Carrying Enemies)

**기존 시스템 활용**: `ADREnemy`의 `bCarriesPart = true`

| 특성 | 값 |
|---|---|
| 적 수 | 2마리 |
| 부품 수 | 각 1개씩 (총 2개) |
| 이동 | 시작 타일 밟기 전: 정지 / 밟은 후: 도주 AI 활성화 |
| 공격 | 안 함 (도망만) |
| 체력 | 일반 (죽일 수 있음) |
| 사망 시 | `DropPart()` → `ADRCleanserPart` 스폰 (물리 임펄스) |

**부품 드롭 후 픽업 과정**:
1. 적 사망 → `ADRCleanserPart` 스폰
2. 플레이어가 부품 근처 접근 → DetectionSphere(150 유닛) 오버랩
3. 플레이어가 부품을 바라보며 E 키 → `ServerRequestPickupPart()`
4. 부품이 플레이어 손에 부착 (TestLeftHand 소켓)
5. `State.Carrying` 태그 추가 → 이동 속도 감소

**부품 설치 과정**:
1. 부품을 들고 클렌저사이트 InteractionBox(200x200x100) 진입
2. E 키 → `ServerRequestInstallPartToSite()`
3. `InstalledPartsCount` 증가
4. 2개 모두 설치 시 → 클렌저사이트 상태 변경 (Active → PartsCollected)

### 4.5 클렌저사이트 설정

| 특성 | 값 |
|---|---|
| 위치 | 구간 3 가운데 |
| 초기 상태 | Active (부품 수집 대기) |
| 필요 부품 수 | 2개 (`RequiredPartsCount = 2`) |
| 설치 시 피드백 | 사운드 재생, 메시 변경 |
| 부품 완료 시 | 구간 3 클리어 조건 달성 |

### 4.6 구간 3 전체 흐름

```
[문2 통과]
    ↓
[시작 타일 밟기]
    ↓
[적 2마리 도주 AI 활성화]
    ↓
[적 1마리 처치 → 부품 드롭 → 픽업 → 클렌저사이트 설치]
    ↓
[적 2마리 처치 → 부품 드롭 → 픽업 → 클렌저사이트 설치]
    ↓
[클렌저사이트 부품 2/2 설치 완료 → 구간 3 클리어]
    ↓
[발판3 밟기 → TriggerTutorialComplete()]
    ↓
[SaveGame 저장 → 3초 후 MainMenu로 복귀]
```

---

## 5. 전체 튜토리얼 플로우 요약

```
[MainMenu - "Start Game" 클릭]
    ↓ OpenLevel("TutorialMap")
[TutorialMap 로드 - ADRTutorialGameMode]
    ↓
[구간 1: 점프 파쿠르]
  - 장애물을 점프로 넘어가기
  - 조건: 없음 (발판 도달만으로 클리어)
  - 발판1 밟기 → 문1 열림
    ↓
[구간 2: 전투 훈련]
  - 샌드백 적 1마리 (움직이지 않음, 공격 안 함, 무적)
  - 목표 1: 기본 공격(LMB) 5회 적중
  - 목표 2: 물대포(RMB) 3초간 발사
  - 목표 3 시작: 동서남북에 적 4마리 추가 스폰 (총 5마리, 모두 샌드백)
  - 목표 3: SeedCannon(Q)으로 5마리 동시 적중
  - 발판2 밟기 → 문2 열림
    ↓
[구간 3: 부품 수집]
  - 가운데에 클렌저사이트 1개 (Active 상태)
  - 부품을 든 적 2마리 (시작 전 정지)
  - 시작 타일 밟기 → 적 도주 AI 활성화
  - 적 처치 → 부품 드롭 → 픽업 → 클렌저사이트에 설치 (x2)
  - 발판3 밟기 → TriggerTutorialComplete()
    ↓
[SaveGame 저장 (bHasCompletedTutorial = true)]
    ↓
[GameClear UI 표시 → 3초 후 MainMenu 복귀]
    ↓
[MainMenu - HasCompletedTutorial() == true → 로비 모드(Host/Join)]
```

---

## 6. 필요한 새 액터/시스템

### 6.1 튜토리얼 매니저 (Tutorial Manager Actor)

튜토리얼 전체 진행을 관리하는 중앙 액터.

**역할**:
- 현재 구간/목표 추적
- 목표 달성 감지 (데미지 카운트, 타이머, 적중 감지)
- UI 업데이트 (목표 텍스트, 진행도)
- 문 열기/닫기 제어
- 적 스폰 트리거

### 6.2 튜토리얼 문 (Tutorial Gate Actor)

**구성**:
- StaticMeshComponent (문 모델)
- BoxCollision (통행 차단)
- Timeline 또는 InterpTo (열림 애니메이션)

**인터페이스**:
- `OpenGate()` → 열림 애니메이션 재생 + 콜리전 비활성화
- `CloseGate()` → (사용하지 않을 수 있음)

### 6.3 튜토리얼 발판 (Tutorial Pressure Plate Actor)

**구성**:
- StaticMeshComponent (발판 모델)
- BoxCollision (트리거)
- `bSectionCleared` 체크 → 클리어된 상태에서만 작동

**연결**:
- 발판1 → 문1 열기
- 발판2 → 문2 열기
- 발판3 → `TriggerTutorialComplete()` 호출

### 6.4 샌드백 적 (Tutorial Dummy Enemy)

**기존 `ADREnemy` 기반** + 특수 설정:
- `bIsTutorialDummy = true` 플래그
- AI Controller: 비활성화 (이동/공격 안 함)
- 무적: 데미지를 받지만 체력이 0 이하로 떨어지지 않거나 즉시 회복
- 히트 리액션: 재생 (피격 느낌)

**또는**: 전용 `ADRTutorialDummyEnemy` 서브클래스 생성

### 6.5 시작 타일 (Start Trigger Actor)

구간 3 전용. 밟으면 부품 적의 AI를 활성화한다.

**구성**:
- BoxCollision (트리거)
- 시각적 메시 (발광 타일)
- `OnPlayerStepOn()` → 적 2마리의 `EnableAI()` 호출
- 1회성 (다시 밟아도 재작동 안 함)

---

## 7. 튜토리얼 전용 오버레이 UI 시스템

### 7.1 설계 개요

튜토리얼에서는 **WBP_TutorialOverlay**라는 전용 오버레이를 사용한다.
기존 `WBP_StageOverlay`와 유사하지만, 스킬 아이콘의 **점진적 표시**와 **스킬 사용 제한**이 핵심 차이점이다.

```
기존 시스템과의 관계:

ADRHUD
├── WBP_StageOverlay    (스테이지용 — 모든 스킬 아이콘 표시)
├── WBP_LobbyOverlay    (로비용 — 모든 스킬 아이콘 표시)
└── WBP_TutorialOverlay (튜토리얼용 — 스킬 아이콘 점진적 표시)  ← 신규
```

**핵심 원칙**:
- 튜토리얼 GameMode(`ADRTutorialGameMode`)에서 `BP_DRHUD`의 `OverlayWidgetClass`를 `WBP_TutorialOverlay`로 설정
- 또는 `BP_DRTutorialGameMode`에 전용 HUD 클래스(`BP_DRTutorialHUD`)를 지정하여 오버레이 분리

### 7.2 WBP_TutorialOverlay 위젯 구조

```
WBP_TutorialOverlay (UDRUserWidget 상속)
│
├── [체력/물 바] (기존 WBP_StageOverlay와 동일)
│     ├── HealthBar
│     └── WaterBar
│
├── [스킬 아이콘 영역] ← 점진적 표시 제어
│     ├── SkillSlot_LMB  (기본 공격 — ClawSwipe)      → 구간 2 목표 1 시작 시 표시
│     ├── SkillSlot_RMB  (물대포 — WaterPump)          → 구간 2 목표 2 시작 시 표시
│     └── SkillSlot_Q    (시드캐논 — SeedCannon)       → 구간 2 목표 3 시작 시 표시
│
├── [WBP_PhaseObjective] ← 기존 스테이지 PhaseObjective UI 재활용
│     ├── ObjectiveTitle (텍스트)
│     └── ObjectiveProgress (텍스트 또는 프로그레스 바)
│
└── [조작 안내 텍스트] (선택적)
      └── ControlGuideText (현재 스킬 키바인드 안내)
```

### 7.3 스킬 아이콘 점진적 표시 시스템

#### 구간별 스킬 아이콘 가시성

| 구간 | LMB (기본 공격) | RMB (물대포) | Q (시드캐논) |
|---|---|---|---|
| **구간 1** (점프 파쿠르) | **Hidden** | **Hidden** | **Hidden** |
| **구간 2 - 목표 1** (기본 공격 5회) | **Visible** | Hidden | Hidden |
| **구간 2 - 목표 2** (물대포 3초) | Visible | **Visible** | Hidden |
| **구간 2 - 목표 3** (SeedCannon 동시적중) | Visible | Visible | **Visible** |
| **구간 3** (부품 수집) | Visible | Visible | Visible |

#### 스킬 아이콘 표시 연출
새 스킬 아이콘이 나타날 때:
1. **페이드인 애니메이션** — 0에서 1로 불투명도 전환 (약 0.5초)
2. **스케일 팝** — 약간 커졌다가 정상 크기로 (바운스 효과)
3. **강조 효과** — 잠시 글로우/아웃라인 표시 후 정상 상태로
4. 이 시점에서 해당 스킬 사용이 가능해짐

### 7.4 스킬 사용 제한 시스템

#### 접근 방식: 입력 태그 기반 선택적 차단

기존 `Player.Block.InputPressed` 태그는 **모든** 입력을 차단하므로, 튜토리얼에서는 **특정 InputTag만 선택적으로 차단**하는 방식이 필요하다.

**방법 A: 어빌리티 동적 부여/회수 (권장)**
```
[구간 1 시작]
  → 모든 전투 어빌리티 부여하지 않음
    (또는 부여 후 즉시 ClearAbility로 회수)
  → 어빌리티가 없으므로 LMB/RMB/Q 입력해도 아무 반응 없음

[구간 2 - 목표 1 시작]
  → ClawSwipe 어빌리티만 GiveAbility() → LMB 사용 가능
  → BroadcastAbilityInfo() → UI에 LMB 아이콘만 표시

[구간 2 - 목표 2 시작]
  → WaterPump 어빌리티 GiveAbility() → RMB 사용 가능
  → BroadcastAbilityInfo() → UI에 LMB + RMB 아이콘 표시

[구간 2 - 목표 3 시작]
  → SeedCannon 어빌리티 GiveAbility() → Q 사용 가능
  → BroadcastAbilityInfo() → UI에 LMB + RMB + Q 아이콘 표시
```

**장점**:
- GAS 네이티브 시스템 활용 (별도 차단 로직 불필요)
- `BroadcastAbilityInfo()`가 자동으로 현재 부여된 어빌리티만 브로드캐스트 → UI 자동 갱신
- 어빌리티가 없으므로 입력을 눌러도 아무 반응 없음 (자연스러운 차단)

**구현 포인트**:
```cpp
// 튜토리얼 매니저에서 호출
void GrantAbilityForTutorial(ACharacter* Player, TSubclassOf<UGameplayAbility> AbilityClass)
{
    UAbilitySystemComponent* ASC = GetASCFromPlayer(Player);
    if (!ASC) return;

    FGameplayAbilitySpec AbilitySpec(AbilityClass, 1);
    if (const UDRGameplayAbility* DRAbility = Cast<UDRGameplayAbility>(AbilitySpec.Ability))
    {
        AbilitySpec.DynamicAbilityTags.AddTag(DRAbility->StartupInputTag);
        ASC->GiveAbility(AbilitySpec);
    }

    // UI 갱신: 현재 부여된 어빌리티들의 아이콘을 브로드캐스트
    // OverlayWidgetController->BroadcastAbilityInfo() 호출
}
```

**방법 B: 튜토리얼 전용 CharacterClassInfo 사용**
- `DA_TutorialCharacterClassInfo` 데이터에셋 생성
- `StartupAbilities` 배열을 비워둠 (어빌리티 없이 시작)
- `BP_DRTutorialGameMode`에서 이 데이터에셋 사용
- 이후 튜토리얼 매니저가 단계적으로 어빌리티 부여

### 7.5 PhaseObjective UI 재활용

#### 기존 시스템 구조

```
ADRStageGameState (서버)
  ├── CurrentPhaseObjective: FPhaseObjectiveData (Replicated)
  │     ├── PhaseNumber: int32
  │     ├── ObjectiveTitle: FText      ("Secure Cleanser Sites")
  │     ├── ProgressFormat: FText      ("Sites Secured")
  │     └── RequiredCount: int32       (2)
  ├── CurrentObjectiveProgress: int32  (Replicated)
  └── OnPhaseObjectiveChangedDelegate → OverlayWidgetController가 구독

OverlayWidgetController (클라이언트)
  ├── HandlePhaseObjectiveChanged()
  │     ├── OnObjectiveTextChanged.Broadcast(Title, ProgressText)
  │     └── OnObjectiveProgressChanged.Broadcast(Current, Max)
  └── WBP_PhaseObjective (블루프린트)
        ├── ObjectiveTitle 텍스트 갱신
        └── ProgressText 텍스트 갱신
```

#### 튜토리얼에서의 활용

튜토리얼에서는 `ADRStageGameState` 대신 **튜토리얼 매니저 → OverlayWidgetController** 경로로 목표를 업데이트한다.

**방법 A: OverlayWidgetController의 델리게이트를 직접 브로드캐스트**
```cpp
// 튜토리얼 매니저에서 직접 호출
void UpdateTutorialObjective(const FText& Title, const FText& Progress, int32 Current, int32 Max)
{
    // OverlayWidgetController 가져오기
    ADRHUD* HUD = Cast<ADRHUD>(PC->GetHUD());
    UOverlayWidgetController* WC = HUD->GetOverlayWidgetController(Params);

    // 기존 델리게이트를 통해 UI 갱신
    WC->OnObjectiveTextChanged.Broadcast(Title, Progress);
    WC->OnObjectiveProgressChanged.Broadcast(Current, Max);
}
```

**방법 B: 튜토리얼 전용 GameState에서 FPhaseObjectiveData 복제**
- `ADRTutorialGameState` 서브클래스 생성 (필요시)
- 동일한 `FPhaseObjectiveData` 구조체 사용
- 기존 UI 위젯이 그대로 동작

#### 튜토리얼 목표 데이터

| 구간/목표 | ObjectiveTitle | ProgressFormat | RequiredCount |
|---|---|---|---|
| 구간 1 | "장애물을 넘어 이동하세요" | "" (진행도 없음) | 0 |
| 구간 2 - 목표 1 | "기본 공격으로 적을 공격하세요" | "적중" | 5 |
| 구간 2 - 목표 2 | "물대포를 발사하세요" | "초" | 3 |
| 구간 2 - 목표 3 | "시드캐논으로 모든 적을 한 번에 맞추세요" | "동시 적중" | 5 |
| 구간 3 | "부품을 클렌저사이트에 설치하세요" | "설치 완료" | 2 |

### 7.6 WBP_TutorialOverlay 화면 레이아웃

```
┌──────────────────────────────────────────────────────────────────┐
│                                                                  │
│  ┌─────────────────────────────────────┐                         │
│  │ [WBP_PhaseObjective]                │                         │
│  │                                     │                         │
│  │  목표: 기본 공격으로 적을 공격하세요   │                         │
│  │  진행도: 3 / 5                      │                         │
│  │                                     │                         │
│  └─────────────────────────────────────┘                         │
│                                                                  │
│                                                                  │
│                        (게임 화면)                                 │
│                                                                  │
│                                                                  │
│                                                                  │
│                     ┌─────────────────┐                          │
│                     │  조작: 좌클릭    │  ← 조작 안내 텍스트        │
│                     └─────────────────┘                          │
│                                                                  │
│  ┌────────────┐                                                  │
│  │ HP ████░░░ │                                                  │
│  │ WP ██████░ │                                                  │
│  └────────────┘                                                  │
│                                                                  │
│                              ┌─────┐ ┌─────┐ ┌─────┐            │
│                              │ LMB │ │ RMB │ │  Q  │            │
│                              │ ✓  │ │     │ │     │            │
│                              └─────┘ └─────┘ └─────┘            │
│                              (보임)   (숨김)   (숨김)              │
│                                                                  │
└──────────────────────────────────────────────────────────────────┘

※ 구간 2 - 목표 1 시점의 화면
※ LMB 아이콘만 보이고, RMB/Q 아이콘은 아직 숨겨져 있음
```

### 7.7 구간별 UI 텍스트 + 스킬 가시성 통합 테이블

| 구간 | 목표 | ObjectiveTitle | ProgressFormat | Required | LMB | RMB | Q | 조작 안내 |
|---|---|---|---|---|---|---|---|---|
| 1 | 이동+점프 | "장애물을 넘어 이동하세요" | — | 0 | ✕ | ✕ | ✕ | "WASD: 이동 / Space: 점프" |
| 2-1 | 기본 공격 5회 | "기본 공격으로 적을 공격하세요" | "적중" | 5 | **✓** | ✕ | ✕ | "좌클릭: 기본 공격" |
| 2-2 | 물대포 3초 | "물대포를 발사하세요" | "초" | 3 | ✓ | **✓** | ✕ | "우클릭 유지: 물대포" |
| 2-3 | SeedCannon 동시적중 | "시드캐논으로 모든 적을 한 번에 맞추세요" | "동시 적중" | 5 | ✓ | ✓ | **✓** | "Q: 시드캐논" |
| 3 | 부품 수집 | "부품을 클렌저사이트에 설치하세요" | "설치 완료" | 2 | ✓ | ✓ | ✓ | "E: 상호작용" |

✓ = 아이콘 표시 + 스킬 사용 가능 / ✕ = 아이콘 숨김 + 스킬 사용 불가 / **✓** = 이번 단계에서 새로 등장

### 7.8 기존 오버레이 시스템 참조

#### ADRHUD 클래스
**파일**: `Source/DaeRune/Public/UI/HUD/DRHUD.h/.cpp`

```cpp
// 핵심 프로퍼티
UPROPERTY(EditAnywhere) TSubclassOf<UDRUserWidget> OverlayWidgetClass;
UPROPERTY(EditAnywhere) TSubclassOf<UOverlayWidgetController> OverlayWidgetControllerClass;
UPROPERTY() TObjectPtr<UDRUserWidget> OverlayWidget;
UPROPERTY() TObjectPtr<UOverlayWidgetController> OverlayWidgetController;

// 핵심 메서드
void InitOverlay(APlayerController*, APlayerState*, UAbilitySystemComponent*, UAttributeSet*);
UOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams&);
void RemoveOverlay();
void UpdateOverlayForSpectating(...);
```

**InitOverlay 흐름**:
1. OverlayWidget 생성 (OverlayWidgetClass로)
2. OverlayWidgetController 생성/캐시 (싱글톤)
3. 위젯에 컨트롤러 설정 → `SetWidgetController()`
4. 초기값 브로드캐스트 → `BroadcastInitialValues()`
5. **어빌리티 아이콘 브로드캐스트 → `BroadcastAbilityInfo()`**
6. Viewport에 추가

#### OverlayWidgetController 핵심 델리게이트
**파일**: `Source/DaeRune/Public/UI/WidgetController/OverlayWidgetController.h`

```cpp
// 어트리뷰트 변경
FOnAttributeChangedSignature OnHealthChanged;
FOnAttributeChangedSignature OnMaxHealthChanged;
FOnAttributeChangedSignature OnWaterChanged;
FOnAttributeChangedSignature OnMaxWaterChanged;

// 목표 변경 (PhaseObjective)
FOnObjectiveTextChangedSignature OnObjectiveTextChanged;       // (Title, ProgressText)
FOnObjectiveProgressChangedSignature OnObjectiveProgressChanged; // (Current, Max)

// 페이즈 알람
FOnPhaseAlarmSignature OnPhaseAlarm;  // (PhaseText)
```

#### DRWidgetController (부모 클래스) - 어빌리티 아이콘
**파일**: `Source/DaeRune/Public/UI/WidgetController/DRWidgetController.h`

```cpp
// 어빌리티 정보 브로드캐스트
UPROPERTY(BlueprintAssignable)
FAbilityInfoSignature AbilityInfoDelegate;

// 어빌리티 정보 데이터에셋
UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
TObjectPtr<UAbilityInfo> AbilityInfo;

// 현재 부여된 어빌리티를 순회하며 AbilityInfoDelegate로 브로드캐스트
void BroadcastAbilityInfo();
```

#### FDRAbilityInfo 구조체
```cpp
struct FDRAbilityInfo
{
    FGameplayTag AbilityTag;       // Abilities.GardenRobot.ClawSwipe
    FGameplayTag InputTag;         // InputTag.LMB (런타임 설정)
    FGameplayTag CooldownTag;      // 쿨다운 태그
    FGameplayTag AbilityType;      // 어빌리티 분류
    TObjectPtr<const UTexture2D> Icon;                  // 아이콘 텍스처
    TObjectPtr<const UMaterialInterface> BackgroundMaterial;  // 배경 머티리얼
    TSubclassOf<UGameplayAbility> Ability;              // 어빌리티 클래스
};
```

#### FPhaseObjectiveData 구조체
```cpp
struct FPhaseObjectiveData : public FTableRowBase
{
    int32 PhaseNumber = 1;
    FText ObjectiveTitle;      // "Secure Cleanser Sites"
    FText ProgressFormat;      // "Sites Secured"
    int32 RequiredCount = 0;   // 목표 횟수
};
```

### 7.9 어빌리티 부여/차단 시스템 참조

#### 어빌리티 부여 흐름 (기존)
```
ADRGameModeBase (서버)
  → InitializeDefaultAbilities(Character)
    → UDRAbilitySystemLibrary::GiveStartupAbilities(ASC, CharacterClassInfo)
      → CharacterClassInfo->GetClassDefaultInfo(Class).StartupAbilities
      → + CharacterClassInfo->CommonAbilities
      → ASC->AddCharacterAbilities(AllAbilities)

UDRAbilitySystemComponent::AddCharacterAbilities(Abilities)
  → for each AbilityClass:
    → FGameplayAbilitySpec(AbilityClass, 1)
    → DynamicAbilityTags에 StartupInputTag 추가
    → GiveAbility(AbilitySpec)
    → InputTagToAbilityMap에 캐시
  → bStartupAbilitiesGiven = true
  → AbilitiesGivenDelegate.Broadcast()  ← UI가 이 시점에서 아이콘 표시
```

#### 입력 차단 태그 (기존)
```cpp
// DRGameplayTags.h
FGameplayTag Player_Block_InputPressed;   // 모든 Pressed 입력 차단
FGameplayTag Player_Block_InputHeld;      // 모든 Held 입력 차단
FGameplayTag Player_Block_InputReleased;  // 모든 Released 입력 차단
```

**주의**: 이 태그들은 **전체 입력**을 차단하므로, 튜토리얼에서 **특정 스킬만** 선택적으로 차단/해제하려면 어빌리티 동적 부여/회수 방식이 더 적합하다.

#### 어빌리티 동적 부여 API
```cpp
// 어빌리티 부여
FGameplayAbilitySpecHandle ASC->GiveAbility(FGameplayAbilitySpec);

// 어빌리티 회수
ASC->ClearAbility(FGameplayAbilitySpecHandle);

// 현재 부여된 어빌리티 순회
ASC->ForEachAbility(Delegate);

// 어빌리티 아이콘 UI 갱신
WidgetController->BroadcastAbilityInfo();
```

---

## 8. 관련 기존 시스템 참조

### 8.1 어빌리티 파일 경로

| 어빌리티 | C++ 클래스 | 블루프린트 | 입력 |
|---|---|---|---|
| 기본 공격 | `UDRMeleeAttack` | `GA_ClawSwipe` | LMB |
| 물대포 | `UDRWaterPump` | `GA_WaterPump` | RMB (홀드) |
| SeedCannon | `UDRSeedCannon` | `GA_SeedCannon` | Q |

### 8.2 부품 시스템 파일 경로

| 시스템 | 파일 |
|---|---|
| 클렌저사이트 | `Source/DaeRune/Public/Actor/DRCleanserSite.h/.cpp` |
| 클렌저파트 | `Source/DaeRune/Public/Actor/DRCleanserPart.h/.cpp` |
| 적 부품 드롭 | `ADREnemy::DropPart()` |
| 플레이어 픽업 | `ADRPlayerController::ServerRequestPickupPart()` |
| 플레이어 설치 | `ADRPlayerController::ServerRequestInstallPartToSite()` |

### 8.3 튜토리얼 GameMode

| 파일 | 역할 |
|---|---|
| `Source/DaeRune/Public/Game/DRTutorialGameMode.h/.cpp` | 튜토리얼 완료/실패 처리 |
| `TriggerTutorialComplete()` | SaveGame 저장 + GameClear UI + MainMenu 복귀 |
| `HandleWipeout()` | 전멸 시 SaveGame 미저장 + MainMenu 복귀 |

---

## 9. 구현 우선순위

```
Step 1: 튜토리얼 매니저 액터 생성 (구간/목표 추적, 진행 관리)
Step 2: 튜토리얼 문(Gate) 액터 생성
Step 3: 튜토리얼 발판(Pressure Plate) 액터 생성
Step 4: 샌드백 적 시스템 구현 (무적 + 이동/공격 비활성화)
Step 5: 구간 1 구성 (장애물 배치 — 에디터 작업)
Step 6: 구간 2 전투 훈련 시스템 구현
  - 6a: 목표 1 (기본 공격 카운트)
  - 6b: 목표 2 (물대포 타이머)
  - 6c: 목표 3 (SeedCannon 동시 적중 감지 + 적 추가 스폰)
Step 7: 구간 3 부품 수집 시스템 구현
  - 7a: 시작 타일 액터
  - 7b: 부품 적 스폰 + 도주 AI
  - 7c: 클렌저사이트 연동
Step 8: 튜토리얼 UI (목표 표시, 진행도, 조작 안내)
Step 9: 에디터에서 TutorialMap 레벨 배치
Step 10: 통합 테스트
```
