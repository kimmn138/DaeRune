# Plan5: 대기실 잔여 버그 3건 수정 계획 (FreeRoam 전환 + Kick 관련)

## 0) 문서 목적

이전 세션에서 대부분의 대기실 버그를 수정하였으나, 다음 3건의 버그가 남아있다.
이 문서는 각 버그의 **근본 원인**을 코드 레벨에서 분석하고, **안전한 수정 방법**을 상세히 기술한다.

---

## 1) 버그 목록

| # | 증상 | 영향 범위 | 심각도 |
|---|------|-----------|--------|
| 1 | FreeRoam 진입 시 호스트는 정면 고정 시점, 클라이언트는 UIInputMode(마우스 커서만 보이고 조작 불가) | 호스트 + 클라이언트 | **높음** |
| 2 | Kick 후 3p 화면에서 디스플레이 캐릭터 위치가 갱신되지 않음 (3p가 캐릭터 변경 버튼을 누르면 그제서야 이동) | 클라이언트 전용 | 중간 |
| 3 | Kick 후 호스트 화면에서 모든 디스플레이 캐릭터가 바닥을 뚫고 내려가 있음 | 호스트(Listen Server) 전용 | 중간 |

---

## 2) Bug 1: FreeRoam 전환 시 호스트 시점 고정 + 클라이언트 UIInputMode 고착

### 2.1) 현상 상세

PowerOn 실행 후 FreeRoam 상태 진입 시:
- **호스트**: 카메라가 대기실 캐릭터의 FacingRotation 방향에 고정됨. 마우스 Look이 작동하지 않는 것처럼 보임.
- **클라이언트**: `FInputModeUIOnly` 상태가 유지됨. 마우스 커서만 보이고 이동/회전 등 게임 조작이 전혀 먹히지 않음.

### 2.2) 근본 원인 분석

#### PowerOn 실행 흐름 (`DRLobbyGameMode.cpp:308-385`)

```
[서버] LGS->SetLobbyState(Transitioning)
[서버] DestroyAllDisplayCharacters()
[서버] for (each PC):
         SpawnActor<ADRCharacter>(...)          // 새 캐릭터 스폰
         PC->Possess(NewPawn)                   // 서버에서 Possess
         NewPawn->SetReplicateMovement(true)
         CMC->SetMovementMode(MOVE_Walking)
         PC->ClientStartCameraTransitionToCharacter()  // Client RPC
[서버] Timer(2.0s) → SetLobbyState(FreeRoam)
```

#### `ClientStartCameraTransitionToCharacter_Implementation` (DRPlayerController.cpp:1249-1297)

```cpp
void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
    bIsInWaitingRoom = false;

    APawn* MyPawn = GetPawn();
    if (!MyPawn) return;                    // ★ 핵심: Pawn이 null이면 여기서 return

    DRCharacter->SetWaitingRoomVisibility(false);
    SetViewTargetWithBlend(MyPawn, 1.5f);   // 고정카메라→캐릭터 블렌드

    Timer(1.5s) → {
        bAutoManageActiveCameraTarget = true;
        SetViewTarget(MyPawn);
        SetControlRotation(MyPawn->GetActorRotation());  // ★ FacingRotation으로 초기화
        SetInputMode(FInputModeGameOnly());               // ★ Game 인풋으로 전환
        SetShowMouseCursor(false);
        InitOverlayForFreeRoam();
    }
}
```

#### 클라이언트 문제 (UIInputMode 고착)

**근본 원인**: `ClientStartCameraTransitionToCharacter` Client RPC가 클라이언트에서 실행될 때, **Pawn이 아직 리플리케이트되지 않은 상태**일 수 있다.

RPC 실행 순서:
1. 서버에서 `PC->Possess(NewPawn)` 호출 → Pawn 프로퍼티에 리플리케이션 마킹
2. 서버에서 `PC->ClientStartCameraTransitionToCharacter()` Client RPC 전송
3. 클라이언트 수신:
   - Client RPC `ClientStartCameraTransitionToCharacter` 도착
   - 이 시점에서 NewPawn 액터가 아직 클라이언트에 스폰/리플리케이트되지 않았을 수 있음
   - `GetPawn()` → **nullptr 반환**
   - **line 1256에서 `return`** → 1.5초 타이머가 설정되지 않음
   - 결과: `SetInputMode(FInputModeGameOnly())`가 **영원히 호출되지 않음**
   - 클라이언트는 `FInputModeUIOnly` 상태로 영구 고착

**핵심 코드 위치**: `DRPlayerController.cpp:1255-1256`
```cpp
APawn* MyPawn = GetPawn();
if (!MyPawn) return;   // ← Pawn 미도착 시 전체 함수 스킵
```

#### 호스트 문제 (시점 고정)

