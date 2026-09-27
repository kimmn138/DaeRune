// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "Game/DRCosmeticTypes.h"
#include "InputCoreTypes.h"
#include "DRWardrobeScreenWidget.generated.h"

class UDRGameInstance;

/**
 * 로비 옷장 화면의 C++ 베이스. (Plan.md 15.5 참조)
 * UDRUpgradeScreenWidget 의 자매 클래스이며 같은 구조를 따른다.
 *
 * 이 클래스가 책임지는 것:
 *   1) "지금 보고 있는 로봇"과 "지금 보고 있는 카테고리" 판정
 *   2) GameInstance 델리게이트 구독/해제 → BP 이벤트로 갱신 통지
 *   3) 장착 요청 중계 (잠긴 항목 거절 포함)
 *   4) 키 입력 처리 (ESC 닫기 / C 전체 해제)
 *
 * 레이아웃과 그리기는 전부 BP(WBP_Wardrobe)가 한다.
 *
 * ★NativeDestruct 의 구독 해제가 특히 중요하다★ — UDRGameInstance 는 레벨 전환에도
 * 파괴되지 않으므로, 해제를 빼먹으면 죽은 위젯이 델리게이트에 남아 유령 갱신이나 크래시를 일으킨다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRWardrobeScreenWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// 이 화면이 편집 중인 로봇 (= PlayerState 의 선택 클래스)
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	EPlayerCharacterClass GetViewedClass() const;

	// 진행도 API 진입점. 위젯은 GameInstance 를 직접 캐스팅하지 않고 이걸 쓴다.
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	UDRGameInstance* GetProgression() const;

	// 지금 선택된 탭
	UFUNCTION(BlueprintPure, Category = "Cosmetic")
	EDRCosmeticCategory GetCurrentCategory() const { return CurrentCategory; }

	// 탭 전환. 같은 카테고리면 아무 것도 하지 않는다.
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void SetCurrentCategory(EDRCosmeticCategory NewCategory);

	// 닫기 버튼 / ESC 공통 경로 → PC->CloseWardrobeScreen()
	// ★여기를 우회하면 입력 모드가 복구되지 않고 서버로 재보고되지 않는다★
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void RequestClose();

	// 칸 목록 + 탭 선택 표시를 다시 그린다 (최초 1회 + 변경 알림마다 호출된다)
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void RefreshAll();

	/**
	 * 칸 클릭 처리. 성공하면 true.
	 * 잠긴 항목이면 false 를 돌려주고 OnEquipRejected 를 발행한다
	 * (BP 가 흔들림 연출 + 조건 문구를 띄우는 지점 — Plan.md 15.7).
	 */
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	bool TryEquip(FName SkinId);

	// 4개 카테고리 전부 해제 (Clear All 버튼 / C 키)
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void ClearAll();

	// 현재 탭의 칸 목록. 인덱스 0 은 항상 "기본"(장착 해제) 칸이다.
	UFUNCTION(BlueprintCallable, Category = "Cosmetic")
	void GetCurrentSkinViewModels(TArray<FDRSkinViewModel>& OutViewModels) const;

	/**
	 * 어떤 하위 위젯에서든 자신이 속한 옷장 화면을 찾는다.
	 * (UDRUpgradeScreenWidget::FindOwnerScreen 과 같은 탐색 규칙)
	 */
	UFUNCTION(BlueprintPure, Category = "Cosmetic", meta = (DefaultToSelf = "From"))
	static UDRWardrobeScreenWidget* FindOwnerScreen(UWidget* From);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

	// 현재 탭의 칸 목록을 다시 그린다 (GetCurrentSkinViewModels 사용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnRefreshSkins();

	// 탭이 바뀐 직후
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnCategoryChanged(EDRCosmeticCategory NewCategory);

	// 잠긴 항목을 클릭했을 때 (BP: 흔들림 연출 + UnlockHint 표시)
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnEquipRejected(FName SkinId, const FText& UnlockHint);

	// 장착에 성공한 직후.
	// 3D 프리뷰(Plan.md 5.8)가 아직 없으므로 ★지금은 BP 가 이 이벤트로 프리뷰/사운드를 붙인다★.
	// 프리뷰 스테이지가 구현되면 이 자리에서 C++ 이 직접 호출하게 바꾼다.
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnSkinEquipped(FName SkinId);

	// 새 스킨이 해금됐을 때 (BP: 토스트)
	UFUNCTION(BlueprintImplementableEvent, Category = "Cosmetic")
	void OnSkinNewlyUnlocked(FName SkinId);

	// 전체 해제 키. 시안의 [C] 버튼과 같은 동작이다. (Plan.md 15.9)
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic|Input")
	FKey ClearAllKey = EKeys::C;

	// 화면을 여는 순간 보여줄 탭
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cosmetic")
	EDRCosmeticCategory DefaultCategory = EDRCosmeticCategory::Head;

	/**
	 * WBP_Wardrobe 안의 3D 캐릭터 프리뷰 위젯.
	 * ★디자이너가 위젯 이름을 정확히 `Preview_Character` 로 지어야 자동 연결된다★
	 *
	 * Optional 이라 없어도 화면은 정상 동작한다 (프리뷰만 빠진다).
	 */
	UPROPERTY(BlueprintReadOnly, meta = (BindWidgetOptional), Category = "Cosmetic")
	TObjectPtr<class UDRCosmeticPreviewWidget> Preview_Character;

private:
	UFUNCTION()
	void HandleCosmeticsChanged(EPlayerCharacterClass CharacterClass);

	UFUNCTION()
	void HandleSkinUnlocked(FName SkinId);

	/**
	 * WidgetTree 의 모든 UDRWardrobeTabWidget 에 선택 상태를 밀어 넣는다.
	 *
	 * ★BP 가 탭 4개를 일일이 갱신하게 두지 않는 이유★: 탭을 하나 추가하고 갱신 배선을 잊으면
	 * 그 탭만 선택 표시가 안 되는데, 눌러보기 전까지 드러나지 않는다. 여기서 순회하면 잊을 수가 없다.
	 */
	void UpdateTabSelection();

	// 3D 프리뷰에 현재 장착 목록을 밀어 넣는다 (프리뷰 위젯이 없으면 아무 것도 하지 않는다)
	void SyncPreview();

	// 현재 선택된 탭 (런타임 상태 — 저장하지 않는다)
	EDRCosmeticCategory CurrentCategory = EDRCosmeticCategory::Head;
};
