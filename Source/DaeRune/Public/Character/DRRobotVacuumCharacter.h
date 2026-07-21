// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacter.h"
#include "Interaction/DRInteractable.h"
#include "DRRobotVacuumCharacter.generated.h"

class UAnimMontage;
class USphereComponent;
class USoundBase;
class UWidgetComponent;

/** 돌진 충돌 델리게이트 — 캐릭터(C++)가 충돌만 감지하고 데미지 처리는 GA가 담당 (Armadillo FOnRollImpact 패턴) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDashImpact, AActor*, HitActor, const FHitResult&, Hit);

/**
 * 로봇 청소기 플레이어 캐릭터 (Plan3)
 * - 패시브: 다른 플레이어가 F키로 탑승 (마운트 측 상태 관리)
 * - 돌진: 이동/충돌 감지는 캐릭터, 게이지/버프/데미지는 GA_VacuumDash (Armadillo 역할 분담 이식)
 * - 더블 점프: 공중 사용 플래그 관리 (Landed에서 리셋)
 */
UCLASS()
class DAERUNE_API ADRRobotVacuumCharacter : public ADRCharacter, public IDRInteractable
{
	GENERATED_BODY()

public:
	ADRRobotVacuumCharacter();
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 착지 시 공중 Q 사용 플래그 리셋
	virtual void Landed(const FHitResult& Hit) override;

	// 사망 시 라이더 강제 하차 + 자신이 탑승 중이면 링크 해제
	virtual void MulticastHandleDeath_Implementation(const FVector& DeathImpulse) override;

	// ========== 탑승 (마운트 측) ==========

	// 라이더가 붙는 메시 본 소켓 이름 (CombatSocket처럼 BP에서 지정).
	// 씬 컴포넌트가 아닌 본 소켓이라 청소기 애니메이션 재생 시 라이더도 함께 움직인다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount")
	FName RideSocketName = FName("RideSocket");

	// 내 위에 탄 캐릭터 (nullptr = 빈자리)
	UPROPERTY(ReplicatedUsing = OnRep_RiderOnTop, BlueprintReadOnly, Category = "Mount")
	TObjectPtr<ADRCharacter> RiderOnTop;

	// 탑승 가능 검증 (서버) — 라이더 유무/중복 탑승/순환/사망/부품/스턴 검사 (Plan3 §5.2)
	bool CanBeMountedBy(ADRCharacter* Candidate) const;

	// 탑승 실행 (서버 전용)
	void MountRider(ADRCharacter* Rider);

	// 하차 실행 (서버 전용). bLaunchOff = 폴짝 뛰어내리는 임펄스 적용 여부
	void DismountRider(bool bLaunchOff);

	UFUNCTION()
	void OnRep_RiderOnTop();

	// IDRInteractable — 탑승 프롬프트 UI (로컬 코스메틱)
	virtual void SetInteractionUIVisible(bool bShow) override;

	// 상하 시야각 제한 (±도) — 다른 클래스(±89)와 달리 청소기만 좁게 제한.
	// 적용은 ADRPlayerController::HandlePossessedPawnChanged (폰 교체 시 갱신, 타 캐릭터 복귀 시 기본값 복원)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float ViewPitchLimit = 30.f;

	// ========== 돌진 (이동/충돌 담당 — GA_VacuumDash와 협업) ==========

	// 충돌 발생 시 GA에 통지 (서버에서만 브로드캐스트)
	UPROPERTY(BlueprintAssignable, Category = "Dash")
	FOnDashImpact OnDashImpact;

	// 지속 돌진 중 (RepNotify: 루프 사운드/VFX — Phase F)
	UPROPERTY(ReplicatedUsing = OnRep_SustainedDash, BlueprintReadOnly, Category = "Dash")
	bool bSustainedDash = false;

	// GA가 버프 적용/해제 시 호출 (서버) — 활성 중에만 충돌 판정
	void SetDashCollisionEnabled(bool bEnabled);

	// GA가 지속 돌진 시작/종료 시 호출 (서버) — 복제 플래그 + State.RobotVacuum.SustainedDash 태그 동기화
	void SetSustainedDash(bool bEnabled);

	UFUNCTION()
	void OnRep_SustainedDash();

	// 돌진 충돌 임팩트 사운드 (Armadillo MulticastPlayRollImpactSound 패턴 — GA가 FinishDash에서 호출)
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayDashImpactSound(FVector_NetQuantize Location);

	// ========== 더블 점프 ==========

