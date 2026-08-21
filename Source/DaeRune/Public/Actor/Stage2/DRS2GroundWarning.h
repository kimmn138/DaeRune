// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRS2GroundWarning.generated.h"

class UDecalComponent;
class UNiagaraComponent;
class UNiagaraSystem;
class UMaterialInterface;

/**
 * 원형 지면 경고 (Plan7 §4.4)
 *
 * 두더지 보스의 굴착 강습이 솟아오를 지점을 미리 알려 주는 텔레그래프다.
 * 서버가 스폰하고 SetLifeSpan 으로 스스로 사라지므로 별도 정리 코드가 필요 없다.
 *
 * ★반경은 반드시 ADRS2MoleBoss::GetEruptRadius() 가 돌려준 값을 그대로 받는다.
 *   경고 원과 실제 피격 범위가 어긋나면 회피가 불가능해진다.
 */
UCLASS()
class DAERUNE_API ADRS2GroundWarning : public AActor
{
	GENERATED_BODY()

public:
	ADRS2GroundWarning();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** 서버 전용. 스폰 직후 1회 호출. 반경/지속시간을 복제하고 수명을 건다. */
	UFUNCTION(BlueprintCallable, Category = "S2|Warning")
	void InitWarning(float InRadius, float InDuration);

	/** 추종 모드(bWarningFollowsTarget)에서만 사용. 서버가 지면 위치를 갱신한다. */
	UFUNCTION(BlueprintCallable, Category = "S2|Warning")
	void UpdateGroundLocation(const FVector& NewGroundLocation);

	UFUNCTION(BlueprintPure, Category = "S2|Warning")
	float GetWarningRadius() const { return Radius; }

protected:
	virtual void BeginPlay() override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Warning")
	TObjectPtr<USceneComponent> SceneRoot;

	// 지면에 투영되는 경고 원. PoisonGas 관례대로 Pitch -90 으로 눕힌다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Warning")
	TObjectPtr<UDecalComponent> WarningDecal;

	// 선택 연출 (링 파티클 등)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "S2|Warning")
	TObjectPtr<UNiagaraComponent> WarningFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Warning")
	TObjectPtr<UMaterialInterface> DecalMaterial;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Warning")
	TObjectPtr<UNiagaraSystem> WarningNiagaraSystem;

	// 데칼이 지면 아래로 투영되는 깊이 (PoisonGas 는 300 을 쓴다)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "S2|Warning")
	float DecalProjectionDepth = 300.f;

	UPROPERTY(ReplicatedUsing = OnRep_WarningParams, BlueprintReadOnly, Category = "S2|Warning")
	float Radius = 100.f;

	UPROPERTY(ReplicatedUsing = OnRep_WarningParams, BlueprintReadOnly, Category = "S2|Warning")
	float Duration = 0.8f;

	UFUNCTION()
	void OnRep_WarningParams();

	/**
	 * BP 연출 훅. 반경과 지속시간을 받아 링 확장 타임라인, 사운드 등을 돌린다.
	 * 서버(리슨)에서도 호출되도록 InitWarning 이 OnRep 을 수동 호출한다.
	 */
	UFUNCTION(BlueprintImplementableEvent, Category = "S2|Warning")
	void OnWarningBegin(float InRadius, float InDuration);

private:
	// 데칼 크기 등 반경 의존 값을 실제 컴포넌트에 반영
	void ApplyWarningVisual();

	// OnWarningBegin 을 1회만 발화시키기 위한 래치 (BeginPlay 와 OnRep 이 모두 부를 수 있다)
	bool bVisualStarted = false;

	// BeginPlay 이전에는 BP 연출을 시작하지 않는다 (SpawnActorDeferred 경로 대응)
	bool bBegunPlay = false;
};
