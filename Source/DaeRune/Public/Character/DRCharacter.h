// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Engine/NetSerialization.h"
#include "GameplayEffectTypes.h"
#include "DRCharacter.generated.h"

class UWidgetComponent;
class ADRCleanserPart;
class ADRRobotVacuumCharacter;
class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMesh;
class UDRFacialExpressionComponent;
class UAnimMontage;
struct FGameplayEffectContextHandle;

/**
 * �÷��̾� ĳ���� Ŭ����
 */
UCLASS()
class DAERUNE_API ADRCharacter : public ADRCharacterBase
{
	GENERATED_BODY()
	
public:
	ADRCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 플레이어 캐릭터 클래스 (EPlayerCharacterClass)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Replicated, Category = "Character Class Defaults")
	EPlayerCharacterClass PlayerCharacterClass = EPlayerCharacterClass::Gardener;

	// �������� ��Ʈ�ѷ��� ���ǵ� �� ȣ�� (������ GAS �ʱ�ȭ)
	virtual void PossessedBy(AController* NewController) override;
	// PlayerState ���ø����̼� �� ȣ�� (Ŭ���̾�Ʈ�� GAS �ʱ�ȭ)
	virtual void OnRep_PlayerState() override;

	// �÷��̾� ���� ����� RepNotify �Լ���
	virtual void OnRep_Stunned() override;
	virtual void OnRep_Burned() override;

	// ========== 사망 애니메이션 ==========

	// 사망 시 재생할 몽타주들. 비어있으면 기존 동작(몽타주 미재생).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Death")
	TArray<TObjectPtr<UAnimMontage>> DeathMontages;

	// 사망 몽타주 재생 속도
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Death", meta = (ClampMin = "0.1"))
	float DeathMontagePlayRate = 1.0f;

	// 서버가 결정한 사망 몽타주 인덱스 (모든 머신에서 동일한 몽타주 재생)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Combat|Death")
	int32 DeathMontageIndex = INDEX_NONE;

	// BP에서 사망 시점에 추가 연출을 붙이고 싶을 때 사용 (선택)
	UFUNCTION(BlueprintImplementableEvent, Category = "Combat|Death", meta = (DisplayName = "On Character Died"))
	void K2_OnCharacterDied();

	// 사망 처리 override (인덱스 결정 + BP 이벤트 호출)
	virtual void MulticastHandleDeath_Implementation(const FVector& DeathImpulse) override;

	// 사망 상태 동기화 override (bDead가 true가 되면 몽타주 재생)
	virtual void OnRep_Dead() override;

	UFUNCTION(BlueprintCallable, Category = "Camera")
	UCameraComponent* GetFollowCamera() const { return FollowCamera; }

	// ========== 업그레이드 칩 반영 (Plan2.md 7.2 / 8.4) ==========

	// 장착 칩 수치를 캐릭터에 반영한다. 멱등이라 몇 번 호출해도 안전하다.
	//  - 모든 머신: 컨테이너 체력 재계산 (컨테이너 UI 가 클라에서도 맞아야 하므로)
	//  - 서버: GE_Upgrade_Stats 재적용 (MaxHealth / MaxWater / MoveSpeed)
	// 반드시 InitializeDefaultAttributes() 이후에 호출해야 한다
	// (InitializePlayerDefaultAttributes 가 활성 GE 를 제거하고 BaseValue 를 0으로 리셋한다).
	void RefreshUpgradeEffects();

	// �����̳� �ý��� ����
	UPROPERTY(EditDefaultsOnly, Category = "Container System")
	int32 NumContainers = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Container System")
	float ContainerHealth = 100.f;

	// ========== ��ǰ �ý��� ==========

	// ��ǰ ���� ���� ��ȸ
	UFUNCTION(BlueprintCallable, Category = "Part System")
	bool IsCarryingPart() const { return bIsCarryingPart; }

	// ��ǰ ȹ�� ó��
	UFUNCTION(BlueprintCallable, Category = "Part System")
	bool PickupPart(class ADRCleanserPart* Part);

	// ��ǰ ��ġ ó��
	UFUNCTION(BlueprintCallable, Category = "Part System")
	void InstallCarriedPart();

	// ��ǰ ����߸���
	UFUNCTION(BlueprintCallable, Category = "Part System")
	void DropCarriedPart();

	// 피격 등으로 인한 강제 드롭 (드롭 쿨다운을 무시하고 즉시 부품을 내려놓음)
	void ForceDropCarriedPart();

	// 부품 보유 상태가 바뀐 직후 현재 오버랩 중인 CleanserSite/CleanserPart 들의 UI/감지를 재평가
	// (오버랩 영역 안에서 집어들기/내려놓기 시 UI가 갱신되지 않는 문제 해결)
	void RefreshNearbyInteractions();

	// ========== 탑승 시스템 (라이더 측) ==========

	// 내가 타고 있는 로봇 청소기 (nullptr = 미탑승). 서버가 설정, RepNotify로 클라 attach/이동모드 보정.
	UPROPERTY(ReplicatedUsing = OnRep_MountedOn, BlueprintReadOnly, Category = "Mount")
	TObjectPtr<ADRRobotVacuumCharacter> MountedOn;

	UFUNCTION()
	void OnRep_MountedOn();

	UFUNCTION(BlueprintCallable, Category = "Mount")
	bool IsMounted() const { return MountedOn != nullptr; }

	// 내 메시에서 탑승 접점(발바닥 등)이 되는 소켓 이름 (CombatSocket처럼 BP에서 지정).
	// 탑승 높이 결정에 이 소켓의 액터 공간 Z만 사용 — 좌우 회전 시 공전을 막기 위해 XY는 무시.
	// None이거나 소켓이 없으면 캡슐 바닥(반높이)이 탑승 소켓 위에 오도록 올린다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount")
	FName MountAlignSocketName = NAME_None;

	// 마운트에 attach (서버 MountRider + 클라 OnRep_MountedOn 공용).
	// 부모는 기울지 않는 청소기 캡슐(루트) — 본 소켓에 직접 붙이면 본 회전이 매 프레임
	// 라이더 회전에 합성돼 자세가 오염되므로, 소켓은 위치 기준으로만 쓴다
	// (매 프레임 위치 추적은 ADRRobotVacuumCharacter::Tick).
	void AttachToMountSocket(ADRRobotVacuumCharacter* Mount);

	// 탑승 시 소켓 위치에서 루트를 올릴 수직 오프셋 — 발(접점)이 소켓 위에 오게 한다.
	// MountAlignSocketName 소켓이 있으면 그 소켓의 액터 공간 높이, 없으면 캡슐 반높이.
	float GetMountZOffset() const;

	// 서버 전용: 피격 데미지를 마운트 링크(위/아래 1홉)로 전파 (Damage.MountShared 태그로 재전파 방지)
	void PropagateSharedDamage(float Damage, const FGameplayEffectContextHandle& SourceContext);

	// 탑승 공유 데미지에 사용할 GE (기존 공용 GE_Damage 지정 — BP에서 설정)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mount")
	TSubclassOf<UGameplayEffect> MountSharedDamageEffectClass;

	// ========== ī�޶� ==========

	// ���� ī�޶� ����
	UFUNCTION(BlueprintImplementableEvent, Category = "Death")
	void PlayDeathCameraAnimation();

	// ========== WaterPump 3P 빔 (리플리케이트 상태) ==========

	// 물대포 활성화 상태 (서버에서 설정, RepNotify로 비소유 클라이언트에서 3P 빔 관리)
	UPROPERTY(ReplicatedUsing=OnRep_WaterPumpActive, BlueprintReadOnly, Category = "Effects")
	bool bWaterPumpActive = false;

	// 물대포 빔 끝점 (서버에서 갱신, 비소유 클라이언트에서 3P 빔 위치 업데이트용)
	// VFX 끝점이라 0.1 정밀도 양자화로 충분 (NetQuantize 계열은 BlueprintType이 아니므로 BP 노출 제외)
	UPROPERTY(Replicated)
	FVector_NetQuantize10 WaterPumpBeamEndPoint = FVector::ZeroVector;

	// 물대포 사거리 (어빌리티 활성화 시 서버가 UDRWaterPump::WeaponRange 값으로 동기화)
	// 1P/3P 빔 길이 정규화가 같은 값을 쓰게 해서 타 플레이어 시점의 빔 길이 왜곡 방지
	UPROPERTY(Replicated)
	float WaterPumpWeaponRange = 1000.f;

	// Niagara 에셋 (블루프린트 기본값에서 설정 - WaterPump 어빌리티의 WaterCannonEffect와 동일 에셋 지정)
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> WaterPumpEffectAsset;

	// 3P 빔 부착 소켓 이름 (WaterPump 어빌리티의 MuzzleSocketName과 동일하게 설정)
	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	FName WaterPumpMuzzleSocket = FName("TestRightHand");

	UFUNCTION()
	void OnRep_WaterPumpActive();

	// ========== 1��Ī/3��Ī �޽� �ý��� ==========

	// 1��Ī �޽�
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	// 1인칭 시점에서 보이는 부품 메시 (주인에게만 보임)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Part System")
	TObjectPtr<UStaticMeshComponent> FirstPersonPartMesh;

	// Third-person carried part visual. Hidden from the owning player.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Part System")
	TObjectPtr<UStaticMeshComponent> ThirdPersonPartMesh;

	// �޽� ���ü� ������Ʈ
	void UpdateMeshVisibility();

	// 1인칭 부품 메시 표시 (부품 픽업 시 호출)
	void ShowFirstPersonPart(UStaticMesh* InPartMesh);

	// 1인칭 부품 메시 숨기기 (부품 드롭/설치 시 호출)
	void HideFirstPersonPart();

	// Third-person carried part visual.
	void ShowThirdPersonPart(UStaticMesh* InPartMesh);
	void HideThirdPersonPart();
	void RefreshCarriedPartVisuals();

	// 대기실 메시 가시성 전환 (고정 카메라에서 3P 메시를 보여주기 위해)
	void SetWaitingRoomVisibility(bool bInWaitingRoom);

	// 오버헤드 닉네임 위젯 가시성 제어 (WaitingRoom에서는 숨김, FreeRoam부터 표시)
	void SetOverheadWidgetVisibility(bool bVisible);

	// 오버헤드 위젯 컴포넌트 캐시 (호출마다 GetComponents 검색 방지, BeginPlay에서 1회 수집)
	UPROPERTY()
	TArray<TObjectPtr<UWidgetComponent>> CachedOverheadWidgets;

	// ========== 표정 시스템 ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Facial Expression")
	TObjectPtr<UDRFacialExpressionComponent> FacialExpressionComponent;

	// ========== 대기실 슬롯 텔레포트 ==========

	/** 서버에서 호출 → 모든 클라이언트(+서버)에서 즉시 위치 설정 (CMC 우회) */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastTeleportToSlot(FVector Location, FRotator Rotation);

	// ========== 자판기 사운드 동기화 (Plan2.md §3.3) ==========

	/** 일반 공격(코인) 발사 사운드. 서버에서 호출 → 모든 클라이언트 재생.
	    Unreliable: 0.3초당 1회 발사이므로 RPC 누락 시 큰 문제 없음. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayVendingCoinShot(FVector_NetQuantize Location);

	/** 잭팟 캡슐 발사 사운드. CapsuleTier=0(Bronze)/1(Silver)/2(Gold).
	    Reliable: 잭팟은 드물고 임팩트가 큰 이벤트이므로 누락되면 어색함. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayVendingCapsuleShot(uint8 CapsuleTier, FVector_NetQuantize Location);

	/** 자판기 스킬 버프 사용 사운드. Stacks에 따라 피치 증가.
	    Reliable: 1회성, Stacks가 정확히 전달되어야 함. */
	UFUNCTION(NetMulticast, Reliable)
	void MulticastPlayVendingSkillUse(uint8 NewStacks);

	/** 피격 표정 트리거. 서버에서 호출 → 모든 클라이언트에서 Heat 표정 표시.
	    Unreliable: 표정은 보조 연출이므로 1회 누락 허용. */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayHitReactFacial();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// 사망 몽타주가 이미 재생되었는지 (멀티캐스트와 RepNotify가 둘 다 도착해도 1회만 재생)
	bool bDeathMontagePlayed = false;

	// 사망 몽타주 재생 실제 구현 (모든 머신에서 호출 가능, 내부에서 중복 방지)
	virtual void PlayDeathMontage_Internal();

	// UPlayerCharacterClassInfo 기반 어트리뷰트 초기화
	virtual void InitializeDefaultAttributes() const override;

	// ========== 업그레이드 칩 ==========

	// 업그레이드 스탯을 얹는 Infinite GE (SetByCaller). BP 에서 GE_Upgrade_Stats 지정.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	TSubclassOf<UGameplayEffect> UpgradeStatEffectClass;

	// 업그레이드 적용 전후로 체력/물의 현재값 비율을 유지할지.
	// 스폰 시점엔 만피라 그대로 만피가 되고, 중간 재적용에서 공짜 회복이 생기지 않는다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Upgrade")
	bool bPreserveVitalRatioOnUpgradeApply = true;

	// 현재 적용 중인 업그레이드 GE 핸들 (재적용 시 먼저 제거 — ASC 가 PlayerState 에 있어 리스폰 후에도 남는다)
	FActiveGameplayEffectHandle UpgradeStatEffectHandle;

	// ========== ��ǰ ���� ==========

	// ��ǰ ���� ����
	UPROPERTY(ReplicatedUsing = OnRep_bIsCarryingPart, BlueprintReadOnly, Category = "Part System")
	bool bIsCarryingPart;

	// ��� �ִ� ��ǰ
	UPROPERTY(ReplicatedUsing = OnRep_CarriedPart, BlueprintReadOnly, Category = "Part System")
	TObjectPtr<class ADRCleanserPart> CarriedPart;

	// ��ǰ ȹ�� �ð� ���
	float LastPartPickupTime = 0.f;

	// ��ǰ ����Ʈ���� ��ٿ� �ð�
	UPROPERTY(EditDefaultsOnly, Category = "Part System", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float PartDropCooldown = 2.0f;

	// ========== ���ø����̼� �ݹ� ==========

	UFUNCTION()
	void OnRep_bIsCarryingPart();

	UFUNCTION()
	void OnRep_CarriedPart();

	// ========== �̵��ӵ� ���� ==========

	virtual float GetMoveSpeed() override;

private:
	// 부품 드롭 실제 구현 (쿨다운 검사 없음, 권한/보유 검사는 호출부에서 수행)
	void DoDropCarriedPart();

	// 부품 보유 상태 전환 통합 처리 (서버 전용)
	// bIsCarryingPart + State.Carrying 태그 + 비주얼 + 주변 상호작용 재평가를 한곳에서 수행
	// (클라 측 태그 토글은 OnRep_bIsCarryingPart 에서)
	void SetCarryingState(bool bNewCarrying, ADRCleanserPart* Part = nullptr);

	// ========== WaterPump 3P 빔 (내부) ==========

	// 비소유 클라이언트에서 관리하는 3P Niagara 빔 컴포넌트
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> WaterPumpThirdPersonBeam;

	// 3P 빔 업데이트 타이머
	FTimerHandle WaterPumpBeamUpdateTimer;

	// 3P 빔 보간용 현재 표시 위치
	FVector WaterPumpBeamDisplayEndPoint = FVector::ZeroVector;

	// 3P 빔 위치 업데이트 함수 (타이머 콜백)
	void UpdateWaterPumpThirdPersonBeam();

	// GAS 초기화
	virtual void InitAbilityActorInfo() override;

	// 이동 속도 바인딩 초기화 (PossessedBy, OnRep_PlayerState에서 공통 사용)
	void InitializeMoveSpeedBinding();

	// ī�޶� �ý���
	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class UCameraComponent> FollowCamera;
};
