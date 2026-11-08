// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DRInteractable.h"
#include "DRStageSelectActor.generated.h"

class UBoxComponent;
class ADRPlayerController;

/**
 * 로비의 스테이지 선택 포털 — 범위 안에서 F 를 누르면 스테이지 선택 화면을 연다 (★호스트 전용★).
 *
 * 오버랩은 각 머신에서 받는다. 범위에 들어온 로컬 플레이어에게는 누구에게나 화면 상호작용 안내를 띄우고
 * (참가자에게는 키 없이 "방장만 …"), F 바인딩은 호스트(리슨 서버의 로컬 컨트롤러)만 한다.
 */
UCLASS()
class DAERUNE_API ADRStageSelectActor : public AActor, public IDRInteractable
{
	GENERATED_BODY()

public:
	ADRStageSelectActor();

	// IDRInteractable — 호스트는 PromptText, 참가자는 키 없이 HostOnlyPromptText
	virtual FText GetInteractionPromptText(const ADRCharacter* Interactor, bool& bOutUsable) const override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UBoxComponent> InteractionBox;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
    TObjectPtr<UStaticMeshComponent> PortalMesh;

    // 목적지 맵 이름
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Portal")
    FString DestinationMapName;

    // 화면 상호작용 안내 — 호스트 (F = 스테이지 선택)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal")
    FText PromptText;

    // 화면 상호작용 안내 — 참가자 (키 없이 이 문구만)
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Portal")
    FText HostOnlyPromptText;

private:
    // 오버랩 이벤트 (각 머신 로컬)
    UFUNCTION()
    void OnBoxBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    void OnBoxEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    // 상호작용 처리
    UFUNCTION()
    void OnHostInteract();

    UFUNCTION(Server, Reliable)
    void ServerRequestTravel();

    // 잡아 둔 컨트롤러의 지금 폰이 범위 안인가 — 나간 폰의 컨트롤러가 이미 떨어져 있어도 놓칠 일이 없다
    bool IsPawnInRange(const ADRPlayerController* Controller) const;

    // 화면 안내 · F 바인딩을 모두 푼다
    void ReleaseLocalController();
    void ReleaseHostController();

    // 범위 안의 로컬 플레이어 (화면 안내용 — 호스트 · 참가자)
    UPROPERTY()
    TObjectPtr<ADRPlayerController> OverlappingLocalController;

    // 범위 안의 호스트 컨트롤러 (F 바인딩)
    UPROPERTY()
    TObjectPtr<ADRPlayerController> OverlappingHostController;
};
