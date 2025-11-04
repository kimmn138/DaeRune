// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "DRCharacter.generated.h"

class UWidgetComponent;
class ADRCleanserPart;

/**
 * 플레이어 캐릭터 클래스
 */
UCLASS()
class DAERUNE_API ADRCharacter : public ADRCharacterBase
{
	GENERATED_BODY()
	
public:
	ADRCharacter();
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	// 서버에서 컨트롤러가 빙의될 때 호출 (서버용 GAS 초기화)
	virtual void PossessedBy(AController* NewController) override;
	// PlayerState 리플리케이션 시 호출 (클라이언트용 GAS 초기화)
	virtual void OnRep_PlayerState() override;

	// 플레이어 전용 디버프 RepNotify 함수들
	virtual void OnRep_Stunned() override;
	virtual void OnRep_Burned() override;

	// 컨테이너 시스템 설정
	UPROPERTY(EditDefaultsOnly, Category = "Container System")
	int32 NumContainers = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Container System")
	float ContainerHealth = 100.f;

	// ========== 부품 시스템 ==========

	// 부품 보유 상태 조회
	UFUNCTION(BlueprintCallable, Category = "Part System")
	bool IsCarryingPart() const { return bIsCarryingPart; }

	// 부품 획득 처리
	UFUNCTION(BlueprintCallable, Category = "Part System")
	bool PickupPart(class ADRCleanserPart* Part);

	// 부품 설치 처리
	UFUNCTION(BlueprintCallable, Category = "Part System")
	void InstallCarriedPart();

protected:
	// ========== 부품 상태 ==========

	// 부품 보유 여부
	UPROPERTY(ReplicatedUsing = OnRep_bIsCarryingPart, BlueprintReadOnly, Category = "Part System")
	bool bIsCarryingPart;

	// 들고 있는 부품
	UPROPERTY(ReplicatedUsing = OnRep_CarriedPart, BlueprintReadOnly, Category = "Part System")
	TObjectPtr<class ADRCleanserPart> CarriedPart;

	// ========== 리플리케이션 콜백 ==========

	UFUNCTION()
	void OnRep_bIsCarryingPart();

	UFUNCTION()
	void OnRep_CarriedPart();

	// ========== 이동속도 관련 ==========

	virtual float GetMoveSpeed() override;

private:
	// GAS 초기화
	virtual void InitAbilityActorInfo() override;

	// 카메라 시스템
	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class UCameraComponent> FollowCamera;
};
