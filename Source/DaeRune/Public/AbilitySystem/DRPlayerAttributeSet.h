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
	// 컨테이너 정보 설정 (게임 시작 시 한 번만)
	void SetContainerInfo(int32 InNumContainers, float InContainerHealth);

	// 컨테이너 정보 접근자
	int32 GetNumContainers() const { return NumContainers; }
	float GetContainerHealth() const { return ContainerHealth; }
	float GetCorruptMaxHealth() const { return CorruptMaxHealth; }

	// 현재 컨테이너 인덱스 계산
	int32 GetCurrentContainerIndex() const;

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
	void HandleCorruptionPurification(const FEffectProperties& Props, float HealAmount);

	// 캐릭터별 컨테이너 설정 (Replicate 불필요 - 각 클라이언트가 자체 계산)
	UPROPERTY()
	int32 NumContainers = 4;

	UPROPERTY()
	float ContainerHealth = 100.f;

	UPROPERTY()
	float CorruptMaxHealth = 100.f;

	bool bCorrupted = false;

	// 오버플로우 임계값
	static constexpr float OVERFLOW_THRESHOLD = 0.1f;
};