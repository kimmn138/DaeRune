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

	// ===== 부품 시스템 추가 =====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Part System")
	TObjectPtr<UStaticMeshComponent> PartMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Part System")
	bool bCarriesPart = false;

	UPROPERTY(EditDefaultsOnly, Category = "Part System")
	TSubclassOf<AActor> PartActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Part System")
	float PartDropForce = 300.f;

	UFUNCTION(BlueprintCallable, Category = "Part System")
	bool DropPart();

	UFUNCTION(BlueprintPure, Category = "Part System")
	bool HasPart() const { return bCarriesPart && PartMeshComponent && PartMeshComponent->IsVisible(); }

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

	// ========== 占쏙옙 占썸돌 占쏙옙占쏙옙 占시쏙옙占쏙옙 占쌩곤옙 ==========

	// Hit 占싱븝옙트 占쌘들러
	UFUNCTION()
	void OnHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComponent, FVector NormalImpulse, const FHitResult& Hit);

	// 占쏙옙 占쏙옙占쏙옙 占쏙옙占쏙옙 占쌉쇽옙
	void ApplyWallStun();

	// 占쏙옙占쏙옙 占썽역 占쏙옙占쏙옙
	void EndStunImmunity();

	// 넉백 상태 플래그
	UPROPERTY(BlueprintReadOnly, Category = "Combat|Wall Stun")
	bool bIsBeingKnockedBack = false;

	// 占썸돌 占쏙옙占쏙옙 占쌈계값
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Wall Stun", meta = (ClampMin = "100.0", ClampMax = "2000.0"))
	float MinSpeedForStun = 50.f;

	// 占쏙옙占쏙옙 占쏙옙占쏙옙 占시곤옙
	UPROPERTY(EditDefaultsOnly, Category = "Combat|Wall Stun", meta = (ClampMin = "0.5", ClampMax = "5.0"))
	float WallStunDuration = 5.0f;

	// 占쏙옙占쏙옙 占썽역 占시곤옙 (占쏙옙占쏙옙 占쏙옙占쏙옙 占쏙옙)
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

	// 占쏙옙占쏙옙 占썽역 占쏙옙占쏙옙
	bool bIsStunImmune = false;

	// 占쏙옙占쏙옙 占썽역 타占싱몌옙
	FTimerHandle StunImmunityTimerHandle;

	bool bPartDropped = false;
};
