// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DRInteractable.h"
#include "DRS2InteractProp.generated.h"

class UWidgetComponent;
class ADRCharacter;

/**
 * 스테이지2 상호작용 프롭 공용 베이스 (Plan6 §4.10.1)
 *
 * 방2의 조작 대상(레버 5 / 되돌리기·초기화 버튼 / 금고 숫자 버튼 10+)을 전부 이 베이스의
 * 자식 액터로 만든다. 조작 대상 1개 = 액터 1개로 두면 기존 상호작용 배관
 * (타입별 감지 슬롯 + ServerRequestInteract 타입 분기)에 분기 1개만 추가하면 된다.
 *
 * 상태는 프롭이 아니라 소유 퍼즐 액터가 보유·복제한다.
 * 프롭 20여 개가 각자 복제하면 트래픽과 코드가 불필요하게 늘어나기 때문이다.
 */
UCLASS(Abstract)
class DAERUNE_API ADRS2InteractProp : public AActor, public IDRInteractable
{
	GENERATED_BODY()

public:
	ADRS2InteractProp();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 상호작용 가능 여부 (감지/실행 양쪽에서 사용)
	UFUNCTION(BlueprintCallable, Category = "S2|Prop")
	virtual bool CanInteract(const ADRCharacter* Character) const;

	// 상호작용 공용 진입점 (서버). PlayerController 가 이것을 호출한다.
	// ★비가상이다. 쿨다운을 먼저 소비한 뒤 자식의 ExecuteInteract 를 부른다.
	//   자식이 쿨다운 처리를 빠뜨릴 수 없게 하려는 구조다.
	void ServerHandleInteract(ADRCharacter* Character);

	// 프롭 활성/비활성 (서버). 퍼즐이 끝나면 조작을 막는다.
	UFUNCTION(BlueprintCallable, Category = "S2|Prop")
	void SetPropEnabled(bool bNewEnabled);

	// IDRInteractable
	virtual void SetInteractionUIVisible(bool bShow) override;

	int32 GetPropIndex() const { return PropIndex; }

protected:
	// 자식이 구현하는 실제 처리 (서버). 소유 퍼즐에 위임한다.
	// 쿨다운 통과 후에만 호출된다.
	virtual void ExecuteInteract(ADRCharacter* Character) {}

	// ★루트는 빈 SceneComponent 다 (2026-08-27 변경).
	//   PropMesh 를 루트로 두면 PropMesh->SetRelativeLocation 이 사실상 월드 좌표가 되어
	//   눌림 연출을 만들 수 없었다. 루트를 분리해 PropMesh 를 자식으로 내리면
	//   상대 좌표가 정상 동작하고, 액터를 금고 문에 붙여도 연출이 그대로 유지된다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Prop")
	TObjectPtr<USceneComponent> SceneRoot;

	// 연속 조작 방지 대기 시간(초). 0 이면 제한 없음.
	//
	// 입력이 ETriggerEvent::Started 라 한 번 누르면 한 번만 발화한다(2026-08-27 수정).
	// 따라서 연타 방지는 **의도적인 게임 디자인**일 때만 값을 준다 (레버 0.4초).
	// 금고 버튼처럼 빠른 연속 입력이 필요한 곳은 0 으로 둔다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Prop", meta = (ClampMin = "0"))
	float InteractCooldown = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Prop")
	TObjectPtr<UStaticMeshComponent> PropMesh;

	// "F" 상호작용 프롬프트 (로컬 코스메틱)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Prop")
	TObjectPtr<UWidgetComponent> InteractionWidget;

	// 소속 퍼즐/금고 액터. 레벨에서 배선한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Prop")
	TObjectPtr<AActor> OwnerPuzzle;

	// 타일/레버/버튼 번호. 금고 숫자 버튼은 0~9 를 그대로 사용한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Prop")
	int32 PropIndex = INDEX_NONE;

	UPROPERTY(ReplicatedUsing = OnRep_bPropEnabled, BlueprintReadOnly, Category = "S2|Prop")
	bool bPropEnabled = true;

	UFUNCTION()
	void OnRep_bPropEnabled();

	// 조작 연출 진입점 (전 클라 공통). 자식이 C++ 연출을 추가할 때 오버라이드한다.
	// 기본 구현은 BP 훅 OnInteractedVisual() 만 호출한다.
	virtual void PlayInteractedVisual();

	// 누름/당김 연출 (BP 훅). 사운드·VFX 용도.
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Prop")
	void OnInteractedVisual();

	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Prop")
	void OnPropEnabledChanged(bool bNowEnabled);

	// 연출 재생 (전 클라)
	UFUNCTION(NetMulticast, Unreliable)
	void Multicast_PlayInteractedVisual();

private:
	// 마지막 조작 시각 (서버 전용). 프롭 단위이므로 누가 눌렀든 함께 적용된다.
	float LastInteractTime = -BIG_NUMBER;
};

/**
 * 스위치 퍼즐의 레버 (Plan6 §14.2.3)
 * PropIndex = 레버 번호 0~4
 *
 * ★자세(올림/내림)는 레버가 스스로 토글하지 않는다.
 *   레버 ON/OFF 의 진실은 퍼즐의 LeverBits 이며, 라운드 성공 시 퍼즐이 이를 0 으로 초기화한다.
 *   레버가 자체 토글로 자세를 만들면 그 초기화를 놓쳐 "올라간 채로 남는" 버그가 생긴다.
 *   따라서 BeginPlay 에서 퍼즐에 자기를 등록하고, 퍼즐이 SetLeverOn() 으로 자세를 밀어준다.
 */
UCLASS()
class DAERUNE_API ADRS2Lever : public ADRS2InteractProp
{
	GENERATED_BODY()

public:
	ADRS2Lever();

