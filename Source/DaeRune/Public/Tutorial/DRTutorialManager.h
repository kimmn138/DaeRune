// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRTutorialManager.generated.h"

class ADRTutorialGate;
class ADRTutorialPressurePlate;
class ADRTutorialStartTile;
class ADREnemy;
class ADRCleanserSite;
class UOverlayWidgetController;
class UDRAbilitySystemComponent;
class UGameplayAbility;

// 튜토리얼 구간 열거형
UENUM(BlueprintType)
enum class ETutorialSection : uint8
{
	Section1_Parkour     UMETA(DisplayName = "Section1 Parkour"),
	Section2_Combat      UMETA(DisplayName = "Section2 Combat"),
	Section3_PartCollect UMETA(DisplayName = "Section3 Part Collection"),
	Completed            UMETA(DisplayName = "Tutorial Completed")
};

// 구간 2 전투 목표 열거형
UENUM(BlueprintType)
enum class ECombatObjective : uint8
{
	None,
	Objective1_MeleeAttack,    // 기본 공격 5회
	Objective2_WaterPump,      // 물대포 3초
	Objective3_SeedCannon,     // SeedCannon 동시 적중
	AllComplete
};

/**
 * 튜토리얼 전체 진행을 관리하는 중앙 액터
 * 구간/목표 상태를 추적하고, 문/발판/적/UI를 제어한다.
 */
UCLASS(Blueprintable)
class DAERUNE_API ADRTutorialManager : public AActor
{
	GENERATED_BODY()

public:
	ADRTutorialManager();

	// ========== 구간 진행 ==========

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	ETutorialSection GetCurrentSection() const { return CurrentSection; }

	UFUNCTION(BlueprintPure, Category = "Tutorial")
	ECombatObjective GetCurrentObjective() const { return CurrentObjective; }

	// 발판이 밟혔을 때 호출
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void OnPressurePlateActivated(int32 SectionIndex);

	// ========== 구간 2 전투 훈련 ==========

	// 샌드백 적에게 데미지 적중 시 호출 (AbilityTag로 어빌리티 구분)
	UFUNCTION(BlueprintCallable, Category = "Tutorial|Combat")
	void ReportDamageHit(const FGameplayTagContainer& AbilityTags);

	// SeedCannon 폭발 적중 수 보고
	UFUNCTION(BlueprintCallable, Category = "Tutorial|Combat")
	void ReportSeedCannonHits(int32 HitCount);

	// ========== 구간 3 부품 수집 ==========

	// 시작 타일 밟힘 보고
	UFUNCTION(BlueprintCallable, Category = "Tutorial|Parts")
	void OnStartTileActivated();

	// 부품 설치 보고
	UFUNCTION()
	void OnPartInstalledToSite(ADRCleanserSite* Site);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// ========== 에디터 설정: 문/발판 참조 ==========

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
	TObjectPtr<ADRTutorialGate> Gate1;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
	TObjectPtr<ADRTutorialGate> Gate2;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
	TObjectPtr<ADRTutorialPressurePlate> PressurePlate1;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
	TObjectPtr<ADRTutorialPressurePlate> PressurePlate2;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
	TObjectPtr<ADRTutorialPressurePlate> PressurePlate3;

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
	TObjectPtr<ADRTutorialStartTile> StartTile;

	// ========== 에디터 설정: 적 참조 ==========

	// 구간 2 중앙 샌드백 적 (레벨에 미리 배치)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
	TObjectPtr<ADREnemy> DummyEnemy_Center;

	// SeedCannon 목표용 적 스폰 포인트 (4방향, 중앙에서 300 유닛 거리)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
	TArray<TObjectPtr<AActor>> SeedCannonSpawnPoints;

	// SeedCannon 목표용 샌드백 적 클래스
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Config")
	TSubclassOf<ADREnemy> DummyEnemyClass;

	// 구간 3 부품 적 (레벨에 미리 배치)
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
	TArray<TObjectPtr<ADREnemy>> PartEnemies;

	// 구간 3 클렌저사이트
	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Tutorial|References")
	TObjectPtr<ADRCleanserSite> TutorialCleanserSite;

	// ========== 어빌리티 부여 설정 ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Abilities")
	TSubclassOf<UGameplayAbility> ClawSwipeAbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Abilities")
	TSubclassOf<UGameplayAbility> WaterPumpAbilityClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Abilities")
	TSubclassOf<UGameplayAbility> SeedCannonAbilityClass;

	// ========== 목표 수치 설정 ==========

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Config")
	int32 RequiredMeleeHits = 5;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Config")
	float RequiredWaterPumpSeconds = 3.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Tutorial|Config")
	int32 RequiredSimultaneousHits = 5;

private:
	// ========== 상태 ==========

	ETutorialSection CurrentSection = ETutorialSection::Section1_Parkour;
	ECombatObjective CurrentObjective = ECombatObjective::None;

	bool bSection1Cleared = false;
	bool bSection2Cleared = false;
	bool bSection3Cleared = false;

	int32 MeleeHitCount = 0;
	float WaterPumpAccumulatedTime = 0.f;

	UPROPERTY()
	TArray<TObjectPtr<ADREnemy>> SpawnedSeedCannonDummies;

	// ========== 내부 함수 ==========

	void TransitionToSection(ETutorialSection NewSection);
	void TransitionToObjective(ECombatObjective NewObjective);

	void GrantAbilityToPlayer(TSubclassOf<UGameplayAbility> AbilityClass);
	void SpawnSeedCannonDummies();
	void CleanupSeedCannonDummies();

	void SetupDummyEnemy(ADREnemy* Enemy);
	void StopPartEnemyAI();
	void StartPartEnemyAI();

	void UpdateObjectiveUI(const FText& Title, const FText& ProgressFormat, int32 Current, int32 Max);

	UOverlayWidgetController* GetOverlayWidgetController() const;
	UDRAbilitySystemComponent* GetPlayerASC() const;
};
