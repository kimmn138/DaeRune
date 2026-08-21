// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DREnemy.h"
#include "DRS2MoleBoss.generated.h"

class ADRS2ElectricField;
class UAnimMontage;

/**
 * 두더지 엘리트 보스 (Plan7 §4.1) — 스테이지2 방6 열차 구간
 *
 * 상시 상태:
 *  - 위치를 이동하지 않는다 (MOVE_None). 유일한 이동 수단은 스킬1의 융기다.
 *  - 몸의 절반만 지면 위로 내놓는다 (BuriedRatio = 0.5).
 *  - 어그로 대상 방향으로 제자리 Yaw 회전만 한다 —
 *    ★부모 ADREnemy::Tick 의 "ControlRotation 추종" 로직을 그대로 쓰므로
 *      AI 컨트롤러가 SetFocus 만 걸어 주면 된다. 별도 회전 복제가 없다.
 *
 * 스킬 스케일링 축 (Plan7 §2.5-C 확정):
 *  - 기본공격 데미지 : 구간(1/2/3차)별
 *  - 스킬1 데미지    : 인원수(1~4)별
 *  - 스킬1 쿨다운    : 구간별
 *  - 전기장 데미지   : 인원수별, 3차 구간에서만 생성
 *
 * ★페이즈 계약 (UDRS2TrainPhase 가 이미 코드로 요구하고 있다 — Plan7 §1.2)
 *   보관(StashBoss) 직전에 NotifyStashed() 가 반드시 불려야 한다.
 *   그러지 않으면 잠수 중이던 타이머 체인이 살아남아 "숨겨진 보스가 융기해 데미지를 주는" 버그가 난다.
 */
UCLASS()
class DAERUNE_API ADRS2MoleBoss : public ADREnemy
{
	GENERATED_BODY()

public:
	ADRS2MoleBoss();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ========== 페이즈 → 보스 주입 (서버 전용) ==========

	/** 현재 장애물 구간(0/1/2). 발톱 데미지·스킬1 쿨다운의 인덱스이자 전기장 활성 조건이다. */
	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Phase")
	void SetSegmentIndex(int32 InIndex);

	UFUNCTION(BlueprintPure, Category = "MoleBoss|Phase")
	int32 GetSegmentIndex() const { return SegmentIndex; }

	/**
	 * 방6 시작 시점 생존자 수(1~4).
	 *
	 * ★별도 필드를 두지 않고 캐릭터 Level 을 그대로 읽는다 — 두 값이 어긋날 여지를 없앤다.
	 *   페이즈가 SpawnEnemyAt(..., BasePlayerCount) 로 지연 스폰하면서 Level 을 심어 주고,
	 *   그 Level 이 CT_EnemyAttributes(Mole.MaxHealth)와 어빌리티 스펙 레벨을 동시에 인덱싱한다.
	 *   즉 "인원 축" 값은 전부 이 하나에서 파생된다.
	 */
	UFUNCTION(BlueprintPure, Category = "MoleBoss|Phase")
	int32 GetBasePlayerCount() const { return FMath::Clamp(Level, 1, 4); }

	/** ★페이즈가 StashBoss() 직전에 호출. 진행 중 스킬 취소 + 전기장 정리 + 융기 카운터 리셋. */
	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Phase")
	void NotifyStashed();

	/** ★페이즈가 ReappearBoss() 직후에 호출. 이동 봉인 재적용 + 반매몰 자세 재정렬. */
	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Phase")
	void NotifyReappeared();

	// ========== 반매몰 자세 ==========

	/** 발밑 지면을 찾아 캡슐 중심 Z 를 BuriedRatio 에 맞춰 재배치한다. */
	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Pose")
	void SnapToBuriedPose();

	/** 주어진 지면 지점에 반매몰로 설 때의 액터 위치. CenterZ = GroundZ + H * (1 - 2r) */
	UFUNCTION(BlueprintPure, Category = "MoleBoss|Pose")
	FVector MakeBuriedLocation(const FVector& GroundPoint) const;

