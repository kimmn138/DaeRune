// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DRInteractable.h"
#include "DRCleanserPart.generated.h"

class ADRCharacter;
class USphereComponent;
class UWidgetComponent;
class ADRPlayerController;

// 부품 획득 알림 (Plan6 §5.6 - 스테이지2 방2에서 금고 내부 부품 획득을 감지)
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnPartPickedUp, ADRCleanserPart*, Part, ADRCharacter*, Character);

/**
 * Ŭ���� ��ǰ ����
 * Phase2���� �÷��̾ �����Ͽ� Ŭ���� ����Ʈ�� ��ġ�ϴ� ��ǰ
 */
UCLASS()
class DAERUNE_API ADRCleanserPart : public AActor, public IDRInteractable
{
	GENERATED_BODY()
	
public:
	ADRCleanserPart();

	// ========== ��ǰ ���� ��ȸ ==========

	// ȹ�� ���� ���� Ȯ��
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	bool CanBePickedUp() const;

	// 현재 운반 중인지 (Plan6 §5.6 - 방3 부품 동반 검증에 사용)
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	bool IsCarriedNow() const { return bIsCarried; }

	// 획득 알림 (서버). 방2 페이즈가 금고 내부 부품의 획득을 감지하는 데 사용한다.
	UPROPERTY(BlueprintAssignable, Category = "CleanserPart")
	FOnPartPickedUp OnPartPickedUp;

	// 부품 메시 Getter (캐릭터에서 1P 메시 복제에 사용)
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	UStaticMeshComponent* GetPartMesh() const { return PartMesh; }

	// ========== ��ǰ ó�� ==========

	// ��ǰ ȹ�� ó��
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	void PickupPart(ADRCharacter* Character);

	// ��ǰ ��ġ ó��
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	void InstallPart();

	// ĳ���ͷκ��� ����߸��� 
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	void DropFromCarrier();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayPickupSound();

	// ========== ����Ʈ���̽� �ݹ� ========== 

	// 상호작용 UI 표시/숨김 (IDRInteractable - 로컬에서 직접 호출)
	virtual void SetInteractionUIVisible(bool bShow) override;

	// 캐릭터의 현재 오버랩 + 부품 보유 상태를 재평가하여 라인트레이스 감지를 갱신
	// (오버랩 도중 부품을 집어들거나 내려놓을 때 호출)
	void RefreshOverlapStateFor(ADRCharacter* Character);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ========== ������Ʈ ==========

	// ��ǰ �޽�
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PartMesh;

	// ���� ���� (����Ʈ���̽� Ȱ��ȭ��)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> DetectionSphere;

	// ȹ�� UI ���� 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> InteractionWidget;

	// ========== ���� ==========

	// ĳ���Ϳ� ������ ���� �̸�
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CleanserPart|Config")
	FName AttachSocketName = "TestPartHand";

	// ���� ���� �ݰ�
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CleanserPart|Config")
	float DetectionRadius = 300.f;

	// ========== ��ǰ ���� ==========

	// �÷��̾ ��� �ִ� ����
	UPROPERTY(ReplicatedUsing = OnRep_bIsCarried, BlueprintReadOnly, Category = "CleanserPart")
	bool bIsCarried;

	// ��ǰ�� ��� �ִ� ĳ����
	UPROPERTY(ReplicatedUsing = OnRep_CarryingCharacter, BlueprintReadOnly, Category = "CleanserPart")
	TObjectPtr<ADRCharacter> CarryingCharacter;

	// ========== ������ �̺�Ʈ ==========

	UFUNCTION()
	void OnDetectionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDetectionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// ========== ���ø����̼� �ݹ� ==========

	UFUNCTION()
	void OnRep_bIsCarried();

	UFUNCTION()
	void OnRep_CarryingCharacter();

	void RefreshCarriedState();
};
