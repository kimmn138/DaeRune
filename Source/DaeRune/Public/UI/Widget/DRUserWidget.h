// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRUserWidget.generated.h"

/**
 * UDRUserWidget
 *
 * 위젯 컨트롤러와 연결되는 사용자 위젯 클래스임
 */
UCLASS()
class DAERUNE_API UDRUserWidget : public UUserWidget
{
	GENERATED_BODY()
	
public:
	UFUNCTION(BlueprintCallable)
	void SetWidgetController(UObject* InWidgetController); // 위젯 컨트롤러 설정 함수임

	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UObject> WidgetController; // 연결된 위젯 컨트롤러 참조임

protected:
	UFUNCTION(BlueprintImplementableEvent)
	void WidgetControllerSet(); // 컨트롤러 설정 완료 이벤트임
};
