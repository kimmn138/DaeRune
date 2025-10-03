// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameStateBase.h"
#include "DRLobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomCodeGenerated, const FString&, RoomCode);

/**
 * 로비 전용 GameState
 * 방 코드, 캐릭터 선택, 플레이어 목록 등 로비 기능 관리
 */
UCLASS()
class DAERUNE_API ADRLobbyGameState : public ADRGameStateBase
{
	GENERATED_BODY()
	
public:
	ADRLobbyGameState();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// 방 코드 관련
	UFUNCTION(BlueprintCallable, Category = "Lobby|Room")
	FString GetRoomCode() const { return RoomCode; }

	UFUNCTION(BlueprintCallable, Category = "Lobby|Room")
	bool HasRoomCode() const { return !RoomCode.IsEmpty(); }

	void SetRoomCode(const FString& NewRoomCode);

	// 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnRoomCodeGenerated OnRoomCodeGenerated;

protected:
	// 방 코드
	UPROPERTY(ReplicatedUsing = OnRep_RoomCode, BlueprintReadOnly, Category = "Lobby")
	FString RoomCode;

	UFUNCTION()
	void OnRep_RoomCode();

	// 최소 시작 인원
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby")
	int32 MinPlayersToStart;

private:
	// 방 코드 초기화
	UFUNCTION()
	void InitializeRoomCode(const FString& NewRoomCode);
};
