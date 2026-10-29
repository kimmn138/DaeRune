// Copyright DaeRune

#include "Animation/DRAnimNotifyState_MeshNiagara.h"

#include "Components/SkeletalMeshComponent.h"
#include "Particles/ParticleSystemComponent.h"

UFXSystemComponent* UDRAnimNotifyState_MeshNiagara::SpawnEffect(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation) const
{
	UFXSystemComponent* Spawned = Super::SpawnEffect(MeshComp, Animation);
	if (Spawned && MeshComp)
	{
		if (bFollowMeshOwnerVisibility)
		{
			Spawned->SetOwnerNoSee(MeshComp->bOwnerNoSee);
			Spawned->SetOnlyOwnerSee(MeshComp->bOnlyOwnerSee);
		}
	}
	return Spawned;
}

void UDRAnimNotifyState_MeshNiagara::NotifyTick(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, float FrameDeltaTime,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyTick(MeshComp, Animation, FrameDeltaTime, EventReference);

	if (!bRepeatWhileActive || bDestroyAtEnd || !MeshComp) return;

	// 1회성 시스템은 끝나면 bAutoDestroy 로 사라진다 → 태그 붙은 자식이 없을 때만 다시 띄운다 (NotifyBegin 과 같은 태그).
	// ★IsActive 로 판단하지 않는다 — 막 만든 컴포넌트는 시스템 준비 전 잠깐 비활성이라 중복으로 띄우게 된다.
	if (GetSpawnedEffect(MeshComp)) return;

	if (UFXSystemComponent* Respawned = SpawnEffect(MeshComp, Animation))
	{
		Respawned->ComponentTags.AddUnique(GetSpawnedComponentTag());
	}
}
