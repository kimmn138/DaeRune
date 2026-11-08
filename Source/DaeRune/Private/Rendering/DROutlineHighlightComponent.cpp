// Copyright DaeRune

#include "Rendering/DROutlineHighlightComponent.h"

#include "Components/MeshComponent.h"
#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UDROutlineHighlightComponent::UDROutlineHighlightComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	SetIsReplicatedByDefault(true);
}

void UDROutlineHighlightComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(UDROutlineHighlightComponent, bHighlighted);
}

void UDROutlineHighlightComponent::BeginPlay()
{
	Super::BeginPlay();

	// 복제가 BeginPlay 보다 먼저 도착했을 수 있다 (서버/클라 공통)
	ApplyAll();
}

void UDROutlineHighlightComponent::SetHighlighted(bool bNewHighlighted)
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || bHighlighted == bNewHighlighted) return;

	bHighlighted = bNewHighlighted;

	// 리슨 서버에서도 보이도록 수동 호출
	OnRep_bHighlighted();
}

void UDROutlineHighlightComponent::OnRep_bHighlighted()
{
	ApplyAll();
}

void UDROutlineHighlightComponent::AddLinkedActor(AActor* Actor)
{
	if (!IsValid(Actor)) return;

	LinkedActors.AddUnique(Actor);
	ApplyTo(Actor, bHighlighted);
}

void UDROutlineHighlightComponent::ApplyAll()
{
	ApplyTo(GetOwner(), bHighlighted);

	for (const TWeakObjectPtr<AActor>& Linked : LinkedActors)
	{
		ApplyTo(Linked.Get(), bHighlighted);
	}
}

void UDROutlineHighlightComponent::ApplyTo(AActor* Actor, bool bOn)
{
	if (!IsValid(Actor)) return;

	Actor->ForEachComponent<UMeshComponent>(false, [bOn](UMeshComponent* Mesh)
	{
		Mesh->SetRenderCustomDepth(bOn);
		Mesh->SetCustomDepthStencilValue(bOn ? StencilValue : 0);
	});
}
