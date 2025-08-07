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
}

// 전투 상태 돌입
void ADRPlayerState::EnterCombat()
{
    if (!HasAuthority()) return;

    // 이미 전투 중이면 타이머만 리셋
    if (bIsInCombat)
    {
        GetWorld()->GetTimerManager().SetTimer(
            CombatTimerHandle,
            this,
            &ADRPlayerState::CheckCombatExit,
            CombatExitDelay,
            false
        );
        return;
    }

    // 전투 상태 진입
    bIsInCombat = true;

    // 체력 회복은 중단
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

    // 체력 회복이 중단되어 있지 않으므로 시작할 필요 없음
    // 하지만 혹시 모르니 체크
    if (!HealthRegenEffectHandle.IsValid())
    {
        StartHealthRegen();
    }

    OnCombatStateChanged.Broadcast(false);

    UE_LOG(LogTemp, Log, TEXT("ExitCombat - Player exited combat state"));
}

int32 ADRPlayerState::GetCurrentContainerIndex() const
{
    if (const UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSet))
    {
        const float Health = PlayerAS->GetHealth();
        if (Health <= 0.f) return -1; // 부패 상태

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

bool ADRPlayerState::IsPlayerCorrupted() const
{
    if (const UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSet))
    {
        return PlayerAS->IsCorrupted();
    }
    return false;
}

void ADRPlayerState::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        InitializeHealthRegenSpec();

        // 게임 시작 시 무조건 체력 회복 시작
        StartHealthRegen();
    }
}

void ADRPlayerState::OnRep_IsInCombat()
{
    // 클라이언트에서 상태 변경 알림
    OnCombatStateChanged.Broadcast(bIsInCombat);

    UE_LOG(LogTemp, Log, TEXT("OnRep_IsInCombat - Combat state changed to: %s"),
        bIsInCombat ? TEXT("In Combat") : TEXT("Out of Combat"));
}

void ADRPlayerState::StartHealthRegen()
{
    if (!HasAuthority())
    {
        UE_LOG(LogTemp, Warning, TEXT("StartHealthRegen - No authority"));
        return;
    }

    if (!CachedHealthRegenSpec.IsValid())
    {
        UE_LOG(LogTemp, Warning, TEXT("StartHealthRegen - CachedHealthRegenSpec is invalid"));
        InitializeHealthRegenSpec(); // 재시도
        if (!CachedHealthRegenSpec.IsValid())
        {
            return;
        }
    }

    // 이미 활성화된 회복 효과가 있으면 제거
    if (HealthRegenEffectHandle.IsValid())
    {
        AbilitySystemComponent->RemoveActiveGameplayEffect(HealthRegenEffectHandle);
        HealthRegenEffectHandle.Invalidate();
    }

    // 새로운 회복 효과 적용
    HealthRegenEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CachedHealthRegenSpec.Data.Get());

    if (HealthRegenEffectHandle.IsValid())
    {
        UE_LOG(LogTemp, Log, TEXT("StartHealthRegen - Health regeneration started"));
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("StartHealthRegen - Failed to apply health regen effect"));
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
    if (!HealthRegenEffectClass)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeHealthRegenSpec - HealthRegenEffectClass is null!"));
        return;
    }

    if (!AbilitySystemComponent)
    {
        UE_LOG(LogTemp, Error, TEXT("InitializeHealthRegenSpec - AbilitySystemComponent is null!"));
        return;
    }

    FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
    EffectContext.AddSourceObject(this);
    CachedHealthRegenSpec = AbilitySystemComponent->MakeOutgoingSpec(HealthRegenEffectClass, 1.f, EffectContext);

    UE_LOG(LogTemp, Log, TEXT("InitializeHealthRegenSpec - Health regen spec initialized"));
}
