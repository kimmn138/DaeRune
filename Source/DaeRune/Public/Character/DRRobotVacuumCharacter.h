// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacter.h"
#include "Interaction/DRInteractable.h"
#include "DRRobotVacuumCharacter.generated.h"

class UAnimMontage;
class USphereComponent;
class UStaticMeshComponent;
class UAudioComponent;
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
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Mount")
	TObjectPtr<ADRCharacter> RiderOnTop;

	// 탑승 가능 검증 (서버) — 라이더 유무/중복 탑승/순환/사망/부품/스턴 검사 (Plan3 §5.2)
	bool CanBeMountedBy(ADRCharacter* Candidate) const;

	// 탑승 실행 (서버 전용)
	void MountRider(ADRCharacter* Rider);

	// 하차 실행 (서버 전용). bLaunchOff = 폴짝 뛰어내리는 임펄스 적용 여부
	void DismountRider(bool bLaunchOff);

	// IDRInteractable — 탑승 프롬프트 UI (로컬 코스메틱)
	virtual void SetInteractionUIVisible(bool bShow) override;

	// ========== 탑승 칩 (Plan5(탑승칩)) ==========

	// 내 위로 쌓인 인원(RiderOnTop 사슬 길이). 맨 아래 기준 최대 3
	int32 GetRiderCountAbove() const;

	// MountedOn 을 끝까지 내려간 청소기 (내가 맨 아래면 나)
	const ADRRobotVacuumCharacter* GetStackBottom() const;

	// 서버: ① 감속 GE 를 ★태그로★ 지우고, 맨 아래 && 칩① && 인원 > 0 이면 다시 건다 (Plan5 D-80). 몇 번 불러도 안전
	void RefreshRideLoad();

	// 서버: ③ 바로 위 탑승자를 비상 탈출시킨다 — 하차 → 둘 다 회복 → 이 청소기 탑승 금지 (Plan5 D-89)
	void EmergencyEjectRider();

	// 상하 시야각 제한 (±도) — 다른 클래스(±89)와 달리 청소기만 좁게 제한.
	// 적용은 ADRPlayerController::HandlePossessedPawnChanged (폰 교체 시 갱신, 타 캐릭터 복귀 시 기본값 복원)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Camera")
	float ViewPitchLimit = 30.f;

	// ========== 돌진 (이동/충돌 담당 — GA_VacuumDash와 협업) ==========

	// 충돌 발생 시 GA에 통지 (서버에서만 브로드캐스트)
	UPROPERTY(BlueprintAssignable, Category = "Dash")
	FOnDashImpact OnDashImpact;

	// 지속 돌진 중 (RepNotify: 루프 사운드/VFX — Phase F)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Dash")
	bool bSustainedDash = false;

	// GA가 버프 적용/해제 시 호출 (서버) — 활성 중에만 충돌 판정
	void SetDashCollisionEnabled(bool bEnabled);

	// 돌진 중 (게이지 해소 ~ 돌진 종료, 일반 · 지속 공통) — 모든 머신이 OnRep 으로 돌진 이펙트를 켜고 끈다
	UPROPERTY(ReplicatedUsing = OnRep_IsDashing, BlueprintReadOnly, Category = "Dash")
	bool bIsDashing = false;

	void SetDashing(bool bEnabled);   // 서버 전용 (GA_VacuumDash 가 해소 · 종료 · 강제 취소에서 부른다)

	UFUNCTION()
	void OnRep_IsDashing();

	/**
	 * 돌진하는 동안만 보이는 불꽃 껍질 — 구체 메시 + Flames_Mat (팀원 지정: 회전 X -90 · 크기 1.2/1.2/2.0).
	 * 3인칭 메시에 붙고 본인 1인칭 화면에서는 보이지 않는다 (공속 버프 이펙트와 같은 OwnerNoSee).
	 * 위치 · 회전 · 크기는 BP_DRVacuumCleaner 컴포넌트에서 조정한다.
	 */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "VFX|Dash")
	TObjectPtr<UStaticMeshComponent> DashVFXMesh;

	// GA가 지속 돌진 시작/종료 시 호출 (서버) — 복제 플래그 + State.RobotVacuum.SustainedDash 태그 동기화
	void SetSustainedDash(bool bEnabled);

	// ★Plan5(돌진칩)★ ② 돌진 중 아군 통과. GA 가 서버에서 켜고 끈다 → 모든 머신의 OnRep 이 이동 무시 목록을 맞춘다 (Plan5 D-115)
	UPROPERTY(ReplicatedUsing = OnRep_DashPassThroughAllies, BlueprintReadOnly, Category = "Dash")
	bool bDashPassThroughAllies = false;

	void SetDashPassThroughAllies(bool bEnabled);   // 서버 전용

	UFUNCTION()
	void OnRep_DashPassThroughAllies();

	// 돌진 충돌 임팩트 사운드 (Armadillo MulticastPlayRollImpactSound 패턴 — GA가 FinishDash에서 호출)
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayDashImpactSound(FVector_NetQuantize Location);

	// ========== 더블 점프 ==========

	// 공중 Q 사용 횟수 (서버 전용 판정 값 — 복제 불필요). Landed()에서 0 으로
	// ★Plan5(물점프칩)★ bool(bAirJumpUsed) → 횟수. 한도는 GA 의 MaxAirJumps(칩③ SkillAirUseCount +1) (Plan5 D-107)
	uint8 AirJumpsUsed = 0;

	// ========== 애님 전용 복제 플래그 (Plan3 §13.6 — ABP가 매 프레임 폴링) ==========

	// 대쉬 게이지 충전 중 (GA_VacuumDash가 서버에서 세팅) — ABP Dash_Charge 상태 전이용 + 충전 루프 사운드(OnRep)
	UPROPERTY(ReplicatedUsing = OnRep_IsDashCharging, BlueprintReadOnly, Category = "Animation")
	bool bIsDashCharging = false;

	UFUNCTION()
	void OnRep_IsDashCharging();

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

	// ========== 사운드 ==========
	// 재생 경로 두 가지:
	//  - 예측형(공격 · 점프 · 제트 점프): 입력 머신이 바로 재생하고, 서버가 나머지 머신에 멀티캐스트한다 (소유 클라는 건너뜀)
	//  - 상태형(돌진 충전 · 돌진 중 · 공격 충전 · 걷기): 복제 플래그 / 이동 상태를 보고 각 머신이 루프를 켜고 끈다

	// 공기탄 발사음 — GA_VacuumAirShot 이 ReleaseAndFire 에서 (클라 · 서버 인스턴스 모두) 부른다
	void PlayAirShotSound(bool bEnhanced);

	// 공기탄 충전음 켜기/끄기 — GA_VacuumAirShot 이 충전 시작 · 발사 · 종료에서 (클라 · 서버 인스턴스 모두) 부른다
	void SetAttackChargingSound(bool bCharging);

	// 제트 점프음 — GA_VacuumJetJump 가 발동 확정(Commit) 뒤 (클라 · 서버 인스턴스 모두) 부른다
	void PlayJetJumpSound();

	// 돌진 브레이크음 (서버 전용 — GA_VacuumDash::OnBrakePressed)
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayDashBrakeSound();

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

	// ★Plan5(탑승칩)★ 로드아웃 변경 · 빙의 때 ① 감속을 다시 맞춘다 (Plan5 D-81)
	virtual void OnUpgradeEffectsRefreshed() override;
	virtual void RefreshTagDrivenVFX() override;

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

	// 돌진 충돌 임팩트 사운드 (SC_VacuumCrash — BP 기본값)
	UPROPERTY(EditDefaultsOnly, Category = "Dash")
	TObjectPtr<USoundBase> DashImpactSound;

	// ---------- 사운드 (BP_DRVacuumCleaner 기본값으로 지정) ----------

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> AttackSound;            // 공기탄 0~2단계

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> AttackEnhancedSound;    // 공기탄 최종 단계(강화탄)

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> AttackChargingSound;    // 공기탄 충전 (1회 재생, 놓으면 끊김)

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> DashChargingSound;      // 돌진 게이지 충전 (루프)

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> DashStartSound;         // 돌진 시작 1회

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> DashLoopSound;          // 돌진 중 (루프)

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> DashBrakeSound;         // S 브레이크

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> JumpSound;              // 일반 점프 (스페이스)

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> JetJumpSound;           // 더블 점프 스킬 (Q)

	UPROPERTY(EditDefaultsOnly, Category = "Sound")
	TObjectPtr<USoundBase> WalkSound;              // 지면 이동 중 (루프)

	// 이보다 느리면 걷기 소리를 끈다 (수평 속도)
	UPROPERTY(EditDefaultsOnly, Category = "Sound", meta = (ClampMin = "0.0"))
	float WalkSoundMinSpeed = 50.f;

