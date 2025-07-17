// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameplayTagContainer.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "CombatInterface.generated.h"

class UAbilitySystemComponent;
class UNiagaraSystem;
class UAnimMontage;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnASCRegistered, UAbilitySystemComponent*); // ASC 등록 이벤트 델리게이트 선언임
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathSignature, AActor*, DeadActor); // 사망 이벤트 델리게이트 선언임

USTRUCT(BlueprintType)
struct FTaggedMontage // 태그 기반 몽타주 데이터 구조체임
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	UAnimMontage* Montage = nullptr; // 재생할 애니메이션 몽타주 참조임

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag MontageTag; // 몽타주 식별용 태그임

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag SocketTag; // 소켓 식별용 태그임

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	USoundBase* ImpactSound = nullptr; // 임팩트 사운드 참조임
};

// UCombatInterface
//
// 전투 관련 기능 제공용 인터페이스 클래스 선언임
UINTERFACE(MinimalAPI, BlueprintType)
class UCombatInterface : public UInterface
{
	GENERATED_BODY()
};


/**
 * ICombatInterface
 *
 * 전투 동작, 사망, 어빌리티 시스템 연동 등
 * 대상 객체가 구현해야 하는 인터페이스 정의임
 */
class DAERUNE_API ICombatInterface
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	/** 플레이어 레벨 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent)
	int32 GetPlayerLevel();

	/** 몽타주 태그에 해당하는 소켓 위치 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	FVector GetCombatSocketLocation(const FGameplayTag& MontageTag);

	/** 타겟 방향 업데이트 이벤트 함수 선언임 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void UpdateFacingTarget(const FVector& Target);

	/** 히트 리액트 몽타주 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	UAnimMontage* GetHitReactMontage();

	/** 사망 처리 순수 가상 함수 선언임 */
	virtual void Die(const FVector& DeathImpulse) = 0;
	/** 사망 델리게이트 반환 순수 가상 함수 선언임 */
	virtual FOnDeathSignature& GetOnDeathDelegate() = 0;

	/** 사망 여부 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	bool IsDead() const;

	/** 아바타 Actor 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	AActor* GetAvatar();

	/** 공격 몽타주 리스트 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	TArray<FTaggedMontage> GetAttackMontages();

	/** 피 이펙트 시스템 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	UNiagaraSystem* GetBloodEffect();

	/** 태그로 지정된 FTaggedMontage 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	FTaggedMontage GetTaggedMontageByTag(const FGameplayTag& MontageTag);

	/** 미니언 수 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	int32 GetMinionCount();

	/** 미니언 수 증가 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void IncremenetMinionCount(int32 Amount);

	/** 플레이어 클래스 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	EPlayerCharacterClass GetPlayerCharacterClass();

	/** 적 클래스 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	EEnemyCharacterClass GetEnemyCharacterClass();

	/** ASC 등록 델리게이트 반환 순수 가상 함수 선언임 */
	virtual FOnASCRegistered& GetOnASCRegisteredDelegate() = 0;

	/** 충격 루프 상태 설정 이벤트 함수 선언임 */
	UFUNCTION(BlueprintImplementableEvent, BlueprintCallable)
	void SetInShockLoop(bool bInLoop);

	/** 장착 무기 컴포넌트 반환 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	USkeletalMeshComponent* GetWeapon();

	/** 피격 충격 상태 확인 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	bool IsBeingShocked() const;

	/** 피격 충격 상태 설정 함수 선언임 */
	UFUNCTION(BlueprintNativeEvent, BlueprintCallable)
	void SetIsBeingShocked(bool bInShock);
};
