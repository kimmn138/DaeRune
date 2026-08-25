# DaeRune 코드 분석 리포트 (Research)

- 분석일: 2026-07-04 (1차) / 같은 날 2차 전수 분석으로 보강
- 분석 브랜치: `feat/PlayExpo`
- 분석 범위: **`Source/DaeRune` 및 `Plugins/MultiplayerSessions`의 모든 .cpp/.h 전수 정독** (~31,400줄 + 플러그인 ~750줄). 태그 등록 파일(DRGameplayTags.cpp)과 모듈 보일러플레이트만 스킴. §1~6은 1차 분석(핵심 게임플레이), §7~8은 2차 전수 분석에서 추가된 발견.
- 표기: 🔴 심각(버그/치트 가능성/기능 파손) · 🟠 중요(성능·네트워크에 실질 영향) · 🟡 권장(품질·일관성)

---

## 1. 버그 및 잠재적 문제점

### 1-1. 🔴 치트 RPC에 아무런 가드가 없음 — 배포 빌드에서 누구나 페이즈 스킵 가능
`Source/DaeRune/Private/Player/DRPlayerController.cpp:409, 1249`

```cpp
void ADRPlayerController::CheatSkipToNextPhase()
{
    // 개발 빌드에서만 동작하도록 체크   ← 주석만 있고 실제 체크가 없음
    ServerCheatSkipToNextPhase();
}

void ADRPlayerController::ServerCheatSkipToNextPhase_Implementation()
{
    // GameMode 가져와서 바로 페이즈 전환. 권한/빌드 검증 전혀 없음
}
```

`Server, Reliable` RPC이므로 **어떤 클라이언트든 패킷만 보내면 서버에서 실행**된다. 주석의 의도("개발 빌드에서만")가 코드에 반영되어 있지 않다.

**권장 수정**:
- `_Implementation` 안에 `#if !UE_BUILD_SHIPPING` 가드 추가, 또는
- `WithValidation` + 개발 빌드 체크, 또는 UE의 `UCheatManager`로 이전 (CheatManager는 Shipping에서 자동 제외됨).

### 1-2. 🔴 부품 픽업 직후 사망 시 부품이 영구 소실될 수 있음
`Source/DaeRune/Private/Character/DRCharacterBase.cpp:99-105` + `Source/DaeRune/Private/Character/DRCharacter.cpp:273-287`

부패 상태 플레이어가 죽을 때 `Die()`가 `DropCarriedPart()`를 호출하는데, 이 함수는 **드롭 쿨다운(`PartDropCooldown`, 기본 2초)을 검사**한다:

```cpp
// DRCharacterBase::Die()
if (DRCharacter->IsCarryingPart())
{
    DRCharacter->DropCarriedPart();   // ← 쿨다운에 걸리면 그냥 return
}
```

부품을 주운 지 2초 안에 죽으면 드롭이 무시되고, 이후 캐릭터가 2.2초 뒤 `Destroy()`된다. `ADRCleanserPart`는 캐릭터 메시에 attach된 상태이므로 캐릭터와 함께 파괴되거나 허공에 남는다. 부품은 리스폰 로직이 없으므로 **페이즈 진행이 영구히 불가능**해질 수 있다.

**권장 수정**: 사망 경로에서는 이미 존재하는 `ForceDropCarriedPart()`(쿨다운 무시)를 호출.

### 1-3. 🔴 `FMath::RoundToFloat` 반환값 버림 — 반올림이 적용되지 않음
`Source/DaeRune/Private/AbilitySystem/DRPlayerAttributeSet.cpp:170`

```cpp
LocalIncomingDamage *= EliteDebuffModifier;
FMath::RoundToFloat(LocalIncomingDamage);   // ← 반환값을 대입하지 않아 no-op
```

`LocalIncomingDamage = FMath::RoundToFloat(LocalIncomingDamage);` 여야 한다.

### 1-4. 🔴 엘리트 디버프 1.5배 데미지 분기가 영구 데드코드
`Source/DaeRune/Private/AbilitySystem/DRPlayerAttributeSet.cpp:163`

```cpp
if (ADRPlayerState* PS = Cast<ADRPlayerState>(Props.TargetAvatarActor))
```

`Props.TargetAvatarActor`는 **아바타 액터(= ADRCharacter)** 이지 PlayerState가 아니다 (플레이어 ASC는 `InitAbilityActorInfo(DRPlayerState, this)`로 Owner=PS, Avatar=Character). 따라서 이 캐스트는 항상 실패하고 `Debuff_Elite` 1.5배 데미지는 절대 적용되지 않는다.

**권장 수정**: 엘리트 버프가 Roar 오라로 대체되었다면(주석에 그렇게 적혀 있음) 이 블록 자체를 삭제. 유지할 거라면 `Props.TargetCharacter->GetPlayerState<ADRPlayerState>()` 또는 `Props.TargetASC`에서 직접 태그 검사로 수정.

### 1-5. 🟠 벽 스턴 판정이 액터 "이름 문자열"에 의존
`Source/DaeRune/Private/Character/DREnemy.cpp:594-622`

```cpp
if (ActorName.Contains(TEXT("Floor"))) return;
if (!ActorName.Contains(TEXT("StaticMeshActor"))) return;
```

- 레벨 디자이너가 액터 이름을 바꾸거나(예: 한국어 이름, Merge된 메시), Packed Level Actor/HISM으로 교체하면 조용히 깨진다.
- CLAUDE.md에는 *"Hit actor tagged 'Wall'"* 이라고 문서화되어 있어 **코드와 문서가 불일치**한다.
- 부수적으로 `MinSpeedForStun` 기본값이 `50.f`인데 `meta=(ClampMin="100.0")`이라 에디터에서 이 프로퍼티를 건드리는 순간 100 미만으로 되돌릴 수 없다. (`DREnemy.h:221-222`)

**권장 수정**: 전용 오브젝트 채널 또는 `ActorHasTag("Wall")` 기반 판정으로 교체하고 ClampMin을 실제 기본값과 맞춤.

### 1-6. 🟠 호스트 판별이 `PlayerId == 0` 가정에 의존
`Source/DaeRune/Private/Game/DRGameStateBase.cpp:49-67`

UE의 `APlayerState::GetPlayerId()`는 0부터 시작한다는 보장이 없다 (엔진 내부 카운터 기반이고, seamless travel·재접속에 따라 달라질 수 있음). 폴백인 `PlayerArray[0]`도 배열 순서가 접속 순서를 보장하지 않는다. 대기실 UI의 "호스트 표시"와 `ServerRequestPowerOn` 흐름이 이 값에 의존하므로, 특정 상황에서 호스트가 잘못 표시되거나 준비 상태 로직이 꼬일 수 있다.

**권장 수정**: 서버에서 명시적인 `bIsHost` 플래그를 PlayerState에 세팅(리슨 서버라면 해당 PC가 `IsLocalController() && HasAuthority()`인 플레이어)하고 이를 복제.

### 1-7. 🟠 타이머 람다의 `[this]() { if (IsValid(this)) ... }` 패턴은 UB
`Source/DaeRune/Private/Player/DRPlayerController.cpp:860-866`, `DRPlayerController.cpp:1386-1410`, `Source/DaeRune/Private/Character/DRCharacterBase.cpp:138-149` 등

객체가 GC로 파괴된 뒤에는 `this` 포인터 자체가 dangling이므로 `IsValid(this)` 호출 자체가 미정의 동작이다(대부분 동작하는 것처럼 보이지만 보장이 없음). 같은 파일 안에서도 어떤 곳은 `TWeakObjectPtr` 캡처(안전), 어떤 곳은 raw `this` 캡처(위험)로 **패턴이 혼재**한다.

**권장 수정**: 전부 `[WeakThis = TWeakObjectPtr<T>(this)]` 패턴으로 통일. 타이머라면 `FTimerDelegate::CreateWeakLambda(this, ...)` 사용이 가장 깔끔.