	// 공중 Q 사용 여부 (서버 전용 판정 값 — 복제 불필요). Landed()에서 리셋
	bool bAirJumpUsed = false;

	// ========== 애님 전용 복제 플래그 (Plan3 §13.6 — ABP가 매 프레임 폴링) ==========

	// 대쉬 게이지 충전 중 (GA_VacuumDash가 서버에서 세팅) — ABP Dash_Charge 상태 전이용
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Animation")
	bool bIsDashCharging = false;

	// 더블 점프(제트 점프) 체공 중 (GA_VacuumJetJump가 세팅, Landed()에서 해제)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Animation")
	bool bIsJetJumping = false;

	// 제트 점프 발동 횟수 카운터 (서버 전용 증가, 랩어라운드 무해).
	// FP ABP가 값 "변화"를 엣지로 감지해 Jump_Start 재진입 — bIsJetJumping은 착지까지 true 유지라
	// "지상 Q → 공중 Q" 2연속에서 엣지가 안 생기므로 이 카운터가 필요 (Plan5 §4.6)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Animation")
	uint8 JetJumpCounter = 0;

	void SetDashCharging(bool bNew);   // 서버 전용
	void SetJetJumping(bool bNew);     // 서버 전용

	// ========== FP 애니메이션 (Plan5 §5.2) ==========

	// 돌진 종료 FP 몽타주 — S 브레이크 급정거 (BP_DRVacuumCleaner에서 AM_FP_VC_Dash_Stop 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animation")
	TObjectPtr<UAnimMontage> FPDashStopMontage;

	// 돌진 충돌 FP 몽타주 — 일반/지속 돌진 충돌 공통 (AM_FP_VC_Crush 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Dash|Animation")
	TObjectPtr<UAnimMontage> FPDashCrushMontage;

	// 서버(GA FinishDash) → 소유 클라: FP 메시에 돌진 종료 몽타주 재생.
	// Reliable — 1회성 저빈도 연출이지만 유실 시 FP만 뻣뻣하게 남아 눈에 띔
	UFUNCTION(Client, Reliable)
	void ClientPlayDashEndFPMontage(bool bFromImpact);

	// ========== FP 기준 발사 (Plan5 §6) ==========

	// 공기탄 발사 기준 소켓 — FP 메시의 총구
	UPROPERTY(EditDefaultsOnly, Category = "Combat")
	FName FPMuzzleSocketName = FName("Muzzle");

	// CombatSocket.LeftHand(GA_VacuumAirShot의 FireSocketTag) 요청을 FP 메시 Muzzle로 라우팅
	// — 발사 위치/반동 기준을 1인칭 모델에 맞춤
	virtual FVector GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 캡슐 충돌 콜백 — 돌진 충돌 감지 (서버 전용 판정)
	UFUNCTION()
	void OnCapsuleHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	                  UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// 탑승 감지 오버랩 콜백 — 로컬 PC에 감지 후보 등록/해제 (부품 DetectionSphere 패턴)
	UFUNCTION()
	void OnMountDetectionBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                                  UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	                                  bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnMountDetectionEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	                                UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// 오버랩 상태 재평가 후 로컬 PC에 감지 등록/해제
	void RefreshMountOverlapStateFor(ADRCharacter* Character);

	// 근접 감지 스피어 — 부품 감지 패턴과 동일 (PC가 후보 등록, Phase B에서 오버랩 바인딩)
	UPROPERTY(VisibleAnywhere, Category = "Mount")
	TObjectPtr<USphereComponent> MountDetectionSphere;

	// 탑승 프롬프트 위젯 (로컬 코스메틱 — 각 클라이언트 PC가 토글)
	UPROPERTY(VisibleAnywhere, Category = "Mount")
	TObjectPtr<UWidgetComponent> MountPromptWidget;

	// 서버: 돌진 버프 활성 중에만 충돌 판정 (1회 발화 후 스스로 해제)
	bool bDashCollisionArmed = false;

	// 돌진 충돌 오판 방지 최소 속도 — 벽/지형(수직면)에만 적용. Pawn(적/타 플레이어) 충돌은
	// armed(돌진 중)만으로 유효하므로 이 값의 영향을 받지 않는다 (OnCapsuleHit 참고).
	// 기본 이속 550 · 최소 돌진(1칸) 순항 660 기준, 충돌 순간 감속을 흡수하도록 충분히 낮게 설정.
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	float DashImpactMinSpeed = 350.f;

	// 돌진 충돌 임팩트 사운드 (BP에서 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	TObjectPtr<USoundBase> DashImpactSound;
};
