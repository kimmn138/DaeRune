// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRStageSelectTypes.h"
#include "DRStageSelectScreenWidget.generated.h"

class UDRGameInstance;
class UDRStageSelectCardWidget;
class UPanelWidget;
class UButton;

/**
 * 로비 스테이지 선택 화면의 C++ 베이스. (아트: Figma "대룬 ui > 스테이지")
 * UDRWardrobeScreenWidget 과 같은 구조 — ★위젯 생성/제거는 ADRPlayerController 가 한다★
 *
 * 선택 규칙 (시안의 세 화면 모두 "선택된 카드 하나" 가 있다):
 *   - 열면 카드 하나가 이미 선택돼 있다 — InitialSelectedIndex, 없으면 아직 안 깬 첫 스테이지, 다 깼으면 마지막 스테이지
 *   - ←/→ · 마우스 올리기 · 클릭으로 선택을 옮긴다. 잠긴 카드는 건너뛰고, 누르면 흔들린다
 *   - Enter(또는 START 클릭 · 카드 더블클릭)로 선택한 스테이지로 출발, ESC(또는 BACK 클릭)로 닫는다
 *
 * 디자이너 위젯 (이름 고정, 전부 Optional):
 *   Box_Cards  [Panel]   카드가 들어갈 패널. HorizontalBox 면 카드를 아래쪽에 맞춰 CardSpacing 간격으로 늘어놓는다
 *   Btn_Start  [Button]  "ENTER START" 키 안내 (클릭하면 출발)
 *   Btn_Back   [Button]  "ESC BACK" 키 안내 (클릭하면 닫기)
 *
 * 레이아웃 · 브러시는 WBP_StageSelect 가 한다 (DRStageSelectEditorTools 가 짓는다). BP 그래프 배선은 필요 없다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRStageSelectScreenWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "StageSelect")
	UDRGameInstance* GetProgression() const;

	// 카드 목록 + 선택 표시 + 버튼 상태를 다시 그린다
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	void RefreshAll();

	/**
	 * 카드 선택. 성공하면 true.
	 * 잠긴 카드면 false — 카드가 흔들리고 OnSelectRejected 가 발행된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	bool SelectStage(int32 Index);

	// 선택된 스테이지로 출발. 선택이 없거나 이미 출발 중이면 false.
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	bool StartSelectedStage();

	// 닫기는 반드시 여기(→ PC->CloseStageSelectScreen)를 거친다. ★우회하면 입력 모드가 복구되지 않는다★
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	void RequestClose();

	// 키보드/게임패드 선택 이동 (Delta = -1 / +1). 잠긴 카드는 건너뛰고, 끝에서 반대쪽으로 순환한다.
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	void MoveSelection(int32 Delta);

	UFUNCTION(BlueprintPure, Category = "StageSelect")
	int32 GetSelectedIndex() const { return SelectedIndex; }

	UFUNCTION(BlueprintPure, Category = "StageSelect")
	bool HasSelection() const;

	UFUNCTION(BlueprintPure, Category = "StageSelect")
	int32 GetClearedCount() const;

	UFUNCTION(BlueprintPure, Category = "StageSelect")
	int32 GetStageCount() const { return ViewModels.Num(); }

	UFUNCTION(BlueprintPure, Category = "StageSelect")
	const TArray<FDRStageSelectViewModel>& GetViewModels() const { return ViewModels; }

	// ===== 카드가 부른다 =====

	void NotifyCardHovered(int32 Index);
	void NotifyCardClicked(int32 Index);
	void NotifyCardDoubleClicked(int32 Index);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 카드 목록을 다시 그린 직후. ★카드 생성은 C++ 이 이미 했다★ — 연출만 붙인다.
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnRefreshStages();

	// 선택이 바뀐 직후 (BP: 선택 사운드 등)
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnSelectionChanged(int32 NewIndex);

	// 잠긴 카드를 고르려 했을 때 (LockHint = "스테이지 1 클리어 후 열립니다.")
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnSelectRejected(int32 Index, const FText& LockHint);

	// 출발 직전 (BP: 시작 사운드 · 페이드). 이동은 C++ 이 한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnStageStarting(FName StageId);

	// 카드 위젯 클래스 (WBP_StageSelectCard)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect")
	TSubclassOf<UDRStageSelectCardWidget> CardWidgetClass;

	// HorizontalBox 일 때 카드 사이 간격 (Figma 23px)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect", meta = (ClampMin = "0"))
	float CardSpacing = 23.0f;

	// 화면을 열 때 선택해 둘 카드. -1 이면 아직 안 깬 첫 스테이지 (다 깼으면 마지막 스테이지)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect")
	int32 InitialSelectedIndex = -1;

	// ===== 디자이너 위젯 (이름 고정) =====

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Box_Cards;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Start;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UButton> Btn_Back;

private:
	UFUNCTION()
	void HandleStatesChanged();

	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleBackClicked();

	void RebuildCards();
	void UpdateCardStates();
	void UpdateButtons();

	// 열 때 / 선택이 잠겨 버렸을 때 고를 카드
	int32 PickDefaultSelection() const;

	bool IsValidIndex(int32 Index) const { return ViewModels.IsValidIndex(Index); }
	bool IsSelectable(int32 Index) const { return IsValidIndex(Index) && ViewModels[Index].IsSelectable(); }

	// 마지막으로 그린 목록 (GameInstance 가 만든 것)
	TArray<FDRStageSelectViewModel> ViewModels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDRStageSelectCardWidget>> Cards;

	// 선택된 카드 (런타임 상태 — 저장하지 않는다). 다시 그려도 StageId 로 복원한다.
	int32 SelectedIndex = INDEX_NONE;

	// 출발 요청을 보냈다 — 이동 중 중복 입력 방지
	bool bStarting = false;
};
