// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Interfaces/OnlineIdentityInterface.h"
#include "GenericTeamAgentInterface.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRProgressionTypes.h"
#include "Tuning/DRTuningTypes.h"
#include "DRPlayerController.generated.h"

// 상호작용 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInteractPressed);

class UDamageTextComponent;
class UInputMappingContext;
class UInputAction;
struct FInputActionValue;
class UDRInputConfig;
class UDRAbilitySystemComponent;
class ADRCleanserPart;
class ADRCleanserSite;
class ADRRobotVacuumCharacter;
class ADRCharacter;
class ADRWaitingRoomCameraActor;
class UDRWaitingRoomWidget;
class UDRStageSelectScreenWidget;
class ADRS2InteractProp;
class ADRS2SlidePuzzle;
class APlayerState;
class UDRS2SlidePuzzleWidget;
class UDRInteractionComponent;
class UUserWidget;
class ACameraActor;
class USkeletalMeshComponent;
enum class ELobbyState : uint8;

// 현재 레벨 컨텍스트 (레벨 진입 시 1회 판별해 캐시 - 매 프레임 맵 이름 문자열 연산 방지)
enum class EDRLevelContext : uint8
{
	Unknown,
	MainMenu,
	Lobby,
	Tutorial,
	GameLevel
};

// 시작 연출 카메라를 띄운 쪽 — 끝낼 때 갈래가 다르다 (로비: 대기실 카메라로 되돌릴 곳이 있다 / 스테이지: 이 월드의 연출을 끝냈다고 적는다)
enum class EDRIntroCameraKind : uint8
{
	Lobby,		// 로비 Power On — 대기실 카메라에서 출발
	Stage		// 스테이지 시작 — 바라보는 방향 멀리에서 출발
};

// 스테이지 시작 연출 진행 (클라이언트마다, 월드마다)
enum class EDRStageIntroStep : uint8
{
	NotStage,			// 스테이지가 아니다
	WaitingForPawn,		// 내 폰 도착 · BeginPlay 를 기다린다
	WaitingForRender,	// 연출 카메라를 출발 시점에 세웠다 — 화면 준비(패키지 빌드의 PSO 컴파일)를 기다린다
	WaitingForPlayers,	// 서버에 로딩 완료를 알렸다 — 다른 플레이어를 기다린다 (서버 진행 Intro 대기)
	Revealing,			// 모두 마쳤다 — 로딩 화면이 걷히는 동안 출발 시점에 머문다
	Playing,			// 카메라가 다가오는 중
	Done				// 이 월드에서 끝났다 — 조작 · HUD 를 돌려줬다
};

// 전체 화면 UI — 한 번에 하나만 열린다 (하나를 열면 열려 있던 화면은 닫힌다)
UENUM(BlueprintType)
enum class EDRScreen : uint8
{
	None,
	Settings,		// 설정 (어디서든)
	Upgrade,		// 업그레이드 칩 (로비)
	Wardrobe,		// 옷장 (로비)
	StageSelect,	// 스테이지 선택 (로비 · 호스트)
	SlidePuzzle		// 스테이지2 8퍼즐
};

/**
 * DaeRune 플레이어의 입력 처리 및 UI 관리 클래스
 */
UCLASS()
class DAERUNE_API ADRPlayerController : public APlayerController, public IGenericTeamAgentInterface
{
	GENERATED_BODY()

public:
	ADRPlayerController();

	// Team Interface
	virtual FGenericTeamId GetGenericTeamId() const override { return TeamId; }
	virtual void SetGenericTeamId(const FGenericTeamId& NewTeamId) override { TeamId = NewTeamId; }
	
	// 데미지 수치 표시
	// (코스메틱이라 한두 개 유실돼도 무방 - Unreliable로 reliable 큐 부담 제거)
	UFUNCTION(Client, Unreliable)
	void ShowDamageNumber(float DamageAmount, ACharacter* TargetCharacter);

	// 아군/적 구분 표시 변경
	UFUNCTION(BlueprintImplementableEvent, Category = "Corruption")
	void SetTeamVisualsEnabled(bool bEnabled);

	// 상호작용 이벤트
	UPROPERTY(BlueprintAssignable, Category = "Input")
	FOnInteractPressed OnInteractPressed;

	// ========== 상호작용 (부품 · 사이트 · 청소기 · 프롭 · 열차) ==========

	// 상호작용 대상 감지 + 화면 안내 (로컬). 부품 · 사이트 · 청소기의 감지 범위 오버랩이 감지를 켜고 끈다
	UDRInteractionComponent* GetInteractionComponent() const { return InteractionComponent; }

	// 화면 상호작용 안내에 그릴 키 — 입력 매핑(DRContext)에서 찾는다 (못 찾으면 F / Space)
	FKey GetInteractKey() const;
	FKey GetJumpKey() const;

	// 상호작용 요청 통합 RPC (부품 획득 / 사이트 설치)
	// 서버는 클라이언트가 보낸 포인터를 신뢰하지 않고 거리/상태를 검증한다
	UFUNCTION(Server, Reliable)
	void ServerRequestInteract(AActor* Interactable);

	// 부품 획득 요청의 서버 측 최대 허용 거리
	// (감지 반경 300 + 라인트레이스 250 + 이동/지연 여유)
	UPROPERTY(EditDefaultsOnly, Category = "Part System|Config")
	float MaxInteractDistance = 800.f;

	// ========== 탑승 시스템 (Plan3 Phase B) ==========

	// 하차 요청 (점프키 → 서버 검증 후 DismountRider)
	UFUNCTION(Server, Reliable)
	void ServerRequestDismount();

	// 탑승 중 좌우 회전 동기화: MOVE_None에선 CMC 무브 패킷이 안 나가 서버가 클라 요를 모르므로 직접 전송
	UFUNCTION(Server, Unreliable)
	void ServerSetMountedYaw(float NewYaw);

	// 지속 돌진 중 S 브레이크 → GA_VacuumDash에 GameplayEvent(Event.Dash.Brake) 전송 (Plan3 §8.2)
	UFUNCTION(Server, Reliable)
	void ServerRequestDashBrake();

	// 부품 획득 시 UI 표시
	UFUNCTION(BlueprintImplementableEvent, Category = "Part System")
	void OnPartPickedUp();

	UFUNCTION(Client, Reliable)
	void ClientShowPartPickupUI();

	// ========== 튜닝 파일 동기화 (UDRTuningSubsystem) ==========

