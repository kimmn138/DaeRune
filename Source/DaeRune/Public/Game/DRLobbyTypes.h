// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRLobbyTypes.generated.h"

UENUM(BlueprintType)
enum class ELobbyState : uint8
{
	WaitingRoom,     // 대기실 상태 (고정 카메라, UI Only, 캐릭터 조작 불가)
	Transitioning,   // Power On 후 카메라 전환 중
	FreeRoam         // 자유 조작 상태 (기존 로비와 동일)
};

USTRUCT(BlueprintType)
struct FWaitingRoomPlayerInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly) FString PlayerName;
	UPROPERTY(BlueprintReadOnly) EPlayerCharacterClass SelectedClass = EPlayerCharacterClass::Gardener;
	UPROPERTY(BlueprintReadOnly) bool bIsHost = false;
	UPROPERTY(BlueprintReadOnly) bool bIsLocalPlayer = false;
	UPROPERTY(BlueprintReadOnly) bool bIsReady = false;
	UPROPERTY(BlueprintReadOnly) int32 SlotIndex = 0;

	// PlayerState 참조 (킥 시 식별용)
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<APlayerState> OwningPlayerState = nullptr;
};
