// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRCharacter.generated.h"

class UWidgetComponent;
class ADRCleanserPart;
class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMesh;
class UDRFacialExpressionComponent;
class UAnimMontage;

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
	EPlayerCharacterClass PlayerCharacterClass = EPlayerCharacterClass::GardenRobot;

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

	// 부품 보유 상태가 바뀐 직후 현재 오버랩 중인 CleanserSite/CleanserPart 들의 UI/감지를 재평가
	// (오버랩 영역 안에서 집어들기/내려놓기 시 UI가 갱신되지 않는 문제 해결)
	void RefreshNearbyInteractions();

	// ========== ī�޶� ==========

	// ���� ī�޶� ����
	UFUNCTION(BlueprintImplementableEvent, Category = "Death")
	void PlayDeathCameraAnimation();

	// ========== WaterPump 3P 빔 (리플리케이트 상태) ==========

	// 물대포 활성화 상태 (서버에서 설정, RepNotify로 비소유 클라이언트에서 3P 빔 관리)
	UPROPERTY(ReplicatedUsing=OnRep_WaterPumpActive, BlueprintReadOnly, Category = "Effects")
	bool bWaterPumpActive = false;

	// 물대포 빔 끝점 (서버에서 0.1초마다 갱신, 비소유 클라이언트에서 3P 빔 위치 업데이트용)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Effects")
	FVector WaterPumpBeamEndPoint = FVector::ZeroVector;

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
