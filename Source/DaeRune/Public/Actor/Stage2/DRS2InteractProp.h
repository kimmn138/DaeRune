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

	// 연속 조작 방지 대기 시간(초). 0 이면 제한 없음.
	// 레버처럼 연타가 곤란한 프롭만 값을 준다.
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

	// 누름/당김 연출
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

/**
 * 금고 숫자 버튼 (Plan6 §14.2.5)
 * PropIndex = 숫자 0~9. 음수면 입력 초기화 버튼으로 동작한다.
 */
UCLASS()
class DAERUNE_API ADRS2SafeButton : public ADRS2InteractProp
{
	GENERATED_BODY()

protected:
	virtual void ExecuteInteract(ADRCharacter* Character) override;

	// true 면 숫자 입력이 아니라 입력 초기화 버튼으로 동작한다.
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "S2|Prop")
	bool bIsClearButton = false;
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
