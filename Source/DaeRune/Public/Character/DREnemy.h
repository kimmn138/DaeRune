// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "Interaction/EnemyInterface.h"
#include "UI/WidgetController/DRAttributeChangedDelegate.h"
#include "DREnemy.generated.h"

class UBlackboardComponent;
class UDRBillboardWidgetComponent;
class UBehaviorTree;
class ADRAIController;
class UShapeComponent;
class UNiagaraSystem;
class UNiagaraComponent;

// Blackboard 키 상수 (하드코딩 방지)
namespace DRBlackboardKeys
{
	// AI 상태
	inline const FName HitReacting = TEXT("HitReacting");
	inline const FName RangedAttacker = TEXT("RangedAttacker");
	inline const FName HomeLocation = TEXT("HomeLocation");
	inline const FName Dead = TEXT("Dead");
	inline const FName Stunned = TEXT("Stunned");

	// 부품 시스템
	inline const FName HasPart = TEXT("HasPart");

	// 광폭화 시스템
	inline const FName IsEnraged = TEXT("bIsEnraged");
	inline const FName AttackSpeed = TEXT("AttackSpeed");
	inline const FName EliteAttackSpeed = TEXT("EliteAttackSpeed");

	// 타겟팅
	inline const FName FirstAttacker = TEXT("FirstAttacker");
	inline const FName HasFirstAttacker = TEXT("HasFirstAttacker");
	inline const FName TargetToFollow = TEXT("TargetToFollow");
	// ★Plan8(시선집중)★ 타깃 서비스들이 쓰는 거리 키 (Plan8 D-16)
	inline const FName DistanceToTarget = TEXT("DistanceToTarget");
	// ★Plan8(시선집중)★ BTS_CheckValidTarget 의 "벽에 가림" 타이머 — 도발 동안 0 으로 되돌린다 (Plan8 D-16)
	inline const FName UnreachableSince = TEXT("UnreachableSince");

	// DragonFly 전용
	inline const FName IsLockedDown = TEXT("IsLockedDown");
	inline const FName BasicAttackCount = TEXT("BasicAttackCount");
}

/**
 * 적 캐릭터 기본 클래스
 */
UCLASS()
class DAERUNE_API ADREnemy : public ADRCharacterBase, public IEnemyInterface
{
	GENERATED_BODY()
	
public:
	ADREnemy();
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// AI 컨트롤러에 의해 소유될 때 실행
	virtual void PossessedBy(AController* NewController) override;

	/** Combat Interface */
	virtual int32 GetPlayerLevel_Implementation() override;
	virtual void Die(const FVector& DeathImpulse) override;
	virtual void SetCombatTarget_Implementation(AActor* InCombatTarget) override;
	virtual AActor* GetCombatTarget_Implementation() const override;
	/** end Combat Interface */

	UBlackboardComponent* GetBlackboardComponent() const;

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

	// 어그로 상태
	UPROPERTY(BlueprintReadWrite, Replicated, Category = "Combat")
	bool bIsAggroed = false;

	// 히트 리액션 중 이동 속도
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat")
	float HitReactingMoveSpeed = 200.f;

	// 사망 후 생존 시간
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	float LifeSpan = 2.f;

	// 넉백 상태 설정/해제
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void SetKnockbackState(bool bInKnockback);

	// ===== 튜토리얼 더미 시스템 =====

	// 튜토리얼 샌드백 모드 (움직이지 않음, 공격 안 함, 무적)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tutorial")
	bool bIsTutorialDummy = false;

	// 튜토리얼 매니저 참조 (데미지 적중 보고용)
	UPROPERTY(BlueprintReadWrite, Category = "Tutorial")
	TWeakObjectPtr<AActor> TutorialManagerRef;

