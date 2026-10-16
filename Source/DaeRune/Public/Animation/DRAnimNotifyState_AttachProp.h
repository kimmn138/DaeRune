// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "DRAnimNotifyState_AttachProp.generated.h"

class UStaticMesh;
class UStaticMeshComponent;

/**
 * ★Plan8(FP)★ 구간 동안 메시 소켓에 소품(스태틱 메시)을 붙였다가 떼는 노티파이 스테이트.
 *
 * 벨보이 FP 스켈레톤에는 Cap 본이 없어서, FP 던지기 몽타주의 시작 ~ 릴리스 프레임 동안
 * 손 소켓에 Bellboy_Cap 을 붙이는 데 쓴다 (Plan8 D-38). 다른 로봇의 소품에도 그대로 쓸 수 있다.
 *
 *  · 소품은 부모 메시의 OnlyOwnerSee / OwnerNoSee 를 따른다 — FP 메시에 붙이면 주인만 본다
 *  · 노티파이 객체는 에셋당 1개라 여러 캐릭터가 공유한다 → 메시별로 소품을 따로 들고 있는다
 *  · 몽타주가 중간에 끊겨도 NotifyEnd 가 와야 소품이 사라진다 — 게임에서 확인할 것 (Plan8 T-35)
 */
UCLASS(meta = (DisplayName = "Attach Prop (DR)"))
class DAERUNE_API UDRAnimNotifyState_AttachProp : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	/** 붙일 소품 (벨보이: Bellboy_Cap) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	TObjectPtr<UStaticMesh> PropMesh;

	/** 붙일 소켓 또는 본 이름 (벨보이 FP: FPCapSocket) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	FName SocketName = FName("FPCapSocket");

	/** 소켓 기준 보정 트랜스폼 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Prop")
	FTransform RelativeTransform;

private:
	// 메시별로 지금 붙어 있는 소품
	TMap<TWeakObjectPtr<USkeletalMeshComponent>, TWeakObjectPtr<UStaticMeshComponent>> ActiveProps;

	void RemoveProp(USkeletalMeshComponent* MeshComp);
};
