// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "DRUIClickSoundSubsystem.generated.h"

class FDRUIClickSoundInputProcessor;

/**
 * UI 버튼 클릭음 — 게임 화면의 모든 UMG 버튼(SButton)이 눌릴 때 DA_SoundData 의 UIButtonClickSound 를 낸다.
 *
 * 버튼마다 스타일의 Pressed Sound 를 채우지 않는 이유: 버튼이 든 WBP 가 스무 개 가까이 되고(설정 · 로비 메뉴 플러그인 ·
 * 에디터 도구가 다시 만드는 화면 포함) 하나만 빠뜨려도 조용히 소리가 안 나며, 소리를 바꿀 때마다 전부 다시 고쳐야 한다.
 * 대신 Slate 입력 전처리기에서 "이 입력이 버튼을 누르는가"를 SButton 과 같은 조건으로 판정한다:
 *  - 마우스: 왼쪽 버튼 누름/더블클릭 · 마우스 캡처 없음(게임 조작 중엔 뷰포트가 캡처한다) · 드래그 중 아님 ·
 *    커서 아래 경로(비활성 위젯부터 안쪽은 Slate 가 경로에서 뺀다)에 SButton 이 있음
 *  - 키보드/패드: 확인 키(Enter · Space · 패드 A)를 포커스된 SButton 이 받음
 * 이 게임 인스턴스의 뷰포트 안만 본다 — PIE 중 에디터 UI 나 다른 PIE 창의 클릭은 무시한다.
 *
 * ★버튼 스타일에 Pressed Sound 를 따로 넣으면 이 소리와 겹친다.★
 * 버튼이 아닌 클릭 대상(스테이지 선택 카드 등)은 직접 UDRSoundDataAsset::Play2D(…, UIButtonClickSound) 를 부른다.
 */
UCLASS()
class DAERUNE_API UDRUIClickSoundSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	TSharedPtr<FDRUIClickSoundInputProcessor> InputProcessor;
};
