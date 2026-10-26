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

	// 접속 시 호스트 플래그 확정 (리슨 서버의 로컬 컨트롤러 소유 PlayerState)
	virtual void AddPlayerState(APlayerState* PlayerState) override;

	// 호스트 확인
	UFUNCTION(BlueprintCallable, Category = "Game State")
	APlayerState* GetHostPlayer() const;

	UFUNCTION(BlueprintCallable, Category = "Game State")
	bool IsPlayerHost(APlayerState* PlayerState) const;

	// 살아있는 플레이어 리스트 반환
	UFUNCTION(BlueprintCallable, Category = "Spectating")
	TArray<ADRCharacter*> GetAlivePlayers() const;
};
