// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "DRHUD.generated.h"

class UAttributeSet;
class UAbilitySystemComponent;
class UOverlayWidgetController;
class UDRUserWidget;
struct FWidgetControllerParams;

/**
 * ADRHUD
 *
 * 게임 오버레이 HUD 관리 클래스 정의문서
 */
UCLASS()
class DAERUNE_API ADRHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	/**
	 * GetOverlayWidgetController
	 *
	 * 오버레이 위젯 컨트롤러 반환 및 초기화 함수임
	 */
	UOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);

	/**
	 * InitOverlay
	 *
	 * HUD 오버레이 위젯 및 컨트롤러 초기화 함수임
	 */
	void InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

protected:

private:
	/**
	 * OverlayWidget
	 *
	 * 생성된 오버레이 위젯 인스턴스 참조임
	 */
	UPROPERTY()
	TObjectPtr<UDRUserWidget>  OverlayWidget;

	/**
	 * OverlayWidgetClass
	 *
	 * BP에서 설정된 오버레이 위젯 클래스 타입임
	 */
	UPROPERTY(EditAnywhere)
	TSubclassOf<UDRUserWidget> OverlayWidgetClass;

	/**
	 * OverlayWidgetController
	 *
	 * 생성된 오버레이 위젯 컨트롤러 인스턴스 참조임
	 */
	UPROPERTY()
	TObjectPtr<UOverlayWidgetController> OverlayWidgetController;

	/**
	 * OverlayWidgetControllerClass
	 *
	 * BP에서 설정된 오버레이 위젯 컨트롤러 클래스 타입임
	 */
	UPROPERTY(EditAnywhere)
	TSubclassOf<UOverlayWidgetController> OverlayWidgetControllerClass;
};
