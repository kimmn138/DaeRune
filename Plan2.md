# Plan2: 관전 시스템과 대기실 카메라 충돌 해결

## Context

로비에 대기실(WaitingRoom) 시스템을 추가하면서, 기존 관전 시스템의 ViewTarget 복원 로직과 충돌이 발생했다.
게임오버/클리어 후 SeamlessTravel로 로비에 돌아오면 대기실 카메라가 아닌 플레이어 Pawn 시점이 보이는 버그.

**근본 원인**: `OnLevelEntered()`, `ClientStopSpectating()`, `ClientSetWaitingRoomView()` 3개가 모두 ~0.5초 시점에
`SetViewTarget()`을 호출하며 경쟁. 마지막 호출이 최종 ViewTarget을 결정하는데, 결과가 비결정적.

## 해결 전략

`bIsInWaitingRoom` 가드 플래그를 `ADRPlayerController`에 추가하여, 대기실 상태에서는
ViewTarget-to-Pawn 복원 로직을 모두 차단한다. 최소한의 수술적 변경으로 기존 아키텍처를 유지.

## 변경 파일 (2개)

### 1. `Source/DaeRune/Public/Player/DRPlayerController.h`

**변경**: `bIsInWaitingRoom` 멤버 변수 추가

`bIsSpectating` 선언(108번 줄) 근처에 추가:
```cpp
bool bIsInWaitingRoom = false;
```

---

### 2. `Source/DaeRune/Private/Player/DRPlayerController.cpp`

4개 함수 수정:

#### 2-1. `OnLevelEntered()` (564~627번 줄)

**변경 사항**:
- 관전 델리게이트(`OnDeathDelegate`) 명시적 정리 추가 (기존 버그 수정)
- `CurrentSpectatedPlayerIndex` 리셋 추가
- 로비 WaitingRoom 상태 감지 → `bIsInWaitingRoom = true` 설정 + ViewTarget 복원 스킵
- 0.5초 재시도 타이머에 `bIsInWaitingRoom` 가드 추가

```cpp
void ADRPlayerController::OnLevelEntered()
{
    // ... (기존 UI 정리 코드 유지: CurrentResultWidget, SettingsWidget) ...

    // ★ 관전 상태 초기화 (델리게이트 정리 포함)
    if (CurrentSpectatedCharacter.IsValid())
    {
        if (ADRCharacterBase* OldTarget = Cast<ADRCharacterBase>(CurrentSpectatedCharacter.Get()))
        {
            OldTarget->OnDeathDelegate.RemoveDynamic(this, &ADRPlayerController::OnSpectatedPlayerDied);
        }
    }
    bIsSpectating = false;
    CurrentSpectatedCharacter = nullptr;
    CurrentSpectatedPlayerIndex = 0;

    // ★ 대기실 감지 → ViewTarget 복원 차단
    ADRLobbyGameState* LGS = World->GetGameState<ADRLobbyGameState>();
    if (LGS && (LGS->GetLobbyState() == ELobbyState::WaitingRoom
             || LGS->GetLobbyState() == ELobbyState::Transitioning))
    {
        bIsInWaitingRoom = true;
        RestoreDefaultInputMode();
        return;  // ViewTarget은 ClientSetWaitingRoomView RPC가 설정
    }

    bIsInWaitingRoom = false;

    // (기존 ViewTarget 복원 로직 유지 - FreeRoam/Stage에서만 실행됨)
    if (APawn* MyPawn = GetPawn())
    {
        SetViewTarget(MyPawn);
    }
    else
    {
        SetViewTarget(this);
        // 0.5초 재시도 타이머 (내부에 bIsInWaitingRoom 가드 추가)
    }

    RestoreDefaultInputMode();
}
```

#### 2-2. `ClientStopSpectating_Implementation()` (275~328번 줄)

**변경 사항**:
- `bIsInWaitingRoom`이면 관전 상태 정리만 하고 ViewTarget 복원 스킵
- 0.5초 재시도 타이머에도 `bIsInWaitingRoom` 가드 추가

```cpp
void ADRPlayerController::ClientStopSpectating_Implementation()
{
    // (기존 관전 상태 리셋 코드 유지)

    // ★ 대기실이면 ViewTarget 복원 스킵
    if (bIsInWaitingRoom) return;

    // (기존 ViewTarget 복원 로직 유지, 재시도 타이머에 가드 추가)
}
```

#### 2-3. `ClientSetWaitingRoomView_Implementation()` (1134~1145번 줄)

**변경 사항**: `bIsInWaitingRoom = true` 설정 추가

#### 2-4. `ClientStartCameraTransitionToCharacter_Implementation()` (1147~1158번 줄)

**변경 사항**: `bIsInWaitingRoom = false` 설정 추가

---

## 시나리오별 검증

| 시나리오 | 예상 흐름 | 결과 |
|---------|----------|------|
| A: 관전 중 게임오버 → 로비 | OnLevelEntered: WaitingRoom 감지 → 스킵. ClientSetWaitingRoomView: 카메라 전환 | OK |
| B: 살아있는 채로 게임클리어 → 로비 | 동일 | OK |
| C: 최초 로비 접속 | OnLevelEntered: WaitingRoom 감지 → 스킵. PostLogin: 카메라 전환 | OK |
| D: FreeRoam 상태로 로비 복귀 | bIsInWaitingRoom=false → 기존 동작 유지 | OK |
| E: PowerOn (대기실→FreeRoam) | ClientStartCameraTransitionToCharacter: 플래그 해제 | OK |
| F: GameState 미도착 | ClientSetWaitingRoomView RPC가 이후 플래그 설정 + 카메라 전환 | OK |

## 에디터 작업

코드 수정 후 추가 에디터 작업 없음.

## 테스트 방법

1. 빌드 후 PIE 2인 테스트
2. 로비 최초 접속 → 대기실 카메라 확인
3. Stage → 사망 → 관전 → 전멸 → 로비 복귀 → 대기실 카메라 확인
4. Stage → 게임 클리어 → 로비 복귀 → 대기실 카메라 확인
