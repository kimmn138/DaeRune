// Copyright DaeRune

#include "Animation/DRAnimNotifyState_AttachProp.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"

void UDRAnimNotifyState_AttachProp::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp || !PropMesh) return;
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// 같은 메시에 이미 붙어 있으면(구간 재진입) 먼저 뗀다 — 소품이 쌓이지 않게
	RemoveProp(MeshComp);

	UStaticMeshComponent* Prop = NewObject<UStaticMeshComponent>(Owner, NAME_None, RF_Transient);
	Prop->SetStaticMesh(PropMesh);
	Prop->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Prop->SetGenerateOverlapEvents(false);
	Prop->CastShadow = false;
	Prop->bCastDynamicShadow = false;

	// 부모 메시의 가시성 규칙을 따른다 — FP 메시면 주인만 본다 (D-38)
	Prop->SetOnlyOwnerSee(MeshComp->bOnlyOwnerSee);
	Prop->SetOwnerNoSee(MeshComp->bOwnerNoSee);
	// FP 메시와 같은 이유(뷰모델 depth 압축 WPO)로 bounds 를 맞춘다 — 카메라 회전 시 컬링 pop 방지
	Prop->BoundsScale = MeshComp->BoundsScale;

	Prop->SetupAttachment(MeshComp, SocketName);
	Prop->SetRelativeTransform(RelativeTransform);
	Prop->RegisterComponent();

	ActiveProps.Add(MeshComp, Prop);
}

void UDRAnimNotifyState_AttachProp::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	RemoveProp(MeshComp);

	Super::NotifyEnd(MeshComp, Animation, EventReference);
}

FString UDRAnimNotifyState_AttachProp::GetNotifyName_Implementation() const
{
	return PropMesh ? FString::Printf(TEXT("Prop: %s"), *PropMesh->GetName()) : TEXT("Attach Prop (DR)");
}

void UDRAnimNotifyState_AttachProp::RemoveProp(USkeletalMeshComponent* MeshComp)
{
	TWeakObjectPtr<UStaticMeshComponent> Prop;
	if (ActiveProps.RemoveAndCopyValue(MeshComp, Prop) && Prop.IsValid())
	{
		Prop->DestroyComponent();
	}

	// 사라진 메시(캐릭터 파괴)의 항목도 함께 정리
	for (auto It = ActiveProps.CreateIterator(); It; ++It)
	{
		if (!It->Key.IsValid())
		{
			It.RemoveCurrent();
		}
	}
}
