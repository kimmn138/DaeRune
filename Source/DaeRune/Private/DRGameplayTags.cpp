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

	GameplayTags.State_Riding = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Riding"),
		FString("Player is riding on a RobotVacuum")
	);

	GameplayTags.State_RobotVacuum_SustainedDash = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.RobotVacuum.SustainedDash"),
		FString("RobotVacuum is in sustained dash (auto forward)")
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

	// 탑승 공유 데미지 식별 태그 — 데미지 "타입"이 아니므로 DamageTypeTags/DamageTypesToDebuffs에 넣지 않는다
	GameplayTags.Damage_MountShared = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Damage.MountShared"),
		FString("Mount-shared damage marker (prevents re-propagation)")
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

	GameplayTags.Buff_Elite_Roar = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.Elite.Roar"),
		FString("Buff applied to allied enemies by Elite Monster Roar Aura")
	);

	GameplayTags.Buff_VendingMachine_AttackSpeed = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.VendingMachine.AttackSpeed"),
		FString("VendingMachine Attack Speed Buff")
	);

	GameplayTags.Buff_AttackSpeed_Shared = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.AttackSpeed.Shared"),
		FString("자판기 공속 버프 칩 ②가 본인·아군에게 거는 공유 공속 버프 (스택당 ChipTuning.AttackSpeedBuff.SharedSpeedPerStack)")
	);

	GameplayTags.Buff_RobotVacuum_DashSpeed = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.RobotVacuum.DashSpeed"),
		FString("RobotVacuum Dash Move Speed Buff (stackable)")
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

	GameplayTags.Effects_CannotAttack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Effects.CannotAttack"),
		FString("Tag that blocks attack abilities while active")
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
	 * 업그레이드 칩 SetByCaller (GE_Upgrade_Stats / 쿨다운 GE)
	 * Flat = 합연산 값, Mult = 곱연산 값(1 + Percent). Plan2.md 8.4 참조
	 */

	GameplayTags.Data_Upgrade_MaxHealth_Flat = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.Upgrade.MaxHealth.Flat"),
		FString("SetByCaller tag for upgraded MaxHealth additive amount")
	);

	GameplayTags.Data_Upgrade_MaxHealth_Mult = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.Upgrade.MaxHealth.Mult"),
		FString("SetByCaller tag for upgraded MaxHealth multiplier (1 + percent)")
	);

	GameplayTags.Data_Upgrade_MaxWater_Flat = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.Upgrade.MaxWater.Flat"),
		FString("SetByCaller tag for upgraded MaxWater additive amount")
	);

	GameplayTags.Data_Upgrade_MaxWater_Mult = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.Upgrade.MaxWater.Mult"),
		FString("SetByCaller tag for upgraded MaxWater multiplier (1 + percent)")
	);

	GameplayTags.Data_Upgrade_MoveSpeed_Flat = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.Upgrade.MoveSpeed.Flat"),
		FString("SetByCaller tag for upgraded MoveSpeed additive amount")
	);

	GameplayTags.Data_Upgrade_MoveSpeed_Mult = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.Upgrade.MoveSpeed.Mult"),
		FString("SetByCaller tag for upgraded MoveSpeed multiplier (1 + percent)")
	);

	GameplayTags.Data_Cooldown = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.Cooldown"),
		FString("SetByCaller tag for ability cooldown duration")
	);

	/*
	 * 업그레이드 칩 기능 스위치 (Plan3(공용칩) §4.6)
	 * 수치가 아닌 On/Off 효과를 칩으로 표현하기 위한 태그. ASC 에는 부여되지 않는다.
	 */

	GameplayTags.Upgrade_Feature_HUD_Vitals = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.HUD.Vitals"),
		FString("장착 시 체력/물 HUD 를 표시한다")
	);

	GameplayTags.Upgrade_Feature_HUD_SkillIcons = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.HUD.SkillIcons"),
		FString("장착 시 스킬 아이콘 HUD 를 표시한다")
	);

	GameplayTags.Upgrade_Feature_Combat_Berserk = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Combat.Berserk"),
		FString("주는 피해 증폭 + 피격 시 체력 컨테이너 1칸 즉시 소모")
	);

	GameplayTags.Upgrade_Feature_Combat_NoHealing = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Combat.NoHealing"),
		FString("모든 체력 회복 차단")
	);

	GameplayTags.Upgrade_Feature_Combat_Synergy = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Combat.Synergy"),
		FString("근접 팀원 시너지 오라 활성화")
	);

	GameplayTags.Upgrade_Feature_Combat_BloodCost = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Combat.BloodCost"),
		FString("스킬 코스트를 물 대신 체력으로 지불")
	);

	GameplayTags.Buff_Upgrade_Synergy = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.Upgrade.Synergy"),
		FString("시너지 오라 적용 중 (태그 카운트 = 스택 수)")
	);

	// ========== 스킬 전용 칩 기능 스위치 (Plan5(씨앗칩)) ==========

	GameplayTags.Upgrade_Feature_Skill_SeedBomb_Refund = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.SeedBomb.Refund"),
		FString("씨앗 폭탄 직격 시 사용한 물 환급")
	);

	GameplayTags.Upgrade_Feature_Skill_SeedBomb_Slow = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.SeedBomb.Slow"),
		FString("씨앗 폭탄에 피격된 적 이동 속도 감소")
	);

	GameplayTags.Upgrade_Feature_Skill_SeedBomb_Haste = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.SeedBomb.Haste"),
		FString("씨앗 폭탄에 닿은 아군 이동 속도 증가")
	);

	// ========== 물대포 칩 기능 스위치 (Plan5(물대포칩)) ==========

	GameplayTags.Upgrade_Feature_Skill_WaterPump_Ramp = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.WaterPump.Ramp"),
		FString("물대포 지속 명중 시 피해 누적")
	);

	GameplayTags.Upgrade_Feature_Skill_WaterPump_Slow = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.WaterPump.Slow"),
		FString("물대포에 피격된 적 이동 속도 감소")
	);

	GameplayTags.Upgrade_Feature_Skill_WaterPump_Vulnerable = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.WaterPump.Vulnerable"),
		FString("물대포에 피격된 적이 받는 피해 증가")
	);

	GameplayTags.Upgrade_Feature_Skill_WaterPump_Pierce = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.WaterPump.Pierce"),
		FString("물대포 물줄기가 적을 관통")
	);

	// ========== 물대포 칩이 쓰는 진짜 GAS 태그 (Plan5(물대포칩)) ==========

	GameplayTags.Buff_WaterPump_Pressure = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.WaterPump.Pressure"),
		FString("물대포 가압 스택 (태그 카운트 = 스택 수)")
	);

	GameplayTags.Debuff_Vulnerable = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Vulnerable"),
		FString("이 대상이 받는 모든 피해가 증가한다")
	);

	// ========== 기본 공격(발톱) 칩 기능 스위치 (Plan5(발톱칩)) ==========

	GameplayTags.Upgrade_Feature_Skill_Claw_Lifesteal = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Claw.Lifesteal"),
		FString("기본 공격이 적에게 준 피해만큼 시전자 체력 회복")
	);

	GameplayTags.Upgrade_Feature_Skill_Claw_AllyHeal = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Claw.AllyHeal"),
		FString("기본 공격이 아군에게 적중하면 시전자 체력으로 아군 회복")
	);

	GameplayTags.Data_HealthCost = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.HealthCost"),
		FString("SetByCaller tag for a self-inflicted health cost (not damage)")
	);

	// ========== 자판기 기본 공격(동전) 칩 기능 스위치 (Plan5(동전칩)) ==========

	GameplayTags.Upgrade_Feature_Skill_Coin_HitHeal = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Coin.HitHeal"),
		FString("자판기 기본 공격이 적에게 적중할 때마다 시전자 체력 고정량 회복")
	);

	// ========== 자판기 잭팟 칩 기능 스위치 (Plan5(잭팟칩)) ==========

	GameplayTags.Upgrade_Feature_Skill_Jackpot_GoldRush = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Jackpot.GoldRush"),
		FString("잭팟 확률표를 ChipTuning.Jackpot.GoldRushOdds 로 교체")
	);

	GameplayTags.Upgrade_Feature_Skill_Jackpot_Double = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Jackpot.Double"),
		FString("잭팟 캡슐이 같은 색으로 한 발 더 발사, 골드 확률은 브론즈·실버로 재분배")
	);

	GameplayTags.Upgrade_Feature_Skill_Jackpot_Pity = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Jackpot.Pity"),
		FString("브론즈·실버가 나올 때마다 다음 잭팟의 골드 확률 누적 증가, 골드가 나오면 초기화")
	);

	// ========== 자판기 공속 버프 칩 기능 스위치 (Plan5(공속칩)) ==========

	GameplayTags.Upgrade_Feature_Skill_AttackSpeedBuff_Overdrive = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.AttackSpeedBuff.Overdrive"),
		FString("공속 버프 스택당 증가량을 OverdriveSpeedPerStack 으로, 지속을 OverdriveDuration 으로")
	);

	GameplayTags.Upgrade_Feature_Skill_AttackSpeedBuff_Share = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.AttackSpeedBuff.Share"),
		FString("공속 버프를 본인 + SharedRadius 안 아군에게 SharedEffect 로 건다")
	);

	GameplayTags.Upgrade_Feature_Skill_AttackSpeedBuff_Decay = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.AttackSpeedBuff.Decay"),
		FString("공속 버프를 DecayEffect 로 걸어 만료 후 DecayInterval 마다 1스택씩 감소, 스택당 DecaySpeedPerStack")
	);

	// ========== 로봇 청소기 탑승 칩 (Plan5(탑승칩)) ==========

	GameplayTags.Abilities_RobotVacuum_Ride = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.RobotVacuum.Ride"),
		FString("청소기 탑승(패시브) 칩 그룹 키 — 칩 TargetAbilityTag · SkillDisplayNames 전용, GA 에 넣지 않는다")
	);

	GameplayTags.Upgrade_Feature_Skill_Ride_Power = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Ride.Power"),
		FString("맨 아래 청소기 위 인원당 탑승자 전원 주는 피해 증가, 그 청소기 이속 감소")
	);

	GameplayTags.Upgrade_Feature_Skill_Ride_Guard = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Ride.Guard"),
		FString("이 청소기 위 탑승자의 최종 피해를 GuardTransferRatio 만큼 줄여 이 청소기가 대신 받는다")
	);

	GameplayTags.Upgrade_Feature_Skill_Ride_Eject = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Ride.Eject"),
		FString("바로 위 탑승자 체력이 EjectHealthRatio 이하가 되면 비상 탈출 + 회복 + 이 청소기 탑승 금지")
	);

	GameplayTags.Debuff_RobotVacuum_RideLoad = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.RobotVacuum.RideLoad"),
		FString("탑승칩① 감속 GE(GE_RideLoad)가 부여 — 코드가 이 태그로 지운다")
	);

	GameplayTags.Debuff_Ride_Banned = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Ride.Banned"),
		FString("탑승칩③ 비상 탈출 뒤 그 청소기 탑승 금지(GE_RideBan, 시전자 ASC 로 청소기 구분)")
	);

	// ========== 로봇 청소기 기본 공격 칩 (Plan5(공기탄칩)) ==========

	GameplayTags.Upgrade_Feature_Skill_AirShot_FriendlyFire = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.AirShot.FriendlyFire"),
		FString("공기탄이 아군에게도 적과 같은 피해를 준다(탑승 연결은 통과)")
	);

	GameplayTags.Upgrade_Feature_Skill_AirShot_Drag = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.AirShot.Drag"),
		FString("공기탄에 맞은 적에게 흡착 스택(감속 · 받는 피해 증가, 보스 제외)")
	);

	GameplayTags.Debuff_AirShot_Drag = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.AirShot.Drag"),
		FString("공기탄칩② 흡착 표식(GE_AirShot_Drag, Aggregate by Target 스택) — 스택당 받는 피해 증가")
	);

	// ========== 로봇 청소기 물 점프 칩 (Plan5(물점프칩)) ==========

	GameplayTags.Upgrade_Feature_Skill_JetJump_Guard = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.JetJump.Guard"),
		FString("물 점프를 쓰는 순간부터 GuardDuration 동안 무적")
	);

	GameplayTags.State_Invulnerable = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.Invulnerable"),
		FString("무적 — 기본 피해 관문이 모든 피해를 통째로 무효로 한다(모자보다 먼저)")
	);

	// ========== 로봇 청소기 돌진 칩 (Plan5(돌진칩)) ==========

	GameplayTags.Upgrade_Feature_Skill_Dash_PassAllies = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Dash.PassAllies"),
		FString("돌진하는 동안 다른 플레이어와 충돌하지 않고 통과")
	);

	// ========== 벨보이 칩 (Plan5(가방칩 · 도발칩 · 모자칩)) ==========

	GameplayTags.Upgrade_Feature_Skill_Bag_HatDrop = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Bag.HatDrop"),
		FString("기본 공격이 적 1마리를 맞힐 때마다 확률로 벨보이 발밑에 모자가 떨어진다")
	);

	GameplayTags.Upgrade_Feature_Skill_Taunt_Expose = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Taunt.Expose"),
		FString("도발된 적이 받는 모든 피해 증가, 대신 도발된 적의 주는 피해 감소가 사라진다")
	);

	GameplayTags.Upgrade_Feature_Skill_Hat_AllyHit = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Hat.AllyHit"),
		FString("모자가 아군에게 맞으면 피해를 준 뒤 모자를 씌운다")
	);

	GameplayTags.Upgrade_Feature_Skill_Hat_ShareDouble = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Upgrade.Feature.Skill.Hat.ShareDouble"),
		FString("모자 2회 효과를 던진 본인뿐 아니라 모든 플레이어가 받는다")
	);

	// ========== 범용 SetByCaller (Plan5(씨앗칩)) ==========

	GameplayTags.Data_MoveSpeed_Mult = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.MoveSpeed.Mult"),
		FString("SetByCaller tag for a temporary MoveSpeed multiplier")
	);

	GameplayTags.Data_Duration = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.Duration"),
		FString("SetByCaller tag for a generic gameplay effect duration (seconds)")
	);

	GameplayTags.Data_SelfDamage = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.SelfDamage"),
		FString("SetByCaller tag for self damage amount (Plan5 돌진칩 — GE_VacuumDashSelfDamage)")
	);

	/*
	 * 게임플레이 이벤트 (GA 통지용)
	 */

	GameplayTags.Event_Dash_Brake = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Event.Dash.Brake"),
		FString("Sent to GA_VacuumDash when the rider presses backward input during sustained dash")
	);

	/*
	 * 어빌리티 분류
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

	GameplayTags.Abilities_VendingMachine_BasicAttack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.VendingMachine.BasicAttack"),
		FString("VendingMachine Basic Attack Ability Tag")
	);

	GameplayTags.Abilities_VendingMachine_AttackSpeedBuff = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.VendingMachine.AttackSpeedBuff"),
		FString("VendingMachine Attack Speed Buff Ability Tag")
	);

	// ★Plan5(잭팟칩)★ 칩 그룹 키 전용 — GA 에 붙이지 않는다
	GameplayTags.Abilities_VendingMachine_Jackpot = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.VendingMachine.Jackpot"),
		FString("Upgrade chip group key for VendingMachine jackpot chips (not granted to any ability)")
	);

	GameplayTags.Abilities_RobotVacuum_AirShot = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.RobotVacuum.AirShot"),
		FString("RobotVacuum Air Shot Ability Tag")
	);

	GameplayTags.Abilities_RobotVacuum_JetJump = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.RobotVacuum.JetJump"),
		FString("RobotVacuum Jet Jump Ability Tag")
	);

	GameplayTags.Abilities_RobotVacuum_Dash = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.RobotVacuum.Dash"),
		FString("RobotVacuum Dash Ability Tag")
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

	GameplayTags.Cooldown_RobotVacuum_JetJump = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Cooldown.RobotVacuum.JetJump"),
		FString("RobotVacuum JetJump Cooldown Tag - Q 홀드 연속 발동 방지 (Plan3 §15.6)")
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

	/*
	 * GameplayCue - 사운드
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

	GameplayTags.GameplayCue_Skill_VacuumAirShot = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.Skill.VacuumAirShot"),
		FString("RobotVacuum Air Shot Effect")
	);

	GameplayTags.GameplayCue_Skill_VacuumJetJump = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.Skill.VacuumJetJump"),
		FString("RobotVacuum Jet Jump Water Spray Effect")
	);

	GameplayTags.GameplayCue_Skill_VacuumDash = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.Skill.VacuumDash"),
		FString("RobotVacuum Dash Effect")
	);

	/*
	 * 스테이지2 두더지 보스 (Plan7 §5.5)
	 */

	GameplayTags.Abilities_MoleBoss_Claw = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.MoleBoss.Claw"),
		FString("MoleBoss 120-degree 2-hit claw sweep")
	);

	GameplayTags.Abilities_MoleBoss_BurrowStrike = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.MoleBoss.BurrowStrike"),
		FString("MoleBoss burrow -> warning -> erupt skill")
	);

	GameplayTags.Cooldown_MoleBoss_BurrowStrike = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Cooldown.MoleBoss.BurrowStrike"),
		FString("MoleBoss BurrowStrike cooldown (segment dependent)")
	);

	GameplayTags.State_MoleBoss_Burrowed = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("State.MoleBoss.Burrowed"),
		FString("MoleBoss is underground and invulnerable")
	);

	/*
	 * 벨보이 (Plan8)
	 */

	GameplayTags.Abilities_BellBoy_BasicAttack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.BellBoy.BasicAttack"),
		FString("BellBoy suitcase swing (LMB)")
	);

	GameplayTags.Abilities_BellBoy_Taunt = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.BellBoy.Taunt"),
		FString("BellBoy taunt - draws nearby enemies (E)")
	);

	GameplayTags.Abilities_BellBoy_HatThrow = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Abilities.BellBoy.HatThrow"),
		FString("BellBoy hat throw (RMB)")
	);

	GameplayTags.Event_Montage_BellBoy_BasicAttack = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Event.Montage.BellBoy.BasicAttack"),
		FString("BellBoy swing notify - suitcase hit moment")
	);

	GameplayTags.Event_Montage_BellBoy_Taunt = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Event.Montage.BellBoy.Taunt"),
		FString("BellBoy taunt notify - detection starts")
	);

	GameplayTags.Event_Montage_BellBoy_HatThrow = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Event.Montage.BellBoy.HatThrow"),
		FString("BellBoy throw notify - hat leaves the hand")
	);

	// ★Plan8(FP)★ FP 몽타주에서 "TP 이벤트와 같은 순간"을 표시 — 재생 속도 계산용 (D-35 / T-30)
	GameplayTags.Event_Montage_BellBoy_FPSync = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Event.Montage.BellBoy.FPSync"),
		FString("BellBoy FP montage sync point - frame that must line up with the TP event")
	);

	GameplayTags.Debuff_Taunted = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Debuff.Taunted"),
		FString("Enemy is taunted by a BellBoy")
	);

	GameplayTags.Data_Taunt_DamageMult = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.Taunt.DamageMult"),
		FString("SetByCaller - damage multiplier dealt by a taunted enemy")
	);

	// ★Plan5(도발칩)★ 도발칩① — 도발된 적이 받는 피해 배율 (D-129)
	GameplayTags.Data_Taunt_DamageTakenMult = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Data.Taunt.DamageTakenMult"),
		FString("SetByCaller - damage multiplier taken by a taunted enemy")
	);

	GameplayTags.Buff_BellBoy_Hat = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.BellBoy.Hat"),
		FString("Wearing a BellBoy hat (parent - query only)")
	);

	GameplayTags.Buff_BellBoy_Hat_Single = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.BellBoy.Hat.Single"),
		FString("BellBoy hat - blocks 1 hit")
	);

	GameplayTags.Buff_BellBoy_Hat_Double = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("Buff.BellBoy.Hat.Double"),
		FString("BellBoy hat - blocks 2 hits (picked up by its thrower)")
	);

	GameplayTags.GameplayCue_BellBoy_HatBlock = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.BellBoy.HatBlock"),
		FString("BellBoy hat blocked a hit")
	);

	GameplayTags.GameplayCue_BellBoy_Hat_Single = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.BellBoy.Hat.Single"),
		FString("BellBoy hat on head (1 hit)")
	);

	GameplayTags.GameplayCue_BellBoy_Hat_Double = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.BellBoy.Hat.Double"),
		FString("BellBoy hat on head (2 hits)")
	);

	GameplayTags.GameplayCue_BellBoy_Taunted = UGameplayTagsManager::Get().AddNativeGameplayTag(
		FName("GameplayCue.BellBoy.Taunted"),
		FString("Taunted enemy marker")
	);
}
