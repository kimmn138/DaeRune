# Research3: HUD/UI 충돌 분석 및 클라이언트 대기실 스폰 문제

## 1. HUD 초기화 시스템 분석

### 1.1 현재 아키텍처

DaeRune의 UI 시스템은 MVC 패턴을 따른다:

```
AGameMode (BP_DRLobbyGameMode / BP_DRStageGameMode)
  └─ HUDClass = BP_DRHUD_Lobby / BP_DRHUD_Stage

BP_DRHUD (ADRHUD)
  ├─ OverlayWidgetClass = WBP_LobbyOverlay / WBP_StageOverlay  (블루프린트에서 설정)
  ├─ OverlayWidgetControllerClass = BP_OverlayWidgetController  (블루프린트에서 설정)
  ├─ OverlayWidget (인스턴스)
  └─ OverlayWidgetController (싱글톤 인스턴스)
```

**핵심 포인트:**
- 로비와 스테이지가 **각각 다른 BP_HUD**를 사용한다
- 각 BP_HUD에는 **다른 OverlayWidgetClass**가 설정되어 있다 (로비용 vs 스테이지용)
- GameMode의 `HUDClass` 프로퍼티로 어떤 HUD를 쓸지 결정된다

### 1.2 HUD 초기화 흐름 (정상 플로우)

```
GameMode 시작
  → AGameMode::InitGame() → HUDClass 기반으로 AHUD 생성
  → PlayerController 생성 → HUD가 PC에 할당됨
  → DefaultPawnClass 기반으로 Pawn 스폰
  → Pawn의 PossessedBy() 호출
    → ADRCharacter::InitAbilityActorInfo()
      → PC->GetHUD() → ADRHUD*
      → DRHUD->InitOverlay(PC, PS, ASC, AS)
        → CreateWidget(OverlayWidgetClass)   ← ★ 여기서 오버레이 위젯 생성
        → GetOverlayWidgetController()       ← ★ 싱글톤으로 위젯 컨트롤러 생성
        → WidgetController->BindCallbacksToDependencies()  ← ★ ASC/GameState 바인딩
        → Widget->AddToViewport()
```

### 1.3 문제: 대기실에서 InitOverlay가 호출됨

**대기실에서의 흐름:**

```
PostLogin / HandleSeamlessTravelPlayer
  → Super (AGameMode::PostLogin)
    → GetDefaultPawnClassForController() → BP 클래스 결정
    → RestartPlayer() → Pawn 스폰 + Possess
      → ADRCharacter::PossessedBy()
        → InitAbilityActorInfo()
          → DRHUD->InitOverlay(...)  ← ★★ 대기실인데도 오버레이 생성됨!
  → 0.5초 타이머 → AssignPlayerToSlot() + ClientSetWaitingRoomView()
```

**문제점:**
1. `InitAbilityActorInfo()`는 Pawn이 Possess될 때 **무조건** 호출되므로, 대기실인지 아닌지 구분 없이 `InitOverlay()`가 실행된다
2. 로비용 HUD의 `OverlayWidgetClass`가 **로비 전용 오버레이 위젯**이라면, 대기실 화면 위에 로비 오버레이가 깔린다
3. `OverlayWidgetController`가 ASC에 바인딩을 시도하는데, 대기실에서는 아직 게임플레이 관련 데이터가 없어서 의미 없는 바인딩이 실행됨

### 1.4 HUD InitOverlay 세부 코드 분석

