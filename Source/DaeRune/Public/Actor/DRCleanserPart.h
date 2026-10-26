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
 * 클렌저 부품 액터
 * Phase2에서 플레이어가 수집하여 클렌저 사이트에 설치하는 부품
 */
UCLASS()
class DAERUNE_API ADRCleanserPart : public AActor, public IDRInteractable
{
	GENERATED_BODY()
	
public:
	ADRCleanserPart();

	// ========== 부품 상태 조회 ==========

	// 획득 가능 여부 확인
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

	// ========== 부품 처리 ==========

	// 부품 획득 처리
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	void PickupPart(ADRCharacter* Character);

	// 부품 설치 처리
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	void InstallPart();

	// 캐릭터로부터 떨어뜨리기 
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	void DropFromCarrier();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayPickupSound();

	// ========== 라인트레이싱 콜백 ========== 

	// 상호작용 UI 표시/숨김 (IDRInteractable - 로컬에서 직접 호출)
	virtual void SetInteractionUIVisible(bool bShow) override;

	// 캐릭터의 현재 오버랩 + 부품 보유 상태를 재평가하여 라인트레이스 감지를 갱신
	// (오버랩 도중 부품을 집어들거나 내려놓을 때 호출)
	void RefreshOverlapStateFor(ADRCharacter* Character);

protected:
	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ========== 컴포넌트 ==========

	// 부품 메시
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> PartMesh;

	// 감지 범위 (라인트레이싱 활성화용)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> DetectionSphere;

	// 획득 UI 위젯 
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UWidgetComponent> InteractionWidget;

	// ========== 설정 ==========

	// 캐릭터에 부착할 소켓 이름
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CleanserPart|Config")
	FName AttachSocketName = "TestPartHand";

	// 감지 범위 반경
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CleanserPart|Config")
	float DetectionRadius = 300.f;

	// ========== 부품 상태 ==========

	// 플레이어가 들고 있는 상태
	UPROPERTY(ReplicatedUsing = OnRep_bIsCarried, BlueprintReadOnly, Category = "CleanserPart")
	bool bIsCarried;

	// 부품을 들고 있는 캐릭터
	UPROPERTY(ReplicatedUsing = OnRep_CarryingCharacter, BlueprintReadOnly, Category = "CleanserPart")
	TObjectPtr<ADRCharacter> CarryingCharacter;

	// ========== 오버랩 이벤트 ==========

	UFUNCTION()
	void OnDetectionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDetectionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// ========== 리플리케이션 콜백 ==========

	UFUNCTION()
	void OnRep_bIsCarried();

	UFUNCTION()
	void OnRep_CarryingCharacter();

	void RefreshCarriedState();

	// 들고 있는 사람 화면에 1인칭 부품을 띄운다 (들림 · 들고 있는 사람 복제 둘 다 도착해야 가능)
	void RefreshFirstPersonPart() const;
};
