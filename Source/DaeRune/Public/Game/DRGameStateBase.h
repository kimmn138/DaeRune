// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameState.h"
#include "Net/UnrealNetwork.h"
#include "DRGameStateBase.generated.h"

class ADRCharacter;

/**
 * DaeRune의 GameState 베이스 클래스
 * 로비와 스테이지에서 공통으로 사용하는 기능 포함
 */
UCLASS()
class DAERUNE_API ADRGameStateBase : public AGameState
{
	GENERATED_BODY()
	
public:
	ADRGameStateBase();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// PlayerArray 오버라이드 - DRPlayerState 타입 보장
	virtual void AddPlayerState(APlayerState* PlayerState) override;
	virtual void RemovePlayerState(APlayerState* PlayerState) override;

	// 현재 접속 인원
	UFUNCTION(BlueprintCallable, Category = "Game State")
	int32 GetCurrentPlayerCount() const { return CurrentPlayerCount; }

	// 최대 플레이어 수
	UFUNCTION(BlueprintCallable, Category = "Game State")
	int32 GetMaxPlayerCount() const { return MaxPlayerCount; }

	// 호스트 확인
	UFUNCTION(BlueprintCallable, Category = "Game State")
	APlayerState* GetHostPlayer() const;

	UFUNCTION(BlueprintCallable, Category = "Game State")
	bool IsPlayerHost(APlayerState* PlayerState) const;

	// 살아있는 플레이어 리스트 반환
	UFUNCTION(BlueprintCallable, Category = "Spectating")
	TArray<ADRCharacter*> GetAlivePlayers() const;

protected:
	// 현재 플레이어 수
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	int32 CurrentPlayerCount;

	// 최대 플레이어 수
	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Game State")
	int32 MaxPlayerCount;

	// 플레이어 수 업데이트
	virtual void UpdatePlayerCount();
};