**`ADRHUD::InitOverlay()` (DRHUD.cpp)**
```cpp
void ADRHUD::InitOverlay(APlayerController* PC, APlayerState* PS,
                          UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
    // ★ checkf가 있어서 OverlayWidgetClass가 nullptr이면 크래시!
    checkf(OverlayWidgetClass, TEXT("Overlay Widget Class uninitialized"));
    checkf(OverlayWidgetControllerClass, TEXT("Overlay Widget Controller Class uninitialized"));

    // 위젯 생성 및 화면에 추가
    UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
    OverlayWidget = Cast<UDRUserWidget>(Widget);

    // ★ 싱글톤 패턴: 한 번만 생성
    const FWidgetControllerParams WCParams(PC, PS, ASC, AS);
    UOverlayWidgetController* WC = GetOverlayWidgetController(WCParams);

    OverlayWidget->SetWidgetController(WC);
    WC->BroadcastInitialValues();
    Widget->AddToViewport();
}
```

**중요 관찰:**
- `checkf`로 OverlayWidgetClass가 null이면 에디터가 크래시한다
- 매번 호출 시 **새로운 위젯을 생성**하고 AddToViewport한다 (기존 위젯 정리 없음!)
- WidgetController는 싱글톤이라 첫 번째 호출에서만 생성된다
- 그런데 `OverlayWidget`은 매번 새로 만들므로, 중복 호출 시 **이전 위젯이 화면에 남아있는 상태로 새 위젯이 추가**됨

### 1.5 대기실 UI와의 충돌 시나리오

```
시나리오: 클라이언트가 로비에 접속

[시간 T+0] PostLogin → AGameMode::RestartPlayer()
  → Pawn 스폰 → PossessedBy → InitAbilityActorInfo
  → DRHUD->InitOverlay(...)
    → 로비용 오버레이 위젯 생성 + AddToViewport  ← ★ 오버레이가 화면에 뜸

[시간 T+0] (거의 동시) OnLevelEntered() 호출 (클라이언트 로컬)
  → 대기실 감지 → CreateWaitingRoomUI()
    → WaitingRoomWidget->AddToViewport()  ← ★ 대기실 UI도 화면에 뜸

[시간 T+0.5] PostLogin 타이머 실행
  → ClientSetWaitingRoomView(Camera) → 카메라 전환

결과: 화면에 로비 오버레이 + 대기실 UI가 동시에 존재
```

### 1.6 대기실에서 HUD 충돌 해결 방안

**옵션 A: InitOverlay에서 대기실 체크 추가**

`ADRCharacter::InitAbilityActorInfo()` 안에서 대기실인지 확인하고, 대기실이면 `InitOverlay`를 스킵:

```cpp
// InitAbilityActorInfo() 끝부분
if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(GetController()))
{
    // ★ 대기실이면 HUD 오버레이 초기화 스킵
    if (!DRPC->bIsInWaitingRoom)
    {
        if (ADRHUD* DRHUD = Cast<ADRHUD>(DRPC->GetHUD()))
        {
            DRHUD->InitOverlay(DRPC, DRPlayerState, ASC, AttributeSets);
        }
    }
}
```

**장점:** 최소 수술. 대기실에서는 오버레이를 생성하지 않음.
**단점:** PowerOn 후 FreeRoam으로 전환 시 오버레이가 초기화되지 않은 상태. FreeRoam 진입 시 별도로 InitOverlay를 호출해야 함.

**옵션 B: InitOverlay에 중복 생성 방지 추가**

```cpp
void ADRHUD::InitOverlay(...)
{
    // 이미 오버레이가 있으면 리턴
    if (OverlayWidget)
    {
        // 기존 WidgetController 파라미터만 업데이트
        return;
    }
    // ... 기존 코드 ...
}
```

**옵션 C: 로비용 BP_HUD에서 OverlayWidgetClass를 빈 위젯으로 설정**

로비 BP_HUD의 OverlayWidgetClass를 "빈 오버레이"(투명, 아무것도 안 하는 위젯)로 설정하고, 실제 게임 UI는 스테이지 BP_HUD에서만 사용.

**권장: 옵션 A** — 가장 명확하고 의도가 분명함.

---

## 2. 클라이언트 대기실 스폰 문제 분석

### 2.1 문제 증상

