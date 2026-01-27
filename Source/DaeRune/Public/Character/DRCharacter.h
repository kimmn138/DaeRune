// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "Voice/DRVOIPTalker.h"
#include "DRCharacter.generated.h"

class UWidgetComponent;
class ADRCleanserPart;

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

	// ========== 1��Ī/3��Ī �޽� �ý��� ==========

	// 1��Ī �޽�
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Mesh")
	TObjectPtr<USkeletalMeshComponent> FirstPersonMesh;

	// �޽� ���ü� ������Ʈ
	void UpdateMeshVisibility();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

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
