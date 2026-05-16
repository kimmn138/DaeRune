// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRStageSelectActor.generated.h"

class UBoxComponent;
class UWidgetComponent;
class ADRPlayerController;

UCLASS()
class DAERUNE_API ADRStageSelectActor : public AActor
{
	GENERATED_BODY()
	
public:	
	ADRStageSelectActor();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> InteractionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> PortalMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UWidgetComponent> InteractionWidget;

    // ������ �� �̸�
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    FString DestinationMapName;

private:
    // ������ �̺�Ʈ
    UFUNCTION()
    void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    // ��ȣ�ۿ� ó��
    UFUNCTION()
    void OnHostInteract();

    UFUNCTION(Server, Reliable)
    void ServerRequestTravel();

    // 호스트에게 UI를 항상 표시하기 위한 셋업 (호스트 폰을 오너로 지정)
    void SetupHostWidgetVisibility();

    // ���� ������ ���� ȣ��Ʈ ��Ʈ�ѷ�
    UPROPERTY()
    TObjectPtr<ADRPlayerController> OverlappingHostController;

    // UI 가시성을 위해 오너로 설정된 호스트 컨트롤러
    UPROPERTY()
    TObjectPtr<ADRPlayerController> HostController;

    FTimerHandle HostSetupTimerHandle;
};
