// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "Interaction/EnemyInterface.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "DREnemy.generated.h"

class UWidgetComponent;
class UBehaviorTree;
class ADRAIController;

/**
 * 적 캐릭터 기본 클래스
 */
UCLASS()
class DAERUNE_API ADREnemy : public ADRCharacterBase, public IEnemyInterface
{
	GENERATED_BODY()
	
public:
	ADREnemy();
	// AI 컨트롤러에 의해 소유될 때 실행
	virtual void PossessedBy(AController* NewController) override;

	/** Combat Interface */
	virtual int32 GetPlayerLevel_Implementation() override;
	virtual void Die(const FVector& DeathImpulse) override;
	virtual void SetCombatTarget_Implementation(AActor* InCombatTarget) override;
	virtual AActor* GetCombatTarget_Implementation() const override;
	/** end Combat Interface */

	// 현재 전투 대상
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	TObjectPtr<AActor> CombatTarget;

	// 체력 변화 이벤트 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnMaxHealthChanged;

	// 공격 실행 시 물 보상 감소 처리
	UFUNCTION(BlueprintCallable, Category = "Water System")
	void OnAttackExecuted();

	// 히트 리액션 태그 변화 콜백
	void HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	// 히트 리액션 상태 플래그
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bHitReacting = false;

	// 사망 후 생존 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float LifeSpan = 5.f;

	// 넉백 상태 설정/해제
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetKnockbackState(bool bInKnockback);

protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;
	virtual void InitializeDefaultAttributes() const override;
	virtual void StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount) override;

	// 적 레벨
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	int32 Level = 1;

	// 체력바 UI 위젯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UWidgetComponent> HealthBar;

	// AI 비헤이비어 트리
	UPROPERTY(EditAnywhere, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	// AI 컨트롤러 참조
	UPROPERTY()
	TObjectPtr<ADRAIController> DRAIController;

	// 공격당 물 보상 감소량 (음수값)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water System", meta = (ClampMin = "0.0"))
	float WaterReductionPerAttack = -10.f;

	// 물 폭발 반경 (일반 적)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water System", meta = (ClampMin = "100.0"))
	float WaterExplosionRadius = 500.f;

	// 보스 여부 (전체 맵 플레이어에게 물 지급)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Water System")
	bool bIsBoss = false;

	// 물 감소 이펙트 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Water System")
	TSubclassOf<UGameplayEffect> WaterReductionEffectClass;

	// 물 지급 이펙트 클래스
	UPROPERTY(EditDefaultsOnly, Category = "Water System")
	TSubclassOf<UGameplayEffect> WaterGrantEffectClass;

	// ========== 벽 충돌 기절 시스템 추가 ==========

	// Hit 이벤트 핸들러
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	// 벽 스턴 적용 함수
	void ApplyWallStun();

	// 스턴 면역 종료
	void EndStunImmunity();

	// 넉백 상태 플래그
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Wall Stun")
	bool bIsBeingKnockedBack = false;

	// 충돌 강도 임계값
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Wall Stun", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
	float MinSpeedForStun = 50.f;

	// 스턴 지속 시간
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Wall Stun", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float WallStunDuration = 5.0f;

	// 스턴 면역 시간 (스턴 종료 후)
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Wall Stun", meta = (ClampMin = "0.0", ClampMax = "10.0"))
	float StunImmunityDuration = 5.0f;

private:
	// 물 보상 감소 처리
	void ReduceWaterReward();
	// 플레이어들에게 물 지급
	void GrantWaterToPlayers();

	// 공격 횟수 카운터 (물 보상 감소용)
	UPROPERTY()
	int32 AttackCount = 0;

	// 스턴 면역 상태
	bool bIsStunImmune = false;

	// 스턴 면역 타이머
	FTimerHandle StunImmunityTimerHandle;
};