| 증상 | 서버 | 클라이언트 |
|------|------|-----------|
| 카메라 시점 | 고정 카메라 ✅ | **캐릭터 1인칭 시점** ❌ |
| 캐릭터 위치 | 슬롯 위치 (추정) | **클라이언트 옆에 스폰** ❌ |
| 1인칭 메시 | 안 보임 ✅ | **화면에 보임** ❌ |
| 3인칭 메시 (슬롯 위치) | **안 보임** ❌ | **안 보임** ❌ |

### 2.2 서버 vs 클라이언트 Pawn 스폰 타이밍 분석

#### 서버 (Listen Server = 호스트)

```
[T+0] PostLogin(HostPC)
  → Super::PostLogin → RestartPlayer() → SpawnDefaultPawnAtTransform()
    → GetDefaultPawnClassForController() → BP 클래스 결정
    → SpawnActor<APawn>() ← 즉시 스폰됨
    → Possess(NewPawn)
      → PossessedBy() → InitAbilityActorInfo()
  → PostLogin 본문: WaitingRoom 체크

[T+0] ReceivedPlayer() → OnLevelEntered()
  → 대기실 감지 → bIsInWaitingRoom = true
  → RestoreDefaultInputMode() → UIOnly 모드
  → CreateWaitingRoomUI()
  → return (ViewTarget 복원 스킵)

[T+0.5] PostLogin 타이머
  → AssignPlayerToSlot(HostPC) → PositionPawnAtSlot(Pawn, 0)
    → Pawn->SetActorLocation(SlotLocation)     ← ★ 서버 Pawn 이동 OK
    → Pawn->SetActorRotation(FacingRotation)
    → MovementComp->DisableMovement()
  → ClientSetWaitingRoomView(Camera)
    → SetViewTargetWithBlend(Camera, 0.f)       ← ★ 서버 카메라 전환 OK
    → SetInputMode(UIOnly)

결과: 서버는 정상 동작 ✅
```

#### 클라이언트 (Non-Listen Server)

```
[T+0] PostLogin(ClientPC) ← 서버에서 실행!
  → Super::PostLogin → RestartPlayer() → SpawnDefaultPawnAtTransform()
    → SpawnActor<APawn>() ← ★ 서버에서 Pawn 스폰
    → Possess(ClientPawn)
      → PossessedBy() → InitAbilityActorInfo() (서버에서 실행)
  → PostLogin 본문: WaitingRoom 체크 → 타이머 설정

[T+?] 서버에서 스폰된 Pawn이 클라이언트에 리플리케이트됨
  → OnRep_PlayerState() → InitAbilityActorInfo() (클라이언트에서 실행)
    → HUD InitOverlay 등...

[T+?] 클라이언트에서 ReceivedPlayer() → OnLevelEntered()
  → 대기실 감지 → bIsInWaitingRoom = true
  → CreateWaitingRoomUI()
  → return

[T+0.5] PostLogin 타이머 (서버에서 실행!)
  → AssignPlayerToSlot(ClientPC)
    → PositionPawnAtSlot(ClientPawn, SlotIndex)
      → ClientPawn->SetActorLocation(SlotLocation)  ← ★ 서버에서 위치 설정
      → MovementComp->DisableMovement()
  → ClientSetWaitingRoomView(Camera)  ← ★ Client RPC → 클라이언트에서 실행
```

### 2.3 근본 원인 1: 클라이언트 카메라가 캐릭터 시점인 이유

**문제: `ClientSetWaitingRoomView` RPC가 도착하기 전에 클라이언트가 이미 Pawn 시점을 설정함**

클라이언트 측 타이밍:
```
[T+?] OnRep_PlayerState() → InitAbilityActorInfo()
  → HUD InitOverlay (여기서 WidgetController가 ASC에 바인딩)
  → 클라이언트는 자동으로 소유 Pawn을 ViewTarget으로 설정

[T+?] BeginPlay()
  → SetInputMode(GameOnly) ← ★ 게임 입력 모드로 설정됨!
  → bShowMouseCursor = false

[T+?] OnLevelEntered()
  → 대기실 감지 → RestoreDefaultInputMode() → UIOnly
  → CreateWaitingRoomUI()
  → return (ViewTarget을 건드리지 않음!)
```

