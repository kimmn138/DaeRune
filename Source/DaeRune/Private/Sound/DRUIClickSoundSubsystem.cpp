// Copyright DaeRune


#include "Sound/DRUIClickSoundSubsystem.h"
#include "Sound/DRSoundDataAsset.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"

namespace
{
	// UMG UButton 이 만드는 Slate 위젯
	const FName ButtonWidgetType(TEXT("SButton"));
}

/** 입력을 소비하지 않고 보기만 한다 — 모든 Handle* 는 false 를 돌려준다. */
class FDRUIClickSoundInputProcessor : public IInputProcessor
{
public:
	explicit FDRUIClickSoundInputProcessor(UGameInstance* InGameInstance)
		: GameInstance(InGameInstance)
	{
	}

	virtual void Tick(const float DeltaTime, FSlateApplication& SlateApp, TSharedRef<ICursor> Cursor) override {}

	virtual bool HandleMouseButtonDownEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		HandlePointerPress(SlateApp, MouseEvent);
		return false;
	}

	virtual bool HandleMouseButtonDoubleClickEvent(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) override
	{
		// Slate 는 두 번째 클릭을 ButtonDown 대신 DoubleClick 으로 보내고, SButton 은 그것도 누름으로 처리한다
		HandlePointerPress(SlateApp, MouseEvent);
		return false;
	}

	virtual bool HandleKeyDownEvent(FSlateApplication& SlateApp, const FKeyEvent& InKeyEvent) override
	{
		// SButton::OnKeyDown 과 같은 조건 — 확인 키를 포커스된 버튼이 받는다
		if (InKeyEvent.IsRepeat() || SlateApp.GetNavigationActionFromKey(InKeyEvent) != EUINavigationAction::Accept)
		{
			return false;
		}

		const TSharedPtr<SWidget> Focused = SlateApp.GetUserFocusedWidget(InKeyEvent.GetUserIndex());
		if (Focused.IsValid() && Focused->GetType() == ButtonWidgetType && Focused->IsEnabled()
			&& IsInsideViewport(*Focused))
		{
			PlayClick();
		}
		return false;
	}

	virtual const TCHAR* GetDebugName() const override { return TEXT("DRUIClickSound"); }

private:
	void HandlePointerPress(FSlateApplication& SlateApp, const FPointerEvent& MouseEvent) const
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton) return;

		// 캡처 중이면 Slate 는 커서 아래가 아니라 캡처한 위젯에 보낸다(게임 조작 중의 뷰포트, 누르고 있는 버튼) — 새 누름이 아니다
		if (SlateApp.HasUserMouseCapture(MouseEvent.GetUserIndex()) || SlateApp.IsDragDropping()) return;

		const TSharedPtr<SViewport> Viewport = GetViewportWidget();
		if (!Viewport.IsValid()) return;

		// Slate 가 누름을 보낼 경로를 같은 방법으로 찾는다 (비활성 위젯부터 안쪽은 경로에서 빠진다)
		const FWidgetPath Path = SlateApp.LocateWindowUnderMouse(MouseEvent.GetScreenSpacePosition(),
			SlateApp.GetInteractiveTopLevelWindows(), /*bIgnoreEnabledStatus*/ false, MouseEvent.GetUserIndex());
		if (!Path.ContainsWidget(Viewport.Get())) return;

		for (int32 Index = Path.Widgets.Num() - 1; Index >= 0; --Index)
		{
			if (Path.Widgets[Index].Widget->GetType() == ButtonWidgetType)
			{
				PlayClick();
				return;
			}
		}
	}

	bool IsInsideViewport(const SWidget& Widget) const
	{
		const TSharedPtr<SViewport> Viewport = GetViewportWidget();
		if (!Viewport.IsValid()) return false;

		for (TSharedPtr<SWidget> Cursor = Widget.GetParentWidget(); Cursor.IsValid(); Cursor = Cursor->GetParentWidget())
		{
			if (Cursor.Get() == Viewport.Get()) return true;
		}
		return false;
	}

	TSharedPtr<SViewport> GetViewportWidget() const
	{
		const UGameInstance* GI = GameInstance.Get();
		const UGameViewportClient* ViewportClient = GI ? GI->GetGameViewportClient() : nullptr;
		return ViewportClient ? ViewportClient->GetGameViewportWidget() : nullptr;
	}

	void PlayClick() const
	{
		if (const UGameInstance* GI = GameInstance.Get())
		{
			UDRSoundDataAsset::Play2D(GI, &UDRSoundDataAsset::UIButtonClickSound);
		}
	}

	TWeakObjectPtr<UGameInstance> GameInstance;
};

bool UDRUIClickSoundSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// 데디케이티드 서버에는 누를 버튼도 들을 사람도 없다
	return !IsRunningDedicatedServer() && Super::ShouldCreateSubsystem(Outer);
}

void UDRUIClickSoundSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (FSlateApplication::IsInitialized())
	{
		InputProcessor = MakeShared<FDRUIClickSoundInputProcessor>(GetGameInstance());
		FSlateApplication::Get().RegisterInputPreProcessor(InputProcessor);
	}
}

void UDRUIClickSoundSubsystem::Deinitialize()
{
	if (InputProcessor.IsValid() && FSlateApplication::IsInitialized())
	{
		FSlateApplication::Get().UnregisterInputPreProcessor(InputProcessor);
	}
	InputProcessor.Reset();

	Super::Deinitialize();
}