### 1-8. 🟠 Phase3 종료 시 EliteBosses 정리 누락
`Source/DaeRune/Private/Phase/DRPhase3.cpp:94-135`

`OnPhaseEnd()`는 부모(`UDRPhaseBase::OnPhaseEnd`)가 `SpawnedEnemies`만 파괴/언바인드한다. `EliteBosses` 배열은 **파괴도, 사망 델리게이트(`OnEliteEnemyDeath`) 해제도 하지 않는다**. `SkipToNextWave()`에는 정리 코드가 있는데 정규 종료 경로에는 없어 비대칭이다. 게임오버로 끝나면 레벨 전환이라 실질 피해는 적지만, 승리 종료 이후 컨텐츠가 추가되면 엘리트가 잔존한다.

### 1-9. 🟠 여러 부품/사이트 오버랩 시 감지 상태가 꼬일 수 있음
`Source/DaeRune/Private/Player/DRPlayerController.cpp:72-97, 161-184`

`NearbyPart`/`NearbySite`가 **단일 포인터**라서:
1. 부품 A 오버랩 진입 → `NearbyPart = A`
2. 부품 B 오버랩 진입 → `NearbyPart = B` (A는 여전히 오버랩 중)
3. 부품 B 이탈 → `NearbyPart == B`이므로 감지 전체 OFF

→ 아직 A 위에 서 있는데도 감지가 꺼져 A를 주울 수 없게 된다. `ADRCharacter::RefreshNearbyInteractions()`가 일부 케이스를 재평가해 주지만 순수 이동에 의한 이탈은 커버하지 못한다.

**권장 수정**: `TSet<TWeakObjectPtr<>>`로 오버랩 집합을 관리하고, 집합이 비었을 때만 감지 OFF.

### 1-10. 🟡 ExecCalc 디버프 판정이 다중 데미지 타입에서 마지막 것만 남음
`Source/DaeRune/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp:15-45`

`DetermineDebuff`가 모든 데미지 타입을 루프 돌며 성공 시마다 **같은 EffectContext에 덮어쓴다**. 한 GE에 Fire+Lightning을 함께 실으면 마지막으로 성공한 타입의 디버프만 적용된다. 현재 컨텐츠가 단일 타입만 쓴다면 동작하지만, 구조적으로는 타입별 성공 목록을 담거나 "GE당 1타입" 규칙을 주석으로 명시해야 한다.

### 1-11. 🟡 `MakeWidgetControllerParams`의 널 체크 누락
`Source/DaeRune/Private/AbilitySystem/DRAbilitySystemLibrary.cpp:23-42`

```cpp
ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>();
UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();  // PS 널 체크 없음
```

클라이언트 초기화 초반(PlayerState 복제 전)에 위젯 쪽에서 호출되면 크래시. `if (!PS) return false;` 한 줄이면 된다.

### 1-12. 🟡 타이밍 기반 초기화(매직 딜레이)가 곳곳에 존재
- `DRStageGameMode::InitializePhaseSystem` — 페이즈 시작을 1.0초 지연 (`DRStageGameMode.cpp:257-270`)
- `DRPlayerController::InitOverlayForFreeRoam` — 어빌리티 아이콘 갱신 0.1초 지연 (`DRPlayerController.cpp:1656-1683`)
- `ClientStopSpectating`/`OnLevelEntered` — Pawn 없으면 0.5초 후 재시도
- `ClientStartCameraTransitionToCharacter` — 0.1초 × 50회 폴링

전부 "복제가 아직 안 끝났을 수 있으니 기다린다"를 시간으로 덮는 패턴이다. 로컬 테스트에선 통과해도 고핑/저사양 환경에서 재발한다. 가능하면 이벤트 기반(`OnRep_PlayerState`, `AbilitiesGivenDelegate`, `OnPossessedPawnChanged` 등)으로 전환 권장. 특히 페이즈 시작 1초 지연은 클라이언트 로딩이 1초를 넘으면 여전히 목표 UI를 놓친다 — `FPhaseObjectiveData`가 이미 복제 프로퍼티이므로 지연 없이 시작해도 늦게 온 클라이언트는 OnRep으로 따라잡는 구조가 맞다.

---

## 2. 네트워크 최적화

### 2-1. 🟠 상호작용 UI에 서버 왕복 + 전체 멀티캐스트 — 사실상 전부 로컬 처리 가능
`DRPlayerController.cpp:146-159, 225-235` + `DRCleanserPart.cpp:222-232` + `DRCleanserSite.cpp:378-387`

현재 흐름:
```
클라 라인트레이스 감지 → ServerNotifyLineTraceDetected (Server RPC)
→ Part->MulticastShowInteractionUI (NetMulticast, Reliable)
→ 모든 클라이언트가 수신, IsLocalController()인 1명만 위젯 토글
```

"내 화면의 '[E] 줍기' 위젯 표시"라는 **순수 로컬 코스메틱**에 서버 왕복 1회 + N명 전체 멀티캐스트가 쓰인다. 조준을 스치기만 해도 detected/lost 쌍이 발생하므로 부품이 흩어져 있으면 RPC가 꽤 잦다. 판정에 필요한 정보(`bIsCarried`, `CurrentState`, `InstalledPartsCount`, `IsCarryingPart()`)는 이미 전부 복제 프로퍼티라 클라이언트가 스스로 알 수 있다.

**권장 수정**: `ServerNotify*` / `MulticastShowInteractionUI` 4쌍을 전부 제거하고, 라인트레이스 결과에 따라 로컬에서 `InteractionWidget->SetVisibility()` 직접 호출. 트래픽 제거 + UI 반응 지연(왕복 RTT)도 사라진다.

### 2-2. 🟠 `ADRPlayerState::NetUpdateFrequency = 100`은 과도
`Source/DaeRune/Private/Player/DRPlayerState.cpp:24`

PlayerState가 ASC/AttributeSet의 복제 채널인 구조에서 100Hz 갱신 후보는 대역폭을 크게 늘린다(어트리뷰트 변화가 잦은 전투 중 특히). 기본 PlayerState는 훨씬 낮고, GAS 게임도 보통 30~66Hz면 충분하다.

**권장**: `NetUpdateFrequency = 30~50`, `MinNetUpdateFrequency = 5~10`으로 낮추고 체감을 확인. (Health는 서버 권위 + UI 표시용이라 100Hz가 필요 없다.)

### 2-3. 🟠 WaterPump 빔 끝점: 풀정밀도 FVector를 서버 틱마다 복제
`Source/DaeRune/Public/Character/DRCharacter.h:115-116` + `DRWaterPump.cpp:366-370`

- `WaterPumpBeamEndPoint`가 일반 `FVector`(풀정밀도)로 복제된다. VFX 끝점은 `FVector_NetQuantize10`이면 충분 (자판기 사운드 RPC는 이미 `FVector_NetQuantize`를 쓰고 있어 **자체 코드 내 일관성도 어긋남**).
- 채널링 중 매 서버 틱마다 프로퍼티가 dirty가 된다. 끝점이 일정 거리 이상 움직였을 때만 대입하는 임계값(예: 25uu)을 두면 정지 조준 시 트래픽이 0이 된다.
- `COND_SkipOwner`는 이미 잘 적용되어 있음.

### 2-4. 🟠 적 회전 복제: FRotator 전체를 복제하지만 Yaw만 사용
`DREnemy.cpp:76-99` + `DREnemy.h:274-275`

```cpp
ReplicatedTargetRotation = CurrentControlRot;   // Pitch/Roll 포함 복제
...
SetActorRotation(FRotator(0.f, NewRotation.Yaw, 0.f));  // Yaw만 사용
```