**핵심 이슈:** `OnLevelEntered()`에서 `bIsInWaitingRoom = true`로 설정하고 `return`하지만, **ViewTarget을 카메라로 설정하지 않음!** ViewTarget 설정은 `ClientSetWaitingRoomView` RPC에 의존하는데, 이 RPC는 서버의 0.5초 타이머 후에 도착한다.

그 사이에:
1. Pawn이 리플리케이트되어 클라이언트의 ViewTarget이 Pawn으로 자동 설정됨
2. `BeginPlay()`가 `SetInputMode(GameOnly)`를 실행함 (이후 `RestoreDefaultInputMode`가 UIOnly로 변경하지만)
3. 카메라가 이미 Pawn에 붙어있으므로 1인칭 메시가 보임

**그러나 `ClientSetWaitingRoomView` RPC가 T+0.5에 도착하면 카메라가 전환되어야 한다.**

만약 클라이언트에서 카메라가 여전히 캐릭터 시점이라면, **`ClientSetWaitingRoomView` RPC가 클라이언트에 도달하지 않았거나, 도달했지만 효과가 없는 것이다.**

가능한 원인:
1. **WaitingRoomCamera가 null** → `if (!CameraActor) return;`에 의해 아무것도 안 함
2. **RPC 타이밍 문제**: PostLogin 시점에서 클라이언트의 네트워크 연결이 아직 완전하지 않아 Client RPC가 유실
3. **ViewTarget 경쟁**: `ClientSetWaitingRoomView`가 카메라로 설정한 직후, 다른 코드가 ViewTarget을 Pawn으로 되돌림

### 2.4 근본 원인 2: 클라이언트 Pawn이 슬롯 위치에 없는 이유

**`PositionPawnAtSlot()`은 서버에서만 실행된다.** 이것은 맞다 — 서버가 Pawn의 위치를 설정하면 `UCharacterMovementComponent`의 리플리케이션이 이 위치를 클라이언트에 전달해야 한다.

그런데 클라이언트에서 "캐릭터가 옆에 스폰"된다면:
1. **스폰 위치**: `AGameMode::RestartPlayer()`가 Pawn을 스폰할 때 `PlayerStart`나 기본 위치를 사용함
2. **PositionPawnAtSlot**이 0.5초 후에 텔레포트하는데, **`SetActorLocation()`으로 텔레포트 → `MovementComp->DisableMovement()`**
3. `DisableMovement()` 이후 `CharacterMovementComponent`가 더 이상 위치를 업데이트하지 않는다
4. **문제: `SetActorLocation()`은 서버에서 호출되지만, MovementComponent가 비활성화된 상태에서는 클라이언트에 위치가 리플리케이트되지 않을 수 있다**

`UCharacterMovementComponent`의 리플리케이션은 내부적으로 Movement가 활성화된 상태에서의 이동을 리플리케이트한다. `MOVE_None` 상태에서 `SetActorLocation()`을 직접 호출하면, Movement 리플리케이션 채널이 동작하지 않아 **클라이언트에 위치가 전달되지 않는다.**

**해결 방법:**
```cpp
void ADRLobbyGameMode::PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex)
{
    // ...

    // 텔레포트 (서버 → 클라이언트 리플리케이트)
    Pawn->TeleportTo(SlotActor->GetActorLocation(), FacingRotation);
    // 또는
    Pawn->SetActorLocationAndRotation(Location, Rotation, false, nullptr, ETeleportType::TeleportPhysics);

    // 이동 비활성화는 텔레포트 후에
    MovementComp->DisableMovement();

    // ★ 추가: 클라이언트에 즉시 위치 동기화 강제
    MovementComp->FlushServerMoves();  // 또는 ForceReplicationUpdate
}
```

