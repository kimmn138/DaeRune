// Copyright DaeRune


#include "AbilitySystem/AbilityTasks/TargetDataFromCamera.h"
#include "AbilitySystemComponent.h"
#include "DaeRune/DaeRune.h"

// 태스크 생성 함수 구현부 (카메라 대상 데이터 태스크 인스턴스 생성용)
UTargetDataFromCamera* UTargetDataFromCamera::CreateTargetDataFromCamera(UGameplayAbility* OwningAbility, float TraceDistance)
{
    // AbilityTask 생성 및 최대 거리 설정
    UTargetDataFromCamera* Task = NewAbilityTask<UTargetDataFromCamera>(OwningAbility);
    Task->MaxTraceDistance = TraceDistance;
    return Task;
}

// 태스크 활성화 오버라이드 구현부 (로컬 또는 원격 제어 분기 처리용)
void UTargetDataFromCamera::Activate()
{
    // 로컬 컨트롤러 여부 확인
    const bool bIsLocallyControlled = Ability->GetCurrentActorInfo()->IsLocallyControlled();
    if (bIsLocallyControlled)
    {
        // 로컬일 경우 즉시 데이터 전송
        SendCameraTargetData();
    }
    else
    {
        // 원격일 경우 데이터 수신 대기 및 콜백 등록
        const FGameplayAbilitySpecHandle SpecHandle = GetAbilitySpecHandle();
        const FPredictionKey ActivationPredictionKey = GetActivationPredictionKey();
        AbilitySystemComponent.Get()->AbilityTargetDataSetDelegate(SpecHandle, ActivationPredictionKey).AddUObject(this, &UTargetDataFromCamera::OnTargetDataReplicatedCallback);
        const bool bCalledDelegate = AbilitySystemComponent.Get()->CallReplicatedTargetDataDelegatesIfSet(SpecHandle, ActivationPredictionKey);
        if (!bCalledDelegate)
        {
            // 원격 데이터 수신 대기 상태 설정
            SetWaitingOnRemotePlayerData();
        }
    }
}

// 카메라 기반 트레이스 수행 및 대상 데이터 전송 구현부
void UTargetDataFromCamera::SendCameraTargetData()
{
    // 예측 윈도우 범위 설정 객체 생성
    FScopedPredictionWindow ScopedPrediction(AbilitySystemComponent.Get());

    // 플레이어 뷰 위치 및 회전 정보 획득
    FVector CamLoc;
    FRotator CamRot;
    APlayerController* PC = Ability->GetCurrentActorInfo()->PlayerController.Get();
    PC->GetPlayerViewPoint(CamLoc, CamRot);

    // 트레이스 시작점 및 끝점 계산
    FVector Start = CamLoc;
    FVector End = Start + CamRot.Vector() * MaxTraceDistance;

    // 충돌 검사 설정 및 수행
    FHitResult Hit;
    FCollisionQueryParams Params;
    Params.AddIgnoredActor(Ability->GetCurrentActorInfo()->AvatarActor.Get());

    bool bHit = GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_Target, Params);

    // 대상 데이터 핸들 생성 및 히트 결과 할당
    FGameplayAbilityTargetDataHandle DataHandle;
    FGameplayAbilityTargetData_SingleTargetHit* Data = new FGameplayAbilityTargetData_SingleTargetHit();
    Data->HitResult = Hit;
    DataHandle.Add(Data);

    // 서버로 복제된 대상 데이터 전송 호출
    AbilitySystemComponent->ServerSetReplicatedTargetData(
        GetAbilitySpecHandle(),
        GetActivationPredictionKey(),
        DataHandle,
        FGameplayTag(),
        AbilitySystemComponent->ScopedPredictionKey);

    // 델리게이트 브로드캐스트 가능 여부 확인 후 이벤트 발생
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        ValidData.Broadcast(DataHandle);
    }
}

// 원격 플레이어 대상 데이터 수신 콜백 구현부
void UTargetDataFromCamera::OnTargetDataReplicatedCallback(const FGameplayAbilityTargetDataHandle& DataHandle, FGameplayTag ActivationTag)
{
    // 복제된 데이터 소비 처리
    AbilitySystemComponent->ConsumeClientReplicatedTargetData(GetAbilitySpecHandle(), GetActivationPredictionKey());
    // 델리게이트 브로드캐스트 가능 여부 확인 후 이벤트 발생
    if (ShouldBroadcastAbilityTaskDelegates())
    {
        ValidData.Broadcast(DataHandle);
    }
}