- Yaw 1개(`uint16` 압축 또는 float)만 복제하면 복제량이 1/3로 줄어든다. 최대 100마리 스폰 구조에서 유의미하다.
- 또한 서버에서 `SetActorRotation`을 하고 있으므로 `ReplicatedMovement`의 Rotation으로도 회전이 이미 복제된다. 커스텀 보간이 목적이라면 둘 중 하나만 쓰도록 정리해야 **이중 복제**를 피한다.

### 2-5. 🟠 웨이브 남은 시간을 1초마다 float 복제
`DRPhase3.cpp:289-306` + `DRStageGameState.cpp:179-197`

`WaveTimeRemaining`을 1초 주기 타이머로 깎아 `SetWaveRemainingTime()` → 복제. 표준적인 대안은 **종료 시각 1회 복제**:

```cpp
// 서버: 웨이브 시작 시 1회
WaveEndServerTime = GS->GetServerWorldTimeSeconds() + PlayDuration;  // Replicated
// 클라: 매 프레임 로컬 계산
Remaining = WaveEndServerTime - GS->GetServerWorldTimeSeconds();
```

트래픽이 초당 1회 → 웨이브당 1회로 줄고, 복제 지연에 의한 타이머 표시 튐도 사라진다.

### 2-6. 🟡 코스메틱 멀티캐스트가 전부 Reliable
`DRStageGameState.h:163-210` (사운드 5종 + VFX 4종), `DRCleanserSite`의 사운드 RPC 등

Reliable 멀티캐스트는 유실 시 재전송 큐에 쌓여 대역폭 스파이크 시 다른 reliable 트래픽(어빌리티 활성화 등)을 지연시킨다. 반면 `DRCharacter.h:176-194`의 자판기 사운드 RPC들은 Reliable/Unreliable을 사유와 함께 구분해 놓았다(좋은 예). 같은 기준으로:
- 유지(Reliable 적절): 게임오버/클리어 사운드, 페이즈 시작(1회성·중요)
- Unreliable 전환 후보: `Multicast_PlayWaveStartSound`, `Multicast_PlayPoisonGasWarningSound`(주기 반복), 스폰 포인트 VFX 갱신류

또 하나 — 상태성 코스메틱(스폰 포인트 VFX, 독가스 루프 사운드)은 **레이트 조인/relevancy 회복 시 멀티캐스트를 못 받은 클라이언트에서 영구히 어긋난다**. RPC 대신 `bIsToxicGasWave`처럼 복제 프로퍼티 + OnRep으로 바꾸면 유실·조인 문제를 동시에 해결한다.

### 2-7. 🟡 `Multicast_ActivateEnemySpawnPointVFX`가 FTransform 배열 + 에셋 포인터를 전송
`DRStageGameState.cpp:428-462`

- `FTransform`은 개당 ~40바이트(쿼터니언+스케일 포함)인데 실제 사용은 위치/회전뿐이고 스케일은 `FVector(1.f)` 고정. `FVector_NetQuantize` 배열이면 충분.
- 스폰 포인트는 레벨에 박힌 정적 액터이므로 애초에 **클라이언트도 태그 검색으로 스스로 찾을 수 있다**. "VFX 켜라/꺼라" bool 복제 프로퍼티 하나면 충분하다.

### 2-8. 🟡 데미지 숫자 RPC가 Client-Reliable + 매번 컴포넌트 생성
`DRPlayerController.h:44-45` + `DRPlayerController.cpp:54-70`

- 전투 중 매 피격마다 reliable RPC. 코스메틱이므로 `Client, Unreliable`이 안전하다 (한두 개 유실돼도 무방).
- 수신 측에서 매번 `NewObject<UDamageTextComponent>` + `RegisterComponent` — 다수 몬스터 동시 타격(AoE) 시 스파이크. 컴포넌트 풀 또는 동시 표시 상한 권장.

### 2-9. 🟡 CleanserSite ASC가 상시 생성·복제됨
`DRCleanserSite.cpp:67-71`

CLAUDE.md에는 "Phase 3에서 ASC를 얻는다"고 되어 있으나 실제로는 생성자에서 항상 만들어지고 복제도 켜져 있다. 사이트가 1개뿐이라 부담은 작지만 문서와 다르고, Phase1 동안은 불필요한 복제 채널이 열려 있다. 유지한다면 `Mixed` 대신 `Minimal` 복제 모드 검토 + 문서 갱신.

### 2-10. 🟡 기타
- `ADREnemy::NetPriority = 3.0f` (`DREnemy.cpp:34`) — 폰 기본값 수준으로 이미 높다. 적 100마리가 전부 플레이어와 동순위로 경쟁하므로 오히려 낮추고(1.5~2) 거리 기반 우선순위에 맡기는 편이 대역폭 포화 시 플레이어 캐릭터 갱신을 보호한다.
- 레거시 복제 프로퍼티: `CleanserHealth`, `BossHealth`, `RemainingEnemiesInArea`, `bCleanserAreaSecured` (`DRStageGameState.h:253-291`)는 새 2페이즈 구조에서 세터가 호출되지 않거나 사용처가 없다. 복제 목록에서 제거(§5-1 참고).

---

## 3. 성능 최적화

### 3-1. 🟠 AI 디버그 로그가 상시 활성 — 적 1마리당 1초마다 문자열 빌드 + Warning 로그
`Source/DaeRune/Private/AI/DRAIController.cpp:109-151`

```cpp
// BeginPlay: [DEBUG] 매 1초마다 AI 상태 로깅
GetWorld()->GetTimerManager().SetTimer(DebugStateLogTimerHandle, this,
    &ADRAIController::DebugStateLog, 1.0f, true, 1.0f);
```

`DebugStateLog`는 `BehaviorTreeComponent->DescribeActiveTasks()`(BT 순회 + 문자열 조립)와 여러 `FString` 생성을 수행한다. 웨이브 중 적 50~100마리면 **초당 50~100회의 문자열 조립 + 로그 I/O**가 스테이지 내내 돈다. Shipping 가드도 없다.

**권장**: `#if !UE_BUILD_SHIPPING` + CVar 토글로 감싸거나 제거. 같은 계열의 스폰 로그 스팸도 정리:
- `DREnemy.cpp:190` (`BT Started` — 스폰마다)
- `DRPhase3.cpp:729` (`Spawned %s from point...` — 스폰마다)

### 3-2. 🟠 `Look()`이 마우스 입력마다 Subsystem 체인을 조회
`DRPlayerController.cpp:895-918`

마우스를 움직이는 매 프레임마다 `GetGameInstance() → GetSubsystem<UDRSettingsManager>() → GetSettings() → MouseSensitivity`를 탄다. 감도를 멤버에 캐시하고 설정 변경 시 델리게이트로 갱신할 것.

### 3-3. 🟠 레벨 판별 함수가 호출마다 맵 이름 문자열 연산
`DRPlayerController.cpp:1070-1112`

`IsInMainMenu/IsInLobby/IsInGameLevel/IsInTutorial`이 매번 `GetMapName()` 복사 + `RemoveFromStart` + `Contains`를 수행한다. 이 함수들은 `CanShowCharacterInfo()`를 통해 **PlayerTick에서 매 프레임** 호출될 수 있다 (`DRPlayerController.cpp:464`의 Tab Hold 안전장치 경로).

**권장**: `OnLevelEntered()`에서 1회 판별해 enum으로 캐시. 부수적으로 `IsInGameLevel()`의 `Contains(TEXT("Stage1"))` 하드코딩은 Stage2가 생기는 순간 깨진다 — GameMode/GameState 타입 검사로 바꾸는 것이 안전하다.

### 3-4. 🟡 라인트레이스마다 `FindComponentByClass<UCameraComponent>()`
`DRPlayerController.cpp:109, 194`

