// Copyright DaeRune


#include "Player/DRPlayerState.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "DRGameplayTags.h"

ADRPlayerState::ADRPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UDRAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UDRPlayerAttributeSet>("AttributeSet");
	
	NetUpdateFrequency = 100.f;
}

UAbilitySystemComponent* ADRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADRPlayerState, bIsInCombat);
    DOREPLIFETIME(ADRPlayerState, bIsCorrupted);
}

void ADRPlayerState::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        InitializeHealthRegenSpec();

        // 게임 시작 시 정상 상태이므로 체력 회복 체크
        CheckAndStartHealthRegen();

        // 체력 변경 감지를 위한 델리게이트 바인딩
        if (const UDRAttributeSet* DRAS = Cast<UDRAttributeSet>(AttributeSet))
        {
            AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
                DRAS->GetHealthAttribute()).AddUObject(this, &ADRPlayerState::OnHealthChanged);
        }
    }
}

void ADRPlayerState::OnHealthChanged(const FOnAttributeChangeData& Data)
{
    if (!HasAuthority()) return;

    // 부패 상태면 무시
    if (bIsCorrupted || bIsInCombat) return;

    // 현재 컨테이너 체크
    CheckHealthRegenStatus();
}

void ADRPlayerState::CheckHealthRegenStatus()
{
    if (!HasAuthority() || bIsCorrupted) return;

    const UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSet);
    if (!PlayerAS) return;

    const float CurrentHealth = PlayerAS->GetHealth();

    // 현재 컨테이너 계산
    int32 ContainerIndex = FMath::FloorToInt(CurrentHealth / UDRPlayerAttributeSet::CONTAINER_HEALTH);
    if (FMath::IsNearlyEqual(CurrentHealth, ContainerIndex * UDRPlayerAttributeSet::CONTAINER_HEALTH))
    {
        ContainerIndex = FMath::Max(0, ContainerIndex - 1);
    }
    ContainerIndex = FMath::Clamp(ContainerIndex, 0, UDRPlayerAttributeSet::NUM_CONTAINERS - 1);

    const float ContainerMax = (ContainerIndex + 1) * UDRPlayerAttributeSet::CONTAINER_HEALTH;

    // 현재 컨테이너가 가득 찼는지 체크
    const bool bIsContainerFull = FMath::IsNearlyEqual(CurrentHealth, ContainerMax, 0.1f);

    if (bIsContainerFull)
    {
        // 컨테이너가 가득 차면 체력 재생 중지
        if (HealthRegenEffectHandle.IsValid())
        {
            StopHealthRegen();
            UE_LOG(LogTemp, Log, TEXT("Container Full - Health regen stopped at %.1f"), CurrentHealth);
        }
    }
    else
    {
        // 컨테이너가 가득 차지 않았으면 체력 재생 시작
        if (!HealthRegenEffectHandle.IsValid())
        {
            StartHealthRegen();
            UE_LOG(LogTemp, Log, TEXT("Container Not Full - Health regen started at %.1f"), CurrentHealth);
        }
    }
}

// 전투 상태 돌입
void ADRPlayerState::EnterCombat()
{
    if (!HasAuthority()) return;

    if (bIsInCombat)
    {
        // 타이머 리셋
        GetWorld()->GetTimerManager().SetTimer(
            CombatTimerHandle,
            this,
            &ADRPlayerState::CheckCombatExit,
            CombatExitDelay,
            false
        );
        return;
    }

    bIsInCombat = true;

    StopHealthRegen();

    // 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(
        CombatTimerHandle,
        this,
        &ADRPlayerState::CheckCombatExit,
        CombatExitDelay,
        false
    );

    OnCombatStateChanged.Broadcast(true);

    UE_LOG(LogTemp, Log, TEXT("EnterCombat - Player entered combat state"));
}

void ADRPlayerState::CheckCombatExit()
{
    if (!HasAuthority()) return;

    // 전투 종료 조건 확인 (10초간 전투 행동이 없었음)
    ExitCombat();
}

void ADRPlayerState::ExitCombat()
{
    if (!HasAuthority() || !bIsInCombat) return;

    bIsInCombat = false;

    GetWorld()->GetTimerManager().ClearTimer(CombatTimerHandle);

    // 비전투 상태가 되면 체력 재생 체크
    CheckAndStartHealthRegen();

    OnCombatStateChanged.Broadcast(false);

    UE_LOG(LogTemp, Log, TEXT("ExitCombat - Player exited combat state"));
}

