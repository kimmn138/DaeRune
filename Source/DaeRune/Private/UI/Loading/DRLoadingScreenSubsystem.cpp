// Copyright DaeRune


#include "UI/Loading/DRLoadingScreenSubsystem.h"
#include "UI/Loading/DRLoadingScreenWidget.h"
#include "MoviePlayer.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/GameViewportClient.h"
#include "Engine/UserInterfaceSettings.h"
#include "TimerManager.h"
#include "Game/DRStageGameState.h"

#define LOCTEXT_NAMESPACE "DRLoading"

void UDRLoadingScreenSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// 맵 로드 완료 시 퇴장 애니를 재생하기 위해 바인딩.
	PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UDRLoadingScreenSubsystem::HandlePostLoadMap);

	// 심리스 트래블 시작 시(서버·각 클라이언트에서 로컬로 발생) 로딩 화면을 띄우기 위해 바인딩.
	SeamlessTravelStartHandle = FWorldDelegates::OnSeamlessTravelStart.AddUObject(this, &UDRLoadingScreenSubsystem::HandleSeamlessTravelStart);
}

void UDRLoadingScreenSubsystem::Deinitialize()
{
	if (PostLoadMapHandle.IsValid())
	{
		FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
		PostLoadMapHandle.Reset();
	}

	if (SeamlessTravelStartHandle.IsValid())
	{
		FWorldDelegates::OnSeamlessTravelStart.Remove(SeamlessTravelStartHandle);
		SeamlessTravelStartHandle.Reset();
	}

	Super::Deinitialize();
}

UDRLoadingScreenWidget* UDRLoadingScreenSubsystem::CreateAndShow(TSubclassOf<UDRLoadingScreenWidget> WidgetClass, bool bPersistent)
{
	// 데디케이트 서버는 렌더링이 없으므로 스킵.
	if (IsRunningDedicatedServer() || !WidgetClass)
	{
		return nullptr;
	}

	UDRLoadingScreenWidget* Widget = CreateWidget<UDRLoadingScreenWidget>(GetGameInstance(), WidgetClass);
	if (!Widget)
	{
		return nullptr;
	}

	// 다른 모든 UI 위로 오도록 큰 ZOrder.
	// 전역 GEngine->GameViewport가 아니라 이 GameInstance 고유 뷰포트를 사용해야
	// 멀티플레이(한 프로세스에 서버+클라)에서 올바른 뷰포트에 추가된다.
	UGameViewportClient* ViewportClient = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (bPersistent && ViewportClient)
	{
		// 기존 유지 위젯이 있으면 먼저 제거(누수 방지).
		RemovePersistentWidget();

		// 뷰포트에 직접 추가 → 월드 전환(심리스 트래블) 시에도 제거되지 않고 유지된다.
		// 추가한 Slate 인스턴스를 저장해 두었다가 제거 시 그대로 사용한다.
		TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
		ViewportClient->AddViewportWidgetContent(SlateWidget, 10000);
		PersistentSlateWidget = SlateWidget;
	}
	else
	{
		Widget->AddToViewport(10000);
	}
	ActiveWidget = Widget;
	return Widget;
}

void UDRLoadingScreenSubsystem::RemovePersistentWidget()
{
	// 추가할 때와 동일하게 이 GameInstance 고유 뷰포트에서 제거(멀티플레이 안전).
	UGameViewportClient* ViewportClient = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr;
	if (PersistentSlateWidget.IsValid() && ViewportClient)
	{
		ViewportClient->RemoveViewportWidgetContent(PersistentSlateWidget.ToSharedRef());
	}
	PersistentSlateWidget.Reset();
}

