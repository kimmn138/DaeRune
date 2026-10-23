// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRStageSelectTypes.h"
#include "DRStageSelectScreenWidget.generated.h"

class UDRGameInstance;
class UDRStageSelectCardWidget;
class UPanelWidget;
class UTextBlock;
class UButton;

/**
 * 로비 스테이지 선택 화면의 C++ 베이스. (시안: 스테이지 선택.html)
 * UDRWardrobeScreenWidget 과 같은 구조 — ★위젯 생성/제거는 ADRPlayerController 가 한다★
 *
 * 이 클래스가 책임지는 것 (시안의 동작 전부):
 *   1) 카드 목록 그리기 — Box_Cards + CardWidgetClass 가 있으면 C++ 이 카드를 만든다
 *   2) 선택 / 선택 취소 / 잠긴 카드 거절(흔들림 + 경고 문구)
 *   3) 하단 안내 문구 — 호버 · 선택 · 거절 · 취소에 따라 바뀐다 (경고면 경고색)
 *   4) 시작 버튼 활성/비활성, 더블클릭으로 바로 시작
 *   5) 키 입력 — ←/→ 카드 이동, Enter 선택·시작, ESC 선택 취소·닫기 (게임패드 대응 포함)
 *   6) 상단 "클리어 X / N"
 *
 * 디자이너 위젯 (이름 고정, 전부 Optional):
 *   Box_Cards        [Panel]   카드가 들어갈 패널. HorizontalBox 면 카드가 같은 폭으로 나눠 갖는다
 *   Txt_Message      [Text]    하단 안내 문구
 *   Txt_ClearCount   [Text]    클리어한 스테이지 수
 *   Txt_StageCount   [Text]    전체 스테이지 수
 *   Btn_Start        [Button]  시작 (Enter)
 *   Btn_Back         [Button]  뒤로 (ESC)
 *
 * 레이아웃 · 브러시는 BP(WBP_StageSelect)가 한다. BP 그래프 배선은 필요 없다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRStageSelectScreenWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "StageSelect")
	UDRGameInstance* GetProgression() const;

	// 카드 목록 + 선택 표시 + 카운터 + 버튼 상태를 다시 그린다
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	void RefreshAll();

	/**
	 * 카드 선택. 성공하면 true.
	 * 잠긴 카드면 false — 카드가 흔들리고 경고 문구가 뜨며 OnSelectRejected 가 발행된다.
	 */
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	bool SelectStage(int32 Index);

	// 선택 취소 (문구: "선택이 취소되었습니다.")
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	void ClearSelection();

	// 선택된 스테이지로 출발. 선택이 없거나 이미 출발 중이면 false.
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	bool StartSelectedStage();

	// 뒤로 버튼 / ESC — 선택이 있으면 선택 취소, 없으면 화면을 닫는다
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	void RequestBack();

	// 닫기는 반드시 여기(→ PC->CloseStageSelectScreen)를 거친다. ★우회하면 입력 모드가 복구되지 않는다★
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	void RequestClose();

	// 키보드/게임패드 카드 이동 (Delta = -1 / +1, 끝에서 반대쪽으로 순환)
	UFUNCTION(BlueprintCallable, Category = "StageSelect")
	void MoveFocus(int32 Delta);

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
	void NotifyCardUnhovered(int32 Index);
	void NotifyCardClicked(int32 Index);
	void NotifyCardDoubleClicked(int32 Index);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 카드 목록을 다시 그린 직후. ★카드 생성은 C++ 이 이미 했다★ — 연출만 붙인다.
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnRefreshStages();

	// 선택이 바뀐 직후 (Index == INDEX_NONE 이면 선택 취소)
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnSelectionChanged(int32 NewIndex);

	// 잠긴 카드를 고르려 했을 때
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnSelectRejected(int32 Index, const FText& LockHint);

	// 출발 직전 (BP: 시작 사운드 · 페이드). 이동은 C++ 이 한다.
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnStageStarting(FName StageId);

	// 하단 문구가 바뀐 직후 (Txt_Message 는 C++ 이 이미 갱신했다)
	UFUNCTION(BlueprintImplementableEvent, Category = "StageSelect")
	void OnMessageChanged(const FText& Message, bool bIsWarning);

	// 카드 위젯 클래스 (WBP_StageSelectCard)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect")
	TSubclassOf<UDRStageSelectCardWidget> CardWidgetClass;

	// HorizontalBox 일 때 카드 사이 간격 (시안 gap 64px)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect", meta = (ClampMin = "0"))
	float CardSpacing = 64.0f;

	// 화면을 열 때 키보드 포커스를 둘 카드. -1 이면 "진행 가능" 카드 중 첫 번째 (없으면 0번)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect")
	int32 InitialFocusIndex = -1;

	// 하단 문구 색 (시안 --sub / --warn)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	FLinearColor MessageColor = FLinearColor(FColor(0x9D, 0xB9, 0xD6));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "StageSelect|Style")
	FLinearColor WarningMessageColor = FLinearColor(FColor(0xFF, 0x8A, 0x8A));

	// ===== 디자이너 위젯 (이름 고정) =====

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UPanelWidget> Box_Cards;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_Message;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_ClearCount;

	UPROPERTY(BlueprintReadOnly, Category = "StageSelect|Widgets", meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> Txt_StageCount;

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
	void UpdateCounters();
	void UpdateButtons();

	void SetMessage(const FText& Message, bool bIsWarning = false);

	// 선택 상태에 맞는 기본 문구 ("스테이지를 선택하세요." / "스테이지 N 선택됨 ...")
	void ShowIdleMessage();

	// 카드 강조 시 문구 (잠김 경고 / 클리어 / 진행 가능)
	void ShowCardMessage(int32 Index);

	void ShowLockedMessage(int32 Index);

	bool IsValidIndex(int32 Index) const { return ViewModels.IsValidIndex(Index); }

	// 마지막으로 그린 목록 (GameInstance 가 만든 것)
	TArray<FDRStageSelectViewModel> ViewModels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UDRStageSelectCardWidget>> Cards;

	// 선택된 카드 (런타임 상태 — 저장하지 않는다). 다시 그려도 StageId 로 복원한다.
	int32 SelectedIndex = INDEX_NONE;

	// 키보드 포커스 카드 (←/→)
	int32 FocusedIndex = INDEX_NONE;

	// 출발 요청을 보냈다 — 이동 중 중복 입력 방지
	bool bStarting = false;
};
