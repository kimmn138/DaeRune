// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Voice/DRVOIPTalker.h"
#include "DRCharacter.generated.h"

class UWidgetComponent;
class ADRCleanserPart;
class UNiagaraComponent;
class UNiagaraSystem;
class UStaticMesh;
class UDRFacialExpressionComponent;

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

	// ========== 음성 채팅 ==========

	// VOIPTalker 컴포넌트 반환
	UFUNCTION(BlueprintCallable, Category = "Voice Chat")
	UDRVOIPTalker* GetVOIPTalker() const { return VOIPTalkerComponent; }

	// VOIP 청취 컴포넌트
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Voice Chat")
	TObjectPtr<UDRVOIPTalker> VOIPTalkerComponent;

	FTimerHandle PlayerStateRegisterTimerHandle;

	void TryRegisterVoiceTalker();
	void RegisterVoiceTalker();

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

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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

	// 표정 태그 콜백
	void HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount);
	void AttackSpeedBuffTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

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
