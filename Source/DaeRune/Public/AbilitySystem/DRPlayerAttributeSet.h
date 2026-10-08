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

	// ★Plan5(탑승칩)★ 오염 중이면 최대 체력을 CorruptMaxHealth(100)로 다시 맞추고 체력을 KeepHealth(≤ 100)로 되돌린다 (Plan5 D-87).
	// ADRCharacter::RefreshUpgradeEffects 가 칩 GE 를 다시 붙이거나 뗀 뒤 부른다(서버)
	void ReapplyCorruptMaxHealth(float KeepHealth);

protected:
	virtual void HandleIncomingDamage(const FEffectProperties& Props) override;
	virtual void HandleIncomingHealing(const FEffectProperties& Props) override;
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	// 부품을 들고 있을 때 받는 데미지 배율 (1.5배). 피격 시 부품을 떨어뜨린다.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Part System")
	float CarryingPartDamageModifier = 1.5f;

private:
	void ProcessCorruptedDamage(const FEffectProperties& Props, float Damage);
	void ProcessNormalDamage(const FEffectProperties& Props, float Damage);
	float CalculateContainerDamage(float CurrentHealth, float Damage) const;
	void ApplyHitReactAndKnockback(const FEffectProperties& Props);
	void HandleCorruptionPurification(const FEffectProperties& Props, float HealAmount);

	// ★Plan5(탑승칩)★ MaxHealth 의 ★현재값★을 Desired 로 맞춘다 (Plan5 D-87).
	// SetMaxHealth 는 BaseValue 를 바꿔 칩 GE(AddBase)의 가산이 그 위에 다시 붙는다 — 그 몫을 빼고 넣는다
	void SetMaxHealthCurrentValue(float Desired);

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