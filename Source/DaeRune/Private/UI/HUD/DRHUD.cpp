// Copyright DaeRune


#include "UI/HUD/DRHUD.h"
#include "UI/Widget/DRUserWidget.h"
#include "UI/WidgetController/OverlayWidgetController.h"

UOverlayWidgetController* ADRHUD::GetOverlayWidgetController(const FWidgetControllerParams& WCParams)
{
	// �̱��� ����: �� ���� �����ϰ� ����
	if (OverlayWidgetController == nullptr)
	{
		// ��Ʈ�ѷ� ���� �� �ʱ�ȭ
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

	// ��������Ʈ Ŭ���� ���� Ȯ��
	checkf(OverlayWidgetClass, TEXT("Overlay Widget Class uninitialized, please fill out BP_DRHUD"));
	checkf(OverlayWidgetControllerClass, TEXT("Overlay Widget Controller Class uninitialized, please fill out BP_DRHUD"));

	// ���� ����
	UUserWidget* Widget = CreateWidget<UUserWidget>(GetWorld(), OverlayWidgetClass);
	OverlayWidget = Cast<UDRUserWidget>(Widget);

	// WidgetController �Ķ���� ����
	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
	UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);

	// ������ ��Ʈ�ѷ� ����
	OverlayWidget->SetWidgetController(WidgetController);
	// �ʱ� ������ UI�� ��ε�ĳ��Ʈ
	WidgetController->BroadcastInitialValues();
	// 캐릭터 클래스에 맞는 스킬아이콘 위젯 브로드캐스트
	WidgetController->BroadcastSkillIconWidgetClass();
	// 어빌리티 아이콘 갱신 (위젯 컨트롤러 할당 후 호출하여 소실 방지)
	WidgetController->BroadcastAbilityInfo();
	// ȭ�鿡 ���� �߰�
	Widget->AddToViewport();
}

void ADRHUD::UpdateOverlayForSpectating(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
{
	// ���� WidgetController �ı�
	if (OverlayWidgetController)
	{
		OverlayWidgetController->UnbindAllDelegates();

		OverlayWidgetController->ConditionalBeginDestroy();
		OverlayWidgetController = nullptr;
	}

	// ���ο� �Ķ���ͷ� WidgetController ����
	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
	UOverlayWidgetController* WidgetController = GetOverlayWidgetController(WidgetControllerParams);

	// ���� ������ �� ��Ʈ�ѷ� ����
	if (OverlayWidget)
	{
		OverlayWidget->SetWidgetController(WidgetController);
		WidgetController->BroadcastInitialValues();
		WidgetController->BroadcastSkillIconWidgetClass();
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

	if (OverlayWidgetController)
	{
		OverlayWidgetController->UnbindAllDelegates();
		OverlayWidgetController->ConditionalBeginDestroy();
		OverlayWidgetController = nullptr;
	}
}
