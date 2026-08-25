// Copyright DaeRune


#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/DRPlayerState.h"
#include "DRGameplayTags.h"
#include "DaeRune/DRLogChannels.h"

bool UDRGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
    // �⺻ Cost üũ
    if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
    {
        return false;
    }

    // Water Cost�� ������ ���
    const float EffectiveWaterCost = GetEffectiveWaterCostFor(ActorInfo);
    if (EffectiveWaterCost <= 0.f)
    {
        return true;
    }

    // AttributeSet ��������
    UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
    if (!ASC)
    {
        return false;
    }

    const UDRAttributeSet* AttributeSet = Cast<UDRAttributeSet>(
        ASC->GetAttributeSet(UDRAttributeSet::StaticClass()));
    if (!AttributeSet)
    {
        return false;
    }

    float CurrentWater = AttributeSet->GetWater();
    float CurrentHealth = AttributeSet->GetHealth();

    // Water�� ����� ���
    if (CurrentWater >= EffectiveWaterCost)
    {
        return true;
    }

    // Water�� ������ ���, Health�� ���� �������� Ȯ��
    float WaterShortage = EffectiveWaterCost - CurrentWater;
    float RequiredHealth = FMath::FloorToFloat(WaterShortage * 0.5f);

    // Health�� �ּ� 1 �̻� ���� �� �ִ��� Ȯ��
    return CurrentHealth > RequiredHealth;
}

void UDRGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    // �⺻ Cost ����
    Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

    const float EffectiveWaterCost = GetEffectiveWaterCostFor(ActorInfo);
    if (EffectiveWaterCost <= 0.f || !HasAuthority(&ActivationInfo))
    {
        return;
    }

    // Cost GE�� �����Ǿ� ������ SetByCaller�� WaterCost ����
    if (GetCostGameplayEffect())
    {
        UAbilitySystemComponent* ASC = ActorInfo->AbilitySystemComponent.Get();
        if (ASC)
        {
            FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(
                GetCostGameplayEffect()->GetClass(), GetAbilityLevel());

            if (SpecHandle.IsValid())
            {
                const FDRGameplayTags& GameplayTags = FDRGameplayTags::Get();
                SpecHandle.Data.Get()->SetSetByCallerMagnitude(GameplayTags.Cost_Water, EffectiveWaterCost);

                ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
            }
        }
    }
}

void UDRGameplayAbility::OnGiveAbility(const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilitySpec& Spec)
{
    Super::OnGiveAbility(ActorInfo, Spec);

    // ActivationOwnedTags 가 AbilityTags 를 전부 포함하도록 보장.
    // 이렇게 해야 owner ASC 의 태그 카운트 이벤트가 곧 BlockAbilitiesWithTag 변화의 트리거가 되어
    // 스킬아이콘 차단 UI 가 즉시 갱신된다.
    for (const FGameplayTag& Tag : AbilityTags)
    {
        if (!ActivationOwnedTags.HasTagExact(Tag))
        {
            ActivationOwnedTags.AddTag(Tag);
        }
    }
}

// ========================= 업그레이드 칩 반영 =========================

FGameplayTag UDRGameplayAbility::GetUpgradeKeyTag() const
{
    // UDRAbilitySystemComponent::GetAbilityTagFromSpec 과 동일한 규칙 ("Abilities" 접두 태그)
    static const FGameplayTag AbilitiesTag = FGameplayTag::RequestGameplayTag(FName("Abilities"));

    for (const FGameplayTag& Tag : GetAssetTags())
    {
        if (Tag.MatchesTag(AbilitiesTag))
        {
            return Tag;
        }
    }
    return FGameplayTag();
}

const FDRUpgradeRuntime& UDRGameplayAbility::GetUpgradeRuntime() const
{
    return GetUpgradeRuntimeFor(GetCurrentActorInfo());
}

