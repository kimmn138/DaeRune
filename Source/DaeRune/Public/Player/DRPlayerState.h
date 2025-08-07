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

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChanged, bool, bIsInCombat);

/**
 * 
 */
UCLASS()
class DAERUNE_API ADRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ADRPlayerState();
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 전투 시스템
	void EnterCombat();
	void ExitCombat();
	bool IsInCombat() const { return bIsInCombat; }

	// 현재 컨테이너 인덱스 계산
	int32 GetCurrentContainerIndex() const;

	// 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnCombatStateChanged OnCombatStateChanged;

	// 부패 상태 체크
	bool IsPlayerCorrupted() const;

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

	UPROPERTY(ReplicatedUsing = OnRep_IsInCombat)
	bool bIsInCombat = false;

	UFUNCTION()
	void OnRep_IsInCombat();

private:
	FTimerHandle CombatTimerHandle;

	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float CombatExitDelay = 10.0f; // 10초로 변경

	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects")
	TSubclassOf<UGameplayEffect> HealthRegenEffectClass;

	FActiveGameplayEffectHandle HealthRegenEffectHandle;

	void StartHealthRegen();
	void StopHealthRegen();
	void CheckCombatExit();

	// 최적화를 위한 캐싱
	FGameplayEffectSpecHandle CachedHealthRegenSpec;
	void InitializeHealthRegenSpec();
};
