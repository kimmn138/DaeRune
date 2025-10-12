// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "DRPlayerState.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayEffect;

// 전투 상태 변경 알림 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChangedSignature, bool, bIsInCombat);
// 부패 상태 변경 알림 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCorruptedStateChangedSignature, bool, bIsCorrupted);

/**
 * DaeRune 플레이어의 게임 상태 관리 클래스
 */
UCLASS()
class DAERUNE_API ADRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ADRPlayerState();
	// GAS 인터페이스 구현
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// 네트워크 리플리케이션 설정
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 전투 시스템
	void EnterCombat();
	void ExitCombat();
	bool IsInCombat() const { return bIsInCombat; }

	// 컨테이너 시스템 연동
	int32 GetCurrentContainerIndex() const;

	// UI 및 게임플레이 알림 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnCombatStateChangedSignature OnCombatStateChanged;

	UPROPERTY(BlueprintAssignable)
	FOnCorruptedStateChangedSignature OnCorruptedStateChanged;

	// 부패 상태 시스템
	void SetCorruptedState(bool bNewCorrupted);
	bool IsPlayerCorrupted() const;

protected:
	virtual void BeginPlay() override;

	// GAS 컴포넌트들
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

	// 네트워크 동기화 변수들
	UPROPERTY(ReplicatedUsing = OnRep_IsInCombat)
	bool bIsInCombat = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsCorrupted)
	bool bIsCorrupted = false;

	// 리플리케이션 콜백
	UFUNCTION()
	void OnRep_IsInCombat();

	UFUNCTION()
	void OnRep_IsCorrupted();

private:
	// 전투 상태 타이머 관리
	FTimerHandle CombatTimerHandle;

	// 전투 종료 대기 시간
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float CombatExitDelay = 10.0f;

	// 체력 회복 시스템
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects")
	TSubclassOf<UGameplayEffect> HealthRegenEffectClass;

	FActiveGameplayEffectHandle HealthRegenEffectHandle;

	void StartHealthRegen();
	void StopHealthRegen();
	void CheckCombatExit();
	void CheckAndStartHealthRegen();
	void CheckHealthRegenStatus();

	// 체력 변경 감지
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	// 최적화를 위한 캐싱
	FGameplayEffectSpecHandle CachedHealthRegenSpec;
	void InitializeHealthRegenSpec();
};
