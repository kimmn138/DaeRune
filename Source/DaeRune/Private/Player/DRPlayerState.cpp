// Copyright DaeRune


#include "Player/DRPlayerState.h"
#include "AbilitySystem/DRAbilitySystemComponent.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRPlayerAttributeSet.h"
#include "AbilitySystem/Data/GameBalanceConfig.h"
#include "Net/UnrealNetwork.h"
#include "DRGameplayTags.h"
#include "Game/DRLobbyGameMode.h"
#include "Game/DRGameInstance.h"
#include "Game/DRChipCatalog.h"
#include "Character/DRCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Player/DRPlayerController.h"

ADRPlayerState::ADRPlayerState()
{
    // GAS ������Ʈ ���� �� ����
	AbilitySystemComponent = CreateDefaultSubobject<UDRAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UDRPlayerAttributeSet>("AttributeSet");
	
    // ���� ������Ʈ �󵵷� �ǽð� ����ȭ
	// (ASC/어트리뷰트 복제 채널 - UI 표시용이라 100Hz는 과도, 50Hz로 충분)
	SetNetUpdateFrequency(50.f);
	SetMinNetUpdateFrequency(10.f);
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
    DOREPLIFETIME(ADRPlayerState, bIsReady);
    DOREPLIFETIME(ADRPlayerState, bIsHost);
    DOREPLIFETIME(ADRPlayerState, EquippedChips);
    DOREPLIFETIME(ADRPlayerState, StageKillCount);
}

void ADRPlayerState::SetIsHost(bool bNewIsHost)
{
	if (!HasAuthority()) return;

	bIsHost = bNewIsHost;
}

void ADRPlayerState::CopyProperties(APlayerState* PlayerState)
{
	Super::CopyProperties(PlayerState);

	if (ADRPlayerState* DRPS = Cast<ADRPlayerState>(PlayerState))
	{
		DRPS->SelectedPlayerClass = SelectedPlayerClass;
		DRPS->WaitingRoomSlotIndex = WaitingRoomSlotIndex;
		DRPS->bIsHost = bIsHost;

		// 로비↔스테이지 이동 시 장착 칩을 보존한다 (스폰 시점에 이미 알고 있어야 하므로)
		DRPS->EquippedChips = EquippedChips;
		DRPS->RebuildUpgradeRuntime();

		// StageKillCount 는 스테이지 한정 값이라 복사하지 않는다
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

    // 복제로 이미 도착한 장착 목록이 있으면 캐시를 만든다 (OnRep 이 BeginPlay 보다 앞설 수 있음)
    RebuildUpgradeRuntime();

    // 최초 보고는 PlayerController 쪽(OnRep_PlayerState / OnLevelEntered)이 담당한다.
    // PlayerState 가 먼저 도착하고 컨트롤러 연결이 늦는 순서를 그쪽이 흡수한다. (Plan2.md 7.1)
}

// ========================= 업그레이드 칩 =========================

void ADRPlayerState::SetEquippedChips(const TArray<FName>& InChips)
{
    if (!HasAuthority()) return;

    EquippedChips = InChips;
    RebuildUpgradeRuntime();
}

void ADRPlayerState::OnRep_EquippedChips()
{
    RebuildUpgradeRuntime();
}

void ADRPlayerState::RebuildUpgradeRuntime()
{
    CachedUpgradeRuntime.Reset();

    if (const UDRGameInstance* GI = Cast<UDRGameInstance>(UGameplayStatics::GetGameInstance(this)))
    {
        if (const UDRChipCatalog* Catalog = GI->GetChipCatalog())
        {
            Catalog->ResolveLoadout(EquippedChips, SelectedPlayerClass, CachedUpgradeRuntime);
        }
    }

    // 이미 스폰된 폰이 있으면 즉시 반영한다.
    // 보고가 스폰보다 늦게 도착해도 자동으로 치유되는 경로. (Plan2.md 7.2)
    if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetPawn()))
    {
        DRCharacter->RefreshUpgradeEffects();
    }
}

void ADRPlayerState::ReportUpgradeLoadoutIfLocal()
{
    // Owner 포인터는 클라이언트에서 늦게 복제될 수 있어 신뢰하지 않는다.
    // 이 머신의 로컬 컨트롤러 중 나를 PlayerState 로 가진 것을 찾는다.
    UWorld* World = GetWorld();
    if (!World) return;

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        ADRPlayerController* DRPC = Cast<ADRPlayerController>(It->Get());
        if (!DRPC || !DRPC->IsLocalController()) continue;
        if (DRPC->PlayerState != this) continue;

        DRPC->ReportUpgradeLoadout();
        return;
    }
}

// ========================= 스테이지 처치 수 =========================

void ADRPlayerState::AddStageKill()
{
    if (!HasAuthority()) return;

    ++StageKillCount;
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
	RefreshLocalWaitingRoomUI();
}

// 로컬 플레이어의 대기실 UI 갱신 (어느 PlayerState의 복제 콜백에서든 공용)
void ADRPlayerState::RefreshLocalWaitingRoomUI() const
{
	if (UWorld* World = GetWorld())
	{
		if (ADRPlayerController* LocalPC = Cast<ADRPlayerController>(World->GetFirstPlayerController()))
		{
			LocalPC->RefreshWaitingRoomUI();
		}
	}
}

void ADRPlayerState::SetReady(bool bNewReady)
{
	if (!HasAuthority()) return;
	if (bIsReady == bNewReady) return;

	bIsReady = bNewReady;
	ForceNetUpdate();

	OnReadyStateChanged.Broadcast(bIsReady);
}

void ADRPlayerState::OnRep_IsReady()
{
	OnReadyStateChanged.Broadcast(bIsReady);

	// 어떤 플레이어의 준비 상태가 바뀌든 이 클라이언트의 대기실 목록을 갱신
	RefreshLocalWaitingRoomUI();
}

void ADRPlayerState::SetSelectedPlayerClass(EPlayerCharacterClass NewClass)
{
	if (!HasAuthority()) return;
	if (SelectedPlayerClass == NewClass) return;

	SelectedPlayerClass = NewClass;

	// 이전 로봇의 칩이 남아 있으면 안 된다. 새 목록은 클라이언트의 OnRep 보고로 채워진다.
	EquippedChips.Reset();
	RebuildUpgradeRuntime();

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
	// 칩 해석은 소속 클래스로 필터링되므로 클래스가 바뀌면 캐시를 다시 만들어야 한다
	RebuildUpgradeRuntime();

	// 소유 클라이언트: 새 클래스의 장착 목록을 서버에 보고
	ReportUpgradeLoadoutIfLocal();

	OnPlayerClassChanged.Broadcast(this, SelectedPlayerClass);
}