`ADRCharacter::GetFollowCamera()` 접근자가 이미 있으므로 그대로 쓰면 컴포넌트 배열 순회가 사라진다. 0.1초 주기라 크리티컬하진 않지만 공짜 개선.

### 3-5. 🟡 적 Tick이 항상 돈다
`DREnemy.cpp:26-99`

- 회전 보간만을 위해 서버+모든 클라에서 매 프레임 Tick. 적 100마리 기준 무시 못 할 비용. `PrimaryActorTick.TickInterval` 조정 또는 어그로 없는 동안 Tick 게이팅 권장.
- `bIsTutorialDummy`는 매 틱 early-return 대신 아예 `SetActorTickEnabled(false)`가 낫다.
- `AIPerception`이 시야 360°/반경 2000/MaxAge 3초로 전 적에 붙어 있다. 적 수가 많은 웨이브에서 비용이 커지면 갱신 주기/반경 튜닝 여지가 있다.

### 3-6. 🟡 웨이브 스폰 틱마다 DataTable 조회 + 정규화 재수행
`DRPhase3.cpp:673-867`

`SpawnMonsterByCycle()`(스폰 주기마다 호출)이 매번 `GetWaveLevelModifier()`를 부르고, 이 함수는 `FString::Printf` → `FName` 변환 → `FindRow` → `NormalizeModifier`(배열 재구성)를 전부 다시 한다. 웨이브 시작 시 1회 계산해 멤버에 캐시하면 된다. `GetWaveData`도 동일.

### 3-7. 🟡 플레이어 목록이 필요한 곳의 `GetAllActorsOfClass`
`DREnemy.cpp:706-793`(보스 물 지급), `DRArmadilloEnemy.cpp:187`, `DREliteRoar.cpp:57`

`GameState->PlayerArray` 순회(또는 기존 `GetAlivePlayers()`)로 대체하면 월드 전체 액터 순회가 사라진다. 공용 헬퍼 하나로 통일 권장.

### 3-8. 🟡 `SetOverheadWidgetVisibility`가 호출마다 컴포넌트 검색
`DRCharacter.cpp:461-474`

`GetComponents<UWidgetComponent>()`를 매번 수행. BeginPlay에서 1회 수집해 캐시.

---

## 4. 의도와 다르거나 일관성이 없는 부분

### 4-1. 🟠 CLAUDE.md(프로젝트 문서)와 실제 코드의 대규모 드리프트
문서가 설명하는 구조와 현재 코드가 크게 다르다. 이 저장소는 Claude Code가 문서를 신뢰하고 작업하므로 실질적인 위험 요소다:

| CLAUDE.md 기술 | 실제 코드 |
|---|---|
| 4페이즈 구조 (Phase1 확보 → Phase2 부품 → Phase3 방어) | **2페이즈** (case 0: 확보+부품 통합, case 1: 방어 = 구 Phase3) — `DRStageGameMode.cpp:369-392` |
| "2 of 3 sites randomly activated" | 사이트 **1개 강제 유지, 나머지 Destroy** — `DRPhase1::KeepSingleCleanserSite` |
| "Defend sites for 5 minutes" | 5웨이브 클리어 방식 |
| "Dropped on death with physics impulse" | 지면 라인트레이스 후 +80uu 지점에 스폰, 임펄스 없음 — `DREnemy::DropPart` |
| "CleanserSites gain ASC in Phase 3" | ASC는 생성자에서 상시 생성 — `DRCleanserSite.cpp:67` |
| "wave levels increase when site health drops below 50%" | 해당 로직 주석 처리로 비활성 — `DRPhase3.cpp:1119-1130` |
| ECharacterClass: Elementalist/Warrior/Ranger | 플레이어는 별도의 `EPlayerCharacterClass`(Gardener/VendingMachineRobot) 체계 사용 |
| 브랜치: `feat/Phase3` | 현재 `feat/PlayExpo` |

**권장**: CLAUDE.md 현행화. 특히 페이즈 구조/사이트 개수/클래스 체계.

### 4-2. 🟠 "Phase3" 클래스가 실제로는 2번째 페이즈, Phase2는 미사용 레거시
- `UDRPhase3`가 `SetupPhaseObjective(2)`를 호출하고 주석으로 "기존 Phase3가 새 Phase2 역할"이라고 적어 놓았다 (`DRPhase3.cpp:41-42`).
- `UDRPhase2`(`DRPhase2.cpp`)는 새 구조에서 사용되지 않는 것으로 보이며(부품 수집이 Phase1로 통합), `[임시]` 주석과 통째로 주석 처리된 스폰 루프가 남아 있다.
- `ValidatePhaseCompletion()`의 case 번호(0,1) / 클래스 이름(Phase1, Phase3) / 목표 테이블 행 이름(Phase1, Phase2)이 서로 다른 번호 체계 — 새로 온 사람이 반드시 헷갈리는 지점.

**권장**: 의미 기반 리네이밍(`UDRPhaseSecure`, `UDRPhaseDefense` 등)과 `UDRPhase2` 삭제(또는 Deprecated 이동).

### 4-3. 🟠 한글 주석 인코딩 파손(mojibake)이 광범위
`DRPlayerController.cpp`, `DRCharacterBase.cpp`, `DRStageGameMode.cpp`, `DRPhase2.cpp`, `DRPlayerAttributeSet.cpp` 등 다수 파일에서 한글 주석이 `占쏙옙`, `?쒕쾭`, `罹먮┃??` 형태로 깨져 있다. CP949 ↔ UTF-8 재저장이 반복되며 생긴 것으로 보이고, 같은 파일 안에 정상 한글과 깨진 한글이 공존한다.

**권장**: ① 전체 소스 UTF-8(BOM) 통일, ② `.editorconfig`에 `charset = utf-8-bom` 지정, ③ 깨진 주석은 복구 불가능하므로 재작성 또는 삭제. 방치하면 주석이 문서 역할을 못 하고 diff 노이즈만 만든다.

### 4-4. 🟡 `State.Carrying` 태그 토글이 4곳에 분산
- 추가: `ADRCleanserPart::PickupPart` (서버) — `DRCleanserPart.cpp:96-100`
- 제거: `ADRCleanserPart::InstallPart` (서버) — `DRCleanserPart.cpp:156-164`
- 제거: `ADRCharacter::DoDropCarriedPart` (서버) — `DRCharacter.cpp:303-307`
- 추가/제거: `ADRCharacter::OnRep_bIsCarryingPart` (클라) — `DRCharacter.cpp:740-761`

서버 측 추가/제거가 부품 액터와 캐릭터에 나뉘어 있어, 새로운 획득/상실 경로가 생길 때 한쪽을 누락하기 쉽다. **`ADRCharacter::SetCarryingState(bool)` 하나로 모으고**(태그 + `bIsCarryingPart` + 비주얼 + RefreshNearbyInteractions), Part 쪽은 그것만 호출하게 정리 권장. `AddReplicatedLooseGameplayTag`를 쓰면 OnRep 수동 동기화 자체가 필요 없어진다는 점도 검토 가치가 있다.

### 4-5. 🟡 Part/Site 상호작용 코드가 완전한 복붙 이중화
`SetPartDetectionEnabled`/`SetSiteDetectionEnabled`, `FindPartByLineTrace`/`FindSiteByLineTrace`, `ServerNotify*` 4개, `RefreshOverlapStateFor`(Part/Site 각각), `MulticastShowInteractionUI`(Part/Site 각각) — 구조가 동일한 코드가 두 벌이다. `IDRInteractable` 인터페이스(감지 가능 여부 + 위젯 토글)로 일반화하면 코드 절반이 사라지고, §2-1(로컬 처리 전환)과 함께 하면 더 단순해진다.

