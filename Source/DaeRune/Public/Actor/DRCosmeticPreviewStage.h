// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRCosmeticPreviewStage.generated.h"

class ADRCharacter;
class USceneCaptureComponent2D;
class UTextureRenderTarget2D;

/**
 * 옷장 UI 안에 3D 캐릭터를 띄우기 위한 "촬영 스튜디오". (Plan.md 5.8 참조)
 *
 * ★UMG 는 SkeletalMeshComponent 를 위젯 트리에 직접 넣을 수 없다★ — 스톡 위젯이 없다.
 * 그래서 맵 밖에 실제 캐릭터를 세우고 SceneCapture 로 찍어 RenderTarget 에 그린 뒤,
 * 그 텍스처를 UMG Image 에 물린다. ★그림이 아니라 매 프레임 갱신되는 진짜 3D 메시다★ —
 * 옷을 갈아입히면 부착물이 실제로 붙고, 회전시키면 액터가 돌고, ABP 아이들도 그대로 돈다.
 *
 * 프리뷰 캐릭터는 ADRLobbyGameMode::SpawnDisplayCharacter 의 레시피를 재사용한다.
 * 차이는 ★로컬 전용(복제 안 함)★ 이라는 것뿐이다.
 *
 * 배치: 로비 맵의 ★맵 밖★ (예: Z = -20000).
 * PRM_UseShowOnlyList 는 캡처만 격리하지 일반 뷰를 가리지 않으므로,
 * 맵 안에 두면 로비를 돌아다니다 프리뷰 캐릭터를 만나게 된다.
 */
UCLASS()
class DAERUNE_API ADRCosmeticPreviewStage : public AActor
{
	GENERATED_BODY()

public:
	ADRCosmeticPreviewStage();

	// 레벨에서 찾는다. 없으면 nullptr — ★위젯은 프리뷰 영역만 숨기고 계속 동작해야 한다★
	// (레벨에 액터를 안 놓았다고 옷장 기능 전체가 죽으면 안 된다)
	static ADRCosmeticPreviewStage* Find(const UObject* WorldContext);

	// 이 로봇을 프리뷰에 세운다. 같은 클래스면 재스폰하지 않는다.
	void SetPreviewClass(EPlayerCharacterClass CharacterClass);

	// ★실시간 옷 갈아입히기★ — 순수 로컬이라 서버 왕복이 0이다
	void SetPreviewSkins(const TArray<FName>& SkinIds);

	// 드래그 회전 (턴테이블)
	void AddPreviewYaw(float DeltaYaw);

	// 기본 각도로 되돌린다
	void ResetPreviewYaw();

	// 화면 열림/닫힘에 맞춰 캡처를 켜고 끈다. 상시 캡처는 로비 프레임을 그냥 깎아먹는다.
	void SetCaptureActive(bool bActive);

	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	ADRCharacter* GetPreviewCharacter() const { return PreviewCharacter; }

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> SceneRoot;

	// 캐릭터가 설 자리. 에디터에서 옮겨 프레이밍을 잡는다.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> PreviewSpawnPoint;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneCaptureComponent2D> CaptureComponent;

	// BP(BP_CosmeticPreviewStage)에서 RT_CosmeticPreview 를 지정한다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic")
	TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	// 캡처에 함께 담을 배경/소품 액터 (선택). 비워두면 캐릭터만 찍힌다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic")
	TArray<TObjectPtr<AActor>> BackdropActors;

	// 프리뷰 캐릭터의 기본 방향(요). 캡처 카메라를 향하도록 에디터에서 맞춘다.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic")
	float BaseYaw = 0.0f;

private:
	void DestroyPreviewCharacter();

	// ShowOnlyActors 를 배경 + 현재 프리뷰 캐릭터로 다시 채운다
	void RefreshShowOnlyList();

	UPROPERTY()
	TObjectPtr<ADRCharacter> PreviewCharacter;

	// Count = "아직 아무 것도 안 세움"
	EPlayerCharacterClass CurrentPreviewClass = EPlayerCharacterClass::Count;

	float CurrentYaw = 0.0f;
};
