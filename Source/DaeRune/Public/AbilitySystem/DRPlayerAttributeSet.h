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
	// �����̳� ���� ���� (���� ���� �� �� ����)
	void SetContainerInfo(int32 InNumContainers, float InContainerHealth);

	// �����̳� ���� ������
	int32 GetNumContainers() const { return NumContainers; }
	float GetContainerHealth() const { return ContainerHealth; }
	float GetCorruptMaxHealth() const { return CorruptMaxHealth; }

	// ���� �����̳� �ε��� ���
	int32 GetCurrentContainerIndex() const;

	void EnterCorruptedState(const FEffectProperties& Props);
	void ExitCorruptedState(const FEffectProperties& Props);

	bool IsCorrupted() const { return bCorrupted; }

protected:
	virtual void HandleIncomingDamage(const FEffectProperties& Props) override;
	virtual void HandleIncomingHealing(const FEffectProperties& Props) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Debuff")
	float EliteDebuffModifier = 1.5f;

	// 부품을 들고 있을 때 받는 데미지 배율 (1.5배). 피격 시 부품을 떨어뜨린다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Part System")
	float CarryingPartDamageModifier = 1.5f;

private:
	void ProcessCorruptedDamage(const FEffectProperties& Props, float Damage);
	void ProcessNormalDamage(const FEffectProperties& Props, float Damage);
	float CalculateContainerDamage(float CurrentHealth, float Damage) const;
	void ApplyHitReactAndKnockback(const FEffectProperties& Props);
	void HandleCorruptionPurification(const FEffectProperties& Props, float HealAmount);

	// 로비/튜토리얼 등 사망 방지가 필요한 모드인지 확인
	bool ShouldPreventDeath() const;

	// ĳ���ͺ� �����̳� ���� (Replicate ���ʿ� - �� Ŭ���̾�Ʈ�� ��ü ���)
	UPROPERTY()
	int32 NumContainers = 4;

	UPROPERTY()
	float ContainerHealth = 100.f;

	UPROPERTY()
	float CorruptMaxHealth = 100.f;

	bool bCorrupted = false;

	// �����÷ο� �Ӱ谪
	static constexpr float OVERFLOW_THRESHOLD = 0.1f;
};