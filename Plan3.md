# Plan3: 대기실 HUD 충돌 및 클라이언트 스폰 버그 수정

Research3.md 분석 결과를 기반으로 한 상세 수정 계획.

---

## 수정 대상 파일 목록

| # | 파일 | 수정 내용 |
|---|------|-----------|
| 1 | `DRCharacter.h` | `SetWaitingRoomVisibility(bool)` 함수 선언 추가 |
| 2 | `DRCharacter.cpp` | `SetWaitingRoomVisibility` 구현 + `InitAbilityActorInfo()`에서 대기실 체크 |
| 3 | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot()`에서 `TeleportTo()` 사용 + 메시 가시성 전환 |
| 4 | `DRPlayerController.cpp` | `ClientStartCameraTransitionToCharacter`에서 메시 복원 + FreeRoam 진입 시 InitOverlay 호출 |
| 5 | `DRPlayerController.h` | `InitOverlayForFreeRoam()` 함수 선언 추가 |

---

## Step 1: `ADRCharacter`에 `SetWaitingRoomVisibility` 추가

### 1-1. `DRCharacter.h` — 함수 선언

`UpdateMeshVisibility()` 선언 바로 아래에 추가:

```cpp
// 대기실 메시 가시성 전환 (고정 카메라에서 3P 메시를 보여주기 위해)
void SetWaitingRoomVisibility(bool bInWaitingRoom);
```

### 1-2. `DRCharacter.cpp` — 함수 구현

```cpp
void ADRCharacter::SetWaitingRoomVisibility(bool bInWaitingRoom)
{
    if (bInWaitingRoom)
    {
        // 1P 메시 숨기기 (고정 카메라에서 불필요)
        if (FirstPersonMesh)
        {
            FirstPersonMesh->SetVisibility(false);
        }

        // 3P 메시 보이기 (자기 자신도 3인칭으로 보여야 함)
        GetMesh()->SetVisibility(true);
        GetMesh()->SetOwnerNoSee(false);  // 소유자에게도 보이게

        if (Weapon)
        {
            Weapon->SetVisibility(true);
            Weapon->SetOwnerNoSee(false);
        }
    }
    else
    {
        // 일반 FPS 모드 복원
        UpdateMeshVisibility();
        GetMesh()->SetOwnerNoSee(true);
        if (Weapon)
        {
            Weapon->SetOwnerNoSee(true);
        }
    }
}
```

**근거:**
- 대기실에서는 고정 카메라로 모든 캐릭터를 3인칭으로 봐야 한다
- 현재 로컬 플레이어의 3P 메시는 `SetVisibility(false)` + `SetOwnerNoSee(true)` 이중 차단
- 1P 메시는 `SetOnlyOwnerSee(true)` + `SetVisibility(true)`이므로 고정 카메라에서도 보인다
- `SetWaitingRoomVisibility(true)`: 1P 숨기고, 3P 보이게 (OwnerNoSee 해제)
- `SetWaitingRoomVisibility(false)`: 원래 FPS 상태 복원

---

## Step 2: `InitAbilityActorInfo()`에서 대기실 체크 (HUD 충돌 해결)

### 2-1. `DRCharacter.cpp` — `InitAbilityActorInfo()` 수정

**현재 코드 (line 502-509):**
```cpp
if (ADRPlayerController* DRPlayerController = Cast<ADRPlayerController>(GetController()))
{
    if (ADRHUD* DRHUD = Cast<ADRHUD>(DRPlayerController->GetHUD()))
    {
        DRHUD->InitOverlay(DRPlayerController, DRPlayerState, AbilitySystemComponent, AttributeSets);
    }
}
```

**수정 코드:**
```cpp
if (ADRPlayerController* DRPlayerController = Cast<ADRPlayerController>(GetController()))
{
    // 대기실이면 HUD 오버레이 초기화 스킵
    // (FreeRoam 진입 시 별도로 InitOverlay 호출)
    if (!DRPlayerController->bIsInWaitingRoom)
    {
        if (ADRHUD* DRHUD = Cast<ADRHUD>(DRPlayerController->GetHUD()))
        {
            DRHUD->InitOverlay(DRPlayerController, DRPlayerState, AbilitySystemComponent, AttributeSets);
        }
    }
}
```

**근거:**
- `InitAbilityActorInfo()`는 `PossessedBy()`와 `OnRep_PlayerState()`에서 호출됨
- 대기실 상태에서 Pawn이 Possess될 때 불필요하게 로비 오버레이가 생성되어 대기실 UI와 겹침
- `bIsInWaitingRoom` 플래그로 대기실인지 확인하고 스킵
- FreeRoam 진입 시 Step 4에서 별도로 InitOverlay를 호출

**주의사항 — 타이밍:**
- 서버: `PossessedBy()` → `InitAbilityActorInfo()` 시점에 `bIsInWaitingRoom`은 아직 false일 수 있음 (OnLevelEntered가 아직 안 실행됨)
- 그러나 `GetController()`가 서버에서 호출 시 서버의 PC를 가져오므로, 서버 PC의 `bIsInWaitingRoom`은 `ReceivedPlayer()` → `OnLevelEntered()`에서 이미 true로 설정됨
- 클라이언트: `OnRep_PlayerState()` → `InitAbilityActorInfo()` 시점에 `OnLevelEntered()`가 이미 실행되어 `bIsInWaitingRoom = true`

**타이밍 보완:**
서버의 호스트 플레이어의 경우, `PossessedBy()` 시점에 `bIsInWaitingRoom`이 아직 false일 수 있다. 이 경우 `InitOverlay()`가 호출되어 오버레이가 생성된다.

이를 해결하려면 `PostLogin` 타이머에서 대기실 상태일 때 이미 생성된 오버레이를 제거해야 한다. 더 나은 방법은 `ADRHUD::InitOverlay()`에 **중복 생성 방지** 가드를 추가하는 것이다.

### 2-2. `DRHUD.cpp` — `InitOverlay()` 중복 생성 방지

```cpp
void ADRHUD::InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
    // 이미 오버레이가 존재하면 리턴 (중복 생성 방지)
    if (OverlayWidget)
    {
        return;
    }

    checkf(OverlayWidgetClass, TEXT("Overlay Widget Class uninitialized, please fill out BP_DRHUD"));
    checkf(OverlayWidgetControllerClass, TEXT("Overlay Widget Controller Class uninitialized, please fill out BP_DRHUD"));

    // ... 기존 코드 유지 ...
}
```

**근거:** 클래스 변경(`RespawnPlayerWithClass`) 시 Possess → InitAbilityActorInfo → InitOverlay가 다시 호출될 수 있다. 중복 위젯 생성을 방지한다.

---

## Step 3: `PositionPawnAtSlot()`에서 TeleportTo + 메시 가시성 전환

### 3-1. `DRLobbyGameMode.cpp` — `PositionPawnAtSlot()` 수정

**현재 코드 (line 428-453):**
```cpp
void ADRLobbyGameMode::PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex)
{
    if (!Pawn || WaitingRoomSlots.Num() == 0) return;
    int32 ClampedIndex = FMath::Clamp(SlotIndex, 0, WaitingRoomSlots.Num() - 1);
    AActor* SlotActor = WaitingRoomSlots[ClampedIndex];
    if (!SlotActor) return;

    Pawn->SetActorLocation(SlotActor->GetActorLocation());

    if (WaitingRoomCamera)
    {
        FRotator FacingRotation = WaitingRoomCamera->GetCharacterFacingRotation();
        Pawn->SetActorRotation(FacingRotation);
    }

    if (UCharacterMovementComponent* MovementComp =
        Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
    {
        MovementComp->DisableMovement();
    }
}
```

**수정 코드:**
```cpp
void ADRLobbyGameMode::PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex)
{
    if (!Pawn || WaitingRoomSlots.Num() == 0) return;

    int32 ClampedIndex = FMath::Clamp(SlotIndex, 0, WaitingRoomSlots.Num() - 1);
    AActor* SlotActor = WaitingRoomSlots[ClampedIndex];
    if (!SlotActor) return;

    // 카메라를 향하는 회전 계산
    FRotator FacingRotation = FRotator::ZeroRotator;
    if (WaitingRoomCamera)
    {
        FacingRotation = WaitingRoomCamera->GetCharacterFacingRotation();
    }

    // TeleportTo 사용 (클라이언트에 위치가 리플리케이트됨)
    Pawn->TeleportTo(SlotActor->GetActorLocation(), FacingRotation);

    // 이동 비활성화
    if (UCharacterMovementComponent* MovementComp =
        Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
    {
        MovementComp->DisableMovement();
    }

    // 대기실 메시 가시성 전환 (3P 보이기, 1P 숨기기)
    if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(Pawn))
    {
        DRCharacter->SetWaitingRoomVisibility(true);
    }
}
```

**변경 사항:**
1. `SetActorLocation()` + `SetActorRotation()` → `TeleportTo()` 변경
   - `TeleportTo()`는 내부적으로 `TeleportSucceeded` 이벤트를 트리거하고, `CharacterMovementComponent`가 위치를 올바르게 동기화한다
   - `SetActorLocation()`은 `MOVE_None` 상태에서 클라이언트에 리플리케이트되지 않는 문제가 있었음
2. `SetWaitingRoomVisibility(true)` 호출 추가
   - 서버에서 호출되므로 서버 측 Pawn의 메시 가시성이 변경됨
   - 서버(호스트) 본인의 캐릭터에 대해 3P 메시가 보이게 됨

**주의:** `SetWaitingRoomVisibility`는 서버에서 호출되지만, 메시 가시성(SetVisibility, SetOwnerNoSee)은 **렌더링 속성**이므로 **로컬에서만** 효과가 있다. 따라서:
- 서버(호스트)의 본인 Pawn: 서버에서 직접 호출 → OK
- 클라이언트의 본인 Pawn: 서버에서 호출해도 클라이언트에 반영 안 됨 → **Client RPC에서 별도 호출 필요**

이 문제는 Step 3-2에서 해결한다.

### 3-2. `ClientSetWaitingRoomView`에서 클라이언트 측 메시 가시성 전환

**`DRPlayerController.cpp` — `ClientSetWaitingRoomView_Implementation()` 수정**

**현재 코드 (line 1170-1183):**
```cpp
void ADRPlayerController::ClientSetWaitingRoomView_Implementation(
    ADRWaitingRoomCameraActor* CameraActor)
{
    if (!CameraActor) return;

    bIsInWaitingRoom = true;

    SetViewTargetWithBlend(CameraActor, 0.f);

    SetInputMode(FInputModeUIOnly());
    SetShowMouseCursor(true);
}
```

**수정 코드:**
```cpp
void ADRPlayerController::ClientSetWaitingRoomView_Implementation(
    ADRWaitingRoomCameraActor* CameraActor)
{
    if (!CameraActor) return;

    bIsInWaitingRoom = true;

    // 즉시 고정 카메라로 전환
    SetViewTargetWithBlend(CameraActor, 0.f);

    // UI Only 모드 (마우스 커서 ON)
    SetInputMode(FInputModeUIOnly());
    SetShowMouseCursor(true);

    // 소유 Pawn의 메시 가시성 전환 (1P 숨기기, 3P 보이기)
    if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetPawn()))
    {
        DRCharacter->SetWaitingRoomVisibility(true);
    }
}
```

**근거:**
- `ClientSetWaitingRoomView`는 Client RPC이므로 **클라이언트 로컬에서 실행**된다
- 여기서 `SetWaitingRoomVisibility(true)`를 호출하면 클라이언트의 로컬 Pawn에 대해 메시 가시성이 변경됨
- 서버(호스트)도 이 RPC를 받으므로 (ListenServer에서 서버 PC에 대한 Client RPC는 서버에서 바로 실행) 서버 측 처리도 커버됨

**결과:** `PositionPawnAtSlot`의 `SetWaitingRoomVisibility` 호출은 **제거 가능**하다. 하지만 방어적으로 양쪽 다 두는 것이 안전하다 (서버에서 호출 + Client RPC에서 호출).

→ **결론:** `PositionPawnAtSlot`에서는 `SetWaitingRoomVisibility` 호출을 **제거**하고, **`ClientSetWaitingRoomView`에서만** 호출한다. 이유: 렌더링 속성은 로컬에서만 의미가 있으므로, 각 클라이언트의 Client RPC에서 처리하는 것이 정확하다.

최종 `PositionPawnAtSlot`:
```cpp
void ADRLobbyGameMode::PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex)
{
    if (!Pawn || WaitingRoomSlots.Num() == 0) return;

    int32 ClampedIndex = FMath::Clamp(SlotIndex, 0, WaitingRoomSlots.Num() - 1);
    AActor* SlotActor = WaitingRoomSlots[ClampedIndex];
    if (!SlotActor) return;

    FRotator FacingRotation = FRotator::ZeroRotator;
    if (WaitingRoomCamera)
    {
        FacingRotation = WaitingRoomCamera->GetCharacterFacingRotation();
    }

    // TeleportTo 사용 (클라이언트에 위치 리플리케이트)
    Pawn->TeleportTo(SlotActor->GetActorLocation(), FacingRotation);

    // 이동 비활성화
    if (UCharacterMovementComponent* MovementComp =
        Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
    {
        MovementComp->DisableMovement();
    }
}
```

---

## Step 4: FreeRoam 진입 시 메시 복원 + InitOverlay 호출

### 4-1. `DRPlayerController.cpp` — `ClientStartCameraTransitionToCharacter_Implementation()` 수정

**현재 코드 (line 1185-1208):**
```cpp
void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
    bIsInWaitingRoom = false;

    APawn* MyPawn = GetPawn();
    if (!MyPawn) return;

    SetViewTargetWithBlend(MyPawn, 1.5f, EViewTargetBlendFunction::VTBlend_EaseInOut);

    FTimerHandle InputTimerHandle;
    GetWorldTimerManager().SetTimer(
        InputTimerHandle,
        [this]()
        {
            if (!IsValid(this)) return;
            SetInputMode(FInputModeGameOnly());
            SetShowMouseCursor(false);
        },
        1.5f,
        false
    );
}
```

**수정 코드:**
```cpp
void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
    bIsInWaitingRoom = false;

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
            SetInputMode(FInputModeGameOnly());
            SetShowMouseCursor(false);

            // FreeRoam 진입 시 HUD 오버레이 초기화
            // (대기실에서 스킵했으므로 여기서 호출)
            InitOverlayForFreeRoam();
        },
        1.5f,
        false
    );
}
```

### 4-2. `DRPlayerController.h` — `InitOverlayForFreeRoam()` 선언

```cpp
private:
    // FreeRoam 진입 시 HUD 오버레이 초기화 (대기실에서 스킵된 경우)
    void InitOverlayForFreeRoam();