const FDRUpgradeRuntime& UDRGameplayAbility::GetUpgradeRuntimeFor(const FGameplayAbilityActorInfo* ActorInfo) const
{
    // 칩이 없는 경우(적/클렌저 사이트 등)를 위한 항등 캐시
    static const FDRUpgradeRuntime EmptyRuntime;

    if (!ActorInfo) return EmptyRuntime;

    // 플레이어는 ASC 를 PlayerState 가 소유하므로 OwnerActor 가 곧 PlayerState 다
    if (const ADRPlayerState* DRPS = Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get()))
    {
        return DRPS->GetUpgradeRuntime();
    }

    // 폴백: 아바타의 PlayerState (ASC 소유 구조가 다른 캐릭터 대비)
    if (const APawn* AvatarPawn = Cast<APawn>(ActorInfo->AvatarActor.Get()))
    {
        if (const ADRPlayerState* DRPS = AvatarPawn->GetPlayerState<ADRPlayerState>())
        {
            return DRPS->GetUpgradeRuntime();
        }
    }

    return EmptyRuntime;
}

float UDRGameplayAbility::GetUpgradedFloat(EDRUpgradeStat Stat, float BaseValue) const
{
    return GetUpgradeRuntime().ApplySkill(GetUpgradeKeyTag(), Stat, BaseValue);
}

int32 UDRGameplayAbility::GetUpgradedInt(EDRUpgradeStat Stat, int32 BaseValue, int32 MinValue) const
{
    const float Upgraded = GetUpgradedFloat(Stat, static_cast<float>(BaseValue));
    return FMath::Max(MinValue, FMath::RoundToInt(Upgraded));
}

float UDRGameplayAbility::GetEffectiveWaterCost() const
{
    return GetEffectiveWaterCostFor(GetCurrentActorInfo());
}

float UDRGameplayAbility::GetEffectiveWaterCostFor(const FGameplayAbilityActorInfo* ActorInfo) const
{
    if (WaterCost <= 0.f) return 0.f;

    const float Upgraded = GetUpgradeRuntimeFor(ActorInfo)
        .ApplySkill(GetUpgradeKeyTag(), EDRUpgradeStat::SkillWaterCost, WaterCost);

    return FMath::Max(0.f, Upgraded);
}

void UDRGameplayAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    const UGameplayEffect* CooldownGE = GetCooldownGameplayEffect();
    if (!CooldownGE) return;

    // 파생 클래스가 런타임 조건으로 쿨다운을 바꿀 수 있게 훅을 통해 읽는다 (Plan7 §5.4).
    // 기본 구현은 CooldownDuration 을 그대로 돌려준다.
    const float BaseCooldown = GetBaseCooldownDuration(ActorInfo);

    // BaseCooldown 이 0이면 GE 의 고정 Duration 을 그대로 쓴다 (기존 어빌리티 하위 호환).
    // 단 GE Duration 이 SetByCaller 인데 값이 없으면 쿨다운이 조용히 0이 되므로 경고를 남긴다.
    if (BaseCooldown <= 0.f)
    {
        if (CooldownGE->DurationMagnitude.GetMagnitudeCalculationType() == EGameplayEffectMagnitudeCalculation::SetByCaller)
        {
            UE_LOG(LogDR, Warning,
                TEXT("[Upgrade] %s: 쿨다운 GE 가 SetByCaller Duration 인데 CooldownDuration 이 0입니다 — 쿨다운이 적용되지 않습니다."),
                *GetName());
        }

        Super::ApplyCooldown(Handle, ActorInfo, ActivationInfo);
        return;
    }

    FGameplayEffectSpecHandle SpecHandle = MakeOutgoingGameplayEffectSpec(CooldownGE->GetClass(), GetAbilityLevel());
    if (!SpecHandle.IsValid()) return;

    const float UpgradedCooldown = GetUpgradeRuntimeFor(ActorInfo)
        .ApplySkill(GetUpgradeKeyTag(), EDRUpgradeStat::SkillCooldown, BaseCooldown);
    const float EffectiveCooldown = FMath::Max(MinCooldownDuration, UpgradedCooldown);

    SpecHandle.Data->SetSetByCallerMagnitude(FDRGameplayTags::Get().Data_Cooldown, EffectiveCooldown);

    ApplyGameplayEffectSpecToOwner(Handle, ActorInfo, ActivationInfo, SpecHandle);
}
