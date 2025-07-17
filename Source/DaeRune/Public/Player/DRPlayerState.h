// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "DRPlayerState.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;

/**
 * ADRPlayerState
 *
 * 플레이어 상태 및 GAS 인터페이스 구현 클래스임
 */
UCLASS()
class DAERUNE_API ADRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ADRPlayerState();
	// 복제할 프로퍼티 등록 함수 재정의임
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// GAS 컴포넌트 반환 함수 재정의임
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// 어트리뷰트 세트 반환 헬퍼 함수임
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// 플레이어 레벨 반환 인라인 함수임
	FORCEINLINE int32 GetPlayerLevel() const { return Level; }

protected:
	// GAS 어빌리티 시스템 컴포넌트 멤버임
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	// GAS 어트리뷰트 세트 멤버임
	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

private:
	// 플레이어 레벨 복제 프로퍼티임
	UPROPERTY(VisibleAnywhere, ReplicatedUsing = OnRep_Level)
	int32 Level = 1;

	// 레벨 변경 복제 콜백 함수 선언임
	UFUNCTION()
	void OnRep_Level(int32 OldLevel);
};
