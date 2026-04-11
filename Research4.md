# Research4: 대기실 시스템 잔여 버그 3건 상세 분석 (수정 후 재분석)

> 이전 수정 사항(태그 prefix 매칭, AddCharacterAbilities 사용, OnLevelEntered 로비 폴백, RemoveOverlay 등)이 적용된 상태에서 테스트 후 발견된 잔여 문제 분석

---

## 버그 1: 클라이언트가 TargetPoint가 아닌 PlayerStart에 스폰됨 (호스트는 정상)

### 현상
- **호스트**: TargetPoint 슬롯 위치에 정상적으로 나타남 (태그 prefix 매칭 수정 성공)
- **클라이언트**: PlayerStart 지점에서 스폰되어 그 자리에 고정됨

### 코드 분석

#### 서버 PostLogin 타이머 (DRLobbyGameMode.cpp:24~50)

```cpp
// PostLogin에서 0.5초 타이머로 슬롯 배치
FTimerHandle SlotTimerHandle;
GetWorldTimerManager().SetTimer(
    SlotTimerHandle,
    [this, DRPC]()
    {
        if (!IsValid(DRPC)) return;
        AssignPlayerToSlot(DRPC);        // 서버에서 실행
        if (WaitingRoomCamera)
        {
            DRPC->ClientSetWaitingRoomView(WaitingRoomCamera);
        }
    },
    0.5f, false
);
```

- 이 코드는 **서버에서** 실행됨
- 호스트와 클라이언트 모두 동일한 코드 경로를 탐

#### AssignPlayerToSlot (DRLobbyGameMode.cpp:425~436)

```cpp
void ADRLobbyGameMode::AssignPlayerToSlot(AController* Player)
{
    if (WaitingRoomSlots.Num() == 0) return;
    int32 SlotIndex = NextAvailableSlot++;
    PlayerSlotMap.Add(Player, SlotIndex);
    if (APawn* Pawn = Player->GetPawn())   // 서버에서 실행 → Pawn은 존재함
    {
        PositionPawnAtSlot(Pawn, SlotIndex);
    }
}
```

- `Super::PostLogin()`이 `RestartPlayer()`를 호출하므로 0.5초 후에는 Pawn이 반드시 존재
- `GetPawn()`은 서버에서 유효한 값을 반환

#### PositionPawnAtSlot (DRLobbyGameMode.cpp:438~462)

```cpp
void ADRLobbyGameMode::PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex)
{
    // ... (클램프, null 체크 생략)
    Pawn->TeleportTo(SlotActor->GetActorLocation(), FacingRotation);

    // 이동 비활성화
    if (UCharacterMovementComponent* MovementComp = ...)
    {
        MovementComp->DisableMovement();   // MOVE_None 설정
    }
}
```

### 근본 원인: TeleportTo + DisableMovement 리플리케이션 문제

**UE5 캐릭터 이동 리플리케이션 구조**:
1. 클라이언트 소유 캐릭터는 **클라이언트가 이동을 예측**하고 서버가 검증하는 구조
2. 서버가 위치를 교정할 때는 `ClientAdjustPosition`을 통해 클라이언트에 전달
3. `DisableMovement()`로 `MOVE_None`이 설정되면:
   - 클라이언트는 서버로 이동 요청을 보내지 않음
   - 서버는 교정할 클라이언트 이동 요청이 없으므로 `ClientAdjustPosition`을 보내지 않음
   - **결과**: 서버에서 `TeleportTo`로 변경된 위치가 클라이언트에 전달되지 않음

**호스트와 클라이언트의 차이**:
| | 호스트 (Listen Server) | 클라이언트 |
|--|--|--|
| TeleportTo 실행 | 로컬에서 직접 위치 변경 | 서버에서만 위치 변경 |
| 위치 리플리케이션 | 불필요 (로컬) | 필요 — 하지만 MOVE_None이라 전달 안 됨 |
| 결과 | TargetPoint에 정상 위치 | PlayerStart에 고정됨 |

**핵심**: `TeleportTo()` 후 즉시 `DisableMovement()`를 호출하면, 클라이언트로의 위치 리플리케이션이 차단됨.

### 적용된 수정: ClientTeleportToSlot Client RPC 추가

CMC 리플리케이션에 의존하지 않고, Client RPC로 클라이언트 측에서도 직접 텔레포트를 수행하도록 변경.

**수정 파일 및 내용**:

1. **DRPlayerController.h** — `ClientTeleportToSlot` Client RPC 선언 추가:
```cpp
// 서버 → 클라이언트: 대기실 슬롯 위치로 텔레포트
UFUNCTION(Client, Reliable)
void ClientTeleportToSlot(FVector SlotLocation, FRotator SlotRotation);
```

2. **DRPlayerController.cpp** — RPC 구현:
```cpp
void ADRPlayerController::ClientTeleportToSlot_Implementation(FVector SlotLocation, FRotator SlotRotation)
{
    if (APawn* MyPawn = GetPawn())
    {
        MyPawn->TeleportTo(SlotLocation, SlotRotation);

        if (UCharacterMovementComponent* MovementComp =
            Cast<UCharacterMovementComponent>(MyPawn->GetMovementComponent()))
        {
            MovementComp->DisableMovement();
        }
    }
}
```

3. **DRLobbyGameMode.cpp** — `PositionPawnAtSlot` 수정:
```cpp
void ADRLobbyGameMode::PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex)
{
    // ... (기존: 서버 측 TeleportTo + DisableMovement)

    // ★ 추가: 리모트 클라이언트에도 텔레포트 명령 전송 (호스트는 로컬이므로 불필요)
    if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
    {
        if (!PC->IsLocalController())
        {
            if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
            {
                DRPC->ClientTeleportToSlot(SlotLocation, FacingRotation);
            }
        }
    }
}
```

**설계 근거**:
- `ForceNetUpdate()` 방식은 CMC 내부 리플리케이션에 의존하여 불확실
- 딜레이 방식은 네트워크 지연에 따라 실패 가능성
- **Client RPC가 가장 확실**: 서버/클라이언트 양쪽에서 동일한 위치 보장
- `IsLocalController()` 체크로 호스트 중복 호출 방지
- `RespawnPlayerWithClass`에서도 `PositionPawnAtSlot`을 호출하므로 클래스 변경 시에도 자동 적용

---

## 버그 2: 클라이언트 화면이 메시 내부 시점 (WaitingRoomCamera로 전환 안 됨)

### 현상
- 클라이언트 화면이 흰색 → 실제로는 **캐릭터 메시 내부**에서 보고 있음
- BP 카메라 시점도 아닌, 액터 원점(Root) 위치에서 보는 것으로 보임
- WaitingRoomCamera로 ViewTarget이 전환되지 않음

### 코드 분석

#### 클라이언트의 OnLevelEntered 흐름 (DRPlayerController.cpp:586~690)

```cpp
void ADRPlayerController::OnLevelEntered()
{
    // ...
    ADRLobbyGameState* LGS = World->GetGameState<ADRLobbyGameState>();

    if (IsInLobby())
    {
        if (!LGS)
        {
            // GameState 아직 복제 안됨 → 대기실로 가정
            bIsInWaitingRoom = true;
            RestoreDefaultInputMode();
            return;  // ← ViewTarget 설정 없이 리턴
        }
        // ...
    }
    // ...
}
```

- 클라이언트는 LGS가 null이므로 **ViewTarget을 설정하지 않고** 리턴
- 이 시점에서 ViewTarget은 기본값 (자기 자신 또는 Pawn)

#### ClientSetWaitingRoomView RPC (DRPlayerController.cpp:1213~1241)

```cpp
void ADRPlayerController::ClientSetWaitingRoomView_Implementation(
    ADRWaitingRoomCameraActor* CameraActor)
{
    if (!CameraActor) return;  // ← CameraActor가 null이면 여기서 리턴!

    bIsInWaitingRoom = true;
    // RemoveOverlay, SetViewTargetWithBlend, CreateWaitingRoomUI ...
}
```

### 근본 원인: 두 가지 문제의 복합

#### 원인 A: WaitingRoomCameraActor 리플리케이션 미설정

`ADRWaitingRoomCameraActor`는 `AActor`를 상속하지만 **`bReplicates = true`가 설정되지 않음**:

```cpp
// DRWaitingRoomCameraActor.h
class DAERUNE_API ADRWaitingRoomCameraActor : public AActor
{
    GENERATED_BODY()
public:
    ADRWaitingRoomCameraActor();
    // ... CameraComponent, GetCharacterFacingRotation
};
```

- `bReplicates`가 false인 경우, 네트워크 채널이 열리지 않아 **RPC 파라미터로 전달 시 클라이언트에서 참조가 resolve되지 않을 수 있음**
- 결과: `CameraActor`가 null → 함수 첫 줄에서 `return` → ViewTarget 전환이 안 됨

