// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystemInterface.h"
#include "UI/WidgetController/OverlayWidgetController.h"
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

// Ŭ���� ����Ʈ ����
UENUM(BlueprintType)
enum class ECleanserSiteState : uint8
{
	Inactive		UMETA(DisplayName = "Inactive"),		// ��Ȱ��ȭ (���� �� ��)
	Active			UMETA(DisplayName = "Active"),			// Ȱ��ȭ (Phase1���� ���õ�)
	PartsCollected	UMETA(DisplayName = "PartsCollected"),	// ��ǰ ���� �Ϸ� (Phase2)
	Operational		UMETA(DisplayName = "Operational"),		// ���� �� (Phase3 ��� ���)
	Completed		UMETA(DisplayName = "Completed")		// ��� �Ϸ� (Phase4)
};

// Ŭ���� ����Ʈ �ı� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCleanserSiteDestroyed, ADRCleanserSite*, DestroyedSite);
// ��ǰ ��ġ �Ϸ� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPartInstalled, ADRCleanserSite*, Site);
// Ŭ���� ����Ʈ ü�� 50% ���� ��������Ʈ
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCleanserSiteHealthHalf, ADRCleanserSite*, Site);

/**
 * Ŭ���� ��ġ ����
 * - Phase1: 3�� �� 2�� ���õǾ� �� ����
 * - Phase2: ��ǰ ���� �� Ȱ��ȭ
 * - Phase3: ��� ��� (ü�� Ȱ��ȭ)
 * - Phase4: ������ ���
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

	// Phase3 클린저 작동 사운드 (루프 - 시작/종료 시 한 번씩만 호출)
	UFUNCTION(NetMulticast, Reliable)
	void MulticastStartOperatingSound();

	UFUNCTION(NetMulticast, Reliable)
	void MulticastStopOperatingSound();

	// ========== ���� ���� ==========

	// ����Ʈ Ȱ��ȭ (Phase1���� ���õ�)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void ActivateSite();

	// ����Ʈ ��Ȱ��ȭ
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void DeactivateSite();

	// ��ǰ ���� �Ϸ� (Phase2)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void SetPartsCollected();

	// ��ǰ ��ġ
	UFUNCTION(BlueprintCallable, Category = "CleanserSite|Phase2")
	void InstallPart(class ADRCharacter* Character);

	// ���� ��ġ�� ��ǰ ���� ��������
	UFUNCTION(BlueprintCallable, Category = "CleanserSite|Phase2")
	int32 GetInstalledPartsCount() const { return InstalledPartsCount; }

	// ��ǰ ��ġ �Ϸ� ����
	UFUNCTION(BlueprintCallable, Category = "CleanserSite|Phase2")
	bool IsPartInstallationComplete() const { return InstalledPartsCount >= RequiredPartsCount; }

	// ���� ���� ��������
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	ECleanserSiteState GetCurrentState() const { return CurrentState; }

	// ========== ��ġ ���� ==========

	// ���� ��ġ (����Ʈ�� ������ �߾�)
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	FVector GetSpawnLocation() const;

	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	FVector GetClosestSurfacePoint(const FVector& FromLocation) const;

	// ========== �� �޽� ���� ==========

	// ü�� ������ ���� �� �޽� ������ ������Ʈ
	UFUNCTION(BlueprintCallable, Category = "CleanserSite")
	void UpdateWaterMeshScale(float HealthRatio);

	// ========== ��������Ʈ ==========

	// ��ǰ ��ġ �Ϸ� ��������Ʈ
	UPROPERTY(BlueprintAssignable, Category = "CleanserSite|Phase2")
	FOnPartInstalled OnPartInstalled;

	// Ŭ���� ����Ʈ�� �ı��Ǿ��� ��
	UPROPERTY(BlueprintAssignable, Category = "CleanserSite")
	FOnCleanserSiteDestroyed OnCleanserSiteDestroyed;

	// Ŭ���� ����Ʈ ü�� 50% ������ ��
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

	// UI ������Ʈ
	void UpdateInteractionUI() const;

	// Ŭ���� ����Ʈ ���� �ĺ���
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cleanser Site")
	FName CleanserID = NAME_None;

	// ========== Components ==========

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> RootSceneComponent;

	// Ŭ���� �޽�
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> CleanserMesh;

	// Ŭ���� �� �޽�
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> WaterMesh;

	// ��ǰ ��ġ �� �� �޽�
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Mesh Assets")
	TObjectPtr<UStaticMesh> WaterMesh_AfterParts;

	// ��ȣ�ۿ� ����
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UBoxComponent> InteractionBox;

	// ��ȣ�ۿ� UI ����
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

	// �⺻ ü�� �Ӽ� (Phase3���� ���)
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttributes;
	
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultVitalAttributes;

	// ========== State ==========

	// �ʿ��� ��ǰ ����
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CleanserSite|Phase2|Config")
	int32 RequiredPartsCount = 2;

	UPROPERTY(ReplicatedUsing = OnRep_CurrentState, BlueprintReadOnly, Category = "CleanserSite")
	ECleanserSiteState CurrentState;

	// ��ġ�� ��ǰ ����
	UPROPERTY(ReplicatedUsing = OnRep_InstalledPartsCount, BlueprintReadOnly, Category = "CleanserSite|Phase2")
	int32 InstalledPartsCount;

	UFUNCTION()
	void OnRep_CurrentState();

	UFUNCTION()
	void OnRep_InstalledPartsCount();

	// ========== ü�� ���� (Phase3 ����) ==========

	// Phase3���� ü�� Ȱ��ȭ ����
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "CleanserSite")
	bool bHealthEnabled;

	// GAS ���� �Լ�
	void ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass) const;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TObjectPtr<UDRBillboardWidgetComponent> HealthBar;

private:
	// GAS �ʱ�ȭ
	void InitAbilityActorInfo();

	// �� �޽� �ʱ� ������ ����
	FVector InitialWaterMeshScale;
	FVector InitialWaterMeshLocation;

	UPROPERTY()
	TObjectPtr<UAudioComponent> OperatingSoundComponent;
};