int32 ADRPlayerState::GetCurrentContainerIndex() const
{
    if (const UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSet))
    {
        const float Health = PlayerAS->GetHealth();
        if (Health <= 0.f) return -1;

        constexpr float ContainerHealth = 100.f;
        constexpr int32 NumContainers = 4;

        int32 ContainerIndex = FMath::FloorToInt(Health / ContainerHealth);
        if (FMath::IsNearlyEqual(Health, ContainerIndex * ContainerHealth))
        {
            ContainerIndex = FMath::Max(0, ContainerIndex - 1);
        }

        return FMath::Clamp(ContainerIndex, 0, NumContainers - 1);
    }

    return 0;
}

void ADRPlayerState::SetCorruptedState(bool bNewCorrupted)
{
    if (!HasAuthority()) return;

    if (bIsCorrupted == bNewCorrupted) return;

    bIsCorrupted = bNewCorrupted;

    if (bIsCorrupted)
    {
        // 부패 상태 진입: 체력 회복 중지
        StopHealthRegen();

        // 태그 추가 (Gameplay Effect 상호작용용)
        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->AddLooseGameplayTag(FDRGameplayTags::Get().State_Corrupt);
        }

        UE_LOG(LogTemp, Log, TEXT("Entered Corrupted State - Health regen stopped"));
    }
    else
    {
        // 부패 상태 해제: 체력 회복 체크 후 재개
        CheckAndStartHealthRegen();

        // 태그 제거
        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->RemoveLooseGameplayTag(FDRGameplayTags::Get().State_Corrupt);
        }

        UE_LOG(LogTemp, Log, TEXT("Exited Corrupted State - Health regen resumed"));
    }

    // 클라이언트에 알림
    OnCorruptedStateChanged.Broadcast(bIsCorrupted);
}

void ADRPlayerState::CheckAndStartHealthRegen()
{
    if (!HasAuthority() || bIsCorrupted) return;

    // 현재 체력 상태 체크 후 재생 시작 여부 결정
    CheckHealthRegenStatus();
}

bool ADRPlayerState::IsPlayerCorrupted() const
{
    return bIsCorrupted;
}

void ADRPlayerState::OnRep_IsInCombat()
{
    // 클라이언트에서 상태 변경 알림
    OnCombatStateChanged.Broadcast(bIsInCombat);

    UE_LOG(LogTemp, Log, TEXT("OnRep_IsInCombat - Combat state changed to: %s"),
        bIsInCombat ? TEXT("In Combat") : TEXT("Out of Combat"));
}

void ADRPlayerState::OnRep_IsCorrupted()
{
    OnCorruptedStateChanged.Broadcast(bIsCorrupted);

    UE_LOG(LogTemp, Log, TEXT("OnRep_IsCorrupted - Corrupted state: %s"),
        bIsCorrupted ? TEXT("True") : TEXT("False"));
}

void ADRPlayerState::StartHealthRegen()
{
    if (!HasAuthority() || bIsCorrupted) return; // 부패 상태 체크 추가

    // 이미 실행 중이면 무시
    if (HealthRegenEffectHandle.IsValid()) return;

    if (!CachedHealthRegenSpec.IsValid())
    {
        InitializeHealthRegenSpec();
        if (!CachedHealthRegenSpec.IsValid())
        {
            return;
        }
    }

    HealthRegenEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CachedHealthRegenSpec.Data.Get());

    if (HealthRegenEffectHandle.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("StartHealthRegen - Health regeneration started"));
    }
}

void ADRPlayerState::StopHealthRegen()
{
    if (!HasAuthority()) return;

    if (HealthRegenEffectHandle.IsValid())
    {
        AbilitySystemComponent->RemoveActiveGameplayEffect(HealthRegenEffectHandle);
        HealthRegenEffectHandle.Invalidate();

        UE_LOG(LogTemp, Log, TEXT("StopHealthRegen - Health regeneration stopped"));
    }
}

void ADRPlayerState::InitializeHealthRegenSpec()
{
    if (!HealthRegenEffectClass || !AbilitySystemComponent) return;

    FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
    EffectContext.AddSourceObject(this);
    CachedHealthRegenSpec = AbilitySystemComponent->MakeOutgoingSpec(HealthRegenEffectClass, 1.f, EffectContext);

    UE_LOG(LogTemp, Log, TEXT("InitializeHealthRegenSpec - Health regen spec initialized"));
}
