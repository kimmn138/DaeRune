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
	FGameplayTag State_Riding;
	FGameplayTag State_RobotVacuum_SustainedDash;

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

	// 탑승 공유 데미지 식별 (재전파 방지용 — DamageTypeTags에는 포함하지 않음)
	FGameplayTag Damage_MountShared;

	// �⺻ ��
	FGameplayTag Heal;

	// ���� ȿ��
	FGameplayTag Buff_Elite;
	FGameplayTag Buff_Elite_Roar;
	FGameplayTag Buff_VendingMachine_AttackSpeed;
	// ★GAS 태그★ (Plan5(공속칩) D-63). 자판기 칩 ② 가 본인·아군에게 거는 공유 공속 버프. 로봇 공용 이름.
	FGameplayTag Buff_AttackSpeed_Shared;
	FGameplayTag Buff_RobotVacuum_DashSpeed;
	
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

	// 업그레이드 칩 SetByCaller (GE_Upgrade_Stats: Flat = 합연산, Mult = 1 + Percent)
	FGameplayTag Data_Upgrade_MaxHealth_Flat;
	FGameplayTag Data_Upgrade_MaxHealth_Mult;
	FGameplayTag Data_Upgrade_MaxWater_Flat;
	FGameplayTag Data_Upgrade_MaxWater_Mult;
	FGameplayTag Data_Upgrade_MoveSpeed_Flat;
	FGameplayTag Data_Upgrade_MoveSpeed_Mult;

	// 어빌리티 쿨다운 Duration SetByCaller (업그레이드로 보정된 쿨다운 주입)
	FGameplayTag Data_Cooldown;

	// 업그레이드 칩 기능 스위치 (Plan3(공용칩) §4.6).
	// ★GAS 태그가 아니다★ — ASC 에 부여되지 않고 FDRUpgradeRuntime::GrantedTags 에만 모인다.
	// 체력/물은 WBP_HealthWaterBar 한 위젯 안에 같이 있어 하나의 태그로 묶는다.
	FGameplayTag Upgrade_Feature_HUD_Vitals;
	FGameplayTag Upgrade_Feature_HUD_SkillIcons;

	// 돌파 칩 기능 스위치 (Plan3(공용칩) §17.4.2). 역시 ASC 에 부여되지 않는다 —
	// ASC 에 Loose 로 얹으면 어빌리티의 ActivationBlockedTags 와 의도치 않게 상호작용한다.
	FGameplayTag Upgrade_Feature_Combat_Berserk;		// 주는 피해 증폭 + 피격 시 컨테이너 1칸 소모
	FGameplayTag Upgrade_Feature_Combat_NoHealing;		// 모든 체력 회복 차단 (단독 재사용 가능하게 분리)
	FGameplayTag Upgrade_Feature_Combat_Synergy;		// 근접 팀원 시너지 오라 구동
	FGameplayTag Upgrade_Feature_Combat_BloodCost;		// 스킬 코스트를 물 대신 체력으로

	// ★이건 진짜 GAS 태그다★ — GE_Upgrade_Synergy 가 ASC 에 부여하며,
	// 태그 ★카운트★ 가 곧 시너지 스택 수다 (Plan3(공용칩) §17.7.3).
	FGameplayTag Buff_Upgrade_Synergy;

	// 스킬 전용 칩 기능 스위치 (Plan5(씨앗칩)).
	// 역시 ASC 에 부여되지 않고 FDRUpgradeRuntime::GrantedTags 에만 모인다.
	FGameplayTag Upgrade_Feature_Skill_SeedBomb_Refund;	// 씨앗 폭탄 직격 시 물 환급
	FGameplayTag Upgrade_Feature_Skill_SeedBomb_Slow;	// 피격된 적 이동 속도 감소
	FGameplayTag Upgrade_Feature_Skill_SeedBomb_Haste;	// 피격된 아군 이동 속도 증가

	// 물대포 칩 피처 스위치 (Plan5(물대포칩)). 역시 ASC 에 부여되지 않는다.
	FGameplayTag Upgrade_Feature_Skill_WaterPump_Ramp;			// ① 지속 명중 시 피해 누적
	FGameplayTag Upgrade_Feature_Skill_WaterPump_Slow;			// ② 피격 적 감속
	FGameplayTag Upgrade_Feature_Skill_WaterPump_Vulnerable;	// ② 피격 적이 받는 피해 증가
	FGameplayTag Upgrade_Feature_Skill_WaterPump_Pierce;		// ③ 적 관통

	// ★여기부터는 진짜 GAS 태그다★ — 둘 다 C++ 가 런타임에 읽으므로 네이티브여야 한다
	// (씨앗칩의 Debuff.SeedBomb.Slow 등은 C++ 가 안 읽어서 에셋에만 있다).

	// GE_WaterPump_Ramp 가 부여. ★태그 카운트 = 가압 스택 수★ (Plan5(물대포칩) D-9)
	FGameplayTag Buff_WaterPump_Pressure;

	// GE_WaterPump_Vulnerable 이 부여. 이 태그를 가진 대상은 받는 모든 피해가 증가한다.
	// ★물대포 전용 이름을 쓰지 않은 이유★: 다른 스킬도 같은 표식을 재사용할 수 있게 하기 위함.
	FGameplayTag Debuff_Vulnerable;

	// 기본 공격(발톱) 칩 피처 스위치 (Plan5(발톱칩)). ASC 에 부여되지 않는다.
	FGameplayTag Upgrade_Feature_Skill_Claw_Lifesteal;	// ② 적에게 준 피해만큼 자기 회복
	FGameplayTag Upgrade_Feature_Skill_Claw_AllyHeal;	// ② 아군 적중 시 자기 체력으로 아군 회복

	// 자판기 기본 공격(동전) 칩 피처 스위치 (Plan5(동전칩)). ASC 에 부여되지 않는다.
	FGameplayTag Upgrade_Feature_Skill_Coin_HitHeal;	// ② 적에게 적중할 때마다 고정량 자기 회복

	// 자판기 잭팟 칩 피처 스위치 (Plan5(잭팟칩)). ASC 에 부여되지 않는다.
	FGameplayTag Upgrade_Feature_Skill_Jackpot_GoldRush;	// ① 확률표 교체 (골드 25 / 실버 0)
	FGameplayTag Upgrade_Feature_Skill_Jackpot_Double;		// ② 같은 색 캡슐 한 발 더, 골드 0
	FGameplayTag Upgrade_Feature_Skill_Jackpot_Pity;		// ③ 비골드마다 골드 확률 누적 보정

	// 자판기 공속 버프(스킬 2) 칩 피처 스위치 (Plan5(공속칩)). ASC 에 부여되지 않는다.
	FGameplayTag Upgrade_Feature_Skill_AttackSpeedBuff_Overdrive;	// ① 1회 +30%, 지속 5초
	FGameplayTag Upgrade_Feature_Skill_AttackSpeedBuff_Share;		// ② 본인 + 반경 안 아군에게 공유 버프
	FGameplayTag Upgrade_Feature_Skill_AttackSpeedBuff_Decay;		// ③ 1회 +16%, 만료 후 1초에 1스택씩 감소

	// 로봇 청소기 탑승(패시브) 칩 (Plan5(탑승칩)). 피처 스위치는 ASC 에 부여되지 않는다.
	// ★Abilities_RobotVacuum_Ride 는 칩 그룹 키 전용 — 어떤 GA 의 AbilityTags 에도 넣지 말 것 (Plan5 T-90)★
	FGameplayTag Abilities_RobotVacuum_Ride;
	FGameplayTag Upgrade_Feature_Skill_Ride_Power;		// ① 맨 아래에서 위 인원당 탑승자 공격력 +8%, 청소기 이속 −5%
	FGameplayTag Upgrade_Feature_Skill_Ride_Guard;		// ② 위 탑승자의 최종 피해를 80% 줄여 대신 받음
	FGameplayTag Upgrade_Feature_Skill_Ride_Eject;		// ③ 바로 위 탑승자 체력 30% 이하 → 비상 탈출
	FGameplayTag Debuff_RobotVacuum_RideLoad;			// ★GAS 태그★ GE_RideLoad 가 부여 — C++ 이 이 태그로 지운다
	FGameplayTag Debuff_Ride_Banned;					// ★GAS 태그★ GE_RideBan 이 탑승자에게 부여

	// 로봇 청소기 기본 공격(공기탄) 칩 (Plan5(공기탄칩))
	FGameplayTag Upgrade_Feature_Skill_AirShot_FriendlyFire;	// ① 아군에게도 같은 피해
	FGameplayTag Upgrade_Feature_Skill_AirShot_Drag;			// ② 맞은 적 감속 · 받는 피해 증가(스택)
	FGameplayTag Debuff_AirShot_Drag;							// ★GAS 태그★ GE_AirShot_Drag 가 적에게 부여 — C++ 이 스택을 읽는다

	// 로봇 청소기 물 점프 칩 (Plan5(물점프칩))
	FGameplayTag Upgrade_Feature_Skill_JetJump_Guard;	// ② 쓰는 순간 0.3초 무적
	FGameplayTag State_Invulnerable;					// ★GAS 태그★ 모든 피해 무효 — 기본 피해 관문이 읽는다(범용)

	// 로봇 청소기 돌진 칩 (Plan5(돌진칩))
	FGameplayTag Upgrade_Feature_Skill_Dash_PassAllies;	// ② 돌진 중 아군 통과

	// 범용 SetByCaller (Plan5(발톱칩)).
	// 체력을 ★비용으로★ 깎는 GE 가 액수를 밖에서 받게 한다.
	// ★값은 항상 양수(= 비용 액수)로 넣는다. 부호를 빼는 것은 GE 모디파이어의 몫이다.★
	FGameplayTag Data_HealthCost;

	// 범용 SetByCaller (Plan5(씨앗칩)).
	// 이속 증감 GE 가 배율과 지속시간을 밖에서 받게 해, "5초 / 30%" 가
	// GE 에셋이 아니라 DA_ProgressionConfig 한 곳에만 존재하도록 한다.
	FGameplayTag Data_MoveSpeed_Mult;
	FGameplayTag Data_Duration;

	// 범용 SetByCaller (Plan5(돌진칩)). 시전자 자신에게 주는 피해(자해) 양 — 값은 양수
	FGameplayTag Data_SelfDamage;

	// 게임플레이 이벤트 (GA 통지용)
	FGameplayTag Event_Dash_Brake;

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
	// ★Plan5(잭팟칩)★ ★칩 그룹 키 전용★ — 칩 레벨 TargetAbilityTag · SkillDisplayNames 에만 쓴다.
	// 어떤 GA 의 AbilityTags 에도 넣지 말 것: 업그레이드 키(첫 "Abilities.*" 태그)가 흔들린다 (Plan5 T-51)
	FGameplayTag Abilities_VendingMachine_Jackpot;
	FGameplayTag Abilities_RobotVacuum_AirShot;
	FGameplayTag Abilities_RobotVacuum_JetJump;
	FGameplayTag Abilities_RobotVacuum_Dash;

	// Ÿ�� ����
	FGameplayTag Abilities_HitReact;

	// �����Ƽ Ÿ�� �з�
	FGameplayTag Abilities_Type_Offensive;
	FGameplayTag Abilities_Type_Passive;
	FGameplayTag Abilities_Type_None;

	// �����Ƽ ��Ÿ��
	FGameplayTag Cooldown_Fire_FireBolt;
	FGameplayTag Cooldown_RobotVacuum_JetJump;
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
	FGameplayTag GameplayCue_Skill_VacuumAirShot;
	FGameplayTag GameplayCue_Skill_VacuumJetJump;
	FGameplayTag GameplayCue_Skill_VacuumDash;

	// ========== 스테이지2 두더지 보스 (Plan7 §5.5) ==========
	FGameplayTag Abilities_MoleBoss_Claw;
	FGameplayTag Abilities_MoleBoss_BurrowStrike;
	FGameplayTag Cooldown_MoleBoss_BurrowStrike;
	FGameplayTag State_MoleBoss_Burrowed;

	// ========== 벨보이 (Plan8) ==========
	FGameplayTag Abilities_BellBoy_BasicAttack;
	FGameplayTag Abilities_BellBoy_Taunt;
	FGameplayTag Abilities_BellBoy_HatThrow;

	// 몽타주 노티파이(AN_MontageEvent)가 보내는 이벤트
	FGameplayTag Event_Montage_BellBoy_BasicAttack;	// 가방이 닿는 순간
	FGameplayTag Event_Montage_BellBoy_Taunt;		// 도발 판정 시작
	FGameplayTag Event_Montage_BellBoy_HatThrow;	// 모자가 손을 떠나는 순간

	// ★GAS 태그★ — GE_BellBoy_Taunted 가 적에게 부여. C++ 이 읽는다 (Plan8 D-13)
	FGameplayTag Debuff_Taunted;
	// SetByCaller — 도발된 적이 "주는" 피해 배율(0.8). 도발 GE 스펙이 들고 다닌다 (Plan8 D-13)
	FGameplayTag Data_Taunt_DamageMult;

	// ★GAS 태그★ — 모자 보호막 (Plan8 D-20). 부모 태그는 "모자를 쓰고 있나" 조회 전용
	FGameplayTag Buff_BellBoy_Hat;
	FGameplayTag Buff_BellBoy_Hat_Single;
	FGameplayTag Buff_BellBoy_Hat_Double;

	FGameplayTag GameplayCue_BellBoy_HatBlock;
	FGameplayTag GameplayCue_BellBoy_Hat_Single;
	FGameplayTag GameplayCue_BellBoy_Hat_Double;
	FGameplayTag GameplayCue_BellBoy_Taunted;

private:
	static FDRGameplayTags GameplayTags;
};
