// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/CharacterClassInfo.h"
// FDRUpgradeRuntime / FGameplayTag — 업그레이드 칩 조회 헬퍼가 참조형으로 돌려준다 (§17.5)
#include "Game/DRUpgradeTypes.h"
// FActiveGameplayEffectHandle — ApplyTimedSpeedEffect 반환형 (Plan5(씨앗칩))
#include "ActiveGameplayEffectHandle.h"
#include "DRAbilitySystemLibrary.generated.h"

class UAbilityInfo;
class UAbilitySystemComponent;
class UGameBalanceConfig;
class UOverlayWidgetController;
class UUserWidget;
struct FWidgetControllerParams;
// FEffectProperties — 흡혈 판정이 피해 정보를 통째로 받는다 (Plan5(발톱칩))
struct FEffectProperties;

/**
 * 
 */
UCLASS()
class DAERUNE_API UDRAbilitySystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|WidgetController", meta = (DefaultToSelf = "WorldContextObject"))
	static bool MakeWidgetControllerParams(const UObject* WorldContextObject, FWidgetControllerParams& OutWCParams, ADRHUD*& OutDRHUD);

	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|WidgetController", meta = (DefaultToSelf = "WorldContextObject"))
	static UOverlayWidgetController* GetOverlayWidgetController(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static void InitializeDefaultAttributes(const UObject* WorldContextObject, ECharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static void GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, ECharacterClass CharacterClass);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static UCharacterClassInfo* GetCharacterClassInfo(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static UPlayerCharacterClassInfo* GetPlayerCharacterClassInfo(const UObject* WorldContextObject);

	// EPlayerCharacterClass에 매핑된 캐릭터 설명창 위젯 클래스 반환 (Tab Hold 패널용)
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|CharacterClassDefaults", meta = (DefaultToSelf = "WorldContextObject"))
	static TSubclassOf<UUserWidget> GetCharacterInfoWidgetClass(const UObject* WorldContextObject, EPlayerCharacterClass PlayerClass);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static void InitializePlayerDefaultAttributes(const UObject* WorldContextObject, EPlayerCharacterClass PlayerClass, float Level, UAbilitySystemComponent* ASC);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static void GivePlayerStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, EPlayerCharacterClass PlayerClass);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static UAbilityInfo* GetAbilityInfo(const UObject* WorldContextObject);

	// 게임 밸런스 설정 DataAsset 반환
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameBalance")
	static UGameBalanceConfig* GetGameBalanceConfig(const UObject* WorldContextObject);

	// ========== 업그레이드 칩 조회 (Plan3(공용칩) §17.5) ==========
	//
	// ExecCalc / AttributeSet / 컴포넌트는 전부 static 컨텍스트라
	// UDRGameplayAbility::GetUpgradeRuntimeFor(ActorInfo) 를 쓸 수 없다.
	// 같은 조회가 네 군데로 복제되지 않도록 여기로 올린다.

	/**
	 * ASC 소유자의 업그레이드 수치 캐시.
	 *
	 * 플레이어는 ASC 를 PlayerState 가 소유하므로 OwnerActor 가 곧 PlayerState 다.
	 * 적/클렌저 사이트처럼 칩이 없는 대상은 ★빈 캐시★ 를 돌려주므로
	 * 호출부에 null 분기가 필요 없다 (빈 캐시는 항등 함수).
	 */
	static const FDRUpgradeRuntime& GetUpgradeRuntimeForASC(const UAbilitySystemComponent* ASC);

	/**
	 * 칩 기능 스위치(Upgrade.Feature.*) 보유 여부.
	 * ★ASC 의 게임플레이 태그가 아니다★ — 칩 태그는 ASC 에 부여되지 않는다 (Plan3(공용칩) §17.3.1).
	 */
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|Upgrade")
	static bool HasUpgradeFeature(const UAbilitySystemComponent* ASC, FGameplayTag FeatureTag);

	// 컨테이너 정보를 가진 플레이어 어트리뷰트셋. 플레이어가 아니면 nullptr.
	static const class UDRPlayerAttributeSet* GetPlayerAttributeSetFromASC(const UAbilitySystemComponent* ASC);

	/**
	 * 돌파 칩 동적 효과의 튜닝 상수.
	 * ProgressionConfig 미지정이어도 기본값 구조체를 돌려주므로 항상 유효하다.
	 */
	static const struct FDRChipTuning& GetChipTuning(const UObject* WorldContextObject);

	// ========== 액터가 스킬 단위 업그레이드를 읽는 길 (Plan5(씨앗칩)) ==========
	//
	// 폭발 수치를 발사체 액터가 들고 있는 스킬(씨앗 폭탄 등)은 어빌리티 밖에서
	// 업그레이드를 조회해야 한다. 그 단일 창구다.

	/**
	 * ★Plan5(씨앗칩)★ 어빌리티 태그 컨테이너에서 업그레이드 키 태그를 뽑는다.
	 * 규칙은 UDRGameplayAbility::GetUpgradeKeyTag() / UDRAbilitySystemComponent::GetAbilityTagFromSpec
	 * 과 동일하다 — "Abilities" 접두 태그 중 첫 번째.
	 *
	 * 발사체·AoE 액터는 FDamageEffectParams::SourceAbilityTags 로 이 컨테이너를 이미 들고 있다
	 * (MakeDamageEffectParamsFromClassDefaults 가 GetAssetTags() 를 넣어 준다).
	 */
	static FGameplayTag GetUpgradeKeyTagFromAbilityTags(const FGameplayTagContainer& AbilityTags);

	/**
	 * ★Plan5(씨앗칩)★ 소스 ASC 의 장착 칩을 반영한 스킬 단위 수치.
	 *
	 * 어빌리티 밖(발사체·AoE 액터·컴포넌트)에서 업그레이드를 읽는 ★유일한 경로★ 로 쓴다.
	 * 소스가 플레이어가 아니거나 칩이 없으면 빈 캐시가 항등으로 동작해 Base 가 그대로 나온다 —
	 * 호출부에 분기가 필요 없다.
	 */
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|Upgrade")
	static float ApplySkillUpgrade(const UAbilitySystemComponent* SourceASC,
		const FGameplayTagContainer& SourceAbilityTags, EDRUpgradeStat Stat, float Base);

	/**
	 * ★Plan5(씨앗칩)★ 대상에게 일정 시간 이동 속도 배율 GE 를 건다. ★서버에서만 호출할 것★
	 *
	 * @param SpeedMultiplier 최종 배율. 0.7 = 30% 감속, 1.3 = 30% 가속.
	 * @return 적용된 핸들. GE 미지정 / ASC 없음이면 무효 핸들.
	 *
	 * GE 의 배율과 지속시간을 모두 SetByCaller 로 채우므로, 수치는 GE 에셋이 아니라
	 * DA_ProgressionConfig 한 곳에만 존재한다. (Plan5(씨앗칩) D-6)
	 */
	static FActiveGameplayEffectHandle ApplyTimedSpeedEffect(
		AActor* TargetActor, UAbilitySystemComponent* SourceASC,
		const struct FDRTimedSpeedEffect& Config, float SpeedMultiplier);

	/**
	 * ★Plan5(물대포칩)★ 대상에게 "지속시간만 있는" GE 를 건다. ★서버에서만 호출할 것★
	 *
	 * 모디파이어 없이 ★태그만 부여하는★ GE(취약 표식 등)를 위한 것이다.
	 * 이속 배율이 필요하면 위의 ApplyTimedSpeedEffect 를 쓴다.
	 *
	 * GE 의 Duration 은 SetByCaller(Data.Duration)여야 하며, 이 함수가 항상 값을 채운다 —
	 * 안 채우면 지속시간 0 으로 즉시 사라진다.
	 *
	 * @return 적용된 핸들. GE 미지정 / ASC 없음이면 무효 핸들.
	 */
	static FActiveGameplayEffectHandle ApplyTimedTagEffect(
		AActor* TargetActor, UAbilitySystemComponent* SourceASC,
		const TSoftClassPtr<UGameplayEffect>& Effect, float Duration);

	// ========== 회복 / 흡혈 (Plan5(발톱칩)) ==========

	/**
	 * ★Plan5(발톱칩)★ 대상에게 즉시 회복을 적용한다. ★서버에서만 호출할 것★
	 *
	 * GE 는 Instant + UExecCalc_Heal + SetByCaller(Heal) 사양이어야 한다(GE_Heal 재사용).
	 * 회복량 0 이하 / GE 미지정 / ASC 없음이면 조용히 아무 일도 하지 않는다.
	 *
	 * ⚠️ ★이 함수는 대상을 가리지 않는다.★ 팀 판정(아군인가)·유효성 판정(클렌저 사이트인가)을
	 *    ★하지 않는다★ — 주는 대로 힐을 건다. ★거르는 것은 호출부의 책임이다.★
	 *    현재 호출부는 둘 다 앞단에서 걸러 준다:
	 *      · ADRSeedProjectile — 폭발 루프가 클렌저 사이트를 continue 로 건너뛰고,
	 *                            bIsAlly(= !IsNotFriend) 가 true 인 대상에게만 부른다
	 *      · UDRMeleeAttack    — IsNotFriend 로 아군을 양성 판정한 뒤에만 부른다
	 *    새 호출부를 만들 때도 ★반드시★ 같은 판정을 앞에 둘 것.
	 *
	 * @param SourceASC    스펙을 만들 ASC. nullptr 이면 대상 자신이 소스가 된다(자가 회복 형태).
	 * @param SourceObject 이펙트 컨텍스트의 소스 오브젝트. nullptr 이면 소스 ASC 의 아바타를 쓴다.
	 */
	static void ApplyHealToActor(AActor* TargetActor, UAbilitySystemComponent* SourceASC,
		TSubclassOf<UGameplayEffect> HealEffect, float HealAmount, const UObject* SourceObject = nullptr);

	/** 위와 같되 TSoftClassPtr 를 받는다(ChipTuning 에서 바로 넘길 수 있게). */
	static void ApplyHealToActor(AActor* TargetActor, UAbilitySystemComponent* SourceASC,
		const TSoftClassPtr<UGameplayEffect>& HealEffect, float HealAmount, const UObject* SourceObject = nullptr);

	/**
	 * ★Plan5(발톱칩)★ 이 대상에게 회복을 걸면 ★실제로 소비될★ 양.
	 *
	 * "회복시킨 만큼만 체력을 쓴다"를 지키려면 GE 를 걸기 ★전에★ 알아야 한다.
	 *   · 회복 차단(NoHealing) 대상        → 0   (힐이 통째로 버려지므로 비용도 0)
	 *   · 오염된 플레이어                  → 요청량 전부 (양과 무관하게 ★정화★ 에 소비된다)
	 *   · 그 외                            → min(요청량, MaxHealth - Health)
	 */
	static float GetHealableAmount(AActor* TargetActor, float RequestedHeal);

	/**
	 * ★Plan5(발톱칩)★ 이 피해에 적용할 흡혈 비율. 흡혈 대상이 아니면 0.
	 *
	 * 두 조건을 ★모두★ 만족해야 한다:
	 *   ① 소스가 Upgrade.Feature.Skill.Claw.Lifesteal 피처를 갖고 있다
	 *   ② 그 피해가 ★기본 공격에서 나왔다★ (피해 컨텍스트의 SourceAbilityTags 로 판정)
	 *
	 * ★②를 빠뜨리면 씨앗 폭탄·물대포 피해까지 흡혈된다.★
	 *
	 * 칩 지식이 어트리뷰트셋으로 새지 않도록 판정을 여기 모은다 —
	 * UDREnemyAttributeSet 는 이 함수 하나만 부른다.
	 */
	static float GetLifestealRatioForDamage(const FEffectProperties& Props);

	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static bool IsSuccessfulDebuff(const FGameplayEffectContextHandle& EffectContextHandle);

	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static float GetDebuffDamage(const FGameplayEffectContextHandle& EffectContextHandle);

	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static float GetDebuffDuration(const FGameplayEffectContextHandle& EffectContextHandle);

	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static FGameplayTag GetDamageType(const FGameplayEffectContextHandle& EffectContextHandle);

	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static FVector GetDeathImpulse(const FGameplayEffectContextHandle& EffectContextHandle);

	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static FVector GetKnockbackForce(const FGameplayEffectContextHandle& EffectContextHandle);

	// 소스 어빌리티 AssetTags 조회 (탑승 공유 데미지 재전파 방지 판정 등)
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static FGameplayTagContainer GetSourceAbilityTags(const FGameplayEffectContextHandle& EffectContextHandle);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetIsSuccessfulDebuff(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, bool bInSuccessfulDebuff);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetDebuffDamage(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, float InDamage);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetDebuffDuration(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, float InDuration);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetDamageType(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, const FGameplayTag& InDamageType);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetDeathImpulse(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, const FVector& InImpulse);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetKnockbackForce(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, const FVector& InForce);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static void GetLiveObjectsWithinRadius(const UObject* WorldContextObject, TArray<AActor*>& OutOverlappingActors, const TArray<AActor*>& ActorsToIgnore, float Radius, const FVector& SphereOrigin);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static void GetClosestTargets(int32 MaxTargets, const TArray<AActor*>& Actors, TArray<AActor*>& OutClosestTargets, const FVector& Origin);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static AActor* GetClosestCleanserSite(APawn* ControlledPawn);

	// Asker가 Target을 직접 시야로 볼 수 있는지 검사 (벽/지형이 사이를 가로막지 않는지).
	// 비행 적이 벽 너머 플레이어를 타깃팅하지 않도록 BT의 타깃 선정/검증에서 사용.
	// 지상 적의 NavMesh 도달 가능성이 아니라 "벽 너머 차단" 의미. 비행/지상 위치와 무관하게 작동.
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|AI")
	static bool IsActorReachable(APawn* Asker, AActor* Target);

	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static bool IsNotFriend(AActor* FirstActor, AActor* SecondActor);

	/**
	 * 이 공격자가 이 대상에게 데미지를 줄 수 있는가 (BP 어빌리티의 공용 필터).
	 *
	 * ★어빌리티 BP 에서 `Cast To DREnemy` 같은 클래스 검사로 대상을 거르면
	 *   홀로그램 두더지처럼 **캐릭터가 아닌 유효 대상**이 조용히 빠진다.
	 *   대상 판정은 클래스가 아니라 이 함수로 하는 것을 규약으로 한다.
	 *
	 * 통과 조건: 유효한 액터 + 아군이 아님 + (전투 대상이면) 살아 있음
	 */
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static bool IsValidDamageTarget(AActor* Attacker, AActor* Target);

	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|DamageEffect")
	static FGameplayEffectContextHandle ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams);

	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static TArray<FRotator> EvenlySpacedRotators(const FVector& Forward, const FVector& Axis, float Spread, int32 NumRotators);

	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static TArray<FVector> EvenlyRotatedVectors(const FVector& Forward, const FVector& Axis, float Spread, int32 NumVectors);

	// 벽 충돌 체크 유틸리티 함수
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|Combat")
	static bool CheckActorWallCollision(AActor* Target, float CheckDistance = 50.f);

	// ========== GameplayEffect 생성 헬퍼 함수 ==========

	/**
	 * GameplayEffectSpec을 생성하고 적용합니다.
	 * @param ASC - AbilitySystemComponent
	 * @param EffectClass - 적용할 GameplayEffect 클래스
	 * @param SourceObject - 소스 객체 (보통 this)
	 * @param Level - 이펙트 레벨 (기본값 1.0)
	 * @return 생성된 SpecHandle (SetByCaller 등 추가 설정용)
	 */
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static FGameplayEffectSpecHandle CreateAndApplyEffectSpec(
		UAbilitySystemComponent* ASC,
		TSubclassOf<UGameplayEffect> EffectClass,
		AActor* SourceObject,
		float Level = 1.f);

	/**
	 * GameplayEffectSpec만 생성합니다 (적용 전 추가 설정 필요 시).
	 * @param ASC - AbilitySystemComponent
	 * @param EffectClass - GameplayEffect 클래스
	 * @param SourceObject - 소스 객체
	 * @param Level - 이펙트 레벨 (기본값 1.0)
	 * @return 생성된 SpecHandle
	 */
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static FGameplayEffectSpecHandle CreateEffectSpec(
		UAbilitySystemComponent* ASC,
		TSubclassOf<UGameplayEffect> EffectClass,
		AActor* SourceObject,
		float Level = 1.f);

	/**
	 * GameplayEffectSpec에 SetByCaller 값을 설정하고 적용합니다.
	 * @param ASC - 타겟 AbilitySystemComponent
	 * @param SpecHandle - 적용할 SpecHandle
	 * @param Tag - SetByCaller 태그
	 * @param Magnitude - 설정할 값
	 */
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void ApplyEffectSpecWithSetByCaller(
		UAbilitySystemComponent* ASC,
		FGameplayEffectSpecHandle& SpecHandle,
		const FGameplayTag& Tag,
		float Magnitude);
};
