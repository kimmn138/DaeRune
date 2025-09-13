// Copyright DaeRune


#include "Actor/DRWaterSource.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Character/DRCharacter.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

ADRWaterSource::ADRWaterSource()
{

}

void ADRWaterSource::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADRWaterSource, bIsAvailable);
}

void ADRWaterSource::OnOverlap(AActor* TargetActor)
{
    // 블루프린트에서 이 함수를 호출하면 기본 동작 수행
    // 블루프린트에서 추가 로직을 앞뒤에 넣을 수 있음

    if (!HasAuthority()) return;
    if (!bIsAvailable) return;

    // 플레이어면 물 채우기 시도
    if (ADRCharacter* PlayerCharacter = Cast<ADRCharacter>(TargetActor))
    {
        FillPlayerWater(PlayerCharacter);
    }
}

void ADRWaterSource::OnEndOverlap(AActor* TargetActor)
{
    // 블루프린트에서 필요시 오버라이드
}

void ADRWaterSource::FillPlayerWater(AActor* TargetActor)
{
    if (!HasAuthority()) return;
    if (!bIsAvailable) return;

    ADRCharacter* PlayerCharacter = Cast<ADRCharacter>(TargetActor);
    if (!PlayerCharacter) return;

    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PlayerCharacter);
    if (!TargetASC) return;

    const UDRAttributeSet* TargetAS = Cast<UDRAttributeSet>(
        TargetASC->GetAttributeSet(UDRAttributeSet::StaticClass())
    );
    if (!TargetAS) return;

    // 이미 물이 가득 차있으면 무시
    if (FMath::IsNearlyEqual(TargetAS->GetWater(), TargetAS->GetMaxWater()))
    {
        return;
    }

    // 물 채우기 효과 적용
    if (WaterFillEffectClass)
    {
        FGameplayEffectContextHandle EffectContext = TargetASC->MakeEffectContext();
        EffectContext.AddSourceObject(this);

        FGameplayEffectSpecHandle SpecHandle = TargetASC->MakeOutgoingSpec(
            WaterFillEffectClass,
            1.f,
            EffectContext
        );

        if (SpecHandle.IsValid())
        {
            TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

            // 사용됨 알림
            SetWaterSourceAvailable(false);
            StartRechargeTimer();

            // 블루프린트 이벤트 호출
            OnWaterSourceUsed(PlayerCharacter);
        }
    }
}

void ADRWaterSource::SetWaterSourceAvailable(bool bNewAvailable)
{
    if (!HasAuthority()) return;

    if (bIsAvailable != bNewAvailable)
    {
        bIsAvailable = bNewAvailable;
        OnRep_bIsAvailable(); // 서버에서도 호출
    }
}

void ADRWaterSource::StartRechargeTimer()
{
    if (!HasAuthority()) return;

    GetWorld()->GetTimerManager().SetTimer(
        RechargeTimerHandle,
        this,
        &ADRWaterSource::OnSourceRecharged,
        RechargeDuration,
        false
    );
}

void ADRWaterSource::OnSourceRecharged()
{
    if (!HasAuthority()) return;

    SetWaterSourceAvailable(true);

    // 블루프린트 이벤트 호출
    OnWaterSourceRecharged();
}

void ADRWaterSource::OnRep_bIsAvailable()
{
    // 블루프린트 이벤트 호출
    OnAvailabilityChanged(bIsAvailable);
}
