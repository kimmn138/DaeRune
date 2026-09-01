// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/CharacterClassInfo.h"
#include "DRAbilitySystemLibrary.generated.h"

class UAbilityInfo;
class UAbilitySystemComponent;
class UGameBalanceConfig;
class UOverlayWidgetController;
class UUserWidget;
struct FWidgetControllerParams;

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
