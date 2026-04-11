# Plan4: 대기실 버그 3건 수정 계획 (v2)

## 현재 상태 요약

이전 Plan4에서 구현한 수정 사항:
- `MulticastTeleportToSlot` Multicast RPC (버그 1 대응)
- `ClientRestart_Implementation` 오버라이드 (카메라 깜빡임 방지)
- `RespawnPlayerWithClass`에서 `RemoveActiveGameplayEffectBySourceEffect`로 GE 제거 (속도 중첩 방지)

그러나 아래 3가지 버그가 여전히 발생 중:

| # | 버그 | 증상 |
|---|------|------|
| 1 | Kick 후 메시 미이동 | 1P(호스트), 2P, 3P 중 2P를 킥하면 3P의 UI는 2P 자리로 이동하지만 캐릭터 메시는 이동하지 않음 |
| 2 | 캐릭터 변경 버튼 위치 | 캐릭터 변경 버튼이 각 플레이어 자신의 슬롯이 아닌 모두 1P 슬롯에 표시됨 (기능은 정상 작동) |
| 3 | 이동속도 중첩 | 캐릭터를 여러 번 변경하면 변경 횟수만큼 이동속도가 중첩되어 매우 빨라짐 |

---

## 버그 1: Kick 후 3P 캐릭터 메시 미이동

### 현재 코드 흐름

```
KickPlayer() (DRLobbyGameMode.cpp:336)
├── PlayerSlotMap.Remove(TargetPlayer)
├── TargetPlayer->ClientKicked("...") → 킥된 플레이어에게 RPC 전송
├── RepositionAllPlayers()
│   ├── 남은 플레이어 수집 (호스트 우선)
│   ├── PlayerSlotMap 초기화 + NextAvailableSlot = 0
│   └── for each player → AssignPlayerToSlot()
│       └── PositionPawnAtSlot(Pawn, SlotIndex)
│           ├── MulticastTeleportToSlot(Location, Rotation) ← Multicast RPC
│           └── DisableMovement() ← 서버에서 CMC 비활성화
└── for each PC → RefreshWaitingRoomUI() ← 서버에서 직접 호출
```

### 원인 분석

**원인 A: CMC 리플리케이션이 Multicast RPC의 위치 변경을 덮어씀**

대기실에서 CMC는 `MOVE_None` 상태. 하지만 UE5의 CMC 리플리케이션 시스템은 `MOVE_None`에서도 `ReplicatedBasedMovement`나 `ReplicatedMovement`를 통해 위치를 동기화할 수 있음. `MulticastTeleportToSlot`이 모든 엔드포인트에서 위치를 설정하지만, 이후 서버의 CMC가 보내는 unreliable 위치 업데이트가 이전 위치(=원래 슬롯 위치)로 클라이언트를 보정할 수 있음.

특히:
1. `MulticastTeleportToSlot`은 `SetActorLocationAndRotation`으로 Actor 위치를 변경
2. 하지만 CMC 내부의 `LastUpdateLocation`은 여전히 이전 위치
3. CMC가 다음 틱에서 위치를 리플리케이트할 때 이전 위치를 기준으로 보정을 전송
4. 클라이언트의 시뮬레이티드 프록시가 이 보정을 받아 원래 위치로 돌아감

**원인 B: `RefreshWaitingRoomUI()`가 클라이언트에 도달하지 않음**

현재 `KickPlayer()`에서 `RefreshWaitingRoomUI()`를 서버 루프에서 호출하지만, 이 함수는 Client RPC가 아닌 일반 함수. 서버에서 클라이언트 PC에 대해 호출하면, 클라이언트의 `WaitingRoomWidget`은 서버에 존재하지 않으므로 `if (!WaitingRoomWidget) return;`에서 조기 반환. **호스트의 UI만 갱신되고, 클라이언트의 UI는 갱신되지 않음.**

클라이언트 UI가 결국 갱신되는 이유: 킥된 플레이어가 실제로 연결 해제되면 `PlayerArray` 리플리케이션으로 인해 블루프린트 바인딩이 트리거될 수 있음.

### 수정 방안