	// 참가자 → 서버: 방장이 적용 중인 튜닝 값을 보내 달라 (ReceivedPlayer 에서 1회)
	UFUNCTION(Server, Reliable)
	void ServerRequestTuning();

	// 서버 → 참가자: 방장 튜닝 값 조각. 다 모이면 참가자는 자기 파일 대신 이 값을 쓴다 (방장이 다시 읽을 때도 온다)
	UFUNCTION(Client, Reliable)
	void ClientReceiveTuningChunk(int32 Revision, int32 ChunkIndex, int32 ChunkCount, const TArray<FDRTuningOverride>& Chunk);

	// ========== 관전 시스템 ==========

	// 관전 모드 여부
	UPROPERTY(BlueprintReadOnly, Category = "Spectating")
	bool bIsSpectating = false;

	// 대기실 상태 플래그 (ViewTarget 복원 차단용)
	bool bIsInWaitingRoom = false;

	// 현재 관전 중인 플레이어 인덱스
	int32 CurrentSpectatedPlayerIndex = 0;

	// 현재 관전 중인 캐릭터 (직접 참조)
	UPROPERTY()
	TWeakObjectPtr<ACharacter> CurrentSpectatedCharacter;

	// 관전 시작
	UFUNCTION(Client, Reliable)
	void ClientStartSpectating();

	// 관전 종료
	UFUNCTION(Client, Reliable)
	void ClientStopSpectating();

	// 다음 플레이어로 전환
	UFUNCTION(BlueprintCallable, Category = "Spectating")
	void SpectateNextPlayer();

	// 사망 후 Delay 초 뒤 관전을 시작한다 (서버). 그 전에 부활하면 CancelPendingSpectate 로 취소된다.
	void ScheduleSpectating(float Delay);
	void CancelPendingSpectate();
	UFUNCTION(BlueprintCallable, Category = "Spectating")
	void SpectatePreviousPlayer();

	// ========== 전체 화면 (설정 · 업그레이드 · 옷장 · 스테이지 선택 · 8퍼즐) ==========
	//
	// ★화면 UI 는 HUD 가 아니라 PlayerController 가 소유한다★ — 입력 모드 · 커서 · 위젯 생성/제거가 한 곳에 모여 있어야
	// 복귀 규칙이 갈라지지 않는다. 다섯 화면 모두 OpenScreen / CloseScreen 한 곳을 지난다.
	//  - 한 번에 한 화면만 열린다 (열면 열려 있던 화면은 닫힌다)
	//  - 열 때 UIOnly + 커서, 닫을 때 RestoreDefaultInputMode (메인메뉴/튜토리얼/대기실/스테이지 분기)
	//  - 위젯은 C++ 이 만든다 (위젯 클래스는 BP_DRPlayerController 에서 지정). BP 훅은 연출용이다
	// 아래 화면별 함수는 BP · 위젯 · 장치가 부르는 입구다.

	// 지금 열린 화면 (None = 없음)
	UFUNCTION(BlueprintPure, Category = "UI")
	EDRScreen GetActiveScreen() const { return ActiveScreen; }

	UFUNCTION(BlueprintPure, Category = "UI")
	bool IsScreenOpen(EDRScreen Screen) const { return Screen != EDRScreen::None && ActiveScreen == Screen; }

	// 열린 화면이 있으면 닫는다
	UFUNCTION(BlueprintCallable, Category = "UI")
	void CloseActiveScreen() { CloseScreen(ActiveScreen); }

	// 화면이 열린/닫힌 직후 (BP: 사운드 등 연출용 — ★여기서 CreateWidget 하지 말 것★, 두 장이 뜬다)
	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void OnScreenOpened(EDRScreen Screen);

	UFUNCTION(BlueprintImplementableEvent, Category = "UI")
	void OnScreenClosed(EDRScreen Screen);

	// ===== 설정 =====

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void ToggleSettingsMenu();

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void OpenSettingsMenu() { OpenScreen(EDRScreen::Settings); }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	void CloseSettingsMenu() { CloseScreen(EDRScreen::Settings); }

	UFUNCTION(BlueprintCallable, Category = "Settings")
	bool IsSettingsMenuOpen() const { return IsScreenOpen(EDRScreen::Settings); }

	// 설정 화면 위젯 클래스 (BP_DRPlayerController 에서 WBP_SettingsScreen 지정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<UUserWidget> SettingsScreenWidgetClass;

	UFUNCTION(BlueprintCallable, Category = "UI")
	void ToggleFirstPersonMeshAndHUDVisibility();

	// ========== 입력 모드 관리 ==========

	// 현재 레벨에 맞는 입력 모드로 복원
	UFUNCTION(BlueprintCallable, Category = "Input")
	void RestoreDefaultInputMode();

	// 현재 레벨이 메인메뉴인지 확인
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool IsInMainMenu() const;

	// 현재 레벨이 로비인지 확인
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool IsInLobby() const;

	// 게임 레벨인지 확인 (스테이지)
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool IsInGameLevel() const;

	// 현재 레벨이 튜토리얼인지 확인
	UFUNCTION(BlueprintCallable, Category = "Input")
	bool IsInTutorial() const;

	// 레벨 진입 시 공통 초기화 (ReceivedPlayer, PostSeamlessTravel에서 호출)
	void OnLevelEntered();

	// 게임 결과 UI 표시
	UFUNCTION(Client, Reliable)
	void Client_ShowGameOverUI();

	UFUNCTION(Client, Reliable)
	void Client_ShowGameClearUI();

	UFUNCTION(Client, Reliable)
	void Client_ShowTutorialClearUI();

	UFUNCTION(Client, Reliable)
	void ClientStopAllAudio();

	// 레벨 이동 전 설정창 닫기 (서버에서 호출)
	UFUNCTION(Client, Reliable)
	void ClientCloseSettingsMenu();

	// ========== 대기실 카메라 ==========

	// 서버 → 클라이언트: 대기실 슬롯 위치로 텔레포트
	UFUNCTION(Client, Reliable)
	void ClientTeleportToSlot(FVector SlotLocation, FRotator SlotRotation);

	// 서버 → 클라이언트: 대기실 카메라로 ViewTarget 설정
	UFUNCTION(Client, Reliable)
	void ClientSetWaitingRoomView(ADRWaitingRoomCameraActor* CameraActor);

	// 서버 → 클라이언트: 카메라를 캐릭터로 부드럽게 전환 (아래 로비 시작 연출을 시작한다)
	UFUNCTION(Client, Reliable)
	void ClientStartCameraTransitionToCharacter();

