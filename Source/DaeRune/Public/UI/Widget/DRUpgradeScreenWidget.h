// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRUpgradeScreenWidget.generated.h"

class UDRGameInstance;
class UDRUpgradeUIStyle;

/**
 * 로비 업그레이드 화면의 C++ 베이스. (Plan2.md 21.7 참조)
 *
 * 이 클래스가 책임지는 것은 딱 세 가지다:
 *   1) "지금 보고 있는 로봇"이 무엇인지 판정 (선택한 클래스)
 *   2) GameInstance 델리게이트 구독/해제 → BP 이벤트로 갱신 통지
 *   3) ESC / M 키로 닫기
 *
 * 레이아웃과 그리기는 전부 BP(WBP_UpgradeScreen)가 한다.
 *
 * ★NativeDestruct 의 구독 해제가 특히 중요하다★ — UDRGameInstance 는 레벨 전환에도
 * 파괴되지 않으므로, 해제를 빼먹으면 죽은 위젯이 델리게이트에 남아 다음 진입 때
 * 유령 갱신이나 크래시를 일으킨다.
 */
UCLASS(Abstract)
class DAERUNE_API UDRUpgradeScreenWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// 이 화면이 편집 중인 로봇 (= PlayerState 의 선택 클래스)
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	EPlayerCharacterClass GetViewedClass() const;

	// 진행도 API 진입점. 위젯은 GameInstance 를 직접 캐스팅하지 않고 이걸 쓴다.
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	UDRGameInstance* GetProgression() const;

	// 색/브러시 SSOT (미지정이면 nullptr — BP 가 자체 기본값으로 폴백)
	UFUNCTION(BlueprintPure, Category = "Upgrade")
	UDRUpgradeUIStyle* GetUIStyle() const;

	// 닫기 버튼 / ESC 공통 경로 → PC->CloseUpgradeScreen()
	// (닫을 때 서버로 장착 목록이 재보고된다 — 여기를 우회하면 스탯이 갱신되지 않는다)
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void RequestClose();

	// 슬롯/칩/재화를 한 번에 다시 그린다 (최초 1회 + 변경 알림마다 호출된다)
	UFUNCTION(BlueprintCallable, Category = "Upgrade")
	void RefreshAll();

	/**
	 * 어떤 하위 위젯에서든 자신이 속한 업그레이드 화면을 찾는다.
	 *
	 * ★BP 변수(OwnerScreen)로 주입하지 않는 이유★:
	 * 주입은 "화면이 칩을 만들 때 한 줄 넣기"를 잊으면 조용히 None 이 되고,
	 * 그 사실은 드래그를 실제로 해 봐야 드러난다. 여기서 찾으면 잊을 수가 없다.
	 *
	 * 탐색 순서:
	 *  1) 자신의 Outer 체인 (디자이너에 배치된 슬롯 — 화면의 WidgetTree 안에 있다)
	 *  2) 부모 위젯의 Outer 체인 (런타임 생성된 칩 — Outer 가 PlayerController 라 한 단계 올라가야 한다)
	 */
	UFUNCTION(BlueprintPure, Category = "Upgrade", meta = (DefaultToSelf = "From"))
	static UDRUpgradeScreenWidget* FindOwnerScreen(UWidget* From);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// 좌측 6칸을 다시 그린다 (GetSlotViewModels 사용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade")
	void OnRefreshSlots();

	// 우측 칩 목록을 다시 그린다 (GetChipViewModels* 사용)
	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade")
	void OnRefreshChips();

	UFUNCTION(BlueprintImplementableEvent, Category = "Upgrade")
	void OnCurrencyUpdated(int32 NewCurrency);

private:
	UFUNCTION()
	void HandleUpgradesChanged(EPlayerCharacterClass CharacterClass);

	UFUNCTION()
	void HandleCurrencyChanged(int32 NewCurrency);
};
