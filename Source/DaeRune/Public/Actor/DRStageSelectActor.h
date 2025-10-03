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

    // 목적지 맵 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    FString DestinationMapName;

private:
    // 오버랩 이벤트
    UFUNCTION()
    void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    // 상호작용 처리
    UFUNCTION()
    void OnHostInteract();

    UFUNCTION(Server, Reliable)
    void ServerRequestTravel();

    // 현재 오버랩 중인 호스트 컨트롤러
    UPROPERTY()
    TObjectPtr<ADRPlayerController> OverlappingHostController;
};
