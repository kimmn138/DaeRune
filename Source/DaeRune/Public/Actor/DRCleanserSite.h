// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "DRCleanserSite.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class UDRCleanserSiteAttributeSet;
class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class ADRPlayerController;
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
// 부품 설치 완료 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartInstalled, ADRCleanserSite*, Site);
// 클렌저 사이트 체력 50% 이하 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCleanserSiteHealthHalf, ADRCleanserSite*, Site);

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

	UFUNCTION(BlueprintPure, Category = "Cleanser Site")
	FName GetCleanserID() const { return CleanserID; }

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

	// 부품 설치
	UFUNCTION(BlueprintCallable, Category = "CleanserSite|Phase2")
	void InstallPart(class ADRCharacter* Character);

	// 현재 설치된 부품 개수 가져오기
	UFUNCTION(BlueprintCallable, Category = "CleanserSite|Phase2")
	int32 GetInstalledPartsCount() const { return InstalledPartsCount; }

	// 부품 설치 완료 여부
	UFUNCTION(BlueprintCallable, Category = "CleanserSite|Phase2")
	bool IsPartInstallationComplete() const { return InstalledPartsCount >= RequiredPartsCount; }

	// 현재 상태 가져오기
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	ECleanserSiteState GetCurrentState() const { return CurrentState; }

	// ========== 위치 정보 ==========

	// 스폰 위치 (엘리트가 스폰될 중앙)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	FVector GetSpawnLocation() const;

	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	FVector GetClosestSurfacePoint(const FVector& FromLocation) const;

	// ========== 물 메시 관리 ==========

	// 체력 비율에 따라 물 메시 스케일 업데이트
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void UpdateWaterMeshScale(float HealthRatio);

	// ========== 델리게이트 ==========

	// 부품 설치 완료 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "CleanserSite|Phase2")
	FOnPartInstalled OnPartInstalled;

	// 클렌저 사이트가 파괴되었을 때
	UPROPERTY(BlueprintAssignable, Category = "CleanserSite")
	FOnCleanserSiteDestroyed OnCleanserSiteDestroyed;

	// 클렌저 사이트 체력 50% 이하일 때
	UPROPERTY(BlueprintAssignable, Category = "CleanserSite")
	FOnCleanserSiteHealthHalf OnCleanserSiteHealthHalf;

	void InitializeDefaultAttributes() const;

	void UpdateMeshByState();

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// UI 업데이트
	void UpdateInteractionUI() const;

	// 클렌저 사이트 고유 식별자
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleanser Site")
	FName CleanserID = NAME_None;

	// ========== Components ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	// 클렌저 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CleanserMesh;

	// 클렌저 물 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WaterMesh;

	// 부품 설치 후 클렌저 메시
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mesh Assets")
	TObjectPtr<UStaticMesh> CleanserMesh_AfterParts;

	// 부품 설치 후 물 메시
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mesh Assets")
	TObjectPtr<UStaticMesh> WaterMesh_AfterParts;

	// 상호작용 범위
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	// 상호작용 UI 위젯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> InteractionWidget;

	// ========== GAS Components ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GAS")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UDRCleanserSiteAttributeSet> AttributeSet;

	// ========== GAS Attributes ==========

	// 기본 체력 속성 (Phase3에서 사용)
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttributes;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultVitalAttributes;

	// ========== State ==========

	// 필요한 부품 개수
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CleanserSite|Phase2|Config")
	int32 RequiredPartsCount = 2;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentState, BlueprintReadOnly, Category = "CleanserSite")
	ECleanserSiteState CurrentState;

	// 설치된 부품 개수
	UPROPERTY(ReplicatedUsing = OnRep_InstalledPartsCount, BlueprintReadOnly, Category = "CleanserSite|Phase2")
	int32 InstalledPartsCount;

	UFUNCTION()
	void OnRep_CurrentState();

	UFUNCTION()
	static void OnRep_InstalledPartsCount();

	// ========== 체력 관리 (Phase3 전용) ==========

	// Phase3에서 체력 활성화 여부
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "CleanserSite")
	bool bHealthEnabled;

	// GAS 관련 함수
	void ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass) const;

private:
	// GAS 초기화
	void InitAbilityActorInfo();

	// 물 메시 초기 스케일 저장
	FVector InitialWaterMeshScale;
	FVector InitialWaterMeshLocation;
};
