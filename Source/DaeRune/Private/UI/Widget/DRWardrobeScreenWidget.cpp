// Copyright DaeRune


#include "UI/Widget/DRWardrobeScreenWidget.h"
#include "UI/Widget/DRWardrobeTabWidget.h"
#include "UI/Widget/DRCosmeticPreviewWidget.h"
#include "Game/DRGameInstance.h"
#include "Player/DRPlayerController.h"
#include "Player/DRPlayerState.h"
#include "Blueprint/WidgetTree.h"
#include "Components/PanelWidget.h"	// FindOwnerScreen 이 GetParent() 반환형(UPanelWidget)을 업캐스트한다
#include "DaeRune/DRLogChannels.h"

EPlayerCharacterClass UDRWardrobeScreenWidget::GetViewedClass() const
{
	// 옷장은 "지금 고른 로봇"의 옷만 편집한다 (UDRUpgradeScreenWidget::GetViewedClass 와 같은 기준).
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

UDRGameInstance* UDRWardrobeScreenWidget::GetProgression() const
{
	return GetGameInstance<UDRGameInstance>();
}

UDRWardrobeScreenWidget* UDRWardrobeScreenWidget::FindOwnerScreen(UWidget* From)
{
	if (!From) return nullptr;

	// 1) 디자이너 배치 위젯(탭 등) — 화면의 WidgetTree 안에 있으므로 Outer 체인에서 바로 잡힌다
	if (UDRWardrobeScreenWidget* Screen = From->GetTypedOuter<UDRWardrobeScreenWidget>())
	{
		return Screen;
	}

	// 2) 런타임 생성 위젯(칸 등) — CreateWidget 의 Owner 가 PlayerController 라 Outer 로는 못 찾는다.
	//    부모 패널은 화면의 WidgetTree 소속이므로 한 단계만 올라가면 된다.
	for (UWidget* Cursor = From->GetParent(); Cursor; Cursor = Cursor->GetParent())
	{
		if (UDRWardrobeScreenWidget* Screen = Cursor->GetTypedOuter<UDRWardrobeScreenWidget>())
		{
			return Screen;
		}
	}

	UE_LOG(LogDR, Warning,
		TEXT("[Cosmetic] %s 가 소속 옷장 화면을 찾지 못했습니다. 화면 밖에서 만들어졌거나 아직 부모에 붙지 않았습니다."),
		*From->GetName());

	return nullptr;
}

void UDRWardrobeScreenWidget::SetCurrentCategory(EDRCosmeticCategory NewCategory)
{
	if (!DRIsValidCosmeticCategory(NewCategory)) return;
	if (CurrentCategory == NewCategory) return;

	CurrentCategory = NewCategory;

	RefreshAll();
	OnCategoryChanged(CurrentCategory);
}

void UDRWardrobeScreenWidget::RequestClose()
{
	// 닫기는 반드시 컨트롤러를 거친다 — 입력 모드 복구가 거기 묶여 있다.
	if (ADRPlayerController* PC = GetOwningPlayer<ADRPlayerController>())
	{
		PC->CloseWardrobeScreen();
	}
}

void UDRWardrobeScreenWidget::RefreshAll()
{
	UpdateTabSelection();
	OnRefreshSkins();
}

void UDRWardrobeScreenWidget::GetCurrentSkinViewModels(TArray<FDRSkinViewModel>& OutViewModels) const
{
	OutViewModels.Reset();

	if (const UDRGameInstance* GI = GetProgression())
	{
		GI->GetSkinViewModels(GetViewedClass(), CurrentCategory, OutViewModels);
	}
}

bool UDRWardrobeScreenWidget::TryEquip(FName SkinId)
{
	UDRGameInstance* GI = GetProgression();
	if (!GI) return false;

	// 잠긴 항목 — 조건 문구를 찾아 BP 에 넘긴다 (흔들림 연출 + 안내)
	if (!GI->IsSkinUnlocked(SkinId))
	{
		FText Hint;

		TArray<FDRSkinViewModel> ViewModels;
		GetCurrentSkinViewModels(ViewModels);
		for (const FDRSkinViewModel& ViewModel : ViewModels)
		{
			if (ViewModel.SkinId == SkinId)
			{
				Hint = ViewModel.UnlockHint;
				break;
			}
		}

		OnEquipRejected(SkinId, Hint);
		return false;
	}

	if (!GI->EquipSkin(GetViewedClass(), CurrentCategory, SkinId))
	{
		// 카탈로그 불일치 등 — 흔한 경로가 아니라 문구 없이 거절만 알린다
		OnEquipRejected(SkinId, FText::GetEmpty());
		return false;
	}

	// 목록 재바인딩은 OnCosmeticsChanged → HandleCosmeticsChanged 가 담당한다.
	// (같은 칸을 다시 눌러 변경이 없으면 브로드캐스트가 없으므로 중복 갱신도 없다)
	OnSkinEquipped(SkinId);
	return true;
}

void UDRWardrobeScreenWidget::ClearAll()
{
	if (UDRGameInstance* GI = GetProgression())
	{
		// 변경이 있으면 OnCosmeticsChanged 가 목록을 갱신한다
		GI->ClearAllSkins(GetViewedClass());
	}
}

void UDRWardrobeScreenWidget::UpdateTabSelection()
{
	if (!WidgetTree) return;

	WidgetTree->ForEachWidget([this](UWidget* Widget)
	{
		if (UDRWardrobeTabWidget* Tab = Cast<UDRWardrobeTabWidget>(Widget))
		{
			Tab->SetSelected(Tab->Category == CurrentCategory);
		}
	});
}

void UDRWardrobeScreenWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// 화면이 FInputModeUIOnly 로 열리므로(ADRPlayerController::OpenWardrobeScreen)
	// ESC / C 키는 위젯이 직접 받아야 한다. 포커스가 없으면 NativeOnKeyDown 이 아예 호출되지 않는다.
	SetIsFocusable(true);
	SetKeyboardFocus();

	CurrentCategory = DRIsValidCosmeticCategory(DefaultCategory)
		? DefaultCategory
		: EDRCosmeticCategory::Head;

	if (UDRGameInstance* GI = GetProgression())
	{
		GI->OnCosmeticsChanged.AddDynamic(this, &UDRWardrobeScreenWidget::HandleCosmeticsChanged);
		GI->OnSkinUnlocked.AddDynamic(this, &UDRWardrobeScreenWidget::HandleSkinUnlocked);
	}
	else
	{
		UE_LOG(LogDR, Warning,
			TEXT("[Cosmetic] 옷장 화면이 UDRGameInstance 를 찾지 못했습니다. 목록이 비어 있게 됩니다."));
	}

	RefreshAll();
	OnCategoryChanged(CurrentCategory);

	// 3D 프리뷰 기동 — 스테이지가 없으면 프리뷰 위젯이 스스로 접힌다 (Plan.md 5.8.2 폴백)
	if (Preview_Character)
	{
		const UDRGameInstance* GI = GetProgression();
		Preview_Character->InitializePreview(
			GetViewedClass(),
			GI ? GI->GetEquippedSkins(GetViewedClass()) : TArray<FName>());
	}
}

