// Copyright DaeRune


#include "Player/DRPlayerState.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "DRGameplayTags.h"

ADRPlayerState::ADRPlayerState()
{
    // GAS 컴포넌트 생성 및 설정
	AbilitySystemComponent = CreateDefaultSubobject<UDRAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UDRPlayerAttributeSet>("AttributeSet");
	
    // 높은 업데이트 빈도로 실시간 동기화
	NetUpdateFrequency = 100.f;
}

UAbilitySystemComponent* ADRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 전투 및 부패 상태 리플리케이션
    DOREPLIFETIME(ADRPlayerState, bIsInCombat);
    DOREPLIFETIME(ADRPlayerState, bIsCorrupted);
}

void ADRPlayerState::BeginPlay()
{
    Super::BeginPlay();

    // 서버에서만 체력 변경 콜백 등록
    if (HasAuthority())
    {
        // 체력 회복 스펙 미리 캐싱
        InitializeHealthRegenSpec();

        // 게임 시작 시 정상 상태이므로 체력 회복 체크
        CheckAndStartHealthRegen();

        // 체력 변경 감지 콜백 등록
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

    if (bIsCorrupted || bIsInCombat) return;

    // 체력이 변경될 때마다 회복 상태 재평가
    CheckHealthRegenStatus();
}

void ADRPlayerState::CheckHealthRegenStatus()
{
    if (!HasAuthority() || bIsCorrupted || bIsInCombat) return;

    // PlayerAttributeSet에서 컨테이너 상태 확인
    const UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSet);
    if (!PlayerAS) return;

    const float CurrentHealth = PlayerAS->GetHealth();
    const int32 ContainerIndex = PlayerAS->GetCurrentContainerIndex();
    const float ContainerHealth = PlayerAS->GetContainerHealth();
    const float ContainerMax = (ContainerIndex + 1) * ContainerHealth;

    // 현재 컨테이너가 가득 찼는지 확인
    const bool bIsContainerFull = FMath::IsNearlyEqual(CurrentHealth, ContainerMax, 0.1f);

    if (bIsContainerFull)
    {
        // 컨테이너가 가득 차면 체력 회복 중지
        if (HealthRegenEffectHandle.IsValid())
        {
            StopHealthRegen();
        }
    }
    else
    {
        // 컨테이너가 가득 차지 않았으면 체력 회복 시작
        if (!HealthRegenEffectHandle.IsValid())
        {
            StartHealthRegen();
        }
    }
}

void ADRPlayerState::EnterCombat()
{
    if (!HasAuthority()) return;

    if (bIsInCombat)
    {
        // 이미 전투 중이면 타이머만 리셋
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

    StopHealthRegen();

    // 전투 종료 타이머 설정
    GetWorld()->GetTimerManager().SetTimer(
        CombatTimerHandle,
        this,
        &ADRPlayerState::CheckCombatExit,
        CombatExitDelay,
        false
    );

    // 클라이언트에 알림
    OnCombatStateChanged.Broadcast(true);
}

void ADRPlayerState::CheckCombatExit()
{
    if (!HasAuthority()) return;

    // 타이머 만료 시 전투 종료
    ExitCombat();
}

void ADRPlayerState::ExitCombat()
{
    if (!HasAuthority() || !bIsInCombat) return;

    // 전투 상태 해제
    bIsInCombat = false;
    GetWorld()->GetTimerManager().ClearTimer(CombatTimerHandle);

    // 클라이언트 알림
    OnCombatStateChanged.Broadcast(false);

    // 체력 회복 재개 체크
    CheckAndStartHealthRegen();
}

int32 ADRPlayerState::GetCurrentContainerIndex() const
{
    // PlayerAttributeSet의 컨테이너 시스템 연동
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
        // 부패 상태 진입
        StopHealthRegen();

        // 부패 태그 추가
        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->AddLooseGameplayTag(FDRGameplayTags::Get().State_Corrupt);
        }
    }
    else
    {
        // 부패 상태 해제
        CheckAndStartHealthRegen();

        // 부패 태그 제거
        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->RemoveLooseGameplayTag(FDRGameplayTags::Get().State_Corrupt);
        }
    }

    // 클라이언트 알림
    OnCorruptedStateChanged.Broadcast(bIsCorrupted);
}

void ADRPlayerState::CheckAndStartHealthRegen()
{
    if (!HasAuthority() || bIsCorrupted) return;

    // 현재 체력 상태를 확인한 후 회복 시작 여부 결정
    CheckHealthRegenStatus();
}

bool ADRPlayerState::IsPlayerCorrupted() const
{
    return bIsCorrupted;
}

void ADRPlayerState::OnRep_IsInCombat()
{
    // 클라이언트에서 전투 상태 변경 알림
    OnCombatStateChanged.Broadcast(bIsInCombat);
}

void ADRPlayerState::OnRep_IsCorrupted()
{
    // 클라이언트에서 부패 태그 동기화
    if (AbilitySystemComponent)
    {
        if (bIsCorrupted)
        {
            AbilitySystemComponent->AddLooseGameplayTag(FDRGameplayTags::Get().State_Corrupt);
        }
        else
        {
            AbilitySystemComponent->RemoveLooseGameplayTag(FDRGameplayTags::Get().State_Corrupt);
        }
    }

    // 클라이언트 알림
    OnCorruptedStateChanged.Broadcast(bIsCorrupted);
}

void ADRPlayerState::StartHealthRegen()
{
    if (!HasAuthority() || bIsCorrupted) return;

    // 이미 실행 중이면 무시
    if (HealthRegenEffectHandle.IsValid()) return;

    // 캐시된 스펙 검증
    if (!CachedHealthRegenSpec.IsValid())
    {
        InitializeHealthRegenSpec();
        if (!CachedHealthRegenSpec.IsValid()) return;
    }

    // 체력 회복 효과 적용
    HealthRegenEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CachedHealthRegenSpec.Data.Get());
}

void ADRPlayerState::StopHealthRegen()
{
    if (!HasAuthority()) return;

    // 활성화된 체력 회복 효과 제거
    if (HealthRegenEffectHandle.IsValid())
    {
        AbilitySystemComponent->RemoveActiveGameplayEffect(HealthRegenEffectHandle);
        HealthRegenEffectHandle.Invalidate();
    }
}

void ADRPlayerState::InitializeHealthRegenSpec()
{
    if (!HealthRegenEffectClass || !AbilitySystemComponent) return;

    // 성능 최적화를 위한 GameplayEffect 스펙 사전 캐싱
    FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
    EffectContext.AddSourceObject(this);
    CachedHealthRegenSpec = AbilitySystemComponent->MakeOutgoingSpec(HealthRegenEffectClass, 1.f, EffectContext);
}