void UDRLoadingScreenSubsystem::SetupMoviePlayerBridge(TSubclassOf<UDRLoadingScreenWidget> WidgetClass)
{
	if (IsRunningDedicatedServer() || !WidgetClass)
	{
		return;
	}

	// 멈춤 구간용 별도 인스턴스. 기본(애니 없는) 상태 = 꽉 찬 모습 + Throbber 회전.
	UDRLoadingScreenWidget* FreezeWidget = CreateWidget<UDRLoadingScreenWidget>(GetGameInstance(), WidgetClass);
	if (!FreezeWidget)
	{
		return;
	}

	FLoadingScreenAttributes Attr;
	Attr.bAutoCompleteWhenLoadingCompletes = true;
	Attr.MinimumLoadingScreenDisplayTime = MinimumFreezeScreenTime;
	Attr.WidgetLoadingScreen = FreezeWidget->TakeWidget();

	// MoviePlayer 는 UI 배율을 직접 정하지 않으면 1.0 으로 그린다 — 게임 뷰포트는 DPI 커브(짧은 변 1440 → 1.33)를 쓰므로
	// 앞뒤의 등장/퇴장 위젯보다 작게 보였다가 다시 커진다. 같은 커브 값을 넘긴다(OS DPI 배율은 엔진이 이때 끈다)
	if (UGameViewportClient* ViewportClient = GetGameInstance() ? GetGameInstance()->GetGameViewportClient() : nullptr)
	{
		FVector2D ViewportSize;
		ViewportClient->GetViewportSize(ViewportSize);
		if (ViewportSize.X > 0.0 && ViewportSize.Y > 0.0)
		{
			const FIntPoint ViewportPixels(FMath::TruncToInt32(ViewportSize.X), FMath::TruncToInt32(ViewportSize.Y));
			GetMoviePlayer()->SetViewportDPIScale(GetDefault<UUserInterfaceSettings>()->GetDPIScaleBasedOnSize(ViewportPixels));
		}
	}

	GetMoviePlayer()->SetupLoadingScreen(Attr);
}

void UDRLoadingScreenSubsystem::BeginLoadingScreen(TSubclassOf<UDRLoadingScreenWidget> WidgetClass)
{
	if (UDRLoadingScreenWidget* Widget = CreateAndShow(WidgetClass))
	{
		BeginWidget = Widget;
		BeginWidgetFrame = GFrameCounter;
		Widget->PlayIntro();
	}

	// 외부에서 트래블이 일어나므로 멈춤 브리지를 미리 등록하고, 도착 맵에서 아웃트로 예약.
	PendingOutroClass = WidgetClass;
	SetupMoviePlayerBridge(WidgetClass);
}

void UDRLoadingScreenSubsystem::CancelLoadingScreen()
{
	// 트래블이 없으니 도착 맵 아웃트로와 멈춤 구간 화면 예약을 지운다
	// (남겨 두면 나중의 아무 맵 로드에서 엉뚱하게 로딩 화면이 뜬다)
	PendingOutroClass = nullptr;
	if (!IsRunningDedicatedServer())
	{
		GetMoviePlayer()->SetupLoadingScreen(FLoadingScreenAttributes());
	}

	UDRLoadingScreenWidget* Widget = BeginWidget.Get();
	BeginWidget.Reset();
	if (!Widget || !Widget->IsInViewport())
	{
		return;
	}

	// 띄운 프레임에 바로 실패했으면(요청이 그 자리에서 실패) 아직 그려지지 않았다 — 연출 없이 뗀다
	if (BeginWidgetFrame == GFrameCounter)
	{
		Widget->RemoveFromParent();
		if (ActiveWidget == Widget)
		{
			ActiveWidget = nullptr;
		}
		return;
	}

	// 등장 애니가 아직 돌고 있으면 끝난 뒤 퇴장 — 두 애니가 같은 속성을 동시에 움직이지 않게
	if (Widget->IsAnyAnimationPlaying())
	{
		TWeakObjectPtr<UDRLoadingScreenSubsystem> WeakThis(this);
		TWeakObjectPtr<UDRLoadingScreenWidget> WeakWidget(Widget);
		TSharedRef<bool> bTriggered = MakeShared<bool>(false);

		auto CloseAfterIntro = [WeakThis, WeakWidget, bTriggered]()
		{
			if (*bTriggered)
			{
				return;
			}
			*bTriggered = true;

			if (WeakThis.IsValid() && WeakWidget.IsValid())
			{
				WeakThis->PlayOutroAndRemove(WeakWidget.Get());
			}
		};

		Widget->OnIntroFinished.AddLambda(CloseAfterIntro);

		// 안전장치: WBP 가 NotifyIntroFinished 를 부르지 않아도 닫는다
		if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		{
			FTimerHandle SafetyHandle;
			World->GetTimerManager().SetTimer(SafetyHandle, FTimerDelegate::CreateLambda(CloseAfterIntro), MaxIntroWaitTime, false);
		}
		return;
	}

	PlayOutroAndRemove(Widget);
}