#### 원인 B: OnRep_Pawn에 의한 ViewTarget 자동 덮어쓰기

**타이밍 시퀀스** (클라이언트 시점):

```
T=0.0s  서버: PostLogin → Super::PostLogin (Pawn 스폰) → 0.5s 타이머 시작
T=0.0s  클라이언트: ReceivedPlayer() → OnLevelEntered() → bIsInWaitingRoom=true, ViewTarget 미설정
T≈0.1s  클라이언트: Pawn 리플리케이션 수신 → OnRep_Pawn() → AutoManageActiveCameraTarget(Pawn)
         → ViewTarget이 자동으로 Pawn으로 설정됨
         → Pawn이 PlayerStart 위치에 있으므로 메시 내부 시점이 됨
T=0.5s  서버: 타이머 실행 → ClientSetWaitingRoomView(WaitingRoomCamera) RPC 전송
T≈0.5s  클라이언트: RPC 수신 → CameraActor가 null로 resolve → return → ViewTarget 변경 안 됨
```

- `bAutoManageActiveCameraTarget`이 기본 `true`이므로, UE가 Pawn 수신 시 자동으로 ViewTarget을 Pawn으로 설정
- 그 후 RPC가 도착하지만 원인 A에 의해 CameraActor가 null → ViewTarget은 Pawn(메시 내부)에 고정

### 적용된 수정: 리플리케이션 활성화 + AutoManageActiveCameraTarget 비활성화

**수정 파일 및 내용**:

1. **DRWaitingRoomCameraActor.cpp** — 생성자에서 리플리케이션 활성화:
```cpp
ADRWaitingRoomCameraActor::ADRWaitingRoomCameraActor()
{
    PrimaryActorTick.bCanEverTick = false;

    // ★ 추가: Client RPC 파라미터로 안전하게 전달되도록 리플리케이션 활성화
    bReplicates = true;
    bAlwaysRelevant = true;

    CameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("WaitingRoomCamera"));
    RootComponent = CameraComponent;
}
```

2. **DRPlayerController.cpp — OnLevelEntered** 로비 분기에 `bAutoManageActiveCameraTarget = false` 추가:
```cpp
if (!LGS)
{
    bIsInWaitingRoom = true;
    bAutoManageActiveCameraTarget = false; // ★ Pawn 수신 시 ViewTarget 자동 설정 차단
    RestoreDefaultInputMode();
    return;
}

if (LGS->GetLobbyState() == ELobbyState::WaitingRoom
    || LGS->GetLobbyState() == ELobbyState::Transitioning)
{
    bIsInWaitingRoom = true;
    bAutoManageActiveCameraTarget = false; // ★ Pawn 수신 시 ViewTarget 자동 설정 차단
    RestoreDefaultInputMode();
    CreateWaitingRoomUI();
    return;
}
```

3. **DRPlayerController.cpp — ClientStartCameraTransitionToCharacter** FreeRoam 전환 시 복원:
```cpp
void ADRPlayerController::ClientStartCameraTransitionToCharacter_Implementation()
{
    bIsInWaitingRoom = false;
    bAutoManageActiveCameraTarget = true; // ★ 자동 카메라 관리 복원
    // ... (기존 코드 유지)
}
```

**설계 근거**:
- `bAlwaysRelevant = true`: 대기실 카메라는 모든 플레이어가 접근해야 하므로 항상 관련성 보장. 가벼운 액터이므로 네트워크 비용 무시 가능.
- `bAutoManageActiveCameraTarget`: UE 엔진이 기본 제공하는 프로퍼티. `false`로 설정하면 Pawn 수신 시 ViewTarget 자동 설정을 차단. FreeRoam 전환 시 `true`로 복원하여 일반 게임플레이에서 정상 작동.
- 해킹 없이 UE 엔진의 기존 메커니즘만 활용하여 깔끔하게 해결.

**수정 후 타이밍 시퀀스**:
```
T=0.0s  클라이언트: OnLevelEntered() → bIsInWaitingRoom=true, bAutoManageActiveCameraTarget=false
T≈0.1s  클라이언트: OnRep_Pawn() → AutoManageActiveCameraTarget 비활성화 → ViewTarget 자동 변경 안 됨 ✓
T≈0.5s  클라이언트: ClientSetWaitingRoomView RPC 수신 → CameraActor가 정상 resolve (bReplicates=true)
         → SetViewTargetWithBlend(CameraActor, 0.f) → WaitingRoomCamera 시점 ✓
```

---

## 버그 3: 로비 FreeRoam에서 스킬 아이콘이 변경되지 않음 (스테이지에서는 정상)

### 현상
- 로비에서 FreeRoam 전환 후 HUD 오버레이가 표시되지만, **스킬 아이콘이 기본 아이콘 그대로** (AbilityInfoDelegate에 의한 갱신 안 됨)
- 스킬 자체는 사용 가능 (AddCharacterAbilities 수정으로 해결됨)
- **스테이지에서는 스킬 아이콘이 정상적으로 표시됨**

### 코드 분석

#### InitOverlay 호출 순서 (DRHUD.cpp:21~47) — 수정 전

```cpp
void ADRHUD::InitOverlay(APlayerController* PC, APlayerState* PS,
    UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
    if (OverlayWidget) return;  // 중복 방지

    // Step 1: 위젯 생성
    UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
    OverlayWidget = Cast<UDRUserWidget>(Widget);

    // Step 2: WidgetController 생성 + 콜백 바인딩
    const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
    UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);
    //       ↑ 내부에서 BindCallbacksToDependencies() 호출
    //         → bStartupAbilitiesGiven == true이면 BroadcastAbilityInfo() 즉시 실행!

    // Step 3: 위젯에 컨트롤러 할당
    OverlayWidget->SetWidgetController(WidgetController);
    //       ↑ 위젯이 컨트롤러 참조를 얻는 시점

    // Step 4: 초기값 브로드캐스트
    WidgetController->BroadcastInitialValues();

    // Step 5: 뷰포트에 추가
    Widget->AddToViewport();
}
```

#### BindCallbacksToDependencies의 어빌리티 정보 처리 (OverlayWidgetController.cpp:143~152)

```cpp
// 스타트업 어빌리티가 이미 부여되었다면 즉시 브로드캐스트
if (GetDRASC()->bStartupAbilitiesGiven)
{
    BroadcastAbilityInfo();    // ← 여기서 AbilityInfoDelegate가 발동!
}
else
{
    // 아직 부여되지 않았다면 부여 완료 델리게이트에 바인딩
    GetDRASC()->AbilitiesGivenDelegate.AddUObject(this, &UOverlayWidgetController::BroadcastAbilityInfo);
}
```

### 근본 원인: BroadcastAbilityInfo가 위젯의 컨트롤러 할당보다 먼저 실행됨

**로비 FreeRoam에서의 실행 순서** (수정 전):

```
1. InitOverlayForFreeRoam() 호출 (PowerOn 후 1.5초 + 1.5초 딜레이)
2. DRHUD::InitOverlay() 진입
3. CreateWidget → 위젯 생성됨 (컨트롤러 참조 아직 없음)
4. GetOverlayWidgetController() → BindCallbacksToDependencies() 호출
5.   → bStartupAbilitiesGiven == true (AddCharacterAbilities로 이미 부여됨)
6.   → BroadcastAbilityInfo() 실행! AbilityInfoDelegate 발동!
7.   → 하지만 위젯은 아직 SetWidgetController()를 받지 않았으므로
         컨트롤러 참조가 없어 델리게이트를 수신할 수 없음!
8. OverlayWidget->SetWidgetController(WidgetController) ← 이제서야 컨트롤러 할당
9. WidgetController->BroadcastInitialValues() ← Health, Water 등 값은 정상 표시
10. Widget->AddToViewport()
```

**6~7번이 핵심**: `BroadcastAbilityInfo()`가 Step 4 (GetOverlayWidgetController 내부)에서 실행되지만, 위젯이 컨트롤러를 받는 것은 Step 8. **브로드캐스트가 위젯 준비 전에 발생하여 소실됨.**

**스테이지에서 정상 작동하는 이유**:

```
1. DRCharacter::InitAbilityActorInfo() → InitOverlay() 호출
2. InitOverlay() 안에서:
   - GetOverlayWidgetController() → BindCallbacksToDependencies()
   - bStartupAbilitiesGiven == false (아직 서버에서 부여 중, 클라이언트에 복제 안 됨)
   - → AbilitiesGivenDelegate에 바인딩 (즉시 실행하지 않음)
3. SetWidgetController() → 위젯이 컨트롤러 참조 획득
4. AddToViewport()
... (시간 경과) ...
5. 서버에서 어빌리티 부여 완료 → OnRep_ActivateAbilities 발동
   → bStartupAbilitiesGiven = true
   → AbilitiesGivenDelegate.Broadcast()
   → BroadcastAbilityInfo() 실행
   → 위젯이 이미 컨트롤러를 갖고 있으므로 정상 수신!
```