**근본 원인**: 호스트(Listen Server)에서는 Client RPC가 **즉시 동기 실행**된다. 따라서 `GetPawn()`은 null이 아님. 하지만:

1. `ClientStartCameraTransitionToCharacter` 실행 시 `bIsInWaitingRoom = false`로 설정
2. `SetViewTargetWithBlend(MyPawn, 1.5f)` → 1.5초 블렌드 시작
3. **1.5초 후 타이머 콜백**: `SetControlRotation(MyPawn->GetActorRotation())`

문제: `MyPawn->GetActorRotation()`은 스폰 시 `WaitingRoomCamera->GetCharacterFacingRotation()`으로 설정된 방향이다. 이 방향은 **카메라를 향하는 방향** (대기실에서 캐릭터가 카메라를 바라보는 방향)이므로, FreeRoam 진입 시 ControlRotation이 이 고정 방향으로 설정된다.

그러나 이 자체만으로는 "마우스 Look이 안 되는" 문제가 발생하지 않아야 한다 (초기 방향만 카메라 쪽이고, 이후 마우스로 회전 가능해야 함). 추가 원인을 분석하면:

- `bAutoManageActiveCameraTarget`이 대기실에서 `false`로 설정됨 (line 1252 주석 참조)
- 1.5초 타이머에서 `true`로 복원 (line 1276)
- **그런데** `PossessedBy` (DRCharacter.cpp)에서 `bIsInWaitingRoom`이 true일 때 `bAutoManageActiveCameraTarget = false` 설정
- 호스트에서 `Possess` 호출 시점에 `bIsInWaitingRoom`은 아직 `true` (Client RPC 실행 전)
- `Possess` → `PossessedBy` → `bAutoManageActiveCameraTarget = false`
- 이후 `ClientStartCameraTransitionToCharacter` 즉시 실행 → `bIsInWaitingRoom = false`

이 부분은 1.5초 타이머에서 `bAutoManageActiveCameraTarget = true`로 복원되므로 정상적으로 해결되어야 하지만, 만약 타이머 콜백에서 문제가 발생하면 카메라 관리가 비활성 상태로 남을 수 있다.

**추가 확인 필요**: 호스트의 실제 문제가 "완전히 회전 불가"인지, "초기 방향이 카메라 쪽이지만 회전은 가능"인지에 따라 수정 방향이 달라진다. 현재 보고에서는 "시점이 고정된 상태"라고 하므로, `bAutoManageActiveCameraTarget` 복원 실패 또는 InputMode 전환 실패 가능성도 고려한다.

### 2.3) 수정 계획

#### 수정 방향: Pawn 대기 + 재시도 패턴 도입

클라이언트에서 Pawn이 아직 리플리케이트되지 않은 경우, **짧은 간격으로 재시도**하여 Pawn이 도착할 때까지 기다린다.

#### 파일: `DRPlayerController.h`

**변경 1**: 재시도 타이머 핸들 멤버 변수 추가

위치: 기존 대기실 관련 멤버 변수 영역 (`CachedWaitingRoomCamera` 선언 근처)

```cpp
/** ClientStartCameraTransitionToCharacter에서 Pawn 대기용 재시도 타이머 */
FTimerHandle CameraTransitionRetryHandle;
```

#### 파일: `DRPlayerController.cpp`

**변경 2**: `ClientStartCameraTransitionToCharacter_Implementation` 수정

현재 코드 (line 1249-1297):
```cpp
void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
    bIsInWaitingRoom = false;

    APawn* MyPawn = GetPawn();
    if (!MyPawn) return;   // ← 문제: Pawn 없으면 전체 로직 스킵

    // ... 블렌드 + 타이머 로직 ...
}
```

수정 후:
```cpp
void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
    bIsInWaitingRoom = false;

    APawn* MyPawn = GetPawn();
    if (!MyPawn)
    {
        // Pawn이 아직 리플리케이트되지 않음 → 0.1초 간격으로 재시도
        GetWorldTimerManager().SetTimer(
            CameraTransitionRetryHandle,
            [this]()
            {
                if (!IsValid(this)) return;

                APawn* Pawn = GetPawn();
                if (!Pawn) return;  // 아직 없으면 다음 tick에 재시도

                // 타이머 중지 (Pawn 도착 완료)
                GetWorldTimerManager().ClearTimer(CameraTransitionRetryHandle);

                // 원래 카메라 전환 로직 실행
                ExecuteCameraTransitionToCharacter();
            },
            0.1f,   // 100ms 간격
            true    // 반복 실행
        );
        return;
    }

    // Pawn이 이미 있으면 즉시 실행 (호스트 또는 빠른 리플리케이션)
    ExecuteCameraTransitionToCharacter();
}
```

**변경 3**: 카메라 전환 로직을 별도 함수로 추출

