// Copyright DaeRune


#include "AbilitySystem/DRCleanserSiteAttributeSet.h"
#include "Net/UnrealNetwork.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "DRGameplayTags.h"
#include "Actor/DRCleanserSite.h"
#include "AbilitySystem/DRAbilitySystemLibrary.h"

void UDRCleanserSiteAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UDRCleanserSiteAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UDRCleanserSiteAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
}

void UDRCleanserSiteAttributeSet::OnRep_Health(const FGameplayAttributeData& OldHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRCleanserSiteAttributeSet, Health, OldHealth);

	// 클라이언트에서도 체력 변화에 따라 물 메쉬 업데이트
	if (ADRCleanserSite* CleanserSite = Cast<ADRCleanserSite>(GetOwningActor()))
	{
		// 체력 비율 계산
		const float HealthRatio = GetMaxHealth() > 0.0f ? GetHealth() / GetMaxHealth() : 0.0f;
		CleanserSite->UpdateWaterMeshScale(HealthRatio);
	}
}

void UDRCleanserSiteAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const
{
	GAMEPLAYATTRIBUTE_REPNOTIFY(UDRCleanserSiteAttributeSet, MaxHealth, OldMaxHealth);
}

void UDRCleanserSiteAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// 체력은 0과 최대체력 사이로 제한
	if (Attribute == GetHealthAttribute())
	{
		NewValue = FMath::Clamp(NewValue, 0.0f, GetMaxHealth());
	}
}

void UDRCleanserSiteAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	// IncomingDamage 처리
	if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		HandleIncomingDamage(Data);
	}
}

void UDRCleanserSiteAttributeSet::HandleIncomingDamage(const FGameplayEffectModCallbackData& Data)
{
	float LocalIncomingDamage = GetIncomingDamage();
	SetIncomingDamage(0.0f);

	if (LocalIncomingDamage > 0.0f)
	{
		// Plan8(Taunt): a taunted enemy deals -20% damage to the site too (Plan8 D-14).
		// The site AS does not inherit UDRAttributeSet, so the shared damage gate is not used here.
		if (const UAbilitySystemComponent* SourceASC =
				Data.EffectSpec.GetContext().GetOriginalInstigatorAbilitySystemComponent())
		{
			LocalIncomingDamage *= UDRAbilitySystemLibrary::GetTauntDamageMultiplier(SourceASC->GetAvatarActor());
		}

		if (ADRCleanserSite* CleanserSite = Cast<ADRCleanserSite>(GetOwningActor()))
		{
			if(UAbilitySystemComponent* ASC = CleanserSite->GetAbilitySystemComponent())
			{
				// ★피격 큐는 엘리트 디버프(받는 피해 증가)가 걸린 동안의 피격에만 낸다★ — 사운드를 넣을 때부터 이 자리였다.
				// 일반 피격마다 내면 적 수십 마리가 동시에 때릴 때 소리가 겹친다.
				if (ASC->HasMatchingGameplayTag(FDRGameplayTags::Get().Debuff_Elite))
				{
					LocalIncomingDamage = FMath::RoundToFloat(LocalIncomingDamage * EliteDebuffModifier);

					FGameplayCueParameters CueParams;
					CueParams.Location = CleanserSite ? CleanserSite->GetActorLocation() : FVector::ZeroVector;
					CueParams.RawMagnitude = LocalIncomingDamage;

					ASC->ExecuteGameplayCue(
						FDRGameplayTags::Get().GameplayCue_Cleanser_Damage,
						CueParams
					);
				}
			}
		}
		
		// 체력 감소
		const float OldHealth = GetHealth();
		const float NewHealth = GetHealth() - LocalIncomingDamage;
		SetHealth(FMath::Clamp(NewHealth, 0.0f, GetMaxHealth()));

		// 체력 비율 계산
		const float HealthRatio = GetMaxHealth() > 0.0f ? NewHealth / GetMaxHealth() : 0.0f;

		if (ADRCleanserSite* CleanserSite = Cast<ADRCleanserSite>(GetOwningActor()))
		{
			CleanserSite->UpdateWaterMeshScale(HealthRatio);
		}

		// 체력이 0 이 되는 순간에만 파괴 이벤트 (0 이후의 피격마다 다시 방송하지 않는다)
		if (OldHealth > 0.0f && NewHealth <= 0.0f)
		{
			OnHealthZeroDelegate.Broadcast();
		}
	}
}