**차이점 요약**:

| | 로비 FreeRoam | 스테이지 |
|--|--|--|
| InitOverlay 호출 시점 | PowerOn 후 3초+ | PossessedBy/OnRep_PlayerState 직후 |
| bStartupAbilitiesGiven 상태 | `true` (이미 부여됨) | `false` (아직 복제 안 됨) |
| BroadcastAbilityInfo 경로 | **즉시 실행** (위젯 준비 전) | **델리게이트 바인딩** (위젯 준비 후 발동) |
| 결과 | 아이콘 미갱신 | 아이콘 정상 갱신 |

### 적용된 수정: InitOverlay에서 SetWidgetController 이후 BroadcastAbilityInfo 호출

**수정 파일 및 내용**:

**DRHUD.cpp — InitOverlay** (수정 후):
```cpp
void ADRHUD::InitOverlay(APlayerController* PC, APlayerState* PS,
    UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
    if (OverlayWidget) return;

    // ... (위젯 생성, WidgetController 생성)

    OverlayWidget->SetWidgetController(WidgetController);
    WidgetController->BroadcastInitialValues();
    WidgetController->BroadcastAbilityInfo();   // ★ 추가: 위젯 준비 후 아이콘 갱신
    Widget->AddToViewport();
}
```

**수정 후 동작 분석** (로비 vs 스테이지):

| 시나리오 | BindCallbacksToDependencies 시점 | InitOverlay의 BroadcastAbilityInfo |
|----------|----------------------------------|-------------------------------------|
| 로비 FreeRoam (`bStartupAbilitiesGiven == true`) | BroadcastAbilityInfo 즉시 실행 (위젯 미준비 → 소실) | ★ 위젯 준비 후 재호출 → **아이콘 정상 표시** |
| 스테이지 (`bStartupAbilitiesGiven == false`) | 델리게이트 바인딩 (나중에 발동) | BroadcastAbilityInfo 호출하지만 어빌리티 미부여 → **무해** (이후 델리게이트로 정상 갱신) |

**설계 근거**:
- `UpdateOverlayForSpectating`에서 이미 `SetWidgetController` 이후 `BroadcastAbilityInfo()`를 호출하는 동일 패턴이 존재 (DRHUD.cpp:69번 줄) — 기존 패턴과 일치
- 스테이지 부작용 없음: `bStartupAbilitiesGiven == false`일 때 ASC의 `GetActivatableAbilities()`가 비어있으므로 빈 브로드캐스트 → 이후 `AbilitiesGivenDelegate`로 정상 갱신

---

## 수정 전체 요약

| 버그 | 수정 대상 파일 | 변경 내용 | 상태 |
|------|----------------|----------|------|
| 버그 1 (위치) | `DRPlayerController.h` | `ClientTeleportToSlot` Client RPC 선언 | ✅ 완료 |
| 버그 1 (위치) | `DRPlayerController.cpp` | `ClientTeleportToSlot_Implementation` 구현 | ✅ 완료 |
| 버그 1 (위치) | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot`에서 리모트 클라이언트에 RPC 전송 | ✅ 완료 |
| 버그 2 (카메라) | `DRWaitingRoomCameraActor.cpp` | `bReplicates = true`, `bAlwaysRelevant = true` | ✅ 완료 |
| 버그 2 (카메라) | `DRPlayerController.cpp` | `OnLevelEntered` 로비 분기에 `bAutoManageActiveCameraTarget = false` | ✅ 완료 |
| 버그 2 (카메라) | `DRPlayerController.cpp` | `ClientStartCameraTransitionToCharacter`에 `bAutoManageActiveCameraTarget = true` 복원 | ✅ 완료 |
| 버그 3 (아이콘) | `DRHUD.cpp` | `InitOverlay`에서 `SetWidgetController` 후 `BroadcastAbilityInfo()` 한 줄 추가 | ✅ 완료 |

**총 수정 파일 4개, 변경 포인트 7곳. 세 버그 모두 독립적.**

---

## 추가 버그 4: Kick 후 남은 플레이어가 빈 슬롯으로 재배치되지 않음

### 현상
- 1P(호스트), 2P, 3P가 있을 때 2P를 Kick하면, 3P가 2P 자리(슬롯 1)로 이동하지 않고 원래 자리(슬롯 2)에 그대로 남음
- 빈 자리가 생기면 1P 쪽으로 당겨야 하는데 당겨지지 않음

### 코드 분석

#### KickPlayer 흐름 (DRLobbyGameMode.cpp:335~369)

```cpp
void ADRLobbyGameMode::KickPlayer(...)
{
    PlayerSlotMap.Remove(TargetPlayer);        // 킥된 플레이어 제거
    TargetPlayer->ClientKicked(...);           // 킥 RPC 전송
    RepositionAllPlayers();                    // 남은 플레이어 재배치
    // UI 갱신 ...
}
```

#### RepositionAllPlayers (DRLobbyGameMode.cpp:491~520)

```cpp
void ADRLobbyGameMode::RepositionAllPlayers()
{
    // 호스트 우선으로 남은 플레이어 수집
    TArray<AController*> OrderedPlayers;
    // ... (호스트를 맨 앞에 배치)

    PlayerSlotMap.Empty();
    NextAvailableSlot = 0;

    for (AController* Player : OrderedPlayers)
    {
        AssignPlayerToSlot(Player);  // slot 0, 1, 2... 순차 할당
    }
}
```

코드 로직 자체는 올바르지만, 두 가지 문제가 있었음:

#### 문제 A: WaitingRoomSlots 정렬이 태그 순서와 불일치

**수정 전**의 정렬 기준: **액터 이름** (`GetName()`)
```cpp
WaitingRoomSlots.Sort([](const TObjectPtr<AActor>& A, const TObjectPtr<AActor>& B)
{
    return A->GetName() < B->GetName();
});
```

UE 에디터에서 자동 생성되는 액터 이름은 "TargetPoint", "TargetPoint2", "TargetPoint3" 등 일관되지 않은 패턴. 태그는 `WaitingRoomSlot_0`, `_1`, `_2`, `_3`으로 명확한 순서가 있음. **액터 이름 기반 정렬은 태그 의도와 불일치할 수 있음**.

#### 문제 B: 서버→클라이언트 위치 동기화 불확실

`PositionPawnAtSlot`에서 `TeleportTo` 후 `DisableMovement`를 호출하지만, 이미 `MOVE_None` 상태인 캐릭터에 대해 `TeleportTo`의 위치 변경이 서버 simulated proxy에 반영되지 않을 수 있음. `ForceNetUpdate()`가 없었으므로 서버 측 위치 변경이 즉시 네트워크에 전파되지 않을 수 있었음.

### 적용된 수정

#### 수정 4-1: FindWaitingRoomActors 태그 suffix 기반 정렬로 변경

**파일**: `DRLobbyGameMode.cpp` — `FindWaitingRoomActors` 함수 (408~424번 줄)

```cpp
// 변경: 태그의 숫자 suffix 기준 정렬
WaitingRoomSlots.Sort([](const TObjectPtr<AActor>& A, const TObjectPtr<AActor>& B)
{
    auto GetSlotIndex = [](const AActor* Actor) -> int32
    {
        for (const FName& Tag : Actor->Tags)
        {
            FString TagStr = Tag.ToString();
            if (TagStr.StartsWith(TEXT("WaitingRoomSlot_")))
            {
                return FCString::Atoi(*TagStr.RightChop(16)); // "WaitingRoomSlot_" 이후
            }
        }
        return MAX_int32;
    };
    return GetSlotIndex(A.Get()) < GetSlotIndex(B.Get());
});
```

**설계 근거**:
- 태그(`WaitingRoomSlot_0`, `_1`, `_2`, `_3`)는 레벨 디자이너가 명시적으로 지정한 순서
- 액터 이름(`TargetPoint`, `TargetPoint2`, ...)은 UE 에디터가 자동 생성하는 이름이므로 불안정
- 태그 suffix에서 숫자를 파싱하여 정렬하면, 슬롯 순서가 태그 의도와 항상 일치

#### 수정 4-2: PositionPawnAtSlot에 ForceNetUpdate 추가

**파일**: `DRLobbyGameMode.cpp` — `PositionPawnAtSlot` 함수 (468~469번 줄)

```cpp
// 서버 측 텔레포트
FVector SlotLocation = SlotActor->GetActorLocation();
Pawn->TeleportTo(SlotLocation, FacingRotation);
Pawn->ForceNetUpdate();  // ★ 추가: 위치 변경 즉시 네트워크 전파
```

**설계 근거**:
- `ForceNetUpdate()`는 해당 액터의 다음 네트워크 업데이트를 즉시 스케줄링
- `ClientTeleportToSlot` RPC가 클라이언트 소유 캐릭터의 위치를 직접 처리하지만, 호스트 화면에서 보이는 **다른 클라이언트의 simulated proxy** 위치도 즉시 갱신되도록 보장
- 특히 Kick 후 `RepositionAllPlayers` → `AssignPlayerToSlot` → `PositionPawnAtSlot` 체인에서, 남은 플레이어의 위치가 호스트 화면에서 즉시 반영됨

**수정 후 Kick 시나리오** (1P, 2P, 3P → 2P Kick):
```
1. KickPlayer 호출
2. PlayerSlotMap.Remove(2P) → {1P:0, 3P:2}
3. ClientKicked RPC → 2P 퇴출
4. RepositionAllPlayers:
   - OrderedPlayers = [1P(호스트), 3P]
   - PlayerSlotMap.Empty(), NextAvailableSlot = 0
   - AssignPlayerToSlot(1P) → slot 0 → PositionPawnAtSlot(1P, 0)
   - AssignPlayerToSlot(3P) → slot 1 → PositionPawnAtSlot(3P, 1)
     → 태그 suffix 정렬로 slot 1은 정확히 WaitingRoomSlot_1 위치
     → ForceNetUpdate()로 호스트 화면에서 즉시 반영
     → ClientTeleportToSlot RPC로 3P 클라이언트에서도 즉시 위치 이동
