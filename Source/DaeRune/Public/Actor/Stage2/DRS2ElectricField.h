// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "DRS2ElectricField.generated.h"

class ADRCharacter;
class UAudioComponent;
class UDecalComponent;
class UGameplayEffect;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class USoundBase;
class USphereComponent;

/**
 * 두더지 보스 전용 전기장 (Plan7 §4.5 / §2.4)
 *
 * 3차 장애물 구간에서, 보스가 새로 솟아오를 때 "직전에 솟아올랐던 자리"에 생성된다.
 * 동시에 존재하는 전기장은 항상 1개이며 보스가 소유·파괴한다 (링 버퍼 — Plan7 §6.4).
 *
 * ★ADRPoisonGasActor 를 상속하지 않는다.
 *   ① 부모는 BeginPlay 에 3초 경고가 하드코딩되어 있고
 *   ② static OverlapCountMap 을 스테이지1 독가스와 공유하며
 *   ③ 인원수별 데미지를 넣을 SetByCaller 경로가 없다.
 *   따라서 로직은 새로 쓰고 에셋(데칼 머티리얼·나이아가라·사운드)만 재사용한다.
 *
 * ★데미지 대상은 플레이어뿐이다. "Player" 액터 태그가 아니라 Cast<ADRCharacter> 로 판별한다 —
 *   그 태그는 C++ 어디에서도 부여하지 않고 BP 설정에만 존재해 레벨 실수에 취약하다.
 */
UCLASS()
class DAERUNE_API ADRS2ElectricField : public AActor
{
	GENERATED_BODY()

public:
	ADRS2ElectricField();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/**
	 * 서버 전용. 스폰 직후 1회 호출한다.
	 * @param InInstigatorActor 데미지 출처로 기록할 액터 (보스)
	 * @param InRadius          효과 반경. 0 이하면 DefaultRadius 사용
	 * @param InPlayerCount     ★GE 스펙 레벨로 쓴다 = 커브 조회 레벨 = 인원수(1~4).
	 *                          데미지 수치는 GE 의 Modifier(ScalableFloat)가 이 레벨로 커브를 읽어 결정한다.
	 */
	UFUNCTION(BlueprintCallable, Category = "S2|Field")
	void InitField(AActor* InInstigatorActor, float InRadius, int32 InPlayerCount);

	UFUNCTION(BlueprintPure, Category = "S2|Field")
	float GetFieldRadius() const { return FieldRadius; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Field")
	TObjectPtr<USceneComponent> SceneRoot;

	// 플레이어 감지용. Pawn 만 오버랩한다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Field")
	TObjectPtr<USphereComponent> EffectSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Field")
	TObjectPtr<UDecalComponent> GroundDecal;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Field")
	TObjectPtr<UNiagaraComponent> FieldFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Field")
	TObjectPtr<UAudioComponent> LoopAudio;

	/**
	 * 주기 데미지 GE. ★스테이지1 GE_PoisonDamage 와 같은 구조로 만든다.
	 *
	 *   Duration Policy = Infinite
	 *   Period          = 틱 간격 (0.5)
	 *   Modifier        = DRAttributeSet.IncomingDamage, Add,
	 *                     Magnitude = ScalableFloat → CT_Damage / Abilities.MoleBoss.ElectricField, Value = 1
	 *   Executions      = 없음
	 *
	 * ★ExecCalc_Damage 를 쓰지 않는 이유: 지역 위험물은 넉백·사망 임펄스·디버프를 컨텍스트에
	 *   실을 필요가 없고, IncomingDamage 에 값만 넣으면 UDRPlayerAttributeSet::HandleIncomingDamage
	 *   가 컨테이너 체력·부식·업그레이드 칩·피격 큐를 전부 처리한다. GE_PoisonDamage 가 같은 방식이다.
	 * ★데미지 타입 태그가 컨텍스트에 없으므로 디버프(스턴)가 원천적으로 걸리지 않는다.
	 * ★수치는 InitField 가 넘긴 인원수를 스펙 레벨로 삼아 Modifier 의 커브가 결정한다 — 코드에 숫자가 없다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Field")
	TSubclassOf<UGameplayEffect> FieldDamageEffectClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Field")
	TObjectPtr<UMaterialInterface> FieldDecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Field")
	TObjectPtr<UNiagaraSystem> FieldNiagaraSystem;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Field")
	TObjectPtr<USoundBase> FieldLoopSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Field")
	float DefaultRadius = 150.f;

	// 생성 후 활성화까지의 지연. 두더지 융기 자체가 이미 텔레그래프라 기본 0(즉시)이다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Field", meta = (ClampMin = "0.0"))
	float ActivationDelay = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Field")
	float DecalProjectionDepth = 300.f;

	UPROPERTY(ReplicatedUsing = OnRep_FieldRadius, BlueprintReadOnly, Category = "S2|Field")
	float FieldRadius = 150.f;

	UFUNCTION()
	void OnRep_FieldRadius();

	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Field")
	void OnFieldActivated(float InRadius);

	UFUNCTION()
	void OnSphereBegin(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnSphereEnd(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

private:
	/**
	 * ★InitField 는 SpawnActorDeferred 경로에서 BeginPlay 보다 먼저 불린다.
	 *   그 시점엔 오버랩 델리게이트가 아직 바인딩되지 않았으므로 즉시 활성화하면 안 된다.
	 *   그래서 예약만 해 두고 BeginPlay 이후에 StartActivation 이 실제로 켠다.
	 */
	void StartActivation();
	void ActivateField();
	void ApplyToPlayer(ADRCharacter* Player);
	void RemoveFromPlayer(ADRCharacter* Player);
	void RemoveFromAll();
	void ApplyVisual();

	/** GE 스펙 레벨 = 인원수. Modifier 의 ScalableFloat 이 이 레벨로 커브를 읽는다. */
	int32 FieldLevel = 1;

	bool bActivated = false;
	bool bPendingActivation = false;
	bool bBegunPlay = false;

	UPROPERTY()
	TWeakObjectPtr<AActor> FieldInstigator;

	// 현재 이 전기장이 GE 를 걸어 둔 플레이어들
	TMap<TWeakObjectPtr<ADRCharacter>, FActiveGameplayEffectHandle> AppliedHandles;

	FTimerHandle ActivationTimer;
};