	// 카메라 전환 로직 (Pawn 존재 보장 후 호출)
	void ExecuteCameraTransitionToCharacter();

	// ========== 로비 시작 연출 (대기실 카메라 → 1인칭, 2026-11-08) ==========
	//
	// Power On 직후 대기실 카메라에서 출발해 내 캐릭터 앞으로 다가가고, 어깨 위로 넘어가며 돌아서
	// 머리 위에서 정면을 본 뒤 1인칭 눈 위치로 내려앉는다.
	//  - 연출용 카메라 액터를 이 클라이언트에만 하나 만들어 뷰 타깃으로 쓴다. 폰이 아닌 시점이라
	//    내 캐릭터도 남이 보는 모습(3인칭)으로 보이고, 주인 전용 1인칭 팔은 보이지 않는다.
	//    (예전 SetViewTargetWithBlend 는 블렌드 시작 순간부터 폰 시점 규칙이 적용돼 1인칭 팔만 허공에 떠 보였다)
	//  - 연출 동안 이동 · 시선 · 점프 · 상호작용 · 스킬 입력을 막는다 — 방향키를 누른 채 시작해도 캐릭터가 걸어가지 않는다.
	//  - 3인칭 몸은 카메라가 머리 위에서 정면을 봐 몸이 화면 아래로 빠진 뒤에 숨긴다 (내려앉는 동안 몸 속이 안 보인다).
	//  - 도착하면 뷰 타깃을 폰으로 바꾸고 1인칭 팔을 아래에서 올린다.

	// 연출 길이 (초). 서버의 FreeRoam 전환도 이 길이만큼 기다린다
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Intro Camera", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float LobbyIntroDuration = 3.5f;

	// 어깨 위로 넘어갈 때 캡슐 꼭대기보다 더 올라가는 높이 (cm) — 모자 등 머리 위 장식을 스치지 않게 넉넉히
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Intro Camera", meta = (ClampMin = "0.0"))
	float LobbyIntroOverheadClearance = 50.f;

	// 다가가며 지나는 얼굴 앞 지점 — 캡슐 앞면에서 이만큼 떨어진 곳 (cm)
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Intro Camera", meta = (ClampMin = "0.0"))
	float LobbyIntroFrontDistance = 110.f;

	// 도착 후 1인칭 팔이 아래에서 올라오는 시간 (초, 0 = 바로 제자리)
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Intro Camera", meta = (ClampMin = "0.0"))
	float LobbyIntroArmsRaiseTime = 0.35f;

	// 1인칭 팔이 올라오기 시작하는 깊이 (cm, 카메라 기준 아래쪽)
	UPROPERTY(EditDefaultsOnly, Category = "Lobby|Intro Camera", meta = (ClampMin = "0.0"))
	float LobbyIntroArmsRaiseDepth = 40.f;

	float GetLobbyIntroDuration() const { return LobbyIntroDuration; }

	// ========== 스테이지 시작 연출 (모든 플레이어 로딩 → 멀리서 다가와 1인칭으로, 2026-11-08) ==========
	//
	// 스테이지에 들어오면 조작 · HUD 를 막는다. 내 폰이 도착하고 화면이 그릴 준비를 마치면 캐릭터가 바라보는 방향 멀리에
	// 연출 카메라를 세워 두고 서버에 알린다(ServerNotifyStageLoaded) — 로딩 화면은 그대로 덮여 있다.
	// 모두 알려 오면(GameState 진행 Intro) 로딩 화면을 걷고, 다 걷히면 카메라가 내 캐릭터로 다가와 로비 연출과 같은 길
	// (얼굴 앞 → 어깨 위 → 눈)로 1인칭에 들어간다. 끝나면 조작 · HUD 를 돌려주고 서버에 알린다(ServerNotifyStageIntroFinished) —
	// 함께 시작한 플레이어가 모두 끝내면 서버가 진행을 Started 로 바꿔 첫 페이즈와 스테이지 BGM 을 시작한다 (ADRStageGameMode · ADRBGMActor).
	//  - 어깨는 옆에 선 다른 플레이어가 덜 가까운 쪽으로 넘는다 (출발 시점을 그쪽으로 조금 비켜 둔다)
	//  - 연출 카메라 · 몸 숨기기 · 팔 올리기는 로비 연출과 같은 코드다 (BeginIntroCamera ~ FinishIntroCamera)
	//  - 원격 클라는 심리스 트래블 뒤 OnLevelEntered 가 불리지 않는다 — 진행은 월드가 바뀌는 것을 보고 처음부터 시작한다