#### Step 1: 대기실에서 CMC 리플리케이션 비활성화

CMC의 위치 리플리케이션이 Multicast RPC의 위치 설정을 방해하는 것을 근본적으로 방지.

**`DRLobbyGameMode.cpp` — `PositionPawnAtSlot()` 수정:**

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

    FVector SlotLocation = SlotActor->GetActorLocation();

    // ★ 추가: CMC 무브먼트 리플리케이션 비활성화 (대기실에서는 Multicast RPC로 위치 동기화)
    Pawn->SetReplicateMovement(false);

    // Multicast RPC로 모든 엔드포인트에서 직접 위치 설정
    if (ADRCharacter* DRChar = Cast<ADRCharacter>(Pawn))
    {
        DRChar->MulticastTeleportToSlot(SlotLocation, FacingRotation);
    }

    // CMC 비활성화 (서버에서만 — 이동 입력 차단용)
    if (UCharacterMovementComponent* MovementComp =
        Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
    {
        MovementComp->DisableMovement();
    }
}
```

**핵심 변경**: `Pawn->SetReplicateMovement(false)` 추가. 이렇게 하면 CMC의 위치 리플리케이션이 완전히 비활성화되고, `MulticastTeleportToSlot`이 설정한 위치가 CMC에 의해 덮어써지지 않음.

#### Step 2: FreeRoam 진입 시 CMC 리플리케이션 복원

**`DRLobbyGameMode.cpp` — `PowerOn()` 수정:**

```cpp
void ADRLobbyGameMode::PowerOn(ADRPlayerController* Requester)
{
    // ... 기존 코드 ...

    // 3. 모든 플레이어에게 카메라 전환 시작 명령 + 이동 재활성화
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
        {
            if (APawn* Pawn = PC->GetPawn())
            {
                // ★ 추가: CMC 리플리케이션 복원
                Pawn->SetReplicateMovement(true);

                // 기존: 이동 재활성화
                if (UCharacterMovementComponent* MovementComp =
                    Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent()))
                {
                    MovementComp->SetMovementMode(MOVE_Walking);
                }
            }
            PC->ClientStartCameraTransitionToCharacter();
        }
    }
    // ... 기존 코드 ...
}
```

#### Step 3: Client RPC로 `RefreshWaitingRoomUI` 전달

현재 `RefreshWaitingRoomUI`는 서버에서 호출해도 클라이언트에 도달하지 않음. Client RPC 버전 추가.

**`DRPlayerController.h` — Client RPC 선언 추가:**

```cpp
// 서버에서 클라이언트로 대기실 UI 갱신 요청
UFUNCTION(Client, Reliable)
void ClientRefreshWaitingRoomUI();
```

**`DRPlayerController.cpp` — 구현:**

```cpp
void ADRPlayerController::ClientRefreshWaitingRoomUI_Implementation()
{
    RefreshWaitingRoomUI();
}
```

**`DRLobbyGameMode.cpp` — `KickPlayer()`에서 사용:**

```cpp
// 모든 클라이언트의 대기실 UI 갱신
for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
{
    if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
    {
        if (PC->IsLocalController())
        {
            // 호스트: 직접 호출
            PC->RefreshWaitingRoomUI();
        }
        else
        {
            // 클라이언트: Client RPC
            PC->ClientRefreshWaitingRoomUI();
        }
    }
}
```

#### Step 4: `Logout`에서 추가 안전장치

킥된 플레이어가 실제로 연결 해제될 때, 남은 플레이어를 다시 재배치 + UI 갱신.

**`DRLobbyGameMode.cpp` — `Logout()` 수정:**

```cpp
void ADRLobbyGameMode::Logout(AController* Exiting)
{
    // 슬롯 매핑에서 제거 (이미 제거되었을 수 있음 — KickPlayer에서)
    bool bWasInSlotMap = PlayerSlotMap.Remove(Exiting) > 0;

    // 대기실 상태면 재배치 + UI 갱신 (KickPlayer에서 이미 했더라도 안전장치로 재실행)
    ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
    if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
    {
        RepositionAllPlayers();

        // 남은 모든 클라이언트 UI 갱신
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (ADRPlayerController* PC = Cast<ADRPlayerController>(It->Get()))
            {
                if (PC == Exiting) continue; // 나가는 플레이어 스킵
                if (PC->IsLocalController())
                    PC->RefreshWaitingRoomUI();
                else
                    PC->ClientRefreshWaitingRoomUI();
            }
        }
    }

    Super::Logout(Exiting);
    // ... 기존 디버그 메시지 ...
}
```

#### Step 5: `RespawnPlayerWithClass`에서도 CMC 리플리케이션 비활성화 적용

새 폰 스폰 후 `PositionPawnAtSlot`이 호출되면 `SetReplicateMovement(false)`가 자동 적용됨 (Step 1). 하지만 새 폰은 기본적으로 `bReplicateMovement = true`로 스폰되므로, `Possess` → `PossessedBy` 사이에 잠깐 리플리케이션이 활성화됨. 이 구간에서 문제가 발생할 수 있으므로, 스폰 직후 비활성화:

**`DRLobbyGameMode.cpp` — `RespawnPlayerWithClass()` 수정:**

```cpp
// 4. 새 BP 폰 스폰
ADRCharacter* NewPawn = GetWorld()->SpawnActor<ADRCharacter>(*BPClassPtr, SpawnTransform, SpawnParams);
if (!NewPawn) return;

