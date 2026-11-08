// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DROutlineHighlightComponent.generated.h"

/**
 * 외곽선 강조 — "이것이 조작 대상이다"를 물체의 가장자리만 빛내서 알린다
 *
 * 켜면 주인 액터와 연결한 액터(퍼즐 레버 · 금고 버튼 등)의 메시를 커스텀 뎁스에 StencilValue 로 그린다.
 * 그 스텐실의 바깥 가장자리만 맵의 포스트 프로세스 볼륨에 넣은 MI_PP_InteractOutline 이 칠한다
 * (Content/Blueprints/VFX/InteractOutline/) — 볼륨이 없는 맵에서는 아무것도 보이지 않는다.
 *
 * 켜고 끄기는 서버(SetHighlighted)가 하고, 복제된 값으로 각 머신이 자기 메시에 적용한다.
 */
UCLASS(ClassGroup = (DaeRune), meta = (BlueprintSpawnableComponent))
class DAERUNE_API UDROutlineHighlightComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// MI_PP_InteractOutline 이 찾는 스텐실 값. 적 웨이브 외곽선(ADREnemy, 1~5)과 겹치지 않게 둔다
	static constexpr int32 StencilValue = 10;

	UDROutlineHighlightComponent();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 외곽선 켜기/끄기 (서버)
	UFUNCTION(BlueprintCallable, Category = "Outline")
	void SetHighlighted(bool bNewHighlighted);

	UFUNCTION(BlueprintCallable, Category = "Outline")
	bool IsHighlighted() const { return bHighlighted; }

	// 주인과 함께 외곽선을 그릴 액터. 각 머신이 그 액터의 BeginPlay 에서 등록하며, 현재 상태를 바로 적용한다
	void AddLinkedActor(AActor* Actor);

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY(ReplicatedUsing = OnRep_bHighlighted)
	bool bHighlighted = false;

	UFUNCTION()
	void OnRep_bHighlighted();

	// 주인 + 연결한 액터 전부에 현재 상태를 적용한다
	void ApplyAll();

	static void ApplyTo(AActor* Actor, bool bOn);

	TArray<TWeakObjectPtr<AActor>> LinkedActors;
};