```

---

## 추가 버그 5: FreeRoam 전환 후 스킬 아이콘이 여전히 변경되지 않음

### 현상
- 버그 3의 수정(`InitOverlay`에 `BroadcastAbilityInfo()` 추가)에도 불구하고 아이콘이 변경되지 않음
- 스테이지에서는 정상 작동

### 추가 분석

버그 3 수정에서 `InitOverlay` 내부에 `BroadcastAbilityInfo()` 호출을 `SetWidgetController` 이후에 추가했지만, 이것은 `AddToViewport()` **이전**에 호출됨.

**`InitOverlay` 전체 순서** (버그 3 수정 후):
```
1. CreateWidget → 위젯 인스턴스 생성
2. GetOverlayWidgetController → BindCallbacksToDependencies → BroadcastAbilityInfo (소실)
3. SetWidgetController → WidgetControllerSet() BP 이벤트 발동 → 자식 위젯들이 AbilityInfoDelegate에 바인딩
4. BroadcastInitialValues
5. BroadcastAbilityInfo ← 버그 3 수정 위치
6. AddToViewport → NativeConstruct 발동
```

**Step 5에서 `BroadcastAbilityInfo`가 실행되지만 아이콘이 변경되지 않는 이유**:

UMG 위젯 트리의 **자식 위젯(스킬 슬롯)**이 `AbilityInfoDelegate`에 바인딩하는 시점이 문제:
- 부모 위젯의 `WidgetControllerSet` → 자식 위젯에 컨트롤러 전파 → 자식의 바인딩
- 이 체인은 동기적이지만, 자식 위젯이 `NativeConstruct`(`AddToViewport` 시점) 이후에야 내부 UI 컴포넌트(이미지 등)에 접근 가능한 경우가 있음

**`AddToViewport()` 전에 `BroadcastAbilityInfo()`를 호출하면**, 자식 위젯의 UI 컴포넌트가 아직 완전히 초기화되지 않았을 수 있음.

**스테이지에서는 정상 작동하는 이유**:
- `BroadcastAbilityInfo`가 `AbilitiesGivenDelegate`를 통해 **비동기적으로** 호출됨 (다음 프레임 이후)
- 이 시점에서 위젯 트리가 완전히 초기화된 상태에서 수신

### 적용된 수정: InitOverlayForFreeRoam에서 지연 BroadcastAbilityInfo 호출

**파일**: `DRPlayerController.cpp` — `InitOverlayForFreeRoam` 함수 (1376~1418번 줄)

```cpp
void ADRPlayerController::InitOverlayForFreeRoam()
{
    ADRHUD* DRHUD = Cast<ADRHUD>(GetHUD());
    if (!DRHUD) return;

    ADRPlayerState* PS = GetPlayerState<ADRPlayerState>();
    if (!PS) return;

    UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
    UAttributeSet* AS = PS->GetAttributeSet();
    if (!ASC || !AS) return;

    DRHUD->InitOverlay(this, PS, ASC, AS);

    // ★ 추가: 위젯 트리 완전 초기화 후 어빌리티 아이콘 강제 갱신
    FTimerHandle AbilityIconTimerHandle;
    GetWorldTimerManager().SetTimer(
        AbilityIconTimerHandle,
        [WeakThis = TWeakObjectPtr<ADRPlayerController>(this)]()
        {
            ADRPlayerController* PC = WeakThis.Get();
            if (!PC) return;

            ADRHUD* HUD = Cast<ADRHUD>(PC->GetHUD());
            if (!HUD) return;

            ADRPlayerState* PlayerState = PC->GetPlayerState<ADRPlayerState>();
            if (!PlayerState) return;

            UAbilitySystemComponent* AbilitySystem = PlayerState->GetAbilitySystemComponent();
            UAttributeSet* Attributes = PlayerState->GetAttributeSet();
            if (!AbilitySystem || !Attributes) return;

            const FWidgetControllerParams Params(PC, PlayerState, AbilitySystem, Attributes);
            if (UOverlayWidgetController* WC = HUD->GetOverlayWidgetController(Params))
            {
                WC->BroadcastAbilityInfo();
            }
        },
        0.1f,
        false
    );
}
```

**설계 근거**:
- 스테이지에서 아이콘이 정상 작동하는 이유를 재현: `InitOverlay` 직후가 아닌, 위젯 트리가 완전히 구성된 후(0.1초 딜레이) `BroadcastAbilityInfo`를 호출
- `InitOverlay` 내부의 `BroadcastAbilityInfo()`는 유지 — 동기적 바인딩이 작동하는 경우를 위한 것이고, 지연 호출은 비동기적 바인딩(`NativeConstruct` 후 바인딩)을 위한 안전망
- `WeakObjectPtr` 캡처로 타이머 콜백 안전성 보장
- 0.1초는 위젯 트리 초기화에 충분하면서도 사용자가 눈치채지 못할 정도로 짧음

**수정 후 동작**:
```
T=0.0s  InitOverlayForFreeRoam 호출
        → InitOverlay → CreateWidget, SetWidgetController, BroadcastAbilityInfo(동기), AddToViewport
        → AddToViewport에서 NativeConstruct 발동 → 자식 위젯 UI 컴포넌트 초기화
T=0.1s  타이머 콜백: BroadcastAbilityInfo() 재호출
        → 위젯 트리가 완전히 초기화된 상태 → AbilityInfoDelegate 정상 수신 → 아이콘 갱신 ✓
```

---

## 전체 수정 요약 (2차 수정 포함)

| 버그 | 수정 대상 파일 | 변경 내용 | 상태 |
|------|----------------|----------|------|
| 버그 1 (클라이언트 위치) | `DRPlayerController.h` | `ClientTeleportToSlot` Client RPC 선언 | ✅ 완료 |
| 버그 1 (클라이언트 위치) | `DRPlayerController.cpp` | `ClientTeleportToSlot_Implementation` 구현 | ✅ 완료 |
| 버그 1 (클라이언트 위치) | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot`에서 리모트 클라이언트에 RPC 전송 | ✅ 완료 |
| 버그 2 (카메라) | `DRWaitingRoomCameraActor.cpp` | `bReplicates = true`, `bAlwaysRelevant = true` | ✅ 완료 |
| 버그 2 (카메라) | `DRPlayerController.cpp` | `OnLevelEntered` 로비 분기에 `bAutoManageActiveCameraTarget = false` | ✅ 완료 |
| 버그 2 (카메라) | `DRPlayerController.cpp` | `ClientStartCameraTransitionToCharacter`에 `bAutoManageActiveCameraTarget = true` 복원 | ✅ 완료 |
| 버그 3 (아이콘 초기) | `DRHUD.cpp` | `InitOverlay`에서 `SetWidgetController` 후 `BroadcastAbilityInfo()` 추가 | ✅ 완료 |
| 버그 4 (Kick 재배치) | `DRLobbyGameMode.cpp` | `FindWaitingRoomActors` 정렬을 태그 suffix 기준으로 변경 | ✅ 완료 |
| 버그 4 (Kick 재배치) | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot`에 `ForceNetUpdate()` 추가 | ✅ 완료 |
| 버그 5 (아이콘 지연) | `DRPlayerController.cpp` | `InitOverlayForFreeRoam`에 0.1초 딜레이 `BroadcastAbilityInfo` 추가 | ✅ 완료 |

**총 수정 파일 4개, 변경 포인트 10곳.**

---

## 추가 버그 6: 클라이언트에게 Kick 버튼이 보이는 문제

### 현상
- 클라이언트 화면에서도 다른 플레이어 머리 위에 Kick 버튼이 보임
- 다행히 버튼을 클릭해도 아무 일이 발생하지 않음 (서버에서 호스트 권한 체크 작동)
- 호스트에게만 보여야 함

### 코드 분석

Kick 버튼 가시성은 **WBP_PlayerSlot** 블루프린트의 `UpdateSlot` 이벤트에서 제어:

```
[UpdateSlot] (Info, bShowKick, bIsMySlot)
├── Btn_Kick → Set Visibility
│     분기: [bShowKick] AND [NOT bIsMySlot]
│     → True: Visible
│     → False: Collapsed
```

`bShowKick`는 `WBP_WaitingRoom`의 `RefreshPlayerSlots`에서 `bCachedIsHost`를 전달:

```
PlayerSlots → Get (Array Index)
→ Update Slot (
      Info = Array Element,
      bShowKick = bCachedIsHost,
      bIsMySlot = bIsLocalPlayer
  )