```cpp
void ADRPlayerController::ExecuteCameraTransitionToCharacter()
{
    APawn* MyPawn = GetPawn();
    if (!MyPawn) return;

    // 메시 가시성 복원 (3P 숨기기, 1P 보이기 — 일반 FPS 모드)
    if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(MyPawn))
    {
        DRCharacter->SetWaitingRoomVisibility(false);
    }

    // 고정 카메라 → 캐릭터 카메라로 1.5초간 부드럽게 블렌드
    SetViewTargetWithBlend(MyPawn, 1.5f, EViewTargetBlendFunction::VTBlend_EaseInOut);

    // 전환 완료 후 인풋 모드 변경 + HUD 오버레이 초기화
    FTimerHandle InputTimerHandle;
    GetWorldTimerManager().SetTimer(
        InputTimerHandle,
        [this]()
        {
            if (!IsValid(this)) return;

            // (1) 블렌드 완료 후에야 자동 카메라 관리 활성화
            bAutoManageActiveCameraTarget = true;

            // (2) ViewTarget을 Pawn으로 확정 (블렌드 잔여 상태 정리)
            if (APawn* FinalPawn = GetPawn())
            {
                SetViewTarget(FinalPawn);

                // (3) ControlRotation을 Pawn의 현재 회전으로 동기화
                SetControlRotation(FinalPawn->GetActorRotation());
            }

            // (4) 입력 모드 전환
            SetInputMode(FInputModeGameOnly());
            SetShowMouseCursor(false);

            // (5) HUD 오버레이 초기화 (대기실에서 스킵했으므로)
            InitOverlayForFreeRoam();
        },
        1.5f,
        false
    );
}
```

#### 파일: `DRPlayerController.h`

**변경 4**: `ExecuteCameraTransitionToCharacter` 선언 추가

```cpp
/** 카메라 전환 로직 (Pawn 존재 보장 후 호출) */
void ExecuteCameraTransitionToCharacter();
```

### 2.4) 동작 원리

#### 클라이언트 (Pawn 미도착 시)

```
[클라이언트] ClientStartCameraTransitionToCharacter RPC 도착
  → bIsInWaitingRoom = false
  → GetPawn() == nullptr
  → 재시도 타이머 시작 (0.1초 간격)
[0.1초 후] GetPawn() == nullptr → 대기
[0.2초 후] GetPawn() == nullptr → 대기
[0.3초 후] GetPawn() == NewPawn (리플리케이션 완료!)
  → 타이머 중지
  → ExecuteCameraTransitionToCharacter() 실행
  → SetViewTargetWithBlend(MyPawn, 1.5f) + 1.5초 타이머
[1.8초 후] SetInputMode(FInputModeGameOnly()) → 정상 조작 가능
```

#### 호스트 (Pawn 즉시 존재)

```
[호스트] ClientStartCameraTransitionToCharacter 즉시 실행
  → bIsInWaitingRoom = false
  → GetPawn() == NewPawn (서버이므로 즉시 존재)
  → ExecuteCameraTransitionToCharacter() 즉시 실행
  → SetViewTargetWithBlend + 1.5초 타이머
[1.5초 후] bAutoManageActiveCameraTarget = true
           SetViewTarget(MyPawn) (확정)
           SetControlRotation(MyPawn->GetActorRotation())
           SetInputMode(FInputModeGameOnly()) → 정상 조작 가능
```

### 2.5) 영향 범위 분석

- 기존 `ClientStartCameraTransitionToCharacter_Implementation` 내부 로직만 리팩토링 (외부 인터페이스 변경 없음)
- `ExecuteCameraTransitionToCharacter`는 private 헬퍼 → 다른 코드에서 호출하지 않음
- `bIsInWaitingRoom = false`는 재시도 전에 즉시 설정 → `OnRep_Pawn`, `ClientRestart`에서의 대기실 보호 로직 비활성화됨 (의도된 동작)
- 재시도 타이머는 Pawn 도착 시 즉시 중지 → 불필요한 반복 없음

---

## 3) Bug 2: Kick 후 3p 화면에서 디스플레이 캐릭터 위치 미갱신

### 3.1) 현상 상세

- 호스트가 2p를 Kick하면 호스트 화면에서는 3p의 디스플레이 캐릭터가 2p 자리(슬롯 1)로 올바르게 이동함
- 그러나 3p 화면에서는 디스플레이 캐릭터들이 이전 위치 그대로 남아있음
- 3p가 캐릭터 변경 버튼을 누르면 그제서야 올바른 위치로 이동함

### 3.2) 근본 원인 분석

#### `RepositionAllPlayers` (DRLobbyGameMode.cpp:539-588)

