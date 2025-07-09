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

	/** Combat Interface */
	virtual int32 GetPlayerLevel_Implementation() override;
	virtual EPlayerCharacterClass GetPlayerCharacterClass_Implementation() override;
	/** end Combat Interface */

	virtual void OnRep_Stunned() override;
	virtual void OnRep_Burned() override;

protected:
	virtual void InitializeDefaultAttributes() const override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Character Class Defaults")
	EPlayerCharacterClass CharacterClass = EPlayerCharacterClass::GardenRobot;

private:
	virtual void InitAbilityActorInfo() override;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class UCameraComponent> FollowCamera;
};
