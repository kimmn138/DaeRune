// Copyright DaeRune


#include "DRGameplayTags.h"
#include "GameplayTagsManager.h"

FDRGameplayTags FDRGameplayTags::GameplayTags;

void FDRGameplayTags::InitializeNativeGameplayTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();
	auto Add = [&Manager](FGameplayTag& OutTag, const TCHAR* TagName, const TCHAR* DevComment)
	{
		OutTag = Manager.AddNativeGameplayTag(FName(TagName), FString(DevComment));
	};

	/*
	 * 리소스 비용
	 */

	Add(GameplayTags.Cost_Water, TEXT("Cost.Water"), TEXT("Water cost for abilities"));

	/*
	 * 기본 속성
	 */

	Add(GameplayTags.Attributes_Primary_MaxHealth, TEXT("Attributes.Primary.MaxHealth"), TEXT("Maximum amount of Health obtainable"));
	Add(GameplayTags.Attributes_Primary_MaxWater, TEXT("Attributes.Primary.MaxWater"), TEXT("Maximum amount of Water obtainable"));
	Add(GameplayTags.Attributes_Primary_MoveSpeed, TEXT("Attributes.Primary.MoveSpeed"), TEXT("Maximum amount of Speed obtainable"));

	/*
	 * 입력 태그
	 */

	Add(GameplayTags.InputTag_LMB, TEXT("InputTag.LMB"), TEXT("Input Tag for Left Mouse Button"));
	Add(GameplayTags.InputTag_RMB, TEXT("InputTag.RMB"), TEXT("Input Tag for Right Mouse Button"));
	Add(GameplayTags.InputTag_Q, TEXT("InputTag.Q"), TEXT("Input Tag for Q key"));
	Add(GameplayTags.InputTag_E, TEXT("InputTag.E"), TEXT("Input Tag for E key"));

	/*
	 * 플레이어 상태
	 */

	Add(GameplayTags.State_Corrupt, TEXT("State.Corrupt"), TEXT("Player is corrupted (water depleted)"));
	Add(GameplayTags.State_Carrying, TEXT("State.Carrying"), TEXT("Player is carrying a part"));
	Add(GameplayTags.State_InCombat, TEXT("State.InCombat"), TEXT("Player is in combat (recently took damage)"));
	Add(GameplayTags.State_VendingMachine_JackpotReady, TEXT("State.VendingMachine.JackpotReady"), TEXT("VendingMachine Jackpot stacks reached max"));
	Add(GameplayTags.State_Riding, TEXT("State.Riding"), TEXT("Player is riding on a RobotVacuum"));
	Add(GameplayTags.State_RobotVacuum_SustainedDash, TEXT("State.RobotVacuum.SustainedDash"), TEXT("RobotVacuum is in sustained dash (auto forward)"));

	/*
	 * 적 상태
	 */

	Add(GameplayTags.State_Enraged, TEXT("State.Enraged"), TEXT("Enemy is enraged (low health, increased aggression)"));
	Add(GameplayTags.State_Aggroed, TEXT("State.Aggroed"), TEXT("Enemy has aggro on a target"));
	Add(GameplayTags.State_HitReacting, TEXT("State.HitReacting"), TEXT("Enemy is in hit reaction state"));
	Add(GameplayTags.State_LockedDown, TEXT("State.LockedDown"), TEXT("Flying enemy is locked down after skill use (cannot move or attack)"));
	Add(GameplayTags.State_BallForm, TEXT("State.BallForm"), TEXT("Armadillo is in ball form"));
	Add(GameplayTags.State_Enemy_Phase1, TEXT("State.Enemy.Phase1"), TEXT("Enemy belongs to New Phase1; deals 30% reduced damage to players."));

	/*
	 * 기본 데미지, 타입 별 데미지
	 */

	Add(GameplayTags.Damage, TEXT("Damage"), TEXT("Damage"));
	Add(GameplayTags.Damage_Fire, TEXT("Damage.Fire"), TEXT("Fire Damage Type"));
	Add(GameplayTags.Damage_Lightning, TEXT("Damage.Lightning"), TEXT("Lightning Damage Type")); 

	Add(GameplayTags.Damage_Arcane, TEXT("Damage.Arcane"), TEXT("Arcane Damage Type"));
	Add(GameplayTags.Damage_Physical, TEXT("Damage.Physical"), TEXT("Physical Damage Type"));
	Add(GameplayTags.Damage_Bite, TEXT("Damage.Bite"), TEXT("Bite Damage Type"));

	// 탑승 공유 데미지 식별 태그 — 데미지 "타입"이 아니므로 DamageTypeTags/DamageTypesToDebuffs에 넣지 않는다
	Add(GameplayTags.Damage_MountShared, TEXT("Damage.MountShared"), TEXT("Mount-shared damage marker (prevents re-propagation)"));

	/*
	 * 기본 힐
	 */

	Add(GameplayTags.Heal, TEXT("Heal"), TEXT("Heal"));

	/*
	 * 디버프 효과
	 */

	Add(GameplayTags.Buff_Elite, TEXT("Buff.Elite"), TEXT("Buff for Elite Monster"));
	Add(GameplayTags.Buff_Elite_Roar, TEXT("Buff.Elite.Roar"), TEXT("Buff applied to allied enemies by Elite Monster Roar Aura"));
	Add(GameplayTags.Buff_VendingMachine_AttackSpeed, TEXT("Buff.VendingMachine.AttackSpeed"), TEXT("VendingMachine Attack Speed Buff"));
	Add(GameplayTags.Buff_AttackSpeed_Shared, TEXT("Buff.AttackSpeed.Shared"), TEXT("자판기 공속 버프 칩 ②가 본인·아군에게 거는 공유 공속 버프 (스택당 ChipTuning.AttackSpeedBuff.SharedSpeedPerStack)"));
	Add(GameplayTags.Buff_RobotVacuum_DashSpeed, TEXT("Buff.RobotVacuum.DashSpeed"), TEXT("RobotVacuum Dash Move Speed Buff (stackable)"));
	
	/*
	 * 디버프 효과
	 */

	Add(GameplayTags.Debuff_Arcane, TEXT("Debuff.Arcane"), TEXT("Debuff for Arcane damage"));
	Add(GameplayTags.Debuff_Burn, TEXT("Debuff.Burn"), TEXT("Debuff for Fire damage"));
	Add(GameplayTags.Debuff_Physical, TEXT("Debuff.Physical"), TEXT("Debuff for Physical damage"));
	Add(GameplayTags.Debuff_Stun, TEXT("Debuff.Stun"), TEXT("Debuff for Lightning damage"));
	Add(GameplayTags.Debuff_Bleed, TEXT("Debuff.Bleed"), TEXT("Debuff for Bleed damage"));
	Add(GameplayTags.Debuff_Elite, TEXT("Debuff.Elite"), TEXT("Debuff for Elite Monster"));

	/*
	 * 디버프 계산용 태그
	 */

	Add(GameplayTags.Debuff_Chance, TEXT("Debuff.Chance"), TEXT("Debuff Chance"));
	Add(GameplayTags.Debuff_Damage, TEXT("Debuff.Damage"), TEXT("Debuff Damage"));
	Add(GameplayTags.Debuff_Duration, TEXT("Debuff.Duration"), TEXT("Debuff Duration"));

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

	Add(GameplayTags.Effects_HitReact, TEXT("Effects.HitReact"), TEXT("Tag granted when Hit Reacting"));
	Add(GameplayTags.Effects_CannotAttack, TEXT("Effects.CannotAttack"), TEXT("Tag that blocks attack abilities while active"));

	/*
	 * 물 시스템 (SetByCaller 방식)
	 */

	Add(GameplayTags.Water_SetByCaller_Reduction, TEXT("Water.SetByCaller.Reduction"), TEXT("SetByCaller tag for water reduction amount"));
	Add(GameplayTags.Water_SetByCaller_Grant, TEXT("Water.SetByCaller.Grant"), TEXT("SetByCaller tag for water grant amount"));

	/*
	 * 업그레이드 칩 SetByCaller (GE_Upgrade_Stats / 쿨다운 GE)
	 * Flat = 합연산 값, Mult = 곱연산 값(1 + Percent). Plan2.md 8.4 참조
	 */

	Add(GameplayTags.Data_Upgrade_MaxHealth_Flat, TEXT("Data.Upgrade.MaxHealth.Flat"), TEXT("SetByCaller tag for upgraded MaxHealth additive amount"));
	Add(GameplayTags.Data_Upgrade_MaxHealth_Mult, TEXT("Data.Upgrade.MaxHealth.Mult"), TEXT("SetByCaller tag for upgraded MaxHealth multiplier (1 + percent)"));
	Add(GameplayTags.Data_Upgrade_MaxWater_Flat, TEXT("Data.Upgrade.MaxWater.Flat"), TEXT("SetByCaller tag for upgraded MaxWater additive amount"));
	Add(GameplayTags.Data_Upgrade_MaxWater_Mult, TEXT("Data.Upgrade.MaxWater.Mult"), TEXT("SetByCaller tag for upgraded MaxWater multiplier (1 + percent)"));
	Add(GameplayTags.Data_Upgrade_MoveSpeed_Flat, TEXT("Data.Upgrade.MoveSpeed.Flat"), TEXT("SetByCaller tag for upgraded MoveSpeed additive amount"));
	Add(GameplayTags.Data_Upgrade_MoveSpeed_Mult, TEXT("Data.Upgrade.MoveSpeed.Mult"), TEXT("SetByCaller tag for upgraded MoveSpeed multiplier (1 + percent)"));
	Add(GameplayTags.Data_Cooldown, TEXT("Data.Cooldown"), TEXT("SetByCaller tag for ability cooldown duration"));

	/*
	 * 업그레이드 칩 기능 스위치 (Plan3(공용칩) §4.6)
	 * 수치가 아닌 On/Off 효과를 칩으로 표현하기 위한 태그. ASC 에는 부여되지 않는다.
	 */

	Add(GameplayTags.Upgrade_Feature_HUD_Vitals, TEXT("Upgrade.Feature.HUD.Vitals"), TEXT("장착 시 체력/물 HUD 를 표시한다"));
	Add(GameplayTags.Upgrade_Feature_HUD_SkillIcons, TEXT("Upgrade.Feature.HUD.SkillIcons"), TEXT("장착 시 스킬 아이콘 HUD 를 표시한다"));
	Add(GameplayTags.Upgrade_Feature_Combat_Berserk, TEXT("Upgrade.Feature.Combat.Berserk"), TEXT("주는 피해 증폭 + 피격 시 체력 컨테이너 1칸 즉시 소모"));
	Add(GameplayTags.Upgrade_Feature_Combat_NoHealing, TEXT("Upgrade.Feature.Combat.NoHealing"), TEXT("모든 체력 회복 차단"));
	Add(GameplayTags.Upgrade_Feature_Combat_Synergy, TEXT("Upgrade.Feature.Combat.Synergy"), TEXT("근접 팀원 시너지 오라 활성화"));
	Add(GameplayTags.Upgrade_Feature_Combat_BloodCost, TEXT("Upgrade.Feature.Combat.BloodCost"), TEXT("스킬 코스트를 물 대신 체력으로 지불"));
	Add(GameplayTags.Buff_Upgrade_Synergy, TEXT("Buff.Upgrade.Synergy"), TEXT("시너지 오라 적용 중 (태그 카운트 = 스택 수)"));

	// ========== 스킬 전용 칩 기능 스위치 (Plan5(씨앗칩)) ==========

	Add(GameplayTags.Upgrade_Feature_Skill_SeedBomb_Refund, TEXT("Upgrade.Feature.Skill.SeedBomb.Refund"), TEXT("씨앗 폭탄 직격 시 사용한 물 환급"));
	Add(GameplayTags.Upgrade_Feature_Skill_SeedBomb_Slow, TEXT("Upgrade.Feature.Skill.SeedBomb.Slow"), TEXT("씨앗 폭탄에 피격된 적 이동 속도 감소"));
	Add(GameplayTags.Upgrade_Feature_Skill_SeedBomb_Haste, TEXT("Upgrade.Feature.Skill.SeedBomb.Haste"), TEXT("씨앗 폭탄에 닿은 아군 이동 속도 증가"));

	// ========== 물대포 칩 기능 스위치 (Plan5(물대포칩)) ==========

	Add(GameplayTags.Upgrade_Feature_Skill_WaterPump_Ramp, TEXT("Upgrade.Feature.Skill.WaterPump.Ramp"), TEXT("물대포 지속 명중 시 피해 누적"));
	Add(GameplayTags.Upgrade_Feature_Skill_WaterPump_Slow, TEXT("Upgrade.Feature.Skill.WaterPump.Slow"), TEXT("물대포에 피격된 적 이동 속도 감소"));
	Add(GameplayTags.Upgrade_Feature_Skill_WaterPump_Vulnerable, TEXT("Upgrade.Feature.Skill.WaterPump.Vulnerable"), TEXT("물대포에 피격된 적이 받는 피해 증가"));
	Add(GameplayTags.Upgrade_Feature_Skill_WaterPump_Pierce, TEXT("Upgrade.Feature.Skill.WaterPump.Pierce"), TEXT("물대포 물줄기가 적을 관통"));

	// ========== 물대포 칩이 쓰는 진짜 GAS 태그 (Plan5(물대포칩)) ==========

	Add(GameplayTags.Buff_WaterPump_Pressure, TEXT("Buff.WaterPump.Pressure"), TEXT("물대포 가압 스택 (태그 카운트 = 스택 수)"));
	Add(GameplayTags.Debuff_Vulnerable, TEXT("Debuff.Vulnerable"), TEXT("이 대상이 받는 모든 피해가 증가한다"));

	// ========== 기본 공격(발톱) 칩 기능 스위치 (Plan5(발톱칩)) ==========

	Add(GameplayTags.Upgrade_Feature_Skill_Claw_Lifesteal, TEXT("Upgrade.Feature.Skill.Claw.Lifesteal"), TEXT("기본 공격이 적에게 준 피해만큼 시전자 체력 회복"));
	Add(GameplayTags.Upgrade_Feature_Skill_Claw_AllyHeal, TEXT("Upgrade.Feature.Skill.Claw.AllyHeal"), TEXT("기본 공격이 아군에게 적중하면 시전자 체력으로 아군 회복"));
	Add(GameplayTags.Data_HealthCost, TEXT("Data.HealthCost"), TEXT("SetByCaller tag for a self-inflicted health cost (not damage)"));

	// ========== 자판기 기본 공격(동전) 칩 기능 스위치 (Plan5(동전칩)) ==========

	Add(GameplayTags.Upgrade_Feature_Skill_Coin_HitHeal, TEXT("Upgrade.Feature.Skill.Coin.HitHeal"), TEXT("자판기 기본 공격이 적에게 적중할 때마다 시전자 체력 고정량 회복"));

	// ========== 자판기 잭팟 칩 기능 스위치 (Plan5(잭팟칩)) ==========

	Add(GameplayTags.Upgrade_Feature_Skill_Jackpot_GoldRush, TEXT("Upgrade.Feature.Skill.Jackpot.GoldRush"), TEXT("잭팟 확률표를 ChipTuning.Jackpot.GoldRushOdds 로 교체"));
	Add(GameplayTags.Upgrade_Feature_Skill_Jackpot_Double, TEXT("Upgrade.Feature.Skill.Jackpot.Double"), TEXT("잭팟 캡슐이 같은 색으로 한 발 더 발사, 골드 확률은 브론즈·실버로 재분배"));
	Add(GameplayTags.Upgrade_Feature_Skill_Jackpot_Pity, TEXT("Upgrade.Feature.Skill.Jackpot.Pity"), TEXT("브론즈·실버가 나올 때마다 다음 잭팟의 골드 확률 누적 증가, 골드가 나오면 초기화"));

	// ========== 자판기 공속 버프 칩 기능 스위치 (Plan5(공속칩)) ==========

	Add(GameplayTags.Upgrade_Feature_Skill_AttackSpeedBuff_Overdrive, TEXT("Upgrade.Feature.Skill.AttackSpeedBuff.Overdrive"), TEXT("공속 버프 스택당 증가량을 OverdriveSpeedPerStack 으로, 지속을 OverdriveDuration 으로"));
	Add(GameplayTags.Upgrade_Feature_Skill_AttackSpeedBuff_Share, TEXT("Upgrade.Feature.Skill.AttackSpeedBuff.Share"), TEXT("공속 버프를 본인 + SharedRadius 안 아군에게 SharedEffect 로 건다"));
	Add(GameplayTags.Upgrade_Feature_Skill_AttackSpeedBuff_Decay, TEXT("Upgrade.Feature.Skill.AttackSpeedBuff.Decay"), TEXT("공속 버프를 DecayEffect 로 걸어 만료 후 DecayInterval 마다 1스택씩 감소, 스택당 DecaySpeedPerStack"));

	// ========== 로봇 청소기 탑승 칩 (Plan5(탑승칩)) ==========

	Add(GameplayTags.Abilities_RobotVacuum_Ride, TEXT("Abilities.RobotVacuum.Ride"), TEXT("청소기 탑승(패시브) 칩 그룹 키 — 칩 TargetAbilityTag · SkillDisplayNames 전용, GA 에 넣지 않는다"));
	Add(GameplayTags.Upgrade_Feature_Skill_Ride_Power, TEXT("Upgrade.Feature.Skill.Ride.Power"), TEXT("맨 아래 청소기 위 인원당 탑승자 전원 주는 피해 증가, 그 청소기 이속 감소"));
	Add(GameplayTags.Upgrade_Feature_Skill_Ride_Guard, TEXT("Upgrade.Feature.Skill.Ride.Guard"), TEXT("이 청소기 위 탑승자의 최종 피해를 GuardTransferRatio 만큼 줄여 이 청소기가 대신 받는다"));
	Add(GameplayTags.Upgrade_Feature_Skill_Ride_Eject, TEXT("Upgrade.Feature.Skill.Ride.Eject"), TEXT("바로 위 탑승자 체력이 EjectHealthRatio 이하가 되면 비상 탈출 + 회복 + 이 청소기 탑승 금지"));
	Add(GameplayTags.Debuff_RobotVacuum_RideLoad, TEXT("Debuff.RobotVacuum.RideLoad"), TEXT("탑승칩① 감속 GE(GE_RideLoad)가 부여 — 코드가 이 태그로 지운다"));
	Add(GameplayTags.Debuff_Ride_Banned, TEXT("Debuff.Ride.Banned"), TEXT("탑승칩③ 비상 탈출 뒤 그 청소기 탑승 금지(GE_RideBan, 시전자 ASC 로 청소기 구분)"));

	// ========== 로봇 청소기 기본 공격 칩 (Plan5(공기탄칩)) ==========

	Add(GameplayTags.Upgrade_Feature_Skill_AirShot_FriendlyFire, TEXT("Upgrade.Feature.Skill.AirShot.FriendlyFire"), TEXT("공기탄이 아군에게도 적과 같은 피해를 준다(탑승 연결은 통과)"));
	Add(GameplayTags.Upgrade_Feature_Skill_AirShot_Drag, TEXT("Upgrade.Feature.Skill.AirShot.Drag"), TEXT("공기탄에 맞은 적에게 흡착 스택(감속 · 받는 피해 증가, 보스 제외)"));
	Add(GameplayTags.Debuff_AirShot_Drag, TEXT("Debuff.AirShot.Drag"), TEXT("공기탄칩② 흡착 표식(GE_AirShot_Drag, Aggregate by Target 스택) — 스택당 받는 피해 증가"));

	// ========== 로봇 청소기 물 점프 칩 (Plan5(물점프칩)) ==========

	Add(GameplayTags.Upgrade_Feature_Skill_JetJump_Guard, TEXT("Upgrade.Feature.Skill.JetJump.Guard"), TEXT("물 점프를 쓰는 순간부터 GuardDuration 동안 무적"));
	Add(GameplayTags.State_Invulnerable, TEXT("State.Invulnerable"), TEXT("무적 — 기본 피해 관문이 모든 피해를 통째로 무효로 한다(모자보다 먼저)"));

	// ========== 로봇 청소기 돌진 칩 (Plan5(돌진칩)) ==========

	Add(GameplayTags.Upgrade_Feature_Skill_Dash_PassAllies, TEXT("Upgrade.Feature.Skill.Dash.PassAllies"), TEXT("돌진하는 동안 다른 플레이어와 충돌하지 않고 통과"));

	// ========== 벨보이 칩 (Plan5(가방칩 · 도발칩 · 모자칩)) ==========

	Add(GameplayTags.Upgrade_Feature_Skill_Bag_HatDrop, TEXT("Upgrade.Feature.Skill.Bag.HatDrop"), TEXT("기본 공격이 적 1마리를 맞힐 때마다 확률로 벨보이 발밑에 모자가 떨어진다"));
	Add(GameplayTags.Upgrade_Feature_Skill_Taunt_Expose, TEXT("Upgrade.Feature.Skill.Taunt.Expose"), TEXT("도발된 적이 받는 모든 피해 증가, 대신 도발된 적의 주는 피해 감소가 사라진다"));
	Add(GameplayTags.Upgrade_Feature_Skill_Hat_AllyHit, TEXT("Upgrade.Feature.Skill.Hat.AllyHit"), TEXT("모자가 아군에게 맞으면 피해를 준 뒤 모자를 씌운다"));
	Add(GameplayTags.Upgrade_Feature_Skill_Hat_ShareDouble, TEXT("Upgrade.Feature.Skill.Hat.ShareDouble"), TEXT("모자 2회 효과를 던진 본인뿐 아니라 모든 플레이어가 받는다"));

	// ========== 범용 SetByCaller (Plan5(씨앗칩)) ==========

	Add(GameplayTags.Data_MoveSpeed_Mult, TEXT("Data.MoveSpeed.Mult"), TEXT("SetByCaller tag for a temporary MoveSpeed multiplier"));
	Add(GameplayTags.Data_Duration, TEXT("Data.Duration"), TEXT("SetByCaller tag for a generic gameplay effect duration (seconds)"));
	Add(GameplayTags.Data_SelfDamage, TEXT("Data.SelfDamage"), TEXT("SetByCaller tag for self damage amount (Plan5 돌진칩 — GE_VacuumDashSelfDamage)"));

	/*
	 * 게임플레이 이벤트 (GA 통지용)
	 */

	Add(GameplayTags.Event_Dash_Brake, TEXT("Event.Dash.Brake"), TEXT("Sent to GA_VacuumDash when the rider presses backward input during sustained dash"));

	/*
	 * 어빌리티 분류
	 */

	Add(GameplayTags.Abilities_Attack, TEXT("Abilities.Attack"), TEXT("Attack Ability Tag"));
	Add(GameplayTags.Abilities_Death, TEXT("Abilities.Death"), TEXT("Death Ability Tag"));
	Add(GameplayTags.Abilities_Summon, TEXT("Abilities.Summon"), TEXT("Summon Ability Tag"));
	Add(GameplayTags.Abilities_Skill1, TEXT("Abilities.Skill1"), TEXT("Skill1 Ability Tag"));
	Add(GameplayTags.Abilities_Skill2, TEXT("Abilities.Skill2"), TEXT("Skill2 Ability Tag"));

	/*
	 * 구체적 어빌리티 
	 */

	Add(GameplayTags.Abilities_Fire_FireBolt, TEXT("Abilities.Fire.FireBolt"), TEXT("FireBolt Ability Tag"));
	Add(GameplayTags.Abilities_Lightning_Electrocute, TEXT("Abilities.Lightning.Electrocute"), TEXT("Electrocute Ability Tag"));
	Add(GameplayTags.Abilities_GardenRobot_ClawSwipe, TEXT("Abilities.GardenRobot.ClawSwipe"), TEXT("ClawSwipe Ability Tag"));
	Add(GameplayTags.Abilities_GardenRobot_WaterPump, TEXT("Abilities.GardenRobot.WaterPump"), TEXT("WaterPump Ability Tag"));
	Add(GameplayTags.Abilities_GardenRobot_SeedCannon, TEXT("Abilities.GardenRobot.SeedCannon"), TEXT("SeedCannon Ability Tag"));
	Add(GameplayTags.Abilities_VendingMachine_BasicAttack, TEXT("Abilities.VendingMachine.BasicAttack"), TEXT("VendingMachine Basic Attack Ability Tag"));
	Add(GameplayTags.Abilities_VendingMachine_AttackSpeedBuff, TEXT("Abilities.VendingMachine.AttackSpeedBuff"), TEXT("VendingMachine Attack Speed Buff Ability Tag"));

	// ★Plan5(잭팟칩)★ 칩 그룹 키 전용 — GA 에 붙이지 않는다
	Add(GameplayTags.Abilities_VendingMachine_Jackpot, TEXT("Abilities.VendingMachine.Jackpot"), TEXT("Upgrade chip group key for VendingMachine jackpot chips (not granted to any ability)"));
	Add(GameplayTags.Abilities_RobotVacuum_AirShot, TEXT("Abilities.RobotVacuum.AirShot"), TEXT("RobotVacuum Air Shot Ability Tag"));
	Add(GameplayTags.Abilities_RobotVacuum_JetJump, TEXT("Abilities.RobotVacuum.JetJump"), TEXT("RobotVacuum Jet Jump Ability Tag"));
	Add(GameplayTags.Abilities_RobotVacuum_Dash, TEXT("Abilities.RobotVacuum.Dash"), TEXT("RobotVacuum Dash Ability Tag"));


	/*
	 * 타격 반응
	 */

	Add(GameplayTags.Abilities_HitReact, TEXT("Abilities.HitReact"), TEXT("Hit React Ability"));

	/*
	 * 어빌리티 타입 분류
	 */

	Add(GameplayTags.Abilities_Type_None, TEXT("Abilities.Type.None"), TEXT("Type None"));
	Add(GameplayTags.Abilities_Type_Offensive, TEXT("Abilities.Type.Offensive"), TEXT("Type Offensive"));
	Add(GameplayTags.Abilities_Type_Passive, TEXT("Abilities.Type.Passive"), TEXT("Type Passive"));

	/*
	 * 어빌리티 쿨다운
	 */

	Add(GameplayTags.Cooldown_Fire_FireBolt, TEXT("Cooldown.Fire.FireBolt"), TEXT("FireBolt Cooldown Tag"));
	Add(GameplayTags.Cooldown_RobotVacuum_JetJump, TEXT("Cooldown.RobotVacuum.JetJump"), TEXT("RobotVacuum JetJump Cooldown Tag - Q 홀드 연속 발동 방지 (Plan3 §15.6)"));
	Add(GameplayTags.Cooldown_Armadillo_RollCharge, TEXT("Cooldown.Armadillo.RollCharge"), TEXT("Armadillo Roll Charge Cooldown Tag"));
	Add(GameplayTags.Cooldown_Elite_Sweep, TEXT("Cooldown.Elite.Sweep"), TEXT("Elite Sweep Attack Cooldown Tag"));
	Add(GameplayTags.Cooldown_Elite_Roar, TEXT("Cooldown.Elite.Roar"), TEXT("Elite Roar Cooldown Tag"));

	/*
	 * 공격 위치 소켓
	 */

	Add(GameplayTags.CombatSocket_Weapon, TEXT("CombatSocket.Weapon"), TEXT("Weapon"));
	Add(GameplayTags.CombatSocket_RightHand, TEXT("CombatSocket.RightHand"), TEXT("Right Hand"));
	Add(GameplayTags.CombatSocket_LeftHand, TEXT("CombatSocket.LeftHand"), TEXT("Left Hand"));
	Add(GameplayTags.CombatSocket_Tail, TEXT("CombatSocket.Tail"), TEXT("Tail"));

	/*
	 * 공격 몽타주
	 */

	Add(GameplayTags.Montage_Attack_1, TEXT("Montage.Attack.1"), TEXT("Attack 1"));
	Add(GameplayTags.Montage_Attack_2, TEXT("Montage.Attack.2"), TEXT("Attack 2"));
	Add(GameplayTags.Montage_Attack_3, TEXT("Montage.Attack.3"), TEXT("Attack 3"));
	Add(GameplayTags.Montage_Attack_4, TEXT("Montage.Attack.4"), TEXT("Attack 4"));

	/*
	 * 입력 블록
	 */

	Add(GameplayTags.Player_Block_InputHeld, TEXT("Player.Block.InputHeld"), TEXT("Block Input Held callback for input"));
	Add(GameplayTags.Player_Block_InputPressed, TEXT("Player.Block.InputPressed"), TEXT("Block Input Pressed callback for input"));
	Add(GameplayTags.Player_Block_InputReleased, TEXT("Player.Block.InputReleased"), TEXT("Block Input Released callback for input"));

	/*
	 * GameplayCue - 사운드
	 */

	Add(GameplayTags.GameplayCue_Player_Damage, TEXT("GameplayCue.Player.Damage"), TEXT("Player Damaged Sound"));
	Add(GameplayTags.GameplayCue_Player_Death, TEXT("GameplayCue.Player.Death"), TEXT("Player Death Sound"));
	Add(GameplayTags.GameplayCue_Player_LowHealth, TEXT("GameplayCue.Player.LowHealth"), TEXT("Container Low Sound"));
	Add(GameplayTags.GameplayCue_Player_WaterDepleted, TEXT("GameplayCue.Player.WaterDepleted"), TEXT("Water Zero Sound"));
	Add(GameplayTags.GameplayCue_Enemy_Damage, TEXT("GameplayCue.Enemy.Damage"), TEXT("Enemy Damaged Sound"));
	Add(GameplayTags.GameplayCue_Cleanser_Damage, TEXT("GameplayCue.Cleanser.Damage"), TEXT("Cleanser Damaged Sound"));
	Add(GameplayTags.GameplayCue_Skill_WaterPump, TEXT("GameplayCue.Skill.WaterPump"), TEXT("WaterPump Loop Sound/Effect"));
	Add(GameplayTags.GameplayCue_Skill_ClawSwipe, TEXT("GameplayCue.Skill.ClawSwipe"), TEXT("ClawSwipe Effect"));
	Add(GameplayTags.GameplayCue_Skill_VacuumAirShot, TEXT("GameplayCue.Skill.VacuumAirShot"), TEXT("RobotVacuum Air Shot Effect"));
	Add(GameplayTags.GameplayCue_Skill_VacuumJetJump, TEXT("GameplayCue.Skill.VacuumJetJump"), TEXT("RobotVacuum Jet Jump Water Spray Effect"));
	Add(GameplayTags.GameplayCue_Skill_VacuumDash, TEXT("GameplayCue.Skill.VacuumDash"), TEXT("RobotVacuum Dash Effect"));

	/*
	 * 스테이지2 두더지 보스 (Plan7 §5.5)
	 */

	Add(GameplayTags.Abilities_MoleBoss_Claw, TEXT("Abilities.MoleBoss.Claw"), TEXT("MoleBoss 120-degree 2-hit claw sweep"));
	Add(GameplayTags.Abilities_MoleBoss_BurrowStrike, TEXT("Abilities.MoleBoss.BurrowStrike"), TEXT("MoleBoss burrow -> warning -> erupt skill"));
	Add(GameplayTags.Cooldown_MoleBoss_BurrowStrike, TEXT("Cooldown.MoleBoss.BurrowStrike"), TEXT("MoleBoss BurrowStrike cooldown (segment dependent)"));
	Add(GameplayTags.State_MoleBoss_Burrowed, TEXT("State.MoleBoss.Burrowed"), TEXT("MoleBoss is underground and invulnerable"));

	/*
	 * 벨보이 (Plan8)
	 */

	Add(GameplayTags.Abilities_BellBoy_BasicAttack, TEXT("Abilities.BellBoy.BasicAttack"), TEXT("BellBoy suitcase swing (LMB)"));
	Add(GameplayTags.Abilities_BellBoy_Taunt, TEXT("Abilities.BellBoy.Taunt"), TEXT("BellBoy taunt - draws nearby enemies (E)"));
	Add(GameplayTags.Abilities_BellBoy_HatThrow, TEXT("Abilities.BellBoy.HatThrow"), TEXT("BellBoy hat throw (RMB)"));
	Add(GameplayTags.Event_Montage_BellBoy_BasicAttack, TEXT("Event.Montage.BellBoy.BasicAttack"), TEXT("BellBoy swing notify - suitcase hit moment"));
	Add(GameplayTags.Event_Montage_BellBoy_Taunt, TEXT("Event.Montage.BellBoy.Taunt"), TEXT("BellBoy taunt notify - detection starts"));
	Add(GameplayTags.Event_Montage_BellBoy_HatThrow, TEXT("Event.Montage.BellBoy.HatThrow"), TEXT("BellBoy throw notify - hat leaves the hand"));

	// ★Plan8(FP)★ FP 몽타주에서 "TP 이벤트와 같은 순간"을 표시 — 재생 속도 계산용 (D-35 / T-30)
	Add(GameplayTags.Event_Montage_BellBoy_FPSync, TEXT("Event.Montage.BellBoy.FPSync"), TEXT("BellBoy FP montage sync point - frame that must line up with the TP event"));
	Add(GameplayTags.Debuff_Taunted, TEXT("Debuff.Taunted"), TEXT("Enemy is taunted by a BellBoy"));
	Add(GameplayTags.Data_Taunt_DamageMult, TEXT("Data.Taunt.DamageMult"), TEXT("SetByCaller - damage multiplier dealt by a taunted enemy"));

	// ★Plan5(도발칩)★ 도발칩① — 도발된 적이 받는 피해 배율 (D-129)
	Add(GameplayTags.Data_Taunt_DamageTakenMult, TEXT("Data.Taunt.DamageTakenMult"), TEXT("SetByCaller - damage multiplier taken by a taunted enemy"));
	Add(GameplayTags.Buff_BellBoy_Hat, TEXT("Buff.BellBoy.Hat"), TEXT("Wearing a BellBoy hat (parent - query only)"));
	Add(GameplayTags.Buff_BellBoy_Hat_Single, TEXT("Buff.BellBoy.Hat.Single"), TEXT("BellBoy hat - blocks 1 hit"));
	Add(GameplayTags.Buff_BellBoy_Hat_Double, TEXT("Buff.BellBoy.Hat.Double"), TEXT("BellBoy hat - blocks 2 hits (picked up by its thrower)"));
	Add(GameplayTags.GameplayCue_BellBoy_HatBlock, TEXT("GameplayCue.BellBoy.HatBlock"), TEXT("BellBoy hat blocked a hit"));
	Add(GameplayTags.GameplayCue_BellBoy_Hat_Single, TEXT("GameplayCue.BellBoy.Hat.Single"), TEXT("BellBoy hat on head (1 hit)"));
	Add(GameplayTags.GameplayCue_BellBoy_Hat_Double, TEXT("GameplayCue.BellBoy.Hat.Double"), TEXT("BellBoy hat on head (2 hits)"));
	Add(GameplayTags.GameplayCue_BellBoy_Taunted, TEXT("GameplayCue.BellBoy.Taunted"), TEXT("Taunted enemy marker"));
}
