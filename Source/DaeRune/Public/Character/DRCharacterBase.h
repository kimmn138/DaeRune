// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Interaction/CombatInterface.h"
#include "DRCharacterBase.generated.h"

struct FOnAttributeChangeData;
class UDebuffNiagaraComponent;
class UNiagaraSystem;
class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayEffect;
class UGameplayAbility;
class UAnimMontage;

/**
 * 모든 캐릭터의 베이스 클래스
 */
UCLASS(Abstract)
class DAERUNE_API ADRCharacterBase : public ACharacter, public IAbilitySystemInterface, public ICombatInterface
{
	GENERATED_BODY()

public:
	ADRCharacterBase();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const;
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const { return AttributeSets; }
	void SetLevel(int32 NewLevel) { Level = NewLevel; }

	/** Combat Interface */
	virtual UAnimMontage* GetHitReactMontage_Implementation() override;
	virtual void Die(const FVector& DeathImpulse) override;
	virtual FOnDeathSignature& GetOnDeathDelegate() override;
	virtual FVector GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag) override;
	virtual bool IsDead_Implementation() const override;
	virtual AActor* GetAvatar_Implementation() override;
	virtual TArray<FTaggedMontage> GetAttackMontages_Implementation() override;
	virtual UNiagaraSystem* GetBloodEffect_Implementation() override;
	virtual FTaggedMontage GetTaggedMontageByTag_Implementation(const FGameplayTag& MontageTag) override;
	virtual int32 GetMinionCount_Implementation() override;
	virtual ECharacterClass GetCharacterClass_Implementation() override;
	virtual FOnASCRegistered& GetOnASCRegisteredDelegate() override;
	virtual USkeletalMeshComponent* GetWeapon_Implementation() override;
	virtual void SetIsBeingShocked_Implementation(bool bInShock) override; 
	virtual bool IsBeingShocked_Implementation() const override;
	/** end Combat Interface */

	// 델리게이트
	FOnASCRegistered OnAscRegistered;
	FOnDeathSignature OnDeathDelegate;

	// 사망 처리 RPC
	UFUNCTION(NetMulticast, Reliable)
	virtual void MulticastHandleDeath(const FVector& DeathImpulse);

	// ========== 부활 (Plan6 §5.9) ==========
	// 사망 처리(MulticastHandleDeath)의 역연산. 같은 폰을 되살린다.
	// 스테이지2 방4 두더지 클리어 시 사망자를 부활시키는 데 사용한다.
	// HealthRatio = 최대 체력에 대한 비율 (0.5 = 50%)
	UFUNCTION(BlueprintCallable, Category = "Combat|Revive")
	virtual void Revive(const FVector& ReviveLocation, float HealthRatio = 0.5f, float WaterRatio = 0.5f);

	// 체력 · 물을 최대치에 대한 비율로 맞춘다 (서버). 새 폰으로 부활시킬 때도 쓴다.
	void SetReviveAttributes(float HealthRatio, float WaterRatio);

	// 부활 상태 복원 RPC (전 클라 공통 연출/컴포넌트 복원)
	UFUNCTION(NetMulticast, Reliable)
	virtual void MulticastHandleRevive();

	// BP 측 부활 연출 훅.
	// ★Dissolve 는 BP 타임라인으로 진행되므로 머티리얼 파라미터 원복은 여기서 처리해야 한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Revive")
	void K2_OnCharacterRevived();

	// 공격 몽타주 배열
	UPROPERTY(EditAnywhere, Category = "Combat")
	TArray<FTaggedMontage> AttackMontages;

	// 디버프 상태
	UPROPERTY(ReplicatedUsing=OnRep_Stunned, BlueprintReadOnly)
	bool bIsStunned = false;

	UPROPERTY(ReplicatedUsing=OnRep_Burned, BlueprintReadOnly)
	bool bIsBurned = false;

	UPROPERTY(Replicated, BlueprintReadOnly)
	bool bIsBeingShocked = false;

	// RepNotify 함수들
	UFUNCTION()
	virtual void OnRep_Stunned();

	UFUNCTION()
	virtual void OnRep_Burned();

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	// 부활 직전 서버 처리 (파생: 오염 해제 등). 체력 · 물 비율을 맞추기 전에 불린다.
	virtual void PrepareRevive() {}

	// 플레이어 사망 후 시체 정리 타이머 — 그 전에 부활하면 Revive 가 취소한다
	FTimerHandle DeathCleanupTimerHandle;

	// 레벨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	int32 Level = 1;
	
	// 무기 컴포넌트
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<USkeletalMeshComponent> Weapon;

public:
	// 소켓 이름들
	UPROPERTY(EditAnywhere, Category = "Combat")
	FName WeaponTipSocketName;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName LeftHandSocketName;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName RightHandSocketName;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName TailSocketName;

public:
	// 사망 상태 (서버에서 MulticastHandleDeath로 변경, OnRep_Dead로 클라 동기화)
	UPROPERTY(ReplicatedUsing = OnRep_Dead, BlueprintReadOnly, Category = "Combat|Death")
	bool bDead = false;

	UFUNCTION()
	virtual void OnRep_Dead();

protected:
	// 스턴 태그 콜백
	virtual void StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	// 태그로 켜고 끄는 몸 이펙트(공속 버프 · 도발 표시)를 지금 태그 · 사망 상태에 맞춘다.
	// 사망/부활 멀티캐스트 끝에서도 불린다 — 죽은 몸에 이펙트가 남지 않게
	virtual void RefreshTagDrivenVFX() {}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float BaseWalkSpeed = 250.f;

	// 스턴 상태에서의 이동 속도 (기본값: 0 = 움직일 수 없음)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float StunnedMoveSpeed = 0.f;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Abilities")
	TObjectPtr<UAttributeSet> AttributeSets;

	virtual void InitAbilityActorInfo();

	// 기본 속성 GameplayEffect
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultVitalAttributes;
	
	void ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass) const;
	virtual void InitializeDefaultAttributes() const;

	void AddCharacterAbilities();

	virtual void OnMoveSpeedChanged(const FOnAttributeChangeData& Data);

	virtual float GetMoveSpeed();

	// Dissolve 효과
	void Dissolve();

	UFUNCTION(BlueprintImplementableEvent)
	void StartDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);

	UFUNCTION(BlueprintImplementableEvent)
	void StartWeaponDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> DissolveMaterialInstance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UMaterialInstance> WeaponDissolveMaterialInstance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	UNiagaraSystem* BloodEffect;

	// 소환수 관련
	int32 MinionCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	ECharacterClass CharacterClass = ECharacterClass::Warrior;

	// 디버프 나이아가라 컴포넌트들
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDebuffNiagaraComponent> BurnDebuffComponent;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UDebuffNiagaraComponent> StunDebuffComponent;

private:
	// 시작 시 부여할 어빌리티들
	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupPassiveAbilities;

	// 피격 몽타주
	UPROPERTY(EditAnywhere, Category = "Combat")
	TArray<TObjectPtr<UAnimMontage>> HitReactMontages;
};
