// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * DRGameplayTags
 *
 * Singleton containing native Gameplay Tags
 */

struct FDRGameplayTags // 네이티브 게임플레이 태그 싱글톤 구조체임
{
public:
	// 싱글톤 인스턴스 접근임
	static const FDRGameplayTags& Get() { return GameplayTags; }
	// 네이티브 태그 등록 기능임
	static void InitializeNativeGameplayTags();

	// Primary Attributes 그룹임
	FGameplayTag Attributes_Primary_MaxHealth;

	// Input Tags 그룹임
	FGameplayTag InputTag_LMB;
	FGameplayTag InputTag_RMB;
	FGameplayTag InputTag_1;
	FGameplayTag InputTag_2;
	FGameplayTag InputTag_3;
	FGameplayTag InputTag_4;
	FGameplayTag InputTag_Q;
	FGameplayTag InputTag_E;
	FGameplayTag InputTag_Passive_1;
	FGameplayTag InputTag_Passive_2;

	// Damage Types 그룹임
	FGameplayTag Damage;
	FGameplayTag Damage_Fire;
	FGameplayTag Damage_Lightning;
	FGameplayTag Damage_Arcane; 
	FGameplayTag Damage_Physical;

	// Debuff Tags 그룹임
	FGameplayTag Debuff_Burn; 
	FGameplayTag Debuff_Stun;
	FGameplayTag Debuff_Arcane;
	FGameplayTag Debuff_Physical;

	FGameplayTag Debuff_Chance; 
	FGameplayTag Debuff_Damage;
	FGameplayTag Debuff_Duration;
	FGameplayTag Debuff_Frequency;

	// Abilities Tags 그룹임
	FGameplayTag Abilities_Attack;
	FGameplayTag Abilities_Summon;

	FGameplayTag Abilities_HitReact;

	// Abilities Type 그룹임
	FGameplayTag Abilities_Type_Offensive; 
	FGameplayTag Abilities_Type_Passive;
	FGameplayTag Abilities_Type_None;

	// Specific Ability Tags 그룹임
	FGameplayTag Abilities_Fire_FireBolt;
	FGameplayTag Abilities_Lightning_Electrocute;

	// Cooldown Tags 그룹임
	FGameplayTag Cooldown_Fire_FireBolt;

	// Combat Socket Tags 그룹임
	FGameplayTag CombatSocket_Weapon;
	FGameplayTag CombatSocket_RightHand;
	FGameplayTag CombatSocket_LeftHand;
	FGameplayTag CombatSocket_Tail;

	// Montage Tags 그룹임
	FGameplayTag Montage_Attack_1;
	FGameplayTag Montage_Attack_2;
	FGameplayTag Montage_Attack_3;
	FGameplayTag Montage_Attack_4;

	// Helper Containers 그룹임
	TArray<FGameplayTag> DamageTypeTags;
	TMap<FGameplayTag, FGameplayTag> DamageTypesToDebuffs;

	// Effects Tags 그룹임
	FGameplayTag Effects_HitReact;

	// Player Block Input Tags 그룹임
	FGameplayTag Player_Block_InputPressed; 
	FGameplayTag Player_Block_InputHeld;
	FGameplayTag Player_Block_InputReleased;

private:
	static FDRGameplayTags GameplayTags; // 내부 싱글톤 인스턴스임
};
