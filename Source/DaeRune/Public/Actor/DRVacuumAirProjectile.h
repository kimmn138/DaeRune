// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DRProjectile.h"
#include "DRVacuumAirProjectile.generated.h"

class UBoxComponent;
class UNiagaraSystem;

/**
 * 로봇 청소기 공기탄 투사체 (Plan3 §6.2)
 * - 부모 LifeSpan 대신 자체 타이머: ActiveDuration 경과 시 페이드(콜리전 off + 정지) 후 파괴
 * - 강화탄(bEnhanced): 아군 플레이어에게 데미지 없이 넉백만 적용 (부모는 아군을 걸러버리므로 오버라이드)
 * - 바닥 활주: 바닥(BlockingVolume 윗면)에 닿으면 사라지지 않고 바닥을 따라 미끄러진다 — 벽 · 천장에 닿으면 예전처럼 사라진다.
 *   청소기는 노즐을 바닥에 끌고 다녀서, 아래를 보고 쏘면 탄이 바로 바닥에 닿는다 (2026-11-09)
 */
UCLASS()
class DAERUNE_API ADRVacuumAirProjectile : public ADRProjectile
{
	GENERATED_BODY()

public:
	ADRVacuumAirProjectile();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 3단계 강화탄 여부 (스폰 시 서버가 주입, 클라 연출 구분용으로 복제)
	UPROPERTY(Replicated, BlueprintReadWrite, meta = (ExposeOnSpawn = true), Category = "AirShot")
	bool bEnhanced = false;

	// ★Plan5(공기탄칩)★ ① 아군에게도 피해 — FireShot 이 스폰 때(FinishSpawning 전) 넣는다(서버). 클라 적중 연출용으로 복제 (bEnhanced 와 같다)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "AirShot")
	bool bHitsAllies = false;

	// 가로형 판정 박스 (Plan5 §7) — 루트 Sphere는 비균등 스케일 불가라 별도 박스로 판정.
	// 폰에만 오버랩(벽 · 바닥은 Sphere). Extent · 위치는 BP_VacuumAirProjectile 에서 날 이펙트에 맞춘다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AirShot")
	TObjectPtr<UBoxComponent> WideCollision;

protected:
	virtual void BeginPlay() override;
	virtual void Destroyed() override;

	// 강화탄 아군 넉백 분기 후 부모 파이프라인 위임
	virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult) override;

	// 서버: ActiveDuration 경과 시 호출 — 콜리전/이동 정지 후 FadeDuration 뒤 파괴
	void StartFade();

	UFUNCTION()
	void OnRep_Fading();

	// 페이드 연출 시작 (BP: 나이아가라/머티리얼 타임라인)
	UFUNCTION(BlueprintImplementableEvent, Category = "AirShot")
	void OnFadeStarted();

	// 유효 비행 시간 (이후 페이드 시작)
	UPROPERTY(EditDefaultsOnly, Category = "AirShot")
	float ActiveDuration = 2.f;

	// 페이드 연출 시간 (종료 시 Destroy)
	UPROPERTY(EditDefaultsOnly, Category = "AirShot")
	float FadeDuration = 0.5f;

	// 페이드 진행 중 (서버 설정, RepNotify로 클라 연출 트리거)
	UPROPERTY(ReplicatedUsing = OnRep_Fading)
	bool bFading = false;

	// ========== 바닥 활주 ==========

	// 이 값 이상으로 위를 향한 면(법선 Z)은 바닥 — 미끄러진다. 그보다 가파르면 벽 · 천장 — 사라진다 (0.7 ≈ 45°, 캐릭터가 걷는 경사와 같다)
	UPROPERTY(EditDefaultsOnly, Category = "AirShot|Skim", meta = (ClampMin = "0", ClampMax = "1"))
	float FloorNormalMinZ = 0.7f;

	// 처음 바닥에 닿을 때 한 번 — 바람이 바닥을 치며 퍼지는 고리 + 먼지 (WindBladeDestory_NS)
	UPROPERTY(EditDefaultsOnly, Category = "AirShot|Skim")
	TObjectPtr<UNiagaraSystem> SkimTouchdownEffect;

	// 바닥 가까이 지나가는 동안 SkimTrailInterval 마다 바닥에 남기는 먼지 (WindBladeSkimDust_NS — 월드 공간이라 뒤에 남는다)
	UPROPERTY(EditDefaultsOnly, Category = "AirShot|Skim")
	TObjectPtr<UNiagaraSystem> SkimTrailEffect;

	UPROPERTY(EditDefaultsOnly, Category = "AirShot|Skim", meta = (ClampMin = "0.02"))
	float SkimTrailInterval = 0.06f;

	// 구 아래 끝과 바닥 사이가 이만큼 이하일 때만 먼지를 남긴다 (공중으로 떠나면 멈춘다)
	UPROPERTY(EditDefaultsOnly, Category = "AirShot|Skim", meta = (ClampMin = "0"))
	float SkimTrailMaxGap = 12.f;

	// 바닥에 닿아 미끄러지는 중 (서버 설정) — 클라는 RepNotify 로 착지 연출. 클라 자체 시뮬레이션이 먼저 닿으면 그쪽이 먼저 띄운다
	UPROPERTY(ReplicatedUsing = OnRep_Skimming)
	bool bSkimming = false;

	UFUNCTION()
	void OnRep_Skimming();

	// ProjectileMovement 의 반발(bShouldBounce) 이벤트 — 바닥이면 미끄러지고, 벽 · 천장이면 예전 오버랩과 같은 적중 처리
	UFUNCTION()
	void HandleProjectileBounce(const FHitResult& ImpactResult, const FVector& ImpactVelocity);

private:
	FTimerHandle ActiveTimerHandle;
	FTimerHandle FadeTimerHandle;
	FTimerHandle SkimTrailTimerHandle;

	// 착지 연출을 이 머신에서 이미 띄웠나 (반발 이벤트 · RepNotify 중 먼저 온 쪽만)
	bool bTouchdownPlayed = false;

	// 스폰 위치가 바닥에 박혀 있으면 구 반지름만큼 위로 (서버, BeginPlay)
	void LiftAboveFloor();
	// From 아래 Distance 안의 바닥(FloorNormalMinZ 이상) — 구와 같은 채널 · 응답으로 찾는다
	bool FindFloorBelow(const FVector& From, float Distance, FHitResult& OutHit) const;
	void PlaySkimTouchdown();
	void SpawnSkimTrail();

	// BeginPlay 한 프레임 (GFrameCounter) — 이 프레임의 아군 오버랩 = 스폰 순간 시전자 옆에 서 있던 아군 → 넘긴다
	uint64 SpawnFrame = 0;
};