### 2.5 근본 원인 3: 타겟 포인트에 메시가 안 보이는 이유

"타겟 포인트에 아무런 메시가 나오지 않는다"는 것은 **Pawn이 슬롯 위치로 텔레포트되지 않았다**는 의미다.

#### 서버에서도 안 보이는 이유

서버(호스트)의 경우:
```
[T+0.5] PostLogin 타이머 → AssignPlayerToSlot(HostPC)
  → PositionPawnAtSlot(HostPawn, 0)
    → HostPawn->SetActorLocation(SlotLocation)  ← 서버 Pawn 이동됨
```

그런데 서버에서도 타겟 포인트에 메시가 안 보인다면:
1. **카메라가 타겟 포인트를 안 비추고 있을 수 있다** — `WaitingRoomCamera`가 슬롯 위치를 향하지 않음
2. **메시가 OwnerNoSee** — `ADRCharacter`의 3인칭 메시는 `SetOwnerNoSee(true)`로 설정되어 있다:
   ```cpp
   // DRCharacter.cpp 생성자
   GetMesh()->SetOwnerNoSee(true);  // 3P 메시: 소유자에게 안 보임
   ```

   서버(호스트)가 자기 캐릭터를 보려면 3인칭 메시가 보여야 하는데, **`OwnerNoSee`가 true이므로 호스트 자신의 캐릭터는 3인칭 메시가 안 보인다!**

   이것은 1인칭 FPS 시점에서는 정상 동작이지만, 대기실의 고정 카메라에서는 문제가 된다.

3. **`UpdateMeshVisibility()`** — `BeginPlay()`에서 호출:
   ```cpp
   void ADRCharacter::UpdateMeshVisibility()
   {
       const bool bIsLocalPlayer = IsLocallyControlled();
       if (bIsLocalPlayer)
       {
           FirstPersonMesh->SetVisibility(true);   // 1P 보임
           GetMesh()->SetVisibility(false);         // 3P 안 보임
           Weapon->SetVisibility(false);
       }
       else
       {
           FirstPersonMesh->SetVisibility(false);   // 1P 안 보임
           GetMesh()->SetVisibility(true);           // 3P 보임
           Weapon->SetVisibility(true);
       }
   }
   ```

   **로컬 플레이어는 3P 메시가 `SetVisibility(false)` + `SetOwnerNoSee(true)` 이중 차단!**

   대기실에서 고정 카메라로 자기 캐릭터를 봐야 하지만, 3P 메시가 완전히 숨겨져 있다.

   다른 플레이어 캐릭터는:
   - `IsLocallyControlled()` = false → 3P 보임, 1P 안 보임
   - `OwnerNoSee` → 해당 없음 (다른 플레이어의 Pawn이므로)
   - → **다른 플레이어의 캐릭터는 보여야 한다**

   그런데 "서버와 클라이언트 모두" 타겟 포인트에 메시가 안 보인다면, **Pawn이 아예 슬롯 위치로 이동하지 않았을 가능성이 높다.**

#### 디버깅 포인트: AssignPlayerToSlot → PositionPawnAtSlot 체인

```cpp
void ADRLobbyGameMode::AssignPlayerToSlot(AController* Player)
{
    if (WaitingRoomSlots.Num() == 0) return;  // ★ 슬롯이 없으면 조기 리턴!

    int32 SlotIndex = NextAvailableSlot++;
    PlayerSlotMap.Add(Player, SlotIndex);

    if (APawn* Pawn = Player->GetPawn())  // ★ Pawn이 없으면 텔레포트 스킵!
    {
        PositionPawnAtSlot(Pawn, SlotIndex);
    }
}
```

**가능한 실패 조건:**
1. `WaitingRoomSlots.Num() == 0` → `FindWaitingRoomActors()`가 슬롯을 못 찾음
   - TargetPoint에 `"WaitingRoomSlot"` 태그가 없거나
   - `FindWaitingRoomActors()`가 `BeginPlay()` 후 아직 실행 안 됨