### 4-6. 🟡 정책 비일관 모음
- **RPC 신뢰도**: `DRCharacter` 사운드 RPC는 근거 주석과 함께 Reliable/Unreliable 구분(모범) ↔ `DRStageGameState`는 전부 Reliable(무정책).
- **로그**: 커스텀 채널 `LogDR`이 있는데(`DRLogChannels.h`) 대부분 `LogTemp, Warning` 사용.
- **포인터**: `TObjectPtr`와 raw 포인터(`TArray<ADRCleanserSite*> CleanserSites` — `DRStageGameState.h:236`) 혼용.
- **죽음 처리**: `ADRCharacterBase::Die()`가 플레이어는 "corrupt 상태일 때만" 실제 사망 처리하고 비-corrupt면 조용히 아무 것도 안 한다. 의도된 디자인(비-corrupt는 corrupt로 전환)이라면 진입부에 명시적 early-return과 주석으로 표현하는 편이 안전하다.

### 4-7. 🟡 `WeaponRange` 하드코딩 불일치 (3P 빔 길이 왜곡 가능)
`DRCharacter.cpp:717`

```cpp
const float WeaponRange = 1000.0f; // 예시값, 네 실제 값으로 바꿔
```

1P 빔은 `UDRWaterPump::WeaponRange`(어빌리티 프로퍼티)로 정규화하는데 3P 빔은 하드코딩 1000이다. 어빌리티의 실제 값이 1000이 아니면 타 플레이어 시점의 빔 길이 스케일이 어긋난다. 캐릭터 프로퍼티로 승격해 양쪽이 같은 값을 쓰게 하거나 어빌리티 CDO에서 읽을 것. (임시 주석이 그대로 남아 있는 것 자체도 정리 대상.)

---

## 5. 더 깔끔하게 만들 수 있는 부분 (리팩토링/정리)

### 5-1. 죽은 코드/레거시 정리
| 위치 | 내용 |
|---|---|
| `DREnemy.cpp:309-332` | `ReduceWaterReward()` 본문 전체 주석 처리 — 기능을 살릴지 결정하고 삭제/복원 |
| `DRPhase2.cpp` 전체 | 새 구조에서 미사용 추정 + 주석 처리된 스폰 루프 |
| `DRPhase3.cpp:930-949` | `GetDefaultWaveData()` — switch의 모든 case가 비어 있어 항상 기본 구조체 반환. 의미 없음 |
| `DRPhase3.cpp:588-594, 815-817` | 빈 if/else 블록 (`if (Num() < 4) { }` 등) — 경고 로그를 넣거나 삭제 |
| `DRCharacter.cpp:566-577` | 주석 처리된 포인트라이트 코드 |
| `DRStageGameState.h` | `BossHealth`, `bCleanserAreaSecured`, `RemainingEnemiesInArea`, `CleanserHealth` — 미사용 복제 프로퍼티 (§2-10) |
| `DRPlayerController.h:447-448` | `DRAbilitySystemComponent` 캐시 필드가 선언만 되고 `GetASC()`는 매번 새로 조회 — 캐싱을 구현하거나 필드 삭제 |
| `DRPlayerAttributeSet.cpp:299-314` | `ApplyHitReactAndKnockback`의 디버깅 잔해 (빈 for 루프, 빈 else 블록) |
| `DRAbilitySystemComponent.cpp:260-264` | `RegisterAbilityTagEvents()` — 자칭 no-op 함수 |

### 5-2. 매직 넘버 상수화
- 웨이브 수 `5`가 `DRPhase3.cpp`에 최소 4곳 하드코딩 (`> 5`, `>= 5`) — `GameState->GetTotalWaves()` 또는 `static constexpr int32 MaxWaves`로 통일.
- 클래스 개수 `constexpr int32 ClassCount = 2` (`DRPlayerController.cpp:1303`) — `EPlayerCharacterClass`에 `Count` 항목 또는 UEnum 기반 계산.
- `UpdateWaterMeshScale`의 `9.0f` 오프셋 계수, 몬스터 타입 `1/2/3` 매핑(`SelectMonsterClassByType`) — enum/프로퍼티화.
- 동시 스폰 수 `4` (`SpawnMonsterByCycle` 배치 크기, `EnemySpawnPoints.Num() < 4` 등) — 상수로.

### 5-3. 소소한 코드 개선
- `ADRCharacterBase::GetTaggedMontageByTag_Implementation` (`DRCharacterBase.cpp:351-361`): `for (FTaggedMontage TaggedMontage : ...)` — 값 복사 루프. `const FTaggedMontage&`로.
- `IsAbilityInputBlocked`의 `const_cast` (`DRPlayerController.cpp:1227`): `GetASC()`를 `const` 멤버로 만들면 해소.
- `UDRPhase3::OnPhaseEnd`와 `BeginDestroy`의 타이머 6종 해제가 복붙 — `ClearAllPhaseTimers()` 헬퍼로.
- `DRStageGameState`의 사운드 재생 함수 5개가 "AssetManager → SoundData → 널체크 → PlaySound2D" 보일러플레이트 반복 — 공용 헬퍼 하나로.
- `ClientStopSpectating`, `OnLevelEntered` 등의 들여쓰기 붕괴 구간 — 포맷터 일괄 적용.
- `UDRPhase3::InitializeCleanserSite`가 `SiteASC->InitAbilityActorInfo(Site, Site)`를 다시 호출 — `ADRCleanserSite::BeginPlay`에서 이미 수행. 중복.
- `OnRep_IsReady`의 `World->GetFirstPlayerController()` (`DRPlayerState.cpp:373`)와 `OnRep_WaitingRoomSlotIndex`의 `GetOwner()` 캐스트 — 같은 목적(로컬 대기실 UI 갱신)에 서로 다른 접근. 헬퍼로 통일.
- `ADRPlayerController::ServerToggleReady`의 `if (HasAuthority() && IsLocalController()) return;` — Server RPC 안에서 `HasAuthority()`는 항상 true이므로 사실상 `IsLocalController()` 검사다. 의도(리슨 서버 호스트 제외)를 `IsListenServerHost()` 같은 명시적 함수로.

### 5-4. 구조적 제안 (여유 있을 때)
1. **상호작용 시스템 통합** (§2-1 + §4-5): 감지·UI를 전부 클라이언트 로컬로 옮기고 `IDRInteractable`로 Part/Site 통합. 서버에는 `ServerRequestInteract(AActor*)` 하나만 남긴다. 서버는 요청 수신 시 **거리/상태 검증**을 수행 — 현재는 클라이언트가 보낸 Part 포인터를 거리 검증 없이 신뢰하므로(`ServerRequestPickupPart`) 맵 반대편 부품을 줍는 치트가 이론상 가능하다.
2. **페이즈를 데이터 주도로**: `ValidatePhaseCompletion()`의 switch-case(인덱스 하드코딩) 대신 각 `UDRPhaseBase` 파생이 `virtual bool IsCompleted() const`를 구현하면 페이즈 추가/재배열 시 GameMode 수정이 불필요해진다.
3. **초기화 타이밍을 이벤트 기반으로** (§1-12): "N초 기다렸다가 시도" 패턴을 델리게이트 기반으로 교체.

---

## 6. 잘 되어 있는 부분 (유지 권장)

- `TWeakObjectPtr` 기반 스폰 적 추적, 페이즈 종료 시 델리게이트 언바인드 습관 (일부 누락 제외).
- `UDRAbilitySystemComponent`의 InputTag→AbilitySpec 캐시, 정적 태그 캐싱 (`GetAbilityTagFromSpec`).
- 자판기 사운드 RPC의 Reliable/Unreliable 구분과 근거 주석 (`DRCharacter.h:176-194`) — 프로젝트 전체 표준으로 확장할 만함.
- `ADRPlayerState`의 HealthRegen Spec 캐싱 (`CachedHealthRegenSpec`) — GE 스펙 재생성 비용 절약.
- `DREnemy`의 ASC `Minimal` 복제 모드, `WaveOutlineLevel`의 `COND_InitialOnly`, `WaterPumpBeamEndPoint`의 `COND_SkipOwner` — 조건부 복제를 의식적으로 사용.
- `GameBalanceConfig` 데이터 에셋으로 밸런스 값 중앙화.
- 사망 몽타주의 멀티캐스트+RepNotify 이중 안전망과 중복 재생 방지 플래그.
- 리슨 서버에서 OnRep이 안 불리는 케이스(수동 OnRep 호출)를 일관되게 챙기고 있음.