```cpp
void ADRLobbyGameMode::RepositionAllPlayers()
{
    // 1. 슬롯 재할당
    PlayerSlotMap.Empty();
    NextAvailableSlot = 0;
    for (AController* Player : OrderedPlayers)
    {
        AssignPlayerToSlot(Player);
    }

    // 2. 디스플레이 캐릭터 위치 갱신 ★ 문제 지점
    for (auto& Pair : DisplayCharacterMap)
    {
        if (ADRCharacter* Display = Pair.Value.Get())
        {
            int32* SlotIdx = PlayerSlotMap.Find(Pair.Key);
            if (SlotIdx && WaitingRoomSlots.IsValidIndex(*SlotIdx))
            {
                FVector Loc = WaitingRoomSlots[*SlotIdx]->GetActorLocation();
                FRotator Rot = WaitingRoomCamera->GetCharacterFacingRotation();
                Display->SetActorLocationAndRotation(Loc, Rot, ...);  // ★
            }
        }
    }

    BroadcastRefreshWaitingRoomUI();
}
```

#### `SpawnDisplayCharacter` (DRLobbyGameMode.cpp:687-719)

```cpp
void ADRLobbyGameMode::SpawnDisplayCharacter(...)
{
    // ...
    ADRCharacter* Display = GetWorld()->SpawnActor<ADRCharacter>(...);
    // ...
    Display->SetReplicateMovement(false);   // ★ 이동 리플리케이션 비활성화!
    DisplayCharacterMap.Add(Player, Display);
}
```

**근본 원인**: 디스플레이 캐릭터는 `SetReplicateMovement(false)`로 스폰된다. 이 상태에서 서버가 `SetActorLocationAndRotation()`을 호출해도 **위치 변경이 클라이언트에 리플리케이트되지 않는다**.

- **호스트**: 서버이므로 `SetActorLocationAndRotation()`이 로컬에서 즉시 반영됨 → 올바른 위치 표시
- **클라이언트**: 디스플레이 캐릭터의 Movement 리플리케이션이 꺼져있으므로 위치 변경이 전달되지 않음 → 이전 위치 유지

캐릭터 변경 버튼을 누르면 `UpdateDisplayCharacter` → `DestroyDisplayCharacter` → `SpawnDisplayCharacter`가 호출되어 새 디스플레이 캐릭터가 **올바른 슬롯 위치에 스폰**되므로 그때서야 올바른 위치가 보인다.

### 3.3) 수정 계획

#### 수정 방향: 기존 `MulticastTeleportToSlot` RPC 활용

`ADRCharacter`에는 이미 `MulticastTeleportToSlot(FVector, FRotator)` 함수가 있다 (DRCharacter.cpp:332-342). 이 함수는 `UFUNCTION(NetMulticast, Reliable)`로 선언되어 **모든 네트워크 엔드포인트에서 실행**된다.

#### 파일: `DRLobbyGameMode.cpp`

**변경 5**: `RepositionAllPlayers`의 디스플레이 캐릭터 위치 갱신 코드 수정

현재 코드 (line 570-584):
```cpp
// 디스플레이 캐릭터 위치 갱신
for (auto& Pair : DisplayCharacterMap)
{
    if (ADRCharacter* Display = Pair.Value.Get())
    {
        int32* SlotIdx = PlayerSlotMap.Find(Pair.Key);
        if (SlotIdx && WaitingRoomSlots.IsValidIndex(*SlotIdx))
        {
            FVector Loc = WaitingRoomSlots[*SlotIdx]->GetActorLocation();
            FRotator Rot = WaitingRoomCamera
                ? WaitingRoomCamera->GetCharacterFacingRotation()
                : FRotator::ZeroRotator;
            Display->SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::ResetPhysics);
        }
    }
}
```

수정 후:
```cpp
// 디스플레이 캐릭터 위치 갱신 (NetMulticast RPC로 모든 클라이언트에 전파)
for (auto& Pair : DisplayCharacterMap)
{
    if (ADRCharacter* Display = Pair.Value.Get())
    {
        int32* SlotIdx = PlayerSlotMap.Find(Pair.Key);
        if (SlotIdx && WaitingRoomSlots.IsValidIndex(*SlotIdx))
        {
            FVector Loc = WaitingRoomSlots[*SlotIdx]->GetActorLocation();
            FRotator Rot = WaitingRoomCamera
                ? WaitingRoomCamera->GetCharacterFacingRotation()
                : FRotator::ZeroRotator;

            // 캡슐 반높이만큼 Z 보정 (TargetPoint가 바닥 레벨인 경우 바닥 뚫림 방지)
            if (UCapsuleComponent* Capsule = Display->GetCapsuleComponent())
            {
                Loc.Z += Capsule->GetScaledCapsuleHalfHeight();
            }

            // NetMulticast RPC로 모든 엔드포인트에서 위치 갱신
            Display->MulticastTeleportToSlot(Loc, Rot);
        }
    }
}
```