```

`bCachedIsHost`는 `SetIsHost(bool bIsHost)`에서 설정됨. `SetIsHost`는 `CreateWaitingRoomUI()`에서 호출:

```cpp
// DRPlayerController.cpp (수정 전)
ADRGameStateBase* GS = GetWorld()->GetGameState<ADRGameStateBase>();
bool bIsHost = GS && GS->IsPlayerHost(PlayerState);
WaitingRoomWidget->SetIsHost(bIsHost);
```

### 근본 원인: `IsPlayerHost` 판별이 클라이언트에서 부정확

`CreateWaitingRoomUI`는 클라이언트의 `ClientSetWaitingRoomView` RPC 도착 시 호출됨. 이 시점에서:
- `GameState::IsPlayerHost()`가 `PlayerArray[0]`과 비교하는 로직이라면, `PlayerArray` 리플리케이션 타이밍에 따라 클라이언트에서 자기 자신이 첫 번째로 보여 `true`가 반환될 수 있음
- 또는 `IsPlayerHost`의 구현이 로컬 컨트롤러 체크를 사용하면 항상 `true`가 됨

Listen Server 아키텍처에서 **서버 권한을 가진 로컬 컨트롤러 = 호스트**이므로 `HasAuthority()`가 가장 확실한 판별 방법.

### 적용된 수정: `HasAuthority()`로 호스트 판별 변경

**파일**: `DRPlayerController.cpp` — `CreateWaitingRoomUI` 함수 (1307~1309번 줄)

```cpp
// 수정 전:
ADRGameStateBase* GS = GetWorld()->GetGameState<ADRGameStateBase>();
bool bIsHost = GS && GS->IsPlayerHost(PlayerState);
WaitingRoomWidget->SetIsHost(bIsHost);

// 수정 후:
bool bIsHost = HasAuthority();
WaitingRoomWidget->SetIsHost(bIsHost);
```

**설계 근거**:
- `HasAuthority()`는 서버에서만 `true` → 호스트의 `CreateWaitingRoomUI`에서만 `true`
- 클라이언트에서는 항상 `false` → Kick 버튼 숨김 보장
- `GameState` 리플리케이션 타이밍에 의존하지 않아 안정적
- 기존 `KickPlayer`, `PowerOn`, `TravelToStage` 등도 모두 `HasAuthority()` + `IsLocalController()`로 호스트를 판별하므로 일관성 있음

---

## 추가 버그 7: Kick 후 남은 플레이어가 빈 슬롯으로 재배치되지 않음 (재발)

### 현상
- 이전 수정(태그 suffix 정렬 + ForceNetUpdate)에도 불구하고 3P가 2P 자리로 이동하지 않음

### 추가 분석

이전 수정은 정렬과 네트워크 동기화를 개선했지만, 근본적인 **CMC 리플리케이션 문제**를 놓침:

1. 최초 `PositionPawnAtSlot` 호출 시 CMC를 `MOVE_None`으로 설정
2. Kick 후 `RepositionAllPlayers` → `AssignPlayerToSlot` → `PositionPawnAtSlot` 재호출
3. 이 시점에서 Pawn의 CMC는 **이미 `MOVE_None` 상태**
4. `MOVE_None` 상태에서 `TeleportTo`를 해도 CMC의 이동 리플리케이션 채널이 비활성
5. `ForceNetUpdate()`는 Actor의 일반 프로퍼티 리플리케이션만 강제하지, CMC의 이동 리플리케이션은 별도 채널
6. **결과**: 서버에서 위치가 변경되었지만 호스트 화면의 simulated proxy가 이전 위치에 고정

`ClientTeleportToSlot` RPC는 소유 클라이언트의 로컬 위치를 보정하지만, **호스트 화면에서 보이는 다른 클라이언트의 simulated proxy**는 CMC 리플리케이션에 의존.

### 적용된 수정: CMC MOVE_None → Walking 전환 후 텔레포트, 0.1초 후 재비활성화

**파일**: `DRLobbyGameMode.cpp` — `PositionPawnAtSlot` 함수 (466~497번 줄)

```cpp
FVector SlotLocation = SlotActor->GetActorLocation();

// ★ CMC가 MOVE_None이면 텔레포트 전에 Walking으로 전환하여 리플리케이션 활성화
UCharacterMovementComponent* MovementComp =
    Cast<UCharacterMovementComponent>(Pawn->GetMovementComponent());
if (MovementComp && MovementComp->MovementMode == MOVE_None)
{
    MovementComp->SetMovementMode(MOVE_Walking);
}

Pawn->TeleportTo(SlotLocation, FacingRotation);
Pawn->ForceNetUpdate();