void UDRLoadingScreenSubsystem::OpenLevelWithLoadingScreen(TSubclassOf<UDRLoadingScreenWidget> WidgetClass, FName LevelName, const FString& Options)
{
	UDRLoadingScreenWidget* Widget = CreateAndShow(WidgetClass);
	if (!Widget)
	{
		// 위젯을 못 만들면 그냥 바로 로드.
		UGameplayStatics::OpenLevel(GetGameInstance(), LevelName, true, Options);
		return;
	}

	TWeakObjectPtr<UDRLoadingScreenSubsystem> WeakThis(this);
	// 중복 실행 방지용 플래그(인트로 종료 콜백과 안전 타이머 중 먼저 도달한 쪽만 로드).
	TSharedRef<bool> bTriggered = MakeShared<bool>(false);

	auto DoLoad = [WeakThis, WidgetClass, LevelName, Options, bTriggered]()
	{
		if (*bTriggered)
		{
			return;
		}
		*bTriggered = true;

		if (!WeakThis.IsValid())
		{
			return;
		}

		UDRLoadingScreenSubsystem* Self = WeakThis.Get();
		Self->PendingOutroClass = WidgetClass;
		Self->SetupMoviePlayerBridge(WidgetClass);
		UGameplayStatics::OpenLevel(Self->GetGameInstance(), LevelName, true, Options);
	};

	// 정상 경로: 인트로 애니 종료 시 로드.
	Widget->OnIntroFinished.AddLambda(DoLoad);

	// 안전장치: WBP가 NotifyIntroFinished를 안 불러도 일정 시간 뒤 로드(소프트락 방지).
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			FTimerHandle SafetyHandle;
			World->GetTimerManager().SetTimer(SafetyHandle, FTimerDelegate::CreateLambda(DoLoad), MaxIntroWaitTime, false);
		}
	}

	Widget->PlayIntro();
}

void UDRLoadingScreenSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
	// 스테이지→로비 심리스 귀환 도착. 엔진은 심리스 트래블 최종 도착 맵에서도 이 델리게이트를 쏜다.
	// 델리게이트가 전역이라(PIE 한 프로세스에 여러 인스턴스) 이 GameInstance 의 월드인지 확인한다.
	if (bFinishOnSeamlessArrival && LoadedWorld && LoadedWorld->GetGameInstance() == GetGameInstance())
	{
		bFinishOnSeamlessArrival = false;
		FinishLoadingScreen();
		return;
	}

	// 우리가 예약한 전환이 아니면 무시(초기 부팅, 심리스 트래블 등).
	if (!PendingOutroClass)
	{
		return;
	}

	TSubclassOf<UDRLoadingScreenWidget> OutroClass = PendingOutroClass;
	PendingOutroClass = nullptr;
	BeginWidget.Reset();

	UDRLoadingScreenWidget* Widget = CreateAndShow(OutroClass);
	if (!Widget)
	{
		return;
	}

	PlayOutroAndRemove(Widget);
}

void UDRLoadingScreenSubsystem::PlayOutroAndRemove(UDRLoadingScreenWidget* Widget)
{
	if (!Widget) return;

	TWeakObjectPtr<UDRLoadingScreenSubsystem> WeakThis(this);
	TWeakObjectPtr<UDRLoadingScreenWidget> WeakWidget(Widget);

	auto RemoveWidget = [WeakThis, WeakWidget]()
	{
		if (WeakWidget.IsValid())
		{
			WeakWidget->RemoveFromParent();
		}
		if (WeakThis.IsValid() && WeakWidget.IsValid() && WeakThis->ActiveWidget == WeakWidget.Get())
		{
			WeakThis->ActiveWidget = nullptr;
		}
	};

	// 퇴장 애니 종료 시 제거
	Widget->OnOutroFinished.AddLambda(RemoveWidget);

	// ★안전장치★: WBP 가 NotifyOutroFinished 를 부르지 않아도 일정 시간 뒤 강제 제거 —
	// 없으면 로딩 화면이 게임을 영구히 덮는다 (일반 OpenLevel · 심리스 두 경로 공통)
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
	{
		FTimerHandle SafetyHandle;
		World->GetTimerManager().SetTimer(SafetyHandle, FTimerDelegate::CreateLambda(RemoveWidget), MaxIntroWaitTime, false);
	}

	Widget->PlayOutro();
}

void UDRLoadingScreenSubsystem::SetSeamlessLoadingWidgetClass(TSubclassOf<UDRLoadingScreenWidget> WidgetClass)
{
	SeamlessTravelWidgetClass = WidgetClass;
}

