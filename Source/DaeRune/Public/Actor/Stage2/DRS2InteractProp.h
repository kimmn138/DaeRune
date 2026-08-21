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

	// 실제 처리 (서버). 자식이 소유 퍼즐에 위임한다.
	virtual void ServerHandleInteract(ADRCharacter* Character) {}

	// 프롭 활성/비활성 (서버). 퍼즐이 끝나면 조작을 막는다.
	UFUNCTION(BlueprintCallable, Category = "S2|Prop")
	void SetPropEnabled(bool bNewEnabled);

	// IDRInteractable
	virtual void SetInteractionUIVisible(bool bShow) override;

	int32 GetPropIndex() const { return PropIndex; }

protected:
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
};

/**
 * 스위치 퍼즐의 레버 (Plan6 §14.2.3)
 * PropIndex = 레버 번호 0~4
 */
UCLASS()
class DAERUNE_API ADRS2Lever : public ADRS2InteractProp
{
	GENERATED_BODY()

public:
	virtual void ServerHandleInteract(ADRCharacter* Character) override;
};

/**
 * 금고 숫자 버튼 (Plan6 §14.2.5)
 * PropIndex = 숫자 0~9. 음수면 입력 초기화 버튼으로 동작한다.
 */
UCLASS()
class DAERUNE_API ADRS2SafeButton : public ADRS2InteractProp
{
	GENERATED_BODY()

public:
	virtual void ServerHandleInteract(ADRCharacter* Character) override;

protected:
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

public:
	virtual void ServerHandleInteract(ADRCharacter* Character) override;
};