	// ===== 부품 시스템 추가 =====
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Part System")
	TObjectPtr<UStaticMeshComponent> PartMeshComponent;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, ReplicatedUsing = OnRep_bCarriesPart, Category = "Part System")
	bool bCarriesPart = false;

	UPROPERTY(EditDefaultsOnly, Category = "Part System")
	TSubclassOf<AActor> PartActorClass;

	UPROPERTY(EditDefaultsOnly, Category = "Part System")
	float PartDropForce = 300.f;

	UFUNCTION(BlueprintCallable, Category = "Part System")
	bool DropPart();

	UFUNCTION(BlueprintPure, Category = "Part System")
	bool HasPart() const { return bCarriesPart && !bPartDropped; }

	// 서버 권위 setter — Phase1이 런타임에 부품 운반자 지정 시 호출
	UFUNCTION(BlueprintCallable, Category = "Part System")
	void SetCarriesPart(bool bNewCarriesPart);

	UFUNCTION()
	void OnRep_bCarriesPart();

	// 광폭화 시스템

	UPROPERTY(BlueprintReadWrite, Category = "Enemy|Phase")
	bool bIsPhase3Enemy = false;

	// 페이즈3 웨이브 외곽선 레벨 (0 = 외곽선 없음, 1~5 = 웨이브 레벨)
	// 스폰 시 한 번만 세팅되며 살아있는 동안 변하지 않음
	UFUNCTION(BlueprintCallable, Category = "Enemy|Phase")
	void SetWaveOutlineLevel(uint8 NewLevel);

	UPROPERTY(BlueprintReadOnly, Category = "Enemy|Combat")
	bool bIsEnraged = false;

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Combat")
	float EnrageHealthThreshold = 0.2f; // 20%

	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Combat")
	TSubclassOf<UGameplayEffect> EnrageMovementSpeedGE;

	// 광폭화 시 공격 속도 배율 (기본값: 0.5 = 2배 빠름)
	UPROPERTY(EditDefaultsOnly, Category = "Enemy|Combat", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float EnrageAttackSpeedMultiplier = 0.5f;

	void TriggerEnrage();

	// ★Plan5(공기탄칩)★ 보스 여부(bIsBoss) 읽기 전용 — 공기탄칩② 흡착이 보스를 거른다 (Plan5 T-100)
	bool IsBoss() const { return bIsBoss; }

protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;
	virtual void InitializeDefaultAttributes() const override;
	virtual void StunTagChanged(const FGameplayTag CallbackTag, int32 NewCount) override;

	// ========== 도발 표시 (벨보이 시선 집중) ==========

	/**
	 * 도발당한 동안(Debuff.Taunted — GE_BellBoy_Taunted) 머리 위에 붙는 이펙트.
	 * 태그는 Minimal 복제로 모든 클라에 오므로 모두에게 보인다. 도발이 풀리거나 죽으면 꺼진다.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "VFX|Taunt")
	TObjectPtr<UNiagaraSystem> TauntedVFX;

	/** 붙일 메시 소켓. 비우면 캡슐 꼭대기(+TauntedVFXHeightOffset) */
	UPROPERTY(EditDefaultsOnly, Category = "VFX|Taunt")
	FName TauntedVFXSocket = NAME_None;

	/** 캡슐 꼭대기(또는 소켓)에서 위로 띄우는 높이(uu, 캡슐 기준이면 액터 스케일을 따른다) */
	UPROPERTY(EditDefaultsOnly, Category = "VFX|Taunt")
	float TauntedVFXHeightOffset = 40.f;

	void TauntedTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	virtual void RefreshTagDrivenVFX() override;

	virtual float GetMoveSpeed() override;

	// 액터 회전이 ControlRotation 을 따라잡는 보간 속도 (Tick 의 RInterpTo).
	// 기본값 10.0 은 기존 하드코딩 값과 동일하므로 기존 적의 동작은 변하지 않는다.
	// 두더지 보스처럼 "느리게 도는" 연출이 필요한 적만 BP 에서 낮춘다. (Plan7 §5.2)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat", meta = (ClampMin = "0.1"))
	float RotationInterpSpeed = 10.f;

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

	// 죽을 때 자동 발동되는 어빌리티들
	UPROPERTY(BlueprintReadOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> DeathAbilities;

	// Death Ability 활성화
	void ActivateDeathAbilities();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UDRBillboardWidgetComponent> HealthBar;

	// ===== 히트박스 시스템 =====

	// BP에서 "Hitbox" 태그를 달아 추가한 커스텀 히트박스 컴포넌트 목록
	// BeginPlay에서 자동 수집됨. 비어있으면 기본 CapsuleComponent로 히트 판정
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Hitbox")
	TArray<TObjectPtr<UShapeComponent>> HitboxComponents;

	// 커스텀 히트박스 사용 여부 (HitboxComponents가 있으면 true)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat|Hitbox")
	bool bUseCustomHitbox = false;

private:
	// "Hitbox" 태그가 달린 컴포넌트를 자동 수집하고 콜리전을 설정
	void SetupHitboxComponents();
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

	bool bPartDropped = false;

	// 도발 표시 이펙트 — 처음 걸릴 때 만들고 이후엔 켜고 끄기만 한다
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> TauntedVFXComponent;

protected:
	// 외곽선 레벨 (초기 복제만 — 스폰 시 고정)
	UPROPERTY(ReplicatedUsing = OnRep_WaveOutlineLevel)
	uint8 WaveOutlineLevel = 0;

	UFUNCTION()
	void OnRep_WaveOutlineLevel();

	// 메시에 Custom Depth Stencil 값을 적용 (포스트프로세스 외곽선용)
	// 서브클래스에서 추가 메시(예: Armadillo BallFormMesh)에도 적용하려면 오버라이드
	virtual void ApplyWaveOutline();
};