// ★ 추가: 대기실에서는 스폰 직후 CMC 리플리케이션 비활성화
ADRLobbyGameState* LGS = GetGameState<ADRLobbyGameState>();
if (LGS && LGS->GetLobbyState() == ELobbyState::WaitingRoom)
{
    NewPawn->SetReplicateMovement(false);
}
```

### 수정 파일 요약 (버그 1)

| 파일 | 변경 내용 |
|------|----------|
| `DRPlayerController.h` | `ClientRefreshWaitingRoomUI()` Client RPC 선언 |
| `DRPlayerController.cpp` | `ClientRefreshWaitingRoomUI_Implementation()` 구현 |
| `DRLobbyGameMode.cpp` | `PositionPawnAtSlot()`: `SetReplicateMovement(false)` 추가 |
| `DRLobbyGameMode.cpp` | `PowerOn()`: `SetReplicateMovement(true)` 복원 추가 |
| `DRLobbyGameMode.cpp` | `KickPlayer()`: Client RPC로 UI 갱신 변경 |
| `DRLobbyGameMode.cpp` | `Logout()`: 재배치 + UI 갱신 안전장치 추가 |
| `DRLobbyGameMode.cpp` | `RespawnPlayerWithClass()`: 스폰 직후 `SetReplicateMovement(false)` |

---

## 버그 2: 캐릭터 변경 버튼이 모두 1P 슬롯에 표시

### 현재 코드 흐름

```
CreateWaitingRoomUI() (각 클라이언트 로컬에서 실행)
├── WaitingRoomWidget 생성
├── SetIsHost(bIsHost) → BlueprintImplementableEvent
└── RefreshWaitingRoomUI()
    ├── GS->PlayerArray 순회
    ├── 각 플레이어에 대해 FWaitingRoomPlayerInfo 생성:
    │   ├── bIsLocalPlayer = (PS->GetPlayerId() == MyPS->GetPlayerId())
    │   └── SlotIndex = Infos.Num() (순차 인덱스)
    └── WaitingRoomWidget->RefreshPlayerSlots(Infos) → BlueprintImplementableEvent
