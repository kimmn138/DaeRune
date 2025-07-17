// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Data/CharacterClassInfo.h"
#include "DRAbilitySystemLibrary.generated.h"

class UAbilityInfo;
class UAbilitySystemComponent;
class UOverlayWidgetController;
struct FWidgetControllerParams;

/**
 * UDRAbilitySystemLibrary 클래스: 게임플레이 능력 시스템(GAS) 관련 유틸리티 함수 모음
 */
UCLASS()
class DAERUNE_API UDRAbilitySystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
	
public:
	// 위젯 컨트롤러 파라미터 생성 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|WidgetController", meta = (DefaultToSelf = "WorldContextObject"))
	static bool MakeWidgetControllerParams(const UObject* WorldContextObject, FWidgetControllerParams& OutWCParams, ADRHUD*& OutDRHUD);

	// 오버레이 위젯 컨트롤러 반환 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|WidgetController", meta = (DefaultToSelf = "WorldContextObject"))
	static UOverlayWidgetController* GetOverlayWidgetController(const UObject* WorldContextObject);

	// 플레이어 기본 속성 초기화 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static void InitializePlayerDefaultAttributes(const UObject* WorldContextObject, EPlayerCharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC);

	// 적 기본 속성 초기화 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static void InitializeEnemyDefaultAttributes(const UObject* WorldContextObject, EEnemyCharacterClass CharacterClass, float Level, UAbilitySystemComponent* ASC);

	// 플레이어 시작 능력 부여 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static void GivePlayerStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, EPlayerCharacterClass CharacterClass);

	// 적 시작 능력 부여 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static void GiveEnemyStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, EEnemyCharacterClass CharacterClass);

	// 캐릭터 클래스 정보 반환 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static UCharacterClassInfo* GetCharacterClassInfo(const UObject* WorldContextObject);

	// 능력 정보 반환 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|CharacterClassDefaults")
	static UAbilityInfo* GetAbilityInfo(const UObject* WorldContextObject);

	// 디버프 성공 여부 확인 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static bool IsSuccessfulDebuff(const FGameplayEffectContextHandle& EffectContextHandle);

	// 디버프 피해량 반환 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static float GetDebuffDamage(const FGameplayEffectContextHandle& EffectContextHandle);

	// 디버프 지속 시간 반환 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static float GetDebuffDuration(const FGameplayEffectContextHandle& EffectContextHandle);

	// 디버프 빈도 반환 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static float GetDebuffFrequency(const FGameplayEffectContextHandle& EffectContextHandle);

	// 데미지 타입 태그 반환 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static FGameplayTag GetDamageType(const FGameplayEffectContextHandle& EffectContextHandle);

	// 사망 임펄스 반환 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static FVector GetDeathImpulse(const FGameplayEffectContextHandle& EffectContextHandle);

	// 넉백 포스 반환 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static FVector GetKnockbackForce(const FGameplayEffectContextHandle& EffectContextHandle);

	// 디버프 성공 여부 설정 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetIsSuccessfulDebuff(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, bool bInSuccessfulDebuff);

	// 디버프 피해량 설정 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetDebuffDamage(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, float InDamage);

	// 디버프 지속 시간 설정 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetDebuffDuration(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, float InDuration);

	// 디버프 빈도 설정 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetDebuffFrequency(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, float InFrequency);

	// 데미지 타입 태그 설정 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetDamageType(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, const FGameplayTag& InDamageType);

	// 사망 임펄스 설정 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetDeathImpulse(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, const FVector& InImpulse);

	// 넉백 포스 설정 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayEffects")
	static void SetKnockbackForce(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle, const FVector& InForce);

	// 반경 내 살아있는 플레이어 검색 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static void GetLivePlayersWithinRadius(const UObject* WorldContextObject, TArray<AActor*>& OutOverlappingActors, const TArray<AActor*>& ActorsToIgnore, float Radius, const FVector& SphereOrigin);

	// 가장 가까운 타겟 검색 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static void GetClosestTargets(int32 MaxTargets, const TArray<AActor*>& Actors, TArray<AActor*>& OutClosestTargets, const FVector& Origin);

	// 서로 아군이 아닌지 판단 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static bool IsNotFriend(AActor* FirstActor, AActor* SecondActor);

	// 데미지 효과 적용 기능
	UFUNCTION(BlueprintCallable, Category = "DRAbilitySystemLibrary|DamageEffect")
	static FGameplayEffectContextHandle ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams);

	// 균등 회전된 로테이터 배열 반환 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static TArray<FRotator> EvenlySpacedRotators(const FVector& Forward, const FVector& Axis, float Spread, int32 NumRotators);

	// 균등 회전된 벡터 배열 반환 기능
	UFUNCTION(BlueprintPure, Category = "DRAbilitySystemLibrary|GameplayMechanics")
	static TArray<FVector> EvenlyRotatedVectors(const FVector& Forward, const FVector& Axis, float Spread, int32 NumVectors);
};
