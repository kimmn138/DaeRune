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
 * DaeRune ���� HUD Ŭ����
 */
UCLASS()
class DAERUNE_API ADRHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	// OverlayWidgetController ������ (�̱��� ����)
	UOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);

	UFUNCTION(BlueprintCallable, Category = "HUD")
	UDRUserWidget* GetOverlayWidget() const { return OverlayWidget; }

	// �������� UI �ʱ�ȭ
	void InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

	// ������ �������� ������Ʈ
	void UpdateOverlayForSpectating(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

	// 오버레이 제거 (대기실 전환 시 잘못 생성된 오버레이 정리용)
	void RemoveOverlay();

protected:

private:
	// ���� �������� ���� �ν��Ͻ�
	UPROPERTY()
	TObjectPtr<UDRUserWidget>  OverlayWidget;

	// ��������Ʈ���� ������ �������� ���� Ŭ����
	UPROPERTY(EditAnywhere)
	TSubclassOf<UDRUserWidget> OverlayWidgetClass;

	// �������� ���� ��Ʈ�ѷ� �ν��Ͻ�
	UPROPERTY()
	TObjectPtr<UOverlayWidgetController> OverlayWidgetController;

	// ��������Ʈ���� ������ ���� ��Ʈ�ѷ� Ŭ����
	UPROPERTY(EditAnywhere)
	TSubclassOf<UOverlayWidgetController> OverlayWidgetControllerClass;
};
