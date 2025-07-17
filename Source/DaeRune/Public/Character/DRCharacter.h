// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "DRCharacter.generated.h"

/**
 * 캐릭터 기본 클래스 정의
 */
UCLASS()
class DAERUNE_API ADRCharacter : public ADRCharacterBase
{
	GENERATED_BODY()
	
public:
	ADRCharacter();
	// 서버 소유 시 처리 메서드
	virtual void PossessedBy(AController* NewController) override;
	// 플레이어 상태 복제 응답 메서드
	virtual void OnRep_PlayerState() override;

	/** Combat Interface */
	 // 플레이어 레벨 반환 메서드
	virtual int32 GetPlayerLevel_Implementation() override;
	// 플레이어 클래스 반환 메서드
	virtual EPlayerCharacterClass GetPlayerCharacterClass_Implementation() override;
	/** end Combat Interface */

	// 기절 상태 복제 응답 메서드	
	virtual void OnRep_Stunned() override;
	// 화상 상태 복제 응답 메서드
	virtual void OnRep_Burned() override;

protected:
	// 기본 특성 초기화 메서드
	virtual void InitializeDefaultAttributes() const override;

	// 캐릭터 클래스 설정 변수
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	EPlayerCharacterClass CharacterClass = EPlayerCharacterClass::GardenRobot;

private:
	// 어빌리티 액터 정보 초기화 메서드
	virtual void InitAbilityActorInfo() override;

	// 카메라 붐 컴포넌트 포인터
	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class USpringArmComponent> CameraBoom;

	// 팔로우 카메라 컴포넌트 포인터
	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class UCameraComponent> FollowCamera;
};