void UDRWardrobeScreenWidget::SyncPreview()
{
	if (!Preview_Character) return;

	const UDRGameInstance* GI = GetProgression();
	if (!GI) return;

	Preview_Character->UpdatePreviewSkins(GI->GetEquippedSkins(GetViewedClass()));
}

void UDRWardrobeScreenWidget::NativeDestruct()
{
	// GameInstance 는 레벨 전환에도 살아남는다. 해제하지 않으면 죽은 위젯이 델리게이트에 남는다.
	if (UDRGameInstance* GI = GetProgression())
	{
		GI->OnCosmeticsChanged.RemoveDynamic(this, &UDRWardrobeScreenWidget::HandleCosmeticsChanged);
		GI->OnSkinUnlocked.RemoveDynamic(this, &UDRWardrobeScreenWidget::HandleSkinUnlocked);
	}

	Super::NativeDestruct();
}

FReply UDRWardrobeScreenWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
	const FKey PressedKey = InKeyEvent.GetKey();

	// 다른 창들과 동일하게 키 입력으로 닫는다 (마우스 전용 닫기 버튼은 두지 않는다 — Plan.md 15.9)
	if (PressedKey == EKeys::Escape)
	{
		RequestClose();
		return FReply::Handled();
	}

	// 시안의 [C] 버튼과 같은 동작 — 4개 카테고리 전부 해제
	if (ClearAllKey.IsValid() && PressedKey == ClearAllKey)
	{
		ClearAll();
		return FReply::Handled();
	}

	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

void UDRWardrobeScreenWidget::HandleCosmeticsChanged(EPlayerCharacterClass CharacterClass)
{
	// 다른 로봇의 변경은 이 화면과 무관하다
	if (CharacterClass != GetViewedClass()) return;

	OnRefreshSkins();

	// ★장착이 바뀌는 모든 경로(칸 클릭 / Clear All / 치트)가 여기를 지나므로
	//   프리뷰 갱신을 한 곳에만 두면 된다★
	SyncPreview();
}

void UDRWardrobeScreenWidget::HandleSkinUnlocked(FName SkinId)
{
	// 해금은 목록의 잠금 표시를 바꾸므로 다시 그린다
	OnRefreshSkins();
	OnSkinNewlyUnlocked(SkinId);
}
