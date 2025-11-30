// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

/**
 * DRGameplayTags
 *
 * 프로젝트 전역에서 사용되는 GameplayTag들을 중앙 관리하는 싱글톤 클래스
 */

struct FDRGameplayTags
{
public:
	static const FDRGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeGameplayTags();

	// 리소스 비용
	FGameplayTag Cost_Water;

	// 기본 속성
	FGameplayTag Attributes_Primary_MaxHealth;
	FGameplayTag Attributes_Primary_MaxWater;
	FGameplayTag Attributes_Primary_MoveSpeed;

	// 입력 태그
	FGameplayTag InputTag_LMB;
	FGameplayTag InputTag_RMB;
	FGameplayTag InputTag_Q;
	FGameplayTag InputTag_E;

	// 플레이어 상태
	FGameplayTag State_Corrupt;
	FGameplayTag State_Carrying;
	FGameplayTag Enemy_Detected;

	// 기본 데미지, 타입 별 데미지
	FGameplayTag Damage;
	FGameplayTag Damage_Fire;
	FGameplayTag Damage_Lightning;
	FGameplayTag Damage_Arcane; 
	FGameplayTag Damage_Physical;
	FGameplayTag Damage_Bite;

	// 기본 힐
	FGameplayTag Heal;

	// 버프 효과
	FGameplayTag Buff_Elite;
	
	// 디버프 효과
	FGameplayTag Debuff_Burn; 
	FGameplayTag Debuff_Stun;
	FGameplayTag Debuff_Arcane;
	FGameplayTag Debuff_Physical;
	FGameplayTag Debuff_Bleed;
	FGameplayTag Debuff_Elite;

	// 디버프 계산용 태그
	FGameplayTag Debuff_Chance; 
	FGameplayTag Debuff_Damage;
	FGameplayTag Debuff_Duration;

	// 타격 반응 이펙트
	FGameplayTag Effects_HitReact;

	// 물 시스템 (SetByCaller 방식)
	FGameplayTag Water_SetByCaller_Reduction;
	FGameplayTag Water_SetByCaller_Grant;

	// 어빌리티 분류
	FGameplayTag Abilities_Attack;
	FGameplayTag Abilities_Summon;
	FGameplayTag Abilities_Skill1;

	// 구체적 어빌리티 
	FGameplayTag Abilities_Fire_FireBolt;
	FGameplayTag Abilities_Lightning_Electrocute;
	FGameplayTag Abilities_GardenRobot_ClawSwipe;
	FGameplayTag Abilities_GardenRobot_WaterPump;
	FGameplayTag Abilities_GardenRobot_SeedCannon;

	// 타격 반응
	FGameplayTag Abilities_HitReact;

	// 어빌리티 타입 분류
	FGameplayTag Abilities_Type_Offensive;
	FGameplayTag Abilities_Type_Passive;
	FGameplayTag Abilities_Type_None;

	// 어빌리티 쿨타임
	FGameplayTag Cooldown_Fire_FireBolt;

	// 공격 위치 소켓
	FGameplayTag CombatSocket_Weapon;
	FGameplayTag CombatSocket_RightHand;
	FGameplayTag CombatSocket_LeftHand;
	FGameplayTag CombatSocket_Tail;

	// 공격 몽타주
	FGameplayTag Montage_Attack_1;
	FGameplayTag Montage_Attack_2;
	FGameplayTag Montage_Attack_3;
	FGameplayTag Montage_Attack_4;

	// 데미지 타입 배열
	TArray<FGameplayTag> DamageTypeTags;
	// 데미지 타입, 디버프 매핑
	TMap<FGameplayTag, FGameplayTag> DamageTypesToDebuffs;

	// 입력 블록
	FGameplayTag Player_Block_InputPressed; 
	FGameplayTag Player_Block_InputHeld;
	FGameplayTag Player_Block_InputReleased;

private:
	static FDRGameplayTags GameplayTags;
};