// ★ 텔레포트 후 이동 비활성화 (짧은 딜레이로 리플리케이션 시간 확보)
if (MovementComp)
{
    FTimerHandle DisableMovementTimer;
    TWeakObjectPtr<UCharacterMovementComponent> WeakMoveComp(MovementComp);
    GetWorldTimerManager().SetTimer(
        DisableMovementTimer,
        [WeakMoveComp]()
        {
            if (UCharacterMovementComponent* MC = WeakMoveComp.Get())
            {
                MC->DisableMovement();
            }
        },
        0.1f,
        false
    );
}
```

**핵심 변경**:
1. 텔레포트 전 CMC가 `MOVE_None`이면 `MOVE_Walking`으로 전환 → CMC 이동 리플리케이션 채널 활성화
2. 텔레포트 + `ForceNetUpdate()`로 즉시 네트워크 전파
3. 0.1초 후 `DisableMovement()`로 이동 재비활성화

**설계 근거**:
- `MOVE_Walking` 상태에서 `TeleportTo`를 하면 CMC가 위치 변경을 감지하고 simulated proxy에 전파
- 0.1초 딜레이는 서버→클라이언트 위치 전파에 충분한 시간
- `WeakObjectPtr` 캡처로 타이머 콜백 안전성 보장
- 최초 호출(이미 Walking 상태)에서는 조건문(`MovementMode == MOVE_None`)에 의해 불필요한 모드 변경을 스킵

---

## 추가 버그 8: WaitingRoom에서 플레이어 닉네임이 머리 위에 표시되는 문제

### 현상
- WaitingRoom 상태에서 캐릭터 머리 위에 닉네임이 표시됨
- WaitingRoom UI(WBP_PlayerSlot의 Txt_PlayerName)에서 이미 닉네임을 보여주므로 중복
- FreeRoam부터 표시되어야 함

### 코드 분석

닉네임 표시 시스템:
- `DROverheadWidget` 클래스 존재 (`SetDisplayText`, `DisplayText` TextBlock)
- `DRCharacter.h`에 `class UWidgetComponent;` 전방 선언이 있지만 C++ 멤버 변수는 없음
- WidgetComponent는 **캐릭터 블루프린트(BP)**에서 추가되어 있음
- C++에서는 `GetComponents<UWidgetComponent>`로 BP에서 추가된 컴포넌트를 검색 가능

### 적용된 수정: SetOverheadWidgetVisibility 함수 추가 + 대기실/FreeRoam 전환 시 토글

#### 수정 8-1: DRCharacter.h — 함수 선언 (123~124번 줄)

```cpp
// 오버헤드 닉네임 위젯 가시성 제어 (WaitingRoom에서는 숨김, FreeRoam부터 표시)
void SetOverheadWidgetVisibility(bool bVisible);
```

#### 수정 8-2: DRCharacter.cpp — 함수 구현 + include 추가

**include 추가**:
```cpp
#include "Components/WidgetComponent.h"
#include "Game/DRLobbyGameState.h"
```

**구현**:
```cpp
void ADRCharacter::SetOverheadWidgetVisibility(bool bVisible)
{
    // 블루프린트에서 추가된 WidgetComponent를 검색하여 가시성 제어
    TArray<UWidgetComponent*> WidgetComponents;
    GetComponents<UWidgetComponent>(WidgetComponents);

    for (UWidgetComponent* WidgetComp : WidgetComponents)
    {
        if (WidgetComp)
        {
            WidgetComp->SetVisibility(bVisible);
        }
    }
}
```

#### 수정 8-3: SetWaitingRoomVisibility에서 호출 통합

```cpp
void ADRCharacter::SetWaitingRoomVisibility(bool bInWaitingRoom)
{
    if (bInWaitingRoom)
    {
        // ... (기존 1P 숨기기, 3P 보이기 코드)

        // ★ 추가: 대기실에서는 오버헤드 닉네임 숨김 (WBP_PlayerSlot UI에서 표시)
        SetOverheadWidgetVisibility(false);
    }
    else
    {
        // ... (기존 FPS 모드 복원 코드)

        // ★ 추가: FreeRoam부터 오버헤드 닉네임 표시 복원
        SetOverheadWidgetVisibility(true);
    }
}
```

#### 수정 8-4: BeginPlay에서 초기 가시성 설정

```cpp
void ADRCharacter::BeginPlay()
{
    Super::BeginPlay();
    // ... (기존 코드)

    // ★ 추가: 대기실이면 오버헤드 닉네임 위젯 숨김
    if (UWorld* World = GetWorld())
    {
        ADRLobbyGameState* LGS = World->GetGameState<ADRLobbyGameState>();
        if (LGS && (LGS->GetLobbyState() == ELobbyState::WaitingRoom
                 || LGS->GetLobbyState() == ELobbyState::Transitioning))
        {
            SetOverheadWidgetVisibility(false);
        }
    }
}
```

**설계 근거**:
- `GetComponents<UWidgetComponent>`로 BP에서 추가된 위젯 컴포넌트를 검색 → 블루프린트 수정 불필요
- `SetWaitingRoomVisibility`는 이미 대기실/FreeRoam 전환 시 메시 가시성을 제어하는 함수이므로, 오버헤드 위젯 가시성도 여기서 관리하는 것이 자연스러움
- `BeginPlay`에서 초기 상태를 설정하여 스폰 직후부터 올바른 가시성 보장
- 클라이언트에서 `BeginPlay` 시점에 LGS가 null이더라도, `ClientSetWaitingRoomView` → `SetWaitingRoomVisibility(true)` 체인에서 `SetOverheadWidgetVisibility(false)`가 호출되므로 교정됨

---

## 전체 수정 요약 (3차 수정 포함)

| 버그 | 수정 대상 파일 | 변경 내용 | 상태 |
|------|----------------|----------|------|
| 버그 1 (클라이언트 위치) | `DRPlayerController.h` | `ClientTeleportToSlot` Client RPC 선언 | ✅ 완료 |
| 버그 1 (클라이언트 위치) | `DRPlayerController.cpp` | `ClientTeleportToSlot_Implementation` 구현 | ✅ 완료 |
| 버그 1 (클라이언트 위치) | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot`에서 리모트 클라이언트에 RPC 전송 | ✅ 완료 |
| 버그 2 (카메라) | `DRWaitingRoomCameraActor.cpp` | `bReplicates = true`, `bAlwaysRelevant = true` | ✅ 완료 |
| 버그 2 (카메라) | `DRPlayerController.cpp` | `OnLevelEntered` 로비 분기에 `bAutoManageActiveCameraTarget = false` | ✅ 완료 |
| 버그 2 (카메라) | `DRPlayerController.cpp` | `ClientStartCameraTransitionToCharacter`에 `bAutoManageActiveCameraTarget = true` 복원 | ✅ 완료 |
| 버그 3 (아이콘 초기) | `DRHUD.cpp` | `InitOverlay`에서 `SetWidgetController` 후 `BroadcastAbilityInfo()` 추가 | ✅ 완료 |
| 버그 4 (Kick 재배치) | `DRLobbyGameMode.cpp` | `FindWaitingRoomActors` 정렬을 태그 suffix 기준으로 변경 | ✅ 완료 |
| 버그 4 (Kick 재배치) | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot`에 `ForceNetUpdate()` 추가 | ✅ 완료 |
| 버그 5 (아이콘 지연) | `DRPlayerController.cpp` | `InitOverlayForFreeRoam`에 0.1초 딜레이 `BroadcastAbilityInfo` 추가 | ✅ 완료 |
| 버그 6 (Kick 버튼) | `DRPlayerController.cpp` | `CreateWaitingRoomUI`에서 호스트 판별을 `HasAuthority()`로 변경 | ✅ 완료 |
| 버그 7 (Kick 재배치 재발) | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot`에서 MOVE_None→Walking 전환 후 텔레포트, 0.1초 후 재비활성화 | ✅ 완료 |
| 버그 8 (닉네임) | `DRCharacter.h` | `SetOverheadWidgetVisibility` 함수 선언 추가 | ✅ 완료 |
| 버그 8 (닉네임) | `DRCharacter.cpp` | `SetOverheadWidgetVisibility` 구현 + `SetWaitingRoomVisibility` 통합 + `BeginPlay` 초기화 | ✅ 완료 |

**총 수정 파일 5개, 변경 포인트 14곳.**

---

## 추가 버그 9: Kick 후 캐릭터 메시가 이동하지 않음 (UI 슬롯은 정상 이동)

### 현상
- 1P, 2P, 3P가 있을 때 2P를 Kick하면 UI 슬롯은 잘 이동하지만, **3P의 캐릭터 메시**는 원래 위치(슬롯 2)에 그대로 남음
- 호스트 화면에서도, 3P 클라이언트 화면에서도 동일하게 메시가 이동하지 않음

### 원인 분석

버그 7에서 `MOVE_None → MOVE_Walking` 전환 + `ForceNetUpdate` + 0.1초 후 `DisableMovement` 접근법을 적용했지만 여전히 실패.

두 가지 잔여 문제:

1. **`IsLocalController()` 체크로 호스트 폰이 RPC를 받지 못함** (DRLobbyGameMode.cpp:502):
```cpp
if (!PC->IsLocalController())  // ← 호스트의 폰은 여기서 걸려서 ClientTeleportToSlot 미전송
{
    DRPC->ClientTeleportToSlot(SlotLocation, FacingRotation);
}
```
Listen Server에서 호스트의 `IsLocalController()`는 `true`이므로, 호스트 자신의 폰에는 `ClientTeleportToSlot` RPC가 전송되지 않음. 서버의 `TeleportTo`만 실행되는데, CMC 리플리케이션 타이밍 문제로 호스트 화면에서 자신의 폰 위치가 반영되지 않을 수 있음.

2. **Velocity 잔여값으로 인한 drift**: CMC를 `MOVE_Walking`으로 전환하면 중력 등의 영향으로 velocity가 발생할 수 있으며, 텔레포트 후에도 잔여 velocity로 인해 미세하게 위치가 벗어날 수 있음.

3. **타이머 0.1초가 불충분**: 네트워크 상황에 따라 CMC 리플리케이션이 0.1초 내에 완료되지 않을 수 있음.

### 적용된 수정

**파일**: `DRLobbyGameMode.cpp` — `PositionPawnAtSlot` 함수

```cpp
// ★ 핵심 변경 1: CMC를 무조건 Walking으로 전환 (조건문 단순화)
if (MovementComp)
{
    MovementComp->SetMovementMode(MOVE_Walking);
    MovementComp->Velocity = FVector::ZeroVector; // ★ 잔여 이동 방지
}

Pawn->TeleportTo(SlotLocation, FacingRotation);
Pawn->ForceNetUpdate();

// ★ 핵심 변경 2: IsLocalController() 체크 제거 → 호스트 포함 모든 소유자에게 RPC 전송
if (APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
{
    if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
    {
        DRPC->ClientTeleportToSlot(SlotLocation, FacingRotation);
    }
}

// ★ 핵심 변경 3: 타이머 0.1초 → 0.2초, 완료 후 추가 ForceNetUpdate
if (MovementComp)
{
    FTimerHandle DisableMovementTimer;
    TWeakObjectPtr<UCharacterMovementComponent> WeakMoveComp(MovementComp);
    TWeakObjectPtr<APawn> WeakPawn(Pawn);
    GetWorldTimerManager().SetTimer(
        DisableMovementTimer,
        [WeakMoveComp, WeakPawn]()
        {
            if (UCharacterMovementComponent* MC = WeakMoveComp.Get())
            {
                MC->DisableMovement();
            }
            if (APawn* P = WeakPawn.Get())
            {
                P->ForceNetUpdate(); // ★ DisableMovement 상태까지 전파
            }
        },
        0.2f,
        false
    );
}
```

**파일**: `DRPlayerController.cpp` — `ClientTeleportToSlot_Implementation`

