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
    if (!HasAuthority() || bIsCorrupted || bIsInCombat) return;

    const UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSet);
    if (!PlayerAS) return;

    const float CurrentHealth = PlayerAS->GetHealth();
    const int32 ContainerIndex = PlayerAS->GetCurrentContainerIndex();
    const float ContainerHealth = PlayerAS->GetContainerHealth();
    const float ContainerMax = (ContainerIndex + 1) * ContainerHealth;

    // 현재 컨테이너가 가득 찬지 체크
    const bool bIsContainerFull = FMath::IsNearlyEqual(CurrentHealth, ContainerMax, 0.1f);

    if (bIsContainerFull)
    {
        // 컨테이너가 가득 차면 체력 재생 중지
        if (HealthRegenEffectHandle.IsValid())
        {
            StopHealthRegen();
        }
    }
    else
    {
        // 컨테이너가 가득 차지 않았으면 체력 재생 시작
        if (!HealthRegenEffectHandle.IsValid())
        {
            StartHealthRegen();
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

    // 1. 먼저 전투 상태 플래그 변경
    bIsInCombat = false;

    // 2. 타이머 정리
    GetWorld()->GetTimerManager().ClearTimer(CombatTimerHandle);

    // 3. 클라이언트에 알림
    OnCombatStateChanged.Broadcast(false);

    UE_LOG(LogTemp, Log, TEXT("ExitCombat - Player exited combat state"));

    // 4. 마지막에 체력 재생 체크 (bIsInCombat이 false가 된 후)
    CheckAndStartHealthRegen();
}

int32 ADRPlayerState::GetCurrentContainerIndex() const
{
    if (const UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSet))
    {
        return PlayerAS->GetCurrentContainerIndex();
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
}

void ADRPlayerState::OnRep_IsCorrupted()
{
    OnCorruptedStateChanged.Broadcast(bIsCorrupted);
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
