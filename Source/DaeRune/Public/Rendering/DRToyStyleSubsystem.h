// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "DRToyStyleSubsystem.generated.h"

class UMaterialInterface;
class FDRToyStyleViewExtension;

/**
 * 장난감 화풍 — 화면 전체에 포스트 프로세스 머티리얼(MI_PP_ToyStyle)을 씌운다.
 *
 * 맵 · 카메라 · BP 는 건드리지 않는다. 씬 뷰 익스텐션이 게임 화면(PIE · 패키지)과 레벨 에디터 뷰포트에
 * 그 머티리얼을 블렌더블로 붙인다. 씬 캡처(옷장 아이콘 · 미리보기 · 반사 캡처)와 에셋 에디터 미리보기에는 붙이지 않는다.
 *
 *  - 머티리얼은 Scene Color Before DOF 위치라 반투명 이펙트(After DOF)는 그 뒤에 그대로 합성된다.
 *  - 앞에 축소 패스 두 개(LevelPassMaterials)가 화면 전체의 평균 조명을 1/256 텍스처로 줄여 두고, 본 패스가 그 값을 밝기 기준으로 쓴다.
 *    화면 몇 점만 찍어 쓰면 카메라가 움직일 때 기준이 프레임마다 튀어 화면 전체가 깜빡인다.
 *  - 수치는 MI_PP_ToyStyle 에서 고친다 (Strength 0 = 원래 화면, DebugView 로 단계별 확인).
 *  - 콘솔: dr.ToyStyle 0/1 (원래 화면과 비교할 때).
 *  - 그리지 않는 프로세스(데디케이티드 서버 · 커맨드렛 · 쿡)와 bEnabled=False 면 만들어지지 않는다.
 */
UCLASS(Config = Game)
class DAERUNE_API UDRToyStyleSubsystem : public UEngineSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** 씌울 머티리얼(축소 패스들 + 본 패스) — 하나라도 못 읽으면 비운다. 처음 부를 때 한 번 읽어 둔다 (게임 스레드) */
	const TArray<TObjectPtr<UMaterialInterface>>& GetMaterials();

	bool ShouldApplyInEditorViewports() const { return bApplyInEditorViewports; }

	// ===== 설정 — DefaultGame.ini [/Script/DaeRune.DRToyStyleSubsystem] =====

	// false 면 만들지 않는다
	UPROPERTY(Config)
	bool bEnabled = true;

	// 레벨 에디터 뷰포트에도 씌운다 (만드는 화면 = 게임 화면)
	UPROPERTY(Config)
	bool bApplyInEditorViewports = true;

	// 씌울 포스트 프로세스 머티리얼(본 패스). 설정 파일로만 가리키므로 쿡은 DirectoriesToAlwaysCook 에 폴더를 넣어 둔다
	UPROPERTY(Config)
	TSoftObjectPtr<UMaterialInterface> Material;

	// 본 패스가 읽는 축소 패스(UserSceneTexture 출력). 실행 순서는 머티리얼의 Blendable Priority 가 정한다
	UPROPERTY(Config)
	TArray<TSoftObjectPtr<UMaterialInterface>> LevelPassMaterials;

private:
	TSharedPtr<FDRToyStyleViewExtension, ESPMode::ThreadSafe> ViewExtension;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UMaterialInterface>> LoadedMaterials;

	bool bTriedLoad = false;
};
