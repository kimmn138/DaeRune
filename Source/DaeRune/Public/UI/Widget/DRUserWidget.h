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
};
