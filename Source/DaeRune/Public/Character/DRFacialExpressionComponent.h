// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DRFacialExpressionComponent.generated.h"

/**
 * 캐릭터 표정 상태
 * 값이 곧 우선순위 (높을수록 우선)
 */
UENUM(BlueprintType)
enum class EFacialExpression : uint8
{
	Default              = 0,
	Blink                = 1,
	IncreasedAttackSpeed = 2,
	Heat                 = 3,
	Death                = 4
};

/**
 * 플레이어 캐릭터의 얼굴 머티리얼 텍스처를 상황에 따라 동적으로 교체하는 컴포넌트.
 * 3P 메시의 특정 머티리얼 슬롯에 MID를 생성하여 텍스처 파라미터를 변경한다.
 */
UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class DAERUNE_API UDRFacialExpressionComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UDRFacialExpressionComponent();

	/** 대상 메시의 얼굴 머티리얼을 MID로 교체하고 Blink 타이머를 시작한다 */
	UFUNCTION(BlueprintCallable, Category = "Facial Expression")
	void InitializeFaceMaterial(USkeletalMeshComponent* Mesh);

	/** 표정 변경 (Duration <= 0이면 영구 적용) */
	UFUNCTION(BlueprintCallable, Category = "Facial Expression")
	void SetExpression(EFacialExpression Expression, float Duration = -1.f);

	/** 사망 시 호출 - Death 표정으로 영구 전환 + Blink 타이머 정지 */
	UFUNCTION(BlueprintCallable, Category = "Facial Expression")
	void OnDeath();

	/** HitReact 시 호출 - Heat 표정으로 HeatDuration초간 전환 */
	UFUNCTION(BlueprintCallable, Category = "Facial Expression")
	void OnHitReact();

	/** Default 표정으로 복귀 */
	UFUNCTION(BlueprintCallable, Category = "Facial Expression")
	void RevertToDefault();

	/** 런타임에 표정 텍스처를 설정/변경 (BP에서 추가 표정 등록용) */
	UFUNCTION(BlueprintCallable, Category = "Facial Expression")
	void SetExpressionTexture(EFacialExpression Expression, UTexture* Texture);

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ========== 에디터 설정 ==========

	/** 메시에서 얼굴 머티리얼이 위치한 슬롯 인덱스 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression")
	int32 FaceMaterialIndex = 0;

	/** 머티리얼 내 텍스처 파라미터 이름 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression")
	FName FaceTextureParameterName = FName("FaceTexture");

	// ========== 표정 텍스처 ==========

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression|Textures")
	TObjectPtr<UTexture> DefaultTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression|Textures")
	TObjectPtr<UTexture> BlinkTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression|Textures")
	TObjectPtr<UTexture> HeatTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression|Textures")
	TObjectPtr<UTexture> DeathTexture;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression|Textures")
	TObjectPtr<UTexture> IncreasedAttackSpeedTexture;

	// ========== Blink 설정 ==========

	/** Blink 시도 간격 최소값 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression|Blink")
	float BlinkIntervalMin = 5.f;

	/** Blink 시도 간격 최대값 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression|Blink")
	float BlinkIntervalMax = 10.f;

	/** Blink 발생 확률 (0~1) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression|Blink", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BlinkChance = 0.3f;

	/** Blink 유지 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression|Blink")
	float BlinkDuration = 2.f;

	// ========== HitReact 설정 ==========

	/** Heat(피격) 표정 유지 시간 (초) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Facial Expression|HitReact")
	float HeatDuration = 1.f;

private:
	// 현재 활성 표정
	EFacialExpression CurrentExpression = EFacialExpression::Default;

	// 사망 상태 (이후 표정 변경 차단)
	bool bIsDead = false;

	// MID 캐시
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> FaceDynamicMaterial;

	// 대상 메시 캐시
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> TargetMesh;

	// 표정별 텍스처 맵 (런타임 조회용)
	UPROPERTY()
	TMap<EFacialExpression, TObjectPtr<UTexture>> ExpressionTextures;

	// 타이머 핸들
	FTimerHandle ExpressionTimerHandle;
	FTimerHandle BlinkTimerHandle;

	// 내부 함수
	void ApplyTexture(UTexture* Texture);
	void StartBlinkTimer();
	void TryBlink();
	void OnExpressionTimerExpired();
	UTexture* GetTextureForExpression(EFacialExpression Expression) const;
};
