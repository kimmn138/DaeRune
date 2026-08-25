// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameStateBase.h"
#include "Game/DRLobbyTypes.h"
#include "DRLobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomCodeGenerated, const FString&, RoomCode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLobbyStateChanged, ELobbyState, NewState);

/**
 * �κ� ���� GameState
 * �� �ڵ�, ĳ���� ����, �÷��̾� ��� �� �κ� ��� ����
 */
UCLASS()
class DAERUNE_API ADRLobbyGameState : public ADRGameStateBase
{
	GENERATED_BODY()
	
public:
	ADRLobbyGameState();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// �� �ڵ� ����
	UFUNCTION(BlueprintCallable, Category = "Lobby|Room")
	FString GetRoomCode() const { return RoomCode; }

	UFUNCTION(BlueprintCallable, Category = "Lobby|Room")
	bool HasRoomCode() const { return !RoomCode.IsEmpty(); }

	void SetRoomCode(const FString& NewRoomCode);

	// 로비 상태 조회
	UFUNCTION(BlueprintCallable, Category = "Lobby|State")
	ELobbyState GetLobbyState() const { return LobbyState; }

	// 로비 상태 설정 (서버 전용)
	void SetLobbyState(ELobbyState NewState);

	// 호스트를 제외한 모든 플레이어가 준비 상태인지 확인 (비호스트가 없으면 true)
	UFUNCTION(BlueprintPure, Category = "Lobby|State")
	bool AreAllNonHostPlayersReady() const;

	// 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnRoomCodeGenerated OnRoomCodeGenerated;

	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnLobbyStateChanged OnLobbyStateChanged;

protected:
	// 로비 상태
	UPROPERTY(ReplicatedUsing = OnRep_LobbyState, BlueprintReadOnly, Category = "Lobby")
	ELobbyState LobbyState = ELobbyState::WaitingRoom;

	UFUNCTION()
	void OnRep_LobbyState();
	// �� �ڵ�
	UPROPERTY(ReplicatedUsing = OnRep_RoomCode, BlueprintReadOnly, Category = "Lobby")
	FString RoomCode;

	UFUNCTION()
	void OnRep_RoomCode();

	// �ּ� ���� �ο�
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Lobby")
	int32 MinPlayersToStart;

private:
	// 룸 코드 초기화
	UFUNCTION()
	void InitializeRoomCode(const FString& NewRoomCode);

	// 룸 코드 브로드캐스트 (서버/클라이언트 공용)
	void BroadcastRoomCode();
};
