# Plan: PoisonGas 웨이브 경고 UI 구현

## 개요

Research.md의 **방안 1 (Replicated 변수)** 기반. 기존 `bIsWaveRestTime` / `OnPhaseAlarm` 패턴을 그대로 따라 구현한다.

---

## Step 1: DRStageGameState.h 수정

### 1-1. 델리게이트 선언 추가

**위치**: 17줄 (`DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnWaveTimerChanged, ...)`) 바로 아래

```cpp
// 독가스 경고 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToxicGasWarningSignature, bool, bIsToxicGasWave);
```

### 1-2. public 섹션에 세터/게터/델리게이트 추가

**위치**: `SetIsWaveRestTime()` 선언 (118줄) 바로 아래

```cpp
// 독가스 웨이브 여부
void SetIsToxicGasWave(bool bIsToxicGas);

UFUNCTION(BlueprintCallable, Category = "Phase|Defense")
bool IsToxicGasWave() const { return bIsToxicGasWave; }

// 독가스 경고 델리게이트 (모든 클라이언트에서 UI 바인딩용)
UPROPERTY(BlueprintAssignable, Category = "Phase|Warning")
FOnToxicGasWarningSignature OnToxicGasWarningDelegate;
```

### 1-3. protected 섹션에 RepNotify 선언 추가

**위치**: `OnRep_IsWaveRestTime()` 선언 (180-181줄) 바로 아래

```cpp
UFUNCTION()
void OnRep_IsToxicGasWave();
```

### 1-4. private 섹션에 Replicated 변수 추가

**위치**: `bIsWaveRestTime` (231-232줄) 바로 아래

```cpp
UPROPERTY(ReplicatedUsing = OnRep_IsToxicGasWave)
bool bIsToxicGasWave = false;
```

---

## Step 2: DRStageGameState.cpp 수정

### 2-1. 생성자에 초기값 추가

**위치**: `bIsWaveRestTime = false;` (34줄) 바로 아래

```cpp
bIsToxicGasWave = false;
```

### 2-2. GetLifetimeReplicatedProps에 등록

**위치**: `DOREPLIFETIME(ADRStageGameState, bIsWaveRestTime);` (67줄) 바로 아래

```cpp
DOREPLIFETIME(ADRStageGameState, bIsToxicGasWave);
```

### 2-3. 세터 구현 추가

**위치**: `SetIsWaveRestTime()` 구현 (183-191줄) 바로 아래

```cpp
void ADRStageGameState::SetIsToxicGasWave(bool bIsToxicGas)
{
    if (HasAuthority())
    {
        bIsToxicGasWave = bIsToxicGas;
        // 서버에서 즉시 브로드캐스트 (리슨 서버 플레이어용)
        OnToxicGasWarningDelegate.Broadcast(bIsToxicGas);
    }
}
```

### 2-4. RepNotify 구현 추가

**위치**: `OnRep_IsWaveRestTime()` 구현 (245-249줄) 바로 아래

```cpp
void ADRStageGameState::OnRep_IsToxicGasWave()
{
    // 클라이언트에서 복제 후 브로드캐스트
    OnToxicGasWarningDelegate.Broadcast(bIsToxicGasWave);
}
```

---

## Step 3: DRPhase3.cpp 수정

### 3-1. StartNextWave()에서 독가스 웨이브 플래그 설정

**위치**: 기존 `if (Modifier.bSpawnToxicGas)` (228줄) **바로 앞**에 삽입

```cpp
// 독가스 웨이브 경고 UI (모든 클라이언트에 복제)
if (GameState)
{
    GameState->SetIsToxicGasWave(Modifier.bSpawnToxicGas);
}
```

**이유**: `bSpawnToxicGas`가 true든 false든 매 웨이브마다 항상 호출. true이면 경고 표시, false이면 경고 해제. 이렇게 해야 이전 웨이브가 독가스였고 현재 웨이브가 아닐 때 경고가 자동으로 사라진다.

### 3-2. EndCurrentWave()에서 독가스 플래그 리셋

**위치**: `StartRestTime();` 호출 (264줄) **바로 앞**에 삽입

```cpp
// 웨이브 종료 시 독가스 플래그 리셋 (연속 독가스 웨이브에서 RepNotify 재발동 보장)
if (GameState)
{
    GameState->SetIsToxicGasWave(false);
}
```

**이유**: 연속으로 `bSpawnToxicGas = true`인 웨이브가 올 때, RepNotify는 값이 변경될 때만 호출된다. `EndCurrentWave()`에서 false로 리셋하면 → 다음 웨이브 `StartNextWave()`에서 true로 설정 → 값이 false→true로 변경되므로 RepNotify 정상 발동.