---

## 7. Plugins/MultiplayerSessions 분석 (2차)

플러그인의 실소스는 `MultiplayerSessionsSubsystem.h/.cpp`, `Menu.h/.cpp`, 모듈 파일뿐이다 (Intermediate는 생성물). 룸코드 기반 세션 생성/검색/참가 구조 자체는 견고하게 짜여 있다 (Destroy 후 보류 재시도, 초대 큐잉, 데드락 방지 등). 발견된 문제:

### 7-1. 🔴 `HandleNetworkFailure`에서 `SessionInterface` 널 체크 없이 사용
`MultiplayerSessionsSubsystem.cpp:416-431`

```cpp
void UMultiplayerSessionsSubsystem::HandleNetworkFailure(...)
{
    StopVoiceChat();
    SessionInterface->DestroySession(NAME_GameSession);   // ← IsValid() 검사 없음
```

`Initialize()`에서 OnlineSubsystem이 없으면 `SessionInterface`가 설정되지 않는데, 네트워크 실패 델리게이트는 GEngine에 무조건 바인딩된다. 널 상태에서 네트워크 실패가 발생하면 크래시. 그 아래에는 "crash 발생해서 주석 처리했다"는 메모와 함께 죽은 Travel 코드 + 빈 if 블록이 남아 있다 — 근본 원인을 안 고치고 증상만 덮은 흔적이므로 정리 필요.

### 7-2. 🟠 로컬 플레이어 널 체크 누락
`MultiplayerSessionsSubsystem.cpp:113-114, 141-142, 465-466, 496-497`

`GetWorld()->GetFirstLocalPlayerFromController()` 결과를 널 체크 없이 `LocalPlayer->GetPreferredUniqueNetId()`로 deref한다. 4곳 동일. 레벨 전환 타이밍에 호출되면 크래시 가능. (`TryProcessPendingInvite`는 널 체크를 하고 있어 같은 파일 안에서도 비일관.)

### 7-3. 🟠 룸코드 충돌 검사 루프에 재시도 상한 없음
`MultiplayerSessionsSubsystem.cpp:271-289`

`OnFindSessionsComplete`에서 중복 룸코드가 발견되면 새 코드를 생성해 다시 `ValidateAndCreateSessionWithCode()` → 다시 검색. 검색이 (버그·스팀 이상 동작으로) 계속 결과를 반환하면 **무한 검색 루프**가 된다. 재시도 카운터(예: 5회)를 두고 실패 브로드캐스트로 탈출해야 한다.

### 7-4. 🟠 `Deinitialize`에서 초대 델리게이트 해제 누락
`MultiplayerSessionsSubsystem.cpp:44-55`

`Initialize`에서 `AddOnSessionUserInviteAcceptedDelegate_Handle`로 등록한 핸들을 `Deinitialize`에서 `Clear...`하지 않는다. GameInstance 수명이라 실질 피해는 작지만, OnlineSubsystem 쪽 델리게이트 리스트에 무효 엔트리가 남는다.

### 7-5. 🟡 기타
- `LastNumPublicConnections`가 초기화되지 않은 멤버 (`MultiplayerSessionsSubsystem.h:116`). 현재 호출 순서상 문제는 없지만 `{0}` 초기화 권장.
- `LeaveServer`: 호스트 경로가 `DestroySession()` 완료를 기다리지 않고 즉시 `ClientTravel` — 파괴 콜백이 떠나는 월드 기준으로 발화할 수 있다. 클라 경로도 Travel 후 파괴 순서. 세션 정리→여행 순서를 콜백 기반으로 표준화 권장.
- `Menu::JoinButtonClicked`: 룸코드 형식 검증 실패 시 조용히 return — 사용자 피드백(에러 텍스트) 없음. `OnFindSessions`의 `bFoundRoom` 변수는 루프 내 return 때문에 사실상 죽은 코드.
- `StartSession()` / `OnStartSessionComplete()` 빈 구현 — 사용하지 않으면 삭제.
- 이 플러그인도 한글 주석 mojibake가 광범위 (§4-3과 동일 이슈).

---

## 8. 2차 전수 분석 추가 발견 (Source 나머지 전체)

### 8-1. 🔴 OverlayWidgetController의 페이즈 인덱스 `== 2` 하드코딩 — 새 2페이즈 구조에서 방어 UI 바인딩이 통째로 어긋날 수 있음
`OverlayWidgetController.cpp:34, 430-446, 452-459`

- `BroadcastInitialValues`: `GetCurrentPhaseIndex() == 2`일 때만 Phase3 타이머 표시
- `CheckAndBindWaveTimer`: 페이즈 인덱스가 2일 때만 웨이브 타이머 델리게이트 바인딩
- `OnPhaseChanged`: 인덱스 2일 때만 클렌저사이트 체력 바인딩(`BindCallbacksSiteToDependencies`) + 타이머 가시성

`ValidatePhaseCompletion()`은 case 0/1의 2페이즈 체계인데(§4-2), 이 컨트롤러는 옛 3페이즈 체계(방어=인덱스 2)를 기준으로 한다. `BP_DRStageGameMode`의 `PhaseClasses` 배열이 2개로 줄어 있다면 **웨이브 타이머 UI, 독가스/클렌저 HP UI가 전부 바인딩되지 않는다**. 배열이 아직 3개(더미 포함)라면 반대로 GameMode 쪽 switch가 맞지 않는다 — 어느 쪽이든 두 파일 중 하나는 틀린 상태다.

페이즈 알람 텍스트도 옛 구조의 하드코딩("페이즈 1: 확보 / 2: 수집 / 3: 방어", `OnPhaseChanged:464-478`).

**권장**: 인덱스 비교 대신 GameState에 `bIsDefensePhase`(또는 페이즈 타입 enum) 복제 프로퍼티를 두고 UI는 그것만 본다. 알람 텍스트는 `DT_PhaseObjective`(이미 존재)에서 가져온다.

### 8-2. 🔴 `ADRPoisonGasActor::OverlapCountMap`이 클래스 static 전역 상태
`DRPoisonGasActor.cpp:11`

```cpp
TMap<TWeakObjectPtr<AActor>, int32> ADRPoisonGasActor::OverlapCountMap;  // static!
```

- 월드/PIE 세션이 끝나도 절대 초기화되지 않는다. 가스 위에서 죽거나 파괴된 액터는 EndOverlap이 오지 않아 **stale 엔트리 + 카운트 잔존**.
- 잔존 카운트가 있는 채로 다음 게임에서 같은(재사용된) 액터 포인터가 잡히면 "첫 진입"으로 인정되지 않아 슬로우 GE가 적용되지 않는다.
- 리슨서버 + PIE 멀티 인스턴스에서 월드 간 상태가 섞인다.

**권장**: static 제거 → `ADRStageGameState`(이미 독가스 카운트 관리 중) 또는 WorldSubsystem 멤버로 이동, 페이즈 종료 시 Clear.

### 8-3. 🟠 BreakableDoor: 클라이언트에서 파편 연출이 숨겨질 수 있는 OnRep/Multicast 경합
`DRBreakableDoor.cpp:94-144, 204-211`

