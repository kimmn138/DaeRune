// Copyright DaeRune


#include "AbilitySystem/Abilities/DRGameplayAbility.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"
#include "AbilitySystem/DRAttributeSet.h"
#include "Game/DRProgressionConfig.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Pawn.h"
#include "Player/DRPlayerState.h"
#include "DRGameplayTags.h"
#include "DaeRune/DRLogChannels.h"

bool UDRGameplayAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
    // 기본 Cost 체크
    if (!Super::CheckCost(Handle, ActorInfo, OptionalRelevantTags))
    {
        return false;
    }

    // Water Cost가 없으면 통과
    const float EffectiveWaterCost = GetEffectiveWaterCostFor(ActorInfo);
    if (EffectiveWaterCost <= 0.f)
    {
        return true;
    }

    // AttributeSet 가져오기
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

    // 물 · 체력 분배 규칙은 ExecCalc_WaterCost 와 같은 헬퍼를 쓴다 (경계값 불일치 방지)
    return UDRAbilitySystemLibrary::CanPayWaterCost(
        ASC, EffectiveWaterCost, AttributeSet->GetWater(), AttributeSet->GetHealth());
}

void UDRGameplayAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    // 기본 Cost 적용
    Super::ApplyCost(Handle, ActorInfo, ActivationInfo);

    const float EffectiveWaterCost = GetEffectiveWaterCostFor(ActorInfo);
    if (EffectiveWaterCost <= 0.f || !HasAuthority(&ActivationInfo))
    {
        return;
    }

    // Cost GE가 설정되어 있으면 SetByCaller로 WaterCost 전달
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
    else
    {
        // ★Plan5(가방칩)★ 물을 써야 하는데 Cost GE 가 없다 — CheckCost 는 통과하고 물은 안 빠진다 (D-138 · T-133).
        // 예전 GA_VacuumJetJump(Plan5 D-103)와 같은 배선 실수다. 기본 공격처럼 자주 불리므로 클래스당 1회만 알린다.
        static TSet<FName> WarnedClasses;
        const FName ClassName = GetClass()->GetFName();
        if (!WarnedClasses.Contains(ClassName))
        {
            WarnedClasses.Add(ClassName);
            UE_LOG(LogDR, Warning,
                TEXT("[Upgrade] %s: 물 소모 %.1f 인데 Cost Gameplay Effect Class 가 비어 있어 물이 빠지지 않습니다."),
                *GetName(), EffectiveWaterCost);
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
    // 같은 규칙이 발사체 액터에도 필요해져 라이브러리로 올렸다. (Plan5(씨앗칩))
    return UDRAbilitySystemLibrary::GetUpgradeKeyTagFromAbilityTags(GetAssetTags());
}

const FDRUpgradeRuntime& UDRGameplayAbility::GetUpgradeRuntime() const
{
    return GetUpgradeRuntimeFor(GetCurrentActorInfo());
}

const FDRUpgradeRuntime& UDRGameplayAbility::GetUpgradeRuntimeFor(const FGameplayAbilityActorInfo* ActorInfo) const
{
    // 같은 조회가 ExecCalc / AttributeSet / 오라 컴포넌트에도 필요해져 라이브러리로 올렸다.
    // (Plan3(공용칩) §17.5) — 동작은 이전과 동일하다:
    //  ASC 소유자(PlayerState) → 아바타의 PlayerState → 빈 캐시 순.
    static const FDRUpgradeRuntime EmptyRuntime;

    if (!ActorInfo) return EmptyRuntime;

    // ActorInfo->OwnerActor 가 곧 PlayerState 인 경로를 우선 유지한다.
    // CDO 호출 등으로 ASC 가 비어 있을 수 있어 OwnerActor 를 먼저 본다.
    if (const ADRPlayerState* DRPS = Cast<ADRPlayerState>(ActorInfo->OwnerActor.Get()))
    {
        return DRPS->GetUpgradeRuntime();
    }

    return UDRAbilitySystemLibrary::GetUpgradeRuntimeForASC(ActorInfo->AbilitySystemComponent.Get());
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

float UDRGameplayAbility::GetAttackSpeedMultiplier() const
{
    // GetAbilitySystemComponentFromActorInfo() 는 ActorInfo 가 없으면 ensure 를 띄운다 — BP Pure 노드라 직접 확인한다
    return UDRAbilitySystemLibrary::GetAttackSpeedMultiplier(
        CurrentActorInfo ? CurrentActorInfo->AbilitySystemComponent.Get() : nullptr);
}

float UDRGameplayAbility::GetEffectiveWaterCostFor(const FGameplayAbilityActorInfo* ActorInfo) const
{
    // ★Plan5(가방칩)★ 기본 물이 0 이어도 칩의 Flat 을 더한다 — 예전의 `WaterCost <= 0 → 0` 조기 return 은
    // "물 0 → 2"(Flat +2)를 통째로 무시했다 (D-125 · T-132). 칩이 없으면 (0 + 0) × 1 = 0 그대로다.
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
