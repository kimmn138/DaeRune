// Copyright DaeRune


#include "Character/DRFacialExpressionComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Components/SkeletalMeshComponent.h"

UDRFacialExpressionComponent::UDRFacialExpressionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UDRFacialExpressionComponent::InitializeFaceMaterial(USkeletalMeshComponent* Mesh)
{
	if (!Mesh) return;
	TargetMesh = Mesh;

	// 표정 텍스처 맵 초기화
	ExpressionTextures.Add(EFacialExpression::Default, DefaultTexture);
	ExpressionTextures.Add(EFacialExpression::Blink, BlinkTexture);
	ExpressionTextures.Add(EFacialExpression::Heat, HeatTexture);
	ExpressionTextures.Add(EFacialExpression::Death, DeathTexture);
	if (IncreasedAttackSpeedTexture)
	{
		ExpressionTextures.Add(EFacialExpression::IncreasedAttackSpeed, IncreasedAttackSpeedTexture);
	}

	// 기존 머티리얼에서 MID 생성
	UMaterialInterface* BaseMaterial = Mesh->GetMaterial(FaceMaterialIndex);
	if (BaseMaterial)
	{
		FaceDynamicMaterial = UMaterialInstanceDynamic::Create(BaseMaterial, this);
		Mesh->SetMaterial(FaceMaterialIndex, FaceDynamicMaterial);
	}

	// Default 텍스처 적용
	ApplyTexture(DefaultTexture);

	// Blink 타이머 시작
	StartBlinkTimer();
}

void UDRFacialExpressionComponent::SetExpression(EFacialExpression Expression, float Duration)
{
	if (bIsDead) return;

	// 우선순위 비교: 현재보다 낮으면 무시 (Default 상태는 항상 덮어쓸 수 있음)
	const int32 NewPriority = static_cast<int32>(Expression);
	const int32 CurPriority = static_cast<int32>(CurrentExpression);
	if (NewPriority < CurPriority && CurrentExpression != EFacialExpression::Default)
	{
		return;
	}

	// 기존 표정 타이머 취소 (새 표정으로 교체하므로)
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpressionTimerHandle);
	}

	// 표정 텍스처 적용
	CurrentExpression = Expression;
	UTexture* Texture = GetTextureForExpression(Expression);
	ApplyTexture(Texture);

	// Duration > 0이면 해당 시간 후 Default로 복귀
	if (Duration > 0.f && Expression != EFacialExpression::Death)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				ExpressionTimerHandle,
				this,
				&UDRFacialExpressionComponent::OnExpressionTimerExpired,
				Duration,
				false
			);
		}
	}
}

void UDRFacialExpressionComponent::OnDeath()
{
	bIsDead = true;

	// 모든 타이머 정지
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpressionTimerHandle);
		World->GetTimerManager().ClearTimer(BlinkTimerHandle);
	}

	// Death 표정 영구 적용
	CurrentExpression = EFacialExpression::Death;
	ApplyTexture(GetTextureForExpression(EFacialExpression::Death));
}

void UDRFacialExpressionComponent::OnHitReact()
{
	SetExpression(EFacialExpression::Heat, HeatDuration);
}

void UDRFacialExpressionComponent::RevertToDefault()
{
	if (bIsDead) return;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpressionTimerHandle);
	}

	CurrentExpression = EFacialExpression::Default;
	ApplyTexture(GetTextureForExpression(EFacialExpression::Default));
}

void UDRFacialExpressionComponent::SetExpressionTexture(EFacialExpression Expression, UTexture* Texture)
{
	if (Texture)
	{
		ExpressionTextures.Add(Expression, Texture);
	}
}

void UDRFacialExpressionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ExpressionTimerHandle);
		World->GetTimerManager().ClearTimer(BlinkTimerHandle);
	}

	Super::EndPlay(EndPlayReason);
}

void UDRFacialExpressionComponent::ApplyTexture(UTexture* Texture)
{
	if (FaceDynamicMaterial && Texture)
	{
		FaceDynamicMaterial->SetTextureParameterValue(FaceTextureParameterName, Texture);
	}
}

void UDRFacialExpressionComponent::StartBlinkTimer()
{
	if (bIsDead) return;

	UWorld* World = GetWorld();
	if (!World) return;

	const float NextInterval = FMath::RandRange(BlinkIntervalMin, BlinkIntervalMax);
	World->GetTimerManager().SetTimer(
		BlinkTimerHandle,
		this,
		&UDRFacialExpressionComponent::TryBlink,
		NextInterval,
		false
	);
}

void UDRFacialExpressionComponent::TryBlink()
{
	if (bIsDead) return;

	// Default 상태일 때만 Blink 허용
	if (CurrentExpression == EFacialExpression::Default)
	{
		if (FMath::FRand() < BlinkChance)
		{
			SetExpression(EFacialExpression::Blink, BlinkDuration);
		}
	}

	// 다음 Blink 시도 스케줄링
	StartBlinkTimer();
}

void UDRFacialExpressionComponent::OnExpressionTimerExpired()
{
	CurrentExpression = EFacialExpression::Default;
	ApplyTexture(GetTextureForExpression(EFacialExpression::Default));
}

UTexture* UDRFacialExpressionComponent::GetTextureForExpression(EFacialExpression Expression) const
{
	if (const TObjectPtr<UTexture>* Found = ExpressionTextures.Find(Expression))
	{
		return *Found;
	}
	return nullptr;
}
