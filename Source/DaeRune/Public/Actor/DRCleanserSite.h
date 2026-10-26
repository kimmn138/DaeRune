// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "Interaction/DRInteractable.h"
#include "UI/WidgetController/DRAttributeChangedDelegate.h"
#include "DRCleanserSite.generated.h"

class UGameplayEffect;
class UAbilitySystemComponent;
class UDRCleanserSiteAttributeSet;
class UStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class UDRBillboardWidgetComponent;
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

// 부품 설치 완료 델리게이트
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartInstalled, ADRCleanserSite*, Site);

/**
 * 클렌저 설치 지점
 * - 확보 페이즈(UDRPhase1): 하나만 남기고 활성화 → 적 스폰 기준 + 부품 설치
 * - 방어 페이즈(UDRPhase3): 방어 대상 (체력 사용)
 * - 스테이지2: 방3+4 의 부품 설치대 (부품 1개, 재시도 시 EjectInstalledPart 로 되돌림)
 */
UCLASS()
class DAERUNE_API ADRCleanserSite : public AActor, public IAbilitySystemInterface, public IDRInteractable
{
	GENERATED_BODY()
	
public:
	ADRCleanserSite();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// AbilitySystemInterface
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UDRCleanserSiteAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// 체력 변화 이벤트 델리게이트
	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable)
	FOnAttributeChangedSignature OnMaxHealthChanged;

	UFUNCTION(BlueprintPure, Category = "Cleanser Site")
	FName GetCleanserID() const { return CleanserID; }

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayInstallSound(bool bIsComplete);

	// 부품 설치 시 해당 슬롯 메시를 모든 클라이언트에서 표시
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShowInstalledPart(int32 SlotIndex);

	// 상호작용 UI 표시/숨김 (IDRInteractable - 로컬에서 직접 호출)
	virtual void SetInteractionUIVisible(bool bShow) override;

	// Phase3 클린저 작동 사운드 (루프 - 시작/종료 시 한 번씩만 호출)
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartOperatingSound();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopOperatingSound();

	// ========== 상태 관리 ==========

	// 사이트 활성화 (Phase1에서 선택됨)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void ActivateSite();

	// 부품 수집 완료 (Phase2)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void SetPartsCollected();

	// 부품 설치
	UFUNCTION(BlueprintCallable, Category = "CleanserSite|Phase2")
	void InstallPart(class ADRCharacter* Character);

	// 설치된 부품을 되돌린다 (Plan6 §5.12 - 스테이지2 방4 재시도용).
	// 설치 카운트를 되돌리고 설치대 옆에 부품을 다시 스폰한다.
	// 설치 완료 상태였으면 다시 설치를 받는 상태(Active)로 돌린다 — 그래야 재설치 · 상호작용 UI 가 산다.
	// 스테이지1은 이 함수를 호출하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	class ADRCleanserPart* EjectInstalledPart(TSubclassOf<class ADRCleanserPart> PartClass, float EjectOffset = 150.f);

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

	UFUNCTION(BlueprintCallable, Category = "CleanserSite|Phase1")
	TArray<FVector> GetPhase1EnemySpawnLocations() const;

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

	void InitializeDefaultAttributes() const;

	void UpdateMeshByState();

	// 캐릭터의 현재 오버랩 + 부품 보유 상태를 재평가하여 상호작용 UI를 갱신
	// (오버랩 도중 부품을 집어들거나 내려놓을 때 호출)
	void RefreshOverlapStateFor(class ADRCharacter* Character);

protected:
	virtual void PostInitializeComponents() override;
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

	// World locations used by Phase1 normal enemy spawning. The legacy property name is kept to preserve placed map data.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CleanserSite|Phase1|Spawn", meta = (DisplayName = "Phase1 Enemy Spawn World Locations"))
	TArray<FVector> Phase1EnemySpawnOffsets;

	// ========== Components ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	// 클렌저 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CleanserMesh;

	// 클렌저 물 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WaterMesh;

	// 물 메시 Z 스케일 감소량 → 위치 보정 계수
	// (스케일 축소 시 수면이 아래로 내려가 보이도록 메시 형태에 맞춰 조정)
	UPROPERTY(EditDefaultsOnly, Category = "CleanserSite|Water")
	float WaterMeshScaleToOffsetRatio = 9.0f;

	// 부품 설치 후 물 메시
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mesh Assets")
	TObjectPtr<UStaticMesh> WaterMesh_AfterParts;

	// 상호작용 범위
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	// 상호작용 UI 위젯
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> InteractionWidget;

	// ========== 설치된 부품 메시 슬롯 ==========

	// 1번째 부품이 표시될 메시 컴포넌트 (블루프린트에서 메시/위치 설정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|InstalledParts")
	TObjectPtr<UStaticMeshComponent> InstalledPartMesh1;

	// 2번째 부품이 표시될 메시 컴포넌트 (블루프린트에서 메시/위치 설정)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components|InstalledParts")
	TObjectPtr<UStaticMeshComponent> InstalledPartMesh2;

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
	void OnRep_InstalledPartsCount();

	// ========== 체력 관리 (Phase3 전용) ==========

	// GAS 관련 함수
	void ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UDRBillboardWidgetComponent> HealthBar;

private:
	// GAS 초기화
	void InitAbilityActorInfo();

	// 물 메시 초기 상태 (부품 설치 · 체력으로 바뀐 물을 되돌릴 때 기준)
	FVector InitialWaterMeshScale;
	FVector InitialWaterMeshLocation;

	UPROPERTY()
	TObjectPtr<UStaticMesh> InitialWaterStaticMesh;

	UPROPERTY()
	TObjectPtr<UAudioComponent> OperatingSoundComponent;
};