	// 다가오는 시간 (초)
	UPROPERTY(EditDefaultsOnly, Category = "Stage|Intro Camera", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float StageIntroDuration = 3.f;

	// 모두 로딩을 마친 뒤 출발 시점에 머무는 최소 시간 (초) — 로딩 화면 퇴장 애니(WBP_StageLoading 0.5초)보다 길게.
	// 걷히는 중에 움직이면 멀리서 다가오는 앞부분이 가려진다. 로딩 화면이 더 늦게 닫히면(최소 표시 시간) 닫힐 때까지 기다린다
	UPROPERTY(EditDefaultsOnly, Category = "Stage|Intro Camera", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float StageIntroHoldTime = 0.6f;

	// 출발 거리 — 캐릭터 가슴에서 바라보는 방향으로 (cm). 벽 · 문 등 보이는 면에 막히면 그 앞에서 출발한다
	UPROPERTY(EditDefaultsOnly, Category = "Stage|Intro Camera", meta = (ClampMin = "200.0"))
	float StageIntroStartDistance = 1000.f;

	// 출발 높이 — 캐릭터 가슴보다 이만큼 위 (cm). 내려다보며 다가온다
	UPROPERTY(EditDefaultsOnly, Category = "Stage|Intro Camera")
	float StageIntroStartHeight = 120.f;

	// 화면 준비(패키지 빌드에서 메시 PSO 컴파일)를 기다리는 최대 시간 (초) — 넘으면 덜 됐어도 로딩 완료로 알린다
	UPROPERTY(EditDefaultsOnly, Category = "Stage|Intro Camera", meta = (ClampMin = "0.0"))
	float StageIntroMaxRenderWaitTime = 10.f;

	float GetStageIntroDuration() const { return StageIntroDuration; }
	float GetStageIntroHoldTime() const { return StageIntroHoldTime; }

	// 이 월드(스테이지)의 시작 연출이 아직 안 끝났나 — 로딩 · 다른 플레이어 대기 포함. 로컬 컨트롤러만 true
	bool IsStageIntroPending() const;

	// 시작 연출 중인가 (로비 Power On · 스테이지 시작) — 조작 입력 · 상호작용 감지 · HUD 오버레이를 미룬다
	bool IsPlayingStartIntro() const { return bIsPlayingLobbyIntro || IsStageIntroPending(); }

	// 클라 → 서버: 이 스테이지의 로딩(폰 도착 · 화면 준비)을 마쳤다 — 모두 알려 오면 서버가 시작 연출을 튼다
	UFUNCTION(Server, Reliable)
	void ServerNotifyStageLoaded();

	// 클라 → 서버: 시작 연출이 끝났다 (조작을 돌려받았다) — 모두 알려 오면 서버가 첫 페이즈 · 스테이지 BGM 을 시작한다
	UFUNCTION(Server, Reliable)
	void ServerNotifyStageIntroFinished();

	// 모든 액터가 틱한 뒤 카메라를 갱신하기 직전 — 연출 카메라를 여기서 옮긴다 (폰 위치가 이번 프레임 값이다)
	virtual void UpdateCameraManager(float DeltaSeconds) override;

	// ========== 대기실 카메라 보호 ==========

	/** Possess 시 UE 엔진이 호출하는 ClientRestart를 오버라이드하여 대기실에서 ViewTarget 변경 차단 */
	virtual void ClientRestart_Implementation(APawn* NewPawn) override;

	/** Pawn 리플리케이션 시 엔진의 ViewTarget 자동 변경을 대기실에서 차단 */
	virtual void OnRep_Pawn() override;

	/** PlayerState 복제 도착 시점 — 클라이언트가 장착 칩을 서버에 보고하는 최초 경로 */
	virtual void OnRep_PlayerState() override;

	// 서버 → 클라이언트: 킥 당했음을 알림
	UFUNCTION(Client, Reliable)
	void ClientKicked(const FString& Reason);

	// ========== 대기실 UI ==========

	void CreateWaitingRoomUI();
	void DestroyWaitingRoomUI();
	UFUNCTION(BlueprintCallable)
	void RefreshWaitingRoomUI();

	// 서버 → 클라이언트: 대기실 UI 갱신 요청
	UFUNCTION(Client, Reliable)
	void ClientRefreshWaitingRoomUI();

	// 호스트가 Power On 클릭 시
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerRequestPowerOn();

	// 대기실 PowerOn 버튼 클릭 시 호출 (호스트=PowerOn 시도, 클라이언트=준비 토글)
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void OnPowerOnButtonPressed();

	// 클라이언트가 준비/준비 해제 토글 요청
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerToggleReady();

	// 호스트가 Kick 클릭 시
	UFUNCTION(Server, Reliable, BlueprintCallable)
	void ServerRequestKickPlayer(APlayerState* TargetPlayerState);

	// ========== 캐릭터 클래스 선택 ==========

	// 클라이언트에서 호출 → 서버에서 실행
	UFUNCTION(Server, Reliable, Category = "Character Selection")
	void ServerRequestChangeClass(bool bNext);

	// 블루프린트에서도 호출 가능한 래퍼
	UFUNCTION(BlueprintCallable, Category = "Character Selection")
	void RequestChangeClass(bool bNext);

	// Seamless Travel 시 PlayerState 값 유실 방지용 캐시
	EPlayerCharacterClass GetCachedSelectedClass() const { return CachedSelectedClass; }
	void SetCachedSelectedClass(EPlayerCharacterClass InClass) { CachedSelectedClass = InClass; }

	// ========== 업그레이드 칩 (Plan2.md 7.1 / 8.4) ==========

	// 로컬 세이브의 장착 목록을 서버에 보고 (로컬 컨트롤러 전용)
	void ReportUpgradeLoadout();

	// 클라이언트 신고 → 서버가 클래스 일치 확인 + 정화 후 PlayerState 에 싣는다
	UFUNCTION(Server, Reliable)
	void ServerReportUpgradeLoadout(EPlayerCharacterClass ForClass, const TArray<FName>& Chips);

	// ========== 코스메틱 (Plan(옷장) M3 · D-26 — 칩과 같은 경로) ==========

	// 로컬 세이브의 옷 목록(지금 고른 로봇)을 서버에 보고 (로컬 컨트롤러 전용)
	void ReportCosmeticLoadout();

	// 클라이언트 신고 → 서버가 클래스 일치 확인 + 카탈로그 정화 후 PlayerState 에 싣는다
	UFUNCTION(Server, Reliable)
	void ServerReportCosmeticLoadout(EPlayerCharacterClass ForClass, const TArray<FName>& SkinIds);

	// 서버 → 클라: 스테이지 성과 통지. 클라가 로컬 세이브에 재화를 반영한다.
	UFUNCTION(Client, Reliable)
	void Client_GrantStageReward(const FDRStageRewardReport& Report);

	// 마지막으로 반영된 보상 결과 (결과창 위젯이 조회)
	UFUNCTION(BlueprintPure, Category = "Progression|Reward")
	const FDRStageRewardResult& GetLastStageRewardResult() const { return LastStageRewardResult; }

	// ========== 로비 업그레이드 화면 ==========

	// 업그레이드 장치 상호작용으로 호출 (로컬 UI — RPC 없음). 로비에서만 열린다
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void OpenUpgradeScreen() { OpenScreen(EDRScreen::Upgrade); }

	// 닫는 순간 바뀐 장착 상태를 서버에 다시 보고한다
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void CloseUpgradeScreen() { CloseScreen(EDRScreen::Upgrade); }

	UFUNCTION(BlueprintPure, Category = "Upgrade")
	bool IsUpgradeScreenOpen() const { return IsScreenOpen(EDRScreen::Upgrade); }

	// 업그레이드 화면 위젯 클래스 (BP_DRPlayerController 에서 WBP_UpgradeScreen 지정).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UDRUpgradeScreenWidget> UpgradeScreenWidgetClass;

	// C++ 이 만든 화면 위젯 (열 때 생성, 닫을 때 제거)
	UPROPERTY(BlueprintReadWrite, Transient, Category = "UI")
	TObjectPtr<UDRUpgradeScreenWidget> UpgradeScreenWidget;

	// ========== 로비 옷장 화면 (Plan(옷장)) ==========
	//
	// 업그레이드 화면과 ★완전히 같은 구조★ 다. 옷장도 로컬 세이브만 다루므로 RPC 가 없다.

	// 옷장 상호작용으로 호출 (로컬 UI — RPC 없음). 로비에서만 열린다
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void OpenWardrobeScreen() { OpenScreen(EDRScreen::Wardrobe); }

	// 닫는 순간 고른 옷을 서버에 보고한다 (열려 있는 동안의 변경은 프리뷰에만 보인다)
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void CloseWardrobeScreen() { CloseScreen(EDRScreen::Wardrobe); }

	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	bool IsWardrobeScreenOpen() const { return IsScreenOpen(EDRScreen::Wardrobe); }

	// 옷장 화면 위젯 클래스 (BP_DRPlayerController 에서 WBP_Wardrobe 지정).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UDRWardrobeScreenWidget> WardrobeScreenWidgetClass;

	// C++ 이 만든 화면 위젯 (OpenWardrobeScreen 에서 생성, CloseWardrobeScreen 에서 제거)
	UPROPERTY(BlueprintReadWrite, Transient, Category = "UI")
	TObjectPtr<UDRWardrobeScreenWidget> WardrobeScreenWidget;

	// ========== 로비 스테이지 선택 화면 (시안: 스테이지 선택.html) ==========
	//
	// ★호스트 전용★ — 스테이지를 고르는 사람은 호스트뿐이고, 잠김/클리어 판정도 호스트의 로컬 세이브 기준이다.
	// 옷장과 같은 구조로 위젯 생성/제거는 C++ 이 한다.

	// 스테이지 선택 포털(ADRStageSelectActor) 상호작용으로 호출. 호스트가 아니거나 로비가 아니면 무시한다.
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	void OpenStageSelectScreen() { OpenScreen(EDRScreen::StageSelect); }

	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	void CloseStageSelectScreen() { CloseScreen(EDRScreen::StageSelect); }

	UFUNCTION(BlueprintPure, Category = "StageSelect")
	bool IsStageSelectScreenOpen() const { return IsScreenOpen(EDRScreen::StageSelect); }

	// 화면을 열 수 있는 상태인가 (위젯 클래스 지정 + 목록이 비어 있지 않음). 아니면 포털이 예전처럼 바로 이동한다.
	UFUNCTION(BlueprintPure, Category = "StageSelect")
	bool CanUseStageSelectScreen() const;

	/**
	 * 고른 스테이지로 전원 이동 (★호스트 전용★ — 리슨 서버의 호스트는 권한이 있으므로 RPC 없이 바로 처리한다).
	 * 맵은 클라가 보낸 문자열이 아니라 ProgressionConfig 의 StageSelectList 에서 찾는다. 잠긴 스테이지는 거절한다.
	 * @return 이동을 시작했으면 true
	 */
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	bool RequestStageTravel(FName StageId);

	// 스테이지 선택 화면 위젯 클래스 (BP_DRPlayerController 에서 WBP_StageSelect 지정).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TSubclassOf<class UDRStageSelectScreenWidget> StageSelectScreenWidgetClass;

	// C++ 이 만든 화면 위젯 (OpenStageSelectScreen 에서 생성, CloseStageSelectScreen 에서 제거)
	UPROPERTY(BlueprintReadOnly, Transient, Category = "UI")
	TObjectPtr<UDRStageSelectScreenWidget> StageSelectScreenWidget;

	// ========== 치트/디버그 ==========
	// 콘솔 치트(DR*)는 UDRCheatManager 에 있다. 이 함수는 BP 의 0 키와 DRSkipPhase 가 부른다.

	// 테스트용 페이즈 스킵 (개발 빌드 전용 — 서버가 다음 페이즈/웨이브로 넘긴다)
	UFUNCTION(BlueprintCallable, Category = "Cheat|Phase")
	void CheatSkipToNextPhase();

public:

	// ========== 카메라 피치 제한 ==========

	// 위로 바라볼 수 있는 최대 각도 (양수, 기본값 89도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "89.9"))
	float ViewPitchMax = 89.0f;

	// 아래로 바라볼 수 있는 최대 각도 (양수로 입력, 내부적으로 음수 변환됨, 기본값 89도)
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "Camera", meta = (ClampMin = "0.0", ClampMax = "89.9"))
	float ViewPitchMin = 89.0f;

protected:
	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	virtual void ReceivedPlayer() override;
	virtual void PostSeamlessTravel() override;

	// 게임 오버 위젯 클래스
	UPROPERTY(EditDefaultsOnly, Category = "UI|GameResult")
	TSubclassOf<UUserWidget> GameOverWidgetClass;

	// 게임 클리어 위젯 클래스
	UPROPERTY(EditDefaultsOnly, Category = "UI|GameResult")
	TSubclassOf<UUserWidget> GameClearWidgetClass;

	// 튜토리얼 클리어 위젯 클래스
	UPROPERTY(EditDefaultsOnly, Category = "UI|GameResult")
	TSubclassOf<UUserWidget> TutorialGameClearWidgetClass;

	// 현재 표시 중인 결과 위젯
	UPROPERTY()
	TObjectPtr<UUserWidget> CurrentResultWidget;

	// 마지막 스테이지 보상 반영 결과 (결과창 연출용)
	UPROPERTY(BlueprintReadOnly, Category = "Progression|Reward")
	FDRStageRewardResult LastStageRewardResult;

	// 지금 열린 전체 화면 (OpenScreen / CloseScreen 만 바꾼다)
	EDRScreen ActiveScreen = EDRScreen::None;

	// 설정 화면 위젯 (C++ 이 열 때 만들고 닫을 때 지운다)
	UPROPERTY(Transient)
	TObjectPtr<UUserWidget> SettingsScreenWidget;

	// ========== 대기실 UI ==========

	// 대기실 카메라 캐시 (ClientRestart에서 재고정에 사용)
	UPROPERTY()
	TWeakObjectPtr<ADRWaitingRoomCameraActor> CachedWaitingRoomCamera;

	// ClientStartCameraTransitionToCharacter에서 Pawn 대기 폴백 타이머
	FTimerHandle CameraTransitionRetryHandle;

	// Pawn 복제 도착 이벤트 처리 (타이머 재시도 대체)
	UFUNCTION()
	void HandlePossessedPawnChanged(APawn* PreviousPawn, APawn* NewPawn);

	// Pawn 도착 시 ViewTarget 복원 대기 여부
	bool bPendingViewTargetRestore = false;

	// Pawn 도착 시 카메라 전환 실행 대기 여부
	bool bPendingCameraTransition = false;

	// ===== 로비 시작 연출 상태 (이 클라이언트에서만 쓴다) =====

	// ClientStartCameraTransitionToCharacter 부터 연출이 끝날 때까지 (폰 대기 포함)
	bool bIsPlayingLobbyIntro = false;

	// ===== 시작 연출 카메라 (로비 · 스테이지 공용, 이 클라이언트에서만 쓴다) =====

	// 지금 (또는 마지막으로) 연출을 맡은 쪽
	EDRIntroCameraKind IntroCameraKind = EDRIntroCameraKind::Lobby;

	// 연출 카메라가 화면을 잡고 있다 (BeginIntroCamera ~ ResetIntroCamera)
	bool bIntroCameraActive = false;

	// 카메라가 움직이는 중 — false 면 출발 시점에 멈춰 있다 (스테이지: 다른 플레이어의 로딩을 기다리는 동안)
	bool bIntroCameraMoving = false;

	// 3인칭 몸을 이미 숨겼는가
	bool bIntroBodyHidden = false;

	// 움직이기 시작한 시각 (월드 시간) · 길이 (초)
	double IntroStartTime = 0.0;
	float IntroDuration = 0.f;

	// 출발 시점 (로비 = 대기실 카메라, 스테이지 = 바라보는 방향 멀리)
	FMinimalViewInfo IntroStartView;

	// 연출용 카메라 (로컬 스폰, 복제 안 함) · 연출 대상 폰
	TWeakObjectPtr<ACameraActor> IntroCamera;
	TWeakObjectPtr<ADRCharacter> IntroPawn;

	// 도착 후 1인칭 팔 올리기
	TWeakObjectPtr<USkeletalMeshComponent> ArmsRaiseMesh;
	FVector ArmsRaiseBaseLocation = FVector::ZeroVector;
	double ArmsRaiseStartTime = 0.0;

	// 연출 카메라를 띄워 출발 시점에 세운다 (아직 움직이지 않는다) — 실패하면 false
	bool BeginIntroCamera(ADRCharacter* DRCharacter, const FMinimalViewInfo& StartView);

	// 출발 시점에서 1인칭 눈으로 움직이기 시작한다
	void StartIntroCameraMove(float Duration);

	// 연출 카메라를 이번 프레임 위치로 옮긴다 (UpdateCameraManager 에서) — 도착하면 FinishIntroCamera
	void TickIntroCamera();

	// 연출을 끝낸다 — 뷰 타깃을 폰으로, 입력 · HUD 복원. 폰이 사라졌거나 끝까지 안 와도 이걸로 정리한다
	void FinishIntroCamera();

	// 연출 카메라만 지운다 (서버 뷰 타깃 무시도 해제)
	void ResetIntroCamera();

	// 로비 연출 상태까지 지운다 (레벨 이동 — 뷰 타깃 · 입력 모드는 OnLevelEntered 가 맞춘다)
	void ResetLobbyIntroState();

	// ===== 스테이지 시작 연출 진행 (이 클라이언트에서만 쓴다) =====

	EDRStageIntroStep StageIntroStep = EDRStageIntroStep::NotStage;

	// StageIntroStep 이 가리키는 월드 — 바뀌면(심리스 트래블) 처음부터
	TWeakObjectPtr<UWorld> StageIntroWorld;

	// 이 월드에 들어온 시각 · 화면 준비 대기 시작 · 다음 확인 시각 (실시간)
	double StageIntroEnterTime = 0.0;
	double StageRenderWaitStartTime = 0.0;
	double NextStageRenderCheckTime = 0.0;

	// 로딩 화면을 걷기 시작한 시각 (월드 시간)
	double StageIntroRevealTime = 0.0;

	// 스테이지 시작 연출 진행 (UpdateCameraManager 에서 매 프레임)
	void TickStageIntro();

	// 출발 시점 — 캐릭터가 바라보는 방향 멀리에서 캐릭터를 본다
	FMinimalViewInfo MakeStageIntroStartView(ADRCharacter& DRCharacter) const;

	void StartArmsRaise(ADRCharacter* DRCharacter);
	void TickArmsRaise();
	void StopArmsRaise();

	// 대기실 위젯 클래스 (블루프린트에서 설정)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Lobby")
	TSubclassOf<UDRWaitingRoomWidget> WaitingRoomWidgetClass;

	// 현재 대기실 위젯 인스턴스
	UPROPERTY()
	TObjectPtr<UDRWaitingRoomWidget> WaitingRoomWidget;

	// 8퍼즐 창 위젯 클래스 (BP_DRPlayerController 에서 WBP_S2SlidePuzzle 지정)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Stage2")
	TSubclassOf<UDRS2SlidePuzzleWidget> SlidePuzzleWidgetClass;

	// 현재 8퍼즐 창 인스턴스. 닫아도 파괴하지 않고 재사용한다 (Intro 연출이 매번 돌지 않게).
	UPROPERTY()
	TObjectPtr<UDRS2SlidePuzzleWidget> SlidePuzzleWidget;

public:
	// 현재 대기실 위젯 반환 (Blueprint에서 설정창 등이 참조 획득용)
	UFUNCTION(BlueprintPure, Category = "UI|Lobby")
	UDRWaitingRoomWidget* GetWaitingRoomWidget() const { return WaitingRoomWidget; }

	// 해당 InputTag 키를 지금 물리적으로 누르고 있는지.
	// OverlayWidgetController 가 "GA 종료발 Released 재방송"을 억제할지 판단하는 데 쓴다 —
	// 즉발 어빌리티(GA_VacuumJetJump)는 활성화와 같은 프레임에 EndAbility 하므로,
	// 그 종료를 Released 로 그대로 방송하면 키를 누르고 있는데도 눌림 이미지가 1프레임 만에 풀린다.
	bool IsInputTagHeld(const FGameplayTag& InputTag) const { return HeldInputTags.Contains(InputTag); }

protected:

	// LobbyState 변경 시 UI 처리
	UFUNCTION()
	void OnLobbyStateChangedForUI(ELobbyState NewState);

	// 상호작용 대상 감지 (부품 · 사이트 · 청소기 · 프롭 · 열차 칸 — 광선 거리 · 간격도 여기 있다)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Interaction")
	TObjectPtr<UDRInteractionComponent> InteractionComponent;

private:
	FGenericTeamId TeamId;

	// Enhanced Input System 설정
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> DRContext;

	// 기본 입력 액션들
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> JumpAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> InteractAction;

	// 관전 입력 액션
	UPROPERTY(EditAnywhere, Category = "Input|Spectating")
	TObjectPtr<UInputAction> SpectateNextAction;

	UPROPERTY(EditAnywhere, Category = "Input|Spectating")
	TObjectPtr<UInputAction> SpectatePreviousAction;

	// 설정 메뉴 토글 액션
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> ToggleSettingsAction;

	// 캐릭터 설명창 표시(Tab Hold) 입력 액션
	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> CharacterInfoAction;

	// 설정 메뉴 토글 처리
	void HandleToggleSettings();

	// 캐릭터 설명창 Hold 입력 처리
	void HandleCharacterInfoPressed();
	void HandleCharacterInfoReleased();

	// 튜토리얼: Tab으로 캐릭터 설명창 열림을 서버 매니저에 보고
	UFUNCTION(Server, Reliable)
	void ServerReportTutorialCharacterInfoOpened();

	// 현재 캐릭터 설명창이 표시 중인지 추적 (포커스 유실 대비)
	bool bIsCharacterInfoVisible = false;

	// Tab Hold 입력 가능한 컨텍스트인지 검사 (게임 레벨/튜토리얼이며, 대기실/설정창이 아닐 때)
	bool CanShowCharacterInfo() const;

	// 관전 입력 처리
	void HandleSpectateNext();
	void HandleSpectatePrevious();

	// 관전 대상 설정
	void SetSpectateTarget(ACharacter* NewTarget);

	UFUNCTION(Server, Reliable)
	void ServerSetSpectateTarget(ACharacter* NewTarget);

	// 관전 UI 업데이트
	UFUNCTION(Client, Reliable)
	void ClientUpdateSpectatorUI(ACharacter* SpectatedTarget);

	void UpdateSpectatorUI(ACharacter* SpectatedTarget);

	// 관전 대상 사망 처리
	UFUNCTION()
	void OnSpectatedPlayerDied(AActor* DeadActor);

	// 생존자가 한 명이면 그 사람을 관전 대상으로 고정하고 true (다음/이전 전환 공용)
	bool SpectateOnlySurvivor(const TArray<ADRCharacter*>& AliveCharacters);

	// 관전 대상의 사망 델리게이트를 옮겨 묶는다 (로컬 컨트롤러 전용 — bIsSpectating 은 로컬에만 있다)
	void BindSpectatedDeath(ACharacter* NewTarget);
	void UnbindSpectatedDeath();

	// 관전하며 다른 플레이어에게 묶어 둔 HUD 를 내 PlayerState 로 되돌린다
	void RestoreOwnOverlay();

	// 사망 → 관전 시작 예약 (서버)
	FTimerHandle PendingSpectateTimerHandle;

	// 지속 돌진 브레이크 요청 간격 (Move 가 매 프레임 불려도 RPC 는 이 간격으로)
	double LastDashBrakeRequestTime = -1000.0;
	static constexpr double DashBrakeRequestInterval = 0.25;

	// 입력 처리 함수들
	void Move(const FInputActionValue& InputActionValue);
	void Look(const FInputActionValue& InputActionValue);
	void StartJump(const FInputActionValue& InputActionValue);
	void StopJump(const FInputActionValue& InputActionValue);
	// 상호작용 키를 눌렀을 때 — ★분기 순서를 바꾸면 UDRInteractionComponent::BuildPrompt(화면 안내)도 같이 바꾼다★
	void HandleInteract();

	// 이 입력 액션에 매핑된 첫 키보드 키 (DRContext 기준, 없으면 Fallback)
	FKey GetMappedKey(const UInputAction* Action, const FKey& Fallback) const;

	// GAS 어빌리티 입력 처리
	void AbilityInputTagPressed(FGameplayTag InputTag);
	void AbilityInputTagReleased(FGameplayTag InputTag);
	void AbilityInputTagHeld(FGameplayTag InputTag);

	// 어빌리티 입력 설정
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UDRInputConfig> InputConfig;

	UDRAbilitySystemComponent* GetASC() const;

	// 해당 InputTag 의 어빌리티가 지금 차단 상태인지 (Carrying 중이거나 BlockedAbilityTags 에 걸린 상태)
	bool IsAbilityInputBlocked(const FGameplayTag& InputTag) const;

	// 현재 눌려 있는 스킬 InputTag 집합 (IsInputTagHeld 의 백킹 데이터).
	// Pressed/Released 최상단에서 갱신하고, 폰 교체 시 비운다 (키를 누른 채 폰이 바뀌면 Released 가 유실될 수 있음)
	TSet<FGameplayTag> HeldInputTags;

	// 이 컨트롤러가 리슨 서버의 호스트인지 (서버에서 실행 중인 로컬 컨트롤러)
	bool IsListenServerHost() const { return HasAuthority() && IsLocalController(); }

	// 마우스 감도 캐시 (Look()이 매 입력마다 Subsystem 체인을 타지 않도록)
	float CachedMouseSensitivity = 1.0f;

	UFUNCTION()
	void HandleMouseSensitivityChanged(float NewSensitivity);

	// 레벨 컨텍스트 캐시 (Unknown이면 다음 조회 때 재판별 - GameState 복제 지연 대비)
	mutable EDRLevelContext CachedLevelContext = EDRLevelContext::Unknown;

	// 캐시된 레벨 컨텍스트 조회
	EDRLevelContext GetLevelContext() const;

	// 맵 이름/GameState 타입으로 현재 레벨 컨텍스트 판별
	EDRLevelContext DetermineLevelContext() const;

	// 데미지 텍스트 컴포넌트 클래스
	UPROPERTY(EditDefaultsOnly)
	TSubclassOf<UDamageTextComponent> DamageTextComponentClass;

	// 표시 중인 데미지 텍스트 추적 (BP 애니메이션 종료 시 자체 파괴되므로 위크 포인터)
	TArray<TWeakObjectPtr<UDamageTextComponent>> ActiveDamageTexts;

	// 데미지 텍스트 동시 표시 상한 (AoE 다중 타격 시 컴포넌트 생성 스파이크 방지)
	UPROPERTY(EditDefaultsOnly, Category = "UI|Damage")
	int32 MaxConcurrentDamageTexts = 30;

	// Seamless Travel 시 캐릭터 클래스 보존 (PlayerController는 Travel에서 생존)
	EPlayerCharacterClass CachedSelectedClass = EPlayerCharacterClass::Gardener;

	// 탑승 요 동기화 전송 간격 타이머 / 마지막 전송 값 (클라 전용)
	float MountedYawSyncTimer = 0.f;
	float LastSentMountedYaw = 0.f;

	// 결과 화면(게임 오버 · 클리어 · 튜토리얼 클리어) 공통 표시 — 이미 떠 있으면 무시
	void ShowResultWidget(TSubclassOf<UUserWidget> WidgetClass);

	// 점프 키로 하차 요청 (마운트 하차와 동일 관례)
	UFUNCTION(Server, Reliable)
	void ServerRequestTrainDeboard();

public:
	// 8퍼즐 UI 열기 (서버 -> 요청한 클라이언트)
	UFUNCTION(Client, Reliable)
	void Client_OpenSlidePuzzleUI(ADRS2SlidePuzzle* Puzzle);

	/**
	 * 8퍼즐 화면 열기/닫기 (대기실 UI · 게임오버 · 업그레이드 · 옷장과 ★같은 구조★).
	 *
	 * ★화면 UI 는 HUD 가 아니라 PlayerController 가 소유한다★
	 * 이 프로젝트에서 ADRHUD 는 상시 표시되는 오버레이와 캐릭터 설명창만 들고,
	 * 떴다 사라지는 '창'은 전부 여기서 만든다 (CreateWaitingRoomUI / 게임오버 / 업그레이드 / 옷장).
	 * 입력 모드·커서·위젯 생성/제거가 한 곳에 모여 있어야 복귀 규칙이 갈라지지 않는다.
	 *
	 * ★입력 모드는 OpenScreen / CloseScreen 에서만 바꾼다★ (다섯 화면 공통)
	 * 복귀는 RestoreDefaultInputMode() 가 메인메뉴/튜토리얼/대기실/스테이지를 갈라서 처리한다.
	 * 위젯이나 BP 가 각자 SetInputMode 를 부르면 그 규칙이 두 곳으로 갈라진다.
	 *
	 * BP 가 할 일은 없다. 위젯 클래스만 SlidePuzzleWidgetClass 에 지정하면 된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void OpenSlidePuzzleScreen(ADRS2SlidePuzzle* Puzzle) { OpenScreen(EDRScreen::SlidePuzzle, Puzzle); }

	UFUNCTION(BlueprintCallable, Category = "S2|Puzzle")
	void CloseSlidePuzzleScreen() { CloseScreen(EDRScreen::SlidePuzzle); }

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	bool IsSlidePuzzleScreenOpen() const { return IsScreenOpen(EDRScreen::SlidePuzzle); }

	UFUNCTION(BlueprintPure, Category = "S2|Puzzle")
	UDRS2SlidePuzzleWidget* GetSlidePuzzleWidget() const { return SlidePuzzleWidget; }

	// 8퍼즐이 사용 중이라 열지 못했다 (서버 -> 요청한 클라이언트)
	UFUNCTION(Client, Reliable)
	void Client_SlidePuzzleBusy(ADRS2SlidePuzzle* Puzzle, APlayerState* Occupant);

	/** 다른 사람이 조작 중이라 열리지 않았다 (BP 연출 훅 — 선택). "OOO 님이 사용 중" 안내를 띄운다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Puzzle")
	void OnSlidePuzzleScreenBusy(APlayerState* Occupant);

	/**
	 * 8퍼즐 조작 요청 (클라 -> 서버). 2026-09-20 개정.
	 *
	 * ★왜 퍼즐 액터가 아니라 여기에 다는가★
	 * Server RPC 는 그 액터를 클라가 소유할 때만 전달된다. ADRS2SlidePuzzle 은 레벨에 놓인
	 * 무소유 액터라 RPC 가 조용히 버려진다. 클라가 소유한 액터는 자기 PlayerController 뿐이다.
	 *
	 * 판정(점유자 검사 · 인접 검사)은 전부 액터 안에서 한다. 여기서는 위임만 한다.
	 * 해결 판정도 서버가 직접 하므로 "풀었다" 보고 RPC 는 없다 - 그게 있으면 퍼즐 건너뛰기 치트가 된다.
	 */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "S2|Puzzle")
	void Server_SlidePuzzleMove(ADRS2SlidePuzzle* Puzzle, int32 TileId);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "S2|Puzzle")
	void Server_SlidePuzzleUndo(ADRS2SlidePuzzle* Puzzle);

	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "S2|Puzzle")
	void Server_SlidePuzzleReset(ADRS2SlidePuzzle* Puzzle);

	/** 창을 닫았다 -> 조작 점유권 반납. 이게 안 오면 다음 사람이 퍼즐을 못 연다. */
	UFUNCTION(Server, Reliable, BlueprintCallable, Category = "S2|Puzzle")
	void Server_SlidePuzzleRelease(ADRS2SlidePuzzle* Puzzle);

	// 원래의 private 구역으로 복귀 (이 아래 멤버들의 접근 수준을 바꾸지 않기 위함)
