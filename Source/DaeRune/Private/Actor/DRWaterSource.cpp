// Copyright DaeRune


#include "Actor/DRWaterSource.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Character/DRCharacter.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Sound/DRSoundManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/DRSoundDataAsset.h"
#include "DRAssetManager.h"
#include "NiagaraComponent.h"

ADRWaterSource::ADRWaterSource()
{
    AvailableVFXComponent1 = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AvailableVFX1"));
    AvailableVFXComponent1->SetupAttachment(RootComponent);
    AvailableVFXComponent1->bAutoActivate = false;

    AvailableVFXComponent2 = CreateDefaultSubobject<UNiagaraComponent>(TEXT("AvailableVFX2"));
    AvailableVFXComponent2->SetupAttachment(RootComponent);
    AvailableVFXComponent2->bAutoActivate = false;
}

void ADRWaterSource::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADRWaterSource, bIsAvailable);
}

void ADRWaterSource::MulticastPlayWaterGainSound_Implementation()
{
    // Actor의 World를 직접 사용해서 사운드 재생 (클라이언트에서 확실히 동작)
    if (UDRAssetManager* AssetManager = Cast<UDRAssetManager>(UAssetManager::GetIfInitialized()))
    {
        if (UDRSoundDataAsset* SoundData = AssetManager->GetSoundDataAsset())
        {
            if (SoundData->WaterGainSound)
            {
                UGameplayStatics::PlaySoundAtLocation(this, SoundData->WaterGainSound, GetActorLocation());
            }
        }
    }
}

void ADRWaterSource::OnOverlap(AActor* TargetActor)
{
    if (!HasAuthority()) return;
    if (!bIsAvailable) return;

    // �÷��̾�� �� ä��� �õ�
    if (ADRCharacter* PlayerCharacter = Cast<ADRCharacter>(TargetActor))
    {
        FillPlayerWater(PlayerCharacter);
    }
}

void ADRWaterSource::OnEndOverlap(AActor* TargetActor)
{
    // ��������Ʈ���� �ʿ�� �������̵�
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

    // �̹� ���� ���� �������� ����
    if (FMath::IsNearlyEqual(TargetAS->GetWater(), TargetAS->GetMaxWater()))
    {
        return;
    }

    // �� ä��� ȿ�� ����
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

            MulticastPlayWaterGainSound();

            // ���� �˸�
            SetWaterSourceAvailable(false);
            StartRechargeTimer();

            // ��������Ʈ �̺�Ʈ ȣ��
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
        UpdateAvailabilityVFX(bIsAvailable);
        OnRep_bIsAvailable();
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

    // ��������Ʈ �̺�Ʈ ȣ��
    OnWaterSourceRecharged();
}

void ADRWaterSource::BeginPlay()
{
    Super::BeginPlay();

    if (AvailableVFXSystem1 && AvailableVFXComponent1)
    {
        AvailableVFXComponent1->SetAsset(AvailableVFXSystem1);
    }

    if (AvailableVFXSystem2 && AvailableVFXComponent2)
    {
        AvailableVFXComponent2->SetAsset(AvailableVFXSystem2);
    }

    UpdateAvailabilityVFX(bIsAvailable);
}

void ADRWaterSource::UpdateAvailabilityVFX(bool bAvailable)
{
    if (AvailableVFXComponent1)
    {
        if (bAvailable)
        {
            AvailableVFXComponent1->Activate(true);
        }
        else
        {
            AvailableVFXComponent1->Deactivate();
        }
    }

    if (AvailableVFXComponent2)
    {
        if (bAvailable)
        {
            AvailableVFXComponent2->Activate(true);
        }
        else
        {
            AvailableVFXComponent2->Deactivate();
        }
    }
}

void ADRWaterSource::OnRep_bIsAvailable()
{
    UpdateAvailabilityVFX(bIsAvailable);
    OnAvailabilityChanged(bIsAvailable);
}
