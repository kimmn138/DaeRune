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

	/** 씌울 머티리얼 — 못 읽으면 null. 처음 부를 때 한 번 읽어 둔다 (게임 스레드) */
	UMaterialInterface* GetMaterial();

	bool ShouldApplyInEditorViewports() const { return bApplyInEditorViewports; }

	// ===== 설정 — DefaultGame.ini [/Script/DaeRune.DRToyStyleSubsystem] =====

	// false 면 만들지 않는다
	UPROPERTY(Config)
	bool bEnabled = true;

	// 레벨 에디터 뷰포트에도 씌운다 (만드는 화면 = 게임 화면)
	UPROPERTY(Config)
	bool bApplyInEditorViewports = true;

	// 씌울 포스트 프로세스 머티리얼. 설정 파일로만 가리키므로 쿡은 DirectoriesToAlwaysCook 에 폴더를 넣어 둔다
	UPROPERTY(Config)
	TSoftObjectPtr<UMaterialInterface> Material;

private:
	TSharedPtr<FDRToyStyleViewExtension, ESPMode::ThreadSafe> ViewExtension;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> LoadedMaterial;

	bool bTriedLoad = false;
};
