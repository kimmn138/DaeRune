// Copyright DaeRune


#include "UI/HUD/DRHUD.h"
#include "UI/Widget/DRUserWidget.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "Blueprint/UserWidget.h"

UOverlayWidgetController* ADRHUD::GetOverlayWidgetController(const FWidgetControllerParams& WCParams)
{
	// 싱글톤 패턴: 한 번만 생성하고 재사용
	if (OverlayWidgetController == nullptr)
	{
		// 컨트롤러 생성 및 초기화
		OverlayWidgetController = NewObject<UOverlayWidgetController>(this, OverlayWidgetControllerClass);
		OverlayWidgetController->SetWidgetControllerParams(WCParams);
		OverlayWidgetController->BindCallbacksToDependencies();
	}
	return OverlayWidgetController;
}

void ADRHUD::InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	// 이미 오버레이가 존재하면 리턴 (중복 생성 방지)
	if (OverlayWidget)
	{
		return;
	}

	// 블루프린트 클래스 설정 확인
	checkf(OverlayWidgetClass, TEXT("Overlay Widget Class uninitialized, please fill out BP_DRHUD"));
	checkf(OverlayWidgetControllerClass, TEXT("Overlay Widget Controller Class uninitialized, please fill out BP_DRHUD"));

	// 위젯 생성
	UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
	OverlayWidget = Cast<UDRUserWidget>(Widget);

	// WidgetController 파라미터 구성
	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
	UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);

	// 위젯과 컨트롤러 연결
	OverlayWidget->SetWidgetController(WidgetController);
	// 초기 값들을 UI에 브로드캐스트
	WidgetController->BroadcastInitialValues();
	// 캐릭터 클래스에 맞는 스킬아이콘 위젯 브로드캐스트
	WidgetController->BroadcastSkillIconWidgetClass();
	// 캐릭터 클래스에 맞는 조준선 텍스처 세트 브로드캐스트
	WidgetController->BroadcastCrosshairImages();
	// 어빌리티 아이콘 갱신 (위젯 컨트롤러 할당 후 호출하여 소실 방지)
	WidgetController->BroadcastAbilityInfo();
	// 화면에 위젯 추가
	Widget->AddToViewport();
}

void ADRHUD::ReleaseOverlayWidgetController()
{
	if (OverlayWidgetController)
	{
		OverlayWidgetController->UnbindAllDelegates();
		OverlayWidgetController = nullptr;
	}
}

void ADRHUD::RebindOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	ReleaseOverlayWidgetController();

	// 새로운 파라미터로 WidgetController 생성
	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
	UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);

	// 기존 위젯에 새 컨트롤러 연결
	if (OverlayWidget)
	{
		OverlayWidget->SetWidgetController(WidgetController);
		WidgetController->BroadcastInitialValues();
		WidgetController->BroadcastSkillIconWidgetClass();
		WidgetController->BroadcastCrosshairImages();
		WidgetController->BroadcastAbilityInfo();
	}
}

void ADRHUD::RemoveOverlay()
{
	if (OverlayWidget)
	{
		OverlayWidget->RemoveFromParent();
		OverlayWidget = nullptr;
	}

	ReleaseOverlayWidgetController();

	if (CharacterInfoWidget)
	{
		CharacterInfoWidget->RemoveFromParent();
		CharacterInfoWidget = nullptr;
	}
	bHasCachedCharacterInfoClass = false;
}

void ADRHUD::ShowCharacterInfo(EPlayerCharacterClass CharacterClass)
{
	// 캐싱된 위젯이 다른 클래스용이면 제거 후 재생성
	if (CharacterInfoWidget && bHasCachedCharacterInfoClass && CachedCharacterInfoClass != CharacterClass)
	{
		CharacterInfoWidget->RemoveFromParent();
		CharacterInfoWidget = nullptr;
	}

	if (CharacterInfoWidget == nullptr)
	{
		TSubclassOf<UUserWidget> WidgetClass = UDRAbilitySystemLibrary::GetCharacterInfoWidgetClass(this, CharacterClass);
		if (WidgetClass == nullptr) return;

		APlayerController* PC = GetOwningPlayerController();
		if (PC == nullptr) return;

		CharacterInfoWidget = CreateWidget<UUserWidget>(PC, WidgetClass);
		if (CharacterInfoWidget == nullptr) return;

		CachedCharacterInfoClass = CharacterClass;
		bHasCachedCharacterInfoClass = true;
		CharacterInfoWidget->AddToViewport(100);
	}
	else
	{
		CharacterInfoWidget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	}
}

void ADRHUD::HideCharacterInfo()
{
	if (CharacterInfoWidget)
	{
		CharacterInfoWidget->SetVisibility(ESlateVisibility::Collapsed);
	}
}