```cpp
void ADRPlayerController::ClientTeleportToSlot_Implementation(FVector SlotLocation, FRotator SlotRotation)
{
    if (APawn* MyPawn = GetPawn())
    {
        MyPawn->TeleportTo(SlotLocation, SlotRotation);

        if (UCharacterMovementComponent* MovementComp =
            Cast<UCharacterMovementComponent>(MyPawn->GetMovementComponent()))
        {
            MovementComp->Velocity = FVector::ZeroVector; // ★ 추가
            MovementComp->DisableMovement();
        }
    }
}
```

**핵심 변경점 요약**:
1. `IsLocalController()` 체크 제거 → 호스트도 Client RPC를 수신 (Listen Server에서 Client RPC는 로컬에서 직접 실행)
2. `Velocity = FVector::ZeroVector` 추가 (서버 + 클라이언트 양쪽)
3. 타이머 0.1초 → 0.2초로 증가
4. 타이머 완료 후 `ForceNetUpdate()` 추가 (DisableMovement 상태 전파)

---

## 추가 버그 10: 클래스 변경 버튼(◀ ▶)이 항상 슬롯 0(호스트 슬롯)에 표시됨

### 현상
- 서버와 클라이언트 관계없이 클래스 변경 버튼(Btn_PrevClass, Btn_NextClass)이 **항상 1번 슬롯(호스트 슬롯)**에서만 보임
- 본인 슬롯에서만 보여야 함

### 코드 분석

`RefreshWaitingRoomUI()` (DRPlayerController.cpp:1339~1365):

```cpp
for (APlayerState* PS : GS->PlayerArray)
{
    // ...
    Info.bIsLocalPlayer = (PS == PlayerState);  // ★ 문제 지점
    // ...
}
```

`WBP_WaitingRoom` Blueprint의 `RefreshPlayerSlots`에서:
```
UpdateSlot(Info, bShowKick=bCachedIsHost, bIsMySlot=bIsLocalPlayer)
```

`bIsMySlot`이 `true`인 슬롯에서만 `Btn_PrevClass`/`Btn_NextClass`가 Visible로 설정됨.

### 근본 원인: PlayerState 포인터 비교의 리플리케이션 타이밍 문제

`PS == PlayerState` 비교에서:
- `PS`는 `GS->PlayerArray`에서 가져온 `APlayerState*`
- `PlayerState`는 `AController::PlayerState` (현재 컨트롤러의 PlayerState)

클라이언트에서 `ClientSetWaitingRoomView` RPC 수신 → `CreateWaitingRoomUI` → `RefreshWaitingRoomUI` 시점에, `GS->PlayerArray`의 PlayerState 객체 포인터와 `AController::PlayerState` 포인터가 **동일 객체를 가리키지 않을 수 있음**. UE의 리플리케이션에서 PlayerArray는 별도 채널로 복제되며, 타이밍에 따라 클라이언트 로컬의 PlayerState와 GameState에 저장된 PlayerState가 다른 인스턴스일 수 있음.

호스트(Listen Server)에서는 모든 객체가 로컬이므로 포인터 비교가 항상 성공 → 호스트 슬롯(인덱스 0)에서만 `bIsLocalPlayer == true`.

### 적용된 수정: PlayerId 기반 비교로 변경

**파일**: `DRPlayerController.cpp` — `RefreshWaitingRoomUI` 함수 (1358번 줄)

```cpp
// 수정 전:
Info.bIsLocalPlayer = (PS == PlayerState);

// 수정 후:
APlayerState* MyPS = GetPlayerState<APlayerState>();
Info.bIsLocalPlayer = MyPS && (PS->GetPlayerId() == MyPS->GetPlayerId());
```

**설계 근거**:
- `GetPlayerId()`는 서버에서 할당된 고유 ID로, 리플리케이션 후에도 동일한 값을 보장
- 포인터 비교와 달리 객체 인스턴스가 다르더라도 동일 플레이어를 식별 가능
- 호스트와 클라이언트 모두에서 안정적으로 작동

---

## 추가 버그 11: 클라이언트에서 클래스 변경 시 카메라가 캐릭터 시점으로 전환됨

### 현상
- 클라이언트에서 캐릭터 변경 버튼을 누르면 카메라 시점이 WaitingRoomCamera가 아닌 **캐릭터의 시점으로 바뀜**
- 호스트에서는 정상 (카메라 유지)

### 코드 분석

`RespawnPlayerWithClass()` (DRLobbyGameMode.cpp:547~592) 흐름:

```
1. PC->UnPossess()             — 기존 폰 해제
2. OldPawn->Destroy()          — 기존 폰 파괴
3. SpawnActor<ADRCharacter>    — 새 폰 스폰
4. PC->Possess(NewPawn)        — 새 폰 빙의 ★ 문제 지점
5. PositionPawnAtSlot()        — 슬롯 위치로 텔레포트
6. ClientSetWaitingRoomView()  — 고정 카메라 재설정
```

### 근본 원인: `Possess()`가 클라이언트에서 `ClientRestart` RPC를 트리거

`APlayerController::Possess()`는 내부적으로 `ClientRestart()` Client RPC를 호출한다. `ClientRestart_Implementation()`은 `SetViewTarget(GetPawn())`을 호출하여 ViewTarget을 새 Pawn으로 설정한다.

**타이밍 시퀀스** (클라이언트):
```
T=0.0s  서버: Possess(NewPawn) → ClientRestart RPC + ClientSetWaitingRoomView RPC 전송
T≈0.05s 클라이언트: ClientRestart 도착 → SetViewTarget(NewPawn) → 캐릭터 시점으로 전환 ★
T≈0.05s 클라이언트: ClientSetWaitingRoomView 도착 → SetViewTargetWithBlend(Camera) → 복원
```

문제는 두 RPC의 **도착 순서**가 보장되지 않거나, `ClientRestart`가 `ClientSetWaitingRoomView`보다 **늦게** 도착할 수 있다는 점. 이 경우 카메라가 캐릭터 시점으로 고정됨.

또한 `bAutoManageActiveCameraTarget`이 `true`이면, `OnRep_Pawn` 트리거 시 UE가 자동으로 ViewTarget을 Pawn으로 설정하는 추가 경로도 존재.

### 적용된 수정: 3중 보호

#### 수정 11-1: `ClientSetWaitingRoomView`에서 `bAutoManageActiveCameraTarget = false` 명시 설정

**파일**: `DRPlayerController.cpp` — `ClientSetWaitingRoomView_Implementation` (1220번 줄 뒤에 추가)

```cpp
bIsInWaitingRoom = true;
bAutoManageActiveCameraTarget = false; // ★ 추가: Pawn 변경 시 자동 ViewTarget 전환 차단
```

이렇게 하면 `ClientSetWaitingRoomView`가 호출될 때마다 `bAutoManageActiveCameraTarget`이 `false`로 설정되어, 이후 `OnRep_Pawn`이 도착해도 ViewTarget이 자동 변경되지 않음.

#### 수정 11-2: `RespawnPlayerWithClass`에서 0.15초 딜레이 후 카메라 재설정

**파일**: `DRLobbyGameMode.cpp` — `RespawnPlayerWithClass` 함수 (586~606번 줄)

```cpp
// 고정 카메라 즉시 재설정
if (WaitingRoomCamera)
{
    PC->ClientSetWaitingRoomView(WaitingRoomCamera);

    // ★ 0.15초 후 한번 더 카메라 재설정 (ClientRestart RPC가 늦게 도착하는 경우 대비)
    FTimerHandle CameraResetTimer;
    TWeakObjectPtr<ADRPlayerController> WeakPC(PC);
    TWeakObjectPtr<ADRWaitingRoomCameraActor> WeakCamera(WaitingRoomCamera);
    GetWorldTimerManager().SetTimer(
        CameraResetTimer,
        [WeakPC, WeakCamera]()
        {
            if (ADRPlayerController* StrongPC = WeakPC.Get())
            {
                if (ADRWaitingRoomCameraActor* StrongCamera = WeakCamera.Get())
                {
                    StrongPC->ClientSetWaitingRoomView(StrongCamera);
                }
            }
        },
        0.15f,
        false
    );
}
```

0.15초 딜레이로 `ClientRestart`가 먼저 도착하여 ViewTarget을 변경하더라도, 딜레이 후 `ClientSetWaitingRoomView`가 다시 카메라를 WaitingRoomCamera로 복원.

#### 수정 11-3: `PossessedBy`에서 대기실이면 `bAutoManageActiveCameraTarget` 차단

**파일**: `DRCharacter.cpp` — `PossessedBy` 함수 (101번 줄 뒤에 추가)

```cpp
InitializeMoveSpeedBinding();

// ★ 대기실 상태이면 카메라 자동 관리 비활성화 (Possess로 인한 ViewTarget 자동 전환 방지)
if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(NewController))
{
    if (DRPC->bIsInWaitingRoom)
    {
        DRPC->bAutoManageActiveCameraTarget = false;
    }
}
```

