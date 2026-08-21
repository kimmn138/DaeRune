// Copyright DaeRune


#include "UI/Widget/DRUpgradeScreenWidget.h"
#include "Game/DRGameInstance.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Components/PanelWidget.h"	// FindOwnerScreen 이 GetParent() 반환형(UPanelWidget)을 업캐스트한다
#include "DaeRune/DRLogChannels.h"

EPlayerCharacterClass UDRUpgradeScreenWidget::GetViewedClass() const
{
	// 업그레이드는 "지금 고른 로봇"에 대해서만 편집한다.
	// PlayerState 가 아직 안 붙었으면 기본값으로 폴백한다(로비 진입 직후 한 프레임).
	if (const APlayerController* PC = GetOwningPlayer())
	{
		if (const ADRPlayerState* PS = PC->GetPlayerState<ADRPlayerState>())
		{
			return PS->GetSelectedPlayerClass();
		}
	}

	return EPlayerCharacterClass::Gardener;
}

UDRGameInstance* UDRUpgradeScreenWidget::GetProgression() const
{
	return GetGameInstance<UDRGameInstance>();
}

UDRUpgradeUIStyle* UDRUpgradeScreenWidget::GetUIStyle() const
{
	const UDRGameInstance* GI = GetProgression();
	return GI ? GI->GetUpgradeUIStyle() : nullptr;
}

UDRUpgradeScreenWidget* UDRUpgradeScreenWidget::FindOwnerScreen(UWidget* From)
{
	if (!From) return nullptr;

	// 1) 디자이너 배치 위젯(슬롯 등) — 화면의 WidgetTree 안에 있으므로 Outer 체인에서 바로 잡힌다
	if (UDRUpgradeScreenWidget* Screen = From->GetTypedOuter<UDRUpgradeScreenWidget>())
	{
		return Screen;
	}

	// 2) 런타임 생성 위젯(칩 등) — CreateWidget 의 Owner 가 PlayerController 라 Outer 로는 못 찾는다.
	//    부모 패널은 화면의 WidgetTree 소속이므로 한 단계만 올라가면 된다.
	for (UWidget* Cursor = From->GetParent(); Cursor; Cursor = Cursor->GetParent())
	{
		if (UDRUpgradeScreenWidget* Screen = Cursor->GetTypedOuter<UDRUpgradeScreenWidget>())
		{
			return Screen;
		}
	}

	UE_LOG(LogDR, Warning,
		TEXT("[Upgrade] %s 가 소속 업그레이드 화면을 찾지 못했습니다. 화면 밖에서 만들어졌거나 아직 부모에 붙지 않았습니다."),
		*From->GetName());

	return nullptr;
}

void UDRUpgradeScreenWidget::RequestClose()
{
	// 닫기는 반드시 컨트롤러를 거친다 — 입력 모드 복구와 서버 재보고가 거기 묶여 있다.
	if (ADRPlayerController* PC = GetOwningPlayer<ADRPlayerController>())
	{
		PC->CloseUpgradeScreen();
	}
}

void UDRUpgradeScreenWidget::RefreshAll()
{
	OnRefreshSlots();
	OnRefreshChips();

	if (const UDRGameInstance* GI = GetProgression())
	{
		OnCurrencyUpdated(GI->GetCurrency());
	}
}

void UDRUpgradeScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	if (UDRGameInstance* GI = GetProgression())
	{
		GI->OnUpgradesChanged.AddDynamic(this, &UDRUpgradeScreenWidget::HandleUpgradesChanged);
		GI->OnCurrencyChanged.AddDynamic(this, &UDRUpgradeScreenWidget::HandleCurrencyChanged);
	}
	else
	{
		UE_LOG(LogDR, Warning,
			TEXT("[Upgrade] 업그레이드 화면이 UDRGameInstance 를 찾지 못했습니다. 진행도 표시가 비어 있게 됩니다."));
	}

	RefreshAll();
}

void UDRUpgradeScreenWidget::NativeDestruct()
{
	// GameInstance 는 레벨 전환에도 살아남는다. 해제하지 않으면 죽은 위젯이 델리게이트에 남는다.
	if (UDRGameInstance* GI = GetProgression())
	{
		GI->OnUpgradesChanged.RemoveDynamic(this, &UDRUpgradeScreenWidget::HandleUpgradesChanged);
		GI->OnCurrencyChanged.RemoveDynamic(this, &UDRUpgradeScreenWidget::HandleCurrencyChanged);
	}

	Super::NativeDestruct();
}

void UDRUpgradeScreenWidget::HandleUpgradesChanged(EPlayerCharacterClass CharacterClass)
{
	// 다른 로봇의 변경은 이 화면과 무관하다
	if (CharacterClass != GetViewedClass()) return;

	OnRefreshSlots();
	OnRefreshChips();
}

void UDRUpgradeScreenWidget::HandleCurrencyChanged(int32 NewCurrency)
{
	OnCurrencyUpdated(NewCurrency);

	// 재화가 늘면 해금 가능해진 칸이 생기고, 줄면 반대다 → 슬롯도 함께 갱신한다
	OnRefreshSlots();
}
