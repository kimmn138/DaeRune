// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRCleanserPart.generated.h"

class ADRCharacter;
class USphereComponent;
class UWidgetComponent;
class ADRPlayerController;

/**
 * 클렌저 부품 액터
 * Phase2에서 플레이어가 수집하여 클렌저 사이트에 설치하는 부품
 */
UCLASS()
class DAERUNE_API ADRCleanserPart : public AActor
{
	GENERATED_BODY()
	
public:
	ADRCleanserPart();

	// ========== 부품 상태 조회 ==========

	// 획득 가능 여부 확인
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	bool CanBePickedUp() const;

	// ========== 부품 처리 ==========

	// 부품 획득 처리
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	void PickupPart(ADRCharacter* Character);

	// 부품 설치 처리
	UFUNCTION(BlueprintCallable, Category = "CleanserPart")
	void InstallPart();

	// ========== 라인트레이싱 콜백 ========== 

	// UI 표시/숨김 (멀티캐스트 - 모든 클라이언트 실행)
	UFUNCTION(NetMulticast, Reliable)
	void MulticastShowInteractionUI(ADRPlayerController* PlayerController, bool bShow);

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
	FName AttachSocketName = "hand_l";

	// 감지 범위 반경
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CleanserPart|Config")
	float DetectionRadius = 150.f;

	// ========== 부품 상태 ==========

	// 플레이어가 들고 있는 상태
	UPROPERTY(ReplicatedUsing = OnRep_bIsCarried, BlueprintReadOnly, Category = "CleanserPart")
	bool bIsCarried;

	// 부품을 들고 있는 캐릭터
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "CleanserPart")
	TObjectPtr<ADRCharacter> CarryingCharacter;

	// ========== 오버랩 이벤트 ==========

	UFUNCTION()
	void OnDetectionSphereBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

	UFUNCTION()
	void OnDetectionSphereEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

	// ========== 리플리케이션 콜백 ==========

	UFUNCTION()
	void OnRep_bIsCarried();
};
