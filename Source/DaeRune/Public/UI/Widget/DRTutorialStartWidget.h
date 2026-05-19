// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "DRTutorialStartWidget.generated.h"

/**
 * 튜토리얼 시작 메뉴 위젯의 C++ 베이스 클래스
 * 블루프린트에서 상속받아 WBP_TutorialStartMenu로 구현
 */
UCLASS()
class DAERUNE_API UDRTutorialStartWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// "Start Game" 버튼 클릭 시 호출
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void OnStartGameClicked();

	// "Skip Tutorial" 버튼 클릭 시 호출
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void OnSkipTutorialClicked();

	// "Settings" 버튼 클릭 시 호출
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void OnSettingsClicked();

	// "Quit Game" 버튼 클릭 시 호출
	UFUNCTION(BlueprintCallable, Category = "Tutorial")
	void OnQuitClicked();
};