```

### 4-3. `DRPlayerController.cpp` — `InitOverlayForFreeRoam()` 구현

```cpp
void ADRPlayerController::InitOverlayForFreeRoam()
{
    ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD());
    if (!DRHUD) return;

    // 이미 오버레이가 있으면 스킵 (InitOverlay 내부 중복 방지 가드에 의해)
    ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
    if (!PS) return;

    UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
    UAttributeSet* AS = PS->GetAttributeSet();
    if (!ASC || !AS) return;

    DRHUD->InitOverlay(this, PS, ASC, AS);
}
```

**근거:**
- 대기실에서 `InitAbilityActorInfo()`의 `InitOverlay` 호출을 스킵했으므로
- FreeRoam 진입 시점에 오버레이를 초기화해야 함
- `ADRHUD::InitOverlay()`에 Step 2-2의 중복 방지 가드가 있으므로, 이미 생성된 경우 안전하게 스킵됨

---

## Step 5: RespawnPlayerWithClass에서 메시 가시성 재적용

### 5-1. `DRLobbyGameMode.cpp` — `RespawnPlayerWithClass()` 수정

캐릭터 BP를 교체하면 새 Pawn이 스폰되므로 메시 가시성을 다시 적용해야 한다.

**현재 코드의 대기실 처리 부분 (line 516-530):**
```cpp
ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
{
    int32* SlotIndex = PlayerSlotMap.Find(PC);
    if (SlotIndex)
    {
        PositionPawnAtSlot(NewPawn, *SlotIndex);
    }

    if (WaitingRoomCamera)
    {
        PC->ClientSetWaitingRoomView(WaitingRoomCamera);
    }
}
```

**이미 충분:** `ClientSetWaitingRoomView` RPC 내부에서 `SetWaitingRoomVisibility(true)`를 호출하므로, 새 Pawn에 대해서도 메시 가시성이 적용된다. 추가 수정 불필요.

---

## 수정 순서 요약

| 순서 | 파일 | 함수 | 수정 내용 |
|------|------|------|-----------|
| 1 | `DRCharacter.h` | — | `SetWaitingRoomVisibility(bool)` 선언 추가 |
| 2 | `DRCharacter.cpp` | `SetWaitingRoomVisibility` | 새 함수 구현 (1P/3P 전환) |
| 3 | `DRCharacter.cpp` | `InitAbilityActorInfo` | `bIsInWaitingRoom` 체크 추가하여 대기실에서 InitOverlay 스킵 |
| 4 | `DRHUD.cpp` | `InitOverlay` | 중복 생성 방지 가드 추가 (`if (OverlayWidget) return;`) |
| 5 | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot` | `SetActorLocation` → `TeleportTo` 변경 |
| 6 | `DRPlayerController.cpp` | `ClientSetWaitingRoomView_Implementation` | `SetWaitingRoomVisibility(true)` 호출 추가 |
| 7 | `DRPlayerController.h` | — | `InitOverlayForFreeRoam()` 선언 추가 |
| 8 | `DRPlayerController.cpp` | `InitOverlayForFreeRoam` | 새 함수 구현 (HUD InitOverlay 호출) |
| 9 | `DRPlayerController.cpp` | `ClientStartCameraTransitionToCharacter_Implementation` | 메시 복원 + InitOverlayForFreeRoam 호출 추가 |