서버가 `bIsBroken = true`(복제 프로퍼티) 세팅과 `Multicast_PlayBreakEffect()`(파편 물리 연출)를 같은 프레임에 실행한다. 클라이언트에서는 같은 번치에서 **프로퍼티가 RPC보다 먼저 적용**되므로 `OnRep_IsBroken()`이 파편을 `SetVisibility(false)`로 숨긴 뒤에 멀티캐스트가 물리 임펄스를 적용한다 → 보이지 않는 파편이 날아가고, 클라 화면에서는 문이 그냥 사라진다. `OnRep_IsBroken`의 숨김 처리는 late-join 전용 의도이므로, "연출 재생됨" 플래그로 게이트하거나 OnRep에서 일정 시간 뒤에만 숨기도록 해야 한다.

### 8-4. 🟠 OverlayWidgetController::UnbindAllDelegates가 사실상 no-op
`OverlayWidgetController.cpp:267-316`

ASC attribute 델리게이트와 CleanserSite 델리게이트 해제 루프의 본문이 **비어 있고**, 주석으로 "핸들만 무효화(람다에서 WeakThis 체크로 안전)"라고 자인한다. WeakThis 가드 덕에 크래시는 없지만:
- 관전 전환(`UpdateOverlayForSpectating`)마다 관전 대상 PlayerState의 ASC에 죽은 람다 엔트리가 누적된다 (델리게이트 리스트 무한 성장).
- `BindCallbacksToDependencies`의 `EffectAssetTags.AddLambda` 등도 같은 ASC에 재바인딩 시 중복 등록될 수 있다.

핸들을 실제로 사용해 `GetGameplayAttributeValueChangeDelegate(Attr).Remove(Handle)`로 해제하도록 구현할 것 (Attribute별 핸들을 쌍으로 저장하면 됨). 또한 GameState 바인딩 3종이 0.1초 매직 딜레이 타이머로 되어 있는 것도 §1-12와 동일 패턴.

### 8-5. 🟠 시드 폭탄 궤적이 클라이언트에서 어긋날 수 있음
`DRSeedProjectile.cpp:33-39` + `DRSeedCannon.cpp:44-48`

`ADRSeedProjectile`은 `bReplicates`이지만 `SetReplicateMovement(false)` — 원격 클라이언트는 스폰 트랜스폼 + PMC 기본값(`InitialSpeed=700`)으로 로컬 시뮬레이션한다. 그런데 서버는 `SeedCannon`이 스폰 후 `Velocity = LaunchDirection * LaunchSpeed`를 **직접 대입**한다. `LaunchSpeed`(BP 설정값)가 700과 다르면 서버와 클라의 비행 궤적이 달라지고, 폭발 위치만 멀티캐스트로 정정된다 → 클라 화면에서 시드가 엉뚱한 곳을 날다가 다른 곳에서 터진다. 대역폭 절약 의도라면 `InitialSpeed`를 `LaunchSpeed`와 일치시키거나 초기 속도를 스폰 시 1회 복제할 것.

### 8-6. 🟠 적 체력바 빌보드가 적 1마리당 매 프레임 틱
`DRBillboardWidgetComponent.cpp:8-47`

모든 적(+클렌저사이트)의 HealthBar가 매 프레임 `GetFirstPlayerController()` 조회 + `SetWorldRotation`을 수행한다. 적 100마리 상한 기준 컴포넌트 틱 100개/프레임. `PrimaryComponentTick.TickInterval = 0.05f` 정도로 낮추거나, 거리/화면 밖 컬링을 추가하거나, `EWidgetSpace::Screen` 모드(자동 빌보드)로 전환을 검토.

### 8-7. 🟡 게임오버 경로 이원화 — 전멸 시 게임오버 사운드가 안 나옴
`DRGameModeBase.cpp:17-49` vs `DRStageGameMode.cpp:57-88`

- 클렌저 파괴/몬스터 초과 → `TriggerGameOver()`: 사운드 멀티캐스트 + UI + 페이즈 정리 + 로비 귀환.
- 전원 사망(전멸) → `ADRGameModeBase::OnPlayerDied()`: **UI만 표시**하고 `HandleWipeout`으로 귀환. `Multicast_PlayGameOverSound`도, `CurrentPhase->OnPhaseEnd()`도 호출되지 않는다.

전멸 경로가 `TriggerGameOver()`를 타도록 통일하는 것이 맞다 (StageGameMode에서 `OnPlayerDied` override).

### 8-8. 🟡 EnemySpawnGroup이 '사망'이 아닌 '파괴(Destroy)' 기준으로 생존 수 집계
`DREnemySpawnGroup.cpp:45-105`

`OnDestroyed` 바인딩이라 적이 죽어도 `LifeSpan`(기본 2초) 동안 산 것으로 집계된다 → 방 클리어 판정·문 파괴가 2초 지연. 또 페이즈 전환 시 일괄 `Destroy()`되는 적들도 "전멸"로 집계되어 `OnAllEnemiesDead`가 발화한다(현재는 DoorManager가 이미 문을 열어 무해하지만 의미론적 함정). 다른 시스템처럼 `OnDeathDelegate` 기준으로 통일 권장.

### 8-9. 🟡 WaitCooldownChange의 델리게이트 해제 누락
`WaitCooldownChange.cpp:32-39`

`EndTask()`가 태그 이벤트만 해제하고 `OnActiveGameplayEffectAddedDelegateToSelf`에 등록한 콜백은 해제하지 않는다. UI에서 쿨다운 태스크를 반복 생성하면 ASC 델리게이트에 누적.

### 8-10. 🟡 설정 시스템 이중화 + 지역화 자기모순
`DRSettingsManager.cpp` 전반

- **구 시스템**(`ApplyAndSaveAllSettings`/`SetMasterVolume`/…)과 **신 데이터 주도 시스템**(`InitSettings`/`Pending`/`Definitions`)이 공존. 신 시스템이 내부적으로 구 함수를 부르긴 하지만 외부 진입점이 2벌이라 헷갈린다. 구 API를 정리하거나 private으로.
- 언어 설정 기능(ko/en 문화권 전환, `OnLanguageChanged`)을 넣어 놓고 정작 **설정창 라벨 전부가 `FText::FromString(TEXT("해상도"))` 식 한국어 하드코딩** — 영어로 전환해도 설정창은 한국어로 남는다. 튜토리얼(`NSLOCTEXT` 사용, `DRTutorialManager.cpp`)과 지역화 정책이 정반대. `LOCTEXT`/StringTable로 통일해야 언어 기능이 의미가 있다.
- 언어 드롭다운에 Korean 옵션만 등록되어 있어 `LanguageOptionIdToCulture`의 English 매핑은 죽은 코드 (`BuildDefinitions`의 Gameplay.Language).
- `Initialize`의 `LoadObject` 하드코딩 경로 4개 + 로드 실패 시 빈 경고 블록 4개 (`DRSettingsManager.cpp:57-69`) — `TSoftObjectPtr` config 프로퍼티화 권장.

### 8-11. 🟡 SoundManager 서브시스템이 사실상 미사용 레이어
`DRSoundManager.cpp` 전반

`UDRSoundManager`가 SoundData 접근+재생 헬퍼를 제공하지만, 실제 사운드 재생 코드(사이트/부품/시드/자판기/게임스테이트의 멀티캐스트 12곳+)는 전부 `UDRAssetManager::GetIfInitialized() → GetSoundDataAsset() → PlaySoundAtLocation` 보일러플레이트를 직접 반복한다. 한쪽으로 통일: 멀티캐스트 내부에서 `UDRSoundManager`를 쓰거나, SoundManager를 삭제하고 공용 static 헬퍼 하나만 남길 것.

### 8-12. 🟡 튜토리얼 매니저가 "플레이어 0" 전제
`DRTutorialManager.cpp:515-540`

