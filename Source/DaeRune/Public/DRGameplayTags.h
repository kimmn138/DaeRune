// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * DRGameplayTags
 *
 * ������Ʈ �������� ���Ǵ� GameplayTag���� �߾� �����ϴ� �̱��� Ŭ����
 */

struct FDRGameplayTags
{
public:
	static const FDRGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeGameplayTags();

	// ���ҽ� ���
	FGameplayTag Cost_Water;

	// �⺻ �Ӽ�
	FGameplayTag Attributes_Primary_MaxHealth;
	FGameplayTag Attributes_Primary_MaxWater;
	FGameplayTag Attributes_Primary_MoveSpeed;

	// �Է� �±�
	FGameplayTag InputTag_LMB;
	FGameplayTag InputTag_RMB;
	FGameplayTag InputTag_Q;
	FGameplayTag InputTag_E;

	// 플레이어 상태
	FGameplayTag State_Corrupt;
	FGameplayTag State_Carrying;
	FGameplayTag State_InCombat;
	FGameplayTag State_VendingMachine_JackpotReady;

	// 적 상태
	FGameplayTag State_Enraged;
	FGameplayTag State_Aggroed;
	FGameplayTag State_HitReacting;
	FGameplayTag State_LockedDown;
	FGameplayTag State_BallForm;

	// Phase1 적 식별 태그 (플레이어에게 주는 대미지 30% 감소 트리거)
	FGameplayTag State_Enemy_Phase1;

	// �⺻ ������, Ÿ�� �� ������
	FGameplayTag Damage;
	FGameplayTag Damage_Fire;
	FGameplayTag Damage_Lightning;
	FGameplayTag Damage_Arcane; 
	FGameplayTag Damage_Physical;
	FGameplayTag Damage_Bite;

	// �⺻ ��
	FGameplayTag Heal;

	// ���� ȿ��
	FGameplayTag Buff_Elite;
	FGameplayTag Buff_Elite_Roar;
	FGameplayTag Buff_VendingMachine_AttackSpeed;
	
	// ����� ȿ��
	FGameplayTag Debuff_Burn; 
	FGameplayTag Debuff_Stun;
	FGameplayTag Debuff_Arcane;
	FGameplayTag Debuff_Physical;
	FGameplayTag Debuff_Bleed;
	FGameplayTag Debuff_Elite;

	// ����� ���� �±�
	FGameplayTag Debuff_Chance; 
	FGameplayTag Debuff_Damage;
	FGameplayTag Debuff_Duration;

	// Ÿ�� ���� ����Ʈ
	FGameplayTag Effects_HitReact;
	FGameplayTag Effects_CannotAttack;

	// �� �ý��� (SetByCaller ���)
	FGameplayTag Water_SetByCaller_Reduction;
	FGameplayTag Water_SetByCaller_Grant;

	// �����Ƽ �з�
	FGameplayTag Abilities_Attack;
	FGameplayTag Abilities_Death;
	FGameplayTag Abilities_Summon;
	FGameplayTag Abilities_Skill1;
	FGameplayTag Abilities_Skill2;

	// ��ü�� �����Ƽ
	FGameplayTag Abilities_Fire_FireBolt;
	FGameplayTag Abilities_Lightning_Electrocute;
	FGameplayTag Abilities_GardenRobot_ClawSwipe;
	FGameplayTag Abilities_GardenRobot_WaterPump;
	FGameplayTag Abilities_GardenRobot_SeedCannon;
	FGameplayTag Abilities_VendingMachine_BasicAttack;
	FGameplayTag Abilities_VendingMachine_AttackSpeedBuff;

	// Ÿ�� ����
	FGameplayTag Abilities_HitReact;

	// �����Ƽ Ÿ�� �з�
	FGameplayTag Abilities_Type_Offensive;
	FGameplayTag Abilities_Type_Passive;
	FGameplayTag Abilities_Type_None;

	// �����Ƽ ��Ÿ��
	FGameplayTag Cooldown_Fire_FireBolt;
	FGameplayTag Cooldown_Armadillo_RollCharge;
	FGameplayTag Cooldown_Elite_Sweep;
	FGameplayTag Cooldown_Elite_Roar;

	// ���� ��ġ ����
	FGameplayTag CombatSocket_Weapon;
	FGameplayTag CombatSocket_RightHand;
	FGameplayTag CombatSocket_LeftHand;
	FGameplayTag CombatSocket_Tail;

	// ���� ��Ÿ��
	FGameplayTag Montage_Attack_1;
	FGameplayTag Montage_Attack_2;
	FGameplayTag Montage_Attack_3;
	FGameplayTag Montage_Attack_4;

	// ������ Ÿ�� �迭
	TArray<FGameplayTag> DamageTypeTags;
	// ������ Ÿ��, ����� ����
	TMap<FGameplayTag, FGameplayTag> DamageTypesToDebuffs;

	// �Է� ����
	FGameplayTag Player_Block_InputPressed; 
	FGameplayTag Player_Block_InputHeld;
	FGameplayTag Player_Block_InputReleased;

	// ����
	FGameplayTag GameplayCue_Player_Damage;
	FGameplayTag GameplayCue_Player_Death;
	FGameplayTag GameplayCue_Player_LowHealth;
	FGameplayTag GameplayCue_Player_WaterDepleted;
	FGameplayTag GameplayCue_Enemy_Damage;
	FGameplayTag GameplayCue_Cleanser_Damage;
	FGameplayTag GameplayCue_Skill_WaterPump;
	FGameplayTag GameplayCue_Skill_ClawSwipe;

private:
	static FDRGameplayTags GameplayTags;
};