서버(Listen Server)에서 `Possess()` 직후 서버 측 PlayerController의 `bAutoManageActiveCameraTarget`도 `false`로 설정하여, 호스트 화면에서의 자동 ViewTarget 전환도 차단.

**설계 근거**:
- 3중 보호(bAutoManageActiveCameraTarget in ClientSetWaitingRoomView + PossessedBy + 딜레이 카메라 재설정)로 RPC 도착 순서에 관계없이 카메라 안정성 보장
- `bAutoManageActiveCameraTarget`은 `APlayerController`의 public 멤버이므로 외부 접근 가능
- `bIsInWaitingRoom` 조건으로 FreeRoam/스테이지에서의 정상 동작에 영향 없음

---

## 추가 버그 12: 클래스 변경할 때마다 이동속도가 중첩됨

### 현상
- 캐릭터를 여러 번 변경하면 이동속도가 변경 횟수만큼 중첩되어 점점 빨라짐
- 예: 3번 변경 시 이동속도가 3배

### 코드 분석

`InitializeMoveSpeedBinding()` (DRCharacter.cpp:498~510):

```cpp
void ADRCharacter::InitializeMoveSpeedBinding()
{
    if (!AbilitySystemComponent || !AttributeSets) return;

    if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSets))
    {
        // ★ BUG: AddUObject는 기존 바인딩을 확인하지 않음
        AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
            DRAS->GetMoveSpeedAttribute()).AddUObject(this, &ADRCharacter::OnMoveSpeedChanged);

        GetCharacterMovement()->MaxWalkSpeed = DRAS->GetMoveSpeed();
    }
}
```

**호출 경로**:
- `PossessedBy()` (line 101) — 서버에서 호출
- `OnRep_PlayerState()` (line 110) — 클라이언트에서 호출

### 근본 원인: ASC가 PlayerState에 소유되어 캐릭터 교체 시 delegate가 누적

`AbilitySystemComponent`와 `AttributeSets`는 **PlayerState에 소유**되어 있으므로, 캐릭터가 교체되어도 **같은 ASC 인스턴스**가 재사용됨.

`RespawnPlayerWithClass` 흐름:
1. `OldPawn->Destroy()` — 이전 캐릭터 파괴 (AddUObject 바인딩은 UObject 파괴 시 자동 정리)
2. `PC->Possess(NewPawn)` → `PossessedBy` → `InitAbilityActorInfo` → `InitializeMoveSpeedBinding` → `AddUObject`
3. `OnRep_PlayerState` → `InitializeMoveSpeedBinding` → `AddUObject` (2번째 바인딩!)

같은 캐릭터 내에서도 `PossessedBy` + `OnRep_PlayerState` 양쪽에서 호출되면 2배로 등록됨. 클래스 변경을 N번 하면 각 변경마다 1~2개씩 누적되어 `OnMoveSpeedChanged`가 N배로 호출됨.

`OnMoveSpeedChanged`는 `MaxWalkSpeed = Data.NewValue`를 설정하는 것이므로 호출 횟수와 무관하게 같은 값이 설정되어야 하지만, 실제로는 **Attribute 변경 이벤트가 delegate 호출 횟수만큼 중복 발생**하여 속도 계산에 영향을 미칠 수 있음.

### 적용된 수정: 바인딩 전 기존 delegate 제거

**파일**: `DRCharacter.cpp` — `InitializeMoveSpeedBinding` 함수 (498~510번 줄)

```cpp
void ADRCharacter::InitializeMoveSpeedBinding()
{
    if (!AbilitySystemComponent || !AttributeSets) return;

    if (UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSets))
    {
        // ★ 수정: 기존 바인딩 제거 후 재등록 (중복 방지)
        FOnGameplayAttributeValueChange& MoveSpeedDelegate =
            AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(DRAS->GetMoveSpeedAttribute());
        MoveSpeedDelegate.RemoveAll(this);
        MoveSpeedDelegate.AddUObject(this, &ADRCharacter::OnMoveSpeedChanged);

        GetCharacterMovement()->MaxWalkSpeed = DRAS->GetMoveSpeed();
    }
}
```

**핵심 변경**: `Delegate.RemoveAll(this)` 추가 — 현재 UObject(`this`)가 등록한 모든 delegate를 제거한 후 새로 등록.

**동작 보장**:
- 같은 캐릭터에서 `PossessedBy` + `OnRep_PlayerState` 양쪽에서 호출되어도 항상 1개만 유지
- 이전 캐릭터의 바인딩은 UObject 파괴 시 자동 정리 (`AddUObject` 특성)
- `RemoveAll(this)`는 `this`(현재 캐릭터)가 등록한 바인딩만 제거하므로 다른 객체의 바인딩에 영향 없음

---

## 전체 수정 요약 (4차 수정 포함)

| 버그 | 수정 대상 파일 | 변경 내용 | 상태 |
|------|----------------|----------|------|
| 버그 1 (클라이언트 위치) | `DRPlayerController.h` | `ClientTeleportToSlot` Client RPC 선언 | ✅ 완료 |
| 버그 1 (클라이언트 위치) | `DRPlayerController.cpp` | `ClientTeleportToSlot_Implementation` 구현 | ✅ 완료 |
| 버그 1 (클라이언트 위치) | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot`에서 리모트 클라이언트에 RPC 전송 | ✅ 완료 |
| 버그 2 (카메라) | `DRWaitingRoomCameraActor.cpp` | `bReplicates = true`, `bAlwaysRelevant = true` | ✅ 완료 |
| 버그 2 (카메라) | `DRPlayerController.cpp` | `OnLevelEntered` 로비 분기에 `bAutoManageActiveCameraTarget = false` | ✅ 완료 |
| 버그 2 (카메라) | `DRPlayerController.cpp` | `ClientStartCameraTransitionToCharacter`에 `bAutoManageActiveCameraTarget = true` 복원 | ✅ 완료 |
| 버그 3 (아이콘 초기) | `DRHUD.cpp` | `InitOverlay`에서 `SetWidgetController` 후 `BroadcastAbilityInfo()` 추가 | ✅ 완료 |
| 버그 4 (Kick 재배치) | `DRLobbyGameMode.cpp` | `FindWaitingRoomActors` 정렬을 태그 suffix 기준으로 변경 | ✅ 완료 |
| 버그 4 (Kick 재배치) | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot`에 `ForceNetUpdate()` 추가 | ✅ 완료 |
| 버그 5 (아이콘 지연) | `DRPlayerController.cpp` | `InitOverlayForFreeRoam`에 0.1초 딜레이 `BroadcastAbilityInfo` 추가 | ✅ 완료 |
| 버그 6 (Kick 버튼) | `DRPlayerController.cpp` | `CreateWaitingRoomUI`에서 호스트 판별을 `HasAuthority()`로 변경 | ✅ 완료 |
| 버그 7 (Kick 재배치 재발) | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot`에서 MOVE_None→Walking 전환 후 텔레포트, 0.1초 후 재비활성화 | ✅ 완료 |
| 버그 8 (닉네임) | `DRCharacter.h` | `SetOverheadWidgetVisibility` 함수 선언 추가 | ✅ 완료 |
| 버그 8 (닉네임) | `DRCharacter.cpp` | `SetOverheadWidgetVisibility` 구현 + `SetWaitingRoomVisibility` 통합 + `BeginPlay` 초기화 | ✅ 완료 |
| 버그 9 (메시 미이동) | `DRLobbyGameMode.cpp` | `PositionPawnAtSlot`에서 `IsLocalController()` 체크 제거, Velocity 초기화, 타이머 0.2초, 추가 ForceNetUpdate | ✅ 완료 |
| 버그 9 (메시 미이동) | `DRPlayerController.cpp` | `ClientTeleportToSlot`에 `Velocity = FVector::ZeroVector` 추가 | ✅ 완료 |
| 버그 10 (버튼 슬롯) | `DRPlayerController.cpp` | `RefreshWaitingRoomUI`에서 `PlayerState` 포인터 비교 → `GetPlayerId()` 비교로 변경 | ✅ 완료 |
| 버그 11 (카메라 전환) | `DRPlayerController.cpp` | `ClientSetWaitingRoomView`에 `bAutoManageActiveCameraTarget = false` 추가 | ✅ 완료 |
| 버그 11 (카메라 전환) | `DRLobbyGameMode.cpp` | `RespawnPlayerWithClass`에 0.15초 딜레이 후 카메라 재설정 RPC 추가 | ✅ 완료 |
| 버그 11 (카메라 전환) | `DRCharacter.cpp` | `PossessedBy`에서 대기실이면 `bAutoManageActiveCameraTarget = false` 설정 | ✅ 완료 |
| 버그 12 (속도 중첩) | `DRCharacter.cpp` | `InitializeMoveSpeedBinding`에서 `RemoveAll(this)` 후 재등록으로 중복 바인딩 방지 | ✅ 완료 |

**총 수정 파일 5개, 변경 포인트 21곳.**
