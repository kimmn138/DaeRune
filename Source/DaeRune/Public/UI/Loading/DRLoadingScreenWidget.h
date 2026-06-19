// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "DRLoadingScreenWidget.generated.h"

// 등장/퇴장 애니메이션 종료를 알리는 C++ 전용 델리게이트 (서브시스템이 구독)
DECLARE_MULTICAST_DELEGATE(FDROnLoadingAnimFinished);

/**
 * 맵 전환 로딩 화면의 베이스 위젯.
 *
 * 사용 방식:
 *  - 위젯 블루프린트(WBP)가 이 클래스를 상속하여 전환별로 서로 다른 모습을 구현한다.
 *  - 위젯의 "기본(애니 없는) 레이아웃" = 화면을 꽉 채운 '표시 완료' 상태로 디자인한다.
 *      · 등장 애니(PlayIntro)는 화면 밖 → 기본 상태로 들어오는 연출
 *      · 퇴장 애니(PlayOutro)는 기본 상태 → 화면 밖으로 나가는 연출
 *  - 등장 애니가 끝나면 WBP에서 NotifyIntroFinished()를, 퇴장 애니가 끝나면
 *    NotifyOutroFinished()를 반드시 호출해야 한다(애니 시퀀스의 OnFinished 등에서).
 *  - 로드 멈춤 구간에서도 회전이 유지되어야 하는 스피너는 UMG 애니메이션이 아니라
 *    Slate 기반 위젯(Throbber / Circular Throbber)으로 만든다.
 */
UCLASS(Abstract, Blueprintable)
class DAERUNE_API UDRLoadingScreenWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 등장 애니메이션 재생 요청. WBP에서 구현(패널 오른→왼 슬라이드 인 + 스피너 아래→위).
	UFUNCTION(BlueprintImplementableEvent, Category = "Loading")
	void PlayIntro();

	// 퇴장 애니메이션 재생 요청. WBP에서 구현(스피너 아래로 퇴장 + 패널 왼쪽으로 슬라이드 아웃).
	UFUNCTION(BlueprintImplementableEvent, Category = "Loading")
	void PlayOutro();

	// 등장 애니가 끝났을 때 WBP에서 호출.
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void NotifyIntroFinished();

	// 퇴장 애니가 끝났을 때 WBP에서 호출.
	UFUNCTION(BlueprintCallable, Category = "Loading")
	void NotifyOutroFinished();

	FDROnLoadingAnimFinished OnIntroFinished;
	FDROnLoadingAnimFinished OnOutroFinished;
};