	virtual void Tick(float DeltaSeconds) override;

	// 퍼즐이 LeverBits 변경 시 호출한다 (서버·클라 공통).
	void SetLeverOn(bool bNewOn);

protected:
	virtual void BeginPlay() override;
	virtual void ExecuteInteract(ADRCharacter* Character) override;

	// ★레버가 설치된 받침/하우징. 회전하지 않고 제자리에 고정된다.
	//   조준 감지는 액터 단위이므로(HitResult.GetActor()) 이 메시를 맞혀도 레버로 인식된다.
	//   → **상호작용 범위가 넓어진다.**
	//   PropMesh 는 움직이는 레버 손잡이, LeverBaseMesh 는 고정된 설치부다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Lever")
	TObjectPtr<UStaticMeshComponent> LeverBaseMesh;

	// ★배치 회전을 기준으로 한 **상대 변화량**이다. 절대 회전이 아니다.
	//   덕분에 레버를 어느 방향으로 배치하든 항상 자기 축으로 회전한다.
	//   FRotator = (Pitch, Yaw, Roll) 이므로 Y축 회전은 Pitch 다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Lever")
	FRotator OnRotation = FRotator(85.f, 0.f, 0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Lever")
	FRotator OffRotation = FRotator(-25.f, 0.f, 0.f);

	// 두 자세 사이 전환 시간. 0 이면 즉시 스냅한다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Lever", meta = (ClampMin = "0"))
	float ToggleDuration = 0.15f;

private:
	// Alpha 0 = OFF 자세, 1 = ON 자세
	void ApplyPose(float Alpha);

	FQuat OffQuat = FQuat::Identity;
	FQuat OnQuat = FQuat::Identity;

	bool bLeverOn = false;
	bool bPoseInitialized = false;

	float CurrentAlpha = 0.f;
	float FromAlpha = 0.f;
	float ToAlpha = 0.f;
	float Elapsed = 0.f;
	bool bAnimating = false;
};

/** 금고 버튼의 종류 (Plan6 §14.2.5) */
UENUM(BlueprintType)
enum class ES2SafeButtonType : uint8
{
	// 숫자 입력. PropIndex(0~9) 가 입력될 숫자다.
	Digit   UMETA(DisplayName = "숫자 (0~9)"),

	// 맨 뒤 1자리를 지운다
	Delete  UMETA(DisplayName = "Delete - 한 자리 지우기"),

	// 입력 전체를 지운다
	Reset   UMETA(DisplayName = "Reset - 전체 지우기"),

	// 현재 입력을 정답과 비교한다. ★이 버튼을 눌러야 판정이 일어난다.
	Enter   UMETA(DisplayName = "Enter - 확인"),
};

/**
 * 금고 버튼 (Plan6 §14.2.5)
 *
 * ButtonType 으로 4종을 구분한다. Digit 일 때만 PropIndex(0~9) 를 쓴다.
 * 자릿수가 다 차도 자동 판정하지 않으며, Enter 를 눌러야 금고가 열린다.
 */
UCLASS()
class DAERUNE_API ADRS2SafeButton : public ADRS2InteractProp
{
	GENERATED_BODY()

public:
	ADRS2SafeButton();

	virtual void Tick(float DeltaSeconds) override;

	// 금고 문에 붙어 함께 회전할지 (금고가 등록 시 확인한다)
	bool ShouldAttachToDoor() const { return bAttachToDoor; }

protected:
	virtual void BeginPlay() override;
	virtual void ExecuteInteract(ADRCharacter* Character) override;

	// ★눌림 연출을 C++ 에서 처리한다 (2026-08-27).
	//   BP 타임라인으로 만들면 커브 키·Length 설정 실수로 "들어간 채 안 돌아오는" 문제가
	//   생기기 쉬워, 레버 자세와 동일하게 C++ 로 옮겼다. BP 에는 사운드만 넣으면 된다.
	virtual void PlayInteractedVisual() override;

	// PropMesh 의 **상대 좌표** 기준 눌리는 방향·깊이
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Button")
	FVector PressOffset = FVector(-2.f, 0.f, 0.f);

	// 들어가는 시간 (짧을수록 "딸깍" 느낌)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Button", meta = (ClampMin = "0.01"))
	float PressInDuration = 0.05f;

	// 돌아오는 시간
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Button", meta = (ClampMin = "0.01"))
	float PressOutDuration = 0.15f;

	// 버튼 종류. Digit 이면 PropIndex 가 입력 숫자가 된다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Prop")
	ES2SafeButtonType ButtonType = ES2SafeButtonType::Digit;

	// ★true 면 BeginPlay 에서 금고의 DoorMesh 에 부착되어 문이 열릴 때 함께 움직인다.
	//   키패드가 문이 아니라 옆 벽면에 있다면 false 로 둔다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "S2|Prop")
	bool bAttachToDoor = true;

private:
	// 눌림 애니메이션 시작 (전 머신 로컬)
	void StartPressAnimation();

	// BP 에서 배치한 PropMesh 의 원래 상대 위치 (여기서 PressOffset 만큼 이동한다)
	FVector PressBaseLocation = FVector::ZeroVector;

	float PressElapsed = 0.f;
	bool bPressing = false;
};

/**
 * UI 퍼즐 단말 (8퍼즐 - Plan6 §14.2.2, UI 방식으로 변경)
 * 상호작용하면 조작한 플레이어에게 퍼즐 UI를 띄운다.
 */
UCLASS()
class DAERUNE_API ADRS2PuzzleTerminal : public ADRS2InteractProp
{
	GENERATED_BODY()

protected:
	virtual void ExecuteInteract(ADRCharacter* Character) override;
};
