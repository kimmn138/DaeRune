// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "DRCleanserSite.generated.h"

class UAbilitySystemComponent;
class UDRCleanserSiteAttributeSet;
class UStaticMeshComponent;
struct FOnAttributeChangeData;

// 클렌저 사이트 상태
UENUM(BlueprintType)
enum class ECleanserSiteState : uint8
{
	Inactive		UMETA(DisplayName = "Inactive"),		// 비활성화 (선택 안 됨)
	Active			UMETA(DisplayName = "Active"),			// 활성화 (Phase1에서 선택됨)
	PartsCollected	UMETA(DisplayName = "PartsCollected"),	// 부품 수집 완료 (Phase2)
	Operational		UMETA(DisplayName = "Operational"),		// 가동 중 (Phase3 방어 대상)
	Completed		UMETA(DisplayName = "Completed")		// 방어 완료 (Phase4)
};

// 클렌저 사이트 파괴 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCleanserSiteDestroyed, ADRCleanserSite*, DestroyedSite);

/**
 * 클렌저 설치 지점
 * - Phase1: 3개 중 2개 선택되어 적 스폰
 * - Phase2: 부품 결합 및 활성화
 * - Phase3: 방어 대상 (체력 활성화)
 * - Phase4: 보스전 배경
 */
UCLASS()
class DAERUNE_API ADRCleanserSite : public AActor, public IAbilitySystemInterface
{
	GENERATED_BODY()
	
	public:
	ADRCleanserSite();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// AbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UDRCleanserSiteAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// ========== 상태 관리 ==========

	// 사이트 활성화 (Phase1에서 선택됨)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void ActivateSite();

	// 사이트 비활성화
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void DeactivateSite();

	// 부품 수집 완료 (Phase2)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void SetPartsCollected();

	// 가동 시작 (Phase3 - 체력 활성화)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void StartOperation();

	// 가동 종료 (Phase3 끝 - 체력 비활성화)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void StopOperation();

	// 방어 완료 (Phase4)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void SetCompleted();

	// 현재 상태 가져오기
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	ECleanserSiteState GetCurrentState() const { return CurrentState; }

	// ========== 위치 정보 ==========

	// 스폰 위치 (엘리트가 스폰될 중앙)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	FVector GetSpawnLocation() const;

	// ========== 델리게이트 ==========

	// 클렌저 사이트가 파괴되었을 때
	UPROPERTY(BlueprintAssignable, Category = "CleanserSite")
	FOnCleanserSiteDestroyed OnCleanserSiteDestroyed;

protected:
	virtual void BeginPlay() override;

	// ========== Components ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	// 클렌저 메시 (상태에 따라 보이기/숨기기)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CleanserMesh;

	// ========== GAS Components ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UDRCleanserSiteAttributeSet> AttributeSet;

	// ========== State ==========

	UPROPERTY(ReplicatedUsing = OnRep_CurrentState, BlueprintReadOnly, Category = "CleanserSite")
	ECleanserSiteState CurrentState;

	UFUNCTION()
	void OnRep_CurrentState();

	// ========== 체력 관리 (Phase3 전용) ==========

	// Phase3에서 체력 활성화 여부
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "CleanserSite")
	bool bHealthEnabled;

	// 체력 초기화
	void InitializeHealth();

	// 체력 비활성화
	void DisableHealth();

	// 체력 변경 감지
	void OnHealthChanged(const FOnAttributeChangeData& Data);

private:
	// GAS 초기화
	void InitAbilityActorInfo();
};
