// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRHUD.generated.h"

class UAttributeSet;
class UAbilitySystemComponent;
class UOverlayWidgetController;
class UDRUserWidget;
class UUserWidget;
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

	// 이미 생성된 컨트롤러를 그대로 반환 (생성/캐싱은 InitOverlay 단계에서 끝남)
	UFUNCTION(BlueprintCallable, Category = "HUD")
	UOverlayWidgetController* GetOverlayWidgetControllerCached() const { return OverlayWidgetController; }

	UFUNCTION(BlueprintCallable, Category = "HUD")
	UDRUserWidget* GetOverlayWidget() const { return OverlayWidget; }

	// �������� UI �ʱ�ȭ
	void InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

	// ������ �������� ������Ʈ
	void UpdateOverlayForSpectating(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

	// 오버레이 제거 (대기실 전환 시 잘못 생성된 오버레이 정리용)
	void RemoveOverlay();

	// Tab Hold 캐릭터 설명창 표시
	UFUNCTION(BlueprintCallable, Category = "UI|CharacterInfo")
	void ShowCharacterInfo(EPlayerCharacterClass CharacterClass);

	// Tab 해제 시 캐릭터 설명창 숨김
	UFUNCTION(BlueprintCallable, Category = "UI|CharacterInfo")
	void HideCharacterInfo();

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

	// 캐싱된 캐릭터 설명창 위젯 인스턴스 (보였다/숨겼다 토글)
	UPROPERTY()
	TObjectPtr<UUserWidget> CharacterInfoWidget;

	// 현재 캐싱된 위젯이 어떤 캐릭터 클래스용인지 추적 (클래스 변경 시 재생성)
	EPlayerCharacterClass CachedCharacterInfoClass = EPlayerCharacterClass::GardenRobot;
	bool bHasCachedCharacterInfoClass = false;
};
