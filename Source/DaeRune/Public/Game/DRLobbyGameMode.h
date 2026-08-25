// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Game/DRGameModeBase.h"
#include "AbilitySystem/Data/CharacterClassInfo.h"
#include "DRLobbyGameMode.generated.h"

class ADRWaitingRoomCameraActor;
class ADRCharacter;

/**
 *
 */
UCLASS()
class DAERUNE_API ADRLobbyGameMode : public ADRGameModeBase
{
	GENERATED_BODY()
	
public:
	ADRLobbyGameMode();

	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	// SeamlessTravel로 돌아온 플레이어 처리
	virtual void HandleSeamlessTravelPlayer(AController*& C) override;

	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void TravelToStage(const FString& StageMapName, class ADRPlayerController* Requester);

	// Power On 처리 (호스트만 호출 가능)
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void PowerOn(class ADRPlayerController* Requester);

	// 플레이어 준비 상태 설정 (클라이언트 요청 처리, 서버 전용)
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void SetPlayerReady(class ADRPlayerController* Player, bool bReady);

	// 플레이어 킥 (호스트만 호출 가능)
	UFUNCTION(BlueprintCallable, Category = "Lobby")
	void KickPlayer(class ADRPlayerController* Requester, class ADRPlayerController* TargetPlayer);

	// 플레이어 클래스 변경 시 폰 교체 (서버 전용)
	void RespawnPlayerWithClass(class ADRPlayerController* PC, EPlayerCharacterClass NewClass);

	// 플레이어별 DefaultPawnClass 결정
	virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;

protected:
	virtual void BeginPlay() override;
	virtual void HandleWipeout() override;

	// 세션 참가 허용
	void AllowJoinInProgress();

	// 세션 참가 차단
	void BlockJoinInProgress();

	// 로비 재시작
	void RestartLobby();

	// ========== 대기실 슬롯 시스템 ==========

	// 대기실 카메라 (BeginPlay에서 레벨에서 탐색)
	UPROPERTY()
	TObjectPtr<ADRWaitingRoomCameraActor> WaitingRoomCamera;

	// 대기실 슬롯 마커 (ATargetPoint with "WaitingRoomSlot" tag, 액터 이름순 정렬)
	UPROPERTY()
	TArray<TObjectPtr<AActor>> WaitingRoomSlots;

	// 플레이어 → 슬롯 인덱스 매핑
	TMap<AController*, int32> PlayerSlotMap;

	// 다음 사용 가능한 슬롯 인덱스
	int32 NextAvailableSlot = 0;

	// 플레이어를 슬롯에 배치
	void AssignPlayerToSlot(AController* Player);

	// 모든 플레이어 재배치 (킥 후 빈 자리 정리)
	void RepositionAllPlayers();

	// 플레이어 폰을 슬롯 위치로 텔레포트 + 이동 비활성화
	void PositionPawnAtSlot(APawn* Pawn, int32 SlotIndex);

	// 모든 클라이언트에게 대기실 UI 갱신 요청
	void BroadcastRefreshWaitingRoomUI();

	// 대기실 카메라 + 슬롯 마커 탐색
	void FindWaitingRoomActors();

	// ========== 디스플레이 캐릭터 시스템 ==========

	// 플레이어 → 디스플레이 캐릭터 매핑 (대기실에서 비점유 캐릭터 표시용)
	TMap<AController*, TWeakObjectPtr<ADRCharacter>> DisplayCharacterMap;

	// 디스플레이 캐릭터 스폰 (비점유, 이동 비활성화)
	void SpawnDisplayCharacter(AController* Player, EPlayerCharacterClass CharClass, int32 SlotIndex);

	// 클래스 변경 시 디스플레이 캐릭터 교체
	void UpdateDisplayCharacter(AController* Player, EPlayerCharacterClass NewClass);

	// 특정 플레이어의 디스플레이 캐릭터 제거
	void DestroyDisplayCharacter(AController* Player);

	// 모든 디스플레이 캐릭터 제거
	void DestroyAllDisplayCharacters();

private:
	void ExecuteTravel(const FString& StageMapName);
};
