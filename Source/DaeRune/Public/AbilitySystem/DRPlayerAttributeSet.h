// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "DRPlayerAttributeSet.generated.h"

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRPlayerAttributeSet : public UDRAttributeSet
{
	GENERATED_BODY()
	
public:
	// 체력 시스템 상수들을 public static으로 노출
	static constexpr float CONTAINER_HEALTH = 100.f;
	static constexpr int32 NUM_CONTAINERS = 4;
	static constexpr float CORRUPT_MAX_HEALTH = 100.f;
	static constexpr float NORMAL_MAX_HEALTH = CONTAINER_HEALTH * NUM_CONTAINERS;
	static constexpr float OVERFLOW_THRESHOLD = 0.1f;

	void EnterCorruptedState(const FEffectProperties& Props);
	void ExitCorruptedState(const FEffectProperties& Props);

	bool IsCorrupted() const { return bCorrupted; }

protected:
	virtual void HandleIncomingDamage(const FEffectProperties& Props) override;
	virtual void HandleIncomingHealing(const FEffectProperties& Props) override;

private:
	void ProcessCorruptedDamage(const FEffectProperties& Props, float Damage);
	void ProcessNormalDamage(const FEffectProperties& Props, float Damage);
	float CalculateContainerDamage(float CurrentHealth, float Damage) const;
	void ApplyHitReactAndKnockback(const FEffectProperties& Props);

	bool bCorrupted = false;
};