	// ========== 애니메이션 ==========

	/**
	 * 서버 전용. 잠수 몽타주를 재생하고 **재생 길이(초)** 를 돌려준다. 몽타주가 없으면 0.
	 *
	 * ★몽타주 소유권이 BP 가 아니라 여기 있는 이유 (Plan7 §15.6)
	 *  ① 잠수 길이가 스킬 타이밍의 일부다. BP 에셋 길이와 C++ 타이머가 이원화되면
	 *     "파고드는 중간에 사라지는" 버그가 나는데, 길이를 여기서 돌려주면 어긋날 수가 없다.
	 *  ② 원격 클라이언트 재생을 멀티캐스트로 보장해야 한다 (Armadillo 선례 — GA 의 몽타주
	 *     복제만으로는 원격에서 재생되지 않는 사례가 있다).
	 */
	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Anim")
	float PlayBurrowMontage();

	/** 융기 몽타주 길이(초). GA 가 융기 후 어빌리티 유지 시간으로 쓴다. */
	UFUNCTION(BlueprintPure, Category = "MoleBoss|Anim")
	float GetEmergeMontageLength() const;

	// ========== 잠수(무적) ==========

	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Burrow")
	void EnterBurrowedState();

	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Burrow")
	void ExitBurrowedState();

	UFUNCTION(BlueprintPure, Category = "MoleBoss|Burrow")
	bool IsBurrowed() const { return bBurrowed; }

	// ========== 융기 ==========

	/**
	 * 서버 전용. ★순서 고정 —
	 *  ① 숨겨진 상태에서 텔레포트 (클라 보간 슬라이딩을 감춘다)
	 *  ② 회전 + ControlRotation 동기화
	 *  ③ ★융기 몽타주 재생 — **가시화보다 먼저**. 숨겨진 메시도 포즈는 계속 틱하므로
	 *     (SkeletalMeshComponent 기본값 AlwaysTickPoseAndRefreshBones) 첫 프레임(땅속 포즈)이
	 *     적용된 뒤에 보이기 시작한다. 순서가 반대면 기본 포즈로 한 프레임 튀어 보인다.
	 *  ④ 잠수 해제(가시화 + 콜리전 복구)
	 *  ⑤ 융기 카운터 증가 + 전기장 링 버퍼 갱신
	 */
	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Burrow")
	void EruptAt(const FVector& GroundPoint, const FRotator& FacingRotation);

	/** ★융기 AoE 반경. 경고 원과 실제 피격 판정이 반드시 이 함수 하나를 공유해야 한다. */
	UFUNCTION(BlueprintPure, Category = "MoleBoss|Burrow")
	float GetEruptRadius() const;

	UFUNCTION(BlueprintPure, Category = "MoleBoss|Burrow")
	int32 GetEruptCount() const { return EruptCount; }

	// ========== 전기장 ==========

	UFUNCTION(BlueprintCallable, Category = "MoleBoss|Field")
	void DestroyActiveField();

