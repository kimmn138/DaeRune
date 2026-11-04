// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRCosmeticTypes.h"
#include "DRCosmeticPreviewStage.generated.h"

class ADRCharacter;
class USceneCaptureComponent2D;
class UStaticMeshComponent;
class UTextureRenderTarget2D;
struct FStreamableHandle;

/**
 * 옷장 UI 안에 3D 캐릭터를 띄우기 위한 "촬영 스튜디오". (Plan(옷장) D-11)
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

	/**
	 * 이 로봇이 입을 수 있는 옷마다 옷장 칸 아이콘을 찍어 둔다 (Plan(옷장) D-33).
	 * 옷 메시 하나만, 이 로봇이 입었을 때의 방향 그대로 찍는다 — 메시를 비동기로 올린 뒤 찍고,
	 * ★새로 찍은 것이 있을 때만★ OnReady 를 부른다(이미 다 찍혀 있으면 부르지 않는다).
	 * 그린 아이콘(PreviewIcon)이 있는 옷 · 스태틱 메시가 없는 옷은 찍지 않는다 — 칸에 이름만 나온다.
	 */
	void PrepareSkinIcons(EPlayerCharacterClass CharacterClass, FSimpleDelegate OnReady);

	// 찍어 둔 아이콘. 아직 안 찍었거나 다른 로봇용으로 찍혀 있으면 nullptr
	UTextureRenderTarget2D* FindSkinIcon(FName SkinId, EPlayerCharacterClass CharacterClass) const;

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

	/**
	 * 로봇별 캡처 카메라 위치 (이 액터 기준 상대 좌표). 표에 없는 로봇은 CaptureComponent 의 기본 위치를 쓴다.
	 * 로봇마다 키 · 들고 있는 물건(정원사 삽 · 벨보이 가방) · 발 높이가 달라서 구도 하나로는
	 * 잘리거나 떠 보인다 (Plan(옷장) D-28).
	 * 맞추는 법: X(거리) — 어느 각도로 돌려도 좌우가 안 잘리게. Z(높이) — 발이 캐릭터 back 글로우의 바닥선(화면 y≈880)에 오게.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic")
	TMap<EPlayerCharacterClass, FVector> CaptureLocationByClass;

	// ========== 옷장 칸 아이콘 (Plan(옷장) D-33) ==========

	// 아이콘으로 찍을 옷 메시. 캐릭터 캡처에는 안 잡힌다(ShowOnlyActors 에 이 액터가 없다)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> IconMeshComponent;

	// 아이콘 카메라. 표시 설정(ShowFlags · 후처리)은 찍을 때마다 CaptureComponent 것을 그대로 가져온다
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneCaptureComponent2D> IconCaptureComponent;

	// 아이콘 한 변(px). 칸에는 168px 로 줄여 그린다 — 밉맵으로 줄이므로 이 정도면 충분하다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic|Icon", meta = (ClampMin = "64", ClampMax = "1024"))
	int32 IconResolution = 256;

	// 아이콘 카메라 화각. 좁을수록 원근 왜곡이 적다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic|Icon", meta = (ClampMin = "5", ClampMax = "90"))
	float IconFOV = 30.0f;

	// 옷이 그림을 채우는 정도 (1 = 가장 긴 쪽이 가장자리에 닿는다)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic|Icon", meta = (ClampMin = "0.1", ClampMax = "1"))
	float IconFill = 0.85f;

	/**
	 * 아이콘 시점 — ★옷이 있는 쪽★(캐릭터 앞 또는 뒤)에서 Pitch(음수 = 내려다봄) · Yaw 만큼 돌린 곳에서 본다.
	 * 앞 · 뒤는 옷의 중심이 캐릭터 중심보다 IconBehindDepth 넘게 뒤에 있는지로 정한다 — 날개 · 꼬리를 앞에서 찍으면
	 * 몸에 붙는 쪽 면만 보인다. 부위별 표에 없으면 DefaultIconViewOffset.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic|Icon")
	FRotator DefaultIconViewOffset = FRotator(-20.0f, 30.0f, 0.0f);

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic|Icon")
	TMap<EDRCosmeticCategory, FRotator> IconViewOffsetByCategory;

	// 옷 중심이 캐릭터 중심보다 이만큼(uu) 넘게 뒤에 있으면 뒤에서 찍는다. 머리 위 옷은 거의 0 이다
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cosmetic|Icon", meta = (ClampMin = "0"))
	float IconBehindDepth = 20.0f;

private:
	void DestroyPreviewCharacter();

	// 로드가 끝난 옷들을 찍어 SkinIcons 에 넣는다. 하나라도 찍었으면 true
	bool RenderSkinIcons(const TArray<FName>& SkinIds);

	UTextureRenderTarget2D* RenderSkinIcon(const FDRSkinDefinition& Skin);

	// 프리뷰 캐릭터가 기본 각도(BaseYaw)로 섰을 때의 액터 변환 — 드래그로 돌린 각도와 무관하다
	FTransform GetRestingActorTransform() const;

	// 이 옷을 이 로봇이 입었을 때의 월드 변환 — 프리뷰 자리에 기본 각도(BaseYaw)로 선 기준(드래그 회전과 무관).
	// 애니메이션이 아닌 참조 포즈의 소켓을 쓴다 — 찍을 때마다 그림이 흔들리지 않게
	FTransform ComputeWornTransform(EPlayerCharacterClass CharacterClass, const FDRSkinAttachSpec& Spec) const;

	UPROPERTY()
	TMap<FName, TObjectPtr<UTextureRenderTarget2D>> SkinIcons;

	// SkinIcons 를 찍은 로봇 — 로봇이 바뀌면 다시 찍는다 (소켓 방향 · 로봇별 덮어쓰기가 다르다)
	EPlayerCharacterClass IconClass = EPlayerCharacterClass::Count;

	TSharedPtr<FStreamableHandle> IconLoadHandle;
	uint32 IconRequestGeneration = 0;

	// ShowOnlyActors 를 배경 + 현재 프리뷰 캐릭터로 다시 채운다
	void RefreshShowOnlyList();

	UPROPERTY()
	TObjectPtr<ADRCharacter> PreviewCharacter;

	// Count = "아직 아무 것도 안 세움"
	EPlayerCharacterClass CurrentPreviewClass = EPlayerCharacterClass::Count;

	// CaptureLocationByClass 에 없는 로봇용 — 처음 세울 때 컴포넌트의 상대 위치를 기억해 둔다
	TOptional<FVector> DefaultCaptureLocation;

	float CurrentYaw = 0.0f;
};
