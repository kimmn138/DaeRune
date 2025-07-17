// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Interaction/CombatInterface.h"
#include "DRCharacterBase.generated.h"

class UDebuffNiagaraComponent;
class UNiagaraSystem;
class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayEffect;
class UGameplayAbility;
class UAnimMontage;

/**
 * ADRCharacterBase
 *
 * 기본 플레이어 및 적 캐릭터 행동 및 GAS 인터페이스 구현 클래스임
 */
UCLASS(Abstract)
class DAERUNE_API ADRCharacterBase : public ACharacter, public IAbilitySystemInterface, public ICombatInterface
{
	GENERATED_BODY()

public:
	ADRCharacterBase();
	// 복제 프로퍼티 등록 함수 재정의 선언임
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;
	// GAS 컴포넌트 반환 함수 재정의 선언임
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	// 어트리뷰트 세트 반환 헬퍼 선언임
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }

	/** Combat Interface */
	/** CombatInterface 구현: 히트 리액트 몽타주 반환 함수 선언 */
	virtual UAnimMontage* GetHitReactMontage_Implementation() override;
	/** CombatInterface 구현: 캐릭터 사망 처리 함수 선언 */
	virtual void Die(const FVector& DeathImpulse) override;
	/** CombatInterface 구현: 사망 델리게이트 반환 함수 선언 */
	virtual FOnDeathSignature& GetOnDeathDelegate() override;
	/** CombatInterface 구현: 소켓 위치 반환 함수 선언 */
	virtual FVector GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag) override;
	/** CombatInterface 구현: 생존 여부 반환 함수 선언 */
	virtual bool IsDead_Implementation() const override;
	/** CombatInterface 구현: 아바타 액터 반환 함수 선언 */
	virtual AActor* GetAvatar_Implementation() override;
	/** CombatInterface 구현: 공격 몽타주 배열 반환 함수 선언 */
	virtual TArray<FTaggedMontage> GetAttackMontages_Implementation() override;
	/** CombatInterface 구현: 피 이펙트 반환 함수 선언 */	
	virtual UNiagaraSystem* GetBloodEffect_Implementation() override;
	/** CombatInterface 구현: 태그로 몽타주 검색 함수 선언 */
	virtual FTaggedMontage GetTaggedMontageByTag_Implementation(const FGameplayTag& MontageTag) override;
	/** CombatInterface 구현: 미니언 수 반환 함수 선언 */
	virtual int32 GetMinionCount_Implementation() override;
	/** CombatInterface 구현: 미니언 수 증가 함수 선언 */
	virtual void IncremenetMinionCount_Implementation(int32 Amount) override;
	/** CombatInterface 구현: ASC 등록 델리게이트 반환 함수 선언 */
	virtual FOnASCRegistered& GetOnASCRegisteredDelegate() override;
	/** CombatInterface 구현: 무기 메쉬 반환 함수 선언 */
	virtual USkeletalMeshComponent* GetWeapon_Implementation() override;
	/** CombatInterface 구현: 충격 상태 설정 함수 선언 */
	virtual void SetIsBeingShocked_Implementation(bool bInShock) override; 
	/** CombatInterface 구현: 충격 상태 여부 반환 함수 선언 */
	virtual bool IsBeingShocked_Implementation() const override;
	/** end Combat Interface */

	/** OnASCRegistered 델리게이트 멤버 변수 선언 */
	FOnASCRegistered OnAscRegistered;
	/** OnDeath 델리게이트 멤버 변수 선언 */
	FOnDeathSignature OnDeathDelegate;

	/** 네트워크 멀티캐스트용 사망 처리 RPC 선언 */
	UFUNCTION(NetMulticast, Reliable)
	virtual void MulticastHandleDeath(const FVector& DeathImpulse);

	/** 시작 시 부여할 능력 몽타주 배열 멤버 변수 선언 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	TArray<FTaggedMontage> AttackMontages;

	/** 스턴 상태 복제용 프로퍼티 선언 */
	UPROPERTY(ReplicatedUsing=OnRep_Stunned, BlueprintReadOnly)
	bool bIsStunned = false;

	/** 화상 상태 복제용 프로퍼티 선언 */
	UPROPERTY(ReplicatedUsing=OnRep_Burned, BlueprintReadOnly)
	bool bIsBurned = false;

	/** 감전 상태 복제용 프로퍼티 선언 */
	UPROPERTY(Replicated, BlueprintReadOnly)
	bool bIsBeingShocked = false;

	/** 스턴 복제 콜백 함수 선언 */
	UFUNCTION()
	virtual void OnRep_Stunned();

	/** 화상 복제 콜백 함수 선언 */
	UFUNCTION()
	virtual void OnRep_Burned();

protected:
	virtual void BeginPlay() override;

	/** 무기 메쉬 컴포넌트 선언 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USkeletalMeshComponent> Weapon;

	/** 무기 팁 소켓 이름 프로퍼티 선언 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName WeaponTipSocketName;

	/** 왼손 소켓 이름 프로퍼티 선언 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName LeftHandSocketName;

	/** 오른손 소켓 이름 프로퍼티 선언 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName RightHandSocketName;

	/** 꼬리 소켓 이름 프로퍼티 선언 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName TailSocketName;

	/** 사망 상태 플래그 멤버 변수 선언 */
	bool bDead = false;

	/** 스턴 태그 변경 콜백 함수 선언 */
	virtual void StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	/** 기본 이동 속도 프로퍼티 선언 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float BaseWalkSpeed = 250.f;

	/** AbilitySystemComponent 멤버 변수 선언 */
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	/** AttributeSet 멤버 변수 선언 */
	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

	/** ASC 초기화 헬퍼 함수 선언 */
	virtual void InitAbilityActorInfo();

	/** 셀프에게 이펙트 적용 헬퍼 함수 선언 */
	void ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const;
	/** 기본 속성 초기화 함수 선언 */
	virtual void InitializeDefaultAttributes() const;

	/** 능력 부여 함수 선언 */
	void AddCharacterAbilities();

	/** 디졸브 처리 함수 선언 */
	void Dissolve();

	/** 디졸브 타임라인 시작 이벤트(블루프린트 구현) 선언 */
	UFUNCTION(BlueprintImplementableEvent)
	void StartDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);

	/** 무기 디졸브 타임라인 시작 이벤트(블루프린트 구현) 선언 */
	UFUNCTION(BlueprintImplementableEvent)
	void StartWeaponDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);

	/** 디졸브 머티리얼 인스턴스 프로퍼티 선언 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> DissolveMaterialInstance;

	/** 무기 디졸브 머티리얼 인스턴스 프로퍼티 선언 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> WeaponDissolveMaterialInstance;

	/** 피 이펙트 파티클 프로퍼티 선언 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	UNiagaraSystem* BloodEffect;

	/** 사망 사운드 프로퍼티 선언 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	USoundBase* DeathSound;

	/** 미니언 수 멤버 변수 선언 */
	int32 MinionCount = 0;

	/** 화상 디버프 파티클 컴포넌트 선언 */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDebuffNiagaraComponent> BurnDebuffComponent;

	/** 스턴 디버프 파티클 컴포넌트 선언 */
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDebuffNiagaraComponent> StunDebuffComponent;

private:
	/** 시작 능력 클래스 배열 선언 */
	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	/** 시작 패시브 능력 클래스 배열 선언 */
	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupPassiveAbilities;

	/** 히트 리액트 몽타주 프로퍼티 선언 */
	UPROPERTY(EditAnywhere, Category = "Combat")
	TObjectPtr<UAnimMontage> HitReactMontage;
};
