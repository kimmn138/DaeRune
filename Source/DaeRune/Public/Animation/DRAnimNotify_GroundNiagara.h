// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DRAnimNotify_GroundNiagara.generated.h"

class UNiagaraSystem;

/**
 * 애니메이션 프레임에 맞춰 ★땅바닥에★ 나이아가라를 한 번 띄우는 노티파이 (메시에 붙이지 않는다).
 *
 * 두더지 보스의 잠수(AM_Mole_Dig_Hide_Anim → Burrow_NS) · 융기(AM_Mole_Dig_Ambush → Dig_NS) 용.
 *  · 보스는 잠수 몽타주가 끝나면 액터째 숨는다 — 메시에 붙인 이펙트는 같이 사라지므로 월드에 따로 띄운다
 *  · 위치 = 메시 공간 LocationOffset(머리가 땅에 닿는 지점) → 아래로 트레이스해 지면에 붙인다
 *  · Duration > 0 이면 그 시간 뒤 Deactivate — 반복형 시스템이어도 끝이 난다(남은 입자는 자연 소멸)
 * 몽타주는 서버 · 클라 모두에서 재생되므로(Multicast_PlayMoleMontage) 각 머신이 자기 화면에 띄운다.
 */
UCLASS(meta = (DisplayName = "Play Niagara On Ground (DR)"))
class DAERUNE_API UDRAnimNotify_GroundNiagara : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;

	virtual FString GetNotifyName_Implementation() const override;

protected:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	TObjectPtr<UNiagaraSystem> Template;

	/** 메시 공간 오프셋(메시 회전 · 스케일이 적용된다). 머리가 땅에 박히는 지점 등 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	FVector LocationOffset = FVector::ZeroVector;

	/** 액터 Yaw 에 더하는 회전 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	FRotator RotationOffset = FRotator::ZeroRotator;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	FVector Scale = FVector::OneVector;

	/** 아래로 트레이스해 지면(WorldStatic)에 붙인다. 못 찾으면 오프셋 위치 그대로 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara")
	bool bSnapToGround = true;

	/** 트레이스 시작 높이(오프셋 위치 기준 위로) / 길이(아래로) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara", meta = (EditCondition = "bSnapToGround", ClampMin = "0.0"))
	float GroundTraceUp = 300.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara", meta = (EditCondition = "bSnapToGround", ClampMin = "0.0"))
	float GroundTraceDown = 1500.f;

	/** 0 보다 크면 이 시간(초) 뒤 이펙트를 끈다(Deactivate). 0 = 시스템이 스스로 끝날 때까지 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Niagara", meta = (ClampMin = "0.0"))
	float Duration = 0.f;
};
