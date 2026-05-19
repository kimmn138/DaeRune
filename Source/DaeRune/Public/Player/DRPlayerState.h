// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/PlayerState.h"
#include "GameplayEffectTypes.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRPlayerState.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayEffect;

// 전투 상태 변경 알림 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCombatStateChangedSignature, bool, bIsInCombat);
// 오염 상태 변경 알림 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCorruptedStateChangedSignature, bool, bIsCorrupted);
// 캐릭터 클래스 변경 알림 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPlayerClassChanged, ADRPlayerState*, PlayerState, EPlayerCharacterClass, NewClass);

/**
 * DaeRune �÷��̾��� ���� ���� ���� Ŭ����
 */
UCLASS()
class DAERUNE_API ADRPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
public:
	ADRPlayerState();
	// GAS �������̽� ����
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// ��Ʈ��ũ ���ø����̼� ����
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ���� �ý���
	void EnterCombat();
	void ExitCombat();
	bool IsInCombat() const { return bIsInCombat; }

	// �����̳� �ý��� ����
	int32 GetCurrentContainerIndex() const;

	// UI �� �����÷��� �˸� ��������Ʈ
	UPROPERTY(BlueprintAssignable)
	FOnCombatStateChangedSignature OnCombatStateChanged;

	UPROPERTY(BlueprintAssignable)
	FOnCorruptedStateChangedSignature OnCorruptedStateChanged;

	// 오염 상태 시스템
	void SetCorruptedState(bool bNewCorrupted);
	bool IsPlayerCorrupted() const;

	// ========== 대기실 슬롯 ==========

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	int32 GetWaitingRoomSlotIndex() const { return WaitingRoomSlotIndex; }

	void SetWaitingRoomSlotIndex(int32 NewIndex);

	// ========== 캐릭터 클래스 선택 ==========

	UFUNCTION(BlueprintCallable, Category = "Character Selection")
	EPlayerCharacterClass GetSelectedPlayerClass() const { return SelectedPlayerClass; }

	void SetSelectedPlayerClass(EPlayerCharacterClass NewClass);

	UPROPERTY(BlueprintAssignable, Category = "Character Selection")
	FOnPlayerClassChanged OnPlayerClassChanged;

protected:
	virtual void BeginPlay() override;

	// Seamless Travel 시 커스텀 프로퍼티 복사
	virtual void CopyProperties(APlayerState* PlayerState) override;

	// GAS ������Ʈ��
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UAttributeSet> AttributeSet;

	// ��Ʈ��ũ ����ȭ ������
	UPROPERTY(ReplicatedUsing = OnRep_IsInCombat)
	bool bIsInCombat = false;

	UPROPERTY(ReplicatedUsing = OnRep_IsCorrupted)
	bool bIsCorrupted = false;

	// 대기실 슬롯 인덱스
	UPROPERTY(ReplicatedUsing = OnRep_WaitingRoomSlotIndex, BlueprintReadOnly, Category = "Lobby")
	int32 WaitingRoomSlotIndex = -1;

	// 캐릭터 클래스 선택
	UPROPERTY(ReplicatedUsing = OnRep_SelectedPlayerClass, BlueprintReadOnly, Category = "Character Selection")
	EPlayerCharacterClass SelectedPlayerClass = EPlayerCharacterClass::Gardener;

	// 리플리케이션 콜백
	UFUNCTION()
	void OnRep_IsInCombat();

	UFUNCTION()
	void OnRep_IsCorrupted();

	UFUNCTION()
	void OnRep_WaitingRoomSlotIndex();

	UFUNCTION()
	void OnRep_SelectedPlayerClass();

private:
	// ���� ���� Ÿ�̸� ����
	FTimerHandle CombatTimerHandle;

	// ���� ���� ��� �ð�
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	float CombatExitDelay = 5.0f;

	// ü�� ȸ�� �ý���
	UPROPERTY(EditDefaultsOnly, Category = "Gameplay Effects")
	TSubclassOf<UGameplayEffect> HealthRegenEffectClass;

	FActiveGameplayEffectHandle HealthRegenEffectHandle;

	void StartHealthRegen();
	void StopHealthRegen();
	void CheckCombatExit();
	void CheckAndStartHealthRegen();
	void CheckHealthRegenStatus();

	// ü�� ���� ����
	void OnHealthChanged(const FOnAttributeChangeData& Data);

	// ����ȭ�� ���� ĳ��
	FGameplayEffectSpecHandle CachedHealthRegenSpec;
	void InitializeHealthRegenSpec();
};
