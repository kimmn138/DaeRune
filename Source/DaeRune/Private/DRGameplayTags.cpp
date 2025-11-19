// Copyright DaeRune


#include "DRGameplayTags.h"
#include "GameplayTagsManager.h"

FDRGameplayTags FDRGameplayTags::GameplayTags;

void FDRGameplayTags::InitializeNativeGameplayTags()
{
	/*
	 * 리소스 비용
	 */

	GameplayTags.Cost_Water = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Cost.Water"),
		FString("Water cost for abilities")
	);

	/*
	 * 기본 속성
	 */

	GameplayTags.Attributes_Primary_MaxHealth = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Primary.MaxHealth"),
		FString("Maximum amount of Health obtainable")
	);

	GameplayTags.Attributes_Primary_MaxWater = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Primary.MaxWater"),
		FString("Maximum amount of Water obtainable")
	);

	GameplayTags.Attributes_Primary_MoveSpeed = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Attributes.Primary.MoveSpeed"),
		FString("Maximum amount of Speed obtainable")
	);

	/*
	 * 입력 태그
	 */

	GameplayTags.InputTag_LMB = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.LMB"),
		FString("Input Tag for Left Mouse Button")
	);

	GameplayTags.InputTag_RMB = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.RMB"),
		FString("Input Tag for Right Mouse Button")
	);

	GameplayTags.InputTag_Q = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.Q"),
		FString("Input Tag for Q key")
	);

	GameplayTags.InputTag_E = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("InputTag.E"),
		FString("Input Tag for E key")
	);

	/*
	 * 플레이어 상태
	 */

	GameplayTags.State_Corrupt = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Corrupt"),
		FString("State Tag for Player In Corrupt")
	);

	GameplayTags.State_Carrying = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Carrying"),
		FString("State Tag for Player In Carrying Part")
	);

	/*
	 * 기본 데미지, 타입 별 데미지
	 */

	GameplayTags.Damage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage"),
		FString("Damage")
	);

	GameplayTags.Damage_Fire = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage.Fire"),
		FString("Fire Damage Type")
	);

	GameplayTags.Damage_Lightning = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage.Lightning"),
		FString("Lightning Damage Type")
	); 

	GameplayTags.Damage_Arcane = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage.Arcane"),
		FString("Arcane Damage Type")
	);

	GameplayTags.Damage_Physical = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage.Physical"),
		FString("Physical Damage Type")
	);

	GameplayTags.Damage_Bite = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage.Bite"),
		FString("Bite Damage Type")
	);

	/*
	 * 기본 힐
	 */

	GameplayTags.Heal = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Heal"),
		FString("Heal")
	);

	/*
	 * 디버프 효과
	 */

	GameplayTags.Buff_Elite = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.Elite"),
		FString("Buff for Elite Monster")
	);
	
	/*
	 * 디버프 효과
	 */

	GameplayTags.Debuff_Arcane = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Arcane"),
		FString("Debuff for Arcane damage")
	);

	GameplayTags.Debuff_Burn = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Burn"),
		FString("Debuff for Fire damage")
	);

	GameplayTags.Debuff_Physical = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Physical"),
		FString("Debuff for Physical damage")
	);

	GameplayTags.Debuff_Stun = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Stun"),
		FString("Debuff for Lightning damage")
	);

	GameplayTags.Debuff_Bleed = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Bleed"),
		FString("Debuff for Bleed damage")
	);

	GameplayTags.Debuff_Elite = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Elite"),
		FString("Debuff for Elite Monster")
	);

	/*
	 * 디버프 계산용 태그
	 */

	GameplayTags.Debuff_Chance = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Chance"),
		FString("Debuff Chance")
	);

	GameplayTags.Debuff_Damage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Damage"),
		FString("Debuff Damage")
	);

	GameplayTags.Debuff_Duration = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Duration"),
		FString("Debuff Duration")
	);

	/*
	 * 데미지 타입 배열
	 */

	GameplayTags.DamageTypeTags.Add(GameplayTags.Damage_Arcane);
	GameplayTags.DamageTypeTags.Add(GameplayTags.Damage_Lightning);
	GameplayTags.DamageTypeTags.Add(GameplayTags.Damage_Physical);
	GameplayTags.DamageTypeTags.Add(GameplayTags.Damage_Fire);
	GameplayTags.DamageTypeTags.Add(GameplayTags.Damage_Bite);

	/*
	 * 데미지 타입, 디버프 매핑
	 */

	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Arcane, GameplayTags.Debuff_Arcane);
	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Lightning, GameplayTags.Debuff_Stun);
	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Physical, GameplayTags.Debuff_Physical);
	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Fire, GameplayTags.Debuff_Burn);
	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Bite, GameplayTags.Debuff_Bleed);

	/*
	 * 타격 반응 이펙트
	 */

	GameplayTags.Effects_HitReact = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Effects.HitReact"),
		FString("Tag granted when Hit Reacting")
	);

	/*
	 * 물 시스템 (SetByCaller 방식)
	 */

	GameplayTags.Water_SetByCaller_Reduction = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Water.SetByCaller.Reduction"),
		FString("SetByCaller tag for water reduction amount")
	);

	GameplayTags.Water_SetByCaller_Grant = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Water.SetByCaller.Grant"),
		FString("SetByCaller tag for water grant amount")
	);

	/*
	 * 어빌리티 분류
	 */

	GameplayTags.Abilities_Attack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Attack"),
		FString("Attack Ability Tag")
	);

	GameplayTags.Abilities_Summon = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Summon"),
		FString("Summon Ability Tag")
	);

	GameplayTags.Abilities_Skill1 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Skill1"),
		FString("Skill1 Ability Tag")
	);

	/*
	 * 구체적 어빌리티 
	 */

	GameplayTags.Abilities_Fire_FireBolt = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Fire.FireBolt"),
		FString("FireBolt Ability Tag")
	);

	GameplayTags.Abilities_Lightning_Electrocute = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Lightning.Electrocute"),
		FString("Electrocute Ability Tag")
	);

	GameplayTags.Abilities_GardenRobot_ClawSwipe = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.GardenRobot.ClawSwipe"),
		FString("ClawSwipe Ability Tag")
	);

	GameplayTags.Abilities_GardenRobot_WaterPump = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.GardenRobot.WaterPump"),
		FString("WaterPump Ability Tag")
	);

	GameplayTags.Abilities_GardenRobot_SeedCannon = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.GardenRobot.SeedCannon"),
		FString("SeedCannon Ability Tag")
	);

	/*
	 * 타격 반응
	 */

	GameplayTags.Abilities_HitReact = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.HitReact"),
		FString("Hit React Ability")
	);

	/*
	 * 어빌리티 타입 분류
	 */

	GameplayTags.Abilities_Type_None = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Type.None"),
		FString("Type None")
	);

	GameplayTags.Abilities_Type_Offensive = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Type.Offensive"),
		FString("Type Offensive")
	);

	GameplayTags.Abilities_Type_Passive = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Type.Passive"),
		FString("Type Passive")
	);

	/*
	 * 어빌리티 쿨다운
	 */

	GameplayTags.Cooldown_Fire_FireBolt = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Cooldown.Fire.FireBolt"),
		FString("FireBolt Cooldown Tag")
	);

	/*
	 * 공격 위치 소켓
	 */

	GameplayTags.CombatSocket_Weapon = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("CombatSocket.Weapon"),
		FString("Weapon")
	);

	GameplayTags.CombatSocket_RightHand = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("CombatSocket.RightHand"),
		FString("Right Hand")
	);

	GameplayTags.CombatSocket_LeftHand = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("CombatSocket.LeftHand"),
		FString("Left Hand")
	);

	GameplayTags.CombatSocket_Tail = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("CombatSocket.Tail"),
		FString("Tail")
	);

	/*
	 * 공격 몽타주
	 */

	GameplayTags.Montage_Attack_1 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Montage.Attack.1"),
		FString("Attack 1")
	);

	GameplayTags.Montage_Attack_2 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Montage.Attack.2"),
		FString("Attack 2")
	);

	GameplayTags.Montage_Attack_3 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Montage.Attack.3"),
		FString("Attack 3")
	);

	GameplayTags.Montage_Attack_4 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Montage.Attack.4"),
		FString("Attack 4")
	);

	/*
	 * 입력 블록
	 */

	GameplayTags.Player_Block_InputHeld = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Block.InputHeld"),
		FString("Block Input Held callback for input")
	);

	GameplayTags.Player_Block_InputPressed = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Block.InputPressed"),
		FString("Block Input Pressed callback for input")
	);

	GameplayTags.Player_Block_InputReleased = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Player.Block.InputReleased"),
		FString("Block Input Released callback for input")
	);
}
