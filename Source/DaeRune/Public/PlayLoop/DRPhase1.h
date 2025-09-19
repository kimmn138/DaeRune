// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "PlayLoop/DRPhaseBase.h"
#include "DRPhase1.generated.h"

class ACleanserSite;

UCLASS(BlueprintType, Blueprintable)
class DAERUNE_API UDRPhase1 : public UDRPhaseBase
{
	GENERATED_BODY()
	
public:
	//에디터에서 세팅
	UPROPERTY(EditAnywhere, Category = "Phase1")
	TSubclassOf<ACleanserSite> CleanserSiteClass;

	UPROPERTY(EditAnywhere, Category = "Phase1")
	FName SpawnPointTag = "CleanserSpawn";

	//실행중에 사용할 값
	UPROPERTY(BlueprintReadOnly)
	ACleanserSite* ActiveSite = nullptr;

	virtual void Enter() override;
	virtual void Exit()  override;

	//사이트가 완료되었다고 알려줄 때 받을 함수
	UFUNCTION()
	void OnSiteCleared(ACleanserSite* Site);

private:
	AActor* FindRandomSpawnPoint() const; // 태그로 한 번 찾기
};
