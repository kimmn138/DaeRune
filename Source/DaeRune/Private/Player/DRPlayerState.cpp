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
    DOREPLIFETIME(ADRPlayerState, EquippedSkinIds);
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

		// ★입은 채로 스테이지★ — 새 폰의 PossessedBy 가 이 목록을 끌어가 입힌다 (Plan(옷장) M3)
		DRPS->EquippedSkinIds = EquippedSkinIds;

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

    // 옷도 같은 이유로 한 번 더 (폰이 이미 붙어 있는 경우)
    RefreshCosmeticVisuals();

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

    // ★Plan3(공용칩) §17.6.3★ 회복 차단 칩이 새로 켜졌으면 ★이미 돌고 있는★ 재생 GE 를 즉시 뗀다.
    // 로비에서 칩을 끼고 스테이지에 들어오면 이전 GE 가 살아 있을 수 있는데,
    // StartHealthRegen 의 가드만으로는 그걸 막지 못한다.
    if (HasAuthority()
        && CachedUpgradeRuntime.HasFeature(FDRGameplayTags::Get().Upgrade_Feature_Combat_NoHealing))
    {
        StopHealthRegen();
    }

    // ★Plan3(공용칩)★ 수치가 아닌 기능 스위치(HUD 표시 등)를 UI 가 다시 읽게 한다.
    // RefreshUpgradeEffects() 는 어트리뷰트만 건드리므로 이 통지가 따로 필요하다.
    OnUpgradeRuntimeChanged.Broadcast();
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

// ========================= 코스메틱 (Plan(옷장) M3 · D-26) =========================

void ADRPlayerState::SetEquippedSkinIds(const TArray<FName>& InSkinIds)
{
    if (!HasAuthority()) return;

    EquippedSkinIds = InSkinIds;

    // ★리슨 서버 호스트에서는 RepNotify 가 불리지 않는다★ — 서버에서 직접 입힌다 (Plan(옷장) T-23)
    RefreshCosmeticVisuals();

    // 대기실 디스플레이 캐릭터는 PlayerState 가 없어 위 경로를 못 탄다 — 로비면 다시 실어 준다 (Plan(옷장) M5 · T-24)
    if (ADRLobbyGameMode* LobbyGM = GetWorld() ? GetWorld()->GetAuthGameMode<ADRLobbyGameMode>() : nullptr)
    {
        LobbyGM->RefreshDisplayCharacterSkin(Cast<AController>(GetOwner()));
    }
}

void ADRPlayerState::OnRep_EquippedSkinIds()
{
    RefreshCosmeticVisuals();
}

void ADRPlayerState::RefreshCosmeticVisuals()
{
    // 프리뷰 · 대기실 캐릭터는 PlayerState 가 없어 이 경로를 타지 않는다 — 인게임 폰 전용
    if (ADRCharacter* DRCharacter = Cast<ADRCharacter>(GetPawn()))
    {
        DRCharacter->ApplyCosmeticSkins(EquippedSkinIds);
    }
}

void ADRPlayerState::ReportCosmeticIfLocal()
{
    // ReportUpgradeLoadoutIfLocal 과 같은 탐색 — Owner 포인터는 클라에서 늦게 복제될 수 있어 쓰지 않는다
    UWorld* World = GetWorld();
    if (!World) return;

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        ADRPlayerController* DRPC = Cast<ADRPlayerController>(It->Get());
        if (!DRPC || !DRPC->IsLocalController()) continue;
        if (DRPC->PlayerState != this) continue;

        DRPC->ReportCosmeticLoadout();
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

float ADRPlayerState::GetEffectiveCombatExitDelay() const
{
    // 칩이 없으면 Apply() 가 항등이라 CombatExitDelay 가 그대로 나온다.
    const float Upgraded = CachedUpgradeRuntime.Apply(EDRUpgradeStat::HealthRegenDelay, CombatExitDelay);
    return FMath::Max(MinCombatExitDelay, Upgraded);
}

void ADRPlayerState::EnterCombat()
{
    if (!HasAuthority()) return;

    // ★두 SetTimer 가 같은 값을 쓰게 한 번만 계산한다.★
    // 한쪽만 고치면 "첫 피격은 4.5초, 연속 피격은 5.0초" 처럼 조용히 갈린다.
    const float ExitDelay = GetEffectiveCombatExitDelay();

    if (bIsInCombat)
    {
        // �̹� ���� ���̸� Ÿ�̸Ӹ� ����
        GetWorld()->GetTimerManager().SetTimer(
            CombatTimerHandle,
            this,
            &ADRPlayerState::CheckCombatExit,
            ExitDelay,
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
        ExitDelay,
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

    // ★Plan3(공용칩) §17.6.3★ 회복 차단 칩이면 재생 GE 자체를 얹지 않는다.
    // (이미 붙어 있던 GE 는 RebuildUpgradeRuntime 이 떼어내고, 틱은 MMC 가 0 으로 막는다)
    if (CachedUpgradeRuntime.HasFeature(FDRGameplayTags::Get().Upgrade_Feature_Combat_NoHealing)) return;

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

	// 옷도 같다 — 자판기 옷을 청소기가 입는 사고 방지 (Plan(옷장) M3)
	EquippedSkinIds.Reset();
	RefreshCosmeticVisuals();

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

	// ★리슨 서버 호스트★ 는 OnRep_SelectedPlayerClass 가 불리지 않아 새 로봇의 칩·옷을 보고할 기회가 없다 —
	// 여기서 바로 보고한다. 원격 클라이언트의 PlayerState 면 이 머신에 그 컨트롤러가 없어 아무 일도 없다.
	// (칩: 레벨 진입 보고(OnLevelEntered)가 기본 클래스로 먼저 나간 뒤 GameMode 가 저장된 클래스로
	//  바꾸면서 위에서 EquippedChips 를 비운다. 재보고가 없으면 칩 기기를 열고 닫기 전까지 칩이 0개다.)
	ReportUpgradeLoadoutIfLocal();
	ReportCosmeticIfLocal();
}

void ADRPlayerState::OnRep_SelectedPlayerClass()
{
	// 칩 해석은 소속 클래스로 필터링되므로 클래스가 바뀌면 캐시를 다시 만들어야 한다
	RebuildUpgradeRuntime();

	// 소유 클라이언트: 새 클래스의 장착 목록을 서버에 보고
	ReportUpgradeLoadoutIfLocal();
	ReportCosmeticIfLocal();

	OnPlayerClassChanged.Broadcast(this, SelectedPlayerClass);
}
