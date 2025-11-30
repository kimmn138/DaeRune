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
 * DaeRune 메인 HUD 클래스
 */
UCLASS()
class DAERUNE_API ADRHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	// OverlayWidgetController 접근자 (싱글톤 패턴)
	UOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);

	// 오버레이 UI 초기화
	void InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

	// 관전용 오버레이 업데이트
	void UpdateOverlayForSpectating(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

protected:

private:
	// 메인 오버레이 위젯 인스턴스
	UPROPERTY()
	TObjectPtr<UDRUserWidget>  OverlayWidget;

	// 블루프린트에서 설정할 오버레이 위젯 클래스
	UPROPERTY(EditAnywhere)
	TSubclassOf<UDRUserWidget> OverlayWidgetClass;

	// 오버레이 위젯 컨트롤러 인스턴스
	UPROPERTY()
	TObjectPtr<UOverlayWidgetController> OverlayWidgetController;

	// 블루프린트에서 설정할 위젯 컨트롤러 클래스
	UPROPERTY(EditAnywhere)
	TSubclassOf<UOverlayWidgetController> OverlayWidgetControllerClass;
};