```

### 원인 분석

**두 가지 가능한 원인:**

**원인 A: 블루프린트 레이아웃 문제 (가능성 높음)**

`WBP_WaitingRoomOverlay` 블루프린트의 `RefreshPlayerSlots` 구현에서 캐릭터 변경 버튼을 `bIsLocalPlayer` 플래그에 따라 해당 슬롯에 배치하지 않고, 항상 첫 번째 슬롯(인덱스 0)에 배치하고 있을 가능성.

블루프린트에서 자주 발생하는 실수:
- 버튼을 특정 슬롯 위젯의 자식으로 추가하지 않고, 별도의 고정 위치에 배치
- `bIsLocalPlayer` 체크 없이 항상 Slot[0]에 버튼을 추가
- For Each 루프에서 슬롯 인덱스를 사용하지 않고 버튼을 재생성

**원인 B: `bIsLocalPlayer`가 클라이언트에서 잘못 설정됨 (타이밍 문제)**

`CreateWaitingRoomUI()` → `RefreshWaitingRoomUI()` 호출 시점에 클라이언트의 `GetPlayerState()` 또는 `PlayerArray`가 아직 완전히 리플리케이트되지 않았을 수 있음.

```cpp
APlayerState* MyPS = GetPlayerState<APlayerState>();
Info.bIsLocalPlayer = MyPS && (PS->GetPlayerId() == MyPS->GetPlayerId());
```

만약 `MyPS`가 nullptr이면 모든 슬롯에서 `bIsLocalPlayer = false`가 됨. 블루프린트가 "로컬 플레이어가 없으면 기본적으로 슬롯 0에 버튼 배치"라는 폴백 로직을 가지고 있다면, 모든 클라이언트에서 버튼이 1P 슬롯에 표시됨.

### 수정 방안

#### Step 1: `RefreshWaitingRoomUI`에 PlayerState 준비 확인 + 재시도 로직

**`DRPlayerController.cpp` — `RefreshWaitingRoomUI()` 수정:**

```cpp
void ADRPlayerController::RefreshWaitingRoomUI()
{
    if (!WaitingRoomWidget) return;

    ADRLobbyGameState* LGS = GetWorld()->GetGameState<ADRLobbyGameState>();
    ADRGameStateBase* GS = GetWorld()->GetGameState<ADRGameStateBase>();
    if (!LGS || !GS) return;

    // ★ 추가: 로컬 PlayerState 준비 확인
    APlayerState* MyPS = GetPlayerState<APlayerState>();
    if (!MyPS)
    {
        // PlayerState가 아직 리플리케이트되지 않음 → 0.5초 후 재시도
        FTimerHandle RetryHandle;
        GetWorldTimerManager().SetTimer(
            RetryHandle,
            [WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
            {
                if (ADRPlayerController* PC = WeakThis.Get())
                {
                    PC->RefreshWaitingRoomUI();
                }
            },
            0.5f, false
        );
        return;
    }

    TArray<FWaitingRoomPlayerInfo> Infos;
    for (APlayerState* PS : GS->PlayerArray)
    {
        if (!PS) continue;
        ADRPlayerState* DRPS = Cast<ADRPlayerState>(PS);
        if (!DRPS) continue;

        FWaitingRoomPlayerInfo Info;
        Info.PlayerName = PS->GetPlayerName();
        Info.SelectedClass = DRPS->GetSelectedPlayerClass();
        Info.bIsHost = GS->IsPlayerHost(PS);
        Info.bIsLocalPlayer = (PS->GetPlayerId() == MyPS->GetPlayerId());
        Info.OwningPlayerState = PS;
        Info.SlotIndex = Infos.Num();
        Infos.Add(Info);
    }

    WaitingRoomWidget->RefreshPlayerSlots(Infos);
}
```

#### Step 2: `OnRep_SelectedPlayerClass`에서 UI 갱신 트리거

캐릭터 변경 시 클라이언트에서 UI를 자동 갱신하도록 `OnRep` 콜백에서 `RefreshWaitingRoomUI` 호출.

현재 `OnRep_SelectedPlayerClass`은 `OnPlayerClassChanged.Broadcast()`만 호출. 이를 확장하여 모든 클라이언트에서 대기실 UI를 갱신.

**방법**: `RespawnPlayerWithClass`에서 캐릭터 변경 완료 후 모든 클라이언트에 UI 갱신 RPC 전송:

**`DRLobbyGameMode.cpp` — `RespawnPlayerWithClass()` 끝에 추가:**

```cpp
// ★ 추가: 모든 클라이언트의 대기실 UI 갱신 (캐릭터 변경 후)
for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
{
    if (ADRPlayerController* OtherPC = Cast<ADRPlayerController>(It->Get()))
    {
        if (OtherPC->IsLocalController())
            OtherPC->RefreshWaitingRoomUI();
        else
            OtherPC->ClientRefreshWaitingRoomUI();
    }
}
```

#### Step 3: 블루프린트 수정 (WBP_WaitingRoomOverlay)

`RefreshPlayerSlots` 블루프린트 구현에서 다음을 확인/수정:

1. **For Each 루프에서 `PlayerInfos` 배열을 순회**
2. **각 `PlayerInfo.SlotIndex`에 해당하는 슬롯 위젯을 가져옴**
3. **`PlayerInfo.bIsLocalPlayer == true`인 슬롯에만 캐릭터 변경 버튼을 표시**
4. **다른 슬롯의 캐릭터 변경 버튼은 숨김**

```
// 블루프린트 의사 코드:
For Each PlayerInfo in PlayerInfos:
    SlotWidget = GetSlotWidget(PlayerInfo.SlotIndex)
    SlotWidget.SetPlayerName(PlayerInfo.PlayerName)
    SlotWidget.SetCharacterClass(PlayerInfo.SelectedClass)

    // ★ 핵심: 버튼을 해당 슬롯 위젯의 자식으로 배치
    if PlayerInfo.bIsLocalPlayer:
        SlotWidget.ChangeClassButton.SetVisibility(Visible)
    else:
        SlotWidget.ChangeClassButton.SetVisibility(Collapsed)
```

**주의사항**: 현재 모든 슬롯에 버튼이 있지만 1P 위치에 렌더링되는 것은, 버튼이 슬롯 위젯의 자식이 아니라 루트 캔버스의 고정 위치에 배치되어 있기 때문일 가능성이 높음. 버튼을 각 슬롯 위젯 내부(Slot Panel)의 자식으로 이동해야 함.

### 수정 파일 요약 (버그 2)

| 파일 | 변경 내용 |
|------|----------|
| `DRPlayerController.cpp` | `RefreshWaitingRoomUI()`: PlayerState null 재시도 로직 |
| `DRLobbyGameMode.cpp` | `RespawnPlayerWithClass()`: 변경 후 Client RPC UI 갱신 |
| `WBP_WaitingRoomOverlay` (블루프린트) | `RefreshPlayerSlots`: bIsLocalPlayer 기반 버튼 배치 수정 |

---

## 버그 3: 캐릭터 여러 번 변경 시 이동속도 중첩

### 현재 코드 흐름

```
RespawnPlayerWithClass() (DRLobbyGameMode.cpp:514)
├── 1. BPClassPtr 조회
├── 2. OldPawn 위치 저장
├── 3. OldPawn 제거: UnPossess() + Destroy()
├── 4. NewPawn 스폰
├── 4.5. 기존 GE 제거: ★ RemoveActiveGameplayEffectBySourceEffect()
├── 5. Possess(NewPawn)
│   └── PossessedBy() → InitAbilityActorInfo()
│       └── InitializeDefaultAttributes() → ★ CreateAndApplyEffectSpec() (GE 재적용)
├── 6. PositionPawnAtSlot
└── 7. ClientSetWaitingRoomView
```

### 원인 분석

**원인: PrimaryAttributes/VitalAttributes GE가 `Instant` 듀레이션일 가능성**

GAS에서 GameplayEffect의 Duration Type에 따라 동작이 크게 달라짐:

| Duration Type | 동작 | `RemoveActiveGameplayEffectBySourceEffect` 동작 |
|--------------|------|----------------------------------------------|
| **Instant** | 기본값(Base Value)을 **영구적으로** 변경 후 즉시 소멸. 활성 GE 목록에 남지 않음 | **제거할 수 없음** (활성 GE가 없으므로) |
| **Infinite** | 모디파이어로 어트리뷰트 값을 변경하며, 활성 GE 목록에 유지됨 | **제거 가능** |

만약 PrimaryAttributes GE가 **Instant** 타입이면:

```
1회차 변경: Base MoveSpeed = 0 → +600 = 600 (정상)
2회차 변경:
  - RemoveActiveGameplayEffectBySourceEffect → 제거할 활성 GE 없음 (Instant는 이미 소멸)
  - Possess → InitializeDefaultAttributes → Base MoveSpeed = 600 → +600 = 1200 (중첩!)
3회차 변경:
  - Base MoveSpeed = 1200 → +600 = 1800 (3배!)
```

만약 PrimaryAttributes GE가 **Infinite** 타입이면:

```
1회차 변경: Base MoveSpeed = 0, Modifier +600 → 현재값 600 (정상)
2회차 변경:
  - RemoveActiveGameplayEffectBySourceEffect → Modifier 제거 → 현재값 0
  - Possess → InitializeDefaultAttributes → 새 Modifier +600 → 현재값 600 (정상)
```

**확인 방법**: UE 에디터에서 PrimaryAttributes GE 블루프린트 에셋을 열어 `Duration Policy` 확인.

### 수정 방안

#### 방안 A: GE가 Instant인 경우 (가능성 높음) — GE를 Infinite로 변경

**블루프린트 수정**: PrimaryAttributes/VitalAttributes GE의 `Duration Policy`를 `Instant` → `Has Duration` 또는 `Infinite`로 변경.

- `Infinite`로 변경하면 `RemoveActiveGameplayEffectBySourceEffect`로 제거 가능
- Modifier Operation은 `Add`로 설정 (기본값에 더하기)
- 기존 코드의 GE 제거 로직이 정상 작동하게 됨

**주의사항**: Infinite로 변경하면 GE가 활성 상태로 ASC에 유지됨. 스테이지 맵에서도 정상 동작하는지 확인 필요.

#### 방안 B: GE 타입 변경 없이 C++ 코드로 해결 — 어트리뷰트 기본값 직접 리셋

GE 타입을 변경할 수 없는 경우, `RespawnPlayerWithClass`에서 GE 제거 대신 **어트리뷰트 기본값을 0으로 리셋** 후 재적용.

**`DRLobbyGameMode.cpp` — `RespawnPlayerWithClass()` Step 4.5 수정:**

```cpp
// 4.5. 이전 어트리뷰트 리셋
ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>();
if (PS)
{
    UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
    if (ASC)
    {
        // 1단계: 모든 클래스의 활성 어트리뷰트 GE 제거 (Infinite GE용)
        for (auto& Pair : PlayerCharacterClassInfo->CharacterClassInformation)
        {
            FCharacterClassDefaultInfo& Info = Pair.Value;
            if (Info.PrimaryAttributes)
                ASC->RemoveActiveGameplayEffectBySourceEffect(Info.PrimaryAttributes, ASC);
            if (Info.VitalAttributes)
                ASC->RemoveActiveGameplayEffectBySourceEffect(Info.VitalAttributes, ASC);
        }

        // 2단계: ★ 추가 — Instant GE로 인한 누적 방지: 기본값 리셋
        if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(PS->GetAttributeSet()))
        {
            // 기본 어트리뷰트 기본값을 0으로 리셋
            ASC->SetNumericAttributeBase(DRAS->GetMoveSpeedAttribute(), 0.f);
            ASC->SetNumericAttributeBase(DRAS->GetMaxHealthAttribute(), 0.f);
            ASC->SetNumericAttributeBase(DRAS->GetMaxWaterAttribute(), 0.f);
            ASC->SetNumericAttributeBase(DRAS->GetHealthAttribute(), 0.f);
            ASC->SetNumericAttributeBase(DRAS->GetWaterAttribute(), 0.f);
        }
    }
}
```

**이렇게 하면:**
- Infinite GE: 1단계에서 제거됨 → 2단계에서 기본값도 0으로 → Possess → 새 GE 적용 → 정상
- Instant GE: 1단계에서 제거 실패 (활성 GE 없음) → 2단계에서 **기본값을 0으로 강제 리셋** → Possess → 새 Instant GE 적용 → 0 + 600 = 600 (정상!)

#### 방안 C: 가장 안전한 접근 — `InitializeDefaultAttributes` 자체를 멱등(idempotent)하게 수정

**`DRAbilitySystemLibrary.cpp` — `InitializePlayerDefaultAttributes()` 수정:**

```cpp
void UDRAbilitySystemLibrary::InitializePlayerDefaultAttributes(
    const UObject* WorldContextObject, EPlayerCharacterClass PlayerClass,
    float Level, UAbilitySystemComponent* ASC)
{
    AActor* AvatarActor = ASC->GetAvatarActor();

    UPlayerCharacterClassInfo* ClassInfo = GetPlayerCharacterClassInfo(WorldContextObject);
    if (!ClassInfo) return;

    // ★ 추가: 먼저 모든 클래스의 기존 어트리뷰트 GE 제거 (Infinite GE용)
    for (auto& Pair : ClassInfo->CharacterClassInformation)
    {
        FCharacterClassDefaultInfo& Info = Pair.Value;
        if (Info.PrimaryAttributes)
            ASC->RemoveActiveGameplayEffectBySourceEffect(Info.PrimaryAttributes, ASC);
        if (Info.VitalAttributes)
            ASC->RemoveActiveGameplayEffectBySourceEffect(Info.VitalAttributes, ASC);
    }

    // ★ 추가: 기본값 리셋 (Instant GE용)
    if (UDRAttributeSet* DRAS = const_cast<UDRAttributeSet*>(
            Cast<UDRAttributeSet>(ASC->GetAttributeSubobject(UDRAttributeSet::StaticClass()))))
    {
        ASC->SetNumericAttributeBase(DRAS->GetMoveSpeedAttribute(), 0.f);
        ASC->SetNumericAttributeBase(DRAS->GetMaxHealthAttribute(), 0.f);
        ASC->SetNumericAttributeBase(DRAS->GetMaxWaterAttribute(), 0.f);
    }

    FCharacterClassDefaultInfo ClassDefaultInfo = ClassInfo->GetClassDefaultInfo(PlayerClass);

    CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.PrimaryAttributes, AvatarActor, Level);
    CreateAndApplyEffectSpec(ASC, ClassDefaultInfo.VitalAttributes, AvatarActor, Level);
}
```

**이 방안의 장점**: `InitializePlayerDefaultAttributes`가 어디서 호출되든 항상 안전함. 중복 호출해도 어트리뷰트가 중첩되지 않음.

### 권장 수정 순서

1. **먼저 GE 타입 확인**: UE 에디터에서 `DA_PlayerCharacterClassInfo` 데이터 에셋 → GardenRobot/VendingMachineRobot의 `PrimaryAttributes` GE를 열어 `Duration Policy` 확인
2. **Instant이면 → 방안 B 적용** (C++ 코드에서 기본값 리셋)
3. **Infinite이면 → 다른 원인 조사** (GE 제거가 실제로 실행되는지 로그 추가)

### 추가 수정: 어빌리티 중복 부여 방지

`PossessedBy` → `GivePlayerStartupAbilities`가 매번 호출되어 어빌리티가 중복 부여될 수 있음. `RespawnPlayerWithClass`에서 Possess 전에 기존 어빌리티 정리:

**`DRLobbyGameMode.cpp` — `RespawnPlayerWithClass()` Step 4.5에 추가:**

```cpp
// 기존 어빌리티 정리 (중복 부여 방지)
if (ASC)
{
    ASC->ClearAllAbilities();
}
```

**주의**: `ClearAllAbilities()`는 모든 어빌리티를 제거. Possess → `GivePlayerStartupAbilities`에서 다시 부여되므로 안전.

### 수정 파일 요약 (버그 3)

| 파일 | 변경 내용 |
|------|----------|
| `DRLobbyGameMode.cpp` | `RespawnPlayerWithClass()`: 어트리뷰트 기본값 리셋 + 어빌리티 정리 |
| 또는 `DRAbilitySystemLibrary.cpp` | `InitializePlayerDefaultAttributes()`: 멱등성 보장 (방안 C) |
| 또는 GE 블루프린트 에셋 | PrimaryAttributes/VitalAttributes를 Infinite로 변경 (방안 A) |

---

## 전체 구현 순서

### Phase 1: 버그 3 (이동속도 중첩) — 가장 우선

1. UE 에디터에서 PrimaryAttributes GE 듀레이션 타입 확인
2. `RespawnPlayerWithClass`에서 어트리뷰트 기본값 리셋 코드 추가 (방안 B)
3. `ClearAllAbilities()` 추가
4. 테스트: 클래스 3회 이상 변경 → FreeRoam → 이동속도 확인

### Phase 2: 버그 1 (Kick 후 메시 미이동)

1. `PositionPawnAtSlot`에 `SetReplicateMovement(false)` 추가
2. `PowerOn`에 `SetReplicateMovement(true)` 복원 추가
3. `RespawnPlayerWithClass`에서 스폰 직후 `SetReplicateMovement(false)` 추가
4. `ClientRefreshWaitingRoomUI` Client RPC 추가
5. `KickPlayer`에서 Client RPC 사용
6. `Logout`에 재배치 안전장치 추가
7. 테스트: PIE 3인 → 2P 킥 → 호스트/3P 모두에서 메시 이동 확인

### Phase 3: 버그 2 (캐릭터 변경 버튼 위치)

1. `RefreshWaitingRoomUI`에 PlayerState null 재시도 로직 추가
2. `RespawnPlayerWithClass` 끝에 Client RPC UI 갱신 추가
3. `WBP_WaitingRoomOverlay` 블루프린트에서 `bIsLocalPlayer` 기반 버튼 배치 수정
4. 테스트: PIE 3인 → 각 클라이언트에서 자신의 슬롯에만 변경 버튼 표시 확인

---

## 테스트 체크리스트

### 버그 1 검증
- [ ] PIE 3인: 2P Kick → 호스트 화면에서 3P 메시가 2P 슬롯으로 이동
- [ ] PIE 3인: 2P Kick → 3P 클라이언트 화면에서 자신의 메시가 2P 슬롯으로 이동
- [ ] PIE 3인: 2P Kick → 호스트/3P 모두에서 UI 슬롯 순서 정상 표시
- [ ] PIE 2인: 클래스 변경 후 메시가 원래 슬롯에 유지
- [ ] PowerOn 후 모든 플레이어 이동 정상 작동 (SetReplicateMovement 복원 확인)

### 버그 2 검증
- [ ] 호스트: 자신(1P) 슬롯에만 캐릭터 변경 버튼 표시
- [ ] 2P 클라이언트: 자신(2P) 슬롯에만 버튼 표시
- [ ] 3P 클라이언트: 자신(3P) 슬롯에만 버튼 표시
- [ ] 버튼 클릭 → 캐릭터 정상 변경 (기존 기능 유지)
- [ ] 킥 후 슬롯 재배치 → 버튼도 새 슬롯으로 이동

### 버그 3 검증
- [ ] 클래스 3번 이상 변경 후 FreeRoam에서 이동속도 정상
- [ ] 클래스 A → B → A 변경 후 A의 속도가 기본값과 동일
- [ ] Health, Water 등 다른 어트리뷰트도 중첩 없이 정상
- [ ] 스테이지 맵 이동 후 어트리뷰트 정상 작동

---

## 전체 수정 파일 요약

| 파일 | 변경 내용 |
|------|----------|
| `DRPlayerController.h` | `ClientRefreshWaitingRoomUI()` Client RPC 선언 |
| `DRPlayerController.cpp` | `ClientRefreshWaitingRoomUI_Implementation()` 구현, `RefreshWaitingRoomUI()` 재시도 로직 |
| `DRLobbyGameMode.cpp` | `PositionPawnAtSlot()`: `SetReplicateMovement(false)`, `PowerOn()`: 복원, `KickPlayer()`: Client RPC, `Logout()`: 안전장치, `RespawnPlayerWithClass()`: 어트리뷰트 리셋 + 어빌리티 정리 + SetReplicateMovement + Client RPC UI 갱신 |
| `WBP_WaitingRoomOverlay` (블루프린트) | `RefreshPlayerSlots`: bIsLocalPlayer 기반 버튼 배치 |
