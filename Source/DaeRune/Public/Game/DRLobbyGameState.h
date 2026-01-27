// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameStateBase.h"
#include "DRLobbyGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnRoomCodeGenerated, const FString&, RoomCode);

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

	// ��������Ʈ
	UPROPERTY(BlueprintAssignable, Category = "Lobby")
	FOnRoomCodeGenerated OnRoomCodeGenerated;

protected:
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