### 3.4) 동작 원리

1. `MulticastTeleportToSlot`은 `UFUNCTION(NetMulticast, Reliable)`
2. 서버에서 호출하면 **서버 + 모든 클라이언트**에서 `SetActorLocationAndRotation()` 실행
3. `SetReplicateMovement(false)`와 무관하게 RPC를 통해 직접 위치 설정
4. 결과: 호스트와 모든 클라이언트에서 동일한 위치에 디스플레이 캐릭터 표시

### 3.5) 영향 범위 분석

- `RepositionAllPlayers` 함수 내 디스플레이 캐릭터 위치 갱신 부분만 수정
- `MulticastTeleportToSlot`은 이미 존재하는 검증된 함수
- 추가 네트워크 트래픽: 최대 4개 디스플레이 캐릭터 × 1 Multicast RPC = 미미한 부하

---

## 4) Bug 3: Kick 후 호스트에서 디스플레이 캐릭터 바닥 뚫림

### 4.1) 현상 상세

- Kick 후 호스트 화면에서 **모든** 디스플레이 캐릭터가 바닥을 뚫고 내려가 있음
- 클라이언트 화면에서는 바닥 위에 정상적으로 서 있음 (단, Bug 2로 인해 위치 자체가 갱신되지 않은 상태)
- TargetPoint(WaitingRoomSlots)는 바닥 바로 위에 배치되어 있음

### 4.2) 근본 원인 분석

#### 초기 스폰 vs 재배치의 차이

**초기 스폰** (`SpawnDisplayCharacter`, line 687-719):
```cpp
FActorSpawnParameters Params;
Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
ADRCharacter* Display = GetWorld()->SpawnActor<ADRCharacter>(*BPClassPtr, FTransform(Rotation, Location), Params);
```

- `AdjustIfPossibleButAlwaysSpawn`: UE5 엔진이 **캡슐 콜리전을 고려하여 Z 위치를 자동 보정**
- TargetPoint가 바닥 레벨이어도, 캡슐 반높이만큼 위로 올려져서 캐릭터가 바닥 위에 서게 됨
- 결과: 캐릭터가 바닥 위에 정상적으로 서 있음

**재배치** (`RepositionAllPlayers`, line 570-584):
```cpp
FVector Loc = WaitingRoomSlots[*SlotIdx]->GetActorLocation();  // TargetPoint의 원본 위치 (바닥 레벨)
Display->SetActorLocationAndRotation(Loc, Rot, false, nullptr, ETeleportType::ResetPhysics);
```

- `SetActorLocationAndRotation`은 **콜리전 보정을 하지 않음** (`bSweep = false`)
- TargetPoint의 원본 Z 좌표(바닥 레벨)로 직접 텔레포트
- 캡슐의 중심이 바닥 레벨에 놓이므로, 캡슐의 하반부 + 메시가 바닥 아래로 내려감
- Movement가 `DisableMovement()`로 꺼져있어 중력/바닥 보정 없음
- 결과: 캐릭터가 바닥을 뚫고 절반 묻힌 상태

#### 왜 호스트에서만 보이는가?

- 호스트 = 서버: `SetActorLocationAndRotation`이 즉시 반영 → 바닥 뚫린 상태 표시
- 클라이언트: `SetReplicateMovement(false)`로 위치 변경이 전달되지 않음 → 이전 정상 위치 유지 (Bug 2 덕분에 오히려 바닥 뚫림을 안 봄)

### 4.3) 수정 계획

**이 버그는 Bug 2의 수정에 이미 포함되어 있다.**

Bug 2 수정 코드에서 캡슐 반높이 보정을 추가했다:

```cpp
// 캡슐 반높이만큼 Z 보정 (TargetPoint가 바닥 레벨인 경우 바닥 뚫림 방지)
if (UCapsuleComponent* Capsule = Display->GetCapsuleComponent())
{
    Loc.Z += Capsule->GetScaledCapsuleHalfHeight();
}
```

이 보정으로:
1. TargetPoint 위치(바닥 레벨) + 캡슐 반높이 = 캡슐 중심이 바닥 위
2. `MulticastTeleportToSlot`으로 보정된 위치를 서버/클라이언트 모두에 전파
3. 결과: 호스트에서도 캐릭터가 바닥 위에 정상적으로 서 있음

#### 대안: TargetPoint 자체를 캡슐 반높이만큼 위로 올리기

에디터에서 TargetPoint의 Z를 올리면 코드 수정 없이 해결할 수 있으나:
- 캐릭터 클래스별 캡슐 높이가 다를 수 있음
- 향후 캐릭터 추가 시 다시 문제 발생 가능
- **코드에서 동적으로 보정하는 것이 더 안전함**

### 4.4) 추가 안전장치: `SpawnDisplayCharacter`에서도 Z 보정 적용