---

## Step 4: OverlayWidgetController.h 수정

### 4-1. 델리게이트 타입 선언 추가

**위치**: `FOnPhaseAlarmSignature` 선언 (47줄) 바로 아래

```cpp
// 독가스 경고 UI 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnToxicGasWarningUISignature, bool, bIsToxicGasWave);
```

### 4-2. public BlueprintAssignable 델리게이트 추가

**위치**: `OnPhaseAlarm` 선언 (115-116줄) 바로 아래

```cpp
// 독가스 경고 UI 델리게이트 (Blueprint에서 바인딩하여 경고 위젯 표시/숨김)
UPROPERTY(BlueprintAssignable, Category = "Phase|Warning")
FOnToxicGasWarningUISignature OnToxicGasWarning;
```

### 4-3. private 섹션에 바인딩 함수 및 콜백 선언 추가

**위치**: `BindPhaseAlarmDelegate()` 선언 (122줄) 바로 아래

```cpp
void BindToxicGasWarningDelegate();

UFUNCTION()
void OnToxicGasWarningReceived(bool bIsToxicGasWave);
```

### 4-4. private 섹션에 타이머 핸들 추가

**위치**: `PhaseAlarmBindingDelayTimer` 선언 (133줄) 바로 아래

```cpp
FTimerHandle ToxicGasWarningBindingDelayTimer;
```

---

## Step 5: OverlayWidgetController.cpp 수정

### 5-1. BindCallbacksToDependencies()에 독가스 바인딩 호출 추가

**위치**: `BindPhaseAlarmDelegate` 타이머 블록 (168-178줄) 바로 아래

```cpp
// 독가스 경고 델리게이트 바인딩
if (UWorld* World = GetWorld())
{
    World->GetTimerManager().SetTimer(
        ToxicGasWarningBindingDelayTimer,
        this,
        &UOverlayWidgetController::BindToxicGasWarningDelegate,
        0.1f,  // 0.1초 대기 (GameState 복제 대기)
        false  // 한 번만 실행
    );
}
```

**이유**: 기존 Phase 알람/목표 바인딩과 동일한 0.1초 딜레이 패턴. GameState가 클라이언트에 아직 복제되지 않았을 수 있으므로.

### 5-2. BindToxicGasWarningDelegate() 구현 추가

**위치**: `BindPhaseAlarmDelegate()` 구현 (306-324줄) 바로 아래

```cpp
void UOverlayWidgetController::BindToxicGasWarningDelegate()
{
    ADRStageGameState* DRGameState = GetWorld()->GetGameState<ADRStageGameState>();
    if (!DRGameState) return;

    // Dynamic Delegate 바인딩
    DRGameState->OnToxicGasWarningDelegate.AddDynamic(this, &UOverlayWidgetController::OnToxicGasWarningReceived);

    // 현재 상태 즉시 확인 (이미 독가스 웨이브 진행 중일 수 있음 - late joiner)
    if (DRGameState->IsToxicGasWave())
    {
        OnToxicGasWarning.Broadcast(true);
    }
}
```

### 5-3. OnToxicGasWarningReceived() 콜백 구현 추가

**위치**: `BindToxicGasWarningDelegate()` 바로 아래

```cpp
void UOverlayWidgetController::OnToxicGasWarningReceived(bool bIsToxicGasWave)
{
    OnToxicGasWarning.Broadcast(bIsToxicGasWave);
}
```

### 5-4. UnbindAllDelegates()에 독가스 델리게이트 정리 추가

**위치**: `OnPhaseChangedDelegate.RemoveDynamic(...)` 호출 (240줄) 바로 아래

```cpp
DRGameState->OnToxicGasWarningDelegate.RemoveDynamic(this, &UOverlayWidgetController::OnToxicGasWarningReceived);
```

---

## Step 6: Blueprint Widget 경고 UI 구현 (에디터 작업)

> 이 단계는 Unreal Editor에서 수행. C++ 코드 변경 아님.

### 6-1. 기존 Overlay Widget Blueprint에서 바인딩

**파일**: `Content/Blueprints/UI/` 내 오버레이 위젯 블루프린트

1. WidgetController의 `OnToxicGasWarning` 이벤트에 바인딩
2. `bIsToxicGasWave = true` 수신 시 → 경고 위젯 Visible + 페이드인 애니메이션 재생
3. `bIsToxicGasWave = false` 수신 시 → 경고 위젯 Hidden (또는 무시, 자동 페이드아웃에 맡김)

