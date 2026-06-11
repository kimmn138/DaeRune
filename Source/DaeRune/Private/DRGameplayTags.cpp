// Copyright DaeRune


#include "DRGameplayTags.h"
#include "GameplayTagsManager.h"

FDRGameplayTags FDRGameplayTags::GameplayTags;

void FDRGameplayTags::InitializeNativeGameplayTags()
{
	/*
	 * ���ҽ� ���
	 */

	GameplayTags.Cost_Water = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Cost.Water"),
		FString("Water cost for abilities")
	);

	/*
	 * �⺻ �Ӽ�
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
	 * �Է� �±�
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
		FString("Player is corrupted (water depleted)")
	);

	GameplayTags.State_Carrying = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Carrying"),
		FString("Player is carrying a part")
	);

	GameplayTags.State_InCombat = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.InCombat"),
		FString("Player is in combat (recently took damage)")
	);

	GameplayTags.State_VendingMachine_JackpotReady = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.VendingMachine.JackpotReady"),
		FString("VendingMachine Jackpot stacks reached max")
	);

	/*
	 * 적 상태
	 */

	GameplayTags.State_Enraged = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Enraged"),
		FString("Enemy is enraged (low health, increased aggression)")
	);

	GameplayTags.State_Aggroed = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Aggroed"),
		FString("Enemy has aggro on a target")
	);

	GameplayTags.State_HitReacting = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.HitReacting"),
		FString("Enemy is in hit reaction state")
	);

	GameplayTags.State_LockedDown = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.LockedDown"),
		FString("Flying enemy is locked down after skill use (cannot move or attack)")
	);

	GameplayTags.State_BallForm = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.BallForm"),
		FString("Armadillo is in ball form")
	);

	GameplayTags.State_Enemy_Phase1 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Enemy.Phase1"),
		FString("Enemy belongs to New Phase1; deals 30% reduced damage to players.")
	);

	/*
	 * �⺻ ������, Ÿ�� �� ������
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
	 * �⺻ ��
	 */

	GameplayTags.Heal = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Heal"),
		FString("Heal")
	);

	/*
	 * ����� ȿ��
	 */

	GameplayTags.Buff_Elite = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.Elite"),
		FString("Buff for Elite Monster")
	);

	GameplayTags.Buff_Elite_Roar = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.Elite.Roar"),
		FString("Buff applied to allied enemies by Elite Monster Roar Aura")
	);

	GameplayTags.Buff_VendingMachine_AttackSpeed = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.VendingMachine.AttackSpeed"),
		FString("VendingMachine Attack Speed Buff")
	);
	
	/*
	 * ����� ȿ��
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
	 * ����� ���� �±�
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
	 * ������ Ÿ�� �迭
	 */

	GameplayTags.DamageTypeTags.Add(GameplayTags.Damage_Arcane);
	GameplayTags.DamageTypeTags.Add(GameplayTags.Damage_Lightning);
	GameplayTags.DamageTypeTags.Add(GameplayTags.Damage_Physical);
	GameplayTags.DamageTypeTags.Add(GameplayTags.Damage_Fire);
	GameplayTags.DamageTypeTags.Add(GameplayTags.Damage_Bite);

	/*
	 * ������ Ÿ��, ����� ����
	 */

	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Arcane, GameplayTags.Debuff_Arcane);
	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Lightning, GameplayTags.Debuff_Stun);
	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Physical, GameplayTags.Debuff_Physical);
	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Fire, GameplayTags.Debuff_Burn);
	GameplayTags.DamageTypesToDebuffs.Add(GameplayTags.Damage_Bite, GameplayTags.Debuff_Bleed);

	/*
	 * Ÿ�� ���� ����Ʈ
	 */

	GameplayTags.Effects_HitReact = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Effects.HitReact"),
		FString("Tag granted when Hit Reacting")
	);

	GameplayTags.Effects_CannotAttack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Effects.CannotAttack"),
		FString("Tag that blocks attack abilities while active")
	);

	/*
	 * �� �ý��� (SetByCaller ���)
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
	 * �����Ƽ �з�
	 */

	GameplayTags.Abilities_Attack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Attack"),
		FString("Attack Ability Tag")
	);

	GameplayTags.Abilities_Death = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Death"),
		FString("Death Ability Tag")
	);

	GameplayTags.Abilities_Summon = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Summon"),
		FString("Summon Ability Tag")
	);

	GameplayTags.Abilities_Skill1 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Skill1"),
		FString("Skill1 Ability Tag")
	);

	GameplayTags.Abilities_Skill2 = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.Skill2"),
		FString("Skill2 Ability Tag")
	);

	/*
	 * ��ü�� �����Ƽ 
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

	GameplayTags.Abilities_VendingMachine_BasicAttack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.VendingMachine.BasicAttack"),
		FString("VendingMachine Basic Attack Ability Tag")
	);

	GameplayTags.Abilities_VendingMachine_AttackSpeedBuff = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.VendingMachine.AttackSpeedBuff"),
		FString("VendingMachine Attack Speed Buff Ability Tag")
	);


	/*
	 * Ÿ�� ����
	 */

	GameplayTags.Abilities_HitReact = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.HitReact"),
		FString("Hit React Ability")
	);

	/*
	 * �����Ƽ Ÿ�� �з�
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
	 * �����Ƽ ��ٿ�
	 */

	GameplayTags.Cooldown_Fire_FireBolt = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Cooldown.Fire.FireBolt"),
		FString("FireBolt Cooldown Tag")
	);

	GameplayTags.Cooldown_Armadillo_RollCharge = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Cooldown.Armadillo.RollCharge"),
		FString("Armadillo Roll Charge Cooldown Tag")
	);

	GameplayTags.Cooldown_Elite_Sweep = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Cooldown.Elite.Sweep"),
		FString("Elite Sweep Attack Cooldown Tag")
	);

	GameplayTags.Cooldown_Elite_Roar = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Cooldown.Elite.Roar"),
		FString("Elite Roar Cooldown Tag")
	);

	/*
	 * ���� ��ġ ����
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
	 * ���� ��Ÿ��
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
	 * �Է� ����
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

	/*
	 * GameplayCue - ����
	 */

	GameplayTags.GameplayCue_Player_Damage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.Player.Damage"),
		FString("Player Damaged Sound")
	);

	GameplayTags.GameplayCue_Player_Death = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.Player.Death"),
		FString("Player Death Sound")
	);

	GameplayTags.GameplayCue_Player_LowHealth = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.Player.LowHealth"),
		FString("Container Low Sound")
	);

	GameplayTags.GameplayCue_Player_WaterDepleted = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.Player.WaterDepleted"),
		FString("Water Zero Sound")
	);

	GameplayTags.GameplayCue_Enemy_Damage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.Enemy.Damage"),
		FString("Enemy Damaged Sound")
	);

	GameplayTags.GameplayCue_Cleanser_Damage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.Cleanser.Damage"),
		FString("Cleanser Damaged Sound")
	);

	GameplayTags.GameplayCue_Skill_WaterPump = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.Skill.WaterPump"),
		FString("WaterPump Loop Sound/Effect")
	);

	GameplayTags.GameplayCue_Skill_ClawSwipe = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.Skill.ClawSwipe"),
		FString("ClawSwipe Effect")
	);
}