현재 `SpawnDisplayCharacter`는 `AdjustIfPossibleButAlwaysSpawn`에 의존하여 Z 보정을 받고 있다. 이 방법은 대부분 잘 동작하지만, 콜리전이 없는 환경에서 실패할 수 있다. 일관성을 위해 여기서도 명시적 보정을 추가하는 것이 좋다.

그러나 현재 초기 스폰은 정상 동작하고 있으므로 (엔진 `AdjustIfPossibleButAlwaysSpawn`이 잘 동작), **이 부분은 변경하지 않는다.** 불필요한 변경은 위험을 초래한다.

---

## 5) 전체 변경 요약

### 수정 파일 목록

| 파일 | 변경 내용 | 변경 규모 |
|------|-----------|-----------|
| `Source/DaeRune/Public/Player/DRPlayerController.h` | `CameraTransitionRetryHandle` 멤버 변수 추가, `ExecuteCameraTransitionToCharacter()` 선언 추가 | 약 4줄 |
| `Source/DaeRune/Private/Player/DRPlayerController.cpp` | `ClientStartCameraTransitionToCharacter_Implementation` 수정 (Pawn 재시도 로직), `ExecuteCameraTransitionToCharacter` 함수 추가 | 약 50줄 |
| `Source/DaeRune/Private/Game/DRLobbyGameMode.cpp` | `RepositionAllPlayers`의 디스플레이 캐릭터 위치 갱신 로직 수정 (MulticastTeleportToSlot + Z 보정) | 약 10줄 |

### 변경하지 않는 파일 (기존 수정 보호)

- `DRLobbyGameMode.h` - 변경 없음
- `DRCharacter.cpp/h` - 변경 없음 (기존 `MulticastTeleportToSlot` 그대로 활용)
- `DRWaitingRoomCameraActor.cpp/h` - 변경 없음
- `DRLobbyGameState.cpp/h` - 변경 없음
- `DRPlayerState.cpp/h` - 변경 없음

### 버그-수정 매핑

| 버그 | 수정 위치 | 핵심 변경 |
|------|-----------|-----------|
| Bug 1 (클라이언트 UIInputMode 고착) | `DRPlayerController.cpp` | Pawn null 시 재시도 타이머 |
| Bug 1 (호스트 시점 고정) | `DRPlayerController.cpp` | `ExecuteCameraTransitionToCharacter` 내 `SetControlRotation` + `bAutoManageActiveCameraTarget` 복원 |
| Bug 2 (Kick 후 위치 미갱신) | `DRLobbyGameMode.cpp` | `SetActorLocationAndRotation` → `MulticastTeleportToSlot` |
| Bug 3 (바닥 뚫림) | `DRLobbyGameMode.cpp` | Z += CapsuleHalfHeight 보정 |

---

## 6) 테스트 시나리오

### Bug 1 검증

| # | 시나리오 | 예상 결과 |
|---|----------|-----------|
| 1-1 | PowerOn 후 클라이언트에서 이동/회전 | WASD 이동 + 마우스 Look 정상 동작 |
| 1-2 | PowerOn 후 호스트에서 이동/회전 | WASD 이동 + 마우스 Look 정상 동작 |
| 1-3 | 3인 접속 후 PowerOn → 모든 플레이어 조작 확인 | 전원 정상 조작 가능 |

### Bug 2 검증

| # | 시나리오 | 예상 결과 |
|---|----------|-----------|
| 2-1 | 호스트가 2p Kick → 3p 화면에서 디스플레이 캐릭터 위치 | 3p가 2p 슬롯(슬롯 1)으로 즉시 이동 |
| 2-2 | 호스트가 2p Kick → 호스트 화면에서 디스플레이 캐릭터 위치 | 기존과 동일하게 정상 |
| 2-3 | 호스트가 3p Kick → 2p 화면에서 디스플레이 캐릭터 위치 | 변경 없음 (2p는 원래 슬롯 1이므로) |

### Bug 3 검증

| # | 시나리오 | 예상 결과 |
|---|----------|-----------|
| 3-1 | 호스트가 2p Kick → 호스트 화면에서 디스플레이 캐릭터 높이 | 바닥 위에 정상적으로 서 있음 |
| 3-2 | 3인 접속, 호스트가 2p Kick → 3p 화면에서 디스플레이 캐릭터 높이 | 바닥 위에 정상적으로 서 있음 |
| 3-3 | 4인 접속, 호스트가 2p Kick → 재배치 후 모든 캐릭터 높이 확인 | 전원 바닥 위에 정상 |

### 회귀 테스트

