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
 * 적 캐릭터 클래스 정의
 */
UCLASS()
class DAERUNE_API ADREnemy : public ADRCharacterBase, public IEnemyInterface
{
	GENERATED_BODY()
	
public:
	ADREnemy();
	// 소유 처리 메서드 재정의
	virtual void PossessedBy(AController* NewController) override;

	/** Combat Interface */
	// 레벨 반환 구현
	virtual int32 GetPlayerLevel_Implementation() override;
	// 적 클래스 반환 구현
	virtual EEnemyCharacterClass GetEnemyCharacterClass_Implementation() override;
	// 사망 처리 구현
	virtual void Die(const FVector& DeathImpulse) override;
	// 전투 대상 설정 구현
	virtual void SetCombatTarget_Implementation(AActor* InCombatTarget) override;
	// 전투 대상 반환 구현
	virtual AActor* GetCombatTarget_Implementation() const override;
	/** end Combat Interface */

	// 컴뱃 대상 액터 변수
	UPROPERTY(BlueprintReadWrite, Category = "Combat")
	TObjectPtr<AActor> CombatTarget;

	// 체력 변경 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnHealthChanged;

	// 최대 체력 변경 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnMaxHealthChanged;

	void HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	// 피격 반응 상태 불리언 변수
	UPROPERTY(BlueprintReadOnly, Category = "Combat")
	bool bHitReacting = false;

	// 수명 지속 시간 변수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float LifeSpan = 5.f;

protected:
	virtual void BeginPlay() override;
	// 어빌리티 액터 정보 초기화 메서드 재정의
	virtual void InitAbilityActorInfo() override;
	// 기본 특성 초기화 메서드 재정의
	virtual void InitializeDefaultAttributes() const override;
	// 기절 태그 변경 처리 메서드 재정의
	virtual void StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount) override;

	// 적 레벨 변수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	int32 Level = 1;

	// 적 클래스 변수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	EEnemyCharacterClass CharacterClass = EEnemyCharacterClass::Warrior;

	// 체력바 위젯 컴포넌트 포인터
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UWidgetComponent> HealthBar;

	// 행동 트리 자산 포인터
	UPROPERTY(EditAnywhere, Category = "AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	// AI 컨트롤러 포인터
	UPROPERTY()
	TObjectPtr<ADRAIController> DRAIController;
};