2. `Player->GetPawn()` = nullptr → Pawn이 아직 스폰 안 됨
   - PostLogin 0.5초 타이머 시점에서 클라이언트의 Pawn이 아직 없을 수 있음

```cpp
void ADRLobbyGameMode::PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex)
{
    if (!Pawn || WaitingRoomSlots.Num() == 0) return;

    int32 ClampedIndex = FMath::Clamp(SlotIndex, 0, WaitingRoomSlots.Num() - 1);
    AActor* SlotActor = WaitingRoomSlots[ClampedIndex];
    if (!SlotActor) return;

    Pawn->SetActorLocation(SlotActor->GetActorLocation());  // ★ 텔레포트

    if (WaitingRoomCamera)
    {
        FRotator FacingRotation = WaitingRoomCamera->GetCharacterFacingRotation();
        Pawn->SetActorRotation(FacingRotation);
    }

    // 이동 비활성화
    MovementComp->DisableMovement();  // ★ MOVE_None
}
```

### 2.6 문제 원인 종합

#### 문제 1: 캐릭터가 타겟 포인트에 안 보이는 이유 (서버 + 클라이언트)

1. **OwnerNoSee + SetVisibility(false)**: 로컬 플레이어의 3P 메시는 이중으로 숨겨짐. 고정 카메라에서는 3P 메시를 봐야 하므로 대기실 진입 시 `OwnerNoSee(false)`, `SetVisibility(true)` 해줘야 함.

2. **SetActorLocation 리플리케이션 문제**: 서버에서 `SetActorLocation()`으로 위치를 변경해도, `CharacterMovementComponent`가 `MOVE_None`이면 클라이언트에 위치가 리플리케이트되지 않을 수 있다.

3. **타이밍 이슈**: `AssignPlayerToSlot`이 호출될 때 `Player->GetPawn()`이 null이면 텔레포트가 스킵됨.

#### 문제 2: 클라이언트 카메라가 캐릭터 시점인 이유

1. **ClientSetWaitingRoomView RPC 도달 문제**: 서버의 0.5초 타이머에서 보낸 RPC가 클라이언트에 늦게 도착하거나, `WaitingRoomCamera`가 null이어서 RPC가 no-op.

2. **ViewTarget 경쟁**: Pawn의 `OnRep_PlayerState()` → `InitAbilityActorInfo()`가 클라이언트에서 ViewTarget을 Pawn으로 재설정.

3. **BeginPlay의 GameOnly 입력 모드**: `ADRPlayerController::BeginPlay()`가 `SetInputMode(GameOnly)`를 설정하고, `RestoreDefaultInputMode()`에서 UIOnly로 바꾸지만, 카메라 ViewTarget은 여전히 Pawn.

#### 문제 3: 1인칭 메시가 보이는 이유

`ADRCharacter::UpdateMeshVisibility()`에서 `IsLocallyControlled()` = true인 로컬 플레이어는 `FirstPersonMesh->SetVisibility(true)`. 대기실에서 고정 카메라를 사용하더라도 Pawn이 여전히 로컬 소유이므로 1P 메시가 보인다.

---

## 3. 해결 방안

### 3.1 대기실 진입 시 메시 가시성 전환

대기실에 진입하면 로컬 플레이어도 3인칭으로 전환:

```cpp
void ADRCharacter::SetWaitingRoomVisibility(bool bInWaitingRoom)
{
    if (bInWaitingRoom)
    {
        // 1P 메시 숨기기
        FirstPersonMesh->SetVisibility(false);
        // 3P 메시 보이기 (자기 자신도 3P로 보여야 함)
        GetMesh()->SetVisibility(true);
        GetMesh()->SetOwnerNoSee(false);  // ★ 소유자에게도 보이게
        if (Weapon) Weapon->SetVisibility(true);
    }
    else
    {
        // 일반 모드 복원
        UpdateMeshVisibility();
        GetMesh()->SetOwnerNoSee(true);
    }
}
```

