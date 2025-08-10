// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "DRCharacter.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API ADRCharacter : public ADRCharacterBase
{
	GENERATED_BODY()
	
public:
	ADRCharacter();
	virtual void PossessedBy(AController* NewController) override;
	virtual void OnRep_PlayerState() override;

	virtual void OnRep_Stunned() override;
	virtual void OnRep_Burned() override;

	// 컨테이너 시스템 설정
	UPROPERTY(EditDefaultsOnly, Category = "Container System")
	int32 NumContainers = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Container System")
	float ContainerHealth = 100.f;

private:
	virtual void InitAbilityActorInfo() override;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class UCameraComponent> FollowCamera;
};
