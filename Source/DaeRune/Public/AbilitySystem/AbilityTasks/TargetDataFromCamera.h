// Copyright DaeRune

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "GameplayTagContainer.h"
#include "TargetDataFromCamera.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCameraTargetDataSignature, const FGameplayAbilityTargetDataHandle&, DataHandle);

/**
 * 카메라 시점에서 트레이스한 대상 데이터 생성 태스크 클래스 선언
 */
UCLASS()
class DAERUNE_API UTargetDataFromCamera : public UAbilityTask
{
	GENERATED_BODY()
	
public:
    // 태스크 생성 함수 선언 (카메라 기반 대상 데이터 생성용)
    UFUNCTION(BlueprintCallable, Category = "Ability|Tasks", meta = (DisplayName = "TargetDataFromCamera", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
    static UTargetDataFromCamera* CreateTargetDataFromCamera(UGameplayAbility* OwningAbility, float TraceDistance);

    // 유효 대상 데이터 브로드캐스트용 델리게이트
    UPROPERTY(BlueprintAssignable)
    FCameraTargetDataSignature ValidData;

private:
    // 최대 트레이스 거리 변수
    float MaxTraceDistance;

    // 태스크 활성화 시 실행되는 함수 오버라이드
    virtual void Activate() override;
    // 카메라 기준 트레이스 후 대상 데이터 전송 함수
    void SendCameraTargetData();

    // 원격 플레이어로부터 대상 데이터 수신 콜백 함수
    void OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ActivationTag);
};
