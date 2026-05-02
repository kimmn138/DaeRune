// Copyright DaeRune


#include "Player/DRPlayerState.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "AbilitySystem/Data/GameBalanceConfig.h"
#include "Net/UnrealNetwork.h"
#include "DRGameplayTags.h"
#include "Game/DRLobbyGameMode.h"
#include "Player/DRPlayerController.h"

ADRPlayerState::ADRPlayerState()
{
    // GAS ������Ʈ ���� �� ����
	AbilitySystemComponent = CreateDefaultSubobject<UDRAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UDRPlayerAttributeSet>("AttributeSet");
	
    // ���� ������Ʈ �󵵷� �ǽð� ����ȭ
	NetUpdateFrequency = 100.f;
}

UAbilitySystemComponent* ADRPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ADRPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // ���� �� ���� ���� ���ø����̼�
    DOREPLIFETIME(ADRPlayerState, bIsInCombat);
    DOREPLIFETIME(ADRPlayerState, bIsCorrupted);
    DOREPLIFETIME(ADRPlayerState, WaitingRoomSlotIndex);
    DOREPLIFETIME(ADRPlayerState, SelectedPlayerClass);
}

void ADRPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (ADRPlayerState* DRPS = Cast<ADRPlayerState>(PlayerState))
	{
		DRPS->SelectedPlayerClass = SelectedPlayerClass;
		DRPS->WaitingRoomSlotIndex = WaitingRoomSlotIndex;
	}
}

void ADRPlayerState::BeginPlay()
{
    Super::BeginPlay();

    // ���������� ü�� ���� �ݹ� ���
    if (HasAuthority())
    {
        // GameBalanceConfig에서 밸런스 값 적용
        if (const UGameBalanceConfig* BalanceConfig = UDRAbilitySystemLibrary::GetGameBalanceConfig(this))
        {
            CombatExitDelay = BalanceConfig->PlayerCombat.CombatExitDelay;
        }

        // ü�� ȸ�� ���� �̸� ĳ��
        InitializeHealthRegenSpec();

        // ���� ���� �� ���� �����̹Ƿ� ü�� ȸ�� üũ
        CheckAndStartHealthRegen();

        // ü�� ���� ���� �ݹ� ���
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

    // ü���� ����� ������ ȸ�� ���� ����
    CheckHealthRegenStatus();
}

void ADRPlayerState::CheckHealthRegenStatus()
{
    if (!HasAuthority() || bIsCorrupted || bIsInCombat) return;

    // PlayerAttributeSet���� �����̳� ���� Ȯ��
    const UDRPlayerAttributeSet* PlayerAS = Cast<UDRPlayerAttributeSet>(AttributeSet);
    if (!PlayerAS) return;

    const float CurrentHealth = PlayerAS->GetHealth();
    const int32 ContainerIndex = PlayerAS->GetCurrentContainerIndex();
    const float ContainerHealth = PlayerAS->GetContainerHealth();
    const float ContainerMax = (ContainerIndex + 1) * ContainerHealth;

    // ���� �����̳ʰ� ���� á���� Ȯ��
    const bool bIsContainerFull = FMath::IsNearlyEqual(CurrentHealth, ContainerMax, 0.1f);

    if (bIsContainerFull)
    {
        // �����̳ʰ� ���� ���� ü�� ȸ�� ����
        if (HealthRegenEffectHandle.IsValid())
        {
            StopHealthRegen();
        }
    }
    else
    {
        // �����̳ʰ� ���� ���� �ʾ����� ü�� ȸ�� ����
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
        // �̹� ���� ���̸� Ÿ�̸Ӹ� ����
        GetWorld()->GetTimerManager().SetTimer(
            CombatTimerHandle,
            this,
            &ADRPlayerState::CheckCombatExit,
            CombatExitDelay,
            false
        );
        return;
    }

    // ���� ���� ����
    bIsInCombat = true;

    // GAS 태그 추가
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->AddLooseGameplayTag(FDRGameplayTags::Get().State_InCombat);
    }

    StopHealthRegen();

    // ���� ���� Ÿ�̸� ����
    GetWorld()->GetTimerManager().SetTimer(
        CombatTimerHandle,
        this,
        &ADRPlayerState::CheckCombatExit,
        CombatExitDelay,
        false
    );

    // Ŭ���̾�Ʈ�� �˸�
    OnCombatStateChanged.Broadcast(true);
}

void ADRPlayerState::CheckCombatExit()
{
    if (!HasAuthority()) return;

    // Ÿ�̸� ���� �� ���� ����
    ExitCombat();
}

void ADRPlayerState::ExitCombat()
{
    if (!HasAuthority() || !bIsInCombat) return;

    // ���� ���� ����
    bIsInCombat = false;
    GetWorld()->GetTimerManager().ClearTimer(CombatTimerHandle);

    // GAS 태그 제거
    if (AbilitySystemComponent)
    {
        AbilitySystemComponent->RemoveLooseGameplayTag(FDRGameplayTags::Get().State_InCombat);
    }

    // Ŭ���̾�Ʈ �˸�
    OnCombatStateChanged.Broadcast(false);

    // ü�� ȸ�� �簳 üũ
    CheckAndStartHealthRegen();
}

