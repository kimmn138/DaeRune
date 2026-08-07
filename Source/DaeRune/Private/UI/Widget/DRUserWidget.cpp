// Copyright DaeRune


#include "UI/Widget/DRUserWidget.h"
#include "Game/DRSettingsManager.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/RichTextBlock.h"

void UDRUserWidget::SetWidgetController(UObject* InWidgetController)
{
	// WidgetController ���� ����
	WidgetController = InWidgetController;
	// ��������Ʈ ���� �̺�Ʈ ȣ��
	WidgetControllerSet();
}

void UDRUserWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 언어 변경 브로드캐스트 구독 (GameInstance 서브시스템에서 발행).
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDRSettingsManager* Settings = GameInstance->GetSubsystem<UDRSettingsManager>())
		{
			Settings->OnLanguageChanged.AddUniqueDynamic(this, &UDRUserWidget::HandleLanguageChanged);
		}
	}
}

void UDRUserWidget::NativeDestruct()
{
	if (UGameInstance* GameInstance = GetGameInstance())
	{
		if (UDRSettingsManager* Settings = GameInstance->GetSubsystem<UDRSettingsManager>())
		{
			Settings->OnLanguageChanged.RemoveDynamic(this, &UDRUserWidget::HandleLanguageChanged);
		}
	}

	Super::NativeDestruct();
}

void UDRUserWidget::HandleLanguageChanged(const FString& CultureCode)
{
	// 디자이너에 박힌 정적 LOCTEXT 텍스트는 컬처가 바뀌어도 Slate 캐시가 자동 갱신되지 않는다.
	// 자식 Text/RichText 블록에 SynchronizeProperties를 다시 태워 현재 컬처로 재해석시킨다.
	if (WidgetTree)
	{
		WidgetTree->ForEachWidget([](UWidget* Widget)
		{
			if (UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
			{
				TextBlock->SynchronizeProperties();
			}
			else if (URichTextBlock* RichTextBlock = Cast<URichTextBlock>(Widget))
			{
				RichTextBlock->SynchronizeProperties();
			}
		});
	}

	// 코드/BP에서 동적으로 SetText 한 텍스트는 위젯이 이 이벤트에서 직접 다시 세팅.
	OnLanguageChanged();
}