#if WITH_EDITOR
	// 에셋 기본값(예전 생성자 하드코딩 경로 → BP 기본값)이 비었는지 저장 · 데이터 검증 때 알린다
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	// 일반 점프 — 소유 클라 · 서버에서 불린다 (시뮬 프록시는 서버 멀티캐스트로 듣는다)
	virtual void OnJumped_Implementation() override;

private:
	// ★Plan5(탑승칩)★ 서버: 탑승 · 하차 직후 — Rider(청소기면) + 나부터 맨 아래까지 RefreshRideLoad (Plan5 D-81 · T-85)
	void RefreshRideLoadsAfterLinkChange(ADRCharacter* Rider);

	// ★Plan5(돌진칩)★ 이 머신에서 "서로 이동 무시"로 넣은 아군 — 끌 때 정확히 이것만 되돌린다 (이동 무시는 복제되지 않는다)
	TArray<TWeakObjectPtr<ADRCharacter>> PassThroughAllies;

	// ★Plan5(돌진칩)★ 내 캡슐 ↔ 아군 캡슐을 서로 IgnoreActorWhenMoving (켜기 · 끄기 공통, 이 머신에서만)
	void ApplyAllyPassThrough(bool bEnable);

	// ---------- 사운드 ----------

	// 공기탄 충전 중 — 소유 클라는 직접 재생하므로 나머지 머신에만 복제한다 (COND_SkipOwner)
	UPROPERTY(ReplicatedUsing = OnRep_AttackCharging)
	bool bIsAttackCharging = false;

	UFUNCTION()
	void OnRep_AttackCharging();

	// 예측형 사운드: 입력 머신은 바로, 서버는 나머지 머신에 멀티캐스트
	void PlayPredictedSound(USoundBase* Sound);

	// 소유 클라(이미 직접 재생)는 건너뛴다
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayPredictedSound(USoundBase* Sound);

	// 몸에 붙여 1회 재생 (이 머신에서만)
	void PlayAttachedSound(USoundBase* Sound) const;

	// 루프 컴포넌트 켜기/끄기 (이 머신에서만). 같은 상태로 여러 번 불러도 안전
	void SetLoopSound(TObjectPtr<UAudioComponent>& Audio, USoundBase* Sound, bool bPlay);

	// 재생 중이고 페이드 아웃 중이 아니면 true
	static bool IsLoopSoundActive(const UAudioComponent* Audio);

	// 걷기 루프 갱신 (Tick)
	void UpdateWalkSound();

	// 사망 · 파괴 시 모든 루프 정지
	void StopAllLoopSounds();

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> WalkAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> DashChargeAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> DashLoopAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> AttackChargeAudio;
};