호출 시점: `PositionPawnAtSlot()` 또는 `ClientSetWaitingRoomView`에서.

### 3.2 텔레포트를 리플리케이트되는 방식으로 변경

```cpp
void ADRLobbyGameMode::PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex)
{
    // ...

    // ★ TeleportTo 사용 (리플리케이션 지원)
    Pawn->TeleportTo(SlotActor->GetActorLocation(), FacingRotation);

    // 이동 비활성화
    if (UCharacterMovementComponent* MovementComp = ...)
    {
        MovementComp->DisableMovement();
    }
}
```

### 3.3 InitOverlay 대기실 체크

```cpp
// DRCharacter.cpp - InitAbilityActorInfo() 끝부분
if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(GetController()))
{
    // 대기실이면 HUD 오버레이 초기화 스킵
    if (!DRPC->bIsInWaitingRoom)
    {
        if (ADRHUD* DRHUD = Cast<ADRHUD>(DRPC->GetHUD()))
        {
            DRHUD->InitOverlay(DRPC, DRPlayerState, ASC, AttributeSets);
        }
    }
}
```

### 3.4 PostLogin에서 Pawn 확인 강화

```cpp
// PostLogin에서 AssignPlayerToSlot 호출 시 Pawn이 있는지 확인
// Pawn이 없으면 재시도 타이머 추가
```

---

## 4. 전체 타이밍 다이어그램 (이상적 플로우)

```
=== 서버 (호스트) ===
[T+0]   PostLogin → Pawn 스폰 → PossessedBy → InitAbilityActorInfo
                                                  → HUD InitOverlay? (대기실이면 스킵)
[T+0]   ReceivedPlayer → OnLevelEntered → 대기실 감지
                                          → bIsInWaitingRoom = true
                                          → CreateWaitingRoomUI
                                          → RestoreDefaultInputMode (UIOnly)
[T+0.5] 타이머 → AssignPlayerToSlot → PositionPawnAtSlot (TeleportTo)
                                     → SetWaitingRoomVisibility(true)
                → ClientSetWaitingRoomView → SetViewTarget(Camera)

=== 클라이언트 ===
[T+0]   서버에서 PostLogin 처리 (클라이언트는 모름)
[T+?]   Pawn 리플리케이트됨 → OnRep_PlayerState → InitAbilityActorInfo
                                                    → HUD InitOverlay? (대기실이면 스킵)
[T+?]   ReceivedPlayer → OnLevelEntered → 대기실 감지
                                          → bIsInWaitingRoom = true
                                          → CreateWaitingRoomUI
                                          → RestoreDefaultInputMode (UIOnly)
[T+0.5] Pawn 위치 리플리케이트 → 슬롯 위치로 이동
[T+0.5] ClientSetWaitingRoomView RPC 도착 → SetViewTarget(Camera)
                                           → SetWaitingRoomVisibility(true)
```

---

## 5. 요약: 수정이 필요한 파일

| 파일 | 수정 내용 |
|------|-----------|
| `DRCharacter.h/.cpp` | `SetWaitingRoomVisibility(bool)` 추가, `InitAbilityActorInfo()`에서 대기실 체크 |
| `DRLobbyGameMode.cpp` | `PositionPawnAtSlot()`에서 `TeleportTo()` 사용, 메시 가시성 전환 호출 |
| `DRPlayerController.cpp` | `ClientSetWaitingRoomView_Implementation()`에서 소유 Pawn 메시 가시성 전환 |
| `DRPlayerController.cpp` | `ClientStartCameraTransitionToCharacter_Implementation()`에서 메시 가시성 복원 |
| `DRHUD.cpp` | `InitOverlay()`에 중복 생성 방지 추가 (선택적) |