### 6-2. 경고 위젯 UI 구성

- **위치**: 화면 상단 중앙
- **구성**: 아이콘 + 텍스트 ("Toxic Gas Warning" 등)
- **애니메이션**: 페이드인(0.3초) → 유지(3초) → 페이드아웃(0.5초)
- **선택사항**: 경고 사운드 동시 재생

---

## 구현 순서 요약

```
Step 1: DRStageGameState.h    → 변수/델리게이트/세터/RepNotify 선언
Step 2: DRStageGameState.cpp  → 초기화/복제등록/세터/RepNotify 구현
Step 3: DRPhase3.cpp          → StartNextWave()에서 플래그 설정, EndCurrentWave()에서 리셋
Step 4: OverlayWidgetController.h  → UI 델리게이트/바인딩함수/콜백 선언
Step 5: OverlayWidgetController.cpp → 바인딩/콜백/정리 구현
Step 6: Blueprint Widget      → 경고 UI 위젯 제작 (에디터 작업)
```

---

## 수정 파일별 변경량 예상

| 파일 | 추가 줄 수 | 수정 줄 수 | 난이도 |
|------|-----------|-----------|--------|
| `Source/DaeRune/Public/Game/DRStageGameState.h` | ~12줄 | 0 | 낮음 |
| `Source/DaeRune/Private/Game/DRStageGameState.cpp` | ~18줄 | 0 | 낮음 |
| `Source/DaeRune/Private/Phase/DRPhase3.cpp` | ~10줄 | 0 | 낮음 |
| `Source/DaeRune/Public/UI/WidgetController/OverlayWidgetController.h` | ~10줄 | 0 | 낮음 |
| `Source/DaeRune/Private/UI/WidgetController/OverlayWidgetController.cpp` | ~35줄 | 0 | 중간 |
| **총합** | **~85줄** | **0** | - |

모든 변경은 **추가(Add)만** 있고 기존 코드를 수정/삭제하는 부분은 없다.

---

## 데이터 흐름 (전체)

```
[서버] UDRPhase3::StartNextWave()
  │
  │  FWaveLevelModifier Modifier = GetWaveLevelModifier(CurrentWaveLevel);
  │
  ├─ GameState->SetIsToxicGasWave(Modifier.bSpawnToxicGas)     ← Step 3-1
  │     │
  │     ├─ bIsToxicGasWave = value  (Replicated)                ← Step 1-4, 2-2
  │     │
  │     ├─ OnToxicGasWarningDelegate.Broadcast(value)           ← Step 2-3 (서버 즉시)
  │     │     │
  │     │     └─ [서버] OverlayWidgetController::OnToxicGasWarningReceived()  ← Step 5-3
  │     │           └─ OnToxicGasWarning.Broadcast(value)        ← Step 4-2
  │     │                 └─ [Blueprint Widget] 경고 표시/숨김     ← Step 6
  │     │
  │     └─ [네트워크 복제] → OnRep_IsToxicGasWave()              ← Step 2-4
  │           └─ OnToxicGasWarningDelegate.Broadcast(value)
  │                 └─ [클라이언트] 동일 경로로 Widget 업데이트
  │
  ├─ if (Modifier.bSpawnToxicGas) → SpawnToxicGas()             (기존 로직)
  │
  └─ ... (나머지 기존 웨이브 로직)

[서버] UDRPhase3::EndCurrentWave()
  │
  ├─ GameState->SetIsToxicGasWave(false)                        ← Step 3-2
  │     └─ (연속 독가스 웨이브에서 RepNotify 재발동 보장)
  │
  └─ StartRestTime()                                             (기존 로직)
```

---

## 검증 체크리스트

- [ ] 독가스 웨이브 시작 → 서버/클라이언트 모두 경고 UI 표시되는지
- [ ] 비독가스 웨이브 시작 → 경고 UI가 표시되지 않는지
- [ ] 독가스 웨이브 → 독가스 웨이브 (연속) → 두 번째도 경고가 다시 표시되는지
- [ ] 독가스 웨이브 → 비독가스 웨이브 → 경고가 사라지는지
- [ ] 중간에 플레이어가 조인 → 현재 독가스 웨이브이면 경고가 보이는지
- [ ] 관전 모드 전환 → WidgetController 재생성 후 경고가 정상 동작하는지
- [ ] 게임 오버/클리어 → Phase3 종료 시 경고가 정리되는지
- [ ] 경고 UI 애니메이션이 자연스러운지 (페이드인/아웃)