int32 ADRPlayerState::GetCurrentContainerIndex() const
{
    // PlayerAttributeSet�� �����̳� �ý��� ����
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
        // ���� ���� ����
        StopHealthRegen();

        // ���� �±� �߰�
        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->AddLooseGameplayTag(FDRGameplayTags::Get().State_Corrupt);
        }
    }
    else
    {
        // ���� ���� ����
        CheckAndStartHealthRegen();

        // ���� �±� ����
        if (AbilitySystemComponent)
        {
            AbilitySystemComponent->RemoveLooseGameplayTag(FDRGameplayTags::Get().State_Corrupt);
        }
    }

    // Ŭ���̾�Ʈ �˸�
    OnCorruptedStateChanged.Broadcast(bIsCorrupted);
}

void ADRPlayerState::CheckAndStartHealthRegen()
{
    if (!HasAuthority() || bIsCorrupted) return;

    // ���� ü�� ���¸� Ȯ���� �� ȸ�� ���� ���� ����
    CheckHealthRegenStatus();
}

bool ADRPlayerState::IsPlayerCorrupted() const
{
    return bIsCorrupted;
}

void ADRPlayerState::OnRep_IsInCombat()
{
    // 클라이언트에서 GAS 태그 동기화
    if (AbilitySystemComponent)
    {
        if (bIsInCombat)
        {
            AbilitySystemComponent->AddLooseGameplayTag(FDRGameplayTags::Get().State_InCombat);
        }
        else
        {
            AbilitySystemComponent->RemoveLooseGameplayTag(FDRGameplayTags::Get().State_InCombat);
        }
    }

    // Ŭ���̾�Ʈ���� ���� ���� ���� �˸�
    OnCombatStateChanged.Broadcast(bIsInCombat);
}

void ADRPlayerState::OnRep_IsCorrupted()
{
    // Ŭ���̾�Ʈ���� ���� �±� ����ȭ
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

    // Ŭ���̾�Ʈ �˸�
    OnCorruptedStateChanged.Broadcast(bIsCorrupted);
}

void ADRPlayerState::StartHealthRegen()
{
    if (!HasAuthority() || bIsCorrupted) return;

    // �̹� ���� ���̸� ����
    if (HealthRegenEffectHandle.IsValid()) return;

    // ĳ�õ� ���� ����
    if (!CachedHealthRegenSpec.IsValid())
    {
        InitializeHealthRegenSpec();
        if (!CachedHealthRegenSpec.IsValid()) return;
    }

    // ü�� ȸ�� ȿ�� ����
    HealthRegenEffectHandle = AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CachedHealthRegenSpec.Data.Get());
}

void ADRPlayerState::StopHealthRegen()
{
    if (!HasAuthority()) return;

    // Ȱ��ȭ�� ü�� ȸ�� ȿ�� ����
    if (HealthRegenEffectHandle.IsValid())
    {
        AbilitySystemComponent->RemoveActiveGameplayEffect(HealthRegenEffectHandle);
        HealthRegenEffectHandle.Invalidate();
    }
}

void ADRPlayerState::InitializeHealthRegenSpec()
{
    if (!HealthRegenEffectClass || !AbilitySystemComponent) return;

    // ���� ����ȭ�� ���� GameplayEffect ���� ���� ĳ��
    FGameplayEffectContextHandle EffectContext = AbilitySystemComponent->MakeEffectContext();
    EffectContext.AddSourceObject(this);
    CachedHealthRegenSpec = AbilitySystemComponent->MakeOutgoingSpec(HealthRegenEffectClass, 1.f, EffectContext);
}

void ADRPlayerState::SetWaitingRoomSlotIndex(int32 NewIndex)
{
	if (!HasAuthority()) return;
	if (WaitingRoomSlotIndex == NewIndex) return;
	WaitingRoomSlotIndex = NewIndex;
	ForceNetUpdate();
}

void ADRPlayerState::OnRep_WaitingRoomSlotIndex()
{
	if (APlayerController* PC = Cast<APlayerController>(GetOwner()))
	{
		if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
		{
			if (DRPC->IsLocalController())
			{
				DRPC->RefreshWaitingRoomUI();
			}
		}
	}
}

void ADRPlayerState::SetSelectedPlayerClass(EPlayerCharacterClass NewClass)
{
	if (!HasAuthority()) return;
	if (SelectedPlayerClass == NewClass) return;

	SelectedPlayerClass = NewClass;
	OnPlayerClassChanged.Broadcast(this, SelectedPlayerClass);

	// 로비 대기실에서만 폰 교체 실행
	ADRLobbyGameMode* LobbyGM = GetWorld()->GetAuthGameMode<ADRLobbyGameMode>();
	if (LobbyGM)
	{
		APlayerController* PC = Cast<APlayerController>(GetOwner());
		if (ADRPlayerController* DRPC = Cast<ADRPlayerController>(PC))
		{
			LobbyGM->RespawnPlayerWithClass(DRPC, NewClass);
		}
	}
}

void ADRPlayerState::OnRep_SelectedPlayerClass()
{
	OnPlayerClassChanged.Broadcast(this, SelectedPlayerClass);
}
