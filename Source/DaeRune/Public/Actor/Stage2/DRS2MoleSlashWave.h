// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DRProjectile.h"
#include "DRS2MoleSlashWave.generated.h"

class UCapsuleComponent;

/**
 * 두더지 보스 기본공격 투사체 — 45도 대각 검기 (Plan7 §17)
 *
 * 보스가 이동하지 않는(MOVE_None) 설계 탓에 근접 부채꼴 기본공격이 사실상 무의미했다.
 * 이를 "전방으로 날아가는 관통 검기"로 대체한 것이 이 액터다.
 *
 * ★칼날은 진행축(X) 기준 45도 기울어 있다 — 1타는 `/`, 2타는 `\`.
 *   기울기는 **액터 스폰 회전의 Roll** 이 준다 (UDRS2MoleClawAttack::FireSlashWaves).
 *   그래서 판정 캡슐과 메시/나이아가라가 **통째로 같이 기울어** 보이는 것과 맞는 것이 어긋나지 않는다.
 *   ※ Roll 은 진행축 자체를 돌리지 않으므로 비행 궤적은 수평·고도 고정 그대로다.
 *
 * ★부모 ADRProjectile 과 다른 점 3가지
 *  ① 관통 — 부모는 첫 오버랩에서 무조건 Destroy 한다. 여기서는 MaxPierceCount 만큼(0이면 무제한) 뚫는다.
 *  ② 판정 형태 — 루트 Sphere 를 끄고 기울어진 캡슐 하나로 단일화한다.
 *     (스피어를 켜 두면 대각선의 사각이 중앙 구로 메워지고, 지면과 상시 오버랩되어 즉시 소멸한다)
 *  ③ 지형 정지 — 오버랩이 아니라 **프레임 간 수평 선분 트레이스**로 벽을 판정한다 (Tick 주석 참고).
 */
UCLASS()
class DAERUNE_API ADRS2MoleSlashWave : public ADRProjectile
{
	GENERATED_BODY()

public:
	ADRS2MoleSlashWave();

	virtual void Tick(float DeltaSeconds) override;

	/**
	 * 서버 전용. SpawnActorDeferred 직후 FinishSpawning 전에 GA 가 호출한다.
	 *
	 * @param InSpeed    비행 속도(uu/s)
	 * @param InMaxRange 최대 사거리(uu). ★수명은 InMaxRange / InSpeed 로 자동 계산된다 —
	 *                   사거리와 수명이 어긋날 여지를 없앤다.
	 */
	void InitWave(float InSpeed, float InMaxRange);

protected:
	virtual void BeginPlay() override;

	/**
	 * ★부모를 호출하지 않는다.
	 *
	 * 부모는 IsNotFriend 를 통과한 첫 액터에서 무조건 Destroy 하는데, IsNotFriend 는
	 * "양쪽 다 Player/Enemy 태그가 아니면 적으로 간주"하므로 벽·소품까지 통과시킨다.
	 * 즉 부모 로직으로는 관통은커녕 지형에 스치기만 해도 사라진다.
	 * 그래서 데미지 파이프라인만 그대로 따르고 소멸 조건만 갈아끼운다.
	 */
	virtual void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep,
		const FHitResult& SweepResult) override;

	/**
	 * 대각 칼날 판정.
	 *
	 * 캡슐의 축은 로컬 Z 다. 여기에 상대 회전 Roll = 90 을 주어 축을 **액터의 Y(좌우)** 로 눕힌다.
	 * 그 뒤 액터 자체의 스폰 회전 Roll(±45)이 이 칼날을 진행축 기준으로 기울인다.
	 *
	 * ★상대 회전을 BP 에서 건드리면 이중 회전이 되어 칼날이 수직으로 선다. 그대로 둘 것.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SlashWave")
	TObjectPtr<UCapsuleComponent> BladeCapsule;

	/** ★0 = 무제한 관통 (사양 확정값). 0 보다 크면 그 수만큼만 뚫고 소멸한다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SlashWave", meta = (ClampMin = "0"))
	int32 MaxPierceCount = 0;

	/**
	 * 지형지물(벽·열차)에 막혀 소멸할지.
	 * ★배리어(ADRS2Barrier)는 Pawn 만 Block 하고 Visibility 는 Ignore 하므로 검기가 통과한다 —
	 *   "배리어는 투사체를 통과시킨다"는 기존 사양(Plan7 §1.5)과 일치한다.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SlashWave")
	bool bStopOnWorldGeometry = true;

	/** 관통 순간 연출 훅. ★무제한 관통이면 한 검기에서 여러 번 불린다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "SlashWave")
	void OnPierceVisual(const FVector& HitLocation);

private:
	/** ★동일 대상 재타격 금지. 오버랩에서 빠졌다가 다시 들어와도 두 번 맞지 않는다. */
	TSet<TWeakObjectPtr<AActor>> HitActors;

	/**
	 * InitWave 가 계산한 수명. ★BeginPlay 에서 다시 적용해야 한다 —
	 * 부모 BeginPlay 가 자기 LifeSpan(기본 15초)으로 덮어쓰기 때문이다.
	 * (ADRVacuumAirProjectile 이 SetLifeSpan(0) 으로 같은 문제를 처리하는 선례가 있다)
	 */
	float PendingLifeSpan = 0.f;

	int32 PierceCount = 0;

	/** 직전 프레임 위치. 지형 판정용 선분 트레이스의 시작점이다. */
	FVector LastTickLocation = FVector::ZeroVector;
};
