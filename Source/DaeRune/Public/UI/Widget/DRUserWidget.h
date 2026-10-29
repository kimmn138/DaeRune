// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRUserWidget.generated.h"

/**
 * DaeRune 기본 UI 위젯 클래스
 */
UCLASS()
class DAERUNE_API UDRUserWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	// WidgetController 설정
	UFUNCTION(BlueprintCallable)
	void SetWidgetController(UObject* InWidgetController);

	// 연결된 WidgetController 참조
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UObject> WidgetController;

protected:
	// WidgetController 설정 완료 시 블루프린트에서 구현할 이벤트
	UFUNCTION(BlueprintImplementableEvent)
	void WidgetControllerSet();

	/** C++ 하위 위젯용 — BP 의 WidgetControllerSet 보다 먼저 불린다. */
	virtual void NativeWidgetControllerSet() {}

	//~ 언어(컬처) 변경 시 이 위젯의 표시 텍스트를 즉시 갱신하기 위한 훅.
	//  SettingsManager의 OnLanguageChanged를 구독/해제한다(NativeConstruct/Destruct 쌍).
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	/** 언어가 바뀐 직후 호출. BP 위젯은 이 이벤트에서 코드로 세팅한 동적 텍스트를 다시 SetText 하면 된다. */
	UFUNCTION(BlueprintImplementableEvent, Category = "Localization")
	void OnLanguageChanged();

private:
	/** OnLanguageChanged 구독 핸들러: 디자이너에 박힌 정적 LOCTEXT 텍스트를 새 컬처로 다시 밀어넣고 BP 이벤트를 발행. */
	UFUNCTION()
	void HandleLanguageChanged(const FString& CultureCode);
};
