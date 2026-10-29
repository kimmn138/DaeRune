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
 * DaeRune 메인 HUD 클래스
 */
UCLASS()
class DAERUNE_API ADRHUD : public AHUD
{
	GENERATED_BODY()
	
public:
	// OverlayWidgetController 접근자 (싱글톤 패턴)
	UOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);

	// 이미 생성된 컨트롤러를 그대로 반환 (생성/캐싱은 InitOverlay 단계에서 끝남)
	UFUNCTION(BlueprintCallable, Category = "HUD")
	UOverlayWidgetController* GetOverlayWidgetControllerCached() const { return OverlayWidgetController; }

	UFUNCTION(BlueprintCallable, Category = "HUD")
	UDRUserWidget* GetOverlayWidget() const { return OverlayWidget; }

	// 오버레이 UI 초기화
	void InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

	// 이미 떠 있는 오버레이를 다른 PlayerState 에 다시 묶는다 (관전 대상 전환 · 관전 종료 후 내 상태로 복귀)
	void RebindOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

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
	// 메인 오버레이 위젯 인스턴스
	UPROPERTY()
	TObjectPtr<UDRUserWidget>  OverlayWidget;

	// 블루프린트에서 설정할 오버레이 위젯 클래스
	UPROPERTY(EditAnywhere)
	TSubclassOf<UDRUserWidget> OverlayWidgetClass;

	// 오버레이 위젯 컨트롤러 인스턴스
	UPROPERTY()
	TObjectPtr<UOverlayWidgetController> OverlayWidgetController;

	// 컨트롤러의 바인딩을 모두 풀고 참조를 끊는다. 객체는 GC 가 거둔다 —
	// ★ConditionalBeginDestroy 로 직접 파괴하지 않는다★(약참조가 무효화되지 않아 람다가 계속 살아 있고, GC 가 아직 쥔 객체를 부수게 된다)
	void ReleaseOverlayWidgetController();

	// 블루프린트에서 설정할 위젯 컨트롤러 클래스
	UPROPERTY(EditAnywhere)
	TSubclassOf<UOverlayWidgetController> OverlayWidgetControllerClass;

	// 캐싱된 캐릭터 설명창 위젯 인스턴스 (보였다/숨겼다 토글)
	UPROPERTY()
	TObjectPtr<UUserWidget> CharacterInfoWidget;

	// 현재 캐싱된 위젯이 어떤 캐릭터 클래스용인지 추적 (클래스 변경 시 재생성)
	EPlayerCharacterClass CachedCharacterInfoClass = EPlayerCharacterClass::Gardener;
	bool bHasCachedCharacterInfoClass = false;
};