	UFUNCTION(BlueprintPure, Category = "MoleBoss|Field")
	bool IsElectricFieldSegment() const { return SegmentIndex == ElectricFieldSegmentIndex; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void Die(const FVector& DeathImpulse) override;

	// ---------- 애니메이션 ----------

	/** 파고드는 몽타주. 끝 포즈가 "완전히 땅속"이어야 한다 — 이 시점에 메시를 숨긴다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Anim")
	TObjectPtr<UAnimMontage> BurrowMontage;

	/** 솟아오르는 몽타주. ★첫 프레임이 "완전히 땅속"이어야 가시화 순간에 튀지 않는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Anim")
	TObjectPtr<UAnimMontage> EmergeMontage;

	// ---------- 자세 ----------

	/** 0 = 완전 노출, 0.5 = 절반 매몰(사양), 1 = 완전 매몰 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Pose", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BuriedRatio = 0.5f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Pose", meta = (ClampMin = "100.0"))
	float GroundTraceDistance = 1000.f;

	// ---------- 융기 ----------

	/** 융기 AoE 반경 배율. 1.0 = 캡슐 반지름 그대로 (사용자 확정값). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow", meta = (ClampMin = "0.1"))
	float EruptRadiusScale = 1.f;

	/** 잠수 시 자신에게 걸린 디버프를 제거할지. 화상 DoT 가 무적을 뚫는 것을 막는다. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Burrow")
	bool bClearDebuffsOnBurrow = true;

	// ---------- 전기장 ----------

	/** 전기장을 생성하는 구간 인덱스. 3차 = 2 (사용자 확정). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Field")
	int32 ElectricFieldSegmentIndex = 2;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MoleBoss|Field")
	TSubclassOf<ADRS2ElectricField> ElectricFieldClass;

	// ★전기장 데미지 수치는 여기에 없다.
	//   BP_S2ElectricField 의 GE(Modifier ScalableFloat)가 스펙 레벨로 커브를 읽어 정한다.
	//   보스는 InitField 에 "인원수"만 넘긴다 (= 스펙 레벨). GE_PoisonDamage 와 같은 구조다.

	// ---------- 복제 상태 ----------

	/** 클라 연출(3차 전용 이펙트 등)용. 게임플레이 판정은 전부 서버에서 이 값을 읽는다. */
	UPROPERTY(ReplicatedUsing = OnRep_SegmentIndex, BlueprintReadOnly, Category = "MoleBoss|Phase")
	int32 SegmentIndex = 0;

	UPROPERTY(ReplicatedUsing = OnRep_Burrowed, BlueprintReadOnly, Category = "MoleBoss|Burrow")
	bool bBurrowed = false;

	UFUNCTION()
	void OnRep_SegmentIndex();

	UFUNCTION()
	void OnRep_Burrowed();

	// ---------- BP 연출 훅 ----------

	UFUNCTION(BlueprintImplementableEvent, Category = "MoleBoss|FX")
	void OnBurrowVisual();

	UFUNCTION(BlueprintImplementableEvent, Category = "MoleBoss|FX")
	void OnEruptVisual(const FVector& EruptLocation);

	UFUNCTION(BlueprintImplementableEvent, Category = "MoleBoss|FX")
	void OnSegmentChangedVisual(int32 NewSegment);

private:
	/** 이동/중력/넉백을 전면 봉인한다. BeginPlay 와 재등장 시 호출. */
	void LockMovement();

	/** 서버 로컬 재생 + 원격 클라 멀티캐스트. 재생 길이(초) 반환. */
	float PlayMoleMontage(UAnimMontage* Montage);

	/**
	 * ★원격 클라이언트 전용 재생.
	 * GA 가 재생한 몽타주가 원격 클라에 도달하지 않는 사례가 이 프로젝트에 이미 있다
	 * (DRArmadilloEnemy 의 MulticastPlayFormChangeMontage 주석). 그래서 명시적으로 쏜다.
	 * 서버는 PlayMoleMontage 에서 이미 재생했으므로 내부에서 걸러낸다.
	 */
	UFUNCTION(NetMulticast, Reliable)
	void Multicast_PlayMoleMontage(UAnimMontage* Montage);

	/**
	 * 전기장 링 버퍼 (Plan7 §6.4).
	 * 규칙: "직전 융기 지점 1곳에만 전기장이 존재한다."
	 *   융기 n 시점 → P(n-2) 제거, P(n-1) 생성, 현재 지점을 P(n-1) 로 승격
	 */
	void UpdateElectricFieldRing(const FVector& NewEruptGround);

	UPROPERTY()
	int32 EruptCount = 0;

	UPROPERTY()
	TWeakObjectPtr<ADRS2ElectricField> ActiveField;

	FVector PreviousEruptGround = FVector::ZeroVector;
	bool bHasPreviousErupt = false;
};
