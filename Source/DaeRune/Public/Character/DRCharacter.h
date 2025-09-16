// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Character/DRCharacterBase.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "DRCharacter.generated.h"

/**
 * 플레이어 캐릭터 클래스
 */
UCLASS()
class DAERUNE_API ADRCharacter : public ADRCharacterBase
{
	GENERATED_BODY()
	
public:
	ADRCharacter();
	// 서버에서 컨트롤러가 빙의될 때 호출 (서버용 GAS 초기화)
	virtual void PossessedBy(AController* NewController) override;
	// PlayerState 리플리케이션 시 호출 (클라이언트용 GAS 초기화)
	virtual void OnRep_PlayerState() override;

	// 플레이어 전용 디버프 RepNotify 함수들
	virtual void OnRep_Stunned() override;
	virtual void OnRep_Burned() override;

	// 컨테이너 시스템 설정
	UPROPERTY(EditDefaultsOnly, Category = "Container System")
	int32 NumContainers = 4;

	UPROPERTY(EditDefaultsOnly, Category = "Container System")
	float ContainerHealth = 100.f;

private:
	// GAS 초기화
	virtual void InitAbilityActorInfo() override;

	// 카메라 시스템
	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class USpringArmComponent> CameraBoom;

	UPROPERTY(VisibleAnywhere, Category = Camera)
	TObjectPtr<class UCameraComponent> FollowCamera;

public:
	IOnlineSessionPtr OnlineSessionInterface;

protected:
	UFUNCTION(BlueprintCallable)
	void CreateGameSession();

	UFUNCTION(BlueprintCallable)
	void JoinGameSession();

	void OnCreateSessionComplete(FName SessionName, bool bWasSuccessful);
	void OnFindSessionsComplete(bool bWasSuccessful);
	void OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

private:
	FOnCreateSessionCompleteDelegate CreateSessionCompleteDelegate;
	FOnFindSessionsCompleteDelegate FindSessionsCompleteDelegate;
	TSharedPtr<FOnlineSessionSearch> SessionSearch;
	FOnJoinSessionCompleteDelegate JoinSessionCompleteDelegate;
};
