# Research: 메인 메뉴, 로비, 멀티플레이 세션 시스템 분석

## 목차

1. [시스템 전체 흐름 요약](#1-시스템-전체-흐름-요약)
2. [맵 구조](#2-맵-구조)
3. [MultiplayerSessions 플러그인](#3-multiplayersessions-플러그인)
4. [GameMode 계층 구조](#4-gamemode-계층-구조)
5. [GameState 계층 구조](#5-gamestate-계층-구조)
6. [메인 메뉴 시스템](#6-메인-메뉴-시스템)
7. [방 생성 플로우](#7-방-생성-플로우)
8. [방 참가 플로우](#8-방-참가-플로우)
9. [로비 시스템](#9-로비-시스템)
10. [로비 → 스테이지 전환](#10-로비--스테이지-전환)
11. [스테이지 → 로비 복귀](#11-스테이지--로비-복귀)
12. [PlayerController의 레벨 전환 처리](#12-playercontroller의-레벨-전환-처리)
13. [보이스 채팅 시스템](#13-보이스-채팅-시스템)
14. [초대 시스템 (Steam/플랫폼)](#14-초대-시스템-steam플랫폼)
15. [설정 및 구성 파일](#15-설정-및-구성-파일)
16. [핵심 파일 경로 정리](#16-핵심-파일-경로-정리)

---

## 1. 시스템 전체 흐름 요약

```
게임 시작
    ↓
MainMenu.umap 로드 (GameDefaultMap)
    ↓ BP_DRMainMenuGameMode 사용
UMenu 위젯 표시 (Host/Join 버튼 + 방 코드 입력)
    ↓
[방 생성(Host)] 또는 [방 참가(Join)]
    ↓
방 생성 성공 시: ServerTravel → LobbyMap?listen
방 참가 성공 시: ClientTravel → 호스트 주소
    ↓
LobbyMap.umap 로드
    ↓ BP_DRLobbyGameMode 사용
    ↓ BP_DRLobbyGameState가 방 코드 복제
로비에서 대기 (방 코드 표시, 스테이지 포털 존재)
    ↓
호스트가 DRStageSelectActor와 상호작용 (E키)
    ↓
ServerTravel → Stage맵?listen (Seamless Travel)
    ↓
스테이지 게임플레이 (페이즈 시스템)
    ↓
게임 클리어 / 게임 오버
    ↓
ServerTravel → LobbyMap?listen (Seamless Travel)
    ↓
로비로 복귀 (다음 스테이지 선택 가능)
```

---

## 2. 맵 구조

| 맵 파일 | 용도 | 사용 GameMode |
|---------|------|---------------|
| `Content/Maps/MainMenu.umap` | 메인 메뉴 화면 | `BP_DRMainMenuGameMode` |
| `Content/Maps/LobbyMap.umap` | 로비 (대기실) | `BP_DRLobbyGameMode` |
| `Content/Maps/TestMap1.umap` | 스테이지 (개발용) | `BP_DRStageGameMode` |
| `Content/Maps/Stage1.umap` | 스테이지 1 | `BP_DRStageGameMode` |
| `Content/Maps/TransitionMap.umap` | Seamless Travel 전환용 맵 | - |
| `Content/Maps/StartupMap.umap` | 에디터 시작 맵 | - |

**DefaultEngine.ini 설정:**
```ini
GameDefaultMap=/Game/Maps/MainMenu.MainMenu           ; 게임 시작 시 로드되는 맵
EditorStartupMap=/Game/Maps/StartupMap.StartupMap     ; 에디터 PIE 시작 맵
TransitionMap=/Game/Maps/TransitionMap.TransitionMap  ; Seamless Travel 전환 맵
GameInstanceClass=/Game/Blueprints/Game/BP_DRGameInstance.BP_DRGameInstance_C
```

---

## 3. MultiplayerSessions 플러그인

### 3.1 개요

**위치:** `Plugins/MultiplayerSessions/`
**제작자:** KimMinki
**의존성:** OnlineSubsystem, OnlineSubsystemSteam

플러그인은 Unreal의 Online Subsystem을 래핑하여 **방 코드 기반 매칭 시스템**을 제공한다. IP 주소 대신 8자리 영숫자 코드로 세션을 식별한다.

### 3.2 핵심 클래스

#### UMultiplayerSessionsSubsystem (GameInstanceSubsystem)

**파일:**
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Public/MultiplayerSessionsSubsystem.h`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/MultiplayerSessionsSubsystem.cpp`

게임 인스턴스에 종속되는 서브시스템으로, 세션의 전체 생명주기를 관리한다.

**핵심 프로퍼티:**
```cpp
IOnlineSessionPtr SessionInterface;           // Unreal 온라인 세션 인터페이스
IOnlineVoicePtr VoiceInterface;              // 보이스 채팅 인터페이스
TSharedPtr<FOnlineSessionSettings> LastSessionSettings;  // 마지막 세션 설정
TSharedPtr<FOnlineSessionSearch> LastSessionSearch;      // 마지막 검색 결과
FString CurrentRoomCode;                      // 현재 세션의 방 코드
FString PendingRoomCode;                      // 생성 중인 방 코드
FString SearchingRoomCode;                    // 검색 중인 방 코드
bool bIsCreatingWithRoomCode;                 // 방 코드 생성 플로우 진행 중
bool bCreateSessionOnDestroy;                 // 기존 세션 파괴 후 재생성 플래그
int32 LastNumPublicConnections;               // 최대 접속 인원
```

**커스텀 델리게이트 (Blueprint 바인딩 가능):**
```cpp
FMultiplayerOnCreateSessionComplete       // (bool bWasSuccessful)
FMultiplayerOnFindSessionsComplete        // (TArray<FOnlineSessionSearchResult>&, bool)
FMultiplayerOnJoinSessionComplete         // (EOnJoinSessionCompleteResult::Type)
FMultiplayerOnDestroySessionComplete      // (bool bWasSuccessful)
FMultiplayerOnStartSessionComplete        // (bool bWasSuccessful)
FMultiplayerOnRoomCodeGenerated           // (const FString& RoomCode)
```

**BlueprintCallable 함수 목록:**
| 함수 | 설명 |
|------|------|
| `CreateSessionWithRoomCode(int32, FString)` | 방 코드로 세션 생성 |
| `FindSessionByRoomCode(FString)` | 방 코드로 세션 검색 |
| `UpdateSessionJoinability(bool)` | 참가 가능 여부 토글 |
| `LeaveServer()` | 서버 떠나기 (메인 메뉴로) |
| `StartVoiceChat()` | 보이스 채팅 시작 |
| `StopVoiceChat()` | 보이스 채팅 중지 |
| `GetCurrentRoomCode()` | 현재 방 코드 반환 |

---

#### UMenu (UUserWidget)

**파일:**
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Public/Menu.h`
- `Plugins/MultiplayerSessions/Source/MultiplayerSessions/Private/Menu.cpp`

메인 메뉴의 Host/Join UI 위젯이다.

**바인드된 위젯:**
```cpp
UButton* HostButton;                  // 방 만들기 버튼
UButton* JoinButton;                  // 방 참가 버튼
UEditableTextBox* RoomCodeInputBox;   // 방 코드 입력 필드
```

**핵심 프로퍼티:**
```cpp
int32 NumPublicConnections = 4;       // 기본 최대 접속 인원: 4명
FString PathToLobby;                  // 로비 맵 경로 (기본: "/Game/Maps/LobbyMap")
FString PendingJoinRoomCode;          // 참가 시도 중인 방 코드
```

**초기화 (MenuSetup):**
```cpp
void MenuSetup(int32 NumberOfPublicConnections = 4,
               const FString& LobbyPath = "/Game/Maps/LobbyMap")
```
1. LobbyPath에 `?listen` 추가 (Listen Server 활성화)
2. 위젯을 뷰포트에 추가
3. Visibility를 Visible로 설정, Focusable 활성화
4. 인풋 모드를 UI Only로 설정
5. 마우스 커서 표시
6. GameInstance에서 서브시스템 획득
7. 서브시스템의 모든 델리게이트 바인딩

---

### 3.3 방 코드 시스템

**코드 형식:** 8자리 영숫자 (A-Z, 0-9)

```cpp
static constexpr int32 ROOM_CODE_LENGTH = 8;

FString GenerateRoomCode()
{
    // A-Z, 0-9 문자로 8자리 랜덤 생성
    static const FString Characters = TEXT("ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789");
    FString Code;
    for (int32 i = 0; i < ROOM_CODE_LENGTH; i++)
    {
        Code += Characters[FMath::RandRange(0, Characters.Len() - 1)];
    }
    return Code;
}
```

**중복 방지 메커니즘:**
1. 코드 생성 후 `ValidateAndCreateSessionWithCode()` 호출
2. 해당 코드로 기존 세션 검색
3. 중복 발견 시 새 코드 생성 후 재시도
4. 유니크한 코드 확인 후 `CreateSessionInternal()` 호출

**세션 설정에 저장:**
```cpp
LastSessionSettings->Set(FName("RoomCode"), CurrentRoomCode,
                         EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
```

---

### 3.4 세션 설정 상세

세션 생성 시 적용되는 설정:
```cpp
LastSessionSettings->bIsLANMatch = (SubsystemName == "NULL");  // NULL이면 LAN 모드
LastSessionSettings->NumPublicConnections = NumPublicConnections;  // 기본 4
LastSessionSettings->bAllowJoinInProgress = true;
LastSessionSettings->bAllowJoinViaPresence = true;
LastSessionSettings->bShouldAdvertise = true;
LastSessionSettings->bUsesPresence = true;
LastSessionSettings->bUseLobbiesIfAvailable = true;
LastSessionSettings->BuildUniqueId = 1;
```

**LAN vs Online 판별:**
```cpp
IOnlineSubsystem::Get()->GetSubsystemName() == "NULL"  // true면 LAN
// "NULL" = LAN 모드, 그 외 = Online (Steam 등)
```

---

## 4. GameMode 계층 구조

```
AGameMode (UE5 기본)
    ↓
ADRGameModeBase
    ├── ADRMainMenuGameMode   (메인 메뉴)
    ├── ADRLobbyGameMode      (로비)
    └── ADRStageGameMode      (스테이지 게임플레이)
```

### 4.1 ADRGameModeBase (공통 베이스)

**파일:**
- `Source/DaeRune/Public/Game/DRGameModeBase.h`
- `Source/DaeRune/Private/Game/DRGameModeBase.cpp`

모든 게임모드의 공통 기능을 제공한다.

**핵심 기능:**
- **Wipeout 감지:** 모든 플레이어 사망 시 게임 오버 처리
- **레벨 전환 준비:** `PrepareForTravel()` - 모든 클라이언트의 설정 메뉴 닫기 + 오디오 정지
- **Detection Manager 스폰**
- **CharacterClassInfo, AbilityInfo, GameBalanceConfig** 데이터 에셋 보유

**핵심 메서드:**
```cpp
// 플레이어 사망 시 호출 - 전멸 확인
void OnPlayerDied(APlayerState* DeadPlayer);

// 모든 플레이어가 죽었는지 확인
bool CheckTeamWipeout();

// 전멸 처리 (가상 함수 - 서브클래스에서 오버라이드)
virtual void HandleWipeout();

// 맵 전환 전 정리 작업
void PrepareForTravel();
// → 모든 PC에 ClientCloseSettingsMenu() 호출
// → 모든 PC에 ClientStopAllAudio() 호출
```

**핵심 프로퍼티:**
```cpp
float WipeoutDelayTime = 3.0f;        // 전멸 후 전환까지 딜레이
bool bIsWipeoutInProgress = false;     // 전멸 처리 중 플래그
TObjectPtr<UCharacterClassInfo> CharacterClassInfo;
TObjectPtr<UAbilityInfo> AbilityInfo;
TObjectPtr<UGameBalanceConfig> GameBalanceConfig;
```

### 4.2 ADRMainMenuGameMode

**파일:**
- `Source/DaeRune/Public/Game/DRMainMenuGameMode.h`
- `Source/DaeRune/Private/Game/DRMainMenuGameMode.cpp`

**구현:** 빈 클래스. `AGameModeBase`를 상속하며 커스텀 로직 없음.
메인 메뉴에서는 게임플레이가 없으므로 최소한의 구현만 필요하다. 블루프린트 버전(`BP_DRMainMenuGameMode`)에서 DefaultPawnClass 등을 설정한다.

### 4.3 ADRLobbyGameMode

**파일:**
- `Source/DaeRune/Public/Game/DRLobbyGameMode.h`
- `Source/DaeRune/Private/Game/DRLobbyGameMode.cpp`

**핵심 역할:**
- 플레이어 로그인/로그아웃 관리
- Seamless Travel 복귀 처리
- 스테이지 맵으로의 전환 실행
- 세션의 Join In Progress 활성화

**핵심 메서드:**

```cpp
// === 플레이어 생명주기 ===

void PostLogin(APlayerController* NewPlayer);
// - 스테이지에서 복귀 후 플레이어 상태 복원
// - 관전 모드 해제
// - 이동/충돌 컴포넌트 재활성화

void HandleSeamlessTravelPlayer(AController*& C);
// - Seamless Travel로 복귀 시 플레이어 상태 복원
// - PostLogin과 유사한 복원 로직

void Logout(AController* Exiting);
// - 플레이어 퇴장 로깅

// === 레벨 전환 ===

void TravelToStage(const FString& StageMapName, ADRPlayerController* Requester);
// - 호스트만 호출 가능 (IsLocalController 검증)
// - ExecuteTravel() 호출

void ExecuteTravel(const FString& StageMapName);
// - PrepareForTravel() 호출 (UI/오디오 정리)
// - bUseSeamlessTravel = true 설정
// - World->ServerTravel(StageMapName + "?listen")

// === 세션 관리 ===

void AllowJoinInProgress();
// - BeginPlay에서 호출
// - MultiplayerSessionsSubsystem->UpdateSessionJoinability(true)

// === 전멸 처리 ===

void HandleWipeout() override;
// - 게임 오버 UI 표시
// - RestartLobby() 호출

void RestartLobby();
// - 현재 로비 맵을 Seamless Travel로 재로드
```

**핵심 프로퍼티:**
```cpp
FString LobbyMapName = TEXT("LobbyMap");
float WipeoutDelayTime = 2.0f;  // 기본 3초보다 짧은 2초
```

### 4.4 ADRStageGameMode

**파일:**
- `Source/DaeRune/Public/Game/DRStageGameMode.h`
- `Source/DaeRune/Private/Game/DRStageGameMode.cpp`

**세션 관련 핵심 로직:**

```cpp
void BlockJoinInProgress();
// - BeginPlay에서 호출
// - MultiplayerSessionsSubsystem->UpdateSessionJoinability(false)
// - 스테이지 진행 중 신규 참가 차단

void ReturnToLobby();
// - PrepareForTravel() 호출
// - ServerTravel(LobbyMapName + "?listen")
// - bIsWipeoutInProgress 리셋
```

**로비와의 관계:**
- 스테이지 시작 시 `BlockJoinInProgress()` → 참가 차단
- 게임 클리어/오버 시 `ReturnToLobby()` → 로비로 복귀
- 로비 복귀 시 `ADRLobbyGameMode`가 다시 `AllowJoinInProgress()` 호출

---

## 5. GameState 계층 구조

```
AGameStateBase (UE5 기본)
    ↓
ADRGameStateBase
    ├── ADRLobbyGameState    (로비 상태)
    └── ADRStageGameState    (스테이지 상태)
```

### 5.1 ADRGameStateBase

**파일:**
- `Source/DaeRune/Public/Game/DRGameStateBase.h`
- `Source/DaeRune/Private/Game/DRGameStateBase.cpp`

```cpp
// 리플리케이트되는 프로퍼티
UPROPERTY(Replicated)
int32 CurrentPlayerCount;    // 현재 접속 인원

UPROPERTY(Replicated)
int32 MaxPlayerCount = 4;    // 최대 접속 인원

// 주요 기능
void AddPlayerState(APlayerState* PS);    // 플레이어 카운트 업데이트
void RemovePlayerState(APlayerState* PS); // 플레이어 카운트 업데이트
APlayerState* GetHostPlayer() const;      // PlayerId == 0인 호스트 반환
bool IsPlayerHost(APlayerState* PS) const;
TArray<ADRCharacter*> GetAlivePlayers() const;  // 생존 플레이어 목록
```

### 5.2 ADRLobbyGameState

**파일:**
- `Source/DaeRune/Public/Game/DRLobbyGameState.h`
- `Source/DaeRune/Private/Game/DRLobbyGameState.cpp`

**핵심 역할:** 방 코드를 모든 클라이언트에 복제하여 UI에 표시한다.

```cpp
// 리플리케이트되는 프로퍼티
UPROPERTY(ReplicatedUsing = OnRep_RoomCode)
FString RoomCode;

UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
int32 MinPlayersToStart = 1;

// 델리게이트
UPROPERTY(BlueprintAssignable)
FOnRoomCodeGenerated OnRoomCodeGenerated;  // UI 바인딩용

// 핵심 메서드
void InitializeRoomCode(const FString& NewRoomCode);
// - BeginPlay에서 호출 (서버만)
// - MultiplayerSessionsSubsystem의 OnRoomCodeGenerated 델리게이트 바인딩
// - 서브시스템에서 방 코드 가져와서 설정

void SetRoomCode(const FString& NewRoomCode);
// - 서버에서 호출
// - RoomCode 설정 + BroadcastRoomCode()

void OnRep_RoomCode();
// - 클라이언트에서 복제 수신 시 호출
// - BroadcastRoomCode() → UI 업데이트

void BroadcastRoomCode();
// - OnRoomCodeGenerated 델리게이트 브로드캐스트
```

**방 코드 복제 흐름:**
```
서버:
  MultiplayerSessionsSubsystem → 방 코드 생성
      ↓
  ADRLobbyGameState::InitializeRoomCode()
      ↓
  SetRoomCode() → RoomCode 프로퍼티 설정
      ↓
  BroadcastRoomCode() → 서버 UI 업데이트

클라이언트:
  RoomCode 프로퍼티 복제 수신
      ↓
  OnRep_RoomCode() 호출
      ↓
  BroadcastRoomCode() → 클라이언트 UI 업데이트
```

### 5.3 ADRStageGameState (요약)

스테이지 게임플레이에 필요한 모든 페이즈 데이터를 복제한다. (메인 메뉴/로비와 직접 관련 없으므로 간략히 기술)

- 페이즈 인덱스, 상태 (NotStarted/InProgress/Completed/Failed)
- Phase 1: 정화소 확보 여부, 남은 적 수
- Phase 2: 수집한 부품 수, 정화소 활성화 여부
- Phase 3: 현재 웨이브, 정화소 체력, 남은 시간, 독가스 여부
- Phase 4: 보스 체력

---

## 6. 메인 메뉴 시스템

### 6.1 초기화 흐름

```
게임 실행
    ↓
MainMenu.umap 로드 (DefaultEngine.ini의 GameDefaultMap)
    ↓
BP_DRMainMenuGameMode 활성화 (ADRMainMenuGameMode - 빈 클래스)
    ↓
UMenu 위젯이 MenuSetup()으로 초기화됨 (블루프린트에서 호출)
    ↓
인풋 모드: UI Only
마우스 커서: 표시
    ↓
Host 버튼 / Join 버튼 / 방 코드 입력 필드 표시
```

### 6.2 UMenu 위젯 초기화 상세

```cpp
void UMenu::MenuSetup(int32 NumberOfPublicConnections, const FString& LobbyPath)
{
    // 1. 로비 경로에 ?listen 추가
    PathToLobby = LobbyPath + TEXT("?listen");

    // 2. 위젯을 뷰포트에 추가
    AddToViewport();
    SetVisibility(ESlateVisibility::Visible);
    bIsFocusable = true;

    // 3. 인풋 모드 설정
    FInputModeUIOnly InputModeData;
    InputModeData.SetWidgetToFocus(TakeWidget());
    InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    PC->SetInputMode(InputModeData);
    PC->SetShowMouseCursor(true);

    // 4. 서브시스템 획득 및 델리게이트 바인딩
    MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
    MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
    MultiplayerSessionsSubsystem->MultiplayerOnFindSessionsComplete.AddUObject(this, &ThisClass::OnFindSessions);
    MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
    MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnDestroySession);
    MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
}
```

---

## 7. 방 생성 플로우 (Host)

### 7.1 전체 시퀀스 다이어그램

```
플레이어가 Host 버튼 클릭
    ↓
UMenu::HostButtonClicked()
    ↓ HostButton 비활성화 (중복 클릭 방지)
MultiplayerSessionsSubsystem->CreateSessionWithRoomCode(4, "RoomCodeOnly")
    ↓
기존 세션 존재 확인
    ├── 존재함 → DestroySession() 후 bCreateSessionOnDestroy = true
    └── 없음 → 계속 진행
    ↓
GenerateRoomCode() → 8자리 랜덤 코드 생성 (예: "A3K9BX7M")
    ↓
ValidateAndCreateSessionWithCode()
    ↓ 동일 코드의 기존 세션 검색
OnFindSessionsComplete 콜백
    ├── 중복 발견 → 새 코드 생성, 다시 검증
    └── 중복 없음 → CreateSessionInternal()
        ↓
        세션 설정 구성:
        - bIsLANMatch: SubsystemName == "NULL" 기반
        - NumPublicConnections: 4
        - bAllowJoinInProgress: true
        - bUsesPresence: true
        - bUseLobbiesIfAvailable: true
        - RoomCode 커스텀 키 설정
            ↓
        SessionInterface->CreateSession(LocalPlayerId, NAME_GameSession, Settings)
            ↓
        OnCreateSessionComplete 콜백
            ↓
        MultiplayerOnRoomCodeGenerated 델리게이트 브로드캐스트
        MultiplayerOnCreateSessionComplete 델리게이트 브로드캐스트
            ↓
UMenu::OnCreateSession(true)
    ↓
화면에 "Session created successfully!" 메시지 표시
    ↓
World->ServerTravel(PathToLobby)
// PathToLobby = "/Game/Maps/LobbyMap?listen"
    ↓
LobbyMap 로드 → 로비 진입 성공
```

### 7.2 세션 생성 실패 시

```
OnCreateSessionComplete(false)
    ↓
UMenu::OnCreateSession(false)
    ↓
"Failed to create session!" 메시지 표시
HostButton 다시 활성화 (재시도 가능)
```

### 7.3 기존 세션이 있을 때

```
CreateSessionWithRoomCode() 호출
    ↓
SessionInterface->GetNamedSession(NAME_GameSession) != nullptr
    ↓
bCreateSessionOnDestroy = true
LastNumPublicConnections = NumPublicConnections
    ↓
DestroySession() 호출
    ↓
OnDestroySessionComplete 콜백
    ↓
bCreateSessionOnDestroy == true이므로
    ↓
CreateSessionWithRoomCode(LastNumPublicConnections) 재호출
```

---

## 8. 방 참가 플로우 (Join)

### 8.1 전체 시퀀스 다이어그램

```
플레이어가 방 코드 입력 (RoomCodeInputBox)
    ↓
Join 버튼 클릭
    ↓
UMenu::JoinButtonClicked()
    ↓
방 코드 검증:
    ├── 빈 문자열 → "Please enter a room code!" 에러
    ├── 8자리 아님 → "Room code must be exactly 8 characters!" 에러
    ├── 영숫자 아닌 문자 포함 → "Room code must contain only letters and numbers!" 에러
    └── 검증 통과
        ↓
    코드를 대문자로 변환 (ToUpper)
    JoinButton 비활성화
    PendingJoinRoomCode = 입력된 코드
        ↓
    MultiplayerSessionsSubsystem->FindSessionByRoomCode(RoomCode)
        ↓
    세션 검색 시작:
    - MaxSearchResults = 100
    - QuerySettings: RoomCode == 입력값 (Equals)
    - QuerySettings: SEARCH_LOBBIES == true
        ↓
    OnFindSessionsComplete 콜백
        ↓
    UMenu::OnFindSessions(SearchResults, bWasSuccessful)
        ↓
    검색 결과 순회:
        각 결과의 세션 설정에서 "RoomCode" 값 추출
        PendingJoinRoomCode와 비교
            ├── 일치하는 세션 발견
            │       ↓
            │   결과 복사 + Presence 플래그 설정
            │   MultiplayerSessionsSubsystem->JoinSession(Result)
            │       ↓
            │   SessionInterface->JoinSession(LocalPlayerId, NAME_GameSession, Result)
            │       ↓
            │   OnJoinSessionComplete 콜백
            │       ↓
            │   세션에서 RoomCode 추출
            │   MultiplayerOnRoomCodeGenerated 브로드캐스트
            │   MultiplayerOnJoinSessionComplete 브로드캐스트
            │       ↓
            │   UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Success)
            │       ↓
            │   SessionInterface->GetResolvedConnectString(NAME_GameSession, Address)
            │       ↓
            │   PlayerController->ClientTravel(Address, TRAVEL_Absolute)
            │       ↓
            │   호스트의 LobbyMap에 접속 → 로비 진입 성공
            │
            └── 일치하는 세션 없음
                    ↓
                "No session found with room code: XXXXXXXX" 에러 메시지
                JoinButton 다시 활성화
```

### 8.2 참가 실패 시나리오

| 실패 원인 | 처리 |
|-----------|------|
| 방 코드 형식 오류 | 화면에 에러 메시지, Join 버튼 유지 |
| 세션 검색 실패 | Join 버튼 재활성화 |
| 일치하는 세션 없음 | 에러 메시지 + Join 버튼 재활성화 |
| JoinSession 실패 | Join 버튼 재활성화 + PendingJoinRoomCode 클리어 |
| 접속 주소 획득 실패 | 에러 로그 |

---

## 9. 로비 시스템

### 9.1 로비 진입 시 초기화

**호스트 진입 (ServerTravel로 도착):**
```
LobbyMap 로드
    ↓
BP_DRLobbyGameMode 활성화
    ↓
ADRLobbyGameMode::BeginPlay()
    ↓
AllowJoinInProgress()
    → MultiplayerSessionsSubsystem->UpdateSessionJoinability(true)
    → 다른 플레이어의 참가를 허용
    ↓
ADRLobbyGameState::BeginPlay() (서버만)
    ↓
InitializeRoomCode()
    → MultiplayerSessionsSubsystem에서 현재 방 코드 획득
    → SetRoomCode() → 복제 시작
    ↓
로비 UI에 방 코드 표시 (OnRoomCodeGenerated 델리게이트)
```

**클라이언트 진입 (ClientTravel로 도착):**
```
호스트의 LobbyMap에 접속
    ↓
ADRLobbyGameMode::PostLogin(NewPlayer)
    ↓
플레이어 상태 복원:
    - ClientStopSpectating() 호출
    - 이동 컴포넌트 활성화
    - 캡슐 충돌 활성화
    ↓
ADRGameStateBase::AddPlayerState()
    → CurrentPlayerCount 업데이트 (복제)
    ↓
ADRLobbyGameState에서 RoomCode 복제 수신
    → OnRep_RoomCode() → UI 업데이트
```

### 9.2 로비 UI 구성

**블루프린트 에셋:**
- `Content/Blueprints/UI/HUD/BP_DRLobbyHUD.uasset` - 로비 전용 HUD
- `Content/Blueprints/UI/Overlay/WBP_LobbyOverlay.uasset` - 로비 오버레이 위젯

**표시 정보:**
- 방 코드 (ADRLobbyGameState에서 복제)
- 현재 접속 인원 / 최대 인원
- 스테이지 선택 포털 위치 안내

### 9.3 로비의 스테이지 선택 포털 (DRStageSelectActor)

**파일:**
- `Source/DaeRune/Public/Actor/DRStageSelectActor.h`
- `Source/DaeRune/Private/Actor/DRStageSelectActor.cpp`

로비 맵에 배치되는 상호작용 액터로, 호스트만 스테이지 전환을 시작할 수 있다.

**컴포넌트:**
```cpp
UStaticMeshComponent* PortalMesh;          // 포털 시각적 메시 (NoCollision)
UBoxComponent* InteractionBox;             // 상호작용 감지 박스 (200x200x100)
UWidgetComponent* InteractionWidget;       // "E키로 상호작용" UI
```

**프로퍼티:**
```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Stage")
FString DestinationMapName;                // 대상 스테이지 맵 이름 (블루프린트에서 설정)
```

**상호작용 흐름:**
```
플레이어가 InteractionBox에 진입
    ↓
OnBoxBeginOverlap()
    ↓
HasAuthority() 확인 (서버만 처리)
    ↓
진입한 플레이어가 ADRPlayerController인지 확인
    ↓
IsLocalController() 확인 (호스트인지 확인)
    ↓
이미 OverlappingHostController가 있으면 무시 (한 번에 하나만)
    ↓
OverlappingHostController = 진입한 컨트롤러
InteractionWidget 표시
    ↓
플레이어가 E키 입력 → OnHostInteract()
    ↓
ServerRequestTravel() RPC
    ↓
ADRLobbyGameMode::TravelToStage(DestinationMapName, Requester)
    ↓
ExecuteTravel(DestinationMapName)
    → PrepareForTravel() (모든 클라이언트 UI/오디오 정리)
    → bUseSeamlessTravel = true
    → World->ServerTravel(DestinationMapName + "?listen")
```

**보안 검증:**
1. `HasAuthority()` - 서버에서만 처리
2. `IsLocalController()` - Listen Server의 호스트만 허용
3. 단일 컨트롤러 잠금 - 동시에 하나의 플레이어만 상호작용

---

## 10. 로비 → 스테이지 전환

### 10.1 전체 전환 시퀀스

```
호스트가 DRStageSelectActor와 상호작용
    ↓
ADRLobbyGameMode::TravelToStage()
    ↓
ExecuteTravel(StageMapName)
    ↓
PrepareForTravel():
    모든 PlayerController에 대해:
    ├── ClientCloseSettingsMenu()  // 설정 메뉴 닫기
    └── ClientStopAllAudio()       // BGM + VoIP 정리
    ↓
bUseSeamlessTravel = true 설정
    ↓
World->ServerTravel(StageMapName + "?listen")
    ↓
[Seamless Travel 시작]
    ↓
TransitionMap.umap 임시 로드 (전환 중 표시되는 맵)
    ↓
스테이지 맵 로드 완료
    ↓
BP_DRStageGameMode 활성화
    ↓
ADRStageGameMode::BeginPlay()
    ├── BlockJoinInProgress()       // 세션 참가 차단
    ├── InitializePhaseSystem()     // 페이즈 시스템 초기화
    └── CleanserSite 탐색 및 설정
    ↓
각 PlayerController::PostSeamlessTravel()
    ↓
OnLevelEntered()
    ├── UI 정리 (이전 게임 결과 위젯 제거)
    ├── 관전 모드 리셋
    ├── ViewTarget 복원
    └── RestoreDefaultInputMode() → Game Only 모드
    ↓
스테이지 게임플레이 시작
```

### 10.2 Seamless Travel의 특징

- **PlayerController가 보존됨** - 접속이 끊기지 않고 맵만 전환
- **TransitionMap 사용** - 전환 중 빈 맵을 로드하여 로딩 화면 역할
- **?listen 파라미터** - Listen Server 유지를 위해 필수
- **bUseSeamlessTravel = true** 필수 설정

---

## 11. 스테이지 → 로비 복귀

### 11.1 게임 클리어 시

```
모든 페이즈 완료
    ↓
ADRStageGameMode::TriggerGameClear()
    ↓
bIsWipeoutInProgress = true (중복 방지)
Multicast_PlayGameClearSound()
    ↓
NotifyAllPlayersGameEnd(true)
    → 모든 PC에 게임 클리어 UI 표시
    → 모든 PC의 오디오 정지
    ↓
WipeoutDelayTime (5초) 대기
    ↓
ReturnToLobby()
    ↓
PrepareForTravel()
World->ServerTravel(LobbyMapName + "?listen")
    ↓
[Seamless Travel → 로비로 복귀]
```

### 11.2 게임 오버 시 (전멸)

```
모든 플레이어 사망
    ↓
ADRGameModeBase::OnPlayerDied()
    ↓
CheckTeamWipeout() == true
    ↓
bIsWipeoutInProgress = true
모든 PC에 게임 오버 UI 표시
    ↓
WipeoutDelayTime 대기
    ↓
HandleWipeout() → ADRStageGameMode에서 오버라이드
    ↓
ReturnToLobby() (스테이지) 또는 RestartLobby() (로비)
```

### 11.3 로비 복귀 후 처리

```
LobbyMap 로드
    ↓
ADRLobbyGameMode 활성화
    ↓
AllowJoinInProgress() → 세션 참가 다시 허용
    ↓
각 플레이어에 대해 HandleSeamlessTravelPlayer() 호출
    ├── 관전 모드 해제
    ├── 이동 컴포넌트 재활성화
    └── 충돌 컴포넌트 재활성화
    ↓
각 PlayerController::PostSeamlessTravel()
    → OnLevelEntered()
        ├── 게임 결과 위젯 제거
        ├── 관전 상태 리셋
        ├── ViewTarget 복원 (자기 폰으로)
        └── RestoreDefaultInputMode() → Game Only
    ↓
로비에서 다시 대기 (다음 스테이지 선택 가능)
```

---

## 12. PlayerController의 레벨 전환 처리

### 12.1 ADRPlayerController 관련 메서드

**파일:**
- `Source/DaeRune/Public/Player/DRPlayerController.h`
- `Source/DaeRune/Private/Player/DRPlayerController.cpp`

**레벨 진입 공통 처리:**
```cpp
// 최초 접속 시
void ReceivedPlayer()
{
    Super::ReceivedPlayer();
    if (!IsLocalController()) return;
    OnLevelEntered();
}

// Seamless Travel 후
void PostSeamlessTravel()
{
    Super::PostSeamlessTravel();
    if (!IsLocalController()) return;
    OnLevelEntered();
}
```

**OnLevelEntered() - 공통 초기화:**
```cpp
void OnLevelEntered()
{
    // 1. UI 정리
    // - 게임 결과 위젯 (GameOver/GameClear) 제거
    // - 설정 위젯 정리
    // - 메뉴 오픈 상태 리셋

    // 2. 관전 모드 정리
    bIsSpectating = false;
    CurrentSpectatedCharacter = nullptr;

    // 3. ViewTarget 복원
    if (MyPawn)
        SetViewTarget(MyPawn);
    else
        // 0.5초 후 재시도 (폰 스폰 타이밍 이슈 대응)

    // 4. 인풋 모드 복원
    RestoreDefaultInputMode();
}
```

**레벨별 인풋 모드:**
```cpp
void RestoreDefaultInputMode()
{
    if (IsInMainMenu())
    {
        SetInputMode(FInputModeUIOnly());
        SetShowMouseCursor(true);
    }
    else  // 로비 또는 스테이지
    {
        SetInputMode(FInputModeGameOnly());
        SetShowMouseCursor(false);
    }
}
```

**레벨 판별 메서드:**
```cpp
bool IsInMainMenu() const;   // 맵 이름에 "MainMenu" 포함 여부
bool IsInLobby() const;      // 맵 이름에 "Lobby" 포함 여부
bool IsInGameLevel() const;  // 맵 이름에 "Stage1" 포함 여부
```

### 12.2 전환 전 정리 (Client RPC)

```cpp
// 서버 → 클라이언트 호출
void ClientCloseSettingsMenu();  // 설정 메뉴 위젯 닫기
void ClientStopAllAudio();       // BGM 정지 + VoIP 컴포넌트 비활성화/해제
```

`ClientStopAllAudio()`는 VoipListenerSynthComponent를 비활성화하고 렌더 씬에서 해제하여 전환 중 크래시를 방지한다.

---

## 13. 보이스 채팅 시스템

**MultiplayerSessionsSubsystem에서 관리:**

```cpp
// 시작
void StartVoiceChat()
{
    VoiceInterface->RegisterLocalTalker(0);
    VoiceInterface->StartNetworkedVoice(0);
}

// 중지
void StopVoiceChat()
{
    VoiceInterface->ClearVoicePackets();
    VoiceInterface->StopNetworkedVoice(0);
    VoiceInterface->RemoveAllRemoteTalkers();
    VoiceInterface->DisconnectAllEndpoints();
    VoiceInterface->UnregisterLocalTalker(0);
}
```

**보이스 채팅 생명주기:**
- 로비/스테이지에서 `StartVoiceChat()` 호출 (블루프린트에서)
- `LeaveServer()` 호출 시 자동으로 `StopVoiceChat()`
- 네트워크 실패 시 자동으로 `StopVoiceChat()`
- 맵 전환 전 `ClientStopAllAudio()`에서 VoIP 컴포넌트 정리

---

## 14. 초대 시스템 (Steam/플랫폼)

**MultiplayerSessionsSubsystem의 초대 처리:**

```cpp
// 플랫폼 초대 수락 시 콜백
void OnSessionUserInviteAccepted(bool bWasSuccessful, int32 ControllerId,
                                  FUniqueNetIdPtr UserId,
                                  const FOnlineSessionSearchResult& InviteResult)
{
    // 1. 초대 결과 캐싱
    CachedInviteResult = MakeShared<FOnlineSessionSearchResult>(InviteResult);
    bInvitePending = true;

    // 2. 자동 참가 시도
    TryProcessPendingInvite();
}
```

**중복 참가 방지 플래그:**
```cpp
bool bInvitePending;       // 대기 중인 초대 존재
bool bInviteJoinStarted;   // 초대 참가 시작됨 (중복 방지)
TSharedPtr<FOnlineSessionSearchResult> CachedInviteResult;
```

---

## 15. 설정 및 구성 파일

### DefaultEngine.ini (멀티플레이 관련)

```ini
[/Script/Engine.GameEngine]
+NetDriverDefinitions=(DefName="GameNetDriver",
    DriverClassName="OnlineSubsystemSteam.SteamNetDriver",
    DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480          ; Steam 테스트용 App ID (Spacewar)
bInitServerOnClient=true

[OnlineSubsystem]
DefaultPlatformService=Steam

[/Script/OnlineSubsystemSteam.SteamNetDriver]
NetConnectionClassName="OnlineSubsystemSteam.SteamNetConnection"
```

### 맵 관련 설정

```ini
[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Game/Maps/MainMenu.MainMenu
EditorStartupMap=/Game/Maps/StartupMap.StartupMap
TransitionMap=/Game/Maps/TransitionMap.TransitionMap
GameInstanceClass=/Game/Blueprints/Game/BP_DRGameInstance.BP_DRGameInstance_C
```

---

## 16. 핵심 파일 경로 정리

### 소스 코드

| 파일 | 경로 | 역할 |
|------|------|------|
| MultiplayerSessionsSubsystem | `Plugins/MultiplayerSessions/Source/.../MultiplayerSessionsSubsystem.h/.cpp` | 세션 관리 핵심 |
| Menu Widget | `Plugins/MultiplayerSessions/Source/.../Menu.h/.cpp` | 메인 메뉴 Host/Join UI |
| DRGameModeBase | `Source/DaeRune/Public/Game/DRGameModeBase.h/.cpp` | 게임모드 공통 베이스 |
| DRMainMenuGameMode | `Source/DaeRune/Public/Game/DRMainMenuGameMode.h/.cpp` | 메인 메뉴 게임모드 |
| DRLobbyGameMode | `Source/DaeRune/Public/Game/DRLobbyGameMode.h/.cpp` | 로비 게임모드 |
| DRStageGameMode | `Source/DaeRune/Public/Game/DRStageGameMode.h/.cpp` | 스테이지 게임모드 |
| DRGameStateBase | `Source/DaeRune/Public/Game/DRGameStateBase.h/.cpp` | 게임스테이트 공통 베이스 |
| DRLobbyGameState | `Source/DaeRune/Public/Game/DRLobbyGameState.h/.cpp` | 로비 게임스테이트 (방 코드) |
| DRStageGameState | `Source/DaeRune/Public/Game/DRStageGameState.h/.cpp` | 스테이지 게임스테이트 |
| DRPlayerController | `Source/DaeRune/Public/Player/DRPlayerController.h/.cpp` | 레벨 전환 처리 |
| DRStageSelectActor | `Source/DaeRune/Public/Actor/DRStageSelectActor.h/.cpp` | 로비 스테이지 포털 |
| DRHUD | `Source/DaeRune/Public/UI/HUD/DRHUD.h/.cpp` | HUD/위젯 관리 |

### 블루프린트 에셋

| 에셋 | 경로 |
|------|------|
| BP_DRMainMenuGameMode | `Content/Blueprints/Game/BP_DRMainMenuGameMode.uasset` |
| BP_DRLobbyGameMode | `Content/Blueprints/Game/BP_DRLobbyGameMode.uasset` |
| BP_DRLobbyGameState | `Content/Blueprints/Game/BP_DRLobbyGameState.uasset` |
| BP_DRStageGameMode | `Content/Blueprints/Game/BP_DRStageGameMode.uasset` |
| BP_DRLobbyHUD | `Content/Blueprints/UI/HUD/BP_DRLobbyHUD.uasset` |
| WBP_LobbyOverlay | `Content/Blueprints/UI/Overlay/WBP_LobbyOverlay.uasset` |
| BP_DRGameInstance | `Content/Blueprints/Game/BP_DRGameInstance.uasset` |

### 맵 파일

| 맵 | 경로 |
|----|------|
| MainMenu | `Content/Maps/MainMenu.umap` |
| LobbyMap | `Content/Maps/LobbyMap.umap` |
| TransitionMap | `Content/Maps/TransitionMap.umap` |
| TestMap1 | `Content/Maps/TestMap1.umap` |
| Stage1 | `Content/Maps/Stage1.umap` |
| StartupMap | `Content/Maps/StartupMap.umap` |

### 설정 파일

| 파일 | 경로 |
|------|------|
| DefaultEngine.ini | `Config/DefaultEngine.ini` |
| MultiplayerSessions.uplugin | `Plugins/MultiplayerSessions/MultiplayerSessions.uplugin` |

---
---

# Plan: 메인 메뉴 리뉴얼 + 대기실 시스템

## 요구사항 정리

### 현재 상태 (AS-IS)

- 메인 메뉴: 단순한 UI 화면 (Host/Join 버튼 + 방 코드 입력)
- 방 생성/참가 성공 시 → 바로 LobbyMap으로 이동
- LobbyMap 진입 즉시 플레이어 캐릭터를 조작 가능
- 호스트가 스테이지 포털과 상호작용하면 스테이지로 전환

### 변경 후 (TO-BE)

크게 3가지 변화가 생긴다:
1. **메인 메뉴 화면이 시각적으로 변경**된다 (실제 맵을 배경으로 렌더링)
2. **메인 메뉴와 로비(자유 조작) 사이에 "대기실" 단계가 추가**된다 (로비 맵 내 구역)
3. **대기실에서 캐릭터 선택, 킥, 게임 시작 등의 기능**이 제공된다

---

## 상세 플로우

### 1단계: 메인 메뉴 (튜토리얼 미완료 유저)

**조건:** 플레이어가 튜토리얼을 한 번도 완료하지 않은 경우

**화면 구성:**
- 카메라가 **튜토리얼 스테이지 맵**을 실제로 렌더링하여 비추고 있다
- 튜토리얼 스테이지 위에 **플레이어 캐릭터가 서 있는 모습**이 보인다
- 화면 한쪽에 **메인 메뉴 UI**가 오버레이되어 있다 (StartGame 버튼 등)

> **참고:** 튜토리얼 스테이지 맵은 별도로 존재하지만 아직 제작되지 않았다. 후에 만들어질 예정이므로, 현재 구현에서는 튜토리얼 관련 분기는 구조만 잡아두고 실제 튜토리얼 맵 연동은 나중에 한다.

**StartGame 버튼 클릭 시:**
1. 메인 메뉴 UI가 사라진다
2. 카메라가 **부드럽게 이동**하며 플레이어 캐릭터의 시점으로 진입한다
   - 순간이동이 아닌, 카메라가 캐릭터 안으로 "들어가는" 느낌의 연출
   - (카메라가 현재 위치 → 캐릭터 뒤쪽 → 3인칭 게임 카메라 위치로 부드럽게 전환)
3. 카메라 전환이 완료되면 **튜토리얼이 시작**된다
4. 플레이어가 캐릭터를 조작할 수 있게 된다

**튜토리얼 완료 후:**
- 튜토리얼 완료 상태가 저장된다 (SaveGame 또는 로컬 설정)
- 다음 메인 메뉴 진입 시 2단계로 표시된다

---

### 2단계: 메인 메뉴 (튜토리얼 완료 유저)

**조건:** 플레이어가 튜토리얼을 이미 완료한 경우

**화면 구성:**
- 카메라가 **로비 맵(LobbyMap)**을 실제로 렌더링하여 비추고 있다
- 로비 맵 위에 **플레이어 캐릭터가 서 있는 모습**이 보인다
- 화면 한쪽에 **메인 메뉴 UI**가 오버레이되어 있다
  - Host (방 생성) 버튼
  - Join (방 참가) + 방 코드 입력 필드
  - 기타 메뉴 (설정, 종료 등)

> **구현 포인트:** 메인 메뉴 맵 자체가 로비 맵이거나, 메인 메뉴 맵에서 로비 맵의 배경을 렌더링해야 한다. 가장 자연스러운 방식은 **메인 메뉴 자체를 로비 맵(LobbyMap)으로 사용**하되, 메인 메뉴 상태에서는 네트워크 미연결 + UI Only 모드로 동작하는 것이다.

**방 생성(Host) 또는 방 참가(Join) 성공 시:**
- 바로 로비를 자유 조작하는 것이 아니라, **대기실(Waiting Room)** 단계로 진입한다
- 네트워킹 레이어는 기존과 동일:
  - Host → CreateSession → ServerTravel to LobbyMap?listen → 대기실 상태로 시작
  - Join → FindSession → ClientTravel to 호스트 → 대기실 상태로 합류

---

### 3단계: 대기실 (Waiting Room) - 새로 추가되는 단계

**이것이 핵심 변경사항이다. 메인 메뉴와 기존 로비(자유 조작) 사이에 삽입되는 새로운 단계.**

**대기실은 별도의 맵이 아니라 로비 맵(LobbyMap) 내의 특정 구역이다.** 대기실 상태와 자유 조작 상태는 맵 전환 없이 게임 로직(GameState/GameMode)으로 전환된다.

#### 카메라 시스템

- 모든 플레이어가 **공통으로 보는 하나의 고정 카메라 시점**이 있다
- 이 카메라는 대기실 구역의 캐릭터들을 모두 비출 수 있는 위치에 고정되어 움직이지 않는다
- 플레이어의 캐릭터 조작은 **완전히 불가**하다
- 인풋 모드: **UI Only** (마우스 커서 활성, 키보드/마우스로 UI만 조작 가능)

#### 캐릭터 배치

- 로비 맵 내 **지정된 대기 위치(스폰 포인트)**에 캐릭터들이 일렬로 서 있다
- **가장 왼쪽:** 호스트의 캐릭터
- **오른쪽으로 순서대로:** 참가한 플레이어들의 캐릭터 (1열 배치)
- 새 플레이어가 참가할 때마다 오른쪽 끝에 추가된다

#### A. 킥(Kick) 시스템

- **호스트에게만** 각 플레이어 캐릭터의 머리 위에 **Kick 버튼**이 보인다
- 호스트 자신의 캐릭터에는 Kick 버튼이 없다
- Kick 버튼 클릭 시:
  - 해당 플레이어가 방에서 **추방**된다
  - 추방된 플레이어는 메인 메뉴로 돌아간다
  - 대기실의 캐릭터 배치가 **재정렬**된다 (빈 자리 없이 왼쪽으로 당김)

#### B. 캐릭터 선택 시스템

- 각 플레이어는 대기실에서 **자신이 플레이할 캐릭터 클래스를 선택**할 수 있다
- 현재 ECharacterClass: Elementalist, Warrior, Ranger
- **UI 형태:**
  - 자신의 플레이어 캐릭터 **아래에** 현재 선택된 캐릭터의 **이름 UI**가 표시된다
  - 이름 UI의 **양 옆에 화살표 버튼(◀ ▶)**이 있다
  - 화살표 버튼을 누르면 다른 캐릭터 클래스로 순환 변경된다
- 선택한 캐릭터에 따라 대기실에 서 있는 캐릭터의 **외형(메시)이 즉시 변경**된다
- 다른 플레이어들에게도 선택 결과가 **실시간으로 반영**된다 (리플리케이션)

#### C. 게임 시작 (Power On)

- **호스트에게만** Power On 버튼이 보인다
- 호스트가 Power On 버튼을 클릭하면:

**Power On 연출 시퀀스:**
1. 대기실 UI가 사라진다 (킥 버튼, 캐릭터 선택 UI, Power On 버튼 모두)
2. **세션 참가 차단:** `UpdateSessionJoinability(false)` — Power On 이후에는 로비라도 새 플레이어가 참가할 수 없다
3. 각 플레이어의 화면에서 카메라가 **부드럽게 이동**한다
   - 현재 대기실 고정 카메라 시점 → 자신의 플레이어 캐릭터 쪽으로
   - 카메라가 캐릭터 안으로 "들어가는" 연출 (메인 메뉴 튜토리얼 시작과 동일한 방식)
4. 카메라 전환이 완료되면:
   - 인풋 모드가 **Game Only**로 변경된다
   - 플레이어가 **캐릭터를 조작**할 수 있게 된다
5. 이제 기존 로비 상태와 동일하다 (자유 이동, 스테이지 포털 상호작용 가능)

---

### 4단계: 로비 자유 조작 (기존과 동일)

- Power On 연출 이후의 상태
- 기존 LobbyMap에서의 플레이와 동일
- 호스트가 스테이지 포털(DRStageSelectActor)과 상호작용하여 스테이지로 전환
- **이 상태에서는 새 플레이어 참가 불가** (Power On 시 차단됨)

---

### 5단계: 스테이지 게임플레이 (기존과 동일)

- 기존 페이즈 시스템 그대로 동작
- BlockJoinInProgress는 이미 적용된 상태

---

### 6단계: 스테이지 → 로비 복귀

- 게임 클리어 또는 게임 오버 시 로비로 복귀
- **복귀 시 대기실 상태로 돌아간다** (자유 조작 상태가 아님)
- 즉, 복귀 흐름:
  1. ReturnToLobby() → ServerTravel(LobbyMap?listen)
  2. LobbyMap 로드 → **대기실 상태**로 시작
  3. 고정 카메라 시점, UI Only 모드
  4. 캐릭터들이 대기 위치에 다시 배치됨
  5. 캐릭터 선택, Kick 등 다시 가능
  6. 세션 참가 다시 허용: `UpdateSessionJoinability(true)`
  7. 호스트가 다시 Power On을 눌러야 자유 조작 상태로 전환

---

## 전체 플로우 다이어그램

```
게임 시작
    ↓
[튜토리얼 완료 여부 확인]
    │
    ├── 미완료 → 메인 메뉴 (튜토리얼 맵 배경, 실제 렌더링)
    │               │
    │               ├── StartGame 클릭
    │               │       ↓
    │               │   카메라 연출 (고정 → 캐릭터 시점으로 부드럽게 진입)
    │               │       ↓
    │               │   튜토리얼 시작 (캐릭터 조작 가능)
    │               │       ↓
    │               │   튜토리얼 완료 → 완료 플래그 저장
    │               │       ↓
    │               └── 메인 메뉴 (로비 배경)으로 전환
    │
    └── 완료됨 → 메인 메뉴 (로비 맵 배경, 실제 렌더링)
                    │
                    ├── Host 클릭 → CreateSession → ServerTravel(LobbyMap?listen)
                    └── Join 클릭 → FindSession → ClientTravel(호스트 주소)
                            ↓
                    ┌─────────────────────────────────┐
                    │     대기실 (Waiting Room)          │
                    │                                   │
                    │  고정 카메라 / UI Only 모드         │
                    │  캐릭터 1열 배치 (호스트 왼쪽)       │
                    │                                   │
                    │  [호스트 전용]                      │
                    │  - 각 플레이어 머리 위 Kick 버튼     │
                    │  - Power On 버튼                   │
                    │                                   │
                    │  [모든 플레이어]                     │
                    │  - 캐릭터 아래 이름 + ◀ ▶ 선택 UI   │
                    │  - 새 참가자 → 오른쪽에 추가          │
                    │  - 퇴장/추방 → 재정렬               │
                    └─────────────────────────────────┘
                            ↓ 호스트 Power On 클릭
                    세션 참가 차단 (JoinInProgress = false)
                            ↓
                    카메라 연출 (고정 시점 → 각자 캐릭터로 부드럽게 진입)
                            ↓
                    인풋 모드: Game Only / 캐릭터 조작 가능
                            ↓
                    ┌─────────────────────────────────┐
                    │     로비 자유 조작 (기존과 동일)     │
                    │  - 자유 이동                       │
                    │  - 호스트: 스테이지 포털 상호작용     │
                    │  - 새 플레이어 참가 불가             │
                    └─────────────────────────────────┘
                            ↓ 호스트가 스테이지 포털 상호작용
                    ServerTravel(StageMap?listen) [Seamless Travel]
                            ↓
                    ┌─────────────────────────────────┐
                    │     스테이지 게임플레이              │
                    │  - 페이즈 시스템 (기존과 동일)       │
                    │  - BlockJoinInProgress 유지        │
                    └─────────────────────────────────┘
                            ↓ 게임 클리어 / 게임 오버
                    ReturnToLobby() → ServerTravel(LobbyMap?listen)
                            ↓
                    ┌─────────────────────────────────┐
                    │     대기실로 복귀                   │
                    │  - 대기실 상태로 재진입              │
                    │  - 세션 참가 다시 허용               │
                    │  - 캐릭터 재배치, 선택/Kick 가능     │
                    │  - 호스트 Power On 대기             │
                    └─────────────────────────────────┘
                            ↓ (반복)
```

---

## 로비 상태 머신

로비 맵(LobbyMap)은 이제 두 가지 상태를 가진다:

```
┌──────────────┐    Power On     ┌──────────────┐
│              │  ───────────→   │              │
│   WaitingRoom│                 │   FreeRoam   │
│   (대기실)    │  ←───────────   │  (자유 조작)  │
│              │  스테이지 복귀    │              │
└──────────────┘                 └──────────────┘

WaitingRoom 상태:
  - 카메라: 고정 (공통 시점)
  - 인풋: UI Only (마우스 커서 ON)
  - 캐릭터 조작: 불가
  - 세션 참가: 허용
  - UI: Kick 버튼(호스트), 캐릭터 선택, Power On(호스트)

FreeRoam 상태:
  - 카메라: 캐릭터에 부착 (기존 3인칭)
  - 인풋: Game Only (마우스 커서 OFF)
  - 캐릭터 조작: 가능
  - 세션 참가: 차단
  - UI: 기존 로비 HUD
```

**상태 전환:**
- WaitingRoom → FreeRoam: 호스트 Power On 클릭 (카메라 연출 포함)
- FreeRoam → WaitingRoom: 스테이지에서 로비 복귀 시

---

## 카메라 연출 사양

### 공통 연출 패턴: "카메라가 캐릭터 안으로 들어가는" 효과

이 연출은 3곳에서 사용된다:
1. **메인 메뉴 → 튜토리얼 시작** (StartGame 클릭)
2. **대기실 → 자유 조작** (Power On 클릭, 각 플레이어별)
3. (미래) 기타 전환 시

**연출 상세:**
- **시작:** 현재 카메라 위치 (메뉴 고정 카메라 또는 대기실 고정 카메라)
- **목표:** 해당 플레이어 캐릭터의 게임 카메라 위치 (3인칭 카메라 붐 위치)
- **전환 방식:** 부드러운 보간 (Lerp/Ease-In-Out)
  - 카메라가 현재 위치에서 캐릭터를 향해 이동
  - 점점 캐릭터에 가까워지며 "안으로 들어가는" 느낌
  - 최종적으로 캐릭터의 SpringArm/CameraBoom 위치에 도달
- **소요 시간:** 약 1.5~2초 (조절 가능)
- **전환 완료 후:** 인풋 모드 변경 + 캐릭터 조작 활성화

---

## 캐릭터 선택 시스템 사양

### UI 레이아웃

```
        [캐릭터 메시]
              │
     ◀  Elementalist  ▶     ← 캐릭터 이름 + 좌우 화살표 버튼
```

- 각 플레이어의 캐릭터 아래에 **월드 스페이스 또는 스크린 스페이스** 위젯으로 표시
- **이름 텍스트:** 현재 선택된 캐릭터 클래스명 (Elementalist / Warrior / Ranger)
- **◀ 버튼:** 이전 캐릭터 클래스로 변경
- **▶ 버튼:** 다음 캐릭터 클래스로 변경
- 자신의 캐릭터만 화살표 버튼이 **인터랙티브** (다른 플레이어의 것은 이름만 표시)

### 선택 시 동작

1. 화살표 클릭 → 서버에 캐릭터 변경 요청 (Server RPC)
2. 서버에서 PlayerState의 선택 캐릭터 변경 (리플리케이트)
3. 모든 클라이언트에서 해당 플레이어의 대기실 캐릭터 메시 업데이트
4. UI 이름 텍스트 업데이트

### 캐릭터 순환 순서

```
→ Elementalist → Warrior → Ranger → Elementalist → ...
← Elementalist ← Warrior ← Ranger ← Elementalist ← ...
```

---

## 킥(Kick) 시스템 사양

### 호스트 화면

```
     [Kick]              [Kick]            [Kick]
       ↓                   ↓                  ↓
  [호스트 캐릭터]    [플레이어2 캐릭터]   [플레이어3 캐릭터]
       │                   │                  │
  (Kick 없음)         ◀ Warrior ▶       ◀ Ranger ▶
```

- 호스트 자신의 캐릭터 위에는 Kick 버튼이 **없다**
- 다른 플레이어 캐릭터 머리 위에 Kick 버튼이 표시된다
- **월드 스페이스 위젯** (캐릭터 머리 위 고정 위치)

### 일반 플레이어 화면

```
  [호스트 캐릭터]    [나의 캐릭터]       [플레이어3 캐릭터]
       │                 │                   │
  ◀ Elementalist ▶  ◀ Warrior ▶        ◀ Ranger ▶
                    (내것만 조작 가능)   (이름만 표시)
```

- Kick 버튼이 전혀 보이지 않는다
- 자신의 캐릭터 선택 UI만 인터랙티브, 나머지는 읽기 전용

### Kick 실행 흐름

1. 호스트가 Kick 버튼 클릭
2. Server RPC → GameMode에서 해당 플레이어 처리
3. 추방 대상 플레이어의 세션 연결 해제
4. 추방된 플레이어 → 메인 메뉴로 이동
5. 나머지 플레이어들의 대기 위치 **재정렬** (빈 자리 없이 왼쪽으로 당김)
6. 재정렬된 배치가 모든 클라이언트에 **리플리케이트**

---

## 확인 완료 사항

| # | 질문 | 답변 |
|---|------|------|
| 1 | 튜토리얼 스테이지 맵이 별도로 존재하는가? | 별도로 존재하지만 아직 미제작. 후에 제작 예정. 현재는 구조만 준비. |
| 2 | 메인 메뉴 배경이 실제 맵 렌더링인가? | **Yes.** 실제 맵을 렌더링하는 방식. |
| 3 | 대기실은 별도 맵인가, 로비 맵 내 구역인가? | **로비 맵 내의 특정 구역.** 별도 맵 아님. |
| 4 | 대기실에서 플레이어 조작 불가 구현 방식? | **고정 카메라 1개를 모든 플레이어가 공유.** 이동 불가, UI Only 모드. |
| 5 | 캐릭터 선택 UI 형태? | 캐릭터 아래에 이름 UI + 양쪽 화살표 버튼(◀ ▶)으로 순환 선택. |
| 6 | 기존 세션/네트워킹 플로우 유지? | **Yes.** 네트워킹 레이어는 기존과 동일. 대기실은 로비 맵 위의 게임 로직 레이어. |
| 7 | Power On 후 새 플레이어 참가 가능? | **No.** Power On 후 세션 참가 차단 (`JoinInProgress = false`). |
| 8 | 스테이지에서 로비 복귀 시 어떤 상태? | **대기실 상태로 복귀.** 자유 조작이 아닌 대기실에서 다시 시작. 세션 참가 다시 허용. |
