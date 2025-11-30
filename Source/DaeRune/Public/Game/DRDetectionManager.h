// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "DRDetectionManager.generated.h"

class ADRCharacter;
class ADREnemy;

UCLASS()
class DAERUNE_API ADRDetectionManager : public AActor
{
	GENERATED_BODY()
	
public:
    ADRDetectionManager();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    // 탐지 범위
    UPROPERTY(EditDefaultsOnly, Category = "Detection")
    float DetectionRange = 3000.f;

    // 체크 주기
    UPROPERTY(EditDefaultsOnly, Category = "Detection")
    float CheckInterval = 1.0f;

    // 플레이어 시야각
    UPROPERTY(EditDefaultsOnly, Category = "Detection")
    float PlayerFOVAngle = 120.f;

    // 라인트레이스 콜리전 채널
    UPROPERTY(EditDefaultsOnly, Category = "Detection")
    TEnumAsByte<ECollisionChannel> VisibilityChannel = ECC_Visibility;

    FTimerHandle DetectionTimerHandle;

    // 시야각 코사인 값
    float FOVCosine;

    // 탐지 범위 제곱
    float DetectionRangeSq;

    // 주기적으로 호출되는 탐지 체크
    void PerformDetectionCheck();

    // 현재 살아있는 플레이어 목록 수집
    TArray<ADRCharacter*> GetAlivePlayers() const;

    // 플레이어들 주변의 적 수집 (중복 제거)
    TArray<ADREnemy*> GetEnemiesInDetectionRange(const TArray<ADRCharacter*>& Players) const;

    // 특정 플레이어가 특정 적을 볼 수 있는지 체크
    bool CanPlayerSeeEnemy(const ADRCharacter* Player, const ADREnemy* Enemy) const;

    // 적의 발각 태그 업데이트
    void UpdateEnemyDetectionTag(ADREnemy* Enemy, bool bIsDetected) const;
};
