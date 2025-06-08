// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/DRDamageGameplayAbility.h"
#include "DRBeamSpell.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRBeamSpell : public UDRDamageGameplayAbility
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void StoreCameraDataInfo(const FHitResult& HitResult);

	UFUNCTION(BlueprintCallable)
	void StoreOwnerVariables();

	UFUNCTION(BlueprintCallable)
	void TraceFirstTarget(const FVector& BeamTargetLocation);

protected:
	UPROPERTY(BlueprintReadWrite, Category = "Beam")
	FVector CameraHitLocation;

	UPROPERTY(BlueprintReadWrite, Category = "Beam")
	TObjectPtr<AActor> CameraHitActor;

	UPROPERTY(BlueprintReadWrite, Category = "Beam")
	TObjectPtr<ACharacter> OwnerCharacter;
};
