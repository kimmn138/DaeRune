// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "UI/Widget/DRUserWidget.h"
#include "Game/DRLobbyTypes.h"
#include "DRWaitingRoomWidget.generated.h"

/**
 * 대기실 UI의 C++ 베이스 클래스.
 * 블루프린트에서 상속받아 WBP_WaitingRoom으로 구현.
 */
UCLASS()
class DAERUNE_API UDRWaitingRoomWidget : public UDRUserWidget
{
	GENERATED_BODY()

public:
	// 플레이어 슬롯 정보 갱신 (블루프린트에서 UI 업데이트 구현)
	UFUNCTION(BlueprintImplementableEvent, Category = "WaitingRoom")
	void RefreshPlayerSlots(const TArray<FWaitingRoomPlayerInfo>& PlayerInfos);

	// 로비 상태 변경 시 호출
	UFUNCTION(BlueprintImplementableEvent, Category = "WaitingRoom")
	void OnLobbyStateChanged(ELobbyState NewState);

	// 호스트 여부 설정 (Kick/PowerOn 버튼 표시 제어)
	UFUNCTION(BlueprintImplementableEvent, Category = "WaitingRoom")
	void SetIsHost(bool bIsHost);
};