`GetOverlayWidgetController`/`GetPlayerASC`가 `GetFirstPlayerController()` 기반이라 멀티플레이 튜토리얼이면 호스트만 추적/어빌리티 부여된다. `bReplicates = true`로 선언된 액터인데 실질 로직은 싱글 전제 — 의도(싱글 전용)를 주석과 `check`로 명시하거나 전 플레이어 순회로 일반화. `Tick`도 Objective2 동안만 필요하므로 목표 전환 시 `SetActorTickEnabled` 토글 가능.

### 8-13. 🟡 클래스 선택 저장이 PlayerName 키
`DRGameInstance.cpp:79-105`

`PlayerClassSelections`이 `FString PlayerName` 키다. 스팀 닉네임은 중복될 수 있으므로 동명 플레이어 2명이 접속하면 클래스 선택이 서로 덮어써진다. `FUniqueNetIdRepl` 문자열 키 권장.

### 8-14. 🟡 PlayerState 세터의 사이드이펙트 역참조
`DRPlayerState.cpp:383-401`

`SetSelectedPlayerClass()`가 내부에서 `GetAuthGameMode<ADRLobbyGameMode>()`를 캐스트해 **폰 리스폰까지 수행**한다. 스테이지에서 `DRStageGameMode::GetDefaultPawnClassForController`가 이 세터를 호출하는데(로비 GM이 아니라서 무해하게 스킵되지만), "데이터 세터가 게임모드를 알고 리스폰을 트리거"하는 구조는 우발적 리스폰 위험이 있다. 리스폰은 호출자(LobbyGameMode)가 명시적으로 수행하도록 분리 권장.

### 8-15. 🟡 기타 소소한 정리 대상 (2차)
| 위치 | 내용 |
|---|---|
| `DRMeleeAttack.cpp`, `DamageTextComponent.cpp`, `DRInputComponent.cpp`, `DRSoundDataAsset.cpp` | include 한 줄뿐인 빈 .cpp — 유지할 거면 그대로 두어도 무해하나, 빌드 단위 정리 시 삭제 후보 |
| `DRSeedProjectile.cpp:346-420`, `DRSeedCannon.cpp:61-88`, `DRArmadilloEnemy.cpp`의 `#if 0` 블록 | 대량의 주석/비활성 디버그 드로잉 잔존 — 삭제 |
| `DREliteRoar.cpp:56-58` | 오라 틱마다 `GetAllActorsOfClass(ADREnemy)` — Phase3의 `SpawnedEnemies` 참조나 스피어 오버랩으로 대체 |
| `ExecCalc_Heal.cpp:22`, `DRSeedProjectile.cpp:306` | `RequestGameplayTag(FName("Heal"))` ad-hoc 요청 — `FDRGameplayTags`에 네이티브 등록해 중앙 관리 원칙 유지 |
| `DRGameplayAbility.cpp:48` + `ExecCalc_WaterCost.cpp:85` | 물 부족 → 체력 전환 계수 `0.5f`가 두 파일에 하드코딩 — 한쪽만 바꾸면 CheckCost와 실제 차감이 어긋남. 공용 상수로 |
| `DRTutorialManager.cpp:60-67` | 초기 UI 0.5초 딜레이 — §1-12 패턴 |
| `DRLobbyGameMode.cpp:40-94, 119-178` | PostLogin/SeamlessTravel의 0.5초 매직 딜레이 — §1-12 패턴 |
| `DRLobbyGameMode.cpp:475` | `NextAvailableSlot++`이 슬롯 수를 넘어도 증가만 함 — 5번째 이후 입장자는 전부 마지막 슬롯에 겹침(Clamp). 슬롯 수 상한 검사/재사용 로직 필요 |
| `DRStageSelectActor.cpp:60-89` | 호스트 위젯 셋업 0.5초 재시도 폴링 — §1-12 패턴 |
| `DRPhase3.cpp:1295` + `DRPoisonGasActor.cpp:73` | 독가스 경고 3초가 Phase3의 `SetLifeSpan(3.5f)`("경고 3초+활성 0.5초" 주석)와 PoisonGasActor의 `3.0f` 타이머 두 곳에 분산 하드코딩 — 한쪽 변경 시 조용히 어긋남 |
| `UStatusEffectInfo::FindEffectInfoForTag` | 값 복사 루프(`for (FEffectInfo Info : ...)`) — `const &`로 |
| `DRCleanserSite`(위젯 `SetOwnerNoSee(false)`+멀티캐스트 필터) vs `DRStageSelectActor`(위젯 `SetOnlyOwnerSee(true)`+SetOwner) | 같은 "특정 플레이어에게만 위젯 표시" 문제를 서로 다른 방식으로 해결 — §2-1 로컬화로 통일하면 함께 정리됨 |

### 8-16. 문제 없이 확인된 파일 (2차에서 정독, 특이사항 없음)
`DRFlyingEnemy`(사망 낙하 처리 꼼꼼함), `DRWaterSource`, `DRBGMActor`(복제 트랙 인덱스 동기화 깔끔), `DRLoadingScreenSubsystem`(위크 캡처·중복 방지 모범적), `DRMainMenuGameMode`, `DRTutorialGameMode`, `DRLobbyGameState`, `DRGameUserSettings`, `DRSettingsFunctionLibrary`, `DRAssetManager`, `DRSaveGame`, 튜토리얼 액터 4종(Gate/PressurePlate/StartTile/FallZone — 서버 전용 바인딩 올바름), `DRWaitingRoomCameraActor`, `DREffectActor`, `DebuffNiagaraComponent`, `TargetDataFromCamera`(표준 예측 타겟데이터 패턴), `DRAbilityTypes`(NetSerialize 비트마스킹 올바름), `DRAbilitySystemGlobals`, GameplayCue 사운드 2종, `DRFacialExpressionComponent`(타이머 정리 철저), Input 3종, 위젯 보일러플레이트류.

---

## 우선순위 요약 (권장 작업 순서)

1. **치트 RPC 가드** (§1-1) — 한 줄 수정, 보안 이슈.
2. **사망 시 ForceDropCarriedPart** (§1-2) — 진행 불가 버그.
3. **OverlayWidgetController 페이즈 인덱스 `== 2` 정리** (§8-1) — 방어 페이즈 UI(웨이브 타이머/클렌저 HP)가 통째로 어긋날 수 있는 구조 불일치. `PhaseClasses` BP 배열과 함께 반드시 교차 검증.
4. **RoundToFloat no-op / 엘리트 데드코드** (§1-3, §1-4) — 각 한 줄.
5. **PoisonGas static OverlapCountMap 제거** (§8-2) — 세션 간 상태 누수.
6. **AI 디버그 로그 제거** (§3-1) — 웨이브 중 상시 비용 + 로그 스팸.
7. **플러그인 널 체크 3건** (§7-1, §7-2) — 크래시 방지 한 줄 수정들.
8. **상호작용 UI 로컬화** (§2-1, §4-5, §8-15 마지막 행) — 네트워크·구조 동시 개선, 파급 큼.
9. **PlayerState NetUpdateFrequency 하향** (§2-2) — 값 하나.
10. **벽 스턴 태그 기반 전환** (§1-5) — 레벨 작업 안정성.
11. **BreakableDoor OnRep 게이트 / 전멸 시 게임오버 사운드 통일** (§8-3, §8-7) — 눈에 보이는 연출 버그.
12. **CLAUDE.md 현행화 + 주석 인코딩 통일** (§4-1, §4-3) — 이후 모든 작업의 정확도에 영향.
13. 웨이브 타이머 종료시각 방식 전환, 적 회전 Yaw 복제, 체력바 틱 스로틀 (§2-4, §2-5, §8-6).
14. UnbindAllDelegates 실구현, WaitCooldownChange 델리게이트 해제 (§8-4, §8-9).
15. 설정 시스템 정리 + 설정창 지역화 (§8-10), SoundManager 통일 (§8-11).
16. 죽은 코드/레거시 복제 프로퍼티 정리 (§5-1, §8-15).
