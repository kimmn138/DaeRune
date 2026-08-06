// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Actor/DRProjectile.h"
#include "DRVacuumAirProjectile.generated.h"

class UBoxComponent;

/**
 * 로봇 청소기 공기탄 투사체 (Plan3 §6.2)
 * - 부모 LifeSpan 대신 자체 타이머: ActiveDuration 경과 시 페이드(콜리전 off + 정지) 후 파괴
 * - 강화탄(bEnhanced): 아군 플레이어에게 데미지 없이 넉백만 적용 (부모는 아군을 걸러버리므로 오버라이드)
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

	// 가로형 판정 박스 (Plan5 §7) — 루트 Sphere는 비균등 스케일 불가라 별도 박스로 판정.
	// 기본 NoCollision — BP_VacuumAirProjectile에서 프로파일/Extent 설정 (Sphere 세팅 미러)
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

private:
	FTimerHandle ActiveTimerHandle;
	FTimerHandle FadeTimerHandle;
};
