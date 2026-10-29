// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AnimNotifyState_TimedNiagaraEffect.h"
#include "DRAnimNotifyState_MeshNiagara.generated.h"

/**
 * 엔진 "Timed Niagara Effect" 에 두 가지를 더한 노티파이 스테이트.
 *  ① 만든 이펙트가 부모 메시의 OwnerNoSee / OnlyOwnerSee 를 따른다 — 3인칭 몸(OwnerNoSee)에 붙인 몸 이펙트가
 *     1인칭 본인 화면을 가리지 않는다. 다른 사람 화면에는 그대로 보인다.
 *  ② bRepeatWhileActive — 1회성 시스템이 구간보다 짧게 끝나면 구간이 끝날 때까지 다시 띄운다.
 *
 * ★SocketName 을 반드시 지정한다★ — 엔진 부모가 비어 있는 소켓을 무효로 보고 아무것도 띄우지 않는다.
 * 벨보이 도발 몸 이펙트(AM_BB_Taunt, 판정 시작 노티파이 ~ 몽타주 끝, Body_002) 용. 몽타주가 끊기면 NotifyEnd 로 같이 꺼진다.
 */
UCLASS(meta = (DisplayName = "Timed Niagara Effect (Mesh Visibility, DR)"))
class DAERUNE_API UDRAnimNotifyState_MeshNiagara : public UAnimNotifyState_TimedNiagaraEffect
{
	GENERATED_BODY()

public:
	virtual void NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
		const FAnimNotifyEventReference& EventReference) override;

protected:
	/** 시스템이 구간 안에서 끝나면(1회성) 다시 띄운다. Destroy Immediately 를 켜면 동작하지 않는다(끝난 컴포넌트가 남아서) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = NiagaraSystem)
	bool bRepeatWhileActive = true;

	/** 켜면 부모 메시의 OwnerNoSee / OnlyOwnerSee 를 따른다(3인칭 몸이면 본인 화면에서 숨는다). 끄면 모두에게 보인다 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = NiagaraSystem)
	bool bFollowMeshOwnerVisibility = true;

	virtual UFXSystemComponent* SpawnEffect(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) const override;
};
