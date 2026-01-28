// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRBGMActor.generated.h"

class UAudioComponent;
class USoundBase;
class USoundClass;

/**
 * 맵 전체에서 동기화된 BGM을 재생하는 액터
 * 서버 시간 기반으로 모든 플레이어가 같은 시점의 BGM을 들음
 */
UCLASS()
class DAERUNE_API ADRBGMActor : public AActor
{
	GENERATED_BODY()

public:
	ADRBGMActor();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// BGM 재생 (서버에서 호출)
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void PlayBGM();

	// BGM 정지
	UFUNCTION(BlueprintCallable, Category = "BGM")
	void StopBGM(float FadeOutDuration = 1.0f);

protected:
	virtual void BeginPlay() override;

	// BGM 사운드
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	TObjectPtr<USoundBase> BGMSound;

	// 시작 시 자동 재생
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	bool bAutoPlay = true;

	// 페이드 인 시간
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM")
	float FadeInDuration = 2.0f;

	// 볼륨
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BGM", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Volume = 1.0f;

	// BGM 시작 서버 시간 (복제됨)
	UPROPERTY(ReplicatedUsing = OnRep_BGMStartTime)
	float BGMStartServerTime = 0.0f;

	UFUNCTION()
	void OnRep_BGMStartTime();

private:
	UPROPERTY()
	TObjectPtr<UAudioComponent> AudioComponent;

	// 로컬에서 BGM 재생 (동기화된 시간 적용)
	void PlayBGMLocal();

	// BGM 길이 (초)
	float GetBGMDuration() const;
};