| # | 시나리오 | 예상 결과 |
|---|----------|-----------|
| R-1 | 대기실에서 캐릭터 클래스 변경 | 카메라 깜빡임 없이 정상 변경 |
| R-2 | 클라이언트 접속 시 대기실 카메라 | WaitingRoomCamera에 고정 |
| R-3 | PowerOn 후 대기실 UI 제거 | 정상 제거 |
| R-4 | 대기실 → FreeRoam → 스테이지 전환 | 정상 전환 |
| R-5 | FreeRoam에서 오버레이 HUD 표시 | 정상 표시 |

---

## 7) 구현 순서

1. **DRPlayerController.h**: `CameraTransitionRetryHandle` 멤버 변수 + `ExecuteCameraTransitionToCharacter()` 선언 추가
2. **DRPlayerController.cpp**: `ClientStartCameraTransitionToCharacter_Implementation` 수정 (Pawn 재시도 로직)
3. **DRPlayerController.cpp**: `ExecuteCameraTransitionToCharacter` 함수 구현
4. **DRLobbyGameMode.cpp**: `RepositionAllPlayers` 디스플레이 캐릭터 위치 갱신 로직 수정
5. **빌드 확인**: 컴파일 에러 없는지 확인
6. **PIE 테스트**: 2~3인 이상에서 Bug 1, 2, 3 시나리오 테스트
7. **회귀 테스트**: 기존 수정사항 정상 동작 확인

---

## 8) 위험 요소 및 대비책

### 위험 1: Pawn 재시도 타이머가 무한 반복될 가능성

- **상황**: 서버가 Pawn을 스폰하지 못하거나 Possess에 실패한 경우
- **대비**: 최대 재시도 횟수 제한 (예: 50회 = 5초). 초과 시 타이머 중지 + 로그 경고
- **구현**: 재시도 카운터 변수 또는 람다 캡처 내 카운터 사용

```cpp
int32 RetryCount = 0;
GetWorldTimerManager().SetTimer(
    CameraTransitionRetryHandle,
    [this, RetryCount]() mutable
    {
        if (!IsValid(this)) return;
        if (++RetryCount > 50)
        {
            UE_LOG(LogTemp, Warning, TEXT("CameraTransition: Pawn not replicated after 5s, aborting"));
            GetWorldTimerManager().ClearTimer(CameraTransitionRetryHandle);
            // 최소한 InputMode는 전환하여 완전 고착 방지
            SetInputMode(FInputModeGameOnly());
            SetShowMouseCursor(false);
            return;
        }
        // ... 나머지 로직
    },
    0.1f, true
);
```

### 위험 2: CapsuleHalfHeight 보정이 일부 캐릭터에서 부정확

- **상황**: 캐릭터 BP별로 캡슐 크기가 다른 경우
- **대비**: `GetScaledCapsuleHalfHeight()`는 캐릭터 인스턴스의 실제 캡슐 크기를 반환하므로, 캐릭터별로 올바른 보정이 적용됨
- **확인**: 각 캐릭터 클래스(Elementalist, Warrior, Ranger)의 캡슐 크기 차이 확인

### 위험 3: `MulticastTeleportToSlot` 호출 시 CMC 관련 부작용

- **상황**: `MulticastTeleportToSlot_Implementation`에서 `CMC->StopMovementImmediately()` 호출 (line 338-341)
- **대비**: 디스플레이 캐릭터는 이미 `DisableMovement()` 상태이므로 `StopMovementImmediately()`는 no-op
- **확인**: 별도 부작용 없음

### 위험 4: `bIsInWaitingRoom = false` 설정 시점과 OnRep_Pawn 경합

- **상황**: `ClientStartCameraTransitionToCharacter` 진입 시 `bIsInWaitingRoom = false`를 즉시 설정하므로, 이후 도착하는 `OnRep_Pawn`이나 `ClientRestart`에서 대기실 보호 로직이 비활성화됨
- **대비**: 이는 **의도된 동작**. FreeRoam 전환 중이므로 대기실 보호가 필요 없음. 재시도 타이머가 Pawn 도착을 기다리는 동안 `OnRep_Pawn`이 도착하면 엔진 기본 동작(ViewTarget 변경)이 실행되지만, 이후 `ExecuteCameraTransitionToCharacter`에서 올바른 ViewTarget으로 재설정됨.

---

## 9) 코드 변경 상세 (복사-붙여넣기용)

### 9.1) DRPlayerController.h 변경

기존 `CachedWaitingRoomCamera` 선언 근처에 추가:
```cpp
FTimerHandle CameraTransitionRetryHandle;
```

기존 `ClientStartCameraTransitionToCharacter` 선언 근처에 추가:
```cpp
void ExecuteCameraTransitionToCharacter();
```

### 9.2) DRPlayerController.cpp - ClientStartCameraTransitionToCharacter_Implementation 전체 교체

기존 line 1249-1297 전체를 다음으로 교체:

