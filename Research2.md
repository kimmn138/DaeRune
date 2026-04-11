# Research2: 관전 시스템 & 로비 복귀 흐름 종합 분석

## 목차

1. [관전 시스템 전체 흐름](#1-관전-시스템-전체-흐름)
2. [관전 시작: 플레이어 사망](#2-관전-시작-플레이어-사망)
3. [관전 중 동작](#3-관전-중-동작)
4. [관전 종료](#4-관전-종료)
5. [게임 종료 → 로비 복귀 흐름](#5-게임-종료--로비-복귀-흐름)
6. [로비 진입 시 처리 흐름](#6-로비-진입-시-처리-흐름)
7. [시나리오별 상세 타임라인](#7-시나리오별-상세-타임라인)
8. [현재 코드의 문제점 분석](#8-현재-코드의-문제점-분석)
9. [함수 호출 관계도](#9-함수-호출-관계도)
10. [타이머 목록](#10-타이머-목록)

---

## 1. 관전 시스템 전체 흐름

### 상태 다이어그램

```
[살아있음] ──(사망)──→ [사망 연출 4초] ──→ [관전 중] ──(로비 복귀)──→ [로비 대기실]
                                              │                            │
                                              │ (관전 대상 사망)            │ (PowerOn)
                                              ↓                            ↓
                                         [다음 대상으로                  [FreeRoam]
                                          자동 전환 1초]
                                              │
                                              │ (전원 사망 = 게임오버)
                                              ↓
                                         [게임오버 UI 5초] ──→ [로비 복귀]
```

### 관련 플래그

| 변수 | 위치 | 용도 |
|------|------|------|
| `bIsSpectating` | DRPlayerController.h:108 | 관전 모드 활성화 여부 |
| `CurrentSpectatedPlayerIndex` | DRPlayerController.h:111 | 현재 관전 대상 인덱스 |
| `CurrentSpectatedCharacter` | DRPlayerController.h:115 | 현재 관전 대상 (TWeakObjectPtr) |
| `bIsDeadForVoice` | DRPlayerController.h:344 | 사망 후 음성 채팅 상태 |
| `bIsWipeoutInProgress` | DRGameModeBase.h:65 | 전멸 처리 진행 중 여부 |

---

## 2. 관전 시작: 플레이어 사망

### 사망 처리 (`ADRCharacterBase::Die()`) - DRCharacterBase.cpp:82~157

```
Die() 호출 (서버에서 실행)
  │
  ├─ 1. Weapon 분리 (KeepWorld)
  │
  ├─ 2. MulticastHandleDeath(DeathImpulse)  ← 모든 클라이언트에서 사망 애니메이션/VFX
  │
  ├─ 3. 운반 중인 부품 드롭 (DRCharacter::DropCarriedPart)
  │
  ├─ 4. GameMode->OnPlayerDied(PS)  ← 전멸 체크 트리거
  │
  ├─ 5. DRPC->UpdateVoiceChannelForDeathState(true)  ← 음성 채팅 상태 변경
  │
  ├─ 6. [4.0초 타이머] → DRPC->ClientStartSpectating()  ← 관전 시작 (Client RPC)
  │
  └─ 7. [3.5초 타이머] → this->Destroy()  ← 캐릭터 파괴
```

**주의**: 캐릭터 파괴(3.5초)가 관전 시작(4.0초)보다 **먼저** 발생한다.
따라서 관전이 시작될 때 자신의 Pawn은 이미 파괴된 상태이다.
이는 `GetPawn()`이 NULL을 반환하게 만들어, 관전 시 자기 자신의 ViewTarget으로 복원 불가.

### 전멸 체크 (`ADRGameModeBase::OnPlayerDied()`) - DRGameModeBase.cpp:18~50

```
OnPlayerDied(DeadPlayer) (서버에서 실행)
  │
  ├─ bIsWipeoutInProgress 체크 (중복 방지)
  │
  └─ CheckTeamWipeout()  ← 모든 PlayerState 순회, AliveCount == 0 체크
      │
      ├─ false → 아무것도 안 함 (다른 플레이어가 살아있음)
      │
      └─ true (전멸) →
          ├─ bIsWipeoutInProgress = true
          ├─ 모든 PC에 Client_ShowGameOverUI() 호출
          └─ [WipeoutDelayTime 타이머] → HandleWipeout()
```

---

## 3. 관전 중 동작

### 관전 시작 (`ClientStartSpectating`) - DRPlayerController.cpp:257~273

```cpp
ClientStartSpectating_Implementation()  // Client RPC → 클라이언트에서 실행
{
    bIsSpectating = true;
    CurrentSpectatedPlayerIndex = 0;

    AlivePlayers = GameState->GetAlivePlayers();  // 살아있는 플레이어 목록

    if (AlivePlayers.Num() > 0)
        SetSpectateTarget(AlivePlayers[0]);  // 첫 번째 생존자를 관전
}
```

### 관전 대상 설정 (`SetSpectateTarget`) - DRPlayerController.cpp:650~693

**클라이언트 측** (HasAuthority() == false):
```
1. ServerSetSpectateTarget(NewTarget)  → 서버에 알림
2. CurrentSpectatedCharacter = NewTarget  (로컬 캐시)
3. SetViewTarget(NewTarget)  → 카메라를 대상에게
```

**서버 측** (HasAuthority() == true):
```
1. 이전 대상의 OnDeathDelegate 해제
2. CurrentSpectatedCharacter = NewTarget
3. SetViewTarget(NewTarget)  → 카메라를 대상에게
4. ClientUpdateSpectatorUI(NewTarget)  → 클라이언트에 UI 업데이트 요청
5. 새 대상의 OnDeathDelegate 바인딩
```

### 관전 대상 사망 (`OnSpectatedPlayerDied`) - DRPlayerController.cpp:731~749

```
OnSpectatedPlayerDied(DeadActor)  // 델리게이트 콜백
  │
  └─ [1.0초 타이머] → SpectateNextPlayer()  ← 다음 생존자로 전환
```

### 관전 대상 전환 (SpectateNextPlayer / SpectatePreviousPlayer)

```
SpectateNextPlayer()  // DRPlayerController.cpp:330~352
  │
  ├─ bIsSpectating && IsLocalController() 체크
  ├─ GetAlivePlayers() → 생존자 목록
  ├─ AliveCharacters.Num() == 1이면 return (자기만 남음)
  ├─ CurrentSpectatedPlayerIndex = (Index + 1) % Num
  └─ SetSpectateTarget(AliveCharacters[NewIndex])
```

### 관전 중 입력 차단

모든 게임플레이 입력 함수에 `if (bIsSpectating) return;` 가드 존재:

| 함수 | 파일:라인 | 차단 |
|------|-----------|------|
| `Move()` | DRPlayerController.cpp:753 | 이동 |
| `Look()` | DRPlayerController.cpp:776 | 시점 회전 |
| `StartJump()` | DRPlayerController.cpp:801 | 점프 |
| `StopJump()` | DRPlayerController.cpp:812 | 점프 중지 |
| `HandleInteract()` | DRPlayerController.cpp:823 | 상호작용 |
| `AbilityInputTagPressed()` | DRPlayerController.cpp:1036 | 어빌리티 |
| `AbilityInputTagReleased()` | DRPlayerController.cpp:1049 | 어빌리티 |
| `AbilityInputTagHeld()` | DRPlayerController.cpp:1060 | 어빌리티 |
| `PlayerTick()` (부품 감지) | DRPlayerController.cpp:482 | 라인트레이스 |

---

## 4. 관전 종료

### `ClientStopSpectating` - DRPlayerController.cpp:275~328

```
ClientStopSpectating_Implementation()  // Client RPC → 클라이언트에서 실행
  │
  ├─ 1. bIsSpectating = false
  ├─ 2. CurrentSpectatedPlayerIndex = 0
  │
  ├─ 3. 이전 관전 대상의 OnDeathDelegate 해제
  ├─ 4. CurrentSpectatedCharacter.Reset()
  │
  ├─ 5a. (Pawn 있음) → SetViewTarget(MyPawn) + RestoreDefaultInputMode()
  │
  └─ 5b. (Pawn 없음) → [0.5초 재시도 타이머]
              → SetViewTarget(MyPawn) + RestoreDefaultInputMode()
```

**호출되는 시점**:
1. `HandleSeamlessTravelPlayer()` - 로비로 돌아올 때 (DRLobbyGameMode.cpp:131, 154)
2. `PostLogin()` - FreeRoam 상태에서 로그인 시 (DRLobbyGameMode.cpp:61)
3. `OnLevelEntered()` - 직접 호출은 아니지만 `bIsSpectating = false`로 강제 리셋 (DRPlayerController.cpp:591)

---

## 5. 게임 종료 → 로비 복귀 흐름

### 전멸 (Game Over) 경로

```
[스테이지 서버]

전체 사망 → OnPlayerDied() → CheckTeamWipeout() == true
  │
  ├─ 모든 PC에 Client_ShowGameOverUI() → 게임오버 위젯 표시
  │
  └─ TriggerGameOver() (서버)
      │
      ├─ CurrentPhase->OnPhaseEnd()  ← 현재 페이즈 종료
      ├─ Multicast_PlayGameOverSound()
      ├─ NotifyAllPlayersGameEnd(false)
      │     ├─ 모든 PC에 Client_ShowGameOverUI()
      │     └─ 모든 PC에 ClientStopAllAudio()
      │
      └─ [5.0초 타이머] → ReturnToLobby()
              │
              ├─ PrepareForTravel() (서버)
              │     ├─ 모든 PC에 ClientCloseSettingsMenu()
              │     └─ 모든 PC에 ClientStopAllAudio()
              │
              ├─ bUseSeamlessTravel = true
              └─ World->ServerTravel("LobbyMap?listen")
```

### 게임 클리어 (Game Clear) 경로

```
TriggerGameClear() (서버)  ← 거의 동일한 흐름
  │
  ├─ CurrentPhase->OnPhaseEnd()
  ├─ Multicast_PlayGameClearSound()
  ├─ NotifyAllPlayersGameEnd(true)
  │     ├─ 모든 PC에 Client_ShowGameClearUI()
  │     └─ 모든 PC에 ClientStopAllAudio()
  │
  └─ [5.0초 타이머] → ReturnToLobby()
```

### 주의: `PrepareForTravel`에서 관전 종료를 하지 않음

`PrepareForTravel()`은 `ClientCloseSettingsMenu()`과 `ClientStopAllAudio()`만 호출.
**`ClientStopSpectating()`은 호출하지 않는다.**
관전 종료는 `HandleSeamlessTravelPlayer()`에서 로비 도착 후에 처리된다.

즉, **플레이어가 관전 중인 상태로 SeamlessTravel에 진입할 수 있다.**

---

## 6. 로비 진입 시 처리 흐름

### 6.1 SeamlessTravel로 돌아온 경우

SeamlessTravel은 PlayerController를 유지한 채 맵을 전환한다.
따라서 PlayerController의 모든 로컬 변수(bIsSpectating 등)가 이전 맵의 상태를 유지한다.

#### 서버 측: `HandleSeamlessTravelPlayer()` - DRLobbyGameMode.cpp:110~175

```
HandleSeamlessTravelPlayer(Controller)  (서버에서 실행)
  │
  ├─ [WaitingRoom 상태]
  │     └─ [0.5초 타이머] →
  │           ├─ ClientStopSpectating()      ★ 관전 종료 RPC
  │           ├─ AssignPlayerToSlot(DRPC)    ★ 슬롯 배치 (서버)
  │           └─ ClientSetWaitingRoomView()  ★ 대기실 카메라 전환 RPC
  │
  └─ [FreeRoam 상태]
        └─ [0.5초 타이머] →
              ├─ ClientStopSpectating()      ★ 관전 종료 RPC
              ├─ MovementComp->SetMovementMode(MOVE_Walking)
              ├─ MovementComp->SetComponentTickEnabled(true)
              └─ CapsuleComp->SetCollisionEnabled(QueryAndPhysics)
```

#### 클라이언트 측: `PostSeamlessTravel()` → `OnLevelEntered()`

```
PostSeamlessTravel()  (클라이언트에서 실행)
  │
  └─ OnLevelEntered()
        │
        ├─ 1. CurrentResultWidget 제거 (게임오버/클리어 UI)
        ├─ 2. SettingsWidget 정리
        ├─ 3. bIsSpectating = false       ★ 관전 강제 리셋
        ├─ 4. CurrentSpectatedCharacter = nullptr
        │
        ├─ 5a. (Pawn 있음) → SetViewTarget(MyPawn)  ★ ViewTarget을 Pawn으로
        ├─ 5b. (Pawn 없음) → SetViewTarget(this) + [0.5초 재시도] → SetViewTarget(MyPawn)
        │
        └─ 6. RestoreDefaultInputMode()
                └─ IsInLobby() + WaitingRoom → FInputModeUIOnly + ShowMouseCursor
```

### 6.2 최초 접속 (PostLogin) 경우

신규 플레이어가 로비에 처음 접속할 때. 관전 상태가 아니므로 단순.

#### 서버 측: `PostLogin()` - DRLobbyGameMode.cpp:22~108

```
PostLogin(NewPlayer)  (서버에서 실행)
  │
  ├─ [WaitingRoom 상태]
  │     └─ [0.5초 타이머] →
  │           ├─ AssignPlayerToSlot(DRPC)    ★ 슬롯 배치
  │           └─ ClientSetWaitingRoomView()  ★ 카메라 전환
  │           (ClientStopSpectating 호출 안 함 - 최초 접속이므로 관전 중이 아님)
  │
  └─ [FreeRoam 상태]
        └─ [0.5초 타이머] →
              ├─ ClientStopSpectating()
              └─ 움직임/충돌 복원
```

#### 클라이언트 측: `ReceivedPlayer()` → `OnLevelEntered()`

```
ReceivedPlayer()  (클라이언트에서 실행)
  │
  └─ OnLevelEntered()  (위와 동일한 로직)
```

### 6.3 BeginPlay (클라이언트 초기화)

```
ADRPlayerController::BeginPlay()  (클라이언트에서 실행)
  │
  ├─ Enhanced Input Context 추가 (DRContext)
  ├─ bShowMouseCursor = false
  ├─ SetInputMode(FInputModeGameOnly())  ← 임시로 게임 모드
  ├─ SetGenericTeamId(0)
  ├─ 카메라 피치 제한 설정
  ├─ 오디오 설정 적용
  └─ RestoreDefaultInputMode()  ← 로비면 UIOnly로 복원
```

---

## 7. 시나리오별 상세 타임라인

### 시나리오 A: 관전 중 로비로 복귀 (SeamlessTravel)

전형적인 흐름: 플레이어가 사망하여 다른 플레이어를 관전 중, 전멸으로 게임오버.

```
=== 스테이지 맵 ===

[T+0.0s]  플레이어 사망 → Die()
[T+0.0s]  MulticastHandleDeath → 사망 VFX
[T+0.0s]  OnPlayerDied → CheckTeamWipeout (아직 다른 플레이어 생존)
[T+3.5s]  캐릭터 Destroy() → GetPawn() = NULL
[T+4.0s]  ClientStartSpectating → bIsSpectating = true, SetViewTarget(생존자)

... (관전 중, 다른 플레이어도 사망) ...

[T+X]     마지막 플레이어 사망 → OnPlayerDied → CheckTeamWipeout == true
[T+X]     Client_ShowGameOverUI → 게임오버 UI 표시
[T+X]     TriggerGameOver → 5초 타이머 등록

[T+X+5s]  ReturnToLobby()
            → PrepareForTravel (설정창 닫기, 오디오 정리)
            → ServerTravel("LobbyMap?listen")

=== SeamlessTravel 진행 중 ===
(PlayerController 유지, bIsSpectating = true 상태 유지)
(CurrentSpectatedCharacter는 이전 맵 캐릭터 → 파괴됨 → WeakPtr 무효화)

=== 로비 맵 ===

[T+0.0ms] GameMode::BeginPlay()
            → FindWaitingRoomActors() → 카메라 + 슬롯 탐색

[T+~0ms]  PlayerController::PostSeamlessTravel() → OnLevelEntered()
            → bIsSpectating = false  (관전 강제 리셋)
            → CurrentSpectatedCharacter = nullptr
            → SetViewTarget(MyPawn) 또는 SetViewTarget(this) + 0.5초 재시도
            → RestoreDefaultInputMode() → UIOnly

[T+~0ms]  GameMode::HandleSeamlessTravelPlayer()
            → 0.5초 타이머 등록

[T+500ms] HandleSeamlessTravelPlayer 타이머 실행
            → ClientStopSpectating()   ← bIsSpectating은 이미 false
              └─ 중복이지만 해가 되지 않음... 단, ViewTarget을 Pawn으로 덮어씀!
            → AssignPlayerToSlot()     ← 슬롯 배치
            → ClientSetWaitingRoomView() ← 카메라 전환

[T+500ms] OnLevelEntered의 0.5초 재시도 타이머 (Pawn 없었던 경우)
            → SetViewTarget(MyPawn)    ← ★ 카메라 전환을 덮어씀!!!
```

**문제 발생 지점**: T+500ms에서 3개의 RPC/타이머가 거의 동시에 경쟁:
1. `ClientStopSpectating` → `SetViewTarget(MyPawn)` (0.5초 재시도 포함)
2. `ClientSetWaitingRoomView` → `SetViewTargetWithBlend(Camera, 0.f)`
3. `OnLevelEntered`의 0.5초 재시도 → `SetViewTarget(MyPawn)`

마지막으로 실행되는 것이 최종 ViewTarget을 결정한다.

### 시나리오 B: 관전 안 하고 로비로 복귀 (살아있는 채로 게임오버)

플레이어가 살아있는데 다른 원인으로 게임이 끝나는 경우 (예: 게임 클리어).

```
=== 스테이지 맵 ===

[T+0]     TriggerGameClear()
            → Client_ShowGameClearUI() → 게임 클리어 UI
            → 5초 타이머 → ReturnToLobby()

[T+5s]    ReturnToLobby()
            → PrepareForTravel() → 오디오/설정 정리
            → ServerTravel("LobbyMap?listen")

=== SeamlessTravel 진행 중 ===
(bIsSpectating = false, Pawn은 존재하지만 이전 맵에 속함)

=== 로비 맵 ===

[T+~0ms]  PostSeamlessTravel() → OnLevelEntered()
            → bIsSpectating = false (이미 false)
            → SetViewTarget(MyPawn) 또는 재시도
            → RestoreDefaultInputMode() → UIOnly

[T+~0ms]  HandleSeamlessTravelPlayer()
            → 0.5초 타이머 등록

[T+500ms] 타이머 실행
            → ClientStopSpectating()  ← 불필요하지만 무해
            → AssignPlayerToSlot()
            → ClientSetWaitingRoomView() ← 카메라 전환

[T+500ms] OnLevelEntered 재시도 타이머 (만약 Pawn이 없었다면)
            → SetViewTarget(MyPawn) ← ★ 동일한 경쟁 문제
```

### 시나리오 C: 최초 로비 접속 (게임 시작 시)

```
=== 로비 맵 ===

[T+0ms]   GameMode::BeginPlay()
            → FindWaitingRoomActors() → 카메라 + 슬롯 탐색
            → AllowJoinInProgress()

[T+~0ms]  GameMode::PostLogin(호스트)
            → 0.5초 타이머 등록

[T+~0ms]  PlayerController::BeginPlay()
            → SetInputMode(FInputModeGameOnly())  ← 임시
            → RestoreDefaultInputMode() → UIOnly

[T+~0ms]  PlayerController::ReceivedPlayer() → OnLevelEntered()
            → bIsSpectating = false
            → SetViewTarget(MyPawn) 또는 재시도
            → RestoreDefaultInputMode() → UIOnly

[T+500ms] PostLogin 타이머 실행
            → AssignPlayerToSlot()
            → ClientSetWaitingRoomView() ← 카메라 전환

[T+500ms] OnLevelEntered 재시도 타이머 (Pawn이 없었다면)
            → SetViewTarget(MyPawn) ← ★ 카메라를 덮어씀
```

### 시나리오 D: 관전 중 관전 대상이 사망

```
[T+0ms]   관전 대상 사망 → OnSpectatedPlayerDied() 호출
            (관전 대상의 OnDeathDelegate에 바인딩되어 있음)

[T+1.0s]  SpectateNextPlayer() 호출
            → GetAlivePlayers()
            → (생존자 있음) → 다음 생존자에게 SetSpectateTarget()
            → (생존자 없음) → CurrentSpectatedCharacter.Reset()
                              (마지막 대상의 카메라 위치에 고정)

            → (1명만 남음) → return (전환하지 않음, 유일한 생존자 계속 관전)
```

---

## 8. 현재 코드의 문제점 분석

### 문제 1: ViewTarget 타이밍 경쟁 (핵심 버그)

**원인**: `OnLevelEntered()`의 ViewTarget 복원과 `ClientSetWaitingRoomView`가 경쟁

**영향받는 시나리오**: 시나리오 A, B, C 모두

**상세**:
- `OnLevelEntered()`는 무조건 `SetViewTarget(MyPawn)` 호출
- 0.5초 재시도 타이머가 `ClientSetWaitingRoomView`를 덮어씀
- 어떤 것이 마지막에 실행되느냐에 따라 결과가 달라짐 (비결정적)

### 문제 2: `ClientStopSpectating`의 중복 ViewTarget 복원

**원인**: `HandleSeamlessTravelPlayer()`가 WaitingRoom 상태에서 `ClientStopSpectating()` 호출 후 `ClientSetWaitingRoomView()` 호출

**상세**:
```cpp
// DRLobbyGameMode.cpp:131-135 (같은 0.5초 타이머 안에서)
DRPC->ClientStopSpectating();        // → SetViewTarget(MyPawn) 또는 0.5초 재시도
AssignPlayerToSlot(DRPC);
DRPC->ClientSetWaitingRoomView();    // → SetViewTargetWithBlend(Camera, 0.f)
```

`ClientStopSpectating`의 내부 재시도 타이머(0.5초)가 나중에 실행되면 카메라를 덮어쓴다.
**Client RPC는 호출 순서대로 클라이언트에 전달되지만**, 내부 타이머는 별도로 동작한다.

### 문제 3: `OnLevelEntered`에서 관전 델리게이트 정리 누락

**현재 코드**:
```cpp
// DRPlayerController.cpp:591-592
bIsSpectating = false;
CurrentSpectatedCharacter = nullptr;  // 직접 nullptr 대입
```

**문제**:
- `OnDeathDelegate.RemoveDynamic()`을 호출하지 않고 포인터만 nullptr로 설정
- SeamlessTravel 시 이전 맵의 캐릭터는 파괴되므로 실제로는 WeakPtr이 자동 무효화됨
- 하지만 명시적 정리를 하지 않는 것은 코드 안정성 측면에서 불완전

### 문제 4: 캐릭터 파괴 타이밍과 관전 시작 타이밍 역전

```
[3.5초] 캐릭터 Destroy()  ← 먼저 파괴
[4.0초] ClientStartSpectating()  ← 이후 관전 시작
```

이 순서 자체는 의도적일 수 있지만 (파괴 후 관전), 관전 시작 시 자신의 Pawn이 NULL이므로
관전 종료 시 ViewTarget 복원이 즉시 불가능하여 재시도 타이머에 의존하게 된다.

### 문제 5: 관전 중 게임오버 UI 이중 표시

**흐름**:
1. 첫 번째 사망 시 `OnPlayerDied` → 아직 전멸 아님
2. 마지막 사망 시 `OnPlayerDied` → `CheckTeamWipeout` == true
   - 모든 PC에 `Client_ShowGameOverUI()` 호출
3. `TriggerGameOver()` → `NotifyAllPlayersGameEnd(false)`
   - 다시 모든 PC에 `Client_ShowGameOverUI()` 호출

`Client_ShowGameOverUI`에 `if (CurrentResultWidget) return;` 가드가 있어 실제로 중복 표시는 안 되지만, 불필요한 RPC 호출이 2번 발생.

### 문제 6: PostLogin에서 WaitingRoom일 때 ClientStopSpectating 미호출

```cpp
// PostLogin - WaitingRoom 상태 (DRLobbyGameMode.cpp:31-48)
if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
{
    // ClientStopSpectating() 호출 안 함!
    AssignPlayerToSlot(DRPC);
    ClientSetWaitingRoomView(WaitingRoomCamera);
}
```

최초 접속 시에는 문제 없지만, 만약 어떤 이유로 PostLogin이 관전 중인 플레이어에게 호출되면
`bIsSpectating`이 true인 상태로 대기실 카메라가 설정된다.
(실제로는 최초 접속 시 관전 중이 아니므로 큰 문제는 아님)

---

## 9. 함수 호출 관계도

### 사망 → 관전 → 게임오버 → 로비 복귀 전체 흐름

```
ADRCharacterBase::Die()
  ├─→ MulticastHandleDeath()           [모든 클라이언트]
  ├─→ ADRGameModeBase::OnPlayerDied()  [서버]
  │     └─→ CheckTeamWipeout()
  │           └─→ (전멸 시) Client_ShowGameOverUI()  [모든 클라이언트]
  │                         HandleWipeout() 타이머
  │                           └─→ (Stage) TriggerGameOver()
  │                                  └─→ NotifyAllPlayersGameEnd()
  │                                  └─→ ReturnToLobby() 타이머
  │                                         └─→ PrepareForTravel()
  │                                         └─→ ServerTravel()
  ├─→ UpdateVoiceChannelForDeathState() [클라이언트]
  ├─→ ClientStartSpectating()          [클라이언트, 4초 후]
  └─→ Destroy()                        [서버, 3.5초 후]

=== SeamlessTravel ===

ADRLobbyGameMode::HandleSeamlessTravelPlayer()  [서버]
  └─→ (WaitingRoom)
        ├─→ ClientStopSpectating()        [클라이언트]
        │     └─→ SetViewTarget(MyPawn)   (또는 0.5초 재시도)
        │     └─→ RestoreDefaultInputMode()
        ├─→ AssignPlayerToSlot()          [서버]
        │     └─→ PositionPawnAtSlot()
        └─→ ClientSetWaitingRoomView()    [클라이언트]
              └─→ SetViewTargetWithBlend(Camera, 0.f)

ADRPlayerController::PostSeamlessTravel()  [클라이언트]
  └─→ OnLevelEntered()
        ├─→ bIsSpectating = false
        ├─→ SetViewTarget(MyPawn) 또는 0.5초 재시도
        └─→ RestoreDefaultInputMode()
```

### ViewTarget 변경 함수 호출 위치 정리

| 함수 | 파일:라인 | ViewTarget 설정 대상 |
|------|-----------|---------------------|
| `OnLevelEntered` | DRPlayerController.cpp:595 | MyPawn (즉시) |
| `OnLevelEntered` (재시도) | DRPlayerController.cpp:611 | MyPawn (0.5초 후) |
| `ClientStopSpectating` | DRPlayerController.cpp:296 | MyPawn (즉시) |
| `ClientStopSpectating` (재시도) | DRPlayerController.cpp:315 | MyPawn (0.5초 후) |
| `ClientSetWaitingRoomView` | DRPlayerController.cpp:1140 | CameraActor (즉시) |
| `ClientStartCameraTransitionToCharacter` | DRPlayerController.cpp:1152 | MyPawn (1.5초 블렌드) |
| `SetSpectateTarget` (클라) | DRPlayerController.cpp:661 | NewTarget (즉시) |
| `SetSpectateTarget` (서버) | DRPlayerController.cpp:681 | NewTarget (즉시) |
| `ClientStartSpectating` | DRPlayerController.cpp:271 | (SetSpectateTarget을 통해) |

---

## 10. 타이머 목록

### 관전/카메라 관련 타이머

| 이벤트 | 딜레이 | 실행 함수 | 위치 |
|--------|--------|----------|------|
| 사망 → 관전 시작 | 4.0초 | `ClientStartSpectating()` | DRCharacterBase.cpp:120~132 |
| 사망 → 캐릭터 파괴 | 3.5초 | `Destroy()` | DRCharacterBase.cpp:136~148 |
| 관전 대상 사망 → 다음 대상 | 1.0초 | `SpectateNextPlayer()` | DRPlayerController.cpp:737 |
| 관전 종료 → Pawn 재시도 | 0.5초 | `SetViewTarget(MyPawn)` | DRPlayerController.cpp:309~326 |
| OnLevelEntered → Pawn 재시도 | 0.5초 | `SetViewTarget(MyPawn)` | DRPlayerController.cpp:604~621 |
| PostLogin → 슬롯+카메라 | 0.5초 | `AssignPlayerToSlot` + `ClientSetWaitingRoomView` | DRLobbyGameMode.cpp:33~48 |
| HandleSeamlessTravel → 슬롯+카메라 | 0.5초 | `ClientStopSpectating` + `AssignPlayerToSlot` + `ClientSetWaitingRoomView` | DRLobbyGameMode.cpp:124~139 |

### 게임 종료 관련 타이머

| 이벤트 | 딜레이 | 실행 함수 | 위치 |
|--------|--------|----------|------|
| 전멸 → HandleWipeout | WipeoutDelayTime (스테이지 5초, 로비 2초) | `HandleWipeout()` | DRGameModeBase.cpp:42~48 |
| TriggerGameOver → 로비 복귀 | WipeoutDelayTime (5초) | `ReturnToLobby()` | DRStageGameMode.cpp:43~49 |
| TriggerGameClear → 로비 복귀 | WipeoutDelayTime (5초) | `ReturnToLobby()` | DRStageGameMode.cpp:76~82 |

---

## 부록: 핵심 문제 요약

1. **ViewTarget 경쟁**: `OnLevelEntered`/`ClientStopSpectating`/`ClientSetWaitingRoomView`가 동일한 0.5초 시점에 경쟁하며 최종 ViewTarget이 비결정적
2. **대기실 인식 부재**: `OnLevelEntered()`와 `ClientStopSpectating()`이 대기실 상태를 고려하지 않고 무조건 Pawn으로 복원
3. **관전 델리게이트 정리**: `OnLevelEntered()`에서 `OnDeathDelegate` 명시적 해제 없이 포인터만 nullptr 처리
4. **캐릭터 파괴 역전**: 캐릭터가 3.5초에 파괴되고 관전이 4.0초에 시작되어, Pawn 없는 상태로 관전 진입
5. **게임오버 UI 이중 호출**: `OnPlayerDied`와 `TriggerGameOver`에서 중복 호출 (가드로 방어되지만 불필요)