private:

	// 부품 드랍 요청
	UFUNCTION(Server, Reliable)
	void ServerRequestDropPart();
	
	// FreeRoam 진입 시 HUD 오버레이 초기화 (대기실에서 스킵된 경우)
	void InitOverlayForFreeRoam();

	// 서버에서 페이즈 스킵 실행
    UFUNCTION(Server, Reliable)
    void ServerCheatSkipToNextPhase();

	// DRDie / DRRevive 의 서버 쪽 (사망 · 부활은 서버가 멀티캐스트한다)
	UFUNCTION(Server, Reliable)
	void ServerCheatDie();

	UFUNCTION(Server, Reliable)
	void ServerCheatRevive();

	// 콘솔 치트가 위 서버 RPC 를 부른다 (치트 매니저는 복제되지 않아 RPC 를 직접 가질 수 없다)
	friend class UDRCheatManager;

	// ===== 전체 화면 공통 처리 =====

	// 화면을 연다. 열 수 없으면(로비 전용 · 호스트 전용 · 위젯 클래스 미지정) false. 다른 화면이 열려 있으면 먼저 닫는다
	bool OpenScreen(EDRScreen Screen, ADRS2SlidePuzzle* Puzzle = nullptr);

	// 그 화면이 열려 있으면 닫는다. bLevelTransition = 레벨 이동 정리 (입력 모드 복구 · 서버 재보고는 OnLevelEntered 가 한다)
	void CloseScreen(EDRScreen Screen, bool bLevelTransition = false);

	bool CanOpenScreen(EDRScreen Screen, const ADRS2SlidePuzzle* Puzzle) const;
	TSubclassOf<UUserWidget> GetScreenWidgetClass(EDRScreen Screen) const;

	// 화면 위젯 생성 · 표시 (실패하면 false) / 제거
	bool ShowScreenWidget(EDRScreen Screen, ADRS2SlidePuzzle* Puzzle);
	void HideScreenWidget(EDRScreen Screen);
};