```cpp
void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
    bIsInWaitingRoom = false;

    APawn* MyPawn = GetPawn();
    if (!MyPawn)
    {
        // Pawn이 아직 리플리케이트되지 않음 → 0.1초 간격으로 재시도
        int32 MaxRetries = 50; // 5초 제한
        GetWorldTimerManager().SetTimer(
            CameraTransitionRetryHandle,
            [this, MaxRetries, RetryCount = 0]() mutable
            {
                if (!IsValid(this)) return;

                if (++RetryCount > MaxRetries)
                {
                    UE_LOG(LogTemp, Warning, TEXT("CameraTransition: Pawn not replicated after %d retries, forcing input mode"), MaxRetries);
                    GetWorldTimerManager().ClearTimer(CameraTransitionRetryHandle);
                    // 최소한 InputMode 전환으로 완전 고착 방지
                    bAutoManageActiveCameraTarget = true;
                    SetInputMode(FInputModeGameOnly());
                    SetShowMouseCursor(false);
                    InitOverlayForFreeRoam();
                    return;
                }

                APawn* Pawn = GetPawn();
                if (!Pawn) return; // 아직 없으면 다음 반복에서 재시도

                // Pawn 도착 완료 → 타이머 중지 + 카메라 전환 실행
                GetWorldTimerManager().ClearTimer(CameraTransitionRetryHandle);
                ExecuteCameraTransitionToCharacter();
            },
            0.1f,
            true
        );
        return;
    }

    // Pawn이 이미 있으면 즉시 실행 (호스트 또는 빠른 리플리케이션)
    ExecuteCameraTransitionToCharacter();
}
```

### 9.3) DRPlayerController.cpp - ExecuteCameraTransitionToCharacter 신규 함수

`ClientStartCameraTransitionToCharacter_Implementation` 바로 아래에 추가:

```cpp
void ADRPlayerController::ExecuteCameraTransitionToCharacter()
{
    APawn* MyPawn = GetPawn();
    if (!MyPawn) return;

    // 메시 가시성 복원 (3P 숨기기, 1P 보이기 — 일반 FPS 모드)
    if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(MyPawn))
    {
        DRCharacter->SetWaitingRoomVisibility(false);
    }

    // 고정 카메라 → 캐릭터 카메라로 1.5초간 부드럽게 블렌드
    SetViewTargetWithBlend(MyPawn, 1.5f, EViewTargetBlendFunction::VTBlend_EaseInOut);

    // 전환 완료 후 인풋 모드 변경 + HUD 오버레이 초기화
    FTimerHandle InputTimerHandle;
    GetWorldTimerManager().SetTimer(
        InputTimerHandle,
        [this]()
        {
            if (!IsValid(this)) return;

            // (1) 블렌드 완료 후에야 자동 카메라 관리 활성화
            bAutoManageActiveCameraTarget = true;

            // (2) ViewTarget을 Pawn으로 확정 (블렌드 잔여 상태 정리)
            if (APawn* FinalPawn = GetPawn())
            {
                SetViewTarget(FinalPawn);

                // (3) ControlRotation을 Pawn의 현재 회전으로 동기화
                SetControlRotation(FinalPawn->GetActorRotation());
            }

            // (4) 입력 모드 전환
            SetInputMode(FInputModeGameOnly());
            SetShowMouseCursor(false);

            // (5) HUD 오버레이 초기화 (대기실에서 스킵했으므로)
            InitOverlayForFreeRoam();
        },
        1.5f,
        false
    );
}
```

### 9.4) DRLobbyGameMode.cpp - RepositionAllPlayers 디스플레이 캐릭터 위치 갱신 수정

기존 line 569-584 교체:

```cpp
    // 디스플레이 캐릭터 위치 갱신 (NetMulticast RPC로 모든 클라이언트에 전파)
    for (auto& Pair : DisplayCharacterMap)
    {
        if (ADRCharacter* Display = Pair.Value.Get())
        {
            int32* SlotIdx = PlayerSlotMap.Find(Pair.Key);
            if (SlotIdx && WaitingRoomSlots.IsValidIndex(*SlotIdx))
            {
                FVector Loc = WaitingRoomSlots[*SlotIdx]->GetActorLocation();
                FRotator Rot = WaitingRoomCamera
                    ? WaitingRoomCamera->GetCharacterFacingRotation()
                    : FRotator::ZeroRotator;

                // 캡슐 반높이만큼 Z 보정 (TargetPoint가 바닥 레벨인 경우 바닥 뚫림 방지)
                if (UCapsuleComponent* Capsule = Display->GetCapsuleComponent())
                {
                    Loc.Z += Capsule->GetScaledCapsuleHalfHeight();
                }

                // NetMulticast RPC로 모든 엔드포인트에서 위치 갱신
                Display->MulticastTeleportToSlot(Loc, Rot);
            }
        }
    }
```