---

## 예상 결과

### 대기실 진입 시
1. Pawn 스폰 → `InitAbilityActorInfo()` → `bIsInWaitingRoom == true` → **InitOverlay 스킵** ✅
2. 서버 `PositionPawnAtSlot()` → `TeleportTo()` → **클라이언트에 위치 리플리케이트** ✅
3. `ClientSetWaitingRoomView` RPC → 고정 카메라 설정 + `SetWaitingRoomVisibility(true)` → **1P 숨김, 3P 보임** ✅
4. 대기실 UI만 화면에 표시 (오버레이 없음) ✅

### 캐릭터 클래스 변경 시
1. `RespawnPlayerWithClass` → 새 Pawn 스폰 + Possess
2. `InitAbilityActorInfo()` → `bIsInWaitingRoom == true` → InitOverlay 스킵
3. `PositionPawnAtSlot` → TeleportTo → 슬롯 위치로 이동
4. `ClientSetWaitingRoomView` → 메시 가시성 재적용 ✅

### FreeRoam 전환 시 (PowerOn)
1. `ClientStartCameraTransitionToCharacter` RPC
2. `SetWaitingRoomVisibility(false)` → **FPS 모드 복원** (1P 보임, 3P 숨김) ✅
3. 1.5초 블렌드 후 `InitOverlayForFreeRoam()` → **로비 오버레이 생성** ✅
4. `SetInputMode(GameOnly)` → 게임 입력 활성화 ✅
