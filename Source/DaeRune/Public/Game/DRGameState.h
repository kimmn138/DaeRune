// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Net/UnrealNetwork.h"
#include "DRGameState.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAllPlayersDead);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerDied, ADRPlayerState*, DeadPlayer);

class ADRPlayerState;
class UDRAttributeSet;

/**
 * DaeRune의 GameState 클래스
 * 게임 전체 상태 정보를 관리하고 모든 클라이언트에 리플리케이션
 */
UCLASS()
class DAERUNE_API ADRGameState : public AGameState
{
	GENERATED_BODY()
	
	public:
	ADRGameState();

	virtual void BeginPlay() override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// PlayerArray 오버라이드 - DRPlayerState 타입 보장
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	// 방 코드 관련
	UFUNCTION(BlueprintCallable, Category = "Network|Room")
	FString GetRoomCode() const { return RoomCode; }

	UFUNCTION(BlueprintCallable, Category = "Network|Room")
	bool HasRoomCode() const { return !RoomCode.IsEmpty(); }

	void SetRoomCode(const FString& NewRoomCode);

	// 게임 상태 관련
	UFUNCTION(BlueprintCallable, Category = "Game State")
	bool IsGameInProgress() const { return bIsGameInProgress; }

	UFUNCTION(BlueprintCallable, Category = "Game State")
	float GetGameTime() const { return GameTime; }

	UFUNCTION(BlueprintCallable, Category = "Game State")
	int32 GetAlivePlayersCount() const { return AlivePlayersCount; }

	UFUNCTION(BlueprintCallable, Category = "Game State")
	int32 GetDeadPlayersCount() const { return DeadPlayersCount; }

	UFUNCTION(BlueprintCallable, Category = "Game State")
	bool AreAllPlayersDead() const { return AlivePlayersCount == 0 && TotalPlayersCount > 0; }

	// DRPlayerState 배열 접근
	UFUNCTION(BlueprintCallable, Category = "Game State")
	TArray<ADRPlayerState*> GetDRPlayerArray() const;

	UFUNCTION(BlueprintCallable, Category = "Game State")
	TArray<ADRPlayerState*> GetAlivePlayerArray() const;

	// 게임 진행 관련 함수들
	void StartGame();
	void EndGame(bool bVictory);
	void UpdateAlivePlayersCount();

	// 플레이어 상태 체크
	UFUNCTION(BlueprintCallable, Category = "Game State")
	bool IsPlayerAlive(ADRPlayerState* PlayerState) const;

	// 델리게이트
	UPROPERTY(BlueprintAssignable, Category = "Game State")
	FOnAllPlayersDead OnAllPlayersDead;

	UPROPERTY(BlueprintAssignable, Category = "Game State")
	FOnPlayerDied OnPlayerDied;

protected:
	// 방 코드 (호스트가 생성, 모든 클라이언트에 리플리케이션)
	UPROPERTY(ReplicatedUsing = OnRep_RoomCode, BlueprintReadOnly, Category = "Network")
	FString RoomCode;

	UFUNCTION()
	void OnRep_RoomCode();

	// 게임 진행 상태
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	bool bIsGameInProgress;

	// 게임 진행 시간
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	float GameTime;

	// 생존 플레이어 수
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	int32 AlivePlayersCount;

	// 사망 플레이어 수
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	int32 DeadPlayersCount;

	// 전체 플레이어 수
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	int32 TotalPlayersCount;

	// 게임 종료 여부 (모든 플레이어 사망)
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	bool bIsGameOver;

private:
	// 방 코드 초기화 (서버에서만)
	void InitializeRoomCode();

	// 게임 시간 업데이트 타이머
	FTimerHandle GameTimeUpdateHandle;

	// 플레이어 상태 체크 타이머
	FTimerHandle PlayerStateCheckHandle;

	UFUNCTION()
	void UpdateGameTime();

	UFUNCTION()
	void CheckPlayersHealth();

	// 이전 생존자 수 (변화 감지용)
	int32 LastAlivePlayersCount;
};