void UDRLoadingScreenSubsystem::HandleSeamlessTravelStart(UWorld* CurrentWorld, const FString& LevelName)
{
	// 떠나는 월드(로비)에서 위젯을 띄우고 인트로(슬라이드 인) 재생(서버/각 클라 로컬).
	// 뷰포트에 직접(persistent) 붙이므로 로비→트랜지션→스테이지 내내 유지되어 갭(검은 화면)이 없다.
	if (IsRunningDedicatedServer() || !SeamlessTravelWidgetClass)
	{
		return;
	}

	// 델리게이트가 전역이라(PIE 한 프로세스에 여러 인스턴스) 이 GameInstance 의 트래블만 처리한다.
	if (!CurrentWorld || CurrentWorld->GetGameInstance() != GetGameInstance())
	{
		return;
	}

	// 스테이지를 떠나는 심리스 트래블은 로비 귀환뿐이다(게임오버/클리어 → ADRStageGameMode::ReturnToLobby).
	// 같은 WBP 를 쓰되 문구를 바꾸고, 로비에는 닫는 쪽이 없으므로 도착 시 직접 닫도록 표시해 둔다.
	const bool bReturningToLobby = CurrentWorld->GetGameState<ADRStageGameState>() != nullptr;
	bFinishOnSeamlessArrival = bReturningToLobby;
	SeamlessLoadingMessage = bReturningToLobby
		? LOCTEXT("ReturningToLobby", "로비로 돌아가는 중...")
		: FText::GetEmpty();

	// 최소 표시 시간 계산용 시작 시각 기록.
	SeamlessShowStartTime = FPlatformTime::Seconds();

	// (CreateAndShow가 기존 유지 위젯을 먼저 제거하므로 별도 정리는 불필요)
	if (UDRLoadingScreenWidget* Widget = CreateAndShow(SeamlessTravelWidgetClass, /*bPersistent=*/true))
	{
		if (!SeamlessLoadingMessage.IsEmpty())
		{
			Widget->SetLoadingMessage(SeamlessLoadingMessage);
		}
		Widget->PlayIntro();
	}
}

void UDRLoadingScreenSubsystem::FinishLoadingScreen()
{
	// 심리스 트래블 도착(스테이지)에서 호출.
	// 유지(persistent) 위젯이 화면을 덮은 상태로, 최소 표시 시간을 채운 뒤 퇴장을 재생한다.
	if (IsRunningDedicatedServer() || !SeamlessTravelWidgetClass)
	{
		return;
	}

	// 닫을 로딩 화면이 없으면(트래블 없이 맵을 바로 연 경우 등) 퇴장 위젯을 새로 띄우지 않는다.
	if (!PersistentSlateWidget.IsValid())
	{
		return;
	}

	const double Elapsed = FPlatformTime::Seconds() - SeamlessShowStartTime;
	const double Remaining = (double)MinimumSeamlessDisplayTime - Elapsed;

	if (Remaining > 0.0)
	{
		// 아직 최소 시간 미달 → 유지 위젯이 계속 덮고 있는 채로 남은 시간 대기.
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UWorld* World = GI->GetWorld())
			{
				TWeakObjectPtr<UDRLoadingScreenSubsystem> WeakThis(this);
				World->GetTimerManager().SetTimer(SeamlessOutroTimerHandle, FTimerDelegate::CreateLambda([WeakThis]()
				{
					if (WeakThis.IsValid())
					{
						WeakThis->PlaySeamlessOutro();
					}
				}), (float)Remaining, false);
				return;
			}
		}
	}

	PlaySeamlessOutro();
}

bool UDRLoadingScreenSubsystem::IsLoadingScreenVisible() const
{
	// 유지 위젯은 뷰포트에 직접 붙어 IsInViewport 가 false 다. 퇴장 위젯은 애니가 끝나 제거될 때 ActiveWidget 이 비워진다
	return PersistentSlateWidget.IsValid() || (ActiveWidget && ActiveWidget->IsInViewport());
}

void UDRLoadingScreenSubsystem::PlaySeamlessOutro()
{
	if (IsRunningDedicatedServer() || !SeamlessTravelWidgetClass)
	{
		return;
	}

	// 유지(persistent) 위젯은 로비 월드 기준이라 stale world에서 애니메이션이 안 돌 수 있으므로,
	// 도착(스테이지) 월드에서 새 위젯을 만들어 퇴장을 재생한다.
	// 도착 월드에서 새 아웃트로 위젯 생성(일반 AddToViewport). CreateAndShow가 ActiveWidget을 갱신.
	UDRLoadingScreenWidget* Outro = CreateAndShow(SeamlessTravelWidgetClass, /*bPersistent=*/false);

	// 유지 위젯 제거(새 위젯이 이미 같은 모습으로 덮고 있으므로 깜빡임 없음).
	// 저장해 둔 Slate 인스턴스로 제거 → TakeWidget 재생성으로 인한 제거 실패 방지.
	RemovePersistentWidget();

	if (!Outro)
	{
		ActiveWidget = nullptr;
		return;
	}

	if (!SeamlessLoadingMessage.IsEmpty())
	{
		Outro->SetLoadingMessage(SeamlessLoadingMessage);
	}

	PlayOutroAndRemove(Outro);
}

#undef LOCTEXT_NAMESPACE
