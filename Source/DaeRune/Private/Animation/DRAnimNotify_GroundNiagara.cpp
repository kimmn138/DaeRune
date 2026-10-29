// Copyright DaeRune

#include "Animation/DRAnimNotify_GroundNiagara.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TimerManager.h"

void UDRAnimNotify_GroundNiagara::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	UWorld* World = MeshComp ? MeshComp->GetWorld() : nullptr;
	if (!Template || !World || World->GetNetMode() == NM_DedicatedServer) return;

	FVector Location = MeshComp->GetComponentTransform().TransformPosition(LocationOffset);

	AActor* Owner = MeshComp->GetOwner();
	if (bSnapToGround)
	{
		FCollisionQueryParams Params(SCENE_QUERY_STAT(DRGroundNiagara), false, Owner);
		FHitResult Hit;
		if (World->LineTraceSingleByObjectType(Hit, Location + FVector(0.f, 0.f, GroundTraceUp),
			Location - FVector(0.f, 0.f, GroundTraceDown), FCollisionObjectQueryParams(ECC_WorldStatic), Params))
		{
			Location = Hit.ImpactPoint;
		}
	}

	const float Yaw = Owner ? Owner->GetActorRotation().Yaw : MeshComp->GetComponentRotation().Yaw;
	const FRotator Rotation = FRotator(0.f, Yaw, 0.f) + RotationOffset;

	UNiagaraComponent* Spawned = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
		World, Template, Location, Rotation, Scale, /*bAutoDestroy*/ true);

	if (Spawned && Duration > 0.f)
	{
		TWeakObjectPtr<UNiagaraComponent> WeakSpawned = Spawned;
		FTimerHandle Handle;
		World->GetTimerManager().SetTimer(Handle, FTimerDelegate::CreateLambda([WeakSpawned]()
		{
			if (UNiagaraComponent* Comp = WeakSpawned.Get())
			{
				Comp->Deactivate();   // 남은 입자는 끝까지 살고 bAutoDestroy 로 정리된다
			}
		}), Duration, false);
	}
}

FString UDRAnimNotify_GroundNiagara::GetNotifyName_Implementation() const
{
	return Template ? FString::Printf(TEXT("Ground FX: %s"), *Template->GetName()) : TEXT("Ground FX");
}
